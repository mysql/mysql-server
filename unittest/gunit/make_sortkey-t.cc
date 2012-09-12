/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "sql_plugin.h"                         // SHOW_always_last
#include "test_utils.h"

#include "sys_vars.h"

#include "filesort.h"
#include "sql_sort.h"

namespace {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

/**
   Test that sortlength() and make_sortkey() agree on what to do:
   i.e. that there is no buffer underwrite/overwrite in make_sortkey()
   if sortlength() has set a very small size.

   We allocate a buffer, fill it with 'a's and then tell make_sortkey()
   to put it's result somewhere in the middle.
   The buffer should be unchanged outside of the area determined by sortlength.
 */
class MakeSortKeyTest : public ::testing::Test
{
protected:
  MakeSortKeyTest()
  {
    m_sort_fields[0].field= NULL;
    m_sort_fields[1].field= NULL;
    m_sort_fields[0].reverse= false;
    m_sort_fields[1].reverse= false;
    m_sort_param.local_sortorder= m_sort_fields;
    m_sort_param.end= m_sort_param.local_sortorder + 1;
    memset(m_buff, 'a', sizeof(m_buff));
    m_to= &m_buff[8];
  }

  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  void verify_buff(uint length)
  {
    for (uchar *pu= m_buff; pu < m_to; ++pu)
    {
      EXPECT_EQ('a', *pu) << " position " << pu - m_buff;
    }
    for (uchar *pu= m_to + length; pu < m_buff + 100; ++pu)
    {
      EXPECT_EQ('a', *pu) << " position " << pu - m_buff;
    }
  }

  Server_initializer initializer;

  Sort_param m_sort_param;
  SORT_FIELD m_sort_fields[2]; // sortlength() adds an end marker !!
  bool m_multi_byte_charset;
  uchar m_ref_buff[4];         // unused, but needed for make_sortkey()
  uchar m_buff[100];
  uchar *m_to;
};


TEST_F(MakeSortKeyTest, IntResult)
{
  thd()->variables.max_sort_length= 4U;
  m_sort_fields[0].item= new Item_int(42);

  const uint total_length=
    sortlength(thd(), m_sort_fields, 1, &m_multi_byte_charset);
  EXPECT_EQ(sizeof(longlong), total_length);
  EXPECT_FALSE(m_multi_byte_charset);
  EXPECT_EQ(sizeof(longlong), m_sort_fields[0].length);
  EXPECT_EQ(INT_RESULT, m_sort_fields[0].result_type);

  make_sortkey(&m_sort_param, m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}


TEST_F(MakeSortKeyTest, IntResultNull)
{
  thd()->variables.max_sort_length= 4U;
  Item *int_item= m_sort_fields[0].item= new Item_int(42);
  int_item->maybe_null= true;
  int_item->null_value= true;

  const uint total_length=
    sortlength(thd(), m_sort_fields, 1, &m_multi_byte_charset);
  EXPECT_EQ(1 + sizeof(longlong), total_length);
  EXPECT_FALSE(m_multi_byte_charset);
  EXPECT_EQ(sizeof(longlong), m_sort_fields[0].length);
  EXPECT_EQ(INT_RESULT, m_sort_fields[0].result_type);

  make_sortkey(&m_sort_param, m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

TEST_F(MakeSortKeyTest, DecimalResult)
{
  const char dec_str[]= "1234567890.1234567890";
  thd()->variables.max_sort_length= 4U;
  m_sort_fields[0].item=
    new Item_decimal(dec_str, strlen(dec_str), &my_charset_bin);

  const uint total_length=
    sortlength(thd(), m_sort_fields, 1, &m_multi_byte_charset);
  EXPECT_EQ(10U, total_length);
  EXPECT_FALSE(m_multi_byte_charset);
  EXPECT_EQ(10U, m_sort_fields[0].length);
  EXPECT_EQ(DECIMAL_RESULT, m_sort_fields[0].result_type);

  make_sortkey(&m_sort_param, m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

TEST_F(MakeSortKeyTest, RealResult)
{
  const char dbl_str[]= "1234567890.1234567890";
  thd()->variables.max_sort_length= 4U;
  m_sort_fields[0].item= new Item_float(dbl_str, strlen(dbl_str));

  const uint total_length=
    sortlength(thd(), m_sort_fields, 1, &m_multi_byte_charset);
  EXPECT_EQ(sizeof(double), total_length);
  EXPECT_FALSE(m_multi_byte_charset);
  EXPECT_EQ(sizeof(double), m_sort_fields[0].length);
  EXPECT_EQ(REAL_RESULT, m_sort_fields[0].result_type);

  make_sortkey(&m_sort_param, m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

}
