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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <math.h>
#include <string.h>

#include <initializer_list>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/field.h"
#include "sql/filesort.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/common_subexpression_elimination.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/subgraph_enumeration.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/sort_param.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/visible_fields.h"
#include "template_utils.h"
#include "temptable/mock_field_varstring.h"
#include "unittest/gunit/base_mock_field.h"
#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/handler-t.h"
#include "unittest/gunit/mock_field_datetime.h"
#include "unittest/gunit/mock_field_long.h"
#include "unittest/gunit/optimizer_test.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/test_utils.h"

using hypergraph::NodeMap;
using my_testing::Server_initializer;
using std::string;
using std::string_view;
using std::to_string;
using std::unordered_map;
using std::vector;
using testing::_;
using testing::ElementsAre;
using testing::Pair;
using testing::Return;
using testing::StartsWith;
using testing::UnorderedElementsAre;
using namespace std::literals;  // For operator""sv.

static AccessPath *FindBestQueryPlanAndFinalize(THD *thd,
                                                Query_block *query_block,
                                                string *trace) {
  AccessPath *path = FindBestQueryPlan(thd, query_block, trace);
  if (path != nullptr) {
    query_block->join->set_root_access_path(path);
    EXPECT_FALSE(FinalizePlanForQueryBlock(thd, query_block));
  }
  return path;
}

namespace {

/// An error checker which, upon destruction, verifies that a single error was
/// raised while the checker was alive, and that the error had the expected
/// error number. If an error is raised, the THD::is_error() flag will be set,
/// just as in the server. (The default error_handler_hook used by the unit
/// tests, does not set the error flag in the THD.) If expected_errno is 0, it
/// will instead check that no error was raised.
class ErrorChecker {
 public:
  ErrorChecker(const THD *thd, unsigned expected_errno)
      : m_thd(thd),
        m_errno(expected_errno),
        m_saved_error_hook(error_handler_hook) {
    // Use an error handler which sets the THD::is_error() flag.
    error_handler_hook = my_message_sql;
    EXPECT_FALSE(thd->is_error());
  }

  ~ErrorChecker() {
    error_handler_hook = m_saved_error_hook;
    if (m_errno != 0) {
      EXPECT_TRUE(m_thd->is_error());
      EXPECT_EQ(m_errno, m_thd->get_stmt_da()->mysql_errno());
      EXPECT_EQ(1, m_thd->get_stmt_da()->current_statement_cond_count());
    } else {
      EXPECT_FALSE(m_thd->is_error());
    }
  }

 private:
  const THD *m_thd;
  unsigned m_errno;
  ErrorHandlerFunctionPointer m_saved_error_hook;
};

// Sort the nodes in the given graph by name, which makes the test
// a bit more robust against irrelevant changes. Note that we don't
// sort edges, since it's often useful to correlate the code with
// the Graphviz output in the optimizer trace, which isn't sorted.
void SortNodes(JoinHypergraph *graph) {
  // Sort nodes (by alias). We sort a series of indexes first the same way
  // so that we know which went where.
  std::vector<int> node_order;
  for (unsigned i = 0; i < graph->nodes.size(); ++i) {
    node_order.push_back(i);
  }
  std::sort(node_order.begin(), node_order.end(), [graph](int a, int b) {
    return strcmp(graph->nodes[a].table->alias, graph->nodes[b].table->alias) <
           0;
  });
  std::sort(graph->nodes.begin(), graph->nodes.end(),
            [](const JoinHypergraph::Node &a, const JoinHypergraph::Node &b) {
              return strcmp(a.table->alias, b.table->alias) < 0;
            });

  // Remap hyperedges to the new node indexes. Note that we don't
  // change the neighborhood, because nothing in these tests need it.
  int node_map[MAX_TABLES];
  for (unsigned i = 0; i < graph->nodes.size(); ++i) {
    node_map[node_order[i]] = i;
  }
  for (hypergraph::Hyperedge &edge : graph->graph.edges) {
    NodeMap new_left = 0, new_right = 0;
    for (int node_idx : BitsSetIn(edge.left)) {
      new_left |= NodeMap{1} << node_map[node_idx];
    }
    for (int node_idx : BitsSetIn(edge.right)) {
      new_right |= NodeMap{1} << node_map[node_idx];
    }
    edge.left = new_left;
    edge.right = new_right;
  }

  // Remap TES.
  for (Predicate &pred : graph->predicates) {
    NodeMap new_tes = 0;
    for (int node_idx : BitsSetIn(pred.total_eligibility_set)) {
      new_tes |= NodeMap{1} << node_map[node_idx];
    }
    pred.total_eligibility_set = new_tes;
  }
}

}  // namespace

using MakeHypergraphTest = OptimizerTestBase;

TEST_F(MakeHypergraphTest, SingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1", /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(1, graph.nodes.size());
  EXPECT_EQ(0, graph.edges.size());
  EXPECT_EQ(0, graph.predicates.size());

  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
}

TEST_F(MakeHypergraphTest, InnerJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // Simple edges; order doesn't matter.
  ASSERT_EQ(2, graph.edges.size());

  // t1/t2. There is no index information, so the default 0.1 should be used.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x02, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[1].selectivity);

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[0].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, OuterJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON t2.y=t3.y) ON t1.x=t2.x",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // Hyperedges. Order doesn't matter.
  ASSERT_EQ(2, graph.edges.size());

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[0].selectivity);

  // t1/t2; since the predicate is null-rejecting on t2, we can rewrite.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x02, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, OuterJoinNonNullRejecting) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON t2.y=t3.y OR t2.y "
      "IS NULL) ON t1.x=t2.x",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // Hyperedges. Order doesn't matter.
  ASSERT_EQ(2, graph.edges.size());

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(1.0F - (0.9F * 0.9F),
                  graph.edges[0].selectivity);  // OR of two conditions.

  // t1/{t2,t3}; the predicate is not null-rejecting (unlike the previous test),
  // so we need the full hyperedge.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, SemiJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2 JOIN t3 ON "
      "t2.y=t3.y)",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // Hyperedges. Order doesn't matter.
  ASSERT_EQ(2, graph.edges.size());

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[0].selectivity);

  // t1/{t2,t3}.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::SEMIJOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, AntiJoin) {
  // NOTE: Fields must be non-nullable, or NOT IN can not be rewritten.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x NOT IN (SELECT t2.x FROM t2 JOIN t3 ON "
      "t2.y=t3.y)",
      /*nullable=*/false);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // Hyperedges. Order doesn't matter.
  ASSERT_EQ(2, graph.edges.size());

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[0].selectivity);

  // t1/{t2,t3}.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::ANTIJOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, Predicates) {
  // The OR ... IS NULL part is to keep the LEFT JOIN from being simplified
  // to an inner join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x=t2.x "
      "WHERE t1.x=2 AND (t2.y=3 OR t2.y IS NULL)",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(2, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);

  // t1/t2.
  ASSERT_EQ(1, graph.edges.size());
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(0.1F, graph.edges[0].selectivity);

  ASSERT_EQ(2, graph.predicates.size());
  EXPECT_EQ("(t1.x = 2)", ItemToString(graph.predicates[0].condition));
  EXPECT_EQ(0x01, graph.predicates[0].total_eligibility_set);  // Only t1.
  EXPECT_FLOAT_EQ(0.1F,
                  graph.predicates[0].selectivity);  // No specific information.

  EXPECT_EQ("((t2.y = 3) or (t2.y is null))",
            ItemToString(graph.predicates[1].condition));
  EXPECT_GT(graph.predicates[1].selectivity,
            0.1);  // More common due to the OR NULL.
  EXPECT_EQ(0x03,
            graph.predicates[1].total_eligibility_set);  // Both t1 and t2!
}

TEST_F(MakeHypergraphTest, PushdownFromOuterJoinCondition) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 JOIN t3) "
      "ON t1.x=t2.x AND t2.y=t3.y AND t3.z > 3",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t2/t3.
  ASSERT_EQ(2, graph.edges.size());
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  EXPECT_EQ(0, graph.edges[0].expr->join_conditions.size());
  ASSERT_EQ(1, graph.edges[0].expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.y = t3.y)",
            ItemToString(graph.edges[0].expr->equijoin_conditions[0]));

  // t1/(t2,t3).
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[1].expr->type);
  ASSERT_EQ(0, graph.edges[1].expr->join_conditions.size());
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)",
            ItemToString(graph.edges[1].expr->equijoin_conditions[0]));

  // The z > 3 condition should be pushed all the way down to a predicate.
  ASSERT_EQ(1, graph.predicates.size());
  EXPECT_EQ("(t3.z > 3)", ItemToString(graph.predicates[0].condition));
  EXPECT_EQ(0x04, graph.predicates[0].total_eligibility_set);  // Only t3.
}

// See also the PredicatePushdown* tests below.
TEST_F(MakeHypergraphTest, AssociativeRewriteToImprovePushdown) {
  // Note that the WHERE condition needs _both_ associativity and
  // commutativity to become a proper join condition (t2 needs to be
  // pulled out; doing t1 instead would create a degenerate join).
  // The IS NULL is to keep the left join from being converted
  // into an inner join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM (t1 JOIN t2 ON TRUE) LEFT JOIN t3 ON TRUE "
      "WHERE t2.x=t3.x OR t3.x IS NULL",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t2", graph.nodes[0].table->alias);
  EXPECT_STREQ("t1", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t1/t3.
  ASSERT_EQ(2, graph.edges.size());
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[0].expr->type);
  EXPECT_EQ(0, graph.edges[0].expr->join_conditions.size());
  EXPECT_FLOAT_EQ(1.0F, graph.edges[0].selectivity);

  // t2/{t1,t3}. This join should also carry the predicate.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);
  EXPECT_EQ(1, graph.edges[1].expr->join_conditions.size());
  EXPECT_FLOAT_EQ(1.0F, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, Cycle) {
  // If == is outer join and -- is inner join:
  //
  // t6 == t1 -- t2 -- t4 == t5
  //        |  /
  //        | /
  //       t3
  //
  // Note that t6 is on the _left_ side of the inner join, so we should be able
  // to push down conditions to it.

  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM "
      "((t1,t2,t3,t4) LEFT JOIN t5 ON t4.x=t5.x) LEFT JOIN t6 ON t1.x=t6.x "
      "WHERE t1.x=t2.x AND t2.x=t3.x AND t1.x=t3.x AND t2.x=t4.x",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(6, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);
  EXPECT_STREQ("t5", graph.nodes[4].table->alias);
  EXPECT_STREQ("t6", graph.nodes[5].table->alias);

  // t1/t2.
  ASSERT_EQ(6, graph.edges.size());
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);

  // t2/t3.
  EXPECT_EQ(0x04, graph.graph.edges[2].left);
  EXPECT_EQ(0x02, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);

  // t4/t2.
  EXPECT_EQ(0x08, graph.graph.edges[4].left);
  EXPECT_EQ(0x02, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[2].expr->type);

  // t4/t5.
  EXPECT_EQ(0x08, graph.graph.edges[6].left);
  EXPECT_EQ(0x10, graph.graph.edges[6].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[3].expr->type);

  // t1/t6.
  EXPECT_EQ(0x01, graph.graph.edges[8].left);
  EXPECT_EQ(0x20, graph.graph.edges[8].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[4].expr->type);

  // t3/t1; added last because it completes a cycle.
  EXPECT_EQ(0x04, graph.graph.edges[10].left);
  EXPECT_EQ(0x01, graph.graph.edges[10].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[5].expr->type);

  // The three predicates from the cycle should be added, but no others.
  // The TES should be equivalent to the SES, ie., the outer joins should
  // not influence this.
  ASSERT_EQ(3, graph.predicates.size());

  EXPECT_EQ("(t1.x = t2.x)", ItemToString(graph.predicates[0].condition));
  EXPECT_EQ(0x03, graph.predicates[0].total_eligibility_set);  // t1/t2.
  EXPECT_TRUE(graph.predicates[0].was_join_condition);

  EXPECT_EQ("(t2.x = t3.x)", ItemToString(graph.predicates[1].condition));
  EXPECT_EQ(0x06, graph.predicates[1].total_eligibility_set);  // t2/t3.
  EXPECT_TRUE(graph.predicates[1].was_join_condition);

  EXPECT_EQ("(t1.x = t3.x)", ItemToString(graph.predicates[2].condition));
  EXPECT_EQ(0x05, graph.predicates[2].total_eligibility_set);  // t1/t3.
  EXPECT_TRUE(graph.predicates[2].was_join_condition);
}

TEST_F(MakeHypergraphTest, NoCycleBelowOuterJoin) {
  // The OR ... IS NULL part is to keep the LEFT JOIN from being simplified
  // to an inner join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2,t3,t4) ON t1.x=t2.x "
      "WHERE (t2.x=t3.x OR t2.x IS NULL) "
      "AND (t3.x=t4.x OR t3.x IS NULL) "
      "AND (t4.x=t2.x OR t4.x IS NULL)",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(4, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);

  // t2/t3.
  ASSERT_EQ(3, graph.edges.size());
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);

  // {t2,t3}/t4 (due to the Cartesian product).
  EXPECT_EQ(0x06, graph.graph.edges[2].left);
  EXPECT_EQ(0x08, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);

  // t1/{t2,t3,t4} (the outer join).
  EXPECT_EQ(0x01, graph.graph.edges[4].left);
  EXPECT_EQ(0x0e, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[2].expr->type);

  // The three predicates are still there; no extra predicates due to cycles.
  EXPECT_EQ(3, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, CyclePushedFromOuterJoinCondition) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM "
      "t1 LEFT JOIN (t2 JOIN (t3 JOIN t4 ON t3.x=t4.x) ON t2.x=t3.x) "
      "ON t1.x=t2.x AND t2.x=t4.x",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(4, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);

  // t3/t2.
  ASSERT_EQ(4, graph.edges.size());
  EXPECT_EQ(0x04, graph.graph.edges[2].left);
  EXPECT_EQ(0x02, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);

  // t2/t4 (pushed from the ON condition).
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x08, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);

  // t1/{t2,t3,t4} (the outer join).
  EXPECT_EQ(0x01, graph.graph.edges[4].left);
  EXPECT_EQ(0x0e, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[2].expr->type);

  // t3/t4; added last because it completes a cycle.
  EXPECT_EQ(0x04, graph.graph.edges[6].left);
  EXPECT_EQ(0x08, graph.graph.edges[6].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[3].expr->type);

  // The three predicates from the cycle should be added, but no others.
  // The TES should be equivalent to the SES, ie., the outer joins should
  // not influence this.
  ASSERT_EQ(3, graph.predicates.size());

  EXPECT_EQ("(t2.x = t3.x)", ItemToString(graph.predicates[1].condition));
  EXPECT_EQ(0x06, graph.predicates[1].total_eligibility_set);  // t2/t3.
  EXPECT_TRUE(graph.predicates[1].was_join_condition);

  EXPECT_EQ("(t2.x = t4.x)", ItemToString(graph.predicates[0].condition));
  EXPECT_EQ(0x0a, graph.predicates[0].total_eligibility_set);  // t2/t4.
  EXPECT_TRUE(graph.predicates[0].was_join_condition);

  EXPECT_EQ("(t3.x = t4.x)", ItemToString(graph.predicates[2].condition));
  EXPECT_EQ(0x0c, graph.predicates[2].total_eligibility_set);  // t3/t4.
  EXPECT_TRUE(graph.predicates[2].was_join_condition);
}

TEST_F(MakeHypergraphTest, CycleWithNullSafeEqual) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3 WHERE "
      "t1.x <=> t2.x AND t2.y <=> t3.y AND t1.z <=> t3.z",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  // Expect a hypergraph of three nodes, and one simple edge connecting each
  // pair of nodes.
  EXPECT_EQ(3, graph.nodes.size());
  EXPECT_EQ(3, graph.edges.size());

  // All the edges should have equijoin conditions.
  vector<string> predicates;
  for (const JoinPredicate &predicate : graph.edges) {
    const RelationalExpression *expr = predicate.expr;
    EXPECT_TRUE(expr->join_conditions.empty());
    ASSERT_EQ(1, expr->equijoin_conditions.size());
    predicates.push_back(ItemToString(expr->equijoin_conditions[0]));
  }
  EXPECT_THAT(predicates,
              UnorderedElementsAre("(t1.x <=> t2.x)", "(t2.y <=> t3.y)",
                                   "(t1.z <=> t3.z)"));
}

TEST_F(MakeHypergraphTest, MultipleEqualitiesCauseCycle) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x",
                      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t1/t2.
  ASSERT_EQ(3, graph.edges.size());
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[2].left);
  EXPECT_EQ(0x04, graph.graph.edges[2].right);

  // t1/t3 (the cycle edge).
  EXPECT_EQ(0x01, graph.graph.edges[4].left);
  EXPECT_EQ(0x04, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[2].expr->type);
}

TEST_F(MakeHypergraphTest, CyclesGetConsistentSelectivities) {
  // Same setup as MultipleEqualitiesCauseCycle, but with an index on t1.x.
  // The information we get from t1=t2 should also be used for t2=t3,
  // due to the multiple equality.
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x",
                      /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);
  ulong rec_per_key_int[] = {2};
  float rec_per_key[] = {2.0f};
  t1->key_info[0].set_rec_per_key_array(rec_per_key_int, rec_per_key);
  t1->file->stats.records = 100;

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  ASSERT_EQ(3, graph.edges.size());
  EXPECT_FLOAT_EQ(0.02F, graph.edges[0].selectivity);
  EXPECT_FLOAT_EQ(0.02F, graph.edges[1].selectivity);
  EXPECT_FLOAT_EQ(0.02F, graph.edges[2].selectivity);
}

TEST_F(MakeHypergraphTest, MultiEqualityPredicateAppliedOnce) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3, t4 "
      "WHERE t1.x <> t4.y AND t4.z <> t3.y AND t2.z <> t3.x AND "
      "t2.x = t4.x AND t1.y = t2.x",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  ASSERT_EQ(4, graph.nodes.size());
  EXPECT_STREQ("t3", graph.nodes[0].table->alias);
  EXPECT_STREQ("t1", graph.nodes[1].table->alias);
  EXPECT_STREQ("t2", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);

  ASSERT_EQ(4, graph.edges.size());

  // t1/t2: t1.y = t2.x
  EXPECT_EQ(TableBitmap(1), graph.graph.edges[0].left);
  EXPECT_EQ(TableBitmap(2), graph.graph.edges[0].right);
  EXPECT_FLOAT_EQ(COND_FILTER_EQUALITY, graph.edges[0].selectivity);

  // t1/t4: (t1.y = t4.x) and (t1.x <> t4.y)
  EXPECT_EQ(TableBitmap(1), graph.graph.edges[2].left);
  EXPECT_EQ(TableBitmap(3), graph.graph.edges[2].right);
  // Used to apply the equality predicate twice. Once as t1.y = t4.x and
  // once as t4.x = t1.y. Verify that it's applied once now.
  EXPECT_FLOAT_EQ(COND_FILTER_EQUALITY * (1.0f - COND_FILTER_EQUALITY),
                  graph.edges[1].selectivity);

  // t3/t2t4: (t4.z <> t3.y) AND (t2.z <> t3.x)
  EXPECT_EQ(TableBitmap(0), graph.graph.edges[4].left);
  EXPECT_EQ(TableBitmap(2) | TableBitmap(3), graph.graph.edges[4].right);
  EXPECT_FLOAT_EQ((1.0f - COND_FILTER_EQUALITY) * (1.0f - COND_FILTER_EQUALITY),
                  graph.edges[2].selectivity);

  // t2/t4: t2.x = t4.x
  EXPECT_EQ(TableBitmap(2), graph.graph.edges[6].left);
  EXPECT_EQ(TableBitmap(3), graph.graph.edges[6].right);
  EXPECT_FLOAT_EQ(COND_FILTER_EQUALITY, graph.edges[3].selectivity);
}

TEST_F(MakeHypergraphTest, MultiEqualityPredicateNoRedundantJoinCondition) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, (t3 LEFT JOIN t4 ON t3.x = t4.x), t5 "
      "WHERE t2.x = t3.x AND t3.x = t5.x AND t3.x = t3.y AND t1.y <> t5.y",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  ASSERT_EQ(5, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);
  EXPECT_STREQ("t5", graph.nodes[4].table->alias);

  EXPECT_EQ(6, graph.edges.size());

  // Find the edge between t2 and t3.
  int t2_t3_edge_idx = -1;
  for (size_t i = 0; i < graph.graph.edges.size(); ++i) {
    if (graph.graph.edges[i].left == TableBitmap(1) &&
        graph.graph.edges[i].right == TableBitmap(2)) {
      t2_t3_edge_idx = i / 2;
      break;
    }
  }
  ASSERT_NE(-1, t2_t3_edge_idx);

  // Check the condition on the edge. It should be a single equality predicate;
  // either t2.x = t3.x or t2.x = t3.y. It used to have both predicates, and
  // therefore double-count the selectivity. (Having one of the predicates is
  // enough, because t3.x = t3.y will always be applied as a table predicate and
  // make the other join predicate redundant.)
  const JoinPredicate &predicate = graph.edges[t2_t3_edge_idx];
  EXPECT_TRUE(predicate.expr->join_conditions.empty());
  EXPECT_EQ(1, predicate.expr->equijoin_conditions.size());
  EXPECT_FLOAT_EQ(COND_FILTER_EQUALITY, predicate.selectivity);
}

TEST_F(MakeHypergraphTest, MultiEqualityPredicateNoRedundantJoinCondition2) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x = t2.x "
      "JOIN t3 LEFT JOIN t4 ON t3.x = t4.x "
      "JOIN t5 JOIN t6 ON t5.y = t6.x ON t5.x = t3.x ON t1.x = t6.x "
      "WHERE (t3.y IS NULL OR t6.y <> t4.y) AND t3.y <> t5.z",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  ASSERT_EQ(6, graph.nodes.size());
  SortNodes(&graph);
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);
  EXPECT_STREQ("t5", graph.nodes[4].table->alias);
  EXPECT_STREQ("t6", graph.nodes[5].table->alias);

  EXPECT_EQ(11, graph.edges.size());

  // Find the edge {t2,t3,t4}/{t6}
  int edge_idx = -1;
  for (size_t i = 0; i < graph.graph.edges.size(); ++i) {
    if (graph.graph.edges[i].left == TablesBetween(1, 4) &&
        graph.graph.edges[i].right == TableBitmap(5)) {
      edge_idx = i / 2;
      break;
    }
  }
  ASSERT_NE(-1, edge_idx);

  // Check the condition on the edge. In addition to a non-equijoin condition
  // for the OR predicate, it should contain a single equijoin condition. It
  // happens to be t2.x=t6.x, but it could equally well have been t1.x=t6.x.
  // Because of multiple equalities, t1.x=t2.x will already have been applied on
  // the {t1,t2,t3,t4} subplan, and t1.x=t6.x is implied by t1.x=t2.x and
  // t2.x=t6.x. The main point of this test case is to verify that this edge
  // contains only one of those two equijoin conditions, and that its
  // selectivity is not double-counted.
  const JoinPredicate &predicate = graph.edges[edge_idx];
  ASSERT_EQ(1, predicate.expr->join_conditions.size());
  EXPECT_EQ("((t3.y is null) or (t6.y <> t4.y))",
            ItemToString(predicate.expr->join_conditions[0]));
  ASSERT_EQ(1, predicate.expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.x = t6.x)",
            ItemToString(predicate.expr->equijoin_conditions[0]));
  EXPECT_FLOAT_EQ(
      COND_FILTER_ALLPASS          // selectivity of non-equijoin condition
          * COND_FILTER_EQUALITY,  // selectivity of a single equijoin condition
      predicate.selectivity);
}

TEST_F(MakeHypergraphTest, ConflictRulesWithManyTables) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 JOIN t3 LEFT JOIN t4"
      " ON t4.y=t1.y WHERE t2.x = t1.x "
      "AND EXISTS (SELECT 1 FROM t5 WHERE t5.x=t1.x)",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  SortNodes(&graph);
  ASSERT_EQ(5, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);
  EXPECT_STREQ("t5", graph.nodes[4].table->alias);

  for (const JoinPredicate &pred : graph.edges) {
    // We are not interested in the plan. However, while generating
    // conflict rules, earlier it would wrongly place the conflict
    // rule {t4}->{t3} for the edge t1->t5. This was because it
    // was using table_map instead of NodeMap to determine the rule.
    EXPECT_EQ(0, pred.expr->conflict_rules.size());
  }
}

TEST_F(MakeHypergraphTest, HyperpredicatesDoNotBlockExtraCycleEdges) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 "
      "FROM t1 JOIN t2 ON t1.x = t2.x JOIN t3 ON t1.y = t3.y "
      "WHERE t1.z = 0 OR t2.z = 0 OR t3.z = 0",
      /*nullable=*/true);

  // Build (trivial!) multiple equalities from the ON conditions.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t2", graph.nodes[0].table->alias);
  EXPECT_STREQ("t1", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t1/t3.
  ASSERT_EQ(3, graph.edges.size());
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);

  // {t1,t3}/t2. We don't really care how this hyperedge turns out,
  // but we _do_ care that its presence does not prevent a separate
  // t1-t2 edge from being added.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);

  // t1/t2. This edge didn't use to be added. But that effectively blocked
  // the join order (t1 JOIN t2) JOIN t3, which could be advantageous if
  // (t1 JOIN t3) had much higher cardinality than (t1 JOIN t2). So now we
  // want it to be there.
  EXPECT_EQ(0x02, graph.graph.edges[4].left);
  EXPECT_EQ(0x01, graph.graph.edges[4].right);
}

