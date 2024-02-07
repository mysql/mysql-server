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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <string.h>

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

    size_t uplen = my_caseup_str(cs, test_string);
    EXPECT_EQ(5, uplen);
    EXPECT_STREQ(test_string, output_string);

    EXPECT_EQ(0, my_strcasecmp(cs, test_string, output_string));

    size_t downlen = my_casedn_str(cs, test_string);
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

}  // namespace strings_misc_unittest
