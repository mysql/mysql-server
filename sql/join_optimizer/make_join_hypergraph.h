/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH
#define SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH 1

#include <assert.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "map_helpers.h"
#include "my_table_map.h"
#include "sql/item.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/node_map.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/join_optimizer/secondary_engine_costing_flags.h"
#include "sql/mem_root_array.h"
#include "sql/sql_const.h"

class Field;
class JOIN;
class Query_block;
class THD;
struct MEM_ROOT;
struct RelationalExpression;
struct SecondaryEngineNrowsParameters;
struct TABLE;

/**
  A sargable (from “Search ARGument”) predicate is one that we can attempt
  to push down into an index (what we'd call “ref access” or “index range
  scan”/“quick”). This structure denotes one such instance, precomputed from
  all the predicates in the given hypergraph.
 */
struct SargablePredicate {
  // Index into the “predicates” array in the graph.
  int predicate_index;

  // The predicate is assumed to be <field> = <other_side>.
  // Later, we could push down other kinds of relations, such as
  // greater-than.
  Field *field;
  Item *other_side;

  /// True if it is safe to evaluate "other_side" during optimization. It must
  /// be constant during execution. Also, it should not contain subqueries or
  /// stored procedures, which we do not want to execute during optimization.
  bool can_evaluate;
};

/// Information about a join condition that can potentially be pushed down as a
/// sargable predicate for a Node.
struct PushableJoinCondition {
  /// The condition that may be pushed as a sargable predicate.
  Item *cond;
  /// The relational expression from which this condition is pushable.
  const RelationalExpression *from;
};

/**
  A struct containing a join hypergraph of a single query block, encapsulating
  the constraints given by the relational expressions (e.g. inner joins are
  more freely reorderable than outer joins).

  Since the Hypergraph class does not carry any payloads for nodes and edges,
  and we need to associate e.g.  TABLE pointers with each node, we store our
  extra data in “nodes” and “edges”, indexed the same way the hypergraph is
  indexed.
 */
struct JoinHypergraph {
  JoinHypergraph(MEM_ROOT *mem_root, const Query_block *query_block)
      : graph(mem_root),
        nodes(mem_root),
        edges(mem_root),
        predicates(mem_root),
        m_sargable_join_predicates(mem_root),
        m_query_block(query_block) {}

  hypergraph::Hypergraph graph;

  /// Flags set when AccessPaths are proposed to secondary engines for costing.
  /// The intention of these flags is to avoid traversing the AccessPath tree to
  /// check for certain criteria.
  /// TODO (tikoldit) Move to JOIN or Secondary_engine_execution_context, so
  /// that JoinHypergraph can be immutable during planning
  SecondaryEngineCostingFlags secondary_engine_costing_flags{};

  // Maps table->tableno() to an index in “nodes”, also suitable for
  // a bit index in a NodeMap. This is normally the identity mapping,
  // except for when scalar-to-derived conversion is active.
  std::array<int, MAX_TABLES> table_num_to_node_num;

  class Node final {
   public:
    Node(MEM_ROOT *mem_root, TABLE *table, int64_t read_set_width)
        : m_table{table},
          m_sargable_predicates{mem_root},
          m_pushable_conditions{mem_root},
          m_read_set_width{read_set_width} {
      assert(mem_root != nullptr);
      assert(table != nullptr);
    }

    TABLE *table() const { return m_table; }

    void AddSargable(const SargablePredicate &predicate) {
      assert(predicate.predicate_index >= 0);
      assert(predicate.field != nullptr);
      assert(predicate.other_side != nullptr);
      m_sargable_predicates.push_back(predicate);
    }

    const Mem_root_array<SargablePredicate> &sargable_predicates() const {
      return m_sargable_predicates;
    }

    /**
      Add a join condition that is potentially pushable as a sargable predicate
      for this node.

      @param cond   The condition to be considered for pushdown.
      @param from   The relational expression from which this condition
                    originates.
    */
    void AddPushable(Item *cond, const RelationalExpression *from) {
      // Don't add duplicate conditions, as this causes their selectivity to
      // be applied multiple times, giving poor row estimates (cf.
      // bug#36135001).
      if (std::ranges::none_of(m_pushable_conditions,
                               [&](const PushableJoinCondition &other) {
                                 return ItemsAreEqual(cond, other.cond);
                               })) {
        m_pushable_conditions.push_back({.cond = cond, .from = from});
      }
    }

    const Mem_root_array<PushableJoinCondition> &pushable_conditions() const {
      return m_pushable_conditions;
    }

    hypergraph::NodeMap lateral_dependencies() const {
      return m_lateral_dependencies;
    }

    void set_lateral_dependencies(hypergraph::NodeMap dependencies) {
      m_lateral_dependencies = dependencies;
    }

