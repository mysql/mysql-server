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

#include "sql/join_optimizer/graph_simplification.h"

#include <assert.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "sql/handler.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/node_map.h"
#include "sql/join_optimizer/online_cycle_finder.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/join_optimizer/trivial_receiver.h"
#include "sql/mem_root_allocator.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/table.h"

using hypergraph::Hyperedge;
using hypergraph::Hypergraph;
using hypergraph::NodeMap;
using std::fill;
using std::max;
using std::min;
using std::string;
using std::swap;
using std::vector;

namespace {

/**
  Returns whether A is already a part of B, ie., whether it is impossible to
  execute B before A. E.g., for t1 LEFT JOIN (t2 JOIN t3), the t2-t3 join
  will be part of the t1-{t2,t3} hyperedge, and this will return true.

  Note that this definition is much more lenient than the one in the paper
  (Figure 4), which appears to be wrong.
 */
bool IsSubjoin(Hyperedge a, Hyperedge b) {
  return IsSubset(a.left | a.right, b.left | b.right);
}

// Check if combining “left_component” with “right_component” would violate any
// conflict rules.
bool CombiningWouldViolateConflictRules(
    const Mem_root_array<ConflictRule> &conflict_rules, const int *in_component,
    int left_component, int right_component) {
  for (const ConflictRule &cr : conflict_rules) {
    bool applies = false;
    for (int node_idx : BitsSetIn(cr.needed_to_activate_rule)) {
      if (in_component[node_idx] == left_component ||
          in_component[node_idx] == right_component) {
        applies = true;
        break;
      }
    }
    if (applies) {
      for (int node_idx : BitsSetIn(cr.required_nodes)) {
        if (in_component[node_idx] != left_component &&
            in_component[node_idx] != right_component) {
          return true;
        }
      }
    }
  }
  return false;
}

// For a (nonempty) set of tables, find out which component they belong to.
// Will return -1 if they are not all in the same component (or if they
// all belong to component -1), otherwise the component they belong to.
//
// The FindLowestBitSet() seems to be a (small) choke point for the algorithm,
// at least on some CPUs. If we need more speedups, it could be an idea
// to pre-cache the value of that for all hyperedges (as we only ever
// expand hyperedges, and just need any arbitrary bit from them,
// we don't need to invalidate the information).
int GetComponent(const NodeMap *components, const int *in_component,
                 NodeMap tables) {
  assert(tables != 0);
  int component = in_component[FindLowestBitSet(tables)];
  if (component >= 0 &&
      IsSubset(tables, components[static_cast<unsigned>(component)])) {
    return component;
  } else {
    return -1;
  }
}

/**
  Helper algorithm for GetCardinality() and GraphIsJoinable();
  given a set of components (each typically connecting a single table
  at the start), connects them incrementally up through joins and calls
  a given callback every time we do it. The callback must be of type

    bool callback(int left_component, int right_component,
                  const JoinPredicate &pred, int num_changed);

  where num_changed is the number of tables that was in right_component
  but has now been combined with the ones in left_component and were
  moved there (we always move into the component with the lowest index).
  The algorithm ends when callback() returns true, or if no more joins
  are possible.

  In theory, it would be possible to accelerate this mechanism by means of
  the standard union-find algorithm (see e.g.
  https://en.wikipedia.org/wiki/Disjoint-set_data_structure), but since
  MAX_TABLES is so small, just using bitsets seems to work just as well.
  And instead of spending time on that, it would probably be better to
  find a complete join inference algorithm that would make GraphIsJoinable()
  obsolete and thus reduce the number of calls to this function.
 */
template <class Func>
void ConnectComponentsThroughJoins(const JoinHypergraph &graph,
                                   const OnlineCycleFinder &cycles,
                                   Func &&callback_on_join, NodeMap *components,
                                   int *in_component) {
  bool did_anything;
  do {
    did_anything = false;

    // Traverse joins from smaller to larger (as given by the topological
    // sorting that we already have), to increase the probability that we'll get
    // through the list of joins in a single pass.
    for (int edge_idx : cycles.order()) {
      const Hyperedge e = graph.graph.edges[edge_idx * 2];
      int left_component = GetComponent(components, in_component, e.left);
      if (left_component == -1) {
        // We cannot apply this (yet).
        continue;
      }
      if (Overlaps(e.right, components[left_component])) {
        // This join is already applied.
        continue;
      }
      int right_component = GetComponent(components, in_component, e.right);
      if (right_component == -1 ||
          CombiningWouldViolateConflictRules(
              graph.edges[edge_idx].expr->conflict_rules, in_component,
              left_component, right_component)) {
        // We cannot apply this (yet).
        continue;
      }

      // Combine the two components into the one that is numerically smaller.
      // This means that if everything goes into one component, it will be
      // component zero, which we can keep track of the cardinality of.
      if (right_component < left_component) {
        swap(left_component, right_component);
      }
      int num_changed = 0;
      for (int table_idx : BitsSetIn(components[right_component])) {
        in_component[table_idx] = left_component;
        ++num_changed;
      }
      assert(num_changed > 0);
      components[left_component] |= components[right_component];

      if (callback_on_join(left_component, right_component,
                           graph.edges[edge_idx], num_changed)) {
        return;
      }
      did_anything = true;
    }
  } while (did_anything);
}

/**
  For a given set of tables, try to estimate the cardinality of joining them
  together. (This essentially simulates the cardinality we'd get out of
  CostingReceiver, but without computing any costs or actual AccessPaths.)

  This is a fairly expensive operation since we need to iterate over all
  hyperedges several times, so we cache the cardinalities for each hyperedge
  in GraphSimplifier's constructor and then reuse them until the hyperedge
  is changed. We could probably go even further by having a cache based on
  tables_to_join, as many of the hyperedges will share endpoints, but it does
  not seem to be worth it (based on the microbenchmark profiles).
 */
double GetCardinality(NodeMap tables_to_join, const JoinHypergraph &graph,
                      const OnlineCycleFinder &cycles) {
  NodeMap components[MAX_TABLES];  // Which tables belong to each component.
  int in_component[MAX_TABLES];    // Which component each table belongs to.
  double component_cardinality[MAX_TABLES];
  fill(&in_component[0], &in_component[graph.nodes.size()], -1);

  // Start with each (relevant) table in a separate component.
  int num_components = 0;
  for (int node_idx : BitsSetIn(tables_to_join)) {
    components[num_components] = NodeMap{1} << node_idx;
    in_component[node_idx] = num_components;
    // Assume we have to read at least one row from each table, so that we don't
    // end up with zero costs in the rudimentary cost model used by the graph
    // simplification.
    component_cardinality[num_components] =
        max(ha_rows{1}, graph.nodes[node_idx].table->file->stats.records);
    ++num_components;
  }

  uint64_t active_components = BitsBetween(0, num_components);

  // Apply table filters, and also constant predicates.
  //
  // Note that we don't apply the range optimizer here to improve
  // the quality of the selectivities (even if we've already run it
  // on the previous graph). It's probably not that important for
  // our heuristics, but if it turns out to be critical, we could
  // arrange for all single tables to be run before simplification
  // (on the old graph), and then reuse that information.
  for (size_t i = 0; i < graph.num_where_predicates; ++i) {
    const Predicate &pred = graph.predicates[i];
    if (pred.total_eligibility_set == 0) {
      // Just put them on node 0 for simplicity;
      // we only care about the total selectivity,
      // so it doesn't matter when we apply them.
      component_cardinality[0] *= pred.selectivity;
    } else if (IsSubset(pred.total_eligibility_set, tables_to_join) &&
               IsSingleBitSet(pred.total_eligibility_set)) {
      int node_idx = FindLowestBitSet(pred.total_eligibility_set);
      component_cardinality[node_idx] *= pred.selectivity;
    }
  }

  if (num_components == 1) {
    return component_cardinality[0];
  }

  uint64_t multiple_equality_bitmap = 0;
  auto func = [&](int left_component, int right_component,
                  const JoinPredicate &pred, int num_changed [[maybe_unused]]) {
    double cardinality =
        FindOutputRowsForJoin(component_cardinality[left_component],
                              component_cardinality[right_component], &pred);

    // Mark off which multiple equalities we've seen.
    for (int pred_idx = pred.expr->join_predicate_first;
         pred_idx < pred.expr->join_predicate_last; ++pred_idx) {
      int source_multiple_equality_idx =
          graph.predicates[pred_idx].source_multiple_equality_idx;
      if (source_multiple_equality_idx != -1) {
        multiple_equality_bitmap |= uint64_t{1} << source_multiple_equality_idx;
      }
    }

    // Apply all newly applicable WHERE predicates.
    for (size_t i = 0; i < graph.num_where_predicates; ++i) {
      const Predicate &where_pred = graph.predicates[i];
      if (IsSubset(where_pred.total_eligibility_set, tables_to_join) &&
          Overlaps(where_pred.total_eligibility_set,
                   components[left_component]) &&
          Overlaps(where_pred.total_eligibility_set,
                   components[right_component]) &&
          (where_pred.source_multiple_equality_idx == -1 ||
           !IsBitSet(where_pred.source_multiple_equality_idx,
                     multiple_equality_bitmap))) {
        cardinality *= where_pred.selectivity;
        if (where_pred.source_multiple_equality_idx != -1) {
          multiple_equality_bitmap |=
              uint64_t{1} << where_pred.source_multiple_equality_idx;
        }
      }
    }

    // Write the new result into the newly combined component.
    component_cardinality[left_component] = cardinality;
    active_components &= ~(uint64_t{1} << right_component);
    return active_components == 0b1;
  };
  ConnectComponentsThroughJoins(graph, cycles, std::move(func), components,
                                in_component);

  // In rare situations, we could be left in a situation where an edge
  // doesn't contain a joinable set (ie., they are joinable, but only through
  // a hyperedge containing tables outside the given set). The paper
  // doesn't mention this at all, but as a hack, we simply combine them
  // as if they were an inner-equijoin (ie., selectivity 0.1). We could
  // also have chosen to take the maximum cardinality over all the components
  // or something similar, but this seems more neutral.
  for (int component_idx : BitsSetIn(active_components & ~1)) {
    component_cardinality[0] *= component_cardinality[component_idx] * 0.1;
  }
  return component_cardinality[0];
}

/**
  A special, much faster version of GetCardinality() that can be used
  when joining two partitions along a known edge. It reuses the existing
  cardinalities, and just applies the single edge and any missing WHERE
  predicates; this allows it to just make a single pass over those predicates
  and do no other work.
 */
double GetCardinalitySingleJoin(NodeMap left, NodeMap right, double left_rows,
                                double right_rows, const JoinHypergraph &graph,
                                const JoinPredicate &pred) {
  assert(!Overlaps(left, right));
  double cardinality = FindOutputRowsForJoin(left_rows, right_rows, &pred);

  // Mark off which multiple equalities we've seen.
  uint64_t multiple_equality_bitmap = 0;
  for (int pred_idx = pred.expr->join_predicate_first;
       pred_idx < pred.expr->join_predicate_last; ++pred_idx) {
    int source_multiple_equality_idx =
        graph.predicates[pred_idx].source_multiple_equality_idx;
    if (source_multiple_equality_idx != -1) {
      multiple_equality_bitmap |= uint64_t{1} << source_multiple_equality_idx;
    }
  }

  // Apply all newly applicable WHERE predicates.
  for (size_t i = 0; i < graph.num_where_predicates; ++i) {
    const Predicate &where_pred = graph.predicates[i];
    if (IsSubset(where_pred.total_eligibility_set, left | right) &&
        Overlaps(where_pred.total_eligibility_set, left) &&
        Overlaps(where_pred.total_eligibility_set, right) &&
        (where_pred.source_multiple_equality_idx == -1 ||
         !IsBitSet(where_pred.source_multiple_equality_idx,
                   multiple_equality_bitmap))) {
      cardinality *= where_pred.selectivity;
      if (where_pred.source_multiple_equality_idx != -1) {
        multiple_equality_bitmap |= uint64_t{1}
                                    << where_pred.source_multiple_equality_idx;
      }
    }
  }

  return cardinality;
}

/**
  Initialize a DAG containing all inferred join dependencies from the
  hypergraph. These are join dependencies that we cannot violate no matter
  what we do, so we need to make sure we do not try to force join reorderings
  that would be in conflict with them (whether directly or transitively) --
  and the returned OnlineCycleFinder allows us to check out exactly that,
  and also keep maintaining the DAG as we impose more orderings on the graph.

  This graph doesn't necessarily contain all dependencies inherent in the
  hypergraph, but it usually contains most of them. For instance, {t2,t3}-t4 is
  not a subjoin of t1-{t2,t4}, but must often be ordered before it anyway,
  since t2 and t4 are on opposite sides of the former join.
  See GraphSimplificationTest.IndirectHierarcicalJoins for a concrete test.

  Also, in the case of cyclic hypergraphs, the constraints in this DAG may be
  too strict, since it doesn't take into account that in cyclic hypergraphs we
  don't end up using all the edges (since the cycles are caused by redundant
  edges). So even if a constraint cannot be added because it would cause a cycle
  in the DAG, it doesn't mean that the hypergraph is unjoinable, because one of
  the edges involved in the cycle might be redundant and can be bypassed. See
  GraphSimplificationTest.CycleNeighboringHyperedges for a concrete test.

  We really ought to fix this, but it's not obvious how to implement it;
  it seems very difficult to create a test that catches all cases
  _and_ does not have any false positives in the presence of cycles
  (which often enable surprising orderings). Because it doesn't, we need
  additional and fairly expensive checks later on; see comments on
  GraphIsJoinable().
 */
OnlineCycleFinder FindJoinDependencies(const Hypergraph &graph,
                                       MEM_ROOT *mem_root) {
  const Mem_root_array<Hyperedge> &edges = graph.edges;
  OnlineCycleFinder cycles(mem_root, edges.size() / 2);
  for (size_t edge1_idx = 0; edge1_idx < edges.size() / 2; ++edge1_idx) {
    const Hyperedge edge1 = edges[edge1_idx * 2];
    for (size_t edge2_idx = 0; edge2_idx < edges.size() / 2; ++edge2_idx) {
      const Hyperedge edge2 = edges[edge2_idx * 2];
      if (edge1_idx != edge2_idx && IsSubjoin(edge1, edge2)) {
        bool added_cycle [[maybe_unused]] =
            cycles.AddEdge(edge1_idx, edge2_idx);
        assert(!added_cycle);
      }
    }
  }
  return cycles;
}

// Check if the given hypergraph has fewer than “subgraph_pair_limit”
// subgraph pairs, by simply running DPhyp over it.
bool IsQueryGraphSimpleEnough(THD *thd [[maybe_unused]],
                              const JoinHypergraph &graph,
                              int subgraph_pair_limit, MEM_ROOT *mem_root,
                              int *seen_subgraph_pairs) {
  bool error;
  {
    TrivialReceiver counting_receiver(graph, mem_root, subgraph_pair_limit);
    error = EnumerateAllConnectedPartitions(graph.graph, &counting_receiver);
    assert(!thd->is_error());
    if (!error) {
      *seen_subgraph_pairs = counting_receiver.seen_subgraph_pairs;
    }
  }
  mem_root->ClearForReuse();
  return !error;
}

void SetNumberOfSimplifications(int num_simplifications,
                                GraphSimplifier *simplifier) {
  while (simplifier->num_steps_done() < num_simplifications) {
    GraphSimplifier::SimplificationResult error [[maybe_unused]] =
        simplifier->DoSimplificationStep();
    assert(error != GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE);
  }
  while (simplifier->num_steps_done() > num_simplifications) {
    simplifier->UndoSimplificationStep();
  }
}

struct JoinStatus {
  double cost;
  double num_output_rows;
};

/**
  Simulate the (total) costs and cardinalities of joining two sets of tables,
  without actually having an AccessPath for each (which is a bit heavyweight
  for just cost and cardinality). Returns the same type, so that we can
  succinctly simulate joining this to yet more tables.

  The paper generally uses merge join as the cost function heuristic,
  but since we don't have merge join, and nested-loop joins are heavily
  dependent on context such as available indexes, we use instead our standard
  hash join estimation here. When we get merge joins, we should probably
  have a look to see whether switching to its cost function here makes sense.
  (Of course, we don't know what join type we will _actually_ be using until
  we're done with the entire planning!)

  NOTE: Keep this in sync with the cost estimation in ProposeHashJoin().
 */
JoinStatus SimulateJoin(JoinStatus left, JoinStatus right,
                        const JoinPredicate &pred) {
  // If the build cost per row is higher than the probe cost per row, it is
  // beneficial to use the smaller table as build table. Reorder to get the
  // lower cost if the join is commutative and allows reordering.
  static_assert(kHashBuildOneRowCost >= kHashProbeOneRowCost);
  if (OperatorIsCommutative(*pred.expr) &&
      left.num_output_rows < right.num_output_rows) {
    swap(left, right);
  }

  double num_output_rows =
      FindOutputRowsForJoin(left.num_output_rows, right.num_output_rows, &pred);
  double build_cost = right.num_output_rows * kHashBuildOneRowCost;
  double join_cost = build_cost + left.num_output_rows * kHashProbeOneRowCost +
                     num_output_rows * kHashReturnOneRowCost;

  return {left.cost + right.cost + join_cost, num_output_rows};
}

// Helper overloads to call SimulateJoin() for base cases,
// where we don't really care about the cost that went into them
// (they are assumed to be zero).
JoinStatus SimulateJoin(double left_rows, JoinStatus right,
                        const JoinPredicate &pred) {
  return SimulateJoin(JoinStatus{0.0, left_rows}, right, pred);
}

JoinStatus SimulateJoin(JoinStatus left, double right_rows,
                        const JoinPredicate &pred) {
  return SimulateJoin(left, JoinStatus{0.0, right_rows}, pred);
}

JoinStatus SimulateJoin(double left_rows, double right_rows,
                        const JoinPredicate &pred) {
  return SimulateJoin(JoinStatus{0.0, left_rows}, JoinStatus{0.0, right_rows},
                      pred);
}

/**
  See if a given hypergraph is impossible to join, in any way.

  This is a hack to work around the fact that our inference of implicit
  join ordering from the hypergraph is imperfect, so that we can end up
  creating an impossible situation (try to force join A before join B,
  but B must be done before A due to graph constraints). The paper mentions
  that joins must be inferred, but does not provide a complete procedure,
  and the authors were unaware that their assumed procedure did not cover
  all cases (Neumann, personal communication). Thus, we run this after
  each join simplification we apply, to see whether we created such a
  contradiction (if so, we know the opposite ordering is true).

  The algorithm is bare-bones: We put each node (table) into its own
  component, and then run through all join edges to see if we can connect
  those components into larger components. If we can apply enough edges
  (by repeated application of the entire list) that everything is connected
  into the same component, then there is at least one valid join order,
  and the graph is joinable. If not, it is impossible and we return true.
 */
bool GraphIsJoinable(const JoinHypergraph &graph,
                     const OnlineCycleFinder &cycles) {
  NodeMap components[MAX_TABLES];  // Which tables belong to each component.
  int in_component[MAX_TABLES];    // Which component each table belongs to.

  // Start with each table in a separate component.
  for (size_t node_idx = 0; node_idx < graph.nodes.size(); ++node_idx) {
    components[node_idx] = NodeMap{1} << node_idx;
    in_component[node_idx] = node_idx;
  }

  size_t num_in_component0 = 1;
  auto func = [&num_in_component0, &graph](
                  int left_component, int right_component [[maybe_unused]],
                  const JoinPredicate &pred [[maybe_unused]], int num_changed) {
    if (left_component == 0) {
      num_in_component0 += num_changed;
      return num_in_component0 == graph.nodes.size();
    }
    return false;
  };
  ConnectComponentsThroughJoins(graph, cycles, std::move(func), components,
                                in_component);
  return num_in_component0 == graph.nodes.size();
}

}  // namespace

