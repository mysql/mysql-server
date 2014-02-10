/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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
#include <utility>

#include "filesort_utils.h"
#include "table.h"


namespace filesort_buffer_unittest {

class FileSortBufferTest : public ::testing::Test
{
protected:
  virtual void TearDown()
  {
    fs_info.free_sort_buffer();
    EXPECT_TRUE(NULL == fs_info.get_sort_keys());
  }

  Filesort_buffer fs_info;
};


TEST_F(FileSortBufferTest, FileSortBuffer)
{
  const char letters[10]= "abcdefghi";

  uchar *sort_buff= fs_info.alloc_sort_buffer(10, sizeof(char));

  const uchar *null_sort_buff= NULL;
  uchar **null_sort_keys= NULL;
  fs_info.init_record_pointers();
  EXPECT_NE(null_sort_buff, sort_buff);
  EXPECT_NE(null_sort_keys, fs_info.get_sort_keys());
  for (uint ix= 0; ix < 10; ++ix)
  {
    uchar *ptr= fs_info.get_sort_keys()[ix];
    *ptr= letters[ix];
  }
  uchar *data= *fs_info.get_sort_keys();
  const char *str= reinterpret_cast<const char*>(data);
  EXPECT_STREQ(letters, str);

  const size_t expected_size= ALIGN_SIZE(10 * (sizeof(char*) + sizeof(char)));
  EXPECT_EQ(expected_size, fs_info.sort_buffer_size());

  // On 64bit systems, the buffer is full, on 32bit it is not (still 6 bytes left).
  if (sizeof(double) == sizeof(char*))
    EXPECT_TRUE(fs_info.isfull());
  else
    EXPECT_FALSE(fs_info.isfull());
}


TEST_F(FileSortBufferTest, InitRecordPointers)
{
  fs_info.alloc_sort_buffer(10, sizeof(char));
  fs_info.init_record_pointers();
  uchar **ptr= fs_info.get_sort_keys();
  for (uint ix= 0; ix < 10 - 1; ++ix)
  {
    uchar **nxt= ptr + 1;
    EXPECT_EQ(1, *nxt - *ptr) << "index:" << ix;
    ++ptr;
  }
}


TEST_F(FileSortBufferTest, GetNextRecordPointer)
{
  fs_info.alloc_sort_buffer(8, sizeof(int));
  fs_info.init_next_record_pointer();
  size_t spaceleft= 8 * (sizeof(int) + sizeof(uchar*));
  EXPECT_EQ(spaceleft, fs_info.spaceleft());

  uchar *first_record= fs_info.get_next_record_pointer();
  Bounds_checked_array<uchar> raw_buf= fs_info.get_raw_buf();
  EXPECT_EQ(first_record, raw_buf.array());
  spaceleft-= (sizeof(int) + sizeof(uchar*));
  EXPECT_EQ(spaceleft, fs_info.spaceleft());

  fs_info.adjust_next_record_pointer(2);
  spaceleft+= 2;
  EXPECT_EQ(spaceleft, fs_info.spaceleft());

  uchar *second_record= fs_info.get_next_record_pointer();
  EXPECT_NE(first_record, second_record);

  fs_info.reverse_record_pointers();
  EXPECT_EQ(first_record, fs_info.get_sort_keys()[0]);
  EXPECT_EQ(second_record, fs_info.get_sort_keys()[1]);
}


TEST_F(FileSortBufferTest, AssignmentOperator)
{
  fs_info.alloc_sort_buffer(10, sizeof(char));
  fs_info.init_record_pointers();
  Filesort_buffer fs_copy;
  fs_copy= fs_info;
  for (uint ix= 0; ix < 10 - 1; ++ix)
  {
    EXPECT_EQ(fs_copy.get_sort_keys()[ix], fs_info.get_sort_keys()[ix]);
  }
  EXPECT_EQ(fs_copy.get_sort_keys(), fs_info.get_sort_keys());
  EXPECT_EQ(fs_copy.sort_buffer_size(), fs_info.sort_buffer_size());
}


}  // namespace
