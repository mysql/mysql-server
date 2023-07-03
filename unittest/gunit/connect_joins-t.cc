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
#include <string>
#include <unordered_map>
#include <vector>

#include "my_table_map.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_select.h"
#include "unittest/gunit/fake_table.h"
#include "unittest/gunit/optimizer_test.h"

using optimizer_test::Table;
using std::vector;
using ConnectJoinTest = OptimizerTestBase;

// Tests a semijoin access path with two tables.
TEST_F(ConnectJoinTest, SemiJoin) {
  Query_block *query_block =
      ParseAndResolve("SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2)",
                      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 2;

  // Set up plan for two table join - t1 semijoin t2. prefix_tables
  // is unused for this query plan.
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b01));
  tables.push_back(Table("t2", /*plan_idx=*/1, /*prefix_tables=*/0b11));

  SetUpQEPTabs(query_block, /*num_tables=*/2, tables);

  // Set up the semijoin path, by setting "firstmatch_return" to the table
  // where the semijoin iterator will be created. Also attach the join
  // condition.
  JOIN *join = query_block->join;
  join->qep_tab[1].firstmatch_return = 0;
  join->qep_tab[1].set_condition(query_block->join->where_cond);

  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  // This will set up the access paths.
  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/2, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  // Prints out the query plan on failure.
  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  // Verify that we have t1 hash-semijoin t2 on t1.x = t2.x.
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::SEMIJOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &join_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, join_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(join_conditions[0]));

  AccessPath *outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, outer->type);
  EXPECT_EQ(m_fake_tables["t1"], outer->table_scan().table);

  AccessPath *inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, inner->type);
  EXPECT_EQ(m_fake_tables["t2"], inner->table_scan().table);
}

// We test a semijoin with two tables on its inner side (no multiple
// equalities).
TEST_F(ConnectJoinTest, SemiJoinWithInnerJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2 JOIN t3 "
      "ON t2.y=t3.y)",
      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 3;

  // Set up the plan for three table join of the above query.
  // Plan would be t1 SEMIJOIN (t2 JOIN t3). As the optimizer generates plan
  // for an NLJ, the table order would be t1->t3->t2.
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t3", /*plan_idx=*/1, /*prefix_tables=*/0b011));
  tables.push_back(Table("t2", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  SetUpQEPTabs(query_block, /*num_tables=*/3, tables);

  // Setup the semijoin.
  JOIN *join = query_block->join;
  join->qep_tab[2].firstmatch_return = 0;
  join->qep_tab[2].set_condition(query_block->join->where_cond);

  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  // Create access paths now.
  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/3, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  // Verify that we have t1 hash-semijoin (t2 hash join t3 on t2.y = t3.y)
  // on t1.x = t2.x
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::SEMIJOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &semijoin_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, semijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(semijoin_conditions[0]));

  AccessPath *semi_outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, semi_outer->type);
  EXPECT_EQ(m_fake_tables["t1"], semi_outer->table_scan().table);

  AccessPath *semi_inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, semi_inner->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            semi_inner->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &inner_join_conditions =
      semi_inner->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, inner_join_conditions.size());
  EXPECT_EQ("(t2.y = t3.y)", ItemToString(inner_join_conditions[0]));

  AccessPath *first_table_inner = semi_inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_inner->type);
  EXPECT_EQ(m_fake_tables["t2"], first_table_inner->table_scan().table);

  AccessPath *second_table_inner = semi_inner->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, second_table_inner->type);
  EXPECT_EQ(m_fake_tables["t3"], second_table_inner->table_scan().table);
}