GraphSimplifier::GraphSimplifier(JoinHypergraph *graph, MEM_ROOT *mem_root)
    : m_done_steps(mem_root),
      m_undone_steps(mem_root),
      m_edge_cardinalities(Bounds_checked_array<EdgeCardinalities>::Alloc(
          mem_root, graph->edges.size())),
      m_graph(graph),
      m_cycles(FindJoinDependencies(graph->graph, mem_root)),
      m_cache(Bounds_checked_array<NeighborCache>::Alloc(mem_root,
                                                         graph->edges.size())),
      m_pq(CompareByBenefit(),
           {Mem_root_allocator<NeighborCache *>{mem_root}}) {
  for (size_t edge_idx = 0; edge_idx < graph->edges.size(); ++edge_idx) {
    m_edge_cardinalities[edge_idx].left =
        GetCardinality(graph->graph.edges[edge_idx * 2].left, *graph, m_cycles);
    m_edge_cardinalities[edge_idx].right = GetCardinality(
        graph->graph.edges[edge_idx * 2].right, *graph, m_cycles);
    m_cache[edge_idx].best_step.benefit = -HUGE_VAL;
  }

  for (size_t edge_idx = 0; edge_idx < graph->edges.size(); ++edge_idx) {
    RecalculateNeighbors(edge_idx, edge_idx + 1, m_graph->edges.size());
  }
}

