/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <gtest/gtest.h>

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>

namespace mysys_my_symlink {

// For simplicity, we skip this test on Windows.
#if !defined(_WIN32)
TEST(Mysys, MysysMySymlink)
{
  char filename[FN_REFLEN];
  int fd= create_temp_file(filename, NULL, "gunit_mysys_symlink",
                           O_CREAT | O_WRONLY, MYF(MY_WME));
  EXPECT_GT(fd, 0);

  char linkname[FN_REFLEN];
  char *name_end= my_stpcpy(linkname, filename);
  (*name_end++) = 'S';
  *name_end= 0;
  int ret= my_symlink(filename, linkname, MYF(MY_WME));
  EXPECT_EQ(0, ret);

  char resolvedname[FN_REFLEN];
  ret= my_realpath(resolvedname, linkname, MYF(MY_WME));
  EXPECT_EQ(0, ret);

  // In case filename is also based on a symbolic link, like
  // for for example on Mac:  /var -> /private/var
  char resolved_filename[FN_REFLEN];
  ret= my_realpath(resolved_filename, filename, MYF(MY_WME));
  EXPECT_EQ(0, ret);

  EXPECT_STREQ(resolvedname, resolved_filename);

  ret= my_close(fd, MYF(MY_WME));
  EXPECT_EQ(0, ret);

  ret= my_delete_with_symlink(linkname, MYF(MY_WME));
  EXPECT_EQ(0, ret);
}
#endif

}
