/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Implementation for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation
#endif

#include <mysql_priv.h>

/*
  'Dummy' functions to be used when we don't need any handling for a scheduler
  event
 */

static bool init_dummy(void) {return 0;}
static void post_kill_dummy(THD* thd) {}  
static void end_dummy(void) {}
static bool end_thread_dummy(THD *thd, bool cache_thread) { return 0; }

/*
  Initialize default scheduler with dummy functions so that setup functions
  only need to declare those that are relvant for their usage
*/

scheduler_functions::scheduler_functions()
  :init(init_dummy),
   init_new_connection_thread(init_new_connection_handler_thread),
   add_connection(0),                           // Must be defined
   post_kill_notification(post_kill_dummy),
   end_thread(end_thread_dummy), end(end_dummy)
{}


/*
  End connection, in case when we are using 'no-threads'
*/

static bool no_threads_end(THD *thd, bool put_in_cache)
{
  unlink_thd(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  return 1;                                     // Abort handle_one_connection
}


/*
  Initailize scheduler for --thread-handling=no-threads
*/

void one_thread_scheduler(scheduler_functions* func)
{
  func->max_threads= 1;
#ifndef EMBEDDED_LIBRARY
  func->add_connection= handle_connection_in_main_thread;
#endif
  func->init_new_connection_thread= init_dummy;
  func->end_thread= no_threads_end;
}


/*
  Initialize scheduler for --thread-handling=one-thread-per-connection
*/

#ifndef EMBEDDED_LIBRARY
void one_thread_per_connection_scheduler(scheduler_functions* func)
{
  func->max_threads= max_connections;
  func->add_connection= create_thread_to_handle_connection;
  func->end_thread= one_thread_per_connection_end;
}
#endif /* EMBEDDED_LIBRARY */
