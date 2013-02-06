/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "test_utils.h"

#include "opt_range.cc"

namespace opt_range_unittest {

using my_testing::Server_initializer;

class SelArgTest : public ::testing::Test
{
protected:
  SelArgTest()
  {
    memset(&m_opt_param, 0, sizeof(m_opt_param));
  }

  virtual void SetUp()
  {
    initializer.SetUp();
    m_opt_param.thd= thd();
    m_opt_param.mem_root= &m_alloc;
    m_opt_param.current_table= 1<<0;
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
const SEL_ARG  *null_arg= NULL;


class Mock_field_long : public Field_long
{
public:
  Mock_field_long(THD *thd, Item *item)
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

    bitmap_init(&share_allset, 0, sizeof(my_bitmap_map), 0);
    bitmap_set_above(&share_allset, 0, 1); //all bits 1
    m_share.all_set= share_allset;

    memset(&m_table, 0, sizeof(m_table));
    m_table.s= &m_share;

    bitmap_init(&tbl_readset, 0, sizeof(my_bitmap_map), 0);
    m_table.read_set= &tbl_readset;

    bitmap_init(&tbl_writeset, 0, sizeof(my_bitmap_map), 0);
    m_table.write_set= &tbl_writeset;

    m_table.map= 1<<0;
    m_table.in_use= thd;
    this->table_name= &m_table_name;
    this->table= &m_table;
    this->ptr= (uchar*) alloc_root((thd->mem_root), KEY_LENGTH);
    if (item)
      item->save_in_field_no_warnings(this, true);      
  }
  ~Mock_field_long()
  {
    bitmap_free(&share_allset);
    bitmap_free(&tbl_readset);
    bitmap_free(&tbl_writeset);
  }

  // #bytes to store the value - see Field_long::key_lenght()
  static const int KEY_LENGTH= 4;
  const char *m_table_name;
  TABLE_SHARE m_share;
  TABLE       m_table;
  MY_BITMAP   share_allset;
  MY_BITMAP   tbl_readset;
  MY_BITMAP   tbl_writeset;
};


static void print_selarg_ranges(String *s, SEL_ARG *sel_arg, 
                                const KEY_PART_INFO *kpi)
{
  for (SEL_ARG *cur= sel_arg->first(); 
       cur != &null_element; 
       cur= cur->right)
  {
    String current_range;
    append_range(&current_range, kpi, cur->min_value, cur->max_value, 
                 cur->min_flag | cur->max_flag);

    if (s->length() > 0)
      s->append(STRING_WITH_LEN("\n"));

    s->append(current_range);
  }
}


TEST_F(SelArgTest, SimpleCond)
{
  EXPECT_NE(null_tree, get_mm_tree(&m_opt_param, new Item_int(42)));
}


/*
  TODO: Here we try to build a range, but a lot of mocking remains
  before it works as intended. Currently get_mm_tree() returns NULL
  because m_opt_param.key_parts and m_opt_param.key_parts_end have not
  been setup. 
*/
TEST_F(SelArgTest, EqualCond)
{
  Mock_field_long field_long(thd(), NULL);
  m_opt_param.table= &field_long.m_table;
  SEL_TREE *tree= get_mm_tree(&m_opt_param,
                              new Item_equal(new Item_int(42),
                                             new Item_field(&field_long)));
  EXPECT_EQ(null_tree, tree);
}


TEST_F(SelArgTest, SelArgOnevalue)
{
  Mock_field_long field_long7(thd(), new Item_int(7));

  KEY_PART_INFO kpi;
  kpi.init_from_field(&field_long7);

  uchar range_val7[field_long7.KEY_LENGTH];
  field_long7.get_key_image(range_val7, kpi.length, Field::itRAW);

  SEL_ARG sel_arg7(&field_long7, range_val7, range_val7);
  String range_string;
  print_selarg_ranges(&range_string, &sel_arg7, &kpi);
  const char expected[]= "7 <= field_name <= 7";
  EXPECT_STREQ(expected, range_string.c_ptr());

  sel_arg7.min_flag|= NO_MIN_RANGE;
  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg7, &kpi);
  const char expected2[]= "field_name <= 7";
  EXPECT_STREQ(expected2, range_string.c_ptr());

  sel_arg7.max_flag= NEAR_MAX;
  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg7, &kpi);
  const char expected3[]= "field_name < 7";
  EXPECT_STREQ(expected3, range_string.c_ptr());

  sel_arg7.min_flag= NEAR_MIN;
  sel_arg7.max_flag= NO_MAX_RANGE;
  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg7, &kpi);
  const char expected4[]= "7 < field_name";
  EXPECT_STREQ(expected4, range_string.c_ptr());

  sel_arg7.min_flag= 0;
  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg7, &kpi);
  const char expected5[]= "7 <= field_name";
  EXPECT_STREQ(expected5, range_string.c_ptr());
}


