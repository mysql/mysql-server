/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/utils/enumeration_utils.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <algorithm>
#include <list>

#include "m_string.h"  // my_strtoll
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_stacktrace.h"  // my_safe_printf_stderr
#include "my_sys.h"
#include "mysql/binlog/event/control_events.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/int2str.h"
#include "prealloced_array.h"
#include "sql/rpl_gtid.h"
#include "sql/sql_const.h"
#include "sql/thr_malloc.h"

#ifdef MYSQL_SERVER
#include "mysql/psi/psi_memory.h"
#include "mysqld_error.h"  // ER_*
#include "sql/log.h"
#endif

#ifndef MYSQL_SERVER
#include "client/mysqlbinlog.h"
#endif

PSI_memory_key key_memory_Gtid_set_to_string;
PSI_memory_key key_memory_Gtid_set_Interval_chunk;

using std::list;
using std::max;
using std::min;

#define MAX_NEW_CHUNK_ALLOCATE_TRIES 10

PSI_mutex_key Gtid_set::key_gtid_executed_free_intervals_mutex;

const Gtid_set::String_format Gtid_set::default_string_format = {
    "", "", ":", ":", "-", ":", ",\n", "", 0, 0, 1, 1, 1, 1, 2, 0};

const Gtid_set::String_format Gtid_set::sql_string_format = {
    "'", "'", ":", ":", "-", ":", "',\n'", "''", 1, 1, 1, 1, 1, 1, 4, 2};

const Gtid_set::String_format Gtid_set::commented_string_format = {
    "# ", "", ":", ":", "-", ":", ",\n# ", "# [empty]", 2, 0, 1, 1, 1, 1, 4, 9};

Gtid_set::Gtid_set(Tsid_map *_tsid_map, Checkable_rwlock *_tsid_lock)
    : tsid_lock(_tsid_lock),
      tsid_map(_tsid_map),
      m_intervals(key_memory_Gtid_set_Interval_chunk) {
  init();
}

Gtid_set::Gtid_set(Tsid_map *_tsid_map, const char *text,
                   enum_return_status *status, Checkable_rwlock *_tsid_lock)
    : tsid_lock(_tsid_lock),
      tsid_map(_tsid_map),
      m_intervals(key_memory_Gtid_set_Interval_chunk) {
  assert(_tsid_map != nullptr);
  init();
  *status = add_gtid_text(text);
}

void Gtid_set::init() {
  DBUG_TRACE;
  has_cached_string_length = false;
  cached_string_length = 0;
  cached_string_format = nullptr;
  chunks = nullptr;
  free_intervals = nullptr;
  if (tsid_lock)
    mysql_mutex_init(key_gtid_executed_free_intervals_mutex,
                     &free_intervals_mutex, nullptr);
#ifndef NDEBUG
  n_chunks = 0;
#endif
}

Gtid_set::~Gtid_set() {
  DBUG_TRACE;
  Interval_chunk *chunk = chunks;
  while (chunk != nullptr) {
    Interval_chunk *next_chunk = chunk->next;
    my_free(chunk);
    chunk = next_chunk;
#ifndef NDEBUG
    n_chunks--;
#endif
  }
  assert(n_chunks == 0);
  if (tsid_lock) mysql_mutex_destroy(&free_intervals_mutex);
}

enum_return_status Gtid_set::ensure_sidno(rpl_sidno sidno) {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_lock();
  DBUG_PRINT("info", ("sidno=%d get_max_sidno()=%d tsid_map=%p "
                      "sid_map->get_max_sidno()=%d",
                      sidno, get_max_sidno(), tsid_map,
                      tsid_map != nullptr ? tsid_map->get_max_sidno() : 0));
  assert(sidno <= tsid_map->get_max_sidno());
  assert(get_max_sidno() <= tsid_map->get_max_sidno());
  rpl_sidno max_sidno = get_max_sidno();
  if (sidno > max_sidno) {
    /*
      Not all Gtid_sets are protected by an rwlock.  But if this
      Gtid_set is, we assume that the read lock has been taken.
      Then we temporarily upgrade it to a write lock while resizing
      the array, and then we restore it to a read lock at the end.
    */
    bool is_wrlock = false;
    if (tsid_lock != nullptr) {
      is_wrlock = tsid_lock->is_wrlock();
      if (!is_wrlock) {
        tsid_lock->unlock();
        tsid_lock->wrlock();
        // maybe a concurrent thread already resized the Gtid_set
        // while we released the lock; check the condition again
        if (sidno <= max_sidno) {
          tsid_lock->unlock();
          tsid_lock->rdlock();
          RETURN_OK;
        }
      }
    }
    Interval *null_p = nullptr;
    for (rpl_sidno i = max_sidno; i < sidno; i++)
      if (m_intervals.push_back(null_p)) goto error;
    if (tsid_lock != nullptr) {
      if (!is_wrlock) {
        tsid_lock->unlock();
        tsid_lock->rdlock();
      }
    }
  }
  RETURN_OK;
error:
  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}

void Gtid_set::add_interval_memory_lock_taken(int n_ivs, Interval *ivs) {
  DBUG_TRACE;
  assert_free_intervals_locked();
  // make ivs a linked list
  for (int i = 0; i < n_ivs - 1; i++) ivs[i].next = &(ivs[i + 1]);
  Interval_iterator ivit(this);
  ivs[n_ivs - 1].next = ivit.get();
  // add intervals to list of free intervals
  ivit.set(&(ivs[0]));
}

void Gtid_set::create_new_chunk(int size) {
  DBUG_TRACE;
  int i = 0;
  Interval_chunk *new_chunk = nullptr;

  assert_free_intervals_locked();
  /*
    Try to allocate the new chunk in MAX_NEW_CHUNK_ALLOCATE_TRIES
    tries when encountering 'out of memory' situation.
  */
  while (i < MAX_NEW_CHUNK_ALLOCATE_TRIES) {
    /*
      Allocate the new chunk. one element is already pre-allocated, so
      we only add size-1 elements to the size of the struct.
    */
    new_chunk = (Interval_chunk *)my_malloc(
        key_memory_Gtid_set_Interval_chunk,
        sizeof(Interval_chunk) + sizeof(Interval) * (size - 1), MYF(MY_WME));
    if (new_chunk != nullptr) {
#ifdef MYSQL_SERVER
      if (i > 0)
        LogErr(WARNING_LEVEL, ER_RPL_GTID_MEMORY_FINALLY_AVAILABLE, i + 1);
#endif
      break;
    }
    /* Sleep 1 microsecond per try to avoid temporary 'out of memory' */
    my_sleep(1);
    i++;
  }
  /*
    Terminate the server after failed to allocate the new chunk
    in MAX_NEW_CHUNK_ALLOCATE_TRIES tries.
  */
  if (MAX_NEW_CHUNK_ALLOCATE_TRIES == i ||
      DBUG_EVALUATE_IF("rpl_simulate_new_chunk_allocate_failure", 1, 0)) {
    my_safe_print_system_time();
    my_safe_printf_stderr("%s",
                          "[Fatal] Out of memory while allocating "
                          "a new chunk of intervals for storing GTIDs.\n");
    _exit(MYSQLD_FAILURE_EXIT);
  }
  // store the chunk in the list of chunks
  new_chunk->next = chunks;
  chunks = new_chunk;
#ifndef NDEBUG
  n_chunks++;
#endif
  // add the intervals in the chunk to the list of free intervals
  add_interval_memory_lock_taken(size, new_chunk->intervals);
}

