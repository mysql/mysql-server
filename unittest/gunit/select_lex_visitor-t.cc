/* Copyright (c) 2006, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stddef.h>

#include "my_inttypes.h"
#include "sql/current_thd.h"
#include "sql/select_lex_visitor.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/test_utils.h"

namespace select_lex_visitor_unittest {

using my_testing::Server_initializer;
using std::vector;

class SelectLexVisitorTest : public ParserTest {
 protected:
  void SetUp() override { initializer.SetUp(); }
  void TearDown() override { initializer.TearDown(); }
};

/// A visitor that remembers what it has seen.
class Remembering_visitor : public Select_lex_visitor {
 public:
  vector<int> seen_items;
  vector<const char *> field_names;

  Remembering_visitor()
      : m_saw_query_block(false), m_saw_query_block_query_expression(false) {}

  bool visit_union(Query_expression *) override {
    m_saw_query_block_query_expression = true;
    return false;
  }

  bool visit_query_block(Query_block *) override {
    m_saw_query_block = true;
    return false;
  }

  bool visit_item(Item *item) override {
    // Not possible to call val_XXX on item_field. So just store the name.
    if (item->type() == Item::FIELD_ITEM)
      field_names.push_back(item->full_name());
    else
      seen_items.push_back(item->val_int());
    return false;
  }

  bool saw_query_block() { return m_saw_query_block; }
  bool saw_query_block_query_expression() {
    return m_saw_query_block_query_expression;
  }

  ~Remembering_visitor() override = default;

 private:
  bool m_saw_query_block, m_saw_query_block_query_expression;
};

/**
  Google mock only works for objects allocated on the stack, and the Item
  classes appear to have been designed to make it impossible to allocate them
  on the stack because of the mandatory free list. But this little mix-in
  class lets us inherit any item class and do that. See Mock_item_int below
  how to use it.
*/
template <class Item_class>
class Stack_allocated_item : public Item_class {
 public:
  Stack_allocated_item(int value_arg) : Item_class(value_arg) {
    // Undo what Item::Item() does.
    THD *thd = current_thd;
    thd->set_item_list(this->next_free);
    this->next_free = nullptr;
  }
};

class Mock_item_int : public Stack_allocated_item<Item_int> {
 public:
  Mock_item_int() : Stack_allocated_item<Item_int>(42) {}
  MOCK_METHOD3(walk, bool(Item_processor, enum_walk, uchar *));
};

TEST_F(SelectLexVisitorTest, SelectLex) {
  using ::testing::_;

  Mock_item_int where;
  Mock_item_int having;
  EXPECT_CALL(where, walk(_, _, _)).Times(1);
  EXPECT_CALL(having, walk(_, _, _)).Times(1);

  Query_block query_block(thd()->mem_root, &where, &having);

  Query_expression unit(CTX_NONE);

  LEX lex;
  query_block.include_down(&lex, &unit);
  unit.set_query_term(&query_block);
  List<Item> items;
  JOIN join(thd(), &query_block);
  join.where_cond = &where;
  join.having_for_explain = &having;

  query_block.join = &join;
  query_block.parent_lex = &lex;

  Remembering_visitor visitor;
  unit.accept(&visitor);
  EXPECT_TRUE(visitor.saw_query_block());
  EXPECT_TRUE(visitor.saw_query_block_query_expression());
}

TEST_F(SelectLexVisitorTest, InsertList) {
  Query_block *query_block = parse("INSERT INTO t VALUES (1, 2, 3)", 0);
  ASSERT_FALSE(query_block == nullptr);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(3U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(2, visitor.seen_items[1]);
  EXPECT_EQ(3, visitor.seen_items[2]);
}

TEST_F(SelectLexVisitorTest, InsertList2) {
  Query_block *query_block = parse("INSERT INTO t VALUES (1, 2), (3, 4)", 0);
  ASSERT_FALSE(query_block == nullptr);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(4U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(2, visitor.seen_items[1]);
  EXPECT_EQ(3, visitor.seen_items[2]);
  EXPECT_EQ(4, visitor.seen_items[3]);
}

TEST_F(SelectLexVisitorTest, InsertSet) {
  Query_block *query_block = parse("INSERT INTO t SET a=1, b=2, c=3", 0);
  ASSERT_FALSE(query_block == nullptr);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(3U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(2, visitor.seen_items[1]);
  EXPECT_EQ(3, visitor.seen_items[2]);

  ASSERT_EQ(3U, visitor.field_names.size());
  EXPECT_STREQ("a", visitor.field_names[0]);
  EXPECT_STREQ("b", visitor.field_names[1]);
  EXPECT_STREQ("c", visitor.field_names[2]);
}

TEST_F(SelectLexVisitorTest, ReplaceList) {
  Query_block *query_block =
      parse("REPLACE INTO t(a, b, c) VALUES (1,2,3), (4,5,6)", 0);
  ASSERT_FALSE(query_block == nullptr);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(6U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(4, visitor.seen_items[3]);
  EXPECT_EQ(6, visitor.seen_items[5]);

  ASSERT_EQ(3U, visitor.field_names.size());
  EXPECT_STREQ("a", visitor.field_names[0]);
  EXPECT_STREQ("b", visitor.field_names[1]);
  EXPECT_STREQ("c", visitor.field_names[2]);
}

TEST_F(SelectLexVisitorTest, InsertOnDuplicateKey) {
  Query_block *query_block = parse(
      "INSERT INTO t VALUES (1,2) ON DUPLICATE KEY UPDATE c= 44, a= 55", 0);
  ASSERT_FALSE(query_block == nullptr);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(4U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(44, visitor.seen_items[2]);
  EXPECT_EQ(55, visitor.seen_items[3]);

  ASSERT_EQ(2U, visitor.field_names.size());
  EXPECT_STREQ("c", visitor.field_names[0]);
  EXPECT_STREQ("a", visitor.field_names[1]);
}

TEST_F(SelectLexVisitorTest, Update) {
  Query_block *query_block = parse("UPDATE t SET a= 0, c= 25", 0);
  ASSERT_FALSE(query_block == nullptr);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(2U, visitor.seen_items.size());
  EXPECT_EQ(0, visitor.seen_items[0]);
  EXPECT_EQ(25, visitor.seen_items[1]);

  ASSERT_EQ(2U, visitor.field_names.size());
  EXPECT_STREQ("a", visitor.field_names[0]);
  EXPECT_STREQ("c", visitor.field_names[1]);
}
}  // namespace select_lex_visitor_unittest
