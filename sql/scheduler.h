#ifndef SCHEDULER_INCLUDED
#define SCHEDULER_INCLUDED

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
  Classes for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface
#endif

class THD;

/* Functions used when manipulating threads */

struct scheduler_functions
{
  uint max_threads;
  bool (*init)(void);
  bool (*init_new_connection_thread)(void);
  void (*add_connection)(THD *thd);
  void (*thd_wait_begin)(THD *thd, int wait_type);
  void (*thd_wait_end)(THD *thd);
  void (*post_kill_notification)(THD *thd);
  bool (*end_thread)(THD *thd, bool cache_thread);
  void (*end)(void);
};


/**
  Scheduler types enumeration.

  The default of --thread-handling is the first one in the
  thread_handling_names array, this array has to be consistent with
  the order in this array, so to change default one has to change the
  first entry in this enum and the first entry in the
  thread_handling_names array.

  @note The last entry of the enumeration is also used to mark the
  thread handling as dynamic. In this case the name of the thread
  handling is fetched from the name of the plugin that implements it.
*/
enum scheduler_types
{
  /*
    The default of --thread-handling is the first one in the
    thread_handling_names array, this array has to be consistent with
    the order in this array, so to change default one has to change
    the first entry in this enum and the first entry in the
    thread_handling_names array.
  */
  SCHEDULER_ONE_THREAD_PER_CONNECTION=0,
  SCHEDULER_NO_THREADS,
  SCHEDULER_TYPES_COUNT
};

void one_thread_per_connection_scheduler();
void one_thread_scheduler();

/*
 To be used for pool-of-threads (implemeneted differently on various OSs)
*/
class thd_scheduler
{
public:
  /*
    Thread instrumentation for the user job.
    This member holds the instrumentation while the user job is not run
    by a thread.

    Note that this member is not conditionally declared
    (ifdef HAVE_PSI_INTERFACE), because doing so will change the binary
    layout of THD, which is exposed to plugin code that may be compiled
    differently.
  */
  PSI_thread *m_psi;

  void *data;                  /* scheduler-specific data structure */

  thd_scheduler();
  ~thd_scheduler();
};

void *thd_get_scheduler_data(THD *thd);
void thd_set_scheduler_data(THD *thd, void *data);
PSI_thread* thd_get_psi(THD *thd);
void thd_set_psi(THD *thd, PSI_thread *psi);

extern scheduler_functions *thread_scheduler;

#endif
