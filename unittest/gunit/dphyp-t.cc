/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <bitset>
#include <unordered_set>
#include <vector>

#include <gmock/gmock.h>
#include "my_compiler.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "unittest/gunit/benchmark.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Expectation;
using ::testing::Return;
using ::testing::StrictMock;

using hypergraph::Hypergraph;
using hypergraph::NodeMap;
using hypergraph::PrintSet;

class MockReceiver {
 public:
  MOCK_METHOD1(HasSeen, bool(NodeMap));
  MOCK_METHOD1(FoundSingleNode, bool(int));
  MOCK_METHOD3(FoundSubgraphPair, bool(NodeMap, NodeMap, int));
};

TEST(DPhypTest, ExampleHypergraph) {
  MEM_ROOT mem_root;
  /*
    The example graph from the DPhyp paper. One large
    hyperedge and four simple edges.

      R1-.   ,-R4
      |   \ /   |
      R2---x---R5
      |   / \   |
      R3-'   `-R6
   */
  Hypergraph g(&mem_root);
  g.AddNode();                    // R1
  g.AddNode();                    // R2
  g.AddNode();                    // R3
  g.AddNode();                    // R4
  g.AddNode();                    // R5
  g.AddNode();                    // R6
  g.AddEdge(0b000001, 0b000010);  // R1-R2
  g.AddEdge(0b000010, 0b000100);  // R2-R3
  g.AddEdge(0b001000, 0b010000);  // R4-R5
  g.AddEdge(0b010000, 0b100000);  // R5-R6
  g.AddEdge(0b000111, 0b111000);  // {R1,R2,R3}-{R4,R5,R6}

  StrictMock<MockReceiver> mr;
  EXPECT_CALL(mr, FoundSingleNode(0));
  EXPECT_CALL(mr, FoundSingleNode(1));
  EXPECT_CALL(mr, FoundSingleNode(2));
  EXPECT_CALL(mr, FoundSingleNode(3));
  EXPECT_CALL(mr, FoundSingleNode(4));
  EXPECT_CALL(mr, FoundSingleNode(5));

  // Fallback matcher.
  EXPECT_CALL(mr, HasSeen(_)).WillRepeatedly(Return(false));

  // Right side of the graph:

  // Found link between R5 and R6.
  Expectation seen_r5_r6 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b010000, 0b100000, 3));
  EXPECT_CALL(mr, HasSeen(0b110000))
      .After(seen_r5_r6)
      .WillRepeatedly(Return(true));

  // Found link between R4 and R5.
  Expectation seen_r4_r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b001000, 0b010000, 2));
  EXPECT_CALL(mr, HasSeen(0b011000))
      .After(seen_r4_r5)
      .WillRepeatedly(Return(true));

  // Found link between R4 and {R5,R6}, through the R4-R5 edge.
  Expectation seen_r4_r5r6 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b001000, 0b110000, 2));

  // Found like between {R4,R5} and {R6}, through the R5-R6 edge.
  Expectation seen_r4r5_r6 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b011000, 0b100000, 3));

  // {R4,R5,R6} is connected (called only after we've seen its components).
  EXPECT_CALL(mr, HasSeen(0b111000))
      .After(seen_r4_r5r6)
      .After(seen_r4r5_r6)
      .WillRepeatedly(Return(true));

  // Very similar, left side of the graph:

  // Found link between R2 and R3.
  Expectation seen_r2_r3 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b000010, 0b000100, 1));
  EXPECT_CALL(mr, HasSeen(0b000110))
      .After(seen_r2_r3)
      .WillRepeatedly(Return(true));

  // Found link between R1 and R2.
  Expectation seen_r1_r2 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b000001, 0b000010, 0));
  EXPECT_CALL(mr, HasSeen(0b000011))
      .After(seen_r1_r2)
      .WillRepeatedly(Return(true));

  // Found link between R1 and {R2,R3}, through the R1-R2 edge.
  Expectation seen_r1_r2r3 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b000001, 0b000110, 0));

  // Found like between {R1,R2} and {R3}, through the R2-R3 edge.
  Expectation seen_r1r2_r3 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b000011, 0b000100, 1));

  // {R1,R2,R3} is connected (called only after we've seen its components).
  EXPECT_CALL(mr, HasSeen(0b000111))
      .After(seen_r1_r2r3)
      .After(seen_r1r2_r3)
      .WillRepeatedly(Return(true));

  // Found link between {R1,R2,R3} and {R4,R5,R6}.
  EXPECT_CALL(mr, FoundSubgraphPair(0b000111, 0b111000, 4))
      .After(seen_r1_r2r3)
      .After(seen_r1r2_r3)
      .After(seen_r4_r5r6)
      .After(seen_r4r5_r6);

  EXPECT_FALSE(EnumerateAllConnectedPartitions(g, &mr));
}

