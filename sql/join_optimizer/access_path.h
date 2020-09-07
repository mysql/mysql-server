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

#ifndef SQL_JOIN_OPTIMIZER_ACCESS_PATH_H
#define SQL_JOIN_OPTIMIZER_ACCESS_PATH_H

#include <assert.h>
#include <stdint.h>

#include <string>
#include <type_traits>
#include <vector>

#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/mem_root_array.h"
#include "sql/sql_class.h"

class Common_table_expr;
class Filesort;
class Item;
class Item_func_eq;
class JOIN;
class KEY;
class RowIterator;
class QEP_TAB;
class QUICK_SELECT_I;
class SJ_TMP_TABLE;
class Table_function;
class Temp_table_param;
struct AccessPath;
struct ORDER;
struct POSITION;
struct TABLE;
struct TABLE_REF;
enum class JoinType;

struct JoinPredicate {
  explicit JoinPredicate(THD *thd, JoinType type)
      : type(type),
        join_conditions(thd->mem_root),
        equijoin_conditions(thd->mem_root) {}

  JoinType type;

  // Does not include the ones that are in equijoin_conditions.
  Mem_root_array<Item *> join_conditions;
  Mem_root_array<Item_func_eq *> equijoin_conditions;
};

struct AppendPathParameters {
  AccessPath *path;
  JOIN *join;
};

/**
  Access paths are a query planning structure that correspond 1:1 to iterators,
  in that an access path contains pretty much exactly the information
  needed to instantiate given iterator, plus some information that is only
  needed during planning, such as costs. (The new join optimizer will extend
  this somewhat in the future. Some iterators also need the query block,
  ie., JOIN object, they are part of, but that is implicitly available when
  constructing the tree.)

  AccessPath objects build on a variant, ie., they can hold an access path of
  any type (table scan, filter, hash join, sort, etc.), although only one at the
  same time. Currently, they contain 32 bytes of base information that is common
  to any access path (type identifier, costs, etc.), and then up to 40 bytes
  that is type-specific (e.g. for a table scan, the TABLE object). It would be
  nice if we could squeeze it down to 64 and fit a cache line exactly, but it
  does not seem to be easy without fairly large contortions.

  We could have solved this by inheritance, but the fixed-size design makes it
  possible to replace an access path when a better one is found, without
  introducing a new allocation, which will be important when using them as a
  planning structure.
 */
struct AccessPath {
  enum Type {
    // Basic access paths (those with no children, at least nominally).
    TABLE_SCAN,
    INDEX_SCAN,
    REF,
    REF_OR_NULL,
    EQ_REF,
    PUSHED_JOIN_REF,
    FULL_TEXT_SEARCH,
    CONST_TABLE,
    MRR,
    FOLLOW_TAIL,
    INDEX_RANGE_SCAN,
    DYNAMIC_INDEX_RANGE_SCAN,

    // Basic access paths that don't correspond to a specific table.
    TABLE_VALUE_CONSTRUCTOR,
    FAKE_SINGLE_ROW,
    ZERO_ROWS,
    ZERO_ROWS_AGGREGATED,
    MATERIALIZED_TABLE_FUNCTION,
    UNQUALIFIED_COUNT,

    // Joins.
    NESTED_LOOP_JOIN,
    NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL,
    BKA_JOIN,
    HASH_JOIN,

    // Composite access paths.
    FILTER,
    SORT,
    AGGREGATE,
    TEMPTABLE_AGGREGATE,
    LIMIT_OFFSET,
    STREAM,
    MATERIALIZE,
    MATERIALIZE_INFORMATION_SCHEMA_TABLE,
    APPEND,
    WINDOWING,
    WEEDOUT,
    REMOVE_DUPLICATES,
    ALTERNATIVE,
    CACHE_INVALIDATOR
  } type;

  /// Whether this access path counts as one that scans a base table,
  /// and thus should be counted towards examined_rows. It can sometimes
  /// seem a bit arbitrary which iterators count towards examined_rows
  /// and which ones do not, so the only canonical reference is the tests.
  bool count_examined_rows = false;

  /// If an iterator has been instantiated for this access path, points to the
  /// iterator. Used for constructing iterators that need to talk to each other
  /// (e.g. for recursive CTEs, or BKA join), and also for locating timing
  /// information in EXPLAIN ANALYZE queries.
  RowIterator *iterator = nullptr;

