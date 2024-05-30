/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/finalize_plan.h"

#include <assert.h>
#include <algorithm>
#include <functional>

#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_table_map.h"
#include "prealloced_array.h"
#include "sql/filesort.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/replace_item.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/mem_root_array.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_insert.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_resolver.h"
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/visible_fields.h"
#include "sql/window.h"
#include "template_utils.h"

// Convenience functions.
static bool IsMaterializePathForDeduplication(AccessPath *path) {
  return path->type == AccessPath::MATERIALIZE &&
         path->materialize().param->deduplication_reason !=
             MaterializePathParameters::NO_DEDUP;
}
static bool IsMaterializePathForDistinct(AccessPath *path) {
  return path->type == AccessPath::MATERIALIZE &&
         path->materialize().param->deduplication_reason ==
             MaterializePathParameters::DEDUP_FOR_DISTINCT;
}
static bool IsMaterializePathForGroupBy(AccessPath *path) {
  return path->type == AccessPath::MATERIALIZE &&
         path->materialize().param->deduplication_reason ==
             MaterializePathParameters::DEDUP_FOR_GROUP_BY;
}

/**
  Search for visible BIT items, and return true if found. Used specifically for
  avoiding bit-to-long type conversion of visible join fields.
 */
static bool HasVisibleBitItems(bool is_distinct,
                               mem_root_deque<Item *> *distinct_items,
                               bool is_group_by, ORDER *group) {
  if (is_distinct && std::any_of(distinct_items->cbegin(),
                                 distinct_items->cend(), [](const Item *item) {
                                   return !item->hidden &&
                                          item->data_type() == MYSQL_TYPE_BIT;
                                 })) {
    return true;
  }
  // It may happen that a GROUP BY item points to a visible join field. This
  // also will cause the join field to change its type.
  if (is_group_by) {
    for (ORDER *tmp = group; tmp; tmp = tmp->next) {
      if (!(*tmp->item)->hidden && (*tmp->item)->data_type() == MYSQL_TYPE_BIT)
        return true;
    }
  }
  return false;
}

/**
  Replaces field references in an ON DUPLICATE KEY UPDATE clause with references
  to corresponding fields in a temporary table. The changes will be rolled back
  at the end of execution and will have to be redone during optimization in the
  next execution.
 */
static void ReplaceUpdateValuesWithTempTableFields(
    Sql_cmd_insert_select *sql_cmd, Query_block *query_block,
    const mem_root_deque<Item *> &original_fields,
    const mem_root_deque<Item *> &temp_table_fields) {
  assert(CountVisibleFields(original_fields) ==
         CountVisibleFields(temp_table_fields));

  if (sql_cmd->update_value_list.empty()) return;

  auto tmp_field_it = VisibleFields(temp_table_fields).begin();
  for (Item *orig_field : VisibleFields(original_fields)) {
    Item *tmp_field = *tmp_field_it++;
    if (orig_field->type() == Item::FIELD_ITEM) {
      Item::Item_field_replacement replacement(
          down_cast<Item_field *>(orig_field)->field,
          down_cast<Item_field *>(tmp_field), query_block);
      for (Item *&orig_item : sql_cmd->update_value_list) {
        uchar *dummy;
        Item *new_item = orig_item->compile(
            &Item::visit_all_analyzer, &dummy, &Item::replace_item_field,
            pointer_cast<uchar *>(&replacement));
        if (new_item != orig_item) {
          query_block->join->thd->change_item_tree(&orig_item, new_item);
        }
      }
    }
  }
}

/**
  Collects the set of items in the item tree that satisfy the following:

  1) Neither the item itself nor any of its descendants have a reference to a
     ROLLUP expression (item->has_grouping_set_dep() evaluates to false).
  2) The item is either the root item or its parent item does not satisfy 1).

  In other words, we do not collect _every_ item without rollup in the tree.
  Instead we collect the root item of every largest possible subtree where none
  of the items in the subtree have rollup.

  @param root  The root item of the tree to search.
  @param items A collection of items. We add items that satisfy the search
               criteria to this collection.
 */
static void CollectItemsWithoutRollup(Item *root,
                                      mem_root_deque<Item *> *items) {
  CompileItem(
      root,
      [items](Item *item) {
        if (item->has_grouping_set_dep()) {
          // Skip the item and continue searching down the item tree.
          return true;
        } else {
          // Add the item and terminate the search in this branch.
          items->push_back(item);
          return false;
        }
      },
      [](Item *item) { return item; });
}

