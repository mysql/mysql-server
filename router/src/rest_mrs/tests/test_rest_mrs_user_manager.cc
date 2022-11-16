/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mock/mock_session.h"
#include "mrs/users/user_manager.h"

#include "mysql/harness/logging/logging.h"

using namespace testing;
using mrs::users::UserManager;
using AuthUser = UserManager::AuthUser;
using SqlSessionCache = UserManager::SqlSessionCache;
using mysqlrouter::MySQLSession;
using RowProcessor = MySQLSession::RowProcessor;
using Row = MySQLSession::Row;
using FieldValidator = MySQLSession::FieldValidator;

class UserManagerFixture : public Test {
 public:
  struct UserDatabase {
    UserDatabase(const Row &u, const std::vector<Row> &p,
                 const std::vector<Row> &g = {})
        : user{u}, privileges{p}, groups{g} {}
    Row user;
    std::vector<Row> privileges;
    std::vector<Row> groups;
  };

  void SetUp() override {
    mysql_harness::logging::log_debug(
        "Test-Case: %s", UnitTest::GetInstance()->current_test_case()->name());
  }

  AuthUser get_user_from_row(const Row &u, bool set_id = true) {
    AuthUser result;
    result.has_user_id = set_id;
    if (set_id) result.user_id = atol(u[0]);

    result.app_id = atol(u[1]);
    result.name = u[2];
    result.email = u[3];
    result.vendor_user_id = u[4];
    result.login_permitted = atoi(u[5]);

    return result;
  }

  void expect_query_user(const UserDatabase &u) {
    InSequence sequence;
    std::string query_user{
        "SELECT id, auth_app_id, name, email, vendor_user_id, "
        "login_permitted FROM mysql_rest_service_metadata.auth_user "
        "WHERE `auth_app_id`=2 and vendor_user_id='"};
    query_user.append(u.user[4]).append("' ");
    EXPECT_CALL(session_, query(StrEq(query_user), _, _))
        .WillOnce(
            Invoke([u](Unused, const RowProcessor &rp, Unused) { rp(u.user); }))
        .RetiresOnSaturation();

    std::string query_user_privileges{
        "SELECT p.service_id, p.db_schema_id, p.db_object_id, "
        "BIT_OR\\(p.crud_operations\\) as crud FROM.* auth_user_id="};
    query_user_privileges.append(u.user[0]).append("\\)");

    EXPECT_CALL(session_, query(ContainsRegex(query_user_privileges), _, _))
        .WillOnce(Invoke([u](Unused, const RowProcessor &rp, Unused) {
          for (auto &p : u.privileges) rp(p);
        }))
        .RetiresOnSaturation();

    std::string query_user_groups{
        "SELECT user_group_id FROM mysql_rest_service_metadata.user_has_group "
        "WHERE auth_user_id="};
    query_user_groups.append(u.user[0]);

    EXPECT_CALL(session_, query(ContainsRegex(query_user_groups), _, _))
        .WillOnce(Invoke([u](Unused, const RowProcessor &rp, Unused) {
          for (auto &p : u.groups) rp(p);
        }))
        .RetiresOnSaturation();
  }

  StrictMock<MockMySQLSession> session_;

  const uint64_t k_user_4000040400004_id = 4;
  const Row k_row_for_user_4000040400004{
      "4", "2", "John Doe", "john_doe@doe.com", "4000040400004", "1"};
  const std::vector<Row> k_row_for_user_4000040400004_privs{
      Row{"1", nullptr, nullptr, "2"}};
  const AuthUser k_user_4000040400004{
      get_user_from_row(k_row_for_user_4000040400004, false)};
};

TEST_F(UserManagerFixture, fetch_user_from_database) {
  const int k_app_id = 2;
  SqlSessionCache cache{nullptr, &session_};
  UserManager um{false, 3};

  AuthUser user;
  user.app_id = k_app_id;
  user.email = "john_doe@doe.com";
  user.login_permitted = true;
  user.name = "John Doe";
  user.vendor_user_id = "4000040400004";

  expect_query_user(
      {k_row_for_user_4000040400004, k_row_for_user_4000040400004_privs});

  ASSERT_TRUE(um.user_get(&user, &cache));

  ASSERT_TRUE(user.has_user_id);
  ASSERT_EQ(k_user_4000040400004_id, user.user_id);

  ASSERT_EQ(1, user.privileges.size());
  ASSERT_EQ(1, user.privileges[0].service_id);
  ASSERT_EQ(2, user.privileges[0].crud);
}

TEST_F(UserManagerFixture, fetch_user_from_database_once) {
  const int k_app_id = 2;
  SqlSessionCache cache{nullptr, &session_};
  UserManager um{false, 3};

  AuthUser user1;
  user1.app_id = k_app_id;
  user1.email = "john_doe@doe.com";
  user1.login_permitted = true;
  user1.name = "John Doe";
  user1.vendor_user_id = "4000040400004";

  expect_query_user(
      {k_row_for_user_4000040400004, k_row_for_user_4000040400004_privs});

  // First call, UserManager is going to cache data returned from database.
  ASSERT_TRUE(um.user_get(&user1, &cache));

  Mock::VerifyAndClearExpectations(&session_);

  ASSERT_TRUE(user1.has_user_id);
  ASSERT_EQ(k_user_4000040400004_id, user1.user_id);
  ASSERT_EQ(1, user1.privileges.size());
  ASSERT_EQ(1, user1.privileges[0].service_id);
  ASSERT_EQ(2, user1.privileges[0].crud);

  AuthUser user2;
  user2.app_id = k_app_id;
  user2.email = "john_doe@doe.com";
  user2.login_permitted = true;
  user2.name = "John Doe";
  user2.vendor_user_id = "4000040400004";

  // Second call is going to use local cache, the data won't be fetched from
  // database.
  ASSERT_TRUE(um.user_get(&user2, &cache));

  ASSERT_TRUE(user2.has_user_id);
  ASSERT_EQ(k_user_4000040400004_id, user2.user_id);
  ASSERT_EQ(1, user2.privileges.size());
  ASSERT_EQ(1, user2.privileges[0].service_id);
  ASSERT_EQ(2, user2.privileges[0].crud);
}

/**
 * After fetching, the code sees that data provided by remote are
 * different than the router stored in database.
 *
 * In this case router needs to update the database entry.
 */
TEST_F(UserManagerFixture, fetch_user_from_db_and_update) {
  const int k_app_id = 2;
  SqlSessionCache cache{nullptr, &session_};
  UserManager um{false, 3};

  // The user has different mail, than in representation in DB.
  AuthUser user;
  user.app_id = k_app_id;
  user.email = "new_john_doe@doe.com";
  user.login_permitted = true;
  user.name = "John Doe";
  user.vendor_user_id = "4000040400004";

  expect_query_user(
      {k_row_for_user_4000040400004, k_row_for_user_4000040400004_privs});

  EXPECT_CALL(session_,
              query(StrEq("UPDATE mysql_rest_service_metadata.auth_user SET "
                          "auth_app_id=2,name='John Doe', "
                          "email='new_john_doe@doe.com', "
                          "vendor_user_id='4000040400004', "
                          "login_permitted=1 WHERE id=4"),
                    _, _));

  ASSERT_TRUE(um.user_get(&user, &cache));

  ASSERT_TRUE(user.has_user_id);
  ASSERT_EQ(k_user_4000040400004_id, user.user_id);
  ASSERT_EQ(1, user.privileges.size());
  ASSERT_EQ(1, user.privileges[0].service_id);
  ASSERT_EQ(2, user.privileges[0].crud);
}
