/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include <gtest/gtest.h>
#include <mysql/service_my_snprintf.h>

#include "ngs_common/xdatetime.h"

namespace xpl
{

namespace test
{

TEST(xdatetime, date)
{
  EXPECT_TRUE(mysqlx::DateTime(2015, 12, 5));
  EXPECT_EQ("2015/12/05", mysqlx::DateTime(2015, 12, 5).to_string());
  EXPECT_EQ("2015/12/05", mysqlx::DateTime(2015, 12, 5).to_string());
  EXPECT_EQ("0001/01/01", mysqlx::DateTime(1, 1, 1).to_string());
  EXPECT_EQ("0000/00/00", mysqlx::DateTime(0, 0, 0).to_string());
  EXPECT_EQ("9999/12/31", mysqlx::DateTime(9999, 12, 31).to_string());
  EXPECT_FALSE(mysqlx::DateTime(0, 50, 60));
}


TEST(xdatetime, datetime)
{
  EXPECT_EQ("2015/12/05 00:00:00.123456", mysqlx::DateTime(2015, 12, 5, 0, 0, 0, 123456).to_string());
  EXPECT_EQ("0001/01/01 23:45:59.99", mysqlx::DateTime(1, 1, 1, 23, 45, 59, 990000).to_string());
  EXPECT_EQ("0000/00/00 00:00:00", mysqlx::DateTime(0, 0, 0, 0, 0, 0).to_string());
  EXPECT_EQ("9999/12/31 23:59:59.999999", mysqlx::DateTime(9999, 12, 31, 23, 59, 59, 999999).to_string());

  EXPECT_EQ("23:59:59.999999", mysqlx::DateTime(9999, 12, 31, 23, 59, 59, 999999).time().to_string());

  EXPECT_FALSE(mysqlx::DateTime(0, 50, 60, 24, 60, 60));
}



TEST(xdatetime, time)
{
  EXPECT_EQ("00:00:00.123456", mysqlx::Time(false, 0, 0, 0, 123456).to_string());
  EXPECT_EQ("00:00:00.1234", mysqlx::Time(false, 0, 0, 0, 123400).to_string());
  EXPECT_EQ("23:45:59.99", mysqlx::Time(false, 23, 45, 59, 990000).to_string());
  EXPECT_EQ("00:00:00", mysqlx::Time(false, 0, 0, 0).to_string());
  EXPECT_EQ("23:59:59.999999", mysqlx::Time(false, 23, 59, 59, 999999).to_string());
  EXPECT_EQ("00:00:00.33", mysqlx::Time(false, 0, 0, 0, 330000).to_string());
  EXPECT_EQ("-00:00:00.33", mysqlx::Time(true, 0, 0, 0, 330000).to_string());
  EXPECT_EQ("-821:00:00.33", mysqlx::Time(true, 821, 0, 0, 330000).to_string());

  EXPECT_FALSE(mysqlx::Time(false, 24, 60, 60));
}


} // namespace test

} // namespace xpl