  /// Expected number of output rows, -1.0 for unknown.
  double num_output_rows{-1.0};

  /// Expected cost to read all of this access path once; -1.0 for unknown.
  double cost{-1.0};

  // Accessors for the union below.
  auto &table_scan() {
    assert(type == TABLE_SCAN);
    return u.table_scan;
  }
  const auto &table_scan() const {
    assert(type == TABLE_SCAN);
    return u.table_scan;
  }
  auto &index_scan() {
    assert(type == INDEX_SCAN);
    return u.index_scan;
  }
  const auto &index_scan() const {
    assert(type == INDEX_SCAN);
    return u.index_scan;
  }
  auto &ref() {
    assert(type == REF);
    return u.ref;
  }
  const auto &ref() const {
    assert(type == REF);
    return u.ref;
  }
  auto &ref_or_null() {
    assert(type == REF_OR_NULL);
    return u.ref_or_null;
  }
  const auto &ref_or_null() const {
    assert(type == REF_OR_NULL);
    return u.ref_or_null;
  }
  auto &eq_ref() {
    assert(type == EQ_REF);
    return u.eq_ref;
  }
  const auto &eq_ref() const {
    assert(type == EQ_REF);
    return u.eq_ref;
  }
  auto &pushed_join_ref() {
    assert(type == PUSHED_JOIN_REF);
    return u.pushed_join_ref;
  }
  const auto &pushed_join_ref() const {
    assert(type == PUSHED_JOIN_REF);
    return u.pushed_join_ref;
  }
  auto &full_text_search() {
    assert(type == FULL_TEXT_SEARCH);
    return u.full_text_search;
  }
  const auto &full_text_search() const {
    assert(type == FULL_TEXT_SEARCH);
    return u.full_text_search;
  }
  auto &const_table() {
    assert(type == CONST_TABLE);
    return u.const_table;
  }
  const auto &const_table() const {
    assert(type == CONST_TABLE);
    return u.const_table;
  }
  auto &mrr() {
    assert(type == MRR);
    return u.mrr;
  }
  const auto &mrr() const {
    assert(type == MRR);
    return u.mrr;
  }
  auto &follow_tail() {
    assert(type == FOLLOW_TAIL);
    return u.follow_tail;
  }
  const auto &follow_tail() const {
    assert(type == FOLLOW_TAIL);
    return u.follow_tail;
  }
  auto &index_range_scan() {
    assert(type == INDEX_RANGE_SCAN);
    return u.index_range_scan;
  }
  const auto &index_range_scan() const {
    assert(type == INDEX_RANGE_SCAN);
    return u.index_range_scan;
  }
  auto &dynamic_index_range_scan() {
    assert(type == DYNAMIC_INDEX_RANGE_SCAN);
    return u.dynamic_index_range_scan;
  }
  const auto &dynamic_index_range_scan() const {
    assert(type == DYNAMIC_INDEX_RANGE_SCAN);
    return u.dynamic_index_range_scan;
  }
  auto &materialized_table_function() {
    assert(type == MATERIALIZED_TABLE_FUNCTION);
    return u.materialized_table_function;
  }
  const auto &materialized_table_function() const {
    assert(type == MATERIALIZED_TABLE_FUNCTION);
    return u.materialized_table_function;
  }
  auto &unqualified_count() {
    assert(type == UNQUALIFIED_COUNT);
    return u.unqualified_count;
  }
  const auto &unqualified_count() const {
    assert(type == UNQUALIFIED_COUNT);
    return u.unqualified_count;
  }
  auto &table_value_constructor() {
    assert(type == TABLE_VALUE_CONSTRUCTOR);
    return u.table_value_constructor;
  }
  const auto &table_value_constructor() const {
    assert(type == TABLE_VALUE_CONSTRUCTOR);
    return u.table_value_constructor;
  }
  auto &fake_single_row() {
    assert(type == FAKE_SINGLE_ROW);
    return u.fake_single_row;
  }
  const auto &fake_single_row() const {
    assert(type == FAKE_SINGLE_ROW);
    return u.fake_single_row;
  }
  auto &zero_rows() {
    assert(type == ZERO_ROWS);
    return u.zero_rows;
  }
  const auto &zero_rows() const {
    assert(type == ZERO_ROWS);
    return u.zero_rows;
  }
  auto &zero_rows_aggregated() {
    assert(type == ZERO_ROWS_AGGREGATED);
    return u.zero_rows_aggregated;
  }
  const auto &zero_rows_aggregated() const {
    assert(type == ZERO_ROWS_AGGREGATED);
    return u.zero_rows_aggregated;
  }
  auto &hash_join() {
    assert(type == HASH_JOIN);
    return u.hash_join;
  }
  const auto &hash_join() const {
    assert(type == HASH_JOIN);
    return u.hash_join;
  }
  auto &bka_join() {
    assert(type == BKA_JOIN);
    return u.bka_join;
  }
  const auto &bka_join() const {
    assert(type == BKA_JOIN);
    return u.bka_join;
  }
  auto &nested_loop_join() {
    assert(type == NESTED_LOOP_JOIN);
    return u.nested_loop_join;
  }
  const auto &nested_loop_join() const {
    assert(type == NESTED_LOOP_JOIN);
    return u.nested_loop_join;
  }
  auto &nested_loop_semijoin_with_duplicate_removal() {
    assert(type == NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL);
    return u.nested_loop_semijoin_with_duplicate_removal;
  }
  const auto &nested_loop_semijoin_with_duplicate_removal() const {
    assert(type == NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL);
    return u.nested_loop_semijoin_with_duplicate_removal;
  }
  auto &filter() {
    assert(type == FILTER);
    return u.filter;
  }
  const auto &filter() const {
    assert(type == FILTER);
    return u.filter;
  }
  auto &sort() {
    assert(type == SORT);
    return u.sort;
  }
  const auto &sort() const {
    assert(type == SORT);
    return u.sort;
  }
  auto &aggregate() {
    assert(type == AGGREGATE);
    return u.aggregate;
  }
  const auto &aggregate() const {
    assert(type == AGGREGATE);
    return u.aggregate;
  }
  auto &temptable_aggregate() {
    assert(type == TEMPTABLE_AGGREGATE);
    return u.temptable_aggregate;
  }
  const auto &temptable_aggregate() const {
    assert(type == TEMPTABLE_AGGREGATE);
    return u.temptable_aggregate;
  }
  auto &limit_offset() {
    assert(type == LIMIT_OFFSET);
    return u.limit_offset;
  }
  const auto &limit_offset() const {
    assert(type == LIMIT_OFFSET);
    return u.limit_offset;
  }
  auto &stream() {
    assert(type == STREAM);
    return u.stream;
  }
  const auto &stream() const {
    assert(type == STREAM);
    return u.stream;
  }
  auto &materialize() {
    assert(type == MATERIALIZE);
    return u.materialize;
  }
  const auto &materialize() const {
    assert(type == MATERIALIZE);
    return u.materialize;
  }
  auto &materialize_information_schema_table() {
    assert(type == MATERIALIZE_INFORMATION_SCHEMA_TABLE);
    return u.materialize_information_schema_table;
  }
  const auto &materialize_information_schema_table() const {
    assert(type == MATERIALIZE_INFORMATION_SCHEMA_TABLE);
    return u.materialize_information_schema_table;
  }
  auto &append() {
    assert(type == APPEND);
    return u.append;
  }
  const auto &append() const {
    assert(type == APPEND);
    return u.append;
  }
  auto &windowing() {
    assert(type == WINDOWING);
    return u.windowing;
  }
  const auto &windowing() const {
    assert(type == WINDOWING);
    return u.windowing;
  }
  auto &weedout() {
    assert(type == WEEDOUT);
    return u.weedout;
  }
  const auto &weedout() const {
    assert(type == WEEDOUT);
    return u.weedout;
  }
  auto &remove_duplicates() {
    assert(type == REMOVE_DUPLICATES);
    return u.remove_duplicates;
  }
  const auto &remove_duplicates() const {
    assert(type == REMOVE_DUPLICATES);
    return u.remove_duplicates;
  }
  auto &alternative() {
    assert(type == ALTERNATIVE);
    return u.alternative;
  }
  const auto &alternative() const {
    assert(type == ALTERNATIVE);
    return u.alternative;
  }
  auto &cache_invalidator() {
    assert(type == CACHE_INVALIDATOR);
    return u.cache_invalidator;
  }
  const auto &cache_invalidator() const {
    assert(type == CACHE_INVALIDATOR);
    return u.cache_invalidator;
  }

