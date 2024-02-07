/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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
#ifndef SQL_USER_TABLE_INCLUDED
#define SQL_USER_TABLE_INCLUDED

#include "mysql/my_loglevel.h"           // loglevel
#include "sql/sql_system_table_check.h"  // System_table_intact

class THD;

/**
  Enum for ACL tables.
  Keep in sync with Acl_table_names
*/
typedef enum ACL_TABLES {
  TABLE_USER = 0,
  TABLE_DB,
  TABLE_TABLES_PRIV,
  TABLE_COLUMNS_PRIV,
  TABLE_PROCS_PRIV,
  TABLE_PROXIES_PRIV,
  TABLE_ROLE_EDGES,
  TABLE_DEFAULT_ROLES,
  TABLE_DYNAMIC_PRIV,
  TABLE_PASSWORD_HISTORY,
  LAST_ENTRY /* Must always be at the end */
} ACL_TABLES;

/**
  Class to validate the flawlessness of ACL table
  before performing ACL operations.
*/
class Acl_table_intact : public System_table_intact {
 public:
  Acl_table_intact(THD *c_thd, enum loglevel log_level = ERROR_LEVEL)
      : System_table_intact(c_thd, log_level) {}

  /**
    Checks whether an ACL table is intact.

    Works in conjunction with @ref mysql_acl_table_defs and
    Table_check_intact::check()

    @param table Table to check.
    @param acl_table ACL Table "id"

    @retval  false  OK
    @retval  true   There was an error.
  */
  bool check(TABLE *table, ACL_TABLES acl_table) {
    return Table_check_intact::check(thd(), table,
                                     &(mysql_acl_table_defs[acl_table]));
  }

 private:
  static const TABLE_FIELD_DEF mysql_acl_table_defs[];
};

int handle_grant_table(THD *, Table_ref *tables, ACL_TABLES table_no, bool drop,
                       LEX_USER *user_from, LEX_USER *user_to);

#endif /* SQL_USER_TABLE_INCLUDED */
