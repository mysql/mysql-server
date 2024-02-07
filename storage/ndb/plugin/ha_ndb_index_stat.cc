/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ha_ndb_index_stat.h"

#include <ctype.h>
#include <mysql/plugin.h>
#include <mysql/psi/mysql_thread.h>
#include <time.h>
#include <atomic>

#include "m_string.h"
#include "my_dbug.h"
#include "nulls.h"
#include "sql/field.h"
#include "sql/mysqld.h"  // LOCK_global_system_variables
#include "sql/partition_info.h"
#include "sql/table.h"
#include "storage/ndb/plugin/ha_ndbcluster.h"
#include "storage/ndb/plugin/ha_ndbcluster_connection.h"
#include "storage/ndb/plugin/ndb_require.h"

// Implementation still uses its own instance
extern Ndb_index_stat_thread ndb_index_stat_thread;

/* Implemented in ha_ndbcluster.cc */
extern bool ndb_index_stat_get_enable(THD *thd);

// Typedefs for long names
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Index NDBINDEX;

/** ndb_index_stat_thread */
Ndb_index_stat_thread::Ndb_index_stat_thread()
    : Ndb_component("Index Stat", "ndb_index_stat"), client_waiting(false) {}

int Ndb_index_stat_thread::do_init() {
  mysql_mutex_init(PSI_INSTRUMENT_ME, &LOCK_client_waiting, MY_MUTEX_INIT_FAST);
  mysql_cond_init(PSI_INSTRUMENT_ME, &COND_client_waiting);

  mysql_mutex_init(PSI_INSTRUMENT_ME, &stat_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(PSI_INSTRUMENT_ME, &stat_cond);
  return 0;
}

Ndb_index_stat_thread::~Ndb_index_stat_thread() {}

int Ndb_index_stat_thread::do_deinit() {
  mysql_mutex_destroy(&LOCK_client_waiting);
  mysql_cond_destroy(&COND_client_waiting);

  mysql_mutex_destroy(&stat_mutex);
  mysql_cond_destroy(&stat_cond);
  return 0;
}

void Ndb_index_stat_thread::do_wakeup() {
  // Wakeup from potential wait
  log_info("Wakeup");

  wakeup();
}

void Ndb_index_stat_thread::wakeup() {
  mysql_mutex_lock(&LOCK_client_waiting);
  client_waiting = true;
  mysql_cond_signal(&COND_client_waiting);
  mysql_mutex_unlock(&LOCK_client_waiting);
}

struct Ndb_index_stat {
  enum List_type {
    LT_Undef = 0,
    LT_New = 1,    /* new entry added by a table handler */
    LT_Update = 2, /* force kernel update from analyze table */
    LT_Read = 3,   /* read or reread stats into new query cache */
    LT_Idle = 4,   /* stats exist */
    LT_Check = 5,  /* check for new stats */
    LT_Delete = 6, /* delete the entry */
    LT_Error = 7,  /* error, on hold for a while */
    LT_Count = 8
  };
  NdbIndexStat *is;
  int index_id;
  int index_version;
#ifndef NDEBUG
  char id[32];
#endif
  time_t access_time;  /* by any table handler */
  time_t update_time;  /* latest successful update by us */
  time_t load_time;    /* when stats were created by kernel */
  time_t read_time;    /* when stats were read by us (>= load_time) */
  uint sample_version; /* goes with read_time */
  time_t check_time;   /* when checked for updated stats (>= read_time) */
  uint query_bytes;    /* cache query bytes in use */
  uint clean_bytes;    /* cache clean bytes waiting to be deleted */
  uint drop_bytes;     /* cache bytes waiting for drop */
  uint evict_bytes;    /* cache bytes waiting for evict */
  bool force_update;   /* one-time force update from analyze table */
  bool no_stats;       /* have detected that no stats exist */
  NdbIndexStat::Error error;
  NdbIndexStat::Error client_error;
  time_t error_time;
  uint error_count;                  /* forever increasing */
  struct Ndb_index_stat *share_next; /* per-share list */
  int array_index{LT_Undef}; /* Index indicating which of the lists in the
                                "ndb_index_stat_list" array is used */
  struct Ndb_index_stat *list_next;
  struct Ndb_index_stat *list_prev;
  struct NDB_SHARE *share;
  bool to_delete;     /* detached from share and marked for delete */
  bool abort_request; /* abort all requests and allow no more */
  Ndb_index_stat();

 private:
  std::atomic<uint> m_ref_count{0}; /* from client requests */
 public:
  void acquire_client_ref() { m_ref_count++; }
  void release_client_ref() {
    const uint prev_count [[maybe_unused]] = m_ref_count--;
    // Detect decrement from zero, indicates mismatch
    assert(prev_count > 0);
  }
  bool has_client_ref() const { return m_ref_count != 0; }
};

struct Ndb_index_stat_list {
  const char *const name; /* name of list */
  struct Ndb_index_stat *head{nullptr};
  struct Ndb_index_stat *tail{nullptr};
  const Ndb_index_stat::List_type list_type; /* type of list */
  uint count{0}; /* number of entries in list, i.e between "head" and "tail" */
  Ndb_index_stat_list(Ndb_index_stat::List_type _list_type, const char *_name)
      : name(_name), list_type(_list_type) {}
};

extern Ndb_index_stat_list ndb_index_stat_list[];

static time_t ndb_index_stat_time_now = 0;

static time_t ndb_index_stat_time() {
  time_t now = time(nullptr);

  if (unlikely(ndb_index_stat_time_now == 0)) ndb_index_stat_time_now = now;

  if (unlikely(now < ndb_index_stat_time_now)) {
    DBUG_PRINT("index_stat", ("time moved backwards %d seconds",
                              int(ndb_index_stat_time_now - now)));
    now = ndb_index_stat_time_now;
  }

  ndb_index_stat_time_now = now;
  return now;
}

/* Options */

/* Options in string format buffer size */
static const uint ndb_index_stat_option_sz = 512;
static void ndb_index_stat_opt2str(const struct Ndb_index_stat_opt &, char *);

struct Ndb_index_stat_opt {
  enum Unit { Ubool = 1, Usize = 2, Utime = 3, Umsec = 4 };
  enum Flag { Freadonly = (1 << 0), Fcontrol = (1 << 1) };
  struct Val {
    const char *name;
    uint val;
    uint minval;
    uint maxval;
    Unit unit;
    uint flag;
  };
  enum Idx {
    Iloop_enable = 0,
    Iloop_idle = 1,
    Iloop_busy = 2,
    Iupdate_batch = 3,
    Iread_batch = 4,
    Iidle_batch = 5,
    Icheck_batch = 6,
    Icheck_delay = 7,
    Idelete_batch = 8,
    Iclean_delay = 9,
    Ierror_batch = 10,
    Ierror_delay = 11,
    Ievict_batch = 12,
    Ievict_delay = 13,
    Icache_limit = 14,
    Icache_lowpct = 15,
    Izero_total = 16,
    Imax = 17
  };
  Val val[Imax];
  /* Options in string format (SYSVAR ndb_index_stat_option) */
  char *option;
  Ndb_index_stat_opt(char *buf);
  uint get(Idx i) const {
    assert(i < Imax);
    return val[i].val;
  }
  void set(Idx i, uint the_val) {
    assert(i < Imax);
    val[i].val = the_val;
  }
};

Ndb_index_stat_opt::Ndb_index_stat_opt(char *buf) : option(buf) {
#define ival(aname, aval, aminval, amaxval, aunit, aflag) \
  val[I##aname].name = #aname;                            \
  val[I##aname].val = aval;                               \
  val[I##aname].minval = aminval;                         \
  val[I##aname].maxval = amaxval;                         \
  val[I##aname].unit = aunit;                             \
  val[I##aname].flag = aflag
  ival(loop_enable, 1000, 0, ~(uint)0, Umsec, 0);
  ival(loop_idle, 1000, 0, ~(uint)0, Umsec, 0);
  ival(loop_busy, 100, 0, ~(uint)0, Umsec, 0);
  ival(update_batch, 1, 1, ~(uint)0, Usize, 0);
  ival(read_batch, 4, 1, ~(uint)0, Usize, 0);
  ival(idle_batch, 32, 1, ~(uint)0, Usize, 0);
  ival(check_batch, 8, 1, ~(uint)0, Usize, 0);
  ival(check_delay, 600, 0, ~(uint)0, Utime, 0);
  ival(clean_delay, 60, 0, ~(uint)0, Utime, 0);
  ival(delete_batch, 8, 1, ~(uint)0, Usize, 0);
  ival(error_batch, 4, 1, ~(uint)0, Usize, 0);
  ival(error_delay, 60, 0, ~(uint)0, Utime, 0);
  ival(evict_batch, 8, 1, ~(uint)0, Usize, 0);
  ival(evict_delay, 60, 0, ~(uint)0, Utime, 0);
  ival(cache_limit, 32 * 1024 * 1024, 0, ~(uint)0, Usize, 0);
  ival(cache_lowpct, 90, 0, 100, Usize, 0);
  ival(zero_total, 0, 0, 1, Ubool, Fcontrol);
#undef ival

  ndb_index_stat_opt2str(*this, option);
}

/* Hard limits */
static const uint ndb_index_stat_max_evict_batch = 32;

char ndb_index_stat_option_buf[ndb_index_stat_option_sz];
static Ndb_index_stat_opt ndb_index_stat_opt(ndb_index_stat_option_buf);

/* Copy option struct to string buffer */
static void ndb_index_stat_opt2str(const Ndb_index_stat_opt &opt, char *str) {
  DBUG_TRACE;

  char buf[ndb_index_stat_option_sz];
  char *const end = &buf[sizeof(buf)];
  char *ptr = buf;
  *ptr = 0;

  const uint imax = Ndb_index_stat_opt::Imax;
  for (uint i = 0; i < imax; i++) {
    const Ndb_index_stat_opt::Val &v = opt.val[i];
    ptr += strlen(ptr);
    const char *sep = (ptr == buf ? "" : ",");
    const uint sz = ptr < end ? (uint)(end - ptr) : 0;

    switch (v.unit) {
      case Ndb_index_stat_opt::Ubool: {
        assert(v.val == 0 || v.val == 1);
        if (v.val == 0)
          snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else
          snprintf(ptr, sz, "%s%s=1", sep, v.name);
      } break;

      case Ndb_index_stat_opt::Usize: {
        uint m;
        if (v.val == 0)
          snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else if (v.val % (m = 1024 * 1024 * 1024) == 0)
          snprintf(ptr, sz, "%s%s=%uG", sep, v.name, v.val / m);
        else if (v.val % (m = 1024 * 1024) == 0)
          snprintf(ptr, sz, "%s%s=%uM", sep, v.name, v.val / m);
        else if (v.val % (m = 1024) == 0)
          snprintf(ptr, sz, "%s%s=%uK", sep, v.name, v.val / m);
        else
          snprintf(ptr, sz, "%s%s=%u", sep, v.name, v.val);
      } break;

      case Ndb_index_stat_opt::Utime: {
        uint m;
        if (v.val == 0)
          snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else if (v.val % (m = 60 * 60 * 24) == 0)
          snprintf(ptr, sz, "%s%s=%ud", sep, v.name, v.val / m);
        else if (v.val % (m = 60 * 60) == 0)
          snprintf(ptr, sz, "%s%s=%uh", sep, v.name, v.val / m);
        else if (v.val % (m = 60) == 0)
          snprintf(ptr, sz, "%s%s=%um", sep, v.name, v.val / m);
        else
          snprintf(ptr, sz, "%s%s=%us", sep, v.name, v.val);
      } break;

      case Ndb_index_stat_opt::Umsec: {
        if (v.val == 0)
          snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else
          snprintf(ptr, sz, "%s%s=%ums", sep, v.name, v.val);
      } break;

      default:
        assert(false);
        break;
    }
  }

  memset(str, 0, ndb_index_stat_option_sz);
  strcpy(str, buf);
  DBUG_PRINT("index_stat", ("str: \"%s\"", str));
}

static int ndb_index_stat_option_parse(char *p, Ndb_index_stat_opt &opt) {
  DBUG_TRACE;

  char *r = strchr(p, '=');
  if (r == nullptr) return -1;
  *r++ = 0;

  while (isspace(*r)) *r++ = 0;
  if (*r == 0) return -1;

  bool found = false;
  const uint imax = Ndb_index_stat_opt::Imax;
  for (uint i = 0; i < imax; i++) {
    Ndb_index_stat_opt::Val &v = opt.val[i];
    if (strcmp(p, v.name) != 0) continue;
    found = true;

    char *s;
    for (s = r; *s != 0; s++) *s = tolower(*s);
    ulonglong val = my_strtoull(r, &s, 10);

    switch (v.unit) {
      case Ndb_index_stat_opt::Ubool: {
        if ((s > r && *s == 0 && val == 0) || strcmp(r, "off") == 0 ||
            strcmp(r, "false") == 0)
          val = 0;
        else if ((s > r && *s == 0 && val == 1) || strcmp(r, "on") == 0 ||
                 strcmp(r, "true") == 0)
          val = 1;
        else
          return -1;
        v.val = (uint)val;
      } break;

      case Ndb_index_stat_opt::Usize: {
        if (s == r) return -1;
        if (strcmp(s, "") == 0)
          ;
        else if (strcmp(s, "k") == 0)
          val *= 1024;
        else if (strcmp(s, "m") == 0)
          val *= 1024 * 1024;
        else if (strcmp(s, "g") == 0)
          val *= 1024 * 1024 * 1024;
        else
          return -1;
        if (val < v.minval || val > v.maxval) return -1;
        v.val = (uint)val;
      } break;

      case Ndb_index_stat_opt::Utime: {
        if (s == r) return -1;
        if (strcmp(s, "") == 0)
          ;
        else if (strcmp(s, "s") == 0)
          ;
        else if (strcmp(s, "m") == 0)
          val *= 60;
        else if (strcmp(s, "h") == 0)
          val *= 60 * 60;
        else if (strcmp(s, "d") == 0)
          val *= 24 * 60 * 60;
        else
          return -1;
        if (val < v.minval || val > v.maxval) return -1;
        v.val = (uint)val;
      } break;

      case Ndb_index_stat_opt::Umsec: {
        if (s == r) return -1;
        if (strcmp(s, "") == 0)
          ;
        else if (strcmp(s, "ms") == 0)
          ;
        else
          return -1;
        if (val < v.minval || val > v.maxval) return -1;
        v.val = (uint)val;
      } break;

      default:
        assert(false);
        break;
    }
  }

  if (!found) return -1;
  return 0;
}

/* Copy option string to option struct */
static int ndb_index_stat_str2opt(const char *str, Ndb_index_stat_opt &opt) {
  DBUG_TRACE;
  DBUG_PRINT("index_stat", ("str: \"%s\"", str));

  char buf[ndb_index_stat_option_sz];

  assert(str != nullptr);
  if (strlen(str) >= sizeof(buf)) return -1;
  strcpy(buf, str);

  char *p = buf;
  while (1) {
    while (isspace(*p)) p++;
    if (*p == 0) break;

    char *q = strchr(p, ',');
    if (q == p) return -1;
    if (q != nullptr) *q = 0;

    DBUG_PRINT("index_stat", ("parse: %s", p));
    if (ndb_index_stat_option_parse(p, opt) == -1) return -1;

    if (q == nullptr) break;
    p = q + 1;
  }

  ndb_index_stat_opt2str(opt, opt.option);
  return 0;
}

/* Thanks to ha_innodb.cc */

/* Need storage between check and update (assume locked) */
static char ndb_index_stat_option_tmp[ndb_index_stat_option_sz];

int ndb_index_stat_option_check(THD *, SYS_VAR *, void *save,
                                struct st_mysql_value *value) {
  DBUG_TRACE;
  char buf[ndb_index_stat_option_sz];
  int len = sizeof(buf);
  const char *str = value->val_str(value, buf, &len);
  if (str != nullptr) {
    /* Seems to be nothing in buf */
    DBUG_PRINT("index_stat", ("str: %s len: %d", str, len));
    char buf2[ndb_index_stat_option_sz];
    Ndb_index_stat_opt opt(buf2);
    if (ndb_index_stat_str2opt(str, opt) == 0) {
      /* Passed to update */
      strcpy(ndb_index_stat_option_tmp, str);
      *(const char **)save = ndb_index_stat_option_tmp;
      return 0;
    }
  }
  return 1;
}

void ndb_index_stat_option_update(THD *, SYS_VAR *, void *var_ptr,
                                  const void *save) {
  DBUG_TRACE;
  const char *str = *static_cast<const char *const *>(save);
  DBUG_PRINT("index_stat", ("str: %s", str));
  Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  int ret = ndb_index_stat_str2opt(str, opt);
  ndbcluster::ndbrequire(ret == 0);
  *(const char **)var_ptr = ndb_index_stat_opt.option;
}

/* Global stuff */

struct Ndb_index_stat_glob {
  bool th_allow;  /* Queries allowed */
  bool th_enable; /* Stats thread idea of ndb_index_stat_enable */
  bool th_busy;   /* Stats thread is busy-looping */
  uint th_loop;   /* Stats thread current loop wait in ms */
  uint force_update;
  uint wait_update;
  uint no_stats;
  uint wait_stats;
  /* Accumulating counters */
  uint analyze_count; /* Client counters */
  uint analyze_error;
  uint query_count;
  uint query_no_stats;
  uint query_error;
  uint event_act;     /* Events acted on */
  uint event_skip;    /* Events skipped (likely event-to-self) */
  uint event_miss;    /* Events received for unknown index */
  uint refresh_count; /* Successful cache refreshes */
  uint clean_count;   /* Times old caches (1 or more) cleaned */
  uint pinned_count;  /* Times not cleaned due to old cache ref count */
  uint drop_count;    /* From index drop */
  uint evict_count;   /* From LRU cleanup */
  /* Cache */
  uint cache_query_bytes; /* In use */
  uint cache_clean_bytes; /* Obsolete versions not yet removed */
  uint cache_high_bytes;  /* Max ever of above */
  uint cache_drop_bytes;  /* Part of above waiting to be evicted */
  uint cache_evict_bytes; /* Part of above waiting to be evicted */
  char status[2][1024];
  uint status_i;

  Ndb_index_stat_glob();
  void set_status();
  void zero_total();
};

Ndb_index_stat_glob::Ndb_index_stat_glob() {
  th_allow = false;
  th_enable = false;
  th_busy = false;
  th_loop = 0;
  force_update = 0;
  wait_update = 0;
  no_stats = 0;
  wait_stats = 0;
  analyze_count = 0;
  analyze_error = 0;
  query_count = 0;
  query_no_stats = 0;
  query_error = 0;
  event_act = 0;
  event_skip = 0;
  event_miss = 0;
  refresh_count = 0;
  clean_count = 0;
  pinned_count = 0;
  drop_count = 0;
  evict_count = 0;
  cache_query_bytes = 0;
  cache_clean_bytes = 0;
  cache_high_bytes = 0;
  cache_drop_bytes = 0;
  cache_evict_bytes = 0;
  memset(status, 0, sizeof(status));
  status_i = 0;
}

static Ndb_index_stat_glob ndb_index_stat_glob;

/*
  Check if stats thread is running and has initialized required
  objects.  Sync the value with global status ("allow" field).
*/

static bool ndb_index_stat_allow_flag = false;

static bool ndb_index_stat_get_allow() { return ndb_index_stat_allow_flag; }

static bool ndb_index_stat_set_allow(bool flag) {
  if (ndb_index_stat_allow_flag != flag) {
    ndb_index_stat_allow_flag = flag;
    Ndb_index_stat_glob &glob = ndb_index_stat_glob;
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    glob.set_status();
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  }
  return ndb_index_stat_allow_flag;
}

static const char *g_ndb_status_index_stat_status = "";
static long g_ndb_status_index_stat_cache_query = 0;
static long g_ndb_status_index_stat_cache_clean = 0;

/* Update status variable (must hold stat_mutex) */
void Ndb_index_stat_glob::set_status() {
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  char *p = status[status_i];

  // stats thread
  th_allow = ndb_index_stat_get_allow();
  sprintf(p, "allow:%d,enable:%d,busy:%d,loop:%u", th_allow, th_enable, th_busy,
          th_loop);
  p += strlen(p);

  // entry lists
  strcpy(p, ",list:(");
  p += strlen(p);
  uint list_count = 0;
  for (int lt = Ndb_index_stat::LT_New; lt < Ndb_index_stat::LT_Count; lt++) {
    const Ndb_index_stat_list &list = ndb_index_stat_list[lt];
    sprintf(p, "%s:%u,", list.name, list.count);
    p += strlen(p);
    list_count += list.count;
  }
  sprintf(p, "%s:%u)", "total", list_count);
  p += strlen(p);

  // special counters
  sprintf(p, ",analyze:(queue:%u,wait:%u)", force_update, wait_update);
  p += strlen(p);
  sprintf(p, ",stats:(nostats:%u,wait:%u)", no_stats, wait_stats);
  p += strlen(p);

  // accumulating counters
  sprintf(p, ",total:(");
  p += strlen(p);
  sprintf(p, "analyze:(all:%u,error:%u)", analyze_count, analyze_error);
  p += strlen(p);
  sprintf(p, ",query:(all:%u,nostats:%u,error:%u)", query_count, query_no_stats,
          query_error);
  p += strlen(p);
  sprintf(p, ",event:(act:%u,skip:%u,miss:%u)", event_act, event_skip,
          event_miss);
  p += strlen(p);
  sprintf(p, ",cache:(refresh:%u,clean:%u,pinned:%u,drop:%u,evict:%u)",
          refresh_count, clean_count, pinned_count, drop_count, evict_count);
  p += strlen(p);
  sprintf(p, ")");
  p += strlen(p);

  // cache size
  const uint cache_limit = opt.get(Ndb_index_stat_opt::Icache_limit);
  const uint cache_total = cache_query_bytes + cache_clean_bytes;
  double cache_pct = (double)0.0;
  double cache_high_pct = (double)0.0;
  if (cache_limit != 0) {
    cache_pct = (double)100.0 * (double)cache_total / (double)cache_limit;
    cache_high_pct =
        (double)100.0 * (double)cache_high_bytes / (double)cache_limit;
  }
  sprintf(p,
          ",cache:(query:%u,clean:%u"
          ",drop:%u,evict:%u"
          ",usedpct:%.2f,highpct:%.2f)",
          cache_query_bytes, cache_clean_bytes, cache_drop_bytes,
          cache_evict_bytes, cache_pct, cache_high_pct);
  p += strlen(p);

  // alternating status buffers to keep this lock short
  mysql_mutex_lock(&LOCK_global_system_variables);
  g_ndb_status_index_stat_status = status[status_i];
  status_i = (status_i + 1) % 2;
  g_ndb_status_index_stat_cache_query = cache_query_bytes;
  g_ndb_status_index_stat_cache_clean = cache_clean_bytes;
  mysql_mutex_unlock(&LOCK_global_system_variables);
}

static long g_ndb_status_index_stat_event_count = 0;

/* Zero accumulating counters */
void Ndb_index_stat_glob::zero_total() {
  analyze_count = 0;
  analyze_error = 0;
  query_count = 0;
  query_no_stats = 0;
  query_error = 0;
  event_act = 0;
  event_skip = 0;
  event_miss = 0;
  refresh_count = 0;
  clean_count = 0;
  pinned_count = 0;
  drop_count = 0;
  evict_count = 0;
  /* Reset highest use seen to current */
  cache_high_bytes = cache_query_bytes + cache_clean_bytes;
  g_ndb_status_index_stat_event_count = 0;
}

/* Shared index entries */

Ndb_index_stat::Ndb_index_stat() {
  is = nullptr;
  index_id = 0;
  index_version = 0;
#ifndef NDEBUG
  memset(id, 0, sizeof(id));
#endif
  access_time = 0;
  update_time = 0;
  load_time = 0;
  read_time = 0;
  sample_version = 0;
  check_time = 0;
  query_bytes = 0;
  clean_bytes = 0;
  drop_bytes = 0;
  evict_bytes = 0;
  force_update = false;
  no_stats = false;
  error_time = 0;
  error_count = 0;
  share_next = nullptr;
  list_next = nullptr;
  list_prev = nullptr;
  share = nullptr;
  to_delete = false;
  abort_request = false;
}

/*
  Called by stats thread and (rarely) by client.  Caller must hold
  stat_mutex.  Client errors currently have no effect on execution
  since they are probably local e.g. bad range (internal error).
  Argument "from" is 0=stats thread 1=client.
*/
static void ndb_index_stat_error(Ndb_index_stat *st, int from,
                                 const char *place [[maybe_unused]]) {
  time_t now = ndb_index_stat_time();
  NdbIndexStat::Error error = st->is->getNdbError();
  if (error.code == 0) {
    /* Make sure code is not 0 */
    NdbIndexStat::Error error2;
    error = error2;
    error.code = NdbIndexStat::InternalError;
    error.status = NdbError::TemporaryError;
  }
  if (from == 0) {
    st->error = error;
    st->error_time = now; /* Controls proc_error */
  } else
    st->client_error = error;
  st->error_count++;

  DBUG_PRINT("index_stat", ("%s error, code: %d, line: %d, extra: %d", place,
                            error.code, error.line, error.extra));
}

static void ndb_index_stat_clear_error(Ndb_index_stat *st) {
  st->error.code = 0;
  st->error.status = NdbError::Success;
}

/* Lists across shares */

Ndb_index_stat_list ndb_index_stat_list[Ndb_index_stat::LT_Count] = {
    Ndb_index_stat_list(Ndb_index_stat::LT_Undef, nullptr),
    Ndb_index_stat_list(Ndb_index_stat::LT_New, "new"),
    Ndb_index_stat_list(Ndb_index_stat::LT_Update, "update"),
    Ndb_index_stat_list(Ndb_index_stat::LT_Read, "read"),
    Ndb_index_stat_list(Ndb_index_stat::LT_Idle, "idle"),
    Ndb_index_stat_list(Ndb_index_stat::LT_Check, "check"),
    Ndb_index_stat_list(Ndb_index_stat::LT_Delete, "delete"),
    Ndb_index_stat_list(Ndb_index_stat::LT_Error, "error")};

static void ndb_index_stat_list_add(Ndb_index_stat *st, int lt) {
  assert(st != nullptr && st->array_index == Ndb_index_stat::LT_Undef);
  assert(st->list_next == nullptr && st->list_prev == nullptr);
  assert(Ndb_index_stat::LT_New <= lt && lt < Ndb_index_stat::LT_Count);
  Ndb_index_stat_list &list = ndb_index_stat_list[lt];

  DBUG_PRINT("index_stat", ("st %s -> %s", st->id, list.name));

  if (list.count == 0) {
    assert(list.head == nullptr && list.tail == nullptr);
    list.head = st;
    list.tail = st;
  } else {
    assert(list.tail != nullptr && list.tail->list_next == nullptr);
    st->list_prev = list.tail;
    list.tail->list_next = st;
    list.tail = st;
  }
  list.count++;

  st->array_index = lt;
}

static void ndb_index_stat_list_remove(Ndb_index_stat *st) {
  assert(st != nullptr);
  const int lt = st->array_index;
  assert(Ndb_index_stat::LT_New <= lt && lt < Ndb_index_stat::LT_Count);
  Ndb_index_stat_list &list = ndb_index_stat_list[lt];

  DBUG_PRINT("index_stat", ("st %s <- %s", st->id, list.name));

  Ndb_index_stat *next = st->list_next;
  Ndb_index_stat *prev = st->list_prev;

  if (list.head == st) list.head = next;
  if (list.tail == st) list.tail = prev;
  assert(list.count != 0);
  list.count--;

  if (next != nullptr) next->list_prev = prev;
  if (prev != nullptr) prev->list_next = next;

  st->array_index = Ndb_index_stat::LT_Undef;
  st->list_next = nullptr;
  st->list_prev = nullptr;
}

static void ndb_index_stat_list_move(Ndb_index_stat *st, int lt) {
  assert(st != nullptr);
  ndb_index_stat_list_remove(st);
  ndb_index_stat_list_add(st, lt);
}

/* Stats entry changes (must hold stat_mutex) */

static void ndb_index_stat_force_update(Ndb_index_stat *st, bool onoff) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  if (onoff) {
    if (!st->force_update) {
      glob.force_update++;
      st->force_update = true;
      glob.set_status();
    }
  } else {
    if (st->force_update) {
      assert(glob.force_update != 0);
      glob.force_update--;
      st->force_update = false;
      glob.set_status();
    }
  }
}

static void ndb_index_stat_no_stats(Ndb_index_stat *st, bool flag) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  if (st->no_stats != flag) {
    if (flag) {
      glob.no_stats++;
      st->no_stats = true;
    } else {
      assert(glob.no_stats >= 1);
      glob.no_stats -= 1;
      st->no_stats = false;
    }
    glob.set_status();
  }
}

/* Find or add entry under the share */

/* Saved in ndb_index_stat_get_share() under stat_mutex */
struct Ndb_index_stat_snap {
  time_t load_time;
  uint sample_version;
  uint error_count;
  Ndb_index_stat_snap() {
    load_time = 0;
    sample_version = 0;
    error_count = 0;
  }
};

/* Subroutine, have lock */
static Ndb_index_stat *ndb_index_stat_alloc(const NDBINDEX *index,
                                            const NDBTAB *table, int &err_out) {
  err_out = 0;
  Ndb_index_stat *st = new Ndb_index_stat;
  NdbIndexStat *is = new NdbIndexStat;
  if (st != nullptr && is != nullptr) {
    st->is = is;
    st->index_id = index->getObjectId();
    st->index_version = index->getObjectVersion();
#ifndef NDEBUG
    snprintf(st->id, sizeof(st->id), "%d.%d", st->index_id, st->index_version);
#endif
    if (is->set_index(*index, *table) == 0) return st;
    ndb_index_stat_error(st, 1, "set_index");
    err_out = st->client_error.code;
  } else {
    err_out = NdbIndexStat::NoMemError;
  }
  if (is != nullptr) delete is;
  if (st != nullptr) delete st;
  return nullptr;
}

/* Subroutine, have lock */
static Ndb_index_stat *ndb_index_stat_find_share(NDB_SHARE *share,
                                                 const NDBINDEX *index,
                                                 Ndb_index_stat *&st_last) {
  struct Ndb_index_stat *st = share->index_stat_list;
  st_last = nullptr;
  while (st != nullptr) {
    assert(st->share == share);
    assert(st->is != nullptr);
    NdbIndexStat::Head head;
    st->is->get_head(head);
    if (head.m_indexId == (uint)index->getObjectId() &&
        head.m_indexVersion == (uint)index->getObjectVersion())
      break;
    st_last = st;
    st = st->share_next;
  }
  return st;
}

/* Subroutine, have lock */
static void ndb_index_stat_add_share(NDB_SHARE *share, Ndb_index_stat *st,
                                     Ndb_index_stat *st_last) {
  st->share = share;
  if (st_last == nullptr)
    share->index_stat_list = st;
  else
    st_last->share_next = st;
}

static Ndb_index_stat *ndb_index_stat_get_share(NDB_SHARE *share,
                                                const NDBINDEX *index,
                                                const NDBTAB *table,
                                                Ndb_index_stat_snap &snap,
                                                int &err_out, bool allow_add,
                                                bool force_update) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;

  mysql_mutex_lock(&share->mutex);
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  time_t now = ndb_index_stat_time();
  err_out = 0;

  struct Ndb_index_stat *st = nullptr;
  struct Ndb_index_stat *st_last = nullptr;
  do {
    if (unlikely(!ndb_index_stat_get_allow())) {
      err_out = NdbIndexStat::MyNotAllow;
      break;
    }
    st = ndb_index_stat_find_share(share, index, st_last);
    if (st == nullptr) {
      if (!allow_add) {
        err_out = NdbIndexStat::MyNotFound;
        break;
      }
      st = ndb_index_stat_alloc(index, table, err_out);
      if (st == nullptr) {
        assert(err_out != 0);
        break;
      }
      ndb_index_stat_add_share(share, st, st_last);
      ndb_index_stat_list_add(st, Ndb_index_stat::LT_New);
      glob.set_status();
    } else if (unlikely(st->abort_request)) {
      err_out = NdbIndexStat::MyAbortReq;
      break;
    }
    if (force_update) ndb_index_stat_force_update(st, true);
    snap.load_time = st->load_time;
    snap.sample_version = st->sample_version;
    snap.error_count = st->error_count;
    st->access_time = now;
  } while (0);

  if (err_out == 0) {
    st->acquire_client_ref();
  } else
    st = nullptr;

  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  mysql_mutex_unlock(&share->mutex);
  return st;
}

/*
  Prepare to delete index stat entry.  Remove it from per-share
  list and set "to_delete" flag.  Stats thread does real delete.
*/

/* caller must hold stat_mutex */
static void ndb_index_stat_free(Ndb_index_stat *st) {
  DBUG_TRACE;
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  NDB_SHARE *share = st->share;
  assert(share != nullptr);

  Ndb_index_stat *st_head = nullptr;
  Ndb_index_stat *st_tail = nullptr;
  Ndb_index_stat *st_loop = share->index_stat_list;
  uint found [[maybe_unused]] = 0;
  while (st_loop != nullptr) {
    if (st == st_loop) {
      // Unlink entry from NDB_SHARE and request it to be released
      DBUG_PRINT("index_stat", ("st %s stat free one", st->id));
      st_loop = st_loop->share_next;
      st->share_next = nullptr;
      st->share = nullptr;
      assert(!st->to_delete);
      st->to_delete = true;
      st->abort_request = true;
      found++;
    } else {
      if (st_head == nullptr)
        st_head = st_loop;
      else
        st_tail->share_next = st_loop;
      st_tail = st_loop;
      st_loop = st_loop->share_next;
      st_tail->share_next = nullptr;
    }
  }
  assert(found == 1);
  share->index_stat_list = st_head;

  glob.set_status();
}

/* Interface to online drop index */
void ndb_index_stat_free(NDB_SHARE *share, int index_id, int index_version) {
  DBUG_TRACE;
  DBUG_PRINT("index_stat",
             ("(index_id:%d index_version:%d", index_id, index_version));
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);

  Ndb_index_stat *st = share->index_stat_list;
  while (st != nullptr) {
    if (st->index_id == index_id && st->index_version == index_version) {
      ndb_index_stat_free(st);
      glob.drop_count++;
      assert(st->drop_bytes == 0);
      st->drop_bytes = st->query_bytes + st->clean_bytes;
      glob.cache_drop_bytes += st->drop_bytes;
      break;
    }
    st = st->share_next;
  }

  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

void ndb_index_stat_free(NDB_SHARE *share) {
  DBUG_TRACE;
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);

  Ndb_index_stat *st;
  while ((st = share->index_stat_list) != nullptr) {
    // Unlink entry from NDB_SHARE and request it to be released
    DBUG_PRINT("index_stat", ("st %s stat free all", st->id));
    share->index_stat_list = st->share_next;
    st->share_next = nullptr;
    st->share = nullptr;
    assert(!st->to_delete);
    st->to_delete = true;
    st->abort_request = true;
    glob.drop_count++;
    assert(st->drop_bytes == 0);
    st->drop_bytes += st->query_bytes + st->clean_bytes;
    glob.cache_drop_bytes += st->drop_bytes;
  }

  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

/**
   @brief Find first Ndb_index_stat entry matching id and version in any of the
   ndb_index_stat_lists

   @param index_id Id of the index stat to find
   @param index_version Version of the index stat to find

   @return Pointer to Ndb_index_stat or nullptr if no matching index stat found
*/
static Ndb_index_stat *find_entry(int index_id, int index_version) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("id: %d version: %d", index_id, index_version));

  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  // Iterate through array of Ndb_index_stat_list
  for (int lt = Ndb_index_stat::LT_New; lt < Ndb_index_stat::LT_Count; lt++) {
    Ndb_index_stat *st = ndb_index_stat_list[lt].head;
    // Iterate the linked list of Ndb_index_stat
    while (st != nullptr) {
      if (st->index_id == index_id && st->index_version == index_version) {
        // Found Ndb_index_stat with matching id and version
        mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
        return st;
      }
      st = st->list_next;
    }
  }
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  return nullptr;
}

/* Statistics thread sub-routines */

static void ndb_index_stat_cache_move(Ndb_index_stat *st) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  NdbIndexStat::CacheInfo infoBuild;
  NdbIndexStat::CacheInfo infoQuery;

  st->is->get_cache_info(infoBuild, NdbIndexStat::CacheBuild);
  st->is->get_cache_info(infoQuery, NdbIndexStat::CacheQuery);
  const uint new_query_bytes = infoBuild.m_totalBytes;
  const uint old_query_bytes = infoQuery.m_totalBytes;
  DBUG_PRINT("index_stat", ("st %s cache move: query:%u clean:%u", st->id,
                            new_query_bytes, old_query_bytes));
  st->is->move_cache();
  st->query_bytes = new_query_bytes;
  st->clean_bytes += old_query_bytes;
  assert(glob.cache_query_bytes >= old_query_bytes);
  glob.cache_query_bytes -= old_query_bytes;
  glob.cache_query_bytes += new_query_bytes;
  glob.cache_clean_bytes += old_query_bytes;
  const uint cache_total = glob.cache_query_bytes + glob.cache_clean_bytes;
  if (glob.cache_high_bytes < cache_total) glob.cache_high_bytes = cache_total;
}

static bool ndb_index_stat_cache_clean(Ndb_index_stat *st) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  NdbIndexStat::CacheInfo infoClean;

  st->is->get_cache_info(infoClean, NdbIndexStat::CacheClean);
  const uint old_clean_bytes = infoClean.m_totalBytes;
  const uint ref_count = infoClean.m_ref_count;
  DBUG_PRINT("index_stat", ("st %s cache clean: clean:%u ref_count:%u", st->id,
                            old_clean_bytes, ref_count));
  if (ref_count != 0) return false;
  st->is->clean_cache();
  st->clean_bytes = 0;
  assert(glob.cache_clean_bytes >= old_clean_bytes);
  glob.cache_clean_bytes -= old_clean_bytes;
  return true;
}

