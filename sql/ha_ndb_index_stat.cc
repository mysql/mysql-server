/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ha_ndbcluster_glue.h"

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE

#include "ha_ndbcluster.h"
#include "ha_ndb_index_stat.h"
#include <mysql/plugin.h>
#include <ctype.h>

// copied from ha_ndbcluster_binlog.h

extern handlerton *ndbcluster_hton;

inline
void
set_thd_ndb(THD *thd, Thd_ndb *thd_ndb)
{ thd_set_ha_data(thd, ndbcluster_hton, thd_ndb); }

// Typedefs for long names 
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Index NDBINDEX;

struct Ndb_index_stat {
  enum {
    LT_Undef= 0,
    LT_New= 1,          /* new entry added by a table handler */
    LT_Update = 2,      /* force kernel update from analyze table */
    LT_Read= 3,         /* read or reread stats into new query cache */
    LT_Idle= 4,         /* stats exist */
    LT_Check= 5,        /* check for new stats */
    LT_Delete= 6,       /* delete the entry */
    LT_Error= 7,        /* error, on hold for a while */
    LT_Count= 8
  };
  NdbIndexStat* is;
  int index_id;
  int index_version;
#ifndef DBUG_OFF
  char id[32];
#endif
  time_t access_time;   /* by any table handler */
  time_t load_time;     /* when stats were created by kernel */
  time_t read_time;     /* when stats were read by us (>= load_time) */
  uint sample_version;  /* goes with read_time */
  time_t check_time;    /* when checked for updated stats (>= read_time) */
  bool cache_clean;     /* old caches have been deleted */
  uint force_update;    /* one-time force update from analyze table */
  bool no_stats;        /* have detected that no stats exist */
  NdbIndexStat::Error error;
  time_t error_time;
  int error_count;
  struct Ndb_index_stat *share_next; /* per-share list */
  int lt;
  int lt_old;     /* for info only */
  struct Ndb_index_stat *list_next;
  struct Ndb_index_stat *list_prev;
  struct NDB_SHARE *share;
  Ndb_index_stat();
};

struct Ndb_index_stat_list {
  const char *name;
  int lt;
  struct Ndb_index_stat *head;
  struct Ndb_index_stat *tail;
  uint count;
  Ndb_index_stat_list(int the_lt, const char* the_name);
};

extern Ndb_index_stat_list ndb_index_stat_list[];

time_t ndb_index_stat_time_now= 0;

time_t
ndb_index_stat_time()
{
  time_t now= time(0);

  if (unlikely(ndb_index_stat_time_now == 0))
    ndb_index_stat_time_now= now;

  if (unlikely(now < ndb_index_stat_time_now))
  {
    DBUG_PRINT("index_stat", ("time moved backwards %d seconds",
                              int(ndb_index_stat_time_now - now)));
    now= ndb_index_stat_time_now;
  }

  ndb_index_stat_time_now= now;
  return now;
}

bool ndb_index_stat_allow_flag= false;

bool
ndb_index_stat_allow(int flag= -1)
{
  if (flag != -1) {
    pthread_mutex_lock(&ndb_index_stat_list_mutex);
    ndb_index_stat_allow_flag= (bool)flag;
    pthread_mutex_unlock(&ndb_index_stat_list_mutex);
  }
  return ndb_index_stat_allow_flag;
}

/* Options */

/* Options in string format buffer size */
static const uint ndb_index_stat_option_sz= 512;
void ndb_index_stat_opt2str(const struct Ndb_index_stat_opt&, char*);

struct Ndb_index_stat_opt {
  enum Unit {
    Ubool = 1,
    Usize = 2,
    Utime = 3,
    Umsec = 4
  };
  enum Flag {
    Freadonly = (1 << 0)
  };
  struct Val {
    const char* name;
    uint val;
    uint minval;
    uint maxval;
    Unit unit;
    uint flag;
  };
  enum Idx {
    Iloop_checkon = 0,
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
    Imax = 16
  };
  Val val[Imax];
  /* Options in string format (SYSVAR ndb_index_stat_option) */
  char *option;
  Ndb_index_stat_opt(char* buf);
  uint get(Idx i) const {
    assert(i < Imax);
    return val[i].val;
  }
};

