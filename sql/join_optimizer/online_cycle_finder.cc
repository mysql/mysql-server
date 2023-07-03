/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/online_cycle_finder.h"

#include <assert.h>
#include <stddef.h>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <utility>

#include "sql/mem_root_array.h"
#include "sql/sql_array.h"

struct MEM_ROOT;

OnlineCycleFinder::OnlineCycleFinder(MEM_ROOT *mem_root, int num_vertices)
    : m_order(Bounds_checked_array<int>::Alloc(mem_root, num_vertices)),
      m_position_of_node(
          Bounds_checked_array<int>::Alloc(mem_root, num_vertices)),
      m_visited(Bounds_checked_array<bool>::Alloc(mem_root, num_vertices)),
      m_to_shift(mem_root),
      m_edges(mem_root) {
  std::iota(m_order.begin(), m_order.end(), 0);
  std::iota(m_position_of_node.begin(), m_position_of_node.end(), 0);
  std::fill(m_visited.begin(), m_visited.end(), false);
}

bool OnlineCycleFinder::EdgeWouldCreateCycle(int a_idx, int b_idx) {
  assert(a_idx >= 0);
  assert(static_cast<size_t>(a_idx) < m_order.size());
  assert(b_idx >= 0);
  assert(static_cast<size_t>(b_idx) < m_order.size());
  if (a_idx == b_idx) {
    return true;
  }
  int pos_of_a = m_position_of_node[a_idx];
  int pos_of_b = m_position_of_node[b_idx];
  if (pos_of_a < pos_of_b) {
    // Already in the topologically desired order,
    // so we don't need to do any checks.
  } else {
    // We have B first, then A. This is the opposite of what we want.
    // See if we are allowed to move B to the right, by doing a depth-first
    // search. The DFS has two purposes:
    //
    //  1. It finds everything that must come after B, transitively
    //     (and marks it as visited).
    //  2. It sees if A is reachable from B (if so, we have a cycle).
    //
    // As an optimization, we only need to care about the nodes between
    // B and A; all the nodes that are after A won't be affected by moving
    // B to A's immediate right.
    std::fill(m_visited.begin(), m_visited.end(), false);
    if (DepthFirstSearch(b_idx, pos_of_a + 1, /*node_idx_to_avoid=*/a_idx)) {
      // Found a cycle.
      return true;
    }

    // Everything seen during the DFS must be moved to the right,
    // together with B, since it still needs to stay after B.
    MoveAllMarked(pos_of_b, pos_of_a + 1);
  }
  return false;
}

bool OnlineCycleFinder::AddEdge(int a_idx, int b_idx) {
  if (EdgeWouldCreateCycle(a_idx, b_idx)) {
    return true;
  }
  m_edges.emplace(a_idx, b_idx);
  return false;
}

bool OnlineCycleFinder::DepthFirstSearch(int node_idx, int upper_bound,
                                         int node_idx_to_avoid) {
  if (node_idx == node_idx_to_avoid) {
    // This node can reach A, so it must be to the left of A.
    // But our search started from B, which means that the node
    // needs to be to the right of B, ie. B < N < A.
    // But we're trying to add A < B, so we have a cycle.
    return true;
  }

  if (m_visited[node_idx]) {
    // Already seen through some other path; e.g., if we have X-Y and X-Z-Y,
    // we can just ignore Y the second time.
    return false;
  }
  if (m_position_of_node[node_idx] >= upper_bound) {
    // This node comes after A, so we don't care;
    // moving A before B won't affect it negatively.
    // (And we know we also cannot reach A.)
    return false;
  }

  m_visited[node_idx] = true;
  auto first_and_last = m_edges.equal_range(node_idx);
  for (auto dest_node_it = first_and_last.first;
       dest_node_it != first_and_last.second; ++dest_node_it) {
    int dest_node_idx = dest_node_it->second;
    assert(m_position_of_node[dest_node_idx] > m_position_of_node[node_idx]);
    if (DepthFirstSearch(dest_node_idx, upper_bound, node_idx_to_avoid)) {
      // We found a cycle, so abort.
      return true;
    }
  }
  return false;
}

void OnlineCycleFinder::MoveAllMarked(int start_pos, int new_pos) {
  m_to_shift.clear();

  for (int i = start_pos; i < new_pos; ++i) {
    int node_idx = m_order[i];
    if (m_visited[node_idx]) {
      // Needs to move to the right (after upper_bound).
      m_to_shift.push_back(node_idx);
    } else {
      // Not involved, so just leave it where it is, relatively speaking.
      Allocate(node_idx, i - m_to_shift.size());
    }
  }

  for (size_t i = 0; i < m_to_shift.size(); ++i) {
    Allocate(m_to_shift[i], new_pos + i - m_to_shift.size());
  }
}

void OnlineCycleFinder::DeleteEdge(int a_idx, int b_idx) {
  auto [begin, end] = m_edges.equal_range(a_idx);
  for (auto it = begin; it != end; ++it) {
    if (it->second == b_idx) {
      m_edges.erase(it);
      return;
    }
  }
  assert(false);
}
