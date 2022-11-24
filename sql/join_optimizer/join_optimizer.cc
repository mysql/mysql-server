/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/join_optimizer.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <algorithm>
#include <bitset>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ft_global.h"
#include "map_helpers.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "scope_guard.h"
#include "sql/field.h"
#include "sql/filesort.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/build_interesting_orders.h"
#include "sql/join_optimizer/compare_access_paths.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/estimate_selectivity.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/join_optimizer/find_contained_subqueries.h"
#include "sql/join_optimizer/graph_simplification.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/interesting_orders.h"
#include "sql/join_optimizer/interesting_orders_defs.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/node_map.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/secondary_engine_costing_flags.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/join_type.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/opt_costmodel.h"
#include "sql/parse_tree_node_base.h"
#include "sql/partition_info.h"
#include "sql/query_options.h"
#include "sql/range_optimizer/index_range_scan_plan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_analysis.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_array.h"
#include "sql/sql_base.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_partition.h"
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_function.h"
#include "sql/temp_table_param.h"
#include "sql/uniques.h"
#include "sql/window.h"
#include "template_utils.h"

using hypergraph::Hyperedge;
using hypergraph::Node;
using hypergraph::NodeMap;
using std::find_if;
using std::min;
using std::pair;
using std::string;
using std::swap;
using std::vector;

namespace {

string PrintAccessPath(const AccessPath &path, const JoinHypergraph &graph,
                       const char *description_for_trace);
void PrintJoinOrder(const AccessPath *path, string *join_order);

AccessPath *CreateMaterializationPath(THD *thd, JOIN *join, AccessPath *path,
                                      TABLE *temp_table,
                                      Temp_table_param *temp_table_param,
                                      bool copy_items);

AccessPath *GetSafePathToSort(THD *thd, JOIN *join, AccessPath *path,
                              bool need_rowid);

/**
  CostingReceiver contains the main join planning logic, selecting access paths
  based on cost. It receives subplans from DPhyp (see enumerate_subgraph.h),
  assigns them costs based on a cost model, and keeps the ones that are
  cheapest. In the end, this means it will be left with a root access path that
  gives the lowest total cost for joining the tables in the query block, ie.,
  without ORDER BY etc.

  Currently, besides the expected number of produced rows (which is the same no
  matter how we access the table) we keep only a single value per subplan
  (total cost), and thus also only a single best access path. In the future,
  we will have more dimensions to worry about, such as initial cost versus total
  cost (relevant for LIMIT), ordering properties, and so on. At that point,
  there is not necessarily a single “best” access path anymore, and we will need
  to keep multiple ones around, and test all of them as candidates when building
  larger subplans.
 */
class CostingReceiver {
 public:
  CostingReceiver(
      THD *thd, Query_block *query_block, JoinHypergraph &graph,
      const LogicalOrderings *orderings,
      const Mem_root_array<SortAheadOrdering> *sort_ahead_orderings,
      const Mem_root_array<ActiveIndexInfo> *active_indexes,
      const Mem_root_array<FullTextIndexInfo> *fulltext_searches,
      NodeMap fulltext_tables, uint64_t sargable_fulltext_predicates,
      table_map update_delete_target_tables,
      table_map immediate_update_delete_candidates, bool need_rowid,
      SecondaryEngineFlags engine_flags, int subgraph_pair_limit,
      secondary_engine_modify_access_path_cost_t secondary_engine_cost_hook,
      string *trace)
      : m_thd(thd),
        m_query_block(query_block),
        m_access_paths(thd->mem_root),
        m_graph(&graph),
        m_orderings(orderings),
        m_sort_ahead_orderings(sort_ahead_orderings),
        m_active_indexes(active_indexes),
        m_fulltext_searches(fulltext_searches),
        m_fulltext_tables(fulltext_tables),
        m_sargable_fulltext_predicates(sargable_fulltext_predicates),
        m_update_delete_target_nodes(GetNodeMapFromTableMap(
            update_delete_target_tables, graph.table_num_to_node_num)),
        m_immediate_update_delete_candidates(GetNodeMapFromTableMap(
            immediate_update_delete_candidates, graph.table_num_to_node_num)),
        m_need_rowid(need_rowid),
        m_engine_flags(engine_flags),
        m_subgraph_pair_limit(subgraph_pair_limit),
        m_secondary_engine_cost_hook(secondary_engine_cost_hook),
        m_trace(trace) {
    // At least one join type must be supported.
    assert(Overlaps(engine_flags,
                    MakeSecondaryEngineFlags(
                        SecondaryEngineFlag::SUPPORTS_HASH_JOIN,
                        SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN)));
  }

  // Not copyable, but movable so that we can reset it after graph
  // simplification if needed.
  CostingReceiver &operator=(const CostingReceiver &) = delete;
  CostingReceiver &operator=(CostingReceiver &&) = default;
  CostingReceiver(const CostingReceiver &) = delete;
  CostingReceiver(CostingReceiver &&) = default;

  bool HasSeen(NodeMap subgraph) const {
    return m_access_paths.count(subgraph) != 0;
  }

  bool FoundSingleNode(int node_idx);

  // Called EmitCsgCmp() in the DPhyp paper.
  bool FoundSubgraphPair(NodeMap left, NodeMap right, int edge_idx);

  const Prealloced_array<AccessPath *, 4> &root_candidates() {
    const auto it =
        m_access_paths.find(TablesBetween(0, m_graph->nodes.size()));
    assert(it != m_access_paths.end());
    return it->second.paths;
  }

  FunctionalDependencySet active_fds_at_root() const {
    const auto it =
        m_access_paths.find(TablesBetween(0, m_graph->nodes.size()));
    assert(it != m_access_paths.end());
    return it->second.active_functional_dependencies;
  }

  size_t num_subplans() const { return m_access_paths.size(); }

  size_t num_access_paths() const {
    size_t access_paths = 0;
    for (const auto &[nodes, pathset] : m_access_paths) {
      access_paths += pathset.paths.size();
    }
    return access_paths;
  }

  /// True if the result of the join is found to be always empty, typically
  /// because of an impossible WHERE clause.
  bool always_empty() const {
    const auto it =
        m_access_paths.find(TablesBetween(0, m_graph->nodes.size()));
    return it != m_access_paths.end() && it->second.always_empty;
  }

  AccessPath *ProposeAccessPath(
      AccessPath *path, Prealloced_array<AccessPath *, 4> *existing_paths,
      OrderingSet obsolete_orderings, const char *description_for_trace) const;

  bool HasSecondaryEngineCostHook() const {
    return m_secondary_engine_cost_hook != nullptr;
  }

 private:
  THD *m_thd;

  /// The query block we are planning.
  Query_block *m_query_block;

  /**
    Besides the access paths for a set of nodes (see m_access_paths),
    AccessPathSet contains information that is common between all access
    paths for that set. One would believe num_output_rows would be such
    a member (a set of tables should produce the same number of output
    rows no matter the join order), but due to parameterized paths,
    different access paths could have different outputs. delayed_predicates
    is another, but currently, it's already efficiently hidden space-wise
    due to the use of a union.
   */
  struct AccessPathSet {
    Prealloced_array<AccessPath *, 4> paths;
    FunctionalDependencySet active_functional_dependencies{0};

    // Once-interesting orderings that we don't care about anymore,
    // e.g. because they were interesting for a semijoin but that semijoin
    // is now done (with or without using the ordering). This reduces
    // the number of access paths we have to keep in play, since they are
    // de-facto equivalent.
    //
    // Note that if orderings were merged, this could falsely prune out
    // orderings that we would actually need, but as long as all of the
    // relevant ones are semijoin orderings (which are never identical,
    // and never merged with the relevant-at-end orderings), this
    // should not happen.
    OrderingSet obsolete_orderings{0};

    // True if the join of the tables in this set has been found to be always
    // empty (typically because of an impossible WHERE clause).
    bool always_empty{false};
  };

  /**
    For each subset of tables that are connected in the join hypergraph,
    keeps the current best access paths for producing said subset.
    There can be several that are best in different ways; see comments
    on ProposeAccessPath().

    Also used for communicating connectivity information back to DPhyp
    (in HasSeen()); if there's an entry here, that subset will induce
    a connected subgraph of the join hypergraph.
   */
  mem_root_unordered_map<NodeMap, AccessPathSet> m_access_paths;

  /**
    How many subgraph pairs we've seen so far. Used to give up
    if we end up allocating too many resources (prompting us to
    create a simpler join graph and try again).
   */
  int m_num_seen_subgraph_pairs = 0;

  /// The graph we are running over.
  JoinHypergraph *m_graph;

  /// Whether we have applied clamping due to a multi-column EQ_REF at any
  /// point. There is a known issue (see bug #33550360) where this can cause
  /// row count estimates to be inconsistent between different access paths.
  /// Obviously, we should fix this bug by adjusting the selectivities
  /// (and we do for single-column indexes), but for multipart indexes,
  /// this is nontrivial. See the bug for details on some ideas, but the
  /// gist of it is that we probably will want a linear program to adjust
  /// multi-selectivities so that they are consistent, and not above 1/N
  /// (for N-row tables) if there are unique indexes on them.
  ///
  /// The only reason why we collect this information, like
  /// JoinHypergraph::has_reordered_left_joins, is to be able to assert
  /// on inconsistent row counts between APs, excluding this (known) issue.
  bool has_clamped_multipart_eq_ref = false;

  /// Whether we have a semijoin where the inner child is parameterized on the
  /// outer child, and the row estimate of the inner child is possibly clamped,
  /// for example because of some other semijoin. In this case, we may see
  /// inconsistent row count estimates between the ordinary semijoin plan and
  /// the rewrite_semi_to_inner plan, because it's hard to tell how much the
  /// already-applied-as-sargable selectivity affected the row count estimate of
  /// the child.
  ///
  /// The only reason why we collect this information, is to be able to assert
  /// on inconsistent row counts between access paths, excluding this known
  /// issue.
  bool has_semijoin_with_possibly_clamped_child = false;

  /// Keeps track of interesting orderings in this query block.
  /// See LogicalOrderings for more information.
  const LogicalOrderings *m_orderings;

  /// List of all orderings that are candidates for sort-ahead
  /// (because they are, or may eventually become, an interesting ordering).
  const Mem_root_array<SortAheadOrdering> *m_sort_ahead_orderings;

  /// List of all indexes that are active and that we can apply in this query.
  /// Indexes can be useful in several ways: We can use them for ref access,
  /// for index-only scans, or to get interesting orderings.
  const Mem_root_array<ActiveIndexInfo> *m_active_indexes;

  /// List of all active full-text indexes that we can apply in this query.
  const Mem_root_array<FullTextIndexInfo> *m_fulltext_searches;

  /// A map of tables that are referenced by a MATCH function (those tables that
  /// have Table_ref::is_fulltext_searched() == true). It is used for
  /// preventing hash joins involving tables that are full-text searched.
  NodeMap m_fulltext_tables = 0;

  /// The set of WHERE predicates which are on a form that can be satisfied by a
  /// full-text index scan. This includes calls to MATCH with no comparison
  /// operator, and predicates on the form MATCH > const or MATCH >= const
  /// (where const must be high enough to make the comparison return false for
  /// documents with zero score).
  uint64_t m_sargable_fulltext_predicates = 0;

  /// The target tables of an UPDATE or DELETE statement.
  NodeMap m_update_delete_target_nodes = 0;

  /// The set of tables that are candidates for immediate update or delete.
  /// Immediate update/delete means that the rows from the table are deleted
  /// while reading the rows from the topmost iterator. (As opposed to buffered
  /// update/delete, where the row IDs are stored in temporary tables, and only
  /// updated or deleted after the topmost iterator has been read to the end.)
  /// The candidates are those target tables that are only referenced once in
  /// the query. The requirement for immediate deletion is that the deleted row
  /// will not have to be read again later. Currently, at most one of the
  /// candidate tables is chosen, and it is always the outermost table in the
  /// join tree.
  NodeMap m_immediate_update_delete_candidates = 0;

  /// Whether we will be needing row IDs from our tables, typically for
  /// a later sort. If this happens, derived tables cannot use streaming,
  /// but need an actual materialization, since filesort expects to be
  /// able to go back and ask for a given row. (This is different from
  /// when we need row IDs for weedout, which doesn't preclude streaming.
  /// The hypergraph optimizer does not use weedout.)
  bool m_need_rowid;

  /// The flags declared by the secondary engine. In particular, it describes
  /// what kind of access path types should not be created.
  SecondaryEngineFlags m_engine_flags;

  /// The maximum number of pairs of subgraphs we are willing to accept,
  /// or -1 if no limit. If this limit gets hit, we stop traversing the graph
  /// and return an error; the caller will then have to modify the hypergraph
  /// (see GraphSimplifier) to make for a graph with fewer options, so that
  /// planning time will come under an acceptable limit.
  int m_subgraph_pair_limit;

  /// Pointer to a function that modifies the cost estimates of an access path
  /// for execution in a secondary storage engine, or nullptr otherwise.
  secondary_engine_modify_access_path_cost_t m_secondary_engine_cost_hook;

  /// If not nullptr, we store human-readable optimizer trace information here.
  string *m_trace;

  /// A map of tables that can never be on the right side of any join,
  /// ie., they have to be leftmost in the tree. This only affects recursive
  /// table references (ie., when WITH RECURSIVE is in use); they work by
  /// continuously tailing new records, which wouldn't work if we were to
  /// scan them multiple times or put them in a hash table. Naturally,
  /// there must be zero or one bit here; the common case is zero.
  NodeMap forced_leftmost_table = 0;

  /// A special MEM_ROOT for allocating OverflowBitsets that we might end up
  /// discarding, ie. for AccessPaths that do not yet live in m_access_paths.
  /// For any AccessPath that is to have a permanent life (ie., not be
  /// immediately discarded as inferior), the OverflowBitset _must_ be taken
  /// out of this MEM_ROOT and onto the regular one, as it is cleared often.
  /// (This significantly reduces the amount of memory used in situations
  /// where lots of AccessPaths are produced and discarded. Of course,
  /// it only matters for queries with >= 64 predicates.)
  ///
  /// The copying is using CommitBitsetsToHeap(). ProposeAccessPath() will
  /// automatically call CommitBitsetsToHeap() for accepted access paths,
  /// but it will not call it on any of their children. Thus, if you've
  /// added more than one AccessPath in the chain (e.g. if you add a join,
  /// then a sort of that join, and then propose the sort), you will need
  /// to make sure there are no stray bitsets left on this MEM_ROOT.
  ///
  /// Because this can be a source of subtle bugs, you should be conservative
  /// about what bitsets you put here; really, only the ones you could be
  /// allocating many of (like joins) should be candidates.
  MEM_ROOT m_overflow_bitset_mem_root;

  /// A special MEM_ROOT for temporary data for the range optimizer.
  /// It can be discarded immediately after we've decided on the range scans
  /// for a given table (but we reuse its memory as long as there are more
  /// tables left to scan).
  MEM_ROOT m_range_optimizer_mem_root;

  /// For trace use only.
  string PrintSet(NodeMap x) const {
    std::string ret = "{";
    bool first = true;
    for (size_t node_idx : BitsSetIn(x)) {
      if (!first) {
        ret += ",";
      }
      first = false;
      ret += m_graph->nodes[node_idx].table->alias;
    }
    return ret + "}";
  }

  /// For trace use only.
  string PrintSubgraphHeader(const JoinPredicate *edge,
                             const AccessPath &join_path, NodeMap left,
                             NodeMap right) const;

  /// Checks whether the given engine flag is active or not.
  bool SupportedEngineFlag(SecondaryEngineFlag flag) const {
    return Overlaps(m_engine_flags, MakeSecondaryEngineFlags(flag));
  }

  bool FindIndexRangeScans(int node_idx, bool *impossible,
                           double *num_output_rows_after_filter);
  void ProposeIndexMerge(TABLE *table, int node_idx, const SEL_IMERGE &imerge,
                         int pred_idx, bool inexact,
                         bool allow_clustered_primary_key_scan,
                         int num_where_predicates,
                         double num_output_rows_after_filter,
                         RANGE_OPT_PARAM *param,
                         bool *has_clustered_primary_key_scan);

  void TraceAccessPaths(NodeMap nodes);
  void ProposeAccessPathForBaseTable(int node_idx,
                                     double force_num_output_rows_after_filter,
                                     const char *description_for_trace,
                                     AccessPath *path);
  void ProposeAccessPathForIndex(int node_idx,
                                 OverflowBitset applied_predicates,
                                 OverflowBitset subsumed_predicates,
                                 double force_num_output_rows_after_filter,
                                 const char *description_for_trace,
                                 AccessPath *path);
  void ProposeAccessPathWithOrderings(NodeMap nodes,
                                      FunctionalDependencySet fd_set,
                                      OrderingSet obsolete_orderings,
                                      AccessPath *path,
                                      const char *description_for_trace);
  bool ProposeTableScan(TABLE *table, int node_idx,
                        double force_num_output_rows_after_filter);
  bool ProposeIndexScan(TABLE *table, int node_idx,
                        double force_num_output_rows_after_filter,
                        unsigned key_idx, bool reverse, int ordering_idx);
  bool ProposeRefAccess(TABLE *table, int node_idx, unsigned key_idx,
                        double force_num_output_rows_after_filter, bool reverse,
                        table_map allowed_parameter_tables, int ordering_idx);
  bool ProposeAllUniqueIndexLookupsWithConstantKey(int node_idx, bool *found);
  bool RedundantThroughSargable(
      OverflowBitset redundant_against_sargable_predicates,
      const AccessPath *left_path, const AccessPath *right_path, NodeMap left,
      NodeMap right);
  inline pair<bool, bool> AlreadyAppliedAsSargable(
      Item *condition, const AccessPath *left_path,
      const AccessPath *right_path);
  bool ProposeAllFullTextIndexScans(TABLE *table, int node_idx,
                                    double force_num_output_rows_after_filter);
  bool ProposeFullTextIndexScan(TABLE *table, int node_idx,
                                Item_func_match *match, int predicate_idx,
                                int ordering_idx,
                                double force_num_output_rows_after_filter);
  void ProposeNestedLoopJoin(NodeMap left, NodeMap right, AccessPath *left_path,
                             AccessPath *right_path, const JoinPredicate *edge,
                             bool rewrite_semi_to_inner,
                             FunctionalDependencySet new_fd_set,
                             OrderingSet new_obsolete_orderings,
                             bool *wrote_trace);
  void ProposeHashJoin(NodeMap left, NodeMap right, AccessPath *left_path,
                       AccessPath *right_path, const JoinPredicate *edge,
                       FunctionalDependencySet new_fd_set,
                       OrderingSet new_obsolete_orderings,
                       bool rewrite_semi_to_inner, bool *wrote_trace);
  void ApplyPredicatesForBaseTable(int node_idx,
                                   OverflowBitset applied_predicates,
                                   OverflowBitset subsumed_predicates,
                                   bool materialize_subqueries,
                                   AccessPath *path,
                                   FunctionalDependencySet *new_fd_set);
  void ApplyDelayedPredicatesAfterJoin(
      NodeMap left, NodeMap right, const AccessPath *left_path,
      const AccessPath *right_path, int join_predicate_first,
      int join_predicate_last, bool materialize_subqueries,
      AccessPath *join_path, FunctionalDependencySet *new_fd_set);
  double FindAlreadyAppliedSelectivity(const JoinPredicate *edge,
                                       const AccessPath *left_path,
                                       const AccessPath *right_path,
                                       NodeMap left, NodeMap right);

  void CommitBitsetsToHeap(AccessPath *path) const;
  bool BitsetsAreCommitted(AccessPath *path) const;
};

/// Lists the current secondary engine flags in use. If there is no secondary
/// engine, will use a default set of permissive flags suitable for
/// non-secondary engine use.
SecondaryEngineFlags EngineFlags(const THD *thd) {
  if (const handlerton *secondary_engine = SecondaryEngineHandlerton(thd);
      secondary_engine != nullptr) {
    return secondary_engine->secondary_engine_flags;
  }

  return MakeSecondaryEngineFlags(
      SecondaryEngineFlag::SUPPORTS_HASH_JOIN,
      SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN);
}

/// Gets the secondary storage engine cost modification function, if any.
secondary_engine_modify_access_path_cost_t SecondaryEngineCostHook(
    const THD *thd) {
  const handlerton *secondary_engine = SecondaryEngineHandlerton(thd);
  if (secondary_engine == nullptr) {
    return nullptr;
  } else {
    return secondary_engine->secondary_engine_modify_access_path_cost;
  }
}

/// Returns the MATCH function of a predicate that can be pushed down to a
/// full-text index. This can be done if the predicate is a MATCH function,
/// or in some cases (see IsSargableFullTextIndexPredicate() for details)
/// where the predicate is a comparison function which compares the result
/// of MATCH with a constant. For example, predicates on this form could be
/// pushed down to a full-text index:
///
///   WHERE MATCH (x) AGAINST ('search string') AND @<more predicates@>
///
///   WHERE MATCH (x) AGAINST ('search string') > 0.5 AND @<more predicates@>
///
/// Since full-text index scans return documents with positive scores only, an
/// index scan can only be used if the predicate excludes negative or zero
/// scores.
Item_func_match *GetSargableFullTextPredicate(const Predicate &predicate) {
  Item_func *func = down_cast<Item_func *>(predicate.condition);
  switch (func->functype()) {
    case Item_func::MATCH_FUNC:
      // The predicate is MATCH (x) AGAINST ('search string'), which can be
      // pushed to the index.
      return down_cast<Item_func_match *>(func->get_arg(0))->get_master();
    case Item_func::LT_FUNC:
    case Item_func::LE_FUNC:
      // The predicate is const < MATCH or const <= MATCH, with a constant value
      // which makes it pushable.
      assert(func->get_arg(0)->const_item());
      return down_cast<Item_func_match *>(func->get_arg(1))->get_master();
    case Item_func::GT_FUNC:
    case Item_func::GE_FUNC:
      // The predicate is MATCH > const or MATCH >= const, with a constant value
      // which makes it pushable.
      assert(func->get_arg(1)->const_item());
      return down_cast<Item_func_match *>(func->get_arg(0))->get_master();
    default:
      // The predicate is not on a form that can be pushed to a full-text index
      // scan. We should not get here.
      assert(false);
      return nullptr;
  }
}

/// Is the current statement a DELETE statement?
bool IsDeleteStatement(const THD *thd) {
  return thd->lex->sql_command == SQLCOM_DELETE ||
         thd->lex->sql_command == SQLCOM_DELETE_MULTI;
}

/// Is the current statement a DELETE statement?
bool IsUpdateStatement(const THD *thd) {
  return thd->lex->sql_command == SQLCOM_UPDATE ||
         thd->lex->sql_command == SQLCOM_UPDATE_MULTI;
}

void CostingReceiver::TraceAccessPaths(NodeMap nodes) {
  auto it = m_access_paths.find(nodes);
  if (it == m_access_paths.end()) {
    *m_trace += " - ";
    *m_trace += PrintSet(nodes);
    *m_trace += " has no access paths (this should not normally happen)\n";
    return;
  }

  *m_trace += " - current access paths for ";
  *m_trace += PrintSet(nodes);
  *m_trace += ": ";

  bool first = true;
  for (const AccessPath *path : it->second.paths) {
    if (!first) {
      *m_trace += ", ";
    }
    *m_trace += PrintAccessPath(*path, *m_graph, "");
    first = false;
  }
  *m_trace += ")\n";
}

/**
  Called for each table in the query block, at some arbitrary point before we
  start seeing subsets where it's joined to other tables.

  We support table scans and ref access, so we create access paths for both
  (where possible) and cost them. In this context, “tables” in a query block
  also includes virtual tables such as derived tables, so we need to figure out
  if there is a cost for materializing them.
 */
bool CostingReceiver::FoundSingleNode(int node_idx) {
  if (m_thd->is_error()) return true;

  m_graph->secondary_engine_costing_flags &=
      ~SecondaryEngineCostingFlag::HAS_MULTIPLE_BASE_TABLES;

  TABLE *table = m_graph->nodes[node_idx].table;
  Table_ref *tl = table->pos_in_table_list;

  if (m_trace != nullptr) {
    *m_trace += StringPrintf("\nFound node %s [rows=%llu]\n",
                             m_graph->nodes[node_idx].table->alias,
                             table->file->stats.records);
  }

  // First look for unique index lookups that use only constants.
  {
    bool found_eq_ref = false;
    if (ProposeAllUniqueIndexLookupsWithConstantKey(node_idx, &found_eq_ref)) {
      return true;
    }

    // If we found an unparameterized EQ_REF path, we can skip looking for
    // alternative access methods, like parameterized or non-unique index
    // lookups, index range scans or table scans, as they are unlikely to be any
    // better. Returning early to reduce time spent planning the query, which is
    // especially beneficial for point selects.
    if (found_eq_ref) {
      if (m_trace != nullptr) {
        TraceAccessPaths(TableBitmap(node_idx));
      }
      return false;
    }
  }

  // We run the range optimizer before anything else, because we can use
  // its estimates to adjust predicate selectivity, giving us consistent
  // row count estimation between the access paths. (It is also usually
  // more precise for complex range conditions than our default estimates.
  // This is also the reason why we run it even if HA_NO_INDEX_ACCESS is set.)
  double range_optimizer_row_estimate = -1.0;
  {
    auto cleanup_mem_root = create_scope_guard([this, node_idx] {
      if (node_idx == 0) {
        // We won't be calling the range optimizer anymore, so we don't need
        // to keep its temporary allocations around. Note that FoundSingleNode()
        // counts down from N-1 to 0, not up.
        m_range_optimizer_mem_root.Clear();
      } else {
        m_range_optimizer_mem_root.ClearForReuse();
      }
    });
    if (!tl->is_recursive_reference() && m_graph->num_where_predicates > 0) {
      // Note that true error returns in itself is not enough to fail the query;
      // the range optimizer could be out of RAM easily enough, which is
      // nonfatal. That just means we won't be using it for this table.
      bool impossible = false;
      if (FindIndexRangeScans(node_idx, &impossible,
                              &range_optimizer_row_estimate) &&
          m_thd->is_error()) {
        return true;
      }
      if (impossible) {
        const char *const cause = "WHERE condition is always false";
        if (!IsBitSet(tl->tableno(), m_graph->tables_inner_to_outer_or_anti)) {
          // The entire top-level join is going to be empty, so we can abort the
          // planning and return a zero rows plan.
          m_query_block->join->zero_result_cause = cause;
          return true;
        }
        AccessPath *table_path =
            NewTableScanAccessPath(m_thd, table, /*count_examined_rows=*/false);
        AccessPath *zero_path = NewZeroRowsAccessPath(m_thd, table_path, cause);

        // We need to get the set of functional dependencies right,
        // even though we don't need to actually apply any filters.
        FunctionalDependencySet new_fd_set;
        ApplyPredicatesForBaseTable(
            node_idx,
            /*applied_predicates=*/
            MutableOverflowBitset{m_thd->mem_root, m_graph->predicates.size()},
            /*subsumed_predicates=*/
            MutableOverflowBitset{m_thd->mem_root, m_graph->predicates.size()},
            /*materialize_subqueries=*/false, zero_path, &new_fd_set);
        zero_path->filter_predicates =
            MutableOverflowBitset{m_thd->mem_root, m_graph->predicates.size()};
        zero_path->ordering_state =
            m_orderings->ApplyFDs(zero_path->ordering_state, new_fd_set);
        ProposeAccessPathWithOrderings(TableBitmap(node_idx), new_fd_set,
                                       /*obsolete_orderings=*/0, zero_path, "");
        if (m_trace != nullptr) {
          TraceAccessPaths(TableBitmap(node_idx));
        }
        return false;
      }
    }
  }

  if (ProposeTableScan(table, node_idx, range_optimizer_row_estimate)) {
    return true;
  }

  if (Overlaps(table->file->ha_table_flags(), HA_NO_INDEX_ACCESS) ||
      tl->is_recursive_reference()) {
    // We can't use any indexes, so end here.
    if (m_trace != nullptr) {
      TraceAccessPaths(TableBitmap(node_idx));
    }
    return false;
  }

  // Propose index scan (for getting interesting orderings).
  // We only consider those that are more interesting than a table scan;
  // for the others, we don't even need to create the access path and go
  // through the tournament.
  for (const ActiveIndexInfo &order_info : *m_active_indexes) {
    if (order_info.table != table) {
      continue;
    }

    const int forward_order =
        m_orderings->RemapOrderingIndex(order_info.forward_order);
    const int reverse_order =
        m_orderings->RemapOrderingIndex(order_info.reverse_order);
    for (bool reverse : {false, true}) {
      if (reverse && reverse_order == 0) {
        continue;
      }
      const int order = reverse ? reverse_order : forward_order;
      if (order != 0) {
        if (ProposeIndexScan(table, node_idx, range_optimizer_row_estimate,
                             order_info.key_idx, reverse, order)) {
          return true;
        }
      }

      // Propose ref access using only sargable predicates that reference no
      // other table.
      if (ProposeRefAccess(table, node_idx, order_info.key_idx,
                           range_optimizer_row_estimate, reverse,
                           /*allowed_parameter_tables=*/0, order)) {
        return true;
      }

      // Propose ref access using all sargable predicates that also refer to
      // other tables (e.g. t1.x = t2.x). Such access paths can only be used
      // on the inner side of a nested loop join, where all the other
      // referenced tables are among the outer tables of the join. Such path
      // is called a parameterized path.
      //
      // Since indexes can have multiple parts, the access path can also end
      // up being parameterized on multiple outer tables. However, since
      // parameterized paths are less flexible in joining than
      // non-parameterized ones, it can be advantageous to not use all parts
      // of the index; it's impossible to say locally. Thus, we enumerate all
      // possible subsets of table parameters that may be useful, to make sure
      // we don't miss any such paths.
      table_map want_parameter_tables = 0;
      for (const SargablePredicate &sp :
           m_graph->nodes[node_idx].sargable_predicates) {
        if (sp.field->table == table &&
            sp.field->part_of_key.is_set(order_info.key_idx) &&
            !Overlaps(sp.other_side->used_tables(),
                      PSEUDO_TABLE_BITS | table->pos_in_table_list->map())) {
          want_parameter_tables |= sp.other_side->used_tables();
        }
      }
      for (table_map allowed_parameter_tables :
           NonzeroSubsetsOf(want_parameter_tables)) {
        if (ProposeRefAccess(table, node_idx, order_info.key_idx,
                             range_optimizer_row_estimate, reverse,
                             allowed_parameter_tables, order)) {
          return true;
        }
      }
    }
  }

  if (tl->is_fulltext_searched()) {
    if (ProposeAllFullTextIndexScans(table, node_idx,
                                     range_optimizer_row_estimate)) {
      return true;
    }
  }

  if (m_trace != nullptr) {
    TraceAccessPaths(TableBitmap(node_idx));
  }
  return false;
}

// Figure out which predicates we have that are not applied/subsumed
// by scanning this specific index; we already did a check earlier,
// but that was on predicates applied by scanning _any_ index.
// The difference is those that
//
//   a) Use a field that's not part of this index.
//   b) Use a field that our index is only partial for; these are still
//      counted as applied for selectivity purposes (which is possibly
//      overcounting), but need to be rechecked (ie., not subsumed).
//   c) Use a field where we've been told by get_ranges_from_tree()
//      that it had to set up a range that was nonexact (because it was
//      part of a predicate that had a non-equality condition on an
//      earlier keypart). These are handled as for b).
//
// We use this information to build up sets of which fields an
// applied or subsumed predicate is allowed to reference,
// then check each predicate against those lists.
void FindAppliedAndSubsumedPredicatesForRangeScan(
    THD *thd, KEY *key, unsigned used_key_parts, unsigned num_exact_key_parts,
    TABLE *table, OverflowBitset tree_applied_predicates,
    OverflowBitset tree_subsumed_predicates, const JoinHypergraph &graph,
    OverflowBitset *applied_predicates_out,
    OverflowBitset *subsumed_predicates_out) {
  MutableOverflowBitset applied_fields{thd->mem_root, table->s->fields};
  MutableOverflowBitset subsumed_fields{thd->mem_root, table->s->fields};
  MutableOverflowBitset applied_predicates(thd->mem_root,
                                           graph.predicates.size());
  MutableOverflowBitset subsumed_predicates(thd->mem_root,
                                            graph.predicates.size());
  for (unsigned keypart_idx = 0; keypart_idx < used_key_parts; ++keypart_idx) {
    const KEY_PART_INFO &keyinfo = key->key_part[keypart_idx];
    applied_fields.SetBit(keyinfo.field->field_index());
    if (keypart_idx < num_exact_key_parts &&
        !Overlaps(keyinfo.key_part_flag, HA_PART_KEY_SEG)) {
      subsumed_fields.SetBit(keyinfo.field->field_index());
    }
  }
  for (int predicate_idx : BitsSetIn(tree_applied_predicates)) {
    Item *condition = graph.predicates[predicate_idx].condition;
    bool any_not_applied =
        WalkItem(condition, enum_walk::POSTFIX, [&applied_fields](Item *item) {
          return item->type() == Item::FIELD_ITEM &&
                 !IsBitSet(down_cast<Item_field *>(item)->field->field_index(),
                           applied_fields);
        });
    if (any_not_applied) {
      continue;
    }
    applied_predicates.SetBit(predicate_idx);
    if (IsBitSet(predicate_idx, tree_subsumed_predicates)) {
      bool any_not_subsumed = WalkItem(
          condition, enum_walk::POSTFIX, [&subsumed_fields](Item *item) {
            return item->type() == Item::FIELD_ITEM &&
                   !IsBitSet(
                       down_cast<Item_field *>(item)->field->field_index(),
                       subsumed_fields);
          });
      if (!any_not_subsumed) {
        subsumed_predicates.SetBit(predicate_idx);
      }
    }
  }
  *applied_predicates_out = std::move(applied_predicates);
  *subsumed_predicates_out = std::move(subsumed_predicates);
}

struct PossibleRangeScan {
  unsigned idx;
  unsigned mrr_flags;
  unsigned mrr_buf_size;
  unsigned used_key_parts;
  double cost;
  ha_rows num_rows;
  bool is_ror_scan;
  bool is_imerge_scan;
  OverflowBitset applied_predicates;
  OverflowBitset subsumed_predicates;
  Quick_ranges ranges;
};

bool CollectPossibleRangeScans(
    THD *thd, SEL_TREE *tree, RANGE_OPT_PARAM *param,
    OverflowBitset tree_applied_predicates,
    OverflowBitset tree_subsumed_predicates, const JoinHypergraph &graph,
    Mem_root_array<PossibleRangeScan> *possible_scans) {
  for (unsigned idx = 0; idx < param->keys; idx++) {
    SEL_ROOT *root = tree->keys[idx];
    if (root == nullptr || root->type == SEL_ROOT::Type::MAYBE_KEY ||
        root->root->maybe_flag) {
      continue;
    }

    const uint keynr = param->real_keynr[idx];
    const bool covering_index = param->table->covering_keys.is_set(keynr);
    unsigned mrr_flags, buf_size;
    Cost_estimate cost;
    bool is_ror_scan, is_imerge_scan;

    // NOTE: We give in ORDER_NOT_RELEVANT now, but will re-run with
    // ORDER_ASC/ORDER_DESC when actually proposing the index, if that
    // yields an interesting order.
    ha_rows num_rows = check_quick_select(
        thd, param, idx, covering_index, root, /*update_tbl_stats=*/true,
        ORDER_NOT_RELEVANT, /*skip_records_in_range=*/false, &mrr_flags,
        &buf_size, &cost, &is_ror_scan, &is_imerge_scan);
    if (num_rows == HA_POS_ERROR) {
      continue;
    }

    // TODO(sgunders): See if we could have had a pre-filtering mechanism
    // that allowed us to skip extracting these ranges if the path would
    // obviously too high cost. As it is, it's a bit hard to just propose
    // the path and see if it came in, since we need e.g. num_exact_key_parts
    // as an output from this call, and that in turn affects filter cost.
    Quick_ranges ranges(param->return_mem_root);
    unsigned used_key_parts, num_exact_key_parts;
    if (get_ranges_from_tree(param->return_mem_root, param->table,
                             param->key[idx], keynr, root, MAX_REF_PARTS,
                             &used_key_parts, &num_exact_key_parts, &ranges)) {
      return true;
    }

    KEY *key = &param->table->key_info[keynr];

    PossibleRangeScan scan;
    scan.idx = idx;
    scan.mrr_flags = mrr_flags;
    scan.mrr_buf_size = buf_size;
    scan.used_key_parts = used_key_parts;
    scan.cost = cost.total_cost();
    scan.num_rows = num_rows;
    scan.is_ror_scan = is_ror_scan;
    scan.is_imerge_scan = is_imerge_scan;
    scan.ranges = std::move(ranges);
    FindAppliedAndSubsumedPredicatesForRangeScan(
        thd, key, used_key_parts, num_exact_key_parts, param->table,
        tree_applied_predicates, tree_subsumed_predicates, graph,
        &scan.applied_predicates, &scan.subsumed_predicates);
    possible_scans->push_back(std::move(scan));
  }
  return false;
}

/**
  Based on estimates for all the different range scans (which cover different
  but potentially overlapping combinations of predicates), try to find an
  estimate for the number of rows scanning the given table, with all predicates
  applied.

  The #1 priority here is to get a single estimate for all (non-parameterized)
  scans over this table (including non-range scans), that we can reuse for all
  access paths. This makes sure they are fairly compared on cost (and ordering)
  alone; different estimates would be nonsensical, and cause those where we
  happen to have lower estimates to get preferred as they are joined higher up
  in the tree. Obviously, however, it is also attractive to get an estimate that
  is as good as possible. We only really care about the total selectivity of all
  predicates; we don't care to adjust each individual selectivity.

  [Mar07] describes an unbiased estimator that is exactly what we want,
  and [Hav20] demonstrates an efficient calculation method (up to about 20–25
  possible predicates) of this estimator. Long-term, implementing this would be
  our best choice. However, the implementation is not entirely trivial:

   - If the selectivity estimates are not consistent (e.g. S(a AND b) <
     S(a)S(b)), the algorithm will fail to converge. Extra steps are needed to
     correct for this.
   - The efficient algorithm (in [Hav20]) requires a linear algebra library
     (for performant matrix multiplication and Cholesky decomposition).
   - If we have a _lot_ of estimates, even the efficient algorithm fails to
     converge in time (just the answers require 2^n space), and we would need
     additional logic to partition the problem.

  Thus, for the time being, we use an ad-hoc algorithm instead. The estimate
  will not be as good, but it will hopefully be on the pessimistic side
  (overestimating the number of rows). It goes as follows:

    1. Pick the most-covering index (ie., the range scan that applies
       the most number of predicates) that does not cover any predicates we've
       already accounted for. If there are multiple ones, choose the least
       selective.
    2. Multiply in its selectivity, and mark all the predicates it covers
       as accounted for. Repeat #1 and #2 for as long as possible.
    3. For any remaining predicates, multiply by their existing estimate
       (ie., the one not coming from the range optimizer).

  The hope is that in #1, we will usually prefer using selectivity information
  from indexes with more keyparts; e.g., it's better to use an index on (a,b)
  than on (a) alone, since it will take into account the correlation between
  predicates on a and predicates on b.


  [Mar07]: Markl et al: “Consistent Selectivity Estimation Via Maximum Entropy”
  [Hav20]: Havenstein et al: “Fast Entropy Maximization for Selectivity
     Estimation of Conjunctive Predicates on CPUs and GPUs”
 */
double EstimateOutputRowsFromRangeTree(
    THD *thd, const RANGE_OPT_PARAM &param, ha_rows total_rows,
    const Mem_root_array<PossibleRangeScan> &possible_scans,
    const JoinHypergraph &graph, OverflowBitset predicates, string *trace) {
  MutableOverflowBitset remaining_predicates = predicates.Clone(thd->mem_root);
  double selectivity = 1.0;
  while (!IsEmpty(remaining_predicates)) {
    const PossibleRangeScan *best_scan = nullptr;
    int best_cover_size = 0;         // Just a cache, for convenience.
    double best_selectivity = -1.0;  // Same.

    for (const PossibleRangeScan &scan : possible_scans) {
      if (IsEmpty(scan.applied_predicates) ||
          !IsSubset(scan.applied_predicates, remaining_predicates)) {
        continue;
      }
      int cover_size = PopulationCount(scan.applied_predicates);
      // NOTE: The check for num_rows >= total_rows is because total_rows may be
      // outdated, and we wouldn't want to have selectivities above 1.0, or NaN
      // or Inf if total_rows is zero.
      const double scan_selectivity =
          scan.num_rows >= total_rows
              ? 1.0
              : scan.num_rows / static_cast<double>(total_rows);
      if (cover_size > best_cover_size ||
          (cover_size == best_cover_size &&
           scan_selectivity > best_selectivity)) {
        best_scan = &scan;
        best_cover_size = cover_size;
        best_selectivity = scan_selectivity;
      }
    }

    if (best_scan == nullptr) {
      // Couldn't use any more range scans (possibly because all have
      // been used).
      break;
    }

    selectivity *= best_selectivity;

    // Mark these predicates as being dealt with.
    for (int predicate_idx : BitsSetIn(best_scan->applied_predicates)) {
      remaining_predicates.ClearBit(predicate_idx);
    }

    if (trace != nullptr) {
      const unsigned keynr = param.real_keynr[best_scan->idx];
      KEY *key = &param.table->key_info[keynr];
      *trace += StringPrintf(
          " - using selectivity %.3f (%llu rows) from range scan on index %s "
          "to cover ",
          best_selectivity, best_scan->num_rows, key->name);
      bool first = true;
      for (int predicate_idx : BitsSetIn(best_scan->applied_predicates)) {
        if (!first) {
          *trace += " AND ";
        }
        first = false;
        *trace +=
            "(" + ItemToString(graph.predicates[predicate_idx].condition) + ")";
      }
      *trace += "\n";
    }
  }

  // Cover any remaining predicates by single-predicate estimates.
  for (int predicate_idx : BitsSetIn(std::move(remaining_predicates))) {
    if (trace != nullptr) {
      *trace += StringPrintf(
          " - using existing selectivity %.3f from outside range scan "
          "to cover %s\n",
          graph.predicates[predicate_idx].selectivity,
          ItemToString(graph.predicates[predicate_idx].condition).c_str());
    }
    selectivity *= graph.predicates[predicate_idx].selectivity;
  }
  return total_rows * selectivity;
}

/**
  From a collection of index scans, find the single cheapest one and generate an
  AccessPath for it. This is similar to CollectPossibleRangeScans(), except that
  this is for index merge, where we don't want to enumerate all possibilities;
  since we don't care about the ordering of the index (we're going to sort all
  of the rows to deduplicate them anyway), cost is the only interesting metric,
  so we only need to pick out and collect ranges for one of them.
  (This isn't strictly true; sometimes, it can be attractive to choose a
  clustered primary key, so we prefer one if we allow them. See the code about
  is_preferred_cpk below, and the comment on the caller. Also, see about
  exactness below.)

  This function can probably be extended to find ROR-capable scans later
  (just check is_ror_scan instead of is_imerge_scan).

  Note that all such scans are index-only (covering), which is reflected in
  the cost parameters we use.

  *inexact is set to true if and only if the chosen path does not reflect its
  predicate faithfully, and needs to be rechecked. We do not currently take
  into account that this may affect the cost higher up, as the difference
  should be small enough that we don't want the combinatorial explosion.
 */
AccessPath *FindCheapestIndexRangeScan(THD *thd, SEL_TREE *tree,
                                       RANGE_OPT_PARAM *param,
                                       bool prefer_clustered_primary_key_scan,
                                       bool *inexact) {
  double best_cost = DBL_MAX;
  int best_key = -1;
  int best_num_rows = -1;
  unsigned best_mrr_flags = 0, best_mrr_buf_size = 0;
  for (unsigned idx = 0; idx < param->keys; idx++) {
    SEL_ROOT *root = tree->keys[idx];
    if (root == nullptr || root->type == SEL_ROOT::Type::MAYBE_KEY ||
        root->root->maybe_flag) {
      continue;
    }

    unsigned mrr_flags, buf_size;
    Cost_estimate cost;
    bool is_ror_scan, is_imerge_scan;

    ha_rows num_rows =
        check_quick_select(thd, param, idx, /*index_only=*/true, root,
                           /*update_tbl_stats=*/true, ORDER_NOT_RELEVANT,
                           /*skip_records_in_range=*/false, &mrr_flags,
                           &buf_size, &cost, &is_ror_scan, &is_imerge_scan);
    if (num_rows == HA_POS_ERROR || !is_imerge_scan) {
      continue;
    }
    const bool is_preferred_cpk =
        prefer_clustered_primary_key_scan &&
        param->table->file->primary_key_is_clustered() &&
        param->real_keynr[idx] == param->table->s->primary_key;
    if (!is_preferred_cpk && cost.total_cost() > best_cost) {
      continue;
    }

    best_key = idx;
    best_cost = cost.total_cost();
    best_num_rows = num_rows;
    best_mrr_flags = mrr_flags;
    best_mrr_buf_size = buf_size;

    if (is_preferred_cpk) {
      break;
    }
  }
  if (best_key == -1) {
    return nullptr;
  }

  const uint keynr = param->real_keynr[best_key];
  SEL_ROOT *root = tree->keys[best_key];

  Quick_ranges ranges(param->return_mem_root);
  unsigned used_key_parts, num_exact_key_parts;
  if (get_ranges_from_tree(param->return_mem_root, param->table,
                           param->key[best_key], keynr, root, MAX_REF_PARTS,
                           &used_key_parts, &num_exact_key_parts, &ranges)) {
    return nullptr;
  }

  KEY *key = &param->table->key_info[keynr];

  AccessPath *path = new (param->return_mem_root) AccessPath;
  path->type = AccessPath::INDEX_RANGE_SCAN;
  path->init_cost = 0.0;
  path->cost = path->cost_before_filter = best_cost;
  path->set_num_output_rows(best_num_rows);
  path->num_output_rows_before_filter = best_num_rows;
  path->index_range_scan().index = keynr;
  path->index_range_scan().num_used_key_parts = used_key_parts;
  path->index_range_scan().used_key_part = param->key[best_key];
  path->index_range_scan().ranges = &ranges[0];
  path->index_range_scan().num_ranges = ranges.size();
  path->index_range_scan().mrr_flags = best_mrr_flags;
  path->index_range_scan().mrr_buf_size = best_mrr_buf_size;
  path->index_range_scan().can_be_used_for_ror =
      tree->ror_scans_map.is_set(best_key);
  path->index_range_scan().need_rows_in_rowid_order = false;
  path->index_range_scan().can_be_used_for_imerge = true;
  path->index_range_scan().reuse_handler = false;
  path->index_range_scan().geometry = Overlaps(key->flags, HA_SPATIAL);
  path->index_range_scan().reverse = false;
  path->index_range_scan().using_extended_key_parts = false;

  *inexact |= (num_exact_key_parts != used_key_parts);
  return path;
}

/**
  Represents a candidate index merge, ie. an OR expression of several
  range scans across different indexes (that can be reconciled by doing
  deduplication by sorting on row IDs).

  Each predicate (in our usual sense of “part of a top-level AND conjunction in
  WHERE”) can give rise to multiple index merges (if there are AND conjunctions
  within ORs), but one index merge arises from exactly one predicate.
  This is not an inherent limitation, but it is how tree_and() does it;
  if it takes two SEL_TREEs with index merges, it just combines their candidates
  wholesale; each will deal with one predicate, and the other one would just
  have to be applied as a filter.

  This is obviously suboptimal, as there are many cases where we could do
  better. Imagine something like (a = 3 OR b > 3) AND b <= 5, with separate
  indexes on a and b; obviously, we could have applied this as a single index
  merge between two range scans: (a = 3 AND b <= 5) OR (b > 3 AND b <= 5). But
  this is probably not a priority for us, so we follow the range optimizer's
  lead here and record each index merge as covering a separate, single
  predicate.
 */
struct PossibleIndexMerge {
  // The index merge itself (a list of range optimizer trees,
  // implicitly ORed together).
  SEL_IMERGE *imerge;