TEST_F(MakeHypergraphTest, Flattening) {
  // This query is impossible to push cleanly without flattening,
  // or adding broad hyperedges. We want to make sure we don't try to
  // “solve” it by pushing the t2.x = t3.x condition twice.
  // Due to flattening, we also don't get any Cartesian products.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN (t2 JOIN (t3 JOIN t4)) "
      "WHERE t1.y = t4.y AND t2.x = t3.x AND t3.x = t4.x",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  EXPECT_EQ("(multiple equal(t1.y, t4.y) and multiple equal(t2.x, t3.x, t4.x))",
            ItemToString(query_block->where_cond()));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(4, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);

  ASSERT_EQ(4, graph.edges.size());

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)",
            ItemToString(graph.edges[0].expr->equijoin_conditions[0]));

  // t1/t4.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x08, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.y = t4.y)",
            ItemToString(graph.edges[1].expr->equijoin_conditions[0]));

  // t3/t4.
  EXPECT_EQ(0x04, graph.graph.edges[4].left);
  EXPECT_EQ(0x08, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[2].expr->type);
  ASSERT_EQ(1, graph.edges[2].expr->equijoin_conditions.size());
  EXPECT_EQ("(t3.x = t4.x)",
            ItemToString(graph.edges[2].expr->equijoin_conditions[0]));

  // t2/t4.
  EXPECT_EQ(0x02, graph.graph.edges[6].left);
  EXPECT_EQ(0x08, graph.graph.edges[6].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[3].expr->type);
  ASSERT_EQ(1, graph.edges[3].expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.x = t4.x)",
            ItemToString(graph.edges[3].expr->equijoin_conditions[0]));
}

TEST_F(MakeHypergraphTest, PredicatePromotionOnMultipleEquals) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x AND t1.y=t3.y",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t1/t2.
  ASSERT_EQ(3, graph.edges.size());
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(0, graph.edges[0].expr->join_conditions.size());
  ASSERT_EQ(1, graph.edges[0].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)",
            ItemToString(graph.edges[0].expr->equijoin_conditions[0]));

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[2].left);
  EXPECT_EQ(0x04, graph.graph.edges[2].right);
  EXPECT_EQ(0, graph.edges[1].expr->join_conditions.size());
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)",
            ItemToString(graph.edges[1].expr->equijoin_conditions[0]));

  // t1/t3 (the cycle edge). Has both the original condition and the
  // multi-equality condition.
  EXPECT_EQ(0x01, graph.graph.edges[4].left);
  EXPECT_EQ(0x04, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[2].expr->type);
  EXPECT_EQ(0, graph.edges[2].expr->join_conditions.size());
  ASSERT_EQ(2, graph.edges[2].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.y = t3.y)",
            ItemToString(graph.edges[2].expr->equijoin_conditions[0]));
  EXPECT_EQ("(t1.x = t3.x)",
            ItemToString(graph.edges[2].expr->equijoin_conditions[1]));

  // Verify that the ones coming from the multi-equality are marked with
  // the same index, so that they are properly deduplicated.
  ASSERT_EQ(4, graph.predicates.size());

  EXPECT_EQ("(t1.x = t2.x)", ItemToString(graph.predicates[0].condition));
  EXPECT_TRUE(graph.predicates[0].was_join_condition);
  EXPECT_EQ(0, graph.predicates[0].source_multiple_equality_idx);

  EXPECT_EQ("(t2.x = t3.x)", ItemToString(graph.predicates[1].condition));
  EXPECT_TRUE(graph.predicates[1].was_join_condition);
  EXPECT_EQ(0, graph.predicates[1].source_multiple_equality_idx);

  EXPECT_EQ("(t1.y = t3.y)", ItemToString(graph.predicates[2].condition));
  EXPECT_TRUE(graph.predicates[2].was_join_condition);
  EXPECT_EQ(-1, graph.predicates[2].source_multiple_equality_idx);

  EXPECT_EQ("(t1.x = t3.x)", ItemToString(graph.predicates[3].condition));
  EXPECT_TRUE(graph.predicates[3].was_join_condition);
  EXPECT_EQ(0, graph.predicates[3].source_multiple_equality_idx);
}

// Verify that multiple equalities are properly resolved to a single equality,
// and not left as a multiple one. Antijoins have a similar issue.
// Inspired by issues in a larger query (DBT-3 Q21).
TEST_F(MakeHypergraphTest, MultipleEqualityPushedFromJoinConditions) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 "
      "WHERE t1.x=t2.x AND t1.x IN (SELECT t3.x FROM t3) ",
      /*nullable=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t1/t2.
  ASSERT_EQ(2, graph.edges.size());
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  ASSERT_EQ(1, graph.edges[0].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)",
            ItemToString(graph.edges[0].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[0].expr->join_conditions.size());

  // t2/t3 (semijoin). t1/t3 would also be fine. The really important part
  // is that we do not also have a t1/t2 or t1/t3 join conditions.
  EXPECT_EQ(0x02, graph.graph.edges[2].left);
  EXPECT_EQ(0x04, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::SEMIJOIN, graph.edges[1].expr->type);
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)",
            ItemToString(graph.edges[1].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[1].expr->join_conditions.size());
}

TEST_F(MakeHypergraphTest, UnpushableMultipleEqualityCausesCycle) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3, t4 "
      // Two simple equalities that set up a join structure.
      "WHERE t1.y=t2.y AND t2.z=t3.z "
      // And then a multi-equality that is not cleanly pushable onto that
      // structure.
      "AND t1.x=t3.x AND t3.x=t4.x",
      /*nullable=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(4, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);

  ASSERT_EQ(5, graph.edges.size());

  // t1/t2.
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  ASSERT_EQ(1, graph.edges[0].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.y = t2.y)",
            ItemToString(graph.edges[0].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[0].expr->join_conditions.size());

  // t3/t2.
  EXPECT_EQ(0x04, graph.graph.edges[2].left);
  EXPECT_EQ(0x02, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  EXPECT_EQ("(t2.z = t3.z)",
            ItemToString(graph.edges[1].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[1].expr->join_conditions.size());

  // t4/t3 (the first of many cycle edges from the multiple equality).
  EXPECT_EQ(0x08, graph.graph.edges[4].left);
  EXPECT_EQ(0x04, graph.graph.edges[4].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[2].expr->type);
  ASSERT_EQ(1, graph.edges[2].expr->equijoin_conditions.size());
  EXPECT_EQ("(t4.x = t3.x)",
            ItemToString(graph.edges[2].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[2].expr->join_conditions.size());

  // t3/t1 (cycle edge).
  EXPECT_EQ(0x04, graph.graph.edges[6].left);
  EXPECT_EQ(0x01, graph.graph.edges[6].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[3].expr->type);
  ASSERT_EQ(1, graph.edges[3].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t3.x)",
            ItemToString(graph.edges[3].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[3].expr->join_conditions.size());

  // t1/t4 (cycle edge within the cycle, comes from meshing).
  EXPECT_EQ(0x01, graph.graph.edges[8].left);
  EXPECT_EQ(0x08, graph.graph.edges[8].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[4].expr->type);
  ASSERT_EQ(1, graph.edges[4].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t4.x)",
            ItemToString(graph.edges[4].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[4].expr->join_conditions.size());
}

TEST_F(MakeHypergraphTest, UnpushableMultipleEqualityWithSameTableTwice) {
  // The (t2.y, t3.x, t3.y, t4.x) multi-equality is unpushable
  // due to the t1.z = t4.w equality that's already set up;
  // we need to create a cycle from t2/t3/t4, while still not losing
  // the t3.x = t3.y condition.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t1 AS t2, t1 AS t3, t1 AS t4 "
      "WHERE t1.z = t4.w "
      "AND t2.y = t3.x AND t3.x = t3.y AND t3.y = t4.x",
      /*nullable=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(4, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);
  EXPECT_STREQ("t4", graph.nodes[3].table->alias);

  ASSERT_EQ(4, graph.edges.size());

  // We only check that the given edges exist, and that we didn't lose
  // the t3.x = t3.y condition. All edges come from explicit
  // WHERE conditions.

  // t2/t3. Note that we get both t2.y=t3.y and t2.y=t3.x;
  // they come from the same multi-equality and we've already
  // checked t3.x=t3.y, so one is redundant, but we can't
  // figure this out yet.
  EXPECT_EQ(0x02, graph.graph.edges[0].left);
  EXPECT_EQ(0x04, graph.graph.edges[0].right);

  // t1/t4.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x08, graph.graph.edges[2].right);

  // t3/t4.
  EXPECT_EQ(0x04, graph.graph.edges[4].left);
  EXPECT_EQ(0x08, graph.graph.edges[4].right);

  // t2/t4.
  EXPECT_EQ(0x02, graph.graph.edges[6].left);
  EXPECT_EQ(0x08, graph.graph.edges[6].right);

  bool found_predicate = false;
  for (const Predicate &pred : graph.predicates) {
    if (ItemToString(pred.condition) == "(t3.x = t3.y)") {
      found_predicate = true;
    }
  }
  EXPECT_TRUE(found_predicate);
}

TEST_F(MakeHypergraphTest, EqualityPropagationExpandsTopConjunction) {
  // The WHERE clause of the query is a subjunction in which the second leg is
  // found to be always false during equality propagation and removed.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE "
      "(t1.x = t2.x AND t1.x < 10) OR (t1.y = t2.y AND t1.y < t2.y)",
      /*nullable=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  SortNodes(&graph);

  ASSERT_EQ(2, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);

  // Expect to find a simple equijoin condition and a table filter. The table
  // filter used to be part of the join condition, but it should not be.
  ASSERT_EQ(1, graph.edges.size());
  EXPECT_EQ("(t1.x = t2.x)",
            ItemsToString(graph.edges[0].expr->equijoin_conditions));
  EXPECT_EQ("(none)", ItemsToString(graph.edges[0].expr->join_conditions));
  ASSERT_EQ(1, graph.num_where_predicates);
  EXPECT_EQ("(t1.x < 10)", ItemToString(graph.predicates[0].condition));
}

// Sets up a nonsensical query, but the point is that the multiple equality
// on the antijoin can be resolved to either t1.x or t2.x, and it should choose
// the same as is already there due to the inequality in order to not create
// an overly broad hyperedge. This is similar to a situation in DBT-3 Q21.
//
// We test with the inequality referring to both tables in turn, to make sure
// that we're not just getting lucky.
using MakeHypergraphMultipleEqualParamTest = OptimizerTestWithParam<int>;

TEST_P(MakeHypergraphMultipleEqualParamTest,
       MultipleEqualityOnAntijoinGetsIdeallyResolved) {
  const int table_num = GetParam();
  string other_table = (table_num == 0) ? "t1" : "t2";
  string query_str =
      "SELECT 1 FROM t1, t2 WHERE t1.x=t2.x "
      "AND t1.x NOT IN (SELECT t3.x FROM t3 WHERE t3.y <> " +
      other_table + ".y + 1)";
  Query_block *query_block = ParseAndResolve(query_str.c_str(),
                                             /*nullable=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  JoinHypergraph graph(m_thd->mem_root, query_block);
  string trace;
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);

  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // t1/t2. This one should not be too surprising.
  ASSERT_EQ(2, graph.edges.size());
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  ASSERT_EQ(1, graph.edges[0].expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)",
            ItemToString(graph.edges[0].expr->equijoin_conditions[0]));
  EXPECT_EQ(0, graph.edges[0].expr->join_conditions.size());

  // t1/t3 (antijoin) or t2/t3. The important part is that this should _not_
  // be a hyperedge.
  if (table_num == 0) {
    EXPECT_EQ(0x01, graph.graph.edges[2].left);
  } else {
    EXPECT_EQ(0x02, graph.graph.edges[2].left);
  }
  EXPECT_EQ(0x04, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::ANTIJOIN, graph.edges[1].expr->type);
  ASSERT_EQ(1, graph.edges[1].expr->equijoin_conditions.size());
  ASSERT_EQ(1, graph.edges[1].expr->join_conditions.size());
  EXPECT_EQ("(" + other_table + ".x = t3.x)",
            ItemToString(graph.edges[1].expr->equijoin_conditions[0]));
  EXPECT_EQ("(t3.y <> (" + other_table + ".y + 1))",
            ItemToString(graph.edges[1].expr->join_conditions[0]));
}

INSTANTIATE_TEST_SUITE_P(All, MakeHypergraphMultipleEqualParamTest,
                         ::testing::Values(0, 1));

// An alias for better naming.
// We don't verify costs; to do that, we'd probably need to mock out
// the cost model.
using HypergraphOptimizerTest = MakeHypergraphTest;

TEST_F(HypergraphOptimizerTest, SingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  ASSERT_EQ(AccessPath::TABLE_SCAN, root->type);
  EXPECT_EQ(m_fake_tables["t1"], root->table_scan().table);
  EXPECT_FLOAT_EQ(100.0F, root->num_output_rows());
}

TEST_F(HypergraphOptimizerTest, NumberOfAccessPaths) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 "
      "JOIN t2 ON t1.x=t2.x "
      "JOIN t3 ON t1.x=t3.x "
      "JOIN t4 ON t1.x=t4.x "
      "JOIN t5 ON t1.x=t5.x",
      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 101;
  m_fake_tables["t2"]->file->stats.records = 102;
  m_fake_tables["t3"]->file->stats.records = 103;
  m_fake_tables["t4"]->file->stats.records = 104;
  m_fake_tables["t5"]->file->stats.records = 105;

  m_fake_tables["t1"]->file->stats.data_file_length = 100;
  m_fake_tables["t2"]->file->stats.data_file_length = 100;
  m_fake_tables["t3"]->file->stats.data_file_length = 100;
  m_fake_tables["t4"]->file->stats.data_file_length = 100;
  m_fake_tables["t5"]->file->stats.data_file_length = 100;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_TRUE(root != nullptr);
  std::smatch matches;
  std::regex_search(trace, matches,
                    std::regex("keeping a total of ([0-9]+) access paths"));
  ASSERT_EQ(matches.size(), 2);  // One match and one sub-match.
  int paths = std::stoi(matches[1]);
  EXPECT_LT(paths, 100);
}

TEST_F(HypergraphOptimizerTest,
       PredicatePushdown) {  // Also tests nested loop join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x WHERE t2.y=3", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 200;
  m_fake_tables["t2"]->file->stats.records = 3;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The pushed-down filter makes the optimal plan be t2 on the left side,
  // with a nested loop.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);
  EXPECT_FLOAT_EQ(6.0F, root->num_output_rows());  // 60 rows, 10% selectivity.

  // The condition should be posted directly on t2.
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::FILTER, outer->type);
  EXPECT_EQ("(t2.y = 3)", ItemToString(outer->filter().condition));
  EXPECT_FLOAT_EQ(0.3F, outer->num_output_rows());  // 10% default selectivity.

  AccessPath *outer_child = outer->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer_child->type);
  EXPECT_EQ(m_fake_tables["t2"], outer_child->table_scan().table);
  EXPECT_FLOAT_EQ(3.0F, outer_child->num_output_rows());

  // The inner part should have a join condition as a filter.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::FILTER, inner->type);
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(inner->filter().condition));
  EXPECT_FLOAT_EQ(20.0F,
                  inner->num_output_rows());  // 10% default selectivity.

  AccessPath *inner_child = inner->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner_child->type);
  EXPECT_EQ(m_fake_tables["t1"], inner_child->table_scan().table);
}

TEST_F(HypergraphOptimizerTest, PredicatePushdownOuterJoin) {
  // The OR ... IS NULL part is to keep the LEFT JOIN from being simplified
  // to an inner join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x=t2.x "
      "WHERE t1.y=42 AND (t2.y=3 OR t2.y IS NULL)",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 2000;
  m_fake_tables["t2"]->file->stats.records = 3;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  // The t2 filter cannot be pushed down through the join, so it should be
  // on the root.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("((t2.y = 3) or (t2.y is null))",
            ItemToString(root->filter().condition));

  AccessPath *join = root->filter().child;
  ASSERT_EQ(AccessPath::HASH_JOIN, join->type);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN,
            join->hash_join().join_predicate->expr->type);
  EXPECT_FLOAT_EQ(
      200.0F,
      join->num_output_rows());  // Selectivity overridden by outer join.

  // The t1 condition should be pushed down to t1, since it's outer to the join.
  AccessPath *outer = join->hash_join().outer;
  ASSERT_EQ(AccessPath::FILTER, outer->type);
  EXPECT_EQ("(t1.y = 42)", ItemToString(outer->filter().condition));

  AccessPath *t1 = outer->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_EQ(m_fake_tables["t1"], t1->table_scan().table);

  AccessPath *inner = join->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_EQ(m_fake_tables["t2"], inner->table_scan().table);
  EXPECT_FLOAT_EQ(3.0F, inner->num_output_rows());
}

// NOTE: We don't test selectivity here, because it's not necessarily
// correct.
TEST_F(HypergraphOptimizerTest, PartialPredicatePushdown) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 "
      "WHERE (t1.x=1 AND t2.y=2) OR (t1.x=3 AND t2.y=4)",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 200;
  m_fake_tables["t2"]->file->stats.records = 30;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            root->hash_join().join_predicate->expr->type);

  // The WHERE should have been pushed down to a join condition,
  // which should not be removed despite the partial pushdown.
  const Mem_root_array<Item *> &join_conditions =
      root->hash_join().join_predicate->expr->join_conditions;
  ASSERT_EQ(1, join_conditions.size());
  EXPECT_EQ("(((t1.x = 1) and (t2.y = 2)) or ((t1.x = 3) and (t2.y = 4)))",
            ItemToString(join_conditions[0]));

  // t1 should have a partial condition.
  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::FILTER, outer->type);
  EXPECT_EQ("((t1.x = 1) or (t1.x = 3))",
            ItemToString(outer->filter().condition));

  AccessPath *outer_child = outer->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer_child->type);
  EXPECT_EQ(m_fake_tables["t1"], outer_child->table_scan().table);

  // t2 should have a different partial condition.
  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::FILTER, inner->type);
  EXPECT_EQ("((t2.y = 2) or (t2.y = 4))",
            ItemToString(inner->filter().condition));

  AccessPath *inner_child = inner->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner_child->type);
  EXPECT_EQ(m_fake_tables["t2"], inner_child->table_scan().table);
}

TEST_F(HypergraphOptimizerTest, PartialPredicatePushdownOuterJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON "
      "(t1.x=1 AND t2.y=2) OR (t1.x=3 AND t2.y=4)",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 200;
  m_fake_tables["t2"]->file->stats.records = 30;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN,
            root->hash_join().join_predicate->expr->type);

  // The join condition should still be there.
  const Mem_root_array<Item *> &join_conditions =
      root->hash_join().join_predicate->expr->join_conditions;
  ASSERT_EQ(1, join_conditions.size());
  EXPECT_EQ("(((t1.x = 1) and (t2.y = 2)) or ((t1.x = 3) and (t2.y = 4)))",
            ItemToString(join_conditions[0]));

  // t1 should _not_ have a partial condition, as it would
  // cause NULL-complemented rows to be eaten.
  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_EQ(m_fake_tables["t1"], outer->table_scan().table);

  // t2 should have a partial condition.
  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::FILTER, inner->type);
  EXPECT_EQ("((t2.y = 2) or (t2.y = 4))",
            ItemToString(inner->filter().condition));

  AccessPath *inner_child = inner->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner_child->type);
  EXPECT_EQ(m_fake_tables["t2"], inner_child->table_scan().table);
}

TEST_F(HypergraphOptimizerTest, PredicatePushdownToRef) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x=3", /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], t1->field[1], /*unique=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The condition should be gone, and only ref access should be in its place.
  // There shouldn't be EQ_REF, since we only have a partial match.
  ASSERT_EQ(AccessPath::REF, root->type);
  EXPECT_EQ(0, root->ref().ref->key);
  EXPECT_EQ(5, root->ref().ref->key_length);
  EXPECT_EQ(1, root->ref().ref->key_parts);
  EXPECT_FLOAT_EQ(10.0, root->num_output_rows());
}

TEST_F(HypergraphOptimizerTest, NotPredicatePushdownToRef) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.y=3", /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], t1->field[1], /*unique=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // t1.y can't be pushed since t1.x wasn't.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("(t1.y = 3)", ItemToString(root->filter().condition));
}

TEST_F(HypergraphOptimizerTest, MultiPartPredicatePushdownToRef) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.y=3 AND t1.x=2", /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], t1->field[1], /*unique=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Both should be pushed, and we should now use the unique index.
  ASSERT_EQ(AccessPath::EQ_REF, root->type);
  EXPECT_EQ(0, root->eq_ref().ref->key);
  EXPECT_EQ(10, root->eq_ref().ref->key_length);
  EXPECT_EQ(2, root->eq_ref().ref->key_parts);
}

TEST_F(HypergraphOptimizerTest, JoinConditionToRef) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 JOIN t3 ON t2.y=t3.y) ON t1.x=t3.x",
      /*nullable=*/true);
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];
  t2->create_index(t2->field[1], /*column2=*/nullptr, /*unique=*/false);
  t3->create_index(t3->field[0], t3->field[1], /*unique=*/true);

  // Hash join between t2/t3 is attractive, but hash join between t1 and t2/t3
  // should not be.
  m_fake_tables["t1"]->file->stats.records = 1000000;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t3"]->file->stats.records = 1000;
  m_fake_tables["t3"]->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The optimal plan consists of only nested-loop joins.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::OUTER, root->nested_loop_join().join_type);

  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_EQ(m_fake_tables["t1"], outer->table_scan().table);
  EXPECT_FLOAT_EQ(1000000.0F, outer->num_output_rows());

  // The inner part should also be nested-loop.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, inner->type);
  EXPECT_EQ(JoinType::INNER, inner->nested_loop_join().join_type);

  // We should have t2 on the left, and t3 on the right
  // (or we couldn't use the entire unique index).
  AccessPath *t2_path = inner->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2_path->type);
  EXPECT_EQ(m_fake_tables["t2"], t2_path->table_scan().table);
  EXPECT_FLOAT_EQ(100.0F, t2_path->num_output_rows());

  // t3 should use the unique index, and thus be capped at one row.
  AccessPath *t3_path = inner->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::EQ_REF, t3_path->type);
  EXPECT_EQ(m_fake_tables["t3"], t3_path->eq_ref().table);
  EXPECT_FLOAT_EQ(1.0F, t3_path->num_output_rows());

  // t2/t3 is 100 * 1, obviously.
  EXPECT_FLOAT_EQ(100.0F, inner->num_output_rows());

  // The root should have t1 multiplied by t2/t3;
  // since the join predicate is already applied (and subsumed),
  // we should have no further reduction from it.
  EXPECT_FLOAT_EQ(outer->num_output_rows() * inner->num_output_rows(),
                  root->num_output_rows());
}

TEST_F(HypergraphOptimizerTest, PreferWidestEqRefKey) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x = 1 AND t1.y = 2",
                      /*nullable=*/true);

  Fake_TABLE *t1 = m_fake_tables["t1"];

  // Create three unique indexes.
  const int key_x =
      t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/true);
  const int key_xy =
      t1->create_index(t1->field[0], t1->field[1], /*unique=*/true);
  const int key_y =
      t1->create_index(t1->field[1], /*column2=*/nullptr, /*unique=*/true);

  EXPECT_EQ(0, key_x);
  EXPECT_EQ(1, key_xy);
  EXPECT_EQ(2, key_y);

  t1->file->stats.records = 10000;
  t1->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect that we use the widest key. That is, we should pick an EQ_REF on the
  // (x, y) index with no filter, not an EQ_REF on the single-column indexes
  // with a filter on top.
  ASSERT_EQ(AccessPath::EQ_REF, root->type);
  EXPECT_EQ(key_xy, root->eq_ref().ref->key);
}

// Verify that we can push ref access into a hash join's hash table.
TEST_F(HypergraphOptimizerTest, RefIntoHashJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 JOIN t3 ON t2.y=t3.y) ON t1.x=t3.x",
      /*nullable=*/true);
  Fake_TABLE *t3 = m_fake_tables["t3"];
  t3->create_index(t3->field[0], /*column2=*/nullptr, /*unique=*/false);
  ulong rec_per_key_int[] = {1};
  float rec_per_key[] = {0.001f};
  t3->key_info[0].set_rec_per_key_array(rec_per_key_int, rec_per_key);

  // Hash join between t2/t3 is attractive, but hash join between t1 and t2/t3
  // should not be.
  m_fake_tables["t1"]->file->stats.records = 10;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t3"]->file->stats.records = 10000000;
  m_fake_tables["t3"]->file->stats.data_file_length = 1e6;

  // Forbid changing the order of t2/t3, just to get the plan we want.
  // (In a more real situation, we could have e.g. an antijoin outside a left
  // join, but it's a bit tricky to set up in a test.)
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_flags =
      MakeSecondaryEngineFlags(SecondaryEngineFlag::SUPPORTS_HASH_JOIN,
                               SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        if (path->type == AccessPath::NESTED_LOOP_JOIN) {
          AccessPath *outer = path->nested_loop_join().outer;
          if (outer->type == AccessPath::TABLE_SCAN &&
              strcmp(outer->table_scan().table->alias, "t3") == 0) {
            return true;
          }
          if (outer->type == AccessPath::REF &&
              strcmp(outer->ref().table->alias, "t3") == 0) {
            return true;
          }
        }
        return false;
      };

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The t1-{t2,t3} join should be nested loop.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::OUTER, root->nested_loop_join().join_type);

  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_EQ(m_fake_tables["t1"], outer->table_scan().table);

  // The inner part, however, should be a hash join.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, inner->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            inner->hash_join().join_predicate->expr->type);

  // ...and t3 should be on the right, as a ref access against t1.
  AccessPath *t3_path = inner->hash_join().inner;
  ASSERT_EQ(AccessPath::REF, t3_path->type);
  EXPECT_EQ(m_fake_tables["t3"], t3_path->ref().table);
  EXPECT_EQ(0, t3_path->ref().ref->key);
  EXPECT_EQ("t1.x", ItemToString(t3_path->ref().ref->items[0]));
}