void GraphSimplifier::UpdatePQ(size_t edge_idx) {
  NeighborCache &cache = m_cache[edge_idx];
  assert(!std::isnan(cache.best_step.benefit));
  if (cache.index_in_pq == -1) {
    if (cache.best_neighbor != -1) {
      // Push into the queue for the first time.
      m_pq.push(&cache);
    }
  } else {
    if (cache.best_neighbor == -1) {
      // No neighbors remaining, so take it out of the queue.
      m_pq.remove(cache.index_in_pq);
      cache.index_in_pq = -1;
    } else {
      m_pq.update(cache.index_in_pq);
    }
  }
  assert(m_pq.is_valid());
}

void GraphSimplifier::RecalculateNeighbors(size_t edge1_idx, size_t begin,
                                           size_t end) {
  // Go through the neighbors of edge1_idx that are stored on other nodes
  // (because they are numerically lower).
  for (size_t edge2_idx = begin; edge2_idx < min(edge1_idx, end); ++edge2_idx) {
    NeighborCache &other_cache = m_cache[edge2_idx];
    ProposedSimplificationStep step;
    if (EdgesAreNeighboring(edge2_idx, edge1_idx, &step)) {
      if (other_cache.best_neighbor == -1 ||
          step.benefit >= other_cache.best_step.benefit) {
        // This is the new top for the other node. (This includes the case
        // where it was already the top, but has increased.)
        other_cache.best_neighbor = edge1_idx;
        other_cache.best_step = step;
        UpdatePQ(edge2_idx);
        continue;
      }
      // Fall through.
    }
    if (other_cache.best_neighbor == static_cast<int>(edge1_idx)) {
      // This pair was the best neighbor for the other side,
      // and has either decreased in benefit or is no longer
      // an (allowed) neighbor, so we need to re-check
      // if some other node is the best one now.
      //
      // Since edge2_idx < edge1_idx, the recursion is guaranteed
      // to terminate.
      RecalculateNeighbors(edge2_idx, 0, m_graph->edges.size());
    }
  }

  // Add the neighbors that are stored on this node. This is a much simpler
  // case, since we can just throw away everything and start afresh.
  NeighborCache &cache = m_cache[edge1_idx];
  cache.best_neighbor = -1;
  cache.best_step.benefit = -HUGE_VAL;
  for (size_t edge2_idx = max(begin, edge1_idx + 1); edge2_idx < end;
       ++edge2_idx) {
    ProposedSimplificationStep step;
    if (EdgesAreNeighboring(edge1_idx, edge2_idx, &step)) {
      // Stored on this node, so insert it.
      if (cache.best_neighbor == -1 || step.benefit > cache.best_step.benefit) {
        // This is the new top.
        cache.best_neighbor = edge2_idx;
        cache.best_step = step;
      }
    }
  }
  UpdatePQ(edge1_idx);
}

