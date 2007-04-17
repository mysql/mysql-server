#ifndef _EVENT_DATA_OBJECTS_H_
#define _EVENT_DATA_OBJECTS_H_
/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#define EVEX_GET_FIELD_FAILED   -2
#define EVEX_BAD_PARAMS         -5
#define EVEX_MICROSECOND_UNSUP  -6

class sp_head;
class Sql_alloc;

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
  /*
    ENABLED = feature can function normally (is turned on)
    SLAVESIDE_DISABLED = feature is turned off on slave
    DISABLED = feature is turned off
  */
  enum enum_status
  {
    ENABLED = 1,
    DISABLED,
    SLAVESIDE_DISABLED  
  };

  enum enum_on_completion
  {
    ON_COMPLETION_DROP = 1,
    ON_COMPLETION_PRESERVE
  };

  LEX_STRING dbname;
  LEX_STRING name;
  LEX_STRING definer;// combination of user and host

  Time_zone *time_zone;

  Event_basic();
  virtual ~Event_basic();

  virtual int
  load_from_row(THD *thd, TABLE *table) = 0;

protected:
  bool
  load_string_fields(Field **fields, ...);

  bool
  load_time_zone(THD *thd, const LEX_STRING tz_name);
};



class Event_queue_element : public Event_basic
{
protected:
  bool status_changed;
  bool last_executed_changed;

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

  virtual int
  load_from_row(THD *thd, TABLE *table);

  bool
  compute_next_execution_time();

  void
  mark_last_executed(THD *thd);

  bool
  update_timing_fields(THD *thd);
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

  Event_timed();
  virtual ~Event_timed();

  void
  init();

  virtual int
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

  Event_job_data();

  virtual int
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


class Event_parse_data : public Sql_alloc
{
public:

  int on_completion;
  int status;
  longlong originator;
  /*
    do_not_create will be set if STARTS time is in the past and
    on_completion == ON_COMPLETION_DROP.
  */
  bool do_not_create;

  const char *body_begin;

  LEX_STRING dbname;
  LEX_STRING name;
  LEX_STRING definer;// combination of user and host
  LEX_STRING body;
  LEX_STRING comment;

  Item* item_starts;
  Item* item_ends;
  Item* item_execute_at;

  my_time_t starts;
  my_time_t ends;
  my_time_t execute_at;
  my_bool starts_null;
  my_bool ends_null;
  my_bool execute_at_null;

  sp_name *identifier;
  Item* item_expression;
  longlong expression;
  interval_type interval;

  static Event_parse_data *
  new_instance(THD *thd);

  bool
  check_parse_data(THD *thd);

  void
  init_body(THD *thd);

private:

  void
  init_definer(THD *thd);

  void
  init_name(THD *thd, sp_name *spn);

  int
  init_execute_at(THD *thd);

  int
  init_interval(THD *thd);

  int
  init_starts(THD *thd);

  int
  init_ends(THD *thd);

  Event_parse_data();
  ~Event_parse_data();

  void
  report_bad_value(const char *item_name, Item *bad_item);

  void
  check_if_in_the_past(THD *thd, my_time_t ltime_utc);

  Event_parse_data(const Event_parse_data &);	/* Prevent use of these */
  void check_originator_id(THD *thd);
  void operator=(Event_parse_data &);
};


/* Compares only the schema part of the identifier */
bool
event_basic_db_equal(LEX_STRING db, Event_basic *et);

/* Compares the whole identifier*/
bool
event_basic_identifier_equal(LEX_STRING db, LEX_STRING name, Event_basic *b);


#endif /* _EVENT_DATA_OBJECTS_H_ */
