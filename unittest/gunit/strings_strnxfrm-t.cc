/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "benchmark.h"
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

TEST(StrXfrmTest, SimpleUTF8Correctness)
{
  const char* src= "abc æøå 日本語";
  unsigned char buf[32];

  static const unsigned char full_answer_with_pad[32] = {
    0x00, 0x61, 0x00, 0x62, 0x00, 0x63,  // abc
    0x00, 0x20,  // space
    0x00, 0xe6, 0x00, 0xf8, 0x00, 0xe5,  // æøå
    0x00, 0x20,  // space
    0x65, 0xe5, 0x67, 0x2c, 0x8a, 0x9e,  // 日本語
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20  // space for padding
  };

  for (size_t maxlen= 0; maxlen < sizeof(buf); ++maxlen) {
    memset(buf, 0xff, sizeof(buf));
    my_strnxfrm(&my_charset_utf8_bin, buf, maxlen, reinterpret_cast<const uchar *>(src), strlen(src));
    EXPECT_EQ(0, memcmp(buf, full_answer_with_pad, maxlen))
      << "Wrong answer for maxlen " << maxlen;
  }
}

// Benchmark based on reduced test case in Bug #83247 / #24788778.
//
// Note: This benchmark does not exercise any real multibyte characters;
// it is mostly exercising padding. If we change the test string to contain
// e.g. Japanese characters, performance goes down by ~20%.
static void BM_SimpleUTF8Benchmark(size_t num_iterations)
{
  StopBenchmarkTiming();

  static constexpr int key_cols = 12;
  static constexpr int set_key_cols = 6;  // Only the first half is set.
  static constexpr int key_col_chars = 80;
  static constexpr int bytes_Per_char = 3;
  static constexpr int key_bytes = key_col_chars * bytes_Per_char;
  static constexpr int buffer_bytes = key_cols * key_bytes;

  unsigned char source[buffer_bytes];
  unsigned char dest[buffer_bytes];

  const char *content= "PolyFilla27773";
  const int len= strlen(content);

  for (int k= 0, offset= 0; k < set_key_cols; ++k, offset+= key_bytes)
  {
    memcpy(source + offset, content, len);
  }

  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    for (int k= 0, offset= 0; k < key_cols; ++k, offset+= key_bytes)
    {
      if (k < set_key_cols)
      {
        my_strnxfrm(&my_charset_utf8_bin, dest + offset, key_bytes, source + offset, len);
      }
      else
      {
        my_strnxfrm(&my_charset_utf8_bin, dest + offset, key_bytes, source + offset, 0);
      }
    }
  }
  StopBenchmarkTiming();
}
BENCHMARK(BM_SimpleUTF8Benchmark);

}  // namespace
