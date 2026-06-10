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
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "mysql/strconv/strconv.h"                // encode

namespace {

using namespace mysql;

void test_string(strconv::With_quotes with_quotes, std::string_view str,
                 std::string_view expected) {
  strconv::Escaped_format format{with_quotes};

  // Compute length
  ASSERT_EQ(strconv::compute_encoded_length(format, str), expected.size());

  // raw pointer-based API: start+end, non-null-terminated
  {
    char buf[1000];
    char *buf_end = buf + sizeof(buf);
    strconv::encode(format, strconv::out_str_fixed_nz(buf, buf_end), str);
    ASSERT_EQ(std::string_view(buf, buf_end), expected);
  }

  // raw pointer-based API: start+size, null-terminated
  {
    char buf[1000];
    size_t size = sizeof(buf) - 1;
    strconv::encode(format, strconv::out_str_fixed_z(buf, size), str);
    ASSERT_EQ(std::string_view(buf, size), expected);
  }

  // string-based API
  {
    std::string result;
    auto ret = strconv::encode(format, strconv::out_str_growable(result), str);
    ASSERT_OK(ret);
    ASSERT_EQ(result, expected);
  }

  // return-string API, non-throwing
  {
    auto result = strconv::encode(format, str);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result, expected);
  }

  // return-string API, throwing
  {
    auto result = strconv::throwing::encode(format, str);
    ASSERT_EQ(result, expected);
  }
}

/// Test the different API entries to escape and quote strings.
void test_quoted_and_escaped(std::string_view str,
                             const std::string_view escaped) {
  MY_SCOPED_TRACE(escaped);

  {
    MY_SCOPED_TRACE("quoted");

    test_string(strconv::With_quotes::no, str, escaped);
  }

  {
    MY_SCOPED_TRACE("escaped");

    std::string quoted{"\""};
    quoted.append(escaped);
    quoted.append("\"");

    test_string(strconv::With_quotes::yes, str, quoted);
  }
}

/// Test several different strings that need to be quoted.
TEST(LibsStringsEscape, Basic) {
  test_quoted_and_escaped("", "");
  test_quoted_and_escaped(std::string_view{"\0\0", 2}, "\\x00\\x00");
  test_quoted_and_escaped("foo bar", "foo bar");
  test_quoted_and_escaped("\"\\", "\\\"\\\\");
  test_quoted_and_escaped("\n\r\1", "\\n\\r\\x01");
  test_quoted_and_escaped("\xff\x80\x1f", "\\xff\\x80\\x1f");
}

}  // namespace