void Gtid_set::get_free_interval(Interval **out) {
  DBUG_TRACE;
  assert_free_intervals_locked();
  Interval_iterator ivit(this);
  bool simulate_failure = DBUG_EVALUATE_IF(
      "rpl_gtid_get_free_interval_simulate_out_of_memory", true, false);
  if (simulate_failure) DBUG_SET("+d,rpl_simulate_new_chunk_allocate_failure");
  if (ivit.get() == nullptr || simulate_failure)
    create_new_chunk(CHUNK_GROW_SIZE);
  *out = ivit.get();
  ivit.set((*out)->next);
}

void Gtid_set::put_free_interval(Interval *iv) {
  DBUG_TRACE;
  assert_free_intervals_locked();
  Interval_iterator ivit(this);
  iv->next = ivit.get();
  ivit.set(iv);
}

void Gtid_set::clear() {
  DBUG_TRACE;
  has_cached_string_length = false;
  cached_string_length = 0;
  rpl_sidno max_sidno = get_max_sidno();
  if (max_sidno == 0) return;
  Interval_iterator free_ivit(this);
  for (rpl_sidno sidno = 1; sidno <= max_sidno; sidno++) {
    /*
      Link in this list of intervals at the end of the list of
      free intervals.
    */
    Interval_iterator ivit(this, sidno);
    Interval *iv = ivit.get();
    if (iv != nullptr) {
      // find the end of the list of free intervals
      while (free_ivit.get() != nullptr) free_ivit.next();
      // append the present list
      free_ivit.set(iv);
      // clear the pointer to the head of this list
      ivit.set(nullptr);
    }
  }
}

void Gtid_set::clear_set_and_tsid_map() {
  DBUG_TRACE;
  clear();
  /*
    Cleaning the TSID map without cleaning up the Gtid_set intervals may lead
    to a condition were the Gtid_set->get_max_sidno() will be greater than the
    Tsid_map->get_max_sidno().
  */
  m_intervals.clear();
  tsid_map->clear();
  assert(get_max_sidno() == tsid_map->get_max_sidno());
}

void Gtid_set::add_gno_interval(Interval_iterator *ivitp, rpl_gno start,
                                rpl_gno end, Free_intervals_lock *lock) {
  DBUG_TRACE;
  assert(start > 0);
  assert(start < end);
  DBUG_PRINT("info", ("start=%" PRId64 " end=%" PRId64, start, end));
  Interval *iv;
  Interval_iterator ivit = *ivitp;
  has_cached_string_length = false;
  cached_string_length = 0;

  while ((iv = ivit.get()) != nullptr) {
    if (iv->end >= start) {
      if (iv->start > end)
        // (start, end) is strictly before the current interval
        break;
      // (start, end) and (iv->start, iv->end) touch or intersect.
      // Save the start of the merged interval.
      if (iv->start < start) start = iv->start;
      // Remove the current interval as long as the new interval
      // intersects with the next interval.
      while (iv->next && end >= iv->next->start) {
        lock->lock_if_not_locked();
        ivit.remove(this);
        iv = ivit.get();
      }
      // Store the interval in the current interval.
      iv->start = start;
      if (iv->end < end) iv->end = end;
      *ivitp = ivit;
      return;
    }
    ivit.next();
  }
  /*
    We come here if the interval cannot be combined with any existing
    interval: it is after the previous interval (if any) and before
    the current interval (if any). So we allocate a new interval and
    insert it at the current position.
  */
  Interval *new_iv;
  lock->lock_if_not_locked();
  get_free_interval(&new_iv);
  new_iv->start = start;
  new_iv->end = end;
  ivit.insert(new_iv);
  *ivitp = ivit;
}

void Gtid_set::remove_gno_interval(Interval_iterator *ivitp, rpl_gno start,
                                   rpl_gno end, Free_intervals_lock *lock) {
  DBUG_TRACE;
  assert(start < end);
  Interval_iterator ivit = *ivitp;
  Interval *iv;
  has_cached_string_length = false;
  cached_string_length = -1;

  // Skip intervals of 'this' that are completely before the removed interval.
  while (true) {
    iv = ivit.get();
    if (iv == nullptr) goto ok;
    if (iv->end > start) break;
    ivit.next();
  }

  // Now iv ends after the beginning of the removed interval.
  assert(iv != nullptr && iv->end > start);
  if (iv->start < start) {
    if (iv->end > end) {
      // iv cuts also the end of the removed interval: split iv in two
      Interval *new_iv;
      lock->lock_if_not_locked();
      get_free_interval(&new_iv);
      new_iv->start = end;
      new_iv->end = iv->end;
      iv->end = start;
      ivit.next();
      ivit.insert(new_iv);
      goto ok;
    }
    // iv cuts the beginning but not the end of the removed interval:
    // truncate iv, and iterate one step to next interval
    iv->end = start;
    ivit.next();
    iv = ivit.get();
    if (iv == nullptr) goto ok;
  }

  // Now iv starts after the beginning of the removed interval.
  assert(iv != nullptr && iv->start >= start);
  while (iv->end <= end) {
    // iv ends before the end of the removed interval, so it is
    // completely covered: remove iv.
    lock->lock_if_not_locked();
    ivit.remove(this);
    iv = ivit.get();
    if (iv == nullptr) goto ok;
  }

  // Now iv ends after the removed interval.
  assert(iv != nullptr && iv->end > end);
  if (iv->start < end) {
    // iv begins before the end of the removed interval: truncate iv
    iv->start = end;
  }

ok:
  *ivitp = ivit;
}

rpl_gno parse_gno(const char **s) {
  char *endp;
  long long ret = my_strtoll(*s, &endp, 0);
  if (ret < 0 || ret >= GNO_END) return -1;
  *s = endp;
  return static_cast<rpl_gno>(ret);
}

int format_gno(char *s, rpl_gno gno) {
  return static_cast<int>(longlong10_to_str(gno, s, 10) - s);
}

enum_return_status Gtid_set::add_gtid(const mysql::gtid::Gtid &gtid) {
  DBUG_TRACE;
  assert(tsid_map != nullptr);
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();

  rpl_sidno sidno = tsid_map->add_tsid(gtid.get_tsid());
  if (sidno <= 0) {
    RETURN_REPORTED_ERROR;
  }
  PROPAGATE_REPORTED_ERROR(ensure_sidno(sidno));
  _add_gtid(sidno, gtid.get_gno());
  return RETURN_STATUS_OK;
}