 private:
  // We'd prefer if this could be an std::variant, but we don't have C++17 yet.
  // It is private to force all access to be through the type-checking
  // accessors.
  //
  // For information about the meaning of each value, see the corresponding
  // row iterator constructors.
  union {
    struct {
      TABLE *table;
    } table_scan;
    struct {
      TABLE *table;
      int idx;
      bool use_order;
      bool reverse;
    } index_scan;
    struct {
      TABLE *table;
      TABLE_REF *ref;
      bool use_order;
      bool reverse;
    } ref;
    struct {
      TABLE *table;
      TABLE_REF *ref;
      bool use_order;
    } ref_or_null;
    struct {
      TABLE *table;
      TABLE_REF *ref;
      bool use_order;
    } eq_ref;
    struct {
      TABLE *table;
      TABLE_REF *ref;
      bool use_order;
      bool is_unique;
    } pushed_join_ref;
    struct {
      TABLE *table;
      TABLE_REF *ref;
      bool use_order;
    } full_text_search;
    struct {
      TABLE *table;
      TABLE_REF *ref;
    } const_table;
    struct {
      Item *cache_idx_cond;
      TABLE *table;
      TABLE_REF *ref;
      AccessPath *bka_path;
      int mrr_flags;
      bool keep_current_rowid;
    } mrr;
    struct {
      TABLE *table;
    } follow_tail;
    struct {
      TABLE *table;
      QUICK_SELECT_I *quick;
    } index_range_scan;
    struct {
      TABLE *table;
      QEP_TAB *qep_tab;  // Used only for buffering.
    } dynamic_index_range_scan;
    struct {
      TABLE *table;
      Table_function *table_function;
      AccessPath *table_path;
    } materialized_table_function;
    struct {
    } unqualified_count;

