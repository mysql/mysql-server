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

#include "sql/join_optimizer/replace_item.h"
#include "sql/current_thd.h"
#include "sql/item.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/sql_resolver.h"
#include "sql/temp_table_param.h"

static Item *possibly_outerize_replacement(Item *sub_item, Item *replacement) {
  Query_block *dep_from = nullptr;
  switch (sub_item->type()) {
    case Item::FIELD_ITEM:
    case Item::REF_ITEM:
      dep_from = down_cast<Item_ident *>(sub_item)->depended_from;
      break;
    default:;
  }
  if (dep_from != nullptr) {
    if (replacement->real_item()->type() == Item::FIELD_ITEM) {
      Item_field *real_field =
          down_cast<Item_field *>(replacement->real_item());
      Item_field *res = new Item_field(current_thd, real_field);
      res->depended_from = dep_from;
      res->m_table_ref =
          down_cast<Item_field *>(sub_item->real_item())->m_table_ref;
      replacement = res;
    }
  }
  return replacement;
}

/**
  Check what field the given item will be materialized into under the given
  temporary table parameters.

  If the item is materialized (ie., found in items_to_copy), we return a
  canonical Item_field for that field; ie., the same every time. This means
  that you can do the same replacement in a SELECT list and then in
  items_to_copy itself, and still have them match. This is used in particular
  when updating Temp_table_param itself, in FinalizePlanForQueryBlock().

  Normally, we want to search for only the same item, up to references
  (need_exact_match=true). However, in ORDER BY specifications of windows,
  we can sometimes have the same field referred to by different Item_field,
  and the callers may need to set need_exact_match=false, which compares
  using Item::eq() instead. This also disables the behavior of checking
  and propagating Item::hidden.
 */
static Item *FindReplacementItem(Item *item,
                                 const Func_ptr_array &items_to_copy,
                                 bool need_exact_match) {
  if (item->const_for_execution()) {
    // Stop traversing (which we do with a fake replacement with ourselves).
    // This is the only case where we can return an Item that is not an
    // Item_field.
    return item;
  }

  for (const Func_ptr &func : items_to_copy) {
    bool match;
    if (need_exact_match) {
      // For nearly all cases, just comparing the items (by pointer) would
      // be sufficient, but in rare cases involving CTEs (see e.g. the test for
      // bug #26907753), we can have a ref in func.func(), so we need to call
      // real_item() before comparing.
      match = func.func()->hidden == item->hidden &&
              func.func()->real_item() == item->real_item();
    } else {
      match = func.func()->real_item()->eq(item->real_item());
    }
    if (match) {
      Item *item_field = func.result_item();
      if (item_field == nullptr) return nullptr;
      if (need_exact_match) {
        item_field->hidden = item->hidden;
      }
      return item_field;
    }
  }
  return nullptr;
}

Item *FindReplacementOrReplaceMaterializedItems(
    THD *thd, Item *item, const Func_ptr_array &items_to_copy,
    bool need_exact_match, Func_ptr_array *agg_items_to_copy) {
  Item *replacement =
      FindReplacementItem(item, items_to_copy, need_exact_match);

  if (replacement != nullptr) {
    // Replace "@:=<expr>" with "@:=<tmp_table_column>" rather than with
    // "<tmp_table_column>". (See ReplaceSetVarItem() declaration)
    // No need not do this for const items. (1)
    // Also we do not perform the special handling for tmp tables used for
    // anything other than GROUP BY. E.g. windowing. (2)
    if (item->type() == Item::FUNC_ITEM &&
        (down_cast<Item_func *>(item))->functype() ==
            Item_func::SUSERVAR_FUNC &&
        replacement != item &&  // (1)
        replacement->type() == Item::FIELD_ITEM &&
        (down_cast<Item_field *>(replacement))->field->table->group !=
            nullptr)  // (2)
      return ReplaceSetVarItem(thd, item, replacement);

    return replacement;
  }

  // If agg_items_to_copy list is passed, it means we need to generate a new
  // temp-table field for an aggregate item, and save it into the list.
  if (Field *field = item->get_tmp_table_field();
      agg_items_to_copy != nullptr && field != nullptr &&
      field->table->group != nullptr && item->type() == Item::SUM_FUNC_ITEM) {
    Item_sum *sum_item = down_cast<Item_sum *>(item);
    Item *result_item = sum_item->result_item(field);
    assert(result_item != nullptr);

    result_item->item_name = item->item_name;
    result_item->hidden = item->hidden;

    agg_items_to_copy->push_back(Func_ptr{item, field, result_item});

    return result_item;
  }

  // We don't need to care about the hidden flag when modifying the arguments
  // to an item (ie., the item itself isn't in the SELECT list). Non-exact
  // matches are important when modifying arguments within rollup group
  // wrappers, since e.g. rollup_group_item(t1.a) will create a hidden item
  // t1.a, and if we materialize t1.a -> <temporary>.a, we'll need to modify
  // the argument to the rollup group wrapper as well.
  ReplaceMaterializedItems(thd, item, items_to_copy,
                           /*need_exact_match=*/false);
  return item;
}

void ReplaceMaterializedItems(THD *thd, Item *item,
                              const Func_ptr_array &items_to_copy,
                              bool need_exact_match, bool window_frame_buffer) {
  bool modified = false;
  const auto replace_functor =
      [thd, &modified, &items_to_copy, need_exact_match, window_frame_buffer](
          Item *sub_item, Item *, unsigned) -> ReplaceResult {
    Item *replacement = FindReplacementItem(sub_item->real_item(),
                                            items_to_copy, need_exact_match);
    if (replacement != nullptr) {
      if (window_frame_buffer) {
        replacement = possibly_outerize_replacement(sub_item, replacement);
      }
      modified = true;
      // We want to avoid losing the was_null information for items having
      // such information. So for such item, create a copy of it that
      // references the replacement item rather than the original.
      if (sub_item->type() == Item::REF_ITEM) {
        if (Item_ref *ref_item = down_cast<Item_ref *>(sub_item);
            ref_item->ref_type() == Item_ref::NULL_HELPER_REF) {
          Item **ref_replacement = new (thd->mem_root)(Item *);
          *ref_replacement = replacement;
          Item_ref_null_helper *null_helper =
              down_cast<Item_ref_null_helper *>(ref_item);
          replacement = new Item_ref_null_helper(*null_helper, ref_replacement);
        }
      }
      return {ReplaceResult::REPLACE, replacement};
    } else {
      return {ReplaceResult::KEEP_TRAVERSING, nullptr};
    }
  };

  if (window_frame_buffer) {
    LEX::Splitting_window_expression s(thd->lex, true);
    WalkAndReplace(thd, item, std::move(replace_functor));
  } else {
    WalkAndReplace(thd, item, std::move(replace_functor));
  }

  // If the item was modified to reference temporary tables, we need to update
  // its used tables to account for that.
  if (modified) {
    item->update_used_tables();
  }
}

Item *ReplaceSetVarItem(THD *thd, Item *item, Item *new_item) {
  Item_func_set_user_var *suv =
      new Item_func_set_user_var(thd, (Item_func_set_user_var *)item);

  if (!suv || !new_item) return nullptr;  // Memory issue.
  mem_root_deque<Item *> list(thd->mem_root);
  if (list.push_back(new_item)) return nullptr;
  if (suv->set_arguments(&list, true)) return nullptr;
  return suv;
}