TEST(DPhypTest, Loop) {
  MEM_ROOT mem_root;
  /*
    Shows that we can go around a loop and connect R1 to {R2,R3,R4,R5}
    graph through {R2,R5}, even though R5 was not part of R1's
    neighborhood (ie., R2 was chosen as the representative node).
    This requires that we remember that R5 was a part of R1's full
    neighborhood.

            R2----R3
            /     |
           /      |
       R1--       |
           \      |
            \     |
            R5----R4
   */
  Hypergraph g(&mem_root);
  g.AddNode();                  // R1
  g.AddNode();                  // R2
  g.AddNode();                  // R3
  g.AddNode();                  // R4
  g.AddNode();                  // R5
  g.AddEdge(0b00001, 0b10010);  // R1-{R2,R5}
  g.AddEdge(0b00010, 0b00100);  // R2-R3
  g.AddEdge(0b00100, 0b01000);  // R3-R4
  g.AddEdge(0b01000, 0b10000);  // R4-R5

  StrictMock<MockReceiver> mr;
  EXPECT_CALL(mr, FoundSingleNode(0));
  EXPECT_CALL(mr, FoundSingleNode(1));
  EXPECT_CALL(mr, FoundSingleNode(2));
  EXPECT_CALL(mr, FoundSingleNode(3));
  EXPECT_CALL(mr, FoundSingleNode(4));

  // Fallback matcher.
  EXPECT_CALL(mr, HasSeen(_)).WillRepeatedly(Return(false));

  // Found link between R4 and R5.
  Expectation seen_r4_r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b01000, 0b10000, 3));
  EXPECT_CALL(mr, HasSeen(0b11000))
      .After(seen_r4_r5)
      .WillRepeatedly(Return(true));

  // Found link between R3 and R4.
  Expectation seen_r3_r4 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00100, 0b01000, 2));
  EXPECT_CALL(mr, HasSeen(0b01100))
      .After(seen_r3_r4)
      .WillRepeatedly(Return(true));

  // Found link between R3 and {R4,R5}, through the R3-R4 edge.
  Expectation seen_r3_r4r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00100, 0b11000, 2)).After(seen_r4_r5);

  // Found link between {R3,R4} and R5, through the R4-R5 edge.
  Expectation seen_r3r4_r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b01100, 0b10000, 3)).After(seen_r3_r4);

  // {R3,R4,R5} is connected (called only after we've seen its components).
  EXPECT_CALL(mr, HasSeen(0b11100))
      .After(seen_r3_r4r5)
      .After(seen_r3r4_r5)
      .WillRepeatedly(Return(true));

  // Found link between R2 and R3.
  Expectation seen_r2_r3 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00010, 0b00100, 1));
  EXPECT_CALL(mr, HasSeen(0b00110)).WillRepeatedly(Return(true));

  // Found link between R2 and {R3,R4}, through the R2-R3 edge.
  Expectation seen_r2_r3r4 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00010, 0b01100, 1)).After(seen_r3_r4);

  // Found link between {R2,R3} and R4, through the R3-R4 edge.
  Expectation seen_r2r3_r4 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00110, 0b01000, 2)).After(seen_r2_r3);

  // {R2,R3,R4} is connected (called only after we've seen its components).
  EXPECT_CALL(mr, HasSeen(0b01110))
      .After(seen_r2_r3r4)
      .After(seen_r2r3_r4)
      .WillRepeatedly(Return(true));

  // Found link between R2 and {R3,R4,R5}, through the R2-R3 edge.
  Expectation seen_r2_r3r4r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00010, 0b11100, 1))
          .After(seen_r3_r4r5)
          .After(seen_r3r4_r5);

  // Found link between {R2,R3} and {R4,R5}, through the R3-R4 edge.
  Expectation seen_r2r3_r4r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b00110, 0b11000, 2))
          .After(seen_r2_r3)
          .After(seen_r4_r5);

  // Found link between {R2,R3,R4} and R5, through the R4-R5 edge.
  Expectation seen_r2r3r4_r5 =
      EXPECT_CALL(mr, FoundSubgraphPair(0b01110, 0b10000, 3))
          .After(seen_r2r3_r4)
          .After(seen_r2_r3r4);

  // {R2,R3,R4,R5} is connected (called only after we've seen its components).
  EXPECT_CALL(mr, HasSeen(0b11110))
      .After(seen_r2_r3r4r5)
      .After(seen_r2r3_r4r5)
      .After(seen_r2r3r4_r5)
      .WillRepeatedly(Return(true));

  // Finally, found link between R1 and {R2,R3,R4,R5}, through the R1-{R2,R5}
  // edge.
  EXPECT_CALL(mr, FoundSubgraphPair(0b00001, 0b11110, 0))
      .After(seen_r2_r3r4r5)
      .After(seen_r2r3_r4r5)
      .After(seen_r2r3r4_r5);

  EXPECT_FALSE(EnumerateAllConnectedPartitions(g, &mr));
}

