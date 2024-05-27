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

#include <initializer_list>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "my_alloc.h"  // MEM_ROOT
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/mem_root_array.h"
#include "sql/table.h"

class JOIN;

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

AccessPath MakeTableScan(TABLE *table = nullptr) {
  AccessPath path;
  path.type = AccessPath::TABLE_SCAN;
  path.table_scan().table = table;
  return path;
}

AccessPath MakeZeroRows(AccessPath *child) {
  AccessPath path;
  path.type = AccessPath::ZERO_ROWS;
  path.zero_rows().child = child;
  return path;
}

AccessPath MakeAppend(MEM_ROOT *mem_root, AccessPath *c1, AccessPath *c2) {
  AccessPath path;
  path.type = AccessPath::APPEND;

  Mem_root_array<AppendPathParameters> *params =
      new (mem_root) Mem_root_array<AppendPathParameters>(mem_root);
  AppendPathParameters param1, param2;
  param1.path = c1;
  param2.path = c2;
  params->push_back(param1);
  params->push_back(param2);

  path.append().children = params;
  return path;
}

AccessPath MakeStream(TABLE *table, AccessPath *child) {
  AccessPath path;
  path.type = AccessPath::STREAM;
  path.stream().child = child;
  path.stream().table = table;
  return path;
}

AccessPath MakeMaterialize(MEM_ROOT *mem_root, TABLE *table,
                           AccessPath *subquery_path) {
  AccessPath path;
  path.type = AccessPath::MATERIALIZE;
  path.materialize().table_path =
      new (mem_root) AccessPath(MakeTableScan(table));

  MaterializePathParameters *param = new (mem_root) MaterializePathParameters;
  param->m_operands.init(mem_root);
  param->m_operands.emplace_back();
  param->m_operands.back().subquery_path = subquery_path;
  param->table = table;
  path.materialize().param = param;

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
      &nlj1, /*join=*/nullptr, WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
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
  EXPECT_THAT(paths, ElementsAre(&ts2, &ts1, &hj1, &ts3, &nlj2, &ts4, &ts6,
                                 &ts5, &hj2, &nlj3, &nlj1));
}

TEST(WalkAccessPathsTest, ZeroRows) {
  /*
   * Set up this access path tree:
   *
   *                 NLJ1
   *               /      \
   *           NLJ2        NLJ3
   *          /    \      /    \
   *        TS1   ZERO  TS2    TS3
   *                |
   *              NLJ4
   *             /    \
   *           TS4    TS5
   */

  TABLE t1;
  TABLE t2;
  TABLE t3;
  TABLE t4;
  TABLE t5;

  AccessPath ts1 = MakeTableScan(&t1);
  AccessPath ts2 = MakeTableScan(&t2);
  AccessPath ts3 = MakeTableScan(&t3);
  AccessPath ts4 = MakeTableScan(&t4);
  AccessPath ts5 = MakeTableScan(&t5);

  AccessPath nlj4 = MakeNestedLoopJoin(&ts4, &ts5);
  AccessPath zero = MakeZeroRows(&nlj4);
  AccessPath nlj2 = MakeNestedLoopJoin(&ts1, &zero);
  AccessPath nlj3 = MakeNestedLoopJoin(&ts2, &ts3);
  AccessPath nlj1 = MakeNestedLoopJoin(&nlj2, &nlj3);

  // WalkAccessPaths() should not see the paths below the ZERO_ROWS access path.
  {
    vector<AccessPath *> paths;
    WalkAccessPaths(
        &nlj1, /*join=*/nullptr, WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
        [&paths](AccessPath *path, const JOIN *) {
          paths.push_back(path);
          // The return value is ignored when doing post-order traversal.
          return path->type == AccessPath::HASH_JOIN;
        },
        /*post_order_traversal=*/false);
    EXPECT_THAT(paths,
                ElementsAre(&nlj1, &nlj2, &ts1, &zero, &nlj3, &ts2, &ts3));
  }

  // WalkTablesUnderAccessPath() should see all tables when called with
  // include_pruned_tables = true.
  {
    vector<TABLE *> tables;
    WalkTablesUnderAccessPath(
        &nlj1,
        [&tables](TABLE *table) {
          tables.push_back(table);
          return false;
        },
        /*include_pruned_tables=*/true);
    EXPECT_THAT(tables, ElementsAre(&t1, &t4, &t5, &t2, &t3));
  }

  // WalkTablesUnderAccessPath() should not see tables under ZERO_ROWS when
  // called with include_pruned_tables = false.
  {
    vector<TABLE *> tables;
    WalkTablesUnderAccessPath(
        &nlj1,
        [&tables](TABLE *table) {
          tables.push_back(table);
          return false;
        },
        /*include_pruned_tables=*/false);
    EXPECT_THAT(tables, ElementsAre(&t1, &t2, &t3));
  }
}

