/*
   Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"
#include <gtest/gtest.h>


#include <m_ctype.h>
#include <mf_wcomp.h>     // wild_compare_full, wild_one, wild_any
#include <sql_class.h>

namespace strings_utf8_unittest {

class StringsUTF8Test : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    // Save global settings.
    m_charset= system_charset_info;

    system_charset_info= &my_charset_utf8_bin;
  }

  virtual void TearDown()
  {
    // Restore global settings.
    system_charset_info= m_charset;
  }

private:
  CHARSET_INFO *m_charset;
};

TEST_F(StringsUTF8Test, MyStrchr)
{
  const char* null_pos= NULL;
  char* pos;
  char valid_utf8_string[]= "str1";

  /*
    All valid utf8 characters in str arg passed to  my_strchr and  char to be
    found not in str.
  */

  pos= my_strchr(system_charset_info, valid_utf8_string,
                 valid_utf8_string + 3, 't');

  EXPECT_EQ(valid_utf8_string + 1, pos);

  /*
    All valid utf8 characters in str arg passed to  my_strchr and  char to be
    found not in str.
  */
  pos= my_strchr(system_charset_info, valid_utf8_string,
                 valid_utf8_string + 3, 'd');

  ASSERT_EQ(null_pos, pos);

  // Assign an invalid utf8 char to valid_utf8_str
  valid_utf8_string[0]= '\xff';

  // Invalid utf8 character in str arg passed to my_strchr.
  pos= my_strchr(system_charset_info, valid_utf8_string,
                 valid_utf8_string + 3,'y');
  ASSERT_EQ(null_pos, pos);

  // Assign an invalid utf8 char to valid_utf8_str
  valid_utf8_string[0]= '\xED';
  valid_utf8_string[1]= '\xA0';
  valid_utf8_string[2]= '\xBF';

  // Invalid utf8 character in str arg passed to my_strchr.
  pos= my_strchr(system_charset_info, valid_utf8_string,
                 valid_utf8_string + 3,'y');
  ASSERT_EQ(null_pos, pos);
}

TEST_F(StringsUTF8Test, MyStrcasecmpMb)
{
  std::string utf8_src= "str";
  std::string utf8_dst= "str";

  EXPECT_EQ(0, my_strcasecmp_mb(system_charset_info, utf8_src.c_str(),
                                utf8_dst.c_str()));

  utf8_dst[1]= 'd';
  
  // src and dst are different utf8 strings
  EXPECT_EQ(1, my_strcasecmp_mb(system_charset_info, utf8_src.c_str(),
                                utf8_dst.c_str()));

  // dst contain an invalid utf8 string
  utf8_dst[1]= '\xFF';
  EXPECT_EQ(1, my_strcasecmp_mb(system_charset_info, utf8_src.c_str(),
                                utf8_dst.c_str()));

  utf8_dst[0]= '\xED';
  utf8_dst[1]= '\xA0';
  utf8_dst[2]= '\xBF';
  EXPECT_EQ(1, my_strcasecmp_mb(system_charset_info, utf8_src.c_str(),
                                utf8_dst.c_str()));
}

TEST_F(StringsUTF8Test, MyWellFormedLenUtf8)
{
  char utf8_src[32]= "\x00\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xef\xbf\xbf";
  int error;
  
  /* valid utf8 charaters, testing for boundry values */
  EXPECT_EQ(12U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                            utf8_src,
                                                            utf8_src + 12,
                                                            6, &error));
  ASSERT_EQ(0, error);

  /* test for 0 length string */
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src,
                                                           6, &error));
  ASSERT_EQ(0, error);

  /* test for illegal utf8 char */
  utf8_src[0]= '\xc1';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 1,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xdf';
  utf8_src[1]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xe0';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 3,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xe0';
  utf8_src[1]= '\x80';
  utf8_src[2]= '\x80';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 3,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xf0';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 1,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xc2';
  utf8_src[1]= '\x80';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 1,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xef';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\xbf';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xED';
  utf8_src[1]= '\xA0';
  utf8_src[2]= '\xBF';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);
}