TEST(DPhypTest, AbortWithError) {
  MEM_ROOT mem_root;
  /*
    A simple chain.

      R1--R2--R3
   */
  Hypergraph g(&mem_root);
  g.AddNode();                    // R1
  g.AddNode();                    // R2
  g.AddNode();                    // R3
  g.AddEdge(0b000001, 0b000010);  // R1-R2
  g.AddEdge(0b000010, 0b000100);  // R2-R3

  StrictMock<MockReceiver> mr;
  EXPECT_CALL(mr, FoundSingleNode(1));
  EXPECT_CALL(mr, FoundSingleNode(2));

  // Fallback matcher.
  EXPECT_CALL(mr, HasSeen(_)).WillRepeatedly(Return(false));

  // Found link between R2 and R3. We return true (error) here,
  // so the algorithm should abort without ever seeing R1
  // or any of the links to it.
  EXPECT_CALL(mr, FoundSubgraphPair(0b000010, 0b000100, 1))
      .WillOnce(Return(true));

  EXPECT_TRUE(EnumerateAllConnectedPartitions(g, &mr));
}

// A Receiver used for unit tests. It records all subgraph pairs we see,
// allowing us to check afterwards that the correct ones were discovered
// (and no others). It also verifies correct ordering of HasSeen() calls.
struct AccumulatingReceiver {
  struct Subplan {
    NodeMap left, right;
    int edge_idx;
  };

  bool HasSeen(NodeMap subgraph) {
    if (seen_subplans.find(subgraph) == seen_subplans.end()) {
      has_returned_nonconnected.insert(subgraph);
      return false;
    } else {
      assert(has_returned_nonconnected.count(subgraph) == 0);
      return true;
    }
  }

  bool FoundSingleNode(int node_idx) {
    NodeMap map = TableBitmap(node_idx);

    // We must always see all enumerations for a subset before we can
    // use that subset.
    assert(used_in_larger_subset.count(map) == 0);

    // Should be called only once.
    assert(seen_subplans.count(map) == 0);

    seen_subplans.emplace(map, Subplan{0, 0, -1});
    return false;
  }

  bool FoundSubgraphPair(NodeMap left, NodeMap right, int edge_idx) {
    printf("Found connection between %s and %s along edge %d\n",
           PrintSet(left).c_str(), PrintSet(right).c_str(), edge_idx);

    // We must always see all enumerations for a subset before we can
    // use that subset.
    assert(used_in_larger_subset.count(left | right) == 0);
    used_in_larger_subset.insert(left);
    used_in_larger_subset.insert(right);

    // Additional test that in practice tests the same thing.
    assert(has_returned_nonconnected.count(left | right) == 0);

    // We should only get a given subgraph pair once.
    EXPECT_FALSE(SeenSubgraphPair(left, right, edge_idx))
        << "Duplicate connection between " << PrintSet(left) << " and "
        << PrintSet(right) << " along edge " << edge_idx;

    seen_subplans.emplace(left | right, Subplan{left, right, edge_idx});
    return false;
  }

