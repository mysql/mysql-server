/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <assert.h>
#include <stddef.h>

#include <limits>
#include <vector>

#include "my_compiler.h"
#include "priority_queue.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/online_cycle_finder.h"
#include "sql/mem_root_allocator.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"

class THD;
struct JoinHypergraph;

// Exposed for unit testing.
class GraphSimplifier {
 public:
  GraphSimplifier(THD *thd, JoinHypergraph *graph);

  // Do a single simplification step. The return enum is mostly for unit tests;
  // general code only needs to care about whether it returned
  // NO_SIMPLIFICATION_POSSIBLE or not.
  enum SimplificationResult {
    // No (more) simplifications are possible on this hypergraph.
    NO_SIMPLIFICATION_POSSIBLE = 0,

    // We applied a simplification of the graph (forcing one join ahead of
    // another).
    APPLIED_SIMPLIFICATION,

    // We applied a simplification, but it was one that was forced upon us;
    // we intended to apply the opposite, but discovered it would leave the
    // graph
    // in an impossible state. Thus, the graph has been changed, but the actual
    // available join orderings are exactly as they were.
    APPLIED_NOOP,

    // We applied a step that was earlier undone using UndoSimplificationStep().
    // (We do not know whether it was originally APPLIED_SIMPLIFICATION or
    // APPLIED_NOOP.)
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
  int num_steps_done() const {
    assert(m_done_steps.size() < size_t{std::numeric_limits<int>::max()});
    return static_cast<int>(m_done_steps.size());
  }

  // How many steps we've undone.
  int num_steps_undone() const {
    assert(m_undone_steps.size() < size_t{std::numeric_limits<int>::max()});
    return static_cast<int>(m_undone_steps.size());
  }

 private:
  // Update the given join's cache in the priority queue (or take it in
  // or out of the queue), presumably after best_step.benefit has changed
  // for that join.
  //
  // After this operation, m_pq should be in a consistent state.
  void UpdatePQ(size_t edge_idx);

  // Recalculate the benefit of all orderings involving the given edge,
  // i.e., the advantage of ordering any other neighboring join before
  // or after it. (These are stored in m_cache; see NeighborCache for
  // more information on the scheme.) You will typically need to call this
  // after having modified the given join (hyperedge endpoint). Note that
  // if a given ordering has become less advantageous, this may entail
  // recalculating other nodes recursively as well, but this should be rare
  // (again, see the comments on NeighborCache).
  //
  // “begin” and “end” are the range of other joins to compare against
  // (edge1_idx itself is always excluded). It should normally be set to
  // 0 and N (the number of edges) to compare against all, but during the
  // initial population in the constructor, where every pair is considered,
  // it is be used to avoid redundant computation.
  //
  // It would have been nice to somehow be able to use neighbor-of-neighbor
  // information to avoid rescanning all candidates for neighbors
  // (and the paper mentions “materializing all neighbors of a join”),
  // but given how hyperedges work, there doesn't seem to be a trivial way
  // of doing that (after A has absorbed B's into one of its hyperedges,
  // it seems it could gain new neighbors that were neither neighbors of
  // A nor B).
  void RecalculateNeighbors(size_t edge1_idx, size_t begin, size_t end);

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

  THD *m_thd;
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

  // Used for storing which neighbors are possible to simplify,
  // and how attractive they are. This speeds up repeated application of
  // DoSimplificationStep() significantly, as we don't have to recompute
  // the same information over and over again. This is keyed on the numerically
  // lowest join of the join pair, i.e., information about the benefit of
  // ordering join A before or after join B is stored on m_cache[min(A,B)].
  // These take part in a priority queue (see m_pq below), so that we always
  // know cheaply which one is the most attractive.
  //
  // There is a maybe surprising twist here; for any given cache node (join),
  // we only store the most beneficial ordering, and throw away all others.
  // This is because our benefit values keep changing all the time; once we've
  // chosen to put A before B, it means we've changed B, and that means every
  // single join pair involving B now needs to be recalculated anyway
  // (the costs, and thus ordering benefits, are highly dependent on the
  // hyperedge of B). Thus, storing only the best one (and by extension,
  // not having information about the other ones in the priority queue)
  // allows us to very quickly and easily throw away half of the invalidated
  // ones. We still need to check the other half (the ones that may be the best
  // for other nodes) to see if we need to invalidate them, but actual
  // invalidation is rare, as it only happens for the best simplification
  // involving that node (i.e., 1/N).
  //
  // It's unclear if this is the same scheme that the paper alludes to;
  // it mentions a priority queue and ordering by neighbor-involving joins,
  // but very little detail.
  struct NeighborCache {
    // The best simplification involving this join and a higher-indexed join,
    // and the index of that other node. (best_neighbor could be inferred
    // from the indexes in best_step and this index, but we keep it around
    // for simplicity.) best_neighbor == -1 indicates that there are no
    // possible reorderings involving this join and a higher-indexed one
    // (so it should not take part in the priority queue).
    int best_neighbor = -1;
    ProposedSimplificationStep best_step;

    // Where we are in the priority queue (heap index);
    // Priority_queue will update this for us (through MarkNeighborCache)
    // whenever we are insert into or moved around in the queue.
    // This is so that we can easily tell the PQ to recalculate our position
    // whenever best_step.benefit changes. -1 means that we are
    // currently not in the priority queue.
    int index_in_pq = -1;
  };
  Bounds_checked_array<NeighborCache> m_cache;

  // A priority queue of which simplifications are the most attractive,
  // containing pointers into m_cache. See the documentation on NeighborCache
  // for more information.
  struct CompareByBenefit {
    bool operator()(const NeighborCache *a, const NeighborCache *b) const {
      return a->best_step.benefit < b->best_step.benefit;
    }
  };
  struct MarkNeighborCache {
    void operator()(size_t index, NeighborCache **cache) {
      (*cache)->index_in_pq = index;
    }
  };
  Priority_queue<
      NeighborCache *,
      std::vector<NeighborCache *, Mem_root_allocator<NeighborCache *>>,
      CompareByBenefit, MarkNeighborCache>
      m_pq;
};

void SetNumberOfSimplifications(int num_simplifications,
                                GraphSimplifier *simplifier);

// See comment in .cc file.
void SimplifyQueryGraph(THD *thd, int subgraph_pair_limit,
                        JoinHypergraph *graph, GraphSimplifier *simplifier);

#endif  // SQL_JOIN_OPTIMIZER_GRAPH_SIMPLIFICATION_H_
