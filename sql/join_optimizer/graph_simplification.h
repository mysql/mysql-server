/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_GRAPH_SIMPLIFICATION_H_
#define SQL_JOIN_OPTIMIZER_GRAPH_SIMPLIFICATION_H_

/**
  @file

  Heuristic simplification of query graphs to make them execute faster,
  largely a direct implementation of [Neu09] (any references to just
  “the paper” will generally be to that). This is needed for when
  query hypergraphs have too many possible (connected) subgraphs to
  evaluate all of them, and we need to resort to heuristics.

  The algorithm works by evaluating pairs of neighboring joins
  (largely, those that touch some of the same tables), finding obviously _bad_
  pairwise orderings and then disallowing them. I.e., if join A must
  very likely happen before join B (as measured by cost heuristics),
  we disallow the B-before-A join by extending the hyperedge of
  B to include A's nodes. This makes the graph more visually complicated
  (thus making “simplification” a bit of a misnomer), but reduces the search
  space, so that the query generally is faster to plan.

  Obviously, as the algorithm is greedy, it will sometimes make mistakes
  and make for a more expensive (or at least higher-cost) query.
  This isn't necessarily an optimal or even particularly good algorithm;
  e.g. LinDP++ [Rad19] claims significantly better results, especially
  on joins that are 40 tables or more. However, using graph simplification
  allows us to handle large queries reasonably well, while still reusing nearly
  all of our query planning machinery (i.e., we don't have to implement a
  separate query planner and cost model for large queries).

  Also note that graph simplification only addresses the problem of subgraph
  pair explosion. If each subgraph pair generates large amounts of candidate
  access paths (e.g. through parameterized paths), each subgraph pair will in
  itself be expensive, and graph simplification does not concern itself with
  this at all. Thus, to get a complete solution, we must _also_ have heuristic
  pruning of access paths within a subgraph, which we're currently missing.


  [Neu09] Neumann: “Query Simplification: Graceful Degradation for Join-Order
    Optimization”.
  [Rad19] Radke and Neumann: “LinDP++: Generalizing Linearized DP to
    Crossproducts and Non-Inner Joins”.
 */

#include <stddef.h>

#include <string>

#include "my_compiler.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/online_cycle_finder.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"

class THD;
struct JoinHypergraph;
struct MEM_ROOT;

// Exposed for unit testing.
class GraphSimplifier {
 public:
  GraphSimplifier(JoinHypergraph *graph, MEM_ROOT *mem_root);

  // Do a single simplification step. The return enum is mostly for unit tests;
  // general code only needs to care about whether it returned
  // NO_SIMPLIFICATION_POSSIBLE or not.
  enum SimplificationResult {
    // No (more) simplifications are possible on this hypergraph.
    NO_SIMPLIFICATION_POSSIBLE = 0,

    // We applied a simplification of the graph (forcing one join ahead of
    // another).
    APPLIED_SIMPLIFICATION,

    // We applied a step that was earlier undone using UndoSimplificationStep().
    APPLIED_REDO_STEP
  };
  SimplificationResult DoSimplificationStep();

  // Undo the last applied simplification step (by DoSimplificationStep()).
  // Note that this does not reset the internal state, i.e., it only puts
  // the graph back into the state before the last DoSimplificationStep()
  // call. This means that the internal happens-before graph and cardinalities
  // remain as if the step was still done. This is because if calling
  // DoSimplificationStep() after an UndoSimplificationStep() call,
  // no new work is done; the change is simply replayed again, with no
  // new computation done. We only need to search for more simplifications
  // once we've replayed all undone steps. This also means that we make
  // the assumption that nobody else is changing the graph during the
  // lifetime of GraphSimplifier.
  //
  // You can call UndoSimplificationStep() several times, as long as there
  // is at least one simplification step to undo; undo/redo works essentially
  // as a stack.
  void UndoSimplificationStep();

  // How many steps we've (successfully) done and not undone.
  int num_steps_done() const { return m_done_steps.size(); }

 private:
  struct ProposedSimplificationStep {
    double benefit;
    int before_edge_idx;
    int after_edge_idx;
  };

  // Returns whether two joins are neighboring (share edges),
  // and if so, estimates the benefit of joining one before the other
  // (including which one should be first) and writes into “step”.
  ALWAYS_INLINE bool EdgesAreNeighboring(size_t edge1_idx, size_t edge2_idx,
                                         ProposedSimplificationStep *step);

  struct SimplificationStep {
    int before_edge_idx;
    int after_edge_idx;

    // Old and new versions of after_edge_idx.
    hypergraph::Hyperedge old_edge;
    hypergraph::Hyperedge new_edge;
  };

  // Convert a simplification step (join A before join B) to an actual
  // idea of how to modify the given edge (new values for join B's
  // hyperedge endpoints).
  SimplificationStep ConcretizeSimplificationStep(
      GraphSimplifier::ProposedSimplificationStep step);

  // Steps that we have applied so far, in chronological order.
  // Used so that we can undo them easily on UndoSimplificationStep().
  Mem_root_array<SimplificationStep> m_done_steps;

  // Steps that we used to have applied, but have undone, in chronological
  // order of the undo (ie., latest undone step last).
  // DoSimplificationStep() will use these to quickly reapply an undone
  // step if needed (and then move it to the end of done_steps again).
  Mem_root_array<SimplificationStep> m_undone_steps;

  // Cache the cardinalities of (a join of) the nodes on each side of each
  // hyperedge, corresponding 1:1 index-wise to m_graph->edges. So if
  // e.g. m_graph->graph.edges[0].left contains {t1,t2,t4}, then
  // m_edge_cardinalities[0].left will contain the cardinality of joining
  // t1, t2 and t4 together.
  //
  // This cache is so that we don't need to make repeated calls to
  // GetCardinality(), which is fairly expensive. It is updated when we
  // apply simplification steps (which change the hyperedges).
  struct EdgeCardinalities {
    double left;
    double right;
  };
  Bounds_checked_array<EdgeCardinalities> m_edge_cardinalities;

  // The graph we are simplifying.
  JoinHypergraph *m_graph;

  // Stores must-happen-before relationships between the joins (edges),
  // so that we don't end up with impossibilities. See OnlineCycleFinder
  // for more information.
  OnlineCycleFinder m_cycles;
};

// See comment in .cc file.
void SimplifyQueryGraph(THD *thd, int subgraph_pair_limit,
                        JoinHypergraph *graph, std::string *trace);

#endif  // SQL_JOIN_OPTIMIZER_GRAPH_SIMPLIFICATION_H_