  // Checks whether FoundSubgraphPair() was called with the given arguments.
  // Fairly slow for large graphs.
  bool SeenSubgraphPair(NodeMap left, NodeMap right, int edge_idx) {
    const auto subset_subplans = seen_subplans.equal_range(left | right);
    for (auto it = subset_subplans.first; it != subset_subplans.second; ++it) {
      if (it->second.left == left && it->second.right == right &&
          it->second.edge_idx == edge_idx) {
        return true;
      }
    }
    return false;
  }

  std::unordered_set<NodeMap> has_returned_nonconnected;
  std::unordered_set<NodeMap> used_in_larger_subset;
  std::multimap<NodeMap, Subplan> seen_subplans;
};

// A very simple receiver used during benchmarking only, used to isolate
// away receiver performance from the algorithm itself. Probably the fastest
// imaginable implementation; does nothing useful except remember which
// subgraphs are connected, as required for HasSeen().
template <int Size>
struct BenchmarkReceiver {
  bool HasSeen(NodeMap subgraph) { return seen_subplans[subgraph]; }

  bool FoundSingleNode(int node_idx) {
    NodeMap map = TableBitmap(node_idx);
    seen_subplans.set(map);
    return false;
  }

  bool FoundSubgraphPair(NodeMap left, NodeMap right, int) {
    seen_subplans.set(left | right);
    return false;
  }

  static constexpr int num_elements = 1 << Size;
  std::bitset<num_elements> seen_subplans;
};

// Creates a simple chain A-B-C-D-..., and verifies that we get all possible
// permutations.
TEST(DPhypTest, Chain) {
  constexpr int num_elements = 20;

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  for (int i = 0; i < num_elements; ++i) {
    g.AddNode();
    if (i != 0) {
      g.AddEdge(TableBitmap(i - 1), TableBitmap(i));
    }
  }

  AccumulatingReceiver receiver;
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g, &receiver));

  // Look at all possible subchains of the chain.
  int expected_subplans = 0;
  for (int start_idx = 0; start_idx < num_elements; ++start_idx) {
    for (int end_idx = start_idx + 1; end_idx <= num_elements; ++end_idx) {
      NodeMap subset = TablesBetween(start_idx, end_idx);

      if (end_idx == start_idx + 1) {
        // Single node, so should have a single single-node subplan.
        ASSERT_EQ(1, receiver.seen_subplans.count(subset));
        EXPECT_EQ(0, receiver.seen_subplans.find(subset)->second.left);
        EXPECT_EQ(0, receiver.seen_subplans.find(subset)->second.right);
        EXPECT_EQ(-1, receiver.seen_subplans.find(subset)->second.edge_idx);
        ++expected_subplans;
        continue;
      }

      // This subchain should be splittable along all possible midpoints.
      for (int split_after_idx = start_idx; split_after_idx < end_idx - 1;
           ++split_after_idx) {
        NodeMap left = TablesBetween(start_idx, split_after_idx + 1);
        NodeMap right = TablesBetween(split_after_idx + 1, end_idx);
        int edge_idx = split_after_idx;

        EXPECT_TRUE(receiver.SeenSubgraphPair(left, right, edge_idx))
            << "Subset " << PrintSet(subset) << " should be splittable into "
            << PrintSet(left) << " and " << PrintSet(right) << " along edge "
            << edge_idx;
        ++expected_subplans;
      }

      EXPECT_TRUE(receiver.seen_subplans.count(subset));
    }
  }

  // We should have no other subplans than the ones we checked for earlier.
  EXPECT_EQ(expected_subplans, receiver.seen_subplans.size());
}

