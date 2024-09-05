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

#ifndef SQL_JOIN_OPTIMIZER_ACCESS_PATH_H
#define SQL_JOIN_OPTIMIZER_ACCESS_PATH_H

#include <assert.h>
#include <stdint.h>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_table_map.h"
#include "sql/item.h"
// IWYU suggests removing row_iterator.h, but then the inlined short form of
// CreateIteratorFromAccessPath() fails to compile. So use a pragma to keep it.
#include "sql/iterators/row_iterator.h"  // IWYU pragma: keep
#include "sql/join_optimizer/interesting_orders_defs.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/node_map.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/olap.h"
#include "sql/sql_class.h"
#include "sql/table.h"

class Cost_model_server;
class Filesort;
class HashJoinCondition;
class Item_func_match;
class JOIN;
class KEY;
class Query_expression;
class QEP_TAB;
class QUICK_RANGE;
class SJ_TMP_TABLE;
class Table_function;
class Temp_table_param;
class Window;
struct AccessPath;
struct GroupIndexSkipScanParameters;
struct IndexSkipScanParameters;
struct Index_lookup;
struct KEY_PART;
struct POSITION;
struct RelationalExpression;

/**
  A specification that two specific relational expressions
  (e.g., two tables, or a table and a join between two other tables)
  should be joined together. The actual join conditions, if any,
  live inside the “expr” object, as does the join type etc.
 */
struct JoinPredicate {
  RelationalExpression *expr;
  double selectivity;

  // If this join is made using a hash join, estimates the width
  // of each row as stored in the hash table, in bytes.
  size_t estimated_bytes_per_row;

  // The set of (additional) functional dependencies that are active
  // after this join predicate has been applied. E.g. if we're joining
  // on t1.x = t2.x, there will be a bit for that functional dependency.
  // We don't currently support more complex join conditions, but there's
  // no conceptual reason why we couldn't, e.g. a join on a = b + c
  // could give rise to the FD {b, c} → a and possibly even {a, b} → c
  // or {a, c} → b.
  //
  // Used in the processing of interesting orders.
  FunctionalDependencySet functional_dependencies;

  // A less compact form of functional_dependencies, used during building
  // (FunctionalDependencySet bitmaps are only available after all functional
  // indexes have been collected and Build() has been called).
  Mem_root_array<int> functional_dependencies_idx;

  // A semijoin on the following format:
  //
  // SELECT ... FROM t1 WHERE EXISTS
  // (SELECT ... FROM t2 WHERE t1.f1=t2.f2 AND t1.f3=t2.f4 ... t1.fn=t2.fm)
  //
  // may be transformed into an equivalent inner join:
  //
  // SELECT ... FROM (SELECT DISTINCT f2, f4...fm FROM t2) d JOIN t1
  // ON t1.f1=d.f2 AND t1.f3=d.f4 ... t1.fn=d.fm
  //
  // If this is a suitable semijoin: This field will identify the the
  // grouping given by (f2, f4..fm). (@see
  // LogicalOrderings::RemapOrderingIndex() for a description of how this
  // value can be mapped to an actual ordering). The join
  // optimizer will then consider deduplicating on it and applying the
  // above transform. If no such grouping was found, this field will be -1.
  int ordering_idx_needed_for_semijoin_rewrite = -1;

  // Same as ordering_idx_needed_for_semijoin_rewrite, but given to the
  // RemoveDuplicatesIterator for doing the actual grouping. Allocated
  // on the MEM_ROOT. Can be empty, in which case a LIMIT 1 would do.
  Item **semijoin_group = nullptr;
  int semijoin_group_size = 0;
};

/**
  A filter of some sort that is not a join condition (those are stored
  in JoinPredicate objects). AND conditions are typically split up into
  multiple Predicates.
 */
struct Predicate {
  Item *condition;

  // condition->used_tables(), converted to a NodeMap.
  hypergraph::NodeMap used_nodes;

  // tables referred to by the condition, plus any tables whose values
  // can null any of those tables. (Even when reordering outer joins,
  // at least one of those tables will still be present on the
  // left-hand side of the outer join, so this is sufficient.)
  //
  // As a special case, we allow setting RAND_TABLE_BIT, even though it
  // is normally part of a table_map, not a NodeMap.
  hypergraph::NodeMap total_eligibility_set;

  double selectivity;

  // Whether this predicate is a join condition after all; it was promoted
  // to a WHERE predicate since it was part of a cycle (see the comment in
  // AddCycleEdges()). If it is, it is usually ignored so that we don't
  // double-apply join conditions -- but if the join in question was not
  // applied (because the cycle was broken at this point), the predicate
  // would come into play. This is normally registered on the join itself
  // (see RelationalExpression::join_predicate_bitmap), but having the bit
  // on the predicate itself is used to avoid trying to push it down as a
  // sargable predicate.
  bool was_join_condition = false;

  // If this is a join condition that came from a multiple equality,
  // and we have decided to create a mesh from that multiple equality,
  // returns the index of it into the “multiple_equalities” array
  // in MakeJoinHypergraph(). (You don't actually need the array to
  // use this; it's just an opaque index to deduplicate between different
  // predicates.) Otherwise, -1.
  int source_multiple_equality_idx = -1;

  // See the equivalent fields in JoinPredicate.
  FunctionalDependencySet functional_dependencies;
  Mem_root_array<int> functional_dependencies_idx;

  // The list of all subqueries referred to in this predicate, if any.
  // The optimizer uses this to add their materialized/non-materialized
  // costs when evaluating filters.
  Mem_root_array<ContainedSubquery> contained_subqueries;
};

struct AppendPathParameters {
  AccessPath *path;
  JOIN *join;
};

/// To indicate that a row estimate is not yet made.
inline constexpr double kUnknownRowCount = -1.0;

/// To indicate that a cost estimate is not yet made. We use a large negative
/// value to avoid getting a positive result if we by mistake add this to
/// a real (positive) cost.
inline constexpr double kUnknownCost = -1e12;

