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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include <vector>
#include <sstream>
#include <string>

#include "handler-t.h"
#include "fake_table.h"
#include "test_utils.h"

#include "opt_range.cc"

namespace opt_range_unittest {

/**
  Helper class to print which line a failing test was called from.
*/
class TestFailLinePrinter
{
public:
  explicit TestFailLinePrinter(int line) : m_line(line) {}
  int m_line;
};
std::ostream &operator<< (std::ostream &s, const TestFailLinePrinter &v)
{
  return s << "called from line " << v.m_line;
}

/**
  Keep in mind the following boolean algebra definitions and rules
  when reading the tests in this file:

  Operators:
    & (and)
    | (or)
    ! (negation)

  DeMorgans laws:
    DM1: !(X & Y) <==> !X | !Y
    DM2: !(X | Y) <==> !X & !Y

  Boolean axioms:
    A1 (associativity):    X & (Y & Z)  <==>  (X & Y) & Z
                           X | (Y | Z)  <==>  (X | Y) | Z
    A2 (commutativity):    X & Y        <==>  Y & X
                           X | Y        <==>  Y | X
    A3 (identity):         X | false    <==>  X
                           X | true     <==>  true
                           X & false    <==>  false
                           X & true     <==>  X
    A4 (distributivity):   X | (Y & Z)  <==>  (X | Y) & (X | Z)
                           X & (Y | Z)  <==>  (X & Y) | (X & Z)
    A5 (complements):      X | !X       <==>  true
                           X & !X       <==>  false
    A6 (idempotence of |): X | X        <==>  X
    A7 (idempotence of &): X & X        <==>  X

  Also note that the range optimizer follows a relaxed boolean algebra
  where the result may be bigger than boolean algebra rules dictate.
  @See get_mm_tree() for explanation.
*/

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
    remove_jump_scans= false;
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
      key_parts_end->flag=         kpi->key_part_flag;
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

  void add_key(Field *field_to_index)
  {
    List<Field> index_list;
    index_list.push_back(field_to_index);
    add_key(index_list);
  }

