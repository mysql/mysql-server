/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/hypergraph.h"

#include <assert.h>

#include "sql/join_optimizer/bit_utils.h"

namespace hypergraph {

void Hypergraph::AddNode() { nodes.emplace_back(); }

void Hypergraph::AddEdge(NodeMap left, NodeMap right) {
  // Clone the edge, so that on any node, it is guaranteed to be on the
  // “left” side of the edge. This saves a lot of repetitive code and
  // branch misprediction; the microbenchmarks are up 10–30%.
  assert(left != 0);
  assert(right != 0);
  edges.emplace_back(Hyperedge{left, right});
  edges.emplace_back(Hyperedge{right, left});

  size_t left_first_idx = edges.size() - 2;
  size_t right_first_idx = edges.size() - 1;
  AttachEdgeToNodes(left_first_idx, right_first_idx, left, right);
}

// Roughly the same as std::erase_if, but assumes there's exactly one element
// matching, and doesn't care about the relative order after deletion.
template <class T>
static void RemoveElement(const T &element, std::vector<T> *vec) {
  auto it = find(vec->begin(), vec->end(), element);
  assert(it != vec->end());
  *it = vec->back();
  vec->pop_back();
}

void Hypergraph::ModifyEdge(unsigned edge_idx, NodeMap new_left,
                            NodeMap new_right) {
  NodeMap left = edges[edge_idx].left;
  NodeMap right = edges[edge_idx].right;

  const bool old_is_simple = IsSingleBitSet(left) && IsSingleBitSet(right);
  const bool new_is_simple =
      IsSingleBitSet(new_left) && IsSingleBitSet(new_right);

  if (!old_is_simple && !new_is_simple) {
    // An optimized fast-path for changing a complex edge into
    // another complex edge (this is nearly always an extension).
    // Compared to the remove-then-add path below, we don't touch
    // the unchanged nodes (of which there may be many).
    for (size_t left_node : BitsSetIn(left & ~new_left)) {
      RemoveElement(edge_idx, &nodes[left_node].complex_edges);
    }
    for (size_t right_node : BitsSetIn(right & ~new_right)) {
      RemoveElement(edge_idx ^ 1, &nodes[right_node].complex_edges);
    }
    for (size_t left_node : BitsSetIn(new_left & ~left)) {
      nodes[left_node].complex_edges.push_back(edge_idx);
    }
    for (size_t right_node : BitsSetIn(new_right & ~right)) {
      nodes[right_node].complex_edges.push_back(edge_idx ^ 1);
    }
    edges[edge_idx].left = new_left;
    edges[edge_idx].right = new_right;
    edges[edge_idx ^ 1].left = new_right;
    edges[edge_idx ^ 1].right = new_left;
    return;
  }

  // Take out the old edge. Pretty much exactly the opposite of
  // AttachEdgeToNodes().
  if (old_is_simple) {
    Node &left_node = nodes[*BitsSetIn(left).begin()];
    left_node.simple_neighborhood &= ~right;
    RemoveElement(edge_idx, &left_node.simple_edges);

    Node &right_node = nodes[*BitsSetIn(right).begin()];
    right_node.simple_neighborhood &= ~left;
    RemoveElement(edge_idx ^ 1, &right_node.simple_edges);
  } else {
    for (size_t left_node : BitsSetIn(left)) {
      RemoveElement(edge_idx, &nodes[left_node].complex_edges);
    }
    for (size_t right_node : BitsSetIn(right)) {
      RemoveElement(edge_idx ^ 1, &nodes[right_node].complex_edges);
    }
  }

  edges[edge_idx].left = new_left;
  edges[edge_idx].right = new_right;
  edges[edge_idx ^ 1].left = new_right;
  edges[edge_idx ^ 1].right = new_left;

  AttachEdgeToNodes(edge_idx, edge_idx ^ 1, new_left, new_right);
}

void Hypergraph::AttachEdgeToNodes(size_t left_first_idx,
                                   size_t right_first_idx, NodeMap left,
                                   NodeMap right) {
  if (IsSingleBitSet(left) && IsSingleBitSet(right)) {
    size_t left_node = *BitsSetIn(left).begin();
    size_t right_node = *BitsSetIn(right).begin();

    nodes[left_node].simple_neighborhood |= right;
    nodes[right_node].simple_neighborhood |= left;
    nodes[left_node].simple_edges.push_back(left_first_idx);
    nodes[right_node].simple_edges.push_back(right_first_idx);
  } else {
    for (size_t left_node : BitsSetIn(left)) {
      assert(left_node < nodes.size());
      nodes[left_node].complex_edges.push_back(left_first_idx);
    }
    for (size_t right_node : BitsSetIn(right)) {
      assert(right_node < nodes.size());
      nodes[right_node].complex_edges.push_back(right_first_idx);
    }
  }
}

}  // namespace hypergraph