/**
  Creates a temporary table with columns matching the SELECT list of the given
  query block. (In FinalizePlanForQueryBlock(), the SELECT list of the
  query block is updated to point to the fields in the temporary table, but not
  here.)

  This function is used for materializing the query result, either as an
  intermediate step before sorting the final result if the sort requires the
  rows to come from a single table instead of a join, or as the last step if the
  SQL_BUFFER_RESULT query option has been specified. It is also used for setting
  up the output temporary table for window functions.

  NOTE: If after_aggregation = true, it is impossible to call this function
  again later with after_aggregation = false, as count_field_types() will
  remove item->has_aggregation() once called. Thus, we need to set up all
  these temporary tables in FinalizePlanForQueryBlock(), in the right order.
  'is_group_by'=true indicates that the temp table is to be created with rows
  grouped using GROUP BY items.
  'is_distinct'=true indicates that the temp table is to be created with
  distinct rows. (corresponds to SELECT DISTINCT ...)
 */
static TABLE *CreateTemporaryTableFromSelectList(
    THD *thd, Query_block *query_block, Window *window,
    Temp_table_param **temp_table_param_arg, bool after_aggregation,
    bool is_group_by = false, bool is_distinct = false) {
  JOIN *join = query_block->join;
  ORDER *group = (is_group_by ? join->group_list.order : nullptr);
  mem_root_deque<Item *> *items_to_materialize = join->fields;

  assert(!(is_group_by && is_distinct));  // Both cannot be true.

  // We always materialize the items in join->fields. In the pre-aggregation
  // case where we have rollup items in join->fields we additionally add the
  // non-rollup descendants of rollup items to the list of items to materialize.
  // We need to do this because rollup items are removed from items_to_copy in
  // the temporary table and the replacement logic depends on base fields being
  // included.
  if (!after_aggregation &&
      std::any_of(
          items_to_materialize->cbegin(), items_to_materialize->cend(),
          [](const Item *item) { return item->has_grouping_set_dep(); })) {
    items_to_materialize =
        new (thd->mem_root) mem_root_deque<Item *>(thd->mem_root);
    for (Item *item : *join->fields) {
      items_to_materialize->push_back(item);
      if (item->has_grouping_set_dep()) {
        CollectItemsWithoutRollup(item, items_to_materialize);
      }
    }
  }

  Temp_table_param *temp_table_param =
      new (thd->mem_root) Temp_table_param(thd->mem_root);

  // This is for setting group_parts.
  if (group != nullptr) calc_group_buffer(join, group, temp_table_param);

  // temp_table_param.bit_fields_as_long is used to work around the limitation
  // of MEMORY tables not being able to index BIT columns. But we also want
  // to retain the type definition of visible bit columns. So instead, force
  // hash as the deduplication method.
  if (HasVisibleBitItems(is_distinct, items_to_materialize, is_group_by, group))
    temp_table_param->force_hash_field_for_unique = true;

  *temp_table_param_arg = temp_table_param;
  assert(!temp_table_param->precomputed_group_by);
  assert(!temp_table_param->skip_create_table);
  temp_table_param->m_window = window;
  count_field_types(query_block, temp_table_param, *items_to_materialize,
                    /*reset_with_sum_func=*/after_aggregation,
                    /*save_sum_fields=*/after_aggregation);
  temp_table_param->hidden_field_count =
      CountHiddenFields(*items_to_materialize);

  TABLE *temp_table = create_tmp_table(
      thd, temp_table_param, *items_to_materialize, group, is_distinct,
      /*save_sum_fields=*/after_aggregation, query_block->active_options(),
      /*rows_limit=*/HA_POS_ERROR, "<temporary>");
  if (temp_table == nullptr) return nullptr;

  if (after_aggregation) {
    // Most items have been added to items_to_copy in create_tmp_field(), but
    // not non-window aggregate functions, so add them here.
    //
    // Note that MIN/MAX in the presence of an index have special semantics
    // where they are filled out elsewhere and may not have a result field,
    // so we need to skip those that don't have one.
    for (Item *item : *join->fields) {
      if (item->type() == Item::SUM_FUNC_ITEM &&
          !item->real_item()->m_is_window_function &&
          item->get_result_field() != nullptr) {
        temp_table_param->items_to_copy->push_back(
            Func_ptr{item, item->get_result_field()});
      }

      // Verify that all non-constant, non-window-related items
      // have been added to items_to_copy. (For implicitly grouped
      // queries, non-deterministic expressions that don't reference
      // any tables are also considered constant by create_tmp_table(),
      // because they are evaluated exactly once.)
      assert(
          item->const_for_execution() || item->has_wf() ||
          (query_block->is_implicitly_grouped() &&
           IsSubset(item->used_tables(), RAND_TABLE_BIT | INNER_TABLE_BIT)) ||
          std::any_of(
              temp_table_param->items_to_copy->begin(),
              temp_table_param->items_to_copy->end(),
              [item](const Func_ptr &ptr) { return ptr.func() == item; }));
    }
  } else {
    // create_tmp_table() doesn't understand that rollup group items
    // are not materializable before aggregation has run, so we simply
    // take them out of the copy, and the replacement logic will do the rest
    // (e.g. rollup_group_item(t1.x)+2 -> rollup_group_item(<temporary>.x)+2).
    // This works because the base fields are always included. The logic is
    // very similar to what happens in change_to_use_tmp_fields_except_sums().
    //
    // TODO(sgunders): Consider removing the rollup group items on the inner
    // levels, similar to what change_to_use_tmp_fields_except_sums() does.
    auto *new_end = std::remove_if(temp_table_param->items_to_copy->begin(),
                                   temp_table_param->items_to_copy->end(),
                                   [](const Func_ptr &func) {
                                     return func.func()->has_grouping_set_dep();
                                   });
    temp_table_param->items_to_copy->erase(
        new_end, temp_table_param->items_to_copy->end());
  }

  // We made a new table, so make sure it gets properly cleaned up
  // at the end of execution.
  join->temp_tables.push_back(
      JOIN::TemporaryTableToCleanup{temp_table, temp_table_param});

  return temp_table;
}

