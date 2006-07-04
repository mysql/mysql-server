#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_
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
class Event_job_data;

class THD;
typedef bool * (*event_timed_identifier_comparator)(Event_timed*, Event_timed*);

class Event_scheduler_ng;

class Event_queue
{
public:
  Event_queue();

  void
  init_mutexes();
  
  void
  deinit_mutexes();
  
  bool
  init(Event_db_repository *db_repo);
  
  void
  deinit();

  /* Methods for queue management follow */

  int
  create_event(THD *thd, Event_parse_data *et, bool check_existence);

  int
  update_event(THD *thd, Event_parse_data *et, LEX_STRING *new_schema,
               LEX_STRING *new_name);

  bool
  drop_event(THD *thd, sp_name *name);

  int
  drop_schema_events(THD *thd, LEX_STRING schema);

  int
  drop_user_events(THD *thd, LEX_STRING *definer)
  { DBUG_ASSERT(0); return 0;}

  uint
  events_count();

  uint
  events_count_no_lock();

  static bool
  check_system_tables(THD *thd);

  void
  recalculate_queue(THD *thd);
  
  void
  empty_queue();

  Event_timed *
  get_top_for_execution_if_time(THD *thd, time_t now, struct timespec *abstime);
 
  Event_timed*
  get_top();
  
  void
  remove_top();
  
  void
  top_changed();

///////////////protected
  Event_timed *
  find_event(LEX_STRING db, LEX_STRING name, bool remove_from_q);

  int
  load_events_from_db(THD *thd);

  void
  drop_matching_events(THD *thd, LEX_STRING pattern,
                       bool (*)(Event_timed *,LEX_STRING *));

  /* LOCK_event_queue is the mutex which protects the access to the queue. */
  pthread_mutex_t LOCK_event_queue;

  Event_db_repository *db_repository;

  /* The sorted queue with the Event_timed objects */
  QUEUE queue;
  
  uint mutex_last_locked_at_line;
  uint mutex_last_unlocked_at_line;
  const char* mutex_last_locked_in_func;
  const char* mutex_last_unlocked_in_func;
  bool mutex_queue_data_locked;

  /* helper functions for working with mutexes & conditionals */
  void
  lock_data(const char *func, uint line);

  void
  unlock_data(const char *func, uint line);

  void
  on_queue_change();
  
  Event_scheduler_ng *scheduler;
protected:

};

#endif /* _EVENT_QUEUE_H_ */