  // Which WHERE predicate it came from.
  size_t pred_idx;

  // If true, the index merge does not faithfully represent the entire
  // predicate (it could return more rows), and needs to be re-checked
  // with a filter.
  bool inexact;
};

bool CostingReceiver::FindIndexRangeScans(
    int node_idx, bool *impossible, double *num_output_rows_after_filter) {
  *impossible = false;
  *num_output_rows_after_filter = -1.0;
  TABLE *table = m_graph->nodes[node_idx].table;

  RANGE_OPT_PARAM param;
  if (setup_range_optimizer_param(
          m_thd, m_thd->mem_root, &m_range_optimizer_mem_root,
          table->keys_in_use_for_query, table, m_query_block, &param)) {
    return true;
  }
  m_thd->push_internal_handler(&param.error_handler);
  auto cleanup =
      create_scope_guard([thd{m_thd}] { thd->pop_internal_handler(); });

  // For each predicate touching this table only, try to include it into our
  // tree of ranges if we can.
  MutableOverflowBitset all_predicates{m_thd->mem_root,
                                       m_graph->predicates.size()};
  MutableOverflowBitset tree_applied_predicates{m_thd->mem_root,
                                                m_graph->predicates.size()};
  MutableOverflowBitset tree_subsumed_predicates{m_thd->mem_root,
                                                 m_graph->predicates.size()};
  Mem_root_array<PossibleIndexMerge> index_merges(&m_range_optimizer_mem_root);
  const NodeMap my_map = TableBitmap(node_idx);
  SEL_TREE *tree = nullptr;
  for (size_t i = 0; i < m_graph->num_where_predicates; ++i) {
    if (m_graph->predicates[i].total_eligibility_set != my_map) {
      // Only base predicates are eligible for being pushed into range scans.
      continue;
    }
    all_predicates.SetBit(i);

    SEL_TREE *new_tree = get_mm_tree(
        m_thd, &param, INNER_TABLE_BIT, INNER_TABLE_BIT,
        table->pos_in_table_list->map(),
        /*remove_jump_scans=*/true, m_graph->predicates[i].condition);
    if (param.has_errors()) {
      // Probably out of RAM; give up using the range optimizer.
      return true;
    }
    if (new_tree == nullptr || new_tree->type == SEL_TREE::ALWAYS) {
      // Nothing in this predicate could be used as range scans for any of
      // the indexes on this table. Skip the predicate for our purposes;
      // we'll be applying it as a normal one later.
      continue;
    }

    if (new_tree->keys_map.is_clear_all()) {
      // The predicate was not converted into a range scan, so it won't be
      // applied or subsumed by any index range scan.
    } else if (new_tree->inexact) {
      // The predicate was converted into a range scan, but there was some part
      // of it that couldn't be completely represented. We need to note that
      // it was converted, so that we don't double-count its
      // selectivity, but we also need to re-apply it as a filter afterwards,
      // so we cannot set it in subsumed_range_predicates.
      // Of course, we don't know the selectivity of the non-applied parts
      // of the predicate, but it's OK to overcount rows (much better than to
      // undercount them).
      tree_applied_predicates.SetBit(i);
    } else {
      // The predicate was completely represented as a range scan for at least
      // one index. This means we can mark it as subsumed for now, but note that
      // if we end up choosing some index that doesn't include the field as a
      // keypart, or one where (some of) its ranges have to be skipped, we could
      // revert this decision. See SEL_TREE::inexact and get_ranges_from_tree().
      tree_applied_predicates.SetBit(i);
      tree_subsumed_predicates.SetBit(i);
    }

    // Store any index merges this predicate gives rise to. The final ANDed tree
    // will also have a list of index merges, but it's only a combined list of
    // the ones from individual predicates, so we collect them here to know
    // which predicate they came from.
    for (SEL_IMERGE &imerge : new_tree->merges) {
      PossibleIndexMerge merge;
      merge.imerge = &imerge;
      merge.pred_idx = i;

      // If there is more than one candidate merge arising from this predicate,
      // it must be because we had an AND inside an OR (tree_and() is the only
      // case that creates multiple candidates). ANDs in index merges are pretty
      // much always handled nonexactly (see the comment on PossibleIndexMerge),
      // ie., we pick one part of the conjunction and have to check the other
      // by filter. So we need to note here that this has happened.
      merge.inexact = (new_tree->merges.size() > 1);

      // Similarly, if there is also range scan arising from this predicate
      // (again because of an AND inside an OR), we need to handle the index
      // merge nonexactly, as the index merge will need to have the range
      // predicate in a filter on top of it.
      merge.inexact |= !new_tree->keys_map.is_clear_all();

      index_merges.push_back(merge);
    }

    if (tree == nullptr) {
      tree = new_tree;
    } else {
      tree = tree_and(&param, tree, new_tree);
      if (param.has_errors()) {
        // Probably out of RAM; give up using the range optimizer.
        return true;
      }
    }
    if (tree->type == SEL_TREE::IMPOSSIBLE) {
      *impossible = true;
      return false;
    }
  }
  if (tree == nullptr) {
    // There were no range predicates on this table.
    return false;
  }
  assert(tree->type == SEL_TREE::KEY);

  Mem_root_array<PossibleRangeScan> possible_scans(&m_range_optimizer_mem_root);
  OverflowBitset tree_applied_predicates_fixed =
      std::move(tree_applied_predicates);
  OverflowBitset tree_subsumed_predicates_fixed =
      std::move(tree_subsumed_predicates);
  if (CollectPossibleRangeScans(
          m_thd, tree, &param, tree_applied_predicates_fixed,
          tree_subsumed_predicates_fixed, *m_graph, &possible_scans)) {
    return true;
  }
  *num_output_rows_after_filter = EstimateOutputRowsFromRangeTree(
      m_thd, param, table->file->stats.records, possible_scans, *m_graph,
      std::move(all_predicates), m_trace);
  if (Overlaps(table->file->ha_table_flags(), HA_NO_INDEX_ACCESS)) {
    // We only wanted to use the index for estimation, and now we've done that.
    return false;
  }

  // Propose all single-index index range scans.
  for (PossibleRangeScan &scan : possible_scans) {
    const uint keynr = param.real_keynr[scan.idx];
    KEY *key = &param.table->key_info[keynr];

    AccessPath path;
    path.type = AccessPath::INDEX_RANGE_SCAN;
    path.init_cost = 0.0;
    path.cost = path.cost_before_filter = scan.cost;
    path.num_output_rows_before_filter = scan.num_rows;
    path.index_range_scan().index = keynr;
    path.index_range_scan().num_used_key_parts = scan.used_key_parts;
    path.index_range_scan().used_key_part = param.key[scan.idx];
    path.index_range_scan().ranges = &scan.ranges[0];
    path.index_range_scan().num_ranges = scan.ranges.size();
    path.index_range_scan().mrr_flags = scan.mrr_flags;
    path.index_range_scan().mrr_buf_size = scan.mrr_buf_size;
    path.index_range_scan().can_be_used_for_ror =
        tree->ror_scans_map.is_set(scan.idx);
    path.index_range_scan().need_rows_in_rowid_order = false;
    path.index_range_scan().can_be_used_for_imerge = scan.is_imerge_scan;
    path.index_range_scan().reuse_handler = false;
    path.index_range_scan().geometry = Overlaps(key->flags, HA_SPATIAL);
    path.index_range_scan().reverse = false;
    path.index_range_scan().using_extended_key_parts = false;

    if (IsBitSet(node_idx, m_immediate_update_delete_candidates)) {
      path.immediate_update_delete_table = node_idx;
      // Don't allow immediate update of the key that is being scanned.
      if (IsUpdateStatement(m_thd) &&
          uses_index_on_fields(&path, table->write_set)) {
        path.immediate_update_delete_table = -1;
      }
    }

    bool contains_subqueries = false;  // Filled on the first iteration below.

    // First propose the unordered scan, optionally with sorting afterwards.
    for (bool materialize_subqueries : {false, true}) {
      AccessPath new_path = path;
      FunctionalDependencySet new_fd_set;
      ApplyPredicatesForBaseTable(
          node_idx, scan.applied_predicates, scan.subsumed_predicates,
          materialize_subqueries, &new_path, &new_fd_set);

      // Override the number of estimated rows, so that all paths get the same.
      new_path.set_num_output_rows(*num_output_rows_after_filter);

      string description_for_trace = string(key->name) + " range";
      ProposeAccessPathWithOrderings(
          TableBitmap(node_idx), new_fd_set,
          /*obsolete_orderings=*/0, &new_path,
          materialize_subqueries ? "mat. subq" : description_for_trace.c_str());

      if (!materialize_subqueries) {
        contains_subqueries = Overlaps(path.filter_predicates,
                                       m_graph->materializable_predicates);
        if (!contains_subqueries) {
          // Nothing to try to materialize.
          break;
        }
      }
    }

    // Now the ordered scans, if they are interesting.
    for (enum_order order_direction : {ORDER_ASC, ORDER_DESC}) {
      const auto it =
          find_if(m_active_indexes->begin(), m_active_indexes->end(),
                  [table, keynr](const ActiveIndexInfo &info) {
                    return info.table == table &&
                           info.key_idx == static_cast<int>(keynr);
                  });
      assert(it != m_active_indexes->end());
      const int ordering_idx = m_orderings->RemapOrderingIndex(
          order_direction == ORDER_ASC ? it->forward_order : it->reverse_order);
      if (ordering_idx == 0) {
        // Not an interesting order.
        continue;
      }

      // Rerun cost estimation, since sorting may have a cost.
      const bool covering_index = param.table->covering_keys.is_set(keynr);
      bool is_ror_scan, is_imerge_scan;
      Cost_estimate cost;
      ha_rows num_rows [[maybe_unused]] = check_quick_select(
          m_thd, &param, scan.idx, covering_index, tree->keys[scan.idx],
          /*update_tbl_stats=*/true, order_direction,
          /*skip_records_in_range=*/false, &path.index_range_scan().mrr_flags,
          &path.index_range_scan().mrr_buf_size, &cost, &is_ror_scan,
          &is_imerge_scan);
      // NOTE: num_rows may be different from scan.num_rows, if the statistics
      // changed in the meantime. If so, we keep the old estimate, in order to
      // get consistent values.
      path.cost = path.cost_before_filter = cost.total_cost();
      path.index_range_scan().can_be_used_for_imerge = is_imerge_scan;
      path.ordering_state = m_orderings->SetOrder(ordering_idx);
      path.index_range_scan().reverse = (order_direction == ORDER_DESC);

      // Reverse index range scans need to be told whether they should be using
      // extended key parts. If the requested scan ordering follows more
      // interesting orderings than a scan ordered by the user-defined key parts
      // only, it means the extended key parts are needed.
      path.index_range_scan().using_extended_key_parts =
          path.index_range_scan().reverse &&
          m_orderings->MoreOrderedThan(
              path.ordering_state,
              m_orderings->SetOrder(m_orderings->RemapOrderingIndex(
                  it->reverse_order_without_extended_key_parts)),
              /*obsolete_orderings=*/0);

      for (bool materialize_subqueries : {false, true}) {
        AccessPath new_path = path;
        FunctionalDependencySet new_fd_set;
        ApplyPredicatesForBaseTable(
            node_idx, scan.applied_predicates, scan.subsumed_predicates,
            materialize_subqueries, &new_path, &new_fd_set);

        // Override the number of estimated rows, so that all paths get the
        // same.
        new_path.set_num_output_rows(*num_output_rows_after_filter);

        string description_for_trace = string(key->name) + " ordered range";
        auto access_path_it = m_access_paths.find(TableBitmap(node_idx));
        assert(access_path_it != m_access_paths.end());
        ProposeAccessPath(&new_path, &access_path_it->second.paths,
                          /*obsolete_orderings=*/0,
                          materialize_subqueries
                              ? "mat. subq"
                              : description_for_trace.c_str());

        if (!contains_subqueries) {
          // Nothing to try to materialize.
          break;
        }
      }
    }
  }

  // Propose all index merges we have collected. Note that this is only
  // “sort-index” merges, ie., generally collect all the row IDs,
  // deduplicate them by sorting (in a Unique object) and then read all the
  // rows. If the indexes are “ROR compatible” (give out their rows in row ID
  // order directly, without any sort -- typically only for InnoDB indexes with
  // the primary key appended directly after the last key part), we can
  // union/intersect them directly without any sorts (“ROR scans”). However, we
  // do not support that yet; it will be for a future worklog.
  for (const PossibleIndexMerge &imerge : index_merges) {
    for (bool allow_clustered_primary_key_scan : {true, false}) {
      bool has_clustered_primary_key_scan;
      ProposeIndexMerge(table, node_idx, *imerge.imerge, imerge.pred_idx,
                        imerge.inexact, allow_clustered_primary_key_scan,
                        m_graph->num_where_predicates,
                        *num_output_rows_after_filter, &param,
                        &has_clustered_primary_key_scan);
      if (!has_clustered_primary_key_scan) {
        // No need to check scans with clustered key scans disallowed
        // if we didn't choose one to begin with.
        break;
      }
    }
  }
  return false;
}

void CostingReceiver::ProposeIndexMerge(
    TABLE *table, int node_idx, const SEL_IMERGE &imerge, int pred_idx,
    bool inexact, bool allow_clustered_primary_key_scan,
    int num_where_predicates, double num_output_rows_after_filter,
    RANGE_OPT_PARAM *param, bool *has_clustered_primary_key_scan) {
  double cost = 0.0;
  double num_output_rows = 0.0;

  // Clustered primary keys are special; we can deduplicate
  // against them cheaper than running through the Unique object,
  // so we want to keep track of its size to cost them.
  // However, they destroy ordering properties, and if there are
  // very few rows in the scan, it's probably better to avoid the
  // compare, so we need to try both with and without
  // (done in a for loop outside this function).
  *has_clustered_primary_key_scan = false;
  double non_cpk_cost = 0.0;
  double non_cpk_rows = 0.0;

  Mem_root_array<AccessPath *> paths(m_thd->mem_root);
  for (SEL_TREE *tree : imerge.trees) {
    inexact |= tree->inexact;

    // NOTE: If we allow clustered primary key scans, we prefer
    // them here even with a higher cost, in case they make the
    // entire query cheaper due to lower sort costs. (There can
    // only be one in any given index merge, since there is only
    // one primary key.) If we end up choosing it, we will be
    // called again with allow_clustered_primary_key_scan=false.
    AccessPath *path = FindCheapestIndexRangeScan(
        m_thd, tree, param,
        /*prefer_clustered_primary_key_scan=*/allow_clustered_primary_key_scan,
        &inexact);

    if (path == nullptr) {
      // Something failed; ignore.
      return;
    }
    paths.push_back(path);
    cost += path->cost;
    num_output_rows += path->num_output_rows();

    if (allow_clustered_primary_key_scan &&
        table->file->primary_key_is_clustered() &&
        path->index_range_scan().index == table->s->primary_key) {
      assert(!*has_clustered_primary_key_scan);
      *has_clustered_primary_key_scan = true;
    } else {
      non_cpk_cost += path->cost;
      non_cpk_rows = path->num_output_rows();
    }
  }

  double init_cost = non_cpk_cost;

  // If we have a clustered primary key scan, we scan it separately, without
  // going through the deduplication-by-sort. But that means we need to make
  // sure no other rows overlap with it; there's a special operation for this
  // (check if a given row ID falls inside a given primary key range),
  // but it's not free, so add it costs here.
  if (*has_clustered_primary_key_scan) {
    double compare_cost = table->cost_model()->key_compare_cost(non_cpk_rows);
    init_cost += compare_cost;
    cost += compare_cost;
  }

  // Add the cost for the Unique operations. Note that since we read
  // the clustered primary key _last_, we cannot get out a single row
  // before everything has been read and deduplicated. If we want
  // lower init_cost (i.e., for LIMIT), we should probably change this.
  const double rows_to_deduplicate =
      *has_clustered_primary_key_scan ? non_cpk_rows : num_output_rows;
  const double dup_removal_cost =
      Unique::get_use_cost(rows_to_deduplicate, table->file->ref_length,
                           m_thd->variables.sortbuff_size, table->cost_model());
  init_cost += dup_removal_cost;
  cost += dup_removal_cost;

  // Add the cost for converting the sorted row IDs into rows
  // (which is done for all rows, except for clustered primary keys).
  // This happens running for each row, so doesn't get added to init_cost.
  // NOTE: We always give is_interrupted = false, because we don't
  // really know where we will be in the join tree.
  Cost_estimate sweep_cost;
  get_sweep_read_cost(table, non_cpk_rows, /*interrupted=*/false, &sweep_cost);
  cost += sweep_cost.total_cost();

  AccessPath imerge_path;
  imerge_path.type = AccessPath::INDEX_MERGE;
  imerge_path.index_merge().table = table;
  imerge_path.index_merge().forced_by_hint = false;
  imerge_path.index_merge().allow_clustered_primary_key_scan =
      allow_clustered_primary_key_scan;
  imerge_path.index_merge().children = new (param->return_mem_root)
      Mem_root_array<AccessPath *>(std::move(paths));

  imerge_path.cost = imerge_path.cost_before_filter = cost;
  imerge_path.init_cost = init_cost;
  imerge_path.num_output_rows_before_filter =
      min<double>(num_output_rows, num_output_rows_after_filter);
  imerge_path.set_num_output_rows(imerge_path.num_output_rows_before_filter);

  if (IsBitSet(node_idx, m_immediate_update_delete_candidates)) {
    imerge_path.immediate_update_delete_table = node_idx;
    // Don't allow immediate update of any keys being scanned.
    if (IsUpdateStatement(m_thd) &&
        uses_index_on_fields(&imerge_path, table->write_set)) {
      imerge_path.immediate_update_delete_table = -1;
    }
  }

  // Find out which ordering we would follow, if any. We nominally sort
  // everything by row ID (which follows the primary key), but if we have a
  // clustered primary key scan, it is taken after everything else and thus
  // out-of-order (ironically enough).
  if (!*has_clustered_primary_key_scan &&
      table->file->primary_key_is_clustered()) {
    const auto it = find_if(
        m_active_indexes->begin(), m_active_indexes->end(),
        [table](const ActiveIndexInfo &info) {
          return info.table == table &&
                 info.key_idx == static_cast<int>(table->s->primary_key);
        });
    if (it != m_active_indexes->end()) {
      imerge_path.ordering_state = m_orderings->SetOrder(
          m_orderings->RemapOrderingIndex(it->forward_order));
    }
  }

  // An index merge corresponds to one predicate (see comment on
  // PossibleIndexMerge), and subsumes that predicate if and only if it is a
  // faithful representation of everything in it.
  MutableOverflowBitset this_predicate(param->temp_mem_root,
                                       num_where_predicates);
  this_predicate.SetBit(pred_idx);
  OverflowBitset applied_predicates(std::move(this_predicate));
  OverflowBitset subsumed_predicates =
      inexact ? OverflowBitset(MutableOverflowBitset(param->temp_mem_root,
                                                     num_where_predicates))
              : applied_predicates;
  const bool contains_subqueries = Overlaps(imerge_path.filter_predicates,
                                            m_graph->materializable_predicates);
  for (bool materialize_subqueries : {false, true}) {
    AccessPath new_path = imerge_path;
    FunctionalDependencySet new_fd_set;
    ApplyPredicatesForBaseTable(node_idx, applied_predicates,
                                subsumed_predicates, materialize_subqueries,
                                &new_path, &new_fd_set);

    // Override the number of estimated rows, so that all paths get the
    // same.
    new_path.set_num_output_rows(num_output_rows_after_filter);

    ProposeAccessPathWithOrderings(
        TableBitmap(node_idx), new_fd_set,
        /*obsolete_orderings=*/0, &new_path,
        materialize_subqueries ? "mat. subq" : "index merge");

    if (!contains_subqueries) {
      // Nothing to try to materialize.
      break;
    }
  }
}

// Specifies a mapping in an Index_lookup between an index keypart and a
// condition, with the intention to satisfy the condition with the index keypart
// (ref access). Roughly comparable to Key_use in the non-hypergraph optimizer.
struct KeypartForRef {
  // The condition we are pushing down (e.g. t1.f1 = 3).
  Item *condition;

  // The field that is to be matched (e.g. t1.f1).
  Field *field;

  // The value we are matching against (e.g. 3). Could be another field.
  Item *val;

  // Whether this condition would never match if either side is NULL.
  bool null_rejecting;

  // Tables used by the condition. Necessarily includes the table “field”
  // is part of.
  table_map used_tables;

