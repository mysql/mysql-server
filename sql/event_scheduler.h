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

class Event_timed;

class THD;
typedef bool * (*event_timed_identifier_comparator)(Event_timed*, Event_timed*);

int
events_init();

void
events_shutdown();

class Event_scheduler
{
public:
  /* Return codes */
  enum enum_error_code
  {
    OP_OK= 0,
    OP_NOT_RUNNING,
    OP_CANT_KILL,
    OP_CANT_INIT,
    OP_DISABLED_EVENT,
    OP_LOAD_ERROR,
    OP_ALREADY_EXISTS
  };

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

  /* Singleton access */
  static Event_scheduler*
  get_instance();

  /* Methods for queue management follow */

  enum enum_error_code
  create_event(THD *thd, Event_timed *et, bool check_existence);

  enum enum_error_code
  update_event(THD *thd, Event_timed *et, LEX_STRING *new_schema,
                LEX_STRING *new_name);

  bool
  drop_event(THD *thd, Event_timed *et);


  int
  drop_schema_events(THD *thd, LEX_STRING *schema);

  int
  drop_user_events(THD *thd, LEX_STRING *definer, uint *dropped_num)
  { DBUG_ASSERT(0); return 0;}

  uint
  events_count();

  /* State changing methods follow */

  bool
  start();

  enum enum_error_code
  stop();

  bool
  start_suspended();

  bool
  run(THD *thd);

  enum enum_error_code
  suspend_or_resume(enum enum_suspend_or_resume action);
  
  bool 
  init();

  void
  destroy();

  static void
  init_mutexes();
  
  static void
  destroy_mutexes();

  void
  report_error_during_start();

  /* Information retrieving methods follow */

  enum enum_state
  get_state();

  bool
  initialized();

  static int
  dump_internal_status(THD *thd);

  static bool
  check_system_tables(THD *thd);

private:
  Event_timed *
  find_event(Event_timed *etn, bool remove_from_q);

  uint
  workers_count();

  bool
  is_running_or_suspended();

  /* helper functions */
  bool
  execute_top(THD *thd);

  void
  clean_queue(THD *thd);
  
  void
  stop_all_running_events(THD *thd);

  enum enum_error_code
  load_named_event(THD *thd, Event_timed *etn, Event_timed **etn_new);

  int
  load_events_from_db(THD *thd);

  void
  drop_matching_events(THD *thd, LEX_STRING *pattern,
                       bool (*)(Event_timed *,LEX_STRING *));

  bool
  check_n_suspend_if_needed(THD *thd);

  bool
  check_n_wait_for_non_empty_queue(THD *thd);

  /* Singleton DP is used */
  Event_scheduler();
  
  enum enum_cond_vars
  {
    COND_NONE= -1,
    /*
      COND_new_work is a conditional used to signal that there is a change
      of the queue that should inform the executor thread that new event should
      be executed sooner than previously expected, because of add/replace event.
    */
    COND_new_work= 0,
    /*
      COND_started is a conditional used to synchronize the thread in which 
      ::start() was called and the spawned thread. ::start() spawns a new thread
      and then waits on COND_started but also checks when awaken that `state` is
      either RUNNING or CANTSTART. Then it returns back.
    */
    COND_started_or_stopped,
    /*
      Conditional used for signalling from the scheduler thread back to the
      thread that calls ::suspend() or ::resume. Synchronizing the calls.
    */
    COND_suspend_or_resume,
    /* Must be always last */
    COND_LAST
  };

  /* Singleton instance */
  static Event_scheduler singleton;
  
  /* This is the current status of the life-cycle of the manager. */
  enum enum_state state;

  /* Set to start the scheduler in suspended state */
  bool start_scheduler_suspended;

  /*
    LOCK_scheduler_data is the mutex which protects the access to the
    manager's queue as well as used when signalling COND_new_work,
    COND_started and COND_shutdown.
  */
  pthread_mutex_t LOCK_scheduler_data;

  /*
    Holds the thread id of the executor thread or 0 if the executor is not
    running. It is used by ::shutdown() to know which thread to kill with
    kill_one_thread(). The latter wake ups a thread if it is waiting on a
    conditional variable and sets thd->killed to non-zero.
  */
  ulong thread_id;

  pthread_cond_t cond_vars[COND_LAST];
  static const char * const cond_vars_names[COND_LAST];

  /* The MEM_ROOT of the object */
  MEM_ROOT scheduler_root;

  /* The sorted queue with the Event_timed objects */
  QUEUE queue;
  
  uint mutex_last_locked_at_line;
  uint mutex_last_unlocked_at_line;
  const char* mutex_last_locked_in_func;
  const char* mutex_last_unlocked_in_func;
  enum enum_cond_vars cond_waiting_on;
  bool mutex_scheduler_data_locked;

  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);

  int
  cond_wait(enum enum_cond_vars, pthread_mutex_t *mutex);

private:
  /* Prevent use of these */
  Event_scheduler(const Event_scheduler &);
  void operator=(Event_scheduler &);
};

#endif /* _EVENT_SCHEDULER_H_ */