/// Calculate the cost of reading the first row from an access path, given
/// estimates for init cost, total cost and the number of rows returned.
inline double FirstRowCost(double init_cost, double total_cost,
                           double output_rows) {
  assert(init_cost >= 0.0);
  assert(total_cost >= init_cost);
  assert(output_rows >= 0.0);
  if (output_rows <= 1.0) {
    return total_cost;
  }
  return init_cost + (total_cost - init_cost) / output_rows;
}

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
  enum Type : uint8_t {
    // Basic access paths (those with no children, at least nominally).
    // NOTE: When adding more paths to this section, also update GetBasicTable()
    // to handle them.
    TABLE_SCAN,
    SAMPLE_SCAN,
    INDEX_SCAN,
    INDEX_DISTANCE_SCAN,
    REF,
    REF_OR_NULL,
    EQ_REF,
    PUSHED_JOIN_REF,
    FULL_TEXT_SEARCH,
    CONST_TABLE,
    MRR,
    FOLLOW_TAIL,
    INDEX_RANGE_SCAN,
    INDEX_MERGE,
    ROWID_INTERSECTION,
    ROWID_UNION,
    INDEX_SKIP_SCAN,
    GROUP_INDEX_SKIP_SCAN,
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
    WINDOW,
    WEEDOUT,
    REMOVE_DUPLICATES,
    REMOVE_DUPLICATES_ON_INDEX,
    ALTERNATIVE,
    CACHE_INVALIDATOR,

    // Access paths that modify tables.
    DELETE_ROWS,
    UPDATE_ROWS,
  } type;

  /// A general enum to describe the safety of a given operation.
  /// Currently we only use this to describe row IDs, but it can easily
  /// be reused for safety of updating a table we're reading from
  /// (the Halloween problem), or just generally unreproducible results
  /// (e.g. a TABLESAMPLE changing due to external factors).
  ///
  /// Less safe values have higher numerical values.
  enum Safety : uint8_t {
    /// The given operation is always safe on this access path.
    SAFE = 0,

    /// The given operation is safe if this access path is scanned once,
    /// but not if it's scanned multiple times (e.g. used on the inner side
    /// of a nested-loop join). A typical example of this is a derived table
    /// or CTE that is rematerialized on each scan, so that references to
    /// the old values (such as row IDs) are no longer valid.
    SAFE_IF_SCANNED_ONCE = 1,

    /// The given operation is unsafe on this access path, no matter how many
    /// or few times it's scanned. Often, it may help to materialize it
    /// (assuming the materialization itself doesn't use the operation
    /// in question).
    UNSAFE = 2
  };

  /// Whether it is safe to get row IDs (for sorting) from this access path.
  Safety safe_for_rowid = SAFE;

  /// Whether this access path counts as one that scans a base table,
  /// and thus should be counted towards examined_rows. It can sometimes
  /// seem a bit arbitrary which iterators count towards examined_rows
  /// and which ones do not, so the only canonical reference is the tests.
  bool count_examined_rows : 1 {false};

  /// Whether this access path contains a GROUP_INDEX_SKIP_SCAN
  bool has_group_skip_scan : 1 {false};

#ifndef NDEBUG
  /// Whether this access path is forced preferred over all others by means
  /// of a SET DEBUG force_subplan_0x... statement.
  bool forced_by_dbug : 1 {false};