  void add_key(Field *field_to_index1, Field *field_to_index2)
  {
    List<Field> index_list;
    index_list.push_back(field_to_index1);
    index_list.push_back(field_to_index2);
    add_key(index_list);
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

  /**
    Create a table with the requested number of fields. All fields are
    indexed. After calling this function,
    SelArgTest::m_table_fields[i] (i < nbr_fields) stores a
    Mock_field_long.

    @param  nbr_fields     The number of fields in the table
  */
  void create_table_singlecol_idx(int nbr_fields)
  {
    create_table(nbr_fields);
    for (int i= 0; i < nbr_fields; i++)
      m_opt_param->add_key(m_table_fields[i]);
  }

  /**
    Create a table with the requested number of fields without
    creating indexes. After calling this function,
    SelArgTest::m_table_fields[i] (i < nbr_fields) stores a
    Mock_field_long.

    @param  nbr_fields     The number of fields in the table
  */
  void create_table(int nbr_fields)
  {
    for (int i= 1; i <= nbr_fields; i++)
    {
      std::stringstream str;
      str << "field_" << i;
      m_field_names.push_back(str.str());
    }
    create_table(&m_field_names);
  }

  /**
    Types of range predicates that create_tree() and create_item()
    can create
  */
  enum pred_type_enum { GREATER, LESS, BETWEEN, EQUAL, NOT_EQUAL, XOR };

  /**
    Utility funtion used to simplify creation of SEL_TREEs with
    specified range predicate operators and values. Also verifies that
    the created SEL_TREE has the expected range conditions.

    @param type            The type of range predicate operator requested
    @param fld             The field used in the range predicate
    @param val1            The first value used in the range predicate
    @param val2            The second value used in the range predicate.
                           Only used for range predicates that takes two 
                           values (BETWEEN).
    @param expected_result The range conditions the created SEL_TREE
                           is expected to consist of. The format of this
                           string is what opt_range.cc print_tree() produces.

    @return SEL_TREE that has been verified to have expected range conditions.
  */
// Undefined at end of this file
#define create_tree(t, f, v1, v2, er) \
  do_create_tree(t, f, v1, v2, er, TestFailLinePrinter(__LINE__))
  SEL_TREE *do_create_tree(const pred_type_enum type, Mock_field_long *fld,
                           int val1, int val2, const char* expected_result,
                           TestFailLinePrinter called_from_line)
  {
    SEL_TREE *result;
    switch (type)
    {
    case BETWEEN:
      result= get_mm_tree(m_opt_param,
                          new Item_cond_and(create_item(GREATER, fld, val1),
                                            create_item(LESS, fld, val2)));
      break;
    case NOT_EQUAL:
      result= get_mm_tree(m_opt_param,
                          new Item_cond_or(create_item(LESS, fld, val1),
                                           create_item(GREATER, fld, val1)));
      break;
    default:
      result= get_mm_tree(m_opt_param, create_item(type, fld, val1));
    }
    SCOPED_TRACE(called_from_line);
    check_tree_result(result, SEL_TREE::KEY, expected_result);
    return result;
  }

  /**
    Utility funtion used to simplify creation of func items used as
    range predicates.

    @param type            The type of range predicate operator requested
    @param fld             The field used in the range predicate
    @param val1            The value used in the range predicate

    @return Item for the specified range predicate
  */
  Item_func *create_item(pred_type_enum type, Mock_field_long *fld, int value);

  /**
    Create instance of Xor Item_func.

    @param    item1     first item for xor condition.
    @param    item2     second item for xor condition.

    @return pointer to newly created instance of Xor Item.
  */
  Item_func_xor *create_xor_item(Item *item1, Item *item2);
 

  /**
    Check that the use_count of all SEL_ARGs in the SEL_TREE are
    correct.

    @param   tree   The SEL_TREE to check
  */
  void check_use_count(SEL_TREE *tree);
  /**
    Verify that a SEL_TREE has the type and conditions we expect it to
    have.

    @param   tree            The SEL_TREE to check
    @param   expected_type   The type 'tree' is expected to have
    @param   expected_result The range conditions 'tree' is expected
                             to consist of. The format of this string
                             is what opt_range.cc print_tree() produces.
  */
  void check_tree_result(SEL_TREE *tree,
                         const SEL_TREE::Type expected_type,
                         const char* expected_result);
  /**
    Perform OR between two SEL_TREEs and verify that the result of the
    OR operation is as expected.

    @param   tree1           First SEL_TREE that will be ORed
    @param   tree2           Second SEL_TREE that will be ORed
    @param   expected_type   The type the ORed result is expected to have
    @param   expected_result The range conditions the ORed result is expected
                             to consist of. The format of this string
                             is what opt_range.cc print_tree() produces.

    @return SEL_TREE result of the OR operation between tree1 and
            tree2 that has been verified to have expected range
            conditions.
  */
// Undefined at end of this file
#define create_and_check_tree_or(t1, t2, et, er) \
  do_create_and_check_tree_or(t1, t2, et, er, TestFailLinePrinter(__LINE__))
  SEL_TREE *do_create_and_check_tree_or(SEL_TREE *tree1, SEL_TREE *tree2,
                                        const SEL_TREE::Type expected_type,
                                        const char* expected_result,
                                        TestFailLinePrinter called_from_line);
  /**
    Perform AND between two SEL_TREEs and verify that the result of the
    AND operation is as expected.

    @param   tree1           First SEL_TREE that will be ANDed
    @param   tree2           Second SEL_TREE that will be ANDed
    @param   expected_type   The type the ANDed result is expected to have
    @param   expected_result The range conditions the ANDed result is expected
                             to consist of. The format of this string
                             is what opt_range.cc print_tree() produces.

    @return SEL_TREE result of the AND operation between tree1 and
            tree2 that has been verified to have expected range
            conditions.
  */
// Undefined at end of this file
#define create_and_check_tree_and(t1, t2, et, er) \
  do_create_and_check_tree_and(t1, t2, et, er, TestFailLinePrinter(__LINE__))
  SEL_TREE *do_create_and_check_tree_and(SEL_TREE *tree1, SEL_TREE *tree2,
                                         const SEL_TREE::Type expected_type,
                                         const char* expected_result,
                                         TestFailLinePrinter called_from_line);

  Server_initializer initializer;
  MEM_ROOT           m_alloc;

  std::vector<std::string> m_field_names;
  List<Field> m_field_list;
  Fake_TABLE *m_ftable;
  Fake_RANGE_OPT_PARAM *m_opt_param;
  Mock_HANDLER *m_mock_handler;
  std::vector<Mock_field_long*> m_table_fields;

private:
  /**
    Create a table with the fields in field_names_arg. After calling
    this function, SelArgTest::m_table_fields[i] 
    (i < field_names_arg->size()) stores a Mock_field_long.

    @param  field_names_arg  Fields in the table.
  */
  void create_table(std::vector<std::string> *field_names_arg);
};

Item_func*
SelArgTest::create_item(pred_type_enum type, Mock_field_long *fld, int value)
{
  Item_func *result;
  switch (type)
  {
  case GREATER:
    result= new Item_func_gt(new Item_field(fld), new Item_int(value));
    break;
  case LESS:
    result= new Item_func_lt(new Item_field(fld), new Item_int(value));
    break;
  case EQUAL:
    result= new Item_equal(new Item_int(value), new Item_field(fld));
    break;
  case XOR:
    result= new Item_func_xor(new Item_field(fld), new Item_int(value));
    break;
  default:
    result= NULL;
    DBUG_ASSERT(false);
    return result;
  }
  Item *itm= static_cast<Item*>(result);
  result->fix_fields(thd(), &itm);
  return result;
}

Item_func_xor*
SelArgTest::create_xor_item(Item *item1, Item *item2)
{
  Item_func_xor *xor_item= new Item_func_xor(item1, item2);
  Item *itm= static_cast<Item*>(xor_item);
  xor_item->fix_fields(thd(), &itm);
  return xor_item;
}

void SelArgTest::check_use_count(SEL_TREE *tree)
{
  for (uint i= 0; i < m_opt_param->keys; i++)
  {
    SEL_ARG *cur_range= tree->keys[i];
    if (cur_range != NULL)
      EXPECT_FALSE(cur_range->test_use_count(cur_range));
  }
}


void SelArgTest::check_tree_result(SEL_TREE *tree,
                                   const SEL_TREE::Type expected_type,
                                   const char* expected_result)
{
  EXPECT_EQ(expected_type, tree->type);
  if (expected_type != SEL_TREE::KEY)
    return;

  char buff[512];
  String actual_result(buff, sizeof(buff), system_charset_info);
  actual_result.set_charset(system_charset_info);
  actual_result.length(0);
  print_tree(&actual_result, "result", tree, m_opt_param);
  EXPECT_STREQ(expected_result, actual_result.c_ptr());
  SCOPED_TRACE("check_use_count");
  check_use_count(tree);
}


SEL_TREE *
SelArgTest::do_create_and_check_tree_or(SEL_TREE *tree1, SEL_TREE *tree2,
                                        const SEL_TREE::Type expected_type,
                                        const char* expected_result,
                                        TestFailLinePrinter called_from_line)
{
  {
    // Check that tree use counts are OK before OR'ing
    SCOPED_TRACE(called_from_line);
    check_use_count(tree1);
    check_use_count(tree2);
  }

  SEL_TREE *result= tree_or(m_opt_param, tree1, tree2);

  // Tree returned from tree_or()
  SCOPED_TRACE(called_from_line);
  check_tree_result(result, expected_type, expected_result);

  return result;
}


SEL_TREE *
SelArgTest::do_create_and_check_tree_and(SEL_TREE *tree1, SEL_TREE *tree2,
                                         const SEL_TREE::Type expected_type,
                                         const char* expected_result,
                                         TestFailLinePrinter called_from_line)
{
  {
    // Check that tree use counts are OK before AND'ing
    SCOPED_TRACE(called_from_line);
    check_use_count(tree1);
    check_use_count(tree2);
  }

  SEL_TREE *result= tree_and(m_opt_param, tree1, tree2);

  // Tree returned from tree_and()
  SCOPED_TRACE(called_from_line);
  check_tree_result(result, expected_type, expected_result);

  return result;
}


void SelArgTest::create_table(std::vector<std::string> *field_names_arg)
{
  std::vector<std::string>::iterator fld_name_it= field_names_arg->begin();
  for ( ; fld_name_it != field_names_arg->end(); ++fld_name_it)
    m_field_list.push_back(new Mock_field_long(thd(),
                                               NULL,
                                               (*fld_name_it).c_str(),
                                               false)
                           );

  m_ftable= new Fake_TABLE(m_field_list);
  /*
    const_table must be false to avoid that the range optimizer
    evaluates predicates
  */
  m_ftable->const_table= false;
  m_opt_param= new Fake_RANGE_OPT_PARAM(thd(), &m_alloc, m_ftable);
  handlerton *hton= NULL;
  m_mock_handler=
    new NiceMock<Mock_HANDLER>(hton, m_ftable->get_share());
  m_ftable->set_handler(m_mock_handler);

  List_iterator<Field> it(m_field_list);
  for (Field *cur_field= it++; cur_field; cur_field= it++)
  {
    bitmap_set_bit(m_ftable->read_set, cur_field->field_index);
    m_table_fields.push_back(static_cast<Mock_field_long*>(cur_field));
  }

  ON_CALL(*m_mock_handler, index_flags(_, _, true))
    .WillByDefault(Return(HA_READ_RANGE));
}


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
  SEL_TREE *tree= get_mm_tree(&opt_param, create_item(EQUAL, &field_long, 42));
  EXPECT_EQ(null_tree, tree);
}


