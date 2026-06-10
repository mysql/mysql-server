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

#include <gtest/gtest.h>                          // TEST
#include <limits>                                 // numeric_limits
#include <sstream>                                // stringstream
#include <string>                                 // string
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "mysql/strconv/strconv.h"                // encode

namespace {

using namespace mysql;

// ==== compute_encoded_length_text, encode_text, encode_to_streamable ====

/// Test the different API entries to convert to string, which vary by the
/// string type (new std::string, raw pointer, null-terminated raw pointer,
/// stringstream, etc) and error handling (throw or return error).
TEST(LibsStringsToString, Basic) {
  // just compute the length
  { ASSERT_EQ(strconv::compute_encoded_length_text(123), 3); }

  // get the result as an std::optional<std::string>
  {
    auto ret = strconv::encode_text(123);
    ASSERT_TRUE(ret.has_value());
    // clang-tidy wrongly claims that we call std::optional::value without first
    // checking std::optional::has_value.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    ASSERT_EQ(ret.value(), "123");
  }

  // negative number
  {
    auto ret = strconv::encode_text(-123);
    ASSERT_TRUE(ret.has_value());
    // clang-tidy wrongly claims that we call std::optional::value without first
    // checking std::optional::has_value.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    ASSERT_EQ(ret.value(), "-123");
  }

  // get the result as an std::string, allowing exceptions
  { ASSERT_EQ(strconv::throwing::encode_text(123), "123"); }

  // overwrite an existing string with the result
  {
    std::string s;
    ASSERT_OK(strconv::encode_text(strconv::out_str_growable(s), 123));
    ASSERT_EQ(s, "123");
  }

  // write the result to an existing buffer, represented by start+end, not
  // null-terminated. `strconv::encode_text` returns void for fixed Output
  // String Wrappers.
  {
    char buf[] = "xxxx";
    char *buf_end = buf + sizeof(buf);
    ASSERT_VOID(
        strconv::encode_text(strconv::out_str_fixed_nz(buf, buf_end), 123));
    ASSERT_EQ(std::string_view(buf), "123x");
    ASSERT_EQ(buf_end - buf, 3);
  }

  // write the result to an existing buffer, represented by start+length,
  // null-terminated. `strconv::encode_text` returns void for fixed Output
  // String Wrappers.
  {
    char buf[] = "xxxx";
    size_t length = sizeof(buf) - 1;
    ASSERT_VOID(
        strconv::encode_text(strconv::out_str_fixed_z(buf, length), 123));
    ASSERT_EQ(std::string_view(buf), "123");
    ASSERT_EQ(length, 3);
  }

  // send the result to a stream
  {
    std::stringstream out;
    out << strconv::encode_to_streamable(strconv::Text_format{}, 123);
  }

  // concat, concat_text
  {
    ASSERT_EQ(strconv::throwing::concat(strconv::Text_format{}, "a", 1, "",
                                        std::string("b")),
              std::string_view("a1b"));
    ASSERT_EQ(strconv::throwing::concat_text("a", 1, "", std::string("b")),
              std::string_view("a1b"));
  }
}

}  // namespace
