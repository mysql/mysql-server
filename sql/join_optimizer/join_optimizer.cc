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

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_bit.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/psi/psi_base.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/filesort.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/estimate_selectivity.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/mem_root_array.h"
#include "sql/query_options.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"

using hypergraph::Hyperedge;
using hypergraph::Hypergraph;
using hypergraph::NodeMap;
using std::array;
using std::string;
using std::swap;
using std::vector;

namespace {

// These are extremely arbitrary cost model constants. We should revise them
// based on actual query times (possibly using linear regression?), and then
// put them into the cost model to make them user-tunable. However, until
// we've fixed some glaring omissions such as lack of understanding of initial
// cost, any such estimation will be dominated by outliers/noise.
constexpr double kApplyOneFilterCost = 0.01;
constexpr double kAggregateOneRowCost = 0.01;
constexpr double kSortOneRowCost = 0.01;
constexpr double kHashBuildOneRowCost = 0.01;
constexpr double kHashProbeOneRowCost = 0.01;

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
  CostingReceiver(THD *thd, const JoinHypergraph &graph, string *trace)
      : m_thd(thd), m_graph(graph), m_trace(trace) {}

  bool HasSeen(NodeMap subgraph) const {
    return m_access_paths.count(subgraph) != 0;
  }

  bool FoundSingleNode(int node_idx);

  // Called EmitCsgCmp() in the DPhyp paper.
  bool FoundSubgraphPair(NodeMap left, NodeMap right, int edge_idx);

  AccessPath *root() const {
    const auto it = m_access_paths.find(TablesBetween(0, m_graph.nodes.size()));
    assert(it != m_access_paths.end());
    return it->second;
  }

  size_t num_access_paths() const { return m_access_paths.size(); }

 private:
  THD *m_thd;

  /**
    For each subset of tables that are connected in the join hypergraph,
    keeps the current best access path for producing said subset.
    Also used for communicating connectivity information back to DPhyp
    (in HasSeen()); if there's an entry here, that subset will induce
    a connected subgraph of the join hypergraph.
   */
  std::unordered_map<NodeMap, AccessPath *> m_access_paths;

  /// The graph we are running over.
  const JoinHypergraph &m_graph;

  /// If not nullptr, we store human-readable optimizer trace information here.
  string *m_trace;

  /// For trace use only.
  std::string PrintSet(NodeMap x) {
    std::string ret = "{";
    bool first = true;
    for (size_t node_idx : BitsSetIn(x)) {
      if (!first) {
        ret += ",";
      }
      first = false;
      ret += m_graph.nodes[node_idx]->alias;
    }
    return ret + "}";
  }

  void ProposeHashJoin(NodeMap left, NodeMap right, AccessPath *left_path,
                       AccessPath *right_path, const JoinPredicate *edge);
  void ApplyDelayedPredicatesAfterJoin(NodeMap left, NodeMap right,
                                       const AccessPath *left_path,
                                       const AccessPath *right_path,
                                       AccessPath *join_path);
};

/**
  Called for each table in the query block, at some arbitrary point before we
  start seeing subsets where it's joined to other tables.

  Currently, we support table scan only, so we create a single access path
  corresponding to that and cost it. In this context, “tables” in a query block
  also includes virtual tables such as derived tables, so we need to figure out
  if there is a cost for materializing them.
 */