/*
  Exercise range optimizer with xor operator.
*/
TEST_F(SelArgTest, XorCondIndexes)
{
  create_table(1);

  Mock_field_long *field_long= m_table_fields[0];
  m_opt_param->add_key(field_long);
  /*
    XOR is not range optimizible ATM and is treated as
    always true. No SEL_TREE is therefore expected.
  */
  SEL_TREE *tree= get_mm_tree(m_opt_param, create_item(XOR, field_long,  42));
  EXPECT_EQ(null_tree, tree);
}


/*
Exercise range optimizer with xor and different type of operator.
*/
TEST_F(SelArgTest, XorCondWithIndexes)
{
  create_table(5);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];
  Mock_field_long *field_long3= m_table_fields[2];
  Mock_field_long *field_long4= m_table_fields[3];
  Mock_field_long *field_long5= m_table_fields[4];
  m_opt_param->add_key(field_long1);
  m_opt_param->add_key(field_long2);
  m_opt_param->add_key(field_long3);
  m_opt_param->add_key(field_long4);
  m_opt_param->add_key(field_long5);

  /*
    Create SEL_TREE from "field1=7 AND (field1 XOR 42)". Since XOR is not range
    optimizible (treated as always true), we get a tree for "field1=7" only.
  */
  const char expected1[]= "result keys[0]: (7 <= field_1 <= 7)\n";

  SEL_TREE *tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(create_item(XOR, field_long1,  42),
                                      create_item(EQUAL, field_long1, 7)));
  SCOPED_TRACE("");
  check_tree_result(tree, SEL_TREE::KEY, expected1);

  /*
    Create SEL_TREE from "(field1 XOR 0) AND (field1>14)". Since XOR is not range
    optimizible (treated as always true), we get a tree for "field1>14" only.
  */
  const char expected2[]= "result keys[0]: (14 < field_1)\n";

  tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(create_item(XOR, field_long1,  0),
                                      create_item(GREATER, field_long1, 14)));
  SCOPED_TRACE("");
  check_tree_result(tree, SEL_TREE::KEY, expected2);

  /*
    Create SEL_TREE from "(field1<0 AND field1>14) XOR (field1>17)". Since
    XOR is not range optimizible (treated as always true), we get a NULL tree.
  */
  tree= get_mm_tree(m_opt_param,
                    create_xor_item(
                      new Item_cond_and(create_item(LESS, field_long1, 0),
                                        create_item(GREATER, field_long1, 14)),
                      create_item(GREATER, field_long1, 17)));
  SCOPED_TRACE("");
  EXPECT_EQ(null_tree, tree);

  /*
    Create SEL_TREE from
    (field1<0 AND field2>14) AND
    ((field3<0 and field4>14) XOR field5>17) ".
    Since XOR is not range  optimizible (treated as always true),
    we get a tree for "field1<0 AND field2>14" only.
  */
  const char expected3[]=
    "result keys[0]: (field_1 < 0)\n"
    "result keys[1]: (14 < field_2)\n";

  tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(
                      new Item_cond_and(create_item(LESS, field_long1, 0),
                                        create_item(GREATER, field_long2, 14)),
                      create_xor_item(
                        new Item_cond_and(create_item(LESS, field_long3, 0),
                                          create_item(GREATER,
                                                      field_long4, 14)),
                        create_item(GREATER, field_long5, 17))));
  SCOPED_TRACE("");
  check_tree_result(tree, SEL_TREE::KEY, expected3);
}
/*
  Exercise range optimizer with single column index
*/
TEST_F(SelArgTest, GetMMTreeSingleColIndex)
{
  // Create a single-column table with index
  create_table_singlecol_idx(1);

  Mock_field_long *field_long= m_table_fields[0];

  // Expected result of next test:
  const char expected[]= "result keys[0]: (42 <= field_1 <= 42)\n";
  create_tree(EQUAL, field_long, 42, 0, expected);

  // Expected result of next test:
  const char expected2[]=
    "result keys[0]: (42 <= field_1 <= 42) OR (43 <= field_1 <= 43)\n";
  SEL_TREE *tree=
    get_mm_tree(m_opt_param,
                new Item_cond_or(create_item(EQUAL, field_long, 42),
                                 create_item(EQUAL, field_long, 43)));

  SCOPED_TRACE("");
  check_tree_result(tree, SEL_TREE::KEY, expected2);

  // Expected result of next test:
  const char expected3[]=
    "result keys[0]: "
    "(1 <= field_1 <= 1) OR (2 <= field_1 <= 2) OR "
    "(3 <= field_1 <= 3) OR (4 <= field_1 <= 4) OR "
    "(5 <= field_1 <= 5) OR (6 <= field_1 <= 6) OR "
    "(7 <= field_1 <= 7) OR (8 <= field_1 <= 8)\n";
  List<Item> or_list1;
  or_list1.push_back(create_item(EQUAL, field_long, 1));
  or_list1.push_back(create_item(EQUAL, field_long, 2));
  or_list1.push_back(create_item(EQUAL, field_long, 3));
  or_list1.push_back(create_item(EQUAL, field_long, 4));
  or_list1.push_back(create_item(EQUAL, field_long, 5));
  or_list1.push_back(create_item(EQUAL, field_long, 6));
  or_list1.push_back(create_item(EQUAL, field_long, 7));
  or_list1.push_back(create_item(EQUAL, field_long, 8));

  tree= get_mm_tree(m_opt_param, new Item_cond_or(or_list1));
  check_tree_result(tree, SEL_TREE::KEY, expected3);

  // Expected result of next test:
  const char expected4[]= "result keys[0]: (7 <= field_1 <= 7)\n";
  tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(new Item_cond_or(or_list1),
                                      create_item(EQUAL, field_long, 7)));
  SCOPED_TRACE("");
  check_tree_result(tree, SEL_TREE::KEY, expected4);

  // Expected result of next test:
  const char expected5[]=
    "result keys[0]: "
    "(1 <= field_1 <= 1) OR (3 <= field_1 <= 3) OR "
    "(5 <= field_1 <= 5) OR (7 <= field_1 <= 7)\n";
  List<Item> or_list2;
  or_list2.push_back(create_item(EQUAL, field_long, 1));
  or_list2.push_back(create_item(EQUAL, field_long, 3));
  or_list2.push_back(create_item(EQUAL, field_long, 5));
  or_list2.push_back(create_item(EQUAL, field_long, 7));
  or_list2.push_back(create_item(EQUAL, field_long, 9));

  tree= get_mm_tree(m_opt_param,
                    new Item_cond_and(new Item_cond_or(or_list1),
                                      new Item_cond_or(or_list2)));
  SCOPED_TRACE("");
  check_tree_result(tree, SEL_TREE::KEY, expected5);
}


