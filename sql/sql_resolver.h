#ifndef SQL_RESOLVER_INCLUDED
#define SQL_RESOLVER_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

class Item;
class Item_in_subselect;
class THD;
struct TABLE;
struct TABLE_LIST;
typedef class st_select_lex SELECT_LEX;
typedef struct st_order ORDER;
template <typename Element_type> class Bounds_checked_array;
typedef Bounds_checked_array<Item*> Ref_ptr_array;
template <class T> class List;

/** @file Name resolution functions */

bool setup_order(THD *thd, Ref_ptr_array ref_pointer_array, TABLE_LIST *tables,
                 List<Item> &fields, List <Item> &all_fields, ORDER *order);
bool subquery_allows_materialization(Item_in_subselect *predicate,
                                     THD *thd, SELECT_LEX *select_lex,
                                     const SELECT_LEX *outer);
bool validate_gc_assignment(THD * thd, List<Item> *fields,
                            List<Item> *values, TABLE *tab);

#endif /* SQL_RESOLVER_INCLUDED */
