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
#include <string>

#include "libchangestreams/include/mysql/cs/reader/state.h"

namespace cs::reader::unittests {

const std::string DEFAULT_UUID1 = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
const std::string DEFAULT_UUID2 = "aaaaaaaa-aaaa-aaaa-aaaa-bbbbbbbbbbbb";

class ReaderStateTest : public ::testing::Test {
 protected:
  binary_log::gtids::Uuid valid_uuid1;
  binary_log::gtids::Uuid valid_uuid2;
  cs::reader::State state1;

  binary_log::gtids::Gtid gtid1_1{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid1_2{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid2_1{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid1_1_copy{valid_uuid1, 0};

  ReaderStateTest() = default;

  void SetUp() override {
    valid_uuid1.parse(DEFAULT_UUID1.c_str(), DEFAULT_UUID1.size());
    valid_uuid2.parse(DEFAULT_UUID2.c_str(), DEFAULT_UUID2.size());

    gtid1_1 = {valid_uuid1, 1};
    gtid1_2 = {valid_uuid1, 2};
    gtid2_1 = {valid_uuid2, 1};
    gtid1_1_copy = {valid_uuid1, 1};
  }

  void TearDown() override {}
};

TEST_F(ReaderStateTest, StateReset) {
  state1.add_gtid(gtid1_2);
  state1.add_gtid(gtid2_1);
  state1.add_gtid(gtid1_1_copy);

  ASSERT_TRUE(state1.get_gtids().contains(gtid1_1));
  ASSERT_TRUE(state1.get_gtids().contains(gtid1_2));
  ASSERT_TRUE(state1.get_gtids().contains(gtid2_1));

  // clear original state
  state1.reset();

  // assert that the original state is empty
  ASSERT_EQ(state1.get_gtids().count(), 0);

  ASSERT_FALSE(state1.get_gtids().contains(gtid1_1));
  ASSERT_FALSE(state1.get_gtids().contains(gtid1_2));
  ASSERT_FALSE(state1.get_gtids().contains(gtid2_1));

  state1.add_gtid(gtid1_1);
  ASSERT_TRUE(state1.get_gtids().contains(gtid1_1));

  state1.reset();
}

TEST_F(ReaderStateTest, StateAddGtidSet) {
  binary_log::gtids::Gtid_set set;
  cs::reader::State state;

  set.add(gtid1_1);
  set.add(gtid1_2);
  set.add(gtid2_1);

  state.add_gtid_set(set);

  // assert that the size of the gtids in the state is 3
  ASSERT_EQ(state.get_gtids().count(), 3);

  // assert that the gtids added are those expected
  ASSERT_TRUE(state.get_gtids().contains(gtid1_1));
  ASSERT_TRUE(state.get_gtids().contains(gtid1_2));
  ASSERT_TRUE(state.get_gtids().contains(gtid2_1));
}

TEST_F(ReaderStateTest, StateCopyAssignmentOperator) {
  state1.add_gtid(gtid1_1);
  state1.add_gtid(gtid1_2);
  state1.add_gtid(gtid2_1);
  state1.add_gtid(gtid1_1_copy);

  // assignment operator
  cs::reader::State state2 = state1;

  // check that original and copy states match
  ASSERT_EQ(state1, state2);

  ASSERT_TRUE(state2.get_gtids().contains(gtid1_1));
  ASSERT_TRUE(state2.get_gtids().contains(gtid1_2));
  ASSERT_TRUE(state2.get_gtids().contains(gtid2_1));

  // clear original state
  state1.reset();

  // assert original and copy are not equal anymore
  ASSERT_FALSE(state1 == state2);
}

}  // namespace cs::reader::unittests
