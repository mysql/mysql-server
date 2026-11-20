/* Copyright (c) 2026, Oracle and/or its affiliates.

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
#include <memory>

#include "my_sys.h"
#include "mysql/strings/m_ctype.h"

namespace strings_uca_strcasecmp_unittest {

// The legacy UCA handlers for these collations used to leave strcasecmp as
// nullptr, so my_strcasecmp() on them jumped to address 0.
const char *const kCollations[] = {
    "ucs2_unicode_ci",
    "utf16_unicode_ci",
    "utf32_unicode_ci",
    "gb18030_unicode_520_ci",
};

// Exact-size heap copy, so a sanitizer build flags any read past the
// terminator.
std::unique_ptr<char[]> heap_str(const char *s) {
  const size_t len = strlen(s) + 1;
  std::unique_ptr<char[]> buf(new char[len]);
  memcpy(buf.get(), s, len);
  return buf;
}

TEST(StringsUcaStrcasecmpTest, LegacyUcaHandlersHaveStrcasecmp) {
  for (const char *name : kCollations) {
    const CHARSET_INFO *cs = get_charset_by_name(name, MYF(0));
    ASSERT_NE(nullptr, cs) << name;
    ASSERT_NE(nullptr, cs->coll->strcasecmp) << name;

    EXPECT_EQ(0, my_strcasecmp(cs, "", "")) << name;
    EXPECT_EQ(0, my_strcasecmp(cs, "Hello", "hELLO")) << name;
    EXPECT_GT(0, my_strcasecmp(cs, "abc", "abd")) << name;
    EXPECT_LT(0, my_strcasecmp(cs, "abcd", "ABC")) << name;
  }
}

TEST(StringsUcaStrcasecmpTest, NonAsciiInputStaysInBounds) {
  // A lone byte >= 0x80 is where decoding with mb_wc() would run past the
  // terminator for ucs2, utf16 and utf32.
  for (const char *name : kCollations) {
    const CHARSET_INFO *cs = get_charset_by_name(name, MYF(0));
    ASSERT_NE(nullptr, cs) << name;

    auto lone = heap_str("\xC3");
    auto lone_copy = heap_str("\xC3");
    auto surrogate = heap_str("\xD8");
    auto ascii = heap_str("a");
    EXPECT_EQ(0, my_strcasecmp(cs, lone.get(), lone_copy.get())) << name;
    EXPECT_NE(0, my_strcasecmp(cs, surrogate.get(), ascii.get())) << name;
  }
}

}  // namespace strings_uca_strcasecmp_unittest
