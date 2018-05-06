/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <stddef.h>
#include <sys/types.h>
#include <utility>

#include "my_inttypes.h"
#include "my_pointer_arithmetic.h"
#include "sql/filesort_utils.h"
#include "sql/table.h"

namespace filesort_buffer_unittest {

class FileSortBufferTest : public ::testing::Test {
 protected:
  virtual void TearDown() {
    fs_info.free_sort_buffer();
    EXPECT_TRUE(NULL == fs_info.get_sort_keys());
  }

  Filesort_buffer fs_info;
};

TEST_F(FileSortBufferTest, FileSortBuffer) {
  const char letters[10] = "abcdefghi";

  uchar *sort_buff = fs_info.alloc_sort_buffer(10, sizeof(char));

  const uchar *null_sort_buff = NULL;
  uchar **null_sort_keys = NULL;
  fs_info.init_record_pointers();
  EXPECT_NE(null_sort_buff, sort_buff);
  EXPECT_NE(null_sort_keys, fs_info.get_sort_keys());
  for (uint ix = 0; ix < 10; ++ix) {
    uchar *ptr = fs_info.get_sort_keys()[ix];
    *ptr = letters[ix];
  }
  uchar *data = *fs_info.get_sort_keys();
  const char *str = reinterpret_cast<const char *>(data);
  EXPECT_STREQ(letters, str);

  const size_t expected_size = ALIGN_SIZE(10 * (sizeof(char *) + sizeof(char)));
  EXPECT_EQ(expected_size, fs_info.sort_buffer_size());

  // On 64bit systems, the buffer is full, on 32bit it is not (still 6 bytes
  // left).
  if (sizeof(double) == sizeof(char *))
    EXPECT_TRUE(fs_info.isfull());
  else
    EXPECT_FALSE(fs_info.isfull());
}

TEST_F(FileSortBufferTest, InitRecordPointers) {
  fs_info.alloc_sort_buffer(10, sizeof(char));
  fs_info.init_record_pointers();
  uchar **ptr = fs_info.get_sort_keys();
  for (uint ix = 0; ix < 10 - 1; ++ix) {
    uchar **nxt = ptr + 1;
    EXPECT_EQ(1, *nxt - *ptr) << "index:" << ix;
    ++ptr;
  }
}

TEST_F(FileSortBufferTest, GetNextRecordPointer) {
  fs_info.alloc_sort_buffer(8, sizeof(int));
  fs_info.init_next_record_pointer();
  size_t spaceleft = 8 * (sizeof(int) + sizeof(uchar *));
  EXPECT_EQ(spaceleft, fs_info.spaceleft());

  uchar *first_record = fs_info.get_next_record_pointer();
  Bounds_checked_array<uchar> raw_buf = fs_info.get_raw_buf();
  EXPECT_EQ(first_record, raw_buf.array());
  spaceleft -= (sizeof(int) + sizeof(uchar *));
  EXPECT_EQ(spaceleft, fs_info.spaceleft());

  fs_info.adjust_next_record_pointer(2);
  spaceleft += 2;
  EXPECT_EQ(spaceleft, fs_info.spaceleft());

  uchar *second_record = fs_info.get_next_record_pointer();
  EXPECT_NE(first_record, second_record);

  fs_info.reverse_record_pointers();
  EXPECT_EQ(first_record, fs_info.get_sort_keys()[0]);
  EXPECT_EQ(second_record, fs_info.get_sort_keys()[1]);
}

}  // namespace filesort_buffer_unittest
