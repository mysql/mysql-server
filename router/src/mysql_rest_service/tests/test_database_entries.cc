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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "helper/make_shared_ptr.h"
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

TEST(UniversalID, less) {
  ASSERT_TRUE(k_id0 < k_id1);
  ASSERT_TRUE(k_id1 < k_id2);
  ASSERT_TRUE(k_id2 < k_id3);
  ASSERT_TRUE(k_id3 < k_id4);
  ASSERT_TRUE(k_id4 < k_id5);
}

TEST(UniversalID, greater) {
  ASSERT_FALSE(k_id1 < k_id0);
  ASSERT_FALSE(k_id2 < k_id1);
  ASSERT_FALSE(k_id3 < k_id2);
  ASSERT_FALSE(k_id4 < k_id3);
  ASSERT_FALSE(k_id5 < k_id4);
}