TEST_F(StringsUTF8Test, MyIsmbcharUtf8)
{
  char utf8_src[8];

  /* valid utf8 charaters, testing for boundry values */
  utf8_src[0]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 1));

  utf8_src[0]= '\x7f';
  EXPECT_EQ(0U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 1));

  utf8_src[0]= '\xc2';
  utf8_src[1]= '\x80';
  EXPECT_EQ(2U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 2));

  utf8_src[0]= '\xdf';
  utf8_src[1]= '\xbf';
  EXPECT_EQ(2U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 2));

  utf8_src[0]= '\xe0';
  utf8_src[1]= '\xa0';
  utf8_src[2]= '\x80';
  EXPECT_EQ(3U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 3));

  utf8_src[0]= '\xef';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\xbf';
  EXPECT_EQ(3U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 3));

  /* Not testing for illegal charaters as same is tested in above test case */

}

template <size_t SIZE>
struct SL
{
  const char *b;
  SL(const char *be)
    : b(be) {}

  size_t len() const { return SIZE-1; }
  const char *e() const noexcept { return b+len(); }
};

template <size_t SIZE>
SL<SIZE> mk_sl(const char (&sl)[SIZE])
{
  return SL<SIZE>(sl);
}


TEST_F(StringsUTF8Test, WildCmpSelf)
{
  auto input= mk_sl("xx");
  EXPECT_EQ(0, my_wildcmp(system_charset_info, input.b, input.e(),
                          input.b, input.e(),
                          '\\', '?', '*'));
}