    struct {
      // No members (implicit from the JOIN).
    } table_value_constructor;
    struct {
      // No members.
    } fake_single_row;
    struct {
      // See ZeroRowsIterator for an explanation as of why there is
      // a child path here.
      AccessPath *child;
      // Used for EXPLAIN only.
      // TODO(sgunders): make an enum.
      const char *cause;
    } zero_rows;
    struct {
      // Used for EXPLAIN only.
      // TODO(sgunders): make an enum.
      const char *cause;
    } zero_rows_aggregated;

    struct {
      AccessPath *outer, *inner;
      const JoinPredicate *join_predicate;
      bool allow_spill_to_disk;
      bool store_rowids;  // Whether we are below a weedout or not.
      table_map tables_to_get_rowid_for;
    } hash_join;
    struct {
      AccessPath *outer, *inner;
      JoinType join_type;
      unsigned mrr_length_per_rec;
      float rec_per_key;
      bool store_rowids;  // Whether we are below a weedout or not.
      table_map tables_to_get_rowid_for;
    } bka_join;
    struct {
      AccessPath *outer, *inner;
      JoinType join_type;
      bool pfs_batch_mode;
    } nested_loop_join;
    struct {
      AccessPath *outer, *inner;
      const TABLE *table;
      KEY *key;
      size_t key_len;
    } nested_loop_semijoin_with_duplicate_removal;

    struct {
      AccessPath *child;
      Item *condition;
    } filter;
    struct {
      AccessPath *child;
      Filesort *filesort;
      table_map tables_to_get_rowid_for;
    } sort;
    struct {
      AccessPath *child;
      bool rollup;
    } aggregate;
    struct {
      AccessPath *subquery_path;
      Temp_table_param *temp_table_param;
      TABLE *table;
      AccessPath *table_path;
      int ref_slice;
    } temptable_aggregate;
    struct {
      AccessPath *child;
      ha_rows limit;
      ha_rows offset;
      bool count_all_rows;
      bool reject_multiple_rows;
      // Only used when the LIMIT is on a UNION with SQL_CALC_FOUND_ROWS.
      // See SELECT_LEX_UNIT::send_records.
      ha_rows *send_records_override;
    } limit_offset;
    struct {
      AccessPath *child;
      JOIN *join;
      Temp_table_param *temp_table_param;
      TABLE *table;
      bool provide_rowid;
      int ref_slice;
    } stream;
    struct {
      AccessPath *table_path;

