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

#include <vector>

#include "handler-t.h"
#include "fake_table.h"
#include "test_utils.h"

#include "opt_range.cc"

namespace {

using my_testing::Server_initializer;
using my_testing::delete_container_pointers;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::_;

class Fake_RANGE_OPT_PARAM : public RANGE_OPT_PARAM
{
  KEY_PART m_key_parts[64];
  Mem_root_array<KEY_PART_INFO, true> m_kpis;
  
public:

  Fake_RANGE_OPT_PARAM(THD *thd_arg, MEM_ROOT *alloc_arg, TABLE *table_arg)
    : m_kpis(alloc_arg)
  {
    m_kpis.reserve(64);

    thd= thd_arg;
    mem_root= alloc_arg;
    current_table= 1<<0;
    table= table_arg;

    alloced_sel_args= 0;
    using_real_indexes= true;
    key_parts= m_key_parts;
    key_parts_end= m_key_parts;
    keys= 0;
  }

  void add_key(List<Field> fields_in_index)
  {
    List_iterator<Field> it(fields_in_index);
    int cur_kp= 0;

    table->key_info[keys].actual_key_parts= 0;
    for (Field *cur_field= it++; cur_field; cur_field= it++, cur_kp++)
    {
      KEY_PART_INFO *kpi= m_kpis.end();  // Points past the end.
      m_kpis.push_back(KEY_PART_INFO()); // kpi now points to a new element
      kpi->init_from_field(cur_field);

      key_parts_end->key=          keys;
      key_parts_end->part=         cur_kp;
      key_parts_end->length=       kpi->store_length;
      key_parts_end->store_length= kpi->store_length;
      key_parts_end->field=        kpi->field;
      key_parts_end->null_bit=     kpi->null_bit;
      key_parts_end->image_type =  Field::itRAW;

      key_parts_end++;
      table->key_info[keys].key_part[cur_kp]= *kpi;
      table->key_info[keys].actual_key_parts++;
    }
    table->key_info[keys].user_defined_key_parts=
      table->key_info[keys].actual_key_parts;
    real_keynr[keys]= keys;
    keys++;
  }

  ~Fake_RANGE_OPT_PARAM()
  {
    for (uint i= 0; i < keys; i++)
    {
      table->key_info[i].actual_key_parts= 0;
      table->key_info[i].user_defined_key_parts= 0;
    }
  }

};

class Mock_field_long : public Field_long
{
private:
  Fake_TABLE *m_fake_tbl;

public:
  Mock_field_long(THD *thd, Item *item, const char *name, bool create_table)
    : Field_long(0,                           // ptr_arg
                 8,                           // len_arg
                 NULL,                        // null_ptr_arg
                 0,                           // null_bit_arg
                 Field::NONE,                 // unireg_check_arg
                 name ? name : "field_name",  // field_name_arg
                 false,                       // zero_arg
                 false)                       // unsigned_arg
  {
    if (create_table)
      m_fake_tbl= new Fake_TABLE(this);
    else
      m_fake_tbl= NULL;

    this->ptr= (uchar*) alloc_root((thd->mem_root), KEY_LENGTH);
    if (item)
      item->save_in_field_no_warnings(this, true);
  }

  ~Mock_field_long()
  {
    delete m_fake_tbl;
    m_fake_tbl= NULL;
  }

  // #bytes to store the value - see Field_long::key_lenght()
  static const int KEY_LENGTH= 4;
};


class SelArgTest : public ::testing::Test
{
protected:
  SelArgTest() : m_ftable(NULL), m_opt_param(NULL), m_mock_handler(NULL)
  {
  }

  virtual void SetUp()
  {
    initializer.SetUp();
    init_sql_alloc(&m_alloc, thd()->variables.range_alloc_block_size, 0);
  }

  virtual void TearDown()
  {
    delete_container_pointers(m_table_fields);
    delete m_mock_handler;
    delete m_opt_param;
    delete m_ftable;

    initializer.TearDown();
    free_root(&m_alloc, MYF(0));
  }

  THD *thd() { return initializer.thd(); }

  void create_table(List<char*> field_names_arg)
  {
    List_iterator<char*> fld_name_it(field_names_arg);
    for (char **cur_name= fld_name_it++; *cur_name; cur_name= fld_name_it++)
      m_field_list.push_back(new Mock_field_long(thd(), NULL,
                                                 *cur_name, false));

    create_table_common();
  }

  void create_table(const char *field_name)
  {
    m_field_list.push_back(new Mock_field_long(thd(), NULL,
                                               field_name, false));
    create_table_common();
  }

