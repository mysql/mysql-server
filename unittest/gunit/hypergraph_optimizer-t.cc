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

#include <assert.h>
#include <gtest/gtest.h>
#include <string.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/field.h"
#include "sql/filesort.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_subselect.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/join_optimizer/hypergraph.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/make_join_hypergraph.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/nested_join.h"
#include "sql/sort_param.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "template_utils.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/handler-t.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/test_utils.h"

class MakeHypergraphTest : public ::testing::Test {
 public:
  void SetUp() override { m_initializer.SetUp(); }
  void TearDown() override {
    DestroyFakeTables();
    m_initializer.TearDown();
  }

 protected:
  Query_block *ParseAndResolve(const char *query, bool nullable);
  void ResolveFieldToFakeTable(Item *item);
  void ResolveAllFieldsToFakeTable(
      const mem_root_deque<TABLE_LIST *> &join_list);
  void DestroyFakeTables() {
    for (const auto &name_and_table : m_fake_tables) {
      destroy(name_and_table.second);
    }
  }
  handlerton *EnableSecondaryEngine();

  my_testing::Server_initializer m_initializer;
  THD *m_thd = nullptr;
  std::unordered_map<std::string, Fake_TABLE *> m_fake_tables;
};

Query_block *MakeHypergraphTest::ParseAndResolve(const char *query,
                                                 bool nullable) {
  Query_block *query_block = ::parse(&m_initializer, query, 0);
  m_thd = m_initializer.thd();

  // Create fake TABLE objects for all tables mentioned in the query.
  int num_tables = 0;
  for (TABLE_LIST *tl = query_block->get_table_list(); tl != nullptr;
       tl = tl->next_global) {
    Fake_TABLE *fake_table =
        new (m_thd->mem_root) Fake_TABLE(/*num_columns=*/2, nullable);
    fake_table->alias = tl->alias;
    fake_table->pos_in_table_list = tl;
    tl->table = fake_table;
    tl->set_tableno(num_tables++);
    m_fake_tables[tl->alias] = fake_table;
  }

  // Find all Item_field objects, and resolve them to fields in the fake tables.
  ResolveAllFieldsToFakeTable(query_block->top_join_list);

  // Also in any conditions and subqueries within the WHERE condition.
  if (query_block->where_cond() != nullptr) {
    WalkItem(query_block->where_cond(), enum_walk::POSTFIX, [&](Item *item) {
      if (item->type() == Item::SUBSELECT_ITEM) {
        Item_in_subselect *item_subselect =
            down_cast<Item_in_subselect *>(item);
        ResolveFieldToFakeTable(item_subselect->left_expr);
        Query_block *child_query_block =
            item_subselect->unit->first_query_block();
        ResolveAllFieldsToFakeTable(child_query_block->top_join_list);
        if (child_query_block->where_cond() != nullptr) {
          ResolveFieldToFakeTable(child_query_block->where_cond());
        }
        for (Item *field_item : child_query_block->fields) {
          ResolveFieldToFakeTable(field_item);
        }
        return true;  // Don't go down into item_subselect->left_expr again.
      } else if (item->type() == Item::FIELD_ITEM) {
        ResolveFieldToFakeTable(item);
      }
      return false;
    });
  }

  // And in the SELECT, GROUP BY and ORDER BY lists.
  for (Item *item : query_block->fields) {
    ResolveFieldToFakeTable(item);
  }
  for (ORDER *cur_group = query_block->group_list.first; cur_group != nullptr;
       cur_group = cur_group->next) {
    ResolveFieldToFakeTable(*cur_group->item);
  }
  for (ORDER *cur_group = query_block->order_list.first; cur_group != nullptr;
       cur_group = cur_group->next) {
    ResolveFieldToFakeTable(*cur_group->item);
  }

  query_block->prepare(m_thd, nullptr);

  // Create a fake, tiny JOIN. (This would normally be done in optimization.)
  query_block->join = new (m_thd->mem_root) JOIN(m_thd, query_block);
  query_block->join->where_cond = query_block->where_cond();
  query_block->join->having_cond = nullptr;
  query_block->join->fields = &query_block->fields;
  query_block->join->alloc_func_list();

  return query_block;
}