      // Large, and has nontrivial destructors, so split out
      // into its own allocation.
      MaterializePathParameters *param;
    } materialize;
    struct {
      AccessPath *table_path;
      TABLE_LIST *table_list;
      Item *condition;
    } materialize_information_schema_table;
    struct {
      Mem_root_array<AppendPathParameters> *children;
    } append;
    struct {
      AccessPath *child;
      Temp_table_param *temp_table_param;
      int ref_slice;
      bool needs_buffering;
    } windowing;
    struct {
      AccessPath *child;
      SJ_TMP_TABLE *weedout_table;
      table_map tables_to_get_rowid_for;
    } weedout;
    struct {
      AccessPath *child;
      TABLE *table;
      KEY *key;
      unsigned loosescan_key_len;
    } remove_duplicates;
    struct {
      AccessPath *table_scan_path;

      // For the ref.
      AccessPath *child;
      TABLE_REF *used_ref;
    } alternative;
    struct {
      AccessPath *child;
      const char *name;
    } cache_invalidator;
  } u;
};
static_assert(std::is_trivially_destructible<AccessPath>::value,
              "AccessPath must be trivially destructible, as it is allocated "
              "on the MEM_ROOT and not wrapped in unique_ptr_destroy_only"
              "(because multiple candidates during planning could point to "
              "the same access paths, and refcounting would be expensive)");
static_assert(sizeof(AccessPath) <= 72,
              "We will be creating a lot of access paths in the join "
              "optimizer, so be sure not to bloat it without noticing. "
              "(32 bytes for the base, 40 bytes for the variant.)");

inline void CopyCosts(const AccessPath &from, AccessPath *to) {
  to->num_output_rows = from.num_output_rows;
  to->cost = from.cost;
}

// Trivial factory functions for all of the types of access paths above.

inline AccessPath *NewTableScanAccessPath(THD *thd, TABLE *table,
                                          bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::TABLE_SCAN;
  path->count_examined_rows = count_examined_rows;
  path->table_scan().table = table;
  return path;
}

inline AccessPath *NewIndexScanAccessPath(THD *thd, TABLE *table, int idx,
                                          bool use_order, bool reverse,
                                          bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::INDEX_SCAN;
  path->count_examined_rows = count_examined_rows;
  path->index_scan().table = table;
  path->index_scan().idx = idx;
  path->index_scan().use_order = use_order;
  path->index_scan().reverse = reverse;
  return path;
}

inline AccessPath *NewRefAccessPath(THD *thd, TABLE *table, TABLE_REF *ref,
                                    bool use_order, bool reverse,
                                    bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::REF;
  path->count_examined_rows = count_examined_rows;
  path->ref().table = table;
  path->ref().ref = ref;
  path->ref().use_order = use_order;
  path->ref().reverse = reverse;
  return path;
}

inline AccessPath *NewRefOrNullAccessPath(THD *thd, TABLE *table,
                                          TABLE_REF *ref, bool use_order,
                                          bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::REF_OR_NULL;
  path->count_examined_rows = count_examined_rows;
  path->ref_or_null().table = table;
  path->ref_or_null().ref = ref;
  path->ref_or_null().use_order = use_order;
  return path;
}

inline AccessPath *NewEQRefAccessPath(THD *thd, TABLE *table, TABLE_REF *ref,
                                      bool use_order,
                                      bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::EQ_REF;
  path->count_examined_rows = count_examined_rows;
  path->eq_ref().table = table;
  path->eq_ref().ref = ref;
  path->eq_ref().use_order = use_order;
  return path;
}

inline AccessPath *NewPushedJoinRefAccessPath(THD *thd, TABLE *table,
                                              TABLE_REF *ref, bool use_order,
                                              bool is_unique,
                                              bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::PUSHED_JOIN_REF;
  path->count_examined_rows = count_examined_rows;
  path->pushed_join_ref().table = table;
  path->pushed_join_ref().ref = ref;
  path->pushed_join_ref().use_order = use_order;
  path->pushed_join_ref().is_unique = is_unique;
  return path;
}

