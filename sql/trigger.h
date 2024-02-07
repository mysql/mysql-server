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

#ifndef TRIGGER_H_INCLUDED
#define TRIGGER_H_INCLUDED

#include <stdint.h>
#include <string.h>

#include "my_config.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>

#include "lex_string.h"
#include "my_inttypes.h"
#include "mysql_com.h"
#include "sql/table.h"        // GRANT_INFO
#include "sql/trigger_def.h"  // enum_trigger_event_type

class Query_tables_list;
class String;
class THD;
class sp_head;
struct MEM_ROOT;

using sql_mode_t = uint64_t;

/**
  This class represents a trigger object.
  Trigger can be created, initialized, parsed and executed.

  Trigger attributes are usually stored on the memory root of the subject table.
  Trigger object however can exist when the subject table does not. In this
  case, trigger attributes are stored on a separate memory root.

  Trigger objects are created in two ways:

    1. loading from Data Dictionary (by Trigger_loader)

      In this case Trigger object is initialized from the data
      which is directly available in data dictionary;

      @see Trigger::create_from_dd().

    2. creating a new Trigger object that represents the trigger object being
       created by CREATE TRIGGER statement (by Table_trigger_dispatcher).

       In this case Trigger object is created temporarily.

      @see Trigger::create_from_parser().
*/
class Trigger {
 public:
  static Trigger *create_from_parser(THD *thd, TABLE *subject_table,
                                     String *binlog_create_trigger_stmt);

  static Trigger *create_from_dd(
      MEM_ROOT *mem_root, const LEX_CSTRING &trigger_name,
      const LEX_CSTRING &db_name, const LEX_CSTRING &subject_table_name,
      const LEX_CSTRING &definition, const LEX_CSTRING &definition_utf8,
      sql_mode_t sql_mode, const LEX_CSTRING &definer_user,
      const LEX_CSTRING &definer_host, const LEX_CSTRING &client_cs_name,
      const LEX_CSTRING &connection_cl_name, const LEX_CSTRING &db_cl_name,
      enum_trigger_event_type trg_event_type,
      enum_trigger_action_time_type trg_time_type, uint action_order,
      my_timeval created_timestamp);

  /**
    Constructs CREATE TRIGGER statement taking into account a value of
    the DEFINER clause.

    The point of this method is to create canonical forms of CREATE TRIGGER
    statement for writing into the binlog.

    @note
    A statement for the binlog form must preserve FOLLOWS/PRECEDES clause
    if it was in the original statement. The reason for that difference is this:

      - the Data Dictionary preserves the trigger execution order
    (action_order), thus FOLLOWS/PRECEDES clause is not needed.

      - moreover, FOLLOWS/PRECEDES clause usually makes problem in mysqldump,
        because CREATE TRIGGER statement will have a reference to
    not-yet-existing trigger (which is about to be created right after this
    one).

      - thus, FOLLOWS/PRECEDES must not be stored in the Data Dictionary.

      - on the other hand, the binlog contains statements in the user order (as
        the user executes them). Thus, it is important to preserve
        FOLLOWS/PRECEDES clause if the user has specified it so that the trigger
        execution order on master and slave will be the same.

    @param thd                thread context
    @param[out] binlog_query  well-formed CREATE TRIGGER statement for putting
                              into binlog (after successful execution)
    @param def_user           user part of a definer value
    @param def_host           host part of a definer value

    @return Operation status.
      @retval false Success
      @retval true  Failure
  */
  static bool construct_create_trigger_stmt_with_definer(
      THD *thd, String *binlog_query, const LEX_CSTRING &def_user,
      const LEX_CSTRING &def_host);

 public:
  bool execute(THD *thd);

  bool parse(THD *thd, bool is_upgrade);

  void add_tables_and_routines(THD *thd, Query_tables_list *prelocking_ctx,
                               Table_ref *table_list);

  void print_upgrade_warning(THD *thd);

 public:
  /************************************************************************
   * Attribute accessors.
   ***********************************************************************/

  const LEX_CSTRING &get_db_name() const { return m_db_name; }

  const LEX_CSTRING &get_subject_table_name() const {
    return m_subject_table_name;
  }

  const LEX_CSTRING &get_trigger_name() const { return m_trigger_name; }

  const LEX_CSTRING &get_definition() const { return m_definition; }

  const LEX_CSTRING &get_definition_utf8() const { return m_definition_utf8; }

  sql_mode_t get_sql_mode() const { return m_sql_mode; }

  const LEX_CSTRING &get_definer() const { return m_definer; }

  const LEX_CSTRING &get_definer_user() const { return m_definer_user; }

  const LEX_CSTRING &get_definer_host() const { return m_definer_host; }

  const LEX_CSTRING &get_client_cs_name() const { return m_client_cs_name; }

  const LEX_CSTRING &get_connection_cl_name() const {
    return m_connection_cl_name;
  }

  const LEX_CSTRING &get_db_cl_name() const { return m_db_cl_name; }

  enum_trigger_event_type get_event() const { return m_event; }

  const LEX_CSTRING &get_event_as_string() const;

  enum_trigger_action_time_type get_action_time() const {
    return m_action_time;
  }

  const LEX_CSTRING &get_action_time_as_string() const;

  bool is_created_timestamp_null() const {
    return m_created_timestamp.m_tv_sec == 0 &&
           m_created_timestamp.m_tv_usec == 0;
  }

