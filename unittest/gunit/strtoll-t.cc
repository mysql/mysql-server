/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
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
#include <m_string.h>
#include <my_sys.h>

TEST(StringToULLTest, OverflowedNumber)
{
  unsigned long long number;
  int error;
  const char * str= "18446744073709551915";
  number= my_strtoll10(str, 0, &error);
  EXPECT_EQ(number, ULLONG_MAX);
  EXPECT_EQ(error, MY_ERRNO_ERANGE);
}
