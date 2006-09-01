#ifndef _EVENT_H_
#define _EVENT_H_
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
class Event_parse_data;
class Event_db_repository;
class Event_queue;
class Event_queue_element;
class Event_scheduler;

/* Return codes */
enum enum_events_error_code
{
  OP_OK= 0,
  OP_NOT_RUNNING,
  OP_CANT_KILL,
  OP_CANT_INIT,
  OP_DISABLED_EVENT,
  OP_LOAD_ERROR,
  OP_ALREADY_EXISTS
};

int
sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs);


class Events
{
public:
  /*
    Quite NOT the best practice and will be removed once
    Event_timed::drop() and Event_timed is fixed not do drop directly
    or other scheme will be found.
  */
  friend class Event_queue_element;
  
  /* The order should match the order in opt_typelib */
  enum enum_opt_event_scheduler
  {
    EVENTS_OFF= 0,
    EVENTS_ON= 1,
    EVENTS_DISABLED= 5,
  };

  static enum_opt_event_scheduler opt_event_scheduler;
  static TYPELIB opt_typelib;
  static TYPELIB var_typelib;

  bool
  init();

  void
  deinit();

  void
  init_mutexes();

  void
  destroy_mutexes();

  bool
  start_execution_of_events();

  bool
  stop_execution_of_events();

  bool
  is_execution_of_events_started();

  static Events *
  get_instance();

  bool
  create_event(THD *thd, Event_parse_data *parse_data, bool if_exists);

  bool
  update_event(THD *thd, Event_parse_data *parse_data, sp_name *rename_to);

  bool
  drop_event(THD *thd, LEX_STRING dbname, LEX_STRING name, bool if_exists,
             bool only_from_disk);

  void
  drop_schema_events(THD *thd, char *db);

  int
  open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table);

  bool
  show_create_event(THD *thd, LEX_STRING dbname, LEX_STRING name);

  /* Needed for both SHOW CREATE EVENT and INFORMATION_SCHEMA */
  static int
  reconstruct_interval_expression(String *buf, interval_type interval,
                                  longlong expression);

  static int
  fill_schema_events(THD *thd, TABLE_LIST *tables, COND * /* cond */);

  bool
  dump_internal_status(THD *thd);

private:
  bool
  check_system_tables(THD *thd);

  /* Singleton DP is used */
  Events();
  ~Events(){}

  /* Singleton instance */
  static Events singleton;

  Event_queue         *event_queue;
  Event_scheduler     *scheduler;
  Event_db_repository *db_repository;

  pthread_mutex_t LOCK_event_metadata;

  bool check_system_tables_error;

  /* Prevent use of these */
  Events(const Events &);
  void operator=(Events &);
};


#endif /* _EVENT_H_ */