static void ndb_index_stat_cache_evict(Ndb_index_stat *st) {
  NdbIndexStat::Head head;
  NdbIndexStat::CacheInfo infoBuild;
  NdbIndexStat::CacheInfo infoQuery;
  NdbIndexStat::CacheInfo infoClean;
  st->is->get_head(head);
  st->is->get_cache_info(infoBuild, NdbIndexStat::CacheBuild);
  st->is->get_cache_info(infoQuery, NdbIndexStat::CacheQuery);
  st->is->get_cache_info(infoClean, NdbIndexStat::CacheClean);

  DBUG_PRINT("index_stat", ("evict table: %u index: %u version: %u"
                            " sample version: %u"
                            " cache bytes build:%u query:%u clean:%u",
                            head.m_tableId, head.m_indexId, head.m_indexVersion,
                            head.m_sampleVersion, infoBuild.m_totalBytes,
                            infoQuery.m_totalBytes, infoClean.m_totalBytes));

  /* Twice to move all caches to clean */
  ndb_index_stat_cache_move(st);
  ndb_index_stat_cache_move(st);
  /* Unused variable release vs debug nonsense */
  bool ok = false;
  (void)ok;  // USED
  ok = ndb_index_stat_cache_clean(st);
  assert(ok);
}

/* Misc in/out parameters for process steps */
struct Ndb_index_stat_proc {
  NdbIndexStat *is_util;  // For metadata and polling
  Ndb *ndb;
  time_t start;  // start of current processing slice
  time_t now;
  int lt;
  bool busy;
  bool end;
#ifndef NDEBUG
  uint cache_query_bytes;
  uint cache_clean_bytes;
#endif
  Ndb_index_stat_proc()
      : is_util(nullptr),
        ndb(nullptr),
        now(0),
        lt(0),
        busy(false),
        end(false) {}

