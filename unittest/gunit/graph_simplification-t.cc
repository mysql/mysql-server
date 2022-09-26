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

#include <gtest/gtest.h>
#include <stdio.h>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "scope_guard.h"
#include "sql/handler.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/graph_simplification.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/node_map.h"
#include "sql/join_optimizer/online_cycle_finder.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/join_optimizer/trivial_receiver.h"
#include "sql/mem_root_array.h"
#include "sql/table.h"
#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/test_utils.h"

class Item;
class THD;

using hypergraph::NodeMap;
using testing::UnorderedElementsAre;

TEST(OnlineCycleFinderTest, SelfEdges) {
  MEM_ROOT mem_root;
  OnlineCycleFinder cycles(&mem_root, 10);
  EXPECT_TRUE(cycles.AddEdge(5, 5));
  EXPECT_TRUE(cycles.EdgeWouldCreateCycle(5, 5));
}

TEST(OnlineCycleFinderTest, Simple) {
  MEM_ROOT mem_root;
  OnlineCycleFinder cycles(&mem_root, 10);
  EXPECT_FALSE(cycles.EdgeWouldCreateCycle(3, 5));
  EXPECT_FALSE(cycles.EdgeWouldCreateCycle(5, 3));
  EXPECT_FALSE(cycles.AddEdge(3, 5));
  EXPECT_TRUE(cycles.EdgeWouldCreateCycle(5, 3));
}

TEST(OnlineCycleFinderTest, InverseOrderIsFine) {
  MEM_ROOT mem_root;
  OnlineCycleFinder cycles(&mem_root, 10);
  EXPECT_FALSE(cycles.AddEdge(5, 3));
  EXPECT_TRUE(cycles.EdgeWouldCreateCycle(3, 5));
}

TEST(OnlineCycleFinderTest, Transitive) {
  MEM_ROOT mem_root;
  OnlineCycleFinder cycles(&mem_root, 10);
  EXPECT_FALSE(cycles.AddEdge(1, 3));
  EXPECT_FALSE(cycles.AddEdge(3, 5));
  EXPECT_FALSE(cycles.AddEdge(5, 6));
  EXPECT_FALSE(cycles.AddEdge(5, 9));
  EXPECT_FALSE(cycles.EdgeWouldCreateCycle(7, 1));
  EXPECT_TRUE(cycles.EdgeWouldCreateCycle(6, 1));
  EXPECT_TRUE(cycles.EdgeWouldCreateCycle(9, 1));
  EXPECT_FALSE(cycles.EdgeWouldCreateCycle(1, 7));
  EXPECT_FALSE(cycles.EdgeWouldCreateCycle(1, 5));
}

static void AddEdge(THD *thd, RelationalExpression::Type join_type,
                    NodeMap left, NodeMap right, double selectivity,
                    MEM_ROOT *mem_root, JoinHypergraph *graph) {
  JoinPredicate pred;
  pred.selectivity = selectivity;
  pred.expr = new (mem_root) RelationalExpression(thd);
  pred.expr->type = join_type;
  pred.expr->nodes_in_subtree = left | right;
  pred.estimated_bytes_per_row = 0;  // To keep the compiler happy.
  graph->edges.push_back(std::move(pred));
  graph->graph.AddEdge(left, right);
}

namespace {

// Helper for destroying all the Fake_TABLE objects in a JoinHypergraph.
class DestroyNodes {
 public:
  explicit DestroyNodes(const JoinHypergraph *graph) : m_graph(graph) {}
  void operator()() const {
    for (const JoinHypergraph::Node &node : m_graph->nodes) {
      destroy(static_cast<Fake_TABLE *>(node.table));
    }
  }

 private:
  const JoinHypergraph *m_graph;
};

// RAII class which destroys Fake_TABLE objects when it goes out of scope.
using NodeGuard = Scope_guard<DestroyNodes>;

}  // namespace