/*
  Exercise range optimizer with multiple column index
*/
TEST_F(SelArgTest, GetMMTreeMultipleSingleColIndex)
{
  // Create a single-column table without index
  create_table(1);

  Mock_field_long *field_long= m_table_fields[0];

  // Add two indexes covering the same field
  m_opt_param->add_key(field_long);
  m_opt_param->add_key(field_long);

  char buff[512];
  String range_string(buff, sizeof(buff), system_charset_info);
  range_string.set_charset(system_charset_info);

  // Expected result of next test:
  const char expected[]= 
    "result keys[0]: (42 <= field_1 <= 42)\n"
    "result keys[1]: (42 <= field_1 <= 42)\n";
  create_tree(EQUAL, field_long, 42, 0, expected);
}


/*
  Exercise range optimizer with multiple single column indexes
*/
TEST_F(SelArgTest, GetMMTreeOneTwoColIndex)
{
  create_table(2);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];

  m_opt_param->add_key(field_long1, field_long2);

  char buff[512];
  String range_string(buff, sizeof(buff), system_charset_info);
  range_string.set_charset(system_charset_info);

  // Expected result of next test:
  const char expected[]= "result keys[0]: (42 <= field_1 <= 42)\n";
  create_tree(EQUAL, field_long1, 42, 0, expected);

  // Expected result of next test:
  const char expected2[]= 
    "result keys[0]: (42 <= field_1 <= 42 AND 10 <= field_2 <= 10)\n";
  SEL_TREE *tree=
    get_mm_tree(m_opt_param,
                new Item_cond_and(create_item(EQUAL, field_long1, 42),
                                  create_item(EQUAL, field_long2, 10))
                );

  range_string.length(0);
  print_tree(&range_string, "result",tree , m_opt_param);
  EXPECT_STREQ(expected2, range_string.c_ptr());
}


