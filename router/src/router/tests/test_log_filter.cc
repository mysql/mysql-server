/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mysqlrouter/log_filter.h"

namespace mysqlrouter {

class LogFilterTest : public testing::Test {
 public:
  LogFilter log_filter;
};

TEST_F(LogFilterTest, IsStatementNotChangedWhenNoPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS 'password123'";
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(statement));
}

TEST_F(LogFilterTest, IsEmptyPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS ''";
  const std::string pattern(
      "CREATE USER ([[:graph:]]+) WITH mysql_native_password AS "
      "([[:graph:]]*)");
  log_filter.add_pattern(pattern, 2);
  const std::string expected_result(
      "CREATE USER router_xxxx WITH mysql_native_password AS ***");
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsSpecialCharacterPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS '%$_*@'";
  const std::string pattern(
      "CREATE USER ([[:graph:]]+) WITH mysql_native_password AS "
      "([[:graph:]]*)");
  log_filter.add_pattern(pattern, 2);
  const std::string expected_result(
      "CREATE USER router_xxxx WITH mysql_native_password AS ***");
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsPasswordHiddenWhenPatternMatched) {
  const std::string statement =
      "CREATE USER router_xxxx WITH mysql_native_password AS 'password123'";
  const std::string pattern(
      "CREATE USER ([[:graph:]]+) WITH mysql_native_password AS "
      "([[:graph:]]*)");
  log_filter.add_pattern(pattern, 2);
  const std::string expected_result(
      "CREATE USER router_xxxx WITH mysql_native_password AS ***");
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

TEST_F(LogFilterTest, IsMoreThenOneGroupHidden) {
  const std::string statement =
      "ALTER USER \'jeffrey\'@\'localhost\' IDENTIFIED WITH sha256_password BY "
      "\'new_password\' PASSWORD EXPIRE INTERVAL 180 DAY";
  const std::string pattern =
      "ALTER USER ([[:graph:]]+) IDENTIFIED WITH ([[:graph:]]*) BY "
      "([[:graph:]]*) PASSWORD EXPIRE INTERVAL 180 DAY";
  const std::string expected_result =
      "ALTER USER \'jeffrey\'@\'localhost\' IDENTIFIED WITH *** BY *** "
      "PASSWORD EXPIRE INTERVAL 180 DAY";
  log_filter.add_pattern(pattern, std::vector<size_t>{2, 3});
  ASSERT_THAT(log_filter.filter(statement), testing::Eq(expected_result));
}

}  // namespace mysqlrouter