inline AccessPath *NewFullTextSearchAccessPath(THD *thd, TABLE *table,
                                               TABLE_REF *ref, bool use_order,
                                               bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::FULL_TEXT_SEARCH;
  path->count_examined_rows = count_examined_rows;
  path->full_text_search().table = table;
  path->full_text_search().ref = ref;
  path->full_text_search().use_order = use_order;
  return path;
}

inline AccessPath *NewConstTableAccessPath(THD *thd, TABLE *table,
                                           TABLE_REF *ref,
                                           bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::CONST_TABLE;
  path->count_examined_rows = count_examined_rows;
  path->num_output_rows = 1.0;
  path->cost = 0.0;
  path->const_table().table = table;
  path->const_table().ref = ref;
  return path;
}

inline AccessPath *NewMRRAccessPath(THD *thd, Item *cache_idx_cond,
                                    TABLE *table, TABLE_REF *ref,
                                    int mrr_flags) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MRR;
  path->mrr().cache_idx_cond = cache_idx_cond;
  path->mrr().table = table;
  path->mrr().ref = ref;
  path->mrr().mrr_flags = mrr_flags;

  // This will be filled in when the BKA iterator is created.
  path->mrr().bka_path = nullptr;

  return path;
}

inline AccessPath *NewFollowTailAccessPath(THD *thd, TABLE *table,
                                           bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::FOLLOW_TAIL;
  path->count_examined_rows = count_examined_rows;
  path->follow_tail().table = table;
  return path;
}

inline AccessPath *NewIndexRangeScanAccessPath(THD *thd, TABLE *table,
                                               QUICK_SELECT_I *quick,
                                               bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::INDEX_RANGE_SCAN;
  path->count_examined_rows = count_examined_rows;
  path->index_range_scan().table = table;
  path->index_range_scan().quick = quick;
  return path;
}

inline AccessPath *NewDynamicIndexRangeScanAccessPath(
    THD *thd, TABLE *table, QEP_TAB *qep_tab, bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::DYNAMIC_INDEX_RANGE_SCAN;
  path->count_examined_rows = count_examined_rows;
  path->dynamic_index_range_scan().table = table;
  path->dynamic_index_range_scan().qep_tab = qep_tab;
  return path;
}

inline AccessPath *NewMaterializedTableFunctionAccessPath(
    THD *thd, TABLE *table, Table_function *table_function,
    AccessPath *table_path) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MATERIALIZED_TABLE_FUNCTION;
  path->materialized_table_function().table = table;
  path->materialized_table_function().table_function = table_function;
  path->materialized_table_function().table_path = table_path;
  return path;
}

inline AccessPath *NewUnqualifiedCountAccessPath(THD *thd) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::UNQUALIFIED_COUNT;
  return path;
}

inline AccessPath *NewTableValueConstructorAccessPath(THD *thd) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::TABLE_VALUE_CONSTRUCTOR;
  // The iterator keeps track of which row it is at in examined_rows,
  // so we always need to give it the pointer.
  path->count_examined_rows = true;
  return path;
}

inline AccessPath *NewNestedLoopSemiJoinWithDuplicateRemovalAccessPath(
    THD *thd, AccessPath *outer, AccessPath *inner, const TABLE *table,
    KEY *key, size_t key_len) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL;
  path->nested_loop_semijoin_with_duplicate_removal().outer = outer;
  path->nested_loop_semijoin_with_duplicate_removal().inner = inner;
  path->nested_loop_semijoin_with_duplicate_removal().table = table;
  path->nested_loop_semijoin_with_duplicate_removal().key = key;
  path->nested_loop_semijoin_with_duplicate_removal().key_len = key_len;
  return path;
}

inline AccessPath *NewFilterAccessPath(THD *thd, AccessPath *child,
                                       Item *condition) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::FILTER;
  path->filter().child = child;
  path->filter().condition = condition;
  return path;
}

// Not inline, because it needs access to filesort internals
// (which are forward-declared in this file).
AccessPath *NewSortAccessPath(THD *thd, AccessPath *child, Filesort *filesort,
                              bool count_examined_rows);

inline AccessPath *NewAggregateAccessPath(THD *thd, AccessPath *child,
                                          bool rollup) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::AGGREGATE;
  path->aggregate().child = child;
  path->aggregate().rollup = rollup;
  return path;
}

