/*
   Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <gtest/gtest.h>
#include <string>

#include "m_ctype.h"
#include "mf_wcomp.h"     // wild_compare_full, wild_one, wild_any
#include "my_inttypes.h"
#include "sql/sql_class.h"
#include "sql/strfunc.h"  // casedn

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

TEST_F(StringsUTF8Test, Casedn)
{
  std::string a= "aaa";
  std::string A= casedn(system_charset_info, std::string("AAA"));
  EXPECT_EQ(a, A);
  EXPECT_EQ(a.length(), A.length());

  std::string b= "bbbb";
  std::string B= "BBBB";
  casedn(system_charset_info, B);
  EXPECT_EQ(b, B);
  EXPECT_EQ(b.length(), B.length());
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
    MY_CHARSET_LOADER loader;
    my_charset_loader_init_mysys(&loader);
    m_charset= my_collation_get_by_name(&loader, "utf8mb4_0900_ai_ci", MYF(0));
  }

  bool equals(const char *a, const char *b)
  {
    return m_charset->coll->strnncollsp(
      m_charset,
      pointer_cast<const uchar *>(a), strlen(a),
      pointer_cast<const uchar *>(b), strlen(b)) == 0;
  }

private:
  CHARSET_INFO *m_charset;
};

/* Test for string comparison */
TEST_F(StringsUTF8mb4_900Test, MyUCA900Collate)
{
  // SOFT HYPHEN does not equal SPACE (the former has zero weight).
  EXPECT_FALSE(equals(u8"\u00ad", " "));

  // SPACE equals NO-BREAK SPACE.
  EXPECT_TRUE(equals(" ", u8"\u00a0"));

  EXPECT_FALSE(equals(u8"Æ", "A"));
  EXPECT_FALSE(equals(u8"ß", "S"));

  /*
    LATIN CAPITAL LETTER AV WITH HORIZONTAL BAR equals
    LATIN CAPITAL LETTER AV.
  */
  EXPECT_TRUE(equals(u8"\ua73a", u8"\ua738"));

  /*
    LATIN SMALL LETTER AV WITH HORIZONTAL BAR equals
    LATIN SMALL LETTER AV.
  */
  EXPECT_TRUE(equals(u8"\ua73b", u8"\ua739"));
}

class StringsUTF8mb4_900_AS_CS_NoPad_Test : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    MY_CHARSET_LOADER loader;
    my_charset_loader_init_mysys(&loader);
    m_charset= my_collation_get_by_name(&loader, "utf8mb4_0900_as_cs", MYF(0));
  }

  int compare(const char *a, const char *b, bool b_is_prefix)
  {
    return m_charset->coll->strnncoll(
      m_charset,
      pointer_cast<const uchar *>(a), strlen(a),
      pointer_cast<const uchar *>(b), strlen(b),
      b_is_prefix);
  }

private:
  CHARSET_INFO *m_charset;
};

TEST_F(StringsUTF8mb4_900_AS_CS_NoPad_Test, CaseSensitivity)
{
  // Basic sanity checks.
  EXPECT_EQ(compare("abc", "abc", false), 0);
  EXPECT_EQ(compare("ABC", "ABC", false), 0);

  // Letters (level 1) matter more than case (level 3).
  EXPECT_LT(compare("ABC", "def", false), 0);
  EXPECT_LT(compare("abc", "DEF", false), 0);

  // Lowercase sorts before uppercase.
  EXPECT_LT(compare("abc", "Abc", false), 0);
  EXPECT_LT(compare("abc", "aBc", false), 0);
  EXPECT_LT(compare("abc", "ABC", false), 0);
  EXPECT_GT(compare("ABC", "abc", false), 0);

  // Length matters more than case.
  EXPECT_LT(compare("abc", "abcd", false), 0);
  EXPECT_LT(compare("ABC", "abcd", false), 0);
}

TEST_F(StringsUTF8mb4_900_AS_CS_NoPad_Test, PrefixComparison)
{
  // Basic sanity checks.
  EXPECT_EQ(compare("abc", "abc", true), 0);
  EXPECT_EQ(compare("ABC", "ABC", true), 0);
  EXPECT_EQ(compare("abc.", "abc", true), 0);
  EXPECT_EQ(compare("ABC.", "ABC", true), 0);
  EXPECT_EQ(compare("ABC....", "ABC", true), 0);

  // Case sensitivity holds even on prefix matches (lowercase sorts first).
  EXPECT_GT(compare("ABC....", "abc", true), 0);

  // Difference before we get to the prefix logic (lowercase sorts first).
  EXPECT_GT(compare("aBcdef", "abc", true), 0);

  // Prefix matches only go one way.
  EXPECT_LT(compare("AB", "ABC", true), 0);
}

static bool uca_wildcmp(const CHARSET_INFO *cs, const char *str,
                        const char *pattern)
{
  const char *str_end= str + strlen(str);
  const char *pattern_end= pattern + strlen(pattern);
  return !cs->coll->wildcmp(cs, str, str_end, pattern, pattern_end,
                            '\\', '_', '%');
}

