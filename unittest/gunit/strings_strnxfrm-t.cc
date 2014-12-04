/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Bug#16403708 SUBOPTIMAL CODE IN MY_STRNXFRM_SIMPLE()
  Bug#68476    Suboptimal code in my_strnxfrm_simple()

  Below we test some alternative implementations for my_strnxfrm_simple.
  In order to do benchmarking, configure in optimized mode, and
  generate a separate executable for this file:
    cmake -DMERGE_UNITTESTS=0
  You may want to tweak some constants below:
   - experiment with num_iterations
  run './strings_strnxfrm-t --disable-tap-output'
    to see timing reports for your platform.


  Benchmarking with gcc and clang indicates that:

  There is insignificant difference between my_strnxfrm_simple and strnxfrm_new
  when src != dst

  my_strnxfrm_simple() is significantly faster than strnxfrm_new
  when src == dst, especially for long strings.

  Loop unrolling gives significant speedup for large strings.
 */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include <vector>

#include "strnxfrm.h"

namespace strnxfrm_unittest {

#if defined(GTEST_HAS_PARAM_TEST)

#if !defined(DBUG_OFF)
// There is no point in benchmarking anything in debug mode.
const size_t num_iterations= 1ULL;
#else
// Set this so that each test case takes a few seconds.
// And set it back to a small value before pushing!!
// const size_t num_iterations= 20000000ULL;
const size_t num_iterations= 2ULL;
#endif

class StrnxfrmTest : public ::testing::TestWithParam<size_t>
{
protected:
  virtual void SetUp()
  {
    m_length= GetParam();
    m_src.assign(m_length, 0x20);
    m_dst.assign(m_length, 0x20);
  }
  std::vector<uchar> m_src;
  std::vector<uchar> m_dst;
  size_t m_length;
};

size_t test_values[]= {1, 10, 100, 1000};

INSTANTIATE_TEST_CASE_P(Strnxfrm, StrnxfrmTest,
                        ::testing::ValuesIn(test_values));

TEST_P(StrnxfrmTest, OriginalSrcDst)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_orig(&my_charset_latin1,
                  &m_dst[0], m_length, m_length,
                  &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, OriginalUnrolledSrcDst)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_orig_unrolled(&my_charset_latin1,
                           &m_dst[0], m_length, m_length,
                           &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, ModifiedSrcDst)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_new(&my_charset_latin1,
                 &m_dst[0], m_length, m_length,
                 &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, ModifiedUnrolledSrcDst)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_new_unrolled(&my_charset_latin1,
                          &m_dst[0], m_length, m_length,
                          &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, OriginalSrcSrc)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_orig(&my_charset_latin1,
                  &m_src[0], m_length, m_length,
                  &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, OriginalUnrolledSrcSrc)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_orig_unrolled(&my_charset_latin1,
                           &m_src[0], m_length, m_length,
                           &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, ModifiedSrcSrc)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_new(&my_charset_latin1,
                 &m_src[0], m_length, m_length,
                 &m_src[0], m_length, 192);
}

TEST_P(StrnxfrmTest, ModifiedUnrolledSrcSrc)
{
  for (size_t ix= 0; ix < num_iterations; ++ix)
    strnxfrm_new_unrolled(&my_charset_latin1,
                          &m_src[0], m_length, m_length,
                          &m_src[0], m_length, 192);
}

#endif  // GTEST_HAS_PARAM_TEST

}