// Verify that we can make sargable predicates out of multiple equalities.
TEST_F(HypergraphOptimizerTest, MultiEqualitySargable) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3 WHERE t1.x = t2.x AND t2.x = t3.x",
      /*nullable=*/true);
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];
  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/true);
  t3->create_index(t3->field[0], /*column2=*/nullptr, /*unique=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  // The logical plan should be t1/t2/t3, with index lookups on t2 and t3.
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t3"]->file->stats.records = 1000000;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The optimal plan consists of only nested-loop joins (notably left-deep).
  // We don't verify costs.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);

  // The inner part should also be nested-loop.
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, outer->type);
  EXPECT_EQ(JoinType::INNER, outer->nested_loop_join().join_type);

  // t1 is on the very left side.
  AccessPath *t1 = outer->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_EQ(m_fake_tables["t1"], t1->table_scan().table);

  // We have two index lookups; t2 and t3. We don't care about the order.
  ASSERT_EQ(AccessPath::EQ_REF, outer->nested_loop_join().inner->type);
  ASSERT_EQ(AccessPath::EQ_REF, root->nested_loop_join().inner->type);
}

TEST_F(HypergraphOptimizerTest, DoNotApplyBothSargableJoinAndFilterJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3, t4 WHERE t1.x = t2.x AND t2.x = t3.x",
      /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  // The logical plan should be to hash-join t2/t3, then nestloop-join
  // against the index on t1. The t4 table somehow needs to be present
  // to trigger the issue; it doesn't really matter whether it's on the
  // left or right side (since it doesn't have a join condition),
  // but it happens to be put on the right.
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 100000000;
  m_fake_tables["t3"]->file->stats.records = 1000000;
  m_fake_tables["t4"]->file->stats.records = 10000;

  // Incentivize ref access on t1, just to get the plan we want.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_flags =
      MakeSecondaryEngineFlags(SecondaryEngineFlag::SUPPORTS_HASH_JOIN,
                               SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        if (path->type == AccessPath::REF &&
            strcmp(path->ref().table->alias, "t1") == 0) {
          path->cost *= 0.01;
          path->init_cost *= 0.01;
          path->cost_before_filter *= 0.01;
        }
        return false;
      };

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // t4 needs to come in on the top (since we've put it as a Cartesian product);
  // either left or right side. It happens to be on the right.
  // We don't verify costs.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);

  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_EQ(m_fake_tables["t4"], inner->table_scan().table);

  // Now for the meat of the plan. There should be a nested loop,
  // with t2/t3 on the inside and t1 on the outside.
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, outer->type);

  // We don't check the t2/t3 part very thoroughly.
  EXPECT_EQ(AccessPath::HASH_JOIN, outer->nested_loop_join().outer->type);

  // Now for the point of the test: We should have t1 on the inner side,
  // with t1=t2 pushed down into the index, and it should _not_ have a t1=t3
  // filter; even though it would seemingly be attractive to join t1=t3 against
  // the ref access, that would be double-counting the selectivity and thus
  // not permitted. (Well, it would be permitted, but we'd have to add code
  // not to apply the selectivity twice, and then it would just be extra cost
  // applying a redundant filter.)
  AccessPath *inner_inner = outer->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::REF, inner_inner->type);
  EXPECT_STREQ("t1", inner_inner->ref().table->alias);
  EXPECT_EQ(0, inner_inner->ref().ref->key);
  EXPECT_EQ("t2.x", ItemToString(inner_inner->ref().ref->items[0]));
}

// The selectivity of sargable join predicates could in some cases be
// double-counted when the sargable join predicate was part of a cycle in the
// join graph.
TEST_F(HypergraphOptimizerTest, SargableJoinPredicateSelectivity) {
  // The inconsistent row estimates were only seen if the sargable predicate
  // t1.x=t2.x was not fully subsumed by a ref access on t1.x. Achieved by
  // giving t2.x a different type (UNSIGNED) than t1.x (SIGNED).
  Mock_field_long t2_x("x", /*is_nullable=*/false, /*is_unsigned=*/true);
  Mock_field_long t2_y("y", /*is_nullable=*/false, /*is_unsigned=*/false);
  Fake_TABLE *t2 = new (m_thd->mem_root) Fake_TABLE(&t2_x, &t2_y);
  m_fake_tables["t2"] = t2;
  t2->set_created();

  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3 "
      "WHERE t1.x = t2.x AND t1.y = t3.x AND t2.y = t3.y",
      /*nullable=*/false);

  // Add an index on t1(x) to make t1.x=t2.x sargable.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  const int t1_idx =
      t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/false);
  ulong rec_per_key_int[] = {1};
  float rec_per_key[] = {1.0f};
  t1->key_info[t1_idx].set_rec_per_key_array(rec_per_key_int, rec_per_key);

  Fake_TABLE *t3 = m_fake_tables["t3"];
  t1->file->stats.records = 1000;
  t1->file->stats.data_file_length = 1e6;
  t2->file->stats.records = 100;
  t2->file->stats.data_file_length = 1e5;
  t3->file->stats.records = 10;
  t3->file->stats.data_file_length = 1e4;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // We don't really care about which exact plan is chosen, but the inconsistent
  // row estimates were caused by REF access, so make sure our plan has one.
  AccessPath *ref_path = nullptr;
  WalkAccessPaths(root, query_block->join,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [&ref_path](AccessPath *path, const JOIN *) {
                    if (path->type == AccessPath::REF) {
                      EXPECT_EQ(nullptr, ref_path);
                      ref_path = path;
                    }
                    return false;
                  });
  ASSERT_NE(nullptr, ref_path);
  EXPECT_STREQ("t1", ref_path->ref().table->alias);
  EXPECT_EQ(string("t2.x"), ItemToString(ref_path->ref().ref->items[0]));

  // We do care about the estimated cardinality of the result. It used to be
  // much too low because the selectivity of the sargable predicate was applied
  // twice.
  EXPECT_FLOAT_EQ(
      /* Rows from t1: */ rec_per_key[0] *
          /* Rows from t2: */ t2->file->stats.records * COND_FILTER_EQUALITY *
          /* Rows from t3: */ t3->file->stats.records * COND_FILTER_EQUALITY,
      root->num_output_rows());
}

TEST_F(HypergraphOptimizerTest, SargableJoinPredicateWithTypeMismatch) {
  // Give t1.x a different type than t2.x.
  Mock_field_varstring t1_x(/*share=*/nullptr, /*name=*/"x",
                            /*char_len=*/100, /*is_nullable=*/true);
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&t1_x);
  m_fake_tables["t1"] = t1;

  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE t1.x = t2.x", /*nullable=*/true);

  // Add an index on t2(x) to make the join predicate sargable.
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/true);

  // Set up sizes to make index access on t2 preferable.
  t1->file->stats.records = 100;
  t1->file->stats.data_file_length = 1e5;
  t2->file->stats.records = 100000;
  t2->file->stats.data_file_length = 1e7;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect NLJ(t1, EQ_REF(t2)). Because of the type mismatch between t1.x and
  // t2.x, a filter is needed on top of the EQ_REF to make sure no false matches
  // are returned.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  ASSERT_EQ(AccessPath::FILTER, root->nested_loop_join().inner->type);
  EXPECT_EQ("(cast(t1.x as double) = cast(t2.x as double))",
            ItemToString(root->nested_loop_join().inner->filter().condition));
  ASSERT_EQ(AccessPath::EQ_REF,
            root->nested_loop_join().inner->filter().child->type);
  EXPECT_STREQ(
      "t2",
      root->nested_loop_join().inner->filter().child->eq_ref().table->alias);
}

// Test that we can use index for join conditions on the form t1.field =
// f(t2.field), not only for t1.field = t2.field.
TEST_F(HypergraphOptimizerTest, SargableJoinPredicateWithFunction) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE t1.x = t2.x + 1", /*nullable=*/true);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];

  // Add an index on t1.x to make the join predicate sargable.
  t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/true);

  // Set up sizes to make index access on t1 preferable.
  t1->file->stats.records = 100000;
  t1->file->stats.data_file_length = 1e7;
  t2->file->stats.records = 100;
  t2->file->stats.data_file_length = 1e5;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect NLJ(t2, EQ_REF(t1)). A (redundant?) filter is put on top of the
  // index lookup to protect against inexact conversion from t2.x+1 to INT (see
  // ref_lookup_subsumes_comparison()).
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  ASSERT_EQ(AccessPath::FILTER, root->nested_loop_join().inner->type);
  EXPECT_EQ("(t1.x = (t2.x + 1))",
            ItemToString(root->nested_loop_join().inner->filter().condition));
  ASSERT_EQ(AccessPath::EQ_REF,
            root->nested_loop_join().inner->filter().child->type);
  EXPECT_STREQ(
      "t1",
      root->nested_loop_join().inner->filter().child->eq_ref().table->alias);
}

TEST_F(HypergraphOptimizerTest, SargableSubquery) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x = (SELECT 1 FROM t2)", /*nullable=*/true);

  // Plan the subquery first.
  {
    Query_block *subquery =
        query_block->first_inner_query_expression()->first_query_block();
    ResolveQueryBlock(m_thd, subquery, /*nullable=*/true, &m_fake_tables);
    string trace;
    AccessPath *subquery_path =
        FindBestQueryPlanAndFinalize(m_thd, subquery, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.
    // Prints out the query plan on failure.
    SCOPED_TRACE(PrintQueryPlan(0, subquery_path, subquery->join,
                                /*is_root_of_join=*/true));
    EXPECT_EQ(AccessPath::TABLE_SCAN, subquery_path->type);
  }

  // Add an index on t1.x to make the predicate sargable.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/true);

  // Set up sizes to make index lookup preferable.
  t1->file->stats.records = 100000;
  t1->file->stats.data_file_length = 1e7;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect an index lookup with a (redundant?) filter on top.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("(t1.x = (select #2))", ItemToString(root->filter().condition));
  ASSERT_EQ(AccessPath::EQ_REF, root->filter().child->type);
  EXPECT_EQ("(select #2)",
            ItemToString(root->filter().child->eq_ref().ref->items[0]));
  EXPECT_STREQ("t1", root->filter().child->eq_ref().table->alias);
}

TEST_F(HypergraphOptimizerTest, SargableOuterReference) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE (SELECT t2.y FROM t2 WHERE t2.x = t1.x)",
      /*nullable=*/true);

  Query_block *subquery =
      query_block->first_inner_query_expression()->first_query_block();
  ResolveQueryBlock(m_thd, subquery, /*nullable=*/true, &m_fake_tables);

  // Add an index on t2.x to make the predicate in the subquery sargable.
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/true);
  t2->file->stats.records = 100000;
  t2->file->stats.data_file_length = 1e7;

  string trace;
  AccessPath *subquery_path = FindBestQueryPlan(m_thd, subquery, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, subquery_path, subquery->join,
                              /*is_root_of_join=*/true));

  // Expect the subquery to become an index lookup using the outer reference as
  // a constant value.
  ASSERT_EQ(AccessPath::EQ_REF, subquery_path->type);
  EXPECT_STREQ("t2", subquery_path->eq_ref().table->alias);
  EXPECT_EQ("t1.x", ItemToString(subquery_path->eq_ref().ref->items[0]));
  EXPECT_TRUE(subquery_path->eq_ref().ref->items[0]->is_outer_reference());
}

TEST_F(HypergraphOptimizerTest, SargableHyperpredicate) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3 WHERE t1.x = t2.x + t3.x AND t2.y = t3.y",
      /*nullable=*/true);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];

  // Add an index on t1.x to make the join predicate sargable.
  t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/true);

  // Set up sizes to make index access on t1 preferable.
  t1->file->stats.records = 100000;
  t1->file->stats.data_file_length = 1e7;
  t2->file->stats.records = 100;
  t2->file->stats.data_file_length = 1e5;
  t3->file->stats.records = 200;
  t3->file->stats.data_file_length = 2e5;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect the join predicate t1.x = t2.x + t3.x to be sargable and result in
  // an index lookup, giving this plan: NLJ(HJ(t3, t2), FILTER(EQ_REF(t1)))
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(AccessPath::HASH_JOIN, root->nested_loop_join().outer->type);

  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::FILTER, inner->type);
  EXPECT_EQ("(t1.x = (t2.x + t3.x))", ItemToString(inner->filter().condition));

  AccessPath *index_path = inner->filter().child;
  ASSERT_EQ(AccessPath::EQ_REF, index_path->type);
  EXPECT_STREQ("t1", index_path->eq_ref().table->alias);
  EXPECT_EQ("(t2.x + t3.x)", ItemToString(index_path->eq_ref().ref->items[0]));
}

TEST_F(HypergraphOptimizerTest, AntiJoinGetsSameEstimateWithAndWithoutIndex) {
  double ref_output_rows = 0.0;
  for (bool has_index : {false, true}) {
    Query_block *query_block = ParseAndResolve(
        "SELECT 1 FROM t1 WHERE t1.x NOT IN ( SELECT t2.x FROM t2 )",
        /*nullable=*/false);

    m_fake_tables["t1"]->file->stats.records = 10000;

    Fake_TABLE *t2 = m_fake_tables["t2"];
    if (has_index) {
      t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/false);
    }
    t2->file->stats.records = 100;

    string trace;
    AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.

    if (!has_index) {
      ref_output_rows = root->num_output_rows();
    } else {
      EXPECT_FLOAT_EQ(ref_output_rows, root->num_output_rows());
      EXPECT_GE(root->num_output_rows(),
                500.0);  // Due to the 10% fudge factor.
    }

    query_block->cleanup(/*full=*/true);
    ClearFakeTables();
  }
}

// Tests a query which has a predicate that must be delayed until after the
// join, and this predicate contains a subquery that may be materialized. The
// selectivity of the delayed predicate used to be double-counted in the plans
// that used materialization.
TEST_F(HypergraphOptimizerTest, DelayedMaterializablePredicate) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x = t2.x "
      "WHERE t2.y > ALL (SELECT 1)",
      /*nullable=*/false);

  m_fake_tables["t1"]->file->stats.records = 1000;
  m_fake_tables["t2"]->file->stats.records = 100;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  // Expect a FILTER node with the delayed predicate, and its row estimate
  // should be cardinality(t1) * cardinality(t2) * selectivity(t1.x=t2.x) *
  // selectivity(t2.y > ALL).
  EXPECT_FLOAT_EQ(
      1000 * 100 * COND_FILTER_EQUALITY * (1 - COND_FILTER_INEQUALITY),
      root->num_output_rows());
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("<not>((t2.y <= <max>(select #2)))",
            ItemToString(root->filter().condition));
}

TEST_F(HypergraphOptimizerTest, DoNotExpandJoinFiltersMultipleTimes) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM "
      "  t1 "
      "  JOIN t2 ON t1.x = t2.x "
      "  JOIN t3 ON t1.x = t3.x "
      "  JOIN t4 ON t2.y = t4.x",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 1;
  m_fake_tables["t2"]->file->stats.records = 1;
  m_fake_tables["t3"]->file->stats.records = 10;
  m_fake_tables["t4"]->file->stats.records = 10;

  // To provoke the bug, we need a plan where there is only one hash join,
  // and that is with t4 on the outer side (at the very top).
  // It's not clear exactly why this is, but presumably, this constellation
  // causes us to keep (and thus expand) at least two root paths containing
  // the same nested loop, which is required to do expansion twice and thus
  // trigger the issue.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_flags =
      MakeSecondaryEngineFlags(SecondaryEngineFlag::SUPPORTS_HASH_JOIN,
                               SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        if (path->type == AccessPath::NESTED_LOOP_JOIN &&
            Overlaps(GetUsedTableMap(path->nested_loop_join().inner, false),
                     0b1000)) {
          return true;
        }
        if (path->type == AccessPath::HASH_JOIN &&
            GetUsedTableMap(path->hash_join().outer, false) != 0b1000) {
          return true;
        }
        return false;
      };

  AccessPath *root =
      FindBestQueryPlanAndFinalize(m_thd, query_block, /*trace=*/nullptr);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Check that we don't have a filter on top of a filter.
  WalkAccessPaths(root, /*join=*/nullptr, WalkAccessPathPolicy::ENTIRE_TREE,
                  [&](const AccessPath *path, const JOIN *) {
                    if (path->type == AccessPath::FILTER) {
                      EXPECT_NE(AccessPath::FILTER, path->filter().child->type);
                    }
                    return false;
                  });
}

// Verifies that DisallowParameterizedJoinPath() is doing its job.
TEST_F(HypergraphOptimizerTest, InnerNestloopShouldBeLeftDeep) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3, t4 WHERE t1.x=t2.x AND t2.y=t3.y AND "
      "t3.z=t4.z",
      /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];
  Fake_TABLE *t4 = m_fake_tables["t4"];
  t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/false);
  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/false);
  t2->create_index(t2->field[1], /*column2=*/nullptr, /*unique=*/false);
  t3->create_index(t3->field[1], /*column2=*/nullptr, /*unique=*/false);
  t3->create_index(t3->field[2], /*column2=*/nullptr, /*unique=*/false);
  t4->create_index(t4->field[2], /*column2=*/nullptr, /*unique=*/false);

  // We use the secondary engine hook to check that we never try a join between
  // ref accesses. They are not _wrong_, but they are redundant in this
  // situation, so we should prune them out.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_flags =
      MakeSecondaryEngineFlags(SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        if (path->type == AccessPath::NESTED_LOOP_JOIN) {
          AccessPath *outer = path->nested_loop_join().outer;
          AccessPath *inner = path->nested_loop_join().inner;
          EXPECT_FALSE(outer->type == AccessPath::REF &&
                       inner->type == AccessPath::REF);
        }
        return false;
      };

  EXPECT_NE(nullptr, FindBestQueryPlanAndFinalize(m_thd, query_block,
                                                  /*trace=*/nullptr));

  // We don't verify the plan in itself.
}

TEST_F(HypergraphOptimizerTest, CombineFilters) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x = 1 HAVING RAND() > 0.5", /*nullable=*/true);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // We should see a single filter which combines the WHERE clause and the
  // HAVING clause. Not two filters stacked on top of each other.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN, root->filter().child->type);

  EXPECT_EQ("((t1.x = 1) and (rand() > 0.5))",
            ItemToString(root->filter().condition));
}

TEST_F(HypergraphOptimizerTest, InsertCastsInSelectExpressions) {
  Mock_field_datetime t1_x;
  Mock_field_long t1_y(/*is_unsigned=*/false);
  t1_x.field_name = "x";
  t1_y.field_name = "y";

  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&t1_x, &t1_y);
  m_fake_tables["t1"] = t1;
  t1->set_created();

  Query_block *query_block =
      ParseAndResolve("SELECT t1.x = t1.y FROM t1", /*nullable=*/true);
  FindBestQueryPlanAndFinalize(m_thd, query_block, /*trace=*/nullptr);
  ASSERT_EQ(1, query_block->join->fields->size());
  EXPECT_EQ("(cast(t1.x as double) = cast(t1.y as double))",
            ItemToString((*query_block->join->fields)[0]));
}

// Test that we evaluate the most selective and least expensive WHERE predicates
// before the less selective and more expensive ones.
TEST_F(HypergraphOptimizerTest, OrderingOfWherePredicates) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE "
      "t1.x <> 10 AND t1.y = 123 AND "
      "t1.z >= ALL (SELECT t2.x FROM t2) AND "
      "t1.x + t1.y = t1.z + t1.w AND "
      "t1.w = (SELECT MAX(t3.x) FROM t3) AND "
      "t1.x > t1.z",
      /*nullable=*/true);
  ASSERT_NE(nullptr, query_block);

  // Resolve the subqueries too.
  for (Query_expression *expr = query_block->first_inner_query_expression();
       expr != nullptr; expr = expr->next_query_expression()) {
    Query_block *subquery = expr->first_query_block();
    ResolveQueryBlock(m_thd, subquery, /*nullable=*/true, &m_fake_tables);
    string trace;
    AccessPath *subquery_path =
        FindBestQueryPlanAndFinalize(m_thd, subquery, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.
    ASSERT_NE(nullptr, subquery_path);
  }

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ(
      // First the simple predicates, sorted by selectivity.
      "((t1.y = 123) and (t1.x > t1.z) and (t1.x <> 10) and "
      "((t1.x + t1.y) = (t1.z + t1.w)) and "
      // Then the predicates which contain subqueries.
      "<not>((t1.z < <max>(select #2))) and (t1.w = (select #3)))",
      ItemToString(root->filter().condition));
}

TEST_F(HypergraphOptimizerTest, OrderingOfJoinPredicates) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE "
      "t1.x > t2.x AND t1.y = t2.y AND "
      "t1.z + t2.z = (SELECT MAX(t3.x) FROM t3) AND "
      "t1.w < t2.w",
      /*nullable=*/true);
  ASSERT_NE(nullptr, query_block);

  // Resolve the subquery too.
  {
    Query_block *subquery =
        query_block->first_inner_query_expression()->first_query_block();
    ResolveQueryBlock(m_thd, subquery, /*nullable=*/true, &m_fake_tables);
    string trace;
    AccessPath *subquery_path =
        FindBestQueryPlanAndFinalize(m_thd, subquery, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.
    ASSERT_NE(nullptr, subquery_path);
  }

  // Use small tables so that a nested loop join is preferred.
  m_fake_tables["t1"]->file->stats.records = 1;
  m_fake_tables["t2"]->file->stats.records = 1;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  ASSERT_EQ(AccessPath::FILTER, root->nested_loop_join().inner->type);

  // Expect the equijoin conditions to be evaluated before the non-equijoin
  // conditions. Conditions with subqueries should be evaluated last.
  EXPECT_EQ(
      "((t1.y = t2.y) and (t1.x > t2.x) and (t1.w < t2.w) and "
      "((t1.z + t2.z) = (select #2)))",
      ItemToString(root->nested_loop_join().inner->filter().condition));
}

static string PrintSargablePredicate(const SargablePredicate &sp,
                                     const JoinHypergraph &graph) {
  return StringPrintf(
      "%s.%s -> %s [%s]", sp.field->table->alias, sp.field->field_name,
      ItemToString(sp.other_side).c_str(),
      ItemToString(graph.predicates[sp.predicate_index].condition).c_str());
}

// Verify that when we add a cycle in the graph due to a multiple equality,
// that join predicate also becomes sargable.
using HypergraphOptimizerCyclePredicatesSargableTest =
    OptimizerTestWithParam<const char *>;

TEST_P(HypergraphOptimizerCyclePredicatesSargableTest,
       CyclePredicatesSargable) {
  Query_block *query_block = ParseAndResolve(GetParam(),
                                             /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];
  t1->create_index(t1->field[0], /*column2=*/nullptr, /*unique=*/false);
  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/false);
  t3->create_index(t3->field[0], /*column2=*/nullptr, /*unique=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  string trace;
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  JoinHypergraph graph(m_thd->mem_root, query_block);
  bool always_false = false;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, &trace, &graph, &always_false));
  EXPECT_FALSE(always_false);
  FindSargablePredicates(m_thd, &trace, &graph);

  // Each node should have two sargable join predicates
  // (one to each of the other nodes). Verify that they are
  // correctly set up (the order does not matter, though).
  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  ASSERT_EQ(2, graph.nodes[0].sargable_predicates.size());
  EXPECT_EQ(
      "t1.x -> t2.x [(t1.x = t2.x)]",
      PrintSargablePredicate(graph.nodes[0].sargable_predicates[0], graph));
  EXPECT_EQ(
      "t1.x -> t3.x [(t1.x = t3.x)]",
      PrintSargablePredicate(graph.nodes[0].sargable_predicates[1], graph));

  ASSERT_EQ(2, graph.nodes[1].sargable_predicates.size());
  EXPECT_EQ(
      "t2.x -> t3.x [(t2.x = t3.x)]",
      PrintSargablePredicate(graph.nodes[1].sargable_predicates[0], graph));
  EXPECT_EQ(
      "t2.x -> t1.x [(t1.x = t2.x)]",
      PrintSargablePredicate(graph.nodes[1].sargable_predicates[1], graph));

  ASSERT_EQ(2, graph.nodes[2].sargable_predicates.size());
  EXPECT_EQ(
      "t3.x -> t2.x [(t2.x = t3.x)]",
      PrintSargablePredicate(graph.nodes[2].sargable_predicates[0], graph));
  EXPECT_EQ(
      "t3.x -> t1.x [(t1.x = t3.x)]",
      PrintSargablePredicate(graph.nodes[2].sargable_predicates[1], graph));
}

INSTANTIATE_TEST_SUITE_P(
    TrueAndFalse, HypergraphOptimizerCyclePredicatesSargableTest,
    ::testing::Values(
        // With and without an explicit cycle.
        "SELECT 1 FROM t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x AND t1.x=t3.x",
        "SELECT 1 FROM t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x"));

TEST_F(HypergraphOptimizerTest, SimpleInnerJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 1000;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t3"]->file->stats.records = 1000000;

  // Set up some large scan costs to discourage nested loop.
  m_fake_tables["t1"]->file->stats.data_file_length = 10e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t3"]->file->stats.data_file_length = 10000e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // It's pretty obvious given the sizes of these tables that the optimal
  // order for hash join is t3 hj (t1 hj t2). We don't check the costs
  // beyond that.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            root->hash_join().join_predicate->expr->type);

  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_EQ(m_fake_tables["t3"], outer->table_scan().table);

  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, inner->type);

  AccessPath *t1 = inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_EQ(m_fake_tables["t1"], t1->table_scan().table);

  AccessPath *t2 = inner->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_EQ(m_fake_tables["t2"], t2->table_scan().table);

  // We should have seen the other plans, too (in particular, joining
  // {t1} versus {t2,t3}; {t1,t3} versus {t2} is illegal since we don't
  // consider Cartesian products). The six subplans seen are:
  //
  // t1, t2, t3, t1-t2, t2-t3, t1-{t2,t3}, {t1,t2}-t3
  EXPECT_EQ(m_thd->m_current_query_partial_plans, 6);
}

TEST_F(HypergraphOptimizerTest, StraightJoin) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 STRAIGHT_JOIN t2 ON t1.x=t2.x",
                      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;

  // Set up some large scan costs to discourage nested loop.
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The optimal order would be to reorder (t2, t1), but this should be
  // disallowed due to the use of STRAIGHT_JOIN.

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN,
            root->hash_join().join_predicate->expr->type);

  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_EQ(m_fake_tables["t1"], outer->table_scan().table);

  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_EQ(m_fake_tables["t2"], inner->table_scan().table);

  // We should see only the two table scans and then t1-t2, no other orders.
  EXPECT_EQ(m_thd->m_current_query_partial_plans, 3);
}

TEST_F(HypergraphOptimizerTest, StraightJoinWithMoreTables) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 STRAIGHT_JOIN t2 ON t1.x=t2.x "
      "STRAIGHT_JOIN t3 ON t1.y=t3.y STRAIGHT_JOIN "
      "t4 ON (t4.y = t2.y and t3.x <> t4.x)",
      /*nullable=*/true);
  // Make a call to optimize_cond() so that we have the equalities
  // placed at the end in the final where condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 1000;
  m_fake_tables["t3"]->file->stats.records = 100;
  m_fake_tables["t4"]->file->stats.records = 10;

  // Set up some large scan costs to discourage nested loop.
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 10e6;
  m_fake_tables["t3"]->file->stats.data_file_length = 100e6;
  m_fake_tables["t4"]->file->stats.data_file_length = 1000e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The expected order would be
  // ((t1 HJ t2 ON t1.x=t2.x) HJ t3 ON t1.y=t3.y) HJ t4 ON t4.y = t2.y and t3.x
  // <> t4.x )
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN,
            root->hash_join().join_predicate->expr->type);
  RelationalExpression *expr1 = root->hash_join().join_predicate->expr;
  EXPECT_EQ(1, expr1->join_conditions.size());
  ASSERT_EQ(1, expr1->equijoin_conditions.size());
  // Check that the join condition (t3.x <> t4.x) gets added
  // to the top join instead of the join between t3 and t4.
  EXPECT_EQ("(t3.x <> t4.x)", ItemToString(expr1->join_conditions[0]));
  EXPECT_EQ("(t4.y = t2.y)", ItemToString(expr1->equijoin_conditions[0]));

  AccessPath *t4 = root->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t4->type);
  EXPECT_EQ(m_fake_tables["t4"], t4->table_scan().table);

  AccessPath *t1t2t3 = root->hash_join().outer;
  ASSERT_EQ(AccessPath::HASH_JOIN, t1t2t3->type);
  RelationalExpression *expr2 = t1t2t3->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN, expr2->type);
  ASSERT_EQ(1, expr2->equijoin_conditions.size());
  EXPECT_EQ("(t1.y = t3.y)", ItemToString(expr2->equijoin_conditions[0]));

  AccessPath *t3 = t1t2t3->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t3->type);
  EXPECT_EQ(m_fake_tables["t3"], t3->table_scan().table);

  AccessPath *t1t2 = t1t2t3->hash_join().outer;
  ASSERT_EQ(AccessPath::HASH_JOIN, t1t2->type);
  RelationalExpression *expr3 = t1t2->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN, expr3->type);
  ASSERT_EQ(1, expr3->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(expr3->equijoin_conditions[0]));

  AccessPath *t2 = t1t2->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_EQ(m_fake_tables["t2"], t2->table_scan().table);

  AccessPath *t1 = t1t2->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_EQ(m_fake_tables["t1"], t1->table_scan().table);
}