void MakeHypergraphTest::ResolveFieldToFakeTable(Item *item_arg) {
  WalkItem(item_arg, enum_walk::POSTFIX, [&](Item *item) {
    if (item->type() == Item::FIELD_ITEM) {
      Item_field *item_field = down_cast<Item_field *>(item);
      Fake_TABLE *table = m_fake_tables[item_field->table_name];
      item_field->table_ref = table->pos_in_table_list;
      if (strcmp(item_field->field_name, "x") == 0) {
        item_field->field = table->field[0];
      } else if (strcmp(item_field->field_name, "y") == 0) {
        item_field->field = table->field[1];
      } else {
        assert(false);
      }
      item_field->set_nullable(item_field->field->is_nullable());
    }
    item->update_used_tables();
    item->quick_fix_field();
    return false;
  });
}

void MakeHypergraphTest::ResolveAllFieldsToFakeTable(
    const mem_root_deque<TABLE_LIST *> &join_list) {
  for (TABLE_LIST *tl : join_list) {
    if (tl->join_cond() != nullptr) {
      ResolveFieldToFakeTable(tl->join_cond());
    }
    if (tl->nested_join != nullptr) {
      ResolveAllFieldsToFakeTable(tl->nested_join->join_list);
    }
  }
}

handlerton *MakeHypergraphTest::EnableSecondaryEngine() {
  auto hton = new (m_thd->mem_root) Fake_handlerton;
  hton->flags = HTON_SUPPORTS_SECONDARY_ENGINE;
  hton->secondary_engine_supported_access_paths =
      AccessPathTypeBitmap(AccessPath::HASH_JOIN);

  for (const auto &name_and_table : m_fake_tables) {
    name_and_table.second->file->ht = hton;
  }

  m_thd->lex->m_sql_cmd->use_secondary_storage_engine(hton);

  return hton;
}

namespace {
/// An error checker which, upon destruction, verifies that a single error was
/// raised while the checker was alive, and that the error had the expected
/// error number. If an error is raised, the THD::is_error() flag will be set,
/// just as in the server. (The default error_handler_hook used by the unit
/// tests, does not set the error flag in the THD.)
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
    EXPECT_TRUE(m_thd->is_error());
    EXPECT_EQ(m_errno, m_thd->get_stmt_da()->mysql_errno());
    EXPECT_EQ(1, m_thd->get_stmt_da()->current_statement_cond_count());
  }

 private:
  const THD *m_thd;
  unsigned m_errno;
  decltype(error_handler_hook) m_saved_error_hook;
};
}  // namespace

TEST_F(MakeHypergraphTest, SingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1", /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root);
  EXPECT_FALSE(
      MakeJoinHypergraph(m_thd, query_block, /*trace=*/nullptr, &graph));

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

  JoinHypergraph graph(m_thd->mem_root);
  string trace;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, query_block, &trace, &graph));
  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  EXPECT_EQ(graph.graph.nodes.size(), graph.nodes.size());
  EXPECT_EQ(graph.graph.edges.size(), 2 * graph.edges.size());

  ASSERT_EQ(3, graph.nodes.size());
  EXPECT_STREQ("t1", graph.nodes[0].table->alias);
  EXPECT_STREQ("t2", graph.nodes[1].table->alias);
  EXPECT_STREQ("t3", graph.nodes[2].table->alias);

  // Simple edges; order doesn't matter.
  ASSERT_EQ(2, graph.edges.size());

  // t1/t2. There is no index information, so the default 0.1 should be used.
  EXPECT_EQ(0x01, graph.graph.edges[0].left);
  EXPECT_EQ(0x02, graph.graph.edges[0].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[0].expr->type);
  EXPECT_FLOAT_EQ(0.1, graph.edges[0].selectivity);

  // t2/t3.
  EXPECT_EQ(0x02, graph.graph.edges[2].left);
  EXPECT_EQ(0x04, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::INNER_JOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, OuterJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON t2.y=t3.y) ON t1.x=t2.x",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root);
  string trace;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, query_block, &trace, &graph));
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
  EXPECT_FLOAT_EQ(0.1, graph.edges[0].selectivity);

  // t1/{t2,t3}. (This is strictly too conservative; it should be t1/t2.
  // But we don't distinguish between null-rejecting and other conditions yet.)
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, SemiJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2 JOIN t3 ON "
      "t2.y=t3.y)",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root);
  string trace;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, query_block, &trace, &graph));
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
  EXPECT_FLOAT_EQ(0.1, graph.edges[0].selectivity);

  // t1/{t2,t3}.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::SEMIJOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, AntiJoin) {
  // NOTE: Fields must be non-nullable, or NOT IN can not be rewritten.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x NOT IN (SELECT t2.x FROM t2 JOIN t3 ON "
      "t2.y=t3.y)",
      /*nullable=*/false);

  JoinHypergraph graph(m_thd->mem_root);
  string trace;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, query_block, &trace, &graph));
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
  EXPECT_FLOAT_EQ(0.1, graph.edges[0].selectivity);

  // t1/{t2,t3}.
  EXPECT_EQ(0x01, graph.graph.edges[2].left);
  EXPECT_EQ(0x06, graph.graph.edges[2].right);
  EXPECT_EQ(RelationalExpression::ANTIJOIN, graph.edges[1].expr->type);
  EXPECT_FLOAT_EQ(0.1, graph.edges[1].selectivity);

  EXPECT_EQ(0, graph.predicates.size());
}

