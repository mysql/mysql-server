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
#include <algorithm>                              // ranges::remove
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // test_cmp
#include "mysql/uuids/uuid.h"                     // Uuid

namespace {

// Set to true to enable console output. Use for debugging only.
constexpr bool verbose = false;

using namespace mysql::strconv;
using namespace mysql::uuids;

void test_uuid_lt(std::string_view s1, std::string_view s2) {
  MY_SCOPED_TRACE("s1=", s1, " s2=", s2);
  Uuid uuid1;
  Uuid uuid2;
  ASSERT_TRUE(decode(Text_format{}, s1, uuid1).is_ok());
  ASSERT_TRUE(decode(Text_format{}, s2, uuid2).is_ok());
  MY_SCOPED_TRACE("uuid1=", uuid1, " uuid2=", uuid2);
  mysql::debugging::test_cmp(uuid1, uuid2, -1);
}

void test_uuid_eq(Uuid uuid1, Uuid uuid2) {
  MY_SCOPED_TRACE("uuid1=", uuid1, " uuid2=", uuid2);
  mysql::debugging::test_cmp(uuid1, uuid2, 0);
}

void test_uuid_eq(std::string_view s1, std::string_view s2) {
  MY_SCOPED_TRACE("s1=", s1, " s2=", s2);
  Uuid uuid1;
  Uuid uuid2;
  ASSERT_TRUE(decode(Text_format{}, s1, uuid1).is_ok());
  ASSERT_TRUE(decode(Text_format{}, s2, uuid2).is_ok());
  test_uuid_eq(uuid1, uuid2);
}

/// Test the different API entries to escape and quote strings.
void test_uuid(std::string_view str) {
  MY_SCOPED_TRACE("str=", str);

  // Construct the braced form and the no-dash-form
  std::string braced_str("{");
  braced_str += str;
  braced_str += "}";
  std::string bare_str(str);
  auto to_erase = std::ranges::remove(bare_str.begin(), bare_str.end(), '-');
  bare_str.erase(to_erase.begin(), to_erase.end());

  if constexpr (verbose) {
    std::cout << "str=[" << str << "]\n";
    std::cout << "braced=[" << braced_str << "]\n";
    std::cout << "bare=[" << bare_str << "]\n";
  }

  // Test parsing all three text forms
  Uuid uuid1;
  ASSERT_TRUE(decode(Text_format{}, str, uuid1).is_ok());
  Uuid uuid2;
  ASSERT_TRUE(decode(Text_format{}, braced_str, uuid2).is_ok());
  Uuid uuid3;
  ASSERT_TRUE(decode(Text_format{}, bare_str, uuid3).is_ok());

  // Verify that the resulting Uuid is the same for all text forms
  // NOLINTBEGIN(readability-suspicious-call-argument) this is intentional
  test_uuid_eq(uuid1, uuid2);
  test_uuid_eq(uuid2, uuid3);
  test_uuid_eq(uuid3, uuid1);
  // NOLINTEND(readability-suspicious-call-argument)

  // Produce the text format back and verify it is the same as the original.
  auto text = throwing::encode_text(uuid1);
  ASSERT_EQ(text, str);

  // Convert to binary and back and verify we get the same Uuid again.
  auto binary = throwing::encode(Binary_format{}, uuid1);
  ASSERT_EQ(binary.size(), Uuid::byte_size);
  Uuid back;
  auto ret = decode(Binary_format{}, binary, back);
  ASSERT_TRUE(ret.is_ok()) << throwing::encode_text(ret);
  MY_SCOPED_TRACE("back=", back);
  test_uuid_eq(uuid1, back);
}

/// Test UUID parsing, formatting, and comparison.
TEST(LibsUuids, Basic) {
  test_uuid("00000000-0000-0000-0000-000000000000");
  test_uuid("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  test_uuid("63d02e6c-9555-4d16-a8c3-e9d7590103b0");
  test_uuid_lt("00000000-0000-0000-0000-000000000000",
               "00000000000000000000000000000001");
  test_uuid_lt("00000000-0000-0000-0000-000000000000",
               "63d02e6c-9555-4d16-a8c3-e9d7590103b0");
  test_uuid_eq("63d02e6c-9555-4d16-a8c3-e9d7590103b0",
               "{63d02e6c-9555-4d16-a8c3-e9d7590103b0}");
}

}  // namespace