/*
  Exercise range optimizer with three single column indexes
*/
TEST_F(SelArgTest, treeAndSingleColIndex1)
{
  create_table_singlecol_idx(3);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];
  Mock_field_long *field_long3= m_table_fields[2];

  // Expected outputs
  // Single-field range predicates
  const char expected_fld1[]=   "result keys[0]: (10 < field_1 < 13)\n";
  const char expected_fld2_1[]= "result keys[1]: (field_2 < 11)\n";
  const char expected_fld2_2[]= "result keys[1]: (8 < field_2)\n";
  const char expected_fld3[]=   "result keys[2]: (20 < field_3 < 30)\n";

  /*
    Expected result when performing AND of:
      "(field_1 BETWEEN 10 AND 13) & (field_2 < 11)"
  */
  const char expected_and1[]= 
    "result keys[0]: (10 < field_1 < 13)\n"
    "result keys[1]: (field_2 < 11)\n";

  /*
    Expected result when performing AND of:
      "((field_1 BETWEEN 10 AND 13) & (field_2 < 11))
       & 
       (field_3 BETWEEN 20 AND 30)"
  */
  const char expected_and2[]= 
    "result keys[0]: (10 < field_1 < 13)\n"
    "result keys[1]: (field_2 < 11)\n"
    "result keys[2]: (20 < field_3 < 30)\n";

  /*
    Expected result when performing AND of:
      "((field_1 BETWEEN 10 AND 13) &
        (field_2 < 11) & 
        (field_3 BETWEEN 20 AND 30)
       )
       &
       field_2 > 8"
  */
  const char expected_and3[]= 
    "result keys[0]: (10 < field_1 < 13)\n"
    "result keys[1]: (8 < field_2 < 11)\n"        // <- notice lower bound
    "result keys[2]: (20 < field_3 < 30)\n";

  SEL_TREE *tree_and=
    create_and_check_tree_and(
      create_and_check_tree_and(
        create_tree(BETWEEN, field_long1, 10, 13, expected_fld1),
        create_tree(LESS, field_long2, 11, 0, expected_fld2_1),
         SEL_TREE::KEY, expected_and1),
      create_tree(BETWEEN, field_long3, 20, 30, expected_fld3),
       SEL_TREE::KEY, expected_and2
  );

  /*
    Testing Axiom 7: AND'ing a predicate already part of a SEL_TREE
    has no effect.
  */
  create_and_check_tree_and(
    tree_and,
    create_tree(BETWEEN, field_long3, 20, 30, expected_fld3),
     SEL_TREE::KEY, expected_and2 // conditions did not change
  );

  create_and_check_tree_and(
    tree_and,
    create_tree(GREATER, field_long2, 8, 0, expected_fld2_2),
     SEL_TREE::KEY, expected_and3
  );

}