enum_return_status Gtid_set::add_gtid_text(const char *text, bool *anonymous,
                                           bool *starts_with_plus) {
  DBUG_TRACE;
  assert(tsid_map != nullptr);
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  const char *s = text;

  DBUG_PRINT("info", ("adding '%s'", text));

  if (anonymous != nullptr) *anonymous = false;

  if (starts_with_plus != nullptr) {
    SKIP_WHITESPACE();
    if (*s == '+') {
      *starts_with_plus = true;
      s++;
    } else
      *starts_with_plus = false;
  }
  SKIP_WHITESPACE();
  if (*s == 0) {
    DBUG_PRINT("info", ("'%s' is empty", text));
    RETURN_OK;
  }

  Free_intervals_lock lock(this);

  DBUG_PRINT("info", ("'%s' not only whitespace", text));
  // Allocate space for all intervals at once, if nothing is allocated.
  if (chunks == nullptr) {
    // compute number of intervals in text: it is equal to the number of
    // colons
    int n_intervals = 0;
    text = s;
    for (; *s; s++)
      if (*s == Gtid::gtid_separator) n_intervals++;
    // allocate all intervals in one chunk
    lock.lock_if_not_locked();
    create_new_chunk(n_intervals);
    lock.unlock_if_locked();
    s = text;
  }

  while (true) {
    // Skip commas (we allow empty SID:GNO specifications).
    while (*s == ',') {
      s++;
      SKIP_WHITESPACE();
    }

    // We allow empty Gtid_sets containing only commas.
    if (*s == 0) {
      DBUG_PRINT("info", ("successfully parsed"));
      RETURN_OK;
    }

    if (anonymous != nullptr && strncmp(s, "ANONYMOUS", 9) == 0) {
      *anonymous = true;
      s += 9;
    } else {
      // Parse TSID.
      mysql::gtid::Tsid tsid;
      std::size_t characters_read = tsid.from_cstring(s);
      if (characters_read == 0) {
        DBUG_PRINT("info",
                   ("expected UUID; found garbage '%.80s' at char %d in '%s'",
                    s, (int)(s - text), text));
        goto parse_error;
      }
      s += characters_read;
      rpl_sidno sidno = tsid_map->add_tsid(tsid);
      if (sidno <= 0) {
        RETURN_REPORTED_ERROR;
      }
      PROPAGATE_REPORTED_ERROR(ensure_sidno(sidno));
      SKIP_WHITESPACE();
      while (*s == Gtid::gtid_separator) {
        // Skip gtid_separator.
        ++s;
        // parse Tag if any
        mysql::gtid::Tag tag;
        auto tag_chars = tag.from_cstring(s);
        s += tag_chars;
        SKIP_WHITESPACE();
        if (tag_chars > 0) {
          tsid = mysql::gtid::Tsid(tsid.get_uuid(), tag);
          sidno = tsid_map->add_tsid(tsid);
          if (sidno <= 0) {
            RETURN_REPORTED_ERROR;
          }
          PROPAGATE_REPORTED_ERROR(ensure_sidno(sidno));
        } else {
          Interval_iterator ivit(this, sidno);
          // Read start of interval.
          rpl_gno start = parse_gno(&s);
          if (start <= 0) {
            if (start == 0)
              DBUG_PRINT("info", ("expected positive NUMBER; found zero "
                                  "('%.80s') at char %d in '%s'",
                                  s - 1, (int)(s - text) - 1, text));
            else
              DBUG_PRINT("info", ("expected positive NUMBER; found zero or "
                                  "garbage '%.80s' at char %d in '%s'",
                                  s, (int)(s - text), text));
            goto parse_error;
          }
          SKIP_WHITESPACE();

          // Read end of interval.
          rpl_gno end;
          if (*s == '-') {
            s++;
            end = parse_gno(&s);

            if (end < 0) {
              DBUG_PRINT(
                  "info",
                  ("expected NUMBER; found garbage '%.80s' at char %d in '%s'",
                   s, (int)(s - text), text));
              goto parse_error;
            }
            end++;
            SKIP_WHITESPACE();
          } else
            end = start + 1;

          if (end > start) {
            // Add interval.  Use the existing iterator position if the
            // current interval does not begin before it.  Otherwise iterate
            // from the beginning.
            Interval *current = ivit.get();
            if (current == nullptr || start < current->start)
              ivit.init(this, sidno);

            add_gno_interval(&ivit, start, end, &lock);
          }
        }
      }
    }
    // Must be end of string or comma. (Commas are consumed and
    // end-of-loop is detected at the beginning of the loop.)
    if (*s != ',' && *s != 0) {
      DBUG_PRINT("info", ("expected end of string, UUID, or :NUMBER; found "
                          "garbage '%.80s' at char %d in '%s'",
                          s, (int)(s - text), text));
      goto parse_error;
    }
  }
  assert(0);

parse_error:
  BINLOG_ERROR(("Malformed Gtid_set specification '%.200s'.", text),
               (ER_MALFORMED_GTID_SET_SPECIFICATION, MYF(0), text));
  RETURN_REPORTED_ERROR;
}

bool Gtid_set::is_valid(const char *text) {
  DBUG_TRACE;

  const char *s = text;

  SKIP_WHITESPACE();
  if (*s == '+') s++;
  SKIP_WHITESPACE();
  do {
    // Skip commas (we allow empty SID:GNO specifications).
    while (*s == ',') {
      s++;
      SKIP_WHITESPACE();
    }
    if (*s == 0) return true;
    // Parse SID.
    mysql::gtid::Uuid uuid;
    if (uuid.parse(s, mysql::gtid::Uuid::TEXT_LENGTH) != 0) {
      return false;
    }
    s += mysql::gtid::Uuid::TEXT_LENGTH;
    SKIP_WHITESPACE();
    mysql::gtid::Tag tag;  // empty tag
    // Iterate over intervals.
    while (*s == Gtid::gtid_separator) {
      // Skip gtid_separator.
      s++;
      // Parse the next "gtid_separator" separated item,
      // which may be either a tag or an interval
      SKIP_WHITESPACE();
      mysql::gtid::Tag tag_read;
      auto tag_len = tag_read.from_cstring(s);
      s += tag_len;
      if (tag_len > 0) {
        tag = tag_read;
      } else {
        // Read start of interval.
        if (parse_gno(&s) <= 0) return false;
        SKIP_WHITESPACE();
        // Read end of interval
        if (*s == '-') {
          s++;
          if (parse_gno(&s) < 0) return false;
          SKIP_WHITESPACE();
        }
      }
    }
  } while (*s == ',');
  if (*s != 0) return false;

  return true;
}

