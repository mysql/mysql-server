/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* 
 * sql_manager.cc
 * This thread manages various maintenance tasks.
 *
 *   o Flushing the tables every flush_time seconds.
 *   o Berkeley DB: removing unneeded log files.
 */

#include "mysql_priv.h"
#include "sql_manager.h"

ulong volatile manager_status;
bool volatile manager_thread_in_use;

pthread_t manager_thread;
pthread_mutex_t LOCK_manager;
pthread_cond_t COND_manager;

pthread_handler_decl(handle_manager,arg __attribute__((unused)))
{
  int error = 0;
  ulong status;
  struct timespec abstime;
  bool reset_flush_time = TRUE;
  my_thread_init();
  DBUG_ENTER("handle_manager");

  pthread_detach_this_thread();
  manager_thread = pthread_self();
  manager_status = 0;
  manager_thread_in_use = 1;

  for (;;)
  {
    pthread_mutex_lock(&LOCK_manager);
    /* XXX: This will need to be made more general to handle different
     * polling needs. */
    if (flush_time)
    {
      if (reset_flush_time)
      {
#ifdef HAVE_TIMESPEC_TS_SEC
        abstime.ts_sec = time(NULL)+flush_time;	// Bsd 2.1
        abstime.ts_nsec = 0;
#else
        abstime.tv_sec = time(NULL)+flush_time;	// Linux or Solairs
        abstime.tv_nsec = 0;
#endif
        reset_flush_time = FALSE;
      }
      while (!manager_status && !error && !abort_loop)
        error = pthread_cond_timedwait(&COND_manager, &LOCK_manager, &abstime);
    }
    else
      while (!manager_status && !error && !abort_loop)
        error = pthread_cond_wait(&COND_manager, &LOCK_manager);
    status = manager_status;
    manager_status = 0;
    pthread_mutex_unlock(&LOCK_manager);

    if (abort_loop)
      break;

    if (error)  /* == ETIMEDOUT */
    {
      flush_tables();
      error = 0;
      reset_flush_time = TRUE;
    }

#ifdef HAVE_BERKELEY_DB
    if (status & MANAGER_BERKELEY_LOG_CLEANUP)
    {
      berkeley_cleanup_log_files();
      status &= ~MANAGER_BERKELEY_LOG_CLEANUP;
    }
#endif

    if (status)
      DBUG_PRINT("error", ("manager did not handle something: %lx", status));
  }
  manager_thread_in_use = 0;
  my_thread_end();
  DBUG_RETURN(NULL);
}