[[nodiscard]] static NodeGuard AddNodes(int num_nodes, MEM_ROOT *mem_root,
                                        JoinHypergraph *g) {
  for (int i = 0; i < num_nodes; ++i) {
    TABLE *table =
        new (mem_root) Fake_TABLE(/*num_columns=*/1, /*nullable=*/true);
    table->file->stats.records = 1000;

    char *alias = mem_root->ArrayAlloc<char>(20);
    snprintf(alias, 20, "t%d", i + 1);
    table->alias = alias;

    g->nodes.push_back(JoinHypergraph::Node{table, {}, {}});
    g->graph.AddNode();
  }

  return {DestroyNodes(g)};
}

TEST(GraphSimplificationTest, SimpleStar) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // A simple star-join with four tables, similar to what's in the paper.
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(4, &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b10, 0.999,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b100, 0.5,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b1000,
          0.01, &mem_root, &g);

  GraphSimplifier s(&g, &mem_root);

  // Based on the selectivities, joining t1/t4 before t1/t2 will be the best
  // choice. This means we'll broaden the t1/t2 edge to {t1,t4}/t2.
  // (We could have put t4 on any side.)
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(0b1001, g.graph.edges[0].left);
  EXPECT_EQ(0b10, g.graph.edges[0].right);
  EXPECT_EQ(g.graph.edges[0].left, g.graph.edges[1].right);
  EXPECT_EQ(g.graph.edges[0].right, g.graph.edges[1].left);

  // The t1/t2 edge is no longer simple; verify we updated the graph right.
  EXPECT_EQ(0b1100, g.graph.nodes[0].simple_neighborhood);
  EXPECT_EQ(0b0000, g.graph.nodes[1].simple_neighborhood);
  EXPECT_THAT(g.graph.nodes[0].simple_edges, UnorderedElementsAre(2, 4));
  EXPECT_THAT(g.graph.nodes[0].complex_edges, UnorderedElementsAre(0));
  EXPECT_THAT(g.graph.nodes[1].simple_edges, UnorderedElementsAre());
  EXPECT_THAT(g.graph.nodes[1].complex_edges, UnorderedElementsAre(1));

  // Next, we'll do t1/t4 before t1/t3 (again based on selectivities),
  // broadening t1/t3 to {t1,t4}/t3.
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(0b1001, g.graph.edges[2].left);
  EXPECT_EQ(0b100, g.graph.edges[2].right);
  EXPECT_EQ(g.graph.edges[2].left, g.graph.edges[3].right);
  EXPECT_EQ(g.graph.edges[2].right, g.graph.edges[3].left);

  // Finally, t1-t3 before t1-t2, but these edges were already hyperedges.
  // So {t1,t4}-{t2} will be extended to {t1,t3,t4}-{t2}.
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(0b1101, g.graph.edges[0].left);
  EXPECT_EQ(0b10, g.graph.edges[0].right);
  EXPECT_EQ(g.graph.edges[0].left, g.graph.edges[1].right);
  EXPECT_EQ(g.graph.edges[0].right, g.graph.edges[1].left);

  // No further simplification should be possible.
  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());
}

TEST(GraphSimplificationTest, TwoCycles) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // Based on a real test case.
  //
  //    .--t1\             .
  //   /    | \            .
  //   |   t2  t4
  //   \    | /
  //    `--t3/
  //
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(4, &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b10, 0.999,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b10, 0b100, 0.5,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b100, 0.01,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b1000, 0.2,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b100, 0b1000,
          0.8, &mem_root, &g);

  // Do simplification steps until we can't do more. (The number doesn't matter
  // all that much, but it should definitely be more than one.)
  GraphSimplifier s(&g, &mem_root);
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  ASSERT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  ASSERT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());

  // Finally, run DPhyp to make sure the graph is still consistent
  // enough to find a solution.
  TrivialReceiver receiver(g, &mem_root, /*subgraph_pair_limit=*/-1);
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g.graph, &receiver));
  EXPECT_EQ(4, receiver.seen_nodes);
  EXPECT_EQ(5, receiver.seen_subgraph_pairs);
  EXPECT_TRUE(receiver.HasSeen(0b1111));
}