void Gtid_set::add_gno_intervals(rpl_sidno sidno,
                                 Const_interval_iterator other_ivit,
                                 Free_intervals_lock *lock) {
  DBUG_TRACE;
  assert(sidno >= 1 && sidno <= get_max_sidno());
  const Interval *other_iv;
  Interval_iterator ivit(this, sidno);
  while ((other_iv = other_ivit.get()) != nullptr) {
    add_gno_interval(&ivit, other_iv->start, other_iv->end, lock);
    other_ivit.next();
  }
}

void Gtid_set::remove_gno_intervals(rpl_sidno sidno,
                                    Const_interval_iterator other_ivit,
                                    Free_intervals_lock *lock) {
  DBUG_TRACE;
  assert(sidno >= 1 && sidno <= get_max_sidno());
  const Interval *other_iv;
  Interval_iterator ivit(this, sidno);
  while ((other_iv = other_ivit.get()) != nullptr) {
    remove_gno_interval(&ivit, other_iv->start, other_iv->end, lock);
    if (ivit.get() == nullptr) break;
    other_ivit.next();
  }
}

void Gtid_set::remove_intervals_for_sidno(Gtid_set *other, rpl_sidno sidno) {
  // Currently only works if this and other use the same Tsid_map.
  assert(other->tsid_map == tsid_map);
  Const_interval_iterator other_ivit(other, sidno);
  Free_intervals_lock lock(this);
  remove_gno_intervals(sidno, other_ivit, &lock);
}

enum_return_status Gtid_set::add_gtid_set(const Gtid_set *other) {
  /*
    @todo refactor this and remove_gtid_set to avoid duplicated code
  */
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  rpl_sidno max_other_sidno = other->get_max_sidno();
  Free_intervals_lock lock(this);
  if (other->tsid_map == tsid_map) {
    PROPAGATE_REPORTED_ERROR(ensure_sidno(max_other_sidno));
    for (rpl_sidno sidno = 1; sidno <= max_other_sidno; sidno++)
      add_gno_intervals(sidno, Const_interval_iterator(other, sidno), &lock);
  } else {
    Tsid_map *other_tsid_map = other->tsid_map;
    Checkable_rwlock *other_tsid_lock = other->tsid_lock;
    if (other_tsid_lock != nullptr) other_tsid_lock->assert_some_wrlock();
    for (rpl_sidno other_sidno = 1; other_sidno <= max_other_sidno;
         other_sidno++) {
      Const_interval_iterator other_ivit(other, other_sidno);
      if (other_ivit.get() != nullptr) {
        const auto &tsid = other_tsid_map->sidno_to_tsid(other_sidno);
        rpl_sidno this_sidno = tsid_map->add_tsid(tsid);
        if (this_sidno <= 0) RETURN_REPORTED_ERROR;
        PROPAGATE_REPORTED_ERROR(ensure_sidno(this_sidno));
        add_gno_intervals(this_sidno, other_ivit, &lock);
      }
    }
  }
  RETURN_OK;
}

void Gtid_set::remove_gtid_set(const Gtid_set *other) {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  rpl_sidno max_other_sidno = other->get_max_sidno();
  Free_intervals_lock lock(this);
  if (other->tsid_map == tsid_map) {
    rpl_sidno max_sidno = min(max_other_sidno, get_max_sidno());
    for (rpl_sidno sidno = 1; sidno <= max_sidno; sidno++)
      remove_gno_intervals(sidno, Const_interval_iterator(other, sidno), &lock);
  } else {
    Tsid_map *other_tsid_map = other->tsid_map;
    Checkable_rwlock *other_tsid_lock = other->tsid_lock;
    if (other_tsid_lock != nullptr) other_tsid_lock->assert_some_wrlock();
    for (rpl_sidno other_sidno = 1; other_sidno <= max_other_sidno;
         other_sidno++) {
      Const_interval_iterator other_ivit(other, other_sidno);
      if (other_ivit.get() != nullptr) {
        const auto &tsid = other_tsid_map->sidno_to_tsid(other_sidno);
        rpl_sidno this_sidno = tsid_map->tsid_to_sidno(tsid);
        if (this_sidno != 0)
          remove_gno_intervals(this_sidno, other_ivit, &lock);
      }
    }
  }
}

bool Gtid_set::contains_gtid(rpl_sidno sidno, rpl_gno gno) const {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_lock();
  if (sidno > get_max_sidno()) return false;
  assert(sidno >= 1);
  assert(gno >= 1);
  Const_interval_iterator ivit(this, sidno);
  const Interval *iv;
  while ((iv = ivit.get()) != nullptr) {
    if (gno < iv->start)
      return false;
    else if (gno < iv->end)
      return true;
    ivit.next();
  }
  return false;
}

rpl_gno Gtid_set::get_last_gno(rpl_sidno sidno) const {
  DBUG_TRACE;
  rpl_gno gno = 0;

  if (tsid_lock != nullptr) tsid_lock->assert_some_lock();

  if (sidno > get_max_sidno()) return gno;

  Const_interval_iterator ivit(this, sidno);
  const Gtid_set::Interval *iv = ivit.get();
  while (iv != nullptr) {
    gno = iv->end - 1;
    ivit.next();
    iv = ivit.get();
  }

  return gno;
}

long Gtid_set::to_string(char **buf_arg, bool need_lock,
                         const Gtid_set::String_format *sf_arg) const {
  DBUG_TRACE;
  if (tsid_lock != nullptr) {
    if (need_lock)
      tsid_lock->wrlock();
    else
      tsid_lock->assert_some_wrlock();
  }
  size_t len = get_string_length(sf_arg);
  *buf_arg =
      (char *)my_malloc(key_memory_Gtid_set_to_string, len + 1, MYF(MY_WME));
  if (*buf_arg == nullptr) return -1;
  to_string(*buf_arg, false /*need_lock*/, sf_arg);
  if (tsid_lock != nullptr && need_lock) tsid_lock->unlock();
  return len;
}