  // Is it safe to evaluate "val" during optimization? It must be
  // const_for_execution() and contain no subqueries or stored procedures.
  bool can_evaluate;
};

int WasPushedDownToRef(Item *condition, const KeypartForRef *keyparts,
                       unsigned num_keyparts) {
  for (unsigned keypart_idx = 0; keypart_idx < num_keyparts; keypart_idx++) {
    if (condition->eq(keyparts[keypart_idx].condition,
                      /*binary_cmp=*/true)) {
      return keypart_idx;
    }
  }
  return -1;
}

bool ContainsSubqueries(Item *item_arg) {
  // Nearly the same as item_arg->has_subquery(), but different for
  // Item_func_not_all, which we currently do not support.
  return WalkItem(item_arg, enum_walk::POSTFIX, [](Item *item) {
    return item->type() == Item::SUBSELECT_ITEM;
  });
}

bool CostingReceiver::ProposeRefAccess(
    TABLE *table, int node_idx, unsigned key_idx,
    double force_num_output_rows_after_filter, bool reverse,
    table_map allowed_parameter_tables, int ordering_idx) {
  KEY *key = &table->key_info[key_idx];

  if (key->flags & HA_FULLTEXT) {
    return false;
  }

  // Go through each of the sargable predicates and see how many key parts
  // we can match.
  unsigned matched_keyparts = 0;
  unsigned length = 0;
  const unsigned usable_keyparts = actual_key_parts(key);
  KeypartForRef keyparts[MAX_REF_PARTS];
  table_map parameter_tables = 0;

  for (unsigned keypart_idx = 0;
       keypart_idx < usable_keyparts && keypart_idx < MAX_REF_PARTS;
       ++keypart_idx) {
    const KEY_PART_INFO &keyinfo = key->key_part[keypart_idx];
    bool matched_this_keypart = false;

    for (const SargablePredicate &sp :
         m_graph->nodes[node_idx].sargable_predicates) {
      if (!sp.field->part_of_key.is_set(key_idx)) {
        // Quick reject.
        continue;
      }
      Item_func_eq *item = down_cast<Item_func_eq *>(
          m_graph->predicates[sp.predicate_index].condition);
      if (sp.field->eq(keyinfo.field)) {
        const table_map other_side_tables =
            sp.other_side->used_tables() & ~PSEUDO_TABLE_BITS;
        if (IsSubset(other_side_tables, allowed_parameter_tables)) {
          parameter_tables |= other_side_tables;
          matched_this_keypart = true;
          keyparts[keypart_idx].field = sp.field;
          keyparts[keypart_idx].condition = item;
          keyparts[keypart_idx].val = sp.other_side;
          keyparts[keypart_idx].null_rejecting = true;
          keyparts[keypart_idx].used_tables = item->used_tables();
          keyparts[keypart_idx].can_evaluate = sp.can_evaluate;
          ++matched_keyparts;
          length += keyinfo.store_length;
          break;
        }
      }
    }
    if (!matched_this_keypart) {
      break;
    }
  }
  if (matched_keyparts == 0) {
    return false;
  }
  if (parameter_tables != allowed_parameter_tables) {
    // We've already seen this before, with a more lenient subset,
    // so don't try it again.
    return false;
  }

  if (matched_keyparts < usable_keyparts &&
      (table->file->index_flags(key_idx, 0, false) & HA_ONLY_WHOLE_INDEX)) {
    if (m_trace != nullptr) {
      *m_trace += StringPrintf(
          " - %s is whole-key only, and we could only match %d/%d "
          "key parts for ref access\n",
          key->name, matched_keyparts, usable_keyparts);
    }
    return false;
  }

  if (m_trace != nullptr) {
    if (matched_keyparts < usable_keyparts) {
      *m_trace += StringPrintf(
          " - %s is applicable for ref access (using %d/%d key parts only)\n",
          key->name, matched_keyparts, usable_keyparts);
    } else {
      *m_trace +=
          StringPrintf(" - %s is applicable for ref access\n", key->name);
    }
  }

  // Create Index_lookup for this ref, and set it up based on the chosen
  // keyparts.
  Index_lookup *ref = new (m_thd->mem_root) Index_lookup;
  if (init_ref(m_thd, matched_keyparts, length, key_idx, ref)) {
    return true;
  }

  uchar *key_buff = ref->key_buff;
  uchar *null_ref_key = nullptr;
  bool null_rejecting_key = true;
  for (unsigned keypart_idx = 0; keypart_idx < matched_keyparts;
       keypart_idx++) {
    KeypartForRef *keypart = &keyparts[keypart_idx];
    const KEY_PART_INFO *keyinfo = &key->key_part[keypart_idx];

    if (init_ref_part(m_thd, keypart_idx, keypart->val, /*cond_guard=*/nullptr,
                      keypart->null_rejecting, /*const_tables=*/0,
                      keypart->used_tables, keyinfo->null_bit, keyinfo,
                      key_buff, ref)) {
      return true;
    }
    // TODO(sgunders): When we get support for REF_OR_NULL,
    // set null_ref_key = key_buff here if appropriate.
    /*
      The selected key will reject matches on NULL values if:
       - the key field is nullable, and
       - predicate rejects NULL values (keypart->null_rejecting is true), or
       - JT_REF_OR_NULL is not effective.
    */
    if ((keyinfo->field->is_nullable() || table->is_nullable()) &&
        (!keypart->null_rejecting || null_ref_key != nullptr)) {
      null_rejecting_key = false;
    }
    key_buff += keyinfo->store_length;
  }

  double num_output_rows = table->file->stats.records;
  double join_condition_selectivity = 1.0;

  MutableOverflowBitset applied_predicates{m_thd->mem_root,
                                           m_graph->predicates.size()};
  MutableOverflowBitset subsumed_predicates{m_thd->mem_root,
                                            m_graph->predicates.size()};
  for (size_t i = 0; i < m_graph->predicates.size(); ++i) {
    const Predicate &pred = m_graph->predicates[i];
    int keypart_idx =
        WasPushedDownToRef(pred.condition, keyparts, matched_keyparts);
    if (keypart_idx == -1) {
      continue;
    }

    if (pred.was_join_condition) {
      // This predicate was promoted from a join condition to a WHERE predicate,
      // since it was part of a cycle. For purposes of sargable predicates,
      // we always see all relevant join conditions, so skip it this time
      // so that we don't double-count its selectivity.
      applied_predicates.SetBit(i);
      continue;
    }

    if (i < m_graph->num_where_predicates &&
        !IsSingleBitSet(pred.total_eligibility_set)) {
      // This is a WHERE condition that is either nondeterministic,
      // or after an outer join, so it is not sargable. (Having these
      // show up here is very rare, but will get more common when we
      // get to (x=... OR NULL) predicates.)
      continue;
    }

    if (!IsSubset(pred.condition->used_tables() & ~PSEUDO_TABLE_BITS,
                  table->pos_in_table_list->map())) {
      join_condition_selectivity *= pred.selectivity;
    }

    num_output_rows *= pred.selectivity;
    applied_predicates.SetBit(i);

    const KeypartForRef &keypart = keyparts[keypart_idx];
    bool subsumes;
    if (ref_lookup_subsumes_comparison(m_thd, keypart.field, keypart.val,
                                       keypart.can_evaluate, &subsumes)) {
      return true;
    }
    if (subsumes) {
      if (m_trace != nullptr) {
        *m_trace += StringPrintf(" - %s is subsumed by ref access on %s.%s\n",
                                 ItemToString(pred.condition).c_str(),
                                 table->alias, keypart.field->field_name);
      }
      subsumed_predicates.SetBit(i);
    } else {
      if (m_trace != nullptr) {
        *m_trace += StringPrintf(
            " - %s is not fully subsumed by ref access on %s.%s, keeping\n",
            ItemToString(pred.condition).c_str(), table->alias,
            keypart.field->field_name);
      }
    }
  }

  if (force_num_output_rows_after_filter >= 0.0) {
    // The range optimizer has given us an estimate for the number of
    // rows after all filters have been applied, that we should be
    // consistent with. However, that is only filters; not join conditions.
    // The join conditions we apply are completely independent of the
    // filters, so we make our usual independence assumption.
    force_num_output_rows_after_filter *= join_condition_selectivity;
  }

  // We are guaranteed to get a single row back if all of these hold:
  //
  //  - The index must be unique.
  //  - We can never query it with NULL (ie., no keyparts are nullable,
  //    or our condition is already NULL-rejecting), since NULL is
  //    an exception for unique indexes.
  //  - We use all key parts.
  //
  // This matches the logic in create_ref_for_key().
  const bool single_row = Overlaps(actual_key_flags(key), HA_NOSAME) &&
                          (!Overlaps(actual_key_flags(key), HA_NULL_PART_KEY) ||
                           null_rejecting_key) &&
                          matched_keyparts == usable_keyparts;
  if (single_row) {
    // FIXME: This can cause inconsistent row estimates between different access
    // paths doing the same thing, which is bad (it causes index lookups to be
    // unfairly preferred, especially as we add more tables to the join -- and
    // it also causes access path pruning to work less efficiently). See
    // comments in EstimateFieldSelectivity() and on has_clamped_eq_ref.
    if (num_output_rows > 1.0 && matched_keyparts >= 2) {
      has_clamped_multipart_eq_ref = true;
    }
    num_output_rows = std::min(num_output_rows, 1.0);
  }

  const double cost =
      EstimateCostForRefAccess(m_thd, table, key_idx, num_output_rows);

  AccessPath path;
  if (single_row) {
    path.type = AccessPath::EQ_REF;
    path.eq_ref().table = table;
    path.eq_ref().ref = ref;

    // We could set really any ordering here if we wanted to.
    // It's very rare that it should matter, though.
    path.ordering_state = m_orderings->SetOrder(ordering_idx);
  } else {
    path.type = AccessPath::REF;
    path.ref().table = table;
    path.ref().ref = ref;
    path.ref().reverse = reverse;

    // TODO(sgunders): Some storage engines, like NDB, can benefit from
    // use_order = false if we don't actually need the ordering later.
    // Consider adding a cost model for this, and then proposing both
    // with and without order.
    path.ordering_state = m_orderings->SetOrder(ordering_idx);
    path.ref().use_order = (path.ordering_state != 0);
  }

  path.num_output_rows_before_filter = num_output_rows;
  path.cost_before_filter = cost;
  path.init_cost = path.init_once_cost = 0.0;
  path.parameter_tables = GetNodeMapFromTableMap(
      parameter_tables & ~table->pos_in_table_list->map(),
      m_graph->table_num_to_node_num);

  if (IsBitSet(node_idx, m_immediate_update_delete_candidates)) {
    path.immediate_update_delete_table = node_idx;
    // Disallow immediate update on the key being looked up for REF_OR_NULL and
    // REF. It might be safe to update the key on which the REF lookup is
    // performed, but we follow the lead of the old optimizer and don't try it,
    // since we don't know how the engine behaves if doing an index lookup on a
    // changing index.
    //
    // EQ_REF should be safe, though. I has at most one matching row, with a
    // constant lookup value as this is the first table. So this row won't be
    // seen a second time; the iterator won't even try a second read.
    if (path.type != AccessPath::EQ_REF && IsUpdateStatement(m_thd) &&
        is_key_used(table, key_idx, table->write_set)) {
      path.immediate_update_delete_table = -1;
    }
  }

  ProposeAccessPathForIndex(
      node_idx, std::move(applied_predicates), std::move(subsumed_predicates),
      force_num_output_rows_after_filter, key->name, &path);
  return false;
}

/**
  Do we have a sargable predicate which checks if "field" is equal to a
  constant?
 */
bool HasConstantEqualityForField(
    const Mem_root_array<SargablePredicate> &sargable_predicates,
    const Field *field) {
  return std::any_of(sargable_predicates.begin(), sargable_predicates.end(),
                     [field](const SargablePredicate &sp) {
                       return sp.other_side->const_for_execution() &&
                              field->eq(sp.field);
                     });
}

/**
  Proposes all possible unique index lookups using only constants on the given
  table. This is done before exploring any other plans for the table, in order
  to allow early return for point selects, which do not benefit from using other
  access methods.

  @param node_idx    The table to propose index lookups for.
  @param[out] found  Set to true if a unique index lookup is proposed.
  @return True on error.
 */
bool CostingReceiver::ProposeAllUniqueIndexLookupsWithConstantKey(int node_idx,
                                                                  bool *found) {
  const Mem_root_array<SargablePredicate> &sargable_predicates =
      m_graph->nodes[node_idx].sargable_predicates;

  if (sargable_predicates.empty()) {
    return false;
  }

  TABLE *const table = m_graph->nodes[node_idx].table;
  assert(!table->pos_in_table_list->is_recursive_reference());
  assert(!Overlaps(table->file->ha_table_flags(), HA_NO_INDEX_ACCESS));

  for (const ActiveIndexInfo &index_info : *m_active_indexes) {
    if (index_info.table != table) {
      continue;
    }

    const KEY *const key = &table->key_info[index_info.key_idx];

    // EQ_REF is only possible on UNIQUE non-FULLTEXT indexes.
    if (!Overlaps(key->flags, HA_NOSAME) || Overlaps(key->flags, HA_FULLTEXT)) {
      continue;
    }

    const size_t num_key_parts = key->user_defined_key_parts;
    if (num_key_parts > sargable_predicates.size()) {
      // There are not enough predicates to satisfy this key with constants.
      continue;
    }

    if (std::all_of(key->key_part, key->key_part + num_key_parts,
                    [&sargable_predicates](const KEY_PART_INFO &key_part) {
                      return HasConstantEqualityForField(sargable_predicates,
                                                         key_part.field);
                    })) {
      *found = true;
      if (ProposeRefAccess(
              table, node_idx, index_info.key_idx,
              /*force_num_output_rows_after_filter=*/-1.0, /*reverse=*/false,
              /*allowed_parameter_tables=*/0,
              m_orderings->RemapOrderingIndex(index_info.forward_order))) {
        return true;
      }
    }
  }

  return false;
}

void CostingReceiver::ProposeAccessPathForIndex(
    int node_idx, OverflowBitset applied_predicates,
    OverflowBitset subsumed_predicates,
    double force_num_output_rows_after_filter,
    const char *description_for_trace, AccessPath *path) {
  MutableOverflowBitset applied_sargable_join_predicates_tmp =
      applied_predicates.Clone(m_thd->mem_root);
  applied_sargable_join_predicates_tmp.ClearBits(0,
                                                 m_graph->num_where_predicates);
  OverflowBitset applied_sargable_join_predicates =
      std::move(applied_sargable_join_predicates_tmp);

  MutableOverflowBitset subsumed_sargable_join_predicates_tmp =
      subsumed_predicates.Clone(m_thd->mem_root);
  subsumed_sargable_join_predicates_tmp.ClearBits(
      0, m_graph->num_where_predicates);
  OverflowBitset subsumed_sargable_join_predicates =
      std::move(subsumed_sargable_join_predicates_tmp);
  for (bool materialize_subqueries : {false, true}) {
    FunctionalDependencySet new_fd_set;
    ApplyPredicatesForBaseTable(node_idx, applied_predicates,
                                subsumed_predicates, materialize_subqueries,
                                path, &new_fd_set);

    if (force_num_output_rows_after_filter >= 0.0) {
      path->set_num_output_rows(force_num_output_rows_after_filter);
    }

    path->ordering_state =
        m_orderings->ApplyFDs(path->ordering_state, new_fd_set);
    path->applied_sargable_join_predicates() = OverflowBitset::Or(
        m_thd->mem_root, path->applied_sargable_join_predicates(),
        applied_sargable_join_predicates);
    path->subsumed_sargable_join_predicates() = OverflowBitset::Or(
        m_thd->mem_root, path->subsumed_sargable_join_predicates(),
        subsumed_sargable_join_predicates);
    ProposeAccessPathWithOrderings(
        TableBitmap(node_idx), new_fd_set, /*obsolete_orderings=*/0, path,
        materialize_subqueries ? "mat. subq" : description_for_trace);

    if (!Overlaps(path->filter_predicates,
                  m_graph->materializable_predicates)) {
      // Nothing to try to materialize.
      break;
    }
  }
}

bool CostingReceiver::ProposeTableScan(
    TABLE *table, int node_idx, double force_num_output_rows_after_filter) {
  Table_ref *tl = table->pos_in_table_list;
  AccessPath path;
  if (tl->is_recursive_reference()) {
    path.type = AccessPath::FOLLOW_TAIL;
    path.follow_tail().table = table;
    assert(forced_leftmost_table == 0);  // There can only be one, naturally.
    forced_leftmost_table = NodeMap{1} << node_idx;
  } else {
    path.type = AccessPath::TABLE_SCAN;
    path.table_scan().table = table;
  }
  path.count_examined_rows = true;
  path.ordering_state = 0;

  // Doing at least one table scan (this one), so mark the query as such.
  // TODO(sgunders): Move out when we get more types and this access path could
  // be replaced by something else.
  m_thd->set_status_no_index_used();

  const double num_output_rows = table->file->stats.records;
  const double cost = table->file->table_scan_cost().total_cost();

  path.num_output_rows_before_filter = num_output_rows;
  path.init_cost = path.init_once_cost = 0.0;
  path.cost_before_filter = path.cost = cost;
  if (IsBitSet(node_idx, m_immediate_update_delete_candidates)) {
    path.immediate_update_delete_table = node_idx;
    // This is a table scan, but it might be using the clustered key under the
    // cover. If so, don't allow immediate update if it's modifying the
    // primary key.
    if (IsUpdateStatement(m_thd) &&
        Overlaps(table->file->ha_table_flags(), HA_PRIMARY_KEY_IN_READ_INDEX) &&
        !table->s->is_missing_primary_key() &&
        is_key_used(table, table->s->primary_key, table->write_set)) {
      path.immediate_update_delete_table = -1;
    }
  }

  // See if this is an information schema table that must be filled in before
  // we scan.
  if (tl->schema_table != nullptr && tl->schema_table->fill_table) {
    // TODO(sgunders): We don't need to allocate materialize_path on the
    // MEM_ROOT.
    AccessPath *new_path = new (m_thd->mem_root) AccessPath(path);
    AccessPath *materialize_path =
        NewMaterializeInformationSchemaTableAccessPath(m_thd, new_path, tl,
                                                       /*condition=*/nullptr);
    materialize_path->num_output_rows_before_filter = num_output_rows;
    materialize_path->init_cost = path.cost;       // Rudimentary.
    materialize_path->init_once_cost = path.cost;  // Rudimentary.
    materialize_path->cost_before_filter = path.cost;
    materialize_path->cost = path.cost;
    materialize_path->filter_predicates = path.filter_predicates;
    materialize_path->delayed_predicates = path.delayed_predicates;
    new_path->filter_predicates.Clear();
    new_path->delayed_predicates.Clear();
    new_path->set_num_output_rows(num_output_rows);

    assert(!tl->uses_materialization());
    path = *materialize_path;
    assert(path.cost >= 0.0);
  } else if (tl->uses_materialization()) {
    // Move the path to stable storage, since we'll be referring to it.
    AccessPath *stable_path = new (m_thd->mem_root) AccessPath(path);

    // TODO(sgunders): We don't need to allocate materialize_path on the
    // MEM_ROOT.
    AccessPath *materialize_path;
    const char *always_empty_cause = nullptr;
    if (tl->is_table_function()) {
      materialize_path = NewMaterializedTableFunctionAccessPath(
          m_thd, table, tl->table_function, stable_path);
      CopyBasicProperties(*stable_path, materialize_path);
      materialize_path->cost_before_filter = materialize_path->init_cost =
          materialize_path->init_once_cost = materialize_path->cost;
      materialize_path->num_output_rows_before_filter = num_output_rows;

      materialize_path->parameter_tables = GetNodeMapFromTableMap(
          tl->table_function->used_tables() & ~PSEUDO_TABLE_BITS,
          m_graph->table_num_to_node_num);
      if (Overlaps(tl->table_function->used_tables(),
                   OUTER_REF_TABLE_BIT | RAND_TABLE_BIT)) {
        // Make sure the table function is never hashed, ever.
        materialize_path->parameter_tables |= RAND_TABLE_BIT;
      }
    } else {
      // If the derived table is known to be always empty, we may be able to
      // optimize away parts of the outer query block too.
      if (const AccessPath *derived_table_path =
              tl->derived_query_expression()->root_access_path();
          derived_table_path != nullptr &&
          derived_table_path->type == AccessPath::ZERO_ROWS) {
        always_empty_cause = derived_table_path->zero_rows().cause;
      }

      if (always_empty_cause != nullptr &&
          !IsBitSet(tl->tableno(), m_graph->tables_inner_to_outer_or_anti)) {
        // The entire query block can be optimized away. Stop planning.
        m_query_block->join->zero_result_cause = always_empty_cause;
        return true;
      }

      bool rematerialize = Overlaps(tl->derived_query_expression()->uncacheable,
                                    UNCACHEABLE_DEPENDENT);
      if (tl->common_table_expr()) {
        // Handled in clear_corr_derived_tmp_tables(), not here.
        rematerialize = false;
      }
      materialize_path = GetAccessPathForDerivedTable(
          m_thd, tl, table, rematerialize,
          /*invalidators=*/nullptr, m_need_rowid, stable_path);
      // Handle LATERAL.
      materialize_path->parameter_tables =
          GetNodeMapFromTableMap(tl->derived_query_expression()->m_lateral_deps,
                                 m_graph->table_num_to_node_num);

      // If we don't need row IDs, we also don't care about row ID safety.
      // This keeps us from retaining many extra unneeded paths.
      if (!m_need_rowid) {
        materialize_path->safe_for_rowid = AccessPath::SAFE;
      }
    }

    materialize_path->filter_predicates = path.filter_predicates;
    materialize_path->delayed_predicates = path.delayed_predicates;
    stable_path->filter_predicates.Clear();
    stable_path->delayed_predicates.Clear();
    path = *materialize_path;
    assert(path.cost >= 0.0);

    if (always_empty_cause != nullptr) {
      // The entire query block cannot be optimized away, only the inner block
      // for the derived table. But the materialization step is unnecessary, so
      // return a ZERO_ROWS path directly for the derived table. This also
      // allows subtrees of this query block to be removed (if the derived table
      // is inner-joined to some other tables).
      path = *NewZeroRowsAccessPath(
          m_thd, new (m_thd->mem_root) AccessPath(path), always_empty_cause);
    }
  }
  assert(path.cost >= 0.0);

  ProposeAccessPathForBaseTable(node_idx, force_num_output_rows_after_filter,
                                /*description_for_trace=*/"", &path);
  return false;
}

bool CostingReceiver::ProposeIndexScan(
    TABLE *table, int node_idx, double force_num_output_rows_after_filter,
    unsigned key_idx, bool reverse, int ordering_idx) {
  AccessPath path;
  path.type = AccessPath::INDEX_SCAN;
  path.index_scan().table = table;
  path.index_scan().idx = key_idx;
  path.index_scan().use_order = true;
  path.index_scan().reverse = reverse;
  path.count_examined_rows = true;
  path.ordering_state = m_orderings->SetOrder(ordering_idx);

  double num_output_rows = table->file->stats.records;
  double cost;

  // If a table scan and a primary key scan is the very same thing,
  // they should also have the same cost. However, read_cost()
  // is based on number of rows, and table_scan_cost() is based on
  // on-disk size, so it's complete potluck which one gives the
  // higher number. We force primary scan cost to be table scan cost
  // plus an arbitrary 0.1% factor, so that we will always prefer
  // table scans if we don't need the ordering (both for user experience,
  // and in case there _is_ a performance difference in the storage
  // engine), but primary index scans otherwise.
  //
  // Note that this will give somewhat more access paths than is
  // required in some cases.
  if (table->s->primary_key == key_idx &&
      table->file->primary_key_is_clustered()) {
    cost = table->file->table_scan_cost().total_cost() * 1.001;
  } else if (table->covering_keys.is_set(key_idx)) {
    // The index is covering, so we can do an index-only scan.
    cost =
        table->file->index_scan_cost(key_idx, /*ranges=*/1.0, num_output_rows)
            .total_cost();
  } else {
    cost = table->file->read_cost(key_idx, /*ranges=*/1.0, num_output_rows)
               .total_cost();
  }

  path.num_output_rows_before_filter = num_output_rows;
  path.init_cost = path.init_once_cost = 0.0;
  path.cost_before_filter = path.cost = cost;
  if (IsBitSet(node_idx, m_immediate_update_delete_candidates)) {
    path.immediate_update_delete_table = node_idx;
    // Don't allow immediate update of the key that is being scanned.
    if (IsUpdateStatement(m_thd) &&
        is_key_used(table, key_idx, table->write_set)) {
      path.immediate_update_delete_table = -1;
    }
  }

  ProposeAccessPathForBaseTable(node_idx, force_num_output_rows_after_filter,
                                table->key_info[key_idx].name, &path);
  return false;
}

// Checks if a given predicate can be subsumed by a full-text index. It can
// be subsumed if it returns TRUE for all documents returned by the full-text
// index, and FALSE for all other documents. Since a full-text index scan
// returns the documents with a positive score, predicates that are either a
// standalone call to MATCH, a comparison of MATCH > 0, or a comparison of
// 0 < MATCH, are considered subsumable.
//
// We assume that this function is only called on predicates for which
// IsSargableFullTextIndexPredicate() has returned true, so that we
// already know the predicate is a standalone MATCH function or a <, <=, >
// or >= comparing match to a constant.
bool IsSubsumableFullTextPredicate(Item_func *condition) {
  switch (condition->functype()) {
    case Item_func::MATCH_FUNC: {
      // WHERE MATCH (col) AGAINST ('search string') is subsumable.
      return true;
    }
    case Item_func::GT_FUNC: {
      // WHERE MATCH (col) AGAINST ('search string') > 0 is subsumable.
      assert(is_function_of_type(condition->get_arg(0), Item_func::FT_FUNC));
      assert(condition->get_arg(1)->const_item());
      const double value = condition->get_arg(1)->val_real();
      assert(!condition->get_arg(1)->null_value);
      return value == 0;
    }
    case Item_func::LT_FUNC: {
      // WHERE 0 < MATCH (col) AGAINST ('search string') subsumable.
      assert(condition->get_arg(0)->const_item());
      assert(is_function_of_type(condition->get_arg(1), Item_func::FT_FUNC));
      const double value = condition->get_arg(0)->val_real();
      assert(!condition->get_arg(0)->null_value);
      return value == 0;
    }
    case Item_func::GE_FUNC:
      // WHERE MATCH >= const is not subsumable, but assert the predicate is on
      // the expected form.
      assert(is_function_of_type(condition->get_arg(0), Item_func::FT_FUNC));
      assert(condition->get_arg(1)->const_item());
      return false;
    case Item_func::LE_FUNC:
      // WHERE const <= MATCH is not subsumable, but assert the predicate is on
      // the expected form.
      assert(condition->get_arg(0)->const_item());
      assert(is_function_of_type(condition->get_arg(1), Item_func::FT_FUNC));
      return false;
    default:
      // Not a sargable full-text predicate, so we don't expect to be called on
      // it.
      assert(false);
      return false;
  }
}

// Assuming that we have chosen a full-text index scan on the given predicate,
// can we pass the LIMIT of the query block as a hint to the storage engine?
//
// We can do this if we know that the number of rows seen before the LIMIT
// clause is processed, is the same number of rows as returned by the index
// scan. This is the case when:
//
// 1) It is a single-table query. No joins.
//
// 2) There is no aggregation or DISTINCT which could reduce the number of rows.
//
// 3) There is no filtering of the rows returned from the index. That is, there
// is no HAVING clause, and the WHERE clause contains no predicates apart from
// those that can be subsumed by the index.
bool IsLimitHintPushableToFullTextSearch(const Item_func_match *match,
                                         const JoinHypergraph &graph,
                                         uint64_t fulltext_predicates) {
  const Query_block *query_block = graph.query_block();
  assert(query_block->has_ft_funcs());

  // The query has a LIMIT clause.
  if (query_block->join->m_select_limit == HA_POS_ERROR) {
    return false;
  }

  // A single table, no joins.
  if (graph.nodes.size() != 1) {
    return false;
  }

  // No aggregation, DISTINCT or HAVING.
  if (query_block->is_grouped() || query_block->is_distinct() ||
      query_block->join->having_cond != nullptr) {
    return false;
  }

  // The WHERE clause contains full-text predicates only.
  if (fulltext_predicates != BitsBetween(0, graph.predicates.size())) {
    return false;
  }

  // And all the full-text predicates must be subsumed by the index scan.
  for (const Predicate &predicate : graph.predicates) {
    Item_func_match *cond = GetSargableFullTextPredicate(predicate);
    if (cond != match || !IsSubsumableFullTextPredicate(
                             down_cast<Item_func *>(predicate.condition))) {
      return false;
    }
  }

  return true;
}

// Propose full-text index scans for all full-text predicates found in the
// WHERE clause, if any. If an interesting order can be satisfied by an ordered
// full-text index scan using one of the predicates, propose an ordered scan.
// Otherwise, propose an unordered scan. (For completeness, we should have
// proposed both an ordered and an unordered scan when we have an interesting
// order. But we don't have a good estimate for the extra cost of making the
// scan ordered, so we only propose the ordered scan for simplicity. InnoDB, for
// example, uses an ordered scan regardless of whether we request it, so an
// explicitly ordered scan is no more expensive than an implicitly ordered scan,
// and it could potentially avoid a sort higher up in the query plan.)
bool CostingReceiver::ProposeAllFullTextIndexScans(
    TABLE *table, int node_idx, double force_num_output_rows_after_filter) {
  for (const FullTextIndexInfo &info : *m_fulltext_searches) {
    if (info.match->table_ref != table->pos_in_table_list) {
      continue;
    }

    // Propose a full-text index scan for each predicate that uses the MATCH
    // function given by info.match. Note that several predicates can use the
    // same MATCH function, due to Item_func_match's linking equivalent callers
    // to one canonical Item_func_match object (via set_master()/get_master()).
    //
    // For example, we may have:
    //
    //   WHERE MATCH (col) AGAINST ('string') AND
    //         MATCH (col) AGAINST ('string') > 0.3
    //
    // Both MATCH invocations have the same canonical Item_func_match object,
    // since they have the same set of columns and search for the same string.
    // In this case, we want to propose two index scans, and let the optimizer
    // pick the one that gives the plan with the lowest estimated cost.
    for (size_t i : BitsSetIn(m_sargable_fulltext_predicates)) {
      Item_func_match *match =
          GetSargableFullTextPredicate(m_graph->predicates[i]);
      assert(match != nullptr);
      if (match != info.match) continue;
      if (ProposeFullTextIndexScan(table, node_idx, match, i, info.order,
                                   force_num_output_rows_after_filter)) {
        return true;
      }
    }

    // Even if we have no predicates, we may use a full-text index scan if it is
    // possible to pass the LIMIT clause to the index scan, and the LIMIT is no
    // greater than the number of documents returned by the index scan. We only
    // do this if the index scan produces rows in an interesting order. And only
    // if the storage engine supports the extended full-text API, which is
    // required for counting the matches in the index.
    if (m_graph->predicates.empty() && info.order != 0 &&
        IsLimitHintPushableToFullTextSearch(info.match, *m_graph,
                                            m_sargable_fulltext_predicates) &&
        Overlaps(table->file->ha_table_flags(), HA_CAN_FULLTEXT_EXT)) {
      // The full-text function must be initialized before get_count() is
      // called. Even though we call init_search() on it again after the final
      // plan has been chosen, this does not mean the search is performed twice.
      if (info.match->init_search(m_thd)) {
        return true;
      }
      if (m_query_block->join->m_select_limit <= info.match->get_count()) {
        if (ProposeFullTextIndexScan(table, node_idx, info.match,
                                     /*predicate_idx=*/-1, info.order,
                                     force_num_output_rows_after_filter)) {
          return true;
        }
      }
    }
  }

  return false;
}

bool CostingReceiver::ProposeFullTextIndexScan(
    TABLE *table, int node_idx, Item_func_match *match, int predicate_idx,
    int ordering_idx, double force_num_output_rows_after_filter) {
  const unsigned key_idx = match->key;
  Index_lookup *ref = new (m_thd->mem_root) Index_lookup;
  if (init_ref(m_thd, /*keyparts=*/1, /*length=*/0, key_idx, ref)) {
    return true;
  }
  ref->items[0] = match->key_item();

  const Predicate *predicate =
      predicate_idx == -1 ? nullptr : &m_graph->predicates[predicate_idx];
  assert(predicate_idx == -1 ||
         match == GetSargableFullTextPredicate(*predicate));

  MutableOverflowBitset applied_predicates{m_thd->mem_root,
                                           m_graph->predicates.size()};
  MutableOverflowBitset subsumed_predicates{m_thd->mem_root,
                                            m_graph->predicates.size()};
  double num_output_rows;
  double num_output_rows_from_index;
  if (predicate == nullptr) {
    // We have no predicate. The index is used only for ordering. We only do
    // this if we have a limit. Note that we keep the full row number count
    // here, to get consistent results; we only apply the limit for cost
    // calculations.
    assert(m_query_block->join->m_select_limit != HA_POS_ERROR);
    num_output_rows = table->file->stats.records;
    num_output_rows_from_index =
        min(table->file->stats.records, m_query_block->join->m_select_limit);
  } else {
    num_output_rows_from_index =
        table->file->stats.records * predicate->selectivity;
    if (TableBitmap(node_idx) == predicate->total_eligibility_set) {
      applied_predicates.SetBit(predicate_idx);
      if (IsSubsumableFullTextPredicate(
              down_cast<Item_func *>(predicate->condition))) {
        // The predicate can be fully subsumed by the index. Apply the full
        // selectivity on the index scan and mark the predicate as subsumed.
        subsumed_predicates.SetBit(predicate_idx);
      }

      num_output_rows = num_output_rows_from_index;
    } else {
      // We have a MATCH() predicate pushed down to a table that is on the inner
      // side of an outer join. It needs to be re-checked later, so we don't set
      // applied_predicates (and thus, we also cannot set subsumed_predicates).
      // In reality, we've done all the filtering already, but if we said that,
      // we'd get an inconsistent row count. This is one of the few cases where
      // inconsistent row counts are actually possible to get, but given that
      // the situation is so rare (and would have been even rarer if MATCH()
      // conditions triggered outer-to-inner conversions through
      // not_null_tables(), which it cannot as long as MATCH() on NULL returns
      // 0.0 instead of NULL), we opt for the lesser evil and delay the
      // selectivity application to the point of the WHERE().
      num_output_rows = table->file->stats.records;
    }
  }

  const double cost = EstimateCostForRefAccess(m_thd, table, key_idx,
                                               num_output_rows_from_index);

  const LogicalOrderings::StateIndex ordering_state =
      m_orderings->SetOrder(ordering_idx);

  const bool use_order = ordering_state != 0;

  AccessPath *path = NewFullTextSearchAccessPath(
      m_thd, table, ref, match, use_order,
      IsLimitHintPushableToFullTextSearch(match, *m_graph,
                                          m_sargable_fulltext_predicates),
      /*count_examined_rows=*/true);
  path->set_num_output_rows(num_output_rows);
  path->num_output_rows_before_filter = num_output_rows;
  path->cost = path->cost_before_filter = cost;
  path->init_cost = path->init_once_cost = 0;
  path->ordering_state = ordering_state;
  if (IsBitSet(node_idx, m_immediate_update_delete_candidates)) {
    path->immediate_update_delete_table = node_idx;
    // Don't allow immediate update of the key that is being scanned.
    if (IsUpdateStatement(m_thd) &&
        is_key_used(table, key_idx, table->write_set)) {
      path->immediate_update_delete_table = -1;
    }
  }

  ProposeAccessPathForIndex(
      node_idx, std::move(applied_predicates), std::move(subsumed_predicates),
      force_num_output_rows_after_filter, table->key_info[key_idx].name, path);
  return false;
}

void CostingReceiver::ProposeAccessPathForBaseTable(
    int node_idx, double force_num_output_rows_after_filter,
    const char *description_for_trace, AccessPath *path) {
  for (bool materialize_subqueries : {false, true}) {
    FunctionalDependencySet new_fd_set;
    ApplyPredicatesForBaseTable(
        node_idx,
        /*applied_predicates=*/
        MutableOverflowBitset{m_thd->mem_root, m_graph->predicates.size()},
        /*subsumed_predicates=*/
        MutableOverflowBitset{m_thd->mem_root, m_graph->predicates.size()},
        materialize_subqueries, path, &new_fd_set);
    path->ordering_state =
        m_orderings->ApplyFDs(path->ordering_state, new_fd_set);
    if (force_num_output_rows_after_filter >= 0.0) {
      path->set_num_output_rows(force_num_output_rows_after_filter);
    }
    ProposeAccessPathWithOrderings(
        TableBitmap(node_idx), new_fd_set, /*obsolete_orderings=*/0, path,
        materialize_subqueries ? "mat. subq" : description_for_trace);

    if (!Overlaps(path->filter_predicates,
                  m_graph->materializable_predicates)) {
      // Nothing to try to materialize.
      return;
    }
  }
}

/**
  See which predicates that apply to this table. Some can be applied
  right away, some require other tables first and must be delayed.

  @param node_idx Index of the base table in the nodes array.
  @param applied_predicates Bitmap of predicates that are already
    applied by means of ref access, and should not be recalculated selectivity
    for.
  @param subsumed_predicates Bitmap of predicates that are applied
    by means of ref access and do not need to rechecked. Overrides
    applied_predicates.
  @param materialize_subqueries If true, any subqueries in the
    predicate should be materialized. (If there are multiple ones,
    this is an all-or-nothing decision, for simplicity.)
  @param [in,out] path The access path to apply the predicates to.
    Note that if materialize_subqueries is true, a FILTER access path
    will be inserted (overwriting "path", although a copy of it will
    be set as a child), as AccessPath::filter_predicates always assumes
    non-materialized subqueries.
 */
void CostingReceiver::ApplyPredicatesForBaseTable(
    int node_idx, OverflowBitset applied_predicates,
    OverflowBitset subsumed_predicates, bool materialize_subqueries,
    AccessPath *path, FunctionalDependencySet *new_fd_set) {
  double materialize_cost = 0.0;

  const NodeMap my_map = TableBitmap(node_idx);
  path->set_num_output_rows(path->num_output_rows_before_filter);
  path->cost = path->cost_before_filter;
  MutableOverflowBitset filter_predicates{m_thd->mem_root,
                                          m_graph->predicates.size()};
  MutableOverflowBitset delayed_predicates{m_thd->mem_root,
                                           m_graph->predicates.size()};
  new_fd_set->reset();
  for (size_t i = 0; i < m_graph->num_where_predicates; ++i) {
    if (IsBitSet(i, subsumed_predicates)) {
      // Apply functional dependencies for the base table, but no others;
      // this ensures we get the same functional dependencies set no matter what
      // access path we choose. (The ones that refer to multiple tables,
      // which are fairly rare, are not really relevant before the other
      // table(s) have been joined in.)
      if (m_graph->predicates[i].total_eligibility_set == my_map) {
        *new_fd_set |= m_graph->predicates[i].functional_dependencies;
      } else {
        // We have a WHERE predicate that refers to multiple tables,
        // that we can subsume as if it were a join condition
        // (perhaps because it was identical to an actual join condition).
        // The other side of the join will mark it as delayed, so we
        // need to do so, too. Otherwise, we would never apply the
        // associated functional dependency at the right time.
        delayed_predicates.SetBit(i);
      }
      continue;
    }
    // TODO(sgunders): We should also allow conditions that depend on
    // parameterized tables (and also touch this table, of course). See bug
    // #33477822.
    if (m_graph->predicates[i].total_eligibility_set == my_map) {
      filter_predicates.SetBit(i);
      FilterCost cost =
          EstimateFilterCost(m_thd, path->num_output_rows(),
                             m_graph->predicates[i].contained_subqueries);
      if (materialize_subqueries) {
        path->cost += cost.cost_if_materialized;
        materialize_cost += cost.cost_to_materialize;
      } else {
        path->cost += cost.cost_if_not_materialized;
        path->init_cost += cost.init_cost_if_not_materialized;
      }
      if (IsBitSet(i, applied_predicates)) {
        // We already factored in this predicate when calculating
        // the selectivity of the ref access, so don't do it again.
      } else {
        path->set_num_output_rows(path->num_output_rows() *
                                  m_graph->predicates[i].selectivity);
      }
      *new_fd_set |= m_graph->predicates[i].functional_dependencies;
    } else if (Overlaps(m_graph->predicates[i].total_eligibility_set, my_map)) {
      delayed_predicates.SetBit(i);
    }
  }
  path->filter_predicates = std::move(filter_predicates);
  path->delayed_predicates = std::move(delayed_predicates);

  if (materialize_subqueries) {
    CommitBitsetsToHeap(path);
    ExpandSingleFilterAccessPath(m_thd, path, m_query_block->join,
                                 m_graph->predicates,
                                 m_graph->num_where_predicates);
    assert(path->type == AccessPath::FILTER);
    path->filter().materialize_subqueries = true;
    path->cost += materialize_cost;  // Will be subtracted back for rescans.
    path->init_cost += materialize_cost;
    path->init_once_cost += materialize_cost;
  }
}

/**
  Checks if the table given by "node_idx" has all its lateral dependencies
  satisfied by the set of tables given by "tables".
 */
bool LateralDependenciesAreSatisfied(int node_idx, NodeMap tables,
                                     const JoinHypergraph &graph) {
  const Table_ref *table_ref = graph.nodes[node_idx].table->pos_in_table_list;

  if (table_ref->is_derived()) {
    const NodeMap lateral_deps = GetNodeMapFromTableMap(
        table_ref->derived_query_expression()->m_lateral_deps,
        graph.table_num_to_node_num);
    return IsSubset(lateral_deps, tables);
  }

  // Not a lateral derived table, so there are no lateral dependencies, and
  // hence all lateral dependencies are satisfied.
  return true;
}

/**
  Find the set of tables we can join directly against, given that we have the
  given set of tables on one of the sides (effectively the same concept as
  DPhyp's “neighborhood”). Note that having false negatives here is fine
  (it will only make DisallowParameterizedJoinPath() slightly less effective),
  but false positives is not (it may disallow valid parameterized paths,
  ultimately even making LATERAL queries impossible to plan). Thus, we need
  to check conflict rules, and our handling of hyperedges with more than one
  table on the other side may also be a bit too strict (this may need
  adjustments when we get FULL OUTER JOIN).

  If this calculation turns out to be slow, we could probably cache it in
  AccessPathSet, or even try to build it incrementally.
 */
NodeMap FindReachableTablesFrom(NodeMap tables, const JoinHypergraph &graph) {
  const Mem_root_array<Node> &nodes = graph.graph.nodes;
  const Mem_root_array<Hyperedge> &edges = graph.graph.edges;

  NodeMap reachable = 0;
  for (int node_idx : BitsSetIn(tables)) {
    for (int neighbor_idx :
         BitsSetIn(nodes[node_idx].simple_neighborhood & ~reachable)) {
      if (LateralDependenciesAreSatisfied(neighbor_idx, tables, graph)) {
        reachable |= TableBitmap(neighbor_idx);
      }
    }
    for (int edge_idx : nodes[node_idx].complex_edges) {
      if (IsSubset(edges[edge_idx].left, tables)) {
        NodeMap others = edges[edge_idx].right & ~tables;
        if (IsSingleBitSet(others) && !Overlaps(others, reachable) &&
            PassesConflictRules(tables, graph.edges[edge_idx / 2].expr) &&
            LateralDependenciesAreSatisfied(FindLowestBitSet(others), tables,
                                            graph)) {
          reachable |= others;
        }
      }
    }
  }
  return reachable;
}

// Returns whether the given set of parameter tables is partially, but not
// fully, resolved by joining towards the other side.
bool PartiallyResolvedParameterization(NodeMap parameter_tables,
                                       NodeMap other_side) {
  return (parameter_tables & ~other_side) != 0 &&
         (parameter_tables & ~other_side) != parameter_tables;
}

/**
  Decide whether joining the two given paths would create a disallowed
  parameterized path. Parameterized paths are disallowed if they delay
  joining in their parameterizations without reason (ie., they could
  join in a parameterization right away, but don't). This is a trick
  borrowed from Postgres, which essentially forces inner-join ref-lookup
  plans to be left-deep (since such plans never gain anything from being
  bushy), reducing the search space significantly without compromising
  plan quality.
 */
bool DisallowParameterizedJoinPath(AccessPath *left_path,
                                   AccessPath *right_path, NodeMap left,
                                   NodeMap right, NodeMap left_reachable,
                                   NodeMap right_reachable) {
  const NodeMap left_parameters = left_path->parameter_tables & ~RAND_TABLE_BIT;
  const NodeMap right_parameters =
      right_path->parameter_tables & ~RAND_TABLE_BIT;

  if (IsSubset(left_parameters | right_parameters, left | right)) {
    // Not creating a parameterized path, so it's always fine.
    return false;
  }

  if (!Overlaps(right_parameters, right_reachable) &&
      !Overlaps(left_parameters, left_reachable)) {
    // Either left or right cannot resolve any of their parameterizations yet
    // (e.g., we're still on the inside of an outer join that we cannot
    // finish yet), so we cannot avoid keeping them if we want to use index
    // lookups here at all.
    return false;
  }

  // If the outer table partially, but not fully, resolves the inner table's
  // parameterization, we still allow it (otherwise, we could not have
  // multi-part index lookups where the keyparts come from different tables).
  // This is the so-called “star-schema exception”.
  //
  // We need to check both ways, in case we try to swap them for a hash join.
  // Only one of these will ever be true in any given join anyway (joins where
  // we try to resolve the outer path's parameterizations with the inner one
  // are disallowed), so we do not allow more than is required.
  if (PartiallyResolvedParameterization(left_parameters, right) ||
      PartiallyResolvedParameterization(right_parameters, left)) {
    return false;
  }

  // Disallow this join; left or right (or both) should resolve their
  // parameterizations before we try to combine them.
  return true;
}

/**
  Checks if the result of a join is empty, given that it is known that one or
  both of the join legs always produces an empty result.
 */
bool IsEmptyJoin(const RelationalExpression::Type join_type, bool left_is_empty,
                 bool right_is_empty) {
  switch (join_type) {
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
    case RelationalExpression::SEMIJOIN:
      // If either side of an inner join or a semijoin is empty, the result of
      // the join is also empty.
      return left_is_empty || right_is_empty;
    case RelationalExpression::LEFT_JOIN:
    case RelationalExpression::ANTIJOIN:
      // If the outer side of a left join or an antijoin is empty, the result of
      // the join is also empty.
      return left_is_empty;
    case RelationalExpression::FULL_OUTER_JOIN:
      // If both sides of a full outer join are empty, the result of the join is
      // also empty.
      return left_is_empty && right_is_empty;
    case RelationalExpression::TABLE:
    case RelationalExpression::MULTI_INNER_JOIN:
      break;
  }
  assert(false);
  return false;
}

/**
  If the ON clause of a left join only references tables on the right side of
  the join, pushing the condition into the right side is a valid thing to do. If
  such conditions are not pushed down for some reason, and are left in the ON
  clause, HeatWave might reject the query. This happens if the entire join
  condition is degenerate and only references the right side. Such conditions
  are most commonly seen in queries that have gone through subquery_to_derived
  transformation.

  This limitation is worked around here by moving the degenerate join condition
  from the join predicate to a filter path on top of the right path. This is
  only done for secondary storage engines.

  TODO(khatlen): If HeatWave gets capable of processing queries with such
  conditions, this workaround should be removed.
 */
void MoveDegenerateJoinConditionToFilter(THD *thd, Query_block *query_block,
                                         const JoinPredicate **edge,
                                         AccessPath **right_path) {
  assert(SecondaryEngineHandlerton(thd) != nullptr);
  const RelationalExpression *expr = (*edge)->expr;
  assert(expr->type == RelationalExpression::LEFT_JOIN);

  // If we have a degenerate join condition which references some tables on the
  // inner side of the join, and no tables on the outer side, we are allowed to
  // filter on that condition before the join. Do so
  if (expr->conditions_used_tables == 0 ||
      !IsSubset(expr->conditions_used_tables, expr->right->tables_in_subtree)) {
    return;
  }

  // If the join condition only references tables on one side of the join, there
  // cannot be any equijoin conditions, as they reference both sides.
  assert(expr->equijoin_conditions.empty());
  assert(!expr->join_conditions.empty());

  // Create a filter on top of right_path. This filter contains the entire
  // (degenerate) join condition.
  List<Item> conds;
  for (Item *cond : expr->join_conditions) {
    conds.push_back(cond);
  }
  Item *filter_cond = CreateConjunction(&conds);
  AccessPath *filter_path = NewFilterAccessPath(thd, *right_path, filter_cond);
  CopyBasicProperties(**right_path, filter_path);
  filter_path->set_num_output_rows(filter_path->num_output_rows() *
                                   (*edge)->selectivity);
  filter_path->cost += EstimateFilterCost(thd, (*right_path)->num_output_rows(),
                                          filter_cond, query_block)
                           .cost_if_not_materialized;

  // Build a new join predicate with no join condition.
  RelationalExpression *new_expr =
      new (thd->mem_root) RelationalExpression(thd);
  new_expr->type = expr->type;
  new_expr->tables_in_subtree = expr->tables_in_subtree;
  new_expr->nodes_in_subtree = expr->nodes_in_subtree;
  new_expr->left = expr->left;
  new_expr->right = expr->right;

  JoinPredicate *new_edge = new (thd->mem_root) JoinPredicate{
      new_expr, /*selectivity=*/1.0, (*edge)->estimated_bytes_per_row,
      (*edge)->functional_dependencies, /*functional_dependencies_idx=*/{}};

  // Use the filter path and the new join edge with no condition for creating
  // the hash join.
  *right_path = filter_path;
  *edge = new_edge;
}

/**
  Called to signal that it's possible to connect the non-overlapping
  table subsets “left” and “right” through the edge given by “edge_idx”
  (which corresponds to an index in m_graph->edges), ie., we have found
  a legal subplan for joining (left ∪ right). Assign it a cost based on
  the cost of the children and the join method we use. (Currently, there
  is only one -- hash join.)

  There may be multiple such calls for the same subplan; e.g. for
  inner-joining {t1,t2,t3}, we will get calls for both {t1}/{t2,t3}
  and {t1,t2}/{t3}, and need to assign costs to both and keep the
  cheapest one. However, we will not get calls with the two subsets
  in reversed order.
 */
bool CostingReceiver::FoundSubgraphPair(NodeMap left, NodeMap right,
                                        int edge_idx) {
  if (m_thd->is_error()) return true;

  m_graph->secondary_engine_costing_flags |=
      SecondaryEngineCostingFlag::HAS_MULTIPLE_BASE_TABLES;

  if (++m_num_seen_subgraph_pairs > m_subgraph_pair_limit &&
      m_subgraph_pair_limit >= 0) {
    // Bail out; we're going to be needing graph simplification,
    // which the caller will handle for us.
    return true;
  }

  assert(left != 0);
  assert(right != 0);
  assert((left & right) == 0);

  const JoinPredicate *edge = &m_graph->edges[edge_idx];
  if (!PassesConflictRules(left | right, edge->expr)) {
    return false;
  }

  bool is_commutative = OperatorIsCommutative(*edge->expr);

  // If we have an equi-semijoin, and the inner side is deduplicated
  // on the group given by the join predicates, we can rewrite it to an
  // inner join, which is commutative. This is a win in some cases
  // where we have an index on the outer side but not the inner side.
  // (It is rarely a significant win in hash join, especially as we
  // don't propagate orders through it, but we propose it anyway for
  // simplicity.)
  //
  // See the comment on OperatorsAreAssociative() for why we don't
  // also need to change the rules about associativity or l-asscom.
  bool can_rewrite_semi_to_inner =
      edge->expr->type == RelationalExpression::SEMIJOIN &&
      edge->ordering_idx_needed_for_semijoin_rewrite != -1;

  // Enforce that recursive references need to be leftmost.
  if (Overlaps(right, forced_leftmost_table)) {
    if (!is_commutative) {
      assert(IsSingleBitSet(forced_leftmost_table));
      const int node_idx = FindLowestBitSet(forced_leftmost_table);
      my_error(ER_CTE_RECURSIVE_FORBIDDEN_JOIN_ORDER, MYF(0),
               m_graph->nodes[node_idx].table->alias);
      return true;
    }
    swap(left, right);
  }
  if (Overlaps(left, forced_leftmost_table)) {
    is_commutative = false;
    can_rewrite_semi_to_inner = false;
  }

  auto left_it = m_access_paths.find(left);
  assert(left_it != m_access_paths.end());
  auto right_it = m_access_paths.find(right);
  assert(right_it != m_access_paths.end());

  const FunctionalDependencySet new_fd_set =
      left_it->second.active_functional_dependencies |
      right_it->second.active_functional_dependencies |
      edge->functional_dependencies;
  OrderingSet new_obsolete_orderings =
      left_it->second.obsolete_orderings | right_it->second.obsolete_orderings;
  if (edge->ordering_idx_needed_for_semijoin_rewrite >= 1 &&
      edge->ordering_idx_needed_for_semijoin_rewrite < kMaxSupportedOrderings) {
    // This ordering won't be needed anymore after the join is done,
    // so mark it as obsolete.
    new_obsolete_orderings.set(edge->ordering_idx_needed_for_semijoin_rewrite);
  }

  // Check if the join is known to produce an empty result. If so, we will
  // return a ZERO_ROWS path instead of a join path, but we cannot do that just
  // yet. We need to create the join path first and attach it to the ZERO_ROWS
  // path, in case a join higher up in the join tree needs to know which tables
  // are pruned away (typically for null-complementing in outer joins).
  const bool always_empty =
      IsEmptyJoin(edge->expr->type, left_it->second.always_empty,
                  right_it->second.always_empty);

  // If the join is known to produce an empty result, and will be replaced by a
  // ZERO_ROWS path further down, temporarily disable the secondary engine cost
  // hook. There's no point in asking the secondary engine to provide a cost
  // estimate for an access path we know will be discarded.
  const secondary_engine_modify_access_path_cost_t saved_cost_hook =
      m_secondary_engine_cost_hook;
  if (always_empty) {
    m_secondary_engine_cost_hook = nullptr;
  }

  bool wrote_trace = false;

  const NodeMap left_reachable = FindReachableTablesFrom(left, *m_graph);
  const NodeMap right_reachable = FindReachableTablesFrom(right, *m_graph);
  for (AccessPath *right_path : right_it->second.paths) {
    assert(BitsetsAreCommitted(right_path));
    if (edge->expr->join_conditions_reject_all_rows &&
        edge->expr->type != RelationalExpression::FULL_OUTER_JOIN) {
      // If the join condition can never be true, we also don't need to read the
      // right side. For inner joins and semijoins, we can actually just skip
      // reading the left side as well, but if so, the join condition would
      // normally be pulled up into a WHERE condition (or into the join
      // condition of the next higher non-inner join), so we'll never see that
      // in practice, and thus, don't care particularly about the case. We also
      // don't need to care much about the ordering, since we don't propagate
      // the right-hand ordering properties through joins.
      AccessPath *zero_path = NewZeroRowsAccessPath(
          m_thd, right_path, "Join condition rejects all rows");
      MutableOverflowBitset applied_sargable_join_predicates =
          right_path->applied_sargable_join_predicates().Clone(m_thd->mem_root);
      applied_sargable_join_predicates.ClearBits(0,
                                                 m_graph->num_where_predicates);
      zero_path->filter_predicates =
          std::move(applied_sargable_join_predicates);
      zero_path->delayed_predicates = right_path->delayed_predicates;
      right_path = zero_path;
    }
    for (AccessPath *left_path : left_it->second.paths) {
      if (DisallowParameterizedJoinPath(left_path, right_path, left, right,
                                        left_reachable, right_reachable)) {
        continue;
      }

      assert(BitsetsAreCommitted(left_path));
      // For inner joins and full outer joins, the order does not matter.
      // In lieu of a more precise cost model, always keep the one that hashes
      // the fewest amount of rows. (This has lower initial cost, and the same
      // cost.)
      //
      // Finally, if either of the sides are parameterized on something
      // external, flipping the order will not necessarily be allowed (and would
      // cause us to not give a hash join for these tables at all).
      if (is_commutative &&
          !Overlaps(left_path->parameter_tables | right_path->parameter_tables,
                    RAND_TABLE_BIT)) {
        if (left_path->num_output_rows() < right_path->num_output_rows()) {
          ProposeHashJoin(right, left, right_path, left_path, edge, new_fd_set,
                          new_obsolete_orderings,
                          /*rewrite_semi_to_inner=*/false, &wrote_trace);
        } else {
          ProposeHashJoin(left, right, left_path, right_path, edge, new_fd_set,
                          new_obsolete_orderings,
                          /*rewrite_semi_to_inner=*/false, &wrote_trace);
        }
      } else {
        ProposeHashJoin(left, right, left_path, right_path, edge, new_fd_set,
                        new_obsolete_orderings,
                        /*rewrite_semi_to_inner=*/false, &wrote_trace);
        if (is_commutative || can_rewrite_semi_to_inner) {
          ProposeHashJoin(right, left, right_path, left_path, edge, new_fd_set,
                          new_obsolete_orderings,
                          /*rewrite_semi_to_inner=*/can_rewrite_semi_to_inner,
                          &wrote_trace);
        }
      }

      ProposeNestedLoopJoin(left, right, left_path, right_path, edge,
                            /*rewrite_semi_to_inner=*/false, new_fd_set,
                            new_obsolete_orderings, &wrote_trace);
      if (is_commutative || can_rewrite_semi_to_inner) {
        ProposeNestedLoopJoin(
            right, left, right_path, left_path, edge,
            /*rewrite_semi_to_inner=*/can_rewrite_semi_to_inner, new_fd_set,
            new_obsolete_orderings, &wrote_trace);
      }
      m_overflow_bitset_mem_root.ClearForReuse();
    }
  }

  if (always_empty) {
    m_secondary_engine_cost_hook = saved_cost_hook;
    const auto it = m_access_paths.find(left | right);
    if (it != m_access_paths.end() && !it->second.paths.empty() &&
        !it->second.always_empty) {
      AccessPath *first_candidate = it->second.paths.front();
      AccessPath *zero_path =
          NewZeroRowsAccessPath(m_thd, first_candidate, "impossible WHERE");
      MutableOverflowBitset applied_sargable_join_predicates =
          first_candidate->applied_sargable_join_predicates().Clone(
              m_thd->mem_root);
      applied_sargable_join_predicates.ClearBits(0,
                                                 m_graph->num_where_predicates);
      zero_path->filter_predicates =
          std::move(applied_sargable_join_predicates);
      zero_path->delayed_predicates = first_candidate->delayed_predicates;
      zero_path->ordering_state = first_candidate->ordering_state;
      ProposeAccessPathWithOrderings(
          left | right, it->second.active_functional_dependencies,
          it->second.obsolete_orderings, zero_path, "empty join");
    }
  }

  if (m_trace != nullptr) {
    TraceAccessPaths(left | right);
  }
  return false;
}

/**
  Build an access path that deduplicates its input on a certain grouping.
  This is used for converting semijoins to inner joins. If the grouping is
  empty, all rows are the same, and we make a simple LIMIT 1 instead.
 */
AccessPath *DeduplicateForSemijoin(THD *thd, AccessPath *path,
                                   Item **semijoin_group,
                                   int semijoin_group_size) {
  AccessPath *dedup_path;
  if (semijoin_group_size == 0) {
    dedup_path = NewLimitOffsetAccessPath(thd, path, /*limit=*/1, /*offset=*/0,
                                          /*count_all_rows=*/false,
                                          /*reject_multiple_rows=*/false,
                                          /*send_records_override=*/nullptr);
  } else {
    dedup_path = NewRemoveDuplicatesAccessPath(thd, path, semijoin_group,
                                               semijoin_group_size);
    CopyBasicProperties(*path, dedup_path);
    // TODO(sgunders): Model the actual reduction in rows somehow.
    dedup_path->cost += kAggregateOneRowCost * path->num_output_rows();
  }
  return dedup_path;
}

string CostingReceiver::PrintSubgraphHeader(const JoinPredicate *edge,
                                            const AccessPath &join_path,
                                            NodeMap left, NodeMap right) const {
  string ret =
      StringPrintf("\nFound sets %s and %s, connected by condition %s\n",
                   PrintSet(left).c_str(), PrintSet(right).c_str(),
                   GenerateExpressionLabel(edge->expr).c_str());
  for (int pred_idx : BitsSetIn(join_path.filter_predicates)) {
    ret += StringPrintf(
        " - applied (delayed) predicate %s\n",
        ItemToString(m_graph->predicates[pred_idx].condition).c_str());
  }
  return ret;
}

void CostingReceiver::ProposeHashJoin(
    NodeMap left, NodeMap right, AccessPath *left_path, AccessPath *right_path,
    const JoinPredicate *edge, FunctionalDependencySet new_fd_set,
    OrderingSet new_obsolete_orderings, bool rewrite_semi_to_inner,
    bool *wrote_trace) {
  if (!SupportedEngineFlag(SecondaryEngineFlag::SUPPORTS_HASH_JOIN)) return;

  if (Overlaps(left_path->parameter_tables, right) ||
      Overlaps(right_path->parameter_tables, left | RAND_TABLE_BIT)) {
    // Parameterizations must be resolved by nested loop.
    // We can still have parameters from outside the join, though
    // (even in the hash table; but it must be cleared for each Init() then).
    return;
  }

  if (Overlaps(left | right, m_fulltext_tables)) {
    // Evaluation of a full-text function requires that the underlying scan is
    // positioned on the row that contains the value to be searched. It is not
    // enough that table->record[0] contains the row; the handler needs to be
    // actually positioned on the row. This does not work so well with hash
    // joins, since they may return rows in a different order than that of the
    // underlying scan.
    //
    // For now, be conservative and don't propose a hash join if either side of
    // the join contains a full-text searched table. It is possible to be more
    // lenient and allow hash joins if all the full-text search functions on the
    // accessed tables have been fully pushed down to the table/index scan and
    // don't need to be evaluated again outside of the join.
    return;
  }

  // A semijoin by definition should have a semijoin condition to work with and
  // also that the inner table of a semijoin should not be visible outside of
  // the semijoin. However, MySQL's semijoin transformation when combined with
  // outer joins might result in a transformation which might do just that. This
  // transformation cannot be interpreted as is, but instead needs some special
  // handling in optimizer to correctly do the semijoin and outer join. However,
  // this is a problem for hypergraph. For a pattern like:
  // t1 left join (t2 semijoin t3 on true) on t1.a = t2.a and t1.b = t3.a, where
  // a semijoin does not have any condition to work with, it is expected that
  // all joins including the outer join be performed before the duplicate
  // removal happens for semijoin (Complete details in WL#5561). This is not
  // possible with hash joins. Such a pattern is a result of having a subquery
  // in an ON condition like:
  // SELECT * FROM t1 LEFT JOIN t2 ON t1.a= t2.a AND t1.b IN (SELECT a FROM t3);
  // So we ban the transformation itself for hypergraph during resolving.
  //
  // However, this also bans the transformation for a query like this:
  // SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.a AND t1.a IN (SELECT a FROM t3).
  // For the above query, because of the multiple equalities, we could have
  // t1 LEFT JOIN (t2 SEMIJOIN t3 ON t2.a=t3.a) ON t1.a=t2.a which could be
  // executed using hash joins. This is a problem for secondary engine, as
  // without the semijoin transformation, it needs to process subqueries which
  // it cannot at present. So we allow the transformation to go through during
  // resolving when secondary engine optimization is ON and recognize the
  // pattern when hash join is not possible and reject it here. This is not an
  // issue for secondary engine as it eventually rejects such a query because
  // it can only perform hash joins. However it's a problem if we allow for
  // primary engine as hypergraph can go ahead and produce a mix of NLJ and hash
  // joins which leads to wrong results.
  // TODO(Chaithra): It is possible that the various join nests are looked at
  // carefully when relational expressions are created and forcing only NLJ's
  // for such cases.
  if (edge->expr->type == RelationalExpression::LEFT_JOIN &&
      edge->expr->right->type == RelationalExpression::SEMIJOIN) {
    // Check if there is a condition connecting the left side of the outer
    // join and inner side of the semijoin. This is a deviation from the
    // definition of a semijoin which makes it not possible to execute such
    // a plan with hash joins.
    RelationalExpression *semijoin = edge->expr->right;
    const table_map disallowed_tables =
        semijoin->tables_in_subtree & ~GetVisibleTables(semijoin);
    if (disallowed_tables != 0) {
      for (Item *cond : edge->expr->equijoin_conditions) {
        if (Overlaps(disallowed_tables, cond->used_tables()) &&
            Overlaps(edge->expr->left->tables_in_subtree,
                     cond->used_tables())) {
          return;
        }
      }
      for (Item *cond : edge->expr->join_conditions) {
        if (Overlaps(disallowed_tables, cond->used_tables()) &&
            Overlaps(edge->expr->left->tables_in_subtree,
                     cond->used_tables())) {
          return;
        }
      }
    }
  }

  if (edge->expr->type == RelationalExpression::LEFT_JOIN &&
      SecondaryEngineHandlerton(m_thd) != nullptr) {
    MoveDegenerateJoinConditionToFilter(m_thd, m_query_block, &edge,
                                        &right_path);
  }

  assert(BitsetsAreCommitted(left_path));
  assert(BitsetsAreCommitted(right_path));

  AccessPath join_path;
  join_path.type = AccessPath::HASH_JOIN;
  join_path.parameter_tables =
      (left_path->parameter_tables | right_path->parameter_tables) &
      ~(left | right);
  join_path.hash_join().outer = left_path;
  join_path.hash_join().inner = right_path;
  join_path.hash_join().join_predicate = edge;
  join_path.hash_join().store_rowids = false;
  join_path.hash_join().rewrite_semi_to_inner = rewrite_semi_to_inner;
  join_path.hash_join().tables_to_get_rowid_for = 0;
  join_path.hash_join().allow_spill_to_disk = true;

  // The rows from the inner side of a hash join come in different order from
  // that of the underlying scan, so we need to store row IDs for any
  // update/delete target tables on the inner side, so that we know which rows
  // to update or delete. The same applies to rows from the outer side, if the
  // hash join spills to disk, so we need to store row IDs for both sides.
  if (Overlaps(m_update_delete_target_nodes, left | right)) {
    FindTablesToGetRowidFor(&join_path);
  }

  // See the equivalent code in ProposeNestedLoopJoin().
  if (rewrite_semi_to_inner) {
    int ordering_idx = edge->ordering_idx_needed_for_semijoin_rewrite;
    assert(ordering_idx != -1);
    if (ordering_idx != 0 && !m_orderings->DoesFollowOrder(
                                 left_path->ordering_state, ordering_idx)) {
      return;
    }
    assert(edge->expr->type == RelationalExpression::SEMIJOIN);

    // NOTE: We purposefully don't overwrite left_path here, so that we
    // don't have to worry about copying ordering_state etc.
    CommitBitsetsToHeap(left_path);
    join_path.hash_join().outer = DeduplicateForSemijoin(
        m_thd, left_path, edge->semijoin_group, edge->semijoin_group_size);
  }

  // TODO(sgunders): Consider removing redundant join conditions.
  // Normally, it's better to have more equijoin conditions than fewer,
  // but in this case, every row should fall into the same hash bucket anyway,
  // so they do not help.

  double num_output_rows;
  {
    double right_path_already_applied_selectivity =
        FindAlreadyAppliedSelectivity(edge, left_path, right_path, left, right);
    if (right_path_already_applied_selectivity < 0.0) {
      return;
    }
    double outer_input_rows = left_path->num_output_rows();
    double inner_input_rows =
        right_path->num_output_rows() / right_path_already_applied_selectivity;

    // If left and right are flipped for semijoins, we need to flip
    // them back for row calculation (or we'd clamp to the wrong value).
    if (rewrite_semi_to_inner) {
      swap(outer_input_rows, inner_input_rows);
    }

    num_output_rows =
        FindOutputRowsForJoin(outer_input_rows, inner_input_rows, edge);
  }

  // left_path and join_path.hash_join().outer are intentionally different if
  // rewrite_semi_to_inner is true. See comment where DeduplicateForSemijoin()
  // is called above. We want to calculate join cost based on the actual left
  // child, so use join_path.hash_join().outer in cost calculations for
  // join_path.
  const AccessPath *outer = join_path.hash_join().outer;

  // TODO(sgunders): Add estimates for spill-to-disk costs.
  // NOTE: Keep this in sync with SimulateJoin().
  const double build_cost =
      right_path->cost + right_path->num_output_rows() * kHashBuildOneRowCost;
  double cost = outer->cost + build_cost +
                outer->num_output_rows() * kHashProbeOneRowCost +
                num_output_rows * kHashReturnOneRowCost;

  // Note: This isn't strictly correct if the non-equijoin conditions
  // have selectivities far from 1.0; the cost should be calculated
  // on the number of rows after the equijoin conditions, but before
  // the non-equijoin conditions.
  cost += num_output_rows * edge->expr->join_conditions.size() *
          kApplyOneFilterCost;

  join_path.num_output_rows_before_filter = num_output_rows;
  join_path.cost_before_filter = cost;
  join_path.set_num_output_rows(num_output_rows);
  join_path.init_cost = build_cost + outer->init_cost;

  double estimated_bytes_per_row = edge->estimated_bytes_per_row;

  // If the edge is part of a cycle in the hypergraph, there may be other usable
  // join predicates in other edges. MoveFilterPredicatesIntoHashJoinCondition()
  // will widen the hash join predicate in that case, so account for that here.
  // Only relevant when joining more than two tables. Say {t1,t2} HJ {t3}, which
  // could be joined both along a t1-t3 edge and a t2-t3 edge.
  //
  // TODO(khatlen): The cost is still calculated as if the hash join only uses
  // "edge", and that the alternative edges are put in filters on top of the
  // join.
  if (edge->expr->join_predicate_first != edge->expr->join_predicate_last &&
      PopulationCount(left | right) > 2) {
    // Only inner joins are part of cycles.
    assert(edge->expr->type == RelationalExpression::INNER_JOIN);
    for (size_t edge_idx = 0; edge_idx < m_graph->graph.edges.size();
         ++edge_idx) {
      Hyperedge hyperedge = m_graph->graph.edges[edge_idx];
      if (IsSubset(hyperedge.left, left) && IsSubset(hyperedge.right, right)) {
        const JoinPredicate *other_edge = &m_graph->edges[edge_idx / 2];
        assert(other_edge->expr->type == RelationalExpression::INNER_JOIN);
        if (other_edge != edge &&
            PassesConflictRules(left | right, other_edge->expr)) {
          estimated_bytes_per_row += EstimateHashJoinKeyWidth(other_edge->expr);
        }
      }
    }
  }

  const double hash_memory_used_bytes =
      estimated_bytes_per_row * right_path->num_output_rows();

  if (hash_memory_used_bytes <= m_thd->variables.join_buff_size * 0.9 &&
      right_path->parameter_tables == 0) {
    // Fits in memory (with 10% estimation margin), and has
    // no external dependencies, so the hash table can be reused.
    join_path.init_once_cost = build_cost + outer->init_once_cost;
  } else {
    join_path.init_once_cost =
        outer->init_once_cost + right_path->init_once_cost;
  }
  join_path.cost = cost;

  // For each scan, hash join will read the left side once and the right side
  // once, so we are as safe as the least safe of the two. (This isn't true
  // if we set spill_to_disk = false, but we never do that in the hypergraph
  // optimizer.) Note that if the right side fits entirely in RAM, we don't
  // scan it the second time (so we could make the operation _more_ safe
  // than the right side, and we should consider both ways of doing
  // an inner join), but we cannot know that when planning.
  join_path.safe_for_rowid =
      std::max(left_path->safe_for_rowid, right_path->safe_for_rowid);

  // Only trace once; the rest ought to be identical.
  if (m_trace != nullptr && !*wrote_trace) {
    *m_trace += PrintSubgraphHeader(edge, join_path, left, right);
    *wrote_trace = true;
  }

  for (bool materialize_subqueries : {false, true}) {
    AccessPath new_path = join_path;
    FunctionalDependencySet filter_fd_set;
    ApplyDelayedPredicatesAfterJoin(
        left, right, left_path, right_path, edge->expr->join_predicate_first,
        edge->expr->join_predicate_last, materialize_subqueries, &new_path,
        &filter_fd_set);
    // Hash join destroys all ordering information (even from the left side,
    // since we may have spill-to-disk).
    new_path.ordering_state = m_orderings->ApplyFDs(m_orderings->SetOrder(0),
                                                    new_fd_set | filter_fd_set);
    ProposeAccessPathWithOrderings(left | right, new_fd_set | filter_fd_set,
                                   new_obsolete_orderings, &new_path,
                                   materialize_subqueries ? "mat. subq." : "");

    if (!Overlaps(new_path.filter_predicates,
                  m_graph->materializable_predicates)) {
      break;
    }
  }
}

// Of all delayed predicates, see which ones we can apply now, and which
// ones that need to be delayed further.
void CostingReceiver::ApplyDelayedPredicatesAfterJoin(
    NodeMap left, NodeMap right, const AccessPath *left_path,
    const AccessPath *right_path, int join_predicate_first,
    int join_predicate_last, bool materialize_subqueries, AccessPath *join_path,
    FunctionalDependencySet *new_fd_set) {
  // We build up a new FD set each time; it should be the same for the same
  // left/right pair, so it is somewhat redundant, but it allows us to verify
  // that property through the assert in ProposeAccessPathWithOrderings().
  new_fd_set->reset();

  // Keep track of which multiple equalities we have created predicates for
  // so far. We use this to avoid applying redundant predicates, ie. predicates
  // that have already been checked. (This is not only to avoid unneeded work,
  // but to avoid double-counting the selectivity.)
  //
  // Avoiding redundant predicates for a multi-equality is equivalent to never
  // applying those that would cause loops in the subgraph induced by the tables
  // involved in the multi-equality. (In other words, we are building spanning
  // trees in the induced subgraph.) In general, every time we connect two
  // subgraphs, we must apply every relevant multi-equality exactly once,
  // and ignore the others. (This is vaguely reminiscent of Kruskal's algorithm
  // for constructing minimum spanning trees.)
  //
  // DPhyp only ever connects subgraphs that are not already connected
  // (ie., it already constructs spanning trees), so we know that the join
  // conditions applied earlier are never redundant wrt. the rest of the graph.
  // Thus, we only need to test the delayed predicates below; they _may_ contain
  // a multiple equality we haven't already applied, but they may also be new,
  // e.g. in this graph:
  //
  //     b
  //    /|\ .
  //   a | d
  //    \|/
  //     c
  //
  // If we have a multiple equality over {b,c,d}, and connect a-b and then a-c,
  // the edge b-c will come into play and contain a multi-equality that was not
  // applied before. We will need to apply that multi-equality (we will
  // only get one of d-b and d-c). However, if we instead connected d-b
  // and d-c, the edge b-c will now be redundant and must be ignored
  // (except for functional dependencies). We simply track which ones have been
  // applied this iteration by keeping a bitmap of them.
  uint64_t multiple_equality_bitmap = 0;
  for (int pred_idx = join_predicate_first; pred_idx < join_predicate_last;
       ++pred_idx) {
    const Predicate &pred = m_graph->predicates[pred_idx];
    if (pred.source_multiple_equality_idx != -1) {
      multiple_equality_bitmap |= uint64_t{1}
                                  << pred.source_multiple_equality_idx;
    }
  }

  double materialize_cost = 0.0;

  // filter_predicates holds both filter_predicates and
  // applied_sargable_join_predicates. Keep the information about the latter,
  // but reset the one pertaining to the former.
  MutableOverflowBitset filter_predicates =
      OverflowBitset::Or(&m_overflow_bitset_mem_root,
                         left_path->applied_sargable_join_predicates(),
                         right_path->applied_sargable_join_predicates());
  filter_predicates.ClearBits(0, m_graph->num_where_predicates);

  // Predicates we are still delaying.
  MutableOverflowBitset delayed_predicates = OverflowBitset::Xor(
      &m_overflow_bitset_mem_root, left_path->delayed_predicates,
      right_path->delayed_predicates);
  delayed_predicates.ClearBits(join_predicate_first, join_predicate_last);

  // Predicates that were delayed, but that we need to check now.
  // (We don't need to allocate a MutableOverflowBitset for this.)
  const NodeMap ready_tables = left | right;
  for (int pred_idx : BitsSetInBoth(left_path->delayed_predicates,
                                    right_path->delayed_predicates)) {
    if (pred_idx >= join_predicate_first && pred_idx < join_predicate_last) {
      continue;
    }
    const Predicate &pred = m_graph->predicates[pred_idx];
    if (IsSubset(pred.total_eligibility_set, ready_tables)) {
      const auto [already_applied_as_sargable, subsumed] =
          AlreadyAppliedAsSargable(pred.condition, left_path, right_path);
      if (pred.source_multiple_equality_idx == -1 ||
          !IsBitSet(pred.source_multiple_equality_idx,
                    multiple_equality_bitmap)) {
        if (!subsumed) {
          FilterCost cost = EstimateFilterCost(
              m_thd, join_path->num_output_rows(), pred.contained_subqueries);
          if (materialize_subqueries) {
            join_path->cost += cost.cost_if_materialized;
            materialize_cost += cost.cost_to_materialize;
          } else {
            join_path->cost += cost.cost_if_not_materialized;
          }
          if (!already_applied_as_sargable) {
            join_path->set_num_output_rows(join_path->num_output_rows() *
                                           pred.selectivity);
            filter_predicates.SetBit(pred_idx);
          }
        }
        if (pred.source_multiple_equality_idx != -1) {
          multiple_equality_bitmap |= uint64_t{1}
                                      << pred.source_multiple_equality_idx;
        }
      } else if (already_applied_as_sargable) {
        // The two subgraphs are joined by at least two (additional) edges
        // both belonging to the same multiple equality (of which this predicate
        // is one). One of them, not a sargable predicate, happened to be
        // earlier in the list, and was thus deemed to be the representative of
        // that multiple equality. However, we now see another one that is
        // already applied as sargable, and thus, its selectivity has already
        // been included. Thus, we need to remove that selectivity again to
        // avoid double-counting and row count inconsistencies.
        //
        // This is a bit of a hack, but it happens pretty rarely, and it's
        // fairly straightforward. An alternative would be to have a separate
        // scan over all the delayed predicates that were already applied as
        // sargable (predicates like the one we are considering right now),
        // in order to force them into being representative for their multiple
        // equality.
        if (pred.selectivity > 1e-6) {
          join_path->set_num_output_rows(join_path->num_output_rows() /
                                         pred.selectivity);
        }
      }
      *new_fd_set |= pred.functional_dependencies;
    } else {
      delayed_predicates.SetBit(pred_idx);
    }
  }
  join_path->filter_predicates = std::move(filter_predicates);
  join_path->delayed_predicates = std::move(delayed_predicates);

  if (materialize_subqueries) {
    CommitBitsetsToHeap(join_path);
    ExpandSingleFilterAccessPath(m_thd, join_path, m_query_block->join,
                                 m_graph->predicates,
                                 m_graph->num_where_predicates);
    assert(join_path->type == AccessPath::FILTER);
    join_path->filter().materialize_subqueries = true;
    join_path->cost +=
        materialize_cost;  // Will be subtracted back for rescans.
    join_path->init_cost += materialize_cost;
    join_path->init_once_cost += materialize_cost;
  }
}

/**
  Check if we're about to apply a join condition that would be redundant
  with regards to an already-applied sargable predicate, ie., whether our
  join condition and the sargable predicate applies the same multiple equality.
  E.g. if we try to join {t1,t2} and {t3} along t1=t3, but the access path
  for t3 already has applied the join condition t2=t3, and these are from the
  same multiple equality, return true.

  Even though this is totally _legal_, having such a situation is bad, because

    a) It double-counts the selectivity, causing the overall row estimate
       to become too low.
    b) It causes unneeded work by adding a redundant filter.

  b) would normally cause the path to be pruned out due to cost, except that
  the artificially low row count due to a) could make the path attractive as a
  subplan of a larger join. Thus, we simply reject these joins; we'll see a
  different alternative for this join at some point that is not redundant
  (e.g., in the given example, we'd see the t2=t3 join).
 */
bool CostingReceiver::RedundantThroughSargable(
    OverflowBitset redundant_against_sargable_predicates,
    const AccessPath *left_path, const AccessPath *right_path, NodeMap left,
    NodeMap right) {
  // For a join condition to be redundant against an already applied sargable
  // predicate, the applied predicate must somehow connect the left side and the
  // right side. This means either:
  //
  // - One of the paths must be parameterized on at least one of the tables in
  // the other path. In the example above, because t2=t3 is applied on the {t3}
  // path, and t2 is not included in the path, the {t3} path is parameterized on
  // t2. (It is only necessary to check if right_path is parameterized on
  // left_path, since parameterization is always resolved by nested-loop joining
  // in the parameter tables from the outer/left side into the parameterized
  // path on the inner/right side.)
  //
  // - Or both paths are parameterized on some common table that is not part of
  // either path. Say if {t1,t2} has sargable t1=t4 and {t3} has sargable t3=t4,
  // then both paths are parameterized on t4, and joining {t1,t2} with {t3}
  // along t1=t3 is redundant, given all three predicates (t1=t4, t3=t4, t1=t3)
  // are from the same multiple equality.
  //
  // If the parameterization is not like that, we don't need to check any
  // further.
  assert(!Overlaps(left_path->parameter_tables, right));
  if (!Overlaps(right_path->parameter_tables,
                left | left_path->parameter_tables)) {
    return false;
  }

  const auto redundant_and_applied = [](uint64_t redundant_sargable,
                                        uint64_t left_applied,
                                        uint64_t right_applied) {
    return redundant_sargable & (left_applied | right_applied);
  };
  bool redundant_against_something_in_left = false;
  bool redundant_against_something_in_right = false;
  for (size_t predicate_idx :
       OverflowBitsetBitsIn<3, decltype(redundant_and_applied)>(
           {redundant_against_sargable_predicates,
            left_path->applied_sargable_join_predicates(),
            right_path->applied_sargable_join_predicates()},
           redundant_and_applied)) {
    // The sargable condition must work as a join condition for this join
    // (not between tables we've already joined in). Note that the joining
    // could be through two different sargable predicates; they do not have
    // to be the same. E.g., if we have
    //
    //   - t1, with sargable t1.x = t3.x
    //   - t2, with sargable t2.x = t3.x
    //   - Join condition t1.x = t2.x
    //
    // then the join condition is redundant and should be refused,
    // even though neither sargable condition joins t1 and t2 directly.
    //
    // Note that there are more complicated situations, e.g. if t2 instead
    // had t2.x = t4.x in the example above, where we could reject non-redundant
    // join orderings. However, in nearly all such cases,
    // DisallowParameterizedJoinPath() would reject them anyway, and it is not
    // an issue for successfully planning the query, as there would always exist
    // a non-parameterized path that we could use instead.
    const Predicate &sargable_predicate = m_graph->predicates[predicate_idx];
    redundant_against_something_in_left |=
        Overlaps(sargable_predicate.used_nodes, left);
    redundant_against_something_in_right |=
        Overlaps(sargable_predicate.used_nodes, right);
    if (redundant_against_something_in_left &&
        redundant_against_something_in_right) {
      return true;
    }
  }
  return false;
}

/**
  Whether the given join condition is already applied as a sargable predicate
  earlier in the tree (presumably on the right side). This is different from
  RedundantThroughSargable() in that this checks whether we have already applied
  this exact join condition earlier, while the former checks whether we are
  trying to apply a different join condition that is redundant against something
  we've applied earlier.

  The first boolean is whether “condition” is a join condition we've applied
  earlier (as sargable; so we should not count its selectivity again),
  and the second argument is whether that sargable also subsumed the entire
  join condition (so we need not apply it as a filter).
 */
pair<bool, bool> CostingReceiver::AlreadyAppliedAsSargable(
    Item *condition, const AccessPath *left_path,
    const AccessPath *right_path) {
  const auto it = m_graph->sargable_join_predicates.find(condition);
  if (it == m_graph->sargable_join_predicates.end()) {
    return {false, false};
  }

  // NOTE: It is rare that join predicates already have been applied as
  // ref access on the outer side, but not impossible if conditions are
  // duplicated; see e.g. bug #33383388.
  const bool applied =
      IsBitSet(it->second, left_path->applied_sargable_join_predicates()) ||
      IsBitSet(it->second, right_path->applied_sargable_join_predicates());
  const bool subsumed =
      IsBitSet(it->second, left_path->subsumed_sargable_join_predicates()) ||
      IsBitSet(it->second, right_path->subsumed_sargable_join_predicates());
  if (subsumed) {
    assert(applied);
  }
  return {applied, subsumed};
}

void CostingReceiver::ProposeNestedLoopJoin(
    NodeMap left, NodeMap right, AccessPath *left_path, AccessPath *right_path,
    const JoinPredicate *edge, bool rewrite_semi_to_inner,
    FunctionalDependencySet new_fd_set, OrderingSet new_obsolete_orderings,
    bool *wrote_trace) {
  if (!SupportedEngineFlag(SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN))
    return;

  if (Overlaps(left_path->parameter_tables, right)) {
    // The outer table cannot pick up values from the inner,
    // only the other way around.
    return;
  }

  assert(BitsetsAreCommitted(left_path));
  assert(BitsetsAreCommitted(right_path));

  // FULL OUTER JOIN is not possible with nested-loop join.
  assert(edge->expr->type != RelationalExpression::FULL_OUTER_JOIN);

  AccessPath join_path;
  join_path.type = AccessPath::NESTED_LOOP_JOIN;
  join_path.parameter_tables =
      (left_path->parameter_tables | right_path->parameter_tables) &
      ~(left | right);
  join_path.nested_loop_join().pfs_batch_mode = false;
  join_path.nested_loop_join().already_expanded_predicates = false;
  join_path.nested_loop_join().outer = left_path;
  join_path.nested_loop_join().inner = right_path;
  if (rewrite_semi_to_inner) {
    // This join is a semijoin (which is non-commutative), but the caller wants
    // us to try to invert it anyway; or to be precise, it has already inverted
    // it for us, and wants us to make sure that's OK. This is only
    // allowed if we can remove the duplicates from the outer (originally inner)
    // side, so check that it is grouped correctly, and then deduplicate on it.
    //
    // Note that in many cases, the grouping/ordering here would be due to an
    // earlier sort-ahead inserted into the tree. (The other case is due to
    // scanning along an index, but then, we'd usually prefer to
    // use that index for lookups instead of inverting the join. It is possible,
    // though.) If so, it would have been nice to just do a deduplicating sort
    // instead, but it would require is to track deduplication information in
    // the access paths (possibly as part of the ordering state somehow) and
    // track them throughout the join tree, which we don't do at the moment.
    // Thus, there may be an inefficiency here.
    assert(edge->expr->type == RelationalExpression::SEMIJOIN);
    int ordering_idx = edge->ordering_idx_needed_for_semijoin_rewrite;
    assert(ordering_idx != -1);
    if (ordering_idx != 0 && !m_orderings->DoesFollowOrder(
                                 left_path->ordering_state, ordering_idx)) {
      return;
    }
    join_path.nested_loop_join().join_type = JoinType::INNER;

    // NOTE: We purposefully don't overwrite left_path here, so that we
    // don't have to worry about copying ordering_state etc.
    join_path.nested_loop_join().outer = DeduplicateForSemijoin(
        m_thd, left_path, edge->semijoin_group, edge->semijoin_group_size);
  } else if (edge->expr->type == RelationalExpression::STRAIGHT_INNER_JOIN) {
    join_path.nested_loop_join().join_type = JoinType::INNER;
  } else {
    join_path.nested_loop_join().join_type =
        static_cast<JoinType>(edge->expr->type);
  }
  join_path.nested_loop_join().join_predicate = edge;

  // Nested loop joins read the outer table exactly once, and the inner table
  // potentially many times, so we can only perform immediate update or delete
  // on the outer table.
  // TODO(khatlen): If left_path is guaranteed to return at most one row (like a
  // unique index lookup), it should be possible to perform immediate delete
  // from both sides of the nested loop join. The old optimizer already does
  // that for const tables.
  join_path.immediate_update_delete_table =
      left_path->immediate_update_delete_table;

  const AccessPath *inner = join_path.nested_loop_join().inner;
  double inner_rescan_cost = inner->rescan_cost();

  double right_path_already_applied_selectivity = 1.0;
  join_path.nested_loop_join().equijoin_predicates = OverflowBitset{};
  if (edge->expr->join_conditions_reject_all_rows) {
    // We've already taken out all rows from the right-hand side
    // (by means of a ZeroRowsIterator), so no need to add filters;
    // they'd only clutter the EXPLAIN.
    //
    // Note that for obscure cases (inner joins where the join condition
    // was not pulled up due to a pass ordering issue), we might see
    // the left and right path be switched around due to commutativity.
    assert(left_path->type == AccessPath::ZERO_ROWS ||
           right_path->type == AccessPath::ZERO_ROWS);
  } else if (!edge->expr->equijoin_conditions.empty() ||
             !edge->expr->join_conditions.empty()) {
    // Apply join filters. Don't update num_output_rows, as the join's
    // selectivity will already be applied in FindOutputRowsForJoin().
    // NOTE(sgunders): We don't model the effect of short-circuiting filters on
    // the cost here.
    double rows_after_filtering = inner->num_output_rows();

    right_path_already_applied_selectivity =
        FindAlreadyAppliedSelectivity(edge, left_path, right_path, left, right);
    if (right_path_already_applied_selectivity < 0.0) {
      return;
    }

    // num_output_rows is only for cost calculation and display purposes;
    // we hard-code the use of edge->selectivity below, so that we're
    // seeing the same number of rows as for hash join. This might throw
    // the filtering cost off slightly.
    MutableOverflowBitset equijoin_predicates{
        m_thd->mem_root, edge->expr->equijoin_conditions.size()};
    for (size_t join_cond_idx = 0;
         join_cond_idx < edge->expr->equijoin_conditions.size();
         ++join_cond_idx) {
      Item_eq_base *condition = edge->expr->equijoin_conditions[join_cond_idx];
      const CachedPropertiesForPredicate &properties =
          edge->expr->properties_for_equijoin_conditions[join_cond_idx];

      const auto [already_applied_as_sargable, subsumed] =
          AlreadyAppliedAsSargable(condition, left_path, right_path);
      if (!subsumed) {
        equijoin_predicates.SetBit(join_cond_idx);
        inner_rescan_cost += EstimateFilterCost(m_thd, rows_after_filtering,
                                                properties.contained_subqueries)
                                 .cost_if_not_materialized;
        rows_after_filtering *= properties.selectivity;
      }
    }
    for (const CachedPropertiesForPredicate &properties :
         edge->expr->properties_for_join_conditions) {
      inner_rescan_cost += EstimateFilterCost(m_thd, rows_after_filtering,
                                              properties.contained_subqueries)
                               .cost_if_not_materialized;
      rows_after_filtering *= properties.selectivity;
    }
    join_path.nested_loop_join().equijoin_predicates =
        std::move(equijoin_predicates);
  }

  // Ignores the row count from filter_path; see above.
  {
    assert(right_path_already_applied_selectivity >= 0.0);
    double outer_input_rows = left_path->num_output_rows();
    double inner_input_rows =
        right_path->num_output_rows() / right_path_already_applied_selectivity;

    // If left and right are flipped for semijoins, we need to flip
    // them back for row calculation (or we'd clamp to the wrong value).
    if (rewrite_semi_to_inner) {
      swap(outer_input_rows, inner_input_rows);

      if (right_path_already_applied_selectivity < 1.0 &&
          PopulationCount(right) > 1) {
        // If there are multiple inner tables, it is possible that the row count
        // of the inner child is clamped by FindOutputRowsForJoin() by a
        // semijoin nested inside the inner child, and it is therefore difficult
        // to tell whether the already applied selectivity needs to be accounted
        // for or not. Until we have found a way to ensure consistent row
        // estimates between semijoin and rewrite_semi_to_inner with already
        // applied sargable predicates, just set a flag to pacify the assert in
        // ProposeAccessPath().
        has_semijoin_with_possibly_clamped_child = true;
      }
    }

    join_path.num_output_rows_before_filter =
        FindOutputRowsForJoin(outer_input_rows, inner_input_rows, edge);
    join_path.set_num_output_rows(join_path.num_output_rows_before_filter);
  }

  // left_path and join_path.nested_loop_join().outer are intentionally
  // different if rewrite_semi_to_inner is true. See comment where
  // DeduplicateForSemijoin() is called above. We want to calculate join cost
  // based on the actual left child, so use join_path.nested_loop_join().outer
  // in cost calculations for join_path.
  const AccessPath *outer = join_path.nested_loop_join().outer;

  join_path.init_cost = outer->init_cost;

  // NOTE: The ceil() around the number of rows on the left side is a workaround
  // for an issue where we think the left side has a very low cardinality,
  // e.g. 1e-5 rows, and we believe that justifies having something hugely
  // expensive on the right side (e.g. a large table scan). Obviously, this is a
  // band-aid (we should “just” have better row estimation and/or braking
  // factors), but it should be fairly benign in general.
  join_path.cost_before_filter = join_path.cost =
      outer->cost + inner->init_cost +
      inner_rescan_cost * ceil(outer->num_output_rows());

  // Nested-loop preserves any ordering from the outer side. Note that actually,
  // the two orders are _concatenated_ (if you nested-loop join something
  // ordered on (a,b) with something joined on (c,d), the order will be
  // (a,b,c,d)), but the state machine has no way of representing that.
  join_path.ordering_state =
      m_orderings->ApplyFDs(left_path->ordering_state, new_fd_set);

  // We may scan the right side several times, but the left side maybe once.
  // So if the right side is not safe to scan for row IDs after multiple scans,
  // neither are we. But if it's safe, we're exactly as safe as the left side.
  if (right_path->safe_for_rowid != AccessPath::SAFE) {
    join_path.safe_for_rowid = AccessPath::UNSAFE;
  } else {
    join_path.safe_for_rowid = left_path->safe_for_rowid;
  }

  // Only trace once; the rest ought to be identical.
  if (m_trace != nullptr && !*wrote_trace) {
    *m_trace += PrintSubgraphHeader(edge, join_path, left, right);
    *wrote_trace = true;
  }

  for (bool materialize_subqueries : {false, true}) {
    AccessPath new_path = join_path;
    FunctionalDependencySet filter_fd_set;
    ApplyDelayedPredicatesAfterJoin(
        left, right, left_path, right_path, edge->expr->join_predicate_first,
        edge->expr->join_predicate_last, materialize_subqueries, &new_path,
        &filter_fd_set);
    new_path.ordering_state = m_orderings->ApplyFDs(new_path.ordering_state,
                                                    new_fd_set | filter_fd_set);

    const char *description_for_trace = "";
    if (m_trace != nullptr) {
      if (materialize_subqueries && rewrite_semi_to_inner) {
        description_for_trace = "dedup to inner nested loop, mat. subq";
      } else if (rewrite_semi_to_inner) {
        description_for_trace = "dedup to inner nested loop";
      } else if (materialize_subqueries) {
        description_for_trace = "mat. subq";
      }
    }

    ProposeAccessPathWithOrderings(left | right, new_fd_set | filter_fd_set,
                                   new_obsolete_orderings, &new_path,
                                   description_for_trace);

    if (!Overlaps(new_path.filter_predicates,
                  m_graph->materializable_predicates)) {
      break;
    }
  }
}

/**
  Go through all equijoin conditions for the given join, and find out how much
  of its selectivity that has already been applied as ref accesses (which should
  thus be divided away from the join's selectivity).

  Returns -1.0 if there is at least one sargable predicate that is entirely
  redundant, and that this subgraph pair should not be attempted joined at all.
 */
double CostingReceiver::FindAlreadyAppliedSelectivity(
    const JoinPredicate *edge, const AccessPath *left_path,
    const AccessPath *right_path, NodeMap left, NodeMap right) {
  double already_applied = 1.0;
  for (size_t join_cond_idx = 0;
       join_cond_idx < edge->expr->equijoin_conditions.size();
       ++join_cond_idx) {
    Item_eq_base *condition = edge->expr->equijoin_conditions[join_cond_idx];
    const CachedPropertiesForPredicate &properties =
        edge->expr->properties_for_equijoin_conditions[join_cond_idx];

    const auto [already_applied_as_sargable, subsumed] =
        AlreadyAppliedAsSargable(condition, left_path, right_path);
    if (already_applied_as_sargable) {
      // This predicate was already applied as a ref access earlier.
      // Make sure not to double-count its selectivity, and also
      // that we don't reapply it if it was subsumed by the ref access.
      const auto it = m_graph->sargable_join_predicates.find(condition);
      already_applied *= m_graph->predicates[it->second].selectivity;
    } else if (RedundantThroughSargable(
                   properties.redundant_against_sargable_predicates, left_path,
                   right_path, left, right)) {
      if (m_trace != nullptr) {
        *m_trace += " - " + PrintAccessPath(*right_path, *m_graph, "") +
                    " has a sargable predicate that is redundant with our join "
                    "predicate, skipping\n";
      }
      return -1.0;
    }
  }
  return already_applied;
}

uint32_t AddFlag(uint32_t flags, FuzzyComparisonResult flag) {
  return flags | static_cast<uint32_t>(flag);
}

bool HasFlag(uint32_t flags, FuzzyComparisonResult flag) {
  return (flags & static_cast<uint32_t>(flag));
}

}  // namespace