  my_timeval get_created_timestamp() const { return m_created_timestamp; }

  ulonglong get_action_order() const { return m_action_order; }

  void set_action_order(ulonglong action_order) {
    m_action_order = action_order;
  }

  sp_head *get_sp() { return m_sp; }

  GRANT_INFO *get_subject_table_grant() { return &m_subject_table_grant; }

  bool has_parse_error() const { return m_has_parse_error; }

  const char *get_parse_error_message() const { return m_parse_error_message; }

  /**
    Construct a full CREATE TRIGGER statement from Trigger's data members.

    @param [in] thd                       Thread context
    @param [out] full_trigger_definition  Place where a CREATE TRIGGER
                                          statement be stored.

    @return Operation status
      @retval true   Failure
      @retval false  Success
  */

  bool create_full_trigger_definition(const THD *thd,
                                      String *full_trigger_definition) const;

 private:
  Trigger(const LEX_CSTRING &trigger_name, MEM_ROOT *mem_root,
          const LEX_CSTRING &db_name, const LEX_CSTRING &table_name,
          const LEX_CSTRING &definition, const LEX_CSTRING &definition_utf8,
          sql_mode_t sql_mode, const LEX_CSTRING &definer_user,
          const LEX_CSTRING &definer_host, const LEX_CSTRING &client_cs_name,
          const LEX_CSTRING &connection_cl_name, const LEX_CSTRING &db_cl_name,
          enum_trigger_event_type event_type,
          enum_trigger_action_time_type action_time, uint action_order,
          my_timeval created_timestamp);

 public:
  ~Trigger();

 private:
  void set_trigger_name(const LEX_CSTRING &trigger_name) {
    m_trigger_name = trigger_name;
  }

  void set_trigger_def(const LEX_CSTRING &trigger_def) {
    m_definition = trigger_def;
  }

  void set_trigger_def_utf8(const LEX_CSTRING &trigger_def_utf8) {
    m_definition_utf8 = trigger_def_utf8;
  }

  void set_parse_error_message(const char *error_message) {
    m_has_parse_error = true;
    snprintf(m_parse_error_message, sizeof(m_parse_error_message), "%s",
             error_message);
  }

  /**
    Memory root to store all data of this Trigger object.

    This can be a pointer to the subject table memory root, or it can be a
    pointer to a dedicated memory root if subject table does not exist.
  */
  MEM_ROOT *m_mem_root;

  /**
    Full trigger definition reconstructed from a data loaded from the table
    mysql.trigger.
  */
  LEX_CSTRING m_full_trigger_definition;

 private:
  /************************************************************************
   * Mandatory trigger attributes loaded from data dictionary.
   * All these strings are allocated on m_mem_root.
   ***********************************************************************/

  /// Database name.
  LEX_CSTRING m_db_name;

  /// Table name.
  LEX_CSTRING m_subject_table_name;

  /// Trigger definition to save in DD.
  LEX_CSTRING m_definition;

  /// Trigger definition in UTF8 to save in DD.
  LEX_CSTRING m_definition_utf8;

  /// Trigger sql-mode.
  sql_mode_t m_sql_mode;

  /// Trigger definer.
  LEX_CSTRING m_definer;

  /// Trigger definer (user part).
  LEX_CSTRING m_definer_user;

  /// Trigger definer (host part).
  LEX_CSTRING m_definer_host;

  /// Character set context, used for parsing and executing trigger.
  LEX_CSTRING m_client_cs_name;

  /// Collation name of the connection within one a trigger are created.
  LEX_CSTRING m_connection_cl_name;

  /// Default database collation.
  LEX_CSTRING m_db_cl_name;

  /// Trigger event.
  enum_trigger_event_type m_event;

  /// Trigger action time.
  enum_trigger_action_time_type m_action_time;

  /**
    Current time when the trigger was created (measured in milliseconds since
    since 0 hours, 0 minutes, 0 seconds, January 1, 1970, UTC). This is the
    value of CREATED attribute.

    There is special value -- zero means CREATED is not set (NULL).
  */
  my_timeval m_created_timestamp;

  /**
    Action_order value for the trigger. Action_order is the ordinal position
    of the trigger in the list of triggers with the same EVENT_MANIPULATION,
    CONDITION_TIMING, and ACTION_ORIENTATION.
  */
  ulonglong m_action_order;

 private:
  /************************************************************************
   * All these strings are allocated on the trigger table's mem-root.
   ***********************************************************************/

  /// Trigger name.
  LEX_CSTRING m_trigger_name;

 private:
  /************************************************************************
   * Other attributes.
   ***********************************************************************/

  /// Grant information for the trigger.
  GRANT_INFO m_subject_table_grant;

  /// Pointer to the sp_head corresponding to the trigger.
  sp_head *m_sp;

  /// This flags specifies whether the trigger has parse error or not.
  bool m_has_parse_error;

  /**
    This error will be displayed when the user tries to manipulate or invoke
    triggers on a table that has broken triggers. It will get set only once
    per statement and thus will contain the first parse error encountered in
    the trigger file.
  */
  char m_parse_error_message[MYSQL_ERRMSG_SIZE];
};

///////////////////////////////////////////////////////////////////////////

#endif  // TRIGGER_H_INCLUDED
