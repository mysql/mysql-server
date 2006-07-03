#ifndef _EVENT_SCHEDULER_H_
#define _EVENT_SCHEDULER_H_
/* Copyright (C) 2004-2006 MySQL AB

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

class sp_name;
class Event_timed;
class Event_db_repository;

class THD;

int
events_init();

void
events_shutdown();

#include "event_queue.h"
#include "event_scheduler.h"

class Event_scheduler : public Event_queue
{
public:
  enum enum_state
  {
    UNINITIALIZED= 0,
    INITIALIZED,
    COMMENCING,
    CANTSTART,
    RUNNING,
    SUSPENDED,
    IN_SHUTDOWN
  };

  enum enum_suspend_or_resume
  {
    SUSPEND= 1,
    RESUME= 2
  };

  /* This is the current status of the life-cycle of the scheduler. */
  enum enum_state state;


  static void
  create_instance();

  /* Singleton access */
  static Event_scheduler*
  get_instance();

  bool 
  init(Event_db_repository *db_repo);

  void
  destroy();

  /* State changing methods follow */

  bool
  start();

  int
  stop();

  bool
  start_suspended();

  /*
    Need to be public because has to be called from the function 
    passed to pthread_create.
  */
  bool
  run(THD *thd);

  int
  suspend_or_resume(enum enum_suspend_or_resume action);
/*  
  static void
  init_mutexes();
  
  static void
  destroy_mutexes();
*/
  void
  report_error_during_start();

  /* Information retrieving methods follow */

  enum enum_state
  get_state();

  bool
  initialized();

  static int
  dump_internal_status(THD *thd);

  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);

  int
  cond_wait(int cond, pthread_mutex_t *mutex);

  void
  queue_changed();

protected:

  uint
  workers_count();

  /* helper functions */
  bool
  execute_top(THD *thd, Event_timed *et);

  void
  clean_memory(THD *thd);
  
  void
  stop_all_running_events(THD *thd);


  bool
  check_n_suspend_if_needed(THD *thd);

  bool
  check_n_wait_for_non_empty_queue(THD *thd);

  /* Singleton DP is used */
  Event_scheduler();
  

  pthread_mutex_t *LOCK_scheduler_data;
  

  /* Set to start the scheduler in suspended state */
  bool start_scheduler_suspended;

  /*
    Holds the thread id of the executor thread or 0 if the executor is not
    running. It is used by ::shutdown() to know which thread to kill with
    kill_one_thread(). The latter wake ups a thread if it is waiting on a
    conditional variable and sets thd->killed to non-zero.
  */
  ulong thread_id;

  enum enum_cond_vars
  {
    COND_NONE= -1,
    COND_new_work= 0,
    COND_started_or_stopped,
    COND_suspend_or_resume,
    /* Must be always last */
    COND_LAST
  };

  uint mutex_last_locked_at_line_nr;
  uint mutex_last_unlocked_at_line_nr;
  const char* mutex_last_locked_in_func_name;
  const char* mutex_last_unlocked_in_func_name;
  int cond_waiting_on;
  bool mutex_scheduler_data_locked;


  static const char * const cond_vars_names[COND_LAST];

  pthread_cond_t cond_vars[COND_LAST];

private:
  /* Prevent use of these */
  Event_scheduler(const Event_scheduler &);
  void operator=(Event_scheduler &);
};

#endif /* _EVENT_SCHEDULER_H_ */
