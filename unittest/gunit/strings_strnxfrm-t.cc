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
#include "template_utils.h"

using std::to_string;

namespace strnxfrm_unittest {

namespace {

// Simply print out an array.
void print_array(const uchar *arr, size_t len)
{
  for (size_t i= 0; i < len; ++i)
  {
    fprintf(stderr, " %02x", arr[i]);
    if ((i % 8) == 7 || i == len - 1)
      fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}

// A function to compare two arrays and print them out in its entirety
// (for easier context) if they are not equal.
void expect_arrays_equal(const uchar *expected, const uchar *got, size_t len)
{
  int num_err= 0;
  for (size_t i= 0; i < len && num_err < 5; ++i)
  {
    EXPECT_EQ(expected[i], got[i]);
    if (expected[i] != got[i])
      ++num_err;
  }
  if (num_err)
  {
    fprintf(stderr, "Expected:\n");
    for (size_t i= 0; i < len; ++i)
    {
      fprintf(stderr, " %c%02x", expected[i] != got[i] ? '*' : ' ', expected[i]);
      if ((i % 8) == 7 || i == len - 1)
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\nGot:\n");
    for (size_t i= 0; i < len; ++i) {
      fprintf(stderr, " %c%02x", expected[i] != got[i] ? '*' : ' ', got[i]);
      if ((i % 8) == 7 || i == len - 1)
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
  }
}

}  // namespace

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

  for (size_t maxlen= 0; maxlen < sizeof(buf); maxlen += 2) {
    memset(buf, 0xff, sizeof(buf));
    my_strnxfrm(&my_charset_utf8_bin, buf, maxlen,
      pointer_cast<const uchar *>(src), strlen(src));
    expect_arrays_equal(full_answer_with_pad, buf, maxlen);
  }
}

TEST(StrXfrmTest, SimpleUTF8MB4Correctness)
{
  const char* src= "abc æøå 日本語";
  unsigned char buf[30];

  static const unsigned char full_answer_with_pad[30] = {
    0x1c, 0x47, 0x1c, 0x60, 0x1c, 0x7a,  // abc
    0x00, 0x01,  // space
    0x1c, 0x47, 0x1c, 0xaa, 0x1d, 0xdd, 0x1c, 0x47,  // æøå
    0x00, 0x01,  // space
    0xfb, 0x40, 0xe5, 0xe5, 0xfb, 0x40, 0xe7, 0x2c, 0xfb, 0x41, 0x8a, 0x9e,  // 日本語
  };

  for (size_t maxlen= 0; maxlen < sizeof(buf); maxlen += 2) {
    memset(buf, 0xff, sizeof(buf));
    my_strnxfrm(&my_charset_utf8mb4_0900_ai_ci, buf, maxlen,
      pointer_cast<const uchar *>(src), strlen(src));
    expect_arrays_equal(full_answer_with_pad, buf, maxlen);
  }
}

/*
  This and UTF8MB4PadCorrectness_2 together test an edge case where
  we run out of output bytes before we know whether we should strip
  spaces or not. (In _1, we should; in _2, we should not.)
*/
TEST(StrXfrmTest, UTF8MB4PadCorrectness_1)
{
  const char* src= "abc     ";
  unsigned char buf[22];

  static const unsigned char full_answer[22] = {
    0x1c, 0x47, 0x1c, 0x60, 0x1c, 0x7a,  // abc
    0x00, 0x00,  // Level separator.
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20,  // Accents for abc.
    0x00, 0x00,  // Level separator.
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02,  // Case for abc.
  };

  for (size_t maxlen= 0; maxlen < sizeof(buf); maxlen += 2) {
    SCOPED_TRACE("maxlen=" + to_string(maxlen) + "/" + to_string(sizeof(buf)));
    memset(buf, 0xff, sizeof(buf));
    my_strnxfrm(&my_charset_utf8mb4_0900_as_cs, buf, maxlen,
      pointer_cast<const uchar *>(src), strlen(src));
    expect_arrays_equal(full_answer, buf, maxlen);
  }
}

TEST(StrXfrmTest, UTF8MB4PadCorrectness_2)
{
  const char* src= "abc    a";
  unsigned char buf[52];

  static const unsigned char full_answer[52] = {
    0x1c, 0x47, 0x1c, 0x60, 0x1c, 0x7a,  // abc
    0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,  // Four spaces.
    0x1c, 0x47,  // a
    0x00, 0x00,  // Level separator.
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20,  // Accents for abc.
    0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,  // Accents for four spaces.
    0x00, 0x20,  // Accents for a.
    0x00, 0x00,  // Level separator.
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02,  // Case for abc.
    0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,  // Case for four spaces.
    0x00, 0x02,  // Case for a.
  };

  for (size_t maxlen= 0; maxlen < sizeof(buf); maxlen += 2) {
    SCOPED_TRACE("maxlen=" + to_string(maxlen) + "/" + to_string(sizeof(buf)));
    memset(buf, 0xff, sizeof(buf));
    my_strnxfrm(&my_charset_utf8mb4_0900_as_cs, buf, maxlen,
      pointer_cast<const uchar *>(src), strlen(src));
    expect_arrays_equal(full_answer, buf, maxlen);
  }
}

// Benchmark based on reduced test case in Bug #83247 / #24788778.
//
// Note: This benchmark does not exercise any real multibyte characters;
// it is mostly exercising padding. If we change the test string to contain
// e.g. Japanese characters, performance goes down by ~20%.
static void BM_SimpleUTF8(size_t num_iterations)
{
  StopBenchmarkTiming();

  static constexpr int key_cols = 12;
  static constexpr int set_key_cols = 6;  // Only the first half is set.
  static constexpr int key_col_chars = 80;
  static constexpr int bytes_per_char = 3;
  static constexpr int key_bytes = key_col_chars * bytes_per_char;
  static constexpr int buffer_bytes = key_cols * key_bytes;

  unsigned char source[buffer_bytes];
  unsigned char dest[buffer_bytes];

  const char *content= "PolyFilla27773";
  const int len= strlen(content);
  memset(source, 0, sizeof(source));

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
BENCHMARK(BM_SimpleUTF8);

// Verifies using my_charpos to find the length of a string.
// hp_hash.c does this extensively. Not really a strnxfrm benchmark,
// but belongs to the same optimization effort.
static void BM_UTF8MB4StringLength(size_t num_iterations)
{
  StopBenchmarkTiming();

  CHARSET_INFO *cs = &my_charset_utf8mb4_0900_ai_ci;

  // Some English text, then some Norwegian text, then some Japanese,
  // and then a few emoji (the last with skin tone modifiers).
  const char *content= "Premature optimization is the root of all evil. "
    "Våre norske tegn bør æres. 日本語が少しわかります。 ✌️🐶👩🏽";
  const int len= strlen(content);
  int tot_len= 0;

  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    tot_len+= my_charpos(cs, content, content + len, len / cs->mbmaxlen);
  }
  StopBenchmarkTiming();

  EXPECT_NE(0, tot_len);
}
BENCHMARK(BM_UTF8MB4StringLength);

// Benchmark testing the default recommended collation for 8.0, without
// stressing padding as much, but still testing only Latin letters.
static void BM_SimpleUTF8MB4(size_t num_iterations)
{
  StopBenchmarkTiming();

  const char *content= "This is a rather long string that contains only "
    "simple letters that are available in ASCII. This is a common special "
    "case that warrants a benchmark on its own, even if the character set "
    "and collation supports much more complicated scenarios.";
  const int len= strlen(content);

  // Just recorded from a trial run on the string above.
  static constexpr uchar expected[]= {
    0x1e, 0x95, 0x1d, 0x18, 0x1d, 0x32, 0x1e, 0x71,
    0x00, 0x01, 0x1d, 0x32, 0x1e, 0x71, 0x00, 0x01,
    0x1c, 0x47, 0x00, 0x01, 0x1e, 0x33, 0x1c, 0x47,
    0x1e, 0x95, 0x1d, 0x18, 0x1c, 0xaa, 0x1e, 0x33,
    0x00, 0x01, 0x1d, 0x77, 0x1d, 0xdd, 0x1d, 0xb9,
    0x1c, 0xf4, 0x00, 0x01, 0x1e, 0x71, 0x1e, 0x95,
    0x1e, 0x33, 0x1d, 0x32, 0x1d, 0xb9, 0x1c, 0xf4,
    0x00, 0x01, 0x1e, 0x95, 0x1d, 0x18, 0x1c, 0x47,
    0x1e, 0x95, 0x00, 0x01, 0x1c, 0x7a, 0x1d, 0xdd,
    0x1d, 0xb9, 0x1e, 0x95, 0x1c, 0x47, 0x1d, 0x32,
    0x1d, 0xb9, 0x1e, 0x71, 0x00, 0x01, 0x1d, 0xdd,
    0x1d, 0xb9, 0x1d, 0x77, 0x1f, 0x0b, 0x00, 0x01,
    0x1e, 0x71, 0x1d, 0x32, 0x1d, 0xaa, 0x1e, 0x0c,
    0x1d, 0x77, 0x1c, 0xaa, 0x00, 0x01, 0x1d, 0x77,
    0x1c, 0xaa, 0x1e, 0x95, 0x1e, 0x95, 0x1c, 0xaa,
    0x1e, 0x33, 0x1e, 0x71, 0x00, 0x01, 0x1e, 0x95,
    0x1d, 0x18, 0x1c, 0x47, 0x1e, 0x95, 0x00, 0x01,
    0x1c, 0x47, 0x1e, 0x33, 0x1c, 0xaa, 0x00, 0x01,
    0x1c, 0x47, 0x1e, 0xe3, 0x1c, 0x47, 0x1d, 0x32,
    0x1d, 0x77, 0x1c, 0x47, 0x1c, 0x60, 0x1d, 0x77,
    0x1c, 0xaa, 0x00, 0x01, 0x1d, 0x32, 0x1d, 0xb9,
    0x00, 0x01, 0x1c, 0x47, 0x1e, 0x71, 0x1c, 0x7a,
    0x1d, 0x32, 0x1d, 0x32, 0x02, 0x77, 0x00, 0x01,
    0x1e, 0x95, 0x1d, 0x18, 0x1d, 0x32, 0x1e, 0x71,
    0x00, 0x01, 0x1d, 0x32, 0x1e, 0x71, 0x00, 0x01,
    0x1c, 0x47, 0x00, 0x01, 0x1c, 0x7a, 0x1d, 0xdd,
    0x1d, 0xaa, 0x1d, 0xaa, 0x1d, 0xdd, 0x1d, 0xb9,
    0x00, 0x01, 0x1e, 0x71, 0x1e, 0x0c, 0x1c, 0xaa,
    0x1c, 0x7a, 0x1d, 0x32, 0x1c, 0x47, 0x1d, 0x77,
    0x00, 0x01, 0x1c, 0x7a, 0x1c, 0x47, 0x1e, 0x71,
    0x1c, 0xaa, 0x00, 0x01, 0x1e, 0x95, 0x1d, 0x18,
    0x1c, 0x47, 0x1e, 0x95, 0x00, 0x01, 0x1e, 0xf5,
    0x1c, 0x47, 0x1e, 0x33, 0x1e, 0x33, 0x1c, 0x47,
    0x1d, 0xb9, 0x1e, 0x95, 0x1e, 0x71, 0x00, 0x01,
    0x1c, 0x47, 0x00, 0x01, 0x1c, 0x60, 0x1c, 0xaa,
    0x1d, 0xb9, 0x1c, 0x7a, 0x1d, 0x18, 0x1d, 0xaa,
    0x1c, 0x47, 0x1e, 0x33, 0x1d, 0x65, 0x00, 0x01,
    0x1d, 0xdd, 0x1d, 0xb9, 0x00, 0x01, 0x1d, 0x32,
    0x1e, 0x95, 0x1e, 0x71, 0x00, 0x01, 0x1d, 0xdd,
    0x1e, 0xf5, 0x1d, 0xb9, 0x02, 0x22, 0x00, 0x01,
    0x1c, 0xaa, 0x1e, 0xe3, 0x1c, 0xaa, 0x1d, 0xb9,
    0x00, 0x01, 0x1d, 0x32, 0x1c, 0xe5, 0x00, 0x01,
    0x1e, 0x95, 0x1d, 0x18, 0x1c, 0xaa, 0x00, 0x01,
    0x1c, 0x7a, 0x1d, 0x18, 0x1c, 0x47, 0x1e, 0x33,
    0x1c, 0x47, 0x1c, 0x7a, 0x1e, 0x95, 0x1c, 0xaa,
    0x1e, 0x33, 0x00, 0x01, 0x1e, 0x71, 0x1c, 0xaa,
    0x1e, 0x95, 0x00, 0x01, 0x1c, 0x47, 0x1d, 0xb9,
    0x1c, 0x8f, 0x00, 0x01, 0x1c, 0x7a, 0x1d, 0xdd,
    0x1d, 0x77, 0x1d, 0x77, 0x1c, 0x47, 0x1e, 0x95,
    0x1d, 0x32, 0x1d, 0xdd, 0x1d, 0xb9, 0x00, 0x01,
    0x1e, 0x71, 0x1e, 0xb5, 0x1e, 0x0c, 0x1e, 0x0c,
    0x1d, 0xdd, 0x1e, 0x33, 0x1e, 0x95, 0x1e, 0x71,
    0x00, 0x01, 0x1d, 0xaa, 0x1e, 0xb5, 0x1c, 0x7a,
    0x1d, 0x18, 0x00, 0x01, 0x1d, 0xaa, 0x1d, 0xdd,
    0x1e, 0x33, 0x1c, 0xaa, 0x00, 0x01, 0x1c, 0x7a,
    0x1d, 0xdd, 0x1d, 0xaa, 0x1e, 0x0c, 0x1d, 0x77,
    0x1d, 0x32, 0x1c, 0x7a, 0x1c, 0x47, 0x1e, 0x95,
    0x1c, 0xaa, 0x1c, 0x8f, 0x00, 0x01, 0x1e, 0x71,
    0x1c, 0x7a, 0x1c, 0xaa, 0x1d, 0xb9, 0x1c, 0x47,
    0x1e, 0x33, 0x1d, 0x32, 0x1d, 0xdd, 0x1e, 0x71,
    0x02, 0x77
  };
  uchar dest[sizeof(expected)];

  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    my_strnxfrm(&my_charset_utf8mb4_0900_ai_ci, dest, sizeof(dest),
      reinterpret_cast<const uchar *>(content), len);
  }
  StopBenchmarkTiming();

  expect_arrays_equal(expected, dest, sizeof(dest));
}
BENCHMARK(BM_SimpleUTF8MB4);

// Benchmark testing a wider variety of character sets on a more complicated
// collation (the recommended default collation for 8.0), without stressing
// padding as much.
static void BM_MixedUTF8MB4(size_t num_iterations)
{
  StopBenchmarkTiming();

  // Some English text, then some Norwegian text, then some Japanese,
  // and then a few emoji (the last with skin tone modifiers).
  const char *content= "Premature optimization is the root of all evil. "
    "Våre norske tegn bør æres. 日本語が少しわかります。 ✌️🐶👩🏽";
  const int len= strlen(content);

  // Just recorded from a trial run on the string above.
  static constexpr uchar expected[]= {
    0x1e, 0x0c, 0x1e, 0x33, 0x1c, 0xaa, 0x1d, 0xaa, 0x1c,
    0x47, 0x1e, 0x95, 0x1e, 0xb5, 0x1e, 0x33, 0x1c, 0xaa,
    0x00, 0x01, 0x1d, 0xdd, 0x1e, 0x0c, 0x1e, 0x95, 0x1d,
    0x32, 0x1d, 0xaa, 0x1d, 0x32, 0x1f, 0x21, 0x1c, 0x47,
    0x1e, 0x95, 0x1d, 0x32, 0x1d, 0xdd, 0x1d, 0xb9, 0x00,
    0x01, 0x1d, 0x32, 0x1e, 0x71, 0x00, 0x01, 0x1e, 0x95,
    0x1d, 0x18, 0x1c, 0xaa, 0x00, 0x01, 0x1e, 0x33, 0x1d,
    0xdd, 0x1d, 0xdd, 0x1e, 0x95, 0x00, 0x01, 0x1d, 0xdd,
    0x1c, 0xe5, 0x00, 0x01, 0x1c, 0x47, 0x1d, 0x77, 0x1d,
    0x77, 0x00, 0x01, 0x1c, 0xaa, 0x1e, 0xe3, 0x1d, 0x32,
    0x1d, 0x77, 0x02, 0x77, 0x00, 0x01, 0x1e, 0xe3, 0x1c,
    0x47, 0x1e, 0x33, 0x1c, 0xaa, 0x00, 0x01, 0x1d, 0xb9,
    0x1d, 0xdd, 0x1e, 0x33, 0x1e, 0x71, 0x1d, 0x65, 0x1c,
    0xaa, 0x00, 0x01, 0x1e, 0x95, 0x1c, 0xaa, 0x1c, 0xf4,
    0x1d, 0xb9, 0x00, 0x01, 0x1c, 0x60, 0x1d, 0xdd, 0x1e,
    0x33, 0x00, 0x01, 0x1c, 0x47, 0x1c, 0xaa, 0x1e, 0x33,
    0x1c, 0xaa, 0x1e, 0x71, 0x02, 0x77, 0x00, 0x01, 0xfb,
    0x40, 0xe5, 0xe5, 0xfb, 0x40, 0xe7, 0x2c, 0xfb, 0x41,
    0x8a, 0x9e, 0x3d, 0x60, 0xfb, 0x40, 0xdc, 0x11, 0x3d,
    0x66, 0x3d, 0x87, 0x3d, 0x60, 0x3d, 0x83, 0x3d, 0x79,
    0x3d, 0x67, 0x02, 0x8a, 0x00, 0x01, 0x0a, 0x2d, 0x13,
    0xdf, 0x14, 0x12, 0x13, 0xa6
  };
  uchar dest[sizeof(expected)];

  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    my_strnxfrm(&my_charset_utf8mb4_0900_ai_ci, dest, sizeof(dest),
      reinterpret_cast<const uchar *>(content), len);
  }
  StopBenchmarkTiming();