size_t Gtid_set::to_string(char *buf, bool need_lock,
                           const Gtid_set::String_format *sf) const {
  DBUG_TRACE;
  assert(tsid_map != nullptr);
  if (tsid_lock != nullptr) {
    if (need_lock)
      tsid_lock->wrlock();
    else
      tsid_lock->assert_some_wrlock();
  }
  if (sf == nullptr) sf = &default_string_format;
  if (sf->empty_set_string != nullptr && is_empty()) {
    memcpy(buf, sf->empty_set_string, sf->empty_set_string_length);
    buf[sf->empty_set_string_length] = '\0';
    if (tsid_lock != nullptr && need_lock) tsid_lock->unlock();
    return sf->empty_set_string_length;
  }
  assert(get_max_sidno() <= tsid_map->get_max_sidno());
  memcpy(buf, sf->begin, sf->begin_length);
  char *s = buf + sf->begin_length;
  bool first_sidno = true;
  mysql::gtid::Uuid prev_uuid;
  for (const auto &tsid_it : tsid_map->get_sorted_sidno()) {
    rpl_sidno sidno = tsid_it.second;
    if (contains_sidno(sidno)) {
      Const_interval_iterator ivit(this, sidno);
      const Interval *iv = ivit.get();
      auto tsid = tsid_map->sidno_to_tsid(sidno);
      // save UUID
      if (first_sidno || tsid.get_uuid() != prev_uuid) {
        if (first_sidno == false) {
          memcpy(s, sf->gno_sid_separator, sf->gno_sid_separator_length);
          s += sf->gno_sid_separator_length;
        }
        s += tsid.get_uuid().to_string(s);
        prev_uuid = tsid.get_uuid();
        first_sidno = false;
      }
      // save tag and intervals
      if (tsid.is_tagged()) {
        memcpy(s, sf->tag_sid_separator, strlen(sf->tag_sid_separator));
        s += strlen(sf->tag_sid_separator);
        s += tsid.get_tag().to_string(s);
      }
      bool first_gno = true;
      do {
        if (first_gno) {
          memcpy(s, sf->tsid_gno_separator, sf->tsid_gno_separator_length);
          s += sf->tsid_gno_separator_length;
        } else {
          memcpy(s, sf->gno_gno_separator, sf->gno_gno_separator_length);
          s += sf->gno_gno_separator_length;
        }
        s += format_gno(s, iv->start);
        if (iv->end > iv->start + 1) {
          memcpy(s, sf->gno_start_end_separator,
                 sf->gno_start_end_separator_length);
          s += sf->gno_start_end_separator_length;
          s += format_gno(s, iv->end - 1);
        }
        ivit.next();
        iv = ivit.get();
      } while (iv != nullptr);
    }
  }
  memcpy(s, sf->end, sf->end_length);
  s += sf->end_length;
  *s = '\0';

  DBUG_PRINT("info", ("ret='%s' strlen(s)=%zu s-buf=%lu get_string_length=%llu",
                      buf, strlen(buf), (ulong)(s - buf),
                      static_cast<unsigned long long>(get_string_length(sf))));
  assert((ulong)(s - buf) == get_string_length(sf));
  if (tsid_lock != nullptr && need_lock) tsid_lock->unlock();
  return (int)(s - buf);
}

void Gtid_set::get_gtid_intervals(list<Gtid_interval> *gtid_intervals) const {
  DBUG_TRACE;
  assert(tsid_map != nullptr);
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  assert(get_max_sidno() <= tsid_map->get_max_sidno());
  for (const auto &sid_it : tsid_map->get_sorted_sidno()) {
    rpl_sidno sidno = sid_it.second;
    if (contains_sidno(sidno)) {
      Const_interval_iterator ivit(this, sidno);
      const Interval *iv = ivit.get();
      while (iv != nullptr) {
        Gtid_interval gtid_interval;
        gtid_interval.set(sidno, iv->start, iv->end - 1);
        gtid_intervals->push_back(gtid_interval);
        ivit.next();
        iv = ivit.get();
      };
    }
  }
}

/**
  Returns the length that the given rpl_sidno (64 bit integer) would
  have, if it was encoded as a string.
*/
static size_t get_string_length(rpl_gno gno) {
  assert(gno >= 1);
  assert(gno < GNO_END);
  rpl_gno tmp_gno = gno;
  size_t len = 0;
  do {
    tmp_gno /= 10;
    len++;
  } while (tmp_gno != 0);
#ifndef NDEBUG
  char buf[22];
  assert(snprintf(buf, 22, "%" PRId64, gno) == ssize_t(len));
#endif
  return len;
}

bool Gtid_set::contains_tags() const {
  assert(tsid_map != nullptr);
  for (const auto &tsid_it : tsid_map->get_sorted_sidno()) {
    rpl_sidno sidno = tsid_it.second;
    if (contains_sidno(sidno)) {
      Const_interval_iterator ivit(this, sidno);
      if (ivit.get() != nullptr) {
        auto tsid = tsid_map->sidno_to_tsid(sidno);
        if (tsid.is_tagged()) {
          return true;
        }
      }
    }
  }
  return false;
}

size_t Gtid_set::get_string_length(const Gtid_set::String_format *sf) const {
  assert(tsid_map != nullptr);
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  if (sf == nullptr) sf = &default_string_format;
  if (has_cached_string_length == false || cached_string_format != sf) {
    int n_sids = 0, n_sidnos = 0, n_intervals = 0, n_long_intervals = 0;
    size_t total_interval_length = 0;
    size_t total_tsids_length = 0;
    mysql::gtid::Uuid prev_uuid;
    bool first_sidno = true;
    for (const auto &tsid_it : tsid_map->get_sorted_sidno()) {
      rpl_sidno sidno = tsid_it.second;
      if (contains_sidno(sidno)) {
        Const_interval_iterator ivit(this, sidno);
        const Interval *iv = ivit.get();
        if (iv != nullptr) {
          auto tsid = tsid_map->sidno_to_tsid(sidno);
          ++n_sidnos;
          if (tsid.is_tagged()) {
            total_tsids_length +=
                tsid.get_tag().get_length() + sf->tag_sid_separator_length;
          }
          if (first_sidno || tsid.get_uuid() != prev_uuid) {
            total_tsids_length += mysql::gtid::Uuid::TEXT_LENGTH;
            prev_uuid = tsid.get_uuid();
            first_sidno = false;
            ++n_sids;
          }
          do {
            total_interval_length += ::get_string_length(iv->start);
            ++n_intervals;
            if (iv->end - 1 > iv->start) {
              ++n_long_intervals;
              total_interval_length += ::get_string_length(iv->end - 1);
            }
            ivit.next();
            iv = ivit.get();
          } while (iv != nullptr);
        }
      }
    }
    if (n_sids == 0 && sf->empty_set_string != nullptr)
      cached_string_length = sf->empty_set_string_length;
    else {
      cached_string_length = sf->begin_length + sf->end_length;
      if (n_sids > 0)
        cached_string_length +=
            total_tsids_length + ((n_sids - 1) * sf->gno_sid_separator_length) +
            total_interval_length + n_sidnos * sf->tsid_gno_separator_length +
            (n_intervals - n_sidnos) * sf->gno_gno_separator_length +
            n_long_intervals * sf->gno_start_end_separator_length;
    }
    has_cached_string_length = true;
    cached_string_format = sf;
  }
  return cached_string_length;
}

bool Gtid_set::sidno_equals(rpl_sidno sidno, const Gtid_set *other,
                            rpl_sidno other_sidno) const {
  DBUG_TRACE;
  Const_interval_iterator ivit(this, sidno);
  Const_interval_iterator other_ivit(other, other_sidno);
  const Interval *iv = ivit.get();
  const Interval *other_iv = other_ivit.get();
  while (iv != nullptr && other_iv != nullptr) {
    if (!iv->equals(*other_iv)) return false;
    ivit.next();
    other_ivit.next();
    iv = ivit.get();
    other_iv = other_ivit.get();
  }
  if (iv != nullptr || other_iv != nullptr) return false;
  return true;
}