    int64_t read_set_width() const { return m_read_set_width; }

    /* Estimated rows cardinality after table filters. */
    std::optional<double> cardinality;

   private:
    TABLE *m_table;
    // List of all sargable predicates (see SargablePredicate) where
    // the field is part of this table. When we see the node for
    // the first time, we will evaluate all of these and consider
    // creating access paths that exploit these predicates.
    Mem_root_array<SargablePredicate> m_sargable_predicates;

    // Join conditions that are potentially pushable to this node
    // as sargable predicates (if they are sargable, they will be
    // added to sargable_predicates below, together with sargable
    // non-join conditions). This is a verbatim copy of
    // the m_pushable_conditions member in RelationalExpression,
    // which is computed as a side effect during join pushdown.
    // (We could in principle have gone and collected all join conditions
    // ourselves when determining sargable conditions, but there would be
    // a fair amount of duplicated code in determining pushability,
    // which is why regular join pushdown does the computation.)
    Mem_root_array<PushableJoinCondition> m_pushable_conditions;

    // The lateral dependencies of this table. That is, the set of tables that
    // must be available on the outer side of a nested loop join in which this
    // table is on the inner side. This map may be set for LATERAL derived
    // tables and derived tables with outer references, and for table functions.
    hypergraph::NodeMap m_lateral_dependencies{0};

    // The estimated size (in bytes) of a row. This is used when making cost
    // estimates for hash joins. It is cached here to avoid computing it
    // repeatedly.
    int64_t m_read_set_width{0};
  };
  Mem_root_array<Node> nodes;

  // Note that graph.edges contain each edge twice (see Hypergraph
  // for more information), so edges[i] corresponds to graph.edges[i*2].
  Mem_root_array<JoinPredicate> edges;

  // The first <num_filter_predicates> are filter predicates. These are the
  // predicates that may be added as filters on nodes in the join tree by
  // setting the corresponding bit in AccessPath::filter_predicates, which at
  // the end of join optimization gets expanded to proper FILTER access paths by
  // ExpandFilterAccessPaths(). They include:
  //
  // - Actual WHERE predicates that could not be pushed down into one of the
  //   join conditions.
  //
  // - Predicates that could be pushed all the way down and become a table
  //   filter. These could be WHERE predicates, but they could also be
  //   predicates that are possible to push all the way down, but not possible
  //   to pull all the way up. Take for example "SELECT 1 FROM t1 LEFT JOIN t2
  //   ON t1.a=t2.a AND t2.b=1". The t2.b=1 predicate can be pushed down as a
  //   table filter, but it cannot be used as a WHERE predicate, as it would
  //   incorrectly filter out the NULL-complemented rows. Still, such table
  //   filters are also counted in "num_filter_predicates".
  //
  // - Predicates that are join conditions in some inner join that is involved
  //   in a cycle in the join hypergraph. These are applied as filters in the
  //   join tree if the tables are joined via another edge in the cycle. Such
  //   predicates are also not necessarily possible to pull up to the WHERE
  //   clause. If they for example came from an inner join on the inner side of
  //   some outer join, they cannot be applied as WHERE predicates. Even so,
  //   they are still counted in "num_filter_predicates".
  //
  // The rest are sargable join predicates. The latter are in the array
  // solely so they can be part of the regular "applied_filters" bitmap
  // if they are pushed down into an index, so that we know that we
  // don't need to apply them as join conditions later.
  //
  // Layout of predicates[]:
  //
  //   [ Cycle join preds | WHERE preds | Table filter preds | Sargable join
  //   preds ]
  //   <--------------- num_filter_predicates ---------------->
  //
  //   [0, num_filter_predicates):  "filter predicates"
  //       (see description above)
  //   [num_filter_predicates, predicates.size()):
  //       Sargable join predicates (tracked for index pushdown only).
  //
  // If a sargable join predicate comes from a join that is part of a cycle in
  // the hypergraph, it could be present in both partitions of the array.
  Mem_root_array<Predicate> predicates;

  // How many of the predicates in "predicates" are filter predicates. The rest
  // of them are sargable join predicates.
  unsigned num_filter_predicates = 0;

  /// Returns an immutable view of the filter predicates portion of the
  /// predicates array.
  std::span<const Predicate> filter_predicates() const {
    return {predicates.data(), num_filter_predicates};
  }

  /// Returns a mutable view of the filter predicates portion of the predicates
  /// array.
  std::span<Predicate> filter_predicates() {
    return {predicates.data(), num_filter_predicates};
  }

  // A bitmap over predicates that are, or contain, at least one
  // materializable subquery.
  OverflowBitset materializable_predicates{0};

  /// Returns a pointer to the query block that is being planned.
  const Query_block *query_block() const { return m_query_block; }