TEST_F(MakeHypergraphTest, Predicates) {
  // The OR ... IS NULL part is to keep the LEFT JOIN from being simplified
  // to an inner join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x=t2.x "
      "WHERE t1.x=2 AND (t2.y=3 OR t2.y IS NULL)",
      /*nullable=*/true);

  JoinHypergraph graph(m_thd->mem_root);
  string trace;
  EXPECT_FALSE(MakeJoinHypergraph(m_thd, query_block, &trace, &graph));
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
  EXPECT_FLOAT_EQ(0.1, graph.edges[0].selectivity);

  ASSERT_EQ(2, graph.predicates.size());
  EXPECT_EQ("(t1.x = 2)", ItemToString(graph.predicates[0].condition));
  EXPECT_EQ(0x01, graph.predicates[0].total_eligibility_set);  // Only t1.
  EXPECT_FLOAT_EQ(0.1,
                  graph.predicates[0].selectivity);  // No specific information.

  EXPECT_EQ("((t2.y = 3) or (t2.y is null))",
            ItemToString(graph.predicates[1].condition));
  EXPECT_GT(graph.predicates[1].selectivity,
            0.1);  // More common due to the OR NULL.
  EXPECT_EQ(0x03,
            graph.predicates[1].total_eligibility_set);  // Both t1 and t2!
}

// An alias for better naming.
// We don't verify costs; to do that, we'd probably need to mock out
// the cost model.
using HypergraphOptimizerTest = MakeHypergraphTest;

TEST_F(HypergraphOptimizerTest, SingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.

  ASSERT_EQ(AccessPath::TABLE_SCAN, root->type);
  EXPECT_EQ(m_fake_tables["t1"], root->table_scan().table);
  EXPECT_FLOAT_EQ(100, root->num_output_rows);
}

