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

#include "sql/join_optimizer/hypergraph.h"
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
