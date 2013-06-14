/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_TRIGGER_DISPATCHER_H_INCLUDED
#define TABLE_TRIGGER_DISPATCHER_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

#include "table.h"                              /* GRANT_INFO */
#include "trigger.h"

#include <mysqld_error.h>

///////////////////////////////////////////////////////////////////////////

struct TABLE_LIST;

class sp_name;
class Query_tables_list;
class Trigger;

///////////////////////////////////////////////////////////////////////////

/**
  This class holds all information about triggers of table.
*/

class Table_trigger_dispatcher: public Sql_alloc
{
private:
  /// Triggers for this table object. This is the main trigger collection.
  List<Trigger> m_triggers;

  /// Triggers grouped by event, action_time.
  Trigger *m_trigger_map[TRG_EVENT_MAX][TRG_ACTION_MAX];

  /**
    Copy of TABLE::Field array with field pointers set to TABLE::record[1]
    buffer instead of TABLE::record[0] (used for OLD values in on UPDATE
    trigger and DELETE trigger when it is called for REPLACE).
  */
  Field             **m_record1_field;

  /**
    During execution of trigger m_new_field and m_old_field should point to the
    array of fields representing new or old version of row correspondingly
    (so it can point to TABLE::field or to Table_trigger_dispatcher::m_record1_field)
  */
  Field             **m_new_field;
  Field             **m_old_field;

private:
  /**
     This flag indicates that one of the triggers was not parsed successfully,
     and as a precaution the object has entered a state where all trigger
     access results in errors until all such triggers are dropped. It is not
     safe to add triggers since we don't know if the broken trigger has the
     same name or event type. Nor is it safe to invoke any trigger for the
     aforementioned reasons. The only safe operations are drop_trigger and
     drop_all_triggers.

     @see Table_trigger_dispatcher::set_parse_error

    FIXME: get rid of this flag.
   */
  bool m_has_unparseable_trigger;

  /**
    This error will be displayed when the user tries to manipulate or invoke
    triggers on a table that has broken triggers. It will get set only once
    per statement and thus will contain the first parse error encountered in
    the trigger file.
   */
  char m_parse_error_message[MYSQL_ERRMSG_SIZE];

  /**
     Signals to the Table_trigger_dispatcher that a parse error has occurred
     when reading a trigger from file. This makes the Table_trigger_dispatcher
     enter an error state flagged by m_has_unparseable_trigger == true. The
     error message will be used whenever a statement invoking or manipulating
     triggers is issued against the Table_trigger_dispatcher's table.

     @param error_message The error message thrown by the parser.
  */
  void set_parse_error_message(const char *error_message)
  {
    if (!m_has_unparseable_trigger)
    {
      m_has_unparseable_trigger= true;
      strncpy(m_parse_error_message, error_message, sizeof(m_parse_error_message));
    }
  }

  /**
    Checks if there is a broken trigger for this table.

    @retval false if all triggers are Ok.
    @retval true in case there is at least one broken trigger (a trigger which
    SQL-definition can not be parsed) for this table.
  */
  bool check_for_broken_triggers()
  {
    if (m_has_unparseable_trigger)
    {
      my_message(ER_PARSE_ERROR, m_parse_error_message, MYF(0));
      return true;
    }
    return false;
  }

public:
  /** TABLE instance for which this triggers list object was created. */
  TABLE *trigger_table;

  ///////////////////////////////////////////////////////////////////////////

public:
  static bool check_n_load(THD *thd,
                           const char *db_name,
                           const char *table_name,
                           TABLE *table,
                           bool names_only);

  static bool drop_all_triggers(THD *thd, char *db_name, char *table_name);

  static bool change_table_name(THD *thd,
                                const char *db_name,
                                const char *old_alias,
                                const char *old_table,
                                const char *new_db,
                                const char *new_table);

public:
  Table_trigger_dispatcher(TABLE *table)
   :m_record1_field(NULL),
    m_has_unparseable_trigger(false),
    trigger_table(table)
  {
    memset(m_trigger_map, 0, sizeof(m_trigger_map));
  }

  ~Table_trigger_dispatcher();

  bool create_trigger(THD *thd, TABLE_LIST *table, String *stmt_query);

  bool drop_trigger(THD *thd, TABLE_LIST *table, String *stmt_query);

  bool process_triggers(THD *thd, trg_event_type event,
                        trg_action_time_type action_time,
                        bool old_row_is_record1);

  Trigger *get_trigger(trg_event_type event,
                       trg_action_time_type action_time)
  {
    return m_trigger_map[event][action_time];
  }

  const Trigger *get_trigger(trg_event_type event,
                             trg_action_time_type action_time) const
  {
    return m_trigger_map[event][action_time];
  }

  // FIXME: sql_show.cc: remove this method, use get_trigger() instead.
  bool get_trigger_info(THD *thd,
                        trg_event_type event,
                        trg_action_time_type action_time,
                        LEX_STRING *trigger_name,
                        LEX_STRING *trigger_stmt,
                        sql_mode_t *sql_mode,
                        LEX_STRING *definer,
                        LEX_STRING *client_cs_name,
                        LEX_STRING *connection_cl_name,
                        LEX_STRING *db_cl_name);

  // FIXME: sql_show.cc: remove this method, use get_trigger() instead.
  void get_trigger_info(THD *thd,
                        int trigger_idx,
                        LEX_STRING *trigger_name,
                        sql_mode_t *sql_mode,
                        LEX_STRING *sql_original_stmt,
                        LEX_STRING *client_cs_name,
                        LEX_STRING *connection_cl_name,
                        LEX_STRING *db_cl_name);

  // FIXME: sql_show.cc: remove this method -- return Trigger object instead.
  int find_trigger_by_name(const LEX_STRING *trigger_name);

  bool has_triggers(trg_event_type event,
                    trg_action_time_type action_time) const
  {
    return get_trigger(event, action_time) != NULL;
  }

  bool has_delete_triggers() const
  {
    return get_trigger(TRG_EVENT_DELETE, TRG_ACTION_BEFORE) ||
           get_trigger(TRG_EVENT_DELETE, TRG_ACTION_AFTER);
  }

  void set_table(TABLE *new_table);

  void mark_fields_used(trg_event_type event);

  bool add_tables_and_routines_for_triggers(THD *thd,
                                            Query_tables_list *prelocking_ctx,
                                            TABLE_LIST *table_list);
  void enable_fields_temporary_nullability(THD* thd);
  void disable_fields_temporary_nullability();

private:
  bool prepare_record1_accessors();

  bool change_table_name_in_triggers(THD *thd,
                                     const char *old_db_name,
                                     const char *new_db_name,
                                     LEX_STRING *old_table_name,
                                     LEX_STRING *new_table_name,
                                     bool upgrading50to51);

  bool parse_triggers(THD *thd);
  void setup_triggers(THD *thd, bool names_only);

private:
  // Item_trigger_field uses m_new_field and m_old_field directly.
  friend class Item_trigger_field;
};

///////////////////////////////////////////////////////////////////////////

#endif // TABLE_TRIGGER_DISPATCHER_H_INCLUDED
