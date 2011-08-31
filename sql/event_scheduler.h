#ifndef _EVENT_SCHEDULER_H_
#define _EVENT_SCHEDULER_H_
/* Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

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

/**
  @addtogroup Event_Scheduler
  @{
*/
/**
  @file

  Declarations of the scheduler thread class
  and related functionality.

  This file is internal to Event_Scheduler module. Please do not
  include it directly.  All public declarations of Event_Scheduler
  module are in events.h and event_data_objects.h.
*/


class Event_queue;
class Event_job_data;
class Event_db_repository;
class Event_queue_element_for_exec;
class Events;
class THD;

void
pre_init_event_thread(THD* thd);

bool
post_init_event_thread(THD* thd);

void
deinit_event_thread(THD *thd);


class Event_worker_thread
{
public:
  static void
  init(Event_db_repository *db_repository_arg)
  {
    db_repository= db_repository_arg;
  }

  void
  run(THD *thd, Event_queue_element_for_exec *event);

private:
  void
  print_warnings(THD *thd, Event_job_data *et);

  static Event_db_repository *db_repository;
};


class Event_scheduler
{
public:
  Event_scheduler(Event_queue *event_queue_arg);
  ~Event_scheduler();


  /* State changing methods follow */

  bool
  start();

  bool
  stop();

  /*
    Need to be public because has to be called from the function
    passed to pthread_create.
  */
  bool
  run(THD *thd);


  /* Information retrieving methods follow */
  bool
  is_running();

  void
  dump_internal_status();

private:
  uint
  workers_count();

  /* helper functions */
  bool
  execute_top(Event_queue_element_for_exec *event_name);

  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);

  void
  cond_wait(THD *thd, struct timespec *abstime, const char* msg,
            const char *func, uint line);

  mysql_mutex_t LOCK_scheduler_state;

  enum enum_state
  {
    INITIALIZED = 0,
    RUNNING,
    STOPPING
  };

  /* This is the current status of the life-cycle of the scheduler. */
  enum enum_state state;

  THD *scheduler_thd;

  mysql_cond_t COND_state;

  Event_queue *queue;

  uint mutex_last_locked_at_line;
  uint mutex_last_unlocked_at_line;
  const char* mutex_last_locked_in_func;
  const char* mutex_last_unlocked_in_func;
  bool mutex_scheduler_data_locked;
  bool waiting_on_cond;

  ulonglong started_events;

private:
  /* Prevent use of these */
  Event_scheduler(const Event_scheduler &);
  void operator=(Event_scheduler &);
};

/**
  @} (End of group Event_Scheduler)
*/

#endif /* _EVENT_SCHEDULER_H_ */