bool GraphSimplifier::EdgesAreNeighboring(
    size_t edge1_idx, size_t edge2_idx,
    GraphSimplifier::ProposedSimplificationStep *step) {
  const Hyperedge e1 = m_graph->graph.edges[edge1_idx * 2];
  const Hyperedge e2 = m_graph->graph.edges[edge2_idx * 2];
  if (IsSubjoin(e1, e2) || IsSubjoin(e2, e1)) {
    // One is a subjoin of each other, so ordering them is pointless.
    return false;
  }

  const JoinPredicate &j1 = m_graph->edges[edge1_idx];
  const JoinPredicate &j2 = m_graph->edges[edge2_idx];
  const double e1l = m_edge_cardinalities[edge1_idx].left;
  const double e1r = m_edge_cardinalities[edge1_idx].right;
  const double e2l = m_edge_cardinalities[edge2_idx].left;
  const double e2r = m_edge_cardinalities[edge2_idx].right;

  double cost_e1_before_e2;
  double cost_e2_before_e1;
  if (IsSubset(e1.left, e2.left) || IsSubset(e2.left, e1.left)) {
    // e2 is neighboring e1's left side, ie., this case:
    //
    //         e1
    //     L-------R
    //     |
    //  e2 |
    //     |
    //     R
    //
    // We want to find out whether applying e1 before e2 is likely
    // to be beneficial or not. To that extent, we'd like to compute
    //
    //   cost_e1_before_e2 = (e1l JOIN e1r) JOIN e2r
    //   cost_e2_before_e1 = (e2l JOIN e2r) JOIN e1r
    //
    // and then see which one is larger (and by how much it is larger).
    // We then calculate cost1/cost2 and cost2/cost1 to see if any of these
    // numbers are high (which indicates a favorable ordering to lock down
    // early).
    //
    // However, there's a problem in that e1l and e2l are not necessarily
    // identical; for instance, we could have a situation like this,
    // with joins {r0,r1}-r2 and r1-r3:
    //
    //                e1
    //   r0 ----- r1 --- r3
    //    \       /
    //     \     /
    //      \   /
    //       \ /
    //        |
    //     e2 |
    //        |
    //       r2
    //
    // Comparing these two costs would be unfair, as one includes
    // r0 and the other one does not:
    //
    //   cost_e1_before_e2 = (r1 JOIN r3) JOIN r2
    //   cost_e2_before_e1 = ({r0,r1} JOIN r2) JOIN r3
    //
    // So we follow the paper's lead and instead look at cost of
    // joining against an imaginary table with the maximum
    // cardinality of the two left sides, ie. we do
    //
    //   cost_e1_before_e2 = (MAX(|e1l|,|e2l|) JOIN e1r) JOIN e2r
    //   cost_e2_before_e1 = (MAX(|e1l|,|e2l|) JOIN e2r) JOIN e1r
    //
    // We could have tested both against |r0 JOIN r1| (ie., the union
    // of the two sets, which would have the same effect in this specific
    // case), but it would be worse for cacheability, and we haven't made
    // any detailed measurements of whether it actually is better (or worse)
    // for overall quality of the simplifications.
    double common = max(e1l, e2l);
    cost_e1_before_e2 =
        SimulateJoin(SimulateJoin(common, e1r, j1), e2r, j2).cost;
    cost_e2_before_e1 =
        SimulateJoin(SimulateJoin(common, e2r, j2), e1r, j1).cost;
  } else if (IsSubset(e1.left, e2.right) || IsSubset(e2.right, e1.left)) {
    // Analogous to the case above, but e1's left meets e2's right.
    double common = max(e1l, e2r);
    cost_e1_before_e2 =
        SimulateJoin(e2l, SimulateJoin(common, e1r, j1), j2).cost;
    cost_e2_before_e1 =
        SimulateJoin(SimulateJoin(e2l, common, j2), e1r, j1).cost;
  } else if (IsSubset(e1.right, e2.right) || IsSubset(e2.right, e1.right)) {
    // Meets in their right endpoints.
    double common = max(e1r, e2r);
    cost_e1_before_e2 =
        SimulateJoin(e2l, SimulateJoin(e1l, common, j1), j2).cost;
    cost_e2_before_e1 =
        SimulateJoin(e1l, SimulateJoin(e2l, common, j2), j1).cost;
  } else if (IsSubset(e1.right, e2.left) || IsSubset(e2.left, e1.right)) {
    // e1's right meets e2's left.
    double common = max(e1r, e2l);
    cost_e1_before_e2 =
        SimulateJoin(SimulateJoin(e1l, common, j1), e2r, j2).cost;
    cost_e2_before_e1 =
        SimulateJoin(e1l, SimulateJoin(common, e2r, j2), j1).cost;
  } else {
    // Not neighboring.
    return false;
  }

  // Assume the costs are finite and positive. Otherwise, the ratios calculated
  // below might not make sense and return NaN.
  assert(std::isfinite(cost_e1_before_e2));
  assert(std::isfinite(cost_e2_before_e1));
  assert(cost_e1_before_e2 > 0);
  assert(cost_e2_before_e1 > 0);

  if (cost_e1_before_e2 > cost_e2_before_e1) {
    *step = {cost_e1_before_e2 / cost_e2_before_e1, static_cast<int>(edge2_idx),
             static_cast<int>(edge1_idx)};
  } else {
    *step = {cost_e2_before_e1 / cost_e1_before_e2, static_cast<int>(edge1_idx),
             static_cast<int>(edge2_idx)};
  }
  return true;
}