// See if one access path is better than the other across all cost dimensions
// (if so, we say it dominates the other one). If not, we return
// DIFFERENT_STRENGTHS so that both must be kept.
//
// TODO(sgunders): Support turning off certain cost dimensions; e.g., init_cost
// only matters if we have a LIMIT or nested loop semijoin somewhere in the
// query, and it might not matter for secondary engine.
PathComparisonResult CompareAccessPaths(const LogicalOrderings &orderings,
                                        const AccessPath &a,
                                        const AccessPath &b,
                                        OrderingSet obsolete_orderings) {
#ifndef NDEBUG
  // Manual preference overrides everything else.
  // If they're both preferred, tie-break by ordering.
  if (a.forced_by_dbug) {
    return PathComparisonResult::FIRST_DOMINATES;
  } else if (b.forced_by_dbug) {
    return PathComparisonResult::SECOND_DOMINATES;
  }
#endif

  uint32_t flags = 0;

  if (a.parameter_tables != b.parameter_tables) {
    if (!IsSubset(a.parameter_tables, b.parameter_tables)) {
      flags = AddFlag(flags, FuzzyComparisonResult::SECOND_BETTER);
    }
    if (!IsSubset(b.parameter_tables, a.parameter_tables)) {
      flags = AddFlag(flags, FuzzyComparisonResult::FIRST_BETTER);
    }
  }

  // If we have a parameterized path, this means that at some point, it _must_
  // be on the right side of a nested-loop join. This destroys ordering
  // information (at least in our implementation -- see comment in
  // NestedLoopJoin()), so in this situation, consider all orderings as equal.
  // (This is a trick borrowed from Postgres to keep the number of unique access
  // paths down in such situations.)
  const int a_ordering_state = (a.parameter_tables == 0) ? a.ordering_state : 0;
  const int b_ordering_state = (b.parameter_tables == 0) ? b.ordering_state : 0;
  if (orderings.MoreOrderedThan(a_ordering_state, b_ordering_state,
                                obsolete_orderings)) {
    flags = AddFlag(flags, FuzzyComparisonResult::FIRST_BETTER);
  }
  if (orderings.MoreOrderedThan(b_ordering_state, a_ordering_state,
                                obsolete_orderings)) {
    flags = AddFlag(flags, FuzzyComparisonResult::SECOND_BETTER);
  }

  // If one path is safe for row IDs and another one is not,
  // that is also something we need to take into account.
  // Safer values have lower numerical values, so we can compare them
  // as integers.
  if (a.safe_for_rowid < b.safe_for_rowid) {
    flags = AddFlag(flags, FuzzyComparisonResult::FIRST_BETTER);
  } else if (b.safe_for_rowid < a.safe_for_rowid) {
    flags = AddFlag(flags, FuzzyComparisonResult::SECOND_BETTER);
  }

  // A path that allows immediate update or delete of a table is better than
  // a path that allows none.
  if (a.immediate_update_delete_table != b.immediate_update_delete_table) {
    if (a.immediate_update_delete_table == -1) {
      flags = AddFlag(flags, FuzzyComparisonResult::SECOND_BETTER);
    } else if (b.immediate_update_delete_table == -1) {
      flags = AddFlag(flags, FuzzyComparisonResult::FIRST_BETTER);
    }
  }

  // Numerical cost dimensions are compared fuzzily in order to treat paths
  // with insignificant differences as identical.
  constexpr double fuzz_factor = 1.01;

  // Normally, two access paths for the same subplan should have the same
  // number of output rows. However, for parameterized paths, this need not
  // be the case; due to pushdown of sargable conditions into indexes;
  // some filters may be applied earlier, causing fewer rows to be
  // carried around temporarily (until the parameterization is resolved).
  // This can have an advantage in causing less work later even if it's
  // non-optimal now, e.g. by saving on filtering work, or having less work
  // done in other joins. Thus, we need to keep it around as an extra
  // cost dimension.
  flags = AddFlag(flags, FuzzyComparison(a.num_output_rows(),
                                         b.num_output_rows(), fuzz_factor));

  flags = AddFlag(flags, FuzzyComparison(a.cost, b.cost, fuzz_factor));
  flags =
      AddFlag(flags, FuzzyComparison(a.init_cost, b.init_cost, fuzz_factor));
  flags = AddFlag(
      flags, FuzzyComparison(a.rescan_cost(), b.rescan_cost(), fuzz_factor));

  bool a_is_better = HasFlag(flags, FuzzyComparisonResult::FIRST_BETTER);
  bool b_is_better = HasFlag(flags, FuzzyComparisonResult::SECOND_BETTER);
  if (a_is_better && b_is_better) {
    return PathComparisonResult::DIFFERENT_STRENGTHS;
  } else if (a_is_better && !b_is_better) {
    return PathComparisonResult::FIRST_DOMINATES;
  } else if (!a_is_better && b_is_better) {
    return PathComparisonResult::SECOND_DOMINATES;
  } else {  // Fuzzily identical
    bool a_is_slightly_better =
        HasFlag(flags, FuzzyComparisonResult::FIRST_SLIGHTLY_BETTER);
    bool b_is_slightly_better =
        HasFlag(flags, FuzzyComparisonResult::SECOND_SLIGHTLY_BETTER);
    // If one path is no worse in all dimensions and strictly better
    // in at least one dimension we identify it as dominant.
    if (a_is_slightly_better && !b_is_slightly_better) {
      return PathComparisonResult::FIRST_DOMINATES;
    } else if (!a_is_slightly_better && b_is_slightly_better) {
      return PathComparisonResult::SECOND_DOMINATES;
    }
    return PathComparisonResult::IDENTICAL;
  }
}