bool Gtid_set::equals(const Gtid_set *other) const {
  DBUG_TRACE;

  auto contains_any_sidno = [](auto tsid_iter, auto &tsid_map_sorted,
                               const Gtid_set *set) -> bool {
    while (tsid_iter != tsid_map_sorted.end()) {
      const auto &sidno = tsid_iter->second;
      if (set->contains_sidno(sidno)) {
        return true;
      }
      ++tsid_iter;
    }
    return false;
  };

  const Tsid_map *other_tsid_map = other->tsid_map;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  if (other->tsid_lock != nullptr) other->tsid_lock->assert_some_wrlock();

  assert(tsid_map != nullptr);
  assert(other_tsid_map != nullptr);

  const auto &map_sorted = tsid_map->get_sorted_sidno();
  const auto &other_map_sorted = other_tsid_map->get_sorted_sidno();
  auto other_tsid_it = other_map_sorted.begin();
  auto sid_it = map_sorted.begin();
  // iterate over potentially common sidnos
  while (sid_it != map_sorted.end() &&
         other_tsid_it != other_map_sorted.end()) {
    const auto &sidno = sid_it->second;
    const auto &other_sidno = other_tsid_it->second;
    // continue in case sidno from a tsid_map is not in corresponding GTID set
    if (!contains_sidno(sidno)) {
      ++sid_it;
      continue;
    }
    if (!other->contains_sidno(other_sidno)) {
      ++other_tsid_it;
      continue;
    }
    // compare tsids
    if (sid_it->first != other_tsid_it->first) {
      return false;
    }
    // comparing intervals
    if (!sidno_equals(sidno, other, other_sidno)) return false;
    ++sid_it;
    ++other_tsid_it;
  }
  // end of common sidnos, check that GTID sets do not contain other sidnos than
  // common ones
  return (contains_any_sidno(sid_it, map_sorted, this) == false) &&
         (contains_any_sidno(other_tsid_it, other_map_sorted, other) == false);
}

bool Gtid_set::is_interval_subset(Const_interval_iterator *sub,
                                  Const_interval_iterator *super) {
  DBUG_TRACE;
  // check if all intervals for this sidno are contained in some
  // interval of super
  const Interval *super_iv = super->get();
  const Interval *sub_iv = sub->get();

  /*
    Algorithm: Let sub_iv iterate over intervals of sub.  For each
    sub_iv, skip over intervals of super that end before sub_iv.  When we
    find the first super-interval that does not end before sub_iv,
    check if it covers sub_iv.
  */
  do {
    if (super_iv == nullptr) return false;

    // Skip over 'smaller' intervals of super.
    while (sub_iv->start > super_iv->end) {
      super->next();
      super_iv = super->get();
      // If we reach end of super, then no interval covers sub_iv, so
      // sub is not a subset of super.
      if (super_iv == nullptr) return false;
    }

    // If super_iv does not cover sub_iv, then sub is not a subset of
    // super.
    if (sub_iv->start < super_iv->start || sub_iv->end > super_iv->end)
      return false;

    // Next iteration.
    sub->next();
    sub_iv = sub->get();

  } while (sub_iv != nullptr);

  // If every GNO in sub also exists in super, then it was a subset.
  return true;
}

bool Gtid_set::is_subset_for_sid(const Gtid_set *super,
                                 const rpl_sid &sid) const {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  if (super->tsid_lock != nullptr) super->tsid_lock->assert_some_wrlock();
  auto *super_tsid_map = super->tsid_map;
  auto *tsid_map = this->tsid_map;
  assert(tsid_map != nullptr);
  assert(super_tsid_map != nullptr);
  const auto &sorted_sidno_data = tsid_map->get_sorted_sidno();
  for (auto tsid_item = sorted_sidno_data.lower_bound(sid);
       tsid_item != sorted_sidno_data.end(); ++tsid_item) {
    const auto &tsid = tsid_map->sidno_to_tsid(tsid_item->second);
    if (tsid.get_uuid() != sid) {
      return true;
    }
    auto super_sidno = super_tsid_map->tsid_to_sidno(tsid);
    auto this_sidno = tsid_map->tsid_to_sidno(tsid);
    if (!is_subset_for_sidno(super, super_sidno, this_sidno)) {
      return false;
    }
  }
  return true;
}

bool Gtid_set::is_subset_for_sidno(const Gtid_set *super,
                                   rpl_sidno superset_sidno,
                                   rpl_sidno subset_sidno) const {
  DBUG_TRACE;
  /*
    The following assert code is to see that caller acquired
    either write or read lock on global_tsid_lock.
    Note that if it is read lock, then it should also
    acquire lock on sidno.
    i.e., the caller must acquire lock either A1 way or A2 way.
        A1. global_tsid_lock.wrlock()
        A2. global_tsid_lock.rdlock(); gtid_state.lock_sidno(sidno)
  */
  if (tsid_lock != nullptr) super->tsid_lock->assert_some_wrlock();
  if (super->tsid_lock != nullptr) super->tsid_lock->assert_some_wrlock();
  /*
    If subset(i.e, this object) does not have required sid in it, i.e.,
    subset_sidno is zero, then it means it is subset of any given
    super set. Hence return true.
  */
  if (subset_sidno == 0) return true;
  /*
    If superset (i.e., the passed gtid_set) does not have given sid in it,
    i.e., superset_sidno is zero, then it means it cannot be superset
    to any given subset. Hence return false.
  */
  if (superset_sidno == 0) return false;
  /*
    Once we have valid(non-zero) subset's and superset's sid numbers, call
    is_interval_subset().
  */
  Const_interval_iterator subset_ivit(this, subset_sidno);
  Const_interval_iterator superset_ivit(super, superset_sidno);
  if (!is_interval_subset(&subset_ivit, &superset_ivit)) return false;

  return true;
}

bool Gtid_set::is_subset(const Gtid_set *super) const {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  if (super->tsid_lock != nullptr) super->tsid_lock->assert_some_wrlock();

  Tsid_map *super_tsid_map = super->tsid_map;
  rpl_sidno max_sidno = get_max_sidno();
  rpl_sidno super_max_sidno = super->get_max_sidno();

  assert(tsid_map != nullptr);
  assert(super_tsid_map != nullptr);

  /*
    Iterate over sidnos of this Gtid_set where there is at least one
    interval.  For each such sidno, get the corresponding sidno of
    super, and then use is_interval_subset to look for GTIDs that
    exist in this but not in super.
  */
  for (int sidno = 1; sidno <= max_sidno; sidno++) {
    Const_interval_iterator ivit(this, sidno);
    const Interval *iv = ivit.get();
    if (iv != nullptr) {
      // Get the corresponding super_sidno
      int super_sidno = sidno;
      if (super_tsid_map != tsid_map) {
        super_sidno =
            super_tsid_map->tsid_to_sidno(tsid_map->sidno_to_tsid(sidno));
        if (super_sidno == 0) return false;
      }
      if (super_sidno > super_max_sidno) return false;

      // Check if all GNOs in this Gtid_set for sidno exist in other
      // Gtid_set for super_
      Const_interval_iterator super_ivit(super, super_sidno);
      if (!is_interval_subset(&ivit, &super_ivit)) return false;
    }
  }

  // If the GNOs for every SIDNO of sub existed in super, then it was
  // a subset.
  return true;
}

