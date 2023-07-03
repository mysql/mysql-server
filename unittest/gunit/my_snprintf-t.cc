/* Copyright (c) 2018, 2023, Oracle and/or its affiliates. 

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "my_config.h"
#include <gtest/gtest.h>
#include <string>

#include "m_string.h"
#include "mysql_com.h"

namespace my_snprintf_unittest {

using std::string;

string sss= string(2 * MYSQL_ERRMSG_SIZE, 'a');

class SnPrintfTest : public ::testing::Test
{
public:
  virtual void SetUp()
  {
    compile_time_assert(MYSQL_ERRMSG_SIZE == 512);
    memset(m_errmsg_buf, 'x', sizeof(m_errmsg_buf));
  }

  char m_errmsg_buf[MYSQL_ERRMSG_SIZE * 2];
};

TEST_F(SnPrintfTest, FixedPrecisionOne)
{
  EXPECT_EQ(1, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                           "%.1s", sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[1]);
  EXPECT_EQ('x', m_errmsg_buf[2]);
}

TEST_F(SnPrintfTest, FixedPrecisionTwo)
{
  EXPECT_EQ(511, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                             "%.511s", sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[511]);
  EXPECT_EQ('x', m_errmsg_buf[512]);
}

TEST_F(SnPrintfTest, FixedPrecisionThree)
{
  EXPECT_EQ(511, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                             "%.512s", sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[511]);
  EXPECT_EQ('x', m_errmsg_buf[512]);
}

TEST_F(SnPrintfTest, FixedPrecisionFour)
{
  EXPECT_EQ(511, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                             "%.1000s", sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[511]);
  EXPECT_EQ('x', m_errmsg_buf[512]);
}

TEST_F(SnPrintfTest, DynamicPrecisionOne)
{
  EXPECT_EQ(1, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                           "%.*s", 1, sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[1]);
  EXPECT_EQ('x', m_errmsg_buf[2]);
}

TEST_F(SnPrintfTest, DynamicPrecisionTwo)
{
  EXPECT_EQ(511, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                             "%.*s", 511, sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[511]);
  EXPECT_EQ('x', m_errmsg_buf[512]);
}

TEST_F(SnPrintfTest, DynamicPrecisionThree)
{
  EXPECT_EQ(511, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                             "%.*s", 512, sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[511]);
  EXPECT_EQ('x', m_errmsg_buf[512]);
}

TEST_F(SnPrintfTest, DynamicPrecisionFour)
{
  EXPECT_EQ(511, my_snprintf(m_errmsg_buf, MYSQL_ERRMSG_SIZE,
                             "%.*s", 1000, sss.data()));
  EXPECT_EQ('a', m_errmsg_buf[0]);
  EXPECT_EQ('\0', m_errmsg_buf[511]);
  EXPECT_EQ('x', m_errmsg_buf[512]);
}

} // namespace
