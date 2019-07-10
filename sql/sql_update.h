/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_UPDATE_INCLUDED
#define SQL_UPDATE_INCLUDED

#include "sql_class.h"                          /* enum_duplicates */

class Item;
struct TABLE_LIST;
class THD;

typedef class st_select_lex SELECT_LEX;
typedef class st_select_lex_unit SELECT_LEX_UNIT;

bool mysql_prepare_update(THD *thd, TABLE_LIST *table_list,
                          Item **conds, uint order_num, ORDER *order);
int mysql_update(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &values,Item *conds,
		 uint order_num, ORDER *order, ha_rows limit,
		 enum enum_duplicates handle_duplicates, bool ignore,
                 ha_rows *found_return, ha_rows *updated_return);
bool mysql_multi_update(THD *thd, TABLE_LIST *table_list,
                        List<Item> *fields, List<Item> *values,
                        Item *conds, ulonglong options,
                        enum enum_duplicates handle_duplicates, bool ignore,
                        SELECT_LEX_UNIT *unit, SELECT_LEX *select_lex,
                        multi_update **result);
bool records_are_comparable(const TABLE *table);
bool compare_records(const TABLE *table);

#endif /* SQL_UPDATE_INCLUDED */
