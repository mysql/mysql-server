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

#ifndef TRIGGER_H_INCLUDED
#define TRIGGER_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

struct GRANT_INFO;

class sp_head;
class Stored_program_creation_ctx;

/** Event on which trigger is invoked. */
enum trg_event_type
{
  TRG_EVENT_INSERT= 0,
  TRG_EVENT_UPDATE= 1,
  TRG_EVENT_DELETE= 2,
  TRG_EVENT_MAX
};

/*
  We need this two enums here instead of sql_lex.h because
  at least one of them is used by Item_trigger_field interface.

  Time when trigger is invoked (i.e. before or after row actually
  inserted/updated/deleted).
*/
enum trg_action_time_type
{
  TRG_ACTION_BEFORE= 0,
  TRG_ACTION_AFTER= 1,
  TRG_ACTION_MAX
};

#include "my_global.h"
#include "sql_alloc.h"

class Query_tables_list;

// FIXME: remove it.
typedef ulonglong sql_mode_t;


/**
  This is a class that represents a trigger entity.
  Trigger can be created, initialized, parsed and executed.
*/
class Trigger : public Sql_alloc
{
public:
  Trigger(LEX_STRING *db_name,
          LEX_STRING *table_name,
          LEX_STRING *trg_create_str,
          sql_mode_t trg_sql_mode,
          LEX_STRING *trg_definer,
          LEX_STRING *client_cs_name,
          LEX_STRING *connection_cl_name,
          LEX_STRING *db_cl_name)
   :m_trigger_name(NULL),
    m_db_name(db_name),
    m_table_name(table_name),
    m_on_table_name(NULL),
    m_definition(trg_create_str),
    m_sql_mode(trg_sql_mode),
    m_definer(trg_definer),
    m_client_cs_name(client_cs_name),
    m_connection_cl_name(connection_cl_name),
    m_db_cl_name(db_cl_name),
    m_sp(NULL),
    m_action_time(TRG_ACTION_MAX),
    m_event(TRG_EVENT_MAX),
    m_has_parse_error(false)
  {
    memset(&m_subject_table_grant, 0, sizeof (m_subject_table_grant));
  }

  bool init(THD *thd,
            LEX *lex,
            LEX_STRING *trg_name,
            Stored_program_creation_ctx *trg_creation_ctx,
            const LEX_STRING *db_name,
            TABLE *table);

  bool execute(THD *thd);

  void get_info(LEX_STRING *trg_name,
                LEX_STRING *trigger_stmt,
                sql_mode_t *sql_mode,
                LEX_STRING *trg_definer,
                LEX_STRING *trg_definition,
                LEX_STRING *client_cs_name,
                LEX_STRING *connection_cl_name,
                LEX_STRING *db_cl_name) const;

  bool parse_trigger_body(THD *thd, TABLE *table);

  void setup_fields(THD *thd,
                    TABLE *table,
                    Table_trigger_dispatcher *dispatcher);

  void add_tables_and_routines(THD *thd,
                               Query_tables_list *prelocking_ctx,
                               TABLE_LIST *table_list);

  bool is_fields_updated_in_trigger(const MY_BITMAP *used_fields) const;

  void mark_field_used(TABLE *trigger_table);

public:
  bool has_parse_error() const
  {
    return m_has_parse_error;
  }

  const char* get_parse_error_message() const
  {
    return m_parse_error_message;
  }

  void set_parse_error_message(const char *error_message)
  {
    m_has_parse_error= true;
    strncpy(m_parse_error_message, error_message,
            sizeof(m_parse_error_message));
  }

public:
  LEX_STRING *get_trigger_name() const
  {
    return m_trigger_name;
  }

  void set_trigger_name(LEX_STRING *trigger_name)
  {
    m_trigger_name= trigger_name;
  }

  LEX_STRING *get_definition() const
  {
    return m_definition;
  }

  sql_mode_t *get_sql_mode()
  {
    return &m_sql_mode;
  }

  LEX_STRING *get_definer() const
  {
    return m_definer;
  }

  LEX_STRING *get_on_table_name() const
  {
    return m_on_table_name;
  }

  LEX_STRING *get_client_cs_name()
  {
    return m_client_cs_name;
  }

  LEX_STRING *get_connection_cl_name()
  {
    return m_connection_cl_name;
  }

  LEX_STRING *get_db_cl_name()
  {
    return m_db_cl_name;
  }

public:
  trg_action_time_type get_action_time() const
  {
    return m_action_time;
  }

  trg_event_type get_event() const
  {
    return m_event;
  }

  sp_head *get_sp()
  {
    return m_sp;
  }

  GRANT_INFO *get_subject_table_grant()
  {
    return &m_subject_table_grant;
  }

private:
  /// Trigger name.
  LEX_STRING *m_trigger_name;

  /// Database name.
  LEX_STRING *m_db_name;

  /// Table name.
  LEX_STRING *m_table_name;

  /**
    "ON table_name" part in trigger definition, used for
    updating trigger definition during RENAME TABLE.
  */
  LEX_STRING *m_on_table_name;

  /// Grant information for the trigger.
  GRANT_INFO m_subject_table_grant;

  /// Trigger definition to save in TRG-file.
  LEX_STRING *m_definition;

  /// Trigger sql-mode.
  sql_mode_t m_sql_mode;

  /// Trigger definer.
  LEX_STRING *m_definer;

  /*
    Character set context, used for parsing and executing trigger.
  */
  LEX_STRING *m_client_cs_name;
  LEX_STRING *m_connection_cl_name;
  LEX_STRING *m_db_cl_name;
  sp_head *m_sp;

  trg_action_time_type m_action_time;
  trg_event_type m_event;

private:
  bool m_has_parse_error;

  /*
    This error will be displayed when the user tries to manipulate or invoke
    triggers on a table that has broken triggers. It will get set only once
    per statement and thus will contain the first parse error encountered in
    the trigger file.
   */
  char m_parse_error_message[MYSQL_ERRMSG_SIZE];
};

///////////////////////////////////////////////////////////////////////////

#endif // TRIGGER_H_INCLUDED