  expect_arrays_equal(expected, dest, sizeof(dest));
}
BENCHMARK(BM_MixedUTF8MB4);

// Case-sensitive, accent-sensitive benchmark, using the same string as
// BM_SimpleUTF8MB4. This will naturally be slower, since many more weights
// need to be generated.
static void BM_MixedUTF8MB4_AS_CS(size_t num_iterations)
{
  StopBenchmarkTiming();

  // Some English text, then some Norwegian text, then some Japanese,
  // and then a few emoji (the last with skin tone modifiers).
  const char *content= "Premature optimization is the root of all evil. "
    "Våre norske tegn bør æres. 日本語が少しわかります。 ✌️🐶👩🏽";
  const int len= strlen(content);

  // Just recorded from a trial run on the string above. The last four
  // bytes are padding.
  static constexpr uchar expected[]= {
    // Primary weights.
    0x1e, 0x0c, 0x1e, 0x33, 0x1c, 0xaa, 0x1d, 0xaa, 0x1c,
    0x47, 0x1e, 0x95, 0x1e, 0xb5, 0x1e, 0x33, 0x1c, 0xaa,
    0x00, 0x01, 0x1d, 0xdd, 0x1e, 0x0c, 0x1e, 0x95, 0x1d,
    0x32, 0x1d, 0xaa, 0x1d, 0x32, 0x1f, 0x21, 0x1c, 0x47,
    0x1e, 0x95, 0x1d, 0x32, 0x1d, 0xdd, 0x1d, 0xb9, 0x00,
    0x01, 0x1d, 0x32, 0x1e, 0x71, 0x00, 0x01, 0x1e, 0x95,
    0x1d, 0x18, 0x1c, 0xaa, 0x00, 0x01, 0x1e, 0x33, 0x1d,
    0xdd, 0x1d, 0xdd, 0x1e, 0x95, 0x00, 0x01, 0x1d, 0xdd,
    0x1c, 0xe5, 0x00, 0x01, 0x1c, 0x47, 0x1d, 0x77, 0x1d,
    0x77, 0x00, 0x01, 0x1c, 0xaa, 0x1e, 0xe3, 0x1d, 0x32,
    0x1d, 0x77, 0x02, 0x77, 0x00, 0x01, 0x1e, 0xe3, 0x1c,
    0x47, 0x1e, 0x33, 0x1c, 0xaa, 0x00, 0x01, 0x1d, 0xb9,
    0x1d, 0xdd, 0x1e, 0x33, 0x1e, 0x71, 0x1d, 0x65, 0x1c,
    0xaa, 0x00, 0x01, 0x1e, 0x95, 0x1c, 0xaa, 0x1c, 0xf4,
    0x1d, 0xb9, 0x00, 0x01, 0x1c, 0x60, 0x1d, 0xdd, 0x1e,
    0x33, 0x00, 0x01, 0x1c, 0x47, 0x1c, 0xaa, 0x1e, 0x33,
    0x1c, 0xaa, 0x1e, 0x71, 0x02, 0x77, 0x00, 0x01, 0xfb,
    0x40, 0xe5, 0xe5, 0xfb, 0x40, 0xe7, 0x2c, 0xfb, 0x41,
    0x8a, 0x9e, 0x3d, 0x60, 0xfb, 0x40, 0xdc, 0x11, 0x3d,
    0x66, 0x3d, 0x87, 0x3d, 0x60, 0x3d, 0x83, 0x3d, 0x79,
    0x3d, 0x67, 0x02, 0x8a, 0x00, 0x01, 0x0a, 0x2d, 0x13,
    0xdf, 0x14, 0x12, 0x13, 0xa6,
    // Level separator.
    0x00, 0x00,
    // Secondary weights.
    0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x01, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x01, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x01, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x29, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x01, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x01,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x2f, 0x00, 0x20,
    0x00, 0x01, 0x00, 0x20, 0x01, 0x10, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x01, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x37, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x01, 0x00, 0x20,
    0x00, 0x20, 0x00, 0x20, 0x00, 0x20,
    // Level separator.
    0x00, 0x00,
    // Tertiary weights.
    0x00, 0x08, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x01,
    0x00, 0x08, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 0x04,
    0x00, 0x04, 0x00, 0x04, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x02, 0x00, 0x0e, 0x00, 0x02,
    0x00, 0x02, 0x00, 0x0e, 0x00, 0x0e, 0x00, 0x0e,
    0x00, 0x0e, 0x00, 0x0e, 0x00, 0x0e, 0x00, 0x02,
    0x00, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,
    0x00, 0x02,
  };
  uchar dest[sizeof(expected)];

  size_t ret= 0;
  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    ret = my_strnxfrm(&my_charset_utf8mb4_0900_as_cs, dest, sizeof(dest),
      pointer_cast<const uchar *>(content), len);
  }
  StopBenchmarkTiming();