TEST(GraphSimplificationTest, ExistingHyperedge) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // Based on a real test case.
  //
  //   t1 --- t2 --- t3
  //     \   /
  //      \ /
  //       |
  //       |
  //      t4
  //
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(4, &mem_root, &g);
  g.nodes[0].table->file->stats.records = 690;
  g.nodes[1].table->file->stats.records = 6;
  g.nodes[2].table->file->stats.records = 1;
  g.nodes[3].table->file->stats.records = 1;

  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b10, 0.2,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b100, 0b10, 1.0,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b11, 0b1000,
          0.1, &mem_root, &g);

  GraphSimplifier s(&g, &mem_root);

  // First, one of t1-t2 and t2-t3 should come before the other.
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());

  // However, now all that can be done is to put t1-t2 before {t1,t2}-t4,
  // and that is already always the case, so no further simplifications
  // can be done.
  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());

  // Finally, run DPhyp to make sure the graph is still consistent
  // enough to find a solution, and that we are fully simplified.
  TrivialReceiver receiver(g, &mem_root, /*subgraph_pair_limit=*/-1);
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g.graph, &receiver));
  EXPECT_EQ(4, receiver.seen_nodes);
  EXPECT_EQ(3, receiver.seen_subgraph_pairs);
  EXPECT_TRUE(receiver.HasSeen(0b1111));
}

TEST(GraphSimplificationTest, IndirectHierarcicalJoins) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // Based on yet another real test case.
  //
  //   t3 ---- t2 -.
  //     \     /    \             .
  //      \   /      \            .
  //       \ /        --- t1
  //        |        /
  //        |       /
  //        t4 ____/
  //
  // The only possible join order here is first the simple t2-t3 edge,
  // then join in t4, and then t1. But since t1 has zero rows, it seems
  // attractive to take the t1-{t2,t4} join first, and we need to disallow that.
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(4, &mem_root, &g);
  g.nodes[0].table->file->stats.records = 0;
  g.nodes[1].table->file->stats.records = 171;
  g.nodes[2].table->file->stats.records = 6;
  g.nodes[3].table->file->stats.records = 3824;

  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b10, 0b100, 0.2,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b110, 0b1000,
          1.0, &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b1010, 0.1,
          &mem_root, &g);

  GraphSimplifier s(&g, &mem_root);

  // No simplification steps should be possible, except for that we should
  // discover that t1-{t2,t4} must come late (see above).
  EXPECT_EQ(GraphSimplifier::APPLIED_NOOP, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());

  // Finally, run DPhyp to make sure the graph is still consistent
  // enough to find a solution, and that we are fully simplified.
  TrivialReceiver receiver(g, &mem_root, /*subgraph_pair_limit=*/-1);
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g.graph, &receiver));
  EXPECT_EQ(4, receiver.seen_nodes);
  EXPECT_EQ(3, receiver.seen_subgraph_pairs);
  EXPECT_TRUE(receiver.HasSeen(0b1111));
}

TEST(GraphSimplificationTest, IndirectHierarcicalJoins2) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // An even more complicated case.
  //
  //      t1----.
  //     / |    |
  //    /  |   / \        .
  //   t5  |  t4-t3
  //    \  |  /
  //     \ | /
  //      \|/
  //       |
  //       |
  //      t2
  //
  // We need to understand that the join {t1,t4,t5}-t2 depends on the join t3-t4
  // (i.e., we cannot say it should be done before that join). This isn't
  // obvious at all; we need to understand that t3-t4 must be done before
  // t1-{t3,t4} and propagate that information up to the t1 joins.
  // (This is a case where our join inference algorithm fails, but we are being
  // saved by the impossibility check.)
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(5, &mem_root, &g);
  g.nodes[0].table->file->stats.records = 1;
  g.nodes[1].table->file->stats.records = 1;
  g.nodes[2].table->file->stats.records = 1;
  g.nodes[3].table->file->stats.records = 1;
  g.nodes[4].table->file->stats.records = 1;

  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b10000,
          0.1, &mem_root, &g);  // t1-t5.
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b100, 0b1000,
          1.0, &mem_root, &g);  // t3-t4.
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b1100, 0.1,
          &mem_root, &g);  // t1-{t3,t4}.
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b11001, 0b10,
          0.01, &mem_root, &g);  // {t1,t4,t5}-t2.

  GraphSimplifier s(&g, &mem_root);

  // We want first to put {t1,t4,t5}-t2 before t3-t4, but discover it is
  // impossible, so we apply the opposite.
  EXPECT_EQ(GraphSimplifier::APPLIED_NOOP, s.DoSimplificationStep());

  // t1-{t3,t4} can be ordered relative to {t1}-{t5}, but after that,
  // no further simplifications should be possible.
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());

  // Finally, run DPhyp to make sure the graph is still consistent
  // enough to find a solution, and that we are fully simplified.
  TrivialReceiver receiver(g, &mem_root, /*subgraph_pair_limit=*/-1);
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g.graph, &receiver));
  EXPECT_EQ(5, receiver.seen_nodes);
  EXPECT_EQ(4, receiver.seen_subgraph_pairs);
  EXPECT_TRUE(receiver.HasSeen(0b11111));
}

