/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "hexify.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using mysql_harness::hexify;

TEST(Hexify, from_array) {
  auto cont = std::array<char, 3>{{1, 2, 3}};

  EXPECT_EQ(hexify(cont),
            "01 02 03 .. .. .. .. .. .. .. .. .. .. .. .. ..  ...\n");
}

TEST(Hexify, from_string) {
  auto cont = std::string{1, 2, 3};

  EXPECT_EQ(hexify(cont),
            "01 02 03 .. .. .. .. .. .. .. .. .. .. .. .. ..  ...\n");
}

TEST(Hexify, from_string_view) {
  auto cont = std::string_view{"\x01\x02\x03"};

  EXPECT_EQ(hexify(cont),
            "01 02 03 .. .. .. .. .. .. .. .. .. .. .. .. ..  ...\n");
}

TEST(Hexify, from_vector) {
  auto cont = std::vector<uint8_t>{1, 2, 3};

  EXPECT_EQ(hexify(cont),
            "01 02 03 .. .. .. .. .. .. .. .. .. .. .. .. ..  ...\n");
}

TEST(Hexify, printable) {
  auto cont = std::string_view{"abc"};

  EXPECT_EQ(hexify(cont),
            "61 62 63 .. .. .. .. .. .. .. .. .. .. .. .. ..  abc\n");
}

TEST(Hexify, multiline) {
  auto cont = std::string_view{
      "0123456701234567"
      "0123456701234567"};

  EXPECT_EQ(
      hexify(cont),
      "30 31 32 33 34 35 36 37 30 31 32 33 34 35 36 37  0123456701234567\n"
      "30 31 32 33 34 35 36 37 30 31 32 33 34 35 36 37  0123456701234567\n");
}

TEST(Hexify, mostly_fullline) {
  auto cont = std::string_view{"012345670123456"};

  EXPECT_EQ(
      hexify(cont),
      "30 31 32 33 34 35 36 37 30 31 32 33 34 35 36 ..  012345670123456\n");
}

TEST(Hexify, fullline) {
  auto cont = std::string_view{"0123456701234567"};

  EXPECT_EQ(
      hexify(cont),
      "30 31 32 33 34 35 36 37 30 31 32 33 34 35 36 37  0123456701234567\n");
}

TEST(Hexify, fullline_plus_one) {
  auto cont = std::string_view{"01234567012345670"};

  EXPECT_EQ(
      hexify(cont),
      "30 31 32 33 34 35 36 37 30 31 32 33 34 35 36 37  0123456701234567\n"
      "30 .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..  0\n");
}

TEST(Hexify, empty) {
  auto cont = std::string_view{};

  EXPECT_EQ(hexify(cont), "");
}

TEST(Hexify, eight_bit) {
  auto cont = std::array<uint8_t, 3>{{0xf1, 0xf2, 0xf3}};

  EXPECT_THAT(hexify(cont),
              ::testing::AnyOf(
                  "f1 f2 f3 .. .. .. .. .. .. .. .. .. .. .. .. ..  ...\n",
                  "F1 F2 F3 .. .. .. .. .. .. .. .. .. .. .. .. ..  ...\n"));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
