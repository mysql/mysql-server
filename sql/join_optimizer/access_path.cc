/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/access_path.h"

#include "sql/basic_row_iterators.h"
#include "sql/bka_iterator.h"
#include "sql/composite_iterators.h"
#include "sql/filesort.h"
#include "sql/hash_join_iterator.h"
#include "sql/item_sum.h"
#include "sql/ref_row_iterators.h"
#include "sql/sorting_iterator.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "sql/timing_iterator.h"

#include <vector>

using std::vector;

AccessPath *NewSortAccessPath(THD *thd, AccessPath *child, Filesort *filesort,
                              bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::SORT;
  path->count_examined_rows = count_examined_rows;
  path->sort().child = child;
  path->sort().filesort = filesort;

  if (filesort->using_addon_fields()) {
    path->sort().tables_to_get_rowid_for = 0;
  } else {
    if (filesort->tables.size() == 1 &&
        filesort->tables[0]->pos_in_table_list == nullptr) {
      // This can happen if we sort a single temporary table
      // which is not in the table list (e.g., one that was
      // specifically created for us). Filesort has special-casing
      // to always get the row ID in this case.
      path->sort().tables_to_get_rowid_for = 0;
    } else {
      FindTablesToGetRowidFor(path);
    }
  }

  return path;
}

