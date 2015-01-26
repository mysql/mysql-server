#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_
/* Copyright (c) 2004, 2015, Oracle and/or its affiliates. All rights reserved.

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

/**

  @addtogroup Event_Scheduler
  @{

  @file event_queue.h

  Queue of events awaiting execution.
*/

#include "my_global.h"                          // uint
#include "mysql/mysql_lex_string.h"             // LEX_STRING
#include "my_time.h"                    /* my_time_t, interval_type */

#include "event_data_objects.h"
#include "event_parse_data.h"
#include "priority_queue.h"
#include "malloc_allocator.h"

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_LOCK_event_queue;
extern PSI_cond_key key_COND_queue_state;
#endif /* HAVE_PSI_INTERFACE */

class Event_basic;
class Event_queue_element;
class Event_queue_element_for_exec;

class THD;


/**
  Compares the execute_at members of two Event_queue_element instances.
  Used as compare operator for the prioritized queue when shifting
  elements inside.

  SYNOPSIS
    event_queue_element_compare_q()
    @param left     First Event_queue_element object
    @param right    Second Event_queue_element object

  @retval
   -1   left->execute_at < right->execute_at
    0   left->execute_at == right->execute_at
    1   left->execute_at > right->execute_at

  @remark
    execute_at.second_part is not considered during comparison
*/
struct Event_queue_less
{
  /// Maps compare function to strict weak ordering required by Priority_queue.
  bool operator()(Event_queue_element *left, Event_queue_element *right)
  {
    return event_queue_element_compare_q(left, right) > 0;
  }

  int event_queue_element_compare_q(Event_queue_element *left,
                                    Event_queue_element *right)
  {
    if (left->status == Event_parse_data::DISABLED)
      return right->status != Event_parse_data::DISABLED;

    if (right->status == Event_parse_data::DISABLED)
      return 1;

    my_time_t lhs = left->execute_at;
    my_time_t rhs = right->execute_at;
    return (lhs < rhs ? -1 : (lhs > rhs ? 1 : 0));
  }
};


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
  cond_wait(THD *thd, struct timespec *abstime, const PSI_stage_info *stage,
            const char *src_func, const char *src_file, uint src_line);

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
  Priority_queue<Event_queue_element*,
                 std::vector<Event_queue_element*,
                             Malloc_allocator<Event_queue_element*> >,
                 Event_queue_less>
  queue;

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
