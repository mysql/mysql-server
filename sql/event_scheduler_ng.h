#ifndef _EVENT_SCHEDULER_NG_H_
#define _EVENT_SCHEDULER_NG_H_
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
class Event_queue;

class Event_scheduler_ng
{
public:
  Event_scheduler_ng(){}
  ~Event_scheduler_ng(){}

  enum enum_state
  {
    INITIALIZED = 0,
    RUNNING,
    STOPPING
  };

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
  
  bool 
  init_scheduler(Event_queue *queue);

  void
  deinit_scheduler();

  void
  init_mutexes();
  
  void
  deinit_mutexes();

  /* Information retrieving methods follow */

  enum enum_state
  get_state();

  void
  queue_changed();

  static int
  dump_internal_status(THD *thd);

private:
  uint
  workers_count();

  /* helper functions */
  bool
  execute_top(THD *thd, Event_timed *job_data);

  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);
  
  pthread_mutex_t LOCK_scheduler_state;

  /* This is the current status of the life-cycle of the scheduler. */
  enum enum_state state;

  /*
    Holds the thread id of the executor thread or 0 if the scheduler is not
    running. It is used by ::shutdown() to know which thread to kill with
    kill_one_thread(). The latter wake ups a thread if it is waiting on a
    conditional variable and sets thd->killed to non-zero.
  */
  ulong thread_id;

  pthread_cond_t COND_state;
  
  Event_queue *queue;

  uint mutex_last_locked_at_line;
  uint mutex_last_unlocked_at_line;
  const char* mutex_last_locked_in_func;
  const char* mutex_last_unlocked_in_func;
  bool mutex_scheduler_data_locked;

private:
  /* Prevent use of these */
  Event_scheduler_ng(const Event_scheduler_ng &);
  void operator=(Event_scheduler_ng &);
};

#endif /* _EVENT_SCHEDULER_NG_H_ */