// Demonstrates that we need to grow neighborhoods carefully when looking for
// complement seeds. Specifically, when starting with {R1} (which has
// neighborhood {R2,R3,R4}) and growing it with R2, we'd normally only consider
// the neighborhood of R2, since R3 and R4 are now in the forbidden set.
// However, when looking for seeds for the complement of {R1,R2}, we need to
// take R3 and R4 back into account, since they are not forbidden in this
// context.
//
// This test doesn't test precise call ordering, only that we get all the
// expected sets.
TEST(DPhypTest, SmallStar) {
  MEM_ROOT mem_root;
  /*
     R2
     |
     |
     R1---R3
     |
     |
     R4
   */
  Hypergraph g(&mem_root);
  g.AddNode();                  // R1
  g.AddNode();                  // R2
  g.AddNode();                  // R3
  g.AddNode();                  // R4
  g.AddEdge(0b00001, 0b00010);  // R1-R2
  g.AddEdge(0b00001, 0b00100);  // R1-R3
  g.AddEdge(0b00001, 0b01000);  // R1-R4

  StrictMock<MockReceiver> mr;
  EXPECT_CALL(mr, FoundSingleNode(0));
  EXPECT_CALL(mr, FoundSingleNode(1));
  EXPECT_CALL(mr, FoundSingleNode(2));
  EXPECT_CALL(mr, FoundSingleNode(3));

  for (int i = 1; i < 16; ++i) {
    if (IsSingleBitSet(i)) {
      EXPECT_CALL(mr, HasSeen(i))
          .Times(AnyNumber())
          .WillRepeatedly(Return(true));
    } else {
      // Anything containing R1 is connected, anything else is not.
      EXPECT_CALL(mr, HasSeen(i))
          .Times(AnyNumber())
          .WillRepeatedly(Return(i & 1));
    }
  }

  EXPECT_CALL(mr, FoundSubgraphPair(0b0001, 0b0010, 0));  // R1-R2.
  EXPECT_CALL(mr, FoundSubgraphPair(0b0001, 0b0100, 1));  // R1-R3.
  EXPECT_CALL(mr, FoundSubgraphPair(0b0001, 0b1000, 2));  // R1-R4.

  EXPECT_CALL(mr,
              FoundSubgraphPair(0b0011, 0b0100, 1));  // {R1,R2}-R3 along R1-R3.
  EXPECT_CALL(mr,
              FoundSubgraphPair(0b0011, 0b1000, 2));  // {R1,R2}-R4 along R1-R4.

  EXPECT_CALL(mr,
              FoundSubgraphPair(0b0101, 0b0010, 0));  // {R1,R3}-R2 along R1-R2.
  EXPECT_CALL(mr,
              FoundSubgraphPair(0b0101, 0b1000, 2));  // {R1,R3}-R4 along R1-R4.

  EXPECT_CALL(mr,
              FoundSubgraphPair(0b1001, 0b0010, 0));  // {R1,R4}-R2 along R1-R2.
  EXPECT_CALL(mr,
              FoundSubgraphPair(0b1001, 0b0100, 1));  // {R1,R4}-R3 along R1-R3.

  EXPECT_CALL(
      mr, FoundSubgraphPair(0b0111, 0b1000, 2));  // {R1,R2,R3}-R4 along R1-R4.
  EXPECT_CALL(
      mr, FoundSubgraphPair(0b1011, 0b0100, 1));  // {R1,R2,R4}-R3 along R1-R3.
  EXPECT_CALL(
      mr, FoundSubgraphPair(0b1101, 0b0010, 0));  // {R1,R2,R4}-R3 along R1-R2.

  EXPECT_FALSE(EnumerateAllConnectedPartitions(g, &mr));
}

// Creates a clique (everything connected to everything, with simple edges)
// and checks that we get every possible permutation, along every relevant edge.
TEST(DPhypTest, Clique) {
  constexpr int num_elements = 6;

  int edge_indexes[num_elements][num_elements];

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  for (int i = 0; i < num_elements; ++i) {
    g.AddNode();
    for (int j = 0; j < i; ++j) {
      g.AddEdge(TableBitmap(i), TableBitmap(j));
      edge_indexes[i][j] = edge_indexes[j][i] = (g.edges.size() - 1) / 2;
    }
  }

  AccumulatingReceiver receiver;
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g, &receiver));

  int expected_subplans = 0;

  // Look at all possible non-zero subsets of the clique.
  for (NodeMap subset = 1; subset < (NodeMap{1} << num_elements); ++subset) {
    if (IsSingleBitSet(subset)) {
      // Single node, so should have a single single-node subplan.
      ASSERT_EQ(1, receiver.seen_subplans.count(subset));
      EXPECT_EQ(0, receiver.seen_subplans.find(subset)->second.left);
      EXPECT_EQ(0, receiver.seen_subplans.find(subset)->second.right);
      EXPECT_EQ(-1, receiver.seen_subplans.find(subset)->second.edge_idx);
      ++expected_subplans;
      continue;
    }

    // Find all possible two-way partitions of this subset.
    for (NodeMap left : NonzeroSubsetsOf(subset)) {
      if (left == subset) continue;
      NodeMap right = subset & ~left;
      if (IsolateLowestBit(left) > IsolateLowestBit(right)) continue;

      for (size_t left_idx : BitsSetIn(left)) {
        for (size_t right_idx : BitsSetIn(right)) {
          int edge_idx = edge_indexes[left_idx][right_idx];
          EXPECT_TRUE(receiver.SeenSubgraphPair(left, right, edge_idx))
              << "Subset " << PrintSet(subset) << " should be splittable into "
              << PrintSet(left) << " and " << PrintSet(right) << " along edge "
              << edge_idx;
          ++expected_subplans;
        }
      }
    }
  }

  // We should have no other subplans than the ones we checked for earlier.
  EXPECT_EQ(expected_subplans, receiver.seen_subplans.size());
}

