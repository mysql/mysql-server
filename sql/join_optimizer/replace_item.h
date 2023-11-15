/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
    4. The item is not in items_to_copy but it is an aggregate item, so it
       *has* to have a replacement created. In such case, 'agg_items_to_copy' is
       non-null, and it indicates that a new items_to_copy list is to be saved
       into this. It is made up of all such aggregate items that were not found
       while finding replacement. These items need to be added in
       'agg_items_to_copy' so that further items get a direct match for
       subsequent occurences of these items, rather than generating a new
       replacement.  Without this, the replacement does not propagate from the
       bottom to the top plan node.

 */
Item *FindReplacementOrReplaceMaterializedItems(
    THD *thd, Item *item, const Func_ptr_array &items_to_copy,
    bool need_exact_match, Func_ptr_array *agg_items_to_copy = nullptr);

/**
  Like FindReplacementOrReplaceMaterializedItems, but only search _below_ the
  item, ie. ignore point 2 above. This can be useful if doing self-replacement,
  ie., we are replacing source items in items_to_copy and don't want to
  replace an item with its own output.
 */
void ReplaceMaterializedItems(THD *thd, Item *item,
                              const Func_ptr_array &items_to_copy,
                              bool need_exact_match,
                              bool window_frame_buffer = false);

/**
  Replace "@var:=<expr>" with "@var:=<tmp_table_column>" rather than
  "<tmp_table_column>".

  If a join field such as "@var:=expr" points to a temp table field, the
  var assignment won't happen because there is no re-evaluation of the
  materialized field. . So, rather than returning the temp table field,
  return a new Item_func_set_user_var item that points to temp table
  field, so that "@var" gets updated.

  (It's another thing that the temp table field itself is an
  Item_func_set_user_var field, i.e. of the form "@var:=<expr>", which
  means the var assignment redundantly happens for *each* temp table
  record while initializing the table; but this function does not fix
  that)

  TODO: remove this function cf. deprecated setting of variable in
  expressions when it is finally disallowed.
 */
Item *ReplaceSetVarItem(THD *thd, Item *item, Item *new_item);

#endif  // SQL_JOIN_OPTIMIZER_REPLACE_ITEM_H