TEST(WalkAccessPathsTest, ZeroRowsNoChild) {
  AccessPath zero_path = MakeZeroRows(/*child=*/nullptr);

  vector<AccessPath *> paths;
  WalkAccessPaths(&zero_path, /*join=*/nullptr,
                  WalkAccessPathPolicy::ENTIRE_TREE,
                  [&paths](AccessPath *path, const JOIN *) {
                    paths.push_back(path);
                    return false;
                  });
  EXPECT_THAT(paths, ElementsAre(&zero_path));

  for (bool include_pruned_tables : {true, false}) {
    WalkTablesUnderAccessPath(
        &zero_path,
        [](TABLE *) {
          EXPECT_TRUE(false);
          return true;
        },
        include_pruned_tables);
  }
}

TEST(WalkAccessPathsTest, Append) {
  /*
   * Set up this access path tree:
   *
   *                APPEND
   *                /    \
   *              TS1    TS2
   */

  TABLE t1;
  TABLE t2;

  AccessPath ts1 = MakeTableScan(&t1);
  AccessPath ts2 = MakeTableScan(&t2);

  MEM_ROOT mem_root(PSI_NOT_INSTRUMENTED, 1024);
  AccessPath append = MakeAppend(&mem_root, &ts1, &ts2);

  vector<AccessPath *> paths;
  WalkAccessPaths(&append, /*join=*/nullptr, WalkAccessPathPolicy::ENTIRE_TREE,
                  [&paths](AccessPath *path, const JOIN *) {
                    paths.push_back(path);
                    return false;
                  });
  EXPECT_THAT(paths, ElementsAre(&append, &ts1, &ts2));
  paths.clear();

  WalkAccessPaths(&append, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [&paths](AccessPath *path, const JOIN *) {
                    paths.push_back(path);
                    return false;
                  });
  EXPECT_THAT(paths, ElementsAre(&append));
}

TEST(WalkAccessPathsTest, TemptableAggregate) {
  AccessPath ts1 = MakeTableScan();
  AccessPath ts2 = MakeTableScan();
  AccessPath temptable_aggregate;
  temptable_aggregate.type = AccessPath::TEMPTABLE_AGGREGATE;
  temptable_aggregate.temptable_aggregate().subquery_path = &ts1;
  temptable_aggregate.temptable_aggregate().table_path = &ts2;

  vector<AccessPath *> paths;
  WalkAccessPaths(&temptable_aggregate, /*join=*/nullptr,
                  WalkAccessPathPolicy::ENTIRE_TREE,
                  [&paths](AccessPath *path, const JOIN *) {
                    paths.push_back(path);
                    return false;
                  });
  EXPECT_THAT(paths, ElementsAre(&temptable_aggregate, &ts1, &ts2));
  paths.clear();

  WalkAccessPaths(&temptable_aggregate, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [&paths](AccessPath *path, const JOIN *) {
                    paths.push_back(path);
                    return false;
                  });
  // Should not traverse the subquery_path.
  EXPECT_THAT(paths, ElementsAre(&temptable_aggregate, &ts2));
}

TEST(WalkAccessPathsTest, PushedJoinRef) {
  TABLE t1;
  AccessPath pushed_join_ref;
  pushed_join_ref.type = AccessPath::PUSHED_JOIN_REF;
  pushed_join_ref.pushed_join_ref().table = &t1;
  for (bool include_pruned_tables : {true, false}) {
    vector<TABLE *> tables;
    WalkTablesUnderAccessPath(
        &pushed_join_ref,
        [&tables](TABLE *table) {
          tables.push_back(table);
          return false;
        },
        include_pruned_tables);
    EXPECT_THAT(tables, ElementsAre(&t1));
  }
}

TEST(WalkAccessPathsTest, MaterializedTables) {
  MEM_ROOT mem_root{PSI_NOT_INSTRUMENTED, 1024};

  TABLE t1;
  TABLE t2;
  TABLE tmp1;
  TABLE tmp2;

  AccessPath ts1 = MakeTableScan(&t1);
  AccessPath ts2 = MakeTableScan(&t2);

  AccessPath lhs = MakeStream(&tmp1, &ts1);
  AccessPath rhs = MakeMaterialize(&mem_root, &tmp2, &ts2);
  AccessPath join = MakeNestedLoopJoin(&lhs, &rhs);

  /* We have this access path tree:
   *
   *           NESTED_LOOP_JOIN
   *                /   \
   *       STREAM(tmp1) MATERIALIZE(tmp2)
   *              /       \
   *   TABLE_SCAN(t1)   TABLE_SCAN(t2)
   *
   * WalkTablesUnderAccessPath() should see each of the temporary tables (tmp1
   * and tmp2) once, and none of the base tables (t1 and t2). It used to see
   * tmp2 twice due to bug#36190386.
   */

  for (bool include_pruned_tables : {true, false}) {
    vector<const TABLE *> tables;
    WalkTablesUnderAccessPath(
        &join,
        [&tables](const TABLE *table) {
          tables.push_back(table);
          return false;
        },
        include_pruned_tables);
    EXPECT_THAT(tables, ElementsAre(&tmp1, &tmp2));
  }
}

}  // namespace walk_access_paths_test
