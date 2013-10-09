/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Common tests for client/sql_string and sql/sql_string.
  TODO: Why do we have two versions of String?
 */


CHARSET_INFO *system_charset_info= NULL;

TEST(StringTest, EmptyString)
{
  String s;
  const uint32 len= 0;
  EXPECT_EQ(len, s.length());
  EXPECT_EQ(len, s.alloced_length());
}


TEST(StringTest, ShrinkString)
{
  const uint32 len= 3;
  char foo[len]= {'a', 'b', 0};
  String foos(foo, len, &my_charset_bin);
  foos.shrink(1);
  EXPECT_EQ(len, foos.length());
  EXPECT_STREQ("ab", foo);
}


TEST(StringDeathTest, AppendEmptyString)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  String tbl_name;
  const char db_name[]= "aaaaaaa";
  const char table_name[]= "";
  tbl_name.append(String(db_name, system_charset_info));
  tbl_name.append('.');
  tbl_name.append(String(table_name, system_charset_info));
  // We now have eight characters, c_ptr() is not safe.
#ifndef DBUG_OFF
  EXPECT_DEATH_IF_SUPPORTED(tbl_name.c_ptr(), ".*Alloced_length >= .*");
#endif
  EXPECT_STREQ("aaaaaaa.", tbl_name.c_ptr_safe());
}
