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

#ifndef SQL_JOIN_OPTIMIZER_TRIVIAL_RECEIVER_H_
#define SQL_JOIN_OPTIMIZER_TRIVIAL_RECEIVER_H_

#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/make_join_hypergraph.h"

/**
  A very simple receiver to be used with DPhyp; all it does is
  to keep track of which subgraphs it has seen (which is required
  for the algorithm to test connectedness), count them, and stop
  if we reach a given limit.

  This is usable both from unit tests (although we don't actually
  currently use it for such) and for making a cheap test of whether
  the number of subgraph pairs is below a given limit; see GraphSimplifier
  for the latter. (The graph simplification paper, [Neu09], mentions
  running a special mode where we don't check for subgraph complements
  at all, only connected subgraphs, but we haven't investigated
  to what degree this would be possible for our implementation,
  or whether it would be advantageous at all.)
 */
class TrivialReceiver {
 public:
  TrivialReceiver(const JoinHypergraph &graph, MEM_ROOT *mem_root,
                  int subgraph_pair_limit)
      : m_seen_subgraphs(mem_root),
        m_graph(&graph),
        m_subgraph_pair_limit(subgraph_pair_limit) {}

  bool HasSeen(hypergraph::NodeMap subgraph) const {
    return m_seen_subgraphs.count(subgraph) != 0;
  }
  bool FoundSingleNode(int node_idx) {
    ++seen_nodes;
    m_seen_subgraphs.insert(TableBitmap(node_idx));
    return false;
  }

  // Called EmitCsgCmp() in the paper.
  bool FoundSubgraphPair(hypergraph::NodeMap left, hypergraph::NodeMap right,
                         int edge_idx [[maybe_unused]]) {
    const JoinPredicate *edge = &m_graph->edges[edge_idx];
    if (!PassesConflictRules(left | right, edge->expr)) {
      return false;
    }
    ++seen_subgraph_pairs;
    if (m_subgraph_pair_limit >= 0 &&
        seen_subgraph_pairs > m_subgraph_pair_limit) {
      return true;
    }
    assert(left != 0);
    assert(right != 0);
    assert((left & right) == 0);
    m_seen_subgraphs.insert(left | right);
    return false;
  }

  int seen_nodes = 0;
  int seen_subgraph_pairs = 0;

 private:
  mem_root_unordered_set<hypergraph::NodeMap> m_seen_subgraphs;
  const JoinHypergraph *m_graph;
  const int m_subgraph_pair_limit;
};

#endif  // SQL_JOIN_OPTIMIZER_TRIVIAL_RECEIVER_H_
