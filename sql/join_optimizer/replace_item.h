/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_REPLACE_ITEM_H
#define SQL_JOIN_OPTIMIZER_REPLACE_ITEM_H

class Func_ptr;
class Item;
class Item_field;
template <class T>
class Mem_root_array;
class THD;
using Func_ptr_array = Mem_root_array<Func_ptr>;

/*
  Return a new item that is to be used after materialization (as given by
  items_to_copy). There are three main cases:

    1. The item isn't touched by materialization (e.g., because it's constant,
       or because we're not ready to compute it yet).
    2. The item is directly in the items_to_copy list, so it has its own field
       in the resulting temporary table; the corresponding new Item_field
       is returned.
    3. A _part_ of the item is in the items_to_copy list; e.g. say that we
       have an item (t1.x + 1), and t1.x is materialized into <temporary>.x.
       (In particular, this happens when having expressions that contain
       aggregate functions _and_ non-aggregates.) In this case, we go in and
       modify the item in-place, so that the appropriate sub-expressions are
       replaced; in this case, to (<temporary>.x + 1). This assumes that we
       never use the same item before and after a materialization in the
       query plan!
 */
Item *FindReplacementOrReplaceMaterializedItems(
    THD *thd, Item *item, const Func_ptr_array &items_to_copy,
    bool need_exact_match);

/**
  Like FindReplacementOrReplaceMaterializedItems, but only search _below_ the
  item, ie. ignore point 2 above. This can be useful if doing self-replacement,
  ie., we are replacing source items in items_to_copy and don't want to
  replace an item with its own output.
 */
void ReplaceMaterializedItems(THD *thd, Item *item,
                              const Func_ptr_array &items_to_copy,
                              bool need_exact_match);

#endif  // SQL_JOIN_OPTIMIZER_REPLACE_ITEM_H