/**
  Replaces the items in the SELECT list with items that point to fields in a
  temporary table. See FinalizePlanForQueryBlock() for more information.
  Also creates a new items_to_copy list made up of aggregate items that were
  not found while finding replacement. These items need to be added in
  'applied_replacements' so that further items get a direct match for subsequent
  occurences of these items, rather than generating a new replacement.
  Without this, the replacement does not propagate from the bottom to
  the top plan node.
 */
static void ReplaceSelectListWithTempTableFields(
    THD *thd, JOIN *join, const Func_ptr_array &items_to_copy,
    Mem_root_array<const Func_ptr_array *> *applied_replacements) {
  auto fields = new (thd->mem_root) mem_root_deque<Item *>(thd->mem_root);
  Func_ptr_array *agg_items_to_copy =
      new (thd->mem_root) Func_ptr_array(thd->mem_root);

  for (Item *item : *join->fields) {
    fields->push_back(FindReplacementOrReplaceMaterializedItems(
        thd, item, items_to_copy,
        /*need_exact_match=*/true, agg_items_to_copy));
  }
  join->fields = fields;
  if (!agg_items_to_copy->empty())
    applied_replacements->push_back(agg_items_to_copy);
}

/**
  In hypergraph optimizer, slices are currently used only for temp tables
  created for GROUP BY; i.e. temp table aggregation and materialization with
  deduplication (not for DISTINCT deduplication or UNION deduplication).

  For GROUP BY, we require slices to handle subqueries in HAVING clause.

  For DISTINCT, we don't require slices. ORDER BY clause is the only clause
  that is appled after DISTINCT. And the ORDER BY expression is always added as
  a hidden select item, and the temp table always has this item as one of its
  columns.  This means that the expression is already evaluated and
  materialized in the temp table; there is no further evaluation. If it were
  not materialized, any Item refs (e.g. if the expression is a subquery) would
  have required a temp table slice for evaluation, but because it is already
  materialized, we don't require slices.

  (Note: The temp-table item replacement infrastructure doesn't support items
  inside subqueries, hence slices).
*/
static bool InitTmpTableSliceRefs(THD *thd, AccessPath *path, JOIN *join) {
  // These are the only scenarios that use temp table for GROUP BY.
  if (path->type != AccessPath::TEMPTABLE_AGGREGATE &&
      !IsMaterializePathForGroupBy(path))
    return false;

  // There can only be *one* temp table slice required, because there is only
  // *one* group-by clause in a query block.
  assert(join->ref_items[REF_SLICE_TMP1].is_null());

  // Create the tmp table slice from the updated join fields.
  if (join->alloc_ref_item_slice(thd, REF_SLICE_TMP1)) return true;
  join->assign_fields_to_slice(REF_SLICE_TMP1);

  // Create a slot for backing up a slice, and set that slot as the current
  // slice.
  if (join->alloc_ref_item_slice(thd, REF_SLICE_SAVED_BASE)) return true;
  join->copy_ref_item_slice(REF_SLICE_SAVED_BASE, REF_SLICE_ACTIVE);
  join->current_ref_item_slice = REF_SLICE_SAVED_BASE;

  return false;
}

void ReplaceOrderItemsWithTempTableFields(THD *thd, ORDER *order,
                                          const Func_ptr_array &items_to_copy) {
  for (; order != nullptr; order = order->next) {
    Item *temp_field_item = FindReplacementOrReplaceMaterializedItems(
        thd, *order->item, items_to_copy, /*need_exact_match=*/true);
    if (temp_field_item != *order->item) {
      // *order->item points into a memory area (the “base ref slice”)
      // where HAVING might expect to find items _not_ pointing into the
      // temporary table (if there is true materialization, it should run
      // before it to minimize the size of the sorted input), so in order to
      // not disturb it, we create a whole new place for the Item pointer
      // to live.
      //
      // TODO(sgunders): When we get rid of slices altogether,
      // we can skip this.
      thd->change_item_tree(pointer_cast<Item **>(&order->item),
                            pointer_cast<Item *>(new (thd->mem_root) Item *));
      thd->change_item_tree(order->item, temp_field_item);
    }
  }
}

#ifndef NDEBUG
namespace {
/// @return The tables used by the order items.
table_map GetUsedTableMap(const ORDER *order) {
  table_map tables = 0;
  while (order != nullptr) {
    tables |= (*order->item)->used_tables();
    order = order->next;
  }
  return tables;
}

/**
  Checks if the order items in a SORT access path reference any column that is
  not available to it. Specifically, it tests that all columns referenced in the
  order items belong to tables that are available from a child of "sort_path",
  without any intermediate materialization step between the child and
  "sort_path".

  Say we have an access path tree such as this:

      -> Sort
          -> Nested loop join
              -> Table scan on t1
              -> Materialize
                  -> Table scan on t2

  Here, the ordering elements in the sort node may reference columns from t1 or
  from the materialize node, but not from t2. If they reference columns from t2
  directly, it means that something is missing from the set of expressions to
  materialize from t2. Or that something has gone wrong when rewriting the
  expressions in the ordering elements to point into the temporary table.
 */
bool OrderItemsReferenceUnavailableTables(
    const AccessPath *sort_path, table_map used_tables_before_replacement) {
  bool has_temptable_aggregation = false;

  // Do not attempt this if there are temp table aggregation plans. The ORDER
  // BY (and HAVING) items sometimes rely on the ref slices and so avoid the
  // temp-table replacement. One such case is when they are of the form "ORDER
  // BY <expression using column_alias>" where column_alias is a SELECT
  // aggregate expression that does not have a corresponding temp table field.
  // In such cases, when there is no direct replacement of the
  // Item_aggregate_refs or Item_refs in the temp table fields, the replacement
  // logic does not go down into the items they refer to to replace the inner
  // fields. Instead, the ref slices take care of it: the ref items start
  // referring to the appropriate temp table slice during SORT execution. So
  // the WalkItem() logic below will traverse through the Item_ref items and
  // incorrectly find the base tables.
  WalkAccessPaths(
      const_cast<AccessPath *>(sort_path), /*join=*/nullptr,
      WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
      [&has_temptable_aggregation](AccessPath *subpath, const JOIN *) {
        if (subpath->type == AccessPath::TEMPTABLE_AGGREGATE) {
          has_temptable_aggregation = true;
          return true;
        }
        return false;
      });
  if (has_temptable_aggregation) return false;

  // Find which of the base tables referenced from the order items are
  // materialized below the sort path.
  const table_map materialized_base_tables =
      used_tables_before_replacement &
      ~GetUsedTableMap(sort_path, /*include_pruned_tables=*/true);

  if (materialized_base_tables == 0) return false;

  // Check if any of those base tables is still referenced directly, instead of
  // via the temporary table. They should not be referenced directly. Ideally,
  // we'd want to simply check (*order->item)->used_tables() for each order
  // element, but temporary tables are indistinguishable from the base table
  // with tableno() == 0 in the returned table_map (see
  // Item_field::used_tables(), which returns 1 for temporary tables). So
  // instead we walk the order items and check each contained Item_field
  // individually.
  for (const ORDER *order = sort_path->sort().order; order != nullptr;
       order = order->next) {
    if (WalkItem(*order->item, enum_walk::PREFIX,
                 [materialized_base_tables](Item *item) {
                   if (item->type() == Item::FIELD_ITEM) {
                     Item_field *item_field = down_cast<Item_field *>(item);
                     return item_field->m_table_ref != nullptr &&
                            !item_field->is_outer_reference() &&
                            Overlaps(item_field->m_table_ref->map(),
                                     materialized_base_tables);
                   }
                   return false;
                 })) {
      return true;
    }
  }

  return false;
}
}  // namespace
#endif

// If the AccessPath is an operation that copies items into a temporary
// table (MATERIALIZE, STREAM or WINDOW) within the same query block,
// returns the items it's copying (in the form of temporary table parameters).
// If not, return nullptr.
static Temp_table_param *GetItemsToCopy(AccessPath *path) {
  if (path->type == AccessPath::STREAM) {
    if (path->stream().table->pos_in_table_list != nullptr) {
      // Materializes a different query block.
      return nullptr;
    }
    return path->stream().temp_table_param;
  }
  if (path->type == AccessPath::MATERIALIZE) {
    const MaterializePathParameters *param = path->materialize().param;
    if (param->table->pos_in_table_list != nullptr) {
      // Materializes a different query block.
      return nullptr;
    }
    assert(param->m_operands.size() == 1);
    if (!param->m_operands[0].copy_items) {
      return nullptr;
    }
    return param->m_operands[0].temp_table_param;
  }
  if (path->type == AccessPath::TEMPTABLE_AGGREGATE) {
    return path->temptable_aggregate().temp_table_param;
  }
  if (path->type == AccessPath::WINDOW) {
    return path->window().temp_table_param;
  }
  return nullptr;
}

/// See FinalizePlanForQueryBlock().
static bool UpdateReferencesToMaterializedItems(
    THD *thd, Query_block *query_block, AccessPath *path,
    bool after_aggregation,
    Mem_root_array<const Func_ptr_array *> *applied_replacements) {
  JOIN *join = query_block->join;
  const mem_root_deque<Item *> *original_fields = join->fields;
  Temp_table_param *temp_table_param = GetItemsToCopy(path);
  if (temp_table_param != nullptr) {
    // Update source references in this materialization.
    for (const Func_ptr_array *earlier_replacement : *applied_replacements) {
      for (Func_ptr &func : *temp_table_param->items_to_copy) {
        func.set_func(FindReplacementOrReplaceMaterializedItems(
            thd, func.func(), *earlier_replacement,
            /*need_exact_match=*/true));
      }
    }
    applied_replacements->push_back(temp_table_param->items_to_copy);

    // Update SELECT list and IODKU references.
    ReplaceSelectListWithTempTableFields(
        thd, join, *temp_table_param->items_to_copy, applied_replacements);

    // Now that the SELECT list is updated, build tmp table slice out of it.
    if (InitTmpTableSliceRefs(thd, path, join)) return true;

    if (thd->lex->sql_command == SQLCOM_INSERT_SELECT) {
      ReplaceUpdateValuesWithTempTableFields(
          down_cast<Sql_cmd_insert_select *>(thd->lex->m_sql_cmd), query_block,
          *original_fields, *join->fields);
    }
    if (after_aggregation) {
      // Due to the use of Item_aggregate_ref, we can effectively sometimes have
      // sum_func(rollup_wrapper(rollup_wrapper(x), n), n)), and
      // ReplaceSelectListWithTempTableFields() will only be able to remove the
      // inner one. This can be problematic for buffering window functions,
      // which need to be able to load back old values for x and reevaluate
      // the expression -- but it is not able to load back the state of the
      // rollup functions, so we get inconsistency.
      //
      // Thus, unwrap the remaining layer here.
      const auto replace_functor = [](Item *sub_item, Item *,
                                      unsigned) -> ReplaceResult {
        if (is_rollup_group_wrapper(sub_item)) {
          return {ReplaceResult::REPLACE, unwrap_rollup_group(sub_item)};
        } else {
          return {ReplaceResult::KEEP_TRAVERSING, nullptr};
        }
      };
      for (Item *item : *join->fields) {
        WalkAndReplace(thd, item, replace_functor);
      }
    }
  } else if (path->type == AccessPath::SORT) {
    assert(path->sort().filesort == nullptr);

#ifndef NDEBUG
    const table_map used_tables_before_replacement =
        GetUsedTableMap(path->sort().order) & ~PSEUDO_TABLE_BITS;
#endif

    for (const Func_ptr_array *earlier_replacement : *applied_replacements) {
      ReplaceOrderItemsWithTempTableFields(thd, path->sort().order,
                                           *earlier_replacement);
    }

    assert(!OrderItemsReferenceUnavailableTables(
        path, used_tables_before_replacement));

    // Set up a Filesort object for this sort.
    path->sort().filesort = new (thd->mem_root)
        Filesort(thd, CollectTables(thd, path),
                 /*keep_buffers=*/false, path->sort().order, path->sort().limit,
                 path->sort().remove_duplicates, path->sort().force_sort_rowids,
                 path->sort().unwrap_rollup);
    join->filesorts_to_cleanup.push_back(path->sort().filesort);
    if (!path->sort().filesort->using_addon_fields()) {
      FindTablesToGetRowidFor(path);
    }
  } else if (path->type == AccessPath::FILTER) {
    // Only really relevant for in2exists filters that run after windowing, and
    // for some cases of HAVING clauses.
    for (const Func_ptr_array *earlier_replacement : *applied_replacements) {
      // Replace materialized items in the filter. If this is after aggregation,
      // the HAVING clause may be wrapped in Item_aggregate_ref, so we need to
      // see through it and don't require exact match.
      const bool need_exact_match = !after_aggregation;
      path->filter().condition = FindReplacementOrReplaceMaterializedItems(
          thd, path->filter().condition, *earlier_replacement,
          need_exact_match);
    }
  } else if (path->type == AccessPath::REMOVE_DUPLICATES) {
    Item **group_items = path->remove_duplicates().group_items;
    for (int i = 0; i < path->remove_duplicates().group_items_size; ++i) {
      for (const Func_ptr_array *earlier_replacement : *applied_replacements) {
        group_items[i] = FindReplacementOrReplaceMaterializedItems(
            thd, group_items[i], *earlier_replacement,
            /*need_exact_match=*/true);
      }
    }
  }

  return false;
}

