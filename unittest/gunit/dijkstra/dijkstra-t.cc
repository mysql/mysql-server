/* Copyright (c) 2009, 2021, Oracle and/or its affiliates.

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

/*
  This is a simple example of how to use the google unit test framework.

  For an introduction to the constructs used below, see:
  http://code.google.com/p/googletest/wiki/GoogleTestPrimer
*/

#include <gtest/gtest.h>

#include "unittest/gunit/gunit_test_main.h"

#include "sql/Dijkstras_functor.h"

namespace dijkstra_unittest {

class DijkstraTest : public ::testing::Test {
 protected:
  DijkstraTest(){}

 private:
  // Declares (but does not define) copy constructor and assignment operator.
  GTEST_DISALLOW_COPY_AND_ASSIGN_(DijkstraTest);
};

// check that cost is total path cost
// ! floating point cmp
void check_cost(const std::vector<const Edge*>& path , const double& cost){
  double expected_cost = 0;
  for (const Edge* e : path) expected_cost += e->cost;
  EXPECT_FLOAT_EQ(cost, expected_cost);
}

// test dijkstra without heuristic
TEST_F(DijkstraTest, NullHeuristic) {  
  Edge edges[] = {
    // Edge{ id, from, to, cost }
    Edge{ 0, 0, 1, 5.0 },
    Edge{ 1, 0, 2, 12.0 },
    Edge{ 2, 1, 2, 5.0 },
    Edge{ 3, 1, 3, 15.0 },
    Edge{ 4, 2, 3, 5.0 },
    Edge{ 5, 0, 3, 20.0 },
    Edge{ 6, 3, 0, 1.0 },
    Edge{ 7, 3, 4, 3.0 },
    Edge{ 8, 0, 4, 17.0 }
  };
  size_t n_edges = sizeof(edges) / sizeof(Edge);
  
  std::unordered_multimap<int, const Edge*> edge_map;
  for (size_t i = 0; i < n_edges; i++) {
      Edge& e = edges[i];
      edge_map.insert(std::pair(e.from, &e));
  }
  double cost;
  Dijkstra dijkstra(&edge_map);

  // check 0 -> 3
  std::vector<const Edge*> path = dijkstra(0, 3, cost);
  std::vector<const Edge*> expected_path = { &edges[0], &edges[2], &edges[4] };
  EXPECT_EQ(path, expected_path);
  check_cost(path, cost);

  // check 0 -> 4
  path = dijkstra(0, 4, cost);
  expected_path = { &edges[8] };
  EXPECT_EQ(path, expected_path);
  check_cost(path, cost);

  // check 1 -> 0
  path = dijkstra(1, 0, cost);
  expected_path = { &edges[2], &edges[4], &edges[6] };
  EXPECT_EQ(path, expected_path);
  check_cost(path, cost);
}

// test dijkstra with heuristic (A*)
TEST_F(DijkstraTest, EuclideanHeuristic) {
  // TODO
}

}  // namespace dijkstra_unittest