TEST(GraphSimplificationTest, ConflictRules) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // We set up a simple t1-t2-t3 chain join, but with a twist;
  // we'd like to do t2-t3 before t1-t2 (because t3 has zero rows),
  // but we add a conflict rule {t2} → t1 on the edge to prevent that.
  // Naturally, in a real query, that conflict rule would be absorbed
  // into a hyperedge, but we specifically want to test our handling
  // of unabsorbed conflict rules here (which can occur
  // in more complex graphs).
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(3, &mem_root, &g);
  g.nodes[0].table->file->stats.records = 100;
  g.nodes[1].table->file->stats.records = 10000;
  g.nodes[2].table->file->stats.records = 0;

  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b10, 1.0,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b10, 0b100, 1.0,
          &mem_root, &g);

  g.edges[1].expr->conflict_rules.init(&mem_root);
  g.edges[1].expr->conflict_rules.push_back(ConflictRule{0b10, 0b1});

  GraphSimplifier s(&g, &mem_root);

  // It would be fine here to have one simplification step,
  // in theory (t1-t2 before t2-t3), because it's not immediately
  // obvious that it's a no-op. But our implementation chooses to
  // force-insert that as an edge when we try the failed “t2-t3
  // before t1-t2” simplification, so we get the opposite first.
  EXPECT_EQ(GraphSimplifier::APPLIED_NOOP, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());

  // Finally, run DPhyp to make sure the graph is still consistent
  // enough to find a solution, and that we are fully simplified.
  TrivialReceiver receiver(g, &mem_root, /*subgraph_pair_limit=*/-1);
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g.graph, &receiver));
  EXPECT_EQ(3, receiver.seen_nodes);
  EXPECT_EQ(2, receiver.seen_subgraph_pairs);
  EXPECT_TRUE(receiver.HasSeen(0b111));
}

TEST(GraphSimplificationTest, Antijoin) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // (t1_100 JOIN t2_100) ANTIJOIN t3_10000. Normally, it would be better to
  // delay the t2-t3 join to get a more even cost, but since the antijoin
  // produces effectively zero rows, it should be taken immediately.
  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  NodeGuard node_guard = AddNodes(3, &mem_root, &g);
  g.nodes[0].table->file->stats.records = 100;
  g.nodes[1].table->file->stats.records = 100;
  g.nodes[2].table->file->stats.records = 10000;

  AddEdge(initializer.thd(), RelationalExpression::INNER_JOIN, 0b1, 0b10, 1.0,
          &mem_root, &g);
  AddEdge(initializer.thd(), RelationalExpression::ANTIJOIN, 0b10, 0b100, 1.0,
          &mem_root, &g);

  GraphSimplifier s(&g, &mem_root);

  // t1-t2 should be broadened to t1-{t2,t3}, so that t2-t3 is taken first.
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(0b1, g.graph.edges[0].left);
  EXPECT_EQ(0b110, g.graph.edges[0].right);
  EXPECT_EQ(g.graph.edges[0].left, g.graph.edges[1].right);
  EXPECT_EQ(g.graph.edges[0].right, g.graph.edges[1].left);

  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());
}

