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

#ifndef SQL_HANDLER_INCLUDED
#define SQL_HANDLER_INCLUDED

#include "sql_class.h"                 /* enum_ha_read_mode */
#include "my_base.h"                   /* ha_rkey_function, ha_rows */
#include "sql_list.h"                  /* List */

class THD;
struct TABLE_LIST;

bool mysql_ha_open(THD *thd, TABLE_LIST *tables, bool reopen);
bool mysql_ha_close(THD *thd, TABLE_LIST *tables);
bool mysql_ha_read(THD *, TABLE_LIST *,enum enum_ha_read_modes,char *,
                   List<Item> *,enum ha_rkey_function,Item *,ha_rows,ha_rows);
void mysql_ha_flush(THD *thd);
void mysql_ha_flush_tables(THD *thd, TABLE_LIST *all_tables);
void mysql_ha_rm_tables(THD *thd, TABLE_LIST *tables);
void mysql_ha_cleanup(THD *thd);
void mysql_ha_set_explicit_lock_duration(THD *thd);

#endif /* SQL_HANDLER_INCLUDED */
