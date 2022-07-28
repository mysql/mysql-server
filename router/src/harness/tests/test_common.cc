/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common.h"

using ::testing::StrEq;

TEST(TestCommon, truncate_string) {
  // This test tests truncate_string() and truncate_string_r(). Since both
  // utilize the same backend, common functionality tests are ran only on
  // truncate_string().

  using mysql_harness::truncate_string;
  using mysql_harness::truncate_string_r;

  constexpr size_t kMinMaxLen = 6;  // max_len less than this triggers assertion

  // simple case
  {
    const std::string s = "1234567890";
    size_t len = s.size();
    EXPECT_TRUE(truncate_string(s, len + 1) == s);
    EXPECT_TRUE(truncate_string(s, len + 0) == s);
    EXPECT_THAT(truncate_string(s, len - 1), StrEq("123456..."));
  }

// max_len too short
// This test doesn't work in Windows or FreeBSD, because of how ASSERT_DEATH
// works It also fails on release version
#if !defined(_WIN32) && !defined(__FreeBSD__) && !defined(NDEBUG)
  { ASSERT_DEATH(truncate_string("123456", kMinMaxLen - 1), ""); }
#endif

  // string len = kMinMaxLen
  {
    const std::string s = "123456";
    size_t len = s.size();
    EXPECT_TRUE(truncate_string(s, len + 1) == s);
    EXPECT_TRUE(truncate_string(s, len + 0) == s);
    // testing with len-1 would trigger assertion, as shown in the previous test
  }

  // short string
  {
    const std::string s = "1";
    EXPECT_TRUE(truncate_string(s, kMinMaxLen) == s);
  }

  // empty string
  {
    const std::string s = "";
    EXPECT_TRUE(truncate_string(s, kMinMaxLen) == s);
  }

  // thread-safety test
  {
    const std::string &r1 = truncate_string("1234567890", 8);

    std::thread([]() {
      const std::string &r2 = truncate_string("abcdefghij", 8);
      EXPECT_THAT(r2, StrEq("abcde..."));
    }).join();

    // call to truncate_string() in another thread should not overwrite this
    // result
    EXPECT_THAT(r1, StrEq("12345..."));

    // but calling it in this thread will (this funcionality is not a
    // requirement, the test only demonstrates the weakness)
    truncate_string("blablabla", 8);
    EXPECT_THAT(r1, StrEq("blabl..."));
  }

  // re-entry test (using truncate_string_r() instead of truncate_string())
  {
    const std::string &r1 = truncate_string_r("1234567890", 8);

    std::thread([]() {
      const std::string &r2 = truncate_string_r("abcdefghij", 8);
      EXPECT_THAT(r2, StrEq("abcde..."));
    }).join();

    // call to truncate_string_r() in another thread should not overwrite this
    // result
    EXPECT_THAT(r1, StrEq("12345..."));

    // call to truncate_string_r() in this thread should not overwrite this
    // result
    truncate_string_r("blablabla", 8);
    EXPECT_THAT(r1, StrEq("12345..."));
  }
}

TEST(TestCommon, SerialComma) {
  using mysql_harness::serial_comma;

  auto expect_output = [](int count, const std::string &expect) {
    constexpr int primes[]{2, 3, 5, 7, 11};

    std::string res = "Primes are ";
    res += serial_comma(&primes[0], &primes[count]);
    EXPECT_EQ(res, "Primes are " + expect);
  };

  expect_output(1, "2");
  expect_output(2, "2 and 3");
  expect_output(3, "2, 3, and 5");
  expect_output(5, "2, 3, 5, 7, and 11");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
