#ifndef SQL_RESOLVER_INCLUDED
#define SQL_RESOLVER_INCLUDED

/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#include <functional>

class Item;
class THD;
struct TABLE;
class Table_ref;

struct ORDER;
template <typename Element_type>
class Bounds_checked_array;

typedef Bounds_checked_array<Item *> Ref_item_array;
template <class T>
class List;

template <class T>
class mem_root_deque;

/**
  @file sql/sql_resolver.h
  Name resolution functions.
*/

void propagate_nullability(mem_root_deque<Table_ref *> *tables, bool nullable);

bool setup_order(THD *thd, Ref_item_array ref_item_array, Table_ref *tables,
                 mem_root_deque<Item *> *fields, ORDER *order);
bool validate_gc_assignment(const mem_root_deque<Item *> &fields,
                            const mem_root_deque<Item *> &values, TABLE *tab);

bool find_order_in_list(THD *thd, Ref_item_array ref_item_array,
                        Table_ref *tables, ORDER *order,
                        mem_root_deque<Item *> *fields, bool is_group_field,
                        bool is_window_order);

struct ReplaceResult {
  enum {
    // Immediately return with an error.
    ERROR,

    // Replace the given Item with the one in “replacement”;
    // do not traverse further.
    REPLACE,

    // Leave this item alone, but keep traversing into its children.
    KEEP_TRAVERSING
  } action;
  Item *replacement;  // Only for REPLACE.
};

/**
  Walk through the conditions and functions below the given item, and allows the
  given functor to replace it with new items. See ReplaceResult.

  This function is used both during resolving for making permanent changes to
  the item tree, and during optimization for making non-permanent changes.

  Note that this must not be used for permanent changes during optimization,
  as all changes done during optimization will be rolled back if a prepared
  statement is re-executed.

  Note also that if the replaced items have different data types than the
  original items, it may be necessary to adjust comparators in Item_bool_func2
  objects higher up in the tree by calling set_cmp_func() on them. This is not
  done by this function. Partly because it's generally not used for changing
  data types, except in some special cases (at the time of writing, only in
  resolving of ROLLUP columns). But also because of the note above about making
  permanent changes during optimization; set_cmp_func() may make permanent
  changes to the item, which are not rolled back at the end of execution, so
  it's not safe to do this unconditionally under optimization. If adjusting the
  comparators is necessary, the caller of WalkAndReplace() will have to invoke
  set_cmp_func() manually.

  @return true on error.
 */
bool WalkAndReplace(
    THD *thd, Item *item,
    const std::function<ReplaceResult(Item *item, Item *parent,
                                      unsigned argument_idx)> &get_new_item);

#endif /* SQL_RESOLVER_INCLUDED */