template <class Func>
void WalkAccessPaths(AccessPath *path, bool cross_query_blocks, Func &&func) {
  if (func(path)) {
    // Stop recursing in this branch.
    return;
  }
  switch (path->type) {
    case AccessPath::TABLE_SCAN:
    case AccessPath::INDEX_SCAN:
    case AccessPath::REF:
    case AccessPath::REF_OR_NULL:
    case AccessPath::EQ_REF:
    case AccessPath::PUSHED_JOIN_REF:
    case AccessPath::FULL_TEXT_SEARCH:
    case AccessPath::CONST_TABLE:
    case AccessPath::MRR:
    case AccessPath::FOLLOW_TAIL:
    case AccessPath::INDEX_RANGE_SCAN:
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
    case AccessPath::FAKE_SINGLE_ROW:
    case AccessPath::ZERO_ROWS:
    case AccessPath::ZERO_ROWS_AGGREGATED:
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
    case AccessPath::UNQUALIFIED_COUNT:
      // No children.
      return;
    case AccessPath::NESTED_LOOP_JOIN:
      WalkAccessPaths(path->nested_loop_join().outer, cross_query_blocks,
                      std::forward<Func &&>(func));
      WalkAccessPaths(path->nested_loop_join().inner, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
      WalkAccessPaths(path->nested_loop_semijoin_with_duplicate_removal().outer,
                      cross_query_blocks, std::forward<Func &&>(func));
      WalkAccessPaths(path->nested_loop_semijoin_with_duplicate_removal().inner,
                      cross_query_blocks, std::forward<Func &&>(func));
      break;
    case AccessPath::BKA_JOIN:
      WalkAccessPaths(path->bka_join().outer, cross_query_blocks,
                      std::forward<Func &&>(func));
      WalkAccessPaths(path->bka_join().inner, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::HASH_JOIN:
      WalkAccessPaths(path->hash_join().outer, cross_query_blocks,
                      std::forward<Func &&>(func));
      WalkAccessPaths(path->hash_join().inner, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::FILTER:
      WalkAccessPaths(path->filter().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::SORT:
      WalkAccessPaths(path->sort().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::PRECOMPUTED_AGGREGATE:
      WalkAccessPaths(path->precomputed_aggregate().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::AGGREGATE:
      WalkAccessPaths(path->aggregate().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::TEMPTABLE_AGGREGATE:
      WalkAccessPaths(path->temptable_aggregate().subquery_path,
                      cross_query_blocks, std::forward<Func &&>(func));
      WalkAccessPaths(path->temptable_aggregate().table_path,
                      cross_query_blocks, std::forward<Func &&>(func));
      break;
    case AccessPath::LIMIT_OFFSET:
      WalkAccessPaths(path->limit_offset().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::STREAM:
      if (cross_query_blocks) {
        WalkAccessPaths(path->stream().child, cross_query_blocks,
                        std::forward<Func &&>(func));
      }
      break;
    case AccessPath::MATERIALIZE:
      WalkAccessPaths(path->materialize().table_path, cross_query_blocks,
                      std::forward<Func &&>(func));
      if (cross_query_blocks) {
        for (const MaterializePathParameters::QueryBlock &query_block :
             path->materialize().param->query_blocks) {
          WalkAccessPaths(query_block.subquery_path, cross_query_blocks,
                          std::forward<Func &&>(func));
        }
      }
      break;
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
      WalkAccessPaths(path->materialize_information_schema_table().table_path,
                      cross_query_blocks, std::forward<Func &&>(func));
      break;
    case AccessPath::APPEND:
      if (cross_query_blocks) {
        for (const AppendPathParameters &child : *path->append().children) {
          WalkAccessPaths(child.path, cross_query_blocks,
                          std::forward<Func &&>(func));
        }
      }
      break;
    case AccessPath::WINDOWING:
      WalkAccessPaths(path->windowing().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::WEEDOUT:
      WalkAccessPaths(path->weedout().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::REMOVE_DUPLICATES:
      WalkAccessPaths(path->remove_duplicates().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::ALTERNATIVE:
      WalkAccessPaths(path->alternative().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
    case AccessPath::CACHE_INVALIDATOR:
      WalkAccessPaths(path->cache_invalidator().child, cross_query_blocks,
                      std::forward<Func &&>(func));
      break;
  }
}

static AccessPath *FindSingleAccessPathOfType(AccessPath *path,
                                              AccessPath::Type type) {
  AccessPath *found_path = nullptr;

  auto func = [type, &found_path](AccessPath *subpath) {
#ifdef NDEBUG
    constexpr bool fast_exit = true;
#else
    constexpr bool fast_exit = false;
#endif
    if (subpath->type == type) {
      assert(found_path == nullptr);
      found_path = subpath;
      // If not in debug mode, stop as soon as we find the first one.
      if (fast_exit) {
        return true;
      }
    }
    return false;
  };
  WalkAccessPaths(path, /*cross_query_blocks=*/false, func);
  return found_path;
}

static RowIterator *FindSingleIteratorOfType(AccessPath *path,
                                             AccessPath::Type type) {
  return FindSingleAccessPathOfType(path, type)->iterator->real_iterator();
}

table_map GetUsedTables(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::TABLE_SCAN:
      return path->table_scan().table->pos_in_table_list->map();
    case AccessPath::INDEX_SCAN:
      return path->index_scan().table->pos_in_table_list->map();
    case AccessPath::REF:
      return path->ref().table->pos_in_table_list->map();
    case AccessPath::REF_OR_NULL:
      return path->ref_or_null().table->pos_in_table_list->map();
    case AccessPath::EQ_REF:
      return path->eq_ref().table->pos_in_table_list->map();
    case AccessPath::PUSHED_JOIN_REF:
      return path->pushed_join_ref().table->pos_in_table_list->map();
    case AccessPath::FULL_TEXT_SEARCH:
      return path->full_text_search().table->pos_in_table_list->map();
    case AccessPath::CONST_TABLE:
      return path->const_table().table->pos_in_table_list->map();
    case AccessPath::MRR:
      return path->mrr().table->pos_in_table_list->map();
    case AccessPath::FOLLOW_TAIL:
      return path->follow_tail().table->pos_in_table_list->map();
    case AccessPath::INDEX_RANGE_SCAN:
      return path->index_range_scan().table->pos_in_table_list->map();
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
      return path->dynamic_index_range_scan().table->pos_in_table_list->map();
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
    case AccessPath::FAKE_SINGLE_ROW:
    case AccessPath::ZERO_ROWS:
    case AccessPath::ZERO_ROWS_AGGREGATED:
      return 0;
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
      return path->materialized_table_function()
          .table->pos_in_table_list->map();
    case AccessPath::UNQUALIFIED_COUNT:
      // Should never be below anything that needs GetUsedTables().
      assert(false);
      return 0;
    case AccessPath::NESTED_LOOP_JOIN:
      return GetUsedTables(path->nested_loop_join().outer) |
             GetUsedTables(path->nested_loop_join().inner);
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
      return GetUsedTables(
                 path->nested_loop_semijoin_with_duplicate_removal().outer) |
             GetUsedTables(
                 path->nested_loop_semijoin_with_duplicate_removal().inner);
    case AccessPath::BKA_JOIN:
      return GetUsedTables(path->bka_join().outer) |
             GetUsedTables(path->bka_join().inner);
    case AccessPath::HASH_JOIN:
      return GetUsedTables(path->hash_join().outer) |
             GetUsedTables(path->hash_join().inner);
    case AccessPath::FILTER:
      return GetUsedTables(path->filter().child);
    case AccessPath::SORT:
      return GetUsedTables(path->sort().child);
    case AccessPath::AGGREGATE:
      return GetUsedTables(path->aggregate().child);
    case AccessPath::PRECOMPUTED_AGGREGATE:
      return GetUsedTables(path->precomputed_aggregate().child);
    case AccessPath::TEMPTABLE_AGGREGATE:
      return path->temptable_aggregate().table->pos_in_table_list->map();
    case AccessPath::LIMIT_OFFSET:
      return GetUsedTables(path->limit_offset().child);
    case AccessPath::STREAM:
      return path->stream().table->pos_in_table_list->map();
    case AccessPath::MATERIALIZE:
      return GetUsedTables(path->materialize().table_path);
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
      return GetUsedTables(
          path->materialize_information_schema_table().table_path);
    case AccessPath::APPEND: {
      table_map used_tables = 0;
      for (const AppendPathParameters &child : *path->append().children) {
        used_tables |= GetUsedTables(child.path);
      }
      return used_tables;
    }
    case AccessPath::WINDOWING:
      return GetUsedTables(path->windowing().child);
    case AccessPath::WEEDOUT:
      return GetUsedTables(path->weedout().child);
    case AccessPath::REMOVE_DUPLICATES:
      return GetUsedTables(path->remove_duplicates().child);
    case AccessPath::ALTERNATIVE:
      DBUG_ASSERT(GetUsedTables(path->alternative().child) ==
                  path->alternative()
                      .table_scan_path->table_scan()
                      .table->pos_in_table_list->map());
      return path->alternative()
          .table_scan_path->table_scan()
          .table->pos_in_table_list->map();
    case AccessPath::CACHE_INVALIDATOR:
      return GetUsedTables(path->cache_invalidator().child);
  }
  assert(false);
  return 0;
}

// Mirrors QEP_TAB::pfs_batch_update(), with one addition:
// If there is more than one table, batch mode will be handled by the join
// iterators on the probe side, so joins will return false.
bool ShouldEnableBatchMode(AccessPath *path) {
  switch (path->type) {
    case AccessPath::TABLE_SCAN:
    case AccessPath::INDEX_SCAN:
    case AccessPath::REF:
    case AccessPath::REF_OR_NULL:
    case AccessPath::PUSHED_JOIN_REF:
    case AccessPath::FULL_TEXT_SEARCH:
    case AccessPath::INDEX_RANGE_SCAN:
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
      return true;
    case AccessPath::FILTER:
      if (path->filter().condition->has_subquery()) {
        return false;
      } else {
        return ShouldEnableBatchMode(path->filter().child);
      }
    case AccessPath::SORT:
      return ShouldEnableBatchMode(path->sort().child);
    case AccessPath::EQ_REF:
    case AccessPath::CONST_TABLE:
      // These can read only one row per scan, so batch mode will never be a
      // win (fall through).
    default:
      // All others, in particular joins.
      return false;
  }
}

unique_ptr_destroy_only<RowIterator> CreateIteratorFromAccessPath(
    THD *thd, AccessPath *path, JOIN *join, bool eligible_for_batch_mode) {
  unique_ptr_destroy_only<RowIterator> iterator;

  ha_rows *examined_rows = nullptr;
  if (path->count_examined_rows && join != nullptr) {
    examined_rows = &join->examined_rows;
  }

  switch (path->type) {
    case AccessPath::TABLE_SCAN: {
      iterator = NewIterator<TableScanIterator>(thd, path->table_scan().table,
                                                path->table_scan().qep_tab,
                                                examined_rows);
      break;
    }
    case AccessPath::INDEX_SCAN:
      if (path->index_scan().reverse) {
        iterator = NewIterator<IndexScanIterator<true>>(
            thd, path->index_scan().table, path->index_scan().idx,
            path->index_scan().use_order, path->index_scan().qep_tab,
            examined_rows);
      } else {
        iterator = NewIterator<IndexScanIterator<false>>(
            thd, path->index_scan().table, path->index_scan().idx,
            path->index_scan().use_order, path->index_scan().qep_tab,
            examined_rows);
      }
      break;
    case AccessPath::REF:
      if (path->ref().reverse) {
        iterator = NewIterator<RefIterator<true>>(
            thd, path->ref().table, path->ref().ref, path->ref().use_order,
            path->ref().qep_tab, examined_rows);
      } else {
        iterator = NewIterator<RefIterator<false>>(
            thd, path->ref().table, path->ref().ref, path->ref().use_order,
            path->ref().qep_tab, examined_rows);
      }
      break;
    case AccessPath::REF_OR_NULL:
      iterator = NewIterator<RefOrNullIterator>(
          thd, path->ref_or_null().table, path->ref_or_null().ref,
          path->ref_or_null().use_order, path->ref_or_null().qep_tab,
          examined_rows);
      break;
    case AccessPath::EQ_REF:
      iterator = NewIterator<EQRefIterator>(
          thd, path->eq_ref().table, path->eq_ref().ref,
          path->eq_ref().use_order, examined_rows);
      break;
    case AccessPath::PUSHED_JOIN_REF:
      iterator = NewIterator<PushedJoinRefIterator>(
          thd, path->pushed_join_ref().table, path->pushed_join_ref().ref,
          path->pushed_join_ref().use_order, path->pushed_join_ref().is_unique,
          examined_rows);
      break;
    case AccessPath::FULL_TEXT_SEARCH:
      iterator = NewIterator<FullTextSearchIterator>(
          thd, path->full_text_search().table, path->full_text_search().ref,
          path->full_text_search().use_order, examined_rows);
      break;
    case AccessPath::CONST_TABLE:
      iterator =
          NewIterator<ConstIterator>(thd, path->const_table().table,
                                     path->const_table().ref, examined_rows);
      break;
    case AccessPath::MRR: {
      const auto &bka_param = path->mrr().bka_path->bka_join();
      iterator = NewIterator<MultiRangeRowIterator>(
          thd, path->mrr().cache_idx_cond, path->mrr().table, path->mrr().ref,
          path->mrr().mrr_flags, bka_param.join_type, join,
          GetUsedTables(bka_param.outer), bka_param.store_rowids,
          bka_param.tables_to_get_rowid_for);
      break;
    }
    case AccessPath::FOLLOW_TAIL:
      iterator = NewIterator<FollowTailIterator>(thd, path->follow_tail().table,
                                                 path->follow_tail().qep_tab,
                                                 examined_rows);
      break;
    case AccessPath::INDEX_RANGE_SCAN:
      iterator = NewIterator<IndexRangeScanIterator>(
          thd, path->index_range_scan().table, path->index_range_scan().quick,
          path->index_range_scan().qep_tab, examined_rows);
      break;
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
      iterator = NewIterator<DynamicRangeIterator>(
          thd, path->dynamic_index_range_scan().table,
          path->dynamic_index_range_scan().qep_tab, examined_rows);
      break;
    case AccessPath::TABLE_VALUE_CONSTRUCTOR: {
      assert(join != nullptr);
      SELECT_LEX *select_lex = join->select_lex;
      iterator = NewIterator<TableValueConstructorIterator>(
          thd, examined_rows, *select_lex->row_value_list,
          select_lex->join->fields);
      break;
    }
    case AccessPath::FAKE_SINGLE_ROW:
      iterator = NewIterator<FakeSingleRowIterator>(thd, examined_rows);
      break;
    case AccessPath::ZERO_ROWS: {
      unique_ptr_destroy_only<RowIterator> child =
          path->zero_rows().child != nullptr
              ? CreateIteratorFromAccessPath(thd, path->zero_rows().child, join,
                                             /*eligible_for_batch_mode=*/false)
              : nullptr;
      iterator = NewIterator<ZeroRowsIterator>(thd, move(child));
      break;
    }
    case AccessPath::ZERO_ROWS_AGGREGATED:
      iterator =
          NewIterator<ZeroRowsAggregatedIterator>(thd, join, examined_rows);
      break;
    case AccessPath::MATERIALIZED_TABLE_FUNCTION: {
      unique_ptr_destroy_only<RowIterator> table_iterator =
          CreateIteratorFromAccessPath(
              thd, path->materialized_table_function().table_path, join,
              eligible_for_batch_mode);
      iterator = NewIterator<MaterializedTableFunctionIterator>(
          thd, path->materialized_table_function().table_function,
          path->materialized_table_function().table, move(table_iterator));
      break;
    }
    case AccessPath::UNQUALIFIED_COUNT:
      iterator = NewIterator<UnqualifiedCountIterator>(thd, join);
      break;
    case AccessPath::NESTED_LOOP_JOIN: {
      unique_ptr_destroy_only<RowIterator> outer =
          CreateIteratorFromAccessPath(thd, path->nested_loop_join().outer,
                                       join, /*eligible_for_batch_mode=*/false);
      unique_ptr_destroy_only<RowIterator> inner = CreateIteratorFromAccessPath(
          thd, path->nested_loop_join().inner, join, eligible_for_batch_mode);
      iterator = NewIterator<NestedLoopIterator>(
          thd, move(outer), move(inner), path->nested_loop_join().join_type,
          path->nested_loop_join().pfs_batch_mode);
      break;
    }
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL: {
      const auto &param = path->nested_loop_semijoin_with_duplicate_removal();
      unique_ptr_destroy_only<RowIterator> outer = CreateIteratorFromAccessPath(
          thd, param.outer, join, /*eligible_for_batch_mode=*/false);
      unique_ptr_destroy_only<RowIterator> inner = CreateIteratorFromAccessPath(
          thd, param.inner, join, eligible_for_batch_mode);
      iterator = NewIterator<NestedLoopSemiJoinWithDuplicateRemovalIterator>(
          thd, move(outer), move(inner), param.table, param.key, param.key_len);
      break;
    }
    case AccessPath::BKA_JOIN: {
      AccessPath *mrr_path =
          FindSingleAccessPathOfType(path->bka_join().inner, AccessPath::MRR);
      mrr_path->mrr().bka_path = path;

      unique_ptr_destroy_only<RowIterator> outer = CreateIteratorFromAccessPath(
          thd, path->bka_join().outer, join, /*eligible_for_batch_mode=*/false);
      unique_ptr_destroy_only<RowIterator> inner = CreateIteratorFromAccessPath(
          thd, path->bka_join().inner, join, /*eligible_for_batch_mode=*/false);
      MultiRangeRowIterator *mrr_iterator = down_cast<MultiRangeRowIterator *>(
          mrr_path->iterator->real_iterator());
      iterator = NewIterator<BKAIterator>(
          thd, join, move(outer), GetUsedTables(path->bka_join().outer),
          move(inner), thd->variables.join_buff_size,
          path->bka_join().mrr_length_per_rec, path->bka_join().rec_per_key,
          path->bka_join().store_rowids,
          path->bka_join().tables_to_get_rowid_for, mrr_iterator,
          path->bka_join().join_type);
      break;
    }
    case AccessPath::HASH_JOIN: {
      const JoinPredicate *join_predicate = path->hash_join().join_predicate;
      unique_ptr_destroy_only<RowIterator> outer = CreateIteratorFromAccessPath(
          thd, path->hash_join().outer, join, eligible_for_batch_mode);
      unique_ptr_destroy_only<RowIterator> inner = CreateIteratorFromAccessPath(
          thd, path->hash_join().inner, join, /*eligible_for_batch_mode=*/true);
      vector<HashJoinCondition> conditions;
      for (Item_func_eq *cond : join_predicate->equijoin_conditions) {
        conditions.emplace_back(HashJoinCondition(cond, thd->mem_root));
      }
      const bool probe_input_batch_mode =
          eligible_for_batch_mode &&
          ShouldEnableBatchMode(path->hash_join().outer);
      double estimated_build_rows = path->hash_join().inner->num_output_rows;
      if (path->hash_join().inner->num_output_rows < 0.0) {
        // Not all access paths may propagate their costs properly.
        // Choose a fairly safe estimate (it's better to be too large
        // than too small).
        estimated_build_rows = 1048576.0;
      }
      iterator = NewIterator<HashJoinIterator>(
          thd, move(inner), GetUsedTables(path->hash_join().inner),
          estimated_build_rows, move(outer),
          GetUsedTables(path->hash_join().outer),
          path->hash_join().store_rowids,
          path->hash_join().tables_to_get_rowid_for,
          thd->variables.join_buff_size, move(conditions),
          path->hash_join().allow_spill_to_disk, join_predicate->type, join,
          join_predicate->join_conditions, probe_input_batch_mode);
      break;
    }
    case AccessPath::FILTER: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->filter().child, join, eligible_for_batch_mode);
      iterator = NewIterator<FilterIterator>(thd, move(child),
                                             path->filter().condition);
      break;
    }
    case AccessPath::SORT: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->sort().child, join, /*eligible_for_batch_mode=*/true);
      ha_rows num_rows_estimate =
          path->sort().child->num_output_rows < 0.0
              ? HA_POS_ERROR
              : lrint(path->sort().child->num_output_rows);
      Filesort *filesort = path->sort().filesort;
      iterator = NewIterator<SortingIterator>(
          thd, filesort, move(child), num_rows_estimate,
          path->sort().tables_to_get_rowid_for, examined_rows);
      if (filesort->m_remove_duplicates) {
        filesort->tables[0]->duplicate_removal_iterator =
            down_cast<SortingIterator *>(iterator->real_iterator());
      } else {
        filesort->tables[0]->sorting_iterator =
            down_cast<SortingIterator *>(iterator->real_iterator());
      }
      break;
    }
    case AccessPath::PRECOMPUTED_AGGREGATE: {
      unique_ptr_destroy_only<RowIterator> child =
          CreateIteratorFromAccessPath(thd, path->precomputed_aggregate().child,
                                       join, eligible_for_batch_mode);
      iterator = NewIterator<PrecomputedAggregateIterator>(
          thd, move(child), join,
          path->precomputed_aggregate().temp_table_param,
          path->precomputed_aggregate().output_slice);
      break;
    }
    case AccessPath::AGGREGATE: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->aggregate().child, join, eligible_for_batch_mode);
      iterator = NewIterator<AggregateIterator>(
          thd, move(child), join, path->aggregate().temp_table_param,
          path->aggregate().output_slice, path->aggregate().rollup);
      break;
    }
    case AccessPath::TEMPTABLE_AGGREGATE: {
      unique_ptr_destroy_only<RowIterator> subquery_iterator =
          CreateIteratorFromAccessPath(
              thd, path->temptable_aggregate().subquery_path, join,
              /*eligible_for_batch_mode=*/true);
      unique_ptr_destroy_only<RowIterator> table_iterator =
          CreateIteratorFromAccessPath(thd,
                                       path->temptable_aggregate().table_path,
                                       join, eligible_for_batch_mode);
      iterator = NewIterator<TemptableAggregateIterator>(
          thd, move(subquery_iterator),
          path->temptable_aggregate().temp_table_param,
          path->temptable_aggregate().table, move(table_iterator), join,
          path->temptable_aggregate().ref_slice);
      break;
    }
    case AccessPath::LIMIT_OFFSET: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->limit_offset().child, join, eligible_for_batch_mode);
      ha_rows *send_records = nullptr;
      if (path->limit_offset().send_records_override != nullptr) {
        send_records = path->limit_offset().send_records_override;
      } else if (join != nullptr) {
        send_records = &join->send_records;
      }
      iterator = NewIterator<LimitOffsetIterator>(
          thd, move(child), path->limit_offset().limit,
          path->limit_offset().offset, path->limit_offset().count_all_rows,
          path->limit_offset().reject_multiple_rows, send_records);
      break;
    }
    case AccessPath::STREAM: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->stream().child, join, eligible_for_batch_mode);
      iterator = NewIterator<StreamingIterator>(
          thd, move(child), path->stream().temp_table_param,
          path->stream().table,
          path->stream().copy_fields_and_items_in_materialize,
          path->stream().provide_rowid);
      break;
    }
    case AccessPath::MATERIALIZE: {
      unique_ptr_destroy_only<RowIterator> table_iterator =
          CreateIteratorFromAccessPath(thd, path->materialize().table_path,
                                       join, eligible_for_batch_mode);
      MaterializePathParameters *param = path->materialize().param;

      Mem_root_array<MaterializeIterator::QueryBlock> query_blocks(
          thd->mem_root, param->query_blocks.size());
      for (size_t i = 0; i < param->query_blocks.size(); ++i) {
        const MaterializePathParameters::QueryBlock &from =
            param->query_blocks[i];
        MaterializeIterator::QueryBlock &to = query_blocks[i];
        to.subquery_iterator =
            CreateIteratorFromAccessPath(thd, from.subquery_path, from.join,
                                         /*eligible_for_batch_mode=*/true);
        to.select_number = from.select_number;
        to.join = from.join;
        to.disable_deduplication_by_hash_field =
            from.disable_deduplication_by_hash_field;
        to.copy_fields_and_items = from.copy_fields_and_items;
        to.temp_table_param = from.temp_table_param;
        to.is_recursive_reference = from.is_recursive_reference;

        if (to.is_recursive_reference) {
          // Find the recursive reference to ourselves; there should be
          // exactly one, as per the standard.
          RowIterator *recursive_reader = FindSingleIteratorOfType(
              from.subquery_path, AccessPath::FOLLOW_TAIL);
          if (recursive_reader == nullptr) {
            // The recursive reference was optimized away, e.g. due to an
            // impossible WHERE condition, so we're not a recursive
            // reference after all.
            to.is_recursive_reference = false;
          } else {
            to.recursive_reader =
                down_cast<FollowTailIterator *>(recursive_reader);
          }
        }
      }
      JOIN *subjoin = param->ref_slice == -1 ? nullptr : query_blocks[0].join;
      iterator = NewIterator<MaterializeIterator>(
          thd, std::move(query_blocks), param->table, move(table_iterator),
          param->cte, param->unit, subjoin, param->ref_slice,
          param->rematerialize, param->limit_rows, param->reject_multiple_rows);

      if (param->invalidators != nullptr) {
        MaterializeIterator *materialize =
            down_cast<MaterializeIterator *>(iterator->real_iterator());
        for (const AccessPath *invalidator_path : *param->invalidators) {
          // We create iterators left-to-right, so we should have created the
          // invalidators before this.
          assert(invalidator_path->iterator != nullptr);

          materialize->AddInvalidator(down_cast<CacheInvalidatorIterator *>(
              invalidator_path->iterator->real_iterator()));
        }
      }

      break;
    }
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE: {
      unique_ptr_destroy_only<RowIterator> table_iterator =
          CreateIteratorFromAccessPath(
              thd, path->materialize_information_schema_table().table_path,
              join, eligible_for_batch_mode);
      iterator = NewIterator<MaterializeInformationSchemaTableIterator>(
          thd, move(table_iterator),
          path->materialize_information_schema_table().table_list,
          path->materialize_information_schema_table().condition);
      break;
    }
    case AccessPath::APPEND: {
      vector<unique_ptr_destroy_only<RowIterator>> children;
      children.reserve(path->append().children->size());
      for (const AppendPathParameters &child : *path->append().children) {
        children.push_back(CreateIteratorFromAccessPath(
            thd, child.path, child.join, /*eligible_for_batch_mode=*/true));
      }
      iterator = NewIterator<AppendIterator>(thd, move(children));
      break;
    }
    case AccessPath::WINDOWING: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->windowing().child, join, eligible_for_batch_mode);
      if (path->windowing().needs_buffering) {
        iterator = NewIterator<BufferingWindowingIterator>(
            thd, move(child), path->windowing().temp_table_param, join,
            path->windowing().ref_slice);
      } else {
        iterator = NewIterator<WindowingIterator>(
            thd, move(child), path->windowing().temp_table_param, join,
            path->windowing().ref_slice);
      }
      break;
    }
    case AccessPath::WEEDOUT: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->weedout().child, join, eligible_for_batch_mode);
      iterator = NewIterator<WeedoutIterator>(
          thd, move(child), path->weedout().weedout_table,
          path->weedout().tables_to_get_rowid_for);
      break;
    }
    case AccessPath::REMOVE_DUPLICATES: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->remove_duplicates().child, join, eligible_for_batch_mode);
      iterator = NewIterator<RemoveDuplicatesIterator>(
          thd, move(child), path->remove_duplicates().table,
          path->remove_duplicates().key,
          path->remove_duplicates().loosescan_key_len);
      break;
    }
    case AccessPath::ALTERNATIVE: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->alternative().child, join, eligible_for_batch_mode);
      unique_ptr_destroy_only<RowIterator> table_scan_iterator =
          CreateIteratorFromAccessPath(thd, path->alternative().table_scan_path,
                                       join, eligible_for_batch_mode);
      iterator = NewIterator<AlternativeIterator>(
          thd, path->alternative().table_scan_path->table_scan().table,
          move(child), move(table_scan_iterator), path->alternative().used_ref);
      break;
    }
    case AccessPath::CACHE_INVALIDATOR: {
      unique_ptr_destroy_only<RowIterator> child = CreateIteratorFromAccessPath(
          thd, path->cache_invalidator().child, join, eligible_for_batch_mode);
      iterator = NewIterator<CacheInvalidatorIterator>(
          thd, move(child), path->cache_invalidator().name);
      break;
    }
  }

  path->iterator = iterator.get();
  return iterator;
}

