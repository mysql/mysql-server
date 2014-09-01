/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TRIGGER_H_INCLUDED
#define TRIGGER_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

#include "sql_alloc.h"
#include "trigger_def.h"  // enum_trigger_event_type

struct GRANT_INFO;

class sp_head;
class Stored_program_creation_ctx;
struct TABLE;
class Query_tables_list;

typedef ulonglong sql_mode_t;

/**
  This class represents a trigger object.
  Trigger can be created, initialized, parsed and executed.

  Trigger attributes are usually stored on the memory root of the subject table.
  Trigger object however can exist when the subject table does not. In this
  case, trigger attributes are stored on a separate memory root.

  Trigger objects are created in two ways:

    1. loading from Data Dictionary (by Trigger_loader)

      In this case Trigger object is initialized in two phases:
        - from the data which is directly available in TRG-file;
        - from the data which gets available after parsing CREATE TRIGGER
          statement (trigger name, ...)

      @see Trigger::create_from_dd().

    2. creating a new Trigger object that represents the trigger object being
       created by CREATE TRIGGER statement (by Table_trigger_dispatcher).

       In this case Trigger object is created temporarily.

      @see Trigger::create_from_parser().
*/
class Trigger : public Sql_alloc
{
public:
  static Trigger *create_from_parser(THD *thd,
                                     TABLE *subject_table,
                                     String *binlog_create_trigger_stmt);

  static Trigger *create_from_dd(MEM_ROOT *mem_root,
                                 const LEX_CSTRING &db_name,
                                 const LEX_CSTRING &subject_table_name,
                                 const LEX_STRING &definition,
                                 sql_mode_t sql_mode,
                                 const LEX_STRING &definer,
                                 const LEX_STRING &client_cs_name,
                                 const LEX_STRING &connection_cl_name,
                                 const LEX_STRING &db_cl_name,
                                 const longlong *created_timestamp);

public:
  bool execute(THD *thd);

  bool parse(THD *thd);

  void add_tables_and_routines(THD *thd,
                               Query_tables_list *prelocking_ctx,
                               TABLE_LIST *table_list);

  void print_upgrade_warning(THD *thd);

  void rename_subject_table(THD *thd, const LEX_STRING &new_table_name);

public:
  /************************************************************************
   * Attribute accessors.
   ***********************************************************************/

  const LEX_CSTRING &get_db_name() const
  { return m_db_name; }

  const LEX_CSTRING &get_subject_table_name() const
  { return m_subject_table_name; }

  const LEX_STRING &get_trigger_name() const
  { return m_trigger_name; }

  const LEX_STRING &get_definition() const
  { return m_definition; }

  sql_mode_t get_sql_mode() const
  { return m_sql_mode; }

  const LEX_STRING &get_definer() const
  { return m_definer; }

  const LEX_STRING &get_on_table_name() const
  { return m_on_table_name; }

  const LEX_STRING &get_client_cs_name() const
  { return m_client_cs_name; }

  const LEX_STRING &get_connection_cl_name() const
  { return m_connection_cl_name; }

  const LEX_STRING &get_db_cl_name() const
  { return m_db_cl_name; }

  enum_trigger_event_type get_event() const
  { return m_event; }

  enum_trigger_action_time_type get_action_time() const
  { return m_action_time; }

  bool is_created_timestamp_null() const
  { return m_created_timestamp == 0; }

  timeval get_created_timestamp() const
  {
    timeval timestamp_value;
    timestamp_value.tv_sec= static_cast<long>(m_created_timestamp / 100);
    timestamp_value.tv_usec= (m_created_timestamp % 100) * 10000;
    return timestamp_value;
  }

  ulonglong get_action_order() const
  { return m_action_order; }

  void set_action_order(ulonglong action_order)
  { m_action_order= action_order; }

  sp_head *get_sp()
  { return m_sp; }

  GRANT_INFO *get_subject_table_grant()
  { return &m_subject_table_grant; }

  bool has_parse_error() const
  { return m_has_parse_error; }

  const char *get_parse_error_message() const
  { return m_parse_error_message; }

public:
  /************************************************************************
   * To be used by Trigger_loader only
   ***********************************************************************/

  LEX_STRING *get_definition_ptr()
  { return &m_definition; }

  sql_mode_t *get_sql_mode_ptr()
  { return &m_sql_mode; }

  LEX_STRING *get_definer_ptr()
  { return &m_definer; }

  LEX_STRING *get_client_cs_name_ptr()
  { return &m_client_cs_name; }

  LEX_STRING *get_connection_cl_name_ptr()
  { return &m_connection_cl_name; }

  LEX_STRING *get_db_cl_name_ptr()
  { return &m_db_cl_name; }

  longlong *get_created_timestamp_ptr()
  { return &m_created_timestamp; }

private:
  Trigger(MEM_ROOT *mem_root,
          const LEX_CSTRING &db_name,
          const LEX_CSTRING &table_name,
          const LEX_STRING &definition,
          sql_mode_t sql_mode,
          const LEX_STRING &definer,
          const LEX_STRING &client_cs_name,
          const LEX_STRING &connection_cl_name,
          const LEX_STRING &db_cl_name,
          enum_trigger_event_type event_type,
          enum_trigger_action_time_type action_time,
          longlong created_timestamp);

public:
  ~Trigger();

private:
  void set_trigger_name(const LEX_STRING &trigger_name)
  { m_trigger_name= trigger_name; }

  void set_parse_error_message(const char *error_message)
  {
    m_has_parse_error= true;
    strncpy(m_parse_error_message, error_message,
            sizeof(m_parse_error_message));
  }

private:
  /**
    Memory root to store all data of this Trigger object.

    This can be a pointer to the subject table memory root, or it can be a
    pointer to a dedicated memory root if subject table does not exist.
  */
  MEM_ROOT *m_mem_root;

private:
  /************************************************************************
   * Mandatory trigger attributes loaded from TRG-file.
   * All these strings are allocated on m_mem_root.
   ***********************************************************************/

  /// Database name.
  LEX_CSTRING m_db_name;

  /// Table name.
  LEX_CSTRING m_subject_table_name;

  /// Trigger definition to save in TRG-file.
  LEX_STRING m_definition;

  /// Trigger sql-mode.
  sql_mode_t m_sql_mode;

  /// Trigger definer.
  LEX_STRING m_definer;

  /// Character set context, used for parsing and executing trigger.
  LEX_STRING m_client_cs_name;

  /// Collation name of the connection within one a trigger are created.
  LEX_STRING m_connection_cl_name;

  /// Default database collation.
  LEX_STRING m_db_cl_name;

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
  longlong m_created_timestamp;

  /**
    Action_order value for the trigger. Action_order is the ordinal position
    of the trigger in the list of triggers with the same EVENT_MANIPULATION,
    CONDITION_TIMING, and ACTION_ORIENTATION.

    At the moment action order is not explicitly stored in the TRG-file. Trigger
    execution order however is mantained by the order of trigger attributes in
    the TRG-file. This attribute is calculated after loading.
  */
  ulonglong m_action_order;

private:
  /************************************************************************
   * The following attributes can be set only after parsing trigger definition
   * statement (CREATE TRIGGER). There is no way to retrieve them directly from
   * TRG-file.
   *
   * All these strings are allocated on the trigger table's mem-root.
   ***********************************************************************/

  /// Trigger name.
  LEX_STRING m_trigger_name;

  /**
    A pointer to the "ON <table name>" part of the trigger definition. It is
    used for updating trigger definition in RENAME TABLE.
  */
  LEX_STRING m_on_table_name;

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

#endif // TRIGGER_H_INCLUDED