  /// Returns a pointer to the JOIN object of the query block being planned.
  const JOIN *join() const;

  /// Whether, at any point, we could rewrite (t1 LEFT JOIN t2) LEFT JOIN t3
  /// to t1 LEFT JOIN (t2 LEFT JOIN t3) or vice versa. We record this purely to
  /// note that we have a known bug/inconsistency in row count estimation
  /// in this case. Bug #33550360 has a test case, but to sum up:
  /// Assume t1 and t3 has 25 rows, but t2 has zero rows, and selectivities
  /// are 0.1. As long as we clamp the row count in FindOutputRowsForJoin(),
  /// and do not modify these selectivities somehow, the former would give
  /// 62.5 rows, and the second would give 25 rows. This should be fixed
  /// eventually, but for now, at least we register it, so that we do not
  /// assert-fail on inconsistent row counts if this (known) issue could be
  /// the root cause.
  bool has_reordered_left_joins = false;

  // True if estimates for one or more Nodes in the graph have been provided
  // by the secondary engine (i.e. at least one Node has the field `cardinality`
  // set).
  bool has_estimates_from_secondary_engine = false;

  /// The set of nodes that are on the inner side of some outer join.
  hypergraph::NodeMap nodes_inner_to_outer_join = 0;

  /// The set of nodes that are on the inner side of some semijoin.
  hypergraph::NodeMap nodes_inner_to_semijoin = 0;

  /// The set of nodes that are on the inner side of some antijoin.
  hypergraph::NodeMap nodes_inner_to_antijoin = 0;

  /// The set of nodes that are represented by table_function. This will be set
  /// only when optimizing for secondary engine.
  hypergraph::NodeMap nodes_for_table_function = 0;

  int FindSargableJoinPredicate(const Item *predicate) const {
    const auto iter = m_sargable_join_predicates.find(predicate);
    return iter == m_sargable_join_predicates.cend() ? -1 : iter->second;
  }

  void AddSargableJoinPredicate(const Item *predicate, int position) {
    m_sargable_join_predicates.emplace(predicate, position);
  }

  using secondary_engine_nrows_hook_t =
      bool (*)(const SecondaryEngineNrowsParameters &params);

  /// Returns true if the secondary engine nrows hook is available.
  bool has_secondary_engine_nrows_hook() const {
    return m_secondary_engine_nrows_hook != nullptr;
  }

  /// Calls the secondary engine nrows hook.
  /// Requires has_secondary_engine_nrows_hook() == true; asserts otherwise.
  bool call_secondary_engine_nrows_hook(
      const SecondaryEngineNrowsParameters &params) const {
    assert(m_secondary_engine_nrows_hook != nullptr);
    return m_secondary_engine_nrows_hook(params);
  }

  /// Sets the secondary engine nrows hook (nullptr means unavailable).
  void set_secondary_engine_nrows_hook(
      secondary_engine_nrows_hook_t secondary_engine_nrows_hook) {
    m_secondary_engine_nrows_hook = secondary_engine_nrows_hook;
  }

 private:
  secondary_engine_nrows_hook_t m_secondary_engine_nrows_hook = nullptr;

  // For each sargable join condition, maps into its index in “predicates”.
  // We need the predicate index when applying the join to figure out whether
  // we have already applied the predicate or not; see
  // {applied,subsumed}_sargable_join_predicates in AccessPath.
  mem_root_unordered_map<const Item *, int> m_sargable_join_predicates;

  /// A pointer to the query block being planned.
  const Query_block *m_query_block;
};

/**
  Make a join hypergraph from the query block given by “graph->query_block”,
  converting from MySQL's join list structures to the ones expected
  by the hypergraph join optimizer. This includes pushdown of WHERE
  predicates, and detection of conditions suitable for hash join.
  However, it does not include simplification of outer to inner joins;
  that is presumed to have happened earlier.

  The result is suitable for running DPhyp (subgraph_enumeration.h)
  to find optimal join planning.
 */
bool MakeJoinHypergraph(THD *thd, JoinHypergraph *graph,
                        bool *where_is_always_false);

// Exposed for testing only.
void MakeJoinGraphFromRelationalExpression(THD *thd, RelationalExpression *expr,
                                           JoinHypergraph *graph);

hypergraph::NodeMap GetNodeMapFromTableMap(
    table_map table_map,
    const std::array<int, MAX_TABLES> &table_num_to_node_num);

std::string PrintDottyHypergraph(const JoinHypergraph &graph);

/// Estimates the size of the hash join keys generated from the equi-join
/// predicates in "expr".
size_t EstimateHashJoinKeyWidth(const RelationalExpression *expr);

table_map GetVisibleTables(const RelationalExpression *expr);

#endif  // SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH
