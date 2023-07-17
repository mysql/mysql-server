/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_TRIGGER_DISPATCHER_H_INCLUDED
#define TABLE_TRIGGER_DISPATCHER_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <string.h>

#include "lex_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql_com.h"                        // MYSQL_ERRMSG_SIZE
#include "mysqld_error.h"                     // ER_PARSE_ERROR
#include "sql/sql_list.h"                     // List
#include "sql/table_trigger_field_support.h"  // Table_trigger_field_support
#include "sql/trigger_def.h"                  // enum_trigger_action_time_type

class Field;
class Query_tables_list;
class String;
class THD;
class Trigger;
class Trigger_chain;
struct MEM_ROOT;

namespace dd {
class Table;
}  // namespace dd
struct TABLE;
class Table_ref;
template <class T>
class List;

namespace table_cache_unittest {
class Mock_share;
}

///////////////////////////////////////////////////////////////////////////

/**
  This class holds all information about triggers of a table.
*/

class Table_trigger_dispatcher : public Table_trigger_field_support {
 public:
  static Table_trigger_dispatcher *create(TABLE *subject_table);

  bool finalize_load(THD *thd);

 private:
  Table_trigger_dispatcher(TABLE *subject_table);

  friend class table_cache_unittest::Mock_share;

 public:
  ~Table_trigger_dispatcher() override;

  /**
    Checks if there is a broken trigger for this table.

    @retval false if all triggers are Ok.
    @retval true in case there is at least one broken trigger (a trigger which
    SQL-definition can not be parsed) for this table.
  */
  bool check_for_broken_triggers() {
    if (m_parse_error_message) {
      my_message(ER_PARSE_ERROR, m_parse_error_message, MYF(0));
      return true;
    }
    return false;
  }

  /**
    Create trigger for table.

    @param      thd   Thread context
    @param[out] binlog_create_trigger_stmt
                      Well-formed CREATE TRIGGER statement for putting into
    binlog (after successful execution)
    @param      if_not_exists
                      True if 'IF NOT EXISTS' clause was specified
    @param[out] already_exists
                      Set to true if trigger already exists on the same table

    @note
      - Assumes that trigger name is fully qualified.
      - NULL-string means the following LEX_STRING instance:
      { str = 0; length = 0 }.
      - In other words, definer_user and definer_host should contain
      simultaneously NULL-strings (non-SUID/old trigger) or valid strings
      (SUID/new trigger).

    @return Operation status.
      @retval false Success
      @retval true  Failure
  */
  bool create_trigger(THD *thd, String *binlog_create_trigger_stmt,
                      bool if_not_exists, bool &already_exists);

  bool process_triggers(THD *thd, enum_trigger_event_type event,
                        enum_trigger_action_time_type action_time,
                        bool old_row_is_record1);

  Trigger_chain *get_triggers(int event, int action_time) {
    assert(0 <= event && event < TRG_EVENT_MAX);
    assert(0 <= action_time && action_time < TRG_ACTION_MAX);
    return m_trigger_map[event][action_time];
  }

  const Trigger_chain *get_triggers(int event, int action_time) const {
    assert(0 <= event && event < TRG_EVENT_MAX);
    assert(0 <= action_time && action_time < TRG_ACTION_MAX);
    return m_trigger_map[event][action_time];
  }

  Trigger *find_trigger(const LEX_STRING &trigger_name);

  bool has_triggers(enum_trigger_event_type event,
                    enum_trigger_action_time_type action_time) const {
    return get_triggers(event, action_time) != nullptr;
  }

  bool has_update_triggers() const {
    return get_triggers(TRG_EVENT_UPDATE, TRG_ACTION_BEFORE) ||
           get_triggers(TRG_EVENT_UPDATE, TRG_ACTION_AFTER);
  }

  bool has_delete_triggers() const {
    return get_triggers(TRG_EVENT_DELETE, TRG_ACTION_BEFORE) ||
           get_triggers(TRG_EVENT_DELETE, TRG_ACTION_AFTER);
  }

  bool mark_fields(enum_trigger_event_type event);

  bool add_tables_and_routines_for_triggers(THD *thd,
                                            Query_tables_list *prelocking_ctx,
                                            Table_ref *table_list);

  void enable_fields_temporary_nullability(THD *thd);
  void disable_fields_temporary_nullability();

  void print_upgrade_warnings(THD *thd);

  void parse_triggers(THD *thd, List<Trigger> *triggers, bool is_upgrade);

  /**
    Check whether we have finalized loading of triggers for the table
    by parsing their bodies, creating sp_head objects and preparing
    row-accessors.
  */
  bool has_load_been_finalized() { return m_load_finalized; }

 private:
  Trigger_chain *create_trigger_chain(
      MEM_ROOT *mem_root, enum_trigger_event_type event,
      enum_trigger_action_time_type action_time);

  bool prepare_record1_accessors();

  /**
    Remember a parse error that occurred while parsing trigger definitions
    loaded from the Data Dictionary. This makes the Table_trigger_dispatcher
    enter the error state flagged by m_parse_error_message != nullptr . The
    error message will be used whenever a statement invoking or manipulating
    triggers is issued against the Table_trigger_dispatcher's table.

    @param error_message The error message thrown by the parser.
  */
  void set_parse_error_message(const char *error_message);

 private:
  /************************************************************************
   * Table_trigger_field_support interface implementation.
   ***********************************************************************/

  TABLE *get_subject_table() override { return m_subject_table; }

  Field *get_trigger_variable_field(enum_trigger_variable_type v,
                                    int field_index) override {
    return (v == TRG_OLD_ROW) ? m_old_field[field_index]
                              : m_new_field[field_index];
  }

 private:
  /**
    TABLE instance for which this triggers list object was created.
  */
  TABLE *m_subject_table;

  /// Triggers grouped by event, action_time.
  Trigger_chain *m_trigger_map[TRG_EVENT_MAX][TRG_ACTION_MAX];

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
    Error which occurred while parsing one of the triggers for the table;
    nullptr if there was no error for any of its triggers.

    Non-nullptr value indicates that as a precaution the object has entered
    the state where all trigger operations result in errors (referencing
    this error message saved) until all the table triggers are dropped. It is
    not safe to add triggers since it is unknown if the broken trigger has the
    same name or event type. Nor is it safe to invoke any trigger. The only
    safe operations are drop_trigger() and drop_all_triggers().

    @see Table_trigger_dispatcher::set_parse_error()
  */
  const char *m_parse_error_message;

  /** Indicates whether we have finalized loading of triggers for the table. */
  bool m_load_finalized;
};

///////////////////////////////////////////////////////////////////////////

#endif  // TABLE_TRIGGER_DISPATCHER_H_INCLUDED