  ~Ndb_index_stat_proc() { assert(ndb == nullptr); }
};

static void ndb_index_stat_proc_new(Ndb_index_stat_proc &pr,
                                    Ndb_index_stat *st) {
  assert(st->error.code == 0);
  if (st->force_update)
    pr.lt = Ndb_index_stat::LT_Update;
  else
    pr.lt = Ndb_index_stat::LT_Read;
}

static void ndb_index_stat_proc_new(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_New];

  Ndb_index_stat *st_loop = list.head;
  while (st_loop != nullptr) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_new(pr, st);
    assert(pr.lt != Ndb_index_stat::LT_New);
    ndb_index_stat_list_move(st, pr.lt);
  }
  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_proc_update(Ndb_index_stat_proc &pr,
                                       Ndb_index_stat *st) {
  if (st->is->update_stat(pr.ndb) == -1) {
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    ndb_index_stat_error(st, 0, "update_stat");

    /*
      Turn off force update or else proc_error() thinks
      it is a new analyze request.
    */
    ndb_index_stat_force_update(st, false);

    /*
      If the index has an unsupported length,
      remove it from the list and stop monitoring
    */
    if (st->is->getNdbError().code == NdbIndexStat::InvalidKeySize) {
      ndb_index_stat_free(st);
    }

    mysql_cond_broadcast(&ndb_index_stat_thread.stat_cond);
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);

    pr.lt = Ndb_index_stat::LT_Error;
    return;
  }

  pr.now = ndb_index_stat_time();
  st->update_time = pr.now;
  pr.lt = Ndb_index_stat::LT_Read;
}

