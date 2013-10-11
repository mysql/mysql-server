/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

TEST_F(StringsUTF8Test, My_strchr)
{
  char* pos;
  char valid_utf8_string[]= "\u00E5y\u00E6";

  /*
    All valid utf8 characters in str arg passed to  my_strchr and  char to be
    found not in str.
  */
  pos= my_strchr(system_charset_info, valid_utf8_string,
                       valid_utf8_string+3,'y');

  EXPECT_STREQ(pos, valid_utf8_string+2);

  /*
    All valid utf8 characters in str arg passed to  my_strchr and  char to be
    found not in str.
  */
  pos= my_strchr(system_charset_info, valid_utf8_string,
                 valid_utf8_string+3,'d');

  ASSERT_TRUE(NULL == pos);

  // Assign an invalid utf8 char to valid_utf8_str
  valid_utf8_string[0]= 0xff;
  // Invalid utf8 character in str arg passed to my_strchr.
  pos= my_strchr(system_charset_info, valid_utf8_string,
                 valid_utf8_string+3,'y');
  ASSERT_TRUE(NULL == pos);

}

TEST_F(StringsUTF8Test, My_strcasecmp_mb)
{

  char utf8_src[]= "\u00E5y\u00E6";
  char utf8_dst[] = "\u00E5y\u00E6";

  // src and dst are equal utf8 strings
  EXPECT_EQ(my_strcasecmp_mb(system_charset_info, utf8_src, utf8_dst), 0);

  utf8_dst[1]= 'd';
  // src and dst are different utf8 strings
  EXPECT_EQ(my_strcasecmp_mb(system_charset_info, utf8_src, utf8_dst), 1);

  char invalid_utf8_dst[]= "\u00E5\u00FF\u00E6";
  // dst contain an invalid utf8 string
  EXPECT_EQ(my_strcasecmp_mb(system_charset_info, utf8_src, invalid_utf8_dst),
            1);
}
}
