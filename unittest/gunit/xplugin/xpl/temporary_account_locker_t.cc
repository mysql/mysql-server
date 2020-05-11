/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "plugin/x/src/temporary_account_locker.h"

#include "unittest/gunit/xplugin/xpl/assert_error_code.h"

namespace xpl {

namespace test {

namespace {

chrono::Date_time get_datetime(const std::string &date_time) {
  std::istringstream s{date_time};
  std::tm t = {};
  s >> std::get_time(&t, "%Y.%m.%d %H:%M");
  return s.fail() ? chrono::Date_time{}
                  : chrono::System_clock::from_time_t(std::mktime(&t));
}

#define HOURS(n) chrono::Hours(n)
#define DAYS(n) chrono::Hours(24 * n)
#define EMPTY_DATE chrono::Date_time()

const char *const USER = "user";
const char *const HOST = "localhost";
const auto TODAY = get_datetime("2020.05.06 13:45");
const bool PASSWD_PASS = true;
const bool PASSWD_FAIL = false;
const int64_t UNBOUNDED = -1;

#define ASSERT_ACCOUNT_CLEARED(user, host)           \
  {                                                  \
    const auto entry = locker.get_entry(user, host); \
    ASSERT_FALSE(entry);                             \
  }

#define ASSERT_ACCOUNT_TRACKED(user, host, count)    \
  {                                                  \
    const auto entry = locker.get_entry(user, host); \
    ASSERT_TRUE(entry);                              \
    ASSERT_EQ(count, entry->attempt_count);          \
    ASSERT_FALSE(entry->is_locked);                  \
    ASSERT_EQ(EMPTY_DATE, entry->lock_date);         \
  }

#define ASSERT_ACCOUNT_LOCKED(user, host, count, date) \
  {                                                    \
    auto entry = locker.get_entry(user, host);         \
    ASSERT_TRUE(entry);                                \
    ASSERT_EQ(count, entry->attempt_count);            \
    ASSERT_TRUE(entry->is_locked);                     \
    ASSERT_EQ(date, entry->lock_date);                 \
  }
}  // namespace

class Temporary_account_locker_test : public ::testing::Test {
 public:
  Temporary_account_locker locker;
};

TEST_F(Temporary_account_locker_test, check_passwd_pass_restiction_0_0) {
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    locker.check(USER, HOST, 0, 0, PASSWD_PASS, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_pass_restiction_1_0) {
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    locker.check(USER, HOST, 1, 0, PASSWD_PASS, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_pass_restiction_0_1) {
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    locker.check(USER, HOST, 0, 1, PASSWD_PASS, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_fail_restiction_0_0) {
  ASSERT_ERROR_CODE(ER_ACCESS_DENIED_ERROR,
                    locker.check(USER, HOST, 0, 0, PASSWD_FAIL, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_fail_restiction_1_0) {
  ASSERT_ERROR_CODE(ER_ACCESS_DENIED_ERROR,
                    locker.check(USER, HOST, 1, 0, PASSWD_FAIL, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_fail_restiction_0_1) {
  ASSERT_ERROR_CODE(ER_ACCESS_DENIED_ERROR,
                    locker.check(USER, HOST, 0, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_pass_restiction_1_1) {
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    locker.check(USER, HOST, 1, 1, PASSWD_PASS, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_passwd_fail_restiction_1_1) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

////////////////////

TEST_F(Temporary_account_locker_test,
       check_passwd_pass_restiction_1_UNBOUNDED) {
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_PASS, TODAY));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test,
       check_passwd_fail_restiction_1_UNBOUNDED) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

////////////////////

TEST_F(Temporary_account_locker_test, check_twice_restiction_1_1_lock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_PASS, TODAY + HOURS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

TEST_F(Temporary_account_locker_test, check_twice_restiction_1_1_unlock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(ER_X_SUCCESS, locker.check(USER, HOST, 1, 1, PASSWD_PASS,
                                               TODAY + DAYS(1)));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_twice_restiction_2_2_untrack) {
  ASSERT_ERROR_CODE(ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
                    locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(ER_X_SUCCESS, locker.check(USER, HOST, 2, 2, PASSWD_PASS,
                                               TODAY + DAYS(1)));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_three_times_restiction_1_1_lock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_PASS, TODAY + HOURS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_PASS, TODAY + HOURS(2)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_three_times_restiction_1_1_lock_again) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY + HOURS(12)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY + DAYS(1));
}

TEST_F(Temporary_account_locker_test, check_three_times_restiction_1_1_unlock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, 1, PASSWD_PASS, TODAY + HOURS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(ER_X_SUCCESS, locker.check(USER, HOST, 1, 1, PASSWD_PASS,
                                               TODAY + DAYS(1)));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

/////////////////////

TEST_F(Temporary_account_locker_test,
       check_three_times_restiction_2_2_paswd_fail_lock) {
  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY - DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_three_times_restiction_2_2_paswd_pass_lock) {
  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY - DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_PASS, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);
}

TEST_F(Temporary_account_locker_test, check_three_times_restiction_2_2_unlock) {
  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY - HOURS(12)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(ER_X_SUCCESS, locker.check(USER, HOST, 2, 2, PASSWD_PASS,
                                               TODAY + DAYS(2)));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

TEST_F(Temporary_account_locker_test, check_four_times_restiction_2_2_lock) {
  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY - DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_PASS, TODAY + HOURS(25)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_PASS, TODAY + HOURS(47)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_four_times_restiction_2_2_tracked_again) {
  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY - DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY + DAYS(2)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);
}

TEST_F(Temporary_account_locker_test, check_four_times_restiction_2_2_unlock) {
  ASSERT_ERROR_CODE(
      ER_ACCESS_DENIED_ERROR_WITH_PASSWORD,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY - DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_TRACKED(USER, HOST, 1);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 2, 2, PASSWD_FAIL, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 2, TODAY);

  ASSERT_ERROR_CODE(ER_X_SUCCESS, locker.check(USER, HOST, 2, 2, PASSWD_PASS,
                                               TODAY + DAYS(2)));
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);
}

//////////////////

TEST_F(Temporary_account_locker_test, clear_one) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check("ADAM", HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED("ADAM", HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check("BOB", HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(2, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED("BOB", HOST, 1, TODAY);

  locker.clear("ADAM", HOST);
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED("ADAM", HOST);

  locker.clear("BOB", HOST);
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED("BOB", HOST);
}

TEST_F(Temporary_account_locker_test, clear_all) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check("ADAM", HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED("ADAM", HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check("BOB", HOST, 1, 1, PASSWD_FAIL, TODAY));
  ASSERT_EQ(2, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED("BOB", HOST, 1, TODAY);

  locker.clear();
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED("ADAM", HOST);
  ASSERT_ACCOUNT_CLEARED("BOB", HOST);
}

//////////////

TEST_F(Temporary_account_locker_test, check_twice_restiction_1_unbounded_lock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY + DAYS(2)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_twice_restiction_1_unbounded_no_unlock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_PASS, TODAY + DAYS(2)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_three_times_restiction_1_unbounded_lock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY + DAYS(365)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_three_times_restiction_1_unbounded_no_unlock) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_PASS, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_PASS, TODAY + DAYS(365)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);
}

TEST_F(Temporary_account_locker_test,
       check_three_times_restiction_1_unbounded_cleared) {
  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_FAIL, TODAY));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  ASSERT_ERROR_CODE(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      locker.check(USER, HOST, 1, UNBOUNDED, PASSWD_PASS, TODAY + DAYS(1)));
  ASSERT_EQ(1, locker.storage_size());
  ASSERT_ACCOUNT_LOCKED(USER, HOST, 1, TODAY);

  locker.clear(USER, HOST);
  ASSERT_EQ(0, locker.storage_size());
  ASSERT_ACCOUNT_CLEARED(USER, HOST);

  ASSERT_ERROR_CODE(ER_X_SUCCESS, locker.check(USER, HOST, 1, UNBOUNDED,
                                               PASSWD_PASS, TODAY + DAYS(1)));
}

}  // namespace test
}  // namespace xpl
