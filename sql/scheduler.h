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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/*
  Classes for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface
#endif

class THD;

/* Functions used when manipulating threads */

class scheduler_functions
{
public:
  uint max_threads, *connection_count;
  ulong *max_connections;
  bool (*init)(void);
  bool (*init_new_connection_thread)(void);
  void (*add_connection)(THD *thd);
  void (*post_kill_notification)(THD *thd);
  bool (*end_thread)(THD *thd, bool cache_thread);
  void (*end)(void);
  scheduler_functions();
};

enum scheduler_types
{
  SCHEDULER_ONE_THREAD_PER_CONNECTION=0,
  SCHEDULER_NO_THREADS,
  SCHEDULER_POOL_OF_THREADS
};

void one_thread_per_connection_scheduler(scheduler_functions *func,
                                         ulong *arg_max_connections,
                                         uint *arg_connection_count);
void one_thread_scheduler(scheduler_functions* func);

#if defined(HAVE_LIBEVENT) && !defined(EMBEDDED_LIBRARY)

#define HAVE_POOL_OF_THREADS 1

struct event;

class thd_scheduler
{
public:
  bool logged_in;
  struct event* io_event;
  LIST list;
  bool thread_attached;  /* Indicates if THD is attached to the OS thread */
  
#ifndef DBUG_OFF
  char dbug_explain[256];
  bool set_explain;
#endif

  thd_scheduler();
  ~thd_scheduler();
  bool init(THD* parent_thd);
  bool thread_attach();
  void thread_detach();
};

void pool_of_threads_scheduler(scheduler_functions* func);

#else

#define HAVE_POOL_OF_THREADS 0                  /* For easyer tests */
#define pool_of_threads_scheduler(A) \
  one_thread_per_connection_scheduler(A, &max_connections, \
                                      &connection_count)

class thd_scheduler
{};

#endif
