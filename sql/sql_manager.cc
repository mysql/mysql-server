/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/* 
 * sql_manager.cc
 * This thread manages various maintenance tasks.
 *
 *   o Flushing the tables every flush_time seconds.
 */

#include "sql/sql_manager.h"

#include <errno.h>
#include <sys/types.h>
#include <time.h>

#include "log.h"               // sql_print_warning
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_systime.h"
#include "my_thread.h"         // my_thread_t
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql_com.h"
#include "mysqld.h"            // flush_time
#include "sql_base.h"          // tdc_flush_unused_tables

static bool volatile manager_thread_in_use;
static bool abort_manager;

my_thread_t manager_thread;
mysql_mutex_t LOCK_manager;
mysql_cond_t COND_manager;

extern "C" {
static void *handle_manager(void *arg MY_ATTRIBUTE((unused)))
{
  int error = 0;
  struct timespec abstime;
  bool reset_flush_time = TRUE;
  my_thread_init();
  DBUG_ENTER("handle_manager");

  manager_thread= my_thread_self();
  manager_thread_in_use = 1;

  for (;;)
  {
    mysql_mutex_lock(&LOCK_manager);
    /* XXX: This will need to be made more general to handle different
     * polling needs. */
    if (flush_time)
    {
      if (reset_flush_time)
      {
	set_timespec(&abstime, flush_time);
        reset_flush_time = FALSE;
      }
      while ((!error || error == EINTR) && !abort_manager)
        error= mysql_cond_timedwait(&COND_manager, &LOCK_manager, &abstime);
    }
    else
    {
      while ((!error || error == EINTR) && !abort_manager)
        error= mysql_cond_wait(&COND_manager, &LOCK_manager);
    }
    mysql_mutex_unlock(&LOCK_manager);

    if (abort_manager)
      break;

    if (is_timeout(error))
    {
      tdc_flush_unused_tables();
      error = 0;
      reset_flush_time = TRUE;
    }

  }
  manager_thread_in_use = 0;
  DBUG_LEAVE; // Can't use DBUG_RETURN after my_thread_end
  my_thread_end();
  return (NULL);
}
} // extern "C"


/* Start handle manager thread */
void start_handle_manager()
{
  DBUG_ENTER("start_handle_manager");
  abort_manager = false;
  if (flush_time && flush_time != ~(ulong) 0L)
  {
    my_thread_handle hThread;
    int error;
    if ((error= mysql_thread_create(key_thread_handle_manager,
                                    &hThread, &connection_attrib,
                                    handle_manager, 0)))
      sql_print_warning("Can't create handle_manager thread (errno= %d)",
                        error);
  }
  DBUG_VOID_RETURN;
}


/* Initiate shutdown of handle manager thread */
void stop_handle_manager()
{
  DBUG_ENTER("stop_handle_manager");
  abort_manager = true;
  mysql_mutex_lock(&LOCK_manager);
  if (manager_thread_in_use)
  {
    DBUG_PRINT("quit", ("initiate shutdown of handle manager thread: 0x%lx",
                        (ulong)manager_thread));
    mysql_cond_signal(&COND_manager);
  }
  mysql_mutex_unlock(&LOCK_manager);
  DBUG_VOID_RETURN;
}

