/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include "mysql/binlog/event/binary_log.h"

#include <memory>
#include "sql/binlog_ostream.h"
#include "sql/current_thd.h"
#include "sql/log_event.h"
#include "sql/sql_class.h"
#include "unittest/gunit/test_utils.h"

namespace unittests::rpl {

using namespace std;

class GtidSpecificationParsingTest : public ::testing::Test {
 protected:
  GtidSpecificationParsingTest() = default;

  void SetUp() override {}

  void TearDown() override {}

  static void test1() {
    my_testing::Server_initializer srv;
    srv.SetUp();

    Gtid gtid;
    gtid.set(1, 1);

    srv.thd()->system_thread = SYSTEM_THREAD_COMPRESS_GTID_TABLE;

    Tsid_map tsid_map(nullptr);

    auto valid_gtids = {
        "11111111-1111-1111-1111-111111111111:4",
        "11111111-1111-1111-1111-111111111111:10",
        "AUTOMATIC",
        " 11111111-1111-1111-1111-111111111111 : 4 ",
        "auTomatic",
        "AUTOMATIC:a",
        "AUTOMATIC: tag",
        "AUTOMATIC: tag1",
        "AUTOMATIC: taG1",
        "AUTOMATIC: tag_",
        "AUTOMATIC: tag_tag",
        "AUTOMATIC: tag_tag",
        "AUTOMATIC: tag_tag_tag_tag_tag_tag_tag_tag_",
        "AUTOMATIC:_tag",
        "11111111-1111-1111-1111-111111111111:tag_tag_tag_tag_tag_tag_tag_tag_:"
        "10",
        "11111111-1111-1111-1111-111111111111:tag1:10",
        "11111111-1111-1111-1111-111111111111:tAg_:10",
        "11111111-1111-1111-1111-111111111111:tAg_: 10",
        " 11111111-1111-1111-1111-111111111111 : tag : 4 "};
    // note that in Gtid_specification parsing, other UUID formats (with curly
    // braces or without dash) are not accepted (function is called with
    // TEXT_LENGTH)
    auto invalid_gtids = {
        "11111111-1111-1111-1111-11111111111:4",
        "11111111-1111-1111-1111-111111111111:a",
        "1111111111111111111111111111111:1",
        "11111111-1111-1111-1111-111111111111:-1"
        "11111111-1111-1111-1111-111111111111:1 x",
        "11111111-1111-1111-1111-111111111111:0",
        "11111111-1111-1111-1111-111111111111:9223372036854775807",
        "11111111-1111-1111-1111-111111111111:18446744073709551617",
        "g1111111-1111-1111-1111-111111111111:1",
        "11111111-1111-1111-1111- 111111111111:1",
        "11111111 222233334444555555555555:1",
        "{ 11111111222233334444555555555555}:1",
        "{11111111 222233334444555555555555}:1",
        "11111111222233334444555555555555:1",
        "{11111111222233334444555555555555}:1",
        "ANONYMOUS ",
        "AUTOMATIC ",
        " ANONYMOUS",
        " AUTOMATIC",
        "AUTOMATIC:",
        "AUTOMATIC : Ba",
        "AUTOMATIC: 1tag",
        "AUTOMATIC:tag#",
        "AUTOMATIC:tag-",
        "AUTOMATIC: tag_tag_tag_tag_tag_tag_tag_tag_tag",
        "11111111-1111-1111-1111-111111111111:tag_tag_tag_tag_tag_tag_tag_tag_"
        "tag:1",
        "11111111-1111-1111-1111-111111111111:#tag:1",
        "11111111-1111-1111-1111-111111111111:1tag:1",
    };

    for (const auto &valid_gtid_str : valid_gtids) {
      Gtid_specification spec;
      auto status = spec.parse(&tsid_map, valid_gtid_str);
      ASSERT_EQ(status, mysql::utils::Return_status::ok);
      auto is_valid = spec.is_valid(valid_gtid_str);
      ASSERT_TRUE(is_valid);
    }

    for (const auto &invalid_gtid_str : invalid_gtids) {
      Gtid_specification spec;
      auto is_valid = spec.is_valid(invalid_gtid_str);
      ASSERT_FALSE(is_valid);
    }

    srv.TearDown();
  }
};

TEST_F(GtidSpecificationParsingTest, GtidSpecificationParsingTestFormat) {
  GtidSpecificationParsingTest::test1();
  static_assert(std::is_trivially_copyable<Gtid_specification>::value);
  static_assert(std::is_standard_layout<Gtid_specification>::value);
}

}  // namespace unittests::rpl
