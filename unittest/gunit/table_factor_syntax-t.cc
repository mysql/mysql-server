/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
#include <stddef.h>

#include <string>

#include "sql/item_func.h"
#include "sql/nested_join.h"
#include "sql/sql_lex.h"
#include "template_utils.h"
#include "thr_lock.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/test_utils.h"

namespace table_factor_syntax_unittest {

using my_testing::Mock_error_handler;
using my_testing::Server_initializer;

class TableFactorSyntaxTest : public ParserTest {
 protected:
  void test_table_factor_syntax(const char *query, int num_terms,
                                bool expect_syntax_error) {
    Query_block *term1 = parse(query, expect_syntax_error ? ER_PARSE_ERROR : 0);
    EXPECT_EQ(nullptr, term1->first_inner_query_expression());
    EXPECT_EQ(nullptr, term1->next_select_in_list());
    EXPECT_EQ(1, term1->get_fields_list()->front()->val_int());

    Query_expression *top_union = term1->master_query_expression();
    EXPECT_EQ(nullptr, top_union->outer_query_block());

    if (num_terms > 1) {
      Query_block *term2 = term1->next_query_block();
      ASSERT_FALSE(term2 == nullptr);

      EXPECT_EQ(nullptr, term2->first_inner_query_expression());
      EXPECT_EQ(term1, term2->next_select_in_list());
      EXPECT_EQ(2, term2->get_fields_list()->front()->val_int());

      if (num_terms <= 2) {
        EXPECT_EQ(nullptr, term2->next_query_block());
      }

      EXPECT_EQ(top_union, term2->master_query_expression());
    }
  }

