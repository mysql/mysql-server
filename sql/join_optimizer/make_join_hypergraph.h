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

#ifndef SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH
#define SQL_JOIN_OPTIMIZER_MAKE_JOIN_HYPERGRAPH 1

#include <array>
#include <string>

#include "map_helpers.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/secondary_engine_costing_flags.h"
#include "sql/mem_root_array.h"
#include "sql/sql_const.h"

class Field;
class Item;
class JOIN;
class Query_block;
class THD;
struct MEM_ROOT;
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
        sargable_join_predicates(mem_root),
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

  struct Node {
    TABLE *table;

    // Join conditions that are potentially pushable to this node
    // as sargable predicates (if they are sargable, they will be
    // added to sargable_predicates below, together with sargable
    // non-join conditions). This is a verbatim copy of
    // the join_conditions_pushable_to_this member in RelationalExpression,
    // which is computed as a side effect during join pushdown.
    // (We could in principle have gone and collected all join conditions
    // ourselves when determining sargable conditions, but there would be
    // a fair amount of duplicated code in determining pushability,
    // which is why regular join pushdown does the computation.)
    Mem_root_array<Item *> join_conditions_pushable_to_this;

    // List of all sargable predicates (see SargablePredicate) where
    // the field is part of this table. When we see the node for
    // the first time, we will evaluate all of these and consider
    // creating access paths that exploit these predicates.
    Mem_root_array<SargablePredicate> sargable_predicates;
  };
  Mem_root_array<Node> nodes;

  // Note that graph.edges contain each edge twice (see Hypergraph
  // for more information), so edges[i] corresponds to graph.edges[i*2].
  Mem_root_array<JoinPredicate> edges;

  // The first <num_where_predicates> are WHERE predicates;
  // the rest are sargable join predicates. The latter are in the array
  // solely so they can be part of the regular “applied_filters” bitmap
  // if they are pushed down into an index, so that we know that we
  // don't need to apply them as join conditions later.
  Mem_root_array<Predicate> predicates;

  unsigned num_where_predicates = 0;

  // A bitmap over predicates that are, or contain, at least one
  // materializable subquery.
  OverflowBitset materializable_predicates{0};

  // For each sargable join condition, maps into its index in “predicates”.
  // We need the predicate index when applying the join to figure out whether
  // we have already applied the predicate or not; see
  // {applied,subsumed}_sargable_join_predicates in AccessPath.
  mem_root_unordered_map<Item *, int> sargable_join_predicates;

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

  /// The set of tables that are on the inner side of some outer join or
  /// antijoin. If a table is not part of this set, and it is found to be empty,
  /// we can assume that the result of the top-level join will also be empty.
  table_map tables_inner_to_outer_or_anti = 0;

 private:
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
bool MakeJoinHypergraph(THD *thd, std::string *trace, JoinHypergraph *graph,
                        bool *where_is_always_false);

// Exposed for testing only.
void MakeJoinGraphFromRelationalExpression(THD *thd, RelationalExpression *expr,
                                           std::string *trace,
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
