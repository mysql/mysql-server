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


class Event_timed;
class Event_parse_data;

class Events
{
public:
  static ulong opt_event_scheduler;
  static TYPELIB opt_typelib;

  enum enum_table_field
  {
    FIELD_DB = 0,
    FIELD_NAME,
    FIELD_BODY,
    FIELD_DEFINER,
    FIELD_EXECUTE_AT,
    FIELD_INTERVAL_EXPR,
    FIELD_TRANSIENT_INTERVAL,
    FIELD_CREATED,
    FIELD_MODIFIED,
    FIELD_LAST_EXECUTED,
    FIELD_STARTS,
    FIELD_ENDS,
    FIELD_STATUS,
    FIELD_ON_COMPLETION,
    FIELD_SQL_MODE,
    FIELD_COMMENT,
    FIELD_COUNT /* a cool trick to count the number of fields :) */
  };

  static int
  create_event(THD *thd, Event_timed *et, Event_parse_data *parse_data,
               uint create_options, uint *rows_affected);

  static int
  update_event(THD *thd, Event_timed *et, Event_parse_data *parse_data,
               sp_name *new_name, uint *rows_affected);

  static int
  drop_event(THD *thd, Event_timed *et, Event_parse_data *parse_data,
             bool drop_if_exists, uint *rows_affected);

  static int
  open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table);

  static int
  show_create_event(THD *thd, sp_name *spn);

  static int
  reconstruct_interval_expression(String *buf, interval_type interval,
                                  longlong expression);

  static int
  drop_schema_events(THD *thd, char *db);
  
  static int
  dump_internal_status(THD *thd);
  
  static int
  init();
  
  static void
  shutdown();

  static void
  init_mutexes();
  
  static void
  destroy_mutexes();


private:
  /* Prevent use of these */
  Events(const Events &);
  void operator=(Events &);
};


#endif /* _EVENT_H_ */