  EXPECT_EQ(sizeof(expected), ret);
  expect_arrays_equal(expected, dest, ret);
}
BENCHMARK(BM_MixedUTF8MB4_AS_CS);

/*
  A benchmark that illustrates the potential perils of not including the
  range [0x00,0x20) in our fast path; newlines throw us off the fast path
  and reduce speed.

  The newlines are spaced a bit randomly in order not to create a perfectly
  predictable pattern for the branch predictor (benchmark paranoia).
*/
static void BM_NewlineFilledUTF8MB4(size_t num_iterations)
{
  StopBenchmarkTiming();

  const char *content= "This is a\n prett\ny unrealist\nic case; a\nn "
    "Eng\nlish sente\nnce where\n we'\nve added a new\nline every te\nn "
    "bytes or\n so.\n";
  const int len= strlen(content);

  // Just recorded from a trial run on the string above.
  static constexpr uchar expected[]= {
    0x1e, 0x95, 0x1d, 0x18, 0x1d, 0x32, 0x1e, 0x71,
    0x00, 0x01, 0x1d, 0x32, 0x1e, 0x71, 0x00, 0x01,
    0x1c, 0x47, 0x02, 0x02, 0x00, 0x01, 0x1e, 0x0c,
    0x1e, 0x33, 0x1c, 0xaa, 0x1e, 0x95, 0x1e, 0x95,
    0x02, 0x02, 0x1f, 0x0b, 0x00, 0x01, 0x1e, 0xb5,
    0x1d, 0xb9, 0x1e, 0x33, 0x1c, 0xaa, 0x1c, 0x47,
    0x1d, 0x77, 0x1d, 0x32, 0x1e, 0x71, 0x1e, 0x95,
    0x02, 0x02, 0x1d, 0x32, 0x1c, 0x7a, 0x00, 0x01,
    0x1c, 0x7a, 0x1c, 0x47, 0x1e, 0x71, 0x1c, 0xaa,
    0x02, 0x34, 0x00, 0x01, 0x1c, 0x47, 0x02, 0x02,
    0x1d, 0xb9, 0x00, 0x01, 0x1c, 0xaa, 0x1d, 0xb9,
    0x1c, 0xf4, 0x02, 0x02, 0x1d, 0x77, 0x1d, 0x32,
    0x1e, 0x71, 0x1d, 0x18, 0x00, 0x01, 0x1e, 0x71,
    0x1c, 0xaa, 0x1d, 0xb9, 0x1e, 0x95, 0x1c, 0xaa,
    0x02, 0x02, 0x1d, 0xb9, 0x1c, 0x7a, 0x1c, 0xaa,
    0x00, 0x01, 0x1e, 0xf5, 0x1d, 0x18, 0x1c, 0xaa,
    0x1e, 0x33, 0x1c, 0xaa, 0x02, 0x02, 0x00, 0x01,
    0x1e, 0xf5, 0x1c, 0xaa, 0x03, 0x05, 0x02, 0x02,
    0x1e, 0xe3, 0x1c, 0xaa, 0x00, 0x01, 0x1c, 0x47,
    0x1c, 0x8f, 0x1c, 0x8f, 0x1c, 0xaa, 0x1c, 0x8f,
    0x00, 0x01, 0x1c, 0x47, 0x00, 0x01, 0x1d, 0xb9,
    0x1c, 0xaa, 0x1e, 0xf5, 0x02, 0x02, 0x1d, 0x77,
    0x1d, 0x32, 0x1d, 0xb9, 0x1c, 0xaa, 0x00, 0x01,
    0x1c, 0xaa, 0x1e, 0xe3, 0x1c, 0xaa, 0x1e, 0x33,
    0x1f, 0x0b, 0x00, 0x01, 0x1e, 0x95, 0x1c, 0xaa,
    0x02, 0x02, 0x1d, 0xb9, 0x00, 0x01, 0x1c, 0x60,
    0x1f, 0x0b, 0x1e, 0x95, 0x1c, 0xaa, 0x1e, 0x71,
    0x00, 0x01, 0x1d, 0xdd, 0x1e, 0x33, 0x02, 0x02,
    0x00, 0x01, 0x1e, 0x71, 0x1d, 0xdd, 0x02, 0x77,
    0x02, 0x02
  };
  uchar dest[sizeof(expected)];

  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    my_strnxfrm(&my_charset_utf8mb4_0900_ai_ci, dest, sizeof(dest),
      reinterpret_cast<const uchar *>(content), len);
  }
  StopBenchmarkTiming();

  expect_arrays_equal(expected, dest, sizeof(dest));
}
BENCHMARK(BM_NewlineFilledUTF8MB4);

