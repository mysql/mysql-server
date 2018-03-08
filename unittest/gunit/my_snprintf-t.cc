/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved. 

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