/*
  Exercise range optimizer with three single column indexes
*/
TEST_F(SelArgTest, treeOrSingleColIndex1)
{
  create_table_singlecol_idx(3);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];
  Mock_field_long *field_long3= m_table_fields[2];

  // Expected outputs
  // Single-field range predicates
  const char expected_fld1[]=   "result keys[0]: (10 < field_1 < 13)\n";
  const char expected_fld2_1[]= "result keys[1]: (field_2 < 11)\n";
  const char expected_fld2_2[]= "result keys[1]: (8 < field_2)\n";
  const char expected_fld3[]=   "result keys[2]: (20 < field_3 < 30)\n";

  /*
    Expected result when performing OR of:
      "(field_1 BETWEEN 10 AND 13) | (field_2 < 11)"
  */
  const char expected_or1[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 < field_1 < 13)\n"
    "  merge_tree keys[1]: (field_2 < 11)\n";

  /*
    Expected result when performing OR of:
      "((field_1 BETWEEN 10 AND 13) | (field_2 < 11))
       |
       (field_3 BETWEEN 20 AND 30)"
  */
  const char expected_or2[]= 
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 < field_1 < 13)\n"
    "  merge_tree keys[1]: (field_2 < 11)\n"
    "  merge_tree keys[2]: (20 < field_3 < 30)\n";

  SEL_TREE *tree_or2=
    create_and_check_tree_or(
      create_and_check_tree_or(
        create_tree(BETWEEN, field_long1, 10, 13, expected_fld1),
        create_tree(LESS, field_long2, 11, 0, expected_fld2_1),
         SEL_TREE::KEY, expected_or1),
      create_tree(BETWEEN, field_long3, 20, 30, expected_fld3),
       SEL_TREE::KEY, expected_or2
  );

  /*
    Testing Axiom 6: OR'ing a predicate already part of a SEL_TREE
    has no effect.
  */
  SEL_TREE *tree_or3=
    create_and_check_tree_or(
      tree_or2,
      create_tree(BETWEEN, field_long3, 20, 30, expected_fld3),
       SEL_TREE::KEY, expected_or2
  );

  /*
    Perform OR of:
      "((field_1 BETWEEN 10 AND 13) |
        (field_2 < 11) |
        (field_3 BETWEEN 20 AND 30)
       ) |
       (field_2 > 8)"
    
    This is always TRUE due to 
       (field_2 < 11) | (field_2 > 8)  <==> true
  */
  create_and_check_tree_or(
    tree_or3,
    create_tree(GREATER, field_long2, 8, 0, expected_fld2_2),
     SEL_TREE::ALWAYS, ""
  );
}

/*
  Exercise range optimizer with three single column indexes
*/
TEST_F(SelArgTest, treeAndOrComboSingleColIndex1)
{
  create_table_singlecol_idx(3);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];
  Mock_field_long *field_long3= m_table_fields[2];

  // Expected outputs
  // Single-field range predicates
  const char exected_fld1[]= "result keys[0]: (10 < field_1 < 13)\n";
  const char exected_fld2[]= "result keys[1]: (field_2 < 11)\n";
  const char exected_fld3[]= "result keys[2]: (20 < field_3 < 30)\n";

  // What "exected_fld1 & exected_fld2" should produce
  const char expected_and[]=
    "result keys[0]: (10 < field_1 < 13)\n"
    "result keys[1]: (field_2 < 11)\n";

  /*
    What "(exected_fld1 & exected_fld2) | exected_fld3" should
    produce.

    By Axiom 4 (see above), we have that
       X | (Y & Z)  <==>  (X | Y) & (X | Z)

    Thus:

       ((field_1 BETWEEN 10 AND 13) & field_2 < 11) |
       (field_3 BETWEEN 20 AND 30)

         <==> (Axiom 4)

       (field_1 BETWEEN ... | field_3 BETWEEN ...) &
       (field_2 < ...       | field_3 BETWEEN ...)

    But the result above is not created. Instead the following, which
    is incorrect (reads more rows than necessary), is the result:

       (field_1 BETWEEN ... | field_2 < 11 | field_3 BETWEEN ...)
  */
  const char expected_incorrect_or[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 < field_1 < 13)\n"
    "  merge_tree keys[1]: (field_2 < 11)\n"
    "  merge_tree keys[2]: (20 < field_3 < 30)\n";

  create_and_check_tree_or(
    create_and_check_tree_and(
      create_tree(BETWEEN, field_long1, 10, 13, exected_fld1),
      create_tree(LESS, field_long2, 11, 0, exected_fld2),
       SEL_TREE::KEY, expected_and
    ),
    create_tree(BETWEEN, field_long3, 20, 30, exected_fld3),
     SEL_TREE::KEY, expected_incorrect_or
  );
}

