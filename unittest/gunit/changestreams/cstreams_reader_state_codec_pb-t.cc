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

#include "libchangestreams/include/mysql/cs/reader/state.h"
#include "libchangestreams/src/lib/mysql/cs/codec/pb/reader_state_codec_pb.h"

namespace cs::reader::unittests {

const std::string DEFAULT_UUID1 = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
const std::string DEFAULT_UUID2 = "aaaaaaaa-aaaa-aaaa-aaaa-bbbbbbbbbbbb";

class ReaderStateCodecTest : public ::testing::Test {
 protected:
  binary_log::gtids::Uuid valid_uuid1;
  binary_log::gtids::Uuid valid_uuid2;
  cs::reader::State state;

  binary_log::gtids::Gtid gtid1_1{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid1_2{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid2_1{valid_uuid1, 0};
  binary_log::gtids::Gtid gtid1_1_copy{valid_uuid1, 0};

  ReaderStateCodecTest() = default;

  void SetUp() override {
    ASSERT_FALSE(
        valid_uuid1.parse(DEFAULT_UUID1.c_str(), DEFAULT_UUID1.size()));
    ASSERT_FALSE(
        valid_uuid2.parse(DEFAULT_UUID2.c_str(), DEFAULT_UUID2.size()));

    gtid1_1 = {valid_uuid1, 1};
    gtid1_2 = {valid_uuid1, 2};
    gtid2_1 = {valid_uuid2, 1};
    gtid1_1_copy = {valid_uuid1, 1};
  }

  void TearDown() override {}
};

TEST_F(ReaderStateCodecTest, StateBasics) {
  state.add_gtid(gtid1_1);
  state.add_gtid(gtid1_2);
  state.add_gtid(gtid2_1);
  state.add_gtid(gtid1_1_copy);

  ASSERT_TRUE(state.get_gtids().contains(gtid1_1));
  ASSERT_TRUE(state.get_gtids().contains(gtid1_2));
  ASSERT_TRUE(state.get_gtids().contains(gtid2_1));

  // serialize to protobuf
  cs::reader::codec::pb::example::stringstream pb_ss;
  pb_ss << state;

  // de-serialize from protobuf
  cs::reader::State state_copy;
  pb_ss >> state_copy;

  ASSERT_TRUE(state_copy.get_gtids().contains(gtid1_1));
  ASSERT_TRUE(state_copy.get_gtids().contains(gtid1_2));
  ASSERT_TRUE(state_copy.get_gtids().contains(gtid2_1));

  // compare string representation
  std::stringstream ss_state1;
  std::stringstream ss_state2;

  ss_state1 << state;
  ss_state2 << state_copy;

  ASSERT_EQ(ss_state1.str(), ss_state2.str());
}

}  // namespace cs::reader::unittests
