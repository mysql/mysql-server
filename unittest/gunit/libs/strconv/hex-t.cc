// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                      // TEST
#include <limits>                             // numeric_limits
#include <sstream>                            // stringstream
#include <string>                             // string
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/strconv/strconv.h"            // encode

namespace {

using namespace mysql;

/// Test the different API entries to escape and quote strings.
void test_string(std::string_view str, const std::string_view hex) {
  MY_SCOPED_TRACE(hex);
  MY_SCOPED_TRACE(hex.size());
  MY_SCOPED_TRACE(str.size());

  // Length
  ASSERT_EQ(strconv::compute_encoded_length(strconv::Hex_format{}, str),
            hex.size());
  ASSERT_EQ(hex.size(), str.size() * 2);
  ASSERT_EQ(strconv::compute_decoded_length(
                strconv::Repeat::any() | strconv::Hex_format{}, hex),
            str.size());

  // Test
  ASSERT_TRUE(
      strconv::test_decode(strconv::Repeat::any() | strconv::Hex_format{}, hex)
          .is_ok());

  // encode
  ASSERT_EQ(strconv::throwing::encode(strconv::Hex_format{}, str), hex);
  auto opt_str = strconv::encode(strconv::Hex_format{}, str);
  ASSERT_TRUE(opt_str.has_value());
  // clang-tidy wrongly claims that we call std::optional::value without first
  // checking std::optional::has_value.
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  ASSERT_EQ(opt_str.value(), hex);

  // decode
  {
    std::string unhexed;
    ASSERT_TRUE(strconv::decode(strconv::Repeat::any() | strconv::Hex_format{},
                                hex, unhexed)
                    .is_ok());
    assert(unhexed == str);
    ASSERT_EQ(unhexed, str);
  }
  {
    char unhexed[100];
    std::size_t unhexed_len{sizeof(unhexed)};
    auto ret =
        strconv::decode(strconv::Repeat::any() | strconv::Hex_format{}, hex,
                        strconv::out_str_fixed_nz(unhexed, unhexed_len));
    ASSERT_TRUE(ret.is_ok());
    ASSERT_EQ(std::string_view(unhexed, unhexed_len), str);
  }
}

/// Test several different strings that need to be quoted.
TEST(LibsStringsHex, Basic) {
  test_string("", "");
  test_string("abc", "616263");
  test_string(std::string_view{"\0\0", 2}, "0000");
  test_string("\xff\xff", "ffff");
}

void test_error(const strconv::Is_repeat auto &repetitions,
                std::string_view hex, const std::string_view &message) {
  MY_SCOPED_TRACE(hex);
  auto parser = strconv::test_decode(repetitions | strconv::Hex_format{}, hex);
  ASSERT_EQ(strconv::throwing::encode_text(parser), message);
}

/// Test parse errors
TEST(LibsStringsHex, Errors) {
  test_error(strconv::Repeat::any(), "abcd 123",
             "Expected hex digit after 4 characters, marked by "
             "[HERE] in: \"abcd[HERE] 123\"");
  test_error(strconv::Repeat::at_least(1), "",
             "Expected at least two hex digits at the beginning of the string: "
             "\"\"");
  test_error(strconv::Repeat::at_most(2), "abcdef",
             "Expected end of string after 4 characters, marked by [HERE] in: "
             "\"abcd[HERE]ef\"");
}

}  // namespace