/**
  If the given access path needs a temporary table, it instantiates
  said table (we cannot do this until we have a final access path
  list, where we know which temporary tables are created and in which order).
  For window functions, it also needs to forward this information to the
  materialization access path coming right after this window, if any,
  so it uses last_window_temp_table as a buffer to hold this.
 */
static bool DelayedCreateTemporaryTable(THD *thd, Query_block *query_block,
                                        AccessPath *path,
                                        bool after_aggregation,
                                        TABLE **last_window_temp_table,
                                        unsigned *num_windows_seen) {
  if (path->type == AccessPath::WINDOW) {
    // Create the temporary table and parameters.
    Window *window = path->window().window;
    assert(path->window().temp_table == nullptr);
    assert(path->window().temp_table_param == nullptr);
    ++*num_windows_seen;
    window->set_is_last(*num_windows_seen ==
                        query_block->join->m_windows.size());
    if ((path->window().temp_table = CreateTemporaryTableFromSelectList(
             thd, query_block, window, &path->window().temp_table_param,
             /*after_aggregation=*/true)) == nullptr)
      return true;
    path->window().temp_table_param->m_window = window;
    *last_window_temp_table = path->window().temp_table;
  } else if (path->type == AccessPath::MATERIALIZE) {
    const auto &materialized_info = path->materialize();
    if (materialized_info.param->table == nullptr) {
      // A materialization that comes directly after a window is intended to
      // materialize the output of that window, unless it is meant for
      // deduplication.
      if (*last_window_temp_table != nullptr &&
          !IsMaterializePathForDeduplication(path)) {
        materialized_info.param->table =
            materialized_info.table_path->table_scan().table =
                *last_window_temp_table;
      } else {
        // All other materializations are of the SELECT list.
        assert(materialized_info.param->m_operands.size() == 1);
        TABLE *table = CreateTemporaryTableFromSelectList(
            thd, query_block, nullptr,
            &materialized_info.param->m_operands[0].temp_table_param,
            after_aggregation, IsMaterializePathForGroupBy(path),
            IsMaterializePathForDistinct(path));
        if (table == nullptr) return true;
        materialized_info.param->table =
            materialized_info.table_path->table_scan().table = table;
      }

      EstimateMaterializeCost(thd, path);
    }
    *last_window_temp_table = nullptr;
  } else if (path->type == AccessPath::STREAM) {
    if (path->stream().table == nullptr) {
      if ((path->stream().table = CreateTemporaryTableFromSelectList(
               thd, query_block, nullptr, &path->stream().temp_table_param,
               after_aggregation)) == nullptr)
        return true;
    }
    *last_window_temp_table = nullptr;
  } else if (path->type == AccessPath::TEMPTABLE_AGGREGATE) {
    if (path->temptable_aggregate().table == nullptr) {
      TABLE *table = CreateTemporaryTableFromSelectList(
          thd, query_block, nullptr,
          &path->temptable_aggregate().temp_table_param, after_aggregation,
          /*is_group_by=*/true);
      if (table == nullptr) return true;
      path->temptable_aggregate().table =
          path->temptable_aggregate().table_path->table_scan().table = table;
    }
    *last_window_temp_table = nullptr;
  } else {
    *last_window_temp_table = nullptr;
  }
  return false;
}

