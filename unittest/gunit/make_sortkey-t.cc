/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <sys/types.h>

#include "my_inttypes.h"
#include "sql/filesort.h"
#include "sql/sort_param.h"
#include "sql/sql_lex.h"
#include "sql/sql_sort.h"
#include "sql/sys_vars.h"
#include "unittest/gunit/test_utils.h"

namespace make_sortkey_unittest {

using my_testing::Mock_error_handler;
using my_testing::Server_initializer;

/**
   Test that sortlength() and make_sortkey() agree on what to do:
   i.e. that there is no buffer underwrite/overwrite in make_sortkey()
   if sortlength() has set a very small size.

   We allocate a buffer, fill it with 'a's and then tell make_sortkey()
   to put it's result somewhere in the middle.
   The buffer should be unchanged outside of the area determined by sortlength.
 */
class MakeSortKeyTest : public ::testing::Test {
 protected:
  MakeSortKeyTest() {
    m_sort_fields[0] = st_sort_field();
    m_sort_fields[1] = st_sort_field();
    m_sort_param.local_sortorder =
        Bounds_checked_array<st_sort_field>(m_sort_fields, 1);
    memset(m_buff, 'a', sizeof(m_buff));
    m_to = &m_buff[8];
  }

  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  void verify_buff(uint length) {
    for (uchar *pu = m_buff; pu < m_to; ++pu) {
      EXPECT_EQ('a', *pu) << " position " << pu - m_buff;
    }
    for (uchar *pu = m_to + length; pu < m_buff + 100; ++pu) {
      EXPECT_EQ('a', *pu) << " position " << pu - m_buff;
    }
  }

  Server_initializer initializer;

  Sort_param m_sort_param;
  st_sort_field m_sort_fields[2];  // sortlength() adds an end marker !!
  uchar m_ref_buff[4];             // unused, but needed for make_sortkey()
  uchar m_buff[100];
  uchar *m_to;
};

TEST_F(MakeSortKeyTest, IntResult) {
  thd()->variables.max_sort_length = 4U;
  m_sort_fields[0].item = new Item_int(42);

  const uint total_length = sortlength(thd(), m_sort_fields, 1);
  EXPECT_EQ(sizeof(longlong), total_length);
  EXPECT_EQ(sizeof(longlong), m_sort_fields[0].length);
  EXPECT_EQ(INT_RESULT, m_sort_fields[0].result_type);

  m_sort_param.make_sortkey(m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

TEST_F(MakeSortKeyTest, IntResultNull) {
  thd()->variables.max_sort_length = 4U;
  Item *int_item = m_sort_fields[0].item = new Item_int(42);
  int_item->maybe_null = true;
  int_item->null_value = true;

  const uint total_length = sortlength(thd(), m_sort_fields, 1);
  EXPECT_EQ(1 + sizeof(longlong), total_length);
  EXPECT_EQ(sizeof(longlong), m_sort_fields[0].length);
  EXPECT_EQ(INT_RESULT, m_sort_fields[0].result_type);

  m_sort_param.make_sortkey(m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

TEST_F(MakeSortKeyTest, DecimalResult) {
  const char dec_str[] = "1234567890.1234567890";
  thd()->variables.max_sort_length = 4U;
  m_sort_fields[0].item =
      new Item_decimal(POS(), dec_str, strlen(dec_str), &my_charset_bin);
  Parse_context pc(thd(), thd()->lex->current_select());
  EXPECT_FALSE(m_sort_fields[0].item->itemize(&pc, &m_sort_fields[0].item));

  const uint total_length = sortlength(thd(), m_sort_fields, 1);
  EXPECT_EQ(10U, total_length);
  EXPECT_EQ(10U, m_sort_fields[0].length);
  EXPECT_EQ(DECIMAL_RESULT, m_sort_fields[0].result_type);

  m_sort_param.make_sortkey(m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

TEST_F(MakeSortKeyTest, RealResult) {
  const char dbl_str[] = "1234567890.1234567890";
  thd()->variables.max_sort_length = 4U;
  m_sort_fields[0].item = new Item_float(dbl_str, strlen(dbl_str));

  const uint total_length = sortlength(thd(), m_sort_fields, 1);
  EXPECT_EQ(sizeof(double), total_length);
  EXPECT_EQ(sizeof(double), m_sort_fields[0].length);
  EXPECT_EQ(REAL_RESULT, m_sort_fields[0].result_type);

  m_sort_param.make_sortkey(m_to, m_ref_buff);
  SCOPED_TRACE("");
  verify_buff(total_length);
}

}  // namespace make_sortkey_unittest
