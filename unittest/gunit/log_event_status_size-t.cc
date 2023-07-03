/* Copyright (c) 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <array>

#include <gtest/gtest.h>
#include "libbinlogevents/include/binary_log.h"

#include <memory>
#include "sql/binlog_ostream.h"
#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "unittest/gunit/test_utils.h"

#define private public
#include "sql/log_event.h"
#undef private

namespace binary_log::unittests {

using namespace std;

class LogEventStatusSizeTest : public ::testing::Test {
 protected:
  LogEventStatusSizeTest() = default;

  void SetUp() override {}

  void TearDown() override {}

  static std::string fill_str(size_t str_len, char pattern) {
    std::string tmp;
    for (size_t id = 0; id < str_len; ++id) {
      tmp.push_back(pattern);
    }
    return tmp;
  }

  static std::string fill_str(size_t str_len) { return fill_str(str_len, 'a'); }

  static void test_query_log_event_max_status_size() {
    my_testing::Server_initializer srv;
    srv.SetUp();
    auto test_string_namelen = fill_str(NAME_LEN);
    auto test_string_zone = fill_str(MAX_TIME_ZONE_NAME_LENGTH);

    ASSERT_EQ(test_string_namelen.length(), NAME_LEN);

    std::string query = "INSERT INTO t VALUES(1)";

    bool using_trans = false;
    bool immediate = false;
    bool suppress_use = false;
    int error = 0;
    bool ignore_command = false;

    Query_log_event qe(srv.thd(), query.c_str(), query.length(), using_trans,
                       immediate, suppress_use, error, ignore_command);

    Binlog_cache_storage os;
    os.open(50000, 90000);  // random values, bigger than maximal packet size

    // set qe values to simulate maximal size of the status variables
    // artificial data
    qe.flags2_inited = 1;
    qe.sql_mode_inited = 1;
    auto test_catalog_string = fill_str(255);
    qe.catalog_len = test_catalog_string.length();
    qe.catalog = test_catalog_string.c_str();
    qe.auto_increment_increment = 0;
    qe.charset_inited = 1;
    qe.time_zone_len = test_string_zone.length();
    qe.time_zone_str = test_string_zone.c_str();
    qe.lc_time_names_number = 1;
    qe.charset_database_number = 1;
    qe.table_map_for_update = 1;
    qe.thd->binlog_need_explicit_defaults_ts = 1;
    qe.thd->slave_thread = true;
    string user = fill_str(32 * 3);
    string host = fill_str(255);
    LEX_STRING lexStrUser;
    lexStrUser.str = &(user[0]);
    lexStrUser.length = user.length();
    LEX_STRING lexStrHost;
    lexStrHost.str = &(host[0]);
    lexStrHost.length = host.length();

    qe.thd->set_invoker(&lexStrUser, &lexStrHost);
    qe.thd->binlog_invoker();  // sets true
    qe.thd->query_start_usec_used = true;
    qe.thd->binlog_need_explicit_defaults_ts = true;
    qe.ddl_xid = 1;
    qe.need_sql_require_primary_key = 1;
    qe.needs_default_table_encryption = 1;
    qe.default_collation_for_utf8mb4_number = 1;

    for (size_t did = 0; did < MAX_DBS_IN_EVENT_MTS; ++did) {
      std::string db = fill_str(NAME_LEN, 'a' + did);
      qe.thd->add_to_binlog_accessed_dbs(db.c_str());
    }
    // sanity check after filling accessed dbs, db names must be unique
    ASSERT_EQ(qe.thd->get_binlog_accessed_db_names()->elements,
              MAX_DBS_IN_EVENT_MTS);

    qe.write(&os);

    // make sure that the length of status variables is equal to
    // MAX_SIZE_LOG_EVENT_STATUS
    ASSERT_EQ(MAX_SIZE_LOG_EVENT_STATUS, qe.status_vars_len);

    srv.TearDown();
  }
};

TEST_F(LogEventStatusSizeTest, LogEventBoundaryConditions) {
  LogEventStatusSizeTest::test_query_log_event_max_status_size();
}

}  // namespace binary_log::unittests
