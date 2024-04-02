/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#include <map>
#include <string>
#include <vector>

#include "helper/container/generic.h"
#include "mrs/database/helper/gtid.h"

using Gtid = mrs::database::Gtid;
using GtidSet = mrs::database::GtidSet;

static std::string make_uuid(const std::string &range) {
  using namespace std::string_literals;
  return "3E11FA47-71CA-11E1-9E33-C80AA9429562"s + range;
}

TEST(GTID, invalid_gtids) {
  Gtid g1;

  ASSERT_FALSE(g1.parse(""));
  ASSERT_FALSE(
      g1.parse("ASSERT_FALSE(g1.parse("
               "));"));
  ASSERT_FALSE(g1.parse("3E11FA47-71CA"));
  // UUID shorter, missing one character
  ASSERT_FALSE(g1.parse("3E11FA47-71CA-11E1-9E33-C80AA942956:23"));
  ASSERT_FALSE(g1.parse("3E11FA47-71CA-11E1-9E33-C80AA9429562"));
  ASSERT_FALSE(g1.parse("3E11FA47-71CA-11E1-9E33-C80AA9429562:"));
  ASSERT_FALSE(g1.parse(":23"));
}

TEST(GTID, invalid_gtids_sets) {
  GtidSet g1;

  ASSERT_FALSE(g1.parse(""));
  ASSERT_FALSE(
      g1.parse("ASSERT_FALSE(g1.parse("
               "));"));
  ASSERT_FALSE(g1.parse("3E11FA47-71CA"));
  // UUID shorter, missing one character
  ASSERT_FALSE(g1.parse("3E11FA47-71CA-11E1-9E33-C80AA942956:23"));
  ASSERT_FALSE(g1.parse("3E11FA47-71CA-11E1-9E33-C80AA9429562"));
  ASSERT_FALSE(g1.parse("3E11FA47-71CA-11E1-9E33-C80AA9429562:"));
  ASSERT_FALSE(g1.parse(":23"));
}