// We test a semijoin with two tables on its inner side with multiple
// equalities.
TEST_F(ConnectJoinTest, SemiJoinWithMultiEqual) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2 JOIN t3 "
      "ON t2.x=t3.x)",
      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 3;

  // Set up the plan for three table join of the above query.
  // Plan would be t1 SEMIJOIN (t2 JOIN t3). As the optimizer generates plan
  // for an NLJ, the table order would be t1->t3->t2.
  // JOIN_TAB indexing will be based on the position in the table list which is
  // t1,t2,t3. However the planner would pick - t1,t3,t2
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t2", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  tables.push_back(Table("t3", /*plan_idx=*/1, /*prefix_tables=*/0b101));
  SetUpJoinTabs(query_block, /*num_tables=*/3, tables);

  // As mentioned before, the planner at the end would pick - t1,t3,t2
  // QEP_TAB indexing is based on the final plan and not the pos_in_table_list.
  tables.clear();
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t3", /*plan_idx=*/1, /*prefix_tables=*/0b011));
  tables.push_back(Table("t2", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  SetUpQEPTabs(query_block, /*num_tables*/ 3, tables);

  COND_EQUAL *cond_equal = nullptr;
  // Generate multi-equalities.
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  JOIN_TAB *map2table[3];

  // Using the multi-equalities and based on the table order picked, create
  // join conditions.
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++)
    map2table[i] = &query_block->join->join_tab[i];
  Item **where_cond = &query_block->join->where_cond;
  *where_cond =
      substitute_for_best_equal_field(m_thd, query_block->join->where_cond,
                                      query_block->join->cond_equal, map2table);
  EXPECT_EQ("((t3.x = t1.x) and (t2.x = t1.x))", ItemToString(*where_cond));

  JOIN *join = query_block->join;
  // Attach conditions to tables.
  Item *cond = nullptr;
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++) {
    JOIN_TAB *join_tab = &query_block->join->join_tab[i];
    table_map used_tables = join_tab->prefix_tables();
    table_map current_map = 1ULL << i;
    cond = make_cond_for_table(m_thd, *where_cond, used_tables, current_map,
                               /*exclude_expensive_cond=*/false);
    if (cond) join->qep_tab[join_tab->idx()].set_condition(cond);
  }

  // Setup semijoin path.
  join->qep_tab[2].firstmatch_return = 0;

  // Finally create access paths.
  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/3, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  // Verify that we have t1 hash-semijoin (t2 hash join t3 on t2.x = t3.x)
  // on t1.x = t3.x
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::SEMIJOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &semijoin_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, semijoin_conditions.size());
  EXPECT_EQ("(t3.x = t1.x)", ItemToString(semijoin_conditions[0]));

  AccessPath *semi_outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, semi_outer->type);
  EXPECT_EQ(m_fake_tables["t1"], semi_outer->table_scan().table);

  AccessPath *semi_inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, semi_inner->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            semi_inner->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &inner_join_conditions =
      semi_inner->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, inner_join_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)", ItemToString(inner_join_conditions[0]));

  AccessPath *first_table_inner = semi_inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_inner->type);
  EXPECT_EQ(m_fake_tables["t2"], first_table_inner->table_scan().table);

  AccessPath *second_table_inner = semi_inner->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, second_table_inner->type);
  EXPECT_EQ(m_fake_tables["t3"], second_table_inner->table_scan().table);
}