TEST(GraphSimplificationTest, CycleNeighboringHyperedges) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);

  /*
   * Based on a real test case:
   *
   *            t1___
   *            |    \        t6
   *            |     \      /
   *           /|\     +-- t5
   *          / | \   /      \
   *         /  |  \ /        t7
   *       t2--t3--t4
   *         \____/
   *
   * The problem with simplifying this graph is that the initial set of
   * constraints says that all three of t2-t3, t2-t4 and t3-t4 must become
   * before t1-{t2,t3,t4}. So if we later try to add a constraint that makes the
   * latter join come before one of those three joins, the online cycle finder
   * will tell us it's impossible because we get a cycle in the before-after
   * relationship. Which is true, but it doesn't take into account that the
   * final plan will never use more than two of those joins in order to join t2,
   * t3 and t4. So the graph can still be joinable using two of the edges even
   * if the third edge is involved in a cycle in the before-after graph.
   */

  NodeGuard node_guard = AddNodes(7, &mem_root, &g);
  g.nodes[0].table->file->stats.records = 1500;
  g.nodes[1].table->file->stats.records = 6000;
  g.nodes[2].table->file->stats.records = 700;
  g.nodes[3].table->file->stats.records = 200;
  g.nodes[4].table->file->stats.records = 150;
  g.nodes[5].table->file->stats.records = 1000;
  g.nodes[6].table->file->stats.records = 1000;

  THD *thd = initializer.thd();
  AddEdge(thd, RelationalExpression::LEFT_JOIN, 0b1, 0b1110, 0.0007, &mem_root,
          &g);
  AddEdge(thd, RelationalExpression::INNER_JOIN, 0b10, 0b100, 0.005, &mem_root,
          &g);
  AddEdge(thd, RelationalExpression::INNER_JOIN, 0b10, 0b1000, 0.005, &mem_root,
          &g);
  AddEdge(thd, RelationalExpression::INNER_JOIN, 0b100, 0b1000, 0.005,
          &mem_root, &g);
  AddEdge(thd, RelationalExpression::INNER_JOIN, 0b1001, 0b010000, 0.01,
          &mem_root, &g);
  AddEdge(thd, RelationalExpression::INNER_JOIN, 0b10000, 0b100000, 0.02,
          &mem_root, &g);
  AddEdge(thd, RelationalExpression::INNER_JOIN, 0b10000, 0b1000000, 0.021,
          &mem_root, &g);

  // Simplify the above graph as much as possible. The exact steps are not all
  // that important. What matters, is that we're able to get past the third call
  // to DoSimplificationStep(), where we previously hit infinite recursion, and
  // continue to simplify the graph also after we hit the problematic condition.
  GraphSimplifier s(&g, &mem_root);

  // First two simplifications are applied by adding the following constraints:
  //
  // t3-t4 before t2-t3
  // t1-t5 before t2-t4
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());

  // Here we hit the condition that we want to exercise with this test case. We
  // want to add the constraint that {t1,t4}-t5 is before t1-{t2,t3,t4}, but we
  // detect that the graph is not joinable if we do that. Usually, when we
  // detect this, we would add the opposite constraint and return APPLIED_NOOP.
  // However, the online cycle finder detects that adding the opposite
  // constraint will cause a cycle in the before-after graph, and refuses to add
  // it (this is because the online cycle finder doesn't take into account that
  // a cyclic hypergraph contains redundant edges, so we won't end up following
  // all the edges). Since we can't apply the opposite constraint, we instead
  // remove the problematic constraint from the set of potential simplifications
  // and retry with the second most promising simplification. This step
  // completes successfully and adds the constraint t2-t3 is before t2-t4.
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());

  // The next two simplifications we try are:
  //
  // {t1,t4}-t5 before t3-t4
  // {t1,t4}-t5 before t2-t3
  //
  // Both of these make the resulting graph not joinable, so we reject both and
  // instead add the opposite constraint. This time, the opposite constraints
  // are added successfully (as seen by returning APPLIED_NOOP).
  EXPECT_EQ(GraphSimplifier::APPLIED_NOOP, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::APPLIED_NOOP, s.DoSimplificationStep());

  // Attempts to add the following constraints are successful:
  //
  // t5-t6 before t2-t4
  // t5-t7 before t2-t4
  // t5-t6 before {t1,t4}-t5
  // t5-t7 before {t1,t4}-t5
  // t5-t6 before t5-t7
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());
  EXPECT_EQ(GraphSimplifier::APPLIED_SIMPLIFICATION, s.DoSimplificationStep());

  // Nothing more to simplify.
  EXPECT_EQ(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
            s.DoSimplificationStep());

  // Verify that the simplified graph is consistent.
  TrivialReceiver receiver(g, &mem_root, /*subgraph_pair_limit=*/-1);
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g.graph, &receiver));
  EXPECT_EQ(7, receiver.seen_nodes);
  EXPECT_EQ(6, receiver.seen_subgraph_pairs);
  EXPECT_TRUE(receiver.HasSeen(0b1111111));
}

