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

#ifndef SQL_JOIN_OPTIMIZER_ONLINE_CYCLE_FINDER_H_
#define SQL_JOIN_OPTIMIZER_ONLINE_CYCLE_FINDER_H_

#include "map_helpers.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"

struct MEM_ROOT;

/**
  A fast online cycle finder, based on [Pea03]. It keeps a DAG in memory,
  built up incrementally, and is able to reject adding edges that would create
  cycles (or, equivalently, test if adding an edge would create a cycle).
  The amortized cost of checking ϴ(E) insertions is O(V).

  The basic working of the algorithm is to keep a list of all vertices,
  topologically sorted given the order so far. When inserting a new edge,
  we can quickly identify any vertices that would need to be moved in the
  topological sort (they are the ones stored between the two endpoints),
  run a DFS, and see if moving them would cause a contradiction (and thus,
  a cycle). See EdgeWouldCreateCycle() or the paper for more details.

  Note that confusingly enough, when used from the graph simplification
  algorithm, the vertices in this graph represent hyperedges (joins) in the join
  hypergraph, _not_ the vertices (tables) themselves. The edges in this graph
  are happens-before relations between those joins.

  [Pea03] Pearce et al: “Online Cycle Detection and Difference Propagation for
  Pointer Analysis”, section 3.2.
 */
class OnlineCycleFinder {
 public:
  OnlineCycleFinder(MEM_ROOT *mem_root, int num_vertices);

  // Returns true iff this would create a cycle.
  bool EdgeWouldCreateCycle(int a_idx, int b_idx);

  // Adds edge A -> B (A must be before B).
  // Returns true iff this would create a cycle.
  bool AddEdge(int a_idx, int b_idx);

  // Remove edge A -> B. The edge must have been added earlier with AddEdge
  // (or we will assert-fail).
  void DeleteEdge(int a_idx, int b_idx);

  // Returns a topological sort, respecting the added edges.
  // Note that the ordering is entirely arbitrary except for that,
  // and can be changed by e.g. EdgeWouldCreateCycle() calls.
  Bounds_checked_array<int> order() const { return m_order; }

 private:
  bool DepthFirstSearch(int node_idx, int upper_bound, int node_idx_to_avoid);
  void MoveAllMarked(int start_pos, int new_pos);
  void Allocate(int node_idx, int index_in_order) {
    m_order[index_in_order] = node_idx;
    m_position_of_node[node_idx] = index_in_order;
  }

  // List of nodes, in topological order. Called i2n in the paper.
  Bounds_checked_array<int> m_order;

  // For each node index, where in m_order is it?
  // Called n2i in the paper.
  Bounds_checked_array<int> m_position_of_node;

  // For each node, was it seen during this search or not?
  Bounds_checked_array<bool> m_visited;

  // Used as a temporary during MoveAllMarked().
  Mem_root_array<int> m_to_shift;

  // All edges that have been added, keyed by index of the from-node.
  mem_root_unordered_multimap<int, int> m_edges;
};

#endif  // SQL_JOIN_OPTIMIZER_ONLINE_CYCLE_FINDER_H_