/**
  Test for BUG#16164031
*/
TEST_F(SelArgTest, treeAndOrComboSingleColIndex2)
{
  create_table_singlecol_idx(3);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];
  Mock_field_long *field_long3= m_table_fields[2];

  // Single-index predicates
  const char exp_f2_eq1[]=  "result keys[1]: (1 <= field_2 <= 1)\n";
  const char exp_f2_eq2[]=  "result keys[1]: (2 <= field_2 <= 2)\n";
  const char exp_f3_eq[]=   "result keys[2]: (1 <= field_3 <= 1)\n";
  const char exp_f1_lt1[]=  "result keys[0]: (field_1 < 256)\n";

  // OR1: Result of OR'ing f2_eq with f3_eq
  const char exp_or1[]= 
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[1]: (1 <= field_2 <= 1)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n";

  // OR2: Result of OR'ing f1_lt with f2_eq
  const char exp_or2[]= 
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (field_1 < 256)\n"
    "  merge_tree keys[1]: (2 <= field_2 <= 2)\n";

  // AND1: Result of "OR1 & OR2"
  const char exp_and1[]= 
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[1]: (1 <= field_2 <= 1)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n\n"
    "--- alternative 2 ---\n"
    "  merge_tree keys[0]: (field_1 < 256)\n"
    "  merge_tree keys[1]: (2 <= field_2 <= 2)\n";

  SEL_TREE *tree_and1=
    create_and_check_tree_and(
      create_and_check_tree_or(
        create_tree(EQUAL, field_long2, 1, 0, exp_f2_eq1),
        create_tree(EQUAL, field_long3, 1, 0, exp_f3_eq),
        SEL_TREE::KEY, exp_or1),
      create_and_check_tree_or(
        create_tree(LESS, field_long1, 256, 0, exp_f1_lt1),
        create_tree(EQUAL, field_long2, 2, 0, exp_f2_eq2),
        SEL_TREE::KEY, exp_or2),
      SEL_TREE::KEY, exp_and1
    );

  // OR3: Result of "AND1 | field3 = 1"
  const char exp_or3[]= 
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[1]: (1 <= field_2 <= 1)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n\n"
    "--- alternative 2 ---\n"
    "  merge_tree keys[0]: (field_1 < 256)\n"
    "  merge_tree keys[1]: (2 <= field_2 <= 2)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n";
  
  SEL_TREE *tree_or3=
    create_and_check_tree_or(
      tree_and1,
      create_tree(EQUAL, field_long3, 1, 0, exp_f3_eq),
      SEL_TREE::KEY, exp_or3
    );

  // More single-index predicates
  const char exp_f1_lt2[]= "result keys[0]: (field_1 < 35)\n";
  const char exp_f1_gt2[]= "result keys[0]: (257 < field_1)\n";
  const char exp_f1_or[]=  "result keys[0]: (field_1 < 35) OR (257 < field_1)\n";

  // OR4: Result of "OR3 | exp_f1_or"
  const char exp_or4[]= 
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[1]: (1 <= field_2 <= 1)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n"
    "  merge_tree keys[0]: (field_1 < 35) OR (257 < field_1)\n\n"
    "--- alternative 2 ---\n"
    "  merge_tree keys[0]: (field_1 < 256) OR (257 < field_1)\n"
    "  merge_tree keys[1]: (2 <= field_2 <= 2)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n";

  SEL_TREE *tree_or4=
    create_and_check_tree_or(
      tree_or3,
      create_and_check_tree_or(
        create_tree(LESS, field_long1, 35, 0, exp_f1_lt2),
        create_tree(GREATER, field_long1, 257, 0, exp_f1_gt2),
        SEL_TREE::KEY, exp_f1_or
      ),
      SEL_TREE::KEY, exp_or4
    );

  // More single-index predicates
  const char exp_f1_neq[]= "result keys[0]: (field_1 < 255) OR (255 < field_1)\n";
  const char exp_f2_eq3[]= "result keys[1]: (3 <= field_2 <= 3)\n";
  
  // AND2: Result of ANDing these two ^
  const char exp_and2[]=
    "result keys[0]: (field_1 < 255) OR (255 < field_1)\n"
    "result keys[1]: (3 <= field_2 <= 3)\n";

  // OR5: Result of "OR4 | AND3"
  /*
    "(field_1 < 255) OR (255 < field_1)" is lost when performing this
    OR. This results in a bigger set than correct boolean algebra
    rules dictate. @See note about relaxed boolean algebra in
    get_mm_tree().
  */
  const char exp_or5[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[1]: (1 <= field_2 <= 1) OR (3 <= field_2 <= 3)\n"
    "  merge_tree keys[2]: (1 <= field_3 <= 1)\n"
    "  merge_tree keys[0]: (field_1 < 35) OR (257 < field_1)\n";

  create_and_check_tree_or(
    tree_or4,
    create_and_check_tree_and(
      create_tree(NOT_EQUAL, field_long1, 255, 0, exp_f1_neq),
      create_tree(EQUAL, field_long2, 3, 0, exp_f2_eq3),
      SEL_TREE::KEY, exp_and2),
    SEL_TREE::KEY, exp_or5
  );
}