TEST_F(SelArgTest, SelArgBetween)
{
  Mock_field_long field_long3(thd(), new Item_int(3));
  Mock_field_long field_long5(thd(), new Item_int(5));

  KEY_PART_INFO kpi;
  kpi.init_from_field(&field_long3);

  uchar range_val3[field_long3.KEY_LENGTH];
  field_long3.get_key_image(range_val3, kpi.length, Field::itRAW);

  uchar range_val5[field_long5.KEY_LENGTH];
  field_long5.get_key_image(range_val5, kpi.length, Field::itRAW);

  SEL_ARG sel_arg35(&field_long3, range_val3, range_val5);

  String range_string;
  print_selarg_ranges(&range_string, &sel_arg35, &kpi);
  const char expected[]= "3 <= field_name <= 5";
  EXPECT_STREQ(expected, range_string.c_ptr());

  range_string.length(0);
  sel_arg35.min_flag= NEAR_MIN;
  print_selarg_ranges(&range_string, &sel_arg35, &kpi);
  const char expected2[]= "3 < field_name <= 5";
  EXPECT_STREQ(expected2, range_string.c_ptr());

  range_string.length(0);
  sel_arg35.max_flag= NEAR_MAX;
  print_selarg_ranges(&range_string, &sel_arg35, &kpi);
  const char expected3[]= "3 < field_name < 5";
  EXPECT_STREQ(expected3, range_string.c_ptr());

  range_string.length(0);
  sel_arg35.min_flag= 0;
  print_selarg_ranges(&range_string, &sel_arg35, &kpi);
  const char expected4[]= "3 <= field_name < 5";
  EXPECT_STREQ(expected4, range_string.c_ptr());

  range_string.length(0);
  sel_arg35.min_flag= NO_MIN_RANGE;
  sel_arg35.max_flag= 0;
  print_selarg_ranges(&range_string, &sel_arg35, &kpi);
  const char expected5[]= "field_name <= 5";
  EXPECT_STREQ(expected5, range_string.c_ptr());

  range_string.length(0);
  sel_arg35.min_flag= 0;
  sel_arg35.max_flag= NO_MAX_RANGE;
  print_selarg_ranges(&range_string, &sel_arg35, &kpi);
  const char expected6[]= "3 <= field_name";
  EXPECT_STREQ(expected6, range_string.c_ptr());
}

TEST_F(SelArgTest, CopyMax)
{
  Mock_field_long field_long3(thd(), new Item_int(3));
  Mock_field_long field_long5(thd(), new Item_int(5));

  KEY_PART_INFO kpi;
  kpi.init_from_field(&field_long3);

  uchar range_val3[field_long3.KEY_LENGTH];
  field_long3.get_key_image(range_val3, kpi.length, Field::itRAW);

  uchar range_val5[field_long5.KEY_LENGTH];
  field_long5.get_key_image(range_val5, kpi.length, Field::itRAW);

  SEL_ARG sel_arg3(&field_long3, range_val3, range_val3);
  sel_arg3.min_flag= NO_MIN_RANGE;
  SEL_ARG sel_arg5(&field_long5, range_val5, range_val5);
  sel_arg5.min_flag= NO_MIN_RANGE;

  String range_string;
  print_selarg_ranges(&range_string, &sel_arg3, &kpi);
  const char expected[]= "field_name <= 3";
  EXPECT_STREQ(expected, range_string.c_ptr());

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg5, &kpi);
  const char expected2[]= "field_name <= 5";
  EXPECT_STREQ(expected2, range_string.c_ptr());

  /*
    Ranges now:
                       -inf ----------------3-5----------- +inf
    sel_arg3:          [-------------------->
    sel_arg5:          [---------------------->
    Below: merge these two ranges into sel_arg3 using copy_max()
  */
  bool full_range= sel_arg3.copy_max(&sel_arg5);
  // The merged range does not cover all possible values
  EXPECT_FALSE(full_range);

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg3, &kpi);
  const char expected3[]= "field_name <= 5";
  EXPECT_STREQ(expected3, range_string.c_ptr());

  range_string.length(0);
  sel_arg5.min_flag= 0;
  sel_arg5.max_flag= NO_MAX_RANGE;
  print_selarg_ranges(&range_string, &sel_arg5, &kpi);
  const char expected4[]= "5 <= field_name";
  EXPECT_STREQ(expected4, range_string.c_ptr());

  /*
    Ranges now:
                       -inf ----------------3-5----------- +inf
    sel_arg3:          [---------------------->
    sel_arg5:                                 <---------------]
    Below: merge these two ranges into sel_arg3 using copy_max()
  */

  full_range= sel_arg3.copy_max(&sel_arg5);
  // The new range covers all possible values
  EXPECT_TRUE(full_range);

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg3, &kpi);
  const char expected5[]= "field_name";
  EXPECT_STREQ(expected5, range_string.c_ptr());
}

