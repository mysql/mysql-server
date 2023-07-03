/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/log_filter.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std::literals::string_literals;

namespace mysqlrouter {

class LogFilterTest : public testing::Test {
 protected:
  LogFilter log_filter;
  static const std::string create_pattern_;
  static const std::string alter_pattern_;
};
/*static*/ const std::string LogFilterTest::create_pattern_ =
    "(CREATE USER '([[:graph:]]+)' WITH mysql_native_password AS) "
    "([[:graph:]]*)";
/*static*/ const std::string LogFilterTest::alter_pattern_ =
    "(ALTER USER [[:graph:]]+ IDENTIFIED WITH) ([[:graph:]]*) (BY) "
    "([[:graph:]]*) (PASSWORD EXPIRE INTERVAL 180 DAY)";

TEST_F(LogFilterTest, IsStatementNotChangedWhenNoPatternAdded) {
  const std::string statement =
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS 'password123'";
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(statement));
}

TEST_F(LogFilterTest, IsStatementNotChangedWhenNoPatternMatched) {
  const std::string statement =
      "xxxxxx USER 'router_1t3f' WITH mysql_native_password AS 'password123'";
  log_filter.add_pattern(create_pattern_, "***");
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(statement));
}

TEST_F(LogFilterTest, IsEmptyPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS ''";
  log_filter.add_pattern(create_pattern_, "$1 ***");
  const std::string expected_result(
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS ***");
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsSpecialCharacterPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS '%$_*@'";
  log_filter.add_pattern(create_pattern_, "$1 ***");
  const std::string expected_result(
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS ***");
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS 'password123'";
  log_filter.add_pattern(create_pattern_, "$1 ***");
  const std::string expected_result(
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS ***");
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsPasswordHiddenWhenPatternSameAsReplacement) {
  // this is a cornercase that exists if password is passed in plaintext && is
  // '***'
  const std::string statement =
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS '***'";
  log_filter.add_pattern(create_pattern_, "$1 ***");
  const std::string expected_result(
      "CREATE USER 'router_1t3f' WITH mysql_native_password AS ***");
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsMoreThenOneGroupHidden) {
  const std::string statement =
      "ALTER USER \'jeffrey\'@\'localhost\' IDENTIFIED WITH sha256_password BY "
      "\'new_password\' PASSWORD EXPIRE INTERVAL 180 DAY";
  const std::string expected_result =
      "ALTER USER \'jeffrey\'@\'localhost\' IDENTIFIED WITH *** BY *** "
      "PASSWORD EXPIRE INTERVAL 180 DAY";
  log_filter.add_pattern(alter_pattern_, "$1 *** $3 *** $5");
  EXPECT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

}  // namespace mysqlrouter

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