TEST(UCAWildCmpTest, UCA900WildCmp)
{
  MY_CHARSET_LOADER loader;
  my_charset_loader_init_mysys(&loader);
  CHARSET_INFO *cs= my_collation_get_by_name(&loader, "utf8mb4_0900_ai_ci", MYF(0));

  EXPECT_TRUE(uca_wildcmp(cs, "abc", "abc"));
  EXPECT_TRUE(uca_wildcmp(cs, "Abc", "aBc"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "_bc"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "a_c"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "ab_"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "%c"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "a%c"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "a%"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcdef", "a%d_f"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcdefg", "a%d%g"));
  EXPECT_TRUE(uca_wildcmp(cs, "a\\", "a\\"));
  EXPECT_TRUE(uca_wildcmp(cs, "aa\\", "a%\\"));
  EXPECT_TRUE(uca_wildcmp(cs, "Y", u8"\u00dd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcd", "abcde"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcde", "abcd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcde", "a%f"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcdef", "a%%f"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcd", "a__d"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcd", "a\\bcd"));
  EXPECT_FALSE(uca_wildcmp(cs, "a\\bcd", "abcd"));
  EXPECT_TRUE(uca_wildcmp(cs, "abdbcd", "a%cd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abecd", "a%bd"));

}

TEST(UCAWildCmpTest, UCA900WildCmpCaseSensitive)
{
  MY_CHARSET_LOADER loader;
  my_charset_loader_init_mysys(&loader);
  CHARSET_INFO *cs= my_collation_get_by_name(&loader, "utf8mb4_0900_as_cs", MYF(0));

  EXPECT_TRUE(uca_wildcmp(cs, "abc", "abc"));
  EXPECT_FALSE(uca_wildcmp(cs, "Abc", "aBc"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "_bc"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "a_c"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "ab_"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "%c"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "a%c"));
  EXPECT_TRUE(uca_wildcmp(cs, "abc", "a%"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcdef", "a%d_f"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcdefg", "a%d%g"));
  EXPECT_TRUE(uca_wildcmp(cs, "a\\", "a\\"));
  EXPECT_TRUE(uca_wildcmp(cs, "aa\\", "a%\\"));
  EXPECT_FALSE(uca_wildcmp(cs, "Y", u8"\u00dd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcd", "abcde"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcde", "abcd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcde", "a%f"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcdef", "a%%f"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcd", "a__d"));
  EXPECT_TRUE(uca_wildcmp(cs, "abcd", "a\\bcd"));
  EXPECT_FALSE(uca_wildcmp(cs, "a\\bcd", "abcd"));
  EXPECT_TRUE(uca_wildcmp(cs, "abdbcd", "a%cd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abecd", "a%bd"));

}

TEST(UCAWildCmpTest, UCA900WildCmp_AS_CI)
{
  MY_CHARSET_LOADER loader;
  my_charset_loader_init_mysys(&loader);
  CHARSET_INFO *cs= my_collation_get_by_name(&loader, "utf8mb4_0900_as_ci",
                                             MYF(0));
  EXPECT_TRUE(uca_wildcmp(cs, "ǎḄÇ", "Ǎḅç"));
  EXPECT_FALSE(uca_wildcmp(cs, "ÁḆĈ", "Ǎḅç"));
  EXPECT_TRUE(uca_wildcmp(cs, "ǍBc", "_bc"));
  EXPECT_TRUE(uca_wildcmp(cs, "Aḅc", "a_c"));
  EXPECT_TRUE(uca_wildcmp(cs, "Abç", "ab_"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅç", "%ç"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅç", "ǎ%Ç"));
  EXPECT_TRUE(uca_wildcmp(cs, "aḅç", "a%"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅçdef", "ǎ%d_f"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅçdefg", "ǎ%d%g"));
  EXPECT_TRUE(uca_wildcmp(cs, "ǎ\\", "Ǎ\\"));
  EXPECT_TRUE(uca_wildcmp(cs, "ǎa\\", "Ǎ%\\"));
  EXPECT_FALSE(uca_wildcmp(cs, "Y", u8"\u00dd"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcd", "Ǎḅçde"));
  EXPECT_FALSE(uca_wildcmp(cs, "abcde", "Ǎḅçd"));
  EXPECT_FALSE(uca_wildcmp(cs, "Ǎḅçde", "a%f"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅçdef", "ǎ%%f"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅçd", "ǎ__d"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅçd", "ǎ\\ḄÇd"));
  EXPECT_FALSE(uca_wildcmp(cs, "a\\bcd", "Ǎḅçd"));
  EXPECT_TRUE(uca_wildcmp(cs, "Ǎḅdbçd", "ǎ%Çd"));
  EXPECT_FALSE(uca_wildcmp(cs, "Ǎḅeçd", "a%bd"));
}
}
