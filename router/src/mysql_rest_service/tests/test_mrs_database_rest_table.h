/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_DATABASE_REST_TABLE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_DATABASE_REST_TABLE_H_

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include "mrs/database/query_rest_table.h"
#include "mrs/database/query_rest_table_single_row.h"
#include "mrs/http/error.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/mysql_session.h"
#include "test_mrs_object_utils.h"

class DatabaseRestTableTest : public testing::Test {
 public:
  std::unique_ptr<mysqlrouter::MySQLSession> m_;
  std::map<std::string, int> initial_table_sizes_;
  std::string initial_binlog_file_;
  uint64_t initial_binlog_position_ = 0;

  void SetUp() override;
  void TearDown() override;

  void reset_test();
  void snapshot();

  void create_schema();

  void drop_schema();

  int num_rows_added(const std::string &table);

  std::string next_auto_inc(const std::string &table) {
    m_->execute("ANALYZE TABLE mrstestdb." + table);
    auto row =
        m_->query_one("SHOW TABLE STATUS FROM mrstestdb LIKE '" + table + "'");
    auto id = (*row)[10];
    return id ? id : "1";
  }

  bool binlog_changed() const;

  rapidjson::Document get_one(
      std::shared_ptr<mrs::database::entry::Object> object,
      const mrs::database::PrimaryKeyColumnValues &pk);

  void execute(const std::string &sql) { m_->execute(sql); }
};

#define EXPECT_NO_CHANGES() EXPECT_FALSE(binlog_changed())

#define EXPECT_ROWS_ADDED(table, num) EXPECT_EQ(num, num_rows_added(table))

using mysql_harness::utility::string_format;

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_DATABASE_REST_TABLE_H_