// Constructs a hypergraph of A LEFT JOIN (B LEFT JOIN (C LEFT JOIN ...)),
// for null-tolerant joins; ie., no reordering is possible and only one
// possible plan should exist.
TEST(DPhypTest, OuterJoinChain) {
  constexpr int num_nodes = 5;

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  g.AddNode();                  // R1
  g.AddNode();                  // R2
  g.AddNode();                  // R3
  g.AddNode();                  // R4
  g.AddNode();                  // R5
  g.AddEdge(0b11110, 0b00001);  // R1-{R2,R3,R4,R5}
  g.AddEdge(0b11100, 0b00010);  // R2-{R3,R4,R5}
  g.AddEdge(0b11000, 0b00100);  // R3-{R4,R5}
  g.AddEdge(0b10000, 0b01000);  // R4-R5

  AccumulatingReceiver receiver;
  EXPECT_FALSE(EnumerateAllConnectedPartitions(g, &receiver));

  int expected_subplans = 0;

  for (size_t node_idx = 0; node_idx < num_nodes; ++node_idx) {
    NodeMap subset = TableBitmap(node_idx);
    ASSERT_EQ(1, receiver.seen_subplans.count(subset));
    EXPECT_EQ(0, receiver.seen_subplans.find(subset)->second.left);
    EXPECT_EQ(0, receiver.seen_subplans.find(subset)->second.right);
    EXPECT_EQ(-1, receiver.seen_subplans.find(subset)->second.edge_idx);
    ++expected_subplans;
  }

  for (size_t edge_idx = 0; edge_idx < num_nodes - 1; ++edge_idx) {
    NodeMap subset = g.edges[edge_idx * 2].left | g.edges[edge_idx * 2].right;
    ASSERT_EQ(1, receiver.seen_subplans.count(subset));

    // NOTE: The edges come out flipped compared to the order we added them,
    // due to the ordering properties.
    EXPECT_EQ(g.edges[edge_idx * 2].right,
              receiver.seen_subplans.find(subset)->second.left);
    EXPECT_EQ(g.edges[edge_idx * 2].left,
              receiver.seen_subplans.find(subset)->second.right);

    EXPECT_EQ(edge_idx, receiver.seen_subplans.find(subset)->second.edge_idx);
    ++expected_subplans;
  }

  // We should have no other subplans than the ones we checked for earlier.
  EXPECT_EQ(expected_subplans, receiver.seen_subplans.size());
}

