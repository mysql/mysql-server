/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "authentication.h"

// mysql_native_password

struct MySQLNativePasswordParam {
  std::string nonce;
  std::string password;

  std::vector<uint8_t> expected;
};

class MySQLNativePasswordTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<MySQLNativePasswordParam> {};

TEST_P(MySQLNativePasswordTest, scramble) {
  const std::string nonce(GetParam().nonce);
  const std::string password(GetParam().password);
  const std::vector<uint8_t> expected(GetParam().expected);

  EXPECT_EQ(nonce.size(), 20);
  EXPECT_THAT(expected.size(), ::testing::AnyOf(20, 0));

  auto res = MySQLNativePassword::scramble(nonce, password);
  ASSERT_TRUE(res);
  EXPECT_THAT(res->size(), ::testing::AnyOf(20, 0));
  EXPECT_EQ(*res, expected);
}

const MySQLNativePasswordParam mysql_native_password_params[] = {
    {"01234567890123456789", "123", {0xa1, 0x22, 0xab, 0x20, 0x96, 0x5c, 0xfe,
                                     0x1f, 0x2e, 0xe1, 0x56, 0x39, 0x5e, 0xe4,
                                     0xc1, 0xe6, 0x43, 0x78, 0xf6, 0x40}},
    {"01234567890123456789", "", {}},
};

INSTANTIATE_TEST_SUITE_P(Spec, MySQLNativePasswordTest,
                         ::testing::ValuesIn(mysql_native_password_params));

// clear_text_password

TEST(ClearTextPasswordConstants, name) {
  EXPECT_STREQ(ClearTextPassword::name, "mysql_clear_password");
}

struct ClearTextPasswordParam {
  std::string nonce;
  std::string password;

  std::vector<uint8_t> expected;
};

class ClearTextPasswordTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ClearTextPasswordParam> {};

TEST_P(ClearTextPasswordTest, scramble) {
  const std::string nonce(GetParam().nonce);
  const std::string password(GetParam().password);
  const std::vector<uint8_t> expected(GetParam().expected);

  EXPECT_EQ(nonce.size(), 20);

  auto res = ClearTextPassword::scramble(nonce, password);
  ASSERT_TRUE(res);

  EXPECT_EQ(*res, expected);
}

const ClearTextPasswordParam clear_text_password_params[] = {
    {"01234567890123456789", "123", {'1', '2', '3', '\0'}},
    {"01234567890123456789", "", {'\0'}},
};

INSTANTIATE_TEST_SUITE_P(Spec, ClearTextPasswordTest,
                         ::testing::ValuesIn(clear_text_password_params));

// caching_sha2_password

TEST(CachingSha2PasswordConstants, name) {
  EXPECT_STREQ(CachingSha2Password::name, "caching_sha2_password");
}

struct CachingSha2PasswordParam {
  std::string nonce;
  std::string password;

  std::vector<uint8_t> expected;
};

class CachingSha2PasswordTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<CachingSha2PasswordParam> {};

TEST_P(CachingSha2PasswordTest, scramble) {
  const std::string nonce(GetParam().nonce);
  const std::string password(GetParam().password);
  const std::vector<uint8_t> expected(GetParam().expected);

  EXPECT_EQ(nonce.size(), 20);
  EXPECT_THAT(expected.size(), ::testing::AnyOf(32, 0));

  auto res = CachingSha2Password::scramble(nonce, password);
  ASSERT_TRUE(res);
  EXPECT_THAT(res->size(), ::testing::AnyOf(32, 0));
  EXPECT_EQ(*res, expected);
}

const CachingSha2PasswordParam caching_sha2_password_params[] = {
    {"aaaaaaaaaaaaaaaaaaaa",
     "123",
     {0x61, 0xd0, 0x51, 0x7f, 0xba, 0x68, 0x81, 0x7f, 0xe6, 0xca, 0xf6,
      0x58, 0x7a, 0x3b, 0xf4, 0x76, 0xba, 0xfb, 0x2a, 0xf5, 0xdc, 0x3d,
      0x92, 0x17, 0x84, 0x0f, 0xb5, 0xe9, 0xc9, 0xef, 0x7c, 0x5f}},
    {"01234567890123456789", "", {}},
};

INSTANTIATE_TEST_SUITE_P(Spec, CachingSha2PasswordTest,
                         ::testing::ValuesIn(caching_sha2_password_params));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
