/* Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include <my_global.h>
#include <my_sys.h>

namespace like_range_unittest {

/*
  Test that like_range() returns well-formed results.
*/
static void
test_like_range_for_charset(CHARSET_INFO *cs, const char *src, size_t src_len)
{
  char min_str[32], max_str[32];
  size_t min_len, max_len, min_well_formed_len, max_well_formed_len;
  int error= 0;
  
  cs->coll->like_range(cs, src, src_len, '\\', '_', '%',
                       sizeof(min_str),  min_str, max_str, &min_len, &max_len);
  // diag("min_len=%d\tmax_len=%d\t%s", (int) min_len, (int) max_len, cs->name);
  min_well_formed_len= cs->cset->well_formed_len(cs,
                                                 min_str, min_str + min_len,
                                                 10000, &error);
  max_well_formed_len= cs->cset->well_formed_len(cs,
                                                 max_str, max_str + max_len,
                                                 10000, &error);
  EXPECT_EQ(min_len, min_well_formed_len)
    << "Bad min_str: min_well_formed_len=" << min_well_formed_len
    << " min_str[" << min_well_formed_len << "]="
    <<  (uchar) min_str[min_well_formed_len];
  EXPECT_EQ(max_len, max_well_formed_len)
    << "Bad max_str: max_well_formed_len=" << max_well_formed_len
    << " max_str[" << max_well_formed_len << "]="
    << (uchar) max_str[max_well_formed_len];
}


static CHARSET_INFO *charset_list[]=
{
#ifdef HAVE_CHARSET_big5
  &my_charset_big5_chinese_ci,
  &my_charset_big5_bin,
#endif
#ifdef HAVE_CHARSET_euckr
  &my_charset_euckr_korean_ci,
  &my_charset_euckr_bin,
#endif
#ifdef HAVE_CHARSET_gb2312
  &my_charset_gb2312_chinese_ci,
  &my_charset_gb2312_bin,
#endif
#ifdef HAVE_CHARSET_gbk
  &my_charset_gbk_chinese_ci,
  &my_charset_gbk_bin,
#endif
#ifdef HAVE_CHARSET_gb18030
  &my_charset_gb18030_chinese_ci,
  &my_charset_gb18030_bin,
#endif
#ifdef HAVE_CHARSET_latin1
  &my_charset_latin1,
  &my_charset_latin1_bin,
#endif
#ifdef HAVE_CHARSET_sjis
  &my_charset_sjis_japanese_ci,
  &my_charset_sjis_bin,
#endif
#ifdef HAVE_CHARSET_tis620
  &my_charset_tis620_thai_ci,
  &my_charset_tis620_bin,
#endif
#ifdef HAVE_CHARSET_ujis
  &my_charset_ujis_japanese_ci,
  &my_charset_ujis_bin,
#endif
#ifdef HAVE_CHARSET_utf8
  &my_charset_utf8_general_ci,
  &my_charset_utf8_unicode_ci,
  &my_charset_utf8_bin,
#endif
};

#if defined(GTEST_HAS_PARAM_TEST)

class LikeRangeTest : public ::testing::TestWithParam<CHARSET_INFO*>
{
protected:
  virtual void SetUp()
  {
    m_charset= GetParam();
  }
  CHARSET_INFO *m_charset;
};

INSTANTIATE_TEST_CASE_P(Foo1, LikeRangeTest,
                        ::testing::ValuesIn(charset_list));


TEST_P(LikeRangeTest, TestLikeRange)
{
  test_like_range_for_charset(m_charset, "abc%", 4);
}

#endif

}
