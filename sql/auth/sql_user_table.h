/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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
#ifndef SQL_USER_TABLE_INCLUDED
#define SQL_USER_TABLE_INCLUDED

#include <stdarg.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/derror.h"                 /* ER_DEFAULT */
#include "sql/log.h"                    /* error_log_printf */
#include "sql/table.h"

class THD;

/**  Enum for ACL tables */
typedef enum ACL_TABLES
{
  TABLE_USER= 0,
  TABLE_DB,
  TABLE_TABLES_PRIV,
  TABLE_COLUMNS_PRIV,
  TABLE_PROCS_PRIV,
  TABLE_PROXIES_PRIV,
  TABLE_ROLE_EDGES,
  TABLE_DEFAULT_ROLES,
  TABLE_DYNAMIC_PRIV,
  TABLE_PASSWORD_HISTORY,
  LAST_ENTRY  /* Must always be at the end */
} ACL_TABLES;


/**
  Class to validate the flawlessness of ACL table
  before performing ACL operations.
*/
class Acl_table_intact : public Table_check_intact
{
public:
  Acl_table_intact(THD *c_thd) : thd(c_thd) { has_keys= TRUE; }

  /**
    Checks whether an ACL table is intact.

    Works in conjunction with @ref mysql_acl_table_defs and
    Table_check_intact::check()

    @param table Table to check.
    @param acl_table ACL Table "id"

    @retval  false  OK
    @retval  true   There was an error.
  */
  bool check(TABLE *table, ACL_TABLES acl_table)
  {
    return Table_check_intact::check(thd, table,
                                     &(mysql_acl_table_defs[acl_table]));
  }

protected:
  void report_error(uint code, const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)))
  {
    va_list args;
    va_start(args, fmt);

    if (code == 0)
      error_log_printf(WARNING_LEVEL, fmt, args);
    else if (code == ER_CANNOT_LOAD_FROM_TABLE_V2)
    {
      char *db_name, *table_name;
      db_name= va_arg(args, char *);
      table_name= va_arg(args, char *);
      my_error(code, MYF(ME_ERRORLOG), db_name, table_name);
    }
    else
      my_printv_error(code, ER_THD(thd, code), MYF(ME_ERRORLOG), args);

    va_end(args);
  }

private:
  THD *thd;
  static const TABLE_FIELD_DEF mysql_acl_table_defs[];
};


int handle_grant_table(THD *thd, TABLE_LIST *tables, ACL_TABLES table_no, bool drop,
                       LEX_USER *user_from, LEX_USER *user_to);

#endif /* SQL_USER_TABLE_INCLUDED */