TEST_F(HypergraphOptimizerTest,
       PredicatePushdown) {  // Also tests nested loop join.
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x WHERE t2.y=3", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 200;
  m_fake_tables["t2"]->file->stats.records = 30;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The pushed-down filter makes the optimal plan be t2 on the left side,
  // with a nested loop.
  ASSERT_EQ(AccessPath::NESTED_LOOP_JOIN, root->type);
  EXPECT_EQ(JoinType::INNER, root->nested_loop_join().join_type);
  EXPECT_FLOAT_EQ(60, root->num_output_rows);  // 600 rows, 10% selectivity.

  // The condition should be posted directly on t2.
  AccessPath *outer = root->nested_loop_join().outer;
  ASSERT_EQ(AccessPath::FILTER, outer->type);
  EXPECT_EQ("(t2.y = 3)", ItemToString(outer->filter().condition));
  EXPECT_FLOAT_EQ(3, outer->num_output_rows);  // 10% default selectivity.

  AccessPath *outer_child = outer->filter().child;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer_child->type);
  EXPECT_EQ(m_fake_tables["t2"], outer_child->table_scan().table);
  EXPECT_FLOAT_EQ(30, outer_child->num_output_rows);

  // The inner part should have a join condition as a filter.
  AccessPath *inner = root->nested_loop_join().inner;
  ASSERT_EQ(AccessPath::FILTER, inner->type);
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(inner->filter().condition));
  EXPECT_FLOAT_EQ(20, inner->num_output_rows);  // 10% default selectivity.

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
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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
      200, join->num_output_rows);  // Selectivity overridden by outer join.

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
  EXPECT_FLOAT_EQ(3, inner->num_output_rows);
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
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // The condition should be gone, and only ref access should be in its place.
  // There shouldn't be EQ_REF, since we only have a partial match.
  ASSERT_EQ(AccessPath::REF, root->type);
  EXPECT_EQ(0, root->ref().ref->key);
  EXPECT_EQ(8, root->ref().ref->key_length);
  EXPECT_EQ(1, root->ref().ref->key_parts);
}

TEST_F(HypergraphOptimizerTest, NotPredicatePushdownToRef) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.y=3", /*nullable=*/true);
  Fake_TABLE *t1 = m_fake_tables["t1"];
  t1->create_index(t1->field[0], t1->field[1], /*unique=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, query_block->join,
                              /*is_root_of_join=*/true));

  // Both should be pushed, and we should now use the unique index.
  ASSERT_EQ(AccessPath::EQ_REF, root->type);
  EXPECT_EQ(0, root->eq_ref().ref->key);
  EXPECT_EQ(16, root->eq_ref().ref->key_length);
  EXPECT_EQ(2, root->eq_ref().ref->key_parts);
}

TEST_F(HypergraphOptimizerTest, SimpleInnerJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 10000;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t3"]->file->stats.records = 1000000;

  // Set up some large scan costs to discourage nested loop.
  m_fake_tables["t1"]->file->stats.data_file_length = 100e6;
  m_fake_tables["t2"]->file->stats.data_file_length = 1e6;
  m_fake_tables["t3"]->file->stats.data_file_length = 10000e6;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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

TEST_F(HypergraphOptimizerTest, DistinctIsDoneAsSort) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y, t1.x FROM t1", /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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
}

TEST_F(HypergraphOptimizerTest, DistinctIsSubsumedByGroup) {
  Query_block *query_block = ParseAndResolve(
      "SELECT DISTINCT t1.y, t1.x, 3 FROM t1 GROUP BY t1.x, t1.y",
      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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
  m_initializer.thd()->variables.sql_mode &= ~MODE_ONLY_FULL_GROUP_BY;
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y FROM t1 ORDER BY t1.x, t1.y",
                      /*nullable=*/true);
  m_initializer.thd()->variables.sql_mode |= MODE_ONLY_FULL_GROUP_BY;

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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

  // Maybe JOIN::destroy() should do this:
  query_block->join->filesorts_to_cleanup.clear();
  query_block->join->filesorts_to_cleanup.shrink_to_fit();
}

TEST_F(HypergraphOptimizerTest, DistinctSubsumesOrderBy) {
  Query_block *query_block =
      ParseAndResolve("SELECT DISTINCT t1.y, t1.x FROM t1 ORDER BY t1.x",
                      /*nullable=*/true);

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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

  // Maybe JOIN::destroy() should do this:
  query_block->join->filesorts_to_cleanup.clear();
  query_block->join->filesorts_to_cleanup.shrink_to_fit();
}

// An alias for better naming.
using HypergraphSecondaryEngineTest = HypergraphOptimizerTest;

TEST_F(HypergraphSecondaryEngineTest, SingleTable) {
  Query_block *query_block =
      ParseAndResolve("SELECT t1.x FROM t1", /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 100;

  // Install a hook that doubles the row count estimate of t1.
  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *, AccessPath *path) {
    EXPECT_EQ(AccessPath::TABLE_SCAN, path->type);
    EXPECT_STREQ("t1", path->table_scan().table->alias);
    path->num_output_rows *= 2;
    return false;
  };

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  ASSERT_EQ(AccessPath::TABLE_SCAN, root->type);
  EXPECT_EQ(m_fake_tables["t1"], root->table_scan().table);
  EXPECT_FLOAT_EQ(200, root->num_output_rows);
}