static void BM_HashSimpleUTF8MB4(size_t num_iterations)
{
  StopBenchmarkTiming();

  const char *content= "This is a rather long string that contains only "
    "simple letters that are available in ASCII. This is a common special "
    "case that warrants a benchmark on its own, even if the character set "
    "and collation supports much more complicated scenarios.";
  const int len= strlen(content);

  ulong nr1= 1, nr2= 4;

  StartBenchmarkTiming();
  for (size_t i= 0; i < num_iterations; ++i)
  {
    my_charset_utf8mb4_0900_ai_ci.coll->hash_sort(&my_charset_utf8mb4_0900_ai_ci,
      reinterpret_cast<const uchar *>(content), len, &nr1, &nr2);
  }
  StopBenchmarkTiming();

  /*
    Just to keep the compiler from optimizing away everything; this is highly
    unlikely to ever happen given hash function that's not totally broken.
    Don't test for an exact value; it will vary by platform and number
    of iterations.
  */
  EXPECT_FALSE(nr1 == 0 && nr2 == 0);
}
BENCHMARK(BM_HashSimpleUTF8MB4);

TEST(PadCollationTest, BasicTest)
{
  constexpr char foo[] = "foo";
  constexpr char foosp[] = "foo    ";
  constexpr char bar[] = "bar";
  constexpr char foobar[] = "foobar";

  auto my_strnncollsp= my_charset_utf8mb4_0900_ai_ci.coll->strnncollsp;

  // "foo" == "foo"
  EXPECT_EQ(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(foo), strlen(foo),
                           pointer_cast<const uchar *>(foo), strlen(foo)),
            0);
  // "foo" == "foo    "
  EXPECT_EQ(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(foo), strlen(foo),
                           pointer_cast<const uchar *>(foosp), strlen(foosp)),
            0);
  // "foo" > "bar"
  EXPECT_GT(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(foo), strlen(foo),
                           pointer_cast<const uchar *>(bar), strlen(bar)),
            0);
  // "foo" < "foobar" because "foo    " < "foobar"
  EXPECT_LT(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(foo), strlen(foo),
                           pointer_cast<const uchar *>(foobar), strlen(foobar)),
            0);

  // Exactly the same tests in reverse.

  // "foo    " == "foo"
  EXPECT_EQ(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(foosp), strlen(foosp),
                           pointer_cast<const uchar *>(foo), strlen(foo)),
            0);
  // "bar" < "foo"
  EXPECT_LT(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(bar), strlen(bar),
                           pointer_cast<const uchar *>(foo), strlen(foo)),
            0);
  // "foobar" > "foo" because "foobar" > "foo    "
  EXPECT_GT(my_strnncollsp(&my_charset_utf8mb4_0900_ai_ci,
                           pointer_cast<const uchar *>(foobar), strlen(foobar),
                           pointer_cast<const uchar *>(foo), strlen(foo)),
            0);
}

