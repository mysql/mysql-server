/* Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_INSERT_INCLUDED
#define SQL_INSERT_INCLUDED

#include "sql_class.h"                          /* enum_duplicates */
#include "sql_list.h"

/* Instead of including sql_lex.h we add this typedef here */
typedef List<Item> List_item;

bool mysql_prepare_insert(THD *thd, TABLE_LIST *table_list, TABLE *table,
                          List<Item> &fields, List_item *values,
                          List<Item> &update_fields,
                          List<Item> &update_values, enum_duplicates duplic,
                          Item **where, bool select_insert,
                          bool check_fields, bool abort_on_warning);
bool mysql_insert(THD *thd,TABLE_LIST *table,List<Item> &fields,
                  List<List_item> &values, List<Item> &update_fields,
                  List<Item> &update_values, enum_duplicates flag,
                  bool ignore);
void upgrade_lock_type_for_insert(THD *thd, thr_lock_type *lock_type,
                                  enum_duplicates duplic,
                                  bool is_multi_insert);
int check_that_all_fields_are_given_values(THD *thd, TABLE *entry,
                                           TABLE_LIST *table_list);
void prepare_triggers_for_insert_stmt(TABLE *table);
int write_record(THD *thd, TABLE *table,
                 COPY_INFO *info, COPY_INFO *update);
void kill_delayed_threads(void);
bool validate_default_values_of_unset_fields(THD *thd, TABLE *table);

#ifdef EMBEDDED_LIBRARY
inline void kill_delayed_threads(void) {}
#endif

#endif /* SQL_INSERT_INCLUDED */