TEST(GTID, basic) {
  Gtid g1_uuid1_23{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23"};
  Gtid g2_uuid1_24{"3E11FA47-71CA-11E1-9E33-C80AA9429562:24"};
  Gtid g3_uuid2_23{"3E11FA47-71CA-11E1-9E33-C80AA9429563:23"};
  Gtid g4_uuid3_23{"3E11FA47-0000-11E1-9E33-C80AA9429562:23"};
  Gtid g5_uuid3_1{"3E11FA47-0000-11E1-9E33-C80AA9429562:1"};
  Gtid g6_uuid2_23{"3E11FA47-71CA-11E1-9E33-C80AA9429563:23"};
  Gtid g7_uuid3_1_20{"3E11FA47-0000-11E1-9E33-C80AA9429562:1-20"};

  ASSERT_EQ(g6_uuid2_23, g3_uuid2_23);
  ASSERT_EQ(g3_uuid2_23, g6_uuid2_23);

  ASSERT_FALSE(g2_uuid1_24 == g6_uuid2_23);
  ASSERT_FALSE(g2_uuid1_24 == g3_uuid2_23);
  ASSERT_FALSE(g3_uuid2_23 == g4_uuid3_23);
  ASSERT_FALSE(g4_uuid3_23 == g5_uuid3_1);
  ASSERT_FALSE(g7_uuid3_1_20 == g5_uuid3_1);

  ASSERT_TRUE(g6_uuid2_23.contains(g3_uuid2_23));
  ASSERT_TRUE(g3_uuid2_23.contains(g6_uuid2_23));

  ASSERT_FALSE(g2_uuid1_24.contains(g6_uuid2_23));
  ASSERT_FALSE(g2_uuid1_24.contains(g3_uuid2_23));
  ASSERT_FALSE(g3_uuid2_23.contains(g4_uuid3_23));
  ASSERT_FALSE(g4_uuid3_23.contains(g5_uuid3_1));
  ASSERT_TRUE(g7_uuid3_1_20.contains(g5_uuid3_1));
}

TEST(GTID, gtid_set_basic) {
  GtidSet g1_uuid1_23{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23"};
  GtidSet g2_uuid1_24{"3E11FA47-71CA-11E1-9E33-C80AA9429562:24"};
  GtidSet g3_uuid2_23{"3E11FA47-71CA-11E1-9E33-C80AA9429563:23"};
  GtidSet g4_uuid3_23{"3E11FA47-0000-11E1-9E33-C80AA9429562:23"};
  GtidSet g5_uuid3_1{"3E11FA47-0000-11E1-9E33-C80AA9429562:1"};
  GtidSet g5_uuid3_1_20{"3E11FA47-0000-11E1-9E33-C80AA9429562:1-20"};
  GtidSet g6_uuid2_23{"3E11FA47-71CA-11E1-9E33-C80AA9429563:23"};
  GtidSet g7{"3E11FA47-0000-11E1-9E33-C80AA9429562:1-20:23"};

  EXPECT_EQ(g6_uuid2_23, g3_uuid2_23);
  EXPECT_EQ(g3_uuid2_23, g6_uuid2_23);

  EXPECT_FALSE(g2_uuid1_24 == g6_uuid2_23);
  EXPECT_FALSE(g2_uuid1_24 == g3_uuid2_23);
  EXPECT_FALSE(g3_uuid2_23 == g4_uuid3_23);
  EXPECT_FALSE(g4_uuid3_23 == g5_uuid3_1);

  EXPECT_TRUE(g6_uuid2_23.contains(g3_uuid2_23));
  EXPECT_TRUE(g3_uuid2_23.contains(g6_uuid2_23));

  EXPECT_FALSE(g2_uuid1_24.contains(g6_uuid2_23));
  EXPECT_FALSE(g2_uuid1_24.contains(g3_uuid2_23));
  EXPECT_FALSE(g3_uuid2_23.contains(g4_uuid3_23));
  EXPECT_FALSE(g4_uuid3_23.contains(g5_uuid3_1));

  EXPECT_FALSE(g7 == g5_uuid3_1_20);
  EXPECT_FALSE(g7 == g5_uuid3_1);
  EXPECT_FALSE(g7 == g4_uuid3_23);

  EXPECT_TRUE(g7.contains(g5_uuid3_1_20));
  EXPECT_TRUE(g7.contains(g5_uuid3_1));
  EXPECT_TRUE(g7.contains(g4_uuid3_23));
}

TEST(GTID, gtidset_contsains_point) {
  using namespace std::string_literals;
  GtidSet g_1x20_23x24_30{"3E11FA47-0000-11E1-9E33-C80AA9429562:1-20:23-24:30"};
  std::vector<uint64_t> k_start_values{1,  2,  3,  4,  5,  6,  7,  8,
                                       9,  10, 11, 12, 13, 14, 15, 16,
                                       17, 18, 19, 20, 23, 24, 30};

  for (auto v : k_start_values) {
    Gtid g_point{
        ("3E11FA47-0000-11E1-9E33-C80AA9429562:"s + std::to_string(v)).c_str()};
    GtidSet gs_point{
        ("3E11FA47-0000-11E1-9E33-C80AA9429562:"s + std::to_string(v)).c_str()};
    EXPECT_FALSE(g_1x20_23x24_30 == gs_point);
    EXPECT_TRUE(g_1x20_23x24_30.contains(g_point));
    EXPECT_TRUE(g_1x20_23x24_30.contains(gs_point));
  }

  std::vector<uint64_t> non_acceptable;
  for (int i = 1; i <= 50; ++i) {
    if (!helper::container::has(k_start_values, i)) non_acceptable.push_back(i);
  }

  for (auto v : non_acceptable) {
    Gtid g_point{
        ("3E11FA47-0000-11E1-9E33-C80AA9429562:"s + std::to_string(v)).c_str()};
    GtidSet gs_point{
        ("3E11FA47-0000-11E1-9E33-C80AA9429562:"s + std::to_string(v)).c_str()};
    EXPECT_FALSE(g_1x20_23x24_30 == gs_point);
    EXPECT_FALSE(g_1x20_23x24_30.contains(g_point));
    EXPECT_FALSE(g_1x20_23x24_30.contains(gs_point));
  }
}

TEST(GTID, gtid_contsains_point) {
  Gtid g_range_23_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-40"};
  Gtid g_10{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10"};
  Gtid g_22{"3E11FA47-71CA-11E1-9E33-C80AA9429562:22"};
  Gtid g_23{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23"};
  Gtid g_30{"3E11FA47-71CA-11E1-9E33-C80AA9429562:30"};
  Gtid g_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:40"};
  Gtid g_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:41"};
  Gtid g_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:50"};

  ASSERT_FALSE(g_range_23_40.contains(g_10));
  ASSERT_FALSE(g_range_23_40.contains(g_22));
  ASSERT_TRUE(g_range_23_40.contains(g_23));
  ASSERT_TRUE(g_range_23_40.contains(g_30));
  ASSERT_TRUE(g_range_23_40.contains(g_40));
  ASSERT_FALSE(g_range_23_40.contains(g_41));
  ASSERT_FALSE(g_range_23_40.contains(g_50));
}

TEST(GTID, gtid_contsains_range_wide) {
  Gtid g_range_23_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-40"};

  Gtid g_10_15{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-15"};
  Gtid g_10_23{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-23"};
  Gtid g_10_25{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-25"};
  Gtid g_10_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-41"};
  Gtid g_10_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-40"};
  Gtid g_10_39{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-39"};
  Gtid g_23_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-41"};
  Gtid g_23_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-50"};
  Gtid g_39_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:39-41"};
  Gtid g_39_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:39-50"};
  Gtid g_40_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:40-50"};
  Gtid g_41_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:41-50"};

  Gtid g_23_24{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-24"};
  Gtid g_39_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:39-40"};
  Gtid g_23_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-40"};
  Gtid g_30_25{"3E11FA47-71CA-11E1-9E33-C80AA9429562:30-35"};

  ASSERT_FALSE(g_range_23_40.contains(g_10_15));
  ASSERT_FALSE(g_range_23_40.contains(g_10_23));
  ASSERT_FALSE(g_range_23_40.contains(g_10_25));
  ASSERT_FALSE(g_range_23_40.contains(g_10_41));
  ASSERT_FALSE(g_range_23_40.contains(g_10_40));
  ASSERT_FALSE(g_range_23_40.contains(g_10_39));
  ASSERT_FALSE(g_range_23_40.contains(g_23_41));
  ASSERT_FALSE(g_range_23_40.contains(g_23_50));
  ASSERT_FALSE(g_range_23_40.contains(g_39_41));
  ASSERT_FALSE(g_range_23_40.contains(g_39_50));
  ASSERT_FALSE(g_range_23_40.contains(g_40_50));
  ASSERT_FALSE(g_range_23_40.contains(g_41_50));

  ASSERT_TRUE(g_range_23_40.contains(g_23_24));
  ASSERT_TRUE(g_range_23_40.contains(g_39_40));
  ASSERT_TRUE(g_range_23_40.contains(g_23_40));
  ASSERT_TRUE(g_range_23_40.contains(g_30_25));
}

TEST(GTID, gtid_contsains_range_short) {
  Gtid g_range_23_24{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-24"};

  Gtid g_10_15{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-15"};
  Gtid g_10_23{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-23"};
  Gtid g_10_25{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-25"};
  Gtid g_10_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-41"};
  Gtid g_10_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-40"};
  Gtid g_10_39{"3E11FA47-71CA-11E1-9E33-C80AA9429562:10-39"};
  Gtid g_23_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-41"};
  Gtid g_23_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-50"};
  Gtid g_39_41{"3E11FA47-71CA-11E1-9E33-C80AA9429562:39-41"};
  Gtid g_39_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:39-50"};
  Gtid g_40_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:40-50"};
  Gtid g_41_50{"3E11FA47-71CA-11E1-9E33-C80AA9429562:41-50"};
  Gtid g_23_24{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-24"};
  Gtid g_39_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:39-40"};
  Gtid g_23_40{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23-40"};
  Gtid g_30_25{"3E11FA47-71CA-11E1-9E33-C80AA9429562:30-35"};
  Gtid g_22{"3E11FA47-71CA-11E1-9E33-C80AA9429562:22"};
  Gtid g_25{"3E11FA47-71CA-11E1-9E33-C80AA9429562:25"};

  Gtid g_23{"3E11FA47-71CA-11E1-9E33-C80AA9429562:23"};
  Gtid g_24{"3E11FA47-71CA-11E1-9E33-C80AA9429562:24"};

  ASSERT_FALSE(g_range_23_24.contains(g_10_23));
  ASSERT_FALSE(g_range_23_24.contains(g_10_25));
  ASSERT_FALSE(g_range_23_24.contains(g_10_41));
  ASSERT_FALSE(g_range_23_24.contains(g_10_40));
  ASSERT_FALSE(g_range_23_24.contains(g_10_39));
  ASSERT_FALSE(g_range_23_24.contains(g_23_41));
  ASSERT_FALSE(g_range_23_24.contains(g_23_50));
  ASSERT_FALSE(g_range_23_24.contains(g_39_41));
  ASSERT_FALSE(g_range_23_24.contains(g_39_50));
  ASSERT_FALSE(g_range_23_24.contains(g_40_50));
  ASSERT_FALSE(g_range_23_24.contains(g_41_50));
  ASSERT_FALSE(g_range_23_24.contains(g_39_40));
  ASSERT_FALSE(g_range_23_24.contains(g_23_40));
  ASSERT_FALSE(g_range_23_24.contains(g_30_25));
  ASSERT_FALSE(g_range_23_24.contains(g_22));
  ASSERT_FALSE(g_range_23_24.contains(g_25));

  ASSERT_TRUE(g_range_23_24.contains(g_23_24));
  ASSERT_TRUE(g_range_23_24.contains(g_23));
  ASSERT_TRUE(g_range_23_24.contains(g_24));
}

TEST(GTID, gtid_to_string) {
  std::string k_g1{"3E11FA47-71CA-11E1-9E33-C80AA9429562:24"};
  Gtid g1{k_g1};
  ASSERT_EQ(k_g1, g1.to_string());

  std::string k_g2{"3E11FA47-71CA-11E1-9E33-C80AA9429562:2-10"};
  Gtid g2{k_g2};
  ASSERT_EQ(k_g2, g2.to_string());
}

TEST(GTID, gtidset_to_string) {
  std::string k_g1{"3E11FA47-71CA-11E1-9E33-C80AA9429562:24-24:50:60-70"};
  GtidSet g1{k_g1};
  ASSERT_EQ(k_g1, g1.to_string());
}

TEST(GTID, gtidset_merge_point) {
  std::string k_g1{};
  GtidSet g1{make_uuid(":24-25:50:60-70")};
  EXPECT_EQ(make_uuid(":24-25:50:60-70"), g1.to_string());

  ASSERT_TRUE(g1.try_merge(Gtid(make_uuid(":71"))));
  EXPECT_EQ(make_uuid(":24-25:50:60-71"), g1.to_string());

  ASSERT_TRUE(g1.try_merge(Gtid(make_uuid(":23"))));
  EXPECT_EQ(make_uuid(":23-25:50:60-71"), g1.to_string());

  ASSERT_TRUE(g1.try_merge(Gtid(make_uuid(":26"))));
  EXPECT_EQ(make_uuid(":23-26:50:60-71"), g1.to_string());

  ASSERT_TRUE(g1.try_merge(Gtid(make_uuid(":71"))));
  EXPECT_EQ(make_uuid(":23-26:50:60-71"), g1.to_string());
}

TEST(GTID, gtidset_merge_range) {
  std::string k_g1{};
  GtidSet g1{make_uuid(":24-25:50:60-70")};
  EXPECT_EQ(make_uuid(":24-25:50:60-70"), g1.to_string());

  EXPECT_TRUE(g1.try_merge(Gtid(make_uuid(":54-80"))));
  EXPECT_EQ(make_uuid(":24-25:50:54-80"), g1.to_string());

  EXPECT_TRUE(g1.try_merge(Gtid(make_uuid(":23-30"))));
  EXPECT_EQ(make_uuid(":23-30:50:54-80"), g1.to_string());

  EXPECT_TRUE(g1.try_merge(Gtid(make_uuid(":51"))));
  EXPECT_EQ(make_uuid(":23-30:50-51:54-80"), g1.to_string());

  EXPECT_TRUE(g1.try_merge(Gtid(make_uuid(":45-49"))));
  EXPECT_EQ(make_uuid(":23-30:45-51:54-80"), g1.to_string());

  EXPECT_TRUE(g1.try_merge(Gtid(make_uuid(":81-85"))));
  EXPECT_EQ(make_uuid(":23-30:45-51:54-85"), g1.to_string());
}

TEST(GTID, gtidset_insert_point) {
  std::string k_g1{};
  GtidSet g1{make_uuid(":24-25:50:60-70")};
  EXPECT_EQ(make_uuid(":24-25:50:60-70"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":43"))));
  EXPECT_EQ(make_uuid(":24-25:43:50:60-70"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":1"))));
  EXPECT_EQ(make_uuid(":1:24-25:43:50:60-70"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":100"))));
  EXPECT_EQ(make_uuid(":1:24-25:43:50:60-70:100"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":1000"))));
  EXPECT_EQ(make_uuid(":1:24-25:43:50:60-70:100:1000"), g1.to_string());
}

TEST(GTID, gtidset_insert_range) {
  std::string k_g1{};
  GtidSet g1{make_uuid(":100")};
  EXPECT_EQ(make_uuid(":100"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":43-50"))));
  EXPECT_EQ(make_uuid(":43-50:100"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":10-20"))));
  EXPECT_EQ(make_uuid(":10-20:43-50:100"), g1.to_string());

  EXPECT_TRUE(g1.insert(Gtid(make_uuid(":101-200"))));
  EXPECT_EQ(make_uuid(":10-20:43-50:100:101-200"), g1.to_string());
}
