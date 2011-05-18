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

#include "item.h"
#include "sql_class.h"

namespace {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class ItemTest : public ::testing::Test
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

  virtual void SetUp()
  {
    initializer.SetUp();
  }

  virtual void TearDown()
  {
    initializer.TearDown();
  }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};


/**
  This is a simple mock Field class, which verifies that store() is called.
  TODO: Introduce Google Mock to simplify writing of mock classes.
*/
class Mock_field_long : public Field_long
{
public:
  Mock_field_long(uint32 lenght, longlong expected_value)
    : Field_long(0,                             // ptr_arg
                 lenght,                        // len_arg
                 NULL,                          // null_ptr_arg
                 0,                             // null_bit_arg
                 Field::NONE,                   // unireg_check_arg
                 0,                             // field_name_arg
                 false,                         // zero_arg
                 false),                        // unsigned_arg
      m_store_called(0),
      m_expected_value(expected_value)
  {}

  // The destructor verifies that store() has been called.
  virtual ~Mock_field_long()
  {
    EXPECT_EQ(1, m_store_called);
  }

  // Avoid warning about hiding other overloaded versions of store().
  using Field_long::store;

  /*
    This is the only member function we need to override.
    We expect it to be called with specific arguments.
   */
  virtual int store(longlong nr, bool unsigned_val)
  {
    EXPECT_EQ(m_expected_value, nr);
    EXPECT_FALSE(unsigned_val);
    ++m_store_called;
    return 0;
  }

private:
  int      m_store_called;
  longlong m_expected_value;
};


TEST_F(ItemTest, ItemInt)
{
  const int32 val= 42;
  char stringbuf[10];
  (void) my_snprintf(stringbuf, sizeof(stringbuf), "%d", val);

  // An Item expects to be owned by current_thd->free_list,
  // so allocate with new, and do not delete it.
  Item_int *item_int= new Item_int(val);

  EXPECT_EQ(Item::INT_ITEM,      item_int->type());
  EXPECT_EQ(INT_RESULT,          item_int->result_type());
  EXPECT_EQ(MYSQL_TYPE_LONGLONG, item_int->field_type());
  EXPECT_EQ(val,                 item_int->val_int());
  EXPECT_DOUBLE_EQ((double) val, item_int->val_real());
  EXPECT_TRUE(item_int->basic_const_item());

  my_decimal decimal_val;
  EXPECT_EQ(&decimal_val, item_int->val_decimal(&decimal_val));

  String string_val;
  EXPECT_EQ(&string_val, item_int->val_str(&string_val));
  EXPECT_STREQ(stringbuf, string_val.c_ptr_safe());

  {
    // New scope, since we have EXPECT_EQ in the destructor as well.
    Mock_field_long field_val(item_int->max_length, val);
    EXPECT_EQ(0, item_int->save_in_field(&field_val, true));
  }

  Item *clone= item_int->clone_item();
  EXPECT_TRUE(item_int->eq(clone, true));
  EXPECT_TRUE(item_int->eq(item_int, true));

  String print_val;
  item_int->print(&print_val, QT_ORDINARY);
  EXPECT_STREQ(stringbuf, print_val.c_ptr_safe());

  const uint precision= item_int->decimal_precision();
  EXPECT_EQ(MY_INT32_NUM_DECIMAL_DIGITS, precision);

  item_int->neg();
  EXPECT_EQ(-val, item_int->val_int());
  EXPECT_EQ(precision - 1, item_int->decimal_precision());

  // Functions inherited from parent class(es).
  const table_map tmap= 0;
  EXPECT_EQ(tmap, item_int->used_tables());

  /*
   TODO: There are about 100 member functions in Item.
         Figure out which ones are relevant for unit testing here.
  */
}


TEST_F(ItemTest, ItemFuncDesDecrypt)
{
  // Bug #59632 Assertion failed: arg_length > length
  const uint length= 1U;
  Item_int *item_one= new Item_int(1, length);
  Item_int *item_two= new Item_int(2, length);
  Item_func_des_decrypt *item_decrypt=
    new Item_func_des_decrypt(item_two, item_one);
  
  EXPECT_FALSE(item_decrypt->fix_fields(thd(), NULL));
  EXPECT_EQ(length, item_one->max_length);
  EXPECT_EQ(length, item_two->max_length);
  EXPECT_LE(item_decrypt->max_length, length);
}


TEST_F(ItemTest, ItemFuncIntDivOverflow)
{
  const char dividend_str[]=
    "99999999999999999999999999999999999999999"
    "99999999999999999999999999999999999999999";
  const char divisor_str[]= "0.5";
  Item_float *dividend= new Item_float(dividend_str, sizeof(dividend_str));
  Item_float *divisor= new Item_float(divisor_str, sizeof(divisor_str));
  Item_func_int_div* quotient= new Item_func_int_div(dividend, divisor);

  Mock_error_handler error_handler(thd(), ER_TRUNCATED_WRONG_VALUE);
  EXPECT_FALSE(quotient->fix_fields(thd(), NULL));
  initializer.set_expected_error(ER_DATA_OUT_OF_RANGE);
  quotient->val_int();
}


