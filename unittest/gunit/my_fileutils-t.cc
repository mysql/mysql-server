/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "my_global.h"
#include "my_sys.h"
#include "mysql/psi/mysql_file.h"

#if !defined(__WIN__)
TEST(FileUtilsTest, TellPipe)
{
  int pipefd[2];
  EXPECT_EQ(0, pipe(pipefd));
  my_off_t pos= mysql_file_tell(pipefd[1], MYF(0));
  EXPECT_EQ(MY_FILEPOS_ERROR, pos);
  EXPECT_EQ(ESPIPE, my_errno);
  EXPECT_EQ(0, close(pipefd[0]));
  EXPECT_EQ(0, close(pipefd[1]));
}
#endif