#endif

  /// For UPDATE and DELETE statements: The node index of a table which can be
  /// updated or deleted from immediately as the rows are read from the
  /// iterator, if this path is only read from once. -1 if there is no such
  /// table in this path.
  ///
  /// Note that this is an index into CostingReceiver's array of nodes, and is
  /// not necessarily equal to the table number within the query block given by
  /// Table_ref::tableno().
  ///
  /// The table, if any, is currently always the outermost table in the path.
  ///
  /// It is possible to have plans where it would be safe to operate
  /// "immediately" on more than one table. For example, if we do a merge join,
  /// it is safe to perform immediate deletes on tables on the inner side of the
  /// join, since both sides are read only once. (However, we currently do not
  /// support merge joins.)
  ///
  /// Another possibility is when the outer table of a nested loop join is
  /// guaranteed to return at most one row (typically, a unique index lookup
  /// aka. eq_ref). Then it's safe to delete immediately from both sides of the
  /// nested loop join. But we don't to this yet.
  ///
  /// Hash joins read both sides exactly once, However, with hash joins, the
  /// scans on the inner tables are not positioned on the correct row when the
  /// result of the join is returned, so the immediate delete logic will need to
  /// be changed to reposition the underlying scans before doing the immediate
  /// deletes. While this can be done, it makes the benefit of immediate deletes
  /// less obvious for these tables, and it can also be a loss in some cases,
  /// because we lose the deduplication provided by the Unique object used for
  /// buffered deletes (the immediate deletes could end up spending time
  /// repositioning to already deleted rows). So we currently don't attempt to
  /// do immediate deletes from inner tables of hash joins either.
  ///
  /// The outer table of a hash join can be deleted from immediately if the
  /// inner table fits in memory. If the hash join spills to disk, though,
  /// neither the rows of the outer table nor the rows of the inner table come
  /// out in the order of the underlying scan, so it is not safe in general to
  /// perform immediate deletes on the outer table of a hash join.
  ///
  /// If support for immediate operations on multiple tables is added,
  /// this member could be changed from a node index to a NodeMap.
  int8_t immediate_update_delete_table{-1};

  /// Which ordering the rows produced by this path follow, if any
  /// (see interesting_orders.h). This is really a LogicalOrderings::StateIndex,
  /// but we don't want to add a dependency on interesting_orders.h from
  /// this file, so we use the base type instead of the typedef here.
  int ordering_state = 0;

  /// If an iterator has been instantiated for this access path, points to the
  /// iterator. Used for constructing iterators that need to talk to each other
  /// (e.g. for recursive CTEs, or BKA join), and also for locating timing
  /// information in EXPLAIN ANALYZE queries.
  RowIterator *iterator = nullptr;

  double cost() const { return m_cost; }

  double init_cost() const { return m_init_cost; }

  /// The cost of reading the first row.
  double first_row_cost() const {
    return FirstRowCost(m_init_cost, m_cost, m_num_output_rows);
  }

  double init_once_cost() const { return m_init_once_cost; }

  double cost_before_filter() const { return m_cost_before_filter; }

  void set_cost(double val) {
    assert(std::isfinite(val));
    assert(val >= 0.0 || val == kUnknownCost);
    m_cost = val;
  }

  void set_init_cost(double val) {
    assert(std::isfinite(val));
    assert(val >= 0.0 || val == kUnknownCost);
    m_init_cost = val;
  }

  void set_init_once_cost(double val) {
    assert(std::isfinite(val));
    assert(val >= 0.0);
    m_init_once_cost = val;
  }

  void set_cost_before_filter(double val) {
    assert(std::isfinite(val));
    assert(val >= 0.0 || val == kUnknownCost);
    m_cost_before_filter = val;
  }

  /// Return the cost of scanning the given path for the second time
  /// (or later) in the given query block. This is really the interesting
  /// metric, not init_once_cost in itself, but since nearly all paths
  /// have zero init_once_cost, storing that instead allows us to skip
  /// a lot of repeated path->init_once_cost = path->init_cost calls
  /// in the code.
  double rescan_cost() const { return cost() - init_once_cost(); }

  /// If no filter, identical to num_output_rows.
  double num_output_rows_before_filter{kUnknownRowCount};

  /// Bitmap of WHERE predicates that we are including on this access path,
  /// referring to the “predicates” array internal to the join optimizer.
  /// Since bit masks are much cheaper to deal with than creating Item
  /// objects, and we don't invent new conditions during join optimization
  /// (all of them are known when we begin optimization), we stick to
  /// manipulating bit masks during optimization, saying which filters will be
  /// applied at this node (a 1-bit means the filter will be applied here; if
  /// there are multiple ones, they are ANDed together).
  ///
  /// This is used during join optimization only; before iterators are
  /// created, we will add FILTER access paths to represent these instead,
  /// removing the dependency on the array. Said FILTER paths are by
  /// convention created with materialize_subqueries = false, since the by far
  /// most common case is that there are no subqueries in the predicate.
  /// In other words, if you wish to represent a filter with
  /// materialize_subqueries = true, you will need to make an explicit FILTER
  /// node.
  ///
  /// See also nested_loop_join().equijoin_predicates, which is for filters
  /// being applied _before_ nested-loop joins, but is otherwise the same idea.
  OverflowBitset filter_predicates{0};

  /// Bitmap of sargable join predicates that have already been applied
  /// in this access path by means of an index lookup (ref access),
  /// again referring to “predicates”, and thus should not be counted again
  /// for selectivity. Note that the filter may need to be applied
  /// nevertheless (especially in case of type conversions); see
  /// subsumed_sargable_join_predicates.
  ///
  /// Since these refer to the same array as filter_predicates, they will
  /// never overlap with filter_predicates, and so we can reuse the same
  /// memory using an alias (a union would not be allowed, since OverflowBitset
  /// is a class with non-trivial default constructor), even though the meaning
  /// is entirely separate. If N = num_where_predicates in the hypergraph, then
  /// bits 0..(N-1) belong to filter_predicates, and the rest to
  /// applied_sargable_join_predicates.
  OverflowBitset &applied_sargable_join_predicates() {
    return filter_predicates;
  }
  const OverflowBitset &applied_sargable_join_predicates() const {
    return filter_predicates;
  }

  /// Bitmap of WHERE predicates that touch tables we have joined in,
  /// but that we could not apply yet (for instance because they reference
  /// other tables, or because because we could not push them down into
  /// the nullable side of outer joins). Used during planning only
  /// (see filter_predicates).
  OverflowBitset delayed_predicates{0};

  /// Similar to applied_sargable_join_predicates, bitmap of sargable
  /// join predicates that have been applied and will subsume the join
  /// predicate entirely, ie., not only should the selectivity not be
  /// double-counted, but the predicate itself is redundant and need not
  /// be applied as a filter. (It is an error to have a bit set here but not
  /// in applied_sargable_join_predicates.)
  OverflowBitset &subsumed_sargable_join_predicates() {
    return delayed_predicates;
  }
  const OverflowBitset &subsumed_sargable_join_predicates() const {
    return delayed_predicates;
  }

  /// If nonzero, a bitmap of other tables whose joined-in rows must already be
  /// loaded when rows from this access path are evaluated; that is, this
  /// access path must be put on the inner side of a nested-loop join (or
  /// multiple such joins) where the outer side includes all of the given
  /// tables.
  ///
  /// The most obvious case for this is dependent tables in LATERAL, but a more
  /// common case is when we have pushed join conditions referring to those
  /// tables; e.g., if this access path represents t1 and we have a condition
  /// t1.x=t2.x that is pushed down into an index lookup (ref access), t2 will
  /// be set in this bitmap. We can still join in other tables, deferring t2,
  /// but the bit(s) will then propagate, and we cannot be on the right side of
  /// a hash join until parameter_tables is zero again. (Also see
  /// DisallowParameterizedJoinPath() for when we disallow such deferring,
  /// as an optimization.)
  ///
  /// As a special case, we allow setting RAND_TABLE_BIT, even though it
  /// is normally part of a table_map, not a NodeMap. In this case, it specifies
  /// that the access path is entirely noncachable, because it depends on
  /// something nondeterministic or an outer reference, and thus can never be on
  /// the right side of a hash join, ever.
  hypergraph::NodeMap parameter_tables{0};

  /// Auxiliary data used by a secondary storage engine while processing the
  /// access path during optimization and execution. The secondary storage
  /// engine is free to store any useful information in this member, for example
  /// extra statistics or cost estimates. The data pointed to is fully owned by
  /// the secondary storage engine, and it is the responsibility of the
  /// secondary engine to manage the memory and make sure it is properly
  /// destroyed.
  void *secondary_engine_data{nullptr};

  // Accessors for the union below.
  auto &table_scan() {
    assert(type == TABLE_SCAN);
    return u.table_scan;
  }
  const auto &table_scan() const {
    assert(type == TABLE_SCAN);
    return u.table_scan;
  }
  auto &sample_scan() {
    assert(type == SAMPLE_SCAN);
    return u.sample_scan;
  }
  const auto &sample_scan() const {
    assert(type == SAMPLE_SCAN);
    return u.sample_scan;
  }
  auto &index_scan() {
    assert(type == INDEX_SCAN);
    return u.index_scan;
  }
  const auto &index_scan() const {
    assert(type == INDEX_SCAN);
    return u.index_scan;
  }
  auto &index_distance_scan() {
    assert(type == INDEX_DISTANCE_SCAN);
    return u.index_distance_scan;
  }
  const auto &index_distance_scan() const {
    assert(type == INDEX_DISTANCE_SCAN);
    return u.index_distance_scan;
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
  auto &index_merge() {
    assert(type == INDEX_MERGE);
    return u.index_merge;
  }
  const auto &index_merge() const {
    assert(type == INDEX_MERGE);
    return u.index_merge;
  }
  auto &rowid_intersection() {
    assert(type == ROWID_INTERSECTION);
    return u.rowid_intersection;
  }
  const auto &rowid_intersection() const {
    assert(type == ROWID_INTERSECTION);
    return u.rowid_intersection;
  }
  auto &rowid_union() {
    assert(type == ROWID_UNION);
    return u.rowid_union;
  }
  const auto &rowid_union() const {
    assert(type == ROWID_UNION);
    return u.rowid_union;
  }
  auto &index_skip_scan() {
    assert(type == INDEX_SKIP_SCAN);
    return u.index_skip_scan;
  }
  const auto &index_skip_scan() const {
    assert(type == INDEX_SKIP_SCAN);
    return u.index_skip_scan;
  }
  auto &group_index_skip_scan() {
    assert(type == GROUP_INDEX_SKIP_SCAN);
    return u.group_index_skip_scan;
  }
  const auto &group_index_skip_scan() const {
    assert(type == GROUP_INDEX_SKIP_SCAN);
    return u.group_index_skip_scan;
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
  auto &window() {
    assert(type == WINDOW);
    return u.window;
  }
  const auto &window() const {
    assert(type == WINDOW);
    return u.window;
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
  auto &remove_duplicates_on_index() {
    assert(type == REMOVE_DUPLICATES_ON_INDEX);
    return u.remove_duplicates_on_index;
  }
  const auto &remove_duplicates_on_index() const {
    assert(type == REMOVE_DUPLICATES_ON_INDEX);
    return u.remove_duplicates_on_index;
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
  auto &delete_rows() {
    assert(type == DELETE_ROWS);
    return u.delete_rows;
  }
  const auto &delete_rows() const {
    assert(type == DELETE_ROWS);
    return u.delete_rows;
  }
  auto &update_rows() {
    assert(type == UPDATE_ROWS);
    return u.update_rows;
  }
  const auto &update_rows() const {
    assert(type == UPDATE_ROWS);
    return u.update_rows;
  }

  double num_output_rows() const { return m_num_output_rows; }

  void set_num_output_rows(double val) {
    assert(std::isfinite(val));
    assert(val == kUnknownRowCount || val >= 0.0);
    m_num_output_rows = val;
  }

 private:
  /// Expected number of output rows.
  double m_num_output_rows{kUnknownRowCount};

  /// Expected cost to read all of this access path once.
  double m_cost{kUnknownCost};

  /// Expected cost to initialize this access path; ie., cost to read
  /// k out of N rows would be init_cost + (k/N) * (cost - init_cost).
  /// Note that EXPLAIN prints out cost of reading the _first_ row
  /// because it is easier for the user and also easier to measure in
  /// EXPLAIN ANALYZE, but it is easier to do calculations with a pure
  /// initialization cost, so that is what we use in this member.
  /// kUnknownCost for unknown.
  double m_init_cost{kUnknownCost};

  /// Of init_cost, how much of the initialization needs only to be done
  /// once per query block. (This is a cost, not a proportion.)
  /// Ie., if the access path can reuse some its initialization work
  /// if Init() is called multiple times, this member will be nonzero.
  /// A typical example is a materialized table with rematerialize=false;
  /// the second time Init() is called, it's a no-op. Most paths will have
  /// init_once_cost = 0.0, ie., repeated scans will cost the same.
  /// We do not intend to use this field to model cache effects.
  ///
  /// This is currently not printed in EXPLAIN, only optimizer trace.
  double m_init_once_cost{0.0};

  /// If no filter, identical to cost.  init_cost is always the same
  /// (filters have zero initialization cost).
  double m_cost_before_filter{kUnknownCost};

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
      double sampling_percentage;
      enum tablesample_type sampling_type;
    } sample_scan;
    struct {
      TABLE *table;
      int idx;
      bool use_order;
      bool reverse;
    } index_scan;
    struct {
      TABLE *table;
      int idx;
      QUICK_RANGE *range;
      bool reverse;
    } index_distance_scan;
    struct {
      TABLE *table;
      Index_lookup *ref;
      bool use_order;
      bool reverse;
    } ref;
    struct {
      TABLE *table;
      Index_lookup *ref;
      bool use_order;
    } ref_or_null;
    struct {
      TABLE *table;
      Index_lookup *ref;
    } eq_ref;
    struct {
      TABLE *table;
      Index_lookup *ref;
      bool use_order;
      bool is_unique;
    } pushed_join_ref;
    struct {
      TABLE *table;
      Index_lookup *ref;
      bool use_order;
      bool use_limit;
      Item_func_match *ft_func;
    } full_text_search;
    struct {
      TABLE *table;
      Index_lookup *ref;
    } const_table;
    struct {
      TABLE *table;
      Index_lookup *ref;
      AccessPath *bka_path;
      int mrr_flags;
      bool keep_current_rowid;
    } mrr;
    struct {
      TABLE *table;
    } follow_tail;
    struct {
      // The key part(s) we are scanning on. Note that this may be an array.
      // You can get the table we are working on by looking into
      // used_key_parts[0].field->table (it is not stored directly, to avoid
      // going over the AccessPath size limits).
      KEY_PART *used_key_part;

      // The actual ranges we are scanning over (originally derived from “key”).
      // Not a Bounds_checked_array, to save 4 bytes on the length.
      QUICK_RANGE **ranges;
      unsigned num_ranges;

      unsigned mrr_flags;
      unsigned mrr_buf_size;

      // Which index (in the TABLE) we are scanning over, and how many of its
      // key parts we are using.
      unsigned index;
      unsigned num_used_key_parts;

      // If true, the scan can return rows in rowid order.
      bool can_be_used_for_ror : 1;

      // If true, the scan _should_ return rows in rowid order.
      // Should only be set if can_be_used_for_ror == true.
      bool need_rows_in_rowid_order : 1;

      // If true, this plan can be used for index merge scan.
      bool can_be_used_for_imerge : 1;

      // See row intersection for more details.
      bool reuse_handler : 1;

      // Whether we are scanning over a geometry key part.
      bool geometry : 1;

      // Whether we need a reverse scan. Only supported if geometry == false.
      bool reverse : 1;

      // For a reverse scan, if we are using extended key parts. It is needed,
      // to set correct flags when retrieving records.
      bool using_extended_key_parts : 1;
    } index_range_scan;
    struct {
      TABLE *table;
      bool forced_by_hint;
      bool allow_clustered_primary_key_scan;
      Mem_root_array<AccessPath *> *children;
    } index_merge;
    struct {
      TABLE *table;
      Mem_root_array<AccessPath *> *children;

      // Clustered primary key scan, if any.
      AccessPath *cpk_child;

      bool forced_by_hint;
      bool retrieve_full_rows;
      bool need_rows_in_rowid_order;

      // If true, the first child scan should reuse table->file instead of
      // creating its own. This is true if the intersection is the topmost
      // range scan, but _not_ if it's below a union. (The reasons for this
      // are unknown.) It can also be negated by logic involving
      // retrieve_full_rows and is_covering, again for unknown reasons.
      //
      // This is not only for performance; multi-table delete has a hidden
      // dependency on this behavior when running against certain types of
      // tables (e.g. MyISAM), as it assumes table->file is correctly positioned
      // when deleting (and not all table types can transfer the position of one
      // handler to another by using position()).
      bool reuse_handler;

      // true if no row retrieval phase is necessary.
      bool is_covering;
    } rowid_intersection;
    struct {
      TABLE *table;
      Mem_root_array<AccessPath *> *children;
      bool forced_by_hint;
    } rowid_union;
    struct {
      TABLE *table;
      unsigned index;
      unsigned num_used_key_parts;
      bool forced_by_hint;

      // Large, and has nontrivial destructors, so split out into
      // its own allocation.
      IndexSkipScanParameters *param;
    } index_skip_scan;
    struct {
      TABLE *table;
      unsigned index;
      unsigned num_used_key_parts;
      bool forced_by_hint;

      // Large, so split out into its own allocation.
      GroupIndexSkipScanParameters *param;
    } group_index_skip_scan;
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
      Mem_root_array<Item_values_column *> *output_refs;
    } table_value_constructor;
    struct {
      // No members.
    } fake_single_row;
    struct {
      // The child is optional. It is only used for keeping track of which
      // tables are pruned away by this path, and it is only needed when this
      // path is on the inner side of an outer join. See ZeroRowsIterator for
      // details. The child of a ZERO_ROWS access path will not be visited by
      // WalkAccessPaths(). It will be visited by WalkTablesUnderAccessPath()
      // only if called with include_pruned_tables = true. No iterator is
      // created for the child, and the child is not shown by EXPLAIN.
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
      bool rewrite_semi_to_inner;
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
      JoinType join_type;  // Somewhat redundant wrt. join_predicate.
      bool pfs_batch_mode;
      bool already_expanded_predicates;
      const JoinPredicate *join_predicate;

      // Equijoin filters to apply before the join, if any.
      // Indexes into join_predicate->expr->equijoin_conditions.
      // Non-equijoin conditions are always applied.
      // If already_expanded_predicates is true, do not re-expand.
      OverflowBitset equijoin_predicates;

      // NOTE: Due to the nontrivial constructor on equijoin_predicates,
      // this struct needs an initializer, or the union would not be
      // default-constructible. If we need more than one union member
      // with such an initializer, we would probably need to change
      // equijoin_predicates into a uint64_t type-punned to an OverflowBitset.
    } nested_loop_join = {nullptr, nullptr, JoinType::INNER, false, false,
                          nullptr, {}};
    struct {
      AccessPath *outer, *inner;
      const TABLE *table;
      KEY *key;
      size_t key_len;
    } nested_loop_semijoin_with_duplicate_removal;

    struct {
      AccessPath *child;
      Item *condition;

      // This parameter, unlike nearly all others, is not passed to the the
      // actual iterator. Instead, if true, it signifies that when creating
      // the iterator, all materializable subqueries in “condition” should be
      // materialized (with any in2exists condition removed first). In the
      // very rare case that there are two or more such subqueries, this is
      // an all-or-nothing decision, for simplicity.
      //
      // See FinalizeMaterializedSubqueries().
      bool materialize_subqueries;
    } filter;
    struct {
      AccessPath *child;
      Filesort *filesort;
      table_map tables_to_get_rowid_for;

      // If filesort is nullptr: A new filesort will be created at the
      // end of optimization, using this order and flags. Otherwise: Only
      // used by EXPLAIN.
      ORDER *order;
      ha_rows limit;
      bool remove_duplicates;
      bool unwrap_rollup;
      bool force_sort_rowids;
    } sort;
    struct {
      AccessPath *child;
      olap_type olap;
    } aggregate;
    struct {
      AccessPath *subquery_path;
      JOIN *join;
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
      // See Query_expression::send_records.
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
      // NOTE: The only legal access paths within table_path are
      // TABLE_SCAN, REF, REF_OR_NULL, EQ_REF, ALTERNATIVE,
      // CONST_TABLE (somewhat nonsensical), INDEX_SCAN and DYNAMIC_INDEX_SCAN
      AccessPath *table_path;

      // Large, and has nontrivial destructors, so split out
      // into its own allocation.
      MaterializePathParameters *param;
      /** The total cost of executing the queries that we materialize.*/
      double subquery_cost;
      /// The number of materialized rows (as opposed to the number of rows
      /// fetched by table_path). Needed for 'explain'.
      double subquery_rows;
    } materialize;
    struct {
      AccessPath *table_path;
      Table_ref *table_list;
      Item *condition;
    } materialize_information_schema_table;
    struct {
      Mem_root_array<AppendPathParameters> *children;
    } append;
    struct {
      AccessPath *child;
      Window *window;
      TABLE *temp_table;
      Temp_table_param *temp_table_param;
      int ref_slice;
      bool needs_buffering;
    } window;
    struct {
      AccessPath *child;
      SJ_TMP_TABLE *weedout_table;
      table_map tables_to_get_rowid_for;
    } weedout;
    struct {
      AccessPath *child;
      Item **group_items;
      int group_items_size;
    } remove_duplicates;
    struct {
      AccessPath *child;
      TABLE *table;
      KEY *key;
      unsigned loosescan_key_len;
    } remove_duplicates_on_index;
    struct {
      AccessPath *table_scan_path;

      // For the ref.
      AccessPath *child;
      Index_lookup *used_ref;
    } alternative;
    struct {
      AccessPath *child;
      const char *name;
    } cache_invalidator;
    struct {
      AccessPath *child;
      table_map tables_to_delete_from;
      table_map immediate_tables;
    } delete_rows;
    struct {
      AccessPath *child;
      table_map tables_to_update;
      table_map immediate_tables;
    } update_rows;
  } u;
};
static_assert(std::is_trivially_destructible<AccessPath>::value,
              "AccessPath must be trivially destructible, as it is allocated "
              "on the MEM_ROOT and not wrapped in unique_ptr_destroy_only"
              "(because multiple candidates during planning could point to "
              "the same access paths, and refcounting would be expensive)");