bool CostingReceiver::FoundSingleNode(int node_idx) {
  TABLE *table = m_graph.nodes[node_idx];

  AccessPath *path = new (m_thd->mem_root) AccessPath;
  path->type = AccessPath::TABLE_SCAN;
  path->count_examined_rows = true;
  path->table_scan().table = table;

  // Doing at least one table scan (this one), so mark the query as such.
  // TODO(sgunders): Move out when we get more types and this access path could
  // be replaced by something else.
  m_thd->set_status_no_index_used();

  TABLE_LIST *tl = table->pos_in_table_list;

  // Ask the storage engine to update stats.records, if needed.
  // NOTE: ha_archive breaks without this call! (That is probably a bug in
  // ha_archive, though.)
  tl->fetch_number_of_rows();

  double num_output_rows = table->file->stats.records;
  double cost = table->file->table_scan_cost().total_cost();

  path->num_output_rows_before_filter = num_output_rows;
  path->cost_before_filter = cost;

  // See which predicates that apply to this table. Some can be applied right
  // away, some require other tables first and must be delayed.
  const NodeMap my_map = TableBitmap(node_idx);
  path->filter_predicates = 0;
  path->delayed_predicates = 0;
  for (size_t i = 0; i < m_graph.predicates.size(); ++i) {
    if (m_graph.predicates[i].total_eligibility_set == my_map) {
      path->filter_predicates |= uint64_t{1} << i;
      cost += num_output_rows * kApplyOneFilterCost;
      num_output_rows *= m_graph.predicates[i].selectivity;
    } else if (Overlaps(m_graph.predicates[i].total_eligibility_set, my_map)) {
      path->delayed_predicates |= uint64_t{1} << i;
    }
  }

  path->num_output_rows = num_output_rows;
  path->cost = cost;

  if (m_trace != nullptr) {
    *m_trace += StringPrintf("Found node %s [rows=%.0f, cost=%.1f]\n",
                             m_graph.nodes[node_idx]->alias,
                             path->num_output_rows, path->cost);
    for (int pred_idx : BitsSetIn(path->filter_predicates)) {
      *m_trace += StringPrintf(
          " - applied predicate %s\n",
          ItemToString(m_graph.predicates[pred_idx].condition).c_str());
    }
  }

  // See if this is an information schema table that must be filled in before
  // we scan.
  if (tl->schema_table != nullptr && tl->schema_table->fill_table) {
    AccessPath *materialize_path =
        NewMaterializeInformationSchemaTableAccessPath(m_thd, path, tl,
                                                       /*condition=*/nullptr);
    materialize_path->num_output_rows = path->num_output_rows;
    materialize_path->num_output_rows_before_filter =
        path->num_output_rows_before_filter;
    materialize_path->cost_before_filter = path->cost;
    materialize_path->cost = path->cost;
    materialize_path->filter_predicates = path->filter_predicates;
    materialize_path->delayed_predicates = path->delayed_predicates;
    path->filter_predicates = path->delayed_predicates = 0;

    // Some information schema tables have zero as estimate, which can lead
    // to completely wild plans. Add a placeholder to make sure we have
    // _something_ to work with.
    if (materialize_path->num_output_rows_before_filter == 0) {
      path->num_output_rows = 1000;
      path->num_output_rows_before_filter = 1000;
      materialize_path->num_output_rows = 1000;
      materialize_path->num_output_rows_before_filter = 1000;
    }

    path = materialize_path;
  }

  if (tl->uses_materialization()) {
    AccessPath *materialize_path;
    if (tl->is_table_function()) {
      // TODO(sgunders): Queries with these are currently disabled,
      // since they may depend on fields from other tables (and then
      // hash join is not possible). When we support parametrized paths,
      // add the correct parameters here, compute some cost
      // and open up for the queries.
      assert(false);
      materialize_path = NewMaterializedTableFunctionAccessPath(
          m_thd, table, tl->table_function, path);
    } else {
      bool rematerialize = tl->derived_unit()->uncacheable != 0;
      if (tl->common_table_expr()) {
        // Handled in clear_corr_something_something, not here
        rematerialize = false;
      }
      materialize_path =
          GetAccessPathForDerivedTable(m_thd, tl, table, rematerialize,
                                       /*invalidators=*/nullptr, path);
    }

    // TODO(sgunders): Take rematerialization cost into account,
    // or maybe, more lack of it.
    materialize_path->cost += path->cost;
    materialize_path->filter_predicates = path->filter_predicates;
    materialize_path->delayed_predicates = path->delayed_predicates;
    path->filter_predicates = path->delayed_predicates = 0;
    path = materialize_path;
  }

  m_access_paths.emplace(TableBitmap(node_idx), path);
  return false;
}

