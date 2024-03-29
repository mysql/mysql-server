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
#include "auth_digest.h"

#include <gtest/gtest.h>

#include "hexify.h"
#include "mysql/harness/tls_context.h"

using mysql_harness::hexify;

TEST(AuthDigest, native_password_empty) {
  auto scramble_res = mysql_native_password_scramble<std::string>("", "");

  ASSERT_TRUE(scramble_res);
  EXPECT_EQ(hexify(*scramble_res), hexify(std::string_view("")));
}

TEST(AuthDigest, native_password) {
  auto scramble_res = mysql_native_password_scramble<std::string>(
      "01234567890123456789", "pass");

  ASSERT_TRUE(scramble_res);
  EXPECT_EQ(
      *scramble_res,
      std::string_view(
          "\xfc\xcf\xe5\x3a\x9f\x93\xe3\x84\x61\x18\x0b\xb3\x2c\xc4\xac\x9b"
          "\x10\xd0\xc5\xc5"));
}

// check that Ret can be a uint8-vector
TEST(AuthDigest, native_password_vector) {
  auto scramble_res = mysql_native_password_scramble<std::vector<uint8_t>>(
      "01234567890123456789", "pass");

  ASSERT_TRUE(scramble_res);
  EXPECT_EQ(*scramble_res,
            (std::vector<uint8_t>{0xfc, 0xcf, 0xe5, 0x3a, 0x9f, 0x93, 0xe3,
                                  0x84, 0x61, 0x18, 0x0b, 0xb3, 0x2c, 0xc4,
                                  0xac, 0x9b, 0x10, 0xd0, 0xc5, 0xc5}));
}

TEST(AuthDigest, caching_sha2_password) {
  auto scramble_res = caching_sha2_password_scramble<std::string>(
      "01234567890123456789", "pass");

  ASSERT_TRUE(scramble_res);
  EXPECT_EQ(
      *scramble_res,
      std::string_view(
          "\x76\x2e\xe9\xe3\x14\x50\x73\x8a\x2f\x64\xe4\xcf\x83\xa3\x20\xd0"
          "\xae\x9b\xc0\x6c\x58\x8d\x8d\xef\x1a\xb6\xe7\x68\xaa\x90\x78\xac"))
      << hexify(*scramble_res);
}

int main(int argc, char *argv[]) {
  TlsLibraryContext lib_ctx;

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
