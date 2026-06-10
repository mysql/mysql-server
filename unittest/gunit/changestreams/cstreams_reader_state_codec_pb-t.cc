/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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
  mysql::uuids::Uuid valid_uuid1;
  mysql::uuids::Uuid valid_uuid2;
  cs::reader::State state;

  mysql::gtids::Gtid gtid1_1 =
      mysql::gtids::Gtid::throwing_make(valid_uuid1, 1);
  mysql::gtids::Gtid gtid1_2 =
      mysql::gtids::Gtid::throwing_make(valid_uuid1, 1);
  mysql::gtids::Gtid gtid2_1 =
      mysql::gtids::Gtid::throwing_make(valid_uuid1, 1);
  mysql::gtids::Gtid gtid1_1_copy =
      mysql::gtids::Gtid::throwing_make(valid_uuid1, 1);

  ReaderStateCodecTest() = default;

  void SetUp() override {
    ASSERT_TRUE(
        mysql::strconv::decode_text(DEFAULT_UUID1, valid_uuid1).is_ok());
    ASSERT_TRUE(
        mysql::strconv::decode_text(DEFAULT_UUID2, valid_uuid2).is_ok());

    gtid1_1 = mysql::gtids::Gtid::throwing_make(valid_uuid1, 1);
    gtid1_2 = mysql::gtids::Gtid::throwing_make(valid_uuid1, 2);
    gtid2_1 = mysql::gtids::Gtid::throwing_make(valid_uuid2, 1);
    gtid1_1_copy = mysql::gtids::Gtid::throwing_make(valid_uuid1, 1);
  }

  void TearDown() override {}
};

MY_COMPILER_DIAGNOSTIC_PUSH()
// This tests a deprecated feature, so the deprecation warning is expected.
MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")

TEST_F(ReaderStateCodecTest, StateBasics) {
  state.add_gtid(gtid1_1);
  state.add_gtid(gtid1_2);
  state.add_gtid(gtid2_1);
  state.add_gtid(gtid1_1_copy);

  ASSERT_TRUE(mysql::sets::contains_element(state.get_gtids(), gtid1_1));
  ASSERT_TRUE(mysql::sets::contains_element(state.get_gtids(), gtid1_2));
  ASSERT_TRUE(mysql::sets::contains_element(state.get_gtids(), gtid2_1));

  // serialize to protobuf
  cs::reader::codec::pb::example::stringstream pb_ss;
  pb_ss << state;

  // de-serialize from protobuf
  cs::reader::State state_copy;
  pb_ss >> state_copy;

  ASSERT_TRUE(mysql::sets::contains_element(state_copy.get_gtids(), gtid1_1));
  ASSERT_TRUE(mysql::sets::contains_element(state_copy.get_gtids(), gtid1_2));
  ASSERT_TRUE(mysql::sets::contains_element(state_copy.get_gtids(), gtid2_1));

  // compare string representation
  std::stringstream ss_state1;
  std::stringstream ss_state2;

  ss_state1 << state;
  ss_state2 << state_copy;

  ASSERT_EQ(ss_state1.str(), ss_state2.str());
}

MY_COMPILER_DIAGNOSTIC_POP()

}  // namespace cs::reader::unittests