[[nodiscard]] static NodeGuard CreateStarJoin(THD *thd, int graph_size,
                                              std::mt19937 *engine,
                                              MEM_ROOT *mem_root,
                                              JoinHypergraph *g) {
  std::uniform_int_distribution<int> table_size(1, 10000);
  NodeGuard node_guard = AddNodes(graph_size, mem_root, g);
  for (int node_idx = 0; node_idx < graph_size; ++node_idx) {
    g->nodes[node_idx].table->file->stats.records = table_size(*engine);
  }

  std::uniform_real_distribution<double> selectivity(0.001, 1.000);
  for (int node_idx = 1; node_idx < graph_size; ++node_idx) {
    AddEdge(thd, RelationalExpression::INNER_JOIN, 0b1, NodeMap{1} << node_idx,
            selectivity(*engine), mem_root, g);
  }

  return node_guard;
}

[[nodiscard]] static NodeGuard CreateCliqueJoin(THD *thd, int graph_size,
                                                std::mt19937 *engine,
                                                MEM_ROOT *mem_root,
                                                JoinHypergraph *g) {
  std::uniform_int_distribution<int> table_size(1, 10000);
  NodeGuard node_guard = AddNodes(graph_size, mem_root, g);
  for (int node_idx = 0; node_idx < graph_size; ++node_idx) {
    g->nodes[node_idx].table->file->stats.records = table_size(*engine);
  }

  std::uniform_real_distribution<double> selectivity(0.001, 1.000);
  for (int node1_idx = 0; node1_idx < graph_size; ++node1_idx) {
    for (int node2_idx = node1_idx + 1; node2_idx < graph_size; ++node2_idx) {
      AddEdge(thd, RelationalExpression::INNER_JOIN, NodeMap{1} << node1_idx,
              NodeMap{1} << node2_idx, selectivity(*engine), mem_root, g);
    }
  }

  return node_guard;
}

TEST(GraphSimplificationTest, UndoRedo) {
  my_testing::Server_initializer initializer;
  initializer.SetUp();

  // Get consistent seeds between runs and platforms.
  std::mt19937 engine(1234);

  MEM_ROOT mem_root;
  JoinHypergraph g(&mem_root, /*query_block=*/nullptr);
  NodeGuard node_guard = CreateStarJoin(initializer.thd(), /*graph_size=*/20,
                                        &engine, &mem_root, &g);
  GraphSimplifier s(&g, &mem_root);

  std::uniform_int_distribution<int> back_or_forward(0, 4);
  for (;;) {  // Termination condition within loop.
    if (s.num_steps_done() == 0) {
      // We can only go forward.
      ASSERT_NE(GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE,
                s.DoSimplificationStep());
    } else {
      // With 20% probability, undo a step. Otherwise, do one.
      // This ensures we get to try both undos and redos.
      if (back_or_forward(engine) == 0) {
        s.UndoSimplificationStep();
      } else {
        if (s.DoSimplificationStep() ==
            GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE) {
          // We're all simplified.
          break;
        }
      }
    }
  }

  // This is just an empirical number; it can change if the heuristics change.
  // But it shouldn't change if we remove the undo code or change the seed.
  EXPECT_EQ(171, s.num_steps_done());
}

