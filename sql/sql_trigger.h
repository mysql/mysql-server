#ifndef SQL_TRIGGER_INCLUDED
#define SQL_TRIGGER_INCLUDED

/*
   Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.

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

///////////////////////////////////////////////////////////////////////////

/**
  @file

  @brief
  This file contains declarations of global public functions which are used
  directly from parser/executioner to perform basic operations on triggers
  (CREATE TRIGGER, DROP TRIGGER, ALTER TABLE, DROP TABLE, ...)
*/

///////////////////////////////////////////////////////////////////////////

#include "m_string.h"
#include "sql_cmd.h"          // Sql_cmd
#include "my_sqlcommand.h"    // SQLCOM_CREATE_TRIGGER, SQLCOM_DROP_TRIGGER

class THD;
class MDL_ticket;

struct TABLE_LIST;
struct TABLE;
class String;
///////////////////////////////////////////////////////////////////////////

bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create);

bool add_table_for_trigger(THD *thd,
                           const LEX_CSTRING &db_name,
                           const LEX_STRING &trigger_name,
                           bool continue_if_not_exist,
                           TABLE_LIST **table);

bool change_trigger_table_name(THD *thd,
                               const char *db_name,
                               const char *table_alias,
                               const char *table_name,
                               const char *new_db_name,
                               const char *new_table_name);

bool drop_all_triggers(THD *thd,
                       const char *db_name,
                       const char *table_name);

///////////////////////////////////////////////////////////////////////////


/**
  This class has common code for CREATE/DROP TRIGGER statements.
*/

class Sql_cmd_ddl_trigger_common : public Sql_cmd
{
public:

  /**
    Set a table associated with a trigger.

    @param trigger_table  a table associated with a trigger.
  */

  void set_table(TABLE_LIST *trigger_table)
  {
    m_trigger_table= trigger_table;
  }

protected:
  Sql_cmd_ddl_trigger_common()
  : m_trigger_table(nullptr)
  {}

  void restore_original_mdl_state(THD *thd, MDL_ticket *mdl_ticket) const;
  bool check_trg_priv_on_subj_table(THD *thd, TABLE_LIST *table) const;
  TABLE* open_and_lock_subj_table(THD* thd, TABLE_LIST *tables,
                                  MDL_ticket **mdl_ticket) const;
  bool cleanup_on_success(THD *thd, const char* db_name,
                          TABLE *table, const String &stmt_query) const;
  TABLE_LIST *m_trigger_table;
};


/**
  This class implements the CREATE TRIGGER statement.
*/

class Sql_cmd_create_trigger : public Sql_cmd_ddl_trigger_common
{
public:

  /**
    Return the command code for CREATE TRIGGER
  */

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CREATE_TRIGGER;
  }

  virtual bool execute(THD *thd);
};


/**
  This class implements the DROP TRIGGER statement.
*/

class Sql_cmd_drop_trigger : public Sql_cmd_ddl_trigger_common
{
public:

  /**
    Return the command code for DROP TRIGGER
  */

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_DROP_TRIGGER;
  }

  virtual bool execute(THD *thd);
};

#endif /* SQL_TRIGGER_INCLUDED */