Ndb_index_stat_opt::Ndb_index_stat_opt(char* buf) :
  option(buf)
{
#define ival(aname, aval, aminval, amaxval, aunit, aflag) \
  val[I##aname].name = #aname; \
  val[I##aname].val = aval; \
  val[I##aname].minval = aminval; \
  val[I##aname].maxval = amaxval; \
  val[I##aname].unit = aunit; \
  val[I##aname].flag = aflag
  ival(loop_checkon, 1000, 0, ~0, Umsec, 0);
  ival(loop_idle, 1000, 0, ~0, Umsec, 0);
  ival(loop_busy, 100, 0, ~0, Umsec, 0);
  ival(update_batch, 1, 1, ~0, Usize, 0);
  ival(read_batch, 4, 1, ~0, Usize, 0);
  ival(idle_batch, 32, 1, ~0, Usize, 0);
  ival(check_batch, 32, 1, ~0, Usize, 0);
  ival(check_delay, 60, 0, ~0, Utime, 0);
  ival(clean_delay, 0, 0, ~0, Utime, 0);
  ival(delete_batch, 8, 1, ~0, Usize, 0);
  ival(error_batch, 4, 1, ~0, Usize, 0);
  ival(error_delay, 60, 0, ~0, Utime, 0);
  ival(evict_batch, 8, 1, ~0, Usize, 0);
  ival(evict_delay, 60, 0, ~0, Utime, 0);
  ival(cache_limit, 32*1024*1024, 1024*1024, ~0, Usize, 0);
  ival(cache_lowpct, 90, 0, 100, Usize, 0);
#undef ival

  ndb_index_stat_opt2str(*this, option);
}

/* Hard limits */
static const uint ndb_index_stat_max_evict_batch = 32;

char ndb_index_stat_option_buf[ndb_index_stat_option_sz];
Ndb_index_stat_opt ndb_index_stat_opt(ndb_index_stat_option_buf);

/* Copy option struct to string buffer */
void
ndb_index_stat_opt2str(const Ndb_index_stat_opt& opt, char* str)
{
  DBUG_ENTER("ndb_index_stat_opt2str");

  char buf[ndb_index_stat_option_sz];
  char *const end= &buf[sizeof(buf)];
  char* ptr= buf;
  *ptr= 0;

  const uint imax= Ndb_index_stat_opt::Imax;
  for (uint i= 0; i < imax; i++)
  {
    const Ndb_index_stat_opt::Val& v= opt.val[i];
    ptr+= strlen(ptr);
    const char* sep= (ptr == buf ? "" : ",");
    const uint sz= ptr < end ? end - ptr : 0;

    switch (v.unit) {
    case Ndb_index_stat_opt::Ubool:
      {
        DBUG_ASSERT(v.val == 0 || v.val == 1);
        if (v.val == 0)
          my_snprintf(ptr, sz, "%s%s=OFF", sep, v.name);
        else
          my_snprintf(ptr, sz, "%s%s=ON", sep, v.name);
      }
      break;

    case Ndb_index_stat_opt::Usize:
      {
        uint m;
        if (v.val == 0)
          my_snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else if (v.val % (m= 1024*1024*1024) == 0)
          my_snprintf(ptr, sz, "%s%s=%uG", sep, v.name, v.val / m);
        else if (v.val % (m= 1024*1024) == 0)
          my_snprintf(ptr, sz, "%s%s=%uM", sep, v.name, v.val / m);
        else if (v.val % (m= 1024) == 0)
          my_snprintf(ptr, sz, "%s%s=%uK", sep, v.name, v.val / m);
        else
          my_snprintf(ptr, sz, "%s%s=%u", sep, v.name, v.val);
      }
      break;

    case Ndb_index_stat_opt::Utime:
      {
        uint m;
        if (v.val == 0)
          my_snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else if (v.val % (m= 60*60*24) == 0)
          my_snprintf(ptr, sz, "%s%s=%ud", sep, v.name, v.val / m);
        else if (v.val % (m= 60*60) == 0)
          my_snprintf(ptr, sz, "%s%s=%uh", sep, v.name, v.val / m);
        else if (v.val % (m= 60) == 0)
          my_snprintf(ptr, sz, "%s%s=%um", sep, v.name, v.val / m);
        else
          my_snprintf(ptr, sz, "%s%s=%us", sep, v.name, v.val);
      }
      break;

    case Ndb_index_stat_opt::Umsec:
      {
        if (v.val == 0)
          my_snprintf(ptr, sz, "%s%s=0", sep, v.name);
        else
          my_snprintf(ptr, sz, "%s%s=%ums", sep, v.name, v.val);
      }
      break;

    default:
      DBUG_ASSERT(false);
      break;
    }
  }

  memset(str, 0, ndb_index_stat_option_sz);
  strcpy(str, buf);
  DBUG_PRINT("index_stat", ("str: \"%s\"", str));
  DBUG_VOID_RETURN;
}

int
ndb_index_stat_option_parse(char* p, Ndb_index_stat_opt& opt)
{
  DBUG_ENTER("ndb_index_stat_option_parse");

  char *r= strchr(p, '=');
  if (r == 0)
    DBUG_RETURN(-1);
  *r++= 0;

  while (isspace(*r))
    *r++= 0;
  if (*r == 0)
    DBUG_RETURN(-1);

  const uint imax= Ndb_index_stat_opt::Imax;
  for (uint i= 0; i < imax; i++)
  {
    Ndb_index_stat_opt::Val& v= opt.val[i];
    if (strcmp(p, v.name) != 0)
      continue;

    char *s;
    for (s= r; *s != 0; s++)
      *s= tolower(*s);
    ulonglong val= strtoull(r, &s, 10);

    switch (v.unit) {
    case Ndb_index_stat_opt::Ubool:
      {
        if ((s > r && *s == 0 && val == 0) ||
            strcmp(r, "off") == 0 ||
            strcmp(r, "false") == 0)
          val= 0;
        else if ((s > r && *s == 0 && val == 1) ||
            strcmp(r, "on") == 0 ||
            strcmp(r, "true") == 0)
          val= 1;
        else
          DBUG_RETURN(-1);
        v.val= (uint)val;
      }
      break;

    case Ndb_index_stat_opt::Usize:
      {
        if (s == r)
          DBUG_RETURN(-1);
        if (strcmp(s, "") == 0)
          ;
        else if (strcmp(s, "k") == 0)
          val*= 1024;
        else if (strcmp(s, "m") == 0)
          val*= 1024*1024;
        else if (strcmp(s, "g") == 0)
          val*= 1024*1024*1024;
        else
          DBUG_RETURN(-1);
        if (val < v.minval || val > v.maxval)
          DBUG_RETURN(-1);
        v.val= (uint)val;
      }
      break;

    case Ndb_index_stat_opt::Utime:
      {
        if (s == r)
          DBUG_RETURN(-1);
        if (strcmp(s, "") == 0)
          ;
        else if (strcmp(s, "s") == 0)
          ;
        else if (strcmp(s, "m") == 0)
          val*= 60;
        else if (strcmp(s, "h") == 0)
          val*= 60*60;
        else if (strcmp(s, "d") == 0)
          val*= 24*60*60;
        else
          DBUG_RETURN(-1);
        if (val < v.minval || val > v.maxval)
          DBUG_RETURN(-1);
        v.val= (uint)val;
      }
      break;

    case Ndb_index_stat_opt::Umsec:
      {
        if (s == r)
          DBUG_RETURN(-1);
        if (strcmp(s, "") == 0)
          ;
        else if (strcmp(s, "ms") == 0)
          ;
        else
          DBUG_RETURN(-1);
        if (val < v.minval || val > v.maxval)
          DBUG_RETURN(-1);
        v.val= (uint)val;
      }
      break;

    default:
      DBUG_ASSERT(false);
      break;
    }
  }
  DBUG_RETURN(0);
}

/* Copy option string to option struct */
int
ndb_index_stat_str2opt(const char *str, Ndb_index_stat_opt& opt)
{
  DBUG_ENTER("ndb_index_stat_str2opt");
  DBUG_PRINT("index_stat", ("str: \"%s\"", str));

  char buf[ndb_index_stat_option_sz];

  assert(str != 0);
  if (strlen(str) >= sizeof(buf))
    DBUG_RETURN(-1);
  strcpy(buf, str);

  char *p= buf;
  while (1)
  {
    while (isspace(*p))
      p++;
    if (*p == 0)
      break;

    char *q= strchr(p, ',');
    if (q == p)
      DBUG_RETURN(-1);
    if (q != 0)
      *q= 0;

    DBUG_PRINT("index_stat", ("parse: %s", p));
    if (ndb_index_stat_option_parse(p, opt) == -1)
      DBUG_RETURN(-1);

    if (q == 0)
      break;
    p= q + 1;
  }

  ndb_index_stat_opt2str(opt, opt.option);
  DBUG_RETURN(0);
}

/* Thanks to ha_innodb.cc */

/* Need storage between check and update (assume locked) */
char ndb_index_stat_option_tmp[ndb_index_stat_option_sz];
 
int
ndb_index_stat_option_check(MYSQL_THD,
                            struct st_mysql_sys_var *var,
                            void *save,
                            struct st_mysql_value *value)
{
  DBUG_ENTER("ndb_index_stat_option_check");
  char buf[ndb_index_stat_option_sz];
  int len= sizeof(buf);
  const char *str= value->val_str(value, buf, &len);
  if (str != 0)
  {
    /* Seems to be nothing in buf */
    DBUG_PRINT("index_stat", ("str: %s len: %d", str, len));
    char buf2[ndb_index_stat_option_sz];
    Ndb_index_stat_opt opt(buf2);
    if (ndb_index_stat_str2opt(str, opt) == 0)
    {
      /* Passed to update */
      strcpy(ndb_index_stat_option_tmp, str);
      *(const char**)save= ndb_index_stat_option_tmp;
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);
}

void
ndb_index_stat_option_update(MYSQL_THD,
                             struct st_mysql_sys_var *var,
                             void *var_ptr,
                             const void *save)
{
  DBUG_ENTER("ndb_index_stat_option_update");
  const char *str= *(const char**)save;
  DBUG_PRINT("index_stat", ("str: %s", str));
  Ndb_index_stat_opt& opt= ndb_index_stat_opt;
  int ret= ndb_index_stat_str2opt(str, opt);
  assert(ret == 0); NDB_IGNORE_VALUE(ret);
  *(const char**)var_ptr= ndb_index_stat_opt.option;
  DBUG_VOID_RETURN;
}

/* Global stuff */

struct Ndb_index_stat_glob {
  uint list_count[Ndb_index_stat::LT_Count]; /* Temporary use */
  uint total_count;
  uint force_update;
  uint wait_update;
  uint no_stats;
  uint cache_query_bytes; /* In use */
  uint cache_clean_bytes; /* Obsolete versions not yet removed */
  Ndb_index_stat_glob() :
    total_count(0),
    force_update(0),
    wait_update(0),
    no_stats(0),
    cache_query_bytes(0),
    cache_clean_bytes(0)
  {
  }
  void set_list_count()
  {
    total_count= 0;
    int lt;
    for (lt= 0; lt < Ndb_index_stat::LT_Count; lt++)
    {
      const Ndb_index_stat_list &list= ndb_index_stat_list[lt];
      list_count[lt]= list.count;
      total_count++;
    }
  }
  void set_status_variables()
  {
    g_ndb_status_index_stat_cache_query= cache_query_bytes;
    g_ndb_status_index_stat_cache_clean= cache_clean_bytes;
  }
};

Ndb_index_stat_glob ndb_index_stat_glob;

/* Shared index entries */

Ndb_index_stat::Ndb_index_stat()
{
  is= 0;
  index_id= 0;
  index_version= 0;
#ifndef DBUG_OFF
  memset(id, 0, sizeof(id));
#endif
  access_time= 0;
  load_time= 0;
  read_time= 0;
  sample_version= 0;
  check_time= 0;
  cache_clean= false;
  force_update= 0;
  no_stats= false;
  error_time= 0;
  error_count= 0;
  share_next= 0;
  lt= 0;
  lt_old= 0;
  list_next= 0;
  list_prev= 0;
  share= 0;
}

void
ndb_index_stat_error(Ndb_index_stat *st, const char* place, int line)
{
  time_t now= ndb_index_stat_time();
  NdbIndexStat::Error error= st->is->getNdbError();
  if (error.code == 0)
  {
    // XXX why this if
    NdbIndexStat::Error error2;
    error= error2;
    error.code= NdbIndexStat::InternalError;
    error.status= NdbError::TemporaryError;
  }
  st->error= error;
  st->error_time= now;
  st->error_count++;

  DBUG_PRINT("index_stat", ("%s line %d: error %d line %d extra %d",
                            place, line, error.code, error.line, error.extra));
}

void
ndb_index_stat_clear_error(Ndb_index_stat *st)
{
  st->error.code= 0;
  st->error.status= NdbError::Success;
}

/* Lists across shares */

Ndb_index_stat_list::Ndb_index_stat_list(int the_lt, const char* the_name)
{
  lt= the_lt;
  name= the_name;
  head= 0;
  tail= 0;
  count= 0;
}

Ndb_index_stat_list ndb_index_stat_list[Ndb_index_stat::LT_Count] = {
  Ndb_index_stat_list(0, 0),
  Ndb_index_stat_list(Ndb_index_stat::LT_New,    "New"),
  Ndb_index_stat_list(Ndb_index_stat::LT_Update, "Update"),
  Ndb_index_stat_list(Ndb_index_stat::LT_Read,   "Read"),
  Ndb_index_stat_list(Ndb_index_stat::LT_Idle,   "Idle"),
  Ndb_index_stat_list(Ndb_index_stat::LT_Check,  "Check"),
  Ndb_index_stat_list(Ndb_index_stat::LT_Delete, "Delete"),
  Ndb_index_stat_list(Ndb_index_stat::LT_Error,  "Error")
};

void
ndb_index_stat_list_add(Ndb_index_stat* st, int lt)
{
  assert(st != 0 && st->lt == 0);
  assert(st->list_next == 0 && st->list_prev == 0);
  assert(1 <= lt && lt < Ndb_index_stat::LT_Count);
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];

  DBUG_PRINT("index_stat", ("st %s -> %s", st->id, list.name));

  if (list.count == 0)
  {
    assert(list.head == 0 && list.tail == 0);
    list.head= st;
    list.tail= st;
  }
  else
  {
    assert(list.tail != 0 && list.tail->list_next == 0);
    st->list_prev= list.tail;
    list.tail->list_next= st;
    list.tail= st;
  }
  list.count++;

  st->lt= lt;
}

void
ndb_index_stat_list_remove(Ndb_index_stat* st)
{
  assert(st != 0);
  int lt= st->lt;
  assert(1 <= lt && lt < Ndb_index_stat::LT_Count);
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];

  DBUG_PRINT("index_stat", ("st %s <- %s", st->id, list.name));

  Ndb_index_stat* next= st->list_next;
  Ndb_index_stat* prev= st->list_prev;

  if (list.head == st)
    list.head= next;
  if (list.tail == st)
    list.tail= prev;
  assert(list.count != 0);
  list.count--;

  if (next != 0)
    next->list_prev= prev;
  if (prev != 0)
    prev->list_next= next;

  st->lt= 0;
  st->lt_old= 0;
  st->list_next= 0;
  st->list_prev= 0;
}

void
ndb_index_stat_list_move(Ndb_index_stat *st, int lt)
{
  assert(st != 0);
  ndb_index_stat_list_remove(st);
  ndb_index_stat_list_add(st, lt);
}

/* Stats entry changes (must hold stat_mutex) */

void
ndb_index_stat_force_update(Ndb_index_stat *st, bool onoff)
{
  Ndb_index_stat_glob &glob= ndb_index_stat_glob;
  if (onoff)
  {
    /* One more request */
    glob.force_update++;
    st->force_update++;
  }
  else
  {
    /* All done */
    assert(glob.force_update >= st->force_update);
    glob.force_update-= st->force_update;
    st->force_update= 0;
  }
}

void
ndb_index_stat_no_stats(Ndb_index_stat *st, bool flag)
{
  Ndb_index_stat_glob &glob= ndb_index_stat_glob;
  if (st->no_stats != flag)
  {
    if (flag)
    {
      glob.no_stats++;
      st->no_stats= true;
    }
    else
    {
      assert(glob.no_stats >= 1);
      glob.no_stats-= 1;
      st->no_stats= false;
    }
  }
}

/* Find or add entry under the share */

Ndb_index_stat*
ndb_index_stat_alloc(const NDBINDEX *index,
                     const NDBTAB *table,
                     int &err_out)
{
  err_out= 0;
  Ndb_index_stat *st= new Ndb_index_stat;
  NdbIndexStat *is= new NdbIndexStat;
  if (st != 0 && is != 0)
  {
    st->is= is;
    st->index_id= index->getObjectId();
    st->index_version= index->getObjectVersion();
#ifndef DBUG_OFF
    my_snprintf(st->id, sizeof(st->id), "%d.%d", st->index_id, st->index_version);
#endif
    if (is->set_index(*index, *table) == 0)
      return st;
    ndb_index_stat_error(st, "set_index", __LINE__);
    err_out= st->error.code;
  }
  else
  {
    err_out= NdbIndexStat::NoMemError;
  }
  if (is != 0)
    delete is;
  if (st != 0)
    delete st;
  return 0;
}

/* Subroutine, have lock */
Ndb_index_stat*
ndb_index_stat_find_share(NDB_SHARE *share,
                          const NDBINDEX *index,
                          Ndb_index_stat *&st_last)
{
  struct Ndb_index_stat *st= share->index_stat_list;
  st_last= 0;
  while (st != 0)
  {
    assert(st->share == share);
    assert(st->is != 0);
    NdbIndexStat::Head head;
    st->is->get_head(head);
    if (head.m_indexId == (uint)index->getObjectId() &&
        head.m_indexVersion == (uint)index->getObjectVersion())
      break;
    st_last= st;
    st= st->share_next;
  }
  return st;
}

/* Subroutine, have lock */
void
ndb_index_stat_add_share(NDB_SHARE *share,
                         Ndb_index_stat *st,
                         Ndb_index_stat *st_last)
{
  st->share= share;
  if (st_last == 0)
    share->index_stat_list= st;
  else
    st_last->share_next= st;
}

Ndb_index_stat*
ndb_index_stat_get_share(NDB_SHARE *share,
                         const NDBINDEX *index,
                         const NDBTAB *table,
                         int &err_out,
                         bool allow_add,
                         bool force_update)
{
  pthread_mutex_lock(&share->mutex);
  pthread_mutex_lock(&ndb_index_stat_list_mutex);
  pthread_mutex_lock(&ndb_index_stat_stat_mutex);
  time_t now= ndb_index_stat_time();
  err_out= 0;

  struct Ndb_index_stat *st= 0;
  struct Ndb_index_stat *st_last= 0;
  do
  {
    if (unlikely(!ndb_index_stat_allow()))
    {
      err_out= Ndb_index_stat_error_NOT_ALLOW;
      break;
    }
    st= ndb_index_stat_find_share(share, index, st_last);
    if (st == 0)
    {
      if (!allow_add)
      {
        err_out= Ndb_index_stat_error_NOT_FOUND;
        break;
      }
      st= ndb_index_stat_alloc(index, table, err_out);
      if (st == 0)
      {
        assert(err_out != 0);
        break;
      }
      ndb_index_stat_add_share(share, st, st_last);
      ndb_index_stat_list_add(st, Ndb_index_stat::LT_New);
    }
    if (force_update)
      ndb_index_stat_force_update(st, true);
    st->access_time= now;
  }
  while (0);

  pthread_mutex_unlock(&ndb_index_stat_stat_mutex);
  pthread_mutex_unlock(&ndb_index_stat_list_mutex);
  pthread_mutex_unlock(&share->mutex);
  return st;
}

void
ndb_index_stat_free(Ndb_index_stat *st)
{
  pthread_mutex_lock(&ndb_index_stat_list_mutex);
  NDB_SHARE *share= st->share;
  assert(share != 0);

  Ndb_index_stat *st_head= 0;
  Ndb_index_stat *st_tail= 0;
  Ndb_index_stat *st_loop= share->index_stat_list;
  bool found= false;
  while (st_loop != 0) {
    if (st == st_loop) {
      st->share= 0;
      assert(st->lt != 0);
      assert(st->lt != Ndb_index_stat::LT_Delete);
      ndb_index_stat_list_move(st, Ndb_index_stat::LT_Delete);
      st_loop= st_loop->share_next;
      assert(!found);
      found++;
    } else {
      if (st_head == 0)
        st_head= st_loop;
      else
        st_tail->share_next= st_loop;
      st_tail= st_loop;
      st_loop= st_loop->share_next;
      st_tail->share_next= 0;
    }
  }
  assert(found);
  share->index_stat_list= st_head;
  pthread_mutex_unlock(&ndb_index_stat_list_mutex);
}

void
ndb_index_stat_free(NDB_SHARE *share)
{
  pthread_mutex_lock(&ndb_index_stat_list_mutex);
  Ndb_index_stat *st;
  while ((st= share->index_stat_list) != 0)
  {
    share->index_stat_list= st->share_next;
    st->share= 0;
    assert(st->lt != 0);
    assert(st->lt != Ndb_index_stat::LT_Delete);
    ndb_index_stat_list_move(st, Ndb_index_stat::LT_Delete);
  }
  pthread_mutex_unlock(&ndb_index_stat_list_mutex);
}

/* Find entry across shares */
/* wl4124_todo mutex overkill, hash table, can we find table share */
Ndb_index_stat*
ndb_index_stat_find_entry(int index_id, int index_version, int table_id)
{
  DBUG_ENTER("ndb_index_stat_find_entry");
  pthread_mutex_lock(&ndbcluster_mutex);
  pthread_mutex_lock(&ndb_index_stat_list_mutex);
  DBUG_PRINT("index_stat", ("find index:%d version:%d table:%d",
                            index_id, index_version, table_id));

  int lt;
  for (lt=1; lt < Ndb_index_stat::LT_Count; lt++)
  {
    Ndb_index_stat *st=ndb_index_stat_list[lt].head;
    while (st != 0)
    {
      if (st->index_id == index_id &&
          st->index_version == index_version)
      {
        pthread_mutex_unlock(&ndb_index_stat_list_mutex);
        pthread_mutex_unlock(&ndbcluster_mutex);
        DBUG_RETURN(st);
      }
      st= st->list_next;
    }
  }

  pthread_mutex_unlock(&ndb_index_stat_list_mutex);
  pthread_mutex_unlock(&ndbcluster_mutex);
  DBUG_RETURN(0);
}

/* Statistics thread sub-routines */

void
ndb_index_stat_cache_move(Ndb_index_stat *st)
{
  Ndb_index_stat_glob &glob= ndb_index_stat_glob;
  NdbIndexStat::CacheInfo infoBuild;
  NdbIndexStat::CacheInfo infoQuery;

  st->is->get_cache_info(infoBuild, NdbIndexStat::CacheBuild);
  st->is->get_cache_info(infoQuery, NdbIndexStat::CacheQuery);
  const uint new_query_bytes= infoBuild.m_totalBytes;
  const uint old_query_bytes= infoQuery.m_totalBytes;
  DBUG_PRINT("index_stat", ("st %s cache move: query:%u clean:%u",
                            st->id, new_query_bytes, old_query_bytes));
  st->is->move_cache();
  assert(glob.cache_query_bytes >= old_query_bytes);
  glob.cache_query_bytes-= old_query_bytes;
  glob.cache_query_bytes+= new_query_bytes;
  glob.cache_clean_bytes+= old_query_bytes;
  glob.set_status_variables();
}

void
ndb_index_stat_cache_clean(Ndb_index_stat *st)
{
  Ndb_index_stat_glob &glob= ndb_index_stat_glob;
  NdbIndexStat::CacheInfo infoClean;

  st->is->get_cache_info(infoClean, NdbIndexStat::CacheClean);
  const uint old_clean_bytes= infoClean.m_totalBytes;
  DBUG_PRINT("index_stat", ("st %s cache clean: clean:%u",
                            st->id, old_clean_bytes));
  st->is->clean_cache();
  assert(glob.cache_clean_bytes >= old_clean_bytes);
  glob.cache_clean_bytes-= old_clean_bytes;
  glob.set_status_variables();
}

/* Misc in/out parameters for process steps */
struct Ndb_index_stat_proc {
  NdbIndexStat* is_util; // For metadata and polling
  Ndb *ndb;
  time_t now;
  int lt;
  bool busy;
  bool end;
  Ndb_index_stat_proc() :
    is_util(0),
    ndb(0),
    now(0),
    lt(0),
    busy(false),
    end(false)
  {}
};

void
ndb_index_stat_proc_new(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  if (st->error.code != 0)
    pr.lt= Ndb_index_stat::LT_Error;
  else if (st->force_update)
    pr.lt= Ndb_index_stat::LT_Update;
  else
    pr.lt= Ndb_index_stat::LT_Read;
}

void
ndb_index_stat_proc_new(Ndb_index_stat_proc &pr)
{
  pthread_mutex_lock(&ndb_index_stat_list_mutex);
  const int lt= Ndb_index_stat::LT_New;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];

  Ndb_index_stat *st_loop= list.head;
  while (st_loop != 0)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_new(pr, st);
    ndb_index_stat_list_move(st, pr.lt);
  }
  pthread_mutex_unlock(&ndb_index_stat_list_mutex);
}

void
ndb_index_stat_proc_update(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  if (st->is->update_stat(pr.ndb) == -1)
  {
    ndb_index_stat_error(st, "update_stat", __LINE__);
    pr.lt= Ndb_index_stat::LT_Error;
    return;
  }
  pr.lt= Ndb_index_stat::LT_Read;
}

void
ndb_index_stat_proc_update(Ndb_index_stat_proc &pr)
{
  const int lt= Ndb_index_stat::LT_Update;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint batch= opt.get(Ndb_index_stat_opt::Iupdate_batch);

  Ndb_index_stat *st_loop= list.head;
  uint cnt= 0;
  while (st_loop != 0 && cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_update(pr, st);
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

void
ndb_index_stat_proc_read(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  NdbIndexStat::Head head;
  if (st->is->read_stat(pr.ndb) == -1)
  {
    pthread_mutex_lock(&ndb_index_stat_stat_mutex);
    ndb_index_stat_error(st, "read_stat", __LINE__);
    const uint force_update= st->force_update;
    ndb_index_stat_force_update(st, false);

    /* no stats is not unexpected error, unless analyze was done */
    if (st->is->getNdbError().code == NdbIndexStat::NoIndexStats &&
        force_update == 0)
    {
      ndb_index_stat_no_stats(st, true);
      pr.lt= Ndb_index_stat::LT_Idle;
    }
    else
    {
      pr.lt= Ndb_index_stat::LT_Error;
    }

    pthread_cond_broadcast(&ndb_index_stat_stat_cond);
    pthread_mutex_unlock(&ndb_index_stat_stat_mutex);
    return;
  }

  pthread_mutex_lock(&ndb_index_stat_stat_mutex);
  pr.now= ndb_index_stat_time();
  st->is->get_head(head);
  st->load_time= head.m_loadTime;
  st->read_time= pr.now;
  st->sample_version= head.m_sampleVersion;

  ndb_index_stat_force_update(st, false);
  ndb_index_stat_no_stats(st, false);

  ndb_index_stat_cache_move(st);
  st->cache_clean= false;
  pr.lt= Ndb_index_stat::LT_Idle;
  pthread_cond_broadcast(&ndb_index_stat_stat_cond);
  pthread_mutex_unlock(&ndb_index_stat_stat_mutex);
}

void
ndb_index_stat_proc_read(Ndb_index_stat_proc &pr)
{
  const int lt= Ndb_index_stat::LT_Read;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint batch= opt.get(Ndb_index_stat_opt::Iread_batch);

  Ndb_index_stat *st_loop= list.head;
  uint cnt= 0;
  while (st_loop != 0 && cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_read(pr, st);
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

// wl4124_todo detect force_update faster
void
ndb_index_stat_proc_idle(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const int clean_delay= opt.get(Ndb_index_stat_opt::Iclean_delay);
  const int check_delay= opt.get(Ndb_index_stat_opt::Icheck_delay);
  const time_t clean_wait=
    st->cache_clean ? 0 : st->read_time + clean_delay - pr.now;
  const time_t check_wait=
    st->check_time == 0 ? 0 : st->check_time + check_delay - pr.now;

  DBUG_PRINT("index_stat", ("st %s check wait:%lds force update:%u"
                            " clean wait:%lds cache clean:%d",
                            st->id, (long)check_wait, st->force_update,
                            (long)clean_wait, st->cache_clean));

  if (!st->cache_clean && clean_wait <= 0)
  {
    ndb_index_stat_cache_clean(st);
    st->cache_clean= true;
  }
  if (st->force_update)
  {
    pr.lt= Ndb_index_stat::LT_Update;
    return;
  }
  if (check_wait <= 0)
  {
    pr.lt= Ndb_index_stat::LT_Check;
    return;
  }
  pr.lt= Ndb_index_stat::LT_Idle;
}

void
ndb_index_stat_proc_idle(Ndb_index_stat_proc &pr)
{
  const int lt= Ndb_index_stat::LT_Idle;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint batch= opt.get(Ndb_index_stat_opt::Iidle_batch);
  pr.now= ndb_index_stat_time();

  Ndb_index_stat *st_loop= list.head;
  uint cnt= 0;
  while (st_loop != 0 && cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_idle(pr, st);
    // rotates list if entry remains LT_Idle
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

void
ndb_index_stat_proc_check(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  pr.now= ndb_index_stat_time();
  st->check_time= pr.now;
  NdbIndexStat::Head head;
  if (st->is->read_head(pr.ndb) == -1)
  {
    ndb_index_stat_error(st, "read_head", __LINE__);
    /* no stats is not unexpected error */
    if (st->is->getNdbError().code == NdbIndexStat::NoIndexStats)
    {
      ndb_index_stat_no_stats(st, true);
      pr.lt= Ndb_index_stat::LT_Idle;
    }
    else
    {
      pr.lt= Ndb_index_stat::LT_Error;
    }
    return;
  }
  st->is->get_head(head);
  const uint version_old= st->sample_version;
  const uint version_new= head.m_sampleVersion;
  if (version_old != version_new)
  {
    DBUG_PRINT("index_stat", ("st %s sample version old:%u new:%u",
                              st->id, version_old, version_new));
    pr.lt= Ndb_index_stat::LT_Read;
    return;
  }
  pr.lt= Ndb_index_stat::LT_Idle;
}

void
ndb_index_stat_proc_check(Ndb_index_stat_proc &pr)
{
  const int lt= Ndb_index_stat::LT_Check;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint batch= opt.get(Ndb_index_stat_opt::Icheck_batch);

  Ndb_index_stat *st_loop= list.head;
  uint cnt= 0;
  while (st_loop != 0 && cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_check(pr, st);
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

void
ndb_index_stat_proc_evict(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  NdbIndexStat::Head head;
  NdbIndexStat::CacheInfo infoBuild;
  NdbIndexStat::CacheInfo infoQuery;
  NdbIndexStat::CacheInfo infoClean;
  st->is->get_head(head);
  st->is->get_cache_info(infoBuild, NdbIndexStat::CacheBuild);
  st->is->get_cache_info(infoQuery, NdbIndexStat::CacheQuery);
  st->is->get_cache_info(infoClean, NdbIndexStat::CacheClean);

  DBUG_PRINT("index_stat",
             ("evict table: %u index: %u version: %u"
              " sample version: %u"
              " cache bytes build:%u query:%u clean:%u",
              head.m_tableId, head.m_indexId, head.m_indexVersion,
              head.m_sampleVersion,
              infoBuild.m_totalBytes, infoQuery.m_totalBytes, infoClean.m_totalBytes));

  /* Twice to move all caches to clean */
  ndb_index_stat_cache_move(st);
  ndb_index_stat_cache_move(st);
  ndb_index_stat_cache_clean(st);
}

bool
ndb_index_stat_proc_evict()
{
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  Ndb_index_stat_glob &glob= ndb_index_stat_glob;
  uint curr_size= glob.cache_query_bytes + glob.cache_clean_bytes;
  const uint cache_lowpct= opt.get(Ndb_index_stat_opt::Icache_lowpct);
  const uint cache_limit= opt.get(Ndb_index_stat_opt::Icache_limit);
  if (100 * curr_size <= cache_lowpct * cache_limit)
    return false;
  return true;
}

void
ndb_index_stat_proc_evict(Ndb_index_stat_proc &pr, int lt)
{
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint batch= opt.get(Ndb_index_stat_opt::Ievict_batch);
  const int evict_delay= opt.get(Ndb_index_stat_opt::Ievict_delay);
  pr.now= ndb_index_stat_time();

  if (!ndb_index_stat_proc_evict())
    return;

  /* Create a LRU batch */
  Ndb_index_stat* st_lru_arr[ndb_index_stat_max_evict_batch + 1];
  uint st_lru_cnt= 0;
  Ndb_index_stat *st_loop= list.head;
  while (st_loop != 0 && st_lru_cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    if (st->read_time + evict_delay <= pr.now)
    {
      /* Insertion sort into the batch from the end */
      if (st_lru_cnt == 0)
        st_lru_arr[st_lru_cnt++]= st;
      else
      {
        uint i= st_lru_cnt;
        while (i != 0)
        {
          if (st_lru_arr[i-1]->access_time < st->access_time)
            break;
          i--;
        }
        if (i < st_lru_cnt)
        {
          uint j= st_lru_cnt; /* There is place for one more at end */
          while (j > i)
          {
            st_lru_arr[j]= st_lru_arr[j-1];
            j--;
          }
          st_lru_arr[i]= st;
          if (st_lru_cnt < batch)
            st_lru_cnt++;
        }
      }
    }
  }

  /* Process the LRU batch */
  uint cnt= 0;
  while (cnt < st_lru_cnt)
  {
    if (!ndb_index_stat_proc_evict())
      break;

    Ndb_index_stat *st= st_lru_arr[cnt];
    DBUG_PRINT("index_stat", ("st %s proc evict %s", st->id, list.name));
    ndb_index_stat_proc_evict(pr, st);
    ndb_index_stat_free(st);
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

void
ndb_index_stat_proc_evict(Ndb_index_stat_proc &pr)
{
  ndb_index_stat_proc_evict(pr, Ndb_index_stat::LT_Error);
  ndb_index_stat_proc_evict(pr, Ndb_index_stat::LT_Idle);
}

void
ndb_index_stat_proc_delete(Ndb_index_stat_proc &pr)
{
  const int lt= Ndb_index_stat::LT_Delete;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint delete_batch= opt.get(Ndb_index_stat_opt::Idelete_batch);
  const uint batch= !pr.end ? delete_batch : 0xFFFFFFFF;

  Ndb_index_stat *st_loop= list.head;
  uint cnt= 0;
  while (st_loop != 0 && cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_evict(pr, st);
    ndb_index_stat_list_remove(st);
    delete st->is;
    delete st;
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

void
ndb_index_stat_proc_error(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const int error_delay= opt.get(Ndb_index_stat_opt::Ierror_delay);
  const time_t error_wait= st->error_time + error_delay - pr.now;

  if (error_wait <= 0 ||
      /* Analyze issued after previous error */
      st->force_update)
  {
    DBUG_PRINT("index_stat", ("st %s error wait:%ds error count:%u"
                              " force update:%u",
                              st->id, (int)error_wait, st->error_count,
                              st->force_update));
    ndb_index_stat_clear_error(st);
    if (st->force_update)
      pr.lt= Ndb_index_stat::LT_Update;
    else
      pr.lt= Ndb_index_stat::LT_Read;
    return;
  }
  pr.lt= Ndb_index_stat::LT_Error;
}

void
ndb_index_stat_proc_error(Ndb_index_stat_proc &pr)
{
  const int lt= Ndb_index_stat::LT_Error;
  Ndb_index_stat_list &list= ndb_index_stat_list[lt];
  const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
  const uint batch= opt.get(Ndb_index_stat_opt::Ierror_batch);
  pr.now= ndb_index_stat_time();

  Ndb_index_stat *st_loop= list.head;
  uint cnt= 0;
  while (st_loop != 0 && cnt < batch)
  {
    Ndb_index_stat *st= st_loop;
    st_loop= st_loop->list_next;
    DBUG_PRINT("index_stat", ("st %s proc %s", st->id, list.name));
    ndb_index_stat_proc_error(pr, st);
    ndb_index_stat_list_move(st, pr.lt);
    cnt++;
  }
  if (cnt == batch)
    pr.busy= true;
}

void
ndb_index_stat_proc_event(Ndb_index_stat_proc &pr, Ndb_index_stat *st)
{
  /*
    Put on Check list if idle.
    We get event also for our own analyze but this should not matter.
   */
  pr.lt= st->lt;
  if (st->lt == Ndb_index_stat::LT_Idle ||
      st->lt == Ndb_index_stat::LT_Error)
    pr.lt= Ndb_index_stat::LT_Check;
}

void
ndb_index_stat_proc_event(Ndb_index_stat_proc &pr)
{
  NdbIndexStat *is= pr.is_util;
  Ndb *ndb= pr.ndb;
  int ret;
  ret= is->poll_listener(ndb, 0);
  DBUG_PRINT("index_stat", ("poll_listener ret: %d", ret));
  if (ret == -1)
  {
    // wl4124_todo report error
    DBUG_ASSERT(false);
    return;
  }
  if (ret == 0)
    return;

  while (1)
  {
    ret= is->next_listener(ndb);
    DBUG_PRINT("index_stat", ("next_listener ret: %d", ret));
    if (ret == -1)
    {
      // wl4124_todo report error
      DBUG_ASSERT(false);
      return;
    }
    if (ret == 0)
      break;

    NdbIndexStat::Head head;
    is->get_head(head);
    DBUG_PRINT("index_stat", ("next_listener eventType: %d indexId: %u",
                              head.m_eventType, head.m_indexId));

    Ndb_index_stat *st= ndb_index_stat_find_entry(head.m_indexId,
                                                  head.m_indexVersion,
                                                  head.m_tableId);
    /*
      Another process can update stats for an index which is not found
      in this mysqld.  Ignore it.
     */
    if (st != 0)
    {
      DBUG_PRINT("index_stat", ("st %s proc %s", st->id, "Event"));
      ndb_index_stat_proc_event(pr, st);
      if (pr.lt != st->lt)
        ndb_index_stat_list_move(st, pr.lt);
    }
    else
    {
      DBUG_PRINT("index_stat", ("entry not found in this mysqld"));
    }
  }
}

#ifndef DBUG_OFF
void
ndb_index_stat_report(const Ndb_index_stat_glob& old_glob)
{
  Ndb_index_stat_glob new_glob= ndb_index_stat_glob;
  new_glob.set_list_count();

  /* List counts */
  {
    const uint (&old_count)[Ndb_index_stat::LT_Count]= old_glob.list_count;
    const uint (&new_count)[Ndb_index_stat::LT_Count]= new_glob.list_count;
    bool any= false;
    int lt;
    for (lt=1; lt < Ndb_index_stat::LT_Count; lt++)
    {
      const Ndb_index_stat_list &list= ndb_index_stat_list[lt];
      const char* name= list.name;
      if (old_count[lt] != new_count[lt])
      {
        DBUG_PRINT("index_stat", ("%s: %u -> %u",
                                  name, old_count[lt], new_count[lt]));
        any= true;
      }
    }
    if (any)
    {
      const uint bufsz= 20 * Ndb_index_stat::LT_Count;
      char buf[bufsz];
      char *ptr= buf;
      for (lt= 1; lt < Ndb_index_stat::LT_Count; lt++)
      {
        const Ndb_index_stat_list &list= ndb_index_stat_list[lt];
        const char* name= list.name;
        sprintf(ptr, " %s:%u", name, new_count[lt]);
        ptr+= strlen(ptr);
      }
      DBUG_PRINT("index_stat", ("list:%s", buf));
    }
  }

  /* Cache summary */
  {
    const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
    uint query_size= new_glob.cache_query_bytes;
    uint clean_size= new_glob.cache_clean_bytes;
    uint total_size= query_size + clean_size;
    const uint limit= opt.get(Ndb_index_stat_opt::Icache_limit);
    double pct= 100.0;
    if (limit != 0)
      pct= 100.0 * (double)total_size / (double)limit;
    DBUG_PRINT("index_stat", ("cache query:%u clean:%u (%.2f pct)",
                              query_size, clean_size, pct));
  }

  /* Updates waited for and forced updates */
  {
    uint wait_update= new_glob.wait_update;
    uint force_update= new_glob.force_update;
    uint no_stats= new_glob.no_stats;
    DBUG_PRINT("index_stat", ("wait update:%u force update:%u no stats:%u",
                              wait_update, force_update, no_stats));
  }
}
#endif

void
ndb_index_stat_proc(Ndb_index_stat_proc &pr)
{
#ifndef DBUG_OFF
  Ndb_index_stat_glob old_glob= ndb_index_stat_glob;
  old_glob.set_list_count();
#endif

  DBUG_ENTER("ndb_index_stat_proc");

  ndb_index_stat_proc_new(pr);
  ndb_index_stat_proc_update(pr);
  ndb_index_stat_proc_read(pr);
  ndb_index_stat_proc_idle(pr);
  ndb_index_stat_proc_check(pr);
  ndb_index_stat_proc_evict(pr);
  ndb_index_stat_proc_delete(pr);
  ndb_index_stat_proc_error(pr);
  ndb_index_stat_proc_event(pr);

#ifndef DBUG_OFF
  ndb_index_stat_report(old_glob);
#endif
  DBUG_VOID_RETURN;
}

void
ndb_index_stat_end()
{
  DBUG_ENTER("ndb_index_stat_end");
  Ndb_index_stat_proc pr;
  pr.end= true;

  /*
   * Shares have been freed so any index stat entries left should be
   * in LT_Delete.  The first two steps here should be unnecessary.
   */

  ndb_index_stat_allow(0);

  int lt;
  for (lt= 1; lt < Ndb_index_stat::LT_Count; lt++)
  {
    if (lt == (int)Ndb_index_stat::LT_Delete)
      continue;
    Ndb_index_stat_list &list= ndb_index_stat_list[lt];
    Ndb_index_stat *st_loop= list.head;
    while (st_loop != 0)
    {
      Ndb_index_stat *st= st_loop;
      st_loop= st_loop->list_next;
      DBUG_PRINT("index_stat", ("st %s end %s", st->id, list.name));
      pr.lt= Ndb_index_stat::LT_Delete;
      ndb_index_stat_list_move(st, pr.lt);
    }
  }

  /* Real free */
  ndb_index_stat_proc_delete(pr);
  DBUG_VOID_RETURN;
}

/* Index stats thread */

int
ndb_index_stat_check_or_create_systables(Ndb_index_stat_proc &pr)
{
  DBUG_ENTER("ndb_index_stat_check_or_create_systables");

  NdbIndexStat *is= pr.is_util;
  Ndb *ndb= pr.ndb;

  if (is->check_systables(ndb) == 0)
  {
    DBUG_PRINT("index_stat", ("using existing index stats tables"));
    DBUG_RETURN(0);
  }

  if (is->create_systables(ndb) == 0)
  {
    DBUG_PRINT("index_stat", ("created index stats tables"));
    DBUG_RETURN(0);
  }

  if (is->getNdbError().code == 721 ||
      is->getNdbError().code == 4244)
  {
    // race between mysqlds, maybe
    DBUG_PRINT("index_stat", ("create index stats tables failed: error %d line %d",
                              is->getNdbError().code, is->getNdbError().line));
    DBUG_RETURN(-1);
  }

  sql_print_warning("create index stats tables failed: error %d line %d",
                    is->getNdbError().code, is->getNdbError().line);
  DBUG_RETURN(-1);
}

int
ndb_index_stat_check_or_create_sysevents(Ndb_index_stat_proc &pr)
{
  DBUG_ENTER("ndb_index_stat_check_or_create_sysevents");

  NdbIndexStat *is= pr.is_util;
  Ndb *ndb= pr.ndb;

  if (is->check_sysevents(ndb) == 0)
  {
    DBUG_PRINT("index_stat", ("using existing index stats events"));
    DBUG_RETURN(0);
  }

  if (is->create_sysevents(ndb) == 0)
  {
    DBUG_PRINT("index_stat", ("created index stats events"));
    DBUG_RETURN(0);
  }

  if (is->getNdbError().code == 746)
  {
    // race between mysqlds, maybe
    DBUG_PRINT("index_stat", ("create index stats events failed: error %d line %d",
                              is->getNdbError().code, is->getNdbError().line));
    DBUG_RETURN(-1);
  }

  sql_print_warning("create index stats events failed: error %d line %d",
                    is->getNdbError().code, is->getNdbError().line);
  DBUG_RETURN(-1);
}

int
ndb_index_stat_start_listener(Ndb_index_stat_proc &pr)
{
  DBUG_ENTER("ndb_index_stat_start_listener");

  NdbIndexStat *is= pr.is_util;
  Ndb *ndb= pr.ndb;

  if (is->create_listener(ndb) == -1)
  {
    sql_print_warning("create index stats listener failed: error %d line %d",
                      is->getNdbError().code, is->getNdbError().line);
    DBUG_RETURN(-1);
  }

  if (is->execute_listener(ndb) == -1)
  {
    sql_print_warning("execute index stats listener failed: error %d line %d",
                      is->getNdbError().code, is->getNdbError().line);
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

int
ndb_index_stat_stop_listener(Ndb_index_stat_proc &pr)
{
  DBUG_ENTER("ndb_index_stat_stop_listener");

  NdbIndexStat *is= pr.is_util;
  Ndb *ndb= pr.ndb;

  if (is->drop_listener(ndb) == -1)
  {
    sql_print_warning("drop index stats listener failed: error %d line %d",
                      is->getNdbError().code, is->getNdbError().line);
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

pthread_handler_t
ndb_index_stat_thread_func(void *arg __attribute__((unused)))
{
  THD *thd; /* needs to be first for thread_stack */
  struct timespec abstime;
  Thd_ndb *thd_ndb= NULL;

  my_thread_init();
  DBUG_ENTER("ndb_index_stat_thread_func");

  Ndb_index_stat_proc pr;

  bool have_listener;
  have_listener= false;

  // wl4124_todo remove useless stuff copied from utility thread
 
  pthread_mutex_lock(&LOCK_ndb_index_stat_thread);

  thd= new THD; /* note that contructor of THD uses DBUG_ */
  if (thd == NULL)
  {
    my_errno= HA_ERR_OUT_OF_MEM;
    DBUG_RETURN(NULL);
  }
  THD_CHECK_SENTRY(thd);
  pthread_detach_this_thread();
  ndb_index_stat_thread= pthread_self();

  thd->thread_stack= (char*)&thd; /* remember where our stack is */
  if (thd->store_globals())
    goto ndb_index_stat_thread_fail;
  lex_start(thd);
  thd->init_for_queries();
#ifndef NDB_THD_HAS_NO_VERSION
  thd->version=refresh_version;
#endif
  thd->client_capabilities = 0;
  thd->security_ctx->skip_grants();
  my_net_init(&thd->net, 0);

  CHARSET_INFO *charset_connection;
  charset_connection= get_charset_by_csname("utf8",
                                            MY_CS_PRIMARY, MYF(MY_WME));
  thd->variables.character_set_client= charset_connection;
  thd->variables.character_set_results= charset_connection;
  thd->variables.collation_connection= charset_connection;
  thd->update_charset();

  /* Signal successful initialization */
  ndb_index_stat_thread_running= 1;
  pthread_cond_signal(&COND_ndb_index_stat_ready);
  pthread_mutex_unlock(&LOCK_ndb_index_stat_thread);

  /*
    wait for mysql server to start
  */
  mysql_mutex_lock(&LOCK_server_started);
  while (!mysqld_server_started)
  {
    set_timespec(abstime, 1);
    mysql_cond_timedwait(&COND_server_started, &LOCK_server_started,
	                 &abstime);
    if (ndbcluster_terminating)
    {
      mysql_mutex_unlock(&LOCK_server_started);
      pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
      goto ndb_index_stat_thread_end;
    }
  }
  mysql_mutex_unlock(&LOCK_server_started);

  /*
    Wait for cluster to start
  */
  pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
  while (!g_ndb_status.cluster_node_id && (ndbcluster_hton->slot != ~(uint)0))
  {
    /* ndb not connected yet */
    pthread_cond_wait(&COND_ndb_index_stat_thread, &LOCK_ndb_index_stat_thread);
    if (ndbcluster_terminating)
      goto ndb_index_stat_thread_end;
  }
  pthread_mutex_unlock(&LOCK_ndb_index_stat_thread);

  /* Get instance used for sys objects check and create */
  if (!(pr.is_util= new NdbIndexStat))
  {
    sql_print_error("Could not allocate NdbIndexStat is_util object");
    pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
    goto ndb_index_stat_thread_end;
  }

  /* Get thd_ndb for this thread */
  if (!(thd_ndb= Thd_ndb::seize(thd)))
  {
    sql_print_error("Could not allocate Thd_ndb object");
    pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
    goto ndb_index_stat_thread_end;
  }
  set_thd_ndb(thd, thd_ndb);
  thd_ndb->options|= TNO_NO_LOG_SCHEMA_OP;
  if (thd_ndb->ndb->setDatabaseName(NDB_INDEX_STAT_DB) == -1)
  {
    sql_print_error("Could not change index stats thd_ndb database to %s",
                    NDB_INDEX_STAT_DB);
    pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
    goto ndb_index_stat_thread_end;
  }
  pr.ndb= thd_ndb->ndb;

  ndb_index_stat_allow(1);
  bool enable_ok;
  enable_ok= false;

  set_timespec(abstime, 0);
  for (;;)
  {
    pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
    if (!ndbcluster_terminating) {
      int ret= pthread_cond_timedwait(&COND_ndb_index_stat_thread,
                                      &LOCK_ndb_index_stat_thread,
                                      &abstime);
      const char* reason= ret == ETIMEDOUT ? "timed out" : "wake up";
      (void)reason; // USED
      DBUG_PRINT("index_stat", ("loop: %s", reason));
    }
    if (ndbcluster_terminating) /* Shutting down server */
      goto ndb_index_stat_thread_end;
    pthread_mutex_unlock(&LOCK_ndb_index_stat_thread);

    /* const bool enable_ok_new= THDVAR(NULL, index_stat_enable); */
    const bool enable_ok_new= ndb_index_stat_get_enable(NULL);

    do
    {
      if (enable_ok != enable_ok_new)
      {
        DBUG_PRINT("index_stat", ("global enable: %d -> %d",
                                  enable_ok, enable_ok_new));

        if (enable_ok_new)
        {
          // at enable check or create stats tables and events
          if (ndb_index_stat_check_or_create_systables(pr) == -1 ||
              ndb_index_stat_check_or_create_sysevents(pr) == -1 ||
              ndb_index_stat_start_listener(pr) == -1)
          {
            // try again in next loop
            break;
          }
          have_listener= true;
        }
        else
        {
          // not a normal use-case
          if (have_listener)
          {
            if (ndb_index_stat_stop_listener(pr) == 0)
              have_listener= false;
          }
        }
        enable_ok= enable_ok_new;
      }

      if (!enable_ok)
        break;

      pr.busy= false;
      ndb_index_stat_proc(pr);
    } while (0);

    /* Calculate new time to wake up */

    const Ndb_index_stat_opt &opt= ndb_index_stat_opt;
    uint msecs= 0;
    if (!enable_ok)
      msecs= opt.get(Ndb_index_stat_opt::Iloop_checkon);
    else if (!pr.busy)
      msecs= opt.get(Ndb_index_stat_opt::Iloop_idle);
    else
      msecs= opt.get(Ndb_index_stat_opt::Iloop_busy);
    DBUG_PRINT("index_stat", ("sleep %dms", msecs));

    set_timespec_nsec(abstime, msecs * 1000000ULL);
  }

ndb_index_stat_thread_end:
  net_end(&thd->net);

ndb_index_stat_thread_fail:
  if (have_listener)
  {
    if (ndb_index_stat_stop_listener(pr) == 0)
      have_listener= false;
  }
  if (pr.is_util)
  {
    delete pr.is_util;
    pr.is_util= 0;
  }
  if (thd_ndb)
  {
    Thd_ndb::release(thd_ndb);
    set_thd_ndb(thd, NULL);
  }
  thd->cleanup();
  delete thd;
  
  /* signal termination */
  ndb_index_stat_thread_running= 0;
  pthread_cond_signal(&COND_ndb_index_stat_ready);
  pthread_mutex_unlock(&LOCK_ndb_index_stat_thread);
  DBUG_PRINT("exit", ("ndb_index_stat_thread"));

  DBUG_LEAVE;
  my_thread_end();
  pthread_exit(0);
  return NULL;
}

/* Optimizer queries */

static ulonglong
ndb_index_stat_round(double x)
{
  char buf[100];
  if (x < 0.0)
    x= 0.0;
  // my_snprintf has no float and windows has no snprintf
  sprintf(buf, "%.0f", x);
  /* mysql provides strtoull */
  ulonglong n= strtoull(buf, 0, 10);
  return n;
}

int
ndb_index_stat_wait(Ndb_index_stat *st,
                    uint sample_version,
                    bool from_analyze)
{
  DBUG_ENTER("ndb_index_stat_wait");

  pthread_mutex_lock(&ndb_index_stat_stat_mutex);
  int err= 0;
  uint count= 0;
  struct timespec abstime;
  while (true)
  {
    int ret= 0;
    if (count == 0)
    {
      if (st->lt == Ndb_index_stat::LT_Error && !from_analyze)
      {
        err= Ndb_index_stat_error_HAS_ERROR;
        break;
      }
      ndb_index_stat_clear_error(st);
    }
    if (st->no_stats && !from_analyze)
    {
      /* Have detected no stats now or before */
      err= NdbIndexStat::NoIndexStats;
      break;
    }
    if (st->error.code != 0)
    {
      /* A new error has occured */
      err= st->error.code;
      break;
    }
    if (st->sample_version > sample_version)
      break;
    DBUG_PRINT("index_stat", ("st %s wait count:%u",
                              st->id, ++count));
    pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
    pthread_cond_signal(&COND_ndb_index_stat_thread);
    pthread_mutex_unlock(&LOCK_ndb_index_stat_thread);
    set_timespec(abstime, 1);
    ret= pthread_cond_timedwait(&ndb_index_stat_stat_cond,
                                &ndb_index_stat_stat_mutex,
                                &abstime);
    if (ret != 0 && ret != ETIMEDOUT)
    {
      err= ret;
      break;
    }
  }
  pthread_mutex_unlock(&ndb_index_stat_stat_mutex);
  if (err != 0)
  {
    DBUG_PRINT("index_stat", ("st %s wait error: %d",
                               st->id, err));
    DBUG_RETURN(err);
  }
  DBUG_PRINT("index_stat", ("st %s wait ok: sample_version %u -> %u",
                            st->id, sample_version, st->sample_version));
  DBUG_RETURN(0);
}

int
ha_ndbcluster::ndb_index_stat_query(uint inx,
                                    const key_range *min_key,
                                    const key_range *max_key,
                                    NdbIndexStat::Stat& stat,
                                    int from)
{
  DBUG_ENTER("ha_ndbcluster::ndb_index_stat_query");

  const KEY *key_info= table->key_info + inx;
  const NDB_INDEX_DATA &data= m_index[inx];
  const NDBINDEX *index= data.index;
  DBUG_PRINT("index_stat", ("index: %u name: %s", inx, index->getName()));

  int err= 0;

  /* Create an IndexBound struct for the keys */
  NdbIndexScanOperation::IndexBound ib;
  compute_index_bounds(ib, key_info, min_key, max_key, from);
  ib.range_no= 0;

  Ndb_index_stat *st=
    ndb_index_stat_get_share(m_share, index, m_table, err, true, false);
  if (st == 0)
    DBUG_RETURN(err);

  /* Pass old version 0 so existing stats terminates wait at once */
  err= ndb_index_stat_wait(st, 0, false);
  if (err != 0)
    DBUG_RETURN(err);

  if (st->read_time == 0)
  {
    DBUG_PRINT("index_stat", ("no index stats"));
    pthread_mutex_lock(&LOCK_ndb_index_stat_thread);
    pthread_cond_signal(&COND_ndb_index_stat_thread);
    pthread_mutex_unlock(&LOCK_ndb_index_stat_thread);
    DBUG_RETURN(NdbIndexStat::NoIndexStats);
  }

  uint8 bound_lo_buffer[NdbIndexStat::BoundBufferBytes];
  uint8 bound_hi_buffer[NdbIndexStat::BoundBufferBytes];
  NdbIndexStat::Bound bound_lo(st->is, bound_lo_buffer);
  NdbIndexStat::Bound bound_hi(st->is, bound_hi_buffer);
  NdbIndexStat::Range range(bound_lo, bound_hi);

  const NdbRecord* key_record= data.ndb_record_key;
  if (st->is->convert_range(range, key_record, &ib) == -1)
  {
    ndb_index_stat_error(st, "convert_range", __LINE__);
    DBUG_RETURN(st->error.code);
  }
  if (st->is->query_stat(range, stat) == -1)
  {
    /* Invalid cache - should remove the entry */
    ndb_index_stat_error(st, "query_stat", __LINE__);
    DBUG_RETURN(st->error.code);
  }

  DBUG_RETURN(0);
}

int
ha_ndbcluster::ndb_index_stat_get_rir(uint inx,
                                      key_range *min_key,
                                      key_range *max_key,
                                      ha_rows *rows_out)
{
  DBUG_ENTER("ha_ndbcluster::ndb_index_stat_get_rir");
  uint8 stat_buffer[NdbIndexStat::StatBufferBytes];
  NdbIndexStat::Stat stat(stat_buffer);
  int err= ndb_index_stat_query(inx, min_key, max_key, stat, 1);
  if (err == 0)
  {
    double rir= -1.0;
    NdbIndexStat::get_rir(stat, &rir);
    ha_rows rows= ndb_index_stat_round(rir);
    /* Estimate only so cannot return exact zero */
    if (rows == 0)
      rows= 1;
    *rows_out= rows;
#ifndef DBUG_OFF
    char rule[NdbIndexStat::RuleBufferBytes];
    NdbIndexStat::get_rule(stat, rule);
#endif
    DBUG_PRINT("index_stat", ("rir: %u rule: %s", (uint)rows, rule));
    DBUG_RETURN(0);
  }
  DBUG_RETURN(err);
}

int
ha_ndbcluster::ndb_index_stat_set_rpk(uint inx)
{
  DBUG_ENTER("ha_ndbcluster::ndb_index_stat_set_rpk");

  KEY *key_info= table->key_info + inx;
  int err= 0;

  uint8 stat_buffer[NdbIndexStat::StatBufferBytes];
  NdbIndexStat::Stat stat(stat_buffer);
  const key_range *min_key= 0;
  const key_range *max_key= 0;
  err= ndb_index_stat_query(inx, min_key, max_key, stat, 2);
  if (err == 0)
  {
    uint k;
    for (k= 0; k < key_info->user_defined_key_parts; k++)
    {
      double rpk= -1.0;
      NdbIndexStat::get_rpk(stat, k, &rpk);
      ulonglong recs= ndb_index_stat_round(rpk);
      key_info->rec_per_key[k]= (ulong)recs;
#ifndef DBUG_OFF
      char rule[NdbIndexStat::RuleBufferBytes];
      NdbIndexStat::get_rule(stat, rule);
#endif
      DBUG_PRINT("index_stat", ("rpk[%u]: %u rule: %s", k, (uint)recs, rule));
    }
    DBUG_RETURN(0);
  }
  DBUG_RETURN(err);
}

int
ha_ndbcluster::ndb_index_stat_analyze(Ndb *ndb,
                                      uint *inx_list,
                                      uint inx_count)
{
  DBUG_ENTER("ha_ndbcluster::ndb_index_stat_analyze");

  struct {
    uint sample_version;
    uint error_count;
  } old[MAX_INDEXES];

  int err= 0;
  uint i;

  /* Force stats update on each index */
  for (i= 0; i < inx_count; i++)
  {
    uint inx= inx_list[i];
    const NDB_INDEX_DATA &data= m_index[inx];
    const NDBINDEX *index= data.index;
    DBUG_PRINT("index_stat", ("force update: %s", index->getName()));

    Ndb_index_stat *st=
      ndb_index_stat_get_share(m_share, index, m_table, err, true, true);
    if (st == 0)
      DBUG_RETURN(err);

    old[i].sample_version= st->sample_version;
    old[i].error_count= st->error_count;
  }

  /* Wait for each update (or error) */
  for (i = 0; i < inx_count; i++)
  {
    uint inx= inx_list[i];
    const NDB_INDEX_DATA &data= m_index[inx];
    const NDBINDEX *index= data.index;
    DBUG_PRINT("index_stat", ("wait for update: %s", index->getName()));

    Ndb_index_stat *st=
      ndb_index_stat_get_share(m_share, index, m_table, err, false, false);
    if (st == 0)
      DBUG_RETURN(err);

    err= ndb_index_stat_wait(st, old[i].sample_version, true);
    if (err != 0)
      DBUG_RETURN(err);
  }

  DBUG_RETURN(0);
}

#endif