TEST_F(HypergraphOptimizerTest, StraightJoinNotAssociative) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 STRAIGHT_JOIN t2 STRAIGHT_JOIN t3 "
      "STRAIGHT_JOIN t4 WHERE t3.y=t4.y AND t1.x=t2.x",
      /*nullable=*/true);

  // For secondary engine straight joins are not associative.
  m_initializer.thd()->set_secondary_engine_optimization(
      Secondary_engine_optimization::SECONDARY);
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_flags =
      MakeSecondaryEngineFlags(SecondaryEngineFlag::SUPPORTS_HASH_JOIN);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The expected order would be
  // ((t1 HJ t2 ON t1.x = t2.x) HJ t3) HJ t4 ON t3.y = t4.y
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN,
            root->hash_join().join_predicate->expr->type);
  RelationalExpression *expr1 = root->hash_join().join_predicate->expr;
  ASSERT_EQ(1, expr1->equijoin_conditions.size());
  EXPECT_EQ("(t3.y = t4.y)", ItemToString(expr1->equijoin_conditions[0]));

  AccessPath *t4 = root->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t4->type);
  EXPECT_EQ(m_fake_tables["t4"], t4->table_scan().table);

  AccessPath *t1t2t3 = root->hash_join().outer;
  ASSERT_EQ(AccessPath::HASH_JOIN, t1t2t3->type);
  RelationalExpression *expr2 = t1t2t3->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN, expr2->type);

  AccessPath *t3 = t1t2t3->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t3->type);
  EXPECT_EQ(m_fake_tables["t3"], t3->table_scan().table);

  AccessPath *t1t2 = t1t2t3->hash_join().outer;
  ASSERT_EQ(AccessPath::HASH_JOIN, t1t2->type);
  RelationalExpression *expr3 = t1t2->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::STRAIGHT_INNER_JOIN, expr3->type);
  ASSERT_EQ(1, expr3->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(expr3->equijoin_conditions[0]));

  AccessPath *t2 = t1t2->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_EQ(m_fake_tables["t2"], t2->table_scan().table);

  AccessPath *t1 = t1t2->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_EQ(m_fake_tables["t1"], t1->table_scan().table);
}

TEST_F(HypergraphOptimizerTest, NullSafeEqualHashJoin) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1, t2 WHERE t1.x <=> t2.x",
                      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;

  // Set up some large scan costs to discourage nested loop.
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);

  // The <=> predicate should be an equijoin condition.
  const RelationalExpression *expr = root->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::INNER_JOIN, expr->type);
  EXPECT_EQ(0, expr->join_conditions.size());
  ASSERT_EQ(1, expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x <=> t2.x)", ItemToString(expr->equijoin_conditions[0]));
}

TEST_F(HypergraphOptimizerTest, Cycle) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM "
      "t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x AND t1.x=t3.x",
      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // We should see t1, t2, t3, {t1,t2}, {t2,t3}, {t1,t3} and {t1,t2,t3}.
  EXPECT_EQ(m_thd->m_current_query_partial_plans, 7);
}

TEST_F(HypergraphOptimizerTest, CycleFromMultipleEquality) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM "
      "t1,t2,t3 WHERE t1.x=t2.x AND t2.x=t3.x",
      /*nullable=*/true);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // We should see t1, t2, t3, {t1,t2}, {t2,t3}, {t1,t3} and {t1,t2,t3}.
  EXPECT_EQ(m_thd->m_current_query_partial_plans, 7);
}

TEST_F(HypergraphOptimizerTest, UniqueIndexCapsBothWays) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x=t2.x",
                      /*nullable=*/false);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t1->file->stats.records = 1000;
  t2->file->stats.records = 1000;
  t1->create_index(t1->field[0], nullptr, /*unique=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The unique index on t1 isn't usable, but it should inform
  // the selectivity for the hash join nevertheless. (Without it,
  // we would see an estimate of 100k rows, since we don't have
  // selectivity information in our index and fall back to the
  // default selectivity of 0.1 for field = field.)
  EXPECT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_FLOAT_EQ(1000.0, root->num_output_rows());
}

/*
  Sets up this join graph:

    t1 --- t2
    | .     |
    |   .   |
    |     . |
    t3 --- t4

  t1-t3-t4 are joined along the x fields, t1-t2-t4 are joined along the y
  fields. The t1-t4 edge is created only due to multiple equalities, but the
  optimal plan is to use that edge, so that we can use the index on t4 to
  resolve both x and y. The crux of the issue is that this edge must then
  subsume both t1=t4 conditions.
 */
TEST_F(HypergraphOptimizerTest, SubsumedSargableInDoubleCycle) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3, t4 "
      "WHERE t1.x = t3.x AND t3.x = t4.x AND t1.y = t2.y AND t2.y = t4.y",
      /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];
  Fake_TABLE *t4 = m_fake_tables["t4"];
  t1->file->stats.records = 100;
  t2->file->stats.records = 100;
  t3->file->stats.records = 100;
  t4->file->stats.records = 100;
  t4->file->stats.data_file_length = 100e6;
  t3->create_index(t3->field[0], nullptr, /*unique=*/false);
  t4->create_index(t4->field[0], t4->field[1], /*unique=*/false);

  // Build multiple equalities from the WHERE condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The four tables combined together, with three 0.1 selectivities in the
  // x multi-equality and then one on y.
  EXPECT_FLOAT_EQ(10000.0, root->num_output_rows());

  // We should have an index lookup into t4, covering both t1=t4 conditions.
  bool found_t4_index_lookup = false;
  WalkAccessPaths(
      root, /*join=*/nullptr, WalkAccessPathPolicy::ENTIRE_TREE,
      [&](const AccessPath *path, const JOIN *) {
        if (path->type == AccessPath::REF &&
            strcmp("t4", path->ref().table->alias) == 0) {
          found_t4_index_lookup = true;
          EXPECT_EQ(2, path->ref().ref->key_parts);
          EXPECT_EQ("t1.x", ItemToString(path->ref().ref->items[0]));
          EXPECT_EQ("t1.y", ItemToString(path->ref().ref->items[1]));
        }
        return false;
      });
  EXPECT_TRUE(found_t4_index_lookup);

  // And thus, there should be no filter containing both t1 and t4.
  WalkAccessPaths(root, /*join=*/nullptr, WalkAccessPathPolicy::ENTIRE_TREE,
                  [&](const AccessPath *path, const JOIN *) {
                    if (path->type == AccessPath::FILTER) {
                      const string str = ItemToString(path->filter().condition);
                      EXPECT_TRUE(str.find("t1") == string::npos ||
                                  str.find("t4") == string::npos);
                    }
                    return false;
                  });
}

/*
  Sets up a semi-join with this join graph:

    t1    t3
    | \__/ |
    | /  \ |
    t2    t4

  The join predicates for both t1-t2 and t3-t4 are sargable, and the preferred
  paths apply them as sargable. The semi-join predicate should not come from the
  same multiple equality as the sargable predicates, so it should not be made
  redundant by them.
 */
TEST_F(HypergraphOptimizerTest, SemiJoinPredicateNotRedundant) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE t1.y = t2.x AND t1.x IN "
      "(SELECT t3.x FROM t3, t4 WHERE t2.y = t3.y AND t3.x = t4.y)",
      /*nullable=*/true);

  // Create indexes on t1(y) and t4(y).
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[1], /*column2=*/nullptr, /*unique=*/false);
  Fake_TABLE *t4 = m_fake_tables["t4"];
  t4->create_index(t4->field[1], /*column2=*/nullptr, /*unique=*/false);

  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];

  // Adjust sizes so that NLJ(TS(t2), REF(t1)) and NLJ(TS(t3), REF(t4)) are
  // preferred join orders for the smaller joins.
  t1->file->stats.records = 1000;
  t2->file->stats.records = 1;
  t3->file->stats.records = 1;
  t4->file->stats.records = 1000;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Check that the expected plan is produced. Before bug#33619350 no plan was
  // produced at all.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->hash_join().outer->type);
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->hash_join().inner->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN,
            root->hash_join().outer->nested_loop_join().outer->type);
  EXPECT_EQ(AccessPath::REF,
            root->hash_join().outer->nested_loop_join().inner->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN,
            root->hash_join().inner->nested_loop_join().outer->type);
  EXPECT_EQ(AccessPath::REF,
            root->hash_join().inner->nested_loop_join().inner->type);
}

/*
  Another case where the semi-join condition is not redundant. In this case, the
  join condition on the outer side of the semi-join, the join condition on the
  inner side of the semi-join and the semi-join condition are part of the same
  multiple equality. But even so, the semi-join condition is not redundant,
  because none of the other two join conditions references any tables on the
  opposite side of the semi-join.
 */
TEST_F(HypergraphOptimizerTest, SemiJoinPredicateNotRedundant2) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3 WHERE t2.x = t3.x AND t2.x IN "
      "(SELECT t5.x FROM t4, t5 WHERE t4.x = t5.x AND t4.y <> t1.y)",
      /*nullable=*/false);

  // Add an index on t2.x to make the join predicate t2.x = t3.x sargable.
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/false);

  // Add a unique index to make the join predicate t4.x = t5.x sargable.
  Fake_TABLE *t5 = m_fake_tables["t5"];
  t5->create_index(t5->field[0], /*column2=*/nullptr, /*unique=*/true);

  // Set up table sizes so that nested loop joins with REF(t2) and EQ_REF(t5) as
  // the innermost tables are attractive.
  m_fake_tables["t1"]->file->stats.records = 1;
  m_fake_tables["t2"]->file->stats.records = 1000;
  m_fake_tables["t3"]->file->stats.records = 1;
  m_fake_tables["t4"]->file->stats.records = 1;
  m_fake_tables["t5"]->file->stats.records = 1000;

  // Build a multiple equality from the WHERE condition:
  // t2.x = t3.x = t4.x = t5.x
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  EXPECT_EQ(1, cond_equal->current_level.size());
  const Item_equal *eq = cond_equal->current_level.head();
  EXPECT_EQ(nullptr, eq->const_arg());
  EXPECT_EQ(4, eq->get_fields().size());

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::SEMI, root->nested_loop_join().join_type);

  // The innermost table on the left side is a REF lookup subsuming the join
  // condition t2.x = t3.x.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->nested_loop_join().outer->type);
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN,
            root->nested_loop_join().outer->nested_loop_join().inner->type);
  EXPECT_EQ(AccessPath::REF, root->nested_loop_join()
                                 .outer->nested_loop_join()
                                 .inner->nested_loop_join()
                                 .inner->type);

  // The semi-join condition t2.x = t5.x is not redundant, so there should be a
  // filter for it in some form (it ends up as t3.x = t4.x due to multiple
  // equalities).
  ASSERT_EQ(AccessPath::FILTER, root->nested_loop_join().inner->type);
  EXPECT_EQ("((t3.x = t4.x) and (t4.y <> t1.y))",
            ItemToString(root->nested_loop_join().inner->filter().condition));

  // The innermost table on the right side is an EQ_REF lookup subsuming the
  // join condition t4.x = t5.x.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN,
            root->nested_loop_join().inner->filter().child->type);
  EXPECT_EQ(AccessPath::EQ_REF, root->nested_loop_join()
                                    .inner->filter()
                                    .child->nested_loop_join()
                                    .inner->type);
}

TEST_F(HypergraphOptimizerTest, SemijoinToInnerWithSargable) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2) "
      "AND t1.x IN (SELECT t3.x FROM t3)",
      /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  Fake_TABLE *t2 = m_fake_tables["t2"];
  Fake_TABLE *t3 = m_fake_tables["t3"];

  t2->create_index(t2->field[0], /*column2=*/nullptr, /*unique=*/false);

  t1->file->stats.records = 10;
  t2->file->stats.records = 100;
  t3->file->stats.records = 1000;

  // Build a multiple equality from the WHERE condition:
  // t1.x = t2.x = t3.x
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  EXPECT_EQ(1, cond_equal->current_level.size());
  const Item_equal *eq = cond_equal->current_level.head();
  EXPECT_EQ(nullptr, eq->const_arg());
  EXPECT_EQ(3, eq->get_fields().size());

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // We don't really care that much about which plan is chosen here. The main
  // thing we want to check, is that FindBestQueryPlan() didn't hit an assertion
  // because of inconsistent row estimates. The row estimates *are*
  // inconsistent, though, until bug#33550360 is fixed. The returned plan is
  // ((t1 semi-HJ t2) semi-HJ t3).
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  ASSERT_EQ(AccessPath::HASH_JOIN, root->hash_join().outer->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN,
            root->hash_join().outer->hash_join().outer->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN,
            root->hash_join().outer->hash_join().inner->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, root->hash_join().inner->type);
  EXPECT_STREQ(
      "t1",
      root->hash_join().outer->hash_join().outer->table_scan().table->alias);
  EXPECT_STREQ(
      "t2",
      root->hash_join().outer->hash_join().inner->table_scan().table->alias);
  EXPECT_STREQ("t3", root->hash_join().inner->table_scan().table->alias);
}

TEST_F(HypergraphOptimizerTest, SemijoinToInnerWithDegenerateJoinCondition) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE 1 IN (SELECT t2.x FROM t2)",
                      /*nullable=*/false);

  // Make the tables big so that building a hash table of one of them looks
  // expensive.
  m_fake_tables["t1"]->file->stats.records = 1000000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e8;
  m_fake_tables["t2"]->file->stats.records = 1000000;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e8;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect a nested-loop inner join using a limit on t2 to be preferred to a
  // hash semijoin.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);

  const AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::LIMIT_OFFSET, outer->type);
  EXPECT_EQ(0, outer->limit_offset().offset);
  EXPECT_EQ(1, outer->limit_offset().limit);
  ASSERT_EQ(AccessPath::FILTER, outer->limit_offset().child->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN,
            outer->limit_offset().child->filter().child->type);
  EXPECT_STREQ(
      "t2",
      outer->limit_offset().child->filter().child->table_scan().table->alias);

  const AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_STREQ("t1", inner->table_scan().table->alias);
}

/*
  Test a query with two multiple equalities on overlapping, but not identical,
  sets of tables, and where there is a hyperpredicate that references all of the
  tables in one of the multiple equalities.

  The presence of the hyperpredicate used to prevent addition of a cycle edge
  for the tables in the first multiple equality. If the tables in the
  hyperpredicate were joined together without following the hyperedge
  corresponding to the hyperpredicate, via an alternative edge provided by the
  second multiple equality, one application of the first multiple equality would
  be lost, and inconsistent row estimates were seen.

  Now, the presence of a hyperpredicate no longer prevents addition of a cycle
  edge. Both because of the inconsistencies that were seen in this test case,
  and because it turned out to be bad also for performance, as it blocked some
  valid and potentially cheaper join orders.
 */
TEST_F(HypergraphOptimizerTest, HyperpredicatesConsistentRowEstimates) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3, t4 WHERE "
      "t1.x = t2.x AND t2.x = t3.x AND "
      "t2.y = t3.y AND t3.y = t4.y AND "
      "t1.z + t2.z < t3.z",
      /*nullable=*/true);

  const ha_rows t1_rows = m_fake_tables["t1"]->file->stats.records = 1000;
  const ha_rows t2_rows = m_fake_tables["t2"]->file->stats.records = 1000;
  const ha_rows t3_rows = m_fake_tables["t3"]->file->stats.records = 10;
  const ha_rows t4_rows = m_fake_tables["t4"]->file->stats.records = 10;

  // Build two multiple equalities: t1.x = t2.x = t3.x and t2.y = t3.y = t4.y.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  EXPECT_EQ(2, cond_equal->current_level.size());

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  // We don't really care which plan is chosen. The main point is that
  // FindBestQueryPlan() above didn't fail with an assertion about inconsistent
  // row estimates, and that the row estimate here is as expected. (It used to
  // be too high because one of the multiple equalities was only applied once.
  // Both multiple equalities should be applied twice.)
  EXPECT_FLOAT_EQ(
      t1_rows * t2_rows * t3_rows * t4_rows *  // Input rows.
          powf(COND_FILTER_EQUALITY, 4) *      // Selectivity of equalities.
          COND_FILTER_ALLPASS,                 // Selectivity of hyperpredicate.
      root->num_output_rows());
}

TEST_F(HypergraphOptimizerTest, SwitchesOrderToMakeSafeForRowid) {
  // Mark t1.y as a blob, to make sure we need rowids for our sort.
  Mock_field_long t1_x(/*is_unsigned=*/false);
  Base_mock_field_blob t1_y("y", /*length=*/1000000);
  t1_x.field_name = "x";

  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&t1_x, &t1_y);
  m_fake_tables["t1"] = t1;

  t1->set_created();
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.y FROM t1 JOIN t2 ON t1.x=t2.x ORDER BY t1.y, t2.y",
      /*nullable=*/true);

  t1->create_index(t1->field[0], nullptr, /*unique=*/false);
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[0], nullptr, /*unique=*/false);

  // The normal case for rowid-unsafe tables are LATERAL derived tables,
  // but since we don't support derived tables in the unit test,
  // we cheat and mark t2 as unsafe for row IDs manually instead,
  // and also disallow hash join.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_flags =
      MakeSecondaryEngineFlags(SecondaryEngineFlag::SUPPORTS_NESTED_LOOP_JOIN);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        if (path->type == AccessPath::REF &&
            strcmp("t2", path->ref().table->alias) == 0) {
          path->safe_for_rowid = AccessPath::SAFE_IF_SCANNED_ONCE;
        }
        return false;
      };

  m_fake_tables["t1"]->file->stats.records = 99;
  m_fake_tables["t2"]->file->stats.records = 100;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Normally, it would be better to have t1 on the outside
  // and t2 on the inside, since t2 is the larger one, but that would create
  // a materialization, so the better version is to flip.
  ASSERT_EQ(AccessPath::SORT, root->type);
  AccessPath *join = root->sort().child;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, join->type);
  AccessPath *outer = join->nested_loop_join().outer;
  AccessPath *inner = join->nested_loop_join().inner;

  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_STREQ("t2", outer->table_scan().table->alias);

  ASSERT_EQ(AccessPath::REF, inner->type);
  EXPECT_STREQ("t1", inner->ref().table->alias);
}

// Test that a hash join can combine predicates from multiple edges in a cyclic
// hypergraph, and create a wider hash join key than what it gets from the
// single edge. (Previously, the eligible join predicates from other edges in
// the cycle were instead added as post-join filters.)
TEST_F(HypergraphOptimizerTest, MultiPredicateHashJoin) {
  // Test both regular equality and NULL-safe equality. Either kind of equality
  // can be used in the hash join key.
  for (const char *eq_op : {"=", "<=>"}) {
    const string query = StringPrintf(
        "SELECT 1 FROM t1, t2, t3 "
        "WHERE t1.x %s t2.x AND t2.y %s t3.y AND t1.z %s t3.z",
        eq_op, eq_op, eq_op);
    SCOPED_TRACE(query);
    Query_block *query_block = ParseAndResolve(query.data(),
                                               /*nullable=*/true);

    // Sizes that make (t1 HJ t2) HJ t3 the preferred join order.
    m_fake_tables["t1"]->file->stats.records = 90000;
    m_fake_tables["t1"]->file->stats.data_file_length = 9e7;
    m_fake_tables["t2"]->file->stats.records = 100;
    m_fake_tables["t2"]->file->stats.data_file_length = 1e3;
    m_fake_tables["t3"]->file->stats.records = 3000;
    m_fake_tables["t3"]->file->stats.data_file_length = 3e5;

    string trace;
    AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.
    // Prints out the query plan on failure.
    SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                                /*is_root_of_join=*/true));

    // The top-level path should be a HASH_JOIN with two equi-join predicates.
    // In earlier versions, the hash join had only one of the predicates, and
    // the other predicate was in a FILTER on top of it.
    ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
    EXPECT_EQ(0,
              root->hash_join().join_predicate->expr->join_conditions.size());
    {
      vector<string> equijoin_conditions;
      for (Item_eq_base *item :
           root->hash_join().join_predicate->expr->equijoin_conditions) {
        equijoin_conditions.push_back(ItemToString(item));
      }
      EXPECT_THAT(equijoin_conditions,
                  UnorderedElementsAre(StringPrintf("(t2.y %s t3.y)", eq_op),
                                       StringPrintf("(t1.z %s t3.z)", eq_op)));
    }

    ASSERT_EQ(AccessPath::HASH_JOIN, root->hash_join().outer->type);
    ASSERT_EQ(AccessPath::TABLE_SCAN, root->hash_join().inner->type);
    EXPECT_STREQ("t3", root->hash_join().inner->table_scan().table->alias);

    EXPECT_EQ(0, root->hash_join()
                     .outer->hash_join()
                     .join_predicate->expr->join_conditions.size());
    {
      const Mem_root_array<Item_eq_base *> &equijoin_conditions =
          root->hash_join()
              .outer->hash_join()
              .join_predicate->expr->equijoin_conditions;
      ASSERT_EQ(1, equijoin_conditions.size());
      EXPECT_EQ(StringPrintf("(t1.x %s t2.x)", eq_op),
                ItemToString(equijoin_conditions[0]));
    }

    ClearFakeTables();
  }
}

TEST_F(HypergraphOptimizerTest, HashJoinWithEquijoinHyperpredicate) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2, t3 WHERE t1.x = t2.x + t3.x AND t2.y = t3.y",
      /*nullable=*/true);

  // Sizes that make t1 HJ (t2 HJ t3) the preferred join order.
  m_fake_tables["t1"]->file->stats.records = 100000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1000e6;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;
  m_fake_tables["t3"]->file->stats.records = 10;
  m_fake_tables["t3"]->file->stats.data_file_length = 10e6;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The topmost path should be a HASH_JOIN with an equijoin predicate.
  // Previously, the hyperpredicate would be an "extra" condition, not an
  // equijoin condition.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  RelationalExpression *expr = root->hash_join().join_predicate->expr;
  EXPECT_EQ("(t1.x = (t2.x + t3.x))", ItemsToString(expr->equijoin_conditions));
  EXPECT_EQ("(none)", ItemsToString(expr->join_conditions));

  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_STREQ("t1", outer->table_scan().table->alias);

  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, inner->type);
  RelationalExpression *inner_expr = inner->hash_join().join_predicate->expr;
  EXPECT_EQ("(t2.y = t3.y)", ItemsToString(inner_expr->equijoin_conditions));
  EXPECT_EQ("(none)", ItemsToString(inner_expr->join_conditions));

  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->hash_join().outer->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->hash_join().inner->type);
  EXPECT_STREQ("t2", inner->hash_join().outer->table_scan().table->alias);
  EXPECT_STREQ("t3", inner->hash_join().inner->table_scan().table->alias);
}

TEST_F(HypergraphOptimizerTest, HashJoinWithNonEquijoinHyperpredicate) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 JOIN t3 ON t2.y=t3.y ON t1.x+t2.x=t3.x",
      /*nullable=*/true);

  // Sizes that make t1 HJ (t2 HJ t3) the preferred join order.
  m_fake_tables["t1"]->file->stats.records = 100000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1000e6;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;
  m_fake_tables["t3"]->file->stats.records = 10;
  m_fake_tables["t3"]->file->stats.data_file_length = 10e6;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The topmost path should be a HASH_JOIN with a non-equijoin hyperpredicate.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  RelationalExpression *expr = root->hash_join().join_predicate->expr;
  EXPECT_EQ("(none)", ItemsToString(expr->equijoin_conditions));
  EXPECT_EQ("((t1.x + t2.x) = t3.x)", ItemsToString(expr->join_conditions));

  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_STREQ("t1", outer->table_scan().table->alias);

  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, inner->type);
  RelationalExpression *inner_expr = inner->hash_join().join_predicate->expr;
  EXPECT_EQ("(t2.y = t3.y)", ItemsToString(inner_expr->equijoin_conditions));
  EXPECT_EQ("(none)", ItemsToString(inner_expr->join_conditions));

  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->hash_join().outer->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->hash_join().inner->type);
  EXPECT_STREQ("t2", inner->hash_join().outer->table_scan().table->alias);
  EXPECT_STREQ("t3", inner->hash_join().inner->table_scan().table->alias);
}