bool Gtid_set::is_interval_intersection_nonempty(
    Const_interval_iterator *ivit1, Const_interval_iterator *ivit2) {
  DBUG_TRACE;
  const Interval *iv1 = ivit1->get();
  const Interval *iv2 = ivit2->get();
  assert(iv1 != nullptr);
  if (iv2 == nullptr) return false;

  /*
    Algorithm: Let iv1 iterate over all intervals of ivit1.  For each
    iv1, skip over intervals of iv2 that end before iv1.  When we
    reach the first interval that does not end before iv1, check if it
    intersects with iv1.
  */
  do {
    // Skip over intervals of iv2 that end before iv1.
    while (iv2->end <= iv1->start) {
      ivit2->next();
      iv2 = ivit2->get();
      // If we reached the end of ivit2, then there is no intersection.
      if (iv2 == nullptr) return false;
    }

    // If iv1 and iv2 intersect, return true.
    if (iv2->start < iv1->end) return true;

    // Next iteration.
    ivit1->next();
    iv1 = ivit1->get();

  } while (iv1 != nullptr);

  // If we iterated over all intervals of ivit1 without finding any
  // intersection with ivit2, then there is no intersection.
  return false;
}

bool Gtid_set::is_intersection_nonempty(const Gtid_set *other) const {
  DBUG_TRACE;
  /*
    This could in principle be implemented as follows:

    Gtid_set this_minus_other(tsid_map);
    this_minus_other.add_gtid_set(this);
    this_minus_other.remove_gtid_set(other);
    bool ret= equals(&this_minus_other);
    return ret;

    However, that does not check the return values from add_gtid_set
    or remove_gtid_set, and there is no way for this function to
    return an error.
  */
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  if (other->tsid_lock != nullptr) other->tsid_lock->assert_some_wrlock();

  Tsid_map *other_tsid_map = other->tsid_map;
  rpl_sidno max_sidno = get_max_sidno();
  rpl_sidno other_max_sidno = other->get_max_sidno();

  assert(tsid_map != nullptr);
  assert(other_tsid_map != nullptr);

  /*
    Algorithm: iterate over all sidnos of this Gtid_set where there is
    at least one interval.  For each such sidno, find the
    corresponding sidno of the other set.  Then use
    is_interval_intersection_nonempty to check if there are any GTIDs
    that are common to the two sets for this sidno.
  */
  for (int sidno = 1; sidno <= max_sidno; sidno++) {
    Const_interval_iterator ivit(this, sidno);
    const Interval *iv = ivit.get();
    if (iv != nullptr) {
      // Get the corresponding other_sidno.
      int other_sidno = sidno;
      if (other_tsid_map != tsid_map) {
        other_sidno =
            other_tsid_map->tsid_to_sidno(tsid_map->sidno_to_tsid(sidno));
        if (other_sidno == 0) continue;
      }
      if (other_sidno > other_max_sidno) continue;

      // Check if there is any GNO in this for sidno that also exists
      // in other for other_sidno.
      Const_interval_iterator other_ivit(other, other_sidno);
      if (is_interval_intersection_nonempty(&ivit, &other_ivit)) return true;
    }
  }
  return false;
}

enum_return_status Gtid_set::intersection(const Gtid_set *other,
                                          Gtid_set *result) {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  assert(result != nullptr);
  assert(other != nullptr);
  assert(result != this);
  assert(result != other);
  assert(other != this);
  /**
    @todo: This algorithm is simple, a little bit slower than
    necessary.  It would be more efficient to iterate over intervals
    of 'this' and 'other' similar to add_gno_interval(). At the moment
    the performance of this is not super-important. /Sven
  */
  Gtid_set this_minus_other(tsid_map);
  Gtid_set intersection(tsid_map);
  // In set theory, intersection(A, B) == A - (A - B)
  PROPAGATE_REPORTED_ERROR(this_minus_other.add_gtid_set(this));
  this_minus_other.remove_gtid_set(other);
  PROPAGATE_REPORTED_ERROR(intersection.add_gtid_set(this));
  intersection.remove_gtid_set(&this_minus_other);
  PROPAGATE_REPORTED_ERROR(result->add_gtid_set(&intersection));
  RETURN_OK;
}

bool Gtid_set::is_size_greater_than_or_equal(ulonglong num) const {
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  rpl_sidno max_sidno = get_max_sidno();
  ulonglong count = 0;
  for (rpl_sidno sidno = 1; sidno <= max_sidno; sidno++) {
    count += get_gtid_count(sidno);
    if (count >= num) return true;
  }
  return false;
}

namespace {

using namespace mysql::gtid;

void encode_nsids_format(uchar *buf, uint64_t n_sids, Gtid_format gtid_format) {
  uint64_t format_encoded =
      static_cast<uint64_t>(mysql::utils::to_underlying(gtid_format));
  uint64_t format_shifted = format_encoded << 56;
  uint64_t n_sids_encoded = n_sids | format_shifted;
  if (gtid_format == Gtid_format::tagged) {
    n_sids_encoded = format_shifted | (n_sids << 8) | format_encoded;
  }
  int8store(buf, n_sids_encoded);
}

std::tuple<mysql::utils::Return_status, uint64_t, Gtid_format>
decode_nsids_format(const uchar *buf) {
  uint64_t n_sids_encoded = uint8korr(buf);
  uint64_t format_mask = static_cast<uint64_t>(0xff) << 56;
  uint64_t n_sids_mask = ~format_mask;
  uint8_t format_encoded =
      static_cast<uint8_t>((n_sids_encoded & format_mask) >> 56);
  uint64_t n_sids = n_sids_encoded & n_sids_mask;
  auto [gtid_format, conversion_code] =
      mysql::utils::to_enumeration<Gtid_format>(format_encoded);
  if (gtid_format == Gtid_format::tagged) {
    n_sids_mask = static_cast<uint64_t>(0x00ffffffffffff00ULL);
    n_sids = (n_sids_encoded & n_sids_mask) >> 8;
  }
  return std::make_tuple(conversion_code, n_sids, gtid_format);
}

enum_return_status report_gtid_encoding_error() {
  BINLOG_ERROR(("Malformed GTID_set encoding."),
               (ER_MALFORMED_GTID_SET_ENCODING, MYF(0)));
  RETURN_REPORTED_ERROR;
}

}  // namespace

