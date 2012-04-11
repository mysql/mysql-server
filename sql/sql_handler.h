/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_HANDLER_INCLUDED
#define SQL_HANDLER_INCLUDED

#include "sql_class.h"                 /* enum_ha_read_mode */
#include "my_base.h"                   /* ha_rkey_function, ha_rows */
#include "sql_list.h"                  /* List */

class THD;
struct TABLE_LIST;

/**
  Sql_cmd_handler_open represents HANDLER OPEN statement.

  @note Some information about this statement, for example, table to be
        opened is still kept in LEX class.
*/

class Sql_cmd_handler_open : public Sql_cmd
{
public:
  Sql_cmd_handler_open()
  {}

  virtual ~Sql_cmd_handler_open()
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_HA_OPEN;
  }

  virtual bool execute(THD *thd);
};


/**
  Sql_cmd_handler_read represents HANDLER READ statement.

  @note Some information about this statement, for example, table
        list element which identifies HANDLER to be read from,
        WHERE and LIMIT clauses is still kept in LEX class.
*/

class Sql_cmd_handler_read : public Sql_cmd
{
public:
  Sql_cmd_handler_read(enum_ha_read_modes read_mode,
                       const char *key_name,
                       List<Item> *key_expr,
                       ha_rkey_function rkey_mode)
    : m_read_mode(read_mode), m_key_name(key_name), m_key_expr(key_expr),
      m_rkey_mode(rkey_mode)
  {}

  virtual ~Sql_cmd_handler_read()
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_HA_READ;
  }

  virtual bool execute(THD *thd);

private:
  /** Read mode for HANDLER READ: FIRST, NEXT, LAST, ... */
  enum enum_ha_read_modes m_read_mode;

  /**
    Name of key to be used for reading,
    NULL in cases when natural row-order is to be used.
  */
  const char *m_key_name;

  /** Key values to be satisfied. */
  List<Item> *m_key_expr;

  /** Type of condition for key values to be satisfied. */
  enum ha_rkey_function m_rkey_mode;
};


/**
  Sql_cmd_handler_close represents HANDLER CLOSE statement.

  @note Table list element which identifies HANDLER to be closed
        still resides in LEX class.
*/

class Sql_cmd_handler_close : public Sql_cmd
{
public:
  Sql_cmd_handler_close()
  {}

  virtual ~Sql_cmd_handler_close()
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_HA_CLOSE;
  }

  virtual bool execute(THD *thd);
};


void mysql_ha_flush(THD *thd);
void mysql_ha_flush_tables(THD *thd, TABLE_LIST *all_tables);
void mysql_ha_rm_tables(THD *thd, TABLE_LIST *tables);
void mysql_ha_cleanup(THD *thd);
void mysql_ha_set_explicit_lock_duration(THD *thd);

typedef bool Log_func(THD*, TABLE*, bool,
                      const uchar*, const uchar*);

int  binlog_log_row(TABLE* table,
                          const uchar *before_record,
                          const uchar *after_record,
                          Log_func *log_func);

#endif /* SQL_HANDLER_INCLUDED */