static void BM_FullySimplifyStarJoin(int graph_size, size_t num_iterations) {
  StopBenchmarkTiming();

  // Get consistent seeds between runs and platforms.
  std::mt19937 engine(1234);

  for (size_t i = 0; i < num_iterations; ++i) {
    MEM_ROOT mem_root;
    JoinHypergraph g(&mem_root, /*query_block=*/nullptr);
    my_testing::Server_initializer initializer;
    initializer.SetUp();

    NodeGuard node_guard =
        CreateStarJoin(initializer.thd(), graph_size, &engine, &mem_root, &g);

    StartBenchmarkTiming();
    GraphSimplifier s(&g, &mem_root);
    while (s.DoSimplificationStep() !=
           GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE)
      ;
    StopBenchmarkTiming();
  }
}

static void BM_FullySimplifyStarJoin10(size_t num_iterations) {
  BM_FullySimplifyStarJoin(10, num_iterations);
}
static void BM_FullySimplifyStarJoin20(size_t num_iterations) {
  BM_FullySimplifyStarJoin(20, num_iterations);
}
static void BM_FullySimplifyStarJoin30(size_t num_iterations) {
  BM_FullySimplifyStarJoin(30, num_iterations);
}
static void BM_FullySimplifyStarJoin40(size_t num_iterations) {
  BM_FullySimplifyStarJoin(40, num_iterations);
}
static void BM_FullySimplifyStarJoin50(size_t num_iterations) {
  BM_FullySimplifyStarJoin(50, num_iterations);
}
BENCHMARK(BM_FullySimplifyStarJoin10)
BENCHMARK(BM_FullySimplifyStarJoin20)
BENCHMARK(BM_FullySimplifyStarJoin30)
BENCHMARK(BM_FullySimplifyStarJoin40)
BENCHMARK(BM_FullySimplifyStarJoin50)
// NOTE: 100-way star joins are quoted as 160 ms in the paper,
// but since MAX_TABLES == 61, we cannot compare directly.
// Extrapolation indicates that we are doing fairly well, though.

static void BM_FullySimplifyCliqueJoin(int graph_size, size_t num_iterations) {
  StopBenchmarkTiming();

  // Get consistent seeds between runs and platforms.
  std::mt19937 engine(1234);

  for (size_t i = 0; i < num_iterations; ++i) {
    MEM_ROOT mem_root;
    JoinHypergraph g(&mem_root, /*query_block=*/nullptr);
    my_testing::Server_initializer initializer;
    initializer.SetUp();

    NodeGuard node_guard =
        CreateCliqueJoin(initializer.thd(), graph_size, &engine, &mem_root, &g);

    StartBenchmarkTiming();
    GraphSimplifier s(&g, &mem_root);
    while (s.DoSimplificationStep() !=
           GraphSimplifier::NO_SIMPLIFICATION_POSSIBLE)
      ;
    StopBenchmarkTiming();
  }
}

static void BM_FullySimplifyCliqueJoin10(size_t num_iterations) {
  BM_FullySimplifyCliqueJoin(10, num_iterations);
}
static void BM_FullySimplifyCliqueJoin20(size_t num_iterations) {
  BM_FullySimplifyCliqueJoin(20, num_iterations);
}
static void BM_FullySimplifyCliqueJoin30(size_t num_iterations) {
  BM_FullySimplifyCliqueJoin(30, num_iterations);
}
// static void BM_FullySimplifyCliqueJoin40(size_t num_iterations) {
//   BM_FullySimplifyCliqueJoin(40, num_iterations);
// }
// static void BM_FullySimplifyCliqueJoin50(size_t num_iterations) {
//   BM_FullySimplifyCliqueJoin(50, num_iterations);
// }
BENCHMARK(BM_FullySimplifyCliqueJoin10)
BENCHMARK(BM_FullySimplifyCliqueJoin20)
BENCHMARK(BM_FullySimplifyCliqueJoin30)

// Too slow to run on every commit, but can be enabled manually without
// problems.
// BENCHMARK(BM_FullySimplifyCliqueJoin40)
// BENCHMARK(BM_FullySimplifyCliqueJoin50)
