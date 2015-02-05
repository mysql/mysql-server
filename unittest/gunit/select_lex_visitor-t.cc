/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_config.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "parsertest.h"
#include "test_utils.h"
#include "select_lex_visitor.h"
#include "sql_optimizer.h"
#include "sql_lex.h"

namespace select_lex_visitor_unittest {

using my_testing::Server_initializer;
using std::vector;

class SelectLexVisitorTest : public ParserTest
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }
};


/// A visitor that remembers what it has seen.
class Remembering_visitor : public Select_lex_visitor
{
public:
  vector<int> seen_items;

  Remembering_visitor() :
    m_saw_select_lex(false),
    m_saw_select_lex_unit(false)
  {}

  virtual bool visit_union(SELECT_LEX_UNIT *)
  {
    m_saw_select_lex_unit= true;
    return false;
  }

  virtual bool visit_query_block(SELECT_LEX *)
  {
    m_saw_select_lex= true;
    return false;
  }

  virtual bool visit_item(Item *item)
  {
    seen_items.push_back(item->val_int());
    return false;
  }

  bool saw_select_lex() { return m_saw_select_lex; }
  bool saw_select_lex_unit() { return m_saw_select_lex_unit; }

  ~Remembering_visitor() {}

private:
  bool m_saw_select_lex, m_saw_select_lex_unit;
};


/**
  Google mock only works for objects allocated on the stack, and the Item
  classes appear to have been designed to make it impossible to allocate them
  on the stack because of the mandatory free list. But this little mix-in
  class lets us inherit any item class and do that. See Mock_item_int below
  how to use it.
*/
template <class Item_class>
class Stack_allocated_item : public Item_class
{
public:
  Stack_allocated_item(int value) : Item_class(value)
  {
    // Undo what Item::Item() does.
    THD *thd= current_thd;
    thd->free_list= this->next;
    this->next= NULL;
  }
};


class Mock_item_int : public Stack_allocated_item<Item_int>
{
public:
  Mock_item_int() : Stack_allocated_item<Item_int>(42) {}
  MOCK_METHOD3(walk, bool(Item_processor, Item::enum_walk, uchar *));
};


TEST_F(SelectLexVisitorTest, SelectLex)
{
  using ::testing::_;

  Mock_item_int where;
  Mock_item_int having;
  EXPECT_CALL(where, walk(_, _, _)).Times(1);
  EXPECT_CALL(having, walk(_, _, _)).Times(1);

  SELECT_LEX query_block(NULL, NULL, &where, &having, NULL, NULL);
  Item *ref_ptrs[5]= { NULL, NULL, NULL, NULL, NULL };
  query_block.ref_pointer_array= Ref_ptr_array(ref_ptrs, 5);

  SELECT_LEX_UNIT unit(CTX_NONE);

  LEX lex;
  query_block.include_down(&lex, &unit);
  List<Item> items;
  JOIN join(thd(), &query_block);
  join.where_cond= &where;
  join.having_for_explain= &having;

  query_block.join= &join;
  Remembering_visitor visitor;
  unit.accept(&visitor);
  EXPECT_TRUE(visitor.saw_select_lex());
  EXPECT_TRUE(visitor.saw_select_lex_unit());
}


TEST_F(SelectLexVisitorTest, InsertList)
{
  SELECT_LEX *select_lex= parse("INSERT INTO t VALUES (1, 2, 3)", 0);
  ASSERT_FALSE(select_lex == NULL);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(3U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(2, visitor.seen_items[1]);
  EXPECT_EQ(3, visitor.seen_items[2]);
}


TEST_F(SelectLexVisitorTest, InsertList2)
{
  SELECT_LEX *select_lex= parse("INSERT INTO t VALUES (1, 2), (3, 4)", 0);
  ASSERT_FALSE(select_lex == NULL);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(4U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(2, visitor.seen_items[1]);
  EXPECT_EQ(3, visitor.seen_items[2]);
  EXPECT_EQ(4, visitor.seen_items[3]);
}


TEST_F(SelectLexVisitorTest, InsertSet)
{
  SELECT_LEX *select_lex= parse("INSERT INTO t SET a=1, b=2, c=3", 0);
  ASSERT_FALSE(select_lex == NULL);

  Remembering_visitor visitor;
  thd()->lex->accept(&visitor);
  ASSERT_EQ(3U, visitor.seen_items.size());
  EXPECT_EQ(1, visitor.seen_items[0]);
  EXPECT_EQ(2, visitor.seen_items[1]);
  EXPECT_EQ(3, visitor.seen_items[2]);
}

}
