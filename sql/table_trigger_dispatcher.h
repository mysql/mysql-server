/*
   Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "mysql_com.h"                    // MYSQL_ERRMSG_SIZE
#include "mysql/mysql_lex_string.h"       // LEX_STRING
#include "sql_alloc.h"                    // Sql_alloc
#include "sql_list.h"                     // List
#include "table.h"                        // TABLE
#include "trigger_def.h"                  // enum_trigger_action_time_type
#include "table_trigger_field_support.h"  // Table_trigger_field_support
#include "mysqld_error.h"                 // ER_PARSE_ERROR

///////////////////////////////////////////////////////////////////////////

class Query_tables_list;
class String;
class Trigger_chain;
class Trigger;
typedef struct st_mysql_lex_string LEX_STRING;

///////////////////////////////////////////////////////////////////////////

/**
  This class holds all information about triggers of a table.
*/

class Table_trigger_dispatcher : public Sql_alloc,
                                 public Table_trigger_field_support
{
public:
  static Table_trigger_dispatcher *create(TABLE *subject_table);

  bool check_n_load(THD *thd, bool names_only);

private:
  Table_trigger_dispatcher(TABLE *subject_table);

public:
  Table_trigger_dispatcher(const char *db_name, const char *table_name);
  ~Table_trigger_dispatcher();

  Table_trigger_field_support *get_trigger_field_support()
  { return this; }

  List<Trigger> &get_trigger_list()
  { return m_triggers; }

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

  bool create_trigger(THD *thd, String *binlog_create_trigger_stmt);

  bool drop_trigger(THD *thd,
                    const LEX_STRING &trigger_name,
                    bool *trigger_found);

  bool process_triggers(THD *thd, enum_trigger_event_type event,
                        enum_trigger_action_time_type action_time,
                        bool old_row_is_record1);

  Trigger_chain *get_triggers(int event, int action_time)
  {
    DBUG_ASSERT(0 <= event && event < TRG_EVENT_MAX);
    DBUG_ASSERT(0 <= action_time && action_time < TRG_ACTION_MAX);
    return m_trigger_map[event][action_time];
  }

  const Trigger_chain *get_triggers(int event, int action_time) const
  {
    DBUG_ASSERT(0 <= event && event < TRG_EVENT_MAX);
    DBUG_ASSERT(0 <= action_time && action_time < TRG_ACTION_MAX);
    return m_trigger_map[event][action_time];
  }

  Trigger *find_trigger(const LEX_STRING &trigger_name);

  bool has_triggers(enum_trigger_event_type event,
                    enum_trigger_action_time_type action_time) const
  {
    return get_triggers(event, action_time) != NULL;
  }

  bool has_update_triggers() const
  {
    return get_triggers(TRG_EVENT_UPDATE, TRG_ACTION_BEFORE) ||
           get_triggers(TRG_EVENT_UPDATE, TRG_ACTION_AFTER);
  }

  bool has_delete_triggers() const
  {
    return get_triggers(TRG_EVENT_DELETE, TRG_ACTION_BEFORE) ||
           get_triggers(TRG_EVENT_DELETE, TRG_ACTION_AFTER);
  }

  bool mark_fields(enum_trigger_event_type event);

  bool add_tables_and_routines_for_triggers(THD *thd,
                                            Query_tables_list *prelocking_ctx,
                                            TABLE_LIST *table_list);

  void enable_fields_temporary_nullability(THD *thd);
  void disable_fields_temporary_nullability();

  void print_upgrade_warnings(THD *thd);

  bool rename_subject_table(THD *thd,
                                 const char *old_db_name,
                                 const char *new_db_name,
                                 const char *old_table_name_str,
                                 const char *new_table_name_str,
                                 bool upgrading50to51);


private:
  MEM_ROOT *get_mem_root()
  { return m_subject_table ? &m_subject_table->mem_root : &m_mem_root; }

  Trigger_chain *create_trigger_chain(
    enum_trigger_event_type event,
    enum_trigger_action_time_type action_time);

  void parse_triggers(THD *thd);

  bool prepare_record1_accessors();

  bool rebuild_trigger_list();

  /**
    Remember a parse error that occurred while parsing trigger definitions
    loaded from the Data Dictionary. This makes the Table_trigger_dispatcher
    enter the error state flagged by m_has_unparseable_trigger == true. The
    error message will be used whenever a statement invoking or manipulating
    triggers is issued against the Table_trigger_dispatcher's table.

    @param error_message The error message thrown by the parser.
  */
  void set_parse_error_message(const char *error_message)
  {
    if (!m_has_unparseable_trigger)
    {
      m_has_unparseable_trigger= true;
      strncpy(m_parse_error_message,
              error_message, sizeof(m_parse_error_message));
    }
  }

private:
  /************************************************************************
   * Table_trigger_field_support interface implementation.
   ***********************************************************************/

  virtual TABLE *get_subject_table()
  { return m_subject_table; }

  virtual Field *get_trigger_variable_field(enum_trigger_variable_type v,
                                            int field_index)
  {
    return (v == TRG_OLD_ROW) ?
           m_old_field[field_index] :
           m_new_field[field_index];
  }

private:
  /**
    TABLE instance for which this triggers list object was created.

    @note TABLE-instance can be NULL in case when "simple" loading of triggers
    is requested.
  */
  TABLE *m_subject_table;

  /**
    Memory root to allocate all the data of this class.

    It either points to the subject table memory root (in case of "full"
    trigger loading), or it can be a separate mem-root that will be destroyed
    after trigger loading.

    @note never use this attribute directly! Use get_mem_root() instead.
  */
  MEM_ROOT m_mem_root;

  /**
    Schema (database) name.

    If m_subject_table is not NULL, it should be equal to
    m_subject_table->s->db. The thing is that m_subject_table can be NULL, so
    there should be a place to store the schema name.
  */
  LEX_STRING m_db_name;

  /**
    Subject table name.

    If m_subject_table is not NULL, it should be equal to
    m_subject_table->s->table_name. The thing is that m_subject_table can be
    NULL, so there should be a place to store the subject table name.
  */
  LEX_STRING m_subject_table_name;

  /**
    List of all triggers associated with a table.

    There is no guarantee to have any particular order in this list, so this
    list must not be used when the order is important.

    The thing is that this list is used to load triggers, so right after loading
    it has the right order. However, later the list might get unordered because
    of create/drop operations.

    This list also contains triggers with parse errors. Such triggers are not
    stored in m_trigger_map.
  */
  List<Trigger> m_triggers;

  /// Triggers grouped by event, action_time.
  Trigger_chain *m_trigger_map[TRG_EVENT_MAX][TRG_ACTION_MAX];

  /// Special trigger chain to store triggers with parse errors.
  Trigger_chain *m_unparseable_triggers;

  /**
    Copy of TABLE::Field array with field pointers set to TABLE::record[1]
    buffer instead of TABLE::record[0] (used for OLD values in on UPDATE
    trigger and DELETE trigger when it is called for REPLACE).
  */
  Field **m_record1_field;

  /**
    During execution of trigger m_new_field and m_old_field should point to the
    array of fields representing new or old version of row correspondingly
    (so it can point to TABLE::field or to
    Table_trigger_dispatcher::m_record1_field).
  */
  Field **m_new_field;
  Field **m_old_field;

  /**
    This flag indicates that one of the triggers was not parsed successfully,
    and as a precaution the object has entered the state where all trigger
    operations result in errors until all the table triggers are dropped. It is
    not safe to add triggers since it is unknown if the broken trigger has the
    same name or event type. Nor is it safe to invoke any trigger. The only
    safe operations are drop_trigger() and drop_all_triggers().

    @note if a trigger is badly damaged its Trigger-object will be destroyed
    right after parsing, so that it will not get into m_unparseable_triggers
    list. So, we need this and other similar attributes to preserve error
    message about bad trigger.

    We can't use the value of m_parse_error_message as a flag to inform that
    a trigger has a parse error since for multi-byte locale the first byte of
    message can be 0 but the message still be meaningful. It means that just a
    comparison against m_parse_error_message[0] can not be done safely.

    @see Table_trigger_dispatcher::set_parse_error()
  */
  bool m_has_unparseable_trigger;

  /**
    This error will be displayed when the user tries to manipulate or invoke
    triggers on a table that has broken triggers. It is set once per statement
    and thus will contain the first parse error encountered in the trigger file.
   */
  char m_parse_error_message[MYSQL_ERRMSG_SIZE];
};

///////////////////////////////////////////////////////////////////////////

#endif // TABLE_TRIGGER_DISPATCHER_H_INCLUDED
