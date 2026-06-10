/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "helper/string/hex.h"
#include "mrs/authentication/helper/universal_id_container.h"
#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/universal_id.h"
#include "mrs/database/query_rest_table.h"

#include "mock/mock_session.h"

using testing::Test;

using namespace mrs::database::entry;
using UserId = AuthUser::UserId;

TEST(AuthUser, to_string) {
  ASSERT_EQ("00000000000000000000000000000000", UserId().to_string());
  ASSERT_EQ("04000000000000000000000000000000", UserId({0x04}).to_string());
  ASSERT_EQ(
      "00000000000000000000000000000004",
      UserId({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x04}).to_string());
}

UniversalId k_id0{0};
UniversalId k_id1{1};
UniversalId k_id2{2};
UniversalId k_id3{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
UniversalId k_id4{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
UniversalId k_id5{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3};

TEST(UniversalID, equal) {
  ASSERT_EQ(k_id0, k_id0);
  ASSERT_EQ(k_id1, k_id1);
  ASSERT_EQ(k_id2, k_id2);
  ASSERT_EQ(k_id3, k_id3);
  ASSERT_EQ(k_id4, k_id4);
  ASSERT_EQ(k_id5, k_id5);
}

TEST(UniversalID, not_equal) {
  ASSERT_NE(k_id0, k_id1);
  ASSERT_NE(k_id1, k_id2);
  ASSERT_NE(k_id2, k_id3);
  ASSERT_NE(k_id3, k_id4);
  ASSERT_NE(k_id4, k_id5);

  ASSERT_NE(k_id1, k_id0);
  ASSERT_NE(k_id2, k_id1);
  ASSERT_NE(k_id3, k_id2);
  ASSERT_NE(k_id4, k_id3);
  ASSERT_NE(k_id5, k_id4);
}

TEST(UniversalID, less_equal) {
  EXPECT_LE(k_id0, k_id1);
  EXPECT_LE(k_id0, k_id0);

  EXPECT_LE(k_id1, k_id2);
  EXPECT_LE(k_id2, k_id3);
  EXPECT_LE(k_id3, k_id4);
  EXPECT_LE(k_id4, k_id5);
}

TEST(UniversalID, less) {
  EXPECT_LT(k_id0, k_id1);
  EXPECT_LT(k_id1, k_id2);
  EXPECT_LT(k_id2, k_id3);
  EXPECT_LT(k_id3, k_id4);
  EXPECT_LT(k_id4, k_id5);
}

TEST(UniversalID, greater) {
  EXPECT_GT(k_id1, k_id0);
  EXPECT_GT(k_id2, k_id1);
  EXPECT_GT(k_id3, k_id2);
  EXPECT_GT(k_id4, k_id3);
  EXPECT_GT(k_id5, k_id4);
}

TEST(UniversalID, greater_equal) {
  EXPECT_GE(k_id1, k_id0);
  EXPECT_GE(k_id2, k_id1);
  EXPECT_GE(k_id3, k_id2);
  EXPECT_GE(k_id4, k_id3);
  EXPECT_GE(k_id5, k_id4);
}

TEST(UniversalID, equal_to_string) {
  const auto str1 = k_id3.to_string();
  const auto data = helper::string::unhex<std::vector<uint8_t>>(str1);
  UniversalId from_txt_id = UniversalId::from_cstr(
      reinterpret_cast<const char *>(data.data()), data.size());

  ASSERT_EQ(k_id3, from_txt_id);
}

TEST(UniversalID, equal_uuid_converter) {
  using Conv = mrs::authentication::UniversalIdContainer;
  const auto str1 = k_id3.to_string();
  const auto result = helper::string::unhex<Conv>(str1);

  ASSERT_EQ(k_id3, result.get_user_id());
}
