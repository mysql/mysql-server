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

#include "helper/cache/cache.h"
#include "helper/string/random.h"
#include "mrs/database/entry/auth_user.h"

using namespace helper::cache;
using namespace mrs::database::entry;
using namespace testing;

using UserIndex = AuthUser::UserIndex;
using UserId = AuthUser::UserId;
using Lru = helper::cache::policy::Lru;

const char *const kUserVendorId = "123456789";
const AuthUser::UserId kUserId{15, 0};

class MrsCacheUserData : public Test {
 public:
  void SetUp() override {
    user.email = "test@test.com";
    user.name = "Tester Joe";
    user.user_id = kUserId;
    user.has_user_id = true;
    user.vendor_user_id = kUserVendorId;
  }

  template <uint32_t noOfEntries>
  using UserCache = Cache<UserIndex, AuthUser, noOfEntries, Lru>;

  AuthUser create_other_user() {
    static uint64_t other_user_id = 10000;
    AuthUser u;

    u.has_user_id = true;
    u.user_id = UserId{{static_cast<uint8_t>(other_user_id++), 0}};
    u.name = helper::generate_string<10>();
    u.email =
        helper::generate_string<10>() + "@" + helper::generate_string<10>();
    u.login_permitted = rand() % 2;
    u.vendor_user_id = std::to_string(other_user_id + 42200000);

    return u;
  }

  template <typename Cache>
  AuthUser put_user(Cache &cache, const AuthUser &u) {
    cache.set(UserIndex(u), u);

    return u;
  }

  void assertAuthUser(const AuthUser &expected, const AuthUser *result) {
    ASSERT_NE(nullptr, result);
    ASSERT_TRUE(result->has_user_id);
    ASSERT_TRUE(expected.has_user_id);
    ASSERT_EQ(expected.user_id, result->user_id);
    ASSERT_EQ(expected.login_permitted, result->login_permitted);
    ASSERT_EQ(expected.email, result->email);
    ASSERT_EQ(expected.name, result->name);
    ASSERT_EQ(expected.vendor_user_id, result->vendor_user_id);
  }

  AuthUser user;
};

TEST_F(MrsCacheUserData, get_entry_by_vendor_id) {
  UserCache<1> cache;

  cache.set(UserIndex(user), user);
  ASSERT_NE(nullptr, cache.get_cached_value(UserIndex(kUserVendorId)));
}

TEST_F(MrsCacheUserData, get_entry_by_id) {
  UserCache<1> cache;

  cache.set(UserIndex(user), user);
  ASSERT_NE(nullptr, cache.get_cached_value(UserIndex(kUserId)));
}

TEST_F(MrsCacheUserData, multiple_entries_lru1) {
  UserCache<1> cache;

  put_user(cache, create_other_user());
  put_user(cache, create_other_user());
  put_user(cache, create_other_user());
  auto last_user = put_user(cache, create_other_user());
  auto cached_user =
      cache.get_cached_value(UserIndex(last_user.vendor_user_id));

  ASSERT_NO_FATAL_FAILURE(assertAuthUser(last_user, cached_user));

  auto &container = cache.get_container();
  ASSERT_EQ(1, container.size());
}

TEST_F(MrsCacheUserData, multiple_entries_lru3) {
  UserCache<3> cache;

  auto user_1 = put_user(cache, create_other_user());
  auto user_2 = put_user(cache, create_other_user());
  auto user_3 = put_user(cache, create_other_user());
  auto user_4 = put_user(cache, create_other_user());

  auto cached_user2 = cache.get_cached_value(UserIndex(user_2.vendor_user_id));
  auto cached_user3 = cache.get_cached_value(UserIndex(user_3.vendor_user_id));
  auto cached_user4 = cache.get_cached_value(UserIndex(user_4.vendor_user_id));

  ASSERT_NO_FATAL_FAILURE(assertAuthUser(user_2, cached_user2));
  ASSERT_NO_FATAL_FAILURE(assertAuthUser(user_3, cached_user3));
  ASSERT_NO_FATAL_FAILURE(assertAuthUser(user_4, cached_user4));

  auto &container = cache.get_container();
  ASSERT_EQ(3, container.size());
}

TEST_F(MrsCacheUserData, multiple_entries_lru3_intermediate_access) {
  UserCache<3> cache;

  auto user_1 = put_user(cache, create_other_user());
  auto user_2 = put_user(cache, create_other_user());
  auto user_3 = put_user(cache, create_other_user());
  // Move user1 to the cache head.
  cache.get_cached_value(UserIndex(user_1.vendor_user_id));
  auto user_4 = put_user(cache, create_other_user());

  auto cached_user1 = cache.get_cached_value(UserIndex(user_1.vendor_user_id));
  auto cached_user3 = cache.get_cached_value(UserIndex(user_3.vendor_user_id));
  auto cached_user4 = cache.get_cached_value(UserIndex(user_4.vendor_user_id));

  ASSERT_NO_FATAL_FAILURE(assertAuthUser(user_1, cached_user1));
  ASSERT_NO_FATAL_FAILURE(assertAuthUser(user_3, cached_user3));
  ASSERT_NO_FATAL_FAILURE(assertAuthUser(user_4, cached_user4));

  auto &container = cache.get_container();
  ASSERT_EQ(3, container.size());
}
