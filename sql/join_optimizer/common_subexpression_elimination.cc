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

#include <assert.h>

#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/sql_executor.h"
#include "sql/sql_list.h"
#include "template_utils.h"

namespace {

Item *OrGroupWithSomeRemoved(Item_cond_or *or_item,
                             const List<Item> &items_to_remove);

bool IsOr(const Item *item) {
  return item->type() == Item::COND_ITEM &&
         down_cast<const Item_cond *>(item)->functype() ==
             Item_func::COND_OR_FUNC;
}

/**
  Check if “item” is necessary to make the expression true.
  This is case if “expr” is either:

   - The same as “item”, or
   - (something necessary) OR (something necessary)
   - (something necessary) AND anything

  A typical case would be of the latter would be

    (item AND x) OR (item AND y) OR (z AND w AND item)
 */
bool AlwaysPresent(Item *expr, const Item *item) {
  if (expr->eq(item, /*binary_cmp=*/true)) {
    return true;
  }

  // (something necessary) AND (anything), e.g. item AND x AND y.
  if (IsAnd(expr)) {
    for (Item &sub_item : *down_cast<Item_cond_and *>(expr)->argument_list()) {
      if (AlwaysPresent(&sub_item, item)) {
        return true;
      }
    }
    return false;
  }

  // (something necessary) OR (something necessary),
  // e.g. item AND (item OR x).
  if (IsOr(expr)) {
    for (Item &sub_item : *down_cast<Item_cond_or *>(expr)->argument_list()) {
      if (!AlwaysPresent(&sub_item, item)) {
        return false;
      }
    }
    return true;
  }

  // Something else.
  return false;
}

/// Check if “item” matches any item in “items”.
bool MatchesAny(Item *item, const List<Item> &items) {
  for (const Item &other_item : items) {
    if (item->eq(&other_item, /*binary_cmp=*/true)) {
      return true;
    }
  }
  return false;
}

/**
  For all items in an AND conjunction, add those (possibly none) that are not in
  “items_to_remove”. E.g., for a AND b AND c, and items_to_remove=(b),
  adds a and c to “output”.
 */
void ExtractItemsExceptSome(Item_cond_and *and_item,
                            const List<Item> &items_to_remove,
                            List<Item> *output) {
  for (Item &item : *and_item->argument_list()) {
    if (MatchesAny(&item, items_to_remove)) {
      continue;
    }

    if (IsAnd(&item)) {
      ExtractItemsExceptSome(down_cast<Item_cond_and *>(&item), items_to_remove,
                             output);
    } else if (IsOr(&item)) {
      Item *new_item = OrGroupWithSomeRemoved(down_cast<Item_cond_or *>(&item),
                                              items_to_remove);
      if (new_item != nullptr) {
        output->push_back(new_item);
      }
    } else {
      output->push_back(&item);
    }
  }
}

/**
  For an OR disjunction, return a new disjunction with elements from
  “items_to_remove” logically set to TRUE (ie., removed). If any of the
  AND-within-OR groups become empty, the expression is always true and nullptr
  is returned. E.g.:

    (a AND b) OR (c AND d), remove (b)   => a OR (c AND d)
    (a AND b) OR (c AND d), remove (b,c) => a OR d
    (a AND b) OR (c AND d), remove (a,b) => nullptr
 */
Item *OrGroupWithSomeRemoved(Item_cond_or *or_item,
                             const List<Item> &items_to_remove) {
  List<Item> new_args;
  for (Item &item : *or_item->argument_list()) {
    if (MatchesAny(&item, items_to_remove)) {
      // Always true.
      return nullptr;
    } else if (IsAnd(&item)) {
      List<Item> and_args;
      ExtractItemsExceptSome(down_cast<Item_cond_and *>(&item), items_to_remove,
                             &and_args);
      if (and_args.is_empty()) {
        // True.
        return nullptr;
      } else {
        new_args.push_back(CreateConjunction(&and_args));
      }
    } else if (IsOr(&item)) {
      Item *new_item = OrGroupWithSomeRemoved(down_cast<Item_cond_or *>(&item),
                                              items_to_remove);
      if (new_item == nullptr) {
        // x OR TRUE => TRUE.
        return nullptr;
      }
      new_args.push_back(new_item);
    } else {
      new_args.push_back(&item);
    }
  }

  assert(!new_args.is_empty());
  if (new_args.size() == 1) {
    // Should never really happen.
    return new_args.head();
  } else {
    Item_cond_or *item_or = new Item_cond_or(new_args);
    item_or->update_used_tables();
    item_or->quick_fix_field();
    return item_or;
  }
}

}  // namespace

Item *CommonSubexpressionElimination(Item *cond) {
  if (!IsOr(cond)) {
    // Not an OR expression, but we could have something within it.
    return cond;
  }

  Item_cond_or *or_item = down_cast<Item_cond_or *>(cond);
  if (or_item->argument_list()->is_empty()) {
    // An OR with no elements is a false condition. (Such items can be found
    // when remove_eq_conds() has removed all always false legs of the OR
    // condition.)
    return new Item_func_false;
  }

  // Find all items in the first AND of the OR group (or first Item,
  // if it's not an AND conjunction). For each of them, we check
  // if they exist in all the other ANDs as well.
  //
  // NOTE: AlwaysPresent() is doing a little bit of wasted work here,
  // since it doesn't skip the first group.
  List<Item> common_items;
  Item *first_group = or_item->argument_list()->head();
  if (IsAnd(first_group)) {
    Item_cond_and *and_group = down_cast<Item_cond_and *>(first_group);
    for (Item &and_arg : *and_group->argument_list()) {
      if (AlwaysPresent(or_item, &and_arg)) {
        common_items.push_back(&and_arg);
      }
    }
  } else {
    if (AlwaysPresent(or_item, first_group)) {
      common_items.push_back(first_group);
    }
  }

  if (common_items.is_empty()) {
    // No common items, so no CSE is possible.
    return cond;
  }

  // Add all the original OR groups at the end, but with the common items
  // removed. They may be effectively empty (equivalent to TRUE), though,
  // and in that case, we can ignore them. But we'll always have either at least
  // one common element or at least one remainder.
  Item *remainder = OrGroupWithSomeRemoved(or_item, common_items);
  if (remainder != nullptr) {
    common_items.push_back(remainder);
  }
  assert(!common_items.is_empty());
  return CreateConjunction(&common_items);
}