TEST_F(SelArgTest, CopyMin)
{
  Mock_field_long field_long3(thd(), new Item_int(3));
  Mock_field_long field_long5(thd(), new Item_int(5));

  KEY_PART_INFO kpi;
  kpi.init_from_field(&field_long3);

  uchar range_val3[field_long3.KEY_LENGTH];
  field_long3.get_key_image(range_val3, kpi.length, Field::itRAW);

  uchar range_val5[field_long5.KEY_LENGTH];
  field_long5.get_key_image(range_val5, kpi.length, Field::itRAW);

  SEL_ARG sel_arg3(&field_long3, range_val3, range_val3);
  sel_arg3.max_flag= NO_MAX_RANGE;
  SEL_ARG sel_arg5(&field_long5, range_val5, range_val5);
  sel_arg5.max_flag= NO_MAX_RANGE;

  String range_string;
  print_selarg_ranges(&range_string, &sel_arg3, &kpi);
  const char expected[]= "3 <= field_name";
  EXPECT_STREQ(expected, range_string.c_ptr());

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg5, &kpi);
  const char expected2[]= "5 <= field_name";
  EXPECT_STREQ(expected2, range_string.c_ptr());

  /*
    Ranges now:
                       -inf ----------------3-5----------- +inf
    sel_arg3:                               <-----------------]
    sel_arg5:                                 <---------------]
    Below: merge these two ranges into sel_arg3 using copy_max()
  */
  bool full_range= sel_arg5.copy_min(&sel_arg3);
  // The merged range does not cover all possible values
  EXPECT_FALSE(full_range);

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg5, &kpi);
  const char expected3[]= "3 <= field_name";
  EXPECT_STREQ(expected3, range_string.c_ptr());

  range_string.length(0);
  sel_arg3.max_flag= 0;
  sel_arg3.min_flag= NO_MIN_RANGE;
  print_selarg_ranges(&range_string, &sel_arg3, &kpi);
  const char expected4[]= "field_name <= 3";
  EXPECT_STREQ(expected4, range_string.c_ptr());

  /*
    Ranges now:
                       -inf ----------------3-5----------- +inf
    sel_arg3:          [-------------------->                
    sel_arg5:                               <-----------------]
    Below: merge these two ranges into sel_arg5 using copy_min()
  */

  full_range= sel_arg5.copy_min(&sel_arg3);
  // The new range covers all possible values
  EXPECT_TRUE(full_range);

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg5, &kpi);
  const char expected5[]= "field_name";
  EXPECT_STREQ(expected5, range_string.c_ptr());
}


TEST_F(SelArgTest, KeyOr1)
{
  Mock_field_long field_long3(thd(), new Item_int(3));
  Mock_field_long field_long4(thd(), new Item_int(4));

  KEY_PART_INFO kpi;
  kpi.init_from_field(&field_long3);

  uchar range_val3[field_long3.KEY_LENGTH];
  field_long3.get_key_image(range_val3, kpi.length, Field::itRAW);

  uchar range_val4[field_long4.KEY_LENGTH];
  field_long4.get_key_image(range_val4, kpi.length, Field::itRAW);

  SEL_ARG sel_arg_lt3(&field_long3, range_val3, range_val3);
  sel_arg_lt3.part= 0;
  sel_arg_lt3.min_flag= NO_MIN_RANGE;
  sel_arg_lt3.max_flag= NEAR_MAX;

  SEL_ARG sel_arg_gt3(&field_long3, range_val3, range_val3);
  sel_arg_gt3.part= 0;
  sel_arg_gt3.min_flag= NEAR_MIN;
  sel_arg_gt3.max_flag= NO_MAX_RANGE;

  SEL_ARG sel_arg_lt4(&field_long4, range_val4, range_val4);
  sel_arg_lt4.part= 0;
  sel_arg_lt4.min_flag= NO_MIN_RANGE;
  sel_arg_lt4.max_flag= NEAR_MAX;

  String range_string;
  print_selarg_ranges(&range_string, &sel_arg_lt3, &kpi);
  const char expected_lt3[]= "field_name < 3";
  EXPECT_STREQ(expected_lt3, range_string.c_ptr());

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg_gt3, &kpi);
  const char expected_gt3[]= "3 < field_name";
  EXPECT_STREQ(expected_gt3, range_string.c_ptr());

  range_string.length(0);
  print_selarg_ranges(&range_string, &sel_arg_lt4, &kpi);
  const char expected_lt4[]= "field_name < 4";
  EXPECT_STREQ(expected_lt4, range_string.c_ptr());


  /*
    Ranges now:
                       -inf ----------------34----------- +inf
    sel_arg_lt3:       [-------------------->
    sel_arg_gt3:                             <---------------]
    sel_arg_lt4:       [--------------------->
  */

  SEL_ARG *tmp= key_or(NULL, &sel_arg_lt3, &sel_arg_gt3);

  /*
    Ranges now:
                       -inf ----------------34----------- +inf
    tmp:               [--------------------><---------------]
    sel_arg_lt4:       [--------------------->
  */
  range_string.length(0);
  print_selarg_ranges(&range_string, tmp, &kpi);
  const char expected_merged[]= 
    "field_name < 3\n"
    "3 < field_name";
  EXPECT_STREQ(expected_merged, range_string.c_ptr());

  SEL_ARG *tmp2= key_or(NULL, tmp, &sel_arg_lt4);
  EXPECT_EQ(null_arg, tmp2);
}

}


