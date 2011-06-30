#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_
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

  @file event_queue.h

  Queue of events awaiting execution.
*/

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_LOCK_event_queue;
extern PSI_cond_key key_COND_queue_state;
#endif /* HAVE_PSI_INTERFACE */

#include "queues.h"                             // QUEUE
#include "sql_string.h"                         /* LEX_STRING */
#include "my_time.h"                    /* my_time_t, interval_type */

class Event_basic;
class Event_queue_element;
class Event_queue_element_for_exec;

class THD;

/**
  Queue of active events awaiting execution.
*/

class Event_queue
{
public:
  Event_queue();
  ~Event_queue();

  bool
  init_queue(THD *thd);

  /* Methods for queue management follow */

  bool
  create_event(THD *thd, Event_queue_element *new_element,
               bool *created);

  void
  update_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
               Event_queue_element *new_element);

  void
  drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name);

  void
  drop_schema_events(THD *thd, LEX_STRING schema);

  void
  recalculate_activation_times(THD *thd);

  bool
  get_top_for_execution_if_time(THD *thd,
                                Event_queue_element_for_exec **event_name);


  void
  dump_internal_status();

private:
  void
  empty_queue();

  void
  deinit_queue();
  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);

  void
  cond_wait(THD *thd, struct timespec *abstime, const char* msg,
            const char *func, uint line);

  void
  find_n_remove_event(LEX_STRING db, LEX_STRING name);


  void
  drop_matching_events(THD *thd, LEX_STRING pattern,
                       bool (*)(LEX_STRING, Event_basic *));


  void
  dbug_dump_queue(time_t now);

  /* LOCK_event_queue is the mutex which protects the access to the queue. */
  mysql_mutex_t LOCK_event_queue;
  mysql_cond_t COND_queue_state;

  /* The sorted queue with the Event_queue_element objects */
  QUEUE queue;

  my_time_t next_activation_at;

  uint mutex_last_locked_at_line;
  uint mutex_last_unlocked_at_line;
  uint mutex_last_attempted_lock_at_line;
  const char* mutex_last_locked_in_func;
  const char* mutex_last_unlocked_in_func;
  const char* mutex_last_attempted_lock_in_func;
  bool mutex_queue_data_locked;
  bool mutex_queue_data_attempting_lock;
  bool waiting_on_cond;
};
/**
  @} (End of group Event_Scheduler)
*/

#endif /* _EVENT_QUEUE_H_ */
