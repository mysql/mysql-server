/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "sql/auth/auth_common.h"
#include "sql/mysqld.h"

namespace wild_case_compare_unittest {

class WildCaseCompareTest : public ::testing::Test
{
protected:
  WildCaseCompareTest()
  {
  }
  static void TearDownTestCase()
  {
  }
};

TEST_F(WildCaseCompareTest, BasicTest)
{
  EXPECT_EQ(0, wild_case_compare(system_charset_info, "db1", "db%"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info, "db1", "db_"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "db1aaaa", "db_"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info, "db02aaaa", "db__aaaa"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "db02aaaa", "db_aaaa"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info, "db02aaaa", "db%aaaa"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "db02aaaa", "db%aaab"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info,
            "Com_alter_user",
            "%users_lost%"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info,
            "Performance_schema_users_lost",
            "%users_lost%"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info,
            "aaaa_users_lost_aaaa",
            "%users_lost%"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info,
            "aaaa_users_lost_aaaa",
            ""));
  EXPECT_EQ(0, wild_case_compare(system_charset_info,
            "aaaa",
            "%%%%"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info,
            "\\_\\_\\_",
            "_\\_\\_"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info,
            "___",
            "_\\_\\_"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info,
            "___",
            "___"));
  EXPECT_EQ(0, wild_case_compare(system_charset_info, "", "%"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", ""));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "_"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "db%"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "%db"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "_db"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "db_"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "db_aaaa"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "db%aaaa"));
  EXPECT_EQ(1, wild_case_compare(system_charset_info, "", "db%aa_aa"));

}
}

