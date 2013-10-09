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
    std::pair<uint, uint> buffer_properties= fs_info.sort_buffer_properties();
    EXPECT_EQ(0U, buffer_properties.first);
    EXPECT_EQ(0U, buffer_properties.second);
    EXPECT_TRUE(NULL == fs_info.get_sort_keys());
  }

  Filesort_info fs_info;
};


TEST_F(FileSortBufferTest, FileSortBuffer)
{
  const char letters[10]= "abcdefghi";
  std::pair<uint, uint> buffer_properties= fs_info.sort_buffer_properties();
  EXPECT_EQ(0U, buffer_properties.first);
  EXPECT_EQ(0U, buffer_properties.second);

  uchar **sort_keys= fs_info.alloc_sort_buffer(10, sizeof(char));
  buffer_properties= fs_info.sort_buffer_properties();
  EXPECT_EQ(10U, buffer_properties.first);
  EXPECT_EQ(sizeof(char), buffer_properties.second);

  uchar **null_sort_keys= NULL;
  EXPECT_NE(null_sort_keys, sort_keys);
  EXPECT_NE(null_sort_keys, fs_info.get_sort_keys());
  for (uint ix= 0; ix < 10; ++ix)
  {
    uchar *ptr= fs_info.get_record_buffer(ix);
    *ptr= letters[ix];
  }
  uchar *data= *fs_info.get_sort_keys();
  const char *str= reinterpret_cast<const char*>(data);
  EXPECT_STREQ(letters, str);

  const size_t expected_size= 10 * (sizeof(char*) + sizeof(char));
  EXPECT_EQ(expected_size, fs_info.sort_buffer_size());
}


TEST_F(FileSortBufferTest, InitRecordPointers)
{
  fs_info.alloc_sort_buffer(10, sizeof(char));
  fs_info.init_record_pointers();
  uchar **ptr= fs_info.get_sort_keys();
  for (uint ix= 0; ix < 10 - 1; ++ix)
  {
    uchar **nxt= ptr + 1;
    EXPECT_EQ(1, *nxt - *ptr);
    ++ptr;
  }
}


TEST_F(FileSortBufferTest, AssignmentOperator)
{
  fs_info.alloc_sort_buffer(10, sizeof(char));
  Filesort_info fs_copy;
  fs_copy= fs_info;
  for (uint ix= 0; ix < 10 - 1; ++ix)
  {
    EXPECT_EQ(fs_copy.get_record_buffer(ix), fs_info.get_record_buffer(ix));
  }
  EXPECT_EQ(fs_copy.get_sort_keys(), fs_info.get_sort_keys());
  EXPECT_EQ(fs_copy.sort_buffer_size(), fs_info.sort_buffer_size());
}


}  // namespace