static_assert(sizeof(AccessPath) <= 144,
              "We are creating a lot of access paths in the join "
              "optimizer, so be sure not to bloat it without noticing. "
              "(96 bytes for the base, 48 bytes for the variant.)");

inline void CopyBasicProperties(const AccessPath &from, AccessPath *to) {
  to->set_num_output_rows(from.num_output_rows());
  to->set_cost(from.cost());
  to->set_init_cost(from.init_cost());
  to->set_init_once_cost(from.init_once_cost());
  to->parameter_tables = from.parameter_tables;
  to->safe_for_rowid = from.safe_for_rowid;
  to->ordering_state = from.ordering_state;
  to->has_group_skip_scan = from.has_group_skip_scan;
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

inline AccessPath *NewSampleScanAccessPath(THD *thd, TABLE *table,
                                           double sampling_percentage,
                                           bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::SAMPLE_SCAN;
  path->count_examined_rows = count_examined_rows;
  path->sample_scan().table = table;
  path->sample_scan().sampling_percentage = sampling_percentage;
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

inline AccessPath *NewRefAccessPath(THD *thd, TABLE *table, Index_lookup *ref,
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
                                          Index_lookup *ref, bool use_order,
                                          bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::REF_OR_NULL;
  path->count_examined_rows = count_examined_rows;
  path->ref_or_null().table = table;
  path->ref_or_null().ref = ref;
  path->ref_or_null().use_order = use_order;
  return path;
}

inline AccessPath *NewEQRefAccessPath(THD *thd, TABLE *table, Index_lookup *ref,
                                      bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::EQ_REF;
  path->count_examined_rows = count_examined_rows;
  path->eq_ref().table = table;
  path->eq_ref().ref = ref;
  return path;
}

inline AccessPath *NewPushedJoinRefAccessPath(THD *thd, TABLE *table,
                                              Index_lookup *ref, bool use_order,
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
                                               Index_lookup *ref,
                                               Item_func_match *ft_func,
                                               bool use_order, bool use_limit,
                                               bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::FULL_TEXT_SEARCH;
  path->count_examined_rows = count_examined_rows;
  path->full_text_search().table = table;
  path->full_text_search().ref = ref;
  path->full_text_search().use_order = use_order;
  path->full_text_search().use_limit = use_limit;
  path->full_text_search().ft_func = ft_func;
  return path;
}

inline AccessPath *NewConstTableAccessPath(THD *thd, TABLE *table,
                                           Index_lookup *ref,
                                           bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::CONST_TABLE;
  path->count_examined_rows = count_examined_rows;
  path->set_num_output_rows(1.0);
  path->set_cost(0.0);
  path->set_init_cost(0.0);
  path->set_init_once_cost(0.0);
  path->const_table().table = table;
  path->const_table().ref = ref;
  return path;
}

inline AccessPath *NewMRRAccessPath(THD *thd, TABLE *table, Index_lookup *ref,
                                    int mrr_flags) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MRR;
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

AccessPath *NewTableValueConstructorAccessPath(const THD *thd,
                                               const JOIN *join);

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
  path->filter().materialize_subqueries = false;
  path->has_group_skip_scan = child->has_group_skip_scan;
  return path;
}

// Not inline, because it needs access to filesort internals
// (which are forward-declared in this file).
AccessPath *NewSortAccessPath(THD *thd, AccessPath *child, Filesort *filesort,
                              ORDER *order, bool count_examined_rows);

inline AccessPath *NewAggregateAccessPath(THD *thd, AccessPath *child,
                                          olap_type olap) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::AGGREGATE;
  path->aggregate().child = child;
  path->aggregate().olap = olap;
  path->has_group_skip_scan = child->has_group_skip_scan;
  return path;
}