namespace {

string PrintAccessPath(const AccessPath &path, const JoinHypergraph &graph,
                       const char *description_for_trace) {
  string str = "{";
  string join_order;

  switch (path.type) {
    case AccessPath::TABLE_SCAN:
      str += "TABLE_SCAN";
      break;
    case AccessPath::INDEX_SCAN:
      str += "INDEX_SCAN";
      break;
    case AccessPath::REF:
      str += "REF";
      break;
    case AccessPath::REF_OR_NULL:
      str += "REF_OR_NULL";
      break;
    case AccessPath::EQ_REF:
      str += "EQ_REF";
      break;
    case AccessPath::PUSHED_JOIN_REF:
      str += "PUSHED_JOIN_REF";
      break;
    case AccessPath::FULL_TEXT_SEARCH:
      str += "FULL_TEXT_SEARCH";
      break;
    case AccessPath::CONST_TABLE:
      str += "CONST_TABLE";
      break;
    case AccessPath::MRR:
      str += "MRR";
      break;
    case AccessPath::FOLLOW_TAIL:
      str += "FOLLOW_TAIL";
      break;
    case AccessPath::INDEX_RANGE_SCAN:
      str += "INDEX_RANGE_SCAN";
      break;
    case AccessPath::INDEX_MERGE:
      str += "INDEX_MERGE";
      break;
    case AccessPath::ROWID_INTERSECTION:
      str += "ROWID_INTERSECTION";
      break;
    case AccessPath::ROWID_UNION:
      str += "ROWID_UNION";
      break;
    case AccessPath::INDEX_SKIP_SCAN:
      str += "INDEX_SKIP_SCAN";
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      str += "GROUP_INDEX_SKIP_SCAN";
      break;
    case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
      str += "DYNAMIC_INDEX_RANGE_SCAN";
      break;
    case AccessPath::TABLE_VALUE_CONSTRUCTOR:
      str += "TABLE_VALUE_CONSTRUCTOR";
      break;
    case AccessPath::FAKE_SINGLE_ROW:
      str += "FAKE_SINGLE_ROW";
      break;
    case AccessPath::ZERO_ROWS:
      str += "ZERO_ROWS";
      break;
    case AccessPath::ZERO_ROWS_AGGREGATED:
      str += "ZERO_ROWS_AGGREGATED";
      break;
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
      str += "MATERIALIZED_TABLE_FUNCTION";
      break;
    case AccessPath::UNQUALIFIED_COUNT:
      str += "UNQUALIFIED_COUNT";
      break;
    case AccessPath::NESTED_LOOP_JOIN:
      str += "NESTED_LOOP_JOIN";
      PrintJoinOrder(&path, &join_order);
      break;
    case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
      str += "NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL";
      PrintJoinOrder(&path, &join_order);
      break;
    case AccessPath::BKA_JOIN:
      str += "BKA_JOIN";
      PrintJoinOrder(&path, &join_order);
      break;
    case AccessPath::HASH_JOIN:
      str += "HASH_JOIN";
      PrintJoinOrder(&path, &join_order);
      break;
    case AccessPath::FILTER:
      str += "FILTER";
      break;
    case AccessPath::SORT:
      str += "SORT";
      break;
    case AccessPath::AGGREGATE:
      str += "AGGREGATE";
      break;
    case AccessPath::TEMPTABLE_AGGREGATE:
      str += "TEMPTABLE_AGGREGATE";
      break;
    case AccessPath::LIMIT_OFFSET:
      str += "LIMIT_OFFSET";
      break;
    case AccessPath::STREAM:
      str += "STREAM";
      break;
    case AccessPath::MATERIALIZE:
      str += "MATERIALIZE";
      break;
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
      str += "MATERIALIZE_INFORMATION_SCHEMA_TABLE";
      break;
    case AccessPath::APPEND:
      str += "APPEND";
      break;
    case AccessPath::WINDOW:
      str += "WINDOW";
      break;
    case AccessPath::WEEDOUT:
      str += "WEEDOUT";
      break;
    case AccessPath::REMOVE_DUPLICATES:
      str += "REMOVE_DUPLICATES";
      break;
    case AccessPath::REMOVE_DUPLICATES_ON_INDEX:
      str += "REMOVE_DUPLICATES_ON_INDEX";
      break;
    case AccessPath::ALTERNATIVE:
      str += "ALTERNATIVE";
      break;
    case AccessPath::CACHE_INVALIDATOR:
      str += "CACHE_INVALIDATOR";
      break;
    case AccessPath::DELETE_ROWS:
      str += "DELETE_ROWS";
      break;
    case AccessPath::UPDATE_ROWS:
      str += "UPDATE_ROWS";
      break;
  }

  str += StringPrintf(", cost=%.1f, init_cost=%.1f", path.cost, path.init_cost);
  if (path.init_once_cost != 0.0) {
    str += StringPrintf(", rescan_cost=%.1f", path.rescan_cost());
  }
  str += StringPrintf(", rows=%.1f", path.num_output_rows());

  if (!join_order.empty()) str += ", join_order=" + join_order;

  // Print parameter tables, if any.
  if (path.parameter_tables != 0) {
    str += ", parm={";
    bool first = true;
    for (size_t node_idx : BitsSetIn(path.parameter_tables)) {
      if (!first) {
        str += ", ";
      }
      if ((uint64_t{1} << node_idx) == RAND_TABLE_BIT) {
        str += "<random>";
      } else {
        str += graph.nodes[node_idx].table->alias;
      }
      first = false;
    }
    str += "}";
  }

  if (path.ordering_state != 0) {
    str += StringPrintf(", order=%d", path.ordering_state);
  }

  if (path.safe_for_rowid == AccessPath::SAFE_IF_SCANNED_ONCE) {
    str += StringPrintf(", safe_for_rowid_once");
  } else if (path.safe_for_rowid == AccessPath::UNSAFE) {
    str += StringPrintf(", unsafe_for_rowid");
  }

  DBUG_EXECUTE_IF("subplan_tokens", {
    str += ", token=";
    str += GetForceSubplanToken(const_cast<AccessPath *>(&path),
                                graph.query_block()->join);
  });

  if (strcmp(description_for_trace, "") == 0) {
    return str + "}";
  } else {
    return str + "} [" + description_for_trace + "]";
  }
}

/**
  Used by optimizer trace to print join order of join paths.
  Appends into 'join_order' a string that looks something like '(t1,(t2,t3))'
  where t1 is an alias of any kind of table including materialized table, and
  t1 is joined with (t2,t3) where (t2,t3) is another join.
 */
void PrintJoinOrder(const AccessPath *path, string *join_order) {
  assert(path != nullptr);

  auto func = [join_order](const AccessPath *subpath, const JOIN *) {
    // If it's a table, append its name.
    if (const TABLE *table = GetBasicTable(subpath); table != nullptr) {
      *join_order += table->alias;
      return true;
    }

    AccessPath *outer, *inner;
    switch (subpath->type) {
      case AccessPath::NESTED_LOOP_JOIN:
        outer = subpath->nested_loop_join().outer;
        inner = subpath->nested_loop_join().inner;
        break;
      case AccessPath::HASH_JOIN:
        outer = subpath->hash_join().outer;
        inner = subpath->hash_join().inner;
        break;
      case AccessPath::BKA_JOIN:
        outer = subpath->bka_join().outer;
        inner = subpath->bka_join().inner;
        break;
      case AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL:
        outer = subpath->nested_loop_semijoin_with_duplicate_removal().outer;
        inner = subpath->nested_loop_semijoin_with_duplicate_removal().inner;
        break;
      default:
        return false;  // Allow walker to continue.
    }

    // If we are here, we found a join path.
    join_order->push_back('(');
    PrintJoinOrder(outer, join_order);
    join_order->push_back(',');
    PrintJoinOrder(inner, join_order);
    join_order->push_back(')');

    return true;
  };

  // Fetch tables or joins at inner levels.
  WalkAccessPaths(path, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION, func);
  return;
}

/// Commit OverflowBitsets in path (but not its children) to
/// stable storage (see m_overflow_bitset_mem_root).
void CostingReceiver::CommitBitsetsToHeap(AccessPath *path) const {
  if (path->filter_predicates.IsContainedIn(&m_overflow_bitset_mem_root)) {
    path->filter_predicates = path->filter_predicates.Clone(m_thd->mem_root);
  }
  if (path->delayed_predicates.IsContainedIn(&m_overflow_bitset_mem_root)) {
    path->delayed_predicates = path->delayed_predicates.Clone(m_thd->mem_root);
  }
}

/// Check if all bitsets under “path” are committed to stable storage
/// (see m_overflow_bitset_mem_root). Only relevant in debug mode,
/// as it is expensive.
[[maybe_unused]] bool CostingReceiver::BitsetsAreCommitted(
    AccessPath *path) const {
  // Verify that there are no uncommitted bitsets forgotten in children.
  bool all_ok = true;
  WalkAccessPaths(path, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [this, &all_ok](const AccessPath *subpath, const JOIN *) {
                    all_ok &= !subpath->filter_predicates.IsContainedIn(
                        &m_overflow_bitset_mem_root);
                    all_ok &= !subpath->delayed_predicates.IsContainedIn(
                        &m_overflow_bitset_mem_root);
                    return false;
                  });
  return all_ok;
}

/**
  Propose the given access path as an alternative to the existing access paths
  for the same task (assuming any exist at all), and hold a “tournament” to find
  whether it is better than the others. Only the best alternatives are kept,
  as defined by CompareAccessPaths(); a given access path is kept only if
  it is not dominated by any other path in the group (ie., the Pareto frontier
  is computed). This means that the following are all possible outcomes of the
  tournament:

   - The path is discarded, without ever being inserted in the list
     (dominated by at least one existing entry).
   - The path is inserted as a new alternative in the list (dominates none
     but it also not dominated by any -- or the list was empty), leaving it with
     N+1 entries.
   - The path is inserted as a new alternative in the list, but replaces one
     or more entries (dominates them).
   - The path replaces all existing alternatives, and becomes the sole entry
     in the list.

  “description_for_trace” is a short description of the inserted path
  to distinguish it in optimizer trace, if active. For instance, one might
  write “hash join” when proposing a hash join access path. It may be
  the empty string.

  If the access path is discarded, returns nullptr. Otherwise returns
  a pointer to where it was inserted. (This is useful if you need to
  call CommitBitsetsToHeap() on any of its children, or otherwise do
  work only for access paths that were kept.)
 */
AccessPath *CostingReceiver::ProposeAccessPath(
    AccessPath *path, Prealloced_array<AccessPath *, 4> *existing_paths,
    OrderingSet obsolete_orderings, const char *description_for_trace) const {
  if (m_secondary_engine_cost_hook != nullptr) {
    // If an error was raised by a previous invocation of the hook, reject all
    // paths.
    if (m_thd->is_error()) {
      return nullptr;
    }

    if (m_secondary_engine_cost_hook(m_thd, *m_graph, path)) {
      // Rejected by the secondary engine.
      return nullptr;
    }
    assert(!m_thd->is_error());
    assert(path->init_cost <= path->cost);
    if (!IsEmpty(path->filter_predicates)) {
      assert(path->num_output_rows() <= path->num_output_rows_before_filter);
      assert(path->cost_before_filter <= path->cost);
    }
  }

  DBUG_EXECUTE_IF("subplan_tokens", {
    string token =
        "force_subplan_" + GetForceSubplanToken(path, m_query_block->join);
    DBUG_EXECUTE_IF(token.c_str(), path->forced_by_dbug = true;);
  });

  if (existing_paths->empty()) {
    if (m_trace != nullptr) {
      *m_trace += " - " +
                  PrintAccessPath(*path, *m_graph, description_for_trace) +
                  " is first alternative, keeping\n";
    }
    AccessPath *insert_position = new (m_thd->mem_root) AccessPath(*path);
    existing_paths->push_back(insert_position);
    CommitBitsetsToHeap(insert_position);
    return insert_position;
  }

  // Verify that all row counts are consistent (if someone cares, ie. we are
  // either asserting they are, or tracing, so that a user can see it); we can
  // only do this for unparameterized tables (even though most such
  // inconsistencies probably originate further down the tree), since tables
  // with different parameterizations can have different sargable predicates.
  // (If we really wanted to, we could probably fix that as well, though.)
  // These should never happen, up to numerical issues, but they currently do;
  // see bug #33550360.
  const bool has_known_row_count_inconsistency_bugs =
      m_graph->has_reordered_left_joins || has_clamped_multipart_eq_ref ||
      has_semijoin_with_possibly_clamped_child;
  bool verify_consistency = (m_trace != nullptr);
#ifndef NDEBUG
  if (!has_known_row_count_inconsistency_bugs) {
    // Assert that we are consistent, even if we are not tracing.
    verify_consistency = true;
  }
#endif
  if (verify_consistency && path->parameter_tables == 0 &&
      path->num_output_rows() >= 1e-3) {
    for (const AccessPath *other_path : *existing_paths) {
      if (other_path->parameter_tables == 0 &&
          (other_path->num_output_rows() < path->num_output_rows() * 0.99 ||
           other_path->num_output_rows() > path->num_output_rows() * 1.01)) {
        if (m_trace != nullptr) {
          *m_trace += " - WARNING: " + PrintAccessPath(*path, *m_graph, "") +
                      " has inconsistent row counts with " +
                      PrintAccessPath(*other_path, *m_graph, "") + ".";
          if (has_known_row_count_inconsistency_bugs) {
            *m_trace += "\n   This is a bug, but probably a known one.\n";
          } else {
            *m_trace += " This is a bug.\n";
          }
        }
        if (!has_known_row_count_inconsistency_bugs) {
          assert(false);
        }
        break;
      }
    }
  }

  AccessPath *insert_position = nullptr;
  int num_dominated = 0;
  for (size_t i = 0; i < existing_paths->size(); ++i) {
    PathComparisonResult result = CompareAccessPaths(
        *m_orderings, *path, *((*existing_paths)[i]), obsolete_orderings);
    if (result == PathComparisonResult::DIFFERENT_STRENGTHS) {
      continue;
    }
    if (result == PathComparisonResult::IDENTICAL ||
        result == PathComparisonResult::SECOND_DOMINATES) {
      if (m_trace != nullptr) {
        *m_trace += " - " +
                    PrintAccessPath(*path, *m_graph, description_for_trace) +
                    " is not better than existing path " +
                    PrintAccessPath(*(*existing_paths)[i], *m_graph, "") +
                    ", discarding\n";
      }
      return nullptr;
    }
    if (result == PathComparisonResult::FIRST_DOMINATES) {
      ++num_dominated;
      if (insert_position == nullptr) {
        // Replace this path by the new, better one. We continue to search for
        // other paths to dominate. Note that we don't overwrite just yet,
        // because we might want to print out the old one in optimizer trace
        // below.
        insert_position = (*existing_paths)[i];
      } else {
        // The new path is better than the old one, but we don't need to insert
        // it again. Delete the old one by moving the last one into its place
        // (this may be a no-op) and then chopping one off the end.
        (*existing_paths)[i] = existing_paths->back();
        existing_paths->pop_back();
        --i;
      }
    }
  }

  if (insert_position == nullptr) {
    if (m_trace != nullptr) {
      *m_trace += " - " +
                  PrintAccessPath(*path, *m_graph, description_for_trace) +
                  " is potential alternative, keeping\n";
    }
    insert_position = new (m_thd->mem_root) AccessPath(*path);
    existing_paths->emplace_back(insert_position);
    CommitBitsetsToHeap(insert_position);
    return insert_position;
  }

  if (m_trace != nullptr) {
    if (existing_paths->size() == 1) {  // Only one left.
      if (num_dominated == 1) {
        *m_trace +=
            " - " + PrintAccessPath(*path, *m_graph, description_for_trace) +
            " is better than previous " +
            PrintAccessPath(*insert_position, *m_graph, "") + ", replacing\n";
      } else {
        *m_trace +=
            " - " + PrintAccessPath(*path, *m_graph, description_for_trace) +
            " is better than all previous alternatives, replacing all\n";
      }
    } else {
      assert(num_dominated > 0);
      *m_trace += StringPrintf(
          " - %s is better than %d others, replacing them\n",
          PrintAccessPath(*path, *m_graph, description_for_trace).c_str(),
          num_dominated);
    }
  }
  *insert_position = *path;
  CommitBitsetsToHeap(insert_position);
  return insert_position;
}

AccessPath MakeSortPathWithoutFilesort(THD *thd, AccessPath *child,
                                       ORDER *order, int ordering_state,
                                       int num_where_predicates) {
  assert(order != nullptr);
  AccessPath sort_path;
  sort_path.type = AccessPath::SORT;
  sort_path.ordering_state = ordering_state;
  if (!child->applied_sargable_join_predicates()
           .empty()) {  // Will be empty after grouping.
    MutableOverflowBitset applied_sargable_join_predicates =
        child->applied_sargable_join_predicates().Clone(thd->mem_root);
    applied_sargable_join_predicates.ClearBits(0, num_where_predicates);
    sort_path.applied_sargable_join_predicates() =
        std::move(applied_sargable_join_predicates);
  }
  sort_path.delayed_predicates = child->delayed_predicates;
  sort_path.count_examined_rows = false;
  sort_path.sort().child = child;
  sort_path.sort().filesort = nullptr;
  sort_path.sort().tables_to_get_rowid_for = 0;
  sort_path.sort().order = order;
  sort_path.sort().remove_duplicates = false;
  sort_path.sort().unwrap_rollup = true;
  sort_path.sort().limit = HA_POS_ERROR;
  sort_path.sort().force_sort_rowids = false;
  EstimateSortCost(&sort_path);
  return sort_path;
}

void CostingReceiver::ProposeAccessPathWithOrderings(
    NodeMap nodes, FunctionalDependencySet fd_set,
    OrderingSet obsolete_orderings, AccessPath *path,
    const char *description_for_trace) {
  AccessPathSet *path_set;
  // Insert an empty array if none exists.
  {
    const auto [it, inserted] = m_access_paths.emplace(
        nodes,
        AccessPathSet{Prealloced_array<AccessPath *, 4>{PSI_NOT_INSTRUMENTED},
                      fd_set, obsolete_orderings});
    path_set = &it->second;
    if (!inserted) {
      assert(fd_set == path_set->active_functional_dependencies);
      assert(obsolete_orderings == path_set->obsolete_orderings);
    }
  }

  if (path_set->always_empty) {
    // This subtree is already optimized away. Don't propose any alternative
    // plans, since we've already found the optimal one.
    return;
  }

  if (path->type == AccessPath::ZERO_ROWS) {
    // Clear the other candidates seen for this set of nodes, so that we prefer
    // a simple ZERO_ROWS path, even in the case where we have for example a
    // candidate NESTED_LOOP_JOIN path with zero cost.
    path_set->paths.clear();
    // Mark the subtree as optimized away.
    path_set->always_empty = true;
  }

  ProposeAccessPath(path, &path_set->paths, obsolete_orderings,
                    description_for_trace);

  // Don't bother trying sort-ahead if we are done joining;
  // there's no longer anything to be ahead of, so the regular
  // sort operations will take care of it.
  if (nodes == TablesBetween(0, m_graph->nodes.size())) {
    return;
  }

  if (!SupportedEngineFlag(SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN) &&
      SupportedEngineFlag(SecondaryEngineFlag::AGGREGATION_IS_UNORDERED)) {
    // If sortahead cannot propagate through joins to ORDER BY,
    // and also cannot propagate from anything to aggregation or
    // from aggregation to ORDER BY, it is pointless, so don't try.
    // Note that this also removes rewrite to semijoin via duplicate
    // removal, but that's fine, as it is rarely useful without having
    // nested loops against an index on the outer side.
    return;
  }

  // Don't try to sort-ahead parameterized paths; see the comment in
  // CompareAccessPaths for why.
  if (path->parameter_tables != 0) {
    return;
  }

  path = GetSafePathToSort(m_thd, m_query_block->join, path, m_need_rowid);

  // Try sort-ahead for all interesting orderings.
  // (For the final sort, this might not be so much _ahead_, but still
  // potentially useful, if there are multiple orderings where one is a
  // superset of the other.)
  bool path_is_on_heap = false;
  for (const SortAheadOrdering &sort_ahead_ordering : *m_sort_ahead_orderings) {
    if (!IsSubset(sort_ahead_ordering.required_nodes, nodes)) {
      continue;
    }
    if (sort_ahead_ordering.aggregates_required) {
      // For sort-ahead, we don't have any aggregates yet
      // (since we never group-ahead).
      continue;
    }

    LogicalOrderings::StateIndex new_state = m_orderings->ApplyFDs(
        m_orderings->SetOrder(sort_ahead_ordering.ordering_idx), fd_set);
    if (!m_orderings->MoreOrderedThan(new_state, path->ordering_state,
                                      obsolete_orderings)) {
      continue;
    }

    AccessPath sort_path =
        MakeSortPathWithoutFilesort(m_thd, path, sort_ahead_ordering.order,
                                    new_state, m_graph->num_where_predicates);

    char buf[256];
    if (m_trace != nullptr) {
      if (description_for_trace[0] == '\0') {
        snprintf(buf, sizeof(buf), "sort(%d)",
                 sort_ahead_ordering.ordering_idx);
      } else {
        snprintf(buf, sizeof(buf), "%s, sort(%d)", description_for_trace,
                 sort_ahead_ordering.ordering_idx);
      }
    }
    AccessPath *insert_position = ProposeAccessPath(
        &sort_path, &path_set->paths, obsolete_orderings, buf);
    if (insert_position != nullptr && !path_is_on_heap) {
      path = new (m_thd->mem_root) AccessPath(*path);
      CommitBitsetsToHeap(path);
      insert_position->sort().child = path;
      assert(BitsetsAreCommitted(insert_position));
      path_is_on_heap = true;
    }
  }
}

bool CheckSupportedQuery(THD *thd) {
  if (thd->lex->m_sql_cmd != nullptr &&
      thd->lex->m_sql_cmd->using_secondary_storage_engine() &&
      !Overlaps(EngineFlags(thd),
                MakeSecondaryEngineFlags(
                    SecondaryEngineFlag::SUPPORTS_HASH_JOIN,
                    SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN))) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "the secondary engine in use");
    return true;
  }
  return false;
}

