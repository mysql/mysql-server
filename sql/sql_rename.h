/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_RENAME_INCLUDED
#define SQL_RENAME_INCLUDED

class THD;
struct TABLE_LIST;

bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list, bool silent);
bool do_rename(THD *thd, TABLE_LIST *ren_table, const char *new_db,
               const char *new_table_name, const char *new_table_alias,
               bool skip_error);

#endif /* SQL_RENAME_INCLUDED */