static void ndb_index_stat_proc_update(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_Update];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const uint batch = opt.get(Ndb_index_stat_opt::Iupdate_batch);

  Ndb_index_stat *st_loop = list.head;
  uint cnt = 0;
  while (st_loop != nullptr && cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_update(pr, st);
    assert(pr.lt != Ndb_index_stat::LT_Update);
    ndb_index_stat_list_move(st, pr.lt);
    // db op so update status after each
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    glob.set_status();
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
    cnt++;
  }
  if (cnt == batch) pr.busy = true;
}

static void ndb_index_stat_proc_read(Ndb_index_stat_proc &pr,
                                     Ndb_index_stat *st) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  NdbIndexStat::Head head;
  if (st->is->read_stat(pr.ndb) == -1) {
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    ndb_index_stat_error(st, 0, "read_stat");
    const bool force_update = st->force_update;
    ndb_index_stat_force_update(st, false);

    /* no stats is not unexpected error, unless analyze was done */
    if (st->is->getNdbError().code == NdbIndexStat::NoIndexStats &&
        !force_update) {
      ndb_index_stat_no_stats(st, true);
      pr.lt = Ndb_index_stat::LT_Idle;
    } else {
      pr.lt = Ndb_index_stat::LT_Error;
    }

    mysql_cond_broadcast(&ndb_index_stat_thread.stat_cond);
    pr.now = ndb_index_stat_time();
    st->check_time = pr.now;
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
    return;
  }

  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  pr.now = ndb_index_stat_time();
  st->is->get_head(head);
  st->load_time = (time_t)head.m_loadTime;
  st->read_time = pr.now;
  st->sample_version = head.m_sampleVersion;
  st->check_time = pr.now;

  ndb_index_stat_force_update(st, false);
  ndb_index_stat_no_stats(st, false);

  ndb_index_stat_cache_move(st);
  pr.lt = Ndb_index_stat::LT_Idle;
  glob.refresh_count++;
  mysql_cond_broadcast(&ndb_index_stat_thread.stat_cond);
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_proc_read(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_Read];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const uint batch = opt.get(Ndb_index_stat_opt::Iread_batch);

  Ndb_index_stat *st_loop = list.head;
  uint cnt = 0;
  while (st_loop != nullptr && cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_read(pr, st);
    assert(pr.lt != Ndb_index_stat::LT_Read);
    ndb_index_stat_list_move(st, pr.lt);
    // db op so update status after each
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    glob.set_status();
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
    cnt++;
  }
  if (cnt == batch) pr.busy = true;
}