/**
  Set up an access path for streaming or materializing through a temporary
  table. If none is needed (because earlier iterators already materialize
  what needs to be done), returns the path itself.

  The actual temporary table will be created and filled out during finalization.
 */
AccessPath *CreateMaterializationOrStreamingPath(THD *thd, JOIN *join,
                                                 AccessPath *path,
                                                 bool need_rowid,
                                                 bool copy_items) {
  if (!IteratorsAreNeeded(thd, path)) {
    // Let external executors decide for themselves whether they need an
    // intermediate materialization or streaming step. Don't add it to the plan
    // for them.
    return path;
  }

  // See if later sorts will need row IDs from us or not.
  if (!need_rowid) {
    // The common case; we can use streaming.
    if (!copy_items) {
      // StreamingIterator exists only to copy items, so we don't need an
      // iterator here at all.
      return path;
    }
    AccessPath *stream_path = NewStreamingAccessPath(
        thd, path, join, /*temp_table_param=*/nullptr, /*table=*/nullptr,
        /*ref_slice=*/-1);
    EstimateStreamCost(stream_path);
    return stream_path;
  } else {
    // Filesort needs sort by row ID, possibly because large blobs are
    // involved, so we need to actually materialize. (If we wanted a
    // smaller temporary table at the expense of more seeks, we could
    // materialize only aggregate functions and do a multi-table sort
    // by docid, but this situation is rare, so we go for simplicity.)
    return CreateMaterializationPath(thd, join, path, /*temp_table=*/nullptr,
                                     /*temp_table_param=*/nullptr, copy_items);
  }
}

AccessPath *GetSafePathToSort(THD *thd, JOIN *join, AccessPath *path,
                              bool need_rowid) {
  if (need_rowid && path->safe_for_rowid == AccessPath::UNSAFE) {
    // We need to materialize this path before we can sort it,
    // since it might not give us stable row IDs.
    return CreateMaterializationOrStreamingPath(
        thd, join, new (thd->mem_root) AccessPath(*path), need_rowid,
        /*copy_items=*/true);
  } else {
    return path;
  }
}

/**
  Sets up an access path for materializing the results returned from a path in a
  temporary table.
 */
AccessPath *CreateMaterializationPath(THD *thd, JOIN *join, AccessPath *path,
                                      TABLE *temp_table,
                                      Temp_table_param *temp_table_param,
                                      bool copy_items) {
  AccessPath *table_path =
      NewTableScanAccessPath(thd, temp_table, /*count_examined_rows=*/false);
  AccessPath *materialize_path = NewMaterializeAccessPath(
      thd,
      SingleMaterializeQueryBlock(thd, path, /*select_number=*/-1, join,
                                  copy_items, temp_table_param),
      /*invalidators=*/nullptr, temp_table, table_path, /*cte=*/nullptr,
      /*unit=*/nullptr, /*ref_slice=*/-1, /*rematerialize=*/true,
      /*limit_rows=*/HA_POS_ERROR, /*reject_multiple_rows=*/false);

  EstimateMaterializeCost(thd, materialize_path);
  materialize_path->ordering_state = path->ordering_state;
  materialize_path->delayed_predicates = path->delayed_predicates;
  return materialize_path;
}

bool IsMaterializationPath(const AccessPath *path) {
  switch (path->type) {
    case AccessPath::MATERIALIZE:
    case AccessPath::MATERIALIZED_TABLE_FUNCTION:
    case AccessPath::MATERIALIZE_INFORMATION_SCHEMA_TABLE:
      return true;
    default:
      return false;
  }
}

/**
  Is this DELETE target table a candidate for being deleted from immediately,
  while scanning the result of the join? It only checks if it is a candidate for
  immediate delete. Whether it actually ends up being deleted from immediately,
  depends on the plan that is chosen.
 */
bool IsImmediateDeleteCandidate(const Table_ref *table_ref,
                                const Query_block *query_block) {
  assert(table_ref->is_deleted());

  // Cannot delete from the table immediately if it's joined with itself.
  if (unique_table(table_ref, query_block->leaf_tables,
                   /*check_alias=*/false) != nullptr) {
    return false;
  }

  return true;
}

/// Adds all fields of "table" that are referenced from "item" to
/// table->tmp_set.
void AddFieldsToTmpSet(Item *item, TABLE *table) {
  item->walk(&Item::add_field_to_set_processor, enum_walk::SUBQUERY_POSTFIX,
             pointer_cast<uchar *>(table));
}

/**
  Is this UPDATE target table a candidate for being updated immediately, while
  scanning the result of the join? It only checks if it is a candidate for
  immediate update. Whether it actually ends up being updated immediately,
  depends on the plan that is chosen.
 */
bool IsImmediateUpdateCandidate(const Table_ref *table_ref, int node_idx,
                                const JoinHypergraph &graph,
                                table_map target_tables) {
  assert(table_ref->is_updated());
  assert(Overlaps(table_ref->map(), target_tables));
  assert(table_ref->table == graph.nodes[node_idx].table);

  // Cannot update the table immediately if it's joined with itself.
  if (unique_table(table_ref, graph.query_block()->leaf_tables,
                   /*check_alias=*/false) != nullptr) {
    return false;
  }

  TABLE *const table = table_ref->table;

  // Cannot update the table immediately if it modifies a partitioning column,
  // as that could move the row to another partition so that it is seen more
  // than once.
  if (table->part_info != nullptr &&
      table->part_info->num_partitions_used() > 1 &&
      partition_key_modified(table, table->write_set)) {
    return false;
  }

  // If there are at least two tables to update, t1 and t2, t1 being before t2
  // in the plan, we need to collect all fields of t1 which influence the
  // selection of rows from t2. If those fields are also updated, it will not be
  // possible to update t1 on the fly.
  if (!IsSingleBitSet(target_tables)) {
    assert(bitmap_is_clear_all(&table->tmp_set));
    auto restore_tmp_set =
        create_scope_guard([table]() { bitmap_clear_all(&table->tmp_set); });

    // Mark referenced fields in the join conditions in all the simple edges
    // involving this table.
    for (unsigned edge_idx : graph.graph.nodes[node_idx].simple_edges) {
      const RelationalExpression *expr = graph.edges[edge_idx / 2].expr;
      for (Item *condition : expr->join_conditions) {
        AddFieldsToTmpSet(condition, table);
      }
      for (Item_eq_base *condition : expr->equijoin_conditions) {
        AddFieldsToTmpSet(condition, table);
      }
    }

    // Mark referenced fields in the join conditions in all the complex edges
    // involving this table.
    for (unsigned edge_idx : graph.graph.nodes[node_idx].complex_edges) {
      const RelationalExpression *expr = graph.edges[edge_idx / 2].expr;
      for (Item *condition : expr->join_conditions) {
        AddFieldsToTmpSet(condition, table);
      }
      for (Item_eq_base *condition : expr->equijoin_conditions) {
        AddFieldsToTmpSet(condition, table);
      }
    }

    // And mark referenced fields in join conditions that are left in the WHERE
    // clause (typically degenerate join conditions stemming from single-table
    // filters that can't be pushed down due to pseudo-table bits in
    // used_tables()).
    for (unsigned i = 0; i < graph.num_where_predicates; ++i) {
      const Predicate &predicate = graph.predicates[i];
      if (IsProperSubset(TableBitmap(node_idx), predicate.used_nodes)) {
        AddFieldsToTmpSet(predicate.condition, table);
      }
    }

    if (bitmap_is_overlapping(&table->tmp_set, table->write_set)) {
      return false;
    }
  }

  return true;
}

/**
  Finds all the target tables of an UPDATE or DELETE statement. It additionally
  disables covering index scans on the target tables, since ha_update_row() and
  ha_delete_row() can only be called on scans reading the full row.
 */
table_map FindUpdateDeleteTargetTables(const Query_block *query_block) {
  table_map target_tables = 0;
  for (Table_ref *tl = query_block->leaf_tables; tl != nullptr;
       tl = tl->next_leaf) {
    if (tl->is_updated() || tl->is_deleted()) {
      target_tables |= tl->map();
      // Target tables of DELETE and UPDATE need the full row, so disable
      // covering index scans.
      tl->table->no_keyread = true;
      tl->table->covering_keys.clear_all();
    }
  }
  assert(target_tables != 0);
  return target_tables;
}

/**
  Finds all of the target tables of an UPDATE or DELETE statement that are
  candidates from being updated or deleted from immediately while scanning the
  results of the join, without need to buffer the row IDs in a temporary table
  for delayed update/delete after the join has completed. These are candidates
  only; the actual tables to update while scanning, if any, will be chosen based
  on cost during planning.
 */
table_map FindImmediateUpdateDeleteCandidates(const JoinHypergraph &graph,
                                              table_map target_tables,
                                              bool is_delete) {
  table_map candidates = 0;
  for (unsigned node_idx = 0; node_idx < graph.nodes.size(); ++node_idx) {
    const JoinHypergraph::Node &node = graph.nodes[node_idx];
    const Table_ref *tl = node.table->pos_in_table_list;
    if (Overlaps(tl->map(), target_tables)) {
      if (is_delete ? IsImmediateDeleteCandidate(tl, graph.query_block())
                    : IsImmediateUpdateCandidate(tl, node_idx, graph,
                                                 target_tables)) {
        candidates |= tl->map();
      }
    }
  }
  return candidates;
}

// Returns a map containing the node indexes of all tables referenced by a
// full-text MATCH function.
NodeMap FindFullTextSearchedTables(const JoinHypergraph &graph) {
  NodeMap tables = 0;
  for (size_t i = 0; i < graph.nodes.size(); ++i) {
    if (graph.nodes[i].table->pos_in_table_list->is_fulltext_searched()) {
      tables |= TableBitmap(i);
    }
  }
  return tables;
}

// Checks if an item represents a full-text predicate which can be satisfied by
// a full-text index scan. This can be done if the predicate is on one of the
// following forms:
//
//    MATCH(col) AGAINST ('search string')
//    MATCH(col) AGAINST ('search string') > const, where const >= 0
//    MATCH(col) AGAINST ('search string') >= const, where const > 0
//    const < MATCH(col) AGAINST ('search string'), where const >= 0
//    const <= MATCH(col) AGAINST ('search string'), where const > 0
//
// That is, the predicate must return FALSE if MATCH returns zero. The predicate
// cannot be pushed to an index scan if it returns TRUE when MATCH returns zero,
// because a full-text index scan only returns documents with a positive score.
//
// If the item is sargable, the function returns true.
bool IsSargableFullTextIndexPredicate(Item *condition) {
  if (condition->type() != Item::FUNC_ITEM) {
    return false;
  }

  Item_func *func = down_cast<Item_func *>(condition);
  int const_arg_idx = -1;
  bool is_greater_than_op;
  switch (func->functype()) {
    case Item_func::MATCH_FUNC:
      // A standalone MATCH in WHERE is pushable to a full-text index.
      return true;
    case Item_func::GT_FUNC:
      // MATCH > const is pushable to a full-text index if const >= 0. Checked
      // after the switch.
      const_arg_idx = 1;
      is_greater_than_op = true;
      break;
    case Item_func::GE_FUNC:
      // MATCH >= const is pushable to a full-text index if const > 0. Checked
      // after the switch.
      const_arg_idx = 1;
      is_greater_than_op = false;
      break;
    case Item_func::LT_FUNC:
      // Normalize const < MATCH to MATCH > const.
      const_arg_idx = 0;
      is_greater_than_op = true;
      break;
    case Item_func::LE_FUNC:
      // Normalize const <= MATCH to MATCH >= const.
      const_arg_idx = 0;
      is_greater_than_op = false;
      break;
    default:
      // Other kinds of predicates are not pushable to a full-text index.
      return false;
  }

  assert(func->argument_count() == 2);
  assert(const_arg_idx == 0 || const_arg_idx == 1);

  // Only pushable if we have a MATCH function greater-than(-or-equal) a
  // constant value.
  Item *const_arg = func->get_arg(const_arg_idx);
  Item *match_arg = func->get_arg(1 - const_arg_idx);
  if (!is_function_of_type(match_arg, Item_func::FT_FUNC) ||
      !const_arg->const_item()) {
    return false;
  }

  // Evaluate the constant.
  const double value = const_arg->val_real();
  if (const_arg->null_value) {
    // MATCH <op> NULL cannot be pushed to a full-text index.
    return false;
  }

  // Check if the constant is high enough to exclude MATCH = 0, which is the1
  // requirement for being pushable to a full-text index.
  if (is_greater_than_op) {
    return value >= 0;
  } else {
    return value > 0;
  }
}

// Finds all the WHERE predicates that can be satisfied by a full-text index
// scan, and returns a bitmap of those predicates. See
// IsSargableFullTextIndexPredicate() for a description of which predicates are
// sargable.
uint64_t FindSargableFullTextPredicates(const JoinHypergraph &graph) {
  uint64_t fulltext_predicates = 0;
  for (size_t i = 0; i < graph.num_where_predicates; ++i) {
    const Predicate &predicate = graph.predicates[i];
    if (IsSargableFullTextIndexPredicate(predicate.condition)) {
      fulltext_predicates |= uint64_t{1} << i;

      // If the predicate is a standalone MATCH function, flag it as such. This
      // is used by Item_func_match::can_skip_ranking() to determine if ranking
      // is needed. (We could also have set other operation hints here, like
      // FT_OP_GT and FT_OP_GE. These hints are currently not used by any of the
      // storage engines, so we don't set them for now.)
      Item_func *predicate_func = down_cast<Item_func *>(predicate.condition);
      if (predicate_func->functype() == Item_func::MATCH_FUNC) {
        Item_func_match *parent =
            down_cast<Item_func_match *>(predicate_func->get_arg(0))
                ->get_master();
        List<Item_func_match> *funcs =
            parent->table_ref->query_block->ftfunc_list;
        // We only set the hint if this is the only reference to the MATCH
        // function. If it is used other places (for example in the SELECT list
        // or in other predicates) we may still need ranking.
        if (std::none_of(funcs->begin(), funcs->end(),
                         [parent](const Item_func_match &match) {
                           return match.master == parent;
                         })) {
          parent->set_hints_op(FT_OP_NO, 0.0);
        }
      }
    }
  }
  return fulltext_predicates;
}

// Inject casts into comparisons of expressions with incompatible types.
// For example, int_col = string_col is rewritten to
// CAST(int_col AS DOUBLE) = CAST(string_col AS DOUBLE)
bool InjectCastNodes(JoinHypergraph *graph) {
  // Inject cast nodes into the WHERE clause.
  for (Predicate &predicate :
       make_array(graph->predicates.data(), graph->num_where_predicates)) {
    if (predicate.condition->walk(&Item::cast_incompatible_args,
                                  enum_walk::POSTFIX, nullptr)) {
      return true;
    }
  }

  // Inject cast nodes into the join conditions.
  for (JoinPredicate &edge : graph->edges) {
    RelationalExpression *expr = edge.expr;
    if (expr->join_predicate_first != expr->join_predicate_last) {
      // The join predicates have been lifted to the WHERE clause, and casts are
      // already injected into the WHERE clause.
      continue;
    }
    for (Item_eq_base *item : expr->equijoin_conditions) {
      if (item->walk(&Item::cast_incompatible_args, enum_walk::POSTFIX,
                     nullptr)) {
        return true;
      }
    }
    for (Item *item : expr->join_conditions) {
      if (item->walk(&Item::cast_incompatible_args, enum_walk::POSTFIX,
                     nullptr)) {
        return true;
      }
    }
  }

  // Inject cast nodes to the expressions in the SELECT list.
  const JOIN *join = graph->join();
  for (Item *item : *join->fields) {
    if (item->walk(&Item::cast_incompatible_args, enum_walk::POSTFIX,
                   nullptr)) {
      return true;
    }
  }

  // Also GROUP BY expressions and HAVING, to be consistent everywhere.
  for (ORDER *ord = join->group_list.order; ord != nullptr; ord = ord->next) {
    if ((*ord->item)
            ->walk(&Item::cast_incompatible_args, enum_walk::POSTFIX,
                   nullptr)) {
      return true;
    }
  }
  if (join->having_cond != nullptr) {
    if (join->having_cond->walk(&Item::cast_incompatible_args,
                                enum_walk::POSTFIX, nullptr)) {
      return true;
    }
  }

  return false;
}

