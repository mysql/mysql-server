/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "mock/mock_session.h"
#include "mrs/database/query_rest_table_insert.h"

using namespace mrs::database;

using testing::_;
using testing::StrictMock;
using testing::Test;

class DatabaseQueryInsertTest : public Test {
 public:
  StrictMock<MockMySQLSession> mock_session_;
  QueryRestObjectInsert sut_;
};

template <typename C>
auto from_container(C &c) {
  return std::make_pair<typename C::iterator, typename C::iterator>(c.begin(),
                                                                    c.end());
}

TEST_F(DatabaseQueryInsertTest, insert_single_column) {
  EXPECT_CALL(
      mock_session_,
      query("INSERT INTO `schema1`.`table1`(`column1`) VALUES('value1')", _,
            _));
  std::vector<std::string> columns{"column1"};
  std::vector<std::string> values{"value1"};
  sut_.execute(&mock_session_, "schema1", "table1", from_container(columns),
               from_container(values));
}

TEST_F(DatabaseQueryInsertTest, insert_multiple_columns) {
  EXPECT_CALL(mock_session_, query("INSERT INTO `schema1`.`table1`(`column1`,"
                                   "`column2`) VALUES('value1','value2')",
                                   _, _));
  std::vector<std::string> columns{"column1", "column2"};
  std::vector<std::string> values{"value1", "value2"};
  sut_.execute(&mock_session_, "schema1", "table1", from_container(columns),
               from_container(values));
}

TEST_F(DatabaseQueryInsertTest, upinsert_single_column) {
  EXPECT_CALL(
      mock_session_,
      query("INSERT INTO `schema1`.`table1`(`column1`) VALUES('value1') ON "
            "DUPLICATE KEY UPDATE  `column1`='value1'",
            _, _));
  std::vector<std::string> columns{"column1"};
  std::vector<std::string> values{"value1"};
  sut_.execute_with_upsert(&mock_session_, "schema1", "table1",
                           from_container(columns), from_container(values));
}

TEST_F(DatabaseQueryInsertTest, upinsert_multiple_columns) {
  EXPECT_CALL(
      mock_session_,
      query("INSERT INTO `schema1`.`table1`(`column1`,"
            "`column2`) VALUES('value1','value2') ON "
            "DUPLICATE KEY UPDATE  `column1`='value1', `column2`='value2'",
            _, _));
  std::vector<std::string> columns{"column1", "column2"};
  std::vector<std::string> values{"value1", "value2"};
  sut_.execute_with_upsert(&mock_session_, "schema1", "table1",
                           from_container(columns), from_container(values));
}