/// See FinalizePlanForQueryBlock().
static void FinalizeWindowPath(
    THD *thd, Query_block *query_block,
    const mem_root_deque<Item *> &original_fields,
    const Mem_root_array<const Func_ptr_array *> &applied_replacements,
    AccessPath *path) {
  JOIN *join = query_block->join;
  Temp_table_param *temp_table_param = path->window().temp_table_param;
  Window *window = path->window().window;
  for (bool first_replacement = true;
       const Func_ptr_array *earlier_replacement : applied_replacements) {
    window->apply_temp_table(thd, *earlier_replacement, first_replacement);
    first_replacement = false;
  }
  if (path->window().needs_buffering) {
    // Create the framebuffer. Note that it could exist already
    // (with an identical structure) if we are planning twice,
    // for in2exists.
    if (window->frame_buffer() == nullptr) {
      CreateFramebufferTable(thd, *path->window().temp_table_param,
                             *query_block, original_fields, *join->fields,
                             temp_table_param->items_to_copy, window);
    }
  } else {
    for (Func_ptr &func : *temp_table_param->items_to_copy) {
      // Even without buffering, some window functions will read
      // their arguments out of the output table, so we need to apply
      // our own temporary table to them. (For cases with buffering,
      // this replacement, or a less strict version, is done in
      // CreateFramebufferTable().)
      if (func.should_copy(CFT_HAS_WF) || func.should_copy(CFT_WF)) {
        ReplaceMaterializedItems(thd, func.func(),
                                 *temp_table_param->items_to_copy,
                                 /*need_exact_match=*/true);
      }
    }
  }
  window->make_special_rows_cache(thd, path->window().temp_table);
}

static Item *AddCachesAroundConstantConditions(Item *item) {
  cache_const_expr_arg cache_arg;
  cache_const_expr_arg *analyzer_arg = &cache_arg;
  return item->compile(
      &Item::cache_const_expr_analyzer, pointer_cast<uchar **>(&analyzer_arg),
      &Item::cache_const_expr_transformer, pointer_cast<uchar *>(&cache_arg));
}

[[nodiscard]] static bool AddCachesAroundConstantConditionsInPath(
    AccessPath *path) {
  // TODO(sgunders): We could probably also add on sort and GROUP BY
  // expressions, even though most of them should have been removed by the
  // interesting order framework. The same with the SELECT list and
  // expressions used in materializations.
  switch (path->type) {
    case AccessPath::FILTER:
      path->filter().condition =
          AddCachesAroundConstantConditions(path->filter().condition);
      return path->filter().condition == nullptr;
    case AccessPath::HASH_JOIN:
      for (Item *&item :
           path->hash_join().join_predicate->expr->join_conditions) {
        item = AddCachesAroundConstantConditions(item);
        if (item == nullptr) {
          return true;
        }
      }
      return false;
    default:
      return false;
  }
}

/*
  Do the final touchups of the access path tree, once we have selected a final
  plan (ie., there are no more alternatives). There are currently two major
  tasks to do here: Account for materializations (because we cannot do it until
  we have the entire plan), and set up filesorts (because it involves
  constructing new objects, so we don't want to do it for unused candidates).
  The former also influences the latter.

  Materializations in particular are a bit tricky due to the way our item system
  works; expression evaluation cares intimately about _where_ values come from,
  not just what they are (i.e., all non-leaf Items carry references to other
  Items, and pull data only from there). Thus, whenever an Item is materialized,
  references to that Item need to be modified to instead point into the correct
  field in the temporary table. We traverse the tree bottom-up and keep track of
  which materializations are active, and modify the appropriate Item lists at
  any given point, so that they point to the right place. We currently modify:

    - The SELECT list. (There is only one, so we can update it as we go.)
    - Referenced fields for INSERT ... ON DUPLICATE KEY UPDATE (IODKU);
      also updated as we go.
    - Sort keys (e.g. for ORDER BY).
    - The HAVING clause, if the materialize node is below an aggregate node.
      (If the materialization is above aggregation, the HAVING clause has
      already accomplished its mission of filtering out the uninteresting
      results, and will not be evaluated anymore.)

  Surprisingly enough, we also need to update the materialization parameters
  themselves. Say that we first have a materialization that copies
  t1.x -> <temp1>.x. After that, we have a materialization that copies
  t1.x -> <temp2>.x. For this to work properly, we obviously need to go in
  and modify the second one so that it instead says <temp1>.x -> <temp2>.x,
  ie., the copy is done from the correct source.

  You cannot yet insert temporary tables in arbitrary places in the query;
  in particular, we do not yet handle these rewrites (although they would
  very likely be possible):

    - Group elements for aggregations (GROUP BY). Do note that
      create_tmp_table() will replace elements within aggregate functions
      if you set save_sum_funcs=false; you may also want to supplant
      this mechanism.
    - Filters (e.g. WHERE predicates); do note that partial pushdown may
      present its own challenges.
    - Join conditions.
 */