GraphSimplifier::SimplificationStep
GraphSimplifier::ConcretizeSimplificationStep(
    GraphSimplifier::ProposedSimplificationStep step) {
  const Hyperedge e1 = m_graph->graph.edges[step.before_edge_idx * 2];
  const Hyperedge e2 = m_graph->graph.edges[step.after_edge_idx * 2];

  // Find out whether they meet in e2's left or e2's right.
  SimplificationStep full_step;
  full_step.before_edge_idx = step.before_edge_idx;
  full_step.after_edge_idx = step.after_edge_idx;
  full_step.old_edge = e2;
  full_step.new_edge = e2;
  if (IsSubset(e1.left, e2.left) || IsSubset(e2.left, e1.left) ||
      IsSubset(e1.right, e2.left) || IsSubset(e2.left, e1.right)) {
    if (!Overlaps(e2.right, e1.left | e1.right)) {
      m_edge_cardinalities[step.after_edge_idx].left = GetCardinalitySingleJoin(
          e1.left, e1.right, m_edge_cardinalities[step.before_edge_idx].left,
          m_edge_cardinalities[step.before_edge_idx].right, *m_graph,
          m_graph->edges[step.before_edge_idx]);
      full_step.new_edge.left |= e1.left | e1.right;
    } else {
      // We ended up in a situation where the two edges were not
      // clearly separated, so recalculate the cardinality from scratch
      // to be sure. This is slow, but happens fairly rarely.
      NodeMap nodes_to_add = (e1.left | e1.right) & ~e2.right;
      full_step.new_edge.left |= nodes_to_add;
      m_edge_cardinalities[step.after_edge_idx].left =
          GetCardinality(full_step.new_edge.left, *m_graph, m_cycles);
    }
  } else {
    assert(IsSubset(e1.left, e2.right) || IsSubset(e2.right, e1.left) ||
           IsSubset(e1.right, e2.right) || IsSubset(e2.right, e1.right));
    if (!Overlaps(e2.left, e1.left | e1.right)) {
      m_edge_cardinalities[step.after_edge_idx].right =
          GetCardinalitySingleJoin(
              e1.left, e1.right,
              m_edge_cardinalities[step.before_edge_idx].left,
              m_edge_cardinalities[step.before_edge_idx].right, *m_graph,
              m_graph->edges[step.before_edge_idx]);
      full_step.new_edge.right |= e1.left | e1.right;
    } else {
      // We ended up in a situation where the two edges were not
      // clearly separated, so recalculate the cardinality from scratch
      // to be sure. This is slow, but happens fairly rarely.
      NodeMap nodes_to_add = (e1.left | e1.right) & ~e2.left;
      full_step.new_edge.right |= nodes_to_add;
      m_edge_cardinalities[step.after_edge_idx].right =
          GetCardinality(full_step.new_edge.right, *m_graph, m_cycles);
    }
  }
  assert(!Overlaps(full_step.new_edge.left, full_step.new_edge.right));
  assert(!Overlaps(full_step.new_edge.left, full_step.new_edge.right));

  return full_step;
}