static void ndb_index_stat_proc_idle(Ndb_index_stat_proc &pr,
                                     Ndb_index_stat *st) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const longlong clean_delay = opt.get(Ndb_index_stat_opt::Iclean_delay);
  const longlong check_delay = opt.get(Ndb_index_stat_opt::Icheck_delay);

  const longlong pr_now = (longlong)pr.now;
  const longlong st_read_time = (longlong)st->read_time;
  const longlong st_check_time = (longlong)st->check_time;

  const longlong clean_wait = st_read_time + clean_delay - pr_now;
  const longlong check_wait = st_check_time + check_delay - pr_now;

  DBUG_PRINT("index_stat",
             ("st %s clean_wait:%lld check_wait:%lld"
              " force_update:%d to_delete:%d",
              st->id, clean_wait, check_wait, st->force_update, st->to_delete));

  if (st->to_delete) {
    pr.lt = Ndb_index_stat::LT_Delete;
    return;
  }

  if (st->clean_bytes != 0 && clean_wait <= 0) {
    if (ndb_index_stat_cache_clean(st))
      glob.clean_count++;
    else
      glob.pinned_count++;
  }
  if (st->force_update) {
    pr.lt = Ndb_index_stat::LT_Update;
    pr.busy = true;
    return;
  }
  if (check_wait <= 0) {
    // avoid creating "idle" entries on Check list
    const Ndb_index_stat_list &list_check =
        ndb_index_stat_list[Ndb_index_stat::LT_Check];
    const uint check_batch = opt.get(Ndb_index_stat_opt::Icheck_batch);
    if (list_check.count < check_batch) {
      pr.lt = Ndb_index_stat::LT_Check;
      return;
    }
  }
  pr.lt = Ndb_index_stat::LT_Idle;
}

static void ndb_index_stat_proc_idle(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_Idle];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  uint batch = opt.get(Ndb_index_stat_opt::Iidle_batch);
  {
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    const Ndb_index_stat_glob &glob = ndb_index_stat_glob;
    const Ndb_index_stat_list &list_update =
        ndb_index_stat_list[Ndb_index_stat::LT_Update];
    if (glob.force_update > list_update.count) {
      // probably there is a force update waiting on Idle list
      batch = ~(uint)0;
    }
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  }
  // entry may be moved to end of this list
  if (batch > list.count) batch = list.count;
  pr.now = ndb_index_stat_time();

  Ndb_index_stat *st_loop = list.head;
  uint cnt = 0;
  while (st_loop != nullptr && cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_idle(pr, st);
    // rotates list if entry remains LT_Idle
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  // full batch does not set pr.busy
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_proc_check(Ndb_index_stat_proc &pr,
                                      Ndb_index_stat *st) {
  pr.now = ndb_index_stat_time();
  st->check_time = pr.now;
  NdbIndexStat::Head head;
  if (st->is->read_head(pr.ndb) == -1) {
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    ndb_index_stat_error(st, 0, "read_head");
    /* no stats is not unexpected error */
    if (st->is->getNdbError().code == NdbIndexStat::NoIndexStats) {
      ndb_index_stat_no_stats(st, true);
      pr.lt = Ndb_index_stat::LT_Idle;
    } else {
      pr.lt = Ndb_index_stat::LT_Error;
    }
    mysql_cond_broadcast(&ndb_index_stat_thread.stat_cond);
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
    return;
  }
  st->is->get_head(head);
  const uint version_old = st->sample_version;
  const uint version_new = head.m_sampleVersion;
  if (version_old != version_new) {
    DBUG_PRINT("index_stat", ("st %s sample version old:%u new:%u", st->id,
                              version_old, version_new));
    pr.lt = Ndb_index_stat::LT_Read;
    return;
  }
  pr.lt = Ndb_index_stat::LT_Idle;
}

static void ndb_index_stat_proc_check(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_Check];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const uint batch = opt.get(Ndb_index_stat_opt::Icheck_batch);

  Ndb_index_stat *st_loop = list.head;
  uint cnt = 0;
  while (st_loop != nullptr && cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_check(pr, st);
    assert(pr.lt != Ndb_index_stat::LT_Check);
    ndb_index_stat_list_move(st, pr.lt);
    // db op so update status after each
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    glob.set_status();
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
    cnt++;
  }
  if (cnt == batch) pr.busy = true;
}

/* Check if need to evict more */
static bool ndb_index_stat_proc_evict() {
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  uint curr_size = glob.cache_query_bytes + glob.cache_clean_bytes;

  /* Subtract bytes already scheduled for evict */
  assert(curr_size >= glob.cache_evict_bytes);
  curr_size -= glob.cache_evict_bytes;

  const uint cache_lowpct = opt.get(Ndb_index_stat_opt::Icache_lowpct);
  const uint cache_limit = opt.get(Ndb_index_stat_opt::Icache_limit);
  if (100 * curr_size <= cache_lowpct * cache_limit) return false;
  return true;
}

/* Check if st1 is better or as good to evict than st2 */
static bool ndb_index_stat_evict(const Ndb_index_stat *st1,
                                 const Ndb_index_stat *st2) {
  if (st1->access_time < st2->access_time) return true;
  if (st1->access_time == st2->access_time &&
      st1->query_bytes + st1->clean_bytes >=
          st2->query_bytes + st2->clean_bytes)
    return true;
  return false;
}

static void ndb_index_stat_proc_evict(Ndb_index_stat_proc &pr, int lt) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const uint batch = opt.get(Ndb_index_stat_opt::Ievict_batch);
  const longlong evict_delay = opt.get(Ndb_index_stat_opt::Ievict_delay);
  pr.now = ndb_index_stat_time();
  const longlong pr_now = (longlong)pr.now;

  if (!ndb_index_stat_proc_evict()) return;

  /* Mutex entire routine (protect access_time) */
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);

  /* Create a LRU batch */
  Ndb_index_stat *st_lru_arr[ndb_index_stat_max_evict_batch + 1];
  uint st_lru_cnt = 0;
  Ndb_index_stat *st_loop = list.head;
  while (st_loop != nullptr && st_lru_cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    const longlong st_read_time = (longlong)st->read_time;
    if (st_read_time + evict_delay <= pr_now &&
        st->query_bytes + st->clean_bytes != 0 && !st->to_delete) {
      /* Insertion sort into the batch from the end */
      if (st_lru_cnt == 0)
        st_lru_arr[st_lru_cnt++] = st;
      else {
        uint i = st_lru_cnt;
        while (i != 0) {
          const Ndb_index_stat *st1 = st_lru_arr[i - 1];
          if (ndb_index_stat_evict(st1, st)) {
            /*
              The old entry at i-1 is preferred over st.
              Stop at first such entry.  Therefore entries
              after it (>= i) are less preferred than st.
            */
            break;
          }
          i--;
        }
        if (i < st_lru_cnt) {
          /*
            Some old entry is less preferred than st.  If this is
            true for all then i is 0 and st becomes new first entry.
            Otherwise st is inserted after i-1.  In both case entries
            >= i are shifted up.  The extra position at the end of
            st_lru_arr avoids a special case when the array is full.
          */
          uint j = st_lru_cnt;
          while (j > i) {
            st_lru_arr[j] = st_lru_arr[j - 1];
            j--;
          }
          st_lru_arr[i] = st;
          if (st_lru_cnt < batch) st_lru_cnt++;
        }
      }
    }
  }

#ifndef NDEBUG
  for (uint i = 0; i < st_lru_cnt; i++) {
    Ndb_index_stat *st1 = st_lru_arr[i];
    assert(!st1->to_delete && st1->share != nullptr);
    if (i + 1 < st_lru_cnt) {
      Ndb_index_stat *st2 = st_lru_arr[i + 1];
      assert(ndb_index_stat_evict(st1, st2));
    }
  }
#endif

  /* Process the LRU batch */
  uint cnt = 0;
  while (cnt < st_lru_cnt) {
    if (!ndb_index_stat_proc_evict()) break;

    Ndb_index_stat *st = st_lru_arr[cnt];
    DBUG_PRINT("index_stat", ("st %s proc evict %s", st->id, list.name));

    /* Entry may have requests.  Cache is evicted at delete. */
    ndb_index_stat_free(st);
    assert(st->evict_bytes == 0);
    st->evict_bytes = st->query_bytes + st->clean_bytes;
    glob.cache_evict_bytes += st->evict_bytes;
    cnt++;
  }
  if (cnt == batch) pr.busy = true;

  glob.evict_count += cnt;
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_proc_evict(Ndb_index_stat_proc &pr) {
  ndb_index_stat_proc_evict(pr, Ndb_index_stat::LT_Error);
  ndb_index_stat_proc_evict(pr, Ndb_index_stat::LT_Idle);
}

static void ndb_index_stat_proc_delete(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_Delete];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const uint delete_batch = opt.get(Ndb_index_stat_opt::Idelete_batch);
  const uint batch = !pr.end ? delete_batch : ~(uint)0;

  /* Mutex entire routine */
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);

  Ndb_index_stat *st_loop = list.head;
  uint cnt = 0;
  while (st_loop != nullptr && cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));

    // adjust global counters at drop
    ndb_index_stat_force_update(st, false);
    ndb_index_stat_no_stats(st, false);

    /*
      Do not wait for requests to terminate since this could
      risk stats thread hanging. Instead try again next time.
      Presumably clients will eventually notice abort_request.
    */
    if (st->has_client_ref()) {
      DBUG_PRINT("index_stat",
                 ("st %s proc %s: referenced by client", st->id, list.name));
      continue;
    }

    ndb_index_stat_cache_evict(st);
    assert(glob.cache_drop_bytes >= st->drop_bytes);
    glob.cache_drop_bytes -= st->drop_bytes;
    assert(glob.cache_evict_bytes >= st->evict_bytes);
    glob.cache_evict_bytes -= st->evict_bytes;
    ndb_index_stat_list_remove(st);
    delete st->is;
    delete st;
    cnt++;
  }
  if (cnt == batch) pr.busy = true;

  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_proc_error(Ndb_index_stat_proc &pr,
                                      Ndb_index_stat *st) {
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  const longlong error_delay = opt.get(Ndb_index_stat_opt::Ierror_delay);

  const longlong pr_now = (longlong)pr.now;
  const longlong st_error_time = (longlong)st->error_time;
  const longlong error_wait = st_error_time + error_delay - pr_now;

  DBUG_PRINT("index_stat", ("st %s error_wait:%lld error_count:%u"
                            " force_update:%d to_delete:%d",
                            st->id, error_wait, st->error_count,
                            st->force_update, st->to_delete));

  if (st->to_delete) {
    pr.lt = Ndb_index_stat::LT_Delete;
    return;
  }

  if (error_wait <= 0 ||
      /* Analyze issued after previous error */
      st->force_update) {
    ndb_index_stat_clear_error(st);
    if (st->force_update)
      pr.lt = Ndb_index_stat::LT_Update;
    else
      pr.lt = Ndb_index_stat::LT_Read;
    return;
  }
  pr.lt = Ndb_index_stat::LT_Error;
}