/**
  Test for BUG#16241773
*/
TEST_F(SelArgTest, treeAndOrComboSingleColIndex3)
{
  create_table_singlecol_idx(2);

  Mock_field_long *field_long1= m_table_fields[0];
  Mock_field_long *field_long2= m_table_fields[1];

  // Single-index predicates
  const char exp_f1_eq10[]=  "result keys[0]: (10 <= field_1 <= 10)\n";
  const char exp_f2_gtr20[]= "result keys[1]: (20 < field_2)\n";

  const char exp_f1_eq11[]=  "result keys[0]: (11 <= field_1 <= 11)\n";
  const char exp_f2_gtr10[]= "result keys[1]: (10 < field_2)\n";

  // OR1: Result of ORing f1_eq10 and f2_gtr20
  const char exp_or1[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 <= field_1 <= 10)\n"
    "  merge_tree keys[1]: (20 < field_2)\n";

  // OR2: Result of ORing f1_eq11 and f2_gtr10
  const char exp_or2[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (11 <= field_1 <= 11)\n"
    "  merge_tree keys[1]: (10 < field_2)\n";

  // AND1: Result of ANDing OR1 and OR2
  const char exp_and1[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 <= field_1 <= 10)\n"
    "  merge_tree keys[1]: (20 < field_2)\n\n"
    "--- alternative 2 ---\n"
    "  merge_tree keys[0]: (11 <= field_1 <= 11)\n"
    "  merge_tree keys[1]: (10 < field_2)\n";

  SEL_TREE *tree_and1=
    create_and_check_tree_and(
      create_and_check_tree_or(
        create_tree(EQUAL, field_long1, 10, 0, exp_f1_eq10),
        create_tree(GREATER, field_long2, 20, 0, exp_f2_gtr20),
        SEL_TREE::KEY, exp_or1),
      create_and_check_tree_or(
        create_tree(EQUAL, field_long1, 11, 0, exp_f1_eq11),
        create_tree(GREATER, field_long2, 10, 0, exp_f2_gtr10),
        SEL_TREE::KEY, exp_or2),
      SEL_TREE::KEY, exp_and1
    );

  const char exp_f2_eq5[]= "result keys[1]: (5 <= field_2 <= 5)\n";
  // OR3: Result of OR'ing AND1 with f2_eq5
  const char exp_or3[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 <= field_1 <= 10)\n"
    "  merge_tree keys[1]: (5 <= field_2 <= 5) OR (20 < field_2)\n\n"
    "--- alternative 2 ---\n"
    "  merge_tree keys[0]: (11 <= field_1 <= 11)\n"
    "  merge_tree keys[1]: (5 <= field_2 <= 5) OR (10 < field_2)\n";
  SEL_TREE *tree_or3=
    create_and_check_tree_or(
      tree_and1,
      create_tree(EQUAL, field_long2, 5, 0, exp_f2_eq5),
      SEL_TREE::KEY, exp_or3
    );

  const char exp_f2_lt2[]= "result keys[1]: (field_2 < 2)\n";
  // OR4: Result of OR'ing OR3 with f2_lt2
  const char exp_or4[]=
    "result contains the following merges\n"
    "--- alternative 1 ---\n"
    "  merge_tree keys[0]: (10 <= field_1 <= 10)\n"
    "  merge_tree keys[1]: (field_2 < 2) OR "
                          "(5 <= field_2 <= 5) OR (20 < field_2)\n\n"
    "--- alternative 2 ---\n"
    "  merge_tree keys[0]: (11 <= field_1 <= 11)\n"
    "  merge_tree keys[1]: (field_2 < 2) OR "
                          "(5 <= field_2 <= 5) OR (10 < field_2)\n";

  create_and_check_tree_or(
    tree_or3,
    create_tree(LESS, field_long2, 2, 0, exp_f2_lt2),
    SEL_TREE::KEY, exp_or4
  );
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

#undef create_tree
#undef create_and_check_tree_and
#undef create_and_check_tree_or
