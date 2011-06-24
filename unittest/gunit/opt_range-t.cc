/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"

#include "opt_range.cc"

namespace {

using my_testing::Server_initializer;

class SelArgTest : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    Server_initializer::SetUpTestCase();
  }

  static void TearDownTestCase()
  {
    Server_initializer::TearDownTestCase();
  }

  SelArgTest()
  {
    memset(&m_opt_param, 0, sizeof(m_opt_param));
  }

  virtual void SetUp()
  {
    initializer.SetUp();
    m_opt_param.thd= thd();
    m_opt_param.mem_root= &m_alloc;
    init_sql_alloc(&m_alloc, thd()->variables.range_alloc_block_size, 0);
  }

  virtual void TearDown()
  {
    initializer.TearDown();
    free_root(&m_alloc, MYF(0));
  }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
  MEM_ROOT           m_alloc;
  RANGE_OPT_PARAM    m_opt_param;
};

/*
 Experiment with these to measure performance of
   'new (thd->mem_root)' Foo vs. 'new Foo'.
 With gcc 4.4.2 I see ~4% difference (in optimized mode).
*/
const int num_iterations= 10;
const int num_allocs= 10;

TEST_F(SelArgTest, AllocateExplicit)
{
  for (int ix= 0; ix < num_iterations; ++ix)
  {
    free_root(thd()->mem_root, MYF(MY_KEEP_PREALLOC));
    for (int ii= 0; ii < num_allocs; ++ii)
      new (thd()->mem_root) SEL_ARG;
  }
}

TEST_F(SelArgTest, AllocateImplicit)
{
  for (int ix= 0; ix < num_iterations; ++ix)
  {
    free_root(thd()->mem_root, MYF(MY_KEEP_PREALLOC));
    for (int ii= 0; ii < num_allocs; ++ii)
      new SEL_ARG;
  }
}

/*
  We cannot do EXPECT_NE(NULL, get_mm_tree(...))
  because of limits in google test.
 */
const SEL_TREE *null_tree= NULL;


class Mock_field_long : public Field_long
{
public:
  Mock_field_long()
    : Field_long(0,                             // ptr_arg
                 8,                             // len_arg
                 NULL,                          // null_ptr_arg
                 0,                             // null_bit_arg
                 Field::NONE,                   // unireg_check_arg
                 "field_name",                  // field_name_arg
                 false,                         // zero_arg
                 false)                         // unsigned_arg
  {
    m_table_name= "mock_table";
    memset(&m_share, 0, sizeof(m_share));
    const char *foo= "mock_db";
    m_share.db.str= const_cast<char*>(foo);
    m_share.db.length= strlen(m_share.db.str);

    memset(&m_table, 0, sizeof(m_table));
    m_table.s= &m_share;
    this->table_name= &m_table_name;
    this->table= &m_table;
  }
  const char *m_table_name;
  TABLE_SHARE m_share;
  TABLE       m_table;
};


TEST_F(SelArgTest, SimpleCond)
{
  EXPECT_NE(null_tree, get_mm_tree(&m_opt_param, new Item_int(42)));
}


TEST_F(SelArgTest, EqualCond)
{
  Mock_field_long field_long;
  EXPECT_EQ(null_tree,
            get_mm_tree(&m_opt_param,
                        new Item_equal(new Item_int(42),
                                       new Item_field(&field_long))));
}

}