// Checks if any of the full-text indexes are covering for a table. If the query
// only needs the document ID and the rank, there is no need to access table
// rows. Index-only access can only be used if there is an FTS_DOC_ID column in
// the table, and no other columns must be accessed. All covering full-text
// indexes that are found, are added to TABLE::covering_keys.
void EnableFullTextCoveringIndexes(const Query_block *query_block) {
  for (Item_func_match &match : *query_block->ftfunc_list) {
    TABLE *table = match.table_ref->table;
    if (match.master == nullptr && match.key != NO_SUCH_KEY &&
        table->fts_doc_id_field != nullptr &&
        bitmap_is_set(table->read_set,
                      table->fts_doc_id_field->field_index()) &&
        bitmap_bits_set(table->read_set) == 1) {
      table->covering_keys.set_bit(match.key);
    }
  }
}

/**
  Creates a ZERO_ROWS access path for an always empty join result, or a
  ZERO_ROWS_AGGREGATED in case of an implicitly grouped query. The zero rows
  path is wrapped in FILTER (for HAVING) or LIMIT_OFFSET paths as needed, as
  well as UPDATE_ROWS/DELETE_ROWS paths for UPDATE/DELETE statements.
 */
AccessPath *CreateZeroRowsForEmptyJoin(JOIN *join, const char *cause) {
  join->zero_result_cause = cause;
  join->needs_finalize = true;
  join->create_access_paths_for_zero_rows();
  return join->root_access_path();
}

/**
  Creates an AGGREGATE AccessPath, possibly with an intermediary STREAM node if
  one is needed. The creation of the temporary table does not happen here, but
  is left for FinalizePlanForQueryBlock().

  @param thd The current thread.
  @param join The join to which 'path' belongs.
  @param rollup True for "GROUP BY ... WITH ROLLUP".
  @param row_estimate estimated number of output rows, so that we do not
         need to recalculate it, or kUnknownRowCount if unknown.
  @param trace Optimizer trace.
  @returns The AGGREGATE AccessPath.
 */
AccessPath CreateStreamingAggregationPath(THD *thd, AccessPath *path,
                                          JOIN *join, bool rollup,
                                          double row_estimate, string *trace) {
  AccessPath *child_path = path;
  const Query_block *query_block = join->query_block;

  // Create a streaming node, if one is needed. It is needed for aggregation of
  // some full-text queries, because AggregateIterator doesn't preserve the
  // position of the underlying scans.
  if (join->contains_non_aggregated_fts()) {
    child_path = NewStreamingAccessPath(
        thd, path, join, /*temp_table_param=*/nullptr, /*table=*/nullptr,
        /*ref_slice=*/-1);
    CopyBasicProperties(*path, child_path);
  }

  AccessPath aggregate_path;
  aggregate_path.type = AccessPath::AGGREGATE;
  aggregate_path.aggregate().child = child_path;
  aggregate_path.aggregate().rollup = rollup;
  aggregate_path.set_num_output_rows(row_estimate);
  EstimateAggregateCost(&aggregate_path, query_block, trace);
  return aggregate_path;
}

// If we are planned using in2exists, and our SELECT list has a window
// function, the HAVING condition may include parts that refer to window
// functions. (This cannot happen in standard SQL, but we add such conditions
// as part of in2exists processing.) Split them here.
void SplitHavingCondition(THD *thd, Item *cond, Item **having_cond,
                          Item **having_cond_wf) {
  if (cond == nullptr || !cond->has_wf()) {
    *having_cond = cond;
    *having_cond_wf = nullptr;
    return;
  }

  // If we have a IN-to-EXISTS with window functions and multiple columns,
  // we cannot safely push even the ones that are not dependent on the
  // window functions, as some of them would come before the window functions
  // and change their input data incorrectly. So if so, we need to delay
  // all of them.
  const bool delay_all_in2exists = cond->has_wf();

  Mem_root_array<Item *> cond_parts(thd->mem_root);
  ExtractConditions(cond, &cond_parts);

  List<Item> cond_parts_wf;
  List<Item> cond_parts_normal;
  for (Item *item : cond_parts) {
    if (item->has_wf() ||
        (delay_all_in2exists && item->created_by_in2exists())) {
      cond_parts_wf.push_back(item);
    } else {
      cond_parts_normal.push_back(item);
    }
  }
  *having_cond = CreateConjunction(&cond_parts_normal);
  *having_cond_wf = CreateConjunction(&cond_parts_wf);
}

void ApplyHavingCondition(THD *thd, Item *having_cond, Query_block *query_block,
                          const char *description_for_trace, string *trace,
                          Prealloced_array<AccessPath *, 4> *root_candidates,
                          CostingReceiver *receiver) {
  if (having_cond == nullptr) {
    return;
  }

  if (trace != nullptr) {
    *trace += description_for_trace;
  }

  Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
  for (AccessPath *root_path : *root_candidates) {
    AccessPath filter_path;
    filter_path.type = AccessPath::FILTER;
    filter_path.filter().child = root_path;
    filter_path.filter().condition = having_cond;
    // We don't currently bother with materializing subqueries
    // in HAVING, as they should be rare.
    filter_path.filter().materialize_subqueries = false;
    filter_path.set_num_output_rows(
        root_path->num_output_rows() *
        EstimateSelectivity(thd, having_cond, trace));

    const FilterCost filter_cost = EstimateFilterCost(
        thd, root_path->num_output_rows(), having_cond, query_block);

    filter_path.init_cost =
        root_path->init_cost + filter_cost.init_cost_if_not_materialized;

    filter_path.init_once_cost = root_path->init_once_cost;
    filter_path.cost = root_path->cost + filter_cost.cost_if_not_materialized;
    filter_path.num_output_rows_before_filter = filter_path.num_output_rows();
    filter_path.cost_before_filter = filter_path.cost;
    // TODO(sgunders): Collect and apply functional dependencies from
    // HAVING conditions.
    filter_path.ordering_state = root_path->ordering_state;
    receiver->ProposeAccessPath(&filter_path, &new_root_candidates,
                                /*obsolete_orderings=*/0, "");
  }
  *root_candidates = std::move(new_root_candidates);
}

AccessPath MakeSortPathForDistinct(
    THD *thd, AccessPath *root_path, int ordering_idx,
    bool aggregation_is_unordered, const LogicalOrderings &orderings,
    LogicalOrderings::StateIndex ordering_state) {
  AccessPath sort_path;
  sort_path.type = AccessPath::SORT;
  sort_path.count_examined_rows = false;
  sort_path.sort().child = root_path;
  sort_path.sort().filesort = nullptr;
  sort_path.sort().remove_duplicates = true;
  sort_path.sort().unwrap_rollup = false;
  sort_path.sort().limit = HA_POS_ERROR;
  sort_path.sort().force_sort_rowids = false;

  if (aggregation_is_unordered) {
    // Even though we create a sort node for the distinct operation,
    // the engine does not actually sort the rows. (The deduplication
    // flag is the hint in this case.)
    sort_path.ordering_state = 0;
  } else {
    sort_path.ordering_state = ordering_state;
  }

  // This sort is potentially after materialization, so we must make a
  // copy of the ordering so that ReplaceOrderItemsWithTempTableFields()
  // doesn't accidentally rewrite the items in a sort on the same
  // sort-ahead ordering before the materialization.
  ORDER *order_copy =
      BuildSortAheadOrdering(thd, &orderings, orderings.ordering(ordering_idx));
  sort_path.sort().order = order_copy;

  EstimateSortCost(&sort_path);
  return sort_path;
}

JoinHypergraph::Node *FindNodeWithTable(JoinHypergraph *graph, TABLE *table) {
  for (JoinHypergraph::Node &node : graph->nodes) {
    if (node.table == table) {
      return &node;
    }
  }
  return nullptr;
}

Prealloced_array<AccessPath *, 4> ApplyDistinctAndOrder(
    THD *thd, const CostingReceiver &receiver,
    const LogicalOrderings &orderings, bool aggregation_is_unordered,
    int order_by_ordering_idx, int distinct_ordering_idx,
    const Mem_root_array<SortAheadOrdering> &sort_ahead_orderings,
    FunctionalDependencySet fd_set, Query_block *query_block, bool need_rowid,
    bool force_sort_rowids, Prealloced_array<AccessPath *, 4> root_candidates,
    string *trace) {
  JOIN *join = query_block->join;
  assert(join->select_distinct || join->order.order != nullptr);

  if (root_candidates.empty()) {
    // Nothing to do if the secondary engine has rejected all candidates.
    assert(receiver.HasSecondaryEngineCostHook());
    return root_candidates;
  }

  // If we have both ORDER BY and GROUP BY, we need a materialization step
  // after the grouping (if windowing hasn't already given us one) -- although
  // in most cases, we only need to materialize one row at a time (streaming),
  // so the performance loss should be very slight. This is because when
  // filesort only really deals with fields, not values; when it is to “output”
  // a row, it puts back the contents of the sorted table's (or tables')
  // row buffer(s). For expressions that only depend on the current row, such as
  // (f1 + 1), this is fine, but aggregate functions (Item_sum) depend on
  // multiple rows, so we need a field where filesort can put back its value
  // (and of course, subsequent readers need to read from that field
  // instead of trying to evaluate the Item_sum). A temporary table provides
  // just that, so we create one based on the current field list;
  // StreamingIterator (or MaterializeIterator, if we actually need to
  // materialize) will evaluate all the Items in turn and put their values
  // into the temporary table's fields.
  //
  // For simplicity, we materialize all items in the SELECT list, even those
  // that are not aggregate functions. This is a tiny performance loss,
  // but makes things simpler.
  //
  // The test on join->sum_funcs is mainly to avoid having to create temporary
  // tables in unit tests; the rationale is that if there are no aggregate
  // functions, we also cannot sort on them, and thus, we don't get the
  // problem. Note that we can't do this if sorting by row IDs, as
  // AggregateIterator doesn't preserve them (doing so would probably not be
  // worth it for something that's fairly niche).
  //
  // NOTE: If we elide the sort due to interesting orderings, this might
  // be redundant. It is fairly harmless, though.
  if ((query_block->is_explicitly_grouped() &&
       (*join->sum_funcs != nullptr ||
        join->rollup_state != JOIN::RollupState::NONE || need_rowid)) &&
      join->m_windows.is_empty()) {
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      root_path =
          CreateMaterializationOrStreamingPath(thd, join, root_path, need_rowid,
                                               /*copy_items=*/true);
      receiver.ProposeAccessPath(root_path, &new_root_candidates,
                                 /*obsolete_orderings=*/0, "");
    }
    root_candidates = std::move(new_root_candidates);
  }

  // Now create iterators for DISTINCT, if applicable.
  if (join->select_distinct) {
    if (trace != nullptr) {
      *trace += "Applying sort for DISTINCT\n";
    }

    // Remove redundant elements from the grouping before it is applied.
    // Specifically, we want to remove elements that are constant after all
    // predicates have been applied.
    const Ordering grouping =
        ReduceFinalOrdering(thd, orderings, distinct_ordering_idx);

    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      if (grouping.GetElements().empty()) {
        // Only const fields.
        AccessPath *limit_path = NewLimitOffsetAccessPath(
            thd, root_path, /*limit=*/1, /*offset=*/0, join->calc_found_rows,
            /*reject_multiple_rows=*/false,
            /*send_records_override=*/nullptr);
        receiver.ProposeAccessPath(limit_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, "");
        continue;
      }
      if (!aggregation_is_unordered &&
          orderings.DoesFollowOrder(root_path->ordering_state,
                                    distinct_ordering_idx)) {
        // We don't need the sort, and can do with a simpler deduplication.
        // TODO(sgunders): In some cases, we could apply LIMIT 1,
        // which would be slightly more efficient; see e.g. the test for
        // bug #33148369.
        Item **group_items =
            thd->mem_root->ArrayAlloc<Item *>(grouping.GetElements().size());
        for (size_t i = 0; i < grouping.GetElements().size(); ++i) {
          group_items[i] = orderings.item(grouping.GetElements()[i].item);
        }
        AccessPath *dedup_path = NewRemoveDuplicatesAccessPath(
            thd, root_path, group_items, grouping.GetElements().size());
        CopyBasicProperties(*root_path, dedup_path);
        // TODO(sgunders): Model the actual reduction in rows somehow.
        dedup_path->cost += kAggregateOneRowCost * root_path->num_output_rows();
        receiver.ProposeAccessPath(dedup_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, "sort elided");
        continue;
      }

      root_path = GetSafePathToSort(thd, join, root_path, need_rowid);

      // We need to sort. Try all sort-ahead, not just the one directly
      // derived from DISTINCT clause, because the DISTINCT clause might
      // help us elide the sort for ORDER BY later, if the DISTINCT clause
      // is broader than the ORDER BY clause.
      for (const SortAheadOrdering &sort_ahead_ordering :
           sort_ahead_orderings) {
        LogicalOrderings::StateIndex ordering_state = orderings.ApplyFDs(
            orderings.SetOrder(sort_ahead_ordering.ordering_idx), fd_set);
        // A broader DISTINCT could help elide ORDER BY. Not vice versa. Note
        // that ORDER BY would generally be subset of DISTINCT, but not always.
        // E.g. using ANY_VALUE() in ORDER BY would allow it to be not part of
        // DISTINCT.
        if (sort_ahead_ordering.ordering_idx == distinct_ordering_idx) {
          // The ordering derived from DISTINCT. Always propose this one,
          // regardless of whether it also satisfies the ORDER BY ordering.
        } else if (grouping.GetElements().size() <
                   orderings.ordering(sort_ahead_ordering.ordering_idx)
                       .GetElements()
                       .size()) {
          // This sort-ahead ordering is too wide and may cause duplicates to be
          // returned. Don't propose it.
          continue;
        } else if (order_by_ordering_idx == -1) {
          // There is no ORDER BY to satisfy later, so there is no point in
          // trying to find a sort that satisfies both DISTINCT and ORDER BY.
          continue;
        } else if (!orderings.DoesFollowOrder(ordering_state,
                                              distinct_ordering_idx) ||
                   !orderings.DoesFollowOrder(ordering_state,
                                              order_by_ordering_idx)) {
          // The ordering does not satisfy both of the orderings that are
          // interesting to us. So it's no better than the distinct_ordering_idx
          // one. Don't propose it.
          continue;
        }

        // The force_sort_rowids flag is only set for UPDATE and DELETE,
        // which don't have any syntax for specifying DISTINCT.
        assert(!force_sort_rowids);
        AccessPath sort_path = MakeSortPathForDistinct(
            thd, root_path, sort_ahead_ordering.ordering_idx,
            aggregation_is_unordered, orderings, ordering_state);
        receiver.ProposeAccessPath(&sort_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, "");
      }
    }
    root_candidates = std::move(new_root_candidates);
  }

  // Apply ORDER BY, if applicable.
  if (join->order.order != nullptr) {
    if (root_candidates.empty()) {
      // The secondary engine has rejected all candidates.
      assert(receiver.HasSecondaryEngineCostHook());
      return root_candidates;
    }
    Mem_root_array<TABLE *> tables = CollectTables(
        thd, root_candidates[0]);  // Should be same for all paths.
    if (trace != nullptr) {
      *trace += "Applying sort for ORDER BY\n";
    }

    // If we have LIMIT or OFFSET, we apply them here. This is done so that we
    // can push the LIMIT clause down to the SORT node in order to let Filesort
    // take advantage of it.
    const Query_expression *query_expression = join->query_expression();
    const ha_rows limit_rows = query_expression->select_limit_cnt;
    const ha_rows offset_rows = query_expression->offset_limit_cnt;

    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      // No sort is needed if the candidate already follows the
      // required ordering.
      if (orderings.DoesFollowOrder(root_path->ordering_state,
                                    order_by_ordering_idx)) {
        if (limit_rows != HA_POS_ERROR || offset_rows != 0) {
          root_path = NewLimitOffsetAccessPath(
              thd, root_path, limit_rows, offset_rows, join->calc_found_rows,
              /*reject_multiple_rows=*/false,
              /*send_records_override=*/nullptr);
        }
        receiver.ProposeAccessPath(root_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, "sort elided");
      } else {
        const bool push_limit_to_filesort =
            limit_rows != HA_POS_ERROR && !join->calc_found_rows;

        root_path = GetSafePathToSort(thd, join, root_path, need_rowid);

        AccessPath *sort_path = new (thd->mem_root) AccessPath;
        sort_path->type = AccessPath::SORT;
        sort_path->count_examined_rows = false;
        sort_path->immediate_update_delete_table =
            root_path->immediate_update_delete_table;
        sort_path->sort().child = root_path;
        sort_path->sort().filesort = nullptr;
        sort_path->sort().remove_duplicates = false;
        sort_path->sort().unwrap_rollup = false;
        sort_path->sort().limit =
            push_limit_to_filesort ? limit_rows : HA_POS_ERROR;
        sort_path->sort().order = join->order.order;
        EstimateSortCost(sort_path);

        // If this is a DELETE or UPDATE statement, row IDs must be preserved
        // through the ORDER BY clause, so that we know which rows to delete or
        // update.
        sort_path->sort().force_sort_rowids = force_sort_rowids;

        // If we have a LIMIT clause that is not pushed down to the filesort, or
        // if we have an OFFSET clause, we need to add a LIMIT_OFFSET path on
        // top of the SORT node.
        if ((limit_rows != HA_POS_ERROR && !push_limit_to_filesort) ||
            offset_rows != 0) {
          sort_path = NewLimitOffsetAccessPath(
              thd, sort_path, limit_rows, offset_rows, join->calc_found_rows,
              /*reject_multiple_rows=*/false,
              /*send_records_override=*/nullptr);
        }
        receiver.ProposeAccessPath(sort_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, "");
      }
    }
    root_candidates = std::move(new_root_candidates);
  }
  return root_candidates;
}

static AccessPath *ApplyWindow(THD *thd, AccessPath *root_path, Window *window,
                               JOIN *join, bool need_rowid_for_window) {
  AccessPath *window_path =
      NewWindowAccessPath(thd, root_path, window, /*temp_table_param=*/nullptr,
                          /*ref_slice=*/-1, window->needs_buffering());
  CopyBasicProperties(*root_path, window_path);
  window_path->cost += kWindowOneRowCost * window_path->num_output_rows();

  // NOTE: copy_items = false, because the window iterator does the copying
  // itself.
  return CreateMaterializationOrStreamingPath(thd, join, window_path,
                                              need_rowid_for_window,
                                              /*copy_items=*/false);
}

/**
  Find the ordering that allows us to process the most unprocessed windows.
  If specified, we can also demand that the ordering satisfies one or two
  later orderings (for DISTINCT and/or ORDER BY).

  Our priorities are, in strict order:

    1. Satisfying both DISTINCT and ORDER BY (if both are given;
       but see below).
    2. Satisfying the first operation after windowing
       (which is either DISTINCT or ORDER BY).
    3. Satisfying as many windows as possible.
    4. The shortest possible ordering (as a tie-breaker).

  If first_ordering_idx is given, #2 is mandatory. #4 is so that we don't
  get strange situations where the user specifies e.g. OVER (ORDER BY i)
  and we choose an ordering i,j,k,l,... because it happened to be given
  somewhere else.

  Note that normally, it is very hard to satisfy DISTINCT for a window
  function, because generally, it isn't constant for a given input
  (by nature, it also depends on other rows). But it can happen if the
  window frame is static; see main.window_functions_interesting_orders.

  @param join                    Contains the list of windows.
  @param orderings               Logical orderings in the query block.
  @param sort_ahead_orderings    Candidate orderings to consider.
  @param fd_set                  Active functional dependencies.
  @param finished_windows        Windows to ignore.
  @param tmp_buffer              Temporary space for keeping the best list
                                 of windows so far; must be as large as
                                 the number of values.
  @param first_ordering_idx      The first ordering after the query block
                                 that we need to satisfy (-1 if none).
  @param second_ordering_idx     The second ordering after the query block
                                 that we would like to satisfy (-1 if none).
  @param [out] included_windows  Which windows can be sorted using the given
                                 ordering.

  @return An index into sort_ahead_orderings, or -1 if no ordering could
    be found that sorts at least one window (plus, if first_ordering_idx
    is set, follows that ordering).
 */
static int FindBestOrderingForWindow(
    JOIN *join, const LogicalOrderings &orderings,
    FunctionalDependencySet fd_set,
    const Mem_root_array<SortAheadOrdering> &sort_ahead_orderings,
    Bounds_checked_array<bool> finished_windows,
    Bounds_checked_array<bool> tmp_buffer, int first_ordering_idx,
    int second_ordering_idx, Bounds_checked_array<bool> included_windows) {
  if (first_ordering_idx == -1) {
    assert(second_ordering_idx == -1);
  }

  int best_ordering_idx = -1;
  bool best_following_both_orders = false;
  int best_num_matching_windows = 0;
  for (size_t i = 0; i < sort_ahead_orderings.size(); ++i) {
    const int ordering_idx = sort_ahead_orderings[i].ordering_idx;
    LogicalOrderings::StateIndex ordering_state =
        orderings.ApplyFDs(orderings.SetOrder(ordering_idx), fd_set);

    bool following_both_orders = false;
    if (first_ordering_idx != -1) {
      if (!orderings.DoesFollowOrder(ordering_state, first_ordering_idx)) {
        // Following one is mandatory.
        continue;
      }
      if (second_ordering_idx != -1) {
        if (orderings.DoesFollowOrder(ordering_state, second_ordering_idx)) {
          following_both_orders = true;
        } else if (best_following_both_orders) {
          continue;
        }
      }
    }

    // If we are doing sortahead for DISTINCT/ORDER BY:
    // Find windows that are referred to by DISTINCT/ORDER BY,
    // and disallow them. E.g., if we have
    //
    //   SELECT FOO() OVER w1 AS a ... ORDER BY a,
    //
    // we cannot put w1 in the group of windows that are to be sorted
    // together with ORDER BY.
    for (Window &window : join->m_windows) {
      window.m_mark = false;
    }
    Ordering ordering = orderings.ordering(ordering_idx);
    bool any_wf = false;
    for (OrderElement elem : ordering.GetElements()) {
      WalkItem(orderings.item(elem.item), enum_walk::PREFIX,
               [&any_wf](Item *item) {
                 if (item->m_is_window_function) {
                   down_cast<Item_sum *>(item)->window()->m_mark = true;
                   any_wf = true;
                 }
                 return false;
               });
      if (first_ordering_idx == -1 && any_wf) {
        break;
      }
    }

    // If we are doing sorts _before_ DISTINCT/ORDER BY, simply disallow
    // any sorts on window functions. There should be better options
    // available for us.
    if (first_ordering_idx == -1 && any_wf) {
      continue;
    }

    // Now find out which windows can be processed under this order.
    // We use tmp_buffer to hold which one we selected,
    // and then copy it into included_windows if we are the best so far.
    int num_matching_windows = 0;
    for (size_t window_idx = 0; window_idx < join->m_windows.size();
         ++window_idx) {
      Window *window = join->m_windows[window_idx];
      if (window->m_mark || finished_windows[window_idx] ||
          !orderings.DoesFollowOrder(ordering_state, window->m_ordering_idx)) {
        tmp_buffer[window_idx] = false;
        continue;
      }
      tmp_buffer[window_idx] = true;
      ++num_matching_windows;
    }
    if (num_matching_windows == 0) {
      continue;
    }

    bool is_best;
    if (best_ordering_idx == -1) {
      is_best = true;
    } else if (following_both_orders < best_following_both_orders) {
      is_best = false;
    } else if (following_both_orders > best_following_both_orders) {
      is_best = true;
    } else if (num_matching_windows < best_num_matching_windows) {
      is_best = false;
    } else if (num_matching_windows > best_num_matching_windows) {
      is_best = true;
    } else if (orderings.ordering(ordering_idx).GetElements().size() <
               orderings
                   .ordering(
                       sort_ahead_orderings[best_ordering_idx].ordering_idx)
                   .GetElements()
                   .size()) {
      is_best = true;
    } else {
      is_best = false;
    }
    if (is_best) {
      best_ordering_idx = i;
      best_following_both_orders = following_both_orders;
      best_num_matching_windows = num_matching_windows;
      memcpy(included_windows.array(), tmp_buffer.array(),
             sizeof(bool) * included_windows.size());
    }
  }
  return best_ordering_idx;
}

AccessPath *MakeSortPathAndApplyWindows(
    THD *thd, JOIN *join, AccessPath *root_path, int ordering_idx, ORDER *order,
    const LogicalOrderings &orderings,
    Bounds_checked_array<bool> windows_this_iteration,
    FunctionalDependencySet fd_set, int num_where_predicates,
    bool need_rowid_for_window, int single_window_idx,
    Bounds_checked_array<bool> finished_windows, int *num_windows_left) {
  AccessPath sort_path =
      MakeSortPathWithoutFilesort(thd, root_path, order,
                                  /*ordering_state=*/0, num_where_predicates);
  sort_path.ordering_state =
      orderings.ApplyFDs(orderings.SetOrder(ordering_idx), fd_set);
  root_path = new (thd->mem_root) AccessPath(sort_path);

  if (single_window_idx >= 0) {
    root_path = ApplyWindow(thd, root_path, join->m_windows[single_window_idx],
                            join, need_rowid_for_window);
    finished_windows[single_window_idx] = true;
    --(*num_windows_left);
    return root_path;
  }
  for (size_t window_idx = 0; window_idx < join->m_windows.size();
       ++window_idx) {
    if (!windows_this_iteration[window_idx]) {
      continue;
    }
    root_path = ApplyWindow(thd, root_path, join->m_windows[window_idx], join,
                            need_rowid_for_window);
    finished_windows[window_idx] = true;
    --(*num_windows_left);
  }
  return root_path;
}

}  // namespace

/**
  Apply window functions.

  Ordering of window functions is a tricky topic. We can apply window functions
  in any order that we'd like, but we would like to do as few sorts as possible.
  In its most general form, this would entail solving an instance of the
  traveling salesman problem (TSP), and although the number of windows is
  typically small (one or two in most queries), this can blow up for large
  numbers of windows.

  Thankfully, window functions never add or remove rows. We also assume that all
  sorts are equally expensive (which isn't really true, as ones on more columns
  take more processing time and buffer, but it's close enough in practice),
  and we also ignore the fact that as we compute more buffers, the temporary
  tables and sort buffers will get more columns. These assumptions, combined
  with some reasonable assumptions about ordering transitivity (if an ordering A
  is more sorted than an ordering B, and B > C, then also A > C -- the only
  thing that can disturb this is groupings, which we ignore for the sake of
  simplicity), mean that we need to care _only_ about the number of sorts, and
  can do them greedily. Thus, at any point, we pick the ordering that allows us
  to process the largest number of windows, process them, remove them from
  consideration, and repeat until there are none left.

  There is one more ordering complication; after windowing, we may have DISTINCT
  and/or ORDER BY, which may also benefit from groupings/orderings we leave
  after the last window. Thus, first of all, we see if there's an ordering that
  can satisfy them (ideally both if possible) _and_ at least one window; if so,
  we save that ordering and those windows for last.

  Temporary tables are set up in FinalizePlanForQueryBlock(). This is so that
  it is easier to have multiple different orderings for the temporary table
  parameters later.
 */
static Prealloced_array<AccessPath *, 4> ApplyWindowFunctions(
    THD *thd, const CostingReceiver &receiver,
    const LogicalOrderings &orderings, FunctionalDependencySet fd_set,
    bool aggregation_is_unordered, int order_by_ordering_idx,
    int distinct_ordering_idx, const JoinHypergraph &graph,
    const Mem_root_array<SortAheadOrdering> &sort_ahead_orderings,
    Query_block *query_block, int num_where_predicates, bool need_rowid,
    Prealloced_array<AccessPath *, 4> root_candidates, string *trace) {
  JOIN *join = query_block->join;

  // Figure out if windows need row IDs or not; we won't create
  // the temporary tables before later (since the optimal ordering
  // of windows is cost-based), so this is a conservative check.
  bool need_rowid_for_window = need_rowid;
  if (!need_rowid) {
    for (Item *item : *join->fields) {
      if (item->m_is_window_function && item->is_blob_field()) {
        need_rowid_for_window = true;
        break;
      }
    }
  }

  // Windows we're done processing, or have reserved for the last block.
  auto finished_windows =
      Bounds_checked_array<bool>::Alloc(thd->mem_root, join->m_windows.size());

  // Windows we've reserved for the last block (see function comment).
  auto reserved_windows =
      Bounds_checked_array<bool>::Alloc(thd->mem_root, join->m_windows.size());

  // Temporary space for FindBestOrderingForWindow().
  auto tmp =
      Bounds_checked_array<bool>::Alloc(thd->mem_root, join->m_windows.size());

  // Windows we're doing in this pass.
  auto included_windows =
      Bounds_checked_array<bool>::Alloc(thd->mem_root, join->m_windows.size());

  if (trace) {
    *trace += "\n";
  }
  Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
  for (AccessPath *root_path : root_candidates) {
    if (trace) {
      *trace += "Considering window order on top of " +
                PrintAccessPath(*root_path, graph, "") + "\n";
    }

    // First, go through and check which windows we can do without
    // any reordering, just based on the input ordering we get.
    int num_windows_left = join->m_windows.size();
    for (size_t window_idx = 0; window_idx < join->m_windows.size();
         ++window_idx) {
      Window *window = join->m_windows[window_idx];
      if (window->m_ordering_idx == -1 || join->implicit_grouping ||
          orderings.DoesFollowOrder(root_path->ordering_state,
                                    window->m_ordering_idx)) {
        if (trace) {
          *trace += std::string(" - window ") + window->printable_name() +
                    " does not need further sorting\n";
        }
        root_path =
            ApplyWindow(thd, root_path, window, join, need_rowid_for_window);
        finished_windows[window_idx] = true;
        --num_windows_left;
      } else {
        finished_windows[window_idx] = false;
      }
    }

    // Now, see if we can find an ordering that allows us to process
    // at least one window _and_ an operation after the windowing
    // (DISTINCT, ORDER BY). If so, that ordering will be our last.
    int final_sort_ahead_ordering_idx = -1;
    if ((!aggregation_is_unordered || distinct_ordering_idx == -1) &&
        (distinct_ordering_idx != -1 || order_by_ordering_idx != -1)) {
      int first_ordering_idx, second_ordering_idx;
      if (distinct_ordering_idx == -1) {
        first_ordering_idx = order_by_ordering_idx;
        second_ordering_idx = -1;
      } else {
        first_ordering_idx = distinct_ordering_idx;
        second_ordering_idx = order_by_ordering_idx;
      }
      final_sort_ahead_ordering_idx = FindBestOrderingForWindow(
          join, orderings, fd_set, sort_ahead_orderings, finished_windows, tmp,
          first_ordering_idx, second_ordering_idx, reserved_windows);
      for (size_t window_idx = 0; window_idx < join->m_windows.size();
           ++window_idx) {
        finished_windows[window_idx] |= reserved_windows[window_idx];
      }
    }

    // Now all the other orderings, eventually reaching all windows.
    while (num_windows_left > 0) {
      int sort_ahead_ordering_idx = FindBestOrderingForWindow(
          join, orderings, fd_set, sort_ahead_orderings, finished_windows, tmp,
          /*first_ordering_idx=*/-1,
          /*second_ordering_idx=*/-1, included_windows);
      Bounds_checked_array<bool> windows_this_iteration = included_windows;
      if (sort_ahead_ordering_idx == -1) {
        // None left, so take the one we've saved for last.
        sort_ahead_ordering_idx = final_sort_ahead_ordering_idx;
        windows_this_iteration = reserved_windows;
        final_sort_ahead_ordering_idx = -1;
      }

      if (sort_ahead_ordering_idx == -1) {
        // No sort-ahead orderings left, but some windows are left. The
        // remaining windows are handled after this loop.
        break;
      }

      root_path = MakeSortPathAndApplyWindows(
          thd, join, root_path,
          sort_ahead_orderings[sort_ahead_ordering_idx].ordering_idx,
          sort_ahead_orderings[sort_ahead_ordering_idx].order, orderings,
          windows_this_iteration, fd_set, num_where_predicates,
          need_rowid_for_window, /*single_window_idx*/ -1, finished_windows,
          &num_windows_left);
    }
    // The remaining windows (if any) have orderings which are not present in
    // the interesting orders bitmap, e.g. when the number of orders in the
    // query > kMaxSupportedOrderings. Create a sort path for each of
    // these windows using the window's own order instead of looking up an
    // order in the interesting-orders list.
    for (size_t window_idx = 0;
         window_idx < join->m_windows.size() && num_windows_left > 0;
         ++window_idx) {
      if (finished_windows[window_idx]) continue;

      Bounds_checked_array<bool> windows_this_iteration;
      root_path = MakeSortPathAndApplyWindows(
          thd, join, root_path, join->m_windows[window_idx]->m_ordering_idx,
          join->m_windows[window_idx]->sorting_order(thd), orderings,
          windows_this_iteration, fd_set, num_where_predicates,
          need_rowid_for_window, window_idx, finished_windows,
          &num_windows_left);
    }

    assert(num_windows_left == 0);
    receiver.ProposeAccessPath(root_path, &new_root_candidates,
                               /*obsolete_orderings=*/0, "");
  }
  if (trace) {
    *trace += "\n";
  }
  return new_root_candidates;
}

/**
  Find out if "value" has a type which is compatible with "field" so that it can
  be used for an index lookup if there is an index on "field".
 */
static bool CompatibleTypesForIndexLookup(Item_func_eq *eq_item, Field *field,
                                          Item *value) {
  if (!comparable_in_index(eq_item, field, Field::itRAW, eq_item->functype(),
                           value)) {
    // The types are not comparable in the index, so it's not sargable.
    return false;
  }

  if (field->cmp_type() == STRING_RESULT &&
      field->match_collation_to_optimize_range() &&
      field->charset() != eq_item->compare_collation()) {
    // The collations don't match, so it's not sargable.
    return false;
  }

  return true;
}