int compare_through_strxfrm(CHARSET_INFO *cs, const char *a, const char *b)
{
  uchar abuf[256], bbuf[256];
  int alen= my_strnxfrm(
    cs, abuf, sizeof(abuf), pointer_cast<const uchar *>(a), strlen(a));
  int blen= my_strnxfrm(
    cs, bbuf, sizeof(bbuf), pointer_cast<const uchar *>(b), strlen(b));

  if (false)  // Enable this for debugging.
  {
    fprintf(stderr, "\n\nstrxfrm for '%s':\n", a);
    print_array(abuf, alen);
    fprintf(stderr, "strxfrm for '%s':\n", b);
    print_array(bbuf, blen);
  }

  int cmp= memcmp(abuf, bbuf, std::min(alen, blen));
  if (cmp != 0)
    return cmp;

  if (alen == blen)
  {
    return 0;
  }
  else
  {
    return (alen < blen) ? -1 : 1;
  }
}

TEST(PadCollationTest, Strxfrm)
{
  // Basic sanity checks.
  EXPECT_EQ(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "abc", "abc"), 0);
  EXPECT_NE(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "abc", "def"), 0);

  // Spaces from the end should not matter, no matter the collation.
  EXPECT_EQ(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "abc", "abc  "), 0);
  EXPECT_EQ(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "abc", "abc  "), 0);
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "abc", "Abc  "), 0);

  // Same with other types of spaces.
  EXPECT_EQ(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "abc", u8"abc \u00a0"), 0);

  // Non-breaking space should compare _equal_ to space in ai_ci,
  // but _after_ in as_cs.
  EXPECT_EQ(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "abc ", u8"abc\u00a0"), 0);
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "abc ", u8"abc\u00a0"), 0);
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "abc", u8"abc\u00a0"), 0);

  // Also in the middle of the string.
  EXPECT_EQ(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "a c", u8"a\u00a0c"), 0);
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "a c", u8"a\u00a0c"), 0);

  // Verify that space in the middle of the string isn't stripped.
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_ai_ci, "ab  c", "abc"), 0);
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "ab  c", "abc"), 0);

  /*
    This is contrary to the default DUCET ordering, but is needed
    for our algorithm to work.
  */
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, " ", "\t"), 0);
  EXPECT_LT(compare_through_strxfrm(
    &my_charset_utf8mb4_0900_as_cs, "", "\t"), 0);
}