static void BM_Chain20(size_t num_iterations) {
  StopBenchmarkTiming();
  constexpr int num_nodes = 20;

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  for (int i = 0; i < num_nodes; ++i) {
    g.AddNode();
    if (i != 0) {
      g.AddEdge(TableBitmap(i - 1), TableBitmap(i));
    }
  }

  for (size_t i = 0; i < num_iterations; ++i) {
    BenchmarkReceiver<num_nodes> receiver;

    StartBenchmarkTiming();
    EnumerateAllConnectedPartitions(g, &receiver);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_Chain20)

// Like the OuterJoinChain test, just as a benchmark.
//
// Note that even though we only emit one possible plan, this test is not
// that much faster then BM_Chain20. The reason is that even though the
// number of subsets go down from O(n³) to O(n²), each node also is touched
// by more hyperedges (on the order of O(n)), so neighborhood finding has to
// sift through more edges. It would be nice if we had some way of culling
// these “obviously wrong” edges without a linear search (e.g., it is
// meaningless for R5 to traverse a hyperedge to R1 in the neighborhood
// calculation when expanding subgraphs, since it goes “backwards”), but in the
// presence of cycles, there does not seem to be an obvious way of codifying
// this.
static void BM_NestedOuterJoin20(size_t num_iterations) {
  StopBenchmarkTiming();
  constexpr int num_nodes = 20;

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  for (int i = 0; i < num_nodes; ++i) {
    g.AddNode();
  }
  for (int i = 0; i < num_nodes - 1; ++i) {
    g.AddEdge(TableBitmap(i), TablesBetween(i + 1, num_nodes));
  }

  for (size_t i = 0; i < num_iterations; ++i) {
    BenchmarkReceiver<num_nodes> receiver;

    StartBenchmarkTiming();
    EnumerateAllConnectedPartitions(g, &receiver);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_NestedOuterJoin20)

// Benchmark from the DPhyp paper. We only implement the version
// with hyperedges split into cardinality-2 hypernodes.
static void BM_HyperCycle16(size_t num_iterations) {
  StopBenchmarkTiming();
  constexpr int num_nodes = 16;  // A multiple of four.

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  for (int i = 0; i < num_nodes; ++i) {
    g.AddNode();
  }

  // Add the simple edges that create the cycle.
  for (int i = 0; i < num_nodes; ++i) {
    g.AddEdge(TableBitmap(i), TableBitmap((i + 1) % num_nodes));
  }

  // Add some hyperedges.
  for (int i = 0; i < num_nodes; i += 4) {
    g.AddEdge(TablesBetween(i, i + 2), TablesBetween(i + 2, i + 4));
  }

  for (size_t i = 0; i < num_iterations; ++i) {
    BenchmarkReceiver<num_nodes> receiver;

    StartBenchmarkTiming();
    EnumerateAllConnectedPartitions(g, &receiver);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_HyperCycle16)

static void BM_Star17(size_t num_iterations) {
  StopBenchmarkTiming();
  constexpr int num_nodes = 17;

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  g.AddNode();  // The central node.
  for (int i = 1; i < num_nodes; ++i) {
    g.AddNode();
    g.AddEdge(TableBitmap(0), TableBitmap(i));
  }

  for (size_t i = 0; i < num_iterations; ++i) {
    BenchmarkReceiver<num_nodes> receiver;

    StartBenchmarkTiming();
    EnumerateAllConnectedPartitions(g, &receiver);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_Star17)

// Benchmark from the DPhyp paper. This is the version with hyperedges split
// into cardinality-2 hypernodes.
static void BM_HyperStar17_ManyHyperedges(size_t num_iterations) {
  StopBenchmarkTiming();
  constexpr int num_nodes = 17;  // A multiple of four, plus one.

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  g.AddNode();  // The central node.
  for (int i = 1; i < num_nodes; ++i) {
    g.AddNode();
    g.AddEdge(TableBitmap(0), TableBitmap(i));
  }

  // Add some hyperedges.
  constexpr int half = (num_nodes - 1) / 2;
  for (int i = 0; i < half; i += 2) {
    g.AddEdge(TablesBetween(i + 1, i + 3),
              TablesBetween(i + half + 1, i + half + 3));
  }

  for (size_t i = 0; i < num_iterations; ++i) {
    BenchmarkReceiver<num_nodes> receiver;

    StartBenchmarkTiming();
    EnumerateAllConnectedPartitions(g, &receiver);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_HyperStar17_ManyHyperedges)

// Benchmark from the DPhyp paper. This is the version with no hyperedge
// split (only one large hyperedge).
static void BM_HyperStar17_SingleLargeHyperedge(size_t num_iterations) {
  StopBenchmarkTiming();
  constexpr int num_nodes = 17;  // A multiple of two, plus one.

  MEM_ROOT mem_root;
  Hypergraph g(&mem_root);
  g.AddNode();  // The central node.
  for (int i = 1; i < num_nodes; ++i) {
    g.AddNode();
    g.AddEdge(TableBitmap(0), TableBitmap(i));
  }

  // Add a single large hyperedge.
  constexpr int half = (num_nodes - 1) / 2;
  g.AddEdge(TablesBetween(1, half + 1), TablesBetween(half + 1, num_nodes));

  for (size_t i = 0; i < num_iterations; ++i) {
    BenchmarkReceiver<num_nodes> receiver;

    StartBenchmarkTiming();
    EnumerateAllConnectedPartitions(g, &receiver);
    StopBenchmarkTiming();
  }
}
BENCHMARK(BM_HyperStar17_SingleLargeHyperedge)