/**
  Called to signal that it's possible to connect the non-overlapping
  table subsets “left” and “right” through the edge given by “edge_idx”
  (which corresponds to an index in m_graph.edges), ie., we have found
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
  assert(left != 0);
  assert(right != 0);
  assert((left & right) == 0);

  const JoinPredicate *edge = &m_graph.edges[edge_idx];

  AccessPath *left_path = m_access_paths[left];
  AccessPath *right_path = m_access_paths[right];

  // For inner joins, the order does not matter.
  // In lieu of a more precise cost model, always keep the one that hashes
  // the fewest amount of rows.
  if (left_path->num_output_rows < right_path->num_output_rows &&
      edge->expr->type == RelationalExpression::INNER_JOIN) {
    ProposeHashJoin(right, left, right_path, left_path, edge);
  } else {
    ProposeHashJoin(left, right, left_path, right_path, edge);
  }

  if (m_access_paths.size() > 100000) {
    // Bail out; we're going to be needing graph simplification
    // (a separate worklog).
    return true;
  }
  return false;
}

double FindOutputRowsForJoin(AccessPath *left_path, AccessPath *right_path,
                             const JoinPredicate *edge) {
  const double outer_rows = left_path->num_output_rows;
  const double inner_rows = right_path->num_output_rows;
  const double selectivity = edge->selectivity;
  if (edge->expr->type == RelationalExpression::ANTIJOIN) {
    return outer_rows * (1.0 - selectivity);
  } else if (edge->expr->type == RelationalExpression::SEMIJOIN) {
    return outer_rows * selectivity;
  } else {
    double num_output_rows = outer_rows * inner_rows * selectivity;
    if (edge->expr->type == RelationalExpression::LEFT_JOIN) {
      num_output_rows = std::max(num_output_rows, outer_rows);
    }
    return num_output_rows;
  }
}

void CostingReceiver::ProposeHashJoin(NodeMap left, NodeMap right,
                                      AccessPath *left_path,
                                      AccessPath *right_path,
                                      const JoinPredicate *edge) {
  AccessPath join_path;
  join_path.type = AccessPath::HASH_JOIN;
  join_path.hash_join().outer = left_path;
  join_path.hash_join().inner = right_path;
  join_path.hash_join().join_predicate = edge;
  join_path.hash_join().store_rowids = false;
  join_path.hash_join().tables_to_get_rowid_for = 0;
  join_path.hash_join().allow_spill_to_disk = true;

  double num_output_rows = FindOutputRowsForJoin(left_path, right_path, edge);

  // TODO(sgunders): Add estimates for spill-to-disk costs.
  double cost = left_path->cost + right_path->cost;
  cost += right_path->num_output_rows * kHashBuildOneRowCost;
  cost += left_path->num_output_rows * kHashProbeOneRowCost;

  // Note: This isn't strictly correct if the non-equijoin conditions
  // have selectivities far from 1.0; the cost should be calculated
  // on the number of rows after the equijoin conditions, but before
  // the non-equijoin conditions.
  cost += num_output_rows * edge->expr->join_conditions.size() *
          kApplyOneFilterCost;

  join_path.num_output_rows_before_filter = num_output_rows;
  join_path.cost_before_filter = cost;
  join_path.num_output_rows = num_output_rows;
  join_path.cost = cost;

  ApplyDelayedPredicatesAfterJoin(left, right, left_path, right_path,
                                  &join_path);

  AccessPath *path = &join_path;

  if (m_trace != nullptr) {
    *m_trace += StringPrintf(
        "Found sets %s and %s, connected by condition %s [rows=%.0f, "
        "cost=%.1f]\n",
        PrintSet(left).c_str(), PrintSet(right).c_str(),
        GenerateExpressionLabel(edge->expr).c_str(), path->num_output_rows,
        path->cost);
    for (int pred_idx : BitsSetIn(path->filter_predicates)) {
      *m_trace += StringPrintf(
          " - applied (delayed) predicate %s\n",
          ItemToString(m_graph.predicates[pred_idx].condition).c_str());
    }
  }

  auto it = m_access_paths.find(left | right);
  if (it == m_access_paths.end()) {
    if (m_trace != nullptr) {
      *m_trace += " - first alternative for this join, keeping\n";
    }
    m_access_paths.emplace(left | right,
                           new (m_thd->mem_root) AccessPath(*path));
  } else {
    if (it->second->cost > path->cost) {
      if (m_trace != nullptr) {
        *m_trace += StringPrintf(" - cheaper than old cost %.1f, keeping\n",
                                 it->second->cost);
      }
      *it->second = *path;
    } else {
      if (m_trace != nullptr) {
        *m_trace +=
            StringPrintf(" - more expensive than old cost %.1f, discarding\n",
                         it->second->cost);
      }
    }
  }
}

// Of all delayed predicates, see which ones we can apply now, and which
// ones that need to be delayed further.
void CostingReceiver::ApplyDelayedPredicatesAfterJoin(
    NodeMap left, NodeMap right, const AccessPath *left_path,
    const AccessPath *right_path, AccessPath *join_path) {
  join_path->filter_predicates = 0;
  join_path->delayed_predicates =
      left_path->delayed_predicates ^ right_path->delayed_predicates;
  const NodeMap ready_tables = left | right;
  for (int pred_idx : BitsSetIn(left_path->delayed_predicates &
                                right_path->delayed_predicates)) {
    if (IsSubset(m_graph.predicates[pred_idx].total_eligibility_set,
                 ready_tables)) {
      join_path->filter_predicates |= uint64_t{1} << pred_idx;
      join_path->cost += join_path->num_output_rows * kApplyOneFilterCost;
      join_path->num_output_rows *= m_graph.predicates[pred_idx].selectivity;
    } else {
      join_path->delayed_predicates |= uint64_t{1} << pred_idx;
    }
  }
}

/**
  Create a table array from a table bitmap.
 */
