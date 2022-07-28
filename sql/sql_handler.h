/* Copyright (c) 2006, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_HANDLER_INCLUDED
#define SQL_HANDLER_INCLUDED

#include "my_base.h" /* ha_rkey_function, ha_rows */
#include "my_sqlcommand.h"
#include "sql/sql_cmd.h"  // Sql_cmd

class Item;
class THD;
struct TABLE_LIST;

enum class enum_ha_read_modes { RFIRST, RNEXT, RPREV, RLAST, RKEY, RNEXT_SAME };
template <class T>
class mem_root_deque;

/**
  Sql_cmd_handler_open represents HANDLER OPEN statement.

  @note Some information about this statement, for example, table to be
        opened is still kept in LEX class.
*/

class Sql_cmd_handler_open : public Sql_cmd {
 public:
  Sql_cmd_handler_open() = default;

  ~Sql_cmd_handler_open() override = default;

  enum_sql_command sql_command_code() const override { return SQLCOM_HA_OPEN; }

  bool execute(THD *thd) override;
};

/**
  Sql_cmd_handler_read represents HANDLER READ statement.

  @note Some information about this statement, for example, table
        list element which identifies HANDLER to be read from,
        WHERE and LIMIT clauses is still kept in LEX class.
*/

class Sql_cmd_handler_read : public Sql_cmd {
 public:
  Sql_cmd_handler_read(enum_ha_read_modes read_mode, const char *key_name,
                       mem_root_deque<Item *> *key_expr,
                       ha_rkey_function rkey_mode)
      : m_read_mode(read_mode),
        m_key_name(key_name),
        m_key_expr(key_expr),
        m_rkey_mode(rkey_mode) {}

  ~Sql_cmd_handler_read() override = default;

  enum_sql_command sql_command_code() const override { return SQLCOM_HA_READ; }

  bool execute(THD *thd) override;

 private:
  /** Read mode for HANDLER READ: FIRST, NEXT, LAST, ... */
  enum_ha_read_modes m_read_mode;

  /**
    Name of key to be used for reading,
    NULL in cases when natural row-order is to be used.
  */
  const char *m_key_name;

  /** Key values to be satisfied. */
  mem_root_deque<Item *> *m_key_expr;

  /** Type of condition for key values to be satisfied. */
  enum ha_rkey_function m_rkey_mode;
};

/**
  Sql_cmd_handler_close represents HANDLER CLOSE statement.

  @note Table list element which identifies HANDLER to be closed
        still resides in LEX class.
*/

class Sql_cmd_handler_close : public Sql_cmd {
 public:
  Sql_cmd_handler_close() = default;

  ~Sql_cmd_handler_close() override = default;

  enum_sql_command sql_command_code() const override { return SQLCOM_HA_CLOSE; }

  bool execute(THD *thd) override;
};

void mysql_ha_flush(THD *thd);
void mysql_ha_flush_tables(THD *thd, TABLE_LIST *all_tables);
void mysql_ha_flush_table(THD *thd, const char *db_name,
                          const char *table_name);
void mysql_ha_rm_tables(THD *thd, TABLE_LIST *tables);
void mysql_ha_rm_temporary_tables(THD *thd);
void mysql_ha_cleanup(THD *thd);
void mysql_ha_set_explicit_lock_duration(THD *thd);

#endif /* SQL_HANDLER_INCLUDED */
