/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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
#ifndef SQL_USER_TABLE_INCLUDED
#define SQL_USER_TABLE_INCLUDED

#include "sql_connect.h"
#include "table.h"

class THD;

extern const TABLE_FIELD_DEF mysql_db_table_def;

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
  LAST_ENTRY  /* Must always be at the end */
} ACL_TABLES;

int handle_grant_table(THD *thd, TABLE_LIST *tables, ACL_TABLES table_no, bool drop,
                       LEX_USER *user_from, LEX_USER *user_to);

#endif /* SQL_USER_TABLE_INCLUDED */