TEST_F(HypergraphOptimizerTest, HashJoinWithSubqueryPredicate) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 JOIN t3 ON (t1.y = t2.y)"
      "WHERE t3.x = t2.x and (t3.x > ALL (SELECT 4 FROM t4) OR (t3.y = t2.y))",
      /*nullable=*/true);

  // Resolve the subqueries too.
  for (Query_expression *expr = query_block->first_inner_query_expression();
       expr != nullptr; expr = expr->next_query_expression()) {
    Query_block *subquery = expr->first_query_block();
    ResolveQueryBlock(m_thd, subquery, /*nullable=*/true, &m_fake_tables);
    string trace;
    AccessPath *subquery_path =
        FindBestQueryPlanAndFinalize(m_thd, subquery, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.
    ASSERT_NE(nullptr, subquery_path);
  }

  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  // Sizes that make t1 HJ (t2 HJ t3) the preferred join order.
  m_fake_tables["t1"]->file->stats.records = 100000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1000e6;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;
  m_fake_tables["t3"]->file->stats.records = 10;
  m_fake_tables["t3"]->file->stats.data_file_length = 10e6;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  // The top-level path should be a filter access path with a
  // subquery. The subquery should not be moved to the join
  // predicates of the HASH JOIN.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ(
      ItemToString(root->filter().condition),
      "(<not>((t3.x <= (select #2))) or ((t1.y = t2.y) and (t2.y = t3.y)))");

  // Verify that we have (t1 HJ (t2 HJ t3 ON (t3.x = t2.x)) ON (t1.y= t2.y)))
  AccessPath *join = root->filter().child;
  ASSERT_EQ(AccessPath::HASH_JOIN, join->type);
  const Mem_root_array<Item_eq_base *> &equijoin_conditions_t1t2 =
      join->hash_join().join_predicate->expr->equijoin_conditions;
  EXPECT_EQ(1, equijoin_conditions_t1t2.size());
  EXPECT_EQ("(t1.y = t2.y)", ItemToString(equijoin_conditions_t1t2[0]));

  AccessPath *t1 = join->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_EQ(m_fake_tables["t1"], t1->table_scan().table);

  AccessPath *inner = join->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, inner->type);

  const Mem_root_array<Item_eq_base *> &equijoin_conditions_t2t3 =
      inner->hash_join().join_predicate->expr->equijoin_conditions;
  EXPECT_EQ(1, equijoin_conditions_t2t3.size());
  EXPECT_EQ("(t3.x = t2.x)", ItemToString(equijoin_conditions_t2t3[0]));

  AccessPath *t2 = inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_STREQ("t2", t2->table_scan().table->alias);
  AccessPath *t3 = inner->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t3->type);
  EXPECT_STREQ("t3", t3->table_scan().table->alias);
}

namespace {

struct FullTextParam {
  const char *query;
  bool expect_filter;
  bool expect_index;
};

std::ostream &operator<<(std::ostream &os, const FullTextParam &param) {
  return os << param.query;
}

}  // namespace

using HypergraphFullTextTest = OptimizerTestWithParam<FullTextParam>;

TEST_P(HypergraphFullTextTest, FullTextSearch) {
  SCOPED_TRACE(GetParam().query);

  // CREATE TABLE t1(x VARCHAR(100)).
  Base_mock_field_varstring column1(/*length=*/100, /*share=*/nullptr);
  column1.field_name = "x";
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&column1);
  t1->file->stats.records = 10000;
  m_fake_tables["t1"] = t1;
  t1->set_created();

  // CREATE FULLTEXT INDEX idx ON t1(x).
  down_cast<Mock_HANDLER *>(t1->file)->set_ha_table_flags(
      t1->file->ha_table_flags() | HA_CAN_FULLTEXT);
  t1->create_index(&column1, /*column2=*/nullptr, ulong{HA_FULLTEXT});

  Query_block *query_block = ParseAndResolve(GetParam().query,
                                             /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  AccessPath *path = root;

  if (GetParam().expect_filter) {
    ASSERT_EQ(AccessPath::FILTER, path->type);
    path = path->filter().child;
  }

  if (GetParam().expect_index) {
    ASSERT_EQ(AccessPath::FULL_TEXT_SEARCH, path->type);
    // Since there is no ORDER BY in the query, expect an unordered index scan.
    EXPECT_FALSE(query_block->is_ordered());
    EXPECT_FALSE(path->full_text_search().use_order);
  } else {
    EXPECT_EQ(AccessPath::TABLE_SCAN, path->type);
  }
}

static constexpr FullTextParam full_text_queries[] = {
    // Expect a full-text index scan if the predicate returns true for positive
    // scores only. Expect the index scan to have a filter on top of it if the
    // predicate does not return true for all non-zero scores.
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc' IN BOOLEAN MODE)",
     /*expect_filter=*/false,
     /*expect_index=*/true},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc')",
     /*expect_filter=*/false,
     /*expect_index=*/true},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') > 0",
     /*expect_filter=*/false,
     /*expect_index=*/true},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') > 0.5",
     /*expect_filter=*/true,
     /*expect_index=*/true},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') >= 0.5",
     /*expect_filter=*/true,
     /*expect_index=*/true},
    {"SELECT t1.x FROM t1 WHERE 0.5 < MATCH(t1.x) AGAINST ('abc')",
     /*expect_filter=*/true,
     /*expect_index=*/true},
    {"SELECT t1.x FROM t1 WHERE 0.5 <= MATCH(t1.x) AGAINST ('abc')",
     /*expect_filter=*/true,
     /*expect_index=*/true},

    // Expect a table scan if the predicate might return true for zero or
    // negative scores. A filter node is added on top for the predicate.
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') < 0.5",
     /*expect_filter=*/true,
     /*expect_index=*/false},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') <= 0.5",
     /*expect_filter=*/true,
     /*expect_index=*/false},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') >= 0",
     /*expect_filter=*/true,
     /*expect_index=*/false},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') > -1",
     /*expect_filter=*/true,
     /*expect_index=*/false},
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') <> 0.5",
     /*expect_filter=*/true,
     /*expect_index=*/false},
    {"SELECT t1.x FROM t1 WHERE 0.5 > MATCH(t1.x) AGAINST ('abc')",
     /*expect_filter=*/true,
     /*expect_index=*/false},
    {"SELECT t1.x FROM t1 WHERE 0.5 >= MATCH(t1.x) AGAINST ('abc')",
     /*expect_filter=*/true,
     /*expect_index=*/false},

    // Expect a table scan if the predicate checks for an exact score. (Not
    // because an index scan cannot be used, but because it's not a very useful
    // query, so we haven't optimized for it.)
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') = 0.5",
     /*expect_filter=*/true,
     /*expect_index=*/false},

    // Expect a table scan if the predicate is a disjunction.
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc' IN BOOLEAN MODE) "
     "OR MATCH(t1.x) AGAINST ('xyz' IN BOOLEAN MODE)",
     /*expect_filter=*/true,
     /*expect_index=*/false},

    // Expect an index scan if the predicate is a conjunction. A filter node
    // will be added for the predicate that is not subsumed by the index.
    {"SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc' IN BOOLEAN MODE) "
     "AND MATCH(t1.x) AGAINST ('xyz' IN BOOLEAN MODE)",
     /*expect_filter=*/true,
     /*expect_index=*/true},
};
INSTANTIATE_TEST_SUITE_P(FullTextQueries, HypergraphFullTextTest,
                         ::testing::ValuesIn(full_text_queries));

TEST_F(HypergraphOptimizerTest, FullTextSearchNoHashJoin) {
  // CREATE TABLE t1(x VARCHAR(100)).
  Base_mock_field_varstring column1(/*length=*/100, /*share=*/nullptr);
  column1.field_name = "x";
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&column1);
  m_fake_tables["t1"] = t1;
  t1->set_created();

  // CREATE FULLTEXT INDEX idx ON t1(x).
  down_cast<Mock_HANDLER *>(t1->file)->set_ha_table_flags(
      t1->file->ha_table_flags() | HA_CAN_FULLTEXT);
  t1->create_index(&column1, /*column2=*/nullptr, ulong{HA_FULLTEXT});

  Query_block *query_block = ParseAndResolve(
      "SELECT MATCH(t1.x) AGAINST ('abc') FROM t1, t2 WHERE t1.x = t2.x",
      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  // Add some rows to make a hash join more tempting than a nested loop join.
  m_fake_tables["t1"]->file->stats.records = 1000;
  m_fake_tables["t2"]->file->stats.records = 1000;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  // FTS does not work well with hash join, so we force nested loop join for
  // this query.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
}

TEST_F(HypergraphOptimizerTest, FullTextCanSkipRanking) {
  // CREATE TABLE t1(x VARCHAR(100)).
  Base_mock_field_varstring column1(/*length=*/100, /*share=*/nullptr);
  column1.field_name = "x";
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&column1);
  m_fake_tables["t1"] = t1;
  t1->set_created();

  // CREATE FULLTEXT INDEX idx ON t1(x).
  down_cast<Mock_HANDLER *>(t1->file)->set_ha_table_flags(
      t1->file->ha_table_flags() | HA_CAN_FULLTEXT);
  t1->create_index(&column1, /*column2=*/nullptr, ulong{HA_FULLTEXT});

  Query_block *query_block = ParseAndResolve(
      "SELECT MATCH(t1.x) AGAINST ('a') FROM t1 WHERE "
      "MATCH(t1.x) AGAINST ('a') AND "
      "MATCH(t1.x) AGAINST ('b') AND "
      "MATCH(t1.x) AGAINST ('c') AND MATCH(t1.x) AGAINST ('c') > 0.1",
      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  const List<Item_func_match> *ftfuncs = query_block->ftfunc_list;
  ASSERT_EQ(5, ftfuncs->size());

  // MATCH(t1.x) AGAINST ('a') needs ranking because it is used in the
  // SELECT list.
  EXPECT_EQ("(match t1.x against ('a'))", ItemToString((*ftfuncs)[0]));
  EXPECT_EQ(nullptr, (*ftfuncs)[0]->master);
  EXPECT_FALSE((*ftfuncs)[0]->can_skip_ranking());
  EXPECT_EQ((*ftfuncs)[0], (*ftfuncs)[1]->get_master());

  // MATCH (t1.x) AGAINST ('b') does not need ranking, since it's only used
  // in a standalone predicate.
  EXPECT_EQ("(match t1.x against ('b'))", ItemToString((*ftfuncs)[2]));
  EXPECT_EQ(nullptr, (*ftfuncs)[2]->master);
  EXPECT_TRUE((*ftfuncs)[2]->can_skip_ranking());

  // MATCH (t1.x) AGAINST ('c') needs ranking because one of the predicates
  // requires it to return > 0.1.
  EXPECT_EQ("(match t1.x against ('c'))", ItemToString((*ftfuncs)[3]));
  EXPECT_EQ(nullptr, (*ftfuncs)[3]->master);
  EXPECT_FALSE((*ftfuncs)[3]->can_skip_ranking());
  EXPECT_EQ((*ftfuncs)[3], (*ftfuncs)[4]->get_master());
}

TEST_F(HypergraphOptimizerTest, FullTextAvoidDescSort) {
  // CREATE TABLE t1(x VARCHAR(100)).
  Base_mock_field_varstring column1(/*length=*/100, /*share=*/nullptr);
  column1.field_name = "x";
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&column1);
  t1->file->stats.records = 10000;
  m_fake_tables["t1"] = t1;
  t1->set_created();

  // CREATE FULLTEXT INDEX idx ON t1(x).
  down_cast<Mock_HANDLER *>(t1->file)->set_ha_table_flags(
      t1->file->ha_table_flags() | HA_CAN_FULLTEXT);
  t1->create_index(&column1, /*column2=*/nullptr, ulong{HA_FULLTEXT});

  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') "
      "ORDER BY MATCH(t1.x) AGAINST ('abc') DESC",
      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  // Expect no sort in the plan. An ordered index scan is used.
  ASSERT_EQ(AccessPath::FULL_TEXT_SEARCH, root->type);
  EXPECT_TRUE(root->full_text_search().use_order);
}

TEST_F(HypergraphOptimizerTest, FullTextAscSort) {
  // CREATE TABLE t1(x VARCHAR(100)).
  Base_mock_field_varstring column1(/*length=*/100, /*share=*/nullptr);
  column1.field_name = "x";
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&column1);
  t1->file->stats.records = 10000;
  m_fake_tables["t1"] = t1;
  t1->set_created();

  // CREATE FULLTEXT INDEX idx ON t1(x).
  down_cast<Mock_HANDLER *>(t1->file)->set_ha_table_flags(
      t1->file->ha_table_flags() | HA_CAN_FULLTEXT);
  t1->create_index(&column1, /*column2=*/nullptr, ulong{HA_FULLTEXT});

  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x FROM t1 WHERE MATCH(t1.x) AGAINST ('abc') "
      "ORDER BY MATCH(t1.x) AGAINST ('abc') ASC",
      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  // The full-text index can only return results in descending order, so expect
  // a SORT node on top.
  EXPECT_EQ(AccessPath::SORT, root->type);
}

TEST_F(HypergraphOptimizerTest, FullTextDescSortNoPredicate) {
  // CREATE TABLE t1(x VARCHAR(100)).
  Base_mock_field_varstring column1(/*length=*/100, /*share=*/nullptr);
  column1.field_name = "x";
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&column1);
  t1->file->stats.records = 10000;
  m_fake_tables["t1"] = t1;
  t1->set_created();

  // CREATE FULLTEXT INDEX idx ON t1(x).
  down_cast<Mock_HANDLER *>(t1->file)->set_ha_table_flags(
      t1->file->ha_table_flags() | HA_CAN_FULLTEXT);
  t1->create_index(&column1, /*column2=*/nullptr, ulong{HA_FULLTEXT});

  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x FROM t1 ORDER BY MATCH(t1.x) AGAINST ('abc') DESC",
      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  // A full-text index scan cannot be used for ordering when there is no
  // predicate, since the index scan doesn't return all rows (only those with a
  // positive score). Expect a SORT node on top.
  EXPECT_EQ(AccessPath::SORT, root->type);
}

TEST_F(HypergraphOptimizerTest, DistinctIsDoneAsSort) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y, t1.x FROM t1", /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::SORT, root->type);
  Filesort *sort = root->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t1.y", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[1].item));
  EXPECT_TRUE(sort->m_remove_duplicates);

  EXPECT_EQ(AccessPath::TABLE_SCAN, root->sort().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, DistinctIsSubsumedByGroup) {
  Query_block *query_block = ParseAndResolve(
      "SELECT DISTINCT t1.y, t1.x, 3 FROM t1 GROUP BY t1.x, t1.y",
      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::AGGREGATE, root->type);
  AccessPath *child = root->aggregate().child;

  EXPECT_EQ(AccessPath::SORT, child->type);
  EXPECT_FALSE(child->sort().filesort->m_remove_duplicates);
}

TEST_F(HypergraphOptimizerTest, DistinctWithOrderBy) {
  m_thd->variables.sql_mode &= ~MODE_ONLY_FULL_GROUP_BY;
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y FROM t1 ORDER BY t1.x, t1.y",
                      /*nullable=*/true);
  m_thd->variables.sql_mode |= MODE_ONLY_FULL_GROUP_BY;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::SORT, root->type);
  Filesort *sort = root->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t1.y", ItemToString(sort->sortorder[1].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  // We can't coalesce the two sorts, due to the deduplication in this step.
  AccessPath *child = root->sort().child;
  ASSERT_EQ(AccessPath::SORT, child->type);
  Filesort *sort2 = child->sort().filesort;
  ASSERT_EQ(1, sort2->sort_order_length());
  EXPECT_EQ("t1.y", ItemToString(sort2->sortorder[0].item));
  EXPECT_TRUE(sort2->m_remove_duplicates);

  EXPECT_EQ(AccessPath::TABLE_SCAN, child->sort().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, DistinctSubsumesOrderBy) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y, t1.x FROM t1 ORDER BY t1.x",
                      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::SORT, root->type);
  Filesort *sort = root->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t1.y", ItemToString(sort->sortorder[1].item));
  EXPECT_TRUE(sort->m_remove_duplicates);

  // No separate sort for ORDER BY.
  EXPECT_EQ(AccessPath::TABLE_SCAN, root->sort().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SortAheadSingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x, t2.x FROM t1, t2 ORDER BY t2.x",
                      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);

  // The sort should be on t2, which should be on the outer side.
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::SORT, outer->type);
  Filesort *sort = outer->sort().filesort;
  ASSERT_EQ(1, sort->sort_order_length());
  EXPECT_EQ("t2.x", ItemToString(sort->sortorder[0].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  AccessPath *outer_child = outer->sort().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer_child->type);
  EXPECT_STREQ("t2", outer_child->table_scan().table->alias);

  // The inner side should just be t1, no sort.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_STREQ("t1", inner->table_scan().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, CannotSortAheadBeforeBothTablesAreAvailable) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x, t2.x FROM t1, t2 ORDER BY t1.x, t2.x",
                      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The sort should be at the root, because the sort cannot be pushed
  // to e.g. t2 (unlike in the previous test); t1.x isn't available yet.
  ASSERT_EQ(AccessPath::SORT, root->type);

  // Check that there is no pushed sort in the tree.
  WalkAccessPaths(root->sort().child, /*join=*/nullptr,
                  WalkAccessPathPolicy::ENTIRE_TREE,
                  [&](const AccessPath *path, const JOIN *) {
                    EXPECT_NE(AccessPath::SORT, path->type);
                    return false;
                  });

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SortAheadTwoTables) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t2.x, t3.x FROM t1, t2, t3 ORDER BY t1.x, t2.x",
      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t3"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t3"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);

  // There should be a sort pushed down, with t1 and t2 below.
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::SORT, outer->type);
  Filesort *sort = outer->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t2.x", ItemToString(sort->sortorder[1].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  // We don't check that t1 and t2 are actually below there
  // (and we don't care about the join type chosen, even though
  // it should usually be hash join), but we do check
  // that there are no more sorts.
  WalkAccessPaths(outer->sort().child, /*join=*/nullptr,
                  WalkAccessPathPolicy::ENTIRE_TREE,
                  [&](const AccessPath *path, const JOIN *) {
                    EXPECT_NE(AccessPath::SORT, path->type);
                    return false;
                  });

  // The inner side should just be t3, no sort.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_STREQ("t3", inner->table_scan().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, NoSortAheadOnNondeterministicFunction) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x, t2.x FROM t1, t2 ORDER BY t1.x + RAND()",
                      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The sort should _not_ be pushed to t1, but kept at the top.
  // We don't care about the rest of the plan.
  ASSERT_EQ(AccessPath::SORT, root->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SortAheadDueToEquivalence) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t2.x FROM t1 JOIN t2 ON t1.x=t2.x ORDER BY t1.x, t2.x "
      "LIMIT 10",
      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::LIMIT_OFFSET, root->type);
  EXPECT_EQ(10, root->limit_offset().limit);

  // There should be no sort at the limit; join directly.
  AccessPath *join = root->limit_offset().child;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, join->type);

  // The outer side should have a sort, on t1 only.
  AccessPath *outer = join->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::SORT, outer->type);
  Filesort *sort = outer->sort().filesort;
  ASSERT_EQ(1, sort->sort_order_length());
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[0].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  // And it should indeed be t1 that is sorted, since it's the
  // smallest one.
  AccessPath *t1 = outer->sort().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_STREQ("t1", t1->table_scan().table->alias);

  // The inner side should be t2, with the join condition as filter.
  AccessPath *inner = join->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::FILTER, inner->type);
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(inner->filter().condition));

  AccessPath *t2 = inner->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_STREQ("t2", t2->table_scan().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SortAheadDueToUniqueIndex) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t2.x FROM t1 JOIN t2 ON t1.x=t2.x "
      "ORDER BY t1.x, t2.x, t2.y LIMIT 10",
      /*nullable=*/true);

  // Create a unique index on t2.x. This means that t2.y is now
  // redundant, and can (will) be reduced away when creating the homogenized
  // order.
  m_fake_tables["t2"]->create_index(m_fake_tables["t2"]->field[0],
                                    /*column2=*/nullptr, /*unique=*/true);

  m_fake_tables["t1"]->file->stats.records = 200;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 2e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::LIMIT_OFFSET, root->type);
  EXPECT_EQ(10, root->limit_offset().limit);

  // There should be no sort at the limit; join directly.
  AccessPath *join = root->limit_offset().child;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, join->type);

  // The outer side should have a sort, on t1 only.
  AccessPath *outer = join->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::SORT, outer->type);
  Filesort *sort = outer->sort().filesort;
  ASSERT_EQ(1, sort->sort_order_length());
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[0].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  // And it should indeed be t1 that is sorted, since it's the
  // smallest one.
  AccessPath *t1 = outer->sort().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t1->type);
  EXPECT_STREQ("t1", t1->table_scan().table->alias);

  // The inner side should be t2, with the join condition pushed down into an
  // EQ_REF.
  AccessPath *inner = join->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::EQ_REF, inner->type);
  EXPECT_STREQ("t2", inner->eq_ref().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, NoSortAheadOnNonUniqueIndex) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t2.x FROM t1 JOIN t2 ON t1.x=t2.x "
      "ORDER BY t1.x, t2.x, t2.y LIMIT 10",
      /*nullable=*/true);

  // With a non-unique index, there is no functional dependency,
  // and we should resort to sorting the largest table (t2).
  // The rest of the test is equal to SortAheadDueToUniqueIndex,
  // and we don't really verify it.
  m_fake_tables["t2"]->create_index(m_fake_tables["t2"]->field[0],
                                    /*column2=*/nullptr, /*unique=*/false);

  m_fake_tables["t1"]->file->stats.records = 200;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 2e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::LIMIT_OFFSET, root->type);
  EXPECT_EQ(10, root->limit_offset().limit);

  AccessPath *join = root->limit_offset().child;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, join->type);

  // The outer side should have a sort, on t2 only.
  AccessPath *outer = join->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::SORT, outer->type);
  Filesort *sort = outer->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t2.x", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t2.y", ItemToString(sort->sortorder[1].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ElideSortDueToBaseFilters) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t1.y FROM t1 WHERE t1.x=3 ORDER BY t1.x, t1.y",
      /*nullable=*/true);

  m_fake_tables["t1"]->create_index(m_fake_tables["t1"]->field[0],
                                    /*column2=*/nullptr, /*unique=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The sort should be elided entirely due to the unique index
  // and the constant lookup.
  ASSERT_EQ(AccessPath::EQ_REF, root->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ElideSortDueToDelayedFilters) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t1.y FROM t1 LEFT JOIN t2 ON t1.y=t2.y WHERE t2.x IS NULL "
      "ORDER BY t2.x, t2.y ",
      /*nullable=*/true);

  m_fake_tables["t2"]->create_index(m_fake_tables["t2"]->field[0],
                                    /*column2=*/nullptr, /*unique=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // We should have the IS NULL at the root, and no sort, due to the
  // functional dependency from t2.x to t2.y.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("(t2.x is null)", ItemToString(root->filter().condition));
  WalkAccessPaths(root->filter().child, /*join=*/nullptr,
                  WalkAccessPathPolicy::ENTIRE_TREE,
                  [&](const AccessPath *path, const JOIN *) {
                    EXPECT_NE(AccessPath::SORT, path->type);
                    return false;
                  });

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ElideSortDueToIndex) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1 ORDER BY t1.x DESC",
                      /*nullable=*/true);

  m_fake_tables["t1"]->create_index(m_fake_tables["t1"]->field[0],
                                    /*column2=*/nullptr, /*unique=*/false);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;

  // Mark the index as returning ordered results.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_ORDER | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The sort should be elided entirely due to index.
  ASSERT_EQ(AccessPath::INDEX_SCAN, root->type);
  EXPECT_STREQ("t1", root->index_scan().table->alias);
  EXPECT_EQ(0, root->index_scan().idx);
  EXPECT_TRUE(root->index_scan().use_order);
  EXPECT_TRUE(root->index_scan().reverse);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ElideConstSort) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1 ORDER BY 'a', 'b', CONCAT('c')",
                      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The sort should be elided entirely.
  ASSERT_EQ(AccessPath::TABLE_SCAN, root->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ElideRedundantPartsOfSortKey) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE t1.x = t2.x "
      "ORDER BY t1.x, t2.x, 'abc', t1.y, t2.y",
      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::SORT, root->type);

  // Expect redundant elements to be removed from the sort key. t2.x is
  // redundant because of t1.x and the functional dependency t1.x = t2.x. The
  // constant 'abc' does not contribute to the ordering because it has the same
  // value in all rows, and is also removed.
  vector<string> order_items;
  for (const ORDER *order = root->sort().order; order != nullptr;
       order = order->next) {
    order_items.push_back(ItemToString(*order->item));
  }
  EXPECT_THAT(order_items, ElementsAre("t1.x", "t1.y", "t2.y"));

  // Expect the redundant elements to be removed from join->order as well.
  EXPECT_EQ(query_block->join->order.order, root->sort().order);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ElideRedundantSortAfterGrouping) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x = t2.x WHERE t2.x IS NULL "
      "GROUP BY t1.x ORDER BY t2.x",
      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect that there is no SORT on top of the AGGREGATE node, because the
  // ordering requested by the ORDER BY clause is ensured by the predicate.
  EXPECT_EQ(AccessPath::AGGREGATE, root->type);

  // The ORDER BY clause should be optimized away altogether.
  EXPECT_EQ(nullptr, query_block->join->order.order);
}

