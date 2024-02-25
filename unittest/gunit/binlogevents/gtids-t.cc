/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "libbinlogevents/include/gtids/gtid.h"
#include "libbinlogevents/include/gtids/gtidset.h"

namespace binary_log::gtids::unittests {

const std::string DEFAULT_UUID1 = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
const std::string DEFAULT_UUID2 = "aaaaaaaa-aaaa-aaaa-aaaa-bbbbbbbbbbbb";
const std::string DEFAULT_UUID3 = "cccccccc-cccc-cccc-cccc-cccccccccccc";
const std::string INVALID_UUID = "-aaaa-aaaa-aaaa-bbbbbbbbbbbb";

class GtidsTest : public ::testing::Test {
 protected:
  binary_log::gtids::Uuid valid_uuid1;
  binary_log::gtids::Uuid valid_uuid2;
  binary_log::gtids::Uuid valid_uuid3;
  binary_log::gtids::Uuid invalid_uuid;

  binary_log::gtids::Gtid gtid1_1{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid1_2{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid1_100{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid2_1{valid_uuid2, 0};
  binary_log::gtids::Gtid gtid2_2{valid_uuid2, 0};
  binary_log::gtids::Gtid gtid2_1000{valid_uuid2, 0};
  binary_log::gtids::Gtid gtid3_1{valid_uuid3, 0};
  binary_log::gtids::Gtid gtid3_2{valid_uuid3, 0};
  binary_log::gtids::Gtid gtid1_1_copy{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid__1{invalid_uuid, 0};

  GtidsTest() = default;

  void SetUp() override {
    ASSERT_FALSE(
        valid_uuid1.parse(DEFAULT_UUID1.c_str(), DEFAULT_UUID1.size()));
    ASSERT_FALSE(
        valid_uuid2.parse(DEFAULT_UUID2.c_str(), DEFAULT_UUID2.size()));
    ASSERT_FALSE(
        valid_uuid3.parse(DEFAULT_UUID3.c_str(), DEFAULT_UUID3.size()));
    ASSERT_TRUE(invalid_uuid.parse(INVALID_UUID.c_str(), INVALID_UUID.size()));

    gtid1_1 = {valid_uuid1, 1};
    gtid1_2 = {valid_uuid1, 2};
    gtid1_100 = {valid_uuid1, 100};
    gtid2_1 = {valid_uuid2, 1};
    gtid2_2 = {valid_uuid2, 2};
    gtid2_1000 = {valid_uuid2, 1000};
    gtid3_1 = {valid_uuid3, 1};
    gtid3_2 = {valid_uuid3, 2};
    gtid1_1_copy = {valid_uuid1, 1};
    gtid__1 = {invalid_uuid, 1};
  }

  void TearDown() override {}
};

TEST_F(GtidsTest, GtidCopyAssignment) {
  binary_log::gtids::Gtid gtid2_1_assigned = gtid2_1;
  ASSERT_EQ(gtid2_1_assigned, gtid2_1);
}

TEST_F(GtidsTest, GtidComparison) {
  ASSERT_TRUE(gtid1_1 != gtid1_2);
  ASSERT_FALSE(gtid1_1 == gtid1_2);
  ASSERT_FALSE(gtid1_1 == gtid2_1);
  ASSERT_TRUE(gtid1_1 == gtid1_1_copy);
}

TEST_F(GtidsTest, GtidToString) {
  std::stringstream expected;
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << gtid1_1.get_gno();

  ASSERT_EQ(expected.str(), gtid1_1.to_string());

  expected.str("");
  expected << DEFAULT_UUID2 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << gtid2_1.get_gno();
  ASSERT_EQ(expected.str(), gtid2_1.to_string());

  expected.str("");
  expected << INVALID_UUID << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << gtid__1.get_gno();
  ASSERT_NE(expected.str(), gtid__1.to_string());
}

TEST_F(GtidsTest, IntervalsBasics) {
  binary_log::gtids::gno_t range_start = 1;
  binary_log::gtids::gno_t range_end = 2;
  binary_log::gtids::gno_t next_in_range = 3;
  binary_log::gtids::gno_t not_in_range = 100;

  binary_log::gtids::Gno_interval interval{range_start, range_end + 1};

  binary_log::gtids::Gtid next_gtid{valid_uuid1, next_in_range};
  binary_log::gtids::Gtid not_in_range_gtid1{valid_uuid1, not_in_range};
  binary_log::gtids::Gtid not_in_range_gtid2{valid_uuid2, next_in_range};
  binary_log::gtids::Gtid next_gtid_copy{valid_uuid1, next_in_range};

  ASSERT_TRUE(interval.intersects_or_contiguous(
      binary_log::gtids::Gno_interval{next_in_range, next_in_range + 1}));
  ASSERT_FALSE(interval.intersects(
      binary_log::gtids::Gno_interval{not_in_range, not_in_range + 1}));
  ASSERT_TRUE(next_gtid == next_gtid_copy);
}

TEST_F(GtidsTest, IntervalsMerge) {
  binary_log::gtids::Gno_interval interval1{1, 2};
  binary_log::gtids::Gno_interval interval2{3, 4};
  binary_log::gtids::Gno_interval interval3{100, 101};
  binary_log::gtids::Gno_interval interval4{3, 90};

  ASSERT_TRUE(interval1.intersects_or_contiguous(interval2));
  ASSERT_FALSE(interval1.add(interval2));
  ASSERT_TRUE(interval1.add(interval3));
  ASSERT_FALSE(interval1.add(interval4));

  std::stringstream expected;
  expected << interval1.get_start()
           << binary_log::gtids::Gno_interval::SEPARATOR_GNO_START_END
           << interval4.get_end();
  ASSERT_EQ(expected.str(), interval1.to_string());

  expected.str("");
  expected << 100 << binary_log::gtids::Gno_interval::SEPARATOR_GNO_START_END
           << 101;
  ASSERT_EQ(expected.str(), interval3.to_string());
}

TEST_F(GtidsTest, GtidSetBasics) {
  binary_log::gtids::Gtid_set set1;

  std::stringstream expected;
  expected << binary_log::gtids::Gtid_set::EMPTY_GTID_SET;
  ASSERT_EQ(expected.str(), set1.to_string());
  expected.str("");

  set1.add(gtid1_1);
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << gtid1_1.get_gno();
  ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");

  set1.add(gtid1_2);
  binary_log::gtids::Gno_interval i1{1, 2};
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string();
  ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");

  set1.add(gtid1_100);
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid1_100.get_gno();
  ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");

  set1.add(gtid2_1);
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid1_100.get_gno();
  expected << binary_log::gtids::Gtid_set::SEPARATOR_UUID_SETS << DEFAULT_UUID2
           << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << gtid2_1.get_gno();
  // ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");

  set1.add(gtid2_2);
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid1_100.get_gno();
  expected << binary_log::gtids::Gtid_set::SEPARATOR_UUID_SETS;
  expected << DEFAULT_UUID2 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string();
  // ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");

  set1.add(gtid2_1000);
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid1_100.get_gno();
  expected << binary_log::gtids::Gtid_set::SEPARATOR_UUID_SETS;
  expected << DEFAULT_UUID2 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid2_1000.get_gno();
  ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");

  binary_log::gtids::Gtid gtid2_99{valid_uuid2, 99};
  binary_log::gtids::Gtid gtid2_100{valid_uuid2, 100};
  binary_log::gtids::Gtid gtid2_101{valid_uuid2, 101};

  binary_log::gtids::Gtid_set set2;

  set2.add(gtid2_99);
  set2.add(gtid2_100);
  set2.add(gtid2_101);

  binary_log::gtids::Gno_interval i2{gtid2_99.get_gno(), gtid2_101.get_gno()};
  expected << DEFAULT_UUID2 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i2.to_string();
  ASSERT_EQ(set2.to_string(), expected.str());
  expected.str("");

  set1.add(set2);
  expected << DEFAULT_UUID1 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid1_100.get_gno();
  expected << binary_log::gtids::Gtid_set::SEPARATOR_UUID_SETS;
  expected << DEFAULT_UUID2 << binary_log::gtids::Gtid::SEPARATOR_UUID_SEQNO
           << i1.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << i2.to_string()
           << binary_log::gtids::Gtid_set::SEPARATOR_SEQNO_INTERVALS
           << gtid2_1000.get_gno();
  ASSERT_EQ(set1.to_string(), expected.str());
  expected.str("");
}

TEST_F(GtidsTest, GtidSetCopyAssignment) {
  binary_log::gtids::Gtid_set set1;

  set1.add(gtid1_1);
  set1.add(gtid1_2);
  set1.add(gtid2_1);

  binary_log::gtids::Gtid_set set2 = set1;

  // contain the same number of gtids
  ASSERT_EQ(set1.count(), 3);
  ASSERT_EQ(set1.count(), set2.count());

  // assert that the resulting set contains the gtids added to set1
  ASSERT_TRUE(set2.contains(gtid1_1));
  ASSERT_TRUE(set2.contains(gtid1_2));
  ASSERT_TRUE(set2.contains(gtid2_1));

  // assert that both sets are equal
  ASSERT_TRUE(set2 == set1);
}

TEST_F(GtidsTest, GtidSetCountAndEmptyAndReset) {
  binary_log::gtids::Gtid_set set1;

  ASSERT_EQ(set1.count(), 0);
  ASSERT_TRUE(set1.is_empty());

  set1.add(gtid1_1);
  set1.add(gtid1_2);
  set1.add(gtid2_1);

  ASSERT_EQ(set1.count(), 3);
  ASSERT_FALSE(set1.is_empty());

  set1.reset();

  ASSERT_EQ(set1.count(), 0);
  ASSERT_TRUE(set1.is_empty());
}

TEST_F(GtidsTest, GtidSetComparison) {
  binary_log::gtids::Gtid_set set1;
  set1.add(gtid1_1);
  set1.add(gtid2_1);

  // empty set
  binary_log::gtids::Gtid_set empty_set;
  ASSERT_FALSE(set1 == empty_set);

  // same number of uuids as set1, same number of intervals
  binary_log::gtids::Gtid_set equal_set;
  equal_set.add(gtid1_1);
  equal_set.add(gtid2_1);
  ASSERT_TRUE(set1 == equal_set);

  // same number of uuids as set1, different interval in uuid2
  binary_log::gtids::Gtid_set set_with_same_uuids_more_intervals;
  set_with_same_uuids_more_intervals.add(gtid1_1);
  set_with_same_uuids_more_intervals.add(gtid2_1);
  set_with_same_uuids_more_intervals.add(gtid2_2);
  ASSERT_FALSE(set1 == set_with_same_uuids_more_intervals);

  // same number of uuids as set1, but one is different
  binary_log::gtids::Gtid_set set_with_different_uuids;
  set_with_different_uuids.add(gtid1_1);
  set_with_different_uuids.add(gtid3_1);
  ASSERT_FALSE(set1 == set_with_different_uuids);
}

TEST_F(GtidsTest, GtidSetToString) {
  binary_log::gtids::Gtid_set set1;
  binary_log::gtids::Gtid_set set2;

  set1.add(gtid1_1);
  set1.add(gtid1_2);
  set1.add(gtid2_1);

  ASSERT_TRUE(set1.contains(gtid1_1));
  ASSERT_TRUE(set1.contains(gtid1_2));
  ASSERT_TRUE(set1.contains(gtid2_1));

  set2.add(gtid1_1);
  set2.add(gtid1_2);
  set2.add(gtid2_1);

  ASSERT_TRUE(set2.contains(gtid1_1));
  ASSERT_TRUE(set2.contains(gtid1_2));
  ASSERT_TRUE(set2.contains(gtid2_1));

  std::string set1_string = set1.to_string();
  std::string set2_string = set2.to_string();

  ASSERT_EQ(set1_string, set2_string);
}
}  // namespace binary_log::gtids::unittests
