#ifndef _EVENT_DATA_OBJECTS_H_
#define _EVENT_DATA_OBJECTS_H_
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

  @file event_data_objects.h
*/

#include "event_parse_data.h"
#include "thr_lock.h"                           /* thr_lock_type */

class Field;
class THD;
class Time_zone;
struct TABLE;

class Event_queue_element_for_exec
{
public:
  Event_queue_element_for_exec(){};
  ~Event_queue_element_for_exec();

  bool
  init(LEX_STRING dbname, LEX_STRING name);

  LEX_STRING dbname;
  LEX_STRING name;
  bool dropped;
  THD *thd;

private:
  /* Prevent use of these */
  Event_queue_element_for_exec(const Event_queue_element_for_exec &);
  void operator=(Event_queue_element_for_exec &);
};


class Event_basic
{
protected:
  MEM_ROOT mem_root;

public:

  LEX_STRING dbname;
  LEX_STRING name;
  LEX_STRING definer;// combination of user and host

  Time_zone *time_zone;

  Event_basic();
  virtual ~Event_basic();

  virtual bool
  load_from_row(THD *thd, TABLE *table) = 0;

protected:
  bool
  load_string_fields(Field **fields, ...);

  bool
  load_time_zone(THD *thd, const LEX_STRING tz_name);
};



class Event_queue_element : public Event_basic
{
public:
  int on_completion;
  int status;
  longlong originator;

  my_time_t last_executed;
  my_time_t execute_at;
  my_time_t starts;
  my_time_t ends;
  my_bool starts_null;
  my_bool ends_null;
  my_bool execute_at_null;

  longlong expression;
  interval_type interval;

  bool dropped;

  uint execution_count;

  Event_queue_element();
  virtual ~Event_queue_element();

  virtual bool
  load_from_row(THD *thd, TABLE *table);

  bool
  compute_next_execution_time();

  void
  mark_last_executed(THD *thd);
};


class Event_timed : public Event_queue_element
{
  Event_timed(const Event_timed &);	/* Prevent use of these */
  void operator=(Event_timed &);

public:
  LEX_STRING body;

  LEX_STRING definer_user;
  LEX_STRING definer_host;

  LEX_STRING comment;

  ulonglong created;
  ulonglong modified;

  ulong sql_mode;

  class Stored_program_creation_ctx *creation_ctx;
  LEX_STRING body_utf8;

  Event_timed();
  virtual ~Event_timed();

  void
  init();

  virtual bool
  load_from_row(THD *thd, TABLE *table);

  int
  get_create_event(THD *thd, String *buf);
};


class Event_job_data : public Event_basic
{
public:
  LEX_STRING body;
  LEX_STRING definer_user;
  LEX_STRING definer_host;

  ulong sql_mode;

  class Stored_program_creation_ctx *creation_ctx;

  Event_job_data();

  virtual bool
  load_from_row(THD *thd, TABLE *table);

  bool
  execute(THD *thd, bool drop);
private:
  bool
  construct_sp_sql(THD *thd, String *sp_sql);
  bool
  construct_drop_event_sql(THD *thd, String *sp_sql);

  Event_job_data(const Event_job_data &);       /* Prevent use of these */
  void operator=(Event_job_data &);
};


/* Compares only the schema part of the identifier */
bool
event_basic_db_equal(LEX_STRING db, Event_basic *et);

/* Compares the whole identifier*/
bool
event_basic_identifier_equal(LEX_STRING db, LEX_STRING name, Event_basic *b);

/**
  @} (End of group Event_Scheduler)
*/

#endif /* _EVENT_DATA_OBJECTS_H_ */
