/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

// there is a another base64.h in the server's code
#include "mysqlrouter/base64.h"

#include <stdexcept>
#include <tuple>

#include <gtest/gtest.h>

#include "helpers/router_test_helpers.h"  // EXPECT_THROW_LIKE

using vu8 = std::vector<uint8_t>;

using Base64TestParams = std::tuple<std::string,  // encoded
                                    vu8           // decoded
                                    >;

/**
 * @test ensure Base64 codec works
 */
class Base64Test : public ::testing::Test,
                   public ::testing::WithParamInterface<Base64TestParams> {};

TEST_P(Base64Test, decode) {
  EXPECT_EQ(std::get<1>(GetParam()), Base64::decode(std::get<0>(GetParam())));
}

TEST_P(Base64Test, encode) {
  EXPECT_EQ(std::get<0>(GetParam()), Base64::encode(std::get<1>(GetParam())));
}

// cleanup test-names to satisfy googletest's requirements
static std::string sanitise(const std::string &name) {
  std::string out{name};

  for (auto &c : out) {
    if (!isalnum(c)) {
      c = '_';
    }
  }

  return out;
}

INSTANTIATE_TEST_SUITE_P(
    Foo, Base64Test,
    ::testing::Values(std::make_tuple("", vu8{}),  // empty
                      std::make_tuple("Zg==", vu8{'f'}),
                      std::make_tuple("Zm8=", vu8{'f', 'o'}),
                      std::make_tuple("Zm9v", vu8{'f', 'o', 'o'}),
                      std::make_tuple("TWFu", vu8{'M', 'a', 'n'}),
                      std::make_tuple("Zm9vYg==", vu8{'f', 'o', 'o', 'b'}),
                      std::make_tuple("Zm9vYmE=", vu8{'f', 'o', 'o', 'b', 'a'}),
                      std::make_tuple("Zm9vYmFy",
                                      vu8{'f', 'o', 'o', 'b', 'a', 'r'}),
                      std::make_tuple("WWU=", vu8{'Y', 'e'})
                      // end
                      ),
    [](testing::TestParamInfo<Base64TestParams> param_info) {
      return sanitise(std::get<0>(param_info.param).empty()
                          ? "<empty>"
                          : std::get<0>(param_info.param));
    }

);

/**
 * @test ensure invalid cases throw
 *
 * @note encode() can't fail other than bad_alloc
 */
class Base64FailTest : public ::testing::Test,
                       public ::testing::WithParamInterface<
                           std::tuple<std::string,  // encoded
                                      std::string   // exception-msg
                                      >> {};

TEST_P(Base64FailTest, decode) {
  EXPECT_THROW_LIKE(Base64::decode(std::get<0>(GetParam())), std::runtime_error,
                    std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    Foo, Base64FailTest,
    ::testing::Values(std::make_tuple("Z", "invalid sequence"),
                      std::make_tuple("Zg", "missing padding"),
                      std::make_tuple("Zg=", "missing padding"),
                      std::make_tuple("Zg=Z", "invalid char, expected padding"),
                      std::make_tuple("Z===", "invalid char"),
                      std::make_tuple("=", "invalid sequence"),
                      std::make_tuple("==", "missing padding"),
                      std::make_tuple("===", "missing padding"),
                      std::make_tuple("====", "invalid char"),
                      std::make_tuple("\x01\x02==", "invalid char"),
                      std::make_tuple("WWW=", "unused bits")  //
                      //
                      ));

/**
 * @test ensure Crypt Big Endian works
 */
class Radix64CryptBETest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<std::string,  // encoded
                                                      vu8           // decoded
                                                      >> {};

TEST_P(Radix64CryptBETest, decode) {
  EXPECT_EQ(std::get<1>(GetParam()),
            Radix64CryptBE::decode(std::get<0>(GetParam())));
}

TEST_P(Radix64CryptBETest, encode) {
  EXPECT_EQ(std::get<0>(GetParam()),
            Radix64CryptBE::encode(std::get<1>(GetParam())));
}

// valid cases
INSTANTIATE_TEST_SUITE_P(
    Foo, Radix64CryptBETest,
    ::testing::Values(std::make_tuple("", vu8{}),
                      std::make_tuple("JE", vu8{0x55}),
                      std::make_tuple("JOc", vu8{0x55, 0xaa}),
                      std::make_tuple("JOdJ", vu8{0x55, 0xaa, 0x55})));

/**
 * @test ensure Crypt Little Endian works
 */
class Radix64CryptLETest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<std::string,  // encoded
                                                      vu8           // decoded
                                                      >> {};

TEST_P(Radix64CryptLETest, decode) {
  EXPECT_EQ(std::get<1>(GetParam()),
            Radix64Crypt::decode(std::get<0>(GetParam())));
}

TEST_P(Radix64CryptLETest, encode) {
  EXPECT_EQ(std::get<0>(GetParam()),
            Radix64Crypt::encode(std::get<1>(GetParam())));
}

// valid cases
INSTANTIATE_TEST_SUITE_P(
    Foo, Radix64CryptLETest,
    ::testing::Values(std::make_tuple("", vu8{}),
                      std::make_tuple("J/", vu8{0x55}),
                      std::make_tuple("Jd8", vu8{0x55, 0xaa}),
                      std::make_tuple("JdOJ", vu8{0x55, 0xaa, 0x55})));

/**
 * @test ensure Traditional Uuencode works
 */
class Radix64UuencodeTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<std::string,  // encoded
                                                      vu8           // decoded
                                                      >> {};

TEST_P(Radix64UuencodeTest, decode) {
  EXPECT_EQ(std::get<1>(GetParam()),
            Radix64Uuencode::decode(std::get<0>(GetParam())));
}

TEST_P(Radix64UuencodeTest, encode) {
  EXPECT_EQ(std::get<0>(GetParam()),
            Radix64Uuencode::encode(std::get<1>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    Foo, Radix64UuencodeTest,
    ::testing::Values(std::make_tuple("", vu8{}),
                      std::make_tuple("0P``", vu8{'C'}),
                      std::make_tuple("0V$`", vu8{'C', 'a'}),
                      std::make_tuple("0V%T", vu8{'C', 'a', 't'}),
                      std::make_tuple("0V%T30``", vu8{'C', 'a', 't', 'M'})));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