TEST_F(HypergraphOptimizerTest, ElideRedundantSortForDistinct) {
  Query_block *query_block = ParseAndResolve(
      "SELECT DISTINCT t2.x FROM t1 LEFT JOIN t2 ON t1.x = t2.x "
      "WHERE t2.x IS NULL",
      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Expect that there is no SORT for DISTINCT. Since the filter ensures that
  // all rows have the same value, duplicate elimination can be done by adding
  // LIMIT 1 on top of the filter.
  ASSERT_EQ(AccessPath::LIMIT_OFFSET, root->type);
  EXPECT_EQ(0, root->limit_offset().offset);
  EXPECT_EQ(1, root->limit_offset().limit);
  ASSERT_EQ(AccessPath::FILTER, root->limit_offset().child->type);
  EXPECT_EQ("(t2.x is null)",
            ItemToString(root->limit_offset().child->filter().condition));
}

// This case is tricky; the order given by the index is (x, y), but the
// interesting order is just (y). Normally, we only grow orders into interesting
// orders, but here, we have to reduce them as well.
TEST_F(HypergraphOptimizerTest, IndexTailGetsUsed) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x, t1.y FROM t1 WHERE t1.x=42 ORDER BY t1.y",
                      /*nullable=*/true);

  m_fake_tables["t1"]->create_index(m_fake_tables["t1"]->field[0],
                                    m_fake_tables["t1"]->field[1],
                                    /*unique=*/false);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;

  // Mark the index as returning ordered results.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_ORDER | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The sort should be elided entirely due to index.
  ASSERT_EQ(AccessPath::REF, root->type);
  EXPECT_STREQ("t1", root->ref().table->alias);
  EXPECT_EQ(0, root->ref().ref->key);
  EXPECT_EQ(true, root->ref().use_order);
  EXPECT_EQ(false, root->ref().reverse);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SortAheadByCoverToElideSortForGroup) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x FROM t1, t2 GROUP BY t1.x, t1.y ORDER BY t1.y DESC",
      /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The root should be a group, and it should _not_ have a sort beneath it
  // (it should be elided due to sortahead).
  ASSERT_EQ(AccessPath::AGGREGATE, root->type);
  AccessPath *join = root->aggregate().child;
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, join->type);
  AccessPath *outer = join->nested_loop_join().outer;

  // The outer table should be sorted on (y↓, x); it is compatible with the
  // grouping (even though it was on {x, y}), and also compatible with the
  // ordering.
  ASSERT_EQ(AccessPath::SORT, outer->type);
  Filesort *filesort = outer->sort().filesort;
  ASSERT_EQ(2, filesort->sort_order_length());
  EXPECT_EQ("t1.y", ItemToString(filesort->sortorder[0].item));
  EXPECT_TRUE(filesort->sortorder[0].reverse);
  EXPECT_EQ("t1.x", ItemToString(filesort->sortorder[1].item));
  EXPECT_FALSE(filesort->sortorder[1].reverse);

  // We don't test the inner side.

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SatisfyGroupByWithIndex) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1 GROUP BY t1.x",
                      /*nullable=*/true);

  m_fake_tables["t1"]->create_index(m_fake_tables["t1"]->field[0],
                                    /*column2=*/nullptr, /*unique=*/false);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;

  // Mark the index as returning ordered results.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_ORDER | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The root is a group node, of course.
  ASSERT_EQ(AccessPath::AGGREGATE, root->type);
  AccessPath *inner = root->aggregate().child;

  // The grouping should be taking care of by the ordered index.
  EXPECT_EQ(AccessPath::INDEX_SCAN, inner->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SatisfyGroupingForDistinctWithIndex) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y, t1.x FROM t1",
                      /*nullable=*/true);

  m_fake_tables["t1"]->create_index(m_fake_tables["t1"]->field[0],
                                    m_fake_tables["t1"]->field[1],
                                    /*unique=*/false);
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;

  // Mark the index as returning ordered results.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_ORDER | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The root should be a duplicate removal node; no sort.
  // Order of the group items doesn't matter.
  ASSERT_EQ(AccessPath::REMOVE_DUPLICATES, root->type);
  ASSERT_EQ(2, root->remove_duplicates().group_items_size);
  EXPECT_EQ("t1.y", ItemToString(root->remove_duplicates().group_items[0]));
  EXPECT_EQ("t1.x", ItemToString(root->remove_duplicates().group_items[1]));

  // The grouping should be taking care of by the ordered index.
  AccessPath *inner = root->remove_duplicates().child;
  EXPECT_EQ(AccessPath::INDEX_SCAN, inner->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SemiJoinThroughLooseScan) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2)",
                      /*nullable=*/true);

  // Make t1 large and with a relevant index, and t2 small
  // and with none. The best plan then will be to remove
  // duplicates from t2 and then do lookups into t1.
  m_fake_tables["t1"]->create_index(m_fake_tables["t1"]->field[0],
                                    /*column2=*/nullptr,
                                    /*unique=*/true);
  m_fake_tables["t1"]->file->stats.records = 1000000;
  m_fake_tables["t1"]->file->stats.data_file_length = 10000e6;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The join should be changed to an _inner_ join, and the inner side
  // should be an EQ_REF on t1.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);

  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::EQ_REF, inner->type);
  EXPECT_STREQ("t1", inner->eq_ref().table->alias);

  // The outer side is slightly trickier. There should first be
  // a duplicate removal on the join key...
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::REMOVE_DUPLICATES, outer->type);
  ASSERT_EQ(1, outer->remove_duplicates().group_items_size);
  EXPECT_EQ("t2.x", ItemToString(outer->remove_duplicates().group_items[0]));

  // ...then a sort to get the grouping...
  AccessPath *sort = outer->remove_duplicates().child;
  ASSERT_EQ(AccessPath::SORT, sort->type);
  Filesort *filesort = sort->sort().filesort;
  ASSERT_EQ(1, filesort->sort_order_length());
  EXPECT_EQ("t2.x", ItemToString(filesort->sortorder[0].item));

  // Note that ideally, we'd have true here instead of the duplicate removal,
  // but we can't track duplicates-removed status through AccessPaths yet.
  EXPECT_FALSE(filesort->m_remove_duplicates);

  // ...and then finally a table scan.
  AccessPath *t2 = sort->sort().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_STREQ("t2", t2->table_scan().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ImpossibleJoinConditionGivesZeroRows) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 JOIN t3 ON t2.x=t3.x AND 1=2) ON "
      "t1.x=t2.x",
      /*nullable=*/false);

  // We don't need any statistics; the best plan is quite obvious.
  // But we'd like to confirm the estimated row count for the join.
  m_fake_tables["t1"]->file->stats.records = 10;
  m_fake_tables["t2"]->file->stats.records = 1000;
  m_fake_tables["t3"]->file->stats.records = 1000;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Since there are no rows on the right side, we should have a nested loop
  // with t1 on the left side.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::OUTER, root->nested_loop_join().join_type);
  EXPECT_FLOAT_EQ(10.0F, root->num_output_rows());

  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_STREQ("t1", outer->table_scan().table->alias);

  // On the right side, we should have pushed _up_ the 1=2 condition,
  // and seen that it kills all the rows on the right side.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::ZERO_ROWS, inner->type);

  // Just verify that we indeed have a join under there.
  // (It is needed to get the zero row flags set on t2 and t3.)
  EXPECT_EQ(AccessPath::NESTED_LOOP_JOIN, inner->zero_rows().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ImpossibleWhereInJoinGivesZeroRows) {
  // Test a query with an impossible WHERE clause. Add aggregation and ordering
  // and various extra filters to see that the entire query is optimized away.
  // It used to optimize away only the access to the t2 table, and keep the
  // paths for joining, aggregation, sorting, etc on top of the ZERO_ROWS path.
  Query_block *query_block = ParseAndResolve(
      "SELECT MAX(t1.y) FROM t1 LEFT JOIN t2 ON t1.x = t2.x "
      "WHERE t2.y IS NULL AND t2.y IN (1, 2) AND RAND(0) < 0.5 "
      "GROUP BY t1.x HAVING MAX(t1.y) > 0 "
      "ORDER BY MAX(t1.y) LIMIT 20 OFFSET 10",
      /*nullable=*/false);

  // Create an index on t2.y so that the range optimizer analyzes the WHERE
  // clause and detects that it always evaluates to FALSE.
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[1],
                   /*column2=*/nullptr,
                   /*unique=*/false);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  EXPECT_EQ(AccessPath::ZERO_ROWS, root->type);
}

TEST_F(HypergraphOptimizerTest, ImpossibleRangeInJoinWithFilterAndAggregation) {
  // Test a query with an impossible range condition (t2.y IS NULL AND t2.y IN
  // (1, 2)) and a non-pushable condition that has to stay in a post-join filter
  // (RAND(0) < 0.5), and which is implicitly grouped so that it has to return
  // one row even if the join result is empty. Optimizing this query used to hit
  // an assert failure due to inconsistent cost estimates.
  Query_block *query_block = ParseAndResolve(
      "SELECT COUNT(*) FROM t1, t2 WHERE t1.x = t2.x AND "
      "t2.y IS NULL AND t2.y IN (1, 2) AND RAND(0) < 0.5",
      /*nullable=*/true);

  // Create an index on t2.y so that the range optimizer analyzes the WHERE
  // clause and detects that it always evaluates to FALSE.
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[1],
                   /*column2=*/nullptr,
                   /*unique=*/false);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  EXPECT_EQ(AccessPath::ZERO_ROWS_AGGREGATED, root->type);
}

TEST_F(HypergraphOptimizerTest, SimpleRangeScan) {
  Query_block *query_block = ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x < 3",
                                             /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);

  // Mark the index as supporting range scans.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::INDEX_RANGE_SCAN, root->type);
  EXPECT_EQ(0, root->index_range_scan().index);
  // HA_MRR_SUPPORT_SORTED and HA_MRR_USE_DEFAULT_IMPL are added by the handler,
  // not by the optimizer.
  EXPECT_EQ(
      HA_MRR_SUPPORT_SORTED | HA_MRR_USE_DEFAULT_IMPL | HA_MRR_NO_ASSOCIATION,
      root->index_range_scan().mrr_flags);
  ASSERT_EQ(1, root->index_range_scan().num_ranges);
  EXPECT_EQ(NO_MIN_RANGE | NEAR_MAX, root->index_range_scan().ranges[0]->flag);
  string_view max_key{
      pointer_cast<char *>(root->index_range_scan().ranges[0]->max_key),
      root->index_range_scan().ranges[0]->max_length};
  EXPECT_EQ("\x03\x00\x00\x00"sv, max_key);
  EXPECT_FALSE(root->index_range_scan().reverse);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ComplexMultipartRangeScan) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 "
      "WHERE (t1.x < 3 OR t1.x = 5) AND SQRT(t1.x) > 3 AND t1.y >= 15",
      /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->create_index(t1->field[0], t1->field[1], /*unique=*/false);

  // Mark the index as supporting range scans.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // sqrt(t1.x) > 3 isn't doable as a range scan (since we never do algebraic
  // rewrites). The other predicate on t1.x is subsumed, and should not be part
  // of the filter. (t1.x < 3 AND t1.y >= 15) is not representable as a range
  // scan (it gets truncated to just t1.x < 3 for the range), and thus,
  // t1.y >= 15 should also not be subsumed.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("((t1.y >= 15) and (sqrt(t1.x) > 3))",
            ItemToString(root->filter().condition));

  AccessPath *range_scan = root->filter().child;
  ASSERT_EQ(AccessPath::INDEX_RANGE_SCAN, range_scan->type);
  EXPECT_EQ(0, range_scan->index_range_scan().index);
  // HA_MRR_SUPPORT_SORTED and HA_MRR_USE_DEFAULT_IMPL are added by the handler,
  // not by the optimizer.
  EXPECT_EQ(
      HA_MRR_SUPPORT_SORTED | HA_MRR_USE_DEFAULT_IMPL | HA_MRR_NO_ASSOCIATION,
      range_scan->index_range_scan().mrr_flags);
  ASSERT_EQ(2, range_scan->index_range_scan().num_ranges);

  // t1.x < 3 (same as previous test).
  EXPECT_EQ(NO_MIN_RANGE | NEAR_MAX,
            range_scan->index_range_scan().ranges[0]->flag);
  string_view max_key_0{
      pointer_cast<char *>(range_scan->index_range_scan().ranges[0]->max_key),
      range_scan->index_range_scan().ranges[0]->max_length};
  EXPECT_EQ("\x03\x00\x00\x00"sv, max_key_0);

  // t1.x = 5 AND t1.y >= 15 (represented as (x,y) >= (5,15) and (x) <= (5));
  // even though we couldn't fit t1.y >= 15 into the last keypart, it should be
  // included here.
  EXPECT_EQ(0, range_scan->index_range_scan().ranges[1]->flag);
  string_view min_key_1{
      pointer_cast<char *>(range_scan->index_range_scan().ranges[1]->min_key),
      range_scan->index_range_scan().ranges[1]->min_length};
  string_view max_key_1{
      pointer_cast<char *>(range_scan->index_range_scan().ranges[1]->max_key),
      range_scan->index_range_scan().ranges[1]->max_length};
  EXPECT_EQ("\x05\x00\x00\x00\x0f\x00\x00\x00"sv, min_key_1);
  EXPECT_EQ("\x05\x00\x00\x00"sv, max_key_1);

  // It would have been nice to verify here that the filter had a lower
  // output row count than the range scan, due to sqrt(x) > 3 not being
  // part of the range scan. However, the returned selectivity for such
  // estimates is always 1.0, so it's not really visible. Instead, we simply
  // check that both are reasonably sane.
  EXPECT_GT(range_scan->num_output_rows(), 0.0);
  EXPECT_GE(root->num_output_rows(), range_scan->num_output_rows());

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, RangeScanWithReverseOrdering) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x < 3 ORDER BY t1.x DESC",
                      /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);

  // Mark the index as supporting range scans _and_ ordering.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(
          Return(HA_READ_RANGE | HA_READ_ORDER | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::INDEX_RANGE_SCAN, root->type);
  EXPECT_EQ(0, root->index_range_scan().index);
  // We need sorted output, in reverse.
  // HA_MRR_SUPPORT_SORTED and HA_MRR_USE_DEFAULT_IMPL are added by the handler,
  // not by the optimizer.
  EXPECT_EQ(HA_MRR_SUPPORT_SORTED | HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED |
                HA_MRR_NO_ASSOCIATION,
            root->index_range_scan().mrr_flags);
  EXPECT_TRUE(root->index_range_scan().reverse);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ImpossibleRange) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x < 3 AND t1.x > 5",
                      /*nullable=*/false);

  // We need an index, or we would never analyze ranges on t1.x.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);
  ON_CALL(*down_cast<Mock_HANDLER *>(t1->file), index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  EXPECT_EQ(AccessPath::ZERO_ROWS, root->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ImpossibleRangeWithOverflowBitset) {
  // We want to test a query that has an impossible range and enough predicates
  // that they don't fit in an inlined OverflowBitset in the zero-rows access
  // path. We need at least 64 predicates to make OverflowBitset overflow. Also
  // add a join to the query, since an assert failure was seen when proposing a
  // join path with a table with an always false range condition on one of the
  // sides when the number of predicates exceeded what could fit in an inlined
  // OverflowBitset.
  constexpr int number_of_predicates = 70;
  string query =
      "SELECT 1 FROM t1, t2 WHERE t1.x >= 2 AND t1.x <= 1 AND t1.y = t2.y";
  for (int i = 2; i < number_of_predicates; ++i) {
    query += " AND t1.z <> " + to_string(i);
  }

  Query_block *query_block = ParseAndResolve(query.c_str(),
                                             /*nullable=*/false);

  // Add an index on t1.x so that we try a range scan on the
  // impossible range (x >= 2 AND x <= 1).
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);
  ON_CALL(*down_cast<Mock_HANDLER *>(t1->file), index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  EXPECT_EQ(AccessPath::ZERO_ROWS, root->type);
}

TEST_F(HypergraphOptimizerTest, IndexMerge) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x < 3 OR t1.y > 4",
                      /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);
  t1->create_index(t1->field[1], nullptr, /*unique=*/false);

  // Mark the index as supporting range scans.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // No filter; it should be subsumed.
  ASSERT_EQ(AccessPath::INDEX_MERGE, root->type);
  ASSERT_EQ(2, root->index_merge().children->size());

  // t1.x < 3; we don't bother checking the other range, since it's so tedious.
  AccessPath *child0 = (*root->index_merge().children)[0];
  ASSERT_EQ(AccessPath::INDEX_RANGE_SCAN, child0->type);
  ASSERT_EQ(1, child0->index_range_scan().num_ranges);
  EXPECT_EQ(NO_MIN_RANGE | NEAR_MAX,
            child0->index_range_scan().ranges[0]->flag);
  string_view max_key{
      pointer_cast<char *>(child0->index_range_scan().ranges[0]->max_key),
      child0->index_range_scan().ranges[0]->max_length};
  EXPECT_EQ("\x03\x00\x00\x00"sv, max_key);

  AccessPath *child1 = (*root->index_merge().children)[1];
  EXPECT_EQ(AccessPath::INDEX_RANGE_SCAN, child1->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, IndexMergeSubsumesOnlyOnePredicate) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE (t1.x < 3 OR t1.y > 4) AND (t1.y > 0 OR t1.z > "
      "0)",
      /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->create_index(t1->field[0], nullptr, /*unique=*/false);
  t1->create_index(t1->field[1], nullptr, /*unique=*/false);

  // Mark the index as supporting range scans.
  ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
          index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The second predicate should not be subsumed, so we have a filter.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("((t1.y > 0) or (t1.z > 0))",
            ItemToString(root->filter().condition));
  EXPECT_EQ(AccessPath::INDEX_MERGE, root->filter().child->type);

  query_block->cleanup(/*full=*/true);
}

// Tests a case where we have the choice between an index range scan on a set of
// predicates and an index merge scan on another set of predicates. When the
// index range scan is chosen, the index merge predicates must be in a filter on
// top of the range scan. Before bug#34173949, the filter was missing.
TEST_F(HypergraphOptimizerTest, DontSubsumeIndexMergePredicateInRangeScan) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x, t1.y FROM t1 WHERE t1.x IN (71, 255) AND t1.y <> 115 AND "
      "(t1.y = 6 OR t1.x = 29)",
      /*nullable=*/false);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->file->stats.data_file_length = 1e6;

  // Create indexes on (x, y) and on (y).
  EXPECT_EQ(0, t1->create_index(t1->field[0], t1->field[1], /*unique=*/false));
  EXPECT_EQ(1, t1->create_index(t1->field[1], nullptr, /*unique=*/false));

  // Mark the indexes as supporting range scans.
  Mock_HANDLER *handler = down_cast<Mock_HANDLER *>(t1->file);
  EXPECT_CALL(*handler, index_flags)
      .WillRepeatedly(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  // Report smaller ranges in the (x, y) index than in the (y) index, so that a
  // range scan on (x, y) is preferred to a range scan on (y). And also
  // preferred to an index merge or a table scan.
  EXPECT_CALL(*handler, records_in_range(0, _, _)).WillRepeatedly(Return(1));
  EXPECT_CALL(*handler, records_in_range(1, _, _)).WillRepeatedly(Return(10));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The predicate that could have been used for an index merge should be in a
  // filter on top.
  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ("((t1.y = 6) or (t1.x = 29))",
            ItemToString(root->filter().condition));

  // An index range scan has subsumed the rest of the predicates as:
  // (x = 71 AND y < 115) OR (x = 71 AND 115 < y) OR
  // (x = 255 AND y < 115) OR (x = 255 AND 115 < y)
  ASSERT_EQ(AccessPath::INDEX_RANGE_SCAN, root->filter().child->type);
  const auto range_scan = root->filter().child->index_range_scan();
  EXPECT_EQ(0, range_scan.index);
  EXPECT_EQ(4, range_scan.num_ranges);
}

// Test that an index merge doesn't subsume a range predicate that it is AND-ed
// with. This could happen if the AND was contained in an OR, and the OR
// contained an always false condition that allowed the range optimizer to
// eliminate the subjunction.
TEST_F(HypergraphOptimizerTest, DontSubsumeRangePredicateInIndexMerge) {
  // Always false condition: t1.x BETWEEN 5 AND 0
  // Possible range scan: t1.x IS NULL
  // Possible index merge: t1.y = 2 OR t1.z = 3
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x BETWEEN 5 AND 0 "
      "OR (t1.x IS NULL AND (t1.y = 2 OR t1.z = 3))",
      /*nullable=*/true);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 1000;
  t1->file->stats.data_file_length = 1e6;

  // Create indexes on x, y and z.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(i, t1->create_index(t1->field[i], nullptr));
  }

  // Mark the indexes as supporting range scans.
  Mock_HANDLER *handler = down_cast<Mock_HANDLER *>(t1->file);
  EXPECT_CALL(*handler, index_flags)
      .WillRepeatedly(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  // Make the index on x less selective than the other indexes, so that an index
  // merge on y and z is preferred to an index range scan on x.
  EXPECT_CALL(*handler, records_in_range(0, _, _)).WillRepeatedly(Return(100));
  EXPECT_CALL(*handler, records_in_range(1, _, _)).WillRepeatedly(Return(10));
  EXPECT_CALL(*handler, records_in_range(2, _, _)).WillRepeatedly(Return(10));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ(AccessPath::INDEX_MERGE, root->filter().child->type);

  // There needs to be a filter because the index merge doesn't represent the
  // full WHERE clause. It is sufficient with a filter on (t1.x IS NULL), but
  // the hypergraph optimizer cannot currently operate on that granularity, so
  // we get the entire WHERE condition for now.
  EXPECT_EQ(
      "((t1.x between 5 and 0) or "
      "((t1.x is null) and ((t1.y = 2) or (t1.z = 3))))",
      ItemToString(root->filter().condition));
}

TEST_F(HypergraphOptimizerTest, IndexMergePrefersNonCPKToOrderByPrimaryKey) {
  for (bool order_by : {false, true}) {
    SCOPED_TRACE(order_by ? "With ORDER BY" : "Without ORDER BY");

    Query_block *query_block = ParseAndResolve(
        order_by ? "SELECT 1 FROM t1 WHERE t1.x < 3 OR t1.y > 4 ORDER BY t1.x"
                 : "SELECT 1 FROM t1 WHERE t1.x < 3 OR t1.y > 4",
        /*nullable=*/false);

    Fake_TABLE *t1 = m_fake_tables["t1"];
    t1->file->stats.records = 1000;
    t1->s->primary_key =
        t1->create_index(t1->field[0], nullptr, /*unique=*/false);
    t1->create_index(t1->field[1], nullptr, /*unique=*/false);

    // Mark the index as supporting range scans, being ordered, and being
    // clustered.
    ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
            index_flags(_, _, _))
        .WillByDefault(Return(HA_READ_RANGE | HA_READ_ORDER | HA_READ_NEXT |
                              HA_READ_PREV));
    ON_CALL(*down_cast<Mock_HANDLER *>(m_fake_tables["t1"]->file),
            primary_key_is_clustered())
        .WillByDefault(Return(true));

    string trace;
    AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
    SCOPED_TRACE(trace);  // Prints out the trace on failure.
    // Prints out the query plan on failure.
    SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                                /*is_root_of_join=*/true));

    ASSERT_EQ(AccessPath::INDEX_MERGE, root->type);
    EXPECT_EQ(2, root->index_merge().children->size());
    if (order_by) {
      // We should choose a non-clustered primary key scan, since that gets the
      // ordering and thus elides the sort.
      EXPECT_FALSE(root->index_merge().allow_clustered_primary_key_scan);
    } else {
      // If there's no ordering, then using the CPK scan is cheaper.
      EXPECT_TRUE(root->index_merge().allow_clustered_primary_key_scan);
    }

    query_block->cleanup(/*full=*/true);
    ClearFakeTables();
  }
}

TEST_F(HypergraphOptimizerTest, IndexMergeInexactRangeWithOverflowBitset) {
  // CREATE TABLE t1(x VARCHAR(100), y INT, z INT, KEY(x), KEY(y)).
  Mock_field_varstring x(/*share=*/nullptr, /*name=*/"x",
                         /*char_len=*/100, /*is_nullable=*/true);
  Mock_field_long y("y");
  Mock_field_long z("z");
  Fake_TABLE *t1 = new (m_thd->mem_root) Fake_TABLE(&x, &y, &z);
  t1->file->stats.records = 10000;
  t1->file->stats.data_file_length = 1e6;
  t1->create_index(&x, nullptr, /*unique=*/false);
  t1->create_index(&y, nullptr, /*unique=*/false);
  ON_CALL(*down_cast<Mock_HANDLER *>(t1->file), index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));
  m_fake_tables["t1"] = t1;

  // We want to test a query that does an inexact range scan (achieved by having
  // a LIKE predicate on one of the indexed columns) and has enough predicates
  // that they don't fit in an inlined OverflowBitset in the range scan access
  // path. We need at least 64 predicates to make OverflowBitset overflow.
  constexpr int number_of_predicates = 70;
  string predicates = "(((t1.x like 'abc%xyz') or (t1.y > 3))";
  for (int i = 1; i < number_of_predicates; ++i) {
    predicates += " and (t1.z <> " + to_string(i) + ')';
  }
  predicates += ')';

  string query = "SELECT 1 FROM t1 WHERE " + predicates;
  Query_block *query_block = ParseAndResolve(query.c_str(),
                                             /*nullable=*/false);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::FILTER, root->type);
  EXPECT_EQ(AccessPath::INDEX_MERGE, root->filter().child->type);

  // Since an inexact range predicate is used, all predicates should be kept in
  // the filter node on top.
  EXPECT_EQ(predicates, ItemToString(root->filter().condition));
}

TEST_F(HypergraphOptimizerTest, PropagateCondConstants) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1 WHERE t1.x = 10 and t1.x <> 11",
                      /*nullable=*/true);

  m_initializer.thd()->lex->using_hypergraph_optimizer = true;
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             nullptr, &query_block->cond_value));
  // Check that the second predicate in the where condition is removed
  // as it's always true.
  EXPECT_EQ("(t1.x = 10)", ItemToString(query_block->where_cond()));
}

