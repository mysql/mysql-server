#ifndef SQL_RESOLVER_INCLUDED
#define SQL_RESOLVER_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

class Item;
class Item_in_subselect;
class SELECT_LEX;
class THD;
struct TABLE;
struct TABLE_LIST;

struct ORDER;
template <typename Element_type> class Bounds_checked_array;

typedef Bounds_checked_array<Item*> Ref_item_array;
template <class T> class List;

/**
  @file sql/sql_resolver.h
  Name resolution functions.
*/

void propagate_nullability(List<TABLE_LIST> *tables, bool nullable);

bool setup_order(THD *thd, Ref_item_array ref_item_array, TABLE_LIST *tables,
                 List<Item> &fields, List <Item> &all_fields, ORDER *order);
bool subquery_allows_materialization(Item_in_subselect *predicate,
                                     THD *thd, SELECT_LEX *select_lex,
                                     const SELECT_LEX *outer);
bool validate_gc_assignment(List<Item> *fields,
                            List<Item> *values, TABLE *tab);

bool find_order_in_list(THD *thd, Ref_item_array ref_item_array,
                        TABLE_LIST *tables, ORDER *order,
                        List<Item> &fields, List<Item> &all_fields,
                        bool is_group_field);

#endif /* SQL_RESOLVER_INCLUDED */