/*
  This test is disabled by default since it needs ~10 seconds to run,
  even in optimized mode.
*/
TEST(BitfiddlingTest, DISABLED_FastOutOfRange)
{
  unsigned char bytes[4];
  for (int a= 0; a < 256; ++a)
  {
    bytes[0]= a;
    for (int b= 0; b < 256; ++b)
    {
      bytes[1]= b;
      for (int c= 0; c < 256; ++c)
      {
        bytes[2]= c;
        for (int d= 0; d < 256; ++d)
        {
          bytes[3]= d;
          bool any_out_of_range_slow=
            (a < 0x20 || a > 0x7e) ||
            (b < 0x20 || b > 0x7e) ||
            (c < 0x20 || c > 0x7e) ||
            (d < 0x20 || d > 0x7e);

          uint32 four_bytes;
          memcpy(&four_bytes, bytes, sizeof(four_bytes));
          bool any_out_of_range_fast=
            (((four_bytes + 0x01010101u) & 0x80808080) ||
             ((four_bytes - 0x20202020u) & 0x80808080));

          EXPECT_EQ(any_out_of_range_slow, any_out_of_range_fast);
        }
      }
    }
  }
}

/*
  A version of FastOutOfRange that tests the analogous trick for 16-bit
  integers instead (much, much faster).
*/
TEST(BitfiddlingTest, FastOutOfRange16)
{
  unsigned char bytes[2];
  for (int a= 0; a < 256; ++a)
  {
    bytes[0]= a;
    for (int b= 0; b < 256; ++b)
    {
      bytes[1]= b;
      bool any_out_of_range_slow=
        (a < 0x20 || a > 0x7e) ||
        (b < 0x20 || b > 0x7e);

      uint16 two_bytes;
      memcpy(&two_bytes, bytes, sizeof(two_bytes));
      bool any_out_of_range_fast=
        (((two_bytes + uint16{0x0101}) & uint16{0x8080}) ||
         ((two_bytes - uint16{0x2020}) & uint16{0x8080}));

      EXPECT_EQ(any_out_of_range_slow, any_out_of_range_fast);
    }
  }
}