  void test_global_limit(const char *query) {
    Query_block *first_term = parse(query);
    Query_expression *unit = first_term->master_query_expression();
    EXPECT_EQ(1U, unit->global_parameters()->order_list.elements) << query;
    EXPECT_FALSE(unit->global_parameters()->select_limit == nullptr) << query;
  }
};

void check_query_block(Query_block *block, int select_list_item,
                       const char *tablename) {
  ASSERT_EQ(1U, block->num_visible_fields());
  EXPECT_EQ(select_list_item, block->fields.front()->val_int());

  ASSERT_EQ(1U, block->m_table_nest.size());
  EXPECT_STREQ(tablename, block->m_table_nest.front()->alias);
}

TEST_F(TableFactorSyntaxTest, Single) {
  Query_block *term = parse("SELECT 2 FROM (SELECT 1 FROM t1) dt;", 0);
  EXPECT_EQ(nullptr, term->outer_query_block());
  Query_expression *top_union = term->master_query_expression();

  EXPECT_EQ(term, top_union->first_query_block());
  EXPECT_EQ(nullptr, term->next_query_block());

  ASSERT_EQ(1U, term->m_table_nest.size());
  EXPECT_STREQ("dt", term->m_table_nest.front()->alias);

  Query_expression *inner_union = term->first_inner_query_expression();

  Query_block *inner_term = inner_union->first_query_block();

  check_query_block(inner_term, 1, "t1");
}

TEST_F(TableFactorSyntaxTest, TablelessTableSubquery) {
  Query_block *term = parse("SELECT 1 FROM (SELECT 2) a;", 0);
  EXPECT_EQ(nullptr, term->outer_query_block());
  Query_expression *top_union = term->master_query_expression();

  EXPECT_EQ(term, top_union->first_query_block());
  EXPECT_EQ(nullptr, term->next_query_block());

  ASSERT_EQ(1U, term->m_table_nest.size());
  EXPECT_STREQ("a", term->m_table_nest.front()->alias);

  Query_expression *inner_union = term->first_inner_query_expression();

  Query_block *inner_term = inner_union->first_query_block();

  EXPECT_EQ(nullptr, inner_term->first_inner_query_expression());

  EXPECT_NE(
      term,
      term->get_table_list()->derived_query_expression()->first_query_block())
      << "No cycle in the AST, please.";
}

TEST_F(TableFactorSyntaxTest, Union) {
  Query_block *block = parse(
      "SELECT 1 FROM (SELECT 1 FROM t1 UNION SELECT 2 FROM t2) dt "
      "WHERE d1.a = 1",
      0);
  Query_expression *top_union = block->master_query_expression();

  EXPECT_EQ(block, top_union->first_query_block());
  EXPECT_EQ(nullptr, block->next_query_block());

  Table_ref *dt = block->get_table_list();
  EXPECT_EQ(dt, block->context.first_name_resolution_table);

  Item_func *top_where_cond = down_cast<Item_func *>(block->where_cond());
  Item_field *d1a = down_cast<Item_field *>(top_where_cond->arguments()[0]);
  ASSERT_FALSE(d1a->context == nullptr);
  EXPECT_EQ(dt, d1a->context->first_name_resolution_table);

  EXPECT_EQ(1U, block->m_table_nest.size());
  EXPECT_STREQ("dt", block->m_table_nest.front()->alias);

  Query_expression *inner_union = block->first_inner_query_expression();

  Query_block *first_inner_block = inner_union->first_query_block();
  Query_block *second_inner_block = first_inner_block->next_query_block();

  Table_ref *t1 = first_inner_block->get_table_list();
  Table_ref *t2 = second_inner_block->get_table_list();

  EXPECT_EQ(t1, first_inner_block->context.first_name_resolution_table);
  EXPECT_EQ(t2, second_inner_block->context.first_name_resolution_table);

  EXPECT_EQ(nullptr, t1->nested_join);
  EXPECT_EQ(nullptr, t2->nested_join);

  check_query_block(first_inner_block, 1, "t1");
  check_query_block(second_inner_block, 2, "t2");

  EXPECT_EQ(nullptr, block->outer_query_block());
}

TEST_F(TableFactorSyntaxTest, NestedJoin) {
  Query_block *term = parse("SELECT * FROM (t1 JOIN t2 ON TRUE)", 0);
  Query_expression *top_union = term->master_query_expression();

  EXPECT_EQ(term, top_union->first_query_block());
}

TEST_F(TableFactorSyntaxTest, NestedNestedJoin) {
  Query_block *term =
      parse("SELECT * FROM ((t1 JOIN t2 ON TRUE) JOIN t3 ON TRUE)", 0);
  Query_expression *top_union = term->master_query_expression();

  EXPECT_EQ(term, top_union->first_query_block());
}

TEST_F(TableFactorSyntaxTest, NestedTableReferenceList) {
  Query_block *term1 =
      parse("SELECT * FROM t1 LEFT JOIN ( t2 JOIN t3 JOIN t4 ) ON t1.a = t3.a");

  Query_block *term2 =
      parse("SELECT * FROM t1 LEFT JOIN ( t2, t3, t4 ) ON t1.a = t3.a");

  Query_expression *top_union = term1->master_query_expression();
  Query_expression *top_union2 = term2->master_query_expression();

  EXPECT_EQ(term1, top_union->first_query_block());
  EXPECT_EQ(term2, top_union2->first_query_block());

  EXPECT_EQ(4U, term1->m_table_list.elements);
  ASSERT_EQ(4U, term2->m_table_list.elements);

  EXPECT_STREQ("t1", term1->get_table_list()->alias);
  EXPECT_STREQ("t1", term2->get_table_list()->alias);

  EXPECT_STREQ("(nest_last_join)", term1->m_current_table_nest->front()->alias);
  EXPECT_STREQ("(nest_last_join)", term2->m_current_table_nest->front()->alias);

  Table_ref *t2_join_t3_join_t4 = term1->m_current_table_nest->front();
  Table_ref *t2_join_t3_join_t4_2 = term2->m_current_table_nest->front();

  Table_ref *t3_join_t4 = t2_join_t3_join_t4->nested_join->m_tables.front();
  Table_ref *t3_join_t4_2 = t2_join_t3_join_t4_2->nested_join->m_tables.front();

  EXPECT_STREQ("(nest_last_join)", t3_join_t4->alias);
  EXPECT_STREQ("(nest_last_join)", t3_join_t4_2->alias);

  EXPECT_STREQ("t4", t3_join_t4->nested_join->m_tables.front()->alias);
}

TEST_F(TableFactorSyntaxTest, LimitAndOrder) {
  test_global_limit("SELECT 1 AS c UNION (SELECT 1 AS c) ORDER BY c LIMIT 1");
  test_global_limit("(SELECT 1 AS c UNION SELECT 1 AS c) ORDER BY c LIMIT 1");
  test_global_limit("((SELECT 1 AS c) UNION SELECT 1 AS c) ORDER BY c LIMIT 1");
  test_global_limit("(SELECT 1 AS c UNION (SELECT 1 AS c)) ORDER BY c LIMIT 1");
}

}  // namespace table_factor_syntax_unittest