Prealloced_array<TABLE *, 4> CollectTables(SELECT_LEX *select_lex,
                                           table_map tmap) {
  Prealloced_array<TABLE *, 4> tables(PSI_NOT_INSTRUMENTED);
  for (TABLE_LIST *tl = select_lex->leaf_tables; tl != nullptr;
       tl = tl->next_leaf) {
    if (tl->map() & tmap) {
      tables.push_back(tl->table);
    }
  }
  return tables;
}

bool CheckSupportedQuery(THD *thd, JOIN *join) {
  if (join->rollup_state != JOIN::RollupState::NONE) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "ROLLUP");
    return true;
  }
  if (join->select_lex->has_ft_funcs()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "fulltext search");
    return true;
  }
  if (join->select_distinct) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "DISTINCT");
    return true;
  }
  if (join->select_lex->is_recursive()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "recursive CTEs");
    return true;
  }
  if (thd->lex->m_sql_cmd->using_secondary_storage_engine()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "secondary engine");
    return true;
  }
  if (join->select_lex->has_windows()) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "windowing functions");
    return true;
  }
  if (join->select_lex->active_options() & OPTION_BUFFER_RESULT) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "SQL_BUFFER_RESULT");
    return true;
  }
  if (join->select_lex->is_grouped() && join->select_lex->is_ordered()) {
    // This runs into problems with slices currently.
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0),
             "ORDER BY and GROUP BY at the same time");
    return true;
  }
  for (TABLE_LIST *tl = join->select_lex->leaf_tables; tl != nullptr;
       tl = tl->next_leaf) {
    if (tl->is_derived() && tl->derived_unit()->m_lateral_deps) {
      my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "LATERAL");
      return true;
    }
    // We actually support table functions, but not joins against them.
    if (tl->is_table_function()) {
      my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "table functions");
      return true;
    }
  }
  return false;
}

