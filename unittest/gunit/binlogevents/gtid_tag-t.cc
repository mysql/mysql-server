/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "mysql/gtid/gtid.h"
#include "mysql/gtid/gtidset.h"
#include "mysql/gtid/tag.h"
#include "mysql/gtid/tag_plain.h"

namespace mysql::gtid::unittests {

static auto tags_valid = {"a2345678901234567890123456789012",
                          "aDmiN123",
                          "aDmiN123_",
                          "aDmiN123                           ",
                          "    aDmiN123",
                          "",
                          "_aDmiN123"};

static auto tags_results = {"a2345678901234567890123456789012",
                            "admin123",
                            "admin123_",
                            "admin123",
                            "admin123",
                            "",
                            "_admin123"};

static auto tags_invalid = {"12345678901234567890123456789012",
                            "0DmiN123",
                            "a23456789012345678901234567890120",
                            "aDmiN123.",
                            "aDmiN1-23",
                            "aDmiN123                        a",
                            "    aDmiN 123 "};

TEST(GtidTag, Simple) {
  auto result_it = tags_results.begin();
  for (const auto &tag_cstr : tags_valid) {
    Tag current_tag(tag_cstr);  // assertion should pass
    Tag_plain tag_plain(current_tag);
    Tag converted_back_tag(tag_plain);
    std::string arg(tag_cstr);
    auto result_size = current_tag.from_string(arg);
    ASSERT_EQ(result_size, arg.length());
    ASSERT_EQ(current_tag.to_string(), *result_it);
    ASSERT_EQ(current_tag.to_string(), converted_back_tag.to_string());
    ASSERT_EQ(current_tag == converted_back_tag, true);
    ASSERT_EQ(current_tag != converted_back_tag, false);
    ASSERT_EQ(current_tag, Tag(*result_it));
    ++result_it;
  }
  for (const auto &tag_str : tags_invalid) {
    Tag current_tag;
    auto result_size = current_tag.from_string(tag_str);
    ASSERT_EQ(result_size, 0);
  }
  static_assert(std::is_trivially_copyable<Tag_plain>::value);
  static_assert(std::is_standard_layout<Tag_plain>::value);
}

}  // namespace mysql::gtid::unittests
