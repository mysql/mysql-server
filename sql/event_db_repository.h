#ifndef _EVENT_DB_REPOSITORY_H_
#define _EVENT_DB_REPOSITORY_H_

/*
   Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_global.h"
#include "mysql/mysql_lex_string.h"             // LEX_STRING

class Event_basic;
class Event_parse_data;
class THD;
struct TABLE_LIST;
typedef struct st_mysql_lex_string LEX_STRING;
typedef long my_time_t;

/**
  @addtogroup Event_Scheduler
  @{

  @file event_db_repository.h

  Data Dictionary related operations of Event Scheduler.

  This is a private header file of Events module. Please do not include it
  directly. All public declarations of Events module should be stored in
  events.h and event_data_objects.h.
*/

class Event_db_repository
{
public:
  Event_db_repository(){}

  bool create_event(THD *thd, Event_parse_data *parse_data,
                    bool create_if_not, bool *event_already_exists);

  bool update_event(THD *thd, Event_parse_data *parse_data,
                    LEX_STRING *new_dbname, LEX_STRING *new_name);

  bool drop_event(THD *thd, LEX_STRING db, LEX_STRING name,
                  bool drop_if_exists);

  bool drop_schema_events(THD *thd, LEX_STRING schema);

  bool load_named_event(THD *thd, LEX_STRING dbname, LEX_STRING name,
                        Event_basic *et);

  bool fill_schema_events(THD *thd, TABLE_LIST *tables, const char *db);

  bool update_timing_fields_for_event(THD *thd,
                                      LEX_STRING event_db_name,
                                      LEX_STRING event_name,
                                      my_time_t last_executed,
                                      ulonglong status);

  // Disallow copy construction and assignment.
  Event_db_repository(const Event_db_repository &)= delete;
  void operator=(Event_db_repository &)= delete;
};

/**
  @} (End of group Event_Scheduler)
*/
#endif /* _EVENT_DB_REPOSITORY_H_ */
