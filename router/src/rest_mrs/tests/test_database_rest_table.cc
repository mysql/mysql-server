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

#include "helper/make_shared_ptr.h"
#include "mrs/database/query_rest_table.h"

#include "mock/mock_session.h"

using helper::Column;
using mrs::database::QueryRestTable;
using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

class QueryRestTableTests : public Test {
 public:
  StrictMock<MockMySQLSession> mock_session_;
  helper::MakeSharedPtr<QueryRestTable> sut_;
};

TEST_F(QueryRestTableTests, basic_empty_request_throws) {
  std::vector<Column> columns{};

  EXPECT_THROW(sut_->query_entries(&mock_session_, columns, "schema", "obj", 0,
                                   25, "my.url", "c2", true),
               std::invalid_argument);
}

TEST_F(QueryRestTableTests, basic_two_request_without_result) {
  std::vector<Column> columns{{"c1", MYSQL_TYPE_STRING},
                              {"c2", MYSQL_TYPE_INT24, true}};
  EXPECT_CALL(mock_session_, query(StrEq("SELECT JSON_OBJECT("
                                         "'c1',`c1`,"
                                         "'c2',`c2`, "
                                         "'links', JSON_ARRAY(JSON_OBJECT("
                                         "'rel','self',"
                                         "'href',CONCAT('my.url','/',`c2`)))) "
                                         "FROM `schema`.`obj`  LIMIT 0,26"),
                                   _, _));

  sut_->query_entries(&mock_session_, columns, "schema", "obj", 0, 25, "my.url",
                      "c2", true);
}

TEST_F(QueryRestTableTests, basic_two_request_without_result_and_no_links) {
  std::vector<Column> columns{{"c1", MYSQL_TYPE_STRING},
                              {"c2", MYSQL_TYPE_INT24, true}};
  EXPECT_CALL(mock_session_, query(StrEq("SELECT JSON_OBJECT("
                                         "'c1',`c1`,"
                                         "'c2',`c2`, "
                                         "'links', JSON_ARRAY()) "
                                         "FROM `schema`.`obj`  LIMIT 0,26"),
                                   _, _));

  sut_->query_entries(&mock_session_, columns, "schema", "obj", 0, 25, "my.url",
                      "", true);
}