inline AccessPath *NewTemptableAggregateAccessPath(
    THD *thd, AccessPath *subquery_path, JOIN *join,
    Temp_table_param *temp_table_param, TABLE *table, AccessPath *table_path,
    int ref_slice) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::TEMPTABLE_AGGREGATE;
  path->temptable_aggregate().subquery_path = subquery_path;
  path->temptable_aggregate().join = join;
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
  void EstimateLimitOffsetCost(AccessPath * path);
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::LIMIT_OFFSET;
  path->immediate_update_delete_table = child->immediate_update_delete_table;
  path->limit_offset().child = child;
  path->limit_offset().limit = limit;
  path->limit_offset().offset = offset;
  path->limit_offset().count_all_rows = count_all_rows;
  path->limit_offset().reject_multiple_rows = reject_multiple_rows;
  path->limit_offset().send_records_override = send_records_override;
  path->ordering_state = child->ordering_state;
  path->has_group_skip_scan = child->has_group_skip_scan;
  EstimateLimitOffsetCost(path);
  return path;
}

inline AccessPath *NewFakeSingleRowAccessPath(THD *thd,
                                              bool count_examined_rows) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::FAKE_SINGLE_ROW;
  path->count_examined_rows = count_examined_rows;
  path->set_num_output_rows(1.0);
  path->set_cost(0.0);
  path->set_init_cost(0.0);
  path->set_init_once_cost(0.0);
  return path;
}