TEST_F(HypergraphOptimizerTest, PropagationInNonEqualities) {
  Query_block *query_block = ParseAndResolve(
      "SELECT t1.x FROM t1 JOIN t2 WHERE t1.x = t2.x AND t1.x <> t2.x + 10",
      /*nullable=*/true);

  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  m_fake_tables["t1"]->file->stats.records = 100;
  m_fake_tables["t2"]->file->stats.records = 10000;

  // Set up some large scan costs to discourage nested loop.
  m_fake_tables["t1"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 100e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  RelationalExpression *expr = root->hash_join().join_predicate->expr;
  ASSERT_EQ(1, expr->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(expr->equijoin_conditions[0]));

  AccessPath *t1 = root->hash_join().inner;
  ASSERT_EQ(AccessPath::FILTER, t1->type);
  EXPECT_EQ("(t1.x <> (t1.x + 10))", ItemToString(t1->filter().condition));
  EXPECT_EQ(m_fake_tables["t1"], t1->filter().child->table_scan().table);

  AccessPath *t2 = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, t2->type);
  EXPECT_EQ(m_fake_tables["t2"], t2->table_scan().table);
}

TEST_F(HypergraphOptimizerTest, PropagateEqualityToZeroRows) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1, t2 WHERE t1.x = t2.x AND t1.x < t2.x",
                      /*nullable=*/true);

  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  EXPECT_EQ(AccessPath::ZERO_ROWS, root->type);
}

TEST_F(HypergraphOptimizerTest, PropagateEqualityToZeroRowsAggregated) {
  Query_block *query_block = ParseAndResolve(
      "SELECT COUNT(*) FROM t1, t2 WHERE t1.x = t2.x AND t1.x < t2.x",
      /*nullable=*/true);

  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  EXPECT_EQ(AccessPath::ZERO_ROWS_AGGREGATED, root->type);
}

TEST_F(HypergraphOptimizerTest, RowCountImplicitlyGrouped) {
  Query_block *query_block =
      ParseAndResolve("SELECT SUM(t1.x) FROM t1", /*nullable=*/true);

  m_fake_tables["t1"]->file->stats.records = 100000;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Implicitly grouped queries always return a single row.
  EXPECT_EQ(AccessPath::AGGREGATE, root->type);
  EXPECT_FLOAT_EQ(1.0, root->num_output_rows());
}

TEST_F(HypergraphOptimizerTest, SingleTableDeleteWithOrderByLimit) {
  Query_block *query_block =
      ParseAndResolve("DELETE FROM t1 WHERE t1.x > 0 ORDER BY t1.y LIMIT 2",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  EXPECT_EQ(m_fake_tables["t1"]->pos_in_table_list->map(),
            root->delete_rows().immediate_tables);
  ASSERT_EQ(AccessPath::SORT, root->delete_rows().child->type);
  EXPECT_EQ(2, root->delete_rows().child->sort().limit);
  ASSERT_EQ(AccessPath::FILTER, root->delete_rows().child->sort().child->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN,
            root->delete_rows().child->sort().child->filter().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, SingleTableDeleteWithLimit) {
  Query_block *query_block =
      ParseAndResolve("DELETE FROM t1 WHERE t1.x > 0 LIMIT 2",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  EXPECT_EQ(m_fake_tables["t1"]->pos_in_table_list->map(),
            root->delete_rows().immediate_tables);
  ASSERT_EQ(AccessPath::LIMIT_OFFSET, root->delete_rows().child->type);
  ASSERT_EQ(AccessPath::FILTER,
            root->delete_rows().child->limit_offset().child->type);
  EXPECT_EQ(
      AccessPath::TABLE_SCAN,
      root->delete_rows().child->limit_offset().child->filter().child->type);

  query_block->cleanup(/*full=*/true);
}

// Delete from a single table using the multi-table delete syntax.
TEST_F(HypergraphOptimizerTest, DeleteSingleAsMultiTable) {
  Query_block *query_block = ParseAndResolve("DELETE t1 FROM t1 WHERE t1.x = 1",
                                             /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  EXPECT_EQ(m_fake_tables["t1"]->pos_in_table_list->map(),
            root->delete_rows().immediate_tables);
  ASSERT_EQ(AccessPath::FILTER, root->delete_rows().child->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN,
            root->delete_rows().child->filter().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, DeleteFromTwoTables) {
  Query_block *query_block =
      ParseAndResolve("DELETE t1, t2 FROM t1, t2 WHERE t1.x = t2.x",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  m_fake_tables["t1"]->file->stats.records = 1000;
  m_fake_tables["t2"]->file->stats.records = 100;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  ASSERT_EQ(AccessPath::HASH_JOIN, root->delete_rows().child->type);

  // A hash join is chosen, since the tables are so big that a nested loop join
  // is more expensive, even though it does not have to buffer row IDs. The join
  // order (t1, t2) is preferred because t2 is smaller and hashes fewer rows.
  // None of the tables can be deleted from immediately when we use hash join.
  EXPECT_EQ(0, root->delete_rows().immediate_tables);
  ASSERT_EQ(AccessPath::TABLE_SCAN,
            root->delete_rows().child->hash_join().outer->type);
  EXPECT_EQ(m_fake_tables["t1"],
            root->delete_rows().child->hash_join().outer->table_scan().table);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, DeletePreferImmediate) {
  // Delete from one table (t1), but read from one additional table (t2).
  Query_block *query_block =
      ParseAndResolve("DELETE t1 FROM t1, t2 WHERE t1.x = t2.x",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  // Add indexes so that a nested loop join with an index lookup on the inner
  // side is preferred. Make t1 slightly larger, so that the join order (t2, t1)
  // is considered cheaper than (t1, t2) before the cost of buffered deletes is
  // taken into consideration.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/true);
  t1->file->stats.records = 110000;
  t1->file->stats.data_file_length = 1.1e6;
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[0], nullptr, /*unique=*/true);
  t2->file->stats.records = 100000;
  t2->file->stats.data_file_length = 1.0e6;

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->delete_rows().child->type);
  const auto &nested_loop_join = root->delete_rows().child->nested_loop_join();

  // Even though joining (t2, t1) is cheaper, it should choose the order (t1,
  // t2) to allow immediate deletes from t1, which gives a lower total cost for
  // the delete operation.
  EXPECT_EQ(t1->pos_in_table_list->map(), root->delete_rows().immediate_tables);
  ASSERT_EQ(AccessPath::TABLE_SCAN, nested_loop_join.outer->type);
  EXPECT_STREQ("t1", nested_loop_join.outer->table_scan().table->alias);
  ASSERT_EQ(AccessPath::EQ_REF, nested_loop_join.inner->type);
  EXPECT_STREQ("t2", nested_loop_join.inner->eq_ref().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ImmedateDeleteFromRangeScan) {
  Query_block *query_block =
      ParseAndResolve("DELETE t1 FROM t1 WHERE t1.x < 100",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/true);
  t1->file->stats.records = 100000;

  // Mark the index as supporting range scans.
  ON_CALL(*down_cast<Mock_HANDLER *>(t1->file), index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  EXPECT_EQ(t1->pos_in_table_list->map(), root->delete_rows().immediate_tables);
  EXPECT_EQ(AccessPath::INDEX_RANGE_SCAN, root->delete_rows().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, ImmedateDeleteFromIndexMerge) {
  Query_block *query_block =
      ParseAndResolve("DELETE t1 FROM t1 WHERE t1.x > 0 OR t1.y > 0",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/true);
  t1->create_index(t1->field[1], nullptr, /*unique=*/true);
  t1->file->stats.records = 100000;

  // Mark the indexes as supporting range scans.
  ON_CALL(*down_cast<Mock_HANDLER *>(t1->file), index_flags(_, _, _))
      .WillByDefault(Return(HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV));

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_EQ(AccessPath::DELETE_ROWS, root->type);
  EXPECT_EQ(AccessPath::INDEX_MERGE, root->delete_rows().child->type);
  EXPECT_EQ(t1->pos_in_table_list->map(), root->delete_rows().immediate_tables);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphOptimizerTest, UpdatePreferImmediate) {
  // Update one table (t1), but read from one additional table (t2).
  Query_block *query_block =
      ParseAndResolve("UPDATE t1, t2 SET t1.x = t1.x + 1 WHERE t1.x = t2.x",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  // Add indexes so that a nested loop join with an index lookup on the inner
  // side is preferred. Make t1 slightly larger, so that the join order (t2, t1)
  // is considered cheaper than (t1, t2) before the cost of buffered updates is
  // taken into consideration.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], nullptr, /*unique=*/true);
  t1->file->stats.records = 110000;
  t1->file->stats.data_file_length = 1.1e6;
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->create_index(t2->field[0], nullptr, /*unique=*/true);
  t2->file->stats.records = 100000;
  t2->file->stats.data_file_length = 1.0e6;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_EQ(AccessPath::UPDATE_ROWS, root->type);
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->update_rows().child->type);
  const auto &nested_loop_join = root->update_rows().child->nested_loop_join();

  // Even though joining (t2, t1) is cheaper, it should choose the order (t1,
  // t2) to allow immediate update of t1, which gives a lower total cost for
  // the update operation.
  EXPECT_EQ(t1->pos_in_table_list->map(), root->update_rows().immediate_tables);
  ASSERT_EQ(AccessPath::TABLE_SCAN, nested_loop_join.outer->type);
  EXPECT_STREQ("t1", nested_loop_join.outer->table_scan().table->alias);
  ASSERT_EQ(AccessPath::EQ_REF, nested_loop_join.inner->type);
  EXPECT_STREQ("t2", nested_loop_join.inner->eq_ref().table->alias);
}

TEST_F(HypergraphOptimizerTest, UpdateHashJoin) {
  Query_block *query_block =
      ParseAndResolve("UPDATE t1, t2 SET t1.x = 1, t2.x = 2 WHERE t1.y = t2.y",
                      /*nullable=*/false);
  ASSERT_NE(nullptr, query_block);

  // Size the tables so that a hash join is preferable to a nested loop join.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->file->stats.records = 100000;
  t1->file->stats.data_file_length = 1e6;
  Fake_TABLE *t2 = m_fake_tables["t2"];
  t2->file->stats.records = 10000;
  t2->file->stats.data_file_length = 1e5;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::UPDATE_ROWS, root->type);
  // Both tables are updated.
  EXPECT_EQ(t1->pos_in_table_list->map() | t2->pos_in_table_list->map(),
            root->update_rows().tables_to_update);
  // No immediate update with hash join.
  EXPECT_EQ(0, root->update_rows().immediate_tables);

  // Expect a hash join with the smaller table (t2) on the inner side.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->update_rows().child->type);
  const auto &hash_join = root->update_rows().child->hash_join();
  ASSERT_EQ(AccessPath::TABLE_SCAN, hash_join.outer->type);
  EXPECT_EQ(t1, hash_join.outer->table_scan().table);
  ASSERT_EQ(AccessPath::TABLE_SCAN, hash_join.inner->type);
  EXPECT_EQ(t2, hash_join.inner->table_scan().table);
}

// An alias for better naming.
using HypergraphSecondaryEngineTest = HypergraphOptimizerTest;

TEST_F(HypergraphSecondaryEngineTest, SingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  // Install a hook that doubles the row count estimate of t1.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        EXPECT_EQ(AccessPath::TABLE_SCAN, path->type);
        EXPECT_STREQ("t1", path->table_scan().table->alias);
        path->set_num_output_rows(200);
        return false;
      };

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  ASSERT_EQ(AccessPath::TABLE_SCAN, root->type);
  EXPECT_EQ(m_fake_tables["t1"], root->table_scan().table);
  EXPECT_FLOAT_EQ(200.0F, root->num_output_rows());
}

TEST_F(HypergraphSecondaryEngineTest, SimpleInnerJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 10000;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t3"]->file->stats.records = 1000000;

  // Install a hook that changes the row count estimate for t3 to 1.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        // Nested-loop joins have been disabled for the secondary engine.
        EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
        if (path->type == AccessPath::TABLE_SCAN &&
            string(path->table_scan().table->alias) == "t3") {
          path->set_num_output_rows(1);
        }
        return false;
      };

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  // Expect the biggest table to be the outer one. The table statistics tell
  // that this is t3, but the secondary engine cost hook changes the estimate
  // for t3 so that t1 becomes the biggest one.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, root->hash_join().outer->type);
  EXPECT_STREQ("t1", root->hash_join().outer->table_scan().table->alias);
}

TEST_F(HypergraphSecondaryEngineTest, OrderedAggregation) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1 GROUP BY t1.x", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  EnableSecondaryEngine(/*aggregation_is_unordered=*/false);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  ASSERT_EQ(AccessPath::AGGREGATE, root->type);
  ASSERT_EQ(AccessPath::SORT, root->aggregate().child->type);
}

TEST_F(HypergraphSecondaryEngineTest, UnorderedAggregation) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1 GROUP BY t1.x", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  EnableSecondaryEngine(/*aggregation_is_unordered=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  ASSERT_EQ(AccessPath::AGGREGATE, root->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, root->aggregate().child->type);
}

TEST_F(HypergraphSecondaryEngineTest,
       OrderedAggregationCoversDistinctWithOrder) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.x, t1.y FROM t1 ORDER BY t1.y",
                      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  EnableSecondaryEngine(/*aggregation_is_unordered=*/false);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);

  ASSERT_EQ(AccessPath::SORT, root->type);
  Filesort *sort = root->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t1.y", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[1].item));
  EXPECT_TRUE(sort->m_remove_duplicates);

  ASSERT_EQ(AccessPath::TABLE_SCAN, root->sort().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphSecondaryEngineTest, UnorderedAggregationDoesNotCover) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.x, t1.y FROM t1 ORDER BY t1.y",
                      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  EnableSecondaryEngine(/*aggregation_is_unordered=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));
  ASSERT_NE(nullptr, root);
  ASSERT_NE(nullptr, root);

  // The final sort is just a regular sort, no duplicate removal.
  ASSERT_EQ(AccessPath::SORT, root->type);
  Filesort *sort = root->sort().filesort;
  ASSERT_EQ(1, sort->sort_order_length());
  EXPECT_EQ("t1.y", ItemToString(sort->sortorder[0].item));
  EXPECT_FALSE(sort->m_remove_duplicates);

  // Below that, there's a duplicate-removing sort for DISTINCT.
  // Order does not matter, but it happens to choose the cover here.
  AccessPath *distinct = root->sort().child;
  ASSERT_EQ(AccessPath::SORT, distinct->type);
  sort = distinct->sort().filesort;
  ASSERT_EQ(2, sort->sort_order_length());
  EXPECT_EQ("t1.y", ItemToString(sort->sortorder[0].item));
  EXPECT_EQ("t1.x", ItemToString(sort->sortorder[1].item));
  EXPECT_TRUE(sort->m_remove_duplicates);

  ASSERT_EQ(AccessPath::TABLE_SCAN, distinct->sort().child->type);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphSecondaryEngineTest, RejectAllPlans) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        // Nested-loop joins have been disabled for the secondary engine.
        EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
        // Reject all plans.
        return true;
      };

  // No plans will be found, so expect an error.
  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE};

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}

TEST_F(HypergraphSecondaryEngineTest, RejectAllCompletePlans) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        // Reject the path if all three tables are referenced.
        return GetUsedTableMap(path, /*include_pruned_tables=*/true) == 0b111;
      };

  // No plans will be found, so expect an error.
  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE};

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}

TEST_F(HypergraphSecondaryEngineTest, RejectJoinOrders) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);

  // Install a hook that only accepts hash joins where the outer table is a
  // table scan and the inner table is a table scan or another hash join, and
  // which only accepts join orders where the tables are ordered alphabetically
  // by their names.
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        // Nested-loop joins have been disabled for the secondary engine.
        EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
        if (path->type == AccessPath::HASH_JOIN) {
          if (path->hash_join().outer->type != AccessPath::TABLE_SCAN) {
            return true;
          }
          string outer = path->hash_join().outer->table_scan().table->alias;
          string inner;
          if (path->hash_join().inner->type == AccessPath::TABLE_SCAN) {
            inner = path->hash_join().inner->table_scan().table->alias;
          } else {
            EXPECT_EQ(AccessPath::HASH_JOIN, path->hash_join().inner->type);
            EXPECT_EQ(AccessPath::TABLE_SCAN,
                      path->hash_join().inner->hash_join().inner->type);
            inner = path->hash_join()
                        .inner->hash_join()
                        .inner->table_scan()
                        .table->alias;
          }
          // Reject plans where the join order is not alphabetical.
          return outer > inner;
        }
        return false;
      };

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  /*
    Expect the plan to have the following structure, because of the cost hook:

       HJ
      /  \
     t1  HJ
        /  \
       t2  t3
   */

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  const auto &outer_hash = root->hash_join();
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer_hash.outer->type);
  ASSERT_EQ(AccessPath::HASH_JOIN, outer_hash.inner->type);
  const auto &inner_hash = outer_hash.inner->hash_join();
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner_hash.inner->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner_hash.outer->type);

  EXPECT_STREQ("t1", outer_hash.outer->table_scan().table->alias);
  EXPECT_STREQ("t2", inner_hash.outer->table_scan().table->alias);
  EXPECT_STREQ("t3", inner_hash.inner->table_scan().table->alias);
}

/*
  For secondary engines we allow semijoin transformation for subqueries
  present in a join condition. We test if the transformation should
  be rejected or accepted when proposing hash joins.
*/
TEST_F(HypergraphSecondaryEngineTest, SemiJoinWithOuterJoinMultipleEqual) {
  Query_block *query_block = ::parse(&m_initializer,
                                     "SELECT 1 FROM t1 LEFT JOIN t2 ON "
                                     "t1.x=t2.x AND t1.x IN (SELECT x FROM t3)",
                                     0);
  // Set using_hypergraph_optimizer to true and enable secondary engine
  // optimization so that the subquery to semijoin transformation
  // happens as intended. If not, resolver would think its the old join
  // optimizer and does the transformation anyways which makes testing
  // this use case harder.
  m_initializer.thd()->lex->using_hypergraph_optimizer = true;
  m_initializer.thd()->set_secondary_engine_optimization(
      Secondary_engine_optimization::SECONDARY);
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  ResolveQueryBlock(m_initializer.thd(), query_block, true, &m_fake_tables);
  hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);

  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        // Nested-loop joins have been disabled for the secondary engine.
        EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
        // Without the semijoin transformation, a subquery will
        // be placed in the ON condition of the outer join.
        if (path->type == AccessPath::HASH_JOIN) {
          if (path->hash_join().join_predicate->expr->type ==
              RelationalExpression::LEFT_JOIN) {
            RelationalExpression *left_join =
                path->hash_join().join_predicate->expr;
            // We reject all the plans which have subqueries in join conditions.
            if (!left_join->join_conditions.empty() &&
                left_join->join_conditions[0]->has_subquery())
              return true;
          }
        }
        return false;
      };

  // Build multiple equalities from the join condition.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Check if a plan was generated as the query could be executed using
  // hash joins.
  EXPECT_NE(nullptr, root);
  // Plan would be this:
  // t1 LEFT JOIN (t2 SEMIJOIN t3 ON t2.x = t3.x) ON t1.x=t2.x
  // Make sure that the fields from the inner table of the semijoin
  // are not used in the join condition of the outer join.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  const RelationalExpression *left_join =
      root->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, left_join->type);
  ASSERT_EQ(1, left_join->equijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(left_join->equijoin_conditions[0]));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->hash_join().inner->type);
  const RelationalExpression *semijoin =
      root->hash_join().inner->hash_join().join_predicate->expr;
  EXPECT_EQ(RelationalExpression::SEMIJOIN, semijoin->type);
  ASSERT_EQ(1, semijoin->equijoin_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)", ItemToString(semijoin->equijoin_conditions[0]));
}

TEST_F(HypergraphSecondaryEngineTest, SemiJoinWithOuterJoin) {
  Query_block *query_block = ::parse(&m_initializer,
                                     "SELECT 1 FROM t1 LEFT JOIN t2 ON "
                                     "t1.x=t2.x AND t1.y IN (SELECT x FROM t3)",
                                     0);
  // Set using_hypergraph_optimizer to true and enable secondary engine
  // optimization so that the subquery to semijoin transformation
  // happens as intended. If not, resolver would think its the old join
  // optimizer and does the transformation anyways which makes testing
  // this use case harder.
  m_initializer.thd()->lex->using_hypergraph_optimizer = true;
  m_initializer.thd()->set_secondary_engine_optimization(
      Secondary_engine_optimization::SECONDARY);
  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  ResolveQueryBlock(m_initializer.thd(), query_block, true, &m_fake_tables);
  hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);

  // Without the semijoin transformation, a subquery will be placed in the
  // ON condition of the outer join.
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        // Nested-loop joins have been disabled for the secondary engine.
        EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
        if (path->type == AccessPath::HASH_JOIN) {
          if (path->hash_join().join_predicate->expr->type ==
              RelationalExpression::LEFT_JOIN) {
            RelationalExpression *left_join =
                path->hash_join().join_predicate->expr;
            // We reject plans which have subqueries in join conditions.
            if (!left_join->join_conditions.empty() &&
                left_join->join_conditions[0]->has_subquery())
              return true;
          }
        }
        return false;
      };

  // No plans will be found, so expect an error.
  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE};

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Check if all plans were rejected as the query cannot be executed
  // using hash joins.
  EXPECT_EQ(nullptr, root);
}

namespace {
struct RejectionParam {
  // The query to test.
  string query;
  // Path type to reject in the secondary engine cost hook.
  AccessPath::Type rejected_type;
  // Whether or not to expect an error if the specified path type always gives
  // an error or is rejected.
  bool expect_error;
};

std::ostream &operator<<(std::ostream &os, const RejectionParam &param) {
  return os << param.query << '/' << param.rejected_type << '/'
            << param.expect_error;
}
}  // namespace

using HypergraphSecondaryEngineRejectionTest =
    OptimizerTestWithParam<RejectionParam>;

TEST_P(HypergraphSecondaryEngineRejectionTest, RejectPathType) {
  const RejectionParam &param = GetParam();
  Query_block *query_block = ParseAndResolve(param.query.data(),
                                             /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *thd, const JoinHypergraph &, AccessPath *path) {
        EXPECT_FALSE(thd->is_error());
        return path->type == GetParam().rejected_type;
      };

  ErrorChecker error_checker(m_thd,
                             param.expect_error ? ER_SECONDARY_ENGINE : 0);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(param.expect_error, root == nullptr);

  query_block->cleanup(/*full=*/true);
}

TEST_P(HypergraphSecondaryEngineRejectionTest, ErrorOnPathType) {
  const RejectionParam &param = GetParam();
  Query_block *query_block = ParseAndResolve(param.query.data(),
                                             /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *thd, const JoinHypergraph &, AccessPath *path) {
        EXPECT_FALSE(thd->is_error());
        if (path->type == GetParam().rejected_type) {
          my_error(ER_SECONDARY_ENGINE_PLUGIN, MYF(0), "");
          return true;
        } else {
          return false;
        }
      };

  ErrorChecker error_checker(
      m_thd, param.expect_error ? ER_SECONDARY_ENGINE_PLUGIN : 0);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(param.expect_error, root == nullptr);

  query_block->cleanup(/*full=*/true);
}

INSTANTIATE_TEST_SUITE_P(
    ErrorCases, HypergraphSecondaryEngineRejectionTest,
    ::testing::ValuesIn(std::initializer_list<RejectionParam>({
        {"SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x", AccessPath::TABLE_SCAN, true},
        {"SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x", AccessPath::HASH_JOIN, true},
        {"SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x ORDER BY t1.x",
         AccessPath::SORT, true},
        {"SELECT DISTINCT t1.x FROM t1", AccessPath::SORT, true},
        {"SELECT t1.x FROM t1 GROUP BY t1.x HAVING COUNT(*) > 5",
         AccessPath::FILTER, true},
        {"SELECT t1.x FROM t1 GROUP BY t1.x HAVING COUNT(*) > 5 ORDER BY t1.x",
         AccessPath::FILTER, true},
        {"SELECT 1 FROM t1 GROUP BY t1.x ORDER BY SUM(t1.y)",
         AccessPath::STREAM, true},
    })));

INSTANTIATE_TEST_SUITE_P(
    SuccessCases, HypergraphSecondaryEngineRejectionTest,
    ::testing::ValuesIn(std::initializer_list<RejectionParam>(
        {{"SELECT 1 FROM t1 WHERE t1.x=1", AccessPath::HASH_JOIN, false},
         {"SELECT 1 FROM t1 WHERE t1.x=1", AccessPath::SORT, false},
         {"SELECT DISTINCT t1.y, t1.x, 3 FROM t1 GROUP BY t1.x, t1.y",
          AccessPath::SORT, false}})));