bool FinalizePlanForQueryBlock(THD *thd, Query_block *query_block) {
  assert(query_block->join->needs_finalize);
  query_block->join->needs_finalize = false;

  AccessPath *const root_path = query_block->join->root_access_path();
  assert(root_path != nullptr);
  if (root_path->type == AccessPath::EQ_REF) {
    // None of the finalization below is relevant to point selects, so just
    // return immediately.
    return false;
  }

  // If the query is offloaded to an external executor, we don't need to create
  // the internal temporary tables or filesort objects, or rewrite the Item tree
  // to point into them.
  if (!IteratorsAreNeeded(thd, root_path)) {
    return false;
  }

  Query_block *old_query_block = thd->lex->current_query_block();
  thd->lex->set_current_query_block(query_block);

  // We might have stacked multiple FILTERs on top of each other.
  // Combine these into a single FILTER:
  WalkAccessPaths(
      root_path, query_block->join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK,
      [](AccessPath *path, JOIN *join [[maybe_unused]]) {
        if (path->type == AccessPath::FILTER) {
          AccessPath *child = path->filter().child;
          if (child->type == AccessPath::FILTER &&
              child->filter().materialize_subqueries ==
                  path->filter().materialize_subqueries) {
            // Combine conditions into a single FILTER.
            Item *condition = new Item_cond_and(child->filter().condition,
                                                path->filter().condition);
            condition->quick_fix_field();
            condition->update_used_tables();
            condition->apply_is_true();
            path->filter().condition = condition;
            path->filter().child = child->filter().child;
          }
        }
        return false;
      },
      /*post_order_traversal=*/true);

  Mem_root_array<const Func_ptr_array *> applied_replacements(thd->mem_root);
  TABLE *last_window_temp_table = nullptr;
  unsigned num_windows_seen = 0;
  bool error = false;
  bool after_aggregation = false;
  WalkAccessPaths(
      root_path, query_block->join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK,
      [thd, query_block, &applied_replacements, &last_window_temp_table,
       &num_windows_seen, &error,
       &after_aggregation](AccessPath *path, JOIN *join) {
        if (error) return true;
        if (DelayedCreateTemporaryTable(
                thd, query_block, path, after_aggregation,
                &last_window_temp_table, &num_windows_seen)) {
          error = true;
          return true;
        }
        const mem_root_deque<Item *> *original_fields = join->fields;
        if (UpdateReferencesToMaterializedItems(thd, query_block, path,
                                                after_aggregation,
                                                &applied_replacements)) {
          error = true;
          return true;
        }
        if (path->type == AccessPath::WINDOW) {
          FinalizeWindowPath(thd, query_block, *original_fields,
                             applied_replacements, path);
        } else if (path->type == AccessPath::AGGREGATE ||
                   path->type == AccessPath::GROUP_INDEX_SKIP_SCAN ||
                   path->type == AccessPath::TEMPTABLE_AGGREGATE) {
          for (Cached_item &ci : join->group_fields) {
            for (const Func_ptr_array *earlier_replacement :
                 applied_replacements) {
              thd->change_item_tree(
                  ci.get_item_ptr(),
                  FindReplacementOrReplaceMaterializedItems(
                      thd, ci.get_item(), *earlier_replacement,
                      /*need_exact_match=*/true));
            }
          }

          // Set up aggregators, now that fields point into the right temporary
          // table.
          for (Item_sum **func_ptr = join->sum_funcs; *func_ptr != nullptr;
               ++func_ptr) {
            Item_sum *func = *func_ptr;
            Aggregator::Aggregator_type type =
                func->has_with_distinct() ? Aggregator::DISTINCT_AGGREGATOR
                                          : Aggregator::SIMPLE_AGGREGATOR;
            if (func->set_aggregator(type) || func->aggregator_setup(thd)) {
              error = true;
              return true;
            }
          }
          after_aggregation = true;
        }
        if (AddCachesAroundConstantConditionsInPath(path)) {
          error = true;
          return true;
        }
        return false;
      },
      /*post_order_traversal=*/true);

  if (query_block->join->push_to_engines()) return true;

  thd->lex->set_current_query_block(old_query_block);
  return error;
}
