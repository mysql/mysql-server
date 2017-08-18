/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stddef.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "my_sys.h"

#ifndef _WIN32

namespace mysys_my_freopen_unittest {
FILE *null_file= NULL;

class MysysMyFreopenTest : public ::testing::Test
{
public:
  FILE *stream;
  char name[32];
  MysysMyFreopenTest() :
    stream(NULL)
  {
    strncpy(name, "MyFreopen_XXXXXX", 32);
  }
  virtual void SetUp()
  {
    int fd= mkstemp(name);
    stream= fdopen(fd, "a");
  }
  virtual void TearDown()
  {
    if (stream != NULL)
    {
      fclose(stream);
      unlink(name);
    }
  }
};


// Test case demonstates that freopen is not atomic
TEST_F(MysysMyFreopenTest, FreopenFailure)
{
  EXPECT_EQ(null_file, freopen("/", "a", stream));
  const char *txt= "This text should end up in old stream file";
  EXPECT_EQ(EOF, fputs(txt, stream));
  EXPECT_NE(0, ferror(stream));
  EXPECT_EQ(0, fflush(stream));

  char buf[64]= "nada";
  FILE *instream= fopen(name, "r");
  EXPECT_NE(null_file, instream);
  EXPECT_EQ(static_cast<char*>(NULL), fgets(buf, 64, instream));
  EXPECT_EQ(0, ferror(instream));
  EXPECT_NE(0, feof(instream));
  EXPECT_STREQ("nada", buf);
  EXPECT_EQ(0, fclose(instream));
}

// Positive test case for the new version of my_freopen
TEST_F(MysysMyFreopenTest, MyFreopenOK)
{
  char fname[32] = "MyFreopenOK_XXXXXX";
  int fd= mkstemp(fname);

  EXPECT_EQ(stream, my_freopen(fname, "a", stream));
  const char *txt= "This text should end up in fname";
  int txtlen= strlen(txt);
  EXPECT_EQ(32, txtlen);
  EXPECT_LT(0, fputs(txt, stream));
  EXPECT_EQ(0, fflush(stream));

  char buf[64];
  FILE *instream= fdopen(fd, "r");
  EXPECT_NE(null_file, instream);
  EXPECT_EQ(buf, fgets(buf, 64, instream));
  EXPECT_STREQ(txt, buf);
  EXPECT_EQ(0, fclose(instream));
  EXPECT_EQ(0, unlink(fname));
}


// Negative test case for my_reopen. Shows that even if my_reopen
// fails, it is still possible to write to the stream.
TEST_F(MysysMyFreopenTest, MyFreopenFailure)
{
  EXPECT_EQ(null_file, my_freopen("/", "a", stream));
  const char *txt= "This text should end up in old stream file";
  EXPECT_LT(0, fputs(txt, stream));
  EXPECT_EQ(0, fflush(stream));

  char buf[64];
  FILE *instream= fopen(name, "r");
  EXPECT_NE(null_file, instream);
  EXPECT_EQ(buf, fgets(buf, 64, instream));
  EXPECT_STREQ(txt, buf);
  EXPECT_EQ(0, fclose(instream));
}
}
#endif