/**
  Find out whether “item” is a sargable condition; if so, add it to:

   - The list of sargable predicate for the tables (hypergraph nodes)
     the condition touches. For a regular condition, this will typically
     be one table; for a join condition, it will typically be two.
     If “force_table” is non-nullptr, only that table will be considered
     (this is used for join conditions, to ensure that we do not push
     down predicates that cannot, e.g. to the outer side of left joins).

   - The graph's global list of predicates, if it is not already present
     (predicate_index = -1). This will never happen for WHERE conditions,
     only for join conditions.
 */
static void PossiblyAddSargableCondition(THD *thd, Item *item,
                                         TABLE *force_table,
                                         int predicate_index,
                                         bool is_join_condition,
                                         JoinHypergraph *graph, string *trace) {
  if (!is_function_of_type(item, Item_func::EQ_FUNC)) {
    return;
  }
  Item_func_eq *eq_item = down_cast<Item_func_eq *>(item);
  if (eq_item->get_comparator()->get_child_comparator_count() >= 2) {
    return;
  }
  for (unsigned arg_idx = 0; arg_idx < 2; ++arg_idx) {
    Item **args = eq_item->arguments();
    Item *left = args[arg_idx];
    Item *right = args[1 - arg_idx];
    if (left->type() != Item::FIELD_ITEM) {
      continue;
    }
    Field *field = down_cast<Item_field *>(left)->field;
    TABLE *table = field->table;
    if (force_table != nullptr && force_table != table) {
      continue;
    }
    if (field->part_of_key.is_clear_all()) {
      // Not part of any key, so not sargable. (It could be part of a prefix
      // key, though, but we include them for now.)
      continue;
    }
    if (Overlaps(table->file->ha_table_flags(), HA_NO_INDEX_ACCESS)) {
      // Can't use index lookups on this table, so not sargable.
      continue;
    }
    JoinHypergraph::Node *node = FindNodeWithTable(graph, table);
    if (node == nullptr) {
      // A field in a different query block, so not sargable for us.
      continue;
    }

    // If the equality comes from a multiple equality, we have already verified
    // that the types of the arguments match exactly. For other equalities, we
    // need to check more thoroughly if the types are compatible.
    if (eq_item->source_multiple_equality != nullptr) {
      assert(CompatibleTypesForIndexLookup(eq_item, field, right));
    } else if (!CompatibleTypesForIndexLookup(eq_item, field, right)) {
      continue;
    }

    const table_map used_tables_left = table->pos_in_table_list->map();
    const table_map used_tables_right = right->used_tables();

    if (Overlaps(used_tables_left, used_tables_right)) {
      // Not sargable if the tables on the left and right side overlap, such as
      // t1.x = t1.y + t2.x. Will not be sargable in the opposite direction
      // either, so "break" instead of "continue".
      break;
    }

    if (Overlaps(used_tables_right, RAND_TABLE_BIT)) {
      // Non-deterministic predicates are not sargable. Will not be sargable in
      // the opposite direction either, so "break" instead of "continue".
      break;
    }

    if (trace != nullptr) {
      if (is_join_condition) {
        *trace += "Found sargable join condition " + ItemToString(item) +
                  " on " + node->table->alias + "\n";
      } else {
        *trace += "Found sargable condition " + ItemToString(item) + "\n";
      }
    }

    if (predicate_index == -1) {
      // This predicate is not already registered as a predicate
      // (which means in practice that it's a join predicate,
      // not a WHERE predicate), so add it so that we can refer
      // to it in bitmaps.
      Predicate p;
      p.condition = eq_item;
      p.selectivity = EstimateSelectivity(thd, eq_item, trace);
      p.used_nodes =
          GetNodeMapFromTableMap(eq_item->used_tables() & ~PSEUDO_TABLE_BITS,
                                 graph->table_num_to_node_num);
      p.total_eligibility_set =
          ~0;  // Should never be applied as a WHERE predicate.
      p.functional_dependencies_idx.init(thd->mem_root);
      p.contained_subqueries.init(thd->mem_root);  // Empty.
      graph->predicates.push_back(std::move(p));
      predicate_index = graph->predicates.size() - 1;
      graph->sargable_join_predicates.emplace(eq_item, predicate_index);
    }

    // Can we evaluate the right side of the predicate during optimization (in
    // ref_lookup_subsumes_comparison())? Don't consider items with subqueries
    // or stored procedures constant, as we don't want to execute them during
    // optimization.
    const bool can_evaluate = right->const_for_execution() &&
                              !right->has_subquery() && !right->is_expensive();

    node->sargable_predicates.push_back(
        {predicate_index, field, right, can_evaluate});

    // No need to check the opposite order. We have no indexes on constants.
    if (can_evaluate) break;
  }
}

// Find sargable predicates, ie., those that we can push down into indexes.
// See add_key_field().
//
// TODO(sgunders): Include x=y OR NULL predicates, <=> and IS NULL predicates,
// and the special case of COLLATION accepted in add_key_field().
//
// TODO(sgunders): Integrate with the range optimizer, or find some other way
// of accepting <, >, <= and >= predicates.
void FindSargablePredicates(THD *thd, string *trace, JoinHypergraph *graph) {
  if (trace != nullptr) {
    *trace += "\n";
  }
  for (unsigned i = 0; i < graph->num_where_predicates; ++i) {
    if (IsSingleBitSet(graph->predicates[i].total_eligibility_set)) {
      PossiblyAddSargableCondition(thd, graph->predicates[i].condition,
                                   /*force_table=*/nullptr, i,
                                   /*is_join_condition=*/false, graph, trace);
    }
  }
  for (JoinHypergraph::Node &node : graph->nodes) {
    for (Item *cond : node.join_conditions_pushable_to_this) {
      const auto it = graph->sargable_join_predicates.find(cond);
      int predicate_index =
          (it == graph->sargable_join_predicates.end()) ? -1 : it->second;
      PossiblyAddSargableCondition(thd, cond, node.table, predicate_index,
                                   /*is_join_condition=*/true, graph, trace);
    }
  }
}

static bool ComesFromSameMultiEquality(Item *cond1, Item_eq_base *cond2) {
  return cond2->source_multiple_equality != nullptr &&
         is_function_of_type(cond1, Item_func::EQ_FUNC) &&
         down_cast<Item_func_eq *>(cond1)->source_multiple_equality ==
             cond2->source_multiple_equality;
}

/**
  For each edge, cache some information for each of its join conditions.
  This reduces work when repeatedly applying these join conditions later on.
  In particular, FindContainedSubqueries() contains a large amount of
  virtual function calls that we would like to avoid doing every time
  we consider a given join.
 */
static void CacheCostInfoForJoinConditions(THD *thd,
                                           const Query_block *query_block,
                                           JoinHypergraph *graph,
                                           string *trace) {
  for (JoinPredicate &edge : graph->edges) {
    edge.expr->properties_for_equijoin_conditions.init(thd->mem_root);
    edge.expr->properties_for_join_conditions.init(thd->mem_root);
    for (Item_eq_base *cond : edge.expr->equijoin_conditions) {
      CachedPropertiesForPredicate properties;
      properties.selectivity = EstimateSelectivity(thd, cond, trace);
      properties.contained_subqueries.init(thd->mem_root);
      FindContainedSubqueries(
          cond, query_block, [&properties](const ContainedSubquery &subquery) {
            properties.contained_subqueries.push_back(subquery);
          });

      // Cache information about what sargable conditions this join condition
      // would be redundant against, for RedundantThroughSargable().
      // But don't deduplicate against ourselves (in case we're sargable).
      MutableOverflowBitset redundant(thd->mem_root, graph->predicates.size());
      for (unsigned sargable_pred_idx = graph->num_where_predicates;
           sargable_pred_idx < graph->predicates.size(); ++sargable_pred_idx) {
        Item *sargable_condition =
            graph->predicates[sargable_pred_idx].condition;
        if (sargable_condition != cond &&
            ComesFromSameMultiEquality(sargable_condition, cond)) {
          redundant.SetBit(sargable_pred_idx);
        }
      }
      properties.redundant_against_sargable_predicates = std::move(redundant);
      edge.expr->properties_for_equijoin_conditions.push_back(
          std::move(properties));
    }
    for (Item *cond : edge.expr->join_conditions) {
      CachedPropertiesForPredicate properties;
      properties.selectivity = EstimateSelectivity(thd, cond, trace);
      properties.contained_subqueries.init(thd->mem_root);
      FindContainedSubqueries(
          cond, query_block, [&properties](const ContainedSubquery &subquery) {
            properties.contained_subqueries.push_back(subquery);
          });
      edge.expr->properties_for_join_conditions.push_back(
          std::move(properties));
    }
  }
}

/**
  Find the lowest-cost plan (which hopefully is also the cheapest to execute)
  of all the legal ways to execute the query. The overall order of operations is
  largely dictated by the standard:

    1. All joined tables, including join predicates.
    2. WHERE predicates (we push these down into #1 where allowed)
    3. GROUP BY (it is sometimes possible to push this down into #1,
       but we don't have the functionality to do so).
    4. HAVING.
    5. Window functions.
    6. DISTINCT.
    7. ORDER BY.
    8. LIMIT.
    9. SQL_BUFFER_RESULT (a MySQL extension).

  The place where we have the most leeway by far is #1, which is why this
  part of the optimizer is generally called the join optimizer (there are
  potentially billions of different join orderings, whereas each of the
  other steps, except windowing, can only be done in one or two ways).
  But the various outputs of #1 can have different properties, that can make
  for higher or lower costs in the other steps. (For instance, LIMIT will
  affect candidates with different init_cost differently, and ordering
  properties can skip sorting in ORDER BY entirely.) Thus, we allow keeping
  multiple candidates in play at every step if they are meaningfully different,
  and only pick out the winning candidate based on cost at the very end.
 */
AccessPath *FindBestQueryPlan(THD *thd, Query_block *query_block,
                              string *trace) {
  JOIN *join = query_block->join;
  if (CheckSupportedQuery(thd)) return nullptr;

  // The hypergraph optimizer does not do const tables,
  // nor does it evaluate subqueries during optimization.
  assert(
      IsSubset(OPTION_NO_CONST_TABLES | OPTION_NO_SUBQUERY_DURING_OPTIMIZATION,
               query_block->active_options()));

  // In the case of rollup (only): After the base slice list was made, we may
  // have modified the field list to add rollup group items and sum switchers.
  // The resolver also takes care to update these in query_block->order_list.
  // However, even though the hypergraph join optimizer doesn't use slices,
  // setup_order() later modifies order->item to point into the base slice,
  // where the rollup group items are _not_ updated. Thus, we need to refresh
  // the base slice before we do anything.
  //
  // It would be better to have rollup resolving update the base slice directly,
  // but this would break HAVING in the old join optimizer (see the other call
  // to refresh_base_slice(), in JOIN::make_tmp_tables_info()).
  if (join->rollup_state != JOIN::RollupState::NONE) {
    join->refresh_base_slice();
  }

  // NOTE: Normally, we'd expect join->temp_tables and
  // join->filesorts_to_cleanup to be empty, but since we can get called twice
  // for materialized subqueries, there may already be data there that we must
  // keep.

  // Convert the join structures into a hypergraph.
  JoinHypergraph graph(thd->mem_root, query_block);
  bool where_is_always_false = false;
  if (MakeJoinHypergraph(thd, trace, &graph, &where_is_always_false)) {
    return nullptr;
  }

  if (where_is_always_false) {
    if (trace != nullptr) {
      *trace +=
          "Skipping join order optimization because an always false condition "
          "was found in the WHERE clause.\n";
    }
    return CreateZeroRowsForEmptyJoin(join, "WHERE condition is always false");
  }

  FindSargablePredicates(thd, trace, &graph);

  // Now that we have all join conditions, cache some properties
  // that we'd like to use many times.
  CacheCostInfoForJoinConditions(thd, query_block, &graph, trace);

  // Figure out if any later sort will need row IDs.
  bool need_rowid = false;
  if (query_block->is_explicitly_grouped() || join->order.order != nullptr ||
      join->select_distinct || !join->m_windows.is_empty()) {
    // NOTE: This is distinct from SortWillBeOnRowId(), as it also checks blob
    // fields arising from blob-generating functions on non-blob fields.
    for (Item *item : *join->fields) {
      if (item->is_blob_field()) {
        need_rowid = true;
        break;
      }
    }
    for (Table_ref *tl = query_block->leaf_tables; tl != nullptr && !need_rowid;
         tl = tl->next_leaf) {
      if (SortWillBeOnRowId(tl->table)) {
        need_rowid = true;
      }
    }
  }

  // Find out which predicates contain subqueries.
  MutableOverflowBitset materializable_predicates{thd->mem_root,
                                                  graph.predicates.size()};
  for (unsigned i = 0; i < graph.predicates.size(); ++i) {
    if (ContainsSubqueries(graph.predicates[i].condition)) {
      materializable_predicates.SetBit(i);
    }
  }
  graph.materializable_predicates = std::move(materializable_predicates);

  const bool is_topmost_query_block =
      query_block->outer_query_block() == nullptr;
  const bool is_delete = is_topmost_query_block && IsDeleteStatement(thd);
  const bool is_update = is_topmost_query_block && IsUpdateStatement(thd);

  table_map update_delete_target_tables = 0;
  table_map immediate_update_delete_candidates = 0;
  if (is_delete || is_update) {
    update_delete_target_tables = FindUpdateDeleteTargetTables(query_block);
    immediate_update_delete_candidates = FindImmediateUpdateDeleteCandidates(
        graph, update_delete_target_tables, is_delete);
  }

  NodeMap fulltext_tables = 0;
  uint64_t sargable_fulltext_predicates = 0;
  if (query_block->has_ft_funcs()) {
    fulltext_tables = FindFullTextSearchedTables(graph);

    // Check if we have full-text indexes that can be used.
    sargable_fulltext_predicates = FindSargableFullTextPredicates(graph);
    EnableFullTextCoveringIndexes(query_block);
  }

  // Collect interesting orders from ORDER BY, GROUP BY, semijoins and windows.
  // See BuildInterestingOrders() for more detailed information.
  SecondaryEngineFlags engine_flags = EngineFlags(thd);
  LogicalOrderings orderings(thd);
  Mem_root_array<SortAheadOrdering> sort_ahead_orderings(thd->mem_root);
  Mem_root_array<ActiveIndexInfo> active_indexes(thd->mem_root);
  Mem_root_array<FullTextIndexInfo> fulltext_searches(thd->mem_root);
  int order_by_ordering_idx = -1;
  int group_by_ordering_idx = -1;
  int distinct_ordering_idx = -1;
  BuildInterestingOrders(thd, &graph, query_block, &orderings,
                         &sort_ahead_orderings, &order_by_ordering_idx,
                         &group_by_ordering_idx, &distinct_ordering_idx,
                         &active_indexes, &fulltext_searches, trace);

  if (InjectCastNodes(&graph)) return nullptr;

  // Run the actual join optimizer algorithm. This creates an access path
  // for the join as a whole (with lowest possible cost, and thus also
  // hopefully optimal execution time), with all pushable predicates applied.
  if (trace != nullptr) {
    *trace += "\nEnumerating subplans:\n";
  }
  for (const JoinHypergraph::Node &node : graph.nodes) {
    node.table->init_cost_model(thd->cost_model());
  }
  const secondary_engine_modify_access_path_cost_t secondary_engine_cost_hook =
      SecondaryEngineCostHook(thd);
  CostingReceiver receiver(
      thd, query_block, graph, &orderings, &sort_ahead_orderings,
      &active_indexes, &fulltext_searches, fulltext_tables,
      sargable_fulltext_predicates, update_delete_target_tables,
      immediate_update_delete_candidates, need_rowid, EngineFlags(thd),
      thd->variables.optimizer_max_subgraph_pairs, secondary_engine_cost_hook,
      trace);
  if (graph.edges.empty()) {
    // Fast path for single-table queries. No need to run the join enumeration
    // when there is no join. Just visit the only node directly.
    assert(graph.nodes.size() == 1);
    if (receiver.FoundSingleNode(0) && thd->is_error()) {
      return nullptr;
    }
  } else if (EnumerateAllConnectedPartitions(graph.graph, &receiver) &&
             !thd->is_error() && join->zero_result_cause == nullptr) {
    SimplifyQueryGraph(thd, thd->variables.optimizer_max_subgraph_pairs, &graph,
                       trace);

    // Reset the receiver and run the query again, this time with
    // the simplified hypergraph (and no query limit, in case the
    // given limit was just inherently too low, e.g., one subgraph pair
    // and three tables).
    //
    // It's not given that we _must_ reset the receiver; we could
    // probably have reused its state (which could save time and
    // even lead to a better plan, if we have simplified away some
    // join orderings that have already been evaluated).
    // However, more subgraph pairs also often means we get more access
    // paths on the Pareto frontier for each subgraph, and given
    // that we don't currently have any heuristics to curb the
    // amount of those, it is probably good to get the second-order
    // effect as well and do a full reset.
    if (trace) {
      *trace += "Simplified hypergraph:\n";
      *trace += PrintDottyHypergraph(graph);
      *trace += "\nRestarting query planning with the new graph.\n";
    }
    receiver = CostingReceiver(
        thd, query_block, graph, &orderings, &sort_ahead_orderings,
        &active_indexes, &fulltext_searches, fulltext_tables,
        sargable_fulltext_predicates, update_delete_target_tables,
        immediate_update_delete_candidates, need_rowid, EngineFlags(thd),
        /*subgraph_pair_limit=*/-1, secondary_engine_cost_hook, trace);
    // Reset the secondary engine planning flags
    graph.secondary_engine_costing_flags = {};
    if (EnumerateAllConnectedPartitions(graph.graph, &receiver) &&
        thd->is_error()) {
      return nullptr;
    }
  }
  if (thd->is_error()) return nullptr;

  if (join->zero_result_cause != nullptr) {
    if (trace != nullptr) {
      *trace += "The join returns zero rows. Final cost is 0.0.\n";
    }
    return CreateZeroRowsForEmptyJoin(join, join->zero_result_cause);
  }

  // Get the root candidates. If there is a secondary engine cost hook, there
  // may be no candidates, as the hook may have rejected so many access paths
  // that we could not build a complete plan. Otherwise, expect at least one
  // candidate.
  if (secondary_engine_cost_hook != nullptr &&
      (!receiver.HasSeen(TablesBetween(0, graph.nodes.size())) ||
       receiver.root_candidates().empty())) {
    my_error(ER_SECONDARY_ENGINE, MYF(0),
             "All plans were rejected by the secondary storage engine.");
    return nullptr;
  }
  Prealloced_array<AccessPath *, 4> root_candidates =
      receiver.root_candidates();
  assert(!root_candidates.empty());
  thd->m_current_query_partial_plans += receiver.num_subplans();
  if (trace != nullptr) {
    *trace += StringPrintf(
        "\nEnumerated %zu subplans keeping a total of %zu access paths, "
        "got %zu candidate(s) to finalize:\n",
        receiver.num_subplans(), receiver.num_access_paths(),
        root_candidates.size());
  }

  // If we know the result will be empty, there is no point in adding paths for
  // filters, aggregation, windowing and sorting on top of it further down. Just
  // return the empty result directly.
  if (receiver.always_empty()) {
    for (AccessPath *root_path : root_candidates) {
      if (root_path->type == AccessPath::ZERO_ROWS) {
        if (trace != nullptr) {
          *trace += "The join returns zero rows. Final cost is 0.0.\n";
        }
        return CreateZeroRowsForEmptyJoin(join, root_path->zero_rows().cause);
      }
    }
  }

  // Now we have one or more access paths representing joining all the tables
  // together. (There may be multiple ones because they can be better at
  // different metrics.) We apply the post-join operations to all of them in
  // turn, and then finally pick out the one with the lowest total cost,
  // because at the end, other metrics don't really matter any more.
  //
  // We could have stopped caring about e.g. init_cost after LIMIT
  // has been applied (after which it no longer matters), so that we'd get
  // fewer candidates in each step, but this part is so cheap that it's
  // unlikely to be worth it. We go through ProposeAccessPath() mainly
  // because it gives us better tracing.
  if (trace != nullptr) {
    *trace += "Adding final predicates\n";
  }
  FunctionalDependencySet fd_set = receiver.active_fds_at_root();
  bool has_final_predicates = false;
  for (size_t i = 0; i < graph.num_where_predicates; ++i) {
    // Apply any predicates that don't belong to any
    // specific table, or which are nondeterministic.
    if (!Overlaps(graph.predicates[i].total_eligibility_set,
                  TablesBetween(0, graph.nodes.size())) ||
        Overlaps(graph.predicates[i].total_eligibility_set, RAND_TABLE_BIT)) {
      fd_set |= graph.predicates[i].functional_dependencies;
      has_final_predicates = true;
    }
  }

  // Add the final predicates to the root candidates, and expand FILTER access
  // paths for all predicates (not only the final ones) in the entire access
  // path tree of the candidates.
  //
  // It is an unnecessary step if there are no FILTER access paths to expand.
  // It's not so expensive that it's worth spending a lot of effort to find out
  // if it can be skipped, but let's skip it if our only candidate is an EQ_REF
  // with no filter predicates, so that we don't waste time in point selects.
  if (has_final_predicates ||
      !(root_candidates.size() == 1 &&
        root_candidates[0]->type == AccessPath::EQ_REF &&
        IsEmpty(root_candidates[0]->filter_predicates))) {
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (const AccessPath *root_path : root_candidates) {
      for (bool materialize_subqueries : {false, true}) {
        AccessPath path = *root_path;
        double init_once_cost = 0.0;

        MutableOverflowBitset filter_predicates =
            path.filter_predicates.Clone(thd->mem_root);

        // Apply any predicates that don't belong to any
        // specific table, or which are nondeterministic.
        for (size_t i = 0; i < graph.num_where_predicates; ++i) {
          if (!Overlaps(graph.predicates[i].total_eligibility_set,
                        TablesBetween(0, graph.nodes.size())) ||
              Overlaps(graph.predicates[i].total_eligibility_set,
                       RAND_TABLE_BIT)) {
            filter_predicates.SetBit(i);
            FilterCost cost =
                EstimateFilterCost(thd, root_path->num_output_rows(),
                                   graph.predicates[i].contained_subqueries);
            if (materialize_subqueries) {
              path.cost += cost.cost_if_materialized;
              init_once_cost += cost.cost_to_materialize;
            } else {
              path.cost += cost.cost_if_not_materialized;
            }
            path.set_num_output_rows(path.num_output_rows() *
                                     graph.predicates[i].selectivity);
          }
        }
        path.ordering_state = orderings.ApplyFDs(path.ordering_state, fd_set);

        path.filter_predicates = std::move(filter_predicates);
        const bool contains_subqueries =
            Overlaps(path.filter_predicates, graph.materializable_predicates);

        // Now that we have decided on a full plan, expand all
        // the applied filter maps into proper FILTER nodes
        // for execution. This is a no-op in the second
        // iteration.
        ExpandFilterAccessPaths(thd, &path, join, graph.predicates,
                                graph.num_where_predicates);

        if (materialize_subqueries) {
          assert(path.type == AccessPath::FILTER);
          path.filter().materialize_subqueries = true;
          path.cost += init_once_cost;  // Will be subtracted
                                        // back for rescans.
          path.init_cost += init_once_cost;
          path.init_once_cost += init_once_cost;
        }

        receiver.ProposeAccessPath(&path, &new_root_candidates,
                                   /*obsolete_orderings=*/0,
                                   materialize_subqueries ? "mat. subq" : "");

        if (!contains_subqueries) {
          // Nothing to try to materialize.
          break;
        }
      }
    }
    root_candidates = std::move(new_root_candidates);
  }

  // Apply GROUP BY, if applicable. We currently always do this by sorting
  // first and then using streaming aggregation.
  const bool aggregation_is_unordered = Overlaps(
      engine_flags,
      MakeSecondaryEngineFlags(SecondaryEngineFlag::AGGREGATION_IS_UNORDERED));
  if (query_block->is_grouped()) {
    if (join->make_sum_func_list(*join->fields, /*before_group_by=*/true))
      return nullptr;

    graph.secondary_engine_costing_flags |=
        SecondaryEngineCostingFlag::CONTAINS_AGGREGATION_ACCESSPATH;

    if (trace != nullptr) {
      *trace += "Applying aggregation for GROUP BY\n";
    }

    // Reuse this, so that we do not have to recalculate it for each
    // alternative aggregate path.
    double aggregate_rows = kUnknownRowCount;
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      const bool rollup = (join->rollup_state != JOIN::RollupState::NONE);
      const bool group_needs_sort =
          query_block->is_explicitly_grouped() && !aggregation_is_unordered &&
          !orderings.DoesFollowOrder(root_path->ordering_state,
                                     group_by_ordering_idx);

      if (!group_needs_sort) {
        AccessPath aggregate_path = CreateStreamingAggregationPath(
            thd, root_path, join, rollup, aggregate_rows, trace);
        aggregate_rows = aggregate_path.num_output_rows();
        receiver.ProposeAccessPath(&aggregate_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, "sort elided");
        continue;
      }

      root_path = GetSafePathToSort(thd, join, root_path, need_rowid);

      // We need to sort. Try all sort-ahead, not just the one directly derived
      // from GROUP BY clause, because a broader one might help us elide ORDER
      // BY or DISTINCT later.
      for (const SortAheadOrdering &sort_ahead_ordering :
           sort_ahead_orderings) {
        LogicalOrderings::StateIndex ordering_state = orderings.ApplyFDs(
            orderings.SetOrder(sort_ahead_ordering.ordering_idx), fd_set);
        if (!orderings.DoesFollowOrder(ordering_state, group_by_ordering_idx)) {
          continue;
        }
        if (sort_ahead_ordering.aggregates_required) {
          // We can't sort by an aggregate before we've aggregated.
          continue;
        }

        Mem_root_array<TABLE *> tables = CollectTables(thd, root_path);
        AccessPath *sort_path = new (thd->mem_root) AccessPath;
        sort_path->type = AccessPath::SORT;
        sort_path->count_examined_rows = false;
        sort_path->sort().child = root_path;
        sort_path->sort().filesort = nullptr;
        sort_path->sort().remove_duplicates = false;
        sort_path->sort().unwrap_rollup = true;
        sort_path->sort().limit = HA_POS_ERROR;
        sort_path->sort().force_sort_rowids = false;
        sort_path->sort().order = sort_ahead_ordering.order;
        EstimateSortCost(sort_path);
        assert(!aggregation_is_unordered);
        sort_path->ordering_state = ordering_state;

        char description[256];
        if (trace != nullptr) {
          snprintf(description, sizeof(description), "sort(%d)",
                   sort_ahead_ordering.ordering_idx);
        }

        AccessPath aggregate_path = CreateStreamingAggregationPath(
            thd, sort_path, join, rollup, aggregate_rows, trace);
        aggregate_rows = aggregate_path.num_output_rows();
        receiver.ProposeAccessPath(&aggregate_path, &new_root_candidates,
                                   /*obsolete_orderings=*/0, description);
      }
    }
    root_candidates = std::move(new_root_candidates);

    if (make_group_fields(join, join)) {
      return nullptr;
    }

    // Final setup will be done in FinalizePlanForQueryBlock(),
    // when we have all materialization done.
  }

  // Before we apply the HAVING condition, make sure its used_tables() cache is
  // refreshed. The condition might have been rewritten by
  // FinalizePlanForQueryBlock() to point into a temporary table in a previous
  // execution. Even if that change was rolled back at the end of the previous
  // execution, used_tables() may still say it uses the temporary table.
  if (join->having_cond != nullptr) {
    join->having_cond->update_used_tables();
  }

  // Apply HAVING, if applicable (sans any window-related in2exists parts,
  // which we apply below).
  Item *having_cond;
  Item *having_cond_wf;
  SplitHavingCondition(thd, join->having_cond, &having_cond, &having_cond_wf);
  ApplyHavingCondition(thd, having_cond, query_block,
                       "Applying filter for HAVING\n", trace, &root_candidates,
                       &receiver);

  // If we have GROUP BY followed by a window function (which might include
  // ORDER BY), we might need to materialize before the first ordering -- see
  // the comment near the top of ApplyDistinctAndOrder() for why.
  if (query_block->is_explicitly_grouped() && !join->m_windows.is_empty()) {
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      root_path =
          CreateMaterializationOrStreamingPath(thd, join, root_path, need_rowid,
                                               /*copy_items=*/true);
      receiver.ProposeAccessPath(root_path, &new_root_candidates,
                                 /*obsolete_orderings=*/0, "");
    }
    root_candidates = std::move(new_root_candidates);
  }

  join->m_windowing_steps = !join->m_windows.is_empty();
  if (join->m_windowing_steps) {
    graph.secondary_engine_costing_flags |=
        SecondaryEngineCostingFlag::CONTAINS_WINDOW_ACCESSPATH;
    root_candidates = ApplyWindowFunctions(
        thd, receiver, orderings, fd_set, aggregation_is_unordered,
        order_by_ordering_idx, distinct_ordering_idx, graph,
        sort_ahead_orderings, query_block, graph.num_where_predicates,
        need_rowid, std::move(root_candidates), trace);
  }

  ApplyHavingCondition(
      thd, having_cond_wf, query_block,
      "Applying filter for window function in2exists conditions\n", trace,
      &root_candidates, &receiver);

  graph.secondary_engine_costing_flags |=
      SecondaryEngineCostingFlag::HANDLING_DISTINCT_ORDERBY_LIMITOFFSET;
  if (join->select_distinct || join->order.order != nullptr) {
    // UPDATE and DELETE must preserve row IDs through ORDER BY in order to keep
    // track of which rows to update or delete.
    const bool force_sort_rowids = update_delete_target_tables != 0;

    root_candidates = ApplyDistinctAndOrder(
        thd, receiver, orderings, aggregation_is_unordered,
        order_by_ordering_idx, distinct_ordering_idx, sort_ahead_orderings,
        fd_set, query_block, need_rowid, force_sort_rowids,
        std::move(root_candidates), trace);
  }

  // Apply LIMIT and OFFSET, if applicable. If the query block is ordered, they
  // are already applied by ApplyDistinctAndOrder().
  Query_expression *query_expression = join->query_expression();
  if (join->order.order == nullptr &&
      (query_expression->select_limit_cnt != HA_POS_ERROR ||
       query_expression->offset_limit_cnt != 0)) {
    if (trace != nullptr) {
      *trace += "Applying LIMIT\n";
    }
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      AccessPath *limit_path = NewLimitOffsetAccessPath(
          thd, root_path, query_expression->select_limit_cnt,
          query_expression->offset_limit_cnt, join->calc_found_rows,
          /*reject_multiple_rows=*/false,
          /*send_records_override=*/nullptr);
      receiver.ProposeAccessPath(limit_path, &new_root_candidates,
                                 /*obsolete_orderings=*/0, "");
    }
    root_candidates = std::move(new_root_candidates);
  }

  // Add a DELETE_ROWS or UPDATE_ROWS access path if this is the topmost query
  // block of a DELETE statement or an UPDATE statement.
  if (is_delete) {
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      table_map immediate_tables = 0;
      if (root_path->immediate_update_delete_table != -1) {
        immediate_tables = graph.nodes[root_path->immediate_update_delete_table]
                               .table->pos_in_table_list->map();
      }
      AccessPath *delete_path = NewDeleteRowsAccessPath(
          thd, root_path, update_delete_target_tables, immediate_tables);
      EstimateDeleteRowsCost(delete_path);
      receiver.ProposeAccessPath(delete_path, &new_root_candidates,
                                 /*obsolete_orderings=*/0, "");
    }
    root_candidates = std::move(new_root_candidates);
  } else if (is_update) {
    Prealloced_array<AccessPath *, 4> new_root_candidates(PSI_NOT_INSTRUMENTED);
    for (AccessPath *root_path : root_candidates) {
      table_map immediate_tables = 0;
      if (root_path->immediate_update_delete_table != -1) {
        immediate_tables = graph.nodes[root_path->immediate_update_delete_table]
                               .table->pos_in_table_list->map();
      }
      AccessPath *update_path = NewUpdateRowsAccessPath(
          thd, root_path, update_delete_target_tables, immediate_tables);
      EstimateUpdateRowsCost(update_path);
      receiver.ProposeAccessPath(update_path, &new_root_candidates,
                                 /*obsolete_orderings=*/0, "");
    }
    root_candidates = std::move(new_root_candidates);
  }

  if (thd->is_error()) return nullptr;

  if (root_candidates.empty()) {
    // The secondary engine has rejected so many of the post-processing paths
    // (e.g., sorting, limit, grouping) that we could not build a complete plan.
    assert(secondary_engine_cost_hook != nullptr);
    my_error(ER_SECONDARY_ENGINE, MYF(0),
             "All plans were rejected by the secondary storage engine.");
    return nullptr;
  }

  // TODO(sgunders): If we are part of e.g. a derived table and are streamed,
  // we might want to keep multiple root paths around for future use, e.g.,
  // if there is a LIMIT higher up.
  AccessPath *root_path =
      *std::min_element(root_candidates.begin(), root_candidates.end(),
                        [](const AccessPath *a, const AccessPath *b) {
                          return a->cost < b->cost;
                        });

  // Materialize the result if a top-level query block has the SQL_BUFFER_RESULT
  // option, and the chosen root path isn't already a materialization path. Skip
  // the materialization path when using an external executor, since it will
  // have to decide for itself whether and how to do the materialization.
  if (query_block->active_options() & OPTION_BUFFER_RESULT &&
      is_topmost_query_block && !IsMaterializationPath(root_path) &&
      IteratorsAreNeeded(thd, root_path)) {
    if (trace != nullptr) {
      *trace += "Adding temporary table for SQL_BUFFER_RESULT.\n";
    }

    // If we have windows, we may need to add a materialization for the last
    // window here, or create_tmp_table() will not create fields for its window
    // functions. (All other windows have already been materialized.)
    bool copy_items = join->m_windows.is_empty();
    root_path =
        CreateMaterializationPath(thd, join, root_path, /*temp_table=*/nullptr,
                                  /*temp_table_param=*/nullptr, copy_items);
  }

  if (trace != nullptr) {
    *trace += StringPrintf("Final cost is %.1f.\n", root_path->cost);
  }

#ifndef NDEBUG
  WalkAccessPaths(root_path, join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK,
                  [&](const AccessPath *path, const JOIN *) {
                    assert(path->cost >= path->init_cost);
                    assert(path->init_cost >= path->init_once_cost);
                    return false;
                  });
#endif

  join->needs_finalize = true;
  join->best_rowcount = lrint(root_path->num_output_rows());
  join->best_read = root_path->cost;

  // 0 or 1 rows has a special meaning; it means a _guarantee_ we have no more
  // than one (so-called “const tables”). Make sure we don't give that
  // guarantee unless we have a LIMIT.
  if (join->best_rowcount <= 1 &&
      query_expression->select_limit_cnt - query_expression->offset_limit_cnt >
          1) {
    join->best_rowcount = PLACEHOLDER_TABLE_ROW_ESTIMATE;
  }

  return root_path;
}
