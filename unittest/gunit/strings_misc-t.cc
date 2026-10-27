/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <cstring>
#include <string>

#include "my_sys.h"
#include "mysql/strings/collations.h"
#include "mysql/strings/m_ctype.h"
#include "strings/collations_internal.h"

namespace strings_misc_unittest {

CHARSET_INFO *init_collation(const char *name) {
  return get_charset_by_name(name, MYF(0));
}

void test_caseup_casedn(const CHARSET_INFO *cs) {
  char input_string[] = "hello";
  char output_string[] = "HELLO";
  char test_string[10];

  if (my_charset_is_ascii_based(cs) && cs->caseup_multiply == 1 &&
      strcmp("binary", cs->m_coll_name) != 0) {
    // fprintf(stdout, "compatible %s\n", cs->m_coll_name);
    strncpy(test_string, input_string, sizeof(test_string));

    size_t const uplen = my_caseup_str(cs, test_string);
    EXPECT_EQ(5, uplen);
    EXPECT_STREQ(test_string, output_string);

    EXPECT_EQ(0, my_strcasecmp(cs, test_string, output_string));

    size_t const downlen = my_casedn_str(cs, test_string);
    EXPECT_EQ(5, downlen);
    EXPECT_STREQ(test_string, input_string);
  } else {
    // fprintf(stdout, "incompatible %s\n", cs->m_coll_name);
  }
}

TEST(StringsMiscTest, CaseCmpIterate) {
  // Load one collation to get everything going.
  init_collation("utf8mb4_0900_ai_ci");
  mysql::collation_internals::entry->iterate(
      [](const CHARSET_INFO *cs) { test_caseup_casedn(cs); });
}

TEST(StringsMiscTest, EscapeGb18030LeadBytes) {
  for (const char *collation :
       {"gb18030_chinese_ci", "gb18030_bin", "gb18030_unicode_520_ci"}) {
    SCOPED_TRACE(collation);
    const CHARSET_INFO *cs = init_collation(collation);
    ASSERT_NE(nullptr, cs);
    for (unsigned lead = 0x81; lead <= 0xfe; ++lead) {
      SCOPED_TRACE(lead);
      const char input[] = {
          static_cast<char>(lead), '\0', '\n', '\r', '\032', '\'', '"'};
      // Each special character must be tested immediately after the lead byte.
      const char escapes[] = {'0', 'n', 'r', 'Z', '\'', '"'};
      for (size_t i = 0; i < sizeof(escapes); ++i) {
        SCOPED_TRACE(i);
        const char from[] = {input[0], input[i + 1]};
        const char expected[] = {'\\', input[0], '\\', escapes[i], '\0'};
        char to[2 * sizeof(from) + 1];
        ASSERT_EQ(
            4, escape_string_for_mysql(cs, to, sizeof(to), from, sizeof(from)));
        EXPECT_EQ(0, memcmp(to, expected, sizeof(expected)));
      }
    }
  }
}

TEST(StringsMiscTest, EscapeGb18030Sequences) {
  const CHARSET_INFO *cs = init_collation("gb18030_chinese_ci");
  ASSERT_NE(nullptr, cs);
  const struct {
    const char *input;
    const char *expected;
  } cases[] = {
      {"", ""},
      {"ascii", "ascii"},
      {"\x81", "\\\x81"},
      {"\xfe", "\\\xfe"},
      {"\x80\xff", "\x80\xff"},
      {"\x81\x5c'", "\x81\x5c\\'"},
      {"\x81\x40\xfe\xfe", "\x81\x40\xfe\xfe"},
      {"\x81\x30\x81\x30'", "\x81\x30\x81\x30\\'"},
      {"\xfe\x39\xfe\x39", "\xfe\x39\xfe\x39"},
      {"\x81\x30", "\\\x81\x30"},
      {"\x81\x30\x81", "\\\x81\x30\\\x81"},
      {"\x81\x30\x81'", "\\\x81\x30\\\x81\\'"},
  };
  for (const auto &test : cases) {
    const size_t input_length = strlen(test.input);
    const size_t expected_length = strlen(test.expected);
    std::string to(2 * input_length + 1, '\0');
    EXPECT_EQ(expected_length,
              escape_string_for_mysql(cs, &to[0], to.size(), test.input,
                                      input_length));
    EXPECT_EQ(0, memcmp(to.data(), test.expected, expected_length + 1));
  }
}

TEST(StringsMiscTest, EscapeGb18030BufferBoundary) {
  const CHARSET_INFO *cs = init_collation("gb18030_chinese_ci");
  ASSERT_NE(nullptr, cs);
  const char input[] = "\x81'";
  char to[6];
  for (size_t capacity = 1; capacity <= 5; ++capacity) {
    SCOPED_TRACE(capacity);
    memset(to, 'X', sizeof(to));
    const size_t length =
        escape_string_for_mysql(cs, to, capacity, input, sizeof(input) - 1);
    if (capacity < 5) {
      EXPECT_EQ(static_cast<size_t>(-1), length);
      EXPECT_NE(nullptr, memchr(to, '\0', capacity));
    } else {
      EXPECT_EQ(4, length);
      EXPECT_EQ(0, memcmp(to, "\\\x81\\'", 5));
    }
    EXPECT_EQ('X', to[capacity]);
  }
}

}  // namespace strings_misc_unittest