  void create_table(const char *field_name1, const char *field_name2)
  {
    m_field_list.push_back(new Mock_field_long(thd(), NULL,
                                               field_name1, false));
    m_field_list.push_back(new Mock_field_long(thd(), NULL,
                                               field_name2, false));
    create_table_common();
  }

  Server_initializer initializer;
  MEM_ROOT           m_alloc;
  
  List<Field> m_field_list;
  Fake_TABLE *m_ftable;
  Fake_RANGE_OPT_PARAM *m_opt_param;
  Mock_HANDLER *m_mock_handler;
  std::vector<Mock_field_long*> m_table_fields;

private:
  void create_table_common()
  {
    m_ftable= new Fake_TABLE(m_field_list);
    m_opt_param= new Fake_RANGE_OPT_PARAM(thd(), &m_alloc, m_ftable);
    handlerton *hton= NULL;
    m_mock_handler=
      new NiceMock<Mock_HANDLER>(hton, m_ftable->get_share());
    m_ftable->set_handler(m_mock_handler);

    List_iterator<Field> it(m_field_list);
    for (Field *cur_field= it++; cur_field; cur_field= it++)
      m_table_fields.push_back(static_cast<Mock_field_long*>(cur_field));

    ON_CALL(*m_mock_handler, index_flags(_, _, true))
      .WillByDefault(Return(HA_READ_RANGE));
  }
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
  Fake_RANGE_OPT_PARAM opt_param(thd(), &m_alloc, NULL);
  EXPECT_NE(null_tree, get_mm_tree(&opt_param, new Item_int(42)));
}


/*
  Exercise range optimizer without adding indexes
*/
TEST_F(SelArgTest, EqualCondNoIndexes)
{
  Mock_field_long field_long(thd(), NULL, NULL, true);
  Fake_RANGE_OPT_PARAM opt_param(thd(), &m_alloc, field_long.table);
  SEL_TREE *tree= get_mm_tree(&opt_param,
                              new Item_equal(new Item_int(42),
                                             new Item_field(&field_long)));
  EXPECT_EQ(null_tree, tree);
}


/*
  Exercise range optimizer with single column index
*/
TEST_F(SelArgTest, GetMMTreeSingleColIndex)
{
  create_table(NULL);

  Mock_field_long *field_long= m_table_fields[0];

  List<Field> index_list;
  index_list.push_back(field_long);
  m_opt_param->add_key(index_list);

  char buff[512];
  String range_string(buff, sizeof(buff), system_charset_info);
  range_string.set_charset(system_charset_info);

  // Expected result of next test:
  const char expected[]= "result keys[0]: (42 <= field_name <= 42)";
  SEL_TREE *tree= get_mm_tree(m_opt_param,
                              new Item_equal(new Item_int(42),
                                             new Item_field(field_long)));
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected, range_string.c_ptr());


  // Expected result of next test:
  const char expected2[]=
    "result keys[0]: (42 <= field_name <= 42) OR (43 <= field_name <= 43)";
  tree= get_mm_tree(m_opt_param,
                    new Item_cond_or(new Item_equal(new Item_int(42),
                                                    new Item_field(field_long)),
                                     new Item_equal(new Item_int(43),
                                                    new Item_field(field_long)))
                    );
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected2, range_string.c_ptr());


  // Expected result of next test:
  const char expected3[]=
    "result keys[0]: "
    "(1 <= field_name <= 1) OR (2 <= field_name <= 2) OR "
    "(3 <= field_name <= 3) OR (4 <= field_name <= 4) OR "
    "(5 <= field_name <= 5) OR (6 <= field_name <= 6) OR "
    "(7 <= field_name <= 7) OR (8 <= field_name <= 8)";
  List<Item> or_list1;
  or_list1.push_back(new Item_equal(new Item_int(1),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(2),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(3),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(4),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(5),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(6),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(7),
                                    new Item_field(field_long)));
  or_list1.push_back(new Item_equal(new Item_int(8),
                                    new Item_field(field_long)));
  tree= get_mm_tree(m_opt_param, new Item_cond_or(or_list1));
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected3, range_string.c_ptr());


  // Expected result of next test:
  const char expected4[]= "result keys[0]: (7 <= field_name <= 7)";
  Item_equal *eq7= new Item_equal(new Item_int(7),
                                  new Item_field(field_long));
  tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(new Item_cond_or(or_list1), eq7));
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected4, range_string.c_ptr());


  // Expected result of next test:
  const char expected5[]=
    "result keys[0]: "
    "(1 <= field_name <= 1) OR (3 <= field_name <= 3) OR "
    "(5 <= field_name <= 5) OR (7 <= field_name <= 7)";
  List<Item> or_list2;
  or_list2.push_back(new Item_equal(new Item_int(1),
                                    new Item_field(field_long)));
  or_list2.push_back(new Item_equal(new Item_int(3),
                                    new Item_field(field_long)));
  or_list2.push_back(new Item_equal(new Item_int(5),
                                    new Item_field(field_long)));
  or_list2.push_back(new Item_equal(new Item_int(7),
                                    new Item_field(field_long)));
  or_list2.push_back(new Item_equal(new Item_int(9),
                                    new Item_field(field_long)));
  tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(new Item_cond_or(or_list1),
                                      new Item_cond_or(or_list2)));
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected5, range_string.c_ptr());
}


