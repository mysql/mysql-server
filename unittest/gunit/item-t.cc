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

#include "item.h"
#include "sql_class.h"
#include "rpl_handler.h"                        // delegates_init()

namespace {

class ItemTest : public ::testing::Test
{
protected:
  /*
    This is the part of the server global things which have to be initialized
    for this (very simple) unit test. Presumably the list will grow once
    we start writing tests for more advanced classes.
    TODO: Move to a common library.
   */
  static void SetUpTestCase()
  {
    init_thread_environment();
    randominit(&sql_rand, 0, 0);
    xid_cache_init();
    delegates_init();
  }

  static void TearDownTestCase()
  {
    delegates_destroy();
    xid_cache_free();
  }

  ItemTest() : m_thd(NULL) {}

  virtual void SetUp()
  {
    m_thd= new THD(false);
    m_thd->thread_stack= (char*) &m_thd;
    m_thd->store_globals();
  }

  virtual void TearDown()
  {
    m_thd->cleanup_after_query();
    delete m_thd;
  }

private:
  THD      *m_thd;
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

}
