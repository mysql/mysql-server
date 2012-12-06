/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "my_regex.h"

/*
  Thist is just a *very* basic test that things compile/link and execute.
  The test data is taken from the first few lines in regex/tests.
  For a full test suite, see regex/main.c which parses test input
  and tests expected sucess/failure with basic/extended regexps etc. etc.
 */

namespace my_regex_unittest {

const int NSUBS= 10;

class RegexTest : public ::testing::Test
{
protected:
  RegexTest()
  {
    memset(&re, 0, sizeof(re));
  }
  static void TearDownTestCase()
  {
    my_regex_end();
  }

  my_regmatch_t subs[NSUBS];
  my_regex_t re;
};

struct Re_test_data
{
  const char* pattern;                          // Column 1 in regex/tests.
  const int   cflags;                           // Column 2 in regex/tests.
  const char* input;                            // Column 3 in regex/tests.
};

Re_test_data basic_data[]= 
{
  { "a",      MY_REG_BASIC,    "a"   },
  { "abc",    MY_REG_BASIC,    "abc" },
  { "abc|de", MY_REG_EXTENDED, "abc" },
  { "a|b|c",  MY_REG_EXTENDED, "abc" },
  { NULL, 0, NULL }
};

TEST_F(RegexTest, BasicTest)
{
  for (int ix=0; basic_data[ix].pattern; ++ix)
  {
    EXPECT_EQ(0, my_regcomp(&re,
                            basic_data[ix].pattern,
                            basic_data[ix].cflags,
                            &my_charset_latin1));

    int err= my_regexec(&re, basic_data[ix].input, NSUBS, subs, 0);
    EXPECT_EQ(0, err)
      << "my_regexec returned " << err
      << " for pattern '" << basic_data[ix].pattern << "'"
      << " with input '" << basic_data[ix].input << "'";
    my_regfree(&re);
  }
}

}  // namespace