inline AccessPath *NewTemptableAggregateAccessPath(
    THD *thd, AccessPath *subquery_path, Temp_table_param *temp_table_param,
    TABLE *table, AccessPath *table_path, int ref_slice) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::TEMPTABLE_AGGREGATE;
  path->temptable_aggregate().subquery_path = subquery_path;
  path->temptable_aggregate().temp_table_param = temp_table_param;
  path->temptable_aggregate().table = table;
  path->temptable_aggregate().table_path = table_path;
  path->temptable_aggregate().ref_slice = ref_slice;
  return path;
}

inline AccessPath *NewLimitOffsetAccessPath(THD *thd, AccessPath *child,
                                            ha_rows limit, ha_rows offset,
                                            bool count_all_rows,
                                            bool reject_multiple_rows,
                                            ha_rows *send_records_override) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::LIMIT_OFFSET;
  path->limit_offset().child = child;
  path->limit_offset().limit = limit;
  path->limit_offset().offset = offset;
  path->limit_offset().count_all_rows = count_all_rows;
  path->limit_offset().reject_multiple_rows = reject_multiple_rows;
  path->limit_offset().send_records_override = send_records_override;

  if (child->num_output_rows >= 0.0) {
    path->num_output_rows =
        std::min<double>(child->num_output_rows, limit - offset);
  }
  // We have nothing better, since we don't know how much is startup cost.
  path->cost = child->cost;

  return path;
}

inline AccessPath *NewFakeSingleRowAccessPath(THD *thd,
                                              bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::FAKE_SINGLE_ROW;
  path->count_examined_rows = count_examined_rows;
  path->num_output_rows = 1.0;
  path->cost = 0.0;
  return path;
}

inline AccessPath *NewZeroRowsAccessPath(THD *thd, AccessPath *child,
                                         const char *cause) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::ZERO_ROWS;
  path->zero_rows().child = child;
  path->zero_rows().cause = cause;
  return path;
}

inline AccessPath *NewZeroRowsAccessPath(THD *thd, const char *cause) {
  return NewZeroRowsAccessPath(thd, /*child=*/nullptr, cause);
}

inline AccessPath *NewZeroRowsAggregatedAccessPath(THD *thd,
                                                   const char *cause) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::ZERO_ROWS_AGGREGATED;
  path->zero_rows_aggregated().cause = cause;
  return path;
}

inline AccessPath *NewStreamingAccessPath(THD *thd, AccessPath *child,
                                          JOIN *join,
                                          Temp_table_param *temp_table_param,
                                          TABLE *table, int ref_slice) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::STREAM;
  path->stream().child = child;
  path->stream().join = join;
  path->stream().temp_table_param = temp_table_param;
  path->stream().table = table;
  path->stream().ref_slice = ref_slice;
  // Will be set later if we get a weedout access path as parent.
  path->stream().provide_rowid = false;
  return path;
}

inline Mem_root_array<MaterializePathParameters::QueryBlock>
SingleMaterializeQueryBlock(THD *thd, AccessPath *path, int select_number,
                            JOIN *join, bool copy_fields_and_items,
                            Temp_table_param *temp_table_param) {
  assert(path != nullptr);
  Mem_root_array<MaterializePathParameters::QueryBlock> array(thd->mem_root, 1);
  MaterializePathParameters::QueryBlock &query_block = array[0];
  query_block.subquery_path = path;
  query_block.select_number = select_number;
  query_block.join = join;
  query_block.disable_deduplication_by_hash_field = false;
  query_block.copy_fields_and_items = copy_fields_and_items;
  query_block.temp_table_param = temp_table_param;
  return array;
}