ulong hash(CHARSET_INFO *cs, const char *str)
{
  ulong nr1=1, nr2= 4;
  cs->coll->hash_sort(
    cs, pointer_cast<const uchar *>(str), strlen(str), &nr1, &nr2);
  return nr1;
}

/*
  NOTE: In this entire test, there's an infinitesimal chance
  that something that we expect doesn't match, still matches
  by pure accident.
*/
TEST(PadCollationTest, HashSort)
{
  CHARSET_INFO *ai_ci= &my_charset_utf8mb4_0900_ai_ci;
  CHARSET_INFO *as_cs= &my_charset_utf8mb4_0900_as_cs;

  // Basic sanity checks.
  EXPECT_EQ(hash(ai_ci, "abc"), hash(ai_ci, "abc"));
  EXPECT_NE(hash(ai_ci, "abc"), hash(ai_ci, "def"));

  // Spaces from the end should not matter, no matter the collation.
  EXPECT_EQ(hash(ai_ci, "abc"), hash(ai_ci, "abc  "));
  EXPECT_EQ(hash(as_cs, "abc"), hash(as_cs, "abc  "));
  EXPECT_NE(hash(as_cs, "abc"), hash(as_cs, "Abc  "));

  // Same with other types of spaces.
  EXPECT_EQ(hash(ai_ci, "abc"), hash(ai_ci, u8"abc \u00a0"));

  // Non-breaking space should compare _equal_ to space in ai_ci,
  // but _inequal_ in as_cs.
  EXPECT_EQ(hash(ai_ci, "abc "), hash(ai_ci, u8"abc\u00a0"));
  EXPECT_NE(hash(as_cs, "abc "), hash(as_cs, u8"abc\u00a0"));
  EXPECT_NE(hash(as_cs, "abc"), hash(as_cs, u8"abc\u00a0"));

  // Also in the middle of the string.
  EXPECT_EQ(hash(ai_ci, "a c"), hash(ai_ci, u8"a\u00a0c"));
  EXPECT_NE(hash(as_cs, "a c"), hash(as_cs, u8"a\u00a0c"));

  // Verify that space in the middle of the string isn't stripped.
  EXPECT_NE(hash(ai_ci, "ab  c"), hash(ai_ci, "abc"));
  EXPECT_NE(hash(as_cs, "ab  c"), hash(as_cs, "abc"));
}

}  // namespace strnxfrm_unittest
