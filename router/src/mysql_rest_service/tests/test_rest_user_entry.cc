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

#include "mrs/database/entry/auth_user.h"

using namespace mrs::database::entry;
using namespace testing;

using UserId = AuthUser::UserId;

const char *const kUserVendorId = "123456789";
const UserId kUserId{{15, 0}};

class MrsDatabaseEntry : public Test {
 public:
  void SetUp() override {
    user.email = "test@test.com";
    user.name = "Tester Joe";
    user.user_id = kUserId;
    user.has_user_id = true;
    user.vendor_user_id = kUserVendorId;
  }

  AuthUser user;
};

TEST_F(MrsDatabaseEntry, auth_user_indexing_not_matching_empty_idx) {
  AuthUser::UserIndex idxNone;

  ASSERT_FALSE(idxNone == user);
}

TEST_F(MrsDatabaseEntry, auth_user_indexing_not_matching_other_id) {
  AuthUser::UserIndex idx{UserId{10, 0}};

  ASSERT_FALSE(idx == user);
}

TEST_F(MrsDatabaseEntry, auth_user_indexing_not_matching_other_vendor_id) {
  AuthUser::UserIndex idx{"1223211"};

  ASSERT_FALSE(idx == user);
}

TEST_F(MrsDatabaseEntry, auth_user_indexing_matching_on_vendor_id) {
  AuthUser::UserIndex idxVendor{kUserVendorId};

  ASSERT_EQ(idxVendor, user);
}

TEST_F(MrsDatabaseEntry, auth_user_indexing_matching_on_id) {
  AuthUser::UserIndex idxId{kUserId};

  ASSERT_EQ(idxId, user);
}

TEST_F(MrsDatabaseEntry, auth_user_indexing_matching_on_vendor_id_other_user) {
  AuthUser other_user;
  other_user.vendor_user_id = kUserVendorId;
  other_user.name = "Some other name";
  AuthUser::UserIndex idx{other_user};

  ASSERT_EQ(idx, user);
}

TEST_F(MrsDatabaseEntry, auth_user_indexing_matching_on_id_other_user) {
  AuthUser other_user;
  other_user.has_user_id = true;
  other_user.user_id = kUserId;
  other_user.name = "Some other name";
  AuthUser::UserIndex idx{other_user};

  ASSERT_EQ(idx, user);
}