GraphSimplifier::SimplificationResult GraphSimplifier::DoSimplificationStep() {
  // See if we have a cached (previously undone) step that we could apply.
  if (!m_undone_steps.empty()) {
    SimplificationStep step = m_undone_steps.back();
    m_undone_steps.pop_back();
    m_graph->graph.ModifyEdge(step.after_edge_idx * 2, step.new_edge.left,
                              step.new_edge.right);
    m_done_steps.push_back(step);
    return APPLIED_REDO_STEP;
  }

  if (m_pq.empty()) {
    // No (further) simplifications were possible.
    return NO_SIMPLIFICATION_POSSIBLE;
  }
  NeighborCache *cache = m_pq.top();
  ProposedSimplificationStep best_step = cache->best_step;
  bool forced = false;
  if (m_cycles.EdgeWouldCreateCycle(best_step.before_edge_idx,
                                    best_step.after_edge_idx)) {
    // We cannot allow this ordering, so apply the opposite ordering
    // to the graph. This has zero benefit in itself (it just makes
    // explicit what is already true), but it means we will never
    // try to do this step anymore.
    swap(best_step.before_edge_idx, best_step.after_edge_idx);
    forced = true;
  }

  // Make so that e1 is ordered before e2 (i.e., e2 requires e1).
  EdgeCardinalities old_cardinalities =
      m_edge_cardinalities[best_step.after_edge_idx];

  SimplificationStep full_step = ConcretizeSimplificationStep(best_step);

  bool added_cycle [[maybe_unused]] =
      m_cycles.AddEdge(best_step.before_edge_idx, best_step.after_edge_idx);
  assert(!added_cycle);
  m_graph->graph.ModifyEdge(best_step.after_edge_idx * 2,
                            full_step.new_edge.left, full_step.new_edge.right);

  if (!GraphIsJoinable(*m_graph, m_cycles)) {
    // The change we did introduced an impossibility; we made the graph
    // unjoinable. This happens very rarely, but it does, since our
    // happens-before join detection is incomplete (see GraphIsJoinable()
    // and FindJoinDependencies() comments for more details). When this
    // happens, we need to first undo what we just did:
    m_cycles.DeleteEdge(best_step.before_edge_idx, best_step.after_edge_idx);
    m_graph->graph.ModifyEdge(best_step.after_edge_idx * 2,
                              full_step.old_edge.left,
                              full_step.old_edge.right);
    m_edge_cardinalities[best_step.after_edge_idx] = old_cardinalities;

    // Then, we insert the opposite constraint of what we just tried
    // (because we just inferred that it's implicitly in our current graph)
    // and then try again to find a simplification.
    // (We don't modify the graph, but the next iteration will.)
    if (m_cycles.AddEdge(full_step.after_edge_idx, full_step.before_edge_idx)) {
      // Adding the opposite constraint would cause a cycle. This means
      // GraphIsJoinable() says join A cannot be before join B, whereas
      // AddEdge() says join B cannot be before join A. One of them must be
      // wrong. It is likely AddEdge() that gives the wrong answer due to a
      // cycle in the hypergraph. Since we cannot add the opposite constraint in
      // order to prevent that this simplification is applied, we instead remove
      // it from the set of potential simplification before we try again.
      m_pq.pop();
      cache->index_in_pq = -1;
    }
    return DoSimplificationStep();
  }
  RecalculateNeighbors(best_step.after_edge_idx, 0, m_graph->edges.size());
  m_done_steps.push_back(full_step);
  return forced ? APPLIED_NOOP : APPLIED_SIMPLIFICATION;
}

