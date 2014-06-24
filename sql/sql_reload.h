#ifndef SQL_RELOAD_INCLUDED
#define SQL_RELOAD_INCLUDED
/* Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

class THD;
struct TABLE_LIST;

bool reload_acl_and_cache(THD *thd, unsigned long options,
                          TABLE_LIST *tables, int *write_to_binlog);

bool flush_tables_with_read_lock(THD *thd, TABLE_LIST *all_tables);

bool flush_tables_for_export(THD *thd, TABLE_LIST *all_tables);

#endif
