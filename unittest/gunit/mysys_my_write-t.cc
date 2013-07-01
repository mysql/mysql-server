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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Ignore test on windows, as we are mocking away a unix function, see below.
#ifndef _WIN32
namespace mysys_my_write_unittest {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetErrnoAndReturn;

class MockWrite
{
public:
  virtual ~MockWrite() {}
  MOCK_METHOD3(mockwrite, ssize_t(int, const void *, size_t));
};

MockWrite *mockfs= NULL;

// We need to mock away write(2), do it with a macro:
#define write(fd, buf, count) mockfs->mockwrite(fd, buf, count)

/*
  Include the source file, which will give us
  mysys_my_write_unittest::my_write() for testing.
*/
#include "../../mysys/my_write.c"

#undef write

class MysysMyWriteTest : public ::testing::Test
{
  virtual void SetUp()
  {
    mockfs= new MockWrite;
  }
  virtual void TearDown()
  {
    delete mockfs;
    mockfs= NULL;
  }
};


// Test of normal case: write OK
TEST_F(MysysMyWriteTest, MyWriteOK)
{
  uchar buf[4096];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(Return(4096));

  const size_t result= my_write(42, buf, 4096, 0);
  EXPECT_EQ(4096U, result);
}


// Test of normal case: write OK with MY_NABP
TEST_F(MysysMyWriteTest, MyWriteOKNABP)
{
  uchar buf[4096];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(Return(4096));

  const size_t result= my_write(42, buf, 4096, MYF(MY_NABP));
  EXPECT_EQ(0U, result);
}


// Test of disk full: write not OK
TEST_F(MysysMyWriteTest, MyWriteFail)
{
  uchar buf[4096];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(ENOSPC, -1));

  const size_t result= my_write(42, buf, 4096, 0);
  EXPECT_EQ(MY_FILE_ERROR, result);
}


// Test of disk full: write not OK, with MY_NABP
TEST_F(MysysMyWriteTest, MyWriteFailNABP)
{
  uchar buf[4096];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(ENOSPC, -1));

  const size_t result= my_write(42, buf, 4096, MYF(MY_NABP));
  EXPECT_EQ(MY_FILE_ERROR, result);
}


// Test of disk full after partial write.
TEST_F(MysysMyWriteTest, MyWrite8192)
{
  uchar buf[8192];
  InSequence s;
  // Expect call to write 8192 bytes, return 4096.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 8192))
    .Times(1)
    .WillOnce(Return(4096));
  // Expect second call to write remaining 4096 bytes, return disk full.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(ENOSPC, -1));

  const size_t result= my_write(42, buf, 8192, 0);
  EXPECT_EQ(4096U, result);
}


// Test of disk full after partial write.
TEST_F(MysysMyWriteTest, MyWrite8192NABP)
{
  uchar buf[8192];
  InSequence s;
  // Expect call to write 8192 bytes, return 4096.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 8192))
    .Times(1)
    .WillOnce(Return(4096));
  // Expect second call to write remaining 4096 bytes, return disk full.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(ENOSPC, -1));

  const size_t result= my_write(42, buf, 8192, MYF(MY_NABP));
  EXPECT_EQ(MY_FILE_ERROR, result);
}


// Test of partial write, followed by interrupt, followed by successful write.
TEST_F(MysysMyWriteTest, MyWrite8192Interrupt)
{
  uchar buf[8192];
  InSequence s;
  // Expect call to write 8192 bytes, return 4096.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 8192))
    .Times(1)
    .WillOnce(Return(4096));
  // Expect second call to write remaining 4096 bytes, return interrupt.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(EINTR, -1));
  // Expect third call to write remaining 4096 bytes, return 4096.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(Return(4096));

  const size_t result= my_write(42, buf, 8192, 0);
  EXPECT_EQ(8192U, result);
}


// Test of partial write, followed by interrupt, followed by successful write.
TEST_F(MysysMyWriteTest, MyWrite8192InterruptNABP)
{
  uchar buf[8192];
  InSequence s;
  // Expect call to write 8192 bytes, return 4096.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 8192))
    .Times(1)
    .WillOnce(Return(4096));
  // Expect second call to write remaining 4096 bytes, return interrupt.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(EINTR, -1));
  // Expect third call to write remaining 4096 bytes, return 4096.
  EXPECT_CALL(*mockfs, mockwrite(_, _, 4096))
    .Times(1)
    .WillOnce(Return(4096));

  const size_t result= my_write(42, buf, 8192, MYF(MY_NABP));
  EXPECT_EQ(0U, result);
}


// Test of partial write, followed successful write.
TEST_F(MysysMyWriteTest, MyWrite400)
{
  uchar buf[400];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 400))
    .Times(1)
    .WillOnce(Return(200));
  EXPECT_CALL(*mockfs, mockwrite(_, _, 200))
    .Times(1)
    .WillOnce(Return(200));

  const size_t result= my_write(42, buf, 400, 0);
  EXPECT_EQ(400U, result);
}


// Test of partial write, followed successful write.
TEST_F(MysysMyWriteTest, MyWrite400NABP)
{
  uchar buf[400];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 400))
    .Times(1)
    .WillOnce(Return(200));
  EXPECT_CALL(*mockfs, mockwrite(_, _, 200))
    .Times(1)
    .WillOnce(Return(200));

  const size_t result= my_write(42, buf, 400, MYF(MY_NABP));
  EXPECT_EQ(0U, result);
}


// Test of partial write, followed by failure, followed successful write.
TEST_F(MysysMyWriteTest, MyWrite300)
{
  uchar buf[300];
  InSequence s;
  EXPECT_CALL(*mockfs, mockwrite(_, _, 300))
    .Times(1)
    .WillOnce(Return(100));
  EXPECT_CALL(*mockfs, mockwrite(_, _, 200))
    .Times(1)
    .WillOnce(SetErrnoAndReturn(EAGAIN, 0));
  EXPECT_CALL(*mockfs, mockwrite(_, _, 200))
    .Times(1)
    .WillOnce(Return(200));

  const size_t result= my_write(42, buf, 300, 0);
  EXPECT_EQ(300U, result);
}

}
#endif