void GraphSimplifier::UndoSimplificationStep() {
  assert(!m_done_steps.empty());

  SimplificationStep step = m_done_steps.back();
  m_done_steps.pop_back();
  m_graph->graph.ModifyEdge(step.after_edge_idx * 2, step.old_edge.left,
                            step.old_edge.right);
  m_undone_steps.push_back(step);

  // NOTE: As mentioned in the class comments, we don't touch m_cycles
  // or any of the cardinalities here.
}

/**
  Repeatedly apply simplifications (in the order of most to least safe) to the
  given hypergraph, until it is below “subgraph_pair_limit” subgraph pairs
  or we can simplify it no more. Since we cannot know ahead of time exactly
  how many simplification steps required, we need to do this iteratively,
  running DPhyp (with all the actual and expensive costing removed, only
  subgraph pair counting) as we go.

  On the assumption that running DPhyp over the graph is significantly more
  expensive than applying a simplification step, we do this by means of binary
  search (what the paper calls “the full algorithm”). We apply first 1, 2, 4,
  8, 16, etc. steps until we find a number that takes us below the limit.
  Then, we apply a simple binary search between that value and the previous one.
  Once we find the border between too complicated and just simple enough,
  we set the graph to the latter, and the actual query planning will start
  afresh.
 */
void SimplifyQueryGraph(THD *thd, int subgraph_pair_limit,
                        JoinHypergraph *graph, string *trace) {
  if (trace != nullptr) {
    *trace +=
        "\nQuery became too complicated, doing heuristic graph "
        "simplification.\n";
  }

  GraphSimplifier simplifier(graph, thd->mem_root);
  MEM_ROOT counting_mem_root;

  int lower_bound = 0, upper_bound = 1;
  int num_subgraph_pairs_upper = -1;
  for (;;) {  // Termination condition within loop.
    bool hit_upper_limit = false;
    while (simplifier.num_steps_done() < upper_bound) {
      if (simplifier.DoSimplificationStep() ==
          GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE) {
        if (!IsQueryGraphSimpleEnough(thd, *graph, subgraph_pair_limit,
                                      &counting_mem_root,
                                      &num_subgraph_pairs_upper)) {
          // If this happens, the user has set the limit way too low. The query
          // will run with all the simplifications we have found, but the number
          // of subgraph pairs is still above the limit.
          if (trace != nullptr) {
            *trace +=
                "Cannot do any more simplification steps, just running "
                "the query as-is.\n";
          }
          return;
        }

        upper_bound = simplifier.num_steps_done();
        hit_upper_limit = true;
        break;
      }
    }
    if (hit_upper_limit) {
      break;
    }

    // See if our upper bound was enough.
    if (IsQueryGraphSimpleEnough(thd, *graph, subgraph_pair_limit,
                                 &counting_mem_root,
                                 &num_subgraph_pairs_upper)) {
      // It was enough, so run binary search between the upper
      // and lower bounds below. Note that at this point,
      // the rest of the GraphSimplifier operations are cached
      // and thus essentially free.
      break;
    }

    // It wasn't enough, so double the steps and try again.
    lower_bound = upper_bound;
    upper_bound *= 2;
    assert(upper_bound <= 1000000);  // Should never get this high.
  }

  assert(!thd->is_error());

  // Now binary search between the lower and upper bounds to find the least
  // number of simplifications we need to get below the wanted limit.
  // At this point, lower_bound is the highest number that we know for sure
  // isn't enough, and upper_bound is the lowest number that we know for sure
  // is enough.
  while (upper_bound - lower_bound > 1) {
    int mid = (lower_bound + upper_bound) / 2;
    SetNumberOfSimplifications(mid, &simplifier);
    if (IsQueryGraphSimpleEnough(thd, *graph, subgraph_pair_limit,
                                 &counting_mem_root,
                                 &num_subgraph_pairs_upper)) {
      upper_bound = mid;
    } else {
      lower_bound = mid;
    }
  }

  // Now upper_bound is the correct number of steps to use.
  SetNumberOfSimplifications(upper_bound, &simplifier);

  if (trace != nullptr) {
    *trace += StringPrintf(
        "After %d simplification steps, the query graph contains %d "
        "subgraph pairs, which is below the limit.\n",
        upper_bound, num_subgraph_pairs_upper);
  }
}
