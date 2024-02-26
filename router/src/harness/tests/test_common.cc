/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

#include "common.h"

TEST(TestCommon, truncate_string) {
  using mysql_harness::truncate_string;

  constexpr size_t kMinMaxLen = 3;  // max_len less than this skips truncation.

  // simple case
  {
    const std::string s = "1234567890";
    size_t len = s.size();
    EXPECT_EQ(truncate_string(s, len + 1), s);
    EXPECT_EQ(truncate_string(s, len + 0), s);
    EXPECT_EQ(truncate_string(s, len - 1), "123456...");
  }

  { EXPECT_EQ(truncate_string("123", kMinMaxLen - 1), "12"); }

  // string len = kMinMaxLen
  {
    const std::string s = "123456";
    size_t len = s.size();
    EXPECT_EQ(truncate_string(s, len + 1), s);
    EXPECT_EQ(truncate_string(s, len + 0), s);
    // testing with len-1 would trigger assertion, as shown in the previous test
  }

  // short string
  {
    const std::string s = "1";
    EXPECT_EQ(truncate_string(s, kMinMaxLen), s);
  }

  // empty string
  {
    const std::string s;
    EXPECT_EQ(truncate_string(s, kMinMaxLen), s);
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