// Test outer join
TEST_F(ConnectJoinTest, OuterJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 JOIN t2 ON t1.x = t2.x LEFT JOIN t3 "
      "ON t2.x=t3.x",
      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 3;

  // Set up the plan for three table join of the above query.
  // Plan would be t1 JOIN t2 LEFT JOIN t3.
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t2", /*plan_idx=*/1, /*prefix_tables=*/0b011));
  tables.push_back(Table("t3", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  SetUpJoinTabs(query_block, /*num_tables=*/3, tables);
  // Setup outer join info
  // first inner table for including outer join
  query_block->join->join_tab[2].set_first_inner(2);
  // last table for embedding outer join
  query_block->join->join_tab[2].set_last_inner(2);
  // first inner table for embedding outer join
  query_block->join->join_tab[2].set_first_upper(1);

  // Set up QEP_TABs now
  SetUpQEPTabs(query_block, /*num_tables=*/3, tables);
  query_block->join->qep_tab[2].set_first_inner(2);
  query_block->join->qep_tab[2].set_last_inner(2);
  query_block->join->qep_tab[2].set_first_upper(1);

  COND_EQUAL *cond_equal = nullptr;
  // Generate multi-equalities
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  JOIN_TAB *map2table[3];

  // Using the multi-equalities and based on the table order picked, create
  // join conditions for each of the joins.
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++)
    map2table[i] = &query_block->join->join_tab[i];
  Item **where_cond = &query_block->join->where_cond;
  *where_cond =
      substitute_for_best_equal_field(m_thd, query_block->join->where_cond,
                                      query_block->join->cond_equal, map2table);
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(*where_cond));

  JOIN *join = query_block->join;
  // Attach conditions to tables
  Item *cond = nullptr;
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++) {
    JOIN_TAB *join_tab = &query_block->join->join_tab[i];
    table_map used_tables = join_tab->prefix_tables();
    table_map current_map = 1ULL << i;
    cond = make_cond_for_table(m_thd, *where_cond, used_tables, current_map,
                               /*exclude_expensive_cond=*/false);
    if (cond)
      join->qep_tab[join_tab->idx()].set_condition(cond);
    else if (join->qep_tab[join_tab->idx()].condition() !=
             join_tab->condition())
      join->qep_tab[join_tab->idx()].set_condition(join_tab->condition());
  }

  // Add is_not_null_compl condition for the outer join condition attached
  // to t3
  Item *outer_join_cond = new (m_thd->mem_root) Item_func_trig_cond(
      join->qep_tab[2].condition(), /*f - trigger variable=*/nullptr,
      query_block->join, join->qep_tab[2].first_inner(),
      Item_func_trig_cond::IS_NOT_NULL_COMPL);
  outer_join_cond->quick_fix_field();
  join->qep_tab[2].set_condition(outer_join_cond);

  // Finally create access paths.
  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/3, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &leftjoin_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, leftjoin_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)", ItemToString(leftjoin_conditions[0]));

  AccessPath *left_outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            left_outer->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &inner_join_conditions =
      left_outer->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, inner_join_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(inner_join_conditions[0]));

  AccessPath *first_table_outer = left_outer->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_outer->type);
  EXPECT_EQ(m_fake_tables["t2"], first_table_outer->table_scan().table);

  AccessPath *second_table_outer = left_outer->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, second_table_outer->type);
  EXPECT_EQ(m_fake_tables["t1"], second_table_outer->table_scan().table);

  AccessPath *left_inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, left_inner->type);
  EXPECT_EQ(m_fake_tables["t3"], left_inner->table_scan().table);
}

// Test semijoin with outer join on its inner side
TEST_F(ConnectJoinTest, OuterJoinInSemiJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2 LEFT "
      "JOIN t3"
      " ON t2.x=t3.x)",
      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 3;

  // Set up the plan for three table join of the above query.
  // Plan would be t1 SEMIJOIN t2 LEFT JOIN t3.
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t2", /*plan_idx=*/1, /*prefix_tables=*/0b011));
  tables.push_back(Table("t3", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  SetUpJoinTabs(query_block, /*num_tables=*/3, tables);

  // Set up QEP_TABs and the outer join info
  SetUpQEPTabs(query_block, /*num_tables=*/3, tables);
  query_block->join->qep_tab[2].set_first_inner(2);
  query_block->join->qep_tab[2].set_last_inner(2);
  query_block->join->qep_tab[2].set_first_upper(1);

  Item *where_cond = query_block->join->where_cond;

  JOIN *join = query_block->join;
  // Attach conditions to tables
  join->qep_tab[1].set_condition(where_cond);
  join->qep_tab[2].set_condition(query_block->join->join_tab[2].condition());

  // Add is_not_null_compl condition for the outer join condition attached
  // to t3
  Item *outer_join_cond = new (m_thd->mem_root) Item_func_trig_cond(
      join->qep_tab[2].condition(), /* f - trigger variable=*/nullptr,
      query_block->join, join->qep_tab[2].first_inner(),
      Item_func_trig_cond::IS_NOT_NULL_COMPL);
  outer_join_cond->quick_fix_field();
  join->qep_tab[2].set_condition(outer_join_cond);

  join->qep_tab[2].firstmatch_return = 0;

  // Finally create access paths
  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/3, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  // Verify if everything is as expected
  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::SEMIJOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &semijoin_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, semijoin_conditions.size());
  EXPECT_EQ("(t1.x = t2.x)", ItemToString(semijoin_conditions[0]));

  AccessPath *semi_outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, semi_outer->type);
  EXPECT_EQ(m_fake_tables["t1"], semi_outer->table_scan().table);

  AccessPath *semi_inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN,
            semi_inner->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &outer_join_conditions =
      semi_inner->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, outer_join_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)", ItemToString(outer_join_conditions[0]));

  AccessPath *first_table_outer = semi_inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_outer->type);
  EXPECT_EQ(m_fake_tables["t2"], first_table_outer->table_scan().table);

  AccessPath *second_table_outer = semi_inner->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, second_table_outer->type);
  EXPECT_EQ(m_fake_tables["t3"], second_table_outer->table_scan().table);
}

