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

#ifndef _HYPERGRAPH_H
#define _HYPERGRAPH_H 1

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

#include <stdint.h>
#include <vector>

#include "my_compiler.h"

namespace hypergraph {

// Since our graphs can never have more than 61 tables, node sets and edge lists
// are implemented using 64-bit bit sets. This allows for a compact
// representation and very fast set manipulation; the algorithm does a fair
// amount of intersections and unions. If we should need extensions to larger
// graphs later (this will require additional heuristics for reducing the search
// space), we can use dynamic bit sets, although at a performance cost (we'd
// probably templatize off the NodeMap type).
using NodeMap = uint64_t;

struct Node {
  // List of edges (indexes into the hypergraph's “edges” array) that touch this
  // node. We split these into simple edges (only one node on each side) and
  // complex edges (all others), becaues we can often quickly discard all simple
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

  // Speeds up BM_HyperStar17_ManyHyperedges by 5–10%.
  unsigned char
      padding[64 - sizeof(std::vector<unsigned>) * 2 - sizeof(NodeMap)];
};
static_assert(sizeof(Node) == 64, "");

struct Hyperedge {
  // The endpoints (hypernodes) of this hyperedge. See the comment about
  // duplicated edges in Node.
  //
  // left and right may not overlap, and both must have at least one bit set.
  NodeMap left;
  NodeMap right;
};

struct Hypergraph {
  std::vector<Node> nodes;  // Maximum 8*sizeof(NodeMap) elements.
  std::vector<Hyperedge> edges;

  void AddNode();
  void AddEdge(NodeMap left, NodeMap right);
};

}  // namespace hypergraph

#endif  // !defined(_HYPERGRAPH_H)