double EstimateSortCost(double num_rows) {
  if (num_rows <= 1.0) {
    // Avoid NaNs from log2().
    return kSortOneRowCost;
  } else {
    return kSortOneRowCost * num_rows * std::max(log2(num_rows), 1.0);
  }
}

}  // namespace

AccessPath *FindBestQueryPlan(THD *thd, SELECT_LEX *select_lex, string *trace) {
  JOIN *join = select_lex->join;
  if (CheckSupportedQuery(thd, join)) return nullptr;

  // Convert the join structures into a hypergraph.
  JoinHypergraph graph(thd->mem_root);
  if (MakeJoinHypergraph(thd, select_lex, trace, &graph)) {
    return nullptr;
  }

  // Run the actual join optimizer algorithm. This creates an access path
  // for the join as a whole (with lowest possible cost, and thus also
  // hopefully optimal execution time), with all pushable predicates applied.
  if (trace != nullptr) {
    *trace += "Enumerating subplans:\n";
  }
  for (TABLE *table : graph.nodes) {
    table->init_cost_model(thd->cost_model());
  }
  CostingReceiver receiver(thd, graph, trace);
  if (EnumerateAllConnectedPartitions(graph.graph, &receiver)) {
    my_error(ER_HYPERGRAPH_NOT_SUPPORTED_YET, MYF(0), "large join graphs");
    return nullptr;
  }
  thd->m_current_query_partial_plans += receiver.num_access_paths();
  if (trace != nullptr) {
    *trace += StringPrintf("\nEnumerated %zu subplans.\n",
                           receiver.num_access_paths());
  }

  AccessPath *root_path = receiver.root();

  // Apply any predicates that don't belong to any specific table,
  // or which are nondeterministic.
  for (size_t i = 0; i < graph.predicates.size(); ++i) {
    if (!Overlaps(graph.predicates[i].total_eligibility_set,
                  TablesBetween(0, graph.nodes.size())) ||
        Overlaps(graph.predicates[i].total_eligibility_set, RAND_TABLE_BIT)) {
      root_path->filter_predicates |= uint64_t{1} << i;
      root_path->cost += root_path->num_output_rows * kApplyOneFilterCost;
      root_path->num_output_rows *= graph.predicates[i].selectivity;
    }
  }

  // Now that we have decided on a full plan, expand all the applied
  // filter maps into proper FILTER nodes for execution.
  ExpandFilterAccessPaths(thd, root_path, graph.predicates);

  // Apply GROUP BY, if applicable. We currently always do this by sorting
  // first and then using streaming aggregation.
  if (select_lex->is_grouped()) {
    if (join->make_sum_func_list(*join->fields, /*before_group_by=*/true))
      return nullptr;

    if (select_lex->is_explicitly_grouped()) {
      Prealloced_array<TABLE *, 4> tables =
          CollectTables(select_lex, GetUsedTables(root_path));
      Filesort *filesort = new (thd->mem_root)
          Filesort(thd, std::move(tables), /*keep_buffers=*/false,
                   select_lex->group_list.first,
                   /*limit_arg=*/HA_POS_ERROR, /*force_stable_sort=*/false,
                   /*remove_duplicates=*/false, /*force_sort_positions=*/false,
                   /*unwrap_rollup=*/false);
      AccessPath *sort_path = NewSortAccessPath(thd, root_path, filesort,
                                                /*count_examined_rows=*/false);

      sort_path->num_output_rows = root_path->num_output_rows;
      sort_path->cost =
          root_path->cost + EstimateSortCost(root_path->num_output_rows);
      sort_path->num_output_rows_before_filter = sort_path->num_output_rows;
      sort_path->cost_before_filter = sort_path->cost;

      root_path = sort_path;

      if (!filesort->using_addon_fields()) {
        FindTablesToGetRowidFor(sort_path);
      }
    }

    AccessPath *aggregate_path =
        NewAggregateAccessPath(thd, root_path, /*rollup=*/false);

    // TODO(sgunders): How do we estimate how many rows aggregation
    // will be reducing the output by?
    aggregate_path->num_output_rows = root_path->num_output_rows;
    aggregate_path->cost =
        root_path->cost + kAggregateOneRowCost * root_path->num_output_rows;
    aggregate_path->num_output_rows_before_filter =
        aggregate_path->num_output_rows;
    aggregate_path->cost_before_filter = aggregate_path->cost;

    root_path = aggregate_path;

    Item_sum **func_ptr = join->sum_funcs;
    Item_sum *func;
    bool need_distinct = true;  // We don't support loose index scan yet.
    while ((func = *(func_ptr++))) {
      Aggregator::Aggregator_type type =
          need_distinct && func->has_with_distinct()
              ? Aggregator::DISTINCT_AGGREGATOR
              : Aggregator::SIMPLE_AGGREGATOR;
      if (func->set_aggregator(type) || func->aggregator_setup(thd)) {
        return nullptr;
      }
    }
    if (make_group_fields(join, join)) {
      return nullptr;
    }
  }

  // Apply HAVING, if applicable.
  if (join->having_cond != nullptr) {
    AccessPath *filter_path =
        NewFilterAccessPath(thd, root_path, join->having_cond);
    filter_path->num_output_rows =
        root_path->num_output_rows *
        EstimateSelectivity(thd, join->having_cond, trace);
    filter_path->cost =
        root_path->cost + root_path->num_output_rows * kApplyOneFilterCost;
    filter_path->num_output_rows_before_filter = filter_path->num_output_rows;
    filter_path->cost_before_filter = filter_path->cost;

    root_path = filter_path;
  }

  // Apply ORDER BY, if applicable.
  if (select_lex->is_ordered()) {
    Prealloced_array<TABLE *, 4> tables =
        CollectTables(select_lex, GetUsedTables(root_path));
    Filesort *filesort = new (thd->mem_root)
        Filesort(thd, std::move(tables), /*keep_buffers=*/false,
                 select_lex->order_list.first,
                 /*limit_arg=*/HA_POS_ERROR, /*force_stable_sort=*/false,
                 /*remove_duplicates=*/false, /*force_sort_positions=*/false,
                 /*unwrap_rollup=*/false);

    AccessPath *sort_path = NewSortAccessPath(thd, root_path, filesort,
                                              /*count_examined_rows=*/false);
    sort_path->num_output_rows = root_path->num_output_rows;
    sort_path->cost =
        root_path->cost + EstimateSortCost(root_path->num_output_rows);
    sort_path->num_output_rows_before_filter = sort_path->num_output_rows;
    sort_path->cost_before_filter = sort_path->cost;

    root_path = sort_path;

    if (!filesort->using_addon_fields()) {
      FindTablesToGetRowidFor(sort_path);
    }
  }

  // Apply LIMIT, if applicable.
  SELECT_LEX_UNIT *unit = join->unit;
  if (unit->select_limit_cnt != HA_POS_ERROR || unit->offset_limit_cnt != 0) {
    root_path =
        NewLimitOffsetAccessPath(thd, root_path, unit->select_limit_cnt,
                                 unit->offset_limit_cnt, join->calc_found_rows,
                                 /*reject_multiple_rows=*/false,
                                 /*send_records_override=*/nullptr);
  }

  join->best_rowcount = lrint(root_path->num_output_rows);
  join->best_read = root_path->cost;

  // 0 or 1 rows has a special meaning; it means a _guarantee_ we have no more
  // than one (so-called “const tables”). Make sure we don't give that guarantee
  // unless we have a LIMIT.
  if (join->best_rowcount <= 1 &&
      unit->select_limit_cnt - unit->offset_limit_cnt > 1) {
    join->best_rowcount = PLACEHOLDER_TABLE_ROW_ESTIMATE;
  }

  return root_path;
}