TEST_F(HypergraphSecondaryEngineTest, NoRewriteOnFinalization) {
  Query_block *query_block = ParseAndResolve(
      "SELECT SUM(t1.x) FROM t1 GROUP BY t1.y ORDER BY AVG(t1.x)",
      /*nullable=*/true);

  handlerton *handlerton =
      EnableSecondaryEngine(/*aggregation_is_unordered=*/true);
  handlerton->secondary_engine_flags |=
      MakeSecondaryEngineFlags(SecondaryEngineFlag::USE_EXTERNAL_EXECUTOR);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  const string query_plan = PrintQueryPlan(0, root, query_block->join,
                                           /*is_root_of_join=*/true);
  SCOPED_TRACE(query_plan);
  // Verify that finalization was performed.
  EXPECT_FALSE(query_block->join->needs_finalize);

  // There should be no materialization or streaming in the plan.
  ASSERT_EQ(AccessPath::SORT, root->type);
  ASSERT_EQ(AccessPath::AGGREGATE, root->sort().child->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN,
            root->sort().child->aggregate().child->type);

  // The item in the select list should be a SUM. It would have been an
  // Item_field pointing into a temporary table if the USE_EXTERNAL_EXECUTOR
  // flag was not set.
  auto visible_fields = VisibleFields(*query_block->join->fields);
  ASSERT_EQ(1, std::distance(visible_fields.begin(), visible_fields.end()));
  Item *select_list_item = *visible_fields.begin();
  ASSERT_EQ(Item::SUM_FUNC_ITEM, select_list_item->type());
  EXPECT_EQ(Item_sum::SUM_FUNC,
            down_cast<Item_sum *>(select_list_item)->sum_func());

  // The order item should be an AVG. It would have been an Item_field pointing
  // into a temporary table if the USE_EXTERNAL_EXECUTOR flag was not set.
  Item *order_item = *root->sort().order->item;
  ASSERT_EQ(Item::SUM_FUNC_ITEM, order_item->type());
  EXPECT_EQ(Item_sum::AVG_FUNC, down_cast<Item_sum *>(order_item)->sum_func());

  // Make sure the sort key is shown by EXPLAIN.
  EXPECT_THAT(query_plan, StartsWith("-> Sort: avg(t1.x) "));

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphSecondaryEngineTest, ExplainWindowForExternalExecutor) {
  Query_block *query_block =
      ParseAndResolve("SELECT PERCENT_RANK() OVER () FROM t1",
                      /*nullable=*/true);

  // Disable creation of intermediate temporary tables.
  handlerton *handlerton =
      EnableSecondaryEngine(/*aggregation_is_unordered=*/true);
  handlerton->secondary_engine_flags |=
      MakeSecondaryEngineFlags(SecondaryEngineFlag::USE_EXTERNAL_EXECUTOR);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(AccessPath::WINDOW, root->type);
  EXPECT_EQ(AccessPath::TABLE_SCAN, root->window().child->type);

  // Finalization should not create temporary tables for the window functions.
  EXPECT_FALSE(query_block->join->needs_finalize);
  EXPECT_EQ(nullptr, root->window().temp_table);
  EXPECT_EQ(nullptr, root->window().temp_table_param);

  // EXPLAIN for WINDOW paths used to get information from the associated
  // temporary table, which is not available until finalization has run.
  // Finalization is skipped when USE_EXTERNAL_EXECUTOR is enabled, so this used
  // to crash.
  EXPECT_THAT(
      PrintQueryPlan(0, root, query_block->join,
                     /*is_root_of_join=*/true),
      StartsWith("-> Window aggregate with buffering: percent_rank() OVER ()"));

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphSecondaryEngineTest, NoMaterializationForExternalExecutor) {
  Base_mock_field_blob t1_x{"x", Field::MAX_LONG_BLOB_WIDTH};
  Mock_field_long t1_y{"y"};
  m_fake_tables["t1"] = new (m_thd->mem_root) Fake_TABLE{&t1_x, &t1_y};

  Query_block *query_block =
      ParseAndResolve("SELECT MAX(t1.x) FROM t1 GROUP BY t1.y ORDER BY t1.y",
                      /*nullable=*/true);

  // Disable creation of intermediate temporary tables.
  handlerton *handlerton =
      EnableSecondaryEngine(/*aggregation_is_unordered=*/true);
  handlerton->secondary_engine_flags |=
      MakeSecondaryEngineFlags(SecondaryEngineFlag::USE_EXTERNAL_EXECUTOR);

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // There should be no materialization into a temporary table in the plan. If
  // USE_EXTERNAL_EXECUTOR had not been enabled, the plan would have contained a
  // materialization step between AGGREGATE and SORT because of the BLOB column.
  ASSERT_EQ(AccessPath::SORT, root->type);
  ASSERT_EQ(AccessPath::AGGREGATE, root->sort().child->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN,
            root->sort().child->aggregate().child->type);
  EXPECT_STREQ(
      "t1", root->sort().child->aggregate().child->table_scan().table->alias);

  query_block->cleanup(/*full=*/true);
}

TEST_F(HypergraphSecondaryEngineTest, DontCallCostHookForEmptyJoins) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1, t2 WHERE t1.x=t2.x "
      "AND t1.y IS NULL AND t1.y IN (1,2,3)",
      /*nullable=*/true);

  // Create an index on t1.y, so that the range optimizer detects the impossible
  // table filter.
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[1], nullptr, /*unique=*/true);

  // The secondary engine cost hook is stateless, so let's create a thread local
  // variable for it to store the state in.
  thread_local vector<AccessPath> paths;
  paths.clear();

  handlerton *hton = EnableSecondaryEngine(/*aggregation_is_unordered=*/false);
  hton->secondary_engine_modify_access_path_cost =
      [](THD *, const JoinHypergraph &, AccessPath *path) {
        paths.push_back(*path);
        return false;
      };

  string trace;
  AccessPath *root = FindBestQueryPlanAndFinalize(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The join is known to be always empty.
  EXPECT_EQ(AccessPath::ZERO_ROWS, root->type);

  // The secondary engine cost hook should see the TABLE_SCAN on t2, since
  // that's the first table found by the join enumeration algorithm. When the
  // join enumeration goes on to see t1, it detects that t1 has a condition
  // that's always false, and it immediately stops exploring more plans. The
  // hook therefore doesn't see any more plans.
  ASSERT_EQ(1, paths.size());
  ASSERT_EQ(AccessPath::TABLE_SCAN, paths[0].type);
  EXPECT_STREQ("t2", paths[0].table_scan().table->alias);
}

/*
  A hypergraph receiver that doesn't actually cost any plans;
  it only counts the number of possible plans that would be
  considered.
 */
struct CountingReceiver {
  CountingReceiver(const JoinHypergraph &graph, size_t num_relations)
      : m_graph(graph), m_num_subplans(new size_t[1llu << num_relations]) {
    std::fill(m_num_subplans.get(),
              m_num_subplans.get() + (1llu << num_relations), 0);
  }

  bool HasSeen(NodeMap subgraph) { return m_num_subplans[subgraph] != 0; }

  bool FoundSingleNode(int node_idx) {
    NodeMap map = TableBitmap(node_idx);
    ++m_num_subplans[map];
    return false;
  }

  bool FoundSubgraphPair(NodeMap left, NodeMap right, int edge_idx) {
    const JoinPredicate *edge = &m_graph.edges[edge_idx];
    if (!PassesConflictRules(left | right, edge->expr)) {
      return false;
    }
    size_t n = m_num_subplans[left] * m_num_subplans[right];
    if (OperatorIsCommutative(*edge->expr)) {
      m_num_subplans[left | right] += 2 * n;
    } else {
      m_num_subplans[left | right] += n;
    }
    return false;
  }

  size_t count(NodeMap map) const { return m_num_subplans[map]; }

  const JoinHypergraph &m_graph;
  std::unique_ptr<size_t[]> m_num_subplans;
};

RelationalExpression *CloneRelationalExpr(THD *thd,
                                          const RelationalExpression *expr) {
  RelationalExpression *new_expr =
      new (thd->mem_root) RelationalExpression(thd);
  new_expr->type = expr->type;
  new_expr->tables_in_subtree = expr->tables_in_subtree;
  if (new_expr->type == RelationalExpression::TABLE) {
    new_expr->table = expr->table;
  } else {
    new_expr->left = CloneRelationalExpr(thd, expr->left);
    new_expr->right = CloneRelationalExpr(thd, expr->right);
  }
  return new_expr;
}

// Generate all possible complete binary trees of (exactly) the given size,
// consisting only of inner joins, and with fake tables at the leaves.
vector<RelationalExpression *> GenerateAllCompleteBinaryTrees(
    THD *thd, size_t num_relations, size_t start_idx,
    vector<unique_ptr_destroy_only<Fake_TABLE>> *tables) {
  assert(num_relations != 0);

  vector<RelationalExpression *> ret;
  if (num_relations == 1) {
    Fake_TABLE *table = new (thd->mem_root)
        Fake_TABLE(/*column_count=*/1, /*cols_nullable=*/true);
    table->pos_in_table_list->set_tableno(start_idx);
    tables->emplace_back(table);

    // For debugging only.
    char name[32];
    snprintf(name, sizeof(name), "t%zu", start_idx + 1);
    table->alias = sql_strdup(name);
    table->pos_in_table_list->alias = table->alias;

    RelationalExpression *expr = new (thd->mem_root) RelationalExpression(thd);
    expr->type = RelationalExpression::TABLE;
    expr->table = table->pos_in_table_list;
    expr->tables_in_subtree = table->pos_in_table_list->map();

    ret.push_back(expr);
    return ret;
  }

  for (size_t num_left = 1; num_left <= num_relations - 1; ++num_left) {
    size_t num_right = num_relations - num_left;
    vector<RelationalExpression *> left =
        GenerateAllCompleteBinaryTrees(thd, num_left, start_idx, tables);
    vector<RelationalExpression *> right = GenerateAllCompleteBinaryTrees(
        thd, num_right, start_idx + num_left, tables);

    // Generate all pairs of trees, cloning as we go.
    for (size_t i = 0; i < left.size(); ++i) {
      for (size_t j = 0; j < right.size(); ++j) {
        RelationalExpression *expr =
            new (thd->mem_root) RelationalExpression(thd);
        expr->type = RelationalExpression::INNER_JOIN;
        expr->left = CloneRelationalExpr(thd, left[i]);
        expr->right = CloneRelationalExpr(thd, right[j]);
        expr->tables_in_subtree =
            expr->left->tables_in_subtree | expr->right->tables_in_subtree;
        ret.push_back(expr);
      }
    }
  }
  return ret;
}

// For each join operation (starting from idx), try all join types
// and all possible simple, non-degenerate predicaes, calling func()
// for each combination.
template <class Func>
void TryAllPredicates(
    const vector<RelationalExpression *> &join_ops,
    const vector<Item_field *> &fields,
    const vector<RelationalExpression::Type> &join_types,
    unordered_map<RelationalExpression *, table_map> *generated_nulls,
    size_t idx, const Func &func) {
  if (idx == join_ops.size()) {
    func();
    return;
  }

  RelationalExpression *expr = join_ops[idx];
  for (RelationalExpression::Type join_type : join_types) {
    expr->type = join_type;

    // Check which tables are visible after this join
    // (you can't have a predicate pointing into the right side
    // of an antijoin).
    const table_map left_map = expr->left->tables_in_subtree;
    const table_map right_map = expr->right->tables_in_subtree;
    if (join_type == RelationalExpression::ANTIJOIN ||
        join_type == RelationalExpression::SEMIJOIN) {
      expr->tables_in_subtree = left_map;
    } else {
      expr->tables_in_subtree = left_map | right_map;
    }

    (*generated_nulls)[expr] =
        (*generated_nulls)[expr->left] | (*generated_nulls)[expr->right];
    if (join_type == RelationalExpression::LEFT_JOIN) {
      (*generated_nulls)[expr] |= right_map;
    } else if (join_type == RelationalExpression::FULL_OUTER_JOIN) {
      (*generated_nulls)[expr] |= left_map | right_map;
    }

    // Find all pairs of tables under this operation, and construct an equijoin
    // predicate for them.
    for (Item_field *field1 : fields) {
      if (!IsSubset(field1->used_tables(), left_map)) {
        continue;
      }
      if ((join_type == RelationalExpression::INNER_JOIN ||
           join_type == RelationalExpression::SEMIJOIN) &&
          IsSubset(field1->used_tables(), (*generated_nulls)[expr->left])) {
        // Should have be simplified away. (See test comment.)
        continue;
      }
      for (Item_field *field2 : fields) {
        if (!IsSubset(field2->used_tables(), right_map)) {
          continue;
        }
        if ((join_type == RelationalExpression::INNER_JOIN ||
             join_type == RelationalExpression::SEMIJOIN ||
             join_type == RelationalExpression::LEFT_JOIN ||
             join_type == RelationalExpression::ANTIJOIN) &&
            IsSubset(field2->used_tables(), (*generated_nulls)[expr->right])) {
          // Should have be simplified away. (See test comment.)
          continue;
        }

        Item_func_eq *pred = new Item_func_eq(field1, field2);
        pred->update_used_tables();
        pred->quick_fix_field();
        expr->equijoin_conditions[0] = pred;
        expr->conditions_used_tables =
            field1->used_tables() | field2->used_tables();

        TryAllPredicates(join_ops, fields, join_types, generated_nulls, idx + 1,
                         func);
      }
    }
  }
}

std::pair<size_t, size_t> CountTreesAndPlans(
    THD *thd, int num_relations,
    const std::vector<RelationalExpression::Type> &join_types) {
  size_t num_trees = 0, num_plans = 0;

  vector<unique_ptr_destroy_only<Fake_TABLE>> tables;
  vector<RelationalExpression *> roots = GenerateAllCompleteBinaryTrees(
      thd, num_relations, /*start_idx=*/0, &tables);
  for (RelationalExpression *expr : roots) {
    vector<RelationalExpression *> join_ops;
    vector<Item_field *> fields;

    // Which tables can get NULL-complemented rows due to outer joins.
    // We use this to reject inner joins against them, on the basis
    // that they would be simplified away and thus don't count.
    unordered_map<RelationalExpression *, table_map> generated_nulls;

    // Collect lists of all ops, and create tables where needed.
    ForEachOperator(
        expr, [&join_ops, &fields, &generated_nulls](RelationalExpression *op) {
          if (op->type == RelationalExpression::TABLE) {
            Item_field *field = new Item_field(op->table->table->field[0]);
            field->quick_fix_field();
            fields.push_back(field);
            op->tables_in_subtree = op->table->map();
            generated_nulls.emplace(op, 0);
          } else {
            join_ops.push_back(op);
            op->equijoin_conditions.clear();
            op->equijoin_conditions.push_back(nullptr);
          }
        });

    TryAllPredicates(
        join_ops, fields, join_types, &generated_nulls, /*idx=*/0, [&] {
          JoinHypergraph graph(thd->mem_root, /*query_block=*/nullptr);
          for (RelationalExpression *op : join_ops) {
            op->conflict_rules.clear();
          }
          MakeJoinGraphFromRelationalExpression(thd, expr, /*trace=*/nullptr,
                                                &graph);
          CountingReceiver receiver(graph, num_relations);
          ASSERT_FALSE(EnumerateAllConnectedPartitions(graph.graph, &receiver));
          ++num_trees;
          num_plans += receiver.count(TablesBetween(0, num_relations));
        });
  }

  return {num_trees, num_plans};
}

/*
  Reproduces tables 4 and 5 from [Moe13]; builds all possible complete
  binary trees, fills them with all possible join operators from a given
  set, adds a simple (non-degenerate) equality predicate for each,
  and counts the number of plans. By getting numbers that match exactly,
  we can say with a fairly high degree of certainty that we've managed to
  get all the associativity etc. tables correct.

  The paper makes a few unspoken assumptions that are worth noting:

  1. After an antijoin or semijoin, the right side “disappears” and
     can not be used for further join predicates. This is consistent
     with the typical EXISTS / NOT EXISTS formulation in SQL.
  2. Outer joins are assumed simplified away wherever possible, so
     queries like (a JOIN (b LEFT JOIN c ON ...) a.x=c.x) are discarded
     as meaningless -- since the join predicate would discard any NULLs
     generated for c, the LEFT JOIN could just as well be an inner join.
  3. All predicates are assumed to be NULL-rejecting.

  Together, these explain why we have e.g. 26 queries with n=3 and the
  small operator set, instead of 36 (which would be logical for two shapes
  of binary trees, three operators for the top node, three for the bottom node
  and two possible top join predicates) or even more (if including non-nullable
  outer join predicates).

  We don't match the number of empty and nonempty rule sets given, but ours
  are correct and the paper's have a bug that prevents some simplification
  (Moerkotte, personal communication).
 */
TEST(ConflictDetectorTest, CountPlansSmallOperatorSet) {
  Server_initializer initializer;
  initializer.SetUp();
  THD *thd = initializer.thd();
  current_thd = thd;

  vector<RelationalExpression::Type> join_types{
      RelationalExpression::INNER_JOIN, RelationalExpression::LEFT_JOIN,
      RelationalExpression::ANTIJOIN};
  EXPECT_THAT(CountTreesAndPlans(thd, 3, join_types), Pair(26, 88));
  EXPECT_THAT(CountTreesAndPlans(thd, 4, join_types), Pair(344, 4059));
  EXPECT_THAT(CountTreesAndPlans(thd, 5, join_types), Pair(5834, 301898));

  // This takes too long to run for a normal unit test run (~10s in optimized
  // mode).
  if (false) {
    EXPECT_THAT(CountTreesAndPlans(thd, 6, join_types), Pair(117604, 32175460));
    EXPECT_THAT(CountTreesAndPlans(thd, 7, join_types),
                Pair(2708892, 4598129499));
  }
  initializer.TearDown();
}

TEST(ConflictDetectorTest, CountPlansLargeOperatorSet) {
  Server_initializer initializer;
  initializer.SetUp();
  THD *thd = initializer.thd();
  current_thd = thd;

  vector<RelationalExpression::Type> join_types{
      RelationalExpression::INNER_JOIN, RelationalExpression::LEFT_JOIN,
      RelationalExpression::FULL_OUTER_JOIN, RelationalExpression::SEMIJOIN,
      RelationalExpression::ANTIJOIN};
  EXPECT_THAT(CountTreesAndPlans(thd, 3, join_types), Pair(62, 203));
  EXPECT_THAT(CountTreesAndPlans(thd, 4, join_types), Pair(1114, 11148));

  // These take too long to run for a normal unit test run (~80s in optimized
  // mode).
  if (false) {
    EXPECT_THAT(CountTreesAndPlans(thd, 5, join_types), Pair(25056, 934229));
    EXPECT_THAT(CountTreesAndPlans(thd, 6, join_types),
                Pair(661811, 108294798));
    EXPECT_THAT(CountTreesAndPlans(thd, 7, join_types),
                Pair(19846278, 16448441514));
  }
  initializer.TearDown();
}

class CSETest : public OptimizerTestBase {
 protected:
  string TestCSE(const string &expression);
};

string CSETest::TestCSE(const string &expression) {
  // Abuse ParseAndResolve() to get the expression parsed.
  Query_block *query_block = ParseAndResolve(
      ("SELECT 1 FROM t1, t2, t3, t4, t5 WHERE " + expression).c_str(),
      /*nullable=*/true);
  return ItemToString(
      CommonSubexpressionElimination(query_block->join->where_cond));
}

TEST_F(CSETest, NoopSimpleItem) {
  EXPECT_EQ(TestCSE("t1.x=t2.x"), "(t1.x = t2.x)");
}

TEST_F(CSETest, NoopANDNoOR) {
  EXPECT_EQ(TestCSE("t1.x=t2.x AND t2.x = t3.x"),
            "((t1.x = t2.x) and (t2.x = t3.x))");
}

TEST_F(CSETest, NoopORNoAND) {
  EXPECT_EQ(TestCSE("t1.x=t2.x OR t2.x = t3.x"),
            "((t1.x = t2.x) or (t2.x = t3.x))");
}

TEST_F(CSETest, NoopNoCommon) {
  EXPECT_EQ(TestCSE("t1.x=t2.x OR (t2.x = t3.x AND t3.x > 4)"),
            "((t1.x = t2.x) or ((t2.x = t3.x) and (t3.x > 4)))");
}

TEST_F(CSETest, BasicSplit) {
  EXPECT_EQ(TestCSE("(t1.x=t2.x AND t2.x > 3) OR (t1.x=t2.x AND t2.x < 0)"),
            "((t1.x = t2.x) and ((t2.x > 3) or (t2.x < 0)))");
}

TEST_F(CSETest, SplitFromRecursedORGroups) {
  EXPECT_EQ(TestCSE("(t1.x=0 AND t2.x>1) OR ((t1.x=0 AND t2.y>1) OR (t1.x=0 "
                    "AND t2.z>0))"),
            "((t1.x = 0) and ((t2.x > 1) or (t2.y > 1) or (t2.z > 0)))");
}

TEST_F(CSETest, SplitFromRecursedANDGroups) {
  EXPECT_EQ(TestCSE("(t2.x>1 AND (t2.y>1 AND (t1.x=0))) OR "
                    "(t3.x>1 AND (t3.y>1 AND (t1.x=0)))"),
            "((t1.x = 0) and "
            "(((t2.x > 1) and (t2.y > 1)) or ((t3.x > 1) and (t3.y > 1))))");
}

// Split out t1.x > 1 and t2.y < 2, ie., more than one element,
// and they are in different orders. There are multiple items left
// in the rightmost OR group, too.
TEST_F(CSETest, SplitOutMoreThanOneElement) {
  EXPECT_EQ(TestCSE("(t1.x > 1 AND t2.y < 2 AND t2.x > 3) OR ((t2.y < 2 AND "
                    "t1.x > 1 AND t2.x < 1 AND t2.z >= 4))"),
            "((t1.x > 1) and (t2.y < 2) and "
            "((t2.x > 3) or ((t2.x < 1) and (t2.z >= 4))))");
}

TEST_F(CSETest, ShortCircuit) {
  EXPECT_EQ(TestCSE("t1.x=t2.x OR (t1.x=t2.x AND t2.x < 0)"), "(t1.x = t2.x)");
}

TEST_F(CSETest, ShortCircuitWithMultipleElements) {
  EXPECT_EQ(TestCSE("(t1.x=0 AND t1.y=1) OR (t1.x=0 AND t1.y=1)"),
            "((t1.x = 0) and (t1.y = 1))");
}

TEST_F(CSETest, EmptyOr) {
  // remove_eq_conds() may leave degenerate OR conditions with no children if
  // all elements of the OR expression are false. Verify that we don't balk at
  // such items.
  EXPECT_EQ("false", ItemToString(CommonSubexpressionElimination(
                         new (m_thd->mem_root) Item_cond_or)));
}

namespace {
// A fake handler implementation that can be used in microbenchmarks. The
// Mock_HANDLER object in Fake_TABLE has a lot of instrumentation that disturbs
// the timing, so we roll our own lightweight handler instead.
class Fake_handler_for_benchmark final : public handler {
 public:
  explicit Fake_handler_for_benchmark(Fake_TABLE *table_arg)
      : handler(table_arg->file->ht, table_arg->s) {
    set_ha_table(table_arg);
  }

  // Report that range scans are supported, so that the range optimizer has
  // something to work with.
  ulong index_flags(uint, uint, bool) const override {
    return HA_READ_RANGE | HA_READ_NEXT | HA_READ_PREV;
  }

  // Report that primary keys are clustered, to match InnoDB's default.
  bool primary_key_is_clustered() const override { return true; }

  // Just stub out the rest of the functions. Raise assert failures on those
  // that are only expected to be called during execution.

  void position(const uchar *) override { assert(false); }
  int info(uint) override { return 0; }
  const char *table_type() const override { return "fake"; }
  THR_LOCK_DATA **store_lock(THD *, THR_LOCK_DATA **,
                             enum thr_lock_type) override {
    assert(false);
    return nullptr;
  }
  int create(const char *, TABLE *, HA_CREATE_INFO *, dd::Table *) override {
    assert(false);
    return HA_ERR_WRONG_COMMAND;
  }

 protected:
  int rnd_next(uchar *) override {
    assert(false);
    return HA_ERR_WRONG_COMMAND;
  }
  int rnd_pos(uchar *, uchar *) override {
    assert(false);
    return HA_ERR_WRONG_COMMAND;
  }

 private:
  int open(const char *, int, uint, const dd::Table *) override {
    assert(false);
    return HA_ERR_WRONG_COMMAND;
  }
  int close() override {
    assert(false);
    return HA_ERR_WRONG_COMMAND;
  }
  int rnd_init(bool) override {
    assert(false);
    return HA_ERR_WRONG_COMMAND;
  }
  Table_flags table_flags() const override { return 0; }
};
}  // namespace

// Measures the time spent in FindBestQueryPlan() and
// FinalizePlanForQueryBlock() for a point-select query.
static void BM_FindBestQueryPlanPointSelect(size_t num_iterations) {
  StopBenchmarkTiming();

  Server_initializer initializer;
  initializer.SetUp();
  unordered_map<string, Fake_TABLE *> fake_tables;

  THD *const thd = initializer.thd();

  Query_block *const query_block =
      ParseAndResolve("SELECT t1.y FROM t1 WHERE t1.x = 123",
                      /*nullable=*/false, initializer, &fake_tables);

  // Make t1.x the primary key. Add secondary indexes on t1.y and t1.z, just to
  // give the optimizer some more information to look into.
  Fake_TABLE *t1 = fake_tables["t1"];
  Fake_handler_for_benchmark fake_handler(t1);
  t1->set_handler(&fake_handler);
  t1->s->primary_key = t1->create_index(t1->field[0], nullptr, /*unique=*/true);
  t1->create_index(t1->field[1], nullptr, /*unique=*/false);
  t1->create_index(t1->field[2], nullptr, /*unique=*/false);
  t1->file->stats.records = 100000;
  t1->file->stats.data_file_length = 1e8;

  // Build multiple equalities from the WHERE clause.
  COND_EQUAL *cond_equal = nullptr;
  EXPECT_FALSE(optimize_cond(thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  EXPECT_EQ(1, cond_equal->current_level.size());
  EXPECT_TRUE(is_function_of_type(query_block->where_cond(),
                                  Item_func::MULT_EQUAL_FUNC));
  query_block->join->where_cond = query_block->where_cond();

  const size_t mem_root_size_after_resolving = thd->mem_root->allocated_size();

  {
    // Use a separate MEM_ROOT for the allocations done by the hypergraph
    // optimizer, so that this memory can be freed after each iteration without
    // interfering with the data structures allocated during resolving above.
    MEM_ROOT optimize_mem_root;
    Query_arena arena_backup;
    Query_arena arena{&optimize_mem_root, Query_arena::STMT_PREPARED};
    thd->swap_query_arena(arena, &arena_backup);

    StartBenchmarkTiming();

    for (size_t i = 0; i < num_iterations; ++i) {
      assert(query_block->join->where_cond == query_block->where_cond());
      AccessPath *path = FindBestQueryPlan(thd, query_block, /*trace=*/nullptr);
      assert(path != nullptr);
      assert(path->type == AccessPath::EQ_REF);
      query_block->join->set_root_access_path(path);

      [[maybe_unused]] const bool error =
          FinalizePlanForQueryBlock(thd, query_block);
      assert(!error);

      query_block->cleanup(/*full=*/false);
      query_block->join->set_root_access_path(nullptr);
      thd->rollback_item_tree_changes();
      cleanup_items(arena.item_list());
      arena.free_items();
      optimize_mem_root.ClearForReuse();
    }

    StopBenchmarkTiming();

    thd->swap_query_arena(arena_backup, &arena);
  }

  // Check that all the allocations in FindBestQueryPlan() used
  // optimize_mem_root. We don't want the memory footprint to grow for each
  // iteration.
  EXPECT_EQ(mem_root_size_after_resolving, thd->mem_root->allocated_size());

  query_block->cleanup(/*full=*/true);
  DestroyFakeTables(fake_tables);
}
BENCHMARK(BM_FindBestQueryPlanPointSelect)