void Gtid_set::encode(uchar *buf, bool skip_tagged_gtids) const {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  // make place for number of sids
  uint64 n_sids = 0;
  uchar *n_sids_p = buf;
  buf += 8;
  auto format = analyze_encoding_format(skip_tagged_gtids);
  // iterate over sidnos
  rpl_sidno max_sidno = get_max_sidno();
  for (const auto &tsid_item : tsid_map->get_sorted_sidno()) {
    rpl_sidno sidno = tsid_item.second;
    // it is possible that the tsid_map has more SIDNOs than the set.
    if (sidno > max_sidno) continue;
    DBUG_PRINT("info", ("sidno=%d max_sidno=%d tsid_map->max_sidno=%d", sidno,
                        max_sidno, tsid_map->get_max_sidno()));
    auto tsid = tsid_map->sidno_to_tsid(sidno);
    if (tsid.is_tagged() == false || skip_tagged_gtids == false) {
      Const_interval_iterator ivit(this, sidno);
      const Interval *iv = ivit.get();
      if (iv != nullptr) {
        n_sids++;
        // store SID
        auto num_tsid_bytes = tsid.encode_tsid(buf, format);
        buf += num_tsid_bytes;
        // make place for number of intervals
        uint64 n_intervals = 0;
        uchar *n_intervals_p = buf;
        buf += 8;
        // iterate over intervals
        do {
          n_intervals++;
          // store one interval
          int8store(buf, iv->start);
          buf += 8;
          int8store(buf, iv->end);
          buf += 8;
          // iterate to next interval
          ivit.next();
          iv = ivit.get();
        } while (iv != nullptr);
        // store number of intervals
        int8store(n_intervals_p, n_intervals);
      }
    }
  }
  // store number of sids
  encode_nsids_format(n_sids_p, n_sids, format);
  assert(buf - n_sids_p == (int)get_encoded_length(format, skip_tagged_gtids));
}

enum_return_status Gtid_set::add_gtid_encoding(const uchar *encoded,
                                               size_t length,
                                               size_t *actual_length) {
  DBUG_TRACE;
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  size_t pos = 0;
  Free_intervals_lock lock(this);
  // read number of TSIDs
  if (length < 8) {
    DBUG_PRINT("error", ("(length=%lu) < 8", (ulong)length));
    return report_gtid_encoding_error();
  }
  auto [decoding_code, n_sids, gtid_format] = decode_nsids_format(encoded);
  if (decoding_code == mysql::utils::Return_status::error) {
    DBUG_PRINT("error", ("unknown or corrupted GTID set encoding format"));
    return report_gtid_encoding_error();
  }
  pos += 8;
  // iterate over TSIDs
  for (uint sid_counter = 0; sid_counter < n_sids; sid_counter++) {
    // read TSID and number of intervals
    if (length - pos < 16 + 8) {
      DBUG_PRINT("error", ("(length=%lu) - (pos=%lu) < 16 + 8. "
                           "[n_sids=%" PRIu64 " i=%u]",
                           (ulong)length, (ulong)pos, n_sids, sid_counter));
      return report_gtid_encoding_error();
    }
    Tsid tsid;
    pos += tsid.decode_tsid(encoded + pos, length - pos, gtid_format);
    uint64 n_intervals = uint8korr(encoded + pos);
    pos += 8;
    rpl_sidno sidno = tsid_map->add_tsid(tsid);
    if (sidno < 0) {
      DBUG_PRINT("error", ("sidno=%d", sidno));
      RETURN_REPORTED_ERROR;
    }
    PROPAGATE_REPORTED_ERROR(ensure_sidno(sidno));
    // iterate over intervals
    if (length - pos < 2 * 8 * n_intervals) {
      DBUG_PRINT(
          "error",
          ("(length=%lu) - (pos=%lu) < 2 * 8 * (n_intervals=%" PRIu64 ")",
           (ulong)length, (ulong)pos, n_intervals));
      return report_gtid_encoding_error();
    }
    Interval_iterator ivit(this, sidno);
    rpl_gno last = 0;
    for (uint i = 0; i < n_intervals; i++) {
      // read one interval
      rpl_gno start = sint8korr(encoded + pos);
      pos += 8;
      rpl_gno end = sint8korr(encoded + pos);
      pos += 8;
      if (start <= last || end <= start) {
        DBUG_PRINT("error", ("last=%" PRId64 " start=%" PRId64 " end=%" PRId64,
                             last, start, end));
        return report_gtid_encoding_error();
      }
      last = end;
      // Add interval.  Use the existing iterator position if the
      // current interval does not begin before it.  Otherwise iterate
      // from the beginning.
      Interval *current = ivit.get();
      if (current == nullptr || start < current->start) ivit.init(this, sidno);
      DBUG_PRINT("info",
                 ("adding %d:%" PRId64 "-%" PRId64, sidno, start, end - 1));
      add_gno_interval(&ivit, start, end, &lock);
    }
  }
  assert(pos <= length);
  if (actual_length == nullptr) {
    if (pos != length) {
      DBUG_PRINT("error",
                 ("(pos=%lu) != (length=%lu)", (ulong)pos, (ulong)length));
      return report_gtid_encoding_error();
    }
  } else
    *actual_length = pos;

  RETURN_OK;
}

mysql::gtid::Gtid_format Gtid_set::analyze_encoding_format(
    bool skip_tagged_gtids) const {
  if (skip_tagged_gtids == true) {
    return mysql::gtid::Gtid_format::untagged;
  }
  for (const auto &tsid_item : tsid_map->get_sorted_sidno()) {
    rpl_sidno sidno = tsid_item.second;
    auto tsid = tsid_map->sidno_to_tsid(sidno);
    if (tsid.is_tagged()) {
      return mysql::gtid::Gtid_format::tagged;
    }
  }
  return mysql::gtid::Gtid_format::untagged;
}

size_t Gtid_set::get_encoded_length(const mysql::gtid::Gtid_format &format,
                                    bool skip_tagged_gtids) const {
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  size_t ret = 8;
  size_t tag_len = 0;

  rpl_sidno max_sidno = get_max_sidno();
  for (rpl_sidno sidno = 1; sidno <= max_sidno; sidno++)
    if (contains_sidno(sidno)) {
      auto tsid = tsid_map->sidno_to_tsid(sidno);
      if (tsid.is_tagged() == false || skip_tagged_gtids == false) {
        ret += 16 + 8 + 2 * 8 * get_n_intervals(sidno);
        tag_len += tsid.get_tag().get_encoded_length(format);
      }
    }
  if (format == mysql::gtid::Gtid_format::tagged) {
    ret += tag_len;
  }
  return ret;
}

size_t Gtid_set::get_encoded_length(bool skip_tagged_gtids) const {
  if (tsid_lock != nullptr) tsid_lock->assert_some_wrlock();
  Gtid_format gtid_format = analyze_encoding_format(skip_tagged_gtids);
  return get_encoded_length(gtid_format, skip_tagged_gtids);
}