inline AccessPath *NewZeroRowsAccessPath(THD *thd, AccessPath *child,
                                         const char *cause) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::ZERO_ROWS;
  path->zero_rows().child = child;
  path->zero_rows().cause = cause;
  path->set_num_output_rows(0.0);
  path->set_cost(0.0);
  path->set_init_cost(0.0);
  path->set_init_once_cost(0.0);
  path->num_output_rows_before_filter = 0.0;
  path->set_cost_before_filter(0.0);
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
  path->set_num_output_rows(1.0);
  path->set_cost(0.0);
  path->set_init_cost(0.0);
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
  path->has_group_skip_scan = child->has_group_skip_scan;
  return path;
}

inline Mem_root_array<MaterializePathParameters::Operand>
SingleMaterializeQueryBlock(THD *thd, AccessPath *path, int select_number,
                            JOIN *join, bool copy_items,
                            Temp_table_param *temp_table_param) {
  assert(path != nullptr);
  Mem_root_array<MaterializePathParameters::Operand> array(thd->mem_root, 1);
  MaterializePathParameters::Operand &operand = array[0];
  operand.subquery_path = path;
  operand.select_number = select_number;
  operand.join = join;
  operand.disable_deduplication_by_hash_field = false;
  operand.copy_items = copy_items;
  operand.temp_table_param = temp_table_param;
  return array;
}

