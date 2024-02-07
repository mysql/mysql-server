/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
#include <limits.h>

#include "strings/m_ctype_internals.h"

/*

  ==== Purpose ====

  Test if my_strtoll10 overflows values above unsigned long long
  limit correctly.

  ==== Related Bugs and Worklogs ====

  BUG#16997513: MY_STRTOLL10 ACCEPTING OVERFLOWED UNSIGNED LONG LONG
                VALUES AS NORMAL ONES

  ==== Implementation ====

  Check if my_strtoll10 returns the larger unsigned long long and raise
  the overflow error when receiving a number like 18446744073709551915

*/
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/strings/my_strtoll10.h"

TEST(StringToULLTest, OverflowedNumber) {
  unsigned long long number;
  int error;
  const char *str = "18446744073709551915";
  number = my_strtoll10(str, nullptr, &error);
  EXPECT_EQ(number, ULLONG_MAX);
  EXPECT_EQ(error, MY_ERRNO_ERANGE);
}

TEST(StringToULLTest, MiscStrntoull10rndBugs) {
  int error = 0;
  const char *str;
  const char *endptr;
  unsigned long long number;

  str = "-18446744073709551615";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(0, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(LLONG_MIN, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);

  // At ret_too_big: check for (unsigned_flag && negative)
  str = "-18446744073709551616";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(0, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(LLONG_MIN, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);

  str = "-1e19";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(0, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(LLONG_MIN, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);

  // At ret_too_big: check for (unsigned_flag && negative)
  str = "-2e19";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(0, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(LLONG_MIN, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);

  str = "0.9223372036854775807";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);

  // (ull % d) * 2; overflowed to zero.
  str = "0.9223372036854775808";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);

  str = "1.2";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);

  // On seeing the second dot, we need to calculate 'shift' and divide by 10.
  str = "1.2.";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(1, number);
  EXPECT_EQ(0, error);

  str = "92233720368547758000";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(ULLONG_MAX, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(LLONG_MAX, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);

  // On seeing end-of-input, we still have to check for overflow.
  str = "92233720368547758000e+";
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), true, &endptr, &error);
  EXPECT_EQ(ULLONG_MAX, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
  number =
      my_strntoull10rnd_8bit(nullptr, str, strlen(str), false, &endptr, &error);
  EXPECT_EQ(LLONG_MAX, number);
  EXPECT_EQ(MY_ERRNO_ERANGE, error);
}
