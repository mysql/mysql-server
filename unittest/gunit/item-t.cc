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
#include <gmock/gmock.h>

#include "test_utils.h"

#include "item.h"
#include "sql_class.h"
#include "tztime.h"

#include "mock_field_timestamp.h"

namespace item_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;
using ::testing::Return;

class ItemTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};


/**
  This is a simple mock Field class, illustrating how to set expectations on
  type_conversion_status Field_long::store(longlong nr, bool unsigned_val);
*/
class Mock_field_long : public Field_long
{
public:
  Mock_field_long(uint32 lenght)
    : Field_long(0,                             // ptr_arg
                 lenght,                        // len_arg
                 NULL,                          // null_ptr_arg
                 0,                             // null_bit_arg
                 Field::NONE,                   // unireg_check_arg
                 0,                             // field_name_arg
                 false,                         // zero_arg
                 false)                         // unsigned_arg
  {}

  // Avoid warning about hiding other overloaded versions of store().
  using Field_long::store;

  /*
    This is the only member function we need to override.
    Note: Sun Studio needs a little help in resolving longlong.
   */
  MOCK_METHOD2(store, type_conversion_status(::longlong nr, bool unsigned_val));
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

  Mock_field_long field_val(item_int->max_length);
  // We expect to be called with arguments(nr == val, unsigned_val == false)
  EXPECT_CALL(field_val, store(val, false))
    .Times(1)
    .WillRepeatedly(Return(TYPE_OK));
  EXPECT_EQ(TYPE_OK, item_int->save_in_field(&field_val, true));

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


TEST_F(ItemTest, ItemEqual)
{
  // Bug#13720201 VALGRIND: VARIOUS BLOCKS OF BYTES DEFINITELY LOST
  Mock_field_timestamp mft;
  // foo is longer than STRING_BUFFER_USUAL_SIZE used by cmp_item_sort_string.
  const char foo[]=
    "0123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789";
  Item_equal *item_equal=
    new Item_equal(new Item_string(STRING_WITH_LEN(foo), &my_charset_bin),
                   new Item_field(&mft));
  EXPECT_FALSE(item_equal->fix_fields(thd(), NULL));
  EXPECT_EQ(0, item_equal->val_int());
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


TEST_F(ItemTest, ItemFuncExportSet)
{
  String str;
  Item *on_string= new Item_string(STRING_WITH_LEN("on"), &my_charset_bin);
  Item *off_string= new Item_string(STRING_WITH_LEN("off"), &my_charset_bin);
  Item *sep_string= new Item_string(STRING_WITH_LEN(","), &my_charset_bin);
  {
    // Testing basic functionality.
    Item_func_export_set *export_set=
      new Item_func_export_set(new Item_int(2),
                               on_string,
                               off_string,
                               sep_string,
                               new Item_int(4));
    EXPECT_FALSE(export_set->fix_fields(thd(), NULL));
    EXPECT_EQ(&str, export_set->val_str(&str));
    EXPECT_STREQ("off,on,off,off", str.c_ptr_safe());
  }
  {
    // Testing corner case: number_of_bits == zero.
    Item_func_export_set *export_set=
      new Item_func_export_set(new Item_int(2),
                               on_string,
                               off_string,
                               sep_string,
                               new Item_int(0));
    EXPECT_FALSE(export_set->fix_fields(thd(), NULL));
    EXPECT_EQ(&str, export_set->val_str(&str));
    EXPECT_STREQ("", str.c_ptr_safe());
  }

  /*
    Bug#11765562 58545:
    EXPORT_SET() CAN BE USED TO MAKE ENTIRE SERVER COMPLETELY UNRESPONSIVE
   */
  const ulong max_size= 1024;
  const ulonglong repeat= max_size / 2;
  Item *item_int_repeat= new Item_int(repeat);
  Item *string_x= new Item_string(STRING_WITH_LEN("x"), &my_charset_bin);
  String * const null_string= NULL;
  thd()->variables.max_allowed_packet= max_size;
  {
    // Testing overflow caused by 'on-string'.
    Mock_error_handler error_handler(thd(), ER_WARN_ALLOWED_PACKET_OVERFLOWED);
    Item_func_export_set *export_set=
      new Item_func_export_set(new Item_int(0xff),
                               new Item_func_repeat(string_x, item_int_repeat),
                               string_x,
                               sep_string);
    EXPECT_FALSE(export_set->fix_fields(thd(), NULL));
    EXPECT_EQ(null_string, export_set->val_str(&str));
    EXPECT_STREQ("", str.c_ptr_safe());
    EXPECT_EQ(1, error_handler.handle_called());
  }
  {
    // Testing overflow caused by 'off-string'.
    Mock_error_handler error_handler(thd(), ER_WARN_ALLOWED_PACKET_OVERFLOWED);
    Item_func_export_set *export_set=
      new Item_func_export_set(new Item_int(0xff),
                               string_x,
                               new Item_func_repeat(string_x, item_int_repeat),
                               sep_string);
    EXPECT_FALSE(export_set->fix_fields(thd(), NULL));
    EXPECT_EQ(null_string, export_set->val_str(&str));
    EXPECT_STREQ("", str.c_ptr_safe());
    EXPECT_EQ(1, error_handler.handle_called());
  }
  {
    // Testing overflow caused by 'separator-string'.
    Mock_error_handler error_handler(thd(), ER_WARN_ALLOWED_PACKET_OVERFLOWED);
    Item_func_export_set *export_set=
      new Item_func_export_set(new Item_int(0xff),
                               string_x,
                               string_x,
                               new Item_func_repeat(string_x, item_int_repeat));
    EXPECT_FALSE(export_set->fix_fields(thd(), NULL));
    EXPECT_EQ(null_string, export_set->val_str(&str));
    EXPECT_STREQ("", str.c_ptr_safe());
    EXPECT_EQ(1, error_handler.handle_called());
  }
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


TEST_F(ItemTest, ItemFuncNegLongLongMin)
{
  // Bug#14314156 MAIN.FUNC_MATH TEST FAILS ON MYSQL-TRUNK ON PB2
  const longlong longlong_min= LONGLONG_MIN;
  Item_func_neg *item_neg= new Item_func_neg(new Item_int(longlong_min));

  EXPECT_FALSE(item_neg->fix_fields(thd(), NULL));
  initializer.set_expected_error(ER_DATA_OUT_OF_RANGE);
  EXPECT_EQ(0, item_neg->int_op());
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
    new Item_func_set_user_var(var_name, item_str, false);
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


// We never use dynamic_cast, but we expect it to work.
TEST_F(ItemTest, DynamicCast)
{
  Item *item= new Item_int(42);
  const Item_int *null_item= NULL;
  EXPECT_NE(null_item, dynamic_cast<Item_int*>(item));
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


/*
  Testing MYSQL_TIME_cache.
*/
TEST_F(ItemTest, MysqlTimeCache)
{
  String str_buff, *str;
  MYSQL_TIME datetime6=
  { 2011, 11, 7, 10, 20, 30, 123456, 0, MYSQL_TIMESTAMP_DATETIME };
  MYSQL_TIME time6=
  { 0, 0, 0, 10, 20, 30, 123456, 0, MYSQL_TIMESTAMP_TIME };
  struct timeval tv6= {1320661230, 123456};
  const MYSQL_TIME *ltime;
  MYSQL_TIME_cache cache;

  /*
    Testing DATETIME(6).
    Initializing from MYSQL_TIME.
  */
  cache.set_datetime(&datetime6, 6);
  EXPECT_EQ(1840440237558456896LL, cache.val_packed());
  EXPECT_EQ(6, cache.decimals());
  // Call val_str() then cptr()
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("2011-11-07 10:20:30.123456", str->c_ptr_safe());
  EXPECT_STREQ("2011-11-07 10:20:30.123456", cache.cptr());
  cache.set_datetime(&datetime6, 6);
  // Now call the other way around: cptr() then val_str()
  EXPECT_STREQ("2011-11-07 10:20:30.123456", cache.cptr());
  EXPECT_STREQ("2011-11-07 10:20:30.123456", str->c_ptr_safe());
  // Testing get_TIME_ptr()
  ltime= cache.get_TIME_ptr();
  EXPECT_EQ(ltime->year, datetime6.year);
  EXPECT_EQ(ltime->month, datetime6.month);
  EXPECT_EQ(ltime->day, datetime6.day);
  EXPECT_EQ(ltime->hour, datetime6.hour);
  EXPECT_EQ(ltime->minute, datetime6.minute);
  EXPECT_EQ(ltime->second, datetime6.second);
  EXPECT_EQ(ltime->second_part, datetime6.second_part);
  EXPECT_EQ(ltime->neg, datetime6.neg);
  EXPECT_EQ(ltime->time_type, datetime6.time_type);
  // Testing eq()
  {
    MYSQL_TIME datetime6_2= datetime6;
    MYSQL_TIME_cache cache2;
    datetime6_2.second_part+= 1;
    cache2.set_datetime(&datetime6_2, 6);
    EXPECT_EQ(cache.eq(cache), true);
    EXPECT_EQ(cache.eq(cache2), false);
    EXPECT_EQ(cache2.eq(cache2), true);
    EXPECT_EQ(cache2.eq(cache), false);
  }

  /*
     Testing DATETIME(6).
     Initializing from "struct timeval".
  */
  cache.set_datetime(tv6, 6, my_tz_UTC);
  EXPECT_EQ(1840440237558456896LL, cache.val_packed());
  EXPECT_EQ(6, cache.decimals());
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("2011-11-07 10:20:30.123456", str->c_ptr_safe());
  EXPECT_STREQ("2011-11-07 10:20:30.123456", cache.cptr());

  /*
    Testing TIME(6).
    Initializing from MYSQL_TIME.
  */
  cache.set_time(&time6, 6);
  EXPECT_EQ(709173043776LL, cache.val_packed());
  EXPECT_EQ(6, cache.decimals());
  // Call val_str() then cptr()
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("10:20:30.123456", str->c_ptr_safe());
  EXPECT_STREQ("10:20:30.123456", cache.cptr());

  /*
    Testing TIME(6).
    Initializing from "struct timeval".
  */
  cache.set_time(tv6, 6, my_tz_UTC);
  EXPECT_EQ(709173043776LL, cache.val_packed());
  EXPECT_EQ(6, cache.decimals());
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("10:20:30.123456", str->c_ptr_safe());
  EXPECT_STREQ("10:20:30.123456", cache.cptr());

  /*
    Testing DATETIME(5)
  */
  MYSQL_TIME datetime5=
  { 2011, 11, 7, 10, 20, 30, 123450, 0, MYSQL_TIMESTAMP_DATETIME };
  cache.set_datetime(&datetime5, 5);
  EXPECT_EQ(1840440237558456890LL, cache.val_packed());
  EXPECT_EQ(5, cache.decimals());
  /* Call val_str() then cptr() */
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("2011-11-07 10:20:30.12345", str->c_ptr_safe());
  EXPECT_STREQ("2011-11-07 10:20:30.12345", cache.cptr());
  cache.set_datetime(&datetime5, 5);
  /* Now call the other way around: cptr() then val_str() */
  EXPECT_STREQ("2011-11-07 10:20:30.12345", cache.cptr());
  EXPECT_STREQ("2011-11-07 10:20:30.12345", str->c_ptr_safe());

  /*
    Testing DATE.
    Initializing from MYSQL_TIME.
  */
  MYSQL_TIME date=
  { 2011, 11, 7, 0, 0, 0, 0, 0, MYSQL_TIMESTAMP_DATE };
  cache.set_date(&date);
  EXPECT_EQ(1840439528385413120LL, cache.val_packed());
  EXPECT_EQ(0, cache.decimals());
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("2011-11-07", str->c_ptr_safe());
  EXPECT_STREQ("2011-11-07", cache.cptr());

  /*
    Testing DATE.
    Initializing from "struct tm".
  */
  cache.set_date(tv6, my_tz_UTC);
  EXPECT_EQ(1840439528385413120LL, cache.val_packed());
  EXPECT_EQ(0, cache.decimals());
  str= cache.val_str(&str_buff);
  EXPECT_STREQ("2011-11-07", str->c_ptr_safe());
  EXPECT_STREQ("2011-11-07", cache.cptr());
}

extern "C"
{
  // Verifies that Item_func_conv::val_str does not call my_strntoll()
  longlong fail_strntoll(const struct charset_info_st *, const char *s,
                         size_t l, int base, char **e, int *err)
  {
    ADD_FAILURE() << "Unexpected call";
    return 0;
  }
}

class Mock_charset : public CHARSET_INFO
{
public:
  Mock_charset(const CHARSET_INFO &csi)
  {
    CHARSET_INFO *this_as_cset= this;
    *this_as_cset= csi;

    number= 666;
    m_cset_handler= *(csi.cset);
    m_cset_handler.strntoll= fail_strntoll;
    cset= &m_cset_handler;
  }
private:
  MY_CHARSET_HANDLER m_cset_handler;
};

TEST_F(ItemTest, ItemFuncConvIntMin)
{
  Mock_charset charset(*system_charset_info);
  SCOPED_TRACE("");
  Item_func_conv *item_conv=
    new Item_func_conv(new Item_string("5", 1, &charset),
                       new Item_int(INT_MIN),   // from_base
                       new Item_int(INT_MIN));  // to_base
  EXPECT_FALSE(item_conv->fix_fields(thd(), NULL));
  const String *null_string= NULL;
  String str;
  EXPECT_EQ(null_string, item_conv->val_str(&str));
}

}