inline AccessPath *NewMaterializeAccessPath(
    THD *thd, Mem_root_array<MaterializePathParameters::Operand> operands,
    Mem_root_array<const AccessPath *> *invalidators, TABLE *table,
    AccessPath *table_path, Common_table_expr *cte, Query_expression *unit,
    int ref_slice, bool rematerialize, ha_rows limit_rows,
    bool reject_multiple_rows,
    MaterializePathParameters::DedupType dedup_reason =
        MaterializePathParameters::NO_DEDUP) {
  MaterializePathParameters *param =
      new (thd->mem_root) MaterializePathParameters;
  param->m_operands = std::move(operands);
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
  param->limit_rows = (table == nullptr || table->is_union_or_table()
                           ? limit_rows
                           :
                           // INTERSECT, EXCEPT: Enforced by TableScanIterator,
                           // see its constructor
                           HA_POS_ERROR);
  param->reject_multiple_rows = reject_multiple_rows;
  param->deduplication_reason = dedup_reason;

#ifndef NDEBUG
  for (MaterializePathParameters::Operand &operand : param->m_operands) {
    assert(operand.subquery_path != nullptr);
  }
#endif

  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MATERIALIZE;
  path->materialize().table_path = table_path;
  path->materialize().param = param;
  path->materialize().subquery_cost = kUnknownCost;
  path->materialize().subquery_rows = kUnknownRowCount;
  if (rematerialize) {
    path->safe_for_rowid = AccessPath::SAFE_IF_SCANNED_ONCE;
  } else {
    // The default; this is just to be explicit in the code.
    path->safe_for_rowid = AccessPath::SAFE;
  }
  return path;
}

inline AccessPath *NewMaterializeInformationSchemaTableAccessPath(
    THD *thd, AccessPath *table_path, Table_ref *table_list, Item *condition) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE;
  path->materialize_information_schema_table().table_path = table_path;
  path->materialize_information_schema_table().table_list = table_list;
  path->materialize_information_schema_table().condition = condition;
  return path;
}

/// Add path costs c1 and c2, but handle kUnknownCost correctly.
inline double AddCost(double c1, double c2) {
  // If one is undefined, use the other, as we have nothing else.
  if (c1 == kUnknownCost) {
    return c2;
  } else if (c2 == kUnknownCost) {
    return c1;
  } else {
    return c1 + c2;
  }
}

/// Add row counts c1 and c2, but handle kUnknownRowCount correctly.
inline double AddRowCount(double c1, double c2) {
  // If one is undefined, use the other, as we have nothing else.
  if (c1 == kUnknownRowCount) {
    return c2;
  } else if (c2 == kUnknownRowCount) {
    return c1;
  } else {
    return c1 + c2;
  }
}

// The Mem_root_array must be allocated on a MEM_ROOT that lives at least for as
// long as the access path.
inline AccessPath *NewAppendAccessPath(
    THD *thd, Mem_root_array<AppendPathParameters> *children) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::APPEND;
  path->append().children = children;
  double num_output_rows = kUnknownRowCount;
  for (const AppendPathParameters &child : *children) {
    path->set_cost(AddCost(path->cost(), child.path->cost()));
    path->set_init_cost(AddCost(path->init_cost(), child.path->init_cost()));
    path->set_init_once_cost(path->init_once_cost() +
                             child.path->init_once_cost());
    num_output_rows =
        AddRowCount(num_output_rows, child.path->num_output_rows());
  }
  path->set_num_output_rows(num_output_rows);
  return path;
}

inline AccessPath *NewWindowAccessPath(THD *thd, AccessPath *child,
                                       Window *window,
                                       Temp_table_param *temp_table_param,
                                       int ref_slice, bool needs_buffering) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::WINDOW;
  path->window().child = child;
  path->window().window = window;
  path->window().temp_table = nullptr;
  path->window().temp_table_param = temp_table_param;
  path->window().ref_slice = ref_slice;
  path->window().needs_buffering = needs_buffering;
  path->set_num_output_rows(child->num_output_rows());
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
                                                 Item **group_items,
                                                 int group_items_size) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::REMOVE_DUPLICATES;
  path->remove_duplicates().child = child;
  path->remove_duplicates().group_items = group_items;
  path->remove_duplicates().group_items_size = group_items_size;
  path->has_group_skip_scan = child->has_group_skip_scan;
  return path;
}

inline AccessPath *NewRemoveDuplicatesOnIndexAccessPath(
    THD *thd, AccessPath *child, TABLE *table, KEY *key,
    unsigned loosescan_key_len) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::REMOVE_DUPLICATES_ON_INDEX;
  path->remove_duplicates_on_index().child = child;
  path->remove_duplicates_on_index().table = table;
  path->remove_duplicates_on_index().key = key;
  path->remove_duplicates_on_index().loosescan_key_len = loosescan_key_len;
  path->has_group_skip_scan = child->has_group_skip_scan;
  return path;
}

