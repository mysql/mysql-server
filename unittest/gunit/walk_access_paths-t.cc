/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/walk_access_paths.h"

using std::vector;
using testing::ElementsAre;

namespace {

AccessPath MakeNestedLoopJoin(AccessPath *outer, AccessPath *inner) {
  AccessPath join;
  join.type = AccessPath::NESTED_LOOP_JOIN;
  join.nested_loop_join().outer = outer;
  join.nested_loop_join().inner = inner;
  return join;
}

AccessPath MakeHashJoin(AccessPath *outer, AccessPath *inner) {
  AccessPath join;
  join.type = AccessPath::HASH_JOIN;
  join.hash_join().outer = outer;
  join.hash_join().inner = inner;
  return join;
}

AccessPath MakeTableScan() {
  AccessPath path;
  path.type = AccessPath::TABLE_SCAN;
  return path;
}

}  // namespace

namespace walk_access_paths_test {

TEST(WalkAccessPathsTest, PreOrderTraversal) {
  /*
   * Set up this access path tree:
   *
   *                 NLJ1
   *               /      \
   *            NLJ2      NLJ3
   *            /  \      /   \
   *          HJ1  TS3  TS4   HJ2
   *         /  \             /  \
   *       TS1  TS2          TS5  TS6
   */

  AccessPath ts1 = MakeTableScan();
  AccessPath ts2 = MakeTableScan();
  AccessPath ts3 = MakeTableScan();
  AccessPath ts4 = MakeTableScan();
  AccessPath ts5 = MakeTableScan();
  AccessPath ts6 = MakeTableScan();

  AccessPath hj1 = MakeHashJoin(&ts1, &ts2);
  AccessPath hj2 = MakeHashJoin(&ts5, &ts6);

  AccessPath nlj2 = MakeNestedLoopJoin(&hj1, &ts3);
  AccessPath nlj3 = MakeNestedLoopJoin(&ts4, &hj2);
  AccessPath nlj1 = MakeNestedLoopJoin(&nlj2, &nlj3);

  vector<AccessPath *> paths;
  WalkAccessPaths(&nlj1, nullptr, WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [&paths](AccessPath *path, const JOIN *) {
                    paths.push_back(path);
                    // Skip subtrees under a hash join.
                    return path->type == AccessPath::HASH_JOIN;
                  });

  // Expect the tree to have been walked depth-first and pre-order, and
  // everything below a hash join path was skipped.
  EXPECT_THAT(paths, ElementsAre(&nlj1, &nlj2, &hj1, &ts3, &nlj3, &ts4, &hj2));
}

TEST(WalkAccessPathsTest, PostOrderTraversal) {
  /*
   * Set up this access path tree:
   *
   *                 NLJ1
   *               /      \
   *            NLJ2      NLJ3
   *            /  \      /   \
   *          HJ1  TS3  TS4   HJ2
   *         /  \             /  \
   *       TS1  TS2          TS5  TS6
   */

  AccessPath ts1 = MakeTableScan();
  AccessPath ts2 = MakeTableScan();
  AccessPath ts3 = MakeTableScan();
  AccessPath ts4 = MakeTableScan();
  AccessPath ts5 = MakeTableScan();
  AccessPath ts6 = MakeTableScan();

  AccessPath hj1 = MakeHashJoin(&ts1, &ts2);
  AccessPath hj2 = MakeHashJoin(&ts5, &ts6);

  AccessPath nlj2 = MakeNestedLoopJoin(&hj1, &ts3);
  AccessPath nlj3 = MakeNestedLoopJoin(&ts4, &hj2);
  AccessPath nlj1 = MakeNestedLoopJoin(&nlj2, &nlj3);

  vector<AccessPath *> paths;
  WalkAccessPaths(
      &nlj1, nullptr, WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
      [&paths](AccessPath *path, const JOIN *) {
        paths.push_back(path);
        // The return value is ignored when doing post-order traversal.
        return path->type == AccessPath::HASH_JOIN;
      },
      /*post_order_traversal=*/true);

  // Expect the tree to have been walked depth-first and post-order. No subtrees
  // are cut off for post-order traversal, so we see all paths. (Because we've
  // already finished processing the subtree when the functor is called, its
  // returning true does not prevent us recursing into the subtree.)
  EXPECT_THAT(paths, ElementsAre(&ts1, &ts2, &hj1, &ts3, &nlj2, &ts4, &ts5,
                                 &ts6, &hj2, &nlj3, &nlj1));
}

}  // namespace walk_access_paths_test