void FindTablesToGetRowidFor(AccessPath *path) {
  table_map handled_by_others = 0;

  auto add_tables_handled_by_others = [path, &handled_by_others](
                                          AccessPath *subpath) {
    if (path == subpath) return false;  // Skip ourselves.
    switch (subpath->type) {
      case AccessPath::HASH_JOIN:
        handled_by_others |= GetUsedTables(subpath);
        FindTablesToGetRowidFor(subpath);
        return true;  // Don't double-traverse.
      case AccessPath::BKA_JOIN:
        handled_by_others |= GetUsedTables(subpath->bka_join().outer);
        FindTablesToGetRowidFor(subpath);
        return true;  // Don't double-traverse.
      case AccessPath::STREAM:
        subpath->stream().provide_rowid = true;
        handled_by_others |= subpath->stream().table->pos_in_table_list->map();
        // Doesn't really matter, we don't cross query blocks anyway.
        return true;
      default:
        return false;
    }
  };

  switch (path->type) {
    case AccessPath::HASH_JOIN:
      WalkAccessPaths(path, /*cross_query_blocks=*/false,
                      add_tables_handled_by_others);
      path->hash_join().store_rowids = true;
      path->hash_join().tables_to_get_rowid_for =
          GetUsedTables(path) & ~handled_by_others;
      break;
    case AccessPath::BKA_JOIN:
      WalkAccessPaths(path->bka_join().outer, /*cross_query_blocks=*/false,
                      add_tables_handled_by_others);
      path->bka_join().store_rowids = true;
      path->bka_join().tables_to_get_rowid_for =
          GetUsedTables(path->bka_join().outer) & ~handled_by_others;
      break;
    case AccessPath::WEEDOUT:
      WalkAccessPaths(path, /*cross_query_blocks=*/false,
                      add_tables_handled_by_others);
      path->weedout().tables_to_get_rowid_for =
          GetUsedTables(path) & ~handled_by_others;
      break;
    case AccessPath::SORT:
      WalkAccessPaths(path, /*cross_query_blocks=*/false,
                      add_tables_handled_by_others);
      path->sort().tables_to_get_rowid_for =
          GetUsedTables(path) & ~handled_by_others;
      break;
    default:
      abort();
  }
}
