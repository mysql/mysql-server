/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <string>

#include "m_string.h"

namespace string_to_double_unittest {

TEST(StringToDoubleTest, Balloc) {
  // This is the original mtr test case:
  // concat(rpad(-75.51891750, 11283536, 0), "767521D9");
  std::string dec = "-75.5189175";
  std::string zeros(11283536 - dec.length(), '0');
  std::string hexdigs = "767521D9";

  std::string str = dec + zeros + hexdigs;
  const char *str_start = str.c_str();
  const char *str_end = str_start + str.length();
  int error = 0;
  double result = my_strtod(str_start, &str_end, &error);
  EXPECT_DOUBLE_EQ(result, -75.5189175);
  EXPECT_EQ(error, 0);
  EXPECT_EQ(*str_end, 'D');

  str = dec + zeros + "e100";
  str_start = str.c_str();
  str_end = str_start + str.length();
  result = my_strtod(str_start, &str_end, &error);
  EXPECT_DOUBLE_EQ(result, -7.55189175e+101);
}

TEST(StringToDoubleTest, ManyZeros) {
  std::string zeros(DBL_MAX_10_EXP, '0');
  std::string str = "0." + zeros + "12345";
  const char *str_start = str.c_str();
  const char *str_end = str_start + str.length();
  int error = 0;
  double result = my_strtod(str_start, &str_end, &error);
  EXPECT_EQ(result, 1.2345e-309);

  str = "0." + zeros + zeros + "12345";
  str_start = str.c_str();
  str_end = str_start + str.length();
  result = my_strtod(str_start, &str_end, &error);
  EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(StringToDoubleTest, ZerosAndOnes) {
  std::string zeros(DBL_MAX_10_EXP, '0');
  std::string str = "0.";
  for (int i = 0; i < 20; ++i) {
    str.append(zeros);
    str.append("1");
  }
  const char *str_start = str.c_str();
  const char *str_end = str_start + str.length();
  int error = 0;
  double result = my_strtod(str_start, &str_end, &error);
  EXPECT_EQ(result, 1.0e-309);
}

}  // namespace string_to_double_unittest
