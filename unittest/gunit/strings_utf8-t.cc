/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

}