/*
  Exercise range optimizer with multiple column index
*/
TEST_F(SelArgTest, GetMMTreeMultipleSingleColIndex)
{
  create_table(NULL);

  Mock_field_long *field_long= m_table_fields[0];

  List<Field> index_list;
  index_list.push_back(field_long);
  m_opt_param->add_key(index_list);
  m_opt_param->add_key(index_list);

  char buff[512];
  String range_string(buff, sizeof(buff), system_charset_info);
  range_string.set_charset(system_charset_info);

  // Expected result of next test:
  const char expected[]= 
    "result keys[0]: (42 <= field_name <= 42)\n"
    "result keys[1]: (42 <= field_name <= 42)";
  SEL_TREE *tree= get_mm_tree(m_opt_param,
                              new Item_equal(new Item_int(42),
                                             new Item_field(field_long)));
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected, range_string.c_ptr());
}


/*
  Exercise range optimizer with multiple single column indexes
*/
TEST_F(SelArgTest, GetMMTreeSingleMultiColIndex)
{
  create_table("field_1", "field_2");

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];

  List<Field> index_list;
  index_list.push_back(field_long1);
  index_list.push_back(field_long2);
  m_opt_param->add_key(index_list);

  char buff[512];
  String range_string(buff, sizeof(buff), system_charset_info);
  range_string.set_charset(system_charset_info);

  // Expected result of next test:
  const char expected[]= "result keys[0]: (42 <= field_1 <= 42)";
  SEL_TREE *tree= get_mm_tree(m_opt_param,
                              new Item_equal(new Item_int(42),
                                             new Item_field(field_long1)));
  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected, range_string.c_ptr());


  // Expected result of next test:
  const char expected3[]= "result keys[0]: "
    "(42 <= field_1 <= 42 AND 10 <= field_2 <= 10)";

  tree= get_mm_tree(m_opt_param,
                    new
                    Item_cond_and(new Item_equal(new Item_int(42),
                                                 new Item_field(field_long1)),
                                  new Item_equal(new Item_int(10),
                                                 new Item_field(field_long2)))
                    );

  range_string.length(0);
  print_tree(&range_string, "result", tree, m_opt_param);
  EXPECT_STREQ(expected3, range_string.c_ptr());
}


/*
  Create SelArg with various single valued predicate
*/
TEST_F(SelArgTest, SelArgOnevalue)
{
  Mock_field_long field_long7(thd(), new Item_int(7), NULL, true);

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


/*
  Create SelArg with a between predicate
*/
TEST_F(SelArgTest, SelArgBetween)
{
  Mock_field_long field_long3(thd(), new Item_int(3), NULL, true);
  Mock_field_long field_long5(thd(), new Item_int(5), NULL, true);

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

/*
  Test SelArg::CopyMax
*/
TEST_F(SelArgTest, CopyMax)
{
  Mock_field_long field_long3(thd(), new Item_int(3), NULL, true);
  Mock_field_long field_long5(thd(), new Item_int(5), NULL, true);

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

/*
  Test SelArg::CopyMin
*/
TEST_F(SelArgTest, CopyMin)
{
  Mock_field_long field_long3(thd(), new Item_int(3), NULL, true);
  Mock_field_long field_long5(thd(), new Item_int(5), NULL, true);

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


/*
  Test SelArg::KeyOr
*/
TEST_F(SelArgTest, KeyOr1)
{
  Mock_field_long field_long3(thd(), new Item_int(3), NULL, true);
  Mock_field_long field_long4(thd(), new Item_int(4), NULL, true);

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


