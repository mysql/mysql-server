/* Copyright (c) 2007, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Implementation for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation
#endif

#include <sql_priv.h>
#include "unireg.h"                    // REQUIRED: for other includes
#include "scheduler.h"
#include "sql_connect.h"         // init_new_connection_handler_thread
#include "scheduler.h"
#include "sql_callback.h"

/*
  End connection, in case when we are using 'no-threads'
*/

static bool no_threads_end(THD *thd, bool put_in_cache)
{
  unlink_thd(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  return 1;                                     // Abort handle_one_connection
}

static scheduler_functions one_thread_scheduler_functions=
{
  1,                                     // max_threads
  NULL,                                  // init
  init_new_connection_handler_thread,    // init_new_connection_thread
#ifndef EMBEDDED_LIBRARY
  handle_connection_in_main_thread,      // add_connection
#else
  NULL,                                  // add_connection
#endif // EMBEDDED_LIBRARY
  NULL,                                  // thd_wait_begin
  NULL,                                  // thd_wait_end
  NULL,                                  // post_kill_notification
  no_threads_end,                        // end_thread
  NULL,                                  // end
};

#ifndef EMBEDDED_LIBRARY
static scheduler_functions one_thread_per_connection_scheduler_functions=
{
  0,                                     // max_threads
  NULL,                                  // init
  init_new_connection_handler_thread,    // init_new_connection_thread
  create_thread_to_handle_connection,    // add_connection
  NULL,                                  // thd_wait_begin
  NULL,                                  // thd_wait_end
  NULL,                                  // post_kill_notification
  one_thread_per_connection_end,         // end_thread
  NULL,                                  // end
};
#endif  // EMBEDDED_LIBRARY


scheduler_functions *thread_scheduler= NULL;

/** @internal
  Helper functions to allow mysys to call the thread scheduler when
  waiting for locks.
*/

/**@{*/
extern "C"
{
static void scheduler_wait_lock_begin(void) {
  MYSQL_CALLBACK(thread_scheduler,
                 thd_wait_begin, (current_thd, THD_WAIT_TABLE_LOCK));
}

static void scheduler_wait_lock_end(void) {
  MYSQL_CALLBACK(thread_scheduler, thd_wait_end, (current_thd));
}

static void scheduler_wait_sync_begin(void) {
  MYSQL_CALLBACK(thread_scheduler,
                 thd_wait_begin, (current_thd, THD_WAIT_TABLE_LOCK));
}

static void scheduler_wait_sync_end(void) {
  MYSQL_CALLBACK(thread_scheduler, thd_wait_end, (current_thd));
}
};
/**@}*/

/**
  Common scheduler init function.

  The scheduler is either initialized by calling
  one_thread_scheduler() or one_thread_per_connection_scheduler() in
  mysqld.cc, so this init function will always be called.
 */
static void scheduler_init() {
  thr_set_lock_wait_callback(scheduler_wait_lock_begin,
                             scheduler_wait_lock_end);
  thr_set_sync_wait_callback(scheduler_wait_sync_begin,
                             scheduler_wait_sync_end);
}

/*
  Initialize scheduler for --thread-handling=one-thread-per-connection
*/

#ifndef EMBEDDED_LIBRARY
void one_thread_per_connection_scheduler()
{
  scheduler_init();
  one_thread_per_connection_scheduler_functions.max_threads= max_connections;
  thread_scheduler= &one_thread_per_connection_scheduler_functions;
}
#endif

/*
  Initailize scheduler for --thread-handling=no-threads
*/

void one_thread_scheduler()
{
  scheduler_init();
  thread_scheduler= &one_thread_scheduler_functions;
}


/*
  Initialize scheduler for --thread-handling=one-thread-per-connection
*/

/*
  thd_scheduler keeps the link between THD and events.
  It's embedded in the THD class.
*/

thd_scheduler::thd_scheduler()
  : m_psi(NULL), data(NULL)
{
}


thd_scheduler::~thd_scheduler()
{
}

static scheduler_functions *saved_thread_scheduler;
static uint saved_thread_handling;

extern "C"
int my_thread_scheduler_set(scheduler_functions *scheduler)
{
  DBUG_ASSERT(scheduler != 0);

  if (scheduler == NULL)
    return 1;

  saved_thread_scheduler= thread_scheduler;
  saved_thread_handling= thread_handling;
  thread_scheduler= scheduler;
  // Scheduler loaded dynamically
  thread_handling= SCHEDULER_TYPES_COUNT;
  return 0;
}


extern "C"
int my_thread_scheduler_reset()
{
  DBUG_ASSERT(saved_thread_scheduler != NULL);

  if (saved_thread_scheduler == NULL)
    return 1;

  thread_scheduler= saved_thread_scheduler;
  thread_handling= saved_thread_handling;
  saved_thread_scheduler= 0;
  return 0;
}