TEST_F(ItemTest, ItemFuncIntDivUnderflow)
{
  // Bug #11792200 - DIVIDING LARGE NUMBERS CAUSES STACK CORRUPTIONS
  const char dividend_str[]= "1.175494351E-37";
  const char divisor_str[]= "1.7976931348623157E+308";
  Item_float *dividend= new Item_float(dividend_str, sizeof(dividend_str));
  Item_float *divisor= new Item_float(divisor_str, sizeof(divisor_str));
  Item_func_int_div* quotient= new Item_func_int_div(dividend, divisor);

  Mock_error_handler error_handler(thd(), ER_TRUNCATED_WRONG_VALUE);
  EXPECT_FALSE(quotient->fix_fields(thd(), NULL));
  EXPECT_EQ(0, quotient->val_int());
}


/*
  This is not an exhaustive test. It simply demonstrates that more of the
  initializations in mysqld.cc are needed for testing Item_xxx classes.
*/
TEST_F(ItemTest, ItemFuncSetUserVar)
{
  const longlong val1= 1;
  Item_decimal *item_dec= new Item_decimal(val1, false);
  Item_string  *item_str= new Item_string("1", 1, &my_charset_latin1);

  LEX_STRING var_name= { C_STRING_WITH_LEN("a") };
  Item_func_set_user_var *user_var=
    new Item_func_set_user_var(var_name, item_str);
  EXPECT_FALSE(user_var->set_entry(thd(), true));
  EXPECT_FALSE(user_var->fix_fields(thd(), NULL));
  EXPECT_EQ(val1, user_var->val_int());
  
  my_decimal decimal;
  my_decimal *decval_1= user_var->val_decimal(&decimal);
  user_var->save_item_result(item_str);
  my_decimal *decval_2= user_var->val_decimal(&decimal);
  user_var->save_item_result(item_dec);

  EXPECT_EQ(decval_1, decval_2);
  EXPECT_EQ(decval_1, &decimal);
}


// Test of Item::operator new() when we simulate out-of-memory.
TEST_F(ItemTest, OutOfMemory)
{
  Item_int *null_item= NULL;
  Item_int *item= new Item_int(42);
  EXPECT_NE(null_item, item);
  delete null_item;

#if !defined(DBUG_OFF)
  // Setting debug flags triggers enter/exit trace, so redirect to /dev/null.
  DBUG_SET("o," IF_WIN("NUL", "/dev/null"));

  DBUG_SET("+d,simulate_out_of_memory");
  item= new Item_int(42);
  EXPECT_EQ(null_item, item);

  DBUG_SET("+d,simulate_out_of_memory");
  item= new (thd()->mem_root) Item_int(42);
  EXPECT_EQ(null_item, item);
#endif
}

TEST_F(ItemTest, ItemFuncXor)
{
  const uint length= 1U;
  Item_int *item_zero= new Item_int(0, length);
  Item_int *item_one_a= new Item_int(1, length);

  Item_func_xor *item_xor=
    new Item_func_xor(item_zero, item_one_a);

  EXPECT_FALSE(item_xor->fix_fields(thd(), NULL));
  EXPECT_EQ(1, item_xor->val_int());
  EXPECT_EQ(1U, item_xor->decimal_precision());

  Item_int *item_one_b= new Item_int(1, length);

  Item_func_xor *item_xor_same=
    new Item_func_xor(item_one_a, item_one_b);

  EXPECT_FALSE(item_xor_same->fix_fields(thd(), NULL));
  EXPECT_EQ(0, item_xor_same->val_int());
  EXPECT_FALSE(item_xor_same->val_bool());
  EXPECT_FALSE(item_xor_same->is_null());

  String print_buffer;
  item_xor->print(&print_buffer, QT_ORDINARY);
  EXPECT_STREQ("(0 xor 1)", print_buffer.c_ptr_safe());

  Item *neg_xor= item_xor->neg_transformer(thd());
  EXPECT_FALSE(neg_xor->fix_fields(thd(), NULL));
  EXPECT_EQ(0, neg_xor->val_int());
  EXPECT_DOUBLE_EQ(0.0, neg_xor->val_real());
  EXPECT_FALSE(neg_xor->val_bool());
  EXPECT_FALSE(neg_xor->is_null());

  print_buffer= String();
  neg_xor->print(&print_buffer, QT_ORDINARY);
  EXPECT_STREQ("((not(0)) xor 1)", print_buffer.c_ptr_safe());

  Item_func_xor *item_xor_null=
    new Item_func_xor(item_zero, new Item_null());
  EXPECT_FALSE(item_xor_null->fix_fields(thd(), NULL));

  EXPECT_EQ(0, item_xor_null->val_int());
  EXPECT_TRUE(item_xor_null->is_null());
}

}
