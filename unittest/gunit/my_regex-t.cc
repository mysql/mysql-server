/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved. 

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>

#include "extra/regex/my_regex.h"

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

/*
  Bug#20642505: HENRY SPENCER REGULAR EXPRESSIONS (REGEX) LIBRARY

  We have our own variant of the regex code that understands MySQL charsets.
  This test is hear to make sure that we never checkpoint or cherrypick from
  the upstream and end up with a version that isn't patched against a
  potential overflow.
*/
TEST_F(RegexTest, Bug20642505)
{
  my_regex_t  re;
  char       *pattern;
  int         err;
  size_t      len= 684 * 1024 * 1024;

  /*
    We're testing on 32-bit/32-bit only. We could test e.g. with
    64-bit size_t, 32-bit long (for 64-bit Windows and such), but
    then we'd have to allocate twice as much memory, and it's a
    bit heavy as it is.  (In 32/32, we exceed the size_t parameter
    to malloc() as new_ssize exceeds UINT32 / 4, whereas in 64/32,
    new_ssize would exceed LONG_MAX at UINT32 / 2.  (64/32 verified
    in debugger.)
  */
  if ((sizeof(size_t) > 4) || (sizeof(long) > 4))
    return;

  /* set up an empty C string as pattern as regcomp() will strlen() this */
  pattern= (char *) malloc(len);
  EXPECT_FALSE(pattern == NULL);
  memset(pattern, (int) ' ', len);
  pattern[len - 1]= '\0';

  err= my_regcomp(&re, pattern, MY_REG_BASIC,
                  &my_charset_latin1);

  my_regfree(&re);
  free(pattern);

  EXPECT_EQ(err, MY_REG_ESPACE)
    << "my_regcomp returned " << err
    << " instead of MY_REG_ESPACE (" << MY_REG_ESPACE << ")";
}

}  // namespace