// Testing One (?)
TEST_F(StringsUTF8Test, WildCmpPrefixOne)
{
  auto input= mk_sl("xx");
  auto pat= mk_sl("?x");
  EXPECT_EQ(0, my_wildcmp(system_charset_info, input.b, input.e(),
                          pat.b, pat.e(),
                          '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildCmpSuffixOne)
{
  auto input= mk_sl("xx");
  auto pat= mk_sl("x?");
  EXPECT_EQ(0, my_wildcmp(system_charset_info, input.b, input.e(),
                          pat.b, pat.e(),
                          '\\', '?', '*'));
}


// Negative tests
TEST_F(StringsUTF8Test, WildCmpNoPatternNoMatch)
{
  auto input= mk_sl("xx");
  auto nopat= mk_sl("yy");
  EXPECT_EQ(1, my_wildcmp(system_charset_info, input.b, input.e(),
                          nopat.b, nopat.e(),
                          '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildCmpPrefixOneNoMatch)
{
  const char *input= "xx";
  const char *badpat= "?y";
  EXPECT_EQ(1, my_wildcmp(system_charset_info, input, input+2,
                          badpat, badpat+2,
                          '\\', '?', '*'));
}
TEST_F(StringsUTF8Test, WildCmpSuffixOneNoMatch)
{
  const char *input= "abcxx";
  const char *badpat= "x*";
  EXPECT_EQ(1, my_wildcmp(system_charset_info, input, input+5,
                          badpat, badpat+2,
                          '\\', '?', '*'));
}



// Testing Many (*)
TEST_F(StringsUTF8Test, WildCmpPrefixMany)
{
  auto input= mk_sl("abcxx");
  auto pat= mk_sl("*x");
  EXPECT_EQ(0, my_wildcmp(system_charset_info, input.b, input.e(),
                          pat.b, pat.e(),
                          '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildCmpSuffixMany)
{
  auto input= mk_sl("xxabc");
  auto pat= mk_sl("x*");
  EXPECT_EQ(0, my_wildcmp(system_charset_info, input.b, input.e(),
                          pat.b, pat.e(),
                          '\\', '?', '*'));
  EXPECT_EQ(0, wild_compare_full(input.b,
                            pat.b, false,
                            '\\', '?', '*'));
}


// Negative tests
TEST_F(StringsUTF8Test, WildCmpPrefixManyNoMatch)
{
  auto input= mk_sl("abcxx");
  auto badpat= mk_sl("a*xy");
  EXPECT_EQ(-1, my_wildcmp(system_charset_info, input.b, input.e(),
                           badpat.b, badpat.e(),
                           '\\', '?', '*'));

  EXPECT_EQ(-1, my_wildcmp((&my_charset_latin1), input.b, input.e(),
                           badpat.b, badpat.e(),
                           '\\', '?', '*'));

  // Note 1, not -1
  EXPECT_EQ(1, wild_compare_full(input.b, badpat.b, true,
                            '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildCmpSuffixManyNoMatch)
{
  auto input= mk_sl("abcxx");
  auto badpat= mk_sl("y*");
  EXPECT_EQ(1, my_wildcmp(system_charset_info, input.b, input.e(),
                          badpat.b, badpat.e(),
                          '\\', '?', '*'));
  EXPECT_EQ(1, wild_compare_full(input.b, badpat.b, true,
                            '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildComparePrefixMany)
{
  EXPECT_EQ(0, wild_compare_full("xyz_", "*_", true, '\\', '?', '*'));
  EXPECT_EQ(1, wild_compare_full("xyz_", "*a", true, '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildCompareSuffixOne)
{
  EXPECT_EQ(0, wild_compare_full("x_", "x?", true, '\\', '?', '*'));
  EXPECT_EQ(1, wild_compare_full("zz", "x?", true, '\\', '?', '*'));
}

TEST_F(StringsUTF8Test, WildCompareSuffixMany)
{
  EXPECT_EQ(0, wild_compare_full("xyz_", "x*", true, '\\', '?', '*'));
  EXPECT_EQ(1, wild_compare_full("xyz_", "a*", true, '\\', '?', '*'));
}

template <class INPUT, class PATTERN>
void test_cmp_vs_compare(int exp_cmp, int exp_compare,
                         int exp_compare_str_is_pat,
                         const INPUT &input, const PATTERN &pattern,
                         char wo, char wm)
{
  EXPECT_EQ(exp_cmp, my_wildcmp(&my_charset_latin1, input.b, input.e(),
                            pattern.b, pattern.e(), '\\', wo, wm));
  EXPECT_EQ(exp_compare, wild_compare_full(input.b, pattern.b, false, '\\', wo,
                                           wm));
  EXPECT_EQ(exp_compare_str_is_pat, wild_compare_full(input.b, pattern.b, true,
                                                      '\\', wo, wm));
}

TEST_F(StringsUTF8Test, EscapedWildOne)
{
  test_cmp_vs_compare(1, 1, 0, mk_sl("my\\_1"), mk_sl("my\\_1"), '_', '%');
}

TEST_F(StringsUTF8Test, EscapedWildOnePlainPattern)
{
  test_cmp_vs_compare(0, 0, 1, mk_sl("my_1"), mk_sl("my\\_1"), '_', '%');
}

TEST_F(StringsUTF8Test, StrIsPatternEscapes)
{
  EXPECT_EQ(1, wild_compare("my\\_", "my\\_", false));
  EXPECT_EQ(0, wild_compare("my\\_", "my\\\\\\_", false));
  EXPECT_EQ(0, wild_compare("my\\_", "my\\_", true));
}

TEST_F(StringsUTF8Test, StrIsPatternSupersetPattern)
{
  EXPECT_EQ(0, wild_compare("xa_a", "xa%a", true));
  EXPECT_EQ(0, wild_compare("xaaa%", "xa%", true));
  EXPECT_EQ(0, wild_compare("my\\_1", "my\\_%", true));
}

TEST_F(StringsUTF8Test, StrIsPatternUnescapedVsEscaped)
{
  EXPECT_EQ(1, wild_compare("my_1", "my\\_1", true));
  EXPECT_EQ(1, wild_compare("my_1", "my%\\_1", true));
}

TEST_F(StringsUTF8Test, MultiWildMany)
{
  EXPECT_EQ(0, wild_compare_full("t4.ibd", "t4*.ibd*", false, 0, '?', '*'));
}

class StringsUTF8mb4Test : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    // Save global settings.
    m_charset= system_charset_info;

    system_charset_info= &my_charset_utf8mb4_bin;
  }

  virtual void TearDown()
  {
    // Restore global settings.
    system_charset_info= m_charset;
  }

private:
  CHARSET_INFO *m_charset;
};

TEST_F(StringsUTF8mb4Test, MyWellFormedLenUtf8mb4)
{
  char utf8_src[32]= "\x00\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xef\xbf\xbf"
                     "\xf0\x90\x80\x80\xF4\x8F\xBF\xBF";
  int error;

  /* valid utf8mb4 charaters, testing for boundry values */
  EXPECT_EQ(20U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                            utf8_src,
                                                            utf8_src + 20,
                                                            8, &error));
  ASSERT_EQ(0, error);

  /* test for 0 length string */
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src, utf8_src,
                                                           8, &error));
  ASSERT_EQ(0, error);

  /* test for illegal utf8mb4 char */
  utf8_src[0]= '\xc1';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 1,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xdf';
  utf8_src[1]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xe0';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 3,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xe0';
  utf8_src[1]= '\x80';
  utf8_src[2]= '\x80';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 3,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xe0';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 3,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xf0';
  utf8_src[1]= '\x80';
  utf8_src[2]= '\x80';
  utf8_src[3]= '\x80';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 4,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xf4';
  utf8_src[1]= '\x9f';
  utf8_src[2]= '\x80';
  utf8_src[3]= '\x80';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 4,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xf0';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 1,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xc2';
  utf8_src[1]= '\x80';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 1,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xef';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\xbf';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xf4';
  utf8_src[1]= '\x8f';
  utf8_src[2]= '\xbf';
  utf8_src[3]= '\xbf';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);

  utf8_src[0]= '\xED';
  utf8_src[1]= '\xA0';
  utf8_src[2]= '\xBF';
  EXPECT_EQ(0U, system_charset_info->cset->well_formed_len(system_charset_info,
                                                           utf8_src,
                                                           utf8_src + 2,
                                                           1, &error));
  ASSERT_EQ(1, error);
}

TEST_F(StringsUTF8mb4Test, MyIsmbcharUtf8mb4)
{
  char utf8_src[8];

  /* valid utf8mb4 charaters, testing for boundry values */
  utf8_src[0]= '\x00';
  EXPECT_EQ(0U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src,utf8_src + 1));
  utf8_src[0]= '\x7f';
  EXPECT_EQ(0U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 1));
  utf8_src[0]= '\xc2';
  utf8_src[1]= '\x80';
  EXPECT_EQ(2U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 2));
  utf8_src[0]= '\xdf';
  utf8_src[1]= '\xbf';
  EXPECT_EQ(2U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 2));
  utf8_src[0]= '\xe0';
  utf8_src[1]= '\xa0';
  utf8_src[2]= '\x80';
  EXPECT_EQ(3U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 3));
  utf8_src[0]= '\xef';
  utf8_src[1]= '\xbf';
  utf8_src[2]= '\xbf';
  EXPECT_EQ(3U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 3));
  utf8_src[0]= '\xf0';
  utf8_src[1]= '\x90';
  utf8_src[2]= '\x80';
  utf8_src[3]= '\x80';
  EXPECT_EQ(4U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 4));
  utf8_src[0]= '\xf4';
  utf8_src[1]= '\x8f';
  utf8_src[2]= '\xbf';
  utf8_src[3]= '\xbf';
  EXPECT_EQ(4U, system_charset_info->cset->ismbchar(system_charset_info,
                                                    utf8_src, utf8_src + 4));

  /* Not testing for illegal charaters as same is tested in above test case */
}

class StringsUTF8mb4_900Test : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    // Save global settings.
    m_charset= system_charset_info;

    system_charset_info= &my_charset_utf8mb4_0900_ai_ci;
  }

  virtual void TearDown()
  {
    // Restore global settings.
    system_charset_info= m_charset;
  }

private:
  CHARSET_INFO *m_charset;
};

TEST_F(StringsUTF8mb4_900Test, MyUCA900Collate)
{
  uchar utf8mb4_src[8], utf8mb4_dst[8];

  /* Test for string comparison */

  /* U+00AD == U+0020 */
  utf8mb4_src[0]= 0xc2;
  utf8mb4_src[1]= 0xad;
  utf8mb4_src[2]= 0;
  utf8mb4_dst[0]= 0x20;
  utf8mb4_dst[1]= 0;
  EXPECT_FALSE(system_charset_info->coll->strnncollsp(system_charset_info,
                                                      utf8mb4_src, 2,
                                                      utf8mb4_dst, 1));
  /* U+00AD == U+00A0 */
  utf8mb4_src[0]= 0xc2;
  utf8mb4_src[1]= 0xad;
  utf8mb4_src[2]= 0;
  utf8mb4_dst[0]= 0xc2;
  utf8mb4_dst[1]= 0xa0;
  utf8mb4_dst[2]= 0;
  EXPECT_FALSE(system_charset_info->coll->strnncollsp(system_charset_info,
                                                      utf8mb4_src, 2,
                                                      utf8mb4_dst, 2));
  /* U+00C6 != U+0041 */
  utf8mb4_src[0]= 0xc3;
  utf8mb4_src[1]= 0x86;
  utf8mb4_src[2]= 0;
  utf8mb4_dst[0]= 0x41;
  utf8mb4_dst[1]= 0;
  EXPECT_TRUE(system_charset_info->coll->strnncollsp(system_charset_info,
                                                     utf8mb4_src, 2,
                                                     utf8mb4_dst, 1));
  /* U+00DF != U+0053 */
  utf8mb4_src[0]= 0xc3;
  utf8mb4_src[1]= 0x9F;
  utf8mb4_src[2]= 0;
  utf8mb4_dst[0]= 0x53;
  utf8mb4_dst[1]= 0;
  EXPECT_TRUE(system_charset_info->coll->strnncollsp(system_charset_info,
                                                     utf8mb4_src, 2,
                                                     utf8mb4_dst, 1));
  /* U+A73A == U+A738 */
  utf8mb4_src[0]= 0xea;
  utf8mb4_src[1]= 0x9c;
  utf8mb4_src[2]= 0xba;
  utf8mb4_src[3]= 0;
  utf8mb4_dst[0]= 0xea;
  utf8mb4_dst[1]= 0x9c;
  utf8mb4_dst[2]= 0xb8;
  utf8mb4_dst[3]= 0;
  EXPECT_FALSE(system_charset_info->coll->strnncollsp(system_charset_info,
                                                      utf8mb4_src, 3,
                                                      utf8mb4_dst, 3));
  /* U+A73B == U+A739 */
  utf8mb4_src[0]= 0xea;
  utf8mb4_src[1]= 0x9c;
  utf8mb4_src[2]= 0xbb;
  utf8mb4_src[3]= 0;
  utf8mb4_dst[0]= 0xea;
  utf8mb4_dst[1]= 0x9c;
  utf8mb4_dst[2]= 0xb9;
  utf8mb4_dst[3]= 0;
  EXPECT_FALSE(system_charset_info->coll->strnncollsp(system_charset_info,
                                                      utf8mb4_src, 3,
                                                      utf8mb4_dst, 3));
}

}