TEST_F(HypergraphSecondaryEngineTest, SimpleInnerJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);
  m_fake_tables["t1"]->file->stats.records = 10000;
  m_fake_tables["t2"]->file->stats.records = 100;
  m_fake_tables["t3"]->file->stats.records = 1000000;

  // Install a hook that changes the row count estimate for t3 to 1.
  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *, AccessPath *path) {
    // Nested-loop joins have been disabled for the secondary engine.
    EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
    if (path->type == AccessPath::TABLE_SCAN &&
        string(path->table_scan().table->alias) == "t3") {
      path->num_output_rows = 1;
    }
    return false;
  };

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  ASSERT_NE(nullptr, root);

  // Expect the biggest table to be the outer one. The table statistics tell
  // that this is t3, but the secondary engine cost hook changes the estimate
  // for t3 so that t1 becomes the biggest one.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  ASSERT_EQ(AccessPath::TABLE_SCAN, root->hash_join().outer->type);
  EXPECT_STREQ("t1", root->hash_join().outer->table_scan().table->alias);
}

TEST_F(HypergraphSecondaryEngineTest, RejectAllPlans) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *, AccessPath *path) {
    // Nested-loop joins have been disabled for the secondary engine.
    EXPECT_NE(AccessPath::NESTED_LOOP_JOIN, path->type);
    // Reject all plans.
    return true;
  };

  // No plans will be found, so expect an error.
  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE};

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}

TEST_F(HypergraphSecondaryEngineTest, RejectAllCompletePlans) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x JOIN t3 ON t2.y=t3.y",
      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *, AccessPath *path) {
    // Reject the path if all three tables are referenced.
    return GetUsedTables(path) == 0b111;
  };

  // No plans will be found, so expect an error.
  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE};

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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
  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *, AccessPath *path) {
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
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
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

TEST_F(HypergraphSecondaryEngineTest, RejectSort) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x ORDER BY t1.x",
                      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *, AccessPath *path) {
    return path->type == AccessPath::SORT;
  };

  // Expect an error, since there is no valid plan without a sort.
  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE};

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}

TEST_F(HypergraphSecondaryEngineTest, ErrorOnTableScan) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x",
                      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *thd,
                                                      AccessPath *path) {
    EXPECT_FALSE(thd->is_error());
    if (path->type == AccessPath::TABLE_SCAN) {
      my_error(ER_SECONDARY_ENGINE_PLUGIN, MYF(0), "");
      return true;
    } else {
      return false;
    }
  };

  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE_PLUGIN};

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}

TEST_F(HypergraphSecondaryEngineTest, ErrorOnHashJoin) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x",
                      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *thd,
                                                      AccessPath *path) {
    EXPECT_FALSE(thd->is_error());
    if (path->type == AccessPath::HASH_JOIN) {
      my_error(ER_SECONDARY_ENGINE_PLUGIN, MYF(0), "");
      return true;
    } else {
      return false;
    }
  };

  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE_PLUGIN};

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}

TEST_F(HypergraphSecondaryEngineTest, ErrorOnSort) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 JOIN t2 ON t1.x=t2.x ORDER BY t1.x",
                      /*nullable=*/true);

  handlerton *hton = EnableSecondaryEngine();
  hton->secondary_engine_modify_access_path_cost = [](THD *thd,
                                                      AccessPath *path) {
    EXPECT_FALSE(thd->is_error());
    if (path->type == AccessPath::SORT) {
      my_error(ER_SECONDARY_ENGINE_PLUGIN, MYF(0), "");
      return true;
    } else {
      return false;
    }
  };

  ErrorChecker error_checker{m_thd, ER_SECONDARY_ENGINE_PLUGIN};

  string trace;
  AccessPath *root = FindBestQueryPlan(m_thd, query_block, &trace);
  SCOPED_TRACE(trace);  // Prints out the trace on failure.
  EXPECT_EQ(nullptr, root);
}
