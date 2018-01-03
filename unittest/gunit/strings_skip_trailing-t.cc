/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  Bug#68477    Suboptimal code in skip_trailing_space()
  Bug#16395778 SUBOPTIMAL CODE IN SKIP_TRAILING_SPACE()

  Below we test some alternative implementations for skip_trailing_space.
  In order to do benchmarking, configure in optimized mode, and
  generate a separate executable for this file:
    cmake -DMERGE_UNITTESTS=0
  You may want to tweak some constants below:
   - experiment with num_iterations
   - experiment with inserting something in front of the whitespace
   - experiment with different test_values
  run 'strings-t --disable-tap-output' to see timing reports for your platform.
 */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <string>

#include "unittest/gunit/skip_trailing.h"

namespace skip_trailing_space_unittest {

#if defined(GTEST_HAS_PARAM_TEST)

#if !defined(DBUG_OFF)
// There is no point in benchmarking anything in debug mode.
const size_t num_iterations= 1ULL;
#else
// Set this so that each test case takes a few seconds.
// And set it back to a small value before pushing!!
// const size_t num_iterations= 200000000ULL;
const size_t num_iterations= 2ULL;
#endif

class SkipTrailingSpaceTest : public ::testing::TestWithParam<int>
{
protected:
  virtual void SetUp()
  {
    int num_spaces= GetParam();
    // Insert something else (or nothing) here,
    //   to see effects of alignment of data:
    m_string.append("1");
    for (int ix= 0 ; ix < num_spaces; ++ix)
      m_string.append(" ");
    m_length= m_string.length();
  }
  size_t m_length;
  std::string m_string;
};

int test_values[]= {0, 24, 100, 150};

INSTANTIATE_TEST_CASE_P(Skip, SkipTrailingSpaceTest,
                        ::testing::ValuesIn(test_values));

TEST_P(SkipTrailingSpaceTest, Unaligned)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    skip_trailing_unalgn(reinterpret_cast<const uchar*>(m_string.c_str()),
                         m_length);
}

TEST_P(SkipTrailingSpaceTest, Original)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    skip_trailing_orig(reinterpret_cast<const uchar*>(m_string.c_str()),
                       m_length);
}

TEST_P(SkipTrailingSpaceTest, FourByte)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    skip_trailing_4byte(reinterpret_cast<const uchar*>(m_string.c_str()),
                        m_length);
}

TEST_P(SkipTrailingSpaceTest, EightByte)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    skip_trailing_8byte(reinterpret_cast<const uchar*>(m_string.c_str()),
                        m_length);
}

#endif  // GTEST_HAS_PARAM_TEST

}