inline AccessPath *NewMaterializeAccessPath(
    THD *thd,
    Mem_root_array<MaterializePathParameters::QueryBlock> query_blocks,
    Mem_root_array<const AccessPath *> *invalidators, TABLE *table,
    AccessPath *table_path, Common_table_expr *cte, SELECT_LEX_UNIT *unit,
    int ref_slice, bool rematerialize, ha_rows limit_rows,
    bool reject_multiple_rows) {
  MaterializePathParameters *param =
      new (thd->mem_root) MaterializePathParameters;
  param->query_blocks = std::move(query_blocks);
  if (rematerialize) {
    // There's no point in adding invalidators if we're rematerializing
    // every time anyway.
    param->invalidators = nullptr;
  } else {
    param->invalidators = invalidators;
  }
  param->table = table;
  param->cte = cte;
  param->unit = unit;
  param->ref_slice = ref_slice;
  param->rematerialize = rematerialize;
  param->limit_rows = limit_rows;
  param->reject_multiple_rows = reject_multiple_rows;

#ifndef NDEBUG
  for (MaterializePathParameters::QueryBlock &query_block :
       param->query_blocks) {
    assert(query_block.subquery_path != nullptr);
  }
#endif

  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MATERIALIZE;
  path->materialize().table_path = table_path;
  path->materialize().param = param;
  return path;
}

inline AccessPath *NewMaterializeInformationSchemaTableAccessPath(
    THD *thd, AccessPath *table_path, TABLE_LIST *table_list, Item *condition) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE;
  path->materialize_information_schema_table().table_path = table_path;
  path->materialize_information_schema_table().table_list = table_list;
  path->materialize_information_schema_table().condition = condition;
  return path;
}

// The Mem_root_array must be allocated on a MEM_ROOT that lives at least for as
// long as the access path.
inline AccessPath *NewAppendAccessPath(
    THD *thd, Mem_root_array<AppendPathParameters> *children) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::APPEND;
  path->append().children = children;
  return path;
}

inline AccessPath *NewWindowingAccessPath(THD *thd, AccessPath *child,
                                          Temp_table_param *temp_table_param,
                                          int ref_slice, bool needs_buffering) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::WINDOWING;
  path->windowing().child = child;
  path->windowing().temp_table_param = temp_table_param;
  path->windowing().ref_slice = ref_slice;
  path->windowing().needs_buffering = needs_buffering;
  return path;
}

inline AccessPath *NewWeedoutAccessPath(THD *thd, AccessPath *child,
                                        SJ_TMP_TABLE *weedout_table) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::WEEDOUT;
  path->weedout().child = child;
  path->weedout().weedout_table = weedout_table;
  path->weedout().tables_to_get_rowid_for =
      0;  // Must be handled by the caller.
  return path;
}

inline AccessPath *NewRemoveDuplicatesAccessPath(THD *thd, AccessPath *child,
                                                 TABLE *table, KEY *key,
                                                 unsigned loosescan_key_len) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::REMOVE_DUPLICATES;
  path->remove_duplicates().child = child;
  path->remove_duplicates().table = table;
  path->remove_duplicates().key = key;
  path->remove_duplicates().loosescan_key_len = loosescan_key_len;
  return path;
}

inline AccessPath *NewAlternativeAccessPath(THD *thd, AccessPath *child,
                                            AccessPath *table_scan_path,
                                            TABLE_REF *used_ref) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::ALTERNATIVE;
  path->alternative().table_scan_path = table_scan_path;
  path->alternative().child = child;
  path->alternative().used_ref = used_ref;
  return path;
}

inline AccessPath *NewInvalidatorAccessPath(THD *thd, AccessPath *child,
                                            const char *name) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::CACHE_INVALIDATOR;
  path->cache_invalidator().child = child;
  path->cache_invalidator().name = name;
  return path;
}

void FindTablesToGetRowidFor(AccessPath *path);

unique_ptr_destroy_only<RowIterator> CreateIteratorFromAccessPath(
    THD *thd, AccessPath *path, JOIN *join, bool eligible_for_batch_mode);

void SetCostOnTableAccessPath(const Cost_model_server &cost_model,
                              const POSITION *pos, bool is_after_filter,
                              AccessPath *path);

/**
  Returns a map of all tables read when `path` or any of its children are
  exectued. Only iterators that are part of the same query block as `path`
  are considered.

  If a table is read that doesn't have a map, specifically the temporary
  tables made as part of materialization within the same query block,
  RAND_TABLE_BIT will be set as a convention and none of that access path's
  children will be included in the map. In this case, the caller will need to
  manually go in and find said access path, to ask it for its TABLE object.
 */
table_map GetUsedTables(const AccessPath *path);

#endif  // SQL_JOIN_OPTIMIZER_ACCESS_PATH_H
