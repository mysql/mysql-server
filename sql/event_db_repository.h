#ifndef _EVENT_DB_REPOSITORY_H_
#define _EVENT_DB_REPOSITORY_H_
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

/**
  @addtogroup Event_Scheduler
  @{

  @file event_db_repository.h

  Data Dictionary related operations of Event Scheduler.

  This is a private header file of Events module. Please do not include it
  directly. All public declarations of Events module should be stored in
  events.h and event_data_objects.h.
*/

enum enum_events_table_field
{
  ET_FIELD_DB = 0, 
  ET_FIELD_NAME,
  ET_FIELD_BODY,
  ET_FIELD_DEFINER,
  ET_FIELD_EXECUTE_AT,
  ET_FIELD_INTERVAL_EXPR,
  ET_FIELD_TRANSIENT_INTERVAL,
  ET_FIELD_CREATED,
  ET_FIELD_MODIFIED,
  ET_FIELD_LAST_EXECUTED,
  ET_FIELD_STARTS,
  ET_FIELD_ENDS,
  ET_FIELD_STATUS,
  ET_FIELD_ON_COMPLETION,
  ET_FIELD_SQL_MODE,
  ET_FIELD_COMMENT,
  ET_FIELD_ORIGINATOR,
  ET_FIELD_TIME_ZONE,
  ET_FIELD_CHARACTER_SET_CLIENT,
  ET_FIELD_COLLATION_CONNECTION,
  ET_FIELD_DB_COLLATION,
  ET_FIELD_BODY_UTF8,
  ET_FIELD_COUNT /* a cool trick to count the number of fields :) */
};


int
events_table_index_read_for_db(THD *thd, TABLE *schema_table,
                               TABLE *event_table);

int
events_table_scan_all(THD *thd, TABLE *schema_table, TABLE *event_table);


class Event_basic;
class Event_parse_data;

class Event_db_repository
{
public:
  Event_db_repository(){}

  bool
  create_event(THD *thd, Event_parse_data *parse_data, my_bool create_if_not);

  bool
  update_event(THD *thd, Event_parse_data *parse_data, LEX_STRING *new_dbname,
               LEX_STRING *new_name);

  bool
  drop_event(THD *thd, LEX_STRING db, LEX_STRING name, bool drop_if_exists);

  void
  drop_schema_events(THD *thd, LEX_STRING schema);

  bool
  find_named_event(LEX_STRING db, LEX_STRING name, TABLE *table);

  bool
  load_named_event(THD *thd, LEX_STRING dbname, LEX_STRING name, Event_basic *et);

  bool
  open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table);

  bool
  fill_schema_events(THD *thd, TABLE_LIST *tables, const char *db);

  bool
  update_timing_fields_for_event(THD *thd,
                                 LEX_STRING event_db_name,
                                 LEX_STRING event_name,
                                 bool update_last_executed,
                                 my_time_t last_executed,
                                 bool update_status,
                                 ulonglong status);
public:
  static bool
  check_system_tables(THD *thd);
private:
  void
  drop_events_by_field(THD *thd, enum enum_events_table_field field,
                       LEX_STRING field_value);
  bool
  index_read_for_db_for_i_s(THD *thd, TABLE *schema_table, TABLE *event_table,
                            const char *db);

  bool
  table_scan_all_for_i_s(THD *thd, TABLE *schema_table, TABLE *event_table);

private:
  /* Prevent use of these */
  Event_db_repository(const Event_db_repository &);
  void operator=(Event_db_repository &);
};

/**
  @} (End of group Event_Scheduler)
*/
#endif /* _EVENT_DB_REPOSITORY_H_ */