// Test semijoin within outer join.
TEST_F(ConnectJoinTest, SemiJoinInOuterJoin) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 LEFT JOIN t2 ON t1.x = t2.x WHERE "
      "t2.x IN (SELECT t3.x FROM t3)",
      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 3;

  // Set up the plan for three table join of the above query.
  // Plan would be t1 LEFT JOIN t2 SEMI JOIN t3.
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t2", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  tables.push_back(Table("t3", /*plan_idx=*/1, /*prefix_tables=*/0b101));
  SetUpJoinTabs(query_block, /*num_tables=*/3, tables);

  tables.clear();
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b001));
  tables.push_back(Table("t3", /*plan_idx=*/1, /*prefix_tables=*/0b011));
  tables.push_back(Table("t2", /*plan_idx=*/2, /*prefix_tables=*/0b111));
  SetUpQEPTabs(query_block, /*num_tables=*/3, tables);

  // Setup outer join info
  query_block->join->qep_tab[1].set_first_inner(1);
  query_block->join->qep_tab[1].set_last_inner(2);
  query_block->join->qep_tab[1].set_first_upper(0);

  COND_EQUAL *cond_equal = nullptr;
  // Generate multi-equalities
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  JOIN_TAB *map2table[3];

  // Using the multi-equalities and based on the table order picked, create
  // join conditions for each of the joins.
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++)
    map2table[i] = &query_block->join->join_tab[i];
  Item **where_cond = &query_block->join->where_cond;
  *where_cond =
      substitute_for_best_equal_field(m_thd, query_block->join->where_cond,
                                      query_block->join->cond_equal, map2table);
  EXPECT_EQ("((t3.x = t1.x) and (t2.x = t1.x))", ItemToString(*where_cond));

  JOIN *join = query_block->join;
  Item *cond = nullptr;
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++) {
    JOIN_TAB *join_tab = &query_block->join->join_tab[i];
    table_map used_tables = join_tab->prefix_tables();
    table_map current_map = 1ULL << i;
    cond = make_cond_for_table(m_thd, *where_cond, used_tables, current_map,
                               /*exclude_expensive_cond=*/false);
    if (cond) join->qep_tab[join_tab->idx()].set_condition(cond);
  }

  // Add is_not_null_compl condition for the outer join condition attached
  // to t3
  Item *first_outer_join_cond = new (m_thd->mem_root) Item_func_trig_cond(
      join->qep_tab[1].condition(), nullptr, query_block->join,
      join->qep_tab[1].first_inner(), Item_func_trig_cond::IS_NOT_NULL_COMPL);
  first_outer_join_cond->quick_fix_field();
  join->qep_tab[1].set_condition(first_outer_join_cond);

  Item *second_outer_join_cond = new (m_thd->mem_root) Item_func_trig_cond(
      join->qep_tab[2].condition(), nullptr, query_block->join,
      join->qep_tab[2].first_inner(), Item_func_trig_cond::IS_NOT_NULL_COMPL);
  second_outer_join_cond->quick_fix_field();
  join->qep_tab[2].set_condition(second_outer_join_cond);

  join->qep_tab[2].firstmatch_return = 1;

  // Create access paths.
  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/3, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::LEFT_JOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &leftjoin_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, leftjoin_conditions.size());
  EXPECT_EQ("(t3.x = t1.x)", ItemToString(leftjoin_conditions[0]));

  AccessPath *left_outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, left_outer->type);
  EXPECT_EQ(m_fake_tables["t1"], left_outer->table_scan().table);

  AccessPath *left_inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, left_inner->type);
  EXPECT_EQ(RelationalExpression::SEMIJOIN,
            left_inner->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &semi_join_conditions =
      left_inner->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, semi_join_conditions.size());
  EXPECT_EQ("(t2.x = t3.x)", ItemToString(semi_join_conditions[0]));

  AccessPath *first_table_semi = left_inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_semi->type);
  EXPECT_EQ(m_fake_tables["t3"], first_table_semi->table_scan().table);

  AccessPath *second_table_semi = left_inner->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, second_table_semi->type);
  EXPECT_EQ(m_fake_tables["t2"], second_table_semi->table_scan().table);
}

