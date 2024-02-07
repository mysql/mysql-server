/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVICE_THD_WAIT_INCLUDED
#define MYSQL_SERVICE_THD_WAIT_INCLUDED

#include <assert.h>

/**
  @file include/mysql/service_thd_wait.h
  This service provides functions for plugins and storage engines to report
  when they are going to sleep/stall.

  SYNOPSIS
  thd_wait_begin() - call just before a wait begins
  thd                     Thread object
                          Use NULL if the thd is NOT known.
  wait_type               Type of wait
                          1 -- short wait (e.g. for mutex)
                          2 -- medium wait (e.g. for disk io)
                          3 -- large wait (e.g. for locked row/table)
  NOTES
    This is used by the threadpool to have better knowledge of which
    threads that currently are actively running on CPUs. When a thread
    reports that it's going to sleep/stall, the threadpool scheduler is
    free to start another thread in the pool most likely. The expected wait
    time is simply an indication of how long the wait is expected to
    become, the real wait time could be very different.

  thd_wait_end() called immediately after the wait is complete

  thd_wait_end() MUST be called if thd_wait_begin() was called.

  Using thd_wait_...() service is optional but recommended.  Using it will
  improve performance as the thread pool will be more active at managing the
  thread workload.
*/
#ifndef __cplusplus
#error "Not compiling as C++"
#endif

class THD;

/*
  One should only report wait events that could potentially block for a
  long time. A mutex wait is too short of an event to report. The reason
  is that an event which is reported leads to a new thread starts
  executing a query and this has a negative impact of usage of CPU caches
  and thus the expected gain of starting a new thread must be higher than
  the expected cost of lost performance due to starting a new thread.

  Good examples of events that should be reported are waiting for row locks
  that could easily be for many milliseconds or even seconds and the same
  holds true for global read locks, table locks and other meta data locks.
  Another event of interest is going to sleep for an extended time.

  Note that user-level locks no longer use THD_WAIT_USER_LOCK wait type.
  Since their implementation relies on metadata locks manager it uses
  THD_WAIT_META_DATA_LOCK instead.
*/

enum THD_wait_type : int {
  THD_WAIT_NONE = 0,
  THD_WAIT_SLEEP = 1,
  THD_WAIT_DISKIO = 2,
  THD_WAIT_ROW_LOCK = 3,
  THD_WAIT_GLOBAL_LOCK = 4,
  THD_WAIT_META_DATA_LOCK = 5,
  THD_WAIT_TABLE_LOCK = 6,
  THD_WAIT_USER_LOCK = 7,
  THD_WAIT_BINLOG = 8,
  THD_WAIT_GROUP_COMMIT = 9,
  THD_WAIT_SYNC = 10,
  THD_WAIT_TRX_DELAY = 11,
  THD_WAIT_LAST = 12
};

inline const char *THD_wait_type_str(THD_wait_type twt) {
  switch (twt) {
    case THD_WAIT_NONE:
      return "Not waiting";

    case THD_WAIT_SLEEP:
      return "Waiting for sleep";

    case THD_WAIT_DISKIO:
      return "Waiting for Disk IO";

    case THD_WAIT_ROW_LOCK:
      return "Waiting for row lock";

    case THD_WAIT_GLOBAL_LOCK:
      return "Waiting for global lock";

    case THD_WAIT_META_DATA_LOCK:
      return "Waiting for metadata lock";

    case THD_WAIT_TABLE_LOCK:
      return "Waiting for table lock";

    case THD_WAIT_USER_LOCK:
      return "Waiting for user lock";

    case THD_WAIT_BINLOG:
      return "Waiting for binlog";

    case THD_WAIT_GROUP_COMMIT:
      return "Waiting for group commit";

    case THD_WAIT_SYNC:
      return "Waiting for fsync";

    case THD_WAIT_TRX_DELAY:
      return "Waiting for transaction delay";

    case THD_WAIT_LAST:
      return "<Unused LAST marker value>";
  }  // switch
  assert(false);
  return "<Invalid THD_WAIT value>";
}

struct thd_wait_service_st {
  void (*thd_wait_begin_func)(THD *, int);
  void (*thd_wait_end_func)(THD *);
};

#ifdef MYSQL_DYNAMIC_PLUGIN
// We do not support these callbacks for dynamic plugins. To enforce this we
// provide a conflicting declaration for them, so it becomes impossible to use
// them.
void thd_wait_begin();
void thd_wait_end();
#else  /* MYSQL_DYNAMIC_PLUGIN */
// e.g. ha_innodb (static plugin)
void thd_wait_begin(THD *thd, int wait_type);
void thd_wait_end(THD *thd);
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVICE_THD_WAIT_INCLUDED */
