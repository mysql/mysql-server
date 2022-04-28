/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_HELPER_MULTITHREAD_XSYNC_POINT_H_
#define PLUGIN_X_SRC_HELPER_MULTITHREAD_XSYNC_POINT_H_

#include <vector>

#include "my_dbug.h"  // NOLINT(build/include_subdir)

/**
  Another thread synchronization functions for MySQL Server

  In servers source code, there are following synchronization frameworks
  available:

  * my_dbug
  * debug_sync

  Where both can be used to synchronize threads. The problem is that
  both depend on MySQL internal state. For example, "debug_sync"
  requires that there is active THD attached the thread, "my_dbug" requires
  that executing thread was initialized with "my_thread_init". Where both
  options are sufficient for optimizer or innodb, in both cases
  the threads are properly initialized and have THD assigned.

  X Plugin interact with different server callbacks where some of them
  are called at server shutdown, server startup, thread that handle
  unix signals. In some cases the calling thread doesn't have a THD
  or its not initialized by "my_thread_init".

  X Plugin is going to use this sync-library, until server synchronization
  libraries will have those constrains.
*/

namespace xpl {

#ifndef NDEBUG
/**
  Enable "sync-point"

  All enabled sync-points after hitting by a thread (`xdbug_sync_point_check`)
  are going to block until they are "waked-up".
*/
void xdbug_sync_points_enable(const std::vector<const char *> &sync_points);

/**
  Check if the sync-point is enabled, and block

  This function is going to block when "sync_point_name" is set to a valid
  sync-name (not null) and the sync-name was enabled first. Before blocking,
  its going to disable another "sync-point", pointed by `wakeup_sync_point`
  parameter. "disable" sync-point, means that thread blocked at that point
  is going to start running.

  Example:
  Make thread T1 wait for thread T2 after that it is going to execute some
  critical task, after that T2 is going to be enabled:

                       T1                  |               T2
  -----------------------------------------|------------------------------
    1. t1_do_something_other()             | t2_part1_of_its_work();
    2. xdbug_sync_point_check("T1")        | t2_part2_of_its_work();
    3.                                     | xdbug_sync_point_check("T2", "T1")
    4. do_critical_task();                 |
    5. xdbug_sync_point_check(nullptr,"T2")|
    6. t1_do_something_other()             | t2_part3_of_its_work();

 Lets assume that some fault is triggered when `t2_part3_of_its_work` is
 executed after `do_critical_task`, or `t2_part2_of_its_work` is executed before
 `do_critical_task`. Using those `dbug_sync_point_check` calls, it is going to
 make only one thread executing in steps 3,4,5.
*/
void xdbug_sync_point_check(const char *const sync_point_name,
                            const char *const wakeup_sync_point = nullptr);

/**
  Check if DBUG "keyword" is enabled, and block

  Use "my_dbug" framework, wait until user explicitly does some parallel task
  and unblocks this synchronization point.
  This macro is more convenient then "xdbug" functions.

  Example:

                          SESSION1          |             SESSION2
  ------------------------------------------|------------------------------
    1. SET GLOBAL.DEBUG="+d,block_sql";     |
    2. SELECT reach_the_block();            |
    3. dbug_sync_point_check("block_sql");  |
    4.                                      | SELECT do_critical_task();
    5.                                      | SET GLOBAL.DEBUG="-d,block_sql";
    6. receive resultset for 2.             | SELECT do_something_other();
*/
void dbug_sync_point_check(const char *const sync_point_name);
#define XSYNC_WAIT_NONE nullptr
#define XSYNC_WAIT(NAME) NAME
#define XSYNC_WAKE(NAME) NAME

#define XSYNC_POINT_ENABLE(...) ::xpl::xdbug_sync_points_enable({__VA_ARGS__})
#define XSYNC_POINT_CHECK(...) ::xpl::xdbug_sync_point_check(__VA_ARGS__)
#define SYNC_POINT_CHECK(...) ::xpl::dbug_sync_point_check(__VA_ARGS__)
#else

#define XSYNC_POINT_ENABLE(...) \
  do {                          \
  } while (0)
#define XSYNC_POINT_CHECK(...) \
  do {                         \
  } while (0)
#define SYNC_POINT_CHECK(...) \
  do {                        \
  } while (0)

#endif  // NDEBUG

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_MULTITHREAD_XSYNC_POINT_H_