// We test a semijoin having multiple equalites and a non-equal function
TEST_F(ConnectJoinTest, SemiJoinWithNotEqual) {
  Query_block *query_block = ParseAndResolve(
      "SELECT 1 FROM t1 WHERE t1.x IN (SELECT t2.x FROM t2 JOIN t3 "
      "ON t2.x=t3.x JOIN t4 ON t3.x = t4.x where t3.y != t4.y)",
      /*nullable=*/true);

  query_block->join->const_tables = 0;
  query_block->join->primary_tables = query_block->join->tables = 4;

  // Set up the plan for four table join of the above query.
  // Plan would be t1 SEMIJOIN (t2 JOIN t3 JOIN t4). As the optimizer generates
  // plan for an NLJ, the table order would be t1->t4->t3->t2
  // JOIN_TAB indexing will be based on the position in the table list which is
  // t1,t2,t3,t4. However the planner would pick - t1,t4,t3,t2
  vector<Table> tables;
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b0001));
  tables.push_back(Table("t2", /*plan_idx=*/3, /*prefix_tables=*/0b1111));
  tables.push_back(Table("t3", /*plan_idx=*/2, /*prefix_tables=*/0b1101));
  tables.push_back(Table("t4", /*plan_idx=*/1, /*prefix_tables=*/0b1001));
  SetUpJoinTabs(query_block, /*num_tables=*/4, tables);

  // As mentioned before, the planner at the end would pick - t1,t4,t3,t2
  // QEP_TAB indexing is based on the final plan and not the pos_in_table_list.
  tables.clear();
  tables.push_back(Table("t1", /*plan_idx=*/0, /*prefix_tables=*/0b0001));
  tables.push_back(Table("t4", /*plan_idx=*/1, /*prefix_tables=*/0b0011));
  tables.push_back(Table("t3", /*plan_idx=*/2, /*prefix_tables=*/0b0111));
  tables.push_back(Table("t2", /*plan_idx=*/3, /*prefix_tables=*/0b1111));
  SetUpQEPTabs(query_block, /*num_tables*/ 4, tables);

  COND_EQUAL *cond_equal = nullptr;
  // Generate multi-equalities.
  EXPECT_FALSE(optimize_cond(m_thd, query_block->where_cond_ref(), &cond_equal,
                             &query_block->m_table_nest,
                             &query_block->cond_value));
  JOIN_TAB *map2table[4];

  // Using the multi-equalities and based on the table order picked, create
  // join conditions.
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++)
    map2table[i] = &query_block->join->join_tab[i];
  Item **where_cond = &query_block->join->where_cond;
  *where_cond =
      substitute_for_best_equal_field(m_thd, query_block->join->where_cond,
                                      query_block->join->cond_equal, map2table);
  EXPECT_EQ(
      "((t4.x = t1.x) and (t3.x = t1.x) and (t2.x = t1.x) and (t3.y <> t4.y))",
      ItemToString(*where_cond));

  JOIN *join = query_block->join;
  // Attach conditions to tables.
  Item *cond = nullptr;
  for (unsigned int i = 0; i < query_block->join->primary_tables; i++) {
    JOIN_TAB *join_tab = &query_block->join->join_tab[i];
    table_map used_tables = join_tab->prefix_tables();
    table_map current_map = 1ULL << i;
    cond = make_cond_for_table(m_thd, *where_cond, used_tables, current_map,
                               /*exclude_expensive_cond=*/false);
    if (cond) join->qep_tab[join_tab->idx()].set_condition(cond);
  }

  // Setup semijoin path.
  join->qep_tab[3].firstmatch_return = 0;

  // Finally create access paths.
  qep_tab_map unhandled_duplicates = 0;
  qep_tab_map conditions_depend_on_outer_tables = 0;
  vector<PendingInvalidator> pending_invalidators;

  AccessPath *root = ConnectJoins(
      /*upper_first_idx=*/NO_PLAN_IDX, /*first_idx=*/0,
      /*last_idx=*/4, join->qep_tab, m_thd,
      /*calling_context=*/TOP_LEVEL, /*pending_conditions=*/nullptr,
      &pending_invalidators, /*pending_join_conditions=*/nullptr,
      &unhandled_duplicates, &conditions_depend_on_outer_tables);

  SCOPED_TRACE(PrintQueryPlan(0, root, join,
                              /*is_root_of_join=*/true));

  // Verify that we have t1 hash-semijoin (t2 hash join (t3 hash join t4
  // on t3.x = t4.x (with filter on t3.y != t4.y)) on t2.x = t4.x)
  // on t1.x = t4.x)
  // Earlier filter would not be placed with the inner join between
  // t3 and t4
  ASSERT_EQ(AccessPath::HASH_JOIN, root->type);
  EXPECT_EQ(RelationalExpression::SEMIJOIN,
            root->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &semijoin_conditions =
      root->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, semijoin_conditions.size());
  EXPECT_EQ("(t4.x = t1.x)", ItemToString(semijoin_conditions[0]));

  AccessPath *semi_outer = root->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, semi_outer->type);
  EXPECT_EQ(m_fake_tables["t1"], semi_outer->table_scan().table);

  AccessPath *semi_inner = root->hash_join().inner;
  ASSERT_EQ(AccessPath::HASH_JOIN, semi_inner->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            semi_inner->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &inner_join_conditions =
      semi_inner->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, inner_join_conditions.size());
  EXPECT_EQ("(t2.x = t4.x)", ItemToString(inner_join_conditions[0]));

  AccessPath *first_table_inner = semi_inner->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_inner->type);
  EXPECT_EQ(m_fake_tables["t2"], first_table_inner->table_scan().table);

  AccessPath *second_table_inner = semi_inner->hash_join().inner;
  ASSERT_EQ(AccessPath::FILTER, second_table_inner->type);
  EXPECT_EQ("(t3.y <> t4.y)",
            ItemToString(second_table_inner->filter().condition));

  AccessPath *filter_child = second_table_inner->filter().child;
  ASSERT_EQ(AccessPath::HASH_JOIN, filter_child->type);
  EXPECT_EQ(RelationalExpression::INNER_JOIN,
            filter_child->hash_join().join_predicate->expr->type);

  const Mem_root_array<Item_eq_base *> &below_filter_inner_join_conditions =
      filter_child->hash_join().join_predicate->expr->equijoin_conditions;
  ASSERT_EQ(1, below_filter_inner_join_conditions.size());
  EXPECT_EQ("(t3.x = t4.x)",
            ItemToString(below_filter_inner_join_conditions[0]));

  AccessPath *first_table_inner_to_filter = filter_child->hash_join().outer;
  ASSERT_EQ(AccessPath::TABLE_SCAN, first_table_inner_to_filter->type);
  EXPECT_EQ(m_fake_tables["t3"],
            first_table_inner_to_filter->table_scan().table);

  AccessPath *second_table_inner_to_filter = filter_child->hash_join().inner;
  ASSERT_EQ(AccessPath::TABLE_SCAN, second_table_inner_to_filter->type);
  EXPECT_EQ(m_fake_tables["t4"],
            second_table_inner_to_filter->table_scan().table);
}