inline AccessPath *NewAlternativeAccessPath(THD *thd, AccessPath *child,
                                            AccessPath *table_scan_path,
                                            Index_lookup *used_ref) {
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

AccessPath *NewDeleteRowsAccessPath(THD *thd, AccessPath *child,
                                    table_map delete_tables,
                                    table_map immediate_tables);

AccessPath *NewUpdateRowsAccessPath(THD *thd, AccessPath *child,
                                    table_map delete_tables,
                                    table_map immediate_tables);

/**
  Modifies "path" and the paths below it so that they provide row IDs for
  all tables.

  This also figures out how the row IDs should be retrieved for each table in
  the input to the path. If the handler of the table is positioned on the
  correct row while reading the input, handler::position() can be called to get
  the row ID from the handler. However, if the input iterator returns rows
  without keeping the position of the underlying handlers in sync, calling
  handler::position() will not be able to provide the row IDs. Specifically,
  hash join and BKA join do not keep the underlying handlers positioned on the
  right row. Therefore, this function will instruct every hash join or BKA join
  below "path" to maintain row IDs in the join buffer, and updating handler::ref
  in every input table for each row they return. Then "path" does not need to
  call handler::position() to get it (nor should it, since calling it would
  overwrite the correct row ID with a stale one).

  The tables on which "path" should call handler::position() are stored in a
  `tables_to_get_rowid_for` bitset in "path". For all the other tables, it can
  assume that handler::ref already contains the correct row ID.
 */
void FindTablesToGetRowidFor(AccessPath *path);

unique_ptr_destroy_only<RowIterator> CreateIteratorFromAccessPath(
    THD *thd, MEM_ROOT *mem_root, AccessPath *path, JOIN *join,
    bool eligible_for_batch_mode);

// A short form of CreateIteratorFromAccessPath() that implicitly uses the THD's
// MEM_ROOT for storage, which is nearly always what you want. (The only caller
// that does anything else is DynamicRangeIterator.)
inline unique_ptr_destroy_only<RowIterator> CreateIteratorFromAccessPath(
    THD *thd, AccessPath *path, JOIN *join, bool eligible_for_batch_mode) {
  return CreateIteratorFromAccessPath(thd, thd->mem_root, path, join,
                                      eligible_for_batch_mode);
}

void SetCostOnTableAccessPath(const Cost_model_server &cost_model,
                              const POSITION *pos, bool is_after_filter,
                              AccessPath *path);

/**
  Return the TABLE* referred from 'path' if it is a basic access path,
  else a nullptr is returned. Temporary tables, such as those used by
  sorting, aggregate and subquery materialization are not returned.
*/
TABLE *GetBasicTable(const AccessPath *path);

/**
  Returns a map of all tables read when `path` or any of its children are
  executed. Only iterators that are part of the same query block as `path`
  are considered.

  If a table is read that doesn't have a map, specifically the temporary
  tables made as part of materialization within the same query block,
  RAND_TABLE_BIT will be set as a convention and none of that access path's
  children will be included in the map. In this case, the caller will need to
  manually go in and find said access path, to ask it for its TABLE object.

  If include_pruned_tables = true, tables that are hidden under a ZERO_ROWS
  access path (ie., pruned away due to impossible join conditions) will be
  included in the map. This is normally what you want, as those tables need to
  be included whenever you store NULL flags and the likes, but if you don't
  want them (perhaps to specifically check for conditions referring to pruned
  tables), you can set it to false.
 */
table_map GetUsedTableMap(const AccessPath *path, bool include_pruned_tables);

/**
  Find the list of all tables used by this root, stopping at materializations.
  Used for knowing which tables to sort.
 */
Mem_root_array<TABLE *> CollectTables(THD *thd, AccessPath *root_path);

/**
  For each access path in the (sub)tree rooted at “path”, expand any use of
  “filter_predicates” into newly-inserted FILTER access paths, using the given
  predicate list. This is used after finding an optimal set of access paths,
  to normalize the tree so that the remaining consumers do not need to worry
  about filter_predicates and cost_before_filter.

  “join” is the join that “path” is part of.
 */
void ExpandFilterAccessPaths(THD *thd, AccessPath *path, const JOIN *join,
                             const Mem_root_array<Predicate> &predicates,
                             unsigned num_where_predicates);

/**
  Extracts the Item expression from the given “filter_predicates” corresponding
  to the given “mask”.
 */
Item *ConditionFromFilterPredicates(const Mem_root_array<Predicate> &predicates,
                                    OverflowBitset mask,
                                    int num_where_predicates);

/// Like ExpandFilterAccessPaths(), but expands only the single access path
/// at “path”.
void ExpandSingleFilterAccessPath(THD *thd, AccessPath *path, const JOIN *join,
                                  const Mem_root_array<Predicate> &predicates,
                                  unsigned num_where_predicates);

/// Returns the tables that are part of a hash join.
table_map GetHashJoinTables(AccessPath *path);

/**
  Get the conditions to put into the extra conditions of the HashJoinIterator.
  This includes the non-equijoin conditions, as well as any equijoin conditions
  on columns that are too big to include in the hash table. (The old optimizer
  handles equijoin conditions on long columns elsewhere, so the last part only
  applies to the hypergraph optimizer.)

  @param mem_root The root on which to allocate memory, if needed.
  @param using_hypergraph_optimizer True if using the hypergraph optimizer.
  @param equijoin_conditions All the equijoin conditions of the join.
  @param other_conditions All the non-equijoin conditions of the join.

  @return All the conditions to evaluate as "extra conditions" in
  HashJoinIterator, or nullptr on OOM.
 */
const Mem_root_array<Item *> *GetExtraHashJoinConditions(
    MEM_ROOT *mem_root, bool using_hypergraph_optimizer,
    const std::vector<HashJoinCondition> &equijoin_conditions,
    const Mem_root_array<Item *> &other_conditions);

/**
  Update status variables which count how many scans of various types are used
  in a query plan.

  The following status variables are updated: Select_scan, Select_full_join,
  Select_range, Select_full_range_join, Select_range_check. They are also stored
  as performance schema statement events with the same names.

  In addition, the performance schema statement events NO_INDEX_USED and
  NO_GOOD_INDEX_USED are updated, if appropriate.
 */
void CollectStatusVariables(THD *thd, const JOIN *top_join,
                            const AccessPath &top_path);

#endif  // SQL_JOIN_OPTIMIZER_ACCESS_PATH_H
