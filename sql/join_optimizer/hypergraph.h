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

#ifndef SQL_JOIN_OPTIMIZER_HYPERGRAPH_H_
#define SQL_JOIN_OPTIMIZER_HYPERGRAPH_H_ 1

/**
  @file
  Definition of an undirected (join) hypergraph. A hypergraph in this context
  is an undirected graph consisting of nodes and hyperedges, where hyperedges
  are edges that can have more than one node in each side of the edge.
  For instance, in a graph with nodes {A, B, C, D}, a regular undirected edge
  could be e.g. (A,B), while in a hypergraph, an edge such as ({A,C},B) would
  also be allowed. Note that this definition of hypergraphs differs from that
  on Wikipedia.

  The main user of Hypergraph is subgraph_enumeration.h.
 */

#include <stddef.h>
#include <algorithm>
#include <vector>

#include "sql/join_optimizer/node_map.h"
#include "sql/mem_root_array.h"

struct MEM_ROOT;

namespace hypergraph {

struct Node {
  // List of edges (indexes into the hypergraph's “edges” array) that touch this
  // node. We split these into simple edges (only one node on each side) and
  // complex edges (all others), because we can often quickly discard all simple
  // edges by testing the set of interesting nodes against the
  // “simple_neighborhood” bitmap.
  //
  // For optimization purposes, the edges are stored as if they were directed,
  // even though the hypergraph is fundamentally undirected. That is, a (u,v)
  // edge will be duplicated internally to (v,u), and the version that is posted
  // in a node's edge list is the one where the node itself is on the left side.
  // This saves a lot of duplicate code, and also reduces the amount of branch
  // mispredictions significantly (it helps something like 30% on the overall
  // speed).
  std::vector<unsigned> complex_edges, simple_edges;

  // All nodes on the “right” side of an edge in simple_edges.
  NodeMap simple_neighborhood = 0;

 private:
  // Speeds up BM_HyperStar17_ManyHyperedges by 5–10%.
  // (MSVC with debug STL will get a dummy byte here, since the struct is
  // already more than 64 bytes.)
  static constexpr int Size =
      sizeof(std::vector<unsigned>) * 2 + sizeof(NodeMap);
  char padding[std::max<int>(1, 64 - Size)];
};
static_assert(sizeof(Node) >= 64);

struct Hyperedge {
  // The endpoints (hypernodes) of this hyperedge. See the comment about
  // duplicated edges in Node.
  //
  // left and right may not overlap, and both must have at least one bit set.
  NodeMap left;
  NodeMap right;
};

struct Hypergraph {
 public:
  explicit Hypergraph(MEM_ROOT *mem_root) : nodes(mem_root), edges(mem_root) {}
  Mem_root_array<Node> nodes;  // Maximum 8*sizeof(NodeMap) elements.
  Mem_root_array<Hyperedge> edges;

  void AddNode();
  void AddEdge(NodeMap left, NodeMap right);

  // NOTE: Since every edge is stored twice (see AddEdge), also updates the
  // corresponding opposite-direction edge automatically. Also note that this
  // will shift internal edge lists around, so even after no-op changes,
  // you are not guaranteed to get back subgraph pairs in the same order
  // as before.
  void ModifyEdge(unsigned edge_idx, NodeMap new_left, NodeMap new_right);

 private:
  void AttachEdgeToNodes(size_t left_first_idx, size_t right_first_idx,
                         NodeMap left, NodeMap right);
};

}  // namespace hypergraph

#endif  // SQL_JOIN_OPTIMIZER_HYPERGRAPH_H_