static void ndb_index_stat_proc_error(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_list &list = ndb_index_stat_list[Ndb_index_stat::LT_Error];
  const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
  uint batch = opt.get(Ndb_index_stat_opt::Ierror_batch);
  // entry may be moved to end of this list
  if (batch > list.count) batch = list.count;
  pr.now = ndb_index_stat_time();

  Ndb_index_stat *st_loop = list.head;
  uint cnt = 0;
  while (st_loop != nullptr && cnt < batch) {
    Ndb_index_stat *st = st_loop;
    st_loop = st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_error(pr, st);
    // rotates list if entry remains LT_Error
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  // full batch does not set pr.busy
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_proc_event(Ndb_index_stat_proc &pr,
                                      Ndb_index_stat *st) {
  /*
    Put on Check list if idle.
    We get event also for our own analyze but this should not matter.

    bug#13524696
    The useless event-to-self makes an immediate second analyze wait
    for loop_idle time since the entry moves to LT_Check temporarily.
    Ignore the event if an update was done near this processing slice.
   */
  pr.lt = st->array_index;
  if (st->array_index == Ndb_index_stat::LT_Idle ||
      st->array_index == Ndb_index_stat::LT_Error) {
    if (st->update_time < pr.start) {
      DBUG_PRINT("index_stat", ("st %s accept event for check", st->id));
      pr.lt = Ndb_index_stat::LT_Check;
    } else {
      DBUG_PRINT("index_stat", ("st %s ignore likely event to self", st->id));
    }
  } else {
    DBUG_PRINT("index_stat", ("st %s ignore event on array_index=%d", st->id,
                              st->array_index));
  }
}

static void ndb_index_stat_proc_event(Ndb_index_stat_proc &pr) {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  NdbIndexStat *is = pr.is_util;
  Ndb *ndb = pr.ndb;
  int ret;
  ret = is->poll_listener(ndb, 0);
  DBUG_PRINT("index_stat", ("poll_listener ret: %d", ret));
  if (ret == -1) {
    // wl4124_todo report error
    assert(false);
    return;
  }
  if (ret == 0) return;

  while (1) {
    ret = is->next_listener(ndb);
    DBUG_PRINT("index_stat", ("next_listener ret: %d", ret));
    if (ret == -1) {
      // wl4124_todo report error
      assert(false);
      return;
    }
    if (ret == 0) break;

    NdbIndexStat::Head head;
    is->get_head(head);
    DBUG_PRINT("index_stat", ("next_listener eventType: %d indexId: %u",
                              head.m_eventType, head.m_indexId));

    if (head.m_eventType == 4) {
      // Event that denotes that the stats have been updated in the kernel
      g_ndb_status_index_stat_event_count++;
      DBUG_PRINT("index_stat", ("Incremented stat_event_count to %ld",
                                g_ndb_status_index_stat_event_count));
    }
    Ndb_index_stat *st = find_entry(head.m_indexId, head.m_indexVersion);
    /*
      Another process can update stats for an index which is not found
      in this mysqld.  Ignore it.
     */
    if (st != nullptr) {
      DBUG_PRINT("index_stat", ("st %s proc %s", st->id, "event"));
      ndb_index_stat_proc_event(pr, st);
      if (pr.lt != st->array_index) {
        ndb_index_stat_list_move(st, pr.lt);
        glob.event_act++;
      } else
        glob.event_skip++;
    } else {
      DBUG_PRINT("index_stat", ("entry not found in this mysqld"));
      glob.event_miss++;
    }
  }
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  glob.set_status();
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

/* Control options */

static void ndb_index_stat_proc_control() {
  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_opt &opt = ndb_index_stat_opt;

  /* Request to zero accumulating counters */
  if (opt.get(Ndb_index_stat_opt::Izero_total) == 1) {
    mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
    glob.zero_total();
    glob.set_status();
    opt.set(Ndb_index_stat_opt::Izero_total, 0);
    mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  }
}

#ifndef NDEBUG
static void ndb_index_stat_entry_verify(Ndb_index_stat_proc &pr,
                                        const Ndb_index_stat *st) {
  const NDB_SHARE *share = st->share;
  if (st->to_delete) {
    assert(st->share_next == nullptr);
    assert(share == nullptr);
  } else {
    assert(share != nullptr);
    const Ndb_index_stat *st2 = share->index_stat_list;
    assert(st2 != nullptr);
    uint found = 0;
    while (st2 != nullptr) {
      assert(st2->share == share);
      const Ndb_index_stat *st3 = st2->share_next;
      uint guard = 0;
      while (st3 != nullptr) {
        assert(st2 != st3);
        guard++;
        assert(guard <= 1000);  // MAX_INDEXES
        st3 = st3->share_next;
      }
      if (st == st2) found++;
      st2 = st2->share_next;
    }
    assert(found == 1);
  }
  assert(st->read_time <= st->check_time);
  pr.cache_query_bytes += st->query_bytes;
  pr.cache_clean_bytes += st->clean_bytes;
}

static void ndb_index_stat_list_verify(Ndb_index_stat_proc &pr, int lt) {
  const Ndb_index_stat_list &list = ndb_index_stat_list[lt];
  const Ndb_index_stat *st = list.head;
  uint count = 0;
  while (st != nullptr) {
    count++;
    assert(count <= list.count);
    if (st->list_prev != nullptr) {
      assert(st->list_prev->list_next == st);
    }
    if (st->list_next != nullptr) {
      assert(st->list_next->list_prev == st);
    }
    if (count == 1) {
      assert(st == list.head);
    }
    if (count == list.count) {
      assert(st == list.tail);
    }
    if (st == list.head) {
      assert(count == 1);
      assert(st->list_prev == nullptr);
    }
    if (st == list.tail) {
      assert(count == list.count);
      assert(st->list_next == nullptr);
    }
    const Ndb_index_stat *st2 = st->list_next;
    uint guard = 0;
    while (st2 != nullptr) {
      assert(st != st2);
      guard++;
      assert(guard <= list.count);
      st2 = st2->list_next;
    }
    ndb_index_stat_entry_verify(pr, st);
    st = st->list_next;
  }
  assert(count == list.count);
}

static void ndb_index_stat_list_verify(Ndb_index_stat_proc &pr) {
  const Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  pr.cache_query_bytes = 0;
  pr.cache_clean_bytes = 0;

  for (int lt = Ndb_index_stat::LT_New; lt < Ndb_index_stat::LT_Count; lt++)
    ndb_index_stat_list_verify(pr, lt);

  assert(glob.cache_query_bytes == pr.cache_query_bytes);
  assert(glob.cache_clean_bytes == pr.cache_clean_bytes);
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
}

static void ndb_index_stat_report(const Ndb_index_stat_glob &old_glob) {
  const Ndb_index_stat_glob &new_glob = ndb_index_stat_glob;
  const char *old_status = old_glob.status[old_glob.status_i];
  const char *new_status = new_glob.status[new_glob.status_i];

  if (strcmp(old_status, new_status) != 0) {
    DBUG_PRINT("index_stat", ("old_status: %s", old_status));
    DBUG_PRINT("index_stat", ("new_status: %s", new_status));
  }
}
#endif

static void ndb_index_stat_proc(Ndb_index_stat_proc &pr) {
  DBUG_TRACE;

  ndb_index_stat_proc_control();

#ifndef NDEBUG
  ndb_index_stat_list_verify(pr);
  Ndb_index_stat_glob old_glob = ndb_index_stat_glob;
#endif

  pr.start = pr.now = ndb_index_stat_time();

  ndb_index_stat_proc_new(pr);
  ndb_index_stat_proc_update(pr);
  ndb_index_stat_proc_read(pr);
  ndb_index_stat_proc_idle(pr);
  ndb_index_stat_proc_check(pr);
  ndb_index_stat_proc_evict(pr);
  ndb_index_stat_proc_delete(pr);
  ndb_index_stat_proc_error(pr);
  ndb_index_stat_proc_event(pr);

#ifndef NDEBUG
  ndb_index_stat_list_verify(pr);
  ndb_index_stat_report(old_glob);
#endif
}

/*
  Runs after stats thread exits and needs no locks.
*/
void ndb_index_stat_end() {
  DBUG_TRACE;
  Ndb_index_stat_proc pr;
  pr.end = true;

  /*
   * Shares have been freed so any index stat entries left should be
   * in LT_Delete.  The first two steps here should be unnecessary.
   */
  for (int lt = Ndb_index_stat::LT_New; lt < Ndb_index_stat::LT_Count; lt++) {
    if (lt == Ndb_index_stat::LT_Delete) continue;
    Ndb_index_stat_list &list = ndb_index_stat_list[lt];
    Ndb_index_stat *st_loop = list.head;
    while (st_loop != nullptr) {
      Ndb_index_stat *st = st_loop;
      st_loop = st_loop->list_next;
      DBUG_PRINT("index_stat", ("st %s end %s", st->id, list.name));
      pr.lt = Ndb_index_stat::LT_Delete;
      ndb_index_stat_list_move(st, pr.lt);
    }
  }

  /* Real free */
  ndb_index_stat_proc_delete(pr);
}

/* Index stats thread */

int Ndb_index_stat_thread::check_systables(
    const Ndb_index_stat_proc &pr) const {
  DBUG_TRACE;

  if (pr.is_util->check_systables(pr.ndb) == 0) {
    DBUG_PRINT("index_stat", ("using existing index stats tables"));
    return 0;
  }
  return -1;
}

int Ndb_index_stat_thread::check_sysevents(
    const Ndb_index_stat_proc &pr) const {
  DBUG_TRACE;

  if (pr.is_util->check_sysevents(pr.ndb) == 0) {
    DBUG_PRINT("index_stat", ("using existing index stats events"));
    return 0;
  }
  return -1;
}

int Ndb_index_stat_thread::create_ndb(
    Ndb_index_stat_proc *const pr,
    Ndb_cluster_connection *const connection) const {
  DBUG_TRACE;
  assert(pr->ndb == nullptr);
  assert(connection != nullptr);

  pr->ndb = new (std::nothrow) Ndb(connection, NDB_INDEX_STAT_DB);
  if (pr->ndb == nullptr) {
    log_error("Failed to create Ndb object");
    return -1;
  }

  if (pr->ndb->setNdbObjectName("Ndb Index Stat")) {
    log_error("Failed to set Ndb object name. Error = %d: %s",
              pr->ndb->getNdbError().code, pr->ndb->getNdbError().message);
    delete pr->ndb;
    pr->ndb = nullptr;
    return -1;
  }

  if (pr->ndb->init() != 0) {
    log_error("Failed to init Ndb. Error = %d:%s", pr->ndb->getNdbError().code,
              pr->ndb->getNdbError().message);
    delete pr->ndb;
    pr->ndb = nullptr;
    return -1;
  }

  log_info("Created Ndb object '%s', ref: 0x%x", pr->ndb->getNdbObjectName(),
           pr->ndb->getReference());
  return 0;
}

void Ndb_index_stat_thread::drop_ndb(Ndb_index_stat_proc *const pr) const {
  DBUG_TRACE;

  if (pr->is_util->has_listener()) {
    stop_listener(*pr);
  }
  if (pr->ndb != nullptr) {
    delete pr->ndb;
    pr->ndb = nullptr;
  }
}

int Ndb_index_stat_thread::start_listener(const Ndb_index_stat_proc &pr) const {
  DBUG_TRACE;

  NdbIndexStat *is = pr.is_util;
  Ndb *ndb = pr.ndb;

  if (is->create_listener(ndb) == -1) {
    log_info("create index stats listener failed: error %d line %d",
             is->getNdbError().code, is->getNdbError().line);
    return -1;
  }

  if (is->execute_listener(ndb) == -1) {
    log_info("execute index stats listener failed: error %d line %d",
             is->getNdbError().code, is->getNdbError().line);
    // Drop the created listener
    (void)is->drop_listener(ndb);
    return -1;
  }

  return 0;
}

void Ndb_index_stat_thread::stop_listener(const Ndb_index_stat_proc &pr) const {
  DBUG_TRACE;
  (void)pr.is_util->drop_listener(pr.ndb);
}

/* Restart things after system restart */

static bool ndb_index_stat_restart_flag = false;

void ndb_index_stat_restart() {
  DBUG_TRACE;
  ndb_index_stat_restart_flag = true;
  ndb_index_stat_set_allow(false);
}

bool Ndb_index_stat_thread::is_setup_complete() {
  if (ndb_index_stat_get_enable(nullptr)) {
    return ndb_index_stat_get_allow();
  }
  return true;
}

extern Ndb_cluster_connection *g_ndb_cluster_connection;

void Ndb_index_stat_thread::do_run() {
  DBUG_TRACE;

  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  Ndb_index_stat_proc pr;

  log_info("Starting...");

  if (!wait_for_server_started()) {
    mysql_mutex_lock(&LOCK_client_waiting);
    goto ndb_index_stat_thread_end;
  }

  log_verbose(1, "Wait for cluster to start");
  /*
    Wait for cluster to start
  */
  while (!ndb_connection_is_ready(g_ndb_cluster_connection, 1)) {
    /* NDB not connected yet */
    if (is_stop_requested()) {
      /* Terminated with a stop_request */
      mysql_mutex_lock(&LOCK_client_waiting);
      goto ndb_index_stat_thread_end;
    }
  }

  /* Get instance used for sys objects check and create */
  if (!(pr.is_util = new NdbIndexStat)) {
    log_error("Could not allocate NdbIndexStat is_util object");
    mysql_mutex_lock(&LOCK_client_waiting);
    goto ndb_index_stat_thread_end;
  }

  /* Fill in initial status variable */
  mysql_mutex_lock(&stat_mutex);
  glob.set_status();
  mysql_mutex_unlock(&stat_mutex);

  log_info("Started");

  bool enable_ok;
  enable_ok = false;

  // Flag used to indicate if there's a need to check for creation of index
  // stat tables and events
  bool check_sys;
  check_sys = true;

  struct timespec abstime;
  set_timespec(&abstime, 0);
  for (;;) {
    mysql_mutex_lock(&LOCK_client_waiting);
    if (client_waiting == false) {
      const int ret = mysql_cond_timedwait(&COND_client_waiting,
                                           &LOCK_client_waiting, &abstime);
      if (ret == ETIMEDOUT)
        DBUG_PRINT("index_stat", ("loop: timed out"));
      else
        DBUG_PRINT("index_stat", ("loop: wake up"));
    }
    client_waiting = false;
    mysql_mutex_unlock(&LOCK_client_waiting);

    if (is_stop_requested()) /* Shutting down server */
    {
      mysql_mutex_lock(&LOCK_client_waiting);
      goto ndb_index_stat_thread_end;
    }

    /*
     * Next processing slice.  Each time we check that global enable
     * flag is on and that required objects have been found or can be
     * created.  If not, drop out and try again next time.
     *
     * It is allowed to do initial restart of cluster while we are
     * running. In such cases, the listener must be restarted for the event
     * functionality to work correctly
     */
    do {
      // An initial restart may have occurred while this mysqld was left running
      if (ndb_index_stat_restart_flag) {
        ndb_index_stat_restart_flag = false;
        ndb_index_stat_set_allow(false);
        // Stop the listener thus enforcing that it's started again further
        // down in the loop
        if (pr.is_util->has_listener()) stop_listener(pr);
        check_sys = true;  // check if sys objects are gone
        log_info("Initial restart detected");
      }

      // check enable flag
      {
        const bool enable_ok_new = ndb_index_stat_get_enable(nullptr);

        if (enable_ok != enable_ok_new) {
          DBUG_PRINT("index_stat",
                     ("global enable: %d -> %d", enable_ok, enable_ok_new));
          enable_ok = enable_ok_new;
          check_sys = enable_ok;  // check sys objects if enabling
        }
      }

      if (!enable_ok) {
        DBUG_PRINT("index_stat", ("Index stats is not enabled"));
        ndb_index_stat_set_allow(false);
        drop_ndb(&pr);
        break;
      }

      // the Ndb object is needed first
      if (pr.ndb == nullptr) {
        if (create_ndb(&pr, g_ndb_cluster_connection) == -1) break;
      }

      // sys objects
      if (check_sys) {
        // at enable check or create stats tables and events
        if (check_systables(pr) == -1 || check_sysevents(pr) == -1) break;
      }

      // listener is not critical but error means something is wrong
      if (!pr.is_util->has_listener()) {
        if (start_listener(pr) == -1) break;
      }

      // normal processing
      check_sys = false;
      ndb_index_stat_set_allow(true);
      pr.busy = false;
      ndb_index_stat_proc(pr);
    } while (0);

    /* Calculate new time to wake up */

    const Ndb_index_stat_opt &opt = ndb_index_stat_opt;
    uint msecs = 0;
    if (!enable_ok)
      msecs = opt.get(Ndb_index_stat_opt::Iloop_enable);
    else if (!pr.busy)
      msecs = opt.get(Ndb_index_stat_opt::Iloop_idle);
    else
      msecs = opt.get(Ndb_index_stat_opt::Iloop_busy);
    DBUG_PRINT("index_stat", ("sleep %dms", msecs));

    set_timespec_nsec(&abstime, msecs * 1000000ULL);

    /* Update status variable */
    glob.th_enable = enable_ok;
    glob.th_busy = pr.busy;
    glob.th_loop = msecs;
    mysql_mutex_lock(&stat_mutex);
    glob.set_status();
    mysql_mutex_unlock(&stat_mutex);
  }

ndb_index_stat_thread_end:
  log_info("Stopping...");

  /* Prevent clients */
  ndb_index_stat_set_allow(false);

  if (pr.is_util) {
    drop_ndb(&pr);
    delete pr.is_util;
    pr.is_util = nullptr;
  }

  mysql_mutex_unlock(&LOCK_client_waiting);
  DBUG_PRINT("exit", ("ndb_index_stat_thread"));

  log_info("Stopped");
}

/* Optimizer queries */

static ulonglong ndb_index_stat_round(double x) {
  char buf[100];
  if (x < 0.0) x = 0.0;
  snprintf(buf, sizeof(buf), "%.0f", x);
  /* mysql provides my_strtoull */
  ulonglong n = my_strtoull(buf, nullptr, 10);
  return n;
}

/*
  Client waits for query or analyze.  The routines are
  similar but separated for clarity.
*/

static int ndb_index_stat_wait_query(Ndb_index_stat *st,
                                     const Ndb_index_stat_snap &snap) {
  DBUG_TRACE;

  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  int err = 0;
  uint count [[maybe_unused]] = 0;
  struct timespec abstime;
  glob.wait_stats++;
  glob.query_count++;
  while (true) {
    int ret = 0;
    /* Query waits for any samples */
    if (st->sample_version > 0) break;
    if (st->no_stats) {
      /* Have detected no stats now or before */
      err = NdbIndexStat::NoIndexStats;
      glob.query_no_stats++;
      break;
    }
    if (st->error.code != 0) {
      /* An error has occured now or before */
      err = NdbIndexStat::MyHasError;
      glob.query_error++;
      break;
    }
    /*
      Try to detect changes behind our backs.  Should really not
      happen but make sure.
    */
    if (st->load_time != snap.load_time ||
        st->sample_version != snap.sample_version) {
      assert(false);
      err = NdbIndexStat::NoIndexStats;
      break;
    }
    if (st->abort_request) {
      err = NdbIndexStat::MyAbortReq;
      break;
    }
    count++;
    DBUG_PRINT("index_stat", ("st %s wait_query count:%u", st->id, count));
    ndb_index_stat_thread.wakeup();

    set_timespec(&abstime, 1);
    ret = mysql_cond_timedwait(&ndb_index_stat_thread.stat_cond,
                               &ndb_index_stat_thread.stat_mutex, &abstime);
    if (ret != 0 && ret != ETIMEDOUT) {
      err = ret;
      break;
    }
  }
  assert(glob.wait_stats != 0);
  glob.wait_stats--;
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  if (err != 0) {
    DBUG_PRINT("index_stat", ("st %s wait_query error: %d", st->id, err));
    return err;
  }
  DBUG_PRINT("index_stat", ("st %s wait_query ok: sample_version %u -> %u",
                            st->id, snap.sample_version, st->sample_version));
  return 0;
}

static int ndb_index_stat_wait_analyze(Ndb_index_stat *st,
                                       const Ndb_index_stat_snap &snap) {
  DBUG_TRACE;

  Ndb_index_stat_glob &glob = ndb_index_stat_glob;
  mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
  int err = 0;
  uint count [[maybe_unused]] = 0;
  struct timespec abstime;
  glob.wait_update++;
  glob.analyze_count++;
  while (true) {
    int ret = 0;
    /* Analyze waits for newer samples */
    if (st->sample_version > snap.sample_version) break;
    if (st->error_count != snap.error_count) {
      /* A new error has occurred */
      assert(st->error_count > snap.error_count);
      err = st->error.code;
      glob.analyze_error++;
      break;
    }
    /*
      Try to detect changes behind our backs.  If another process
      deleted stats, an analyze here could wait forever.
    */
    if (st->load_time != snap.load_time ||
        st->sample_version != snap.sample_version) {
      assert(false);
      err = NdbIndexStat::AlienUpdate;
      break;
    }
    if (st->abort_request) {
      err = NdbIndexStat::MyAbortReq;
      break;
    }
    if (!st->force_update || glob.wait_update == 0) {
      /**
       * If there is somehow nothing happening and
       * nothing to wait for, then it is an error to wait any
       * longer.
       */
      fprintf(stderr,
              "ndb_index_stat_wait_analyze idx %u st->force_update %u "
              "glob.wait_update %u status : %s\n",
              st->index_id, st->force_update, glob.wait_update,
              g_ndb_status_index_stat_status);
      err = NdbIndexStat::InternalError;
      break;
    }
    count++;
    DBUG_PRINT("index_stat", ("st %s wait_analyze count:%u", st->id, count));
    ndb_index_stat_thread.wakeup();

    set_timespec(&abstime, 1);
    ret = mysql_cond_timedwait(&ndb_index_stat_thread.stat_cond,
                               &ndb_index_stat_thread.stat_mutex, &abstime);
    if (ret != 0 && ret != ETIMEDOUT) {
      err = ret;
      break;
    }
  }
  assert(glob.wait_update != 0);
  glob.wait_update--;
  mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
  if (err != 0) {
    DBUG_PRINT("index_stat", ("st %s wait_analyze error: %d", st->id, err));
    return err;
  }
  DBUG_PRINT("index_stat", ("st %s wait_analyze ok: sample_version %u -> %u",
                            st->id, snap.sample_version, st->sample_version));
  return 0;
}

void compute_index_bounds(NdbIndexScanOperation::IndexBound &bound,
                          const KEY *key_info, const key_range *start_key,
                          const key_range *end_key, int from);

int ha_ndbcluster::ndb_index_stat_query(uint inx, const key_range *min_key,
                                        const key_range *max_key,
                                        NdbIndexStat::Stat &stat, int from) {
  DBUG_TRACE;

  const KEY *key_info = table->key_info + inx;
  const NDB_INDEX_DATA &data = m_index[inx];
  const NDBINDEX *index = data.index;
  DBUG_PRINT("index_stat", ("index: %u name: %s", inx, index->getName()));

  int err = 0;

  /* Create an IndexBound struct for the keys */
  NdbIndexScanOperation::IndexBound ib;
  compute_index_bounds(ib, key_info, min_key, max_key, from);
  ib.range_no = 0;

  Ndb_index_stat_snap snap;
  Ndb_index_stat *st =
      ndb_index_stat_get_share(m_share, index, m_table, snap, err, true, false);
  if (st == nullptr) return err;
  /* Now holding reference to st */

  do {
    err = ndb_index_stat_wait_query(st, snap);
    if (err != 0) break;
    assert(st->sample_version != 0);
    uint8 bound_lo_buffer[NdbIndexStat::BoundBufferBytes];
    uint8 bound_hi_buffer[NdbIndexStat::BoundBufferBytes];
    NdbIndexStat::Bound bound_lo(st->is, bound_lo_buffer);
    NdbIndexStat::Bound bound_hi(st->is, bound_hi_buffer);
    NdbIndexStat::Range range(bound_lo, bound_hi);

    const NdbRecord *key_record = data.ndb_record_key;
    if (st->is->convert_range(range, key_record, &ib) == -1) {
      mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
      ndb_index_stat_error(st, 1, "convert_range");
      err = st->client_error.code;
      mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
      break;
    }
    if (st->is->query_stat(range, stat) == -1) {
      /* Invalid cache - should remove the entry */
      mysql_mutex_lock(&ndb_index_stat_thread.stat_mutex);
      ndb_index_stat_error(st, 1, "query_stat");
      err = st->client_error.code;
      mysql_mutex_unlock(&ndb_index_stat_thread.stat_mutex);
      break;
    }
  } while (0);

  /* Release reference to st */
  st->release_client_ref();

  return err;
}

int ha_ndbcluster::ndb_index_stat_get_rir(uint inx, key_range *min_key,
                                          key_range *max_key,
                                          ha_rows *rows_out) {
  DBUG_TRACE;
  uint8 stat_buffer[NdbIndexStat::StatBufferBytes];
  NdbIndexStat::Stat stat(stat_buffer);
  int err = ndb_index_stat_query(inx, min_key, max_key, stat, 1);
  if (err == 0) {
    /*
     * TODO: 'Rows in range' estimates will be inaccurate for
     * 'pruned-scan' ranges. Need to be solved in a way similar to
     * ::ndb_index_stat_set_rpk()
     */
    const Uint32 fragments = m_table->getFragmentCount();

    /**
     * Check quality of index_stat before it is used to set RPK.
     * There might have been too much update activity on the table,
     * not yet reflected by the statistics, or the single fragment sample
     * being too skeewed such that it does not represent the real data.
     */
    if (stats.records / fragments <= 1) {
      // Too few rows for a single fragment sample to be useful at all
      DBUG_PRINT("index_stat",
                 ("Too few rows in: %s", m_index[inx].index->getName()));
      return NdbIndexStat::NoIndexStats;
    }
    Uint32 rows_in_sample;
    NdbIndexStat::get_numrows(stat, &rows_in_sample);
    const ha_rows estm_rows = rows_in_sample * fragments;
    if (estm_rows * 2 < stats.records || estm_rows / 2 > stats.records) {
      /**
       * Number of estimated rows in statistics deviated too much from
       * what we have recorded on the table stats level. Thus we choose
       * to not use it, handle it as 'NoIndexStats'.
       */
      DBUG_PRINT("index_stat",
                 ("Ignored outdated statistics: %s"
                  ", estm_rows:%llu, records:%llu",
                  m_index[inx].index->getName(), estm_rows, stats.records));
      return NdbIndexStat::NoIndexStats;
    }
    double rir = -1.0;
    NdbIndexStat::get_rir(stat, &rir);
    ha_rows rows = ndb_index_stat_round(rir);
    /* Estimate only so cannot return exact zero */
    if (rows == 0) rows = 1;
    *rows_out = rows;
#ifndef NDEBUG
    char rule[NdbIndexStat::RuleBufferBytes];
    NdbIndexStat::get_rule(stat, rule);
#endif
    DBUG_PRINT("index_stat", ("rir: %u rule: %s", (uint)rows, rule));
    return 0;
  }
  return err;
}

int ha_ndbcluster::ndb_index_stat_set_rpk(uint inx) {
  DBUG_TRACE;

  uint8 stat_buffer[NdbIndexStat::StatBufferBytes];
  NdbIndexStat::Stat stat(stat_buffer);
  const key_range *min_key = nullptr;
  const key_range *max_key = nullptr;
  const int err = ndb_index_stat_query(inx, min_key, max_key, stat, 2);
  if (err == 0) {
    Uint32 rows_in_sample;
    NdbIndexStat::get_numrows(stat, &rows_in_sample);
    const Uint32 fragments = m_table->getFragmentCount();
    const ha_rows estm_rows = rows_in_sample * fragments;

    /**
     * Check quality of index_stat before it is used to get RPK.
     * There might have been too much update activity on the table,
     * not yet reflected by the statistics, or the single fragment sample
     * being too skeewed such that it does not represent the real data.
     */
    if (stats.records / fragments <= 1) {
      // Too few rows for a single fragment sample to be useful at all
      DBUG_PRINT("index_stat",
                 ("Too few rows in: %s", m_index[inx].index->getName()));
      return NdbIndexStat::NoIndexStats;
    }
    if (estm_rows * 2 < stats.records || estm_rows / 2 > stats.records) {
      /**
       * Number of estimated rows in statistics deviated too much from
       * what we have recorded on the table stats level. Thus we choose
       * to not use it, handle it as 'NoIndexStats'.
       */
      DBUG_PRINT("index_stat",
                 ("Ignored outdated statistics: %s"
                  ", estm_rows:%llu, records:%llu",
                  m_index[inx].index->getName(), estm_rows, stats.records));
      return NdbIndexStat::NoIndexStats;
    }
    KEY *key_info = table->key_info + inx;
    const KEY_PART_INFO *key_part_info = key_info->key_part;
    const uint num_part_fields =
        bitmap_bits_set(&m_part_info->full_part_field_set);
    uint num_part_fields_found = 0;
    for (uint k = 0; k < key_info->user_defined_key_parts; k++) {
      double rpk = REC_PER_KEY_UNKNOWN;  // unknown -> -1.0
      const uint field_index = key_part_info[k].field->field_index();
      if (bitmap_is_set(&m_part_info->full_part_field_set, field_index)) {
        num_part_fields_found++;
      }
      if ((k == (key_info->user_defined_key_parts - 1)) &&
          (get_index_type(inx) == UNIQUE_ORDERED_INDEX ||
           get_index_type(inx) == PRIMARY_KEY_ORDERED_INDEX)) {
        /**
         * All key fields in a UQ/PK are specified. No need to consult
         * index stat to know that only a single row will be returned.
         */
        rpk = 1.0;
      } else {
        if (num_part_fields_found >= num_part_fields) {
          /**
           * The records per key calculation assumes independence between
           * distribution of data and key columns. This is true as long as
           * the key parts don't set the entire partition key. In this case
           * the records per key as calculated by one fragment is the
           * records per key also for the entire table since different
           * fragments will have its own set of unique key values in this
           * case. For more information on this see NdbIndexStatImpl.cpp
           * and the method iterative_solution and get_unp_factor.
           */
          assert(num_part_fields_found == num_part_fields);
          NdbIndexStat::get_rpk_pruned(stat, k, &rpk);
        } else {
          NdbIndexStat::get_rpk(stat, k, &rpk);
        }
      }
      if (rpk != REC_PER_KEY_UNKNOWN) {
        key_info->set_records_per_key(k, static_cast<rec_per_key_t>(rpk));
      }
#ifndef NDEBUG
      char rule[NdbIndexStat::RuleBufferBytes];
      NdbIndexStat::get_rule(stat, rule);
#endif
      DBUG_PRINT("index_stat", ("rpk[%u]: %f rule: %s", k, rpk, rule));
    }
    return 0;
  }
  return err;
}

int ha_ndbcluster::ndb_index_stat_analyze(uint *inx_list, uint inx_count) {
  DBUG_TRACE;

  struct Req {
    Ndb_index_stat *st;
    Ndb_index_stat_snap snap;
    int err;
    Req() {
      st = nullptr;
      err = 0;
    }
  };
  Req req[MAX_INDEXES];

  /* Force stats update on each index */
  for (uint i = 0; i < inx_count; i++) {
    Req &r = req[i];
    uint inx = inx_list[i];
    const NDB_INDEX_DATA &data = m_index[inx];
    const NDBINDEX *index = data.index;
    DBUG_PRINT("index_stat", ("force update: %s", index->getName()));

    r.st = ndb_index_stat_get_share(m_share, index, m_table, r.snap, r.err,
                                    true, true);
    assert((r.st != nullptr) == (r.err == 0));
    /* Now holding reference to r.st if r.err == 0 */
  }

  /* Wait for each update */
  for (uint i = 0; i < inx_count; i++) {
    Req &r = req[i];
    uint inx = inx_list[i];
    const NDB_INDEX_DATA &data = m_index[inx];
    const NDBINDEX *index = data.index;
    (void)index;  // USED

    if (r.err == 0) {
      DBUG_PRINT("index_stat", ("wait for update: %s", index->getName()));
      r.err = ndb_index_stat_wait_analyze(r.st, r.snap);
      /* Release reference to r.st */
      r.st->release_client_ref();
    }
  }

  /* Return first error if any */
  int err = 0;
  for (uint i = 0; i < inx_count; i++) {
    Req &r = req[i];
    if (r.err != 0) {
      err = r.err;
      break;
    }
  }

  return err;
}

static SHOW_VAR ndb_status_vars_index_stat[] = {
    {"status", (char *)&g_ndb_status_index_stat_status, SHOW_CHAR_PTR,
     SHOW_SCOPE_GLOBAL},
    {"cache_query", (char *)&g_ndb_status_index_stat_cache_query, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"cache_clean", (char *)&g_ndb_status_index_stat_cache_clean, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"event_count", (char *)&g_ndb_status_index_stat_event_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

int show_ndb_status_index_stat(THD *, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = (char *)&ndb_status_vars_index_stat;
  return 0;
}
