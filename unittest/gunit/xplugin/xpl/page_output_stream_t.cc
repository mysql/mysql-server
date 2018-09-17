/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "plugin/x/ngs/include/ngs/protocol/page_output_stream.h"
#include "unittest/gunit/xplugin/xpl/protobuf_message.h"

using namespace ngs;

namespace im {

namespace tests {

class Page_output_stream_suite : public testing::Test {
 public:
  const ngs::Pool_config default_pool_config = {0, 0, BUFFER_PAGE_SIZE};
  Page_pool page_pool{default_pool_config};
  Page_output_stream stream{page_pool};
};

TEST_F(Page_output_stream_suite, Next) {
  ASSERT_EQ(0u, stream.ByteCount());

  void *ptr;
  int size;

  ASSERT_TRUE(stream.Next(&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);

  ASSERT_EQ(BUFFER_PAGE_SIZE, stream.ByteCount());
}

TEST_F(Page_output_stream_suite, Next_put_data_on_page) {
  uint8 *ptr;
  int size;
  const int data_size = 100;

  ASSERT_EQ(0u, stream.ByteCount());
  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);

  std::fill(ptr, ptr + data_size, 10u);
  stream.BackUp(size - data_size);

  ASSERT_EQ(data_size, stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(1, page.size());
  ASSERT_EQ(data_size, page[0].second);
  for (int i = 0; i < data_size; i++) {
    ASSERT_EQ(10u, page[0].first[i]);
  }
}

TEST_F(Page_output_stream_suite, Next_put_data_on_two_pages) {
  uint8 *ptr;
  int size;

  ASSERT_EQ(0u, stream.ByteCount());
  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);
  std::fill(ptr, ptr + BUFFER_PAGE_SIZE, 11u);
  ASSERT_EQ(BUFFER_PAGE_SIZE, stream.ByteCount());

  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);
  std::fill(ptr, ptr + BUFFER_PAGE_SIZE, 12u);
  ASSERT_EQ(2 * BUFFER_PAGE_SIZE, stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(2, page.size());
  ASSERT_EQ(BUFFER_PAGE_SIZE, page[0].second);
  ASSERT_EQ(BUFFER_PAGE_SIZE, page[1].second);
  for (int i = 0; i < BUFFER_PAGE_SIZE; i++) {
    ASSERT_EQ(11u, page[0].first[i]);
    ASSERT_EQ(12u, page[1].first[i]);
  }
}

TEST_F(Page_output_stream_suite, Backup_page_on_start_put_data_and_restore) {
  uint8 *ptr;
  int size;

  ASSERT_EQ(0u, stream.ByteCount());

  stream.backup_current_position();
  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);
  std::fill(ptr, ptr + BUFFER_PAGE_SIZE, 11u);
  ASSERT_EQ(BUFFER_PAGE_SIZE, stream.ByteCount());

  stream.restore_position();

  ASSERT_EQ(0, stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(0, page.size());
}

TEST_F(Page_output_stream_suite,
       Backup_page_on_start_put_data_on_two_pages_and_restore) {
  uint8 *ptr;
  int size;

  ASSERT_EQ(0u, stream.ByteCount());

  stream.backup_current_position();

  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);
  std::fill(ptr, ptr + BUFFER_PAGE_SIZE, 11u);
  ASSERT_EQ(BUFFER_PAGE_SIZE, stream.ByteCount());

  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);
  std::fill(ptr, ptr + BUFFER_PAGE_SIZE, 12u);
  ASSERT_EQ(2 * BUFFER_PAGE_SIZE, stream.ByteCount());

  stream.restore_position();

  ASSERT_EQ(0, stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(0, page.size());
}

TEST_F(Page_output_stream_suite, Backup_page_on_first_page_and_restore) {
  uint8 *ptr;
  int size;

  ASSERT_EQ(0u, stream.ByteCount());

  const int data_on_first_page = 10;
  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE, size);
  std::fill(ptr, ptr + data_on_first_page, 11u);
  ASSERT_EQ(BUFFER_PAGE_SIZE, stream.ByteCount());
  stream.BackUp(size - data_on_first_page);
  ASSERT_EQ(data_on_first_page, stream.ByteCount());
  stream.backup_current_position();

  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  ASSERT_EQ(BUFFER_PAGE_SIZE - data_on_first_page, size);
  ASSERT_EQ(BUFFER_PAGE_SIZE, stream.ByteCount());

  stream.restore_position();
  ASSERT_EQ(data_on_first_page, stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(1, page.size());
  ASSERT_EQ(data_on_first_page, page[0].second);
  for (int i = 0; i < data_on_first_page; i++) {
    ASSERT_EQ(11u, page[0].first[i]);
  }
}

TEST_F(Page_output_stream_suite, Allow_aliasing) {
  ASSERT_FALSE(stream.AllowsAliasing());
}

static bool WriteDataOnSinglePage(Page_output_stream *stream, const void *data,
                                  int size) {
  auto ptr = stream->reserve_space(size);

  if (!ptr) return false;

  memcpy(ptr, data, size);

  return true;
}

TEST_F(Page_output_stream_suite, Write_on_the_same_page) {
  const int data_commited_with_next = 10;
  const int data_commited_with_write_alias = 10;
  int size;
  uint8 *ptr;

  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  stream.BackUp(size - data_commited_with_next);
  std::fill(ptr, ptr + data_commited_with_next, 11u);
  ASSERT_EQ(data_commited_with_next, stream.ByteCount());

  std::string expected_data(data_commited_with_write_alias, (char)12);
  ASSERT_TRUE(WriteDataOnSinglePage(&stream, expected_data.c_str(),
                                    expected_data.length()));

  ASSERT_EQ(data_commited_with_write_alias + data_commited_with_next,
            stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(1, page.size());
  ASSERT_EQ(data_commited_with_write_alias + data_commited_with_next,
            page[0].second);
  for (int i = 0; i < data_commited_with_next; i++) {
    ASSERT_EQ(11u, page[0].first[i]);
  }

  for (int i = data_commited_with_next;
       i < data_commited_with_next + data_commited_with_write_alias; i++) {
    ASSERT_EQ(12u, page[0].first[i]);
  }
}

TEST_F(Page_output_stream_suite, Write_too_many_data) {
  const std::string expected_data(2 * BUFFER_PAGE_SIZE, ' ');

  ASSERT_FALSE(WriteDataOnSinglePage(&stream, expected_data.c_str(),
                                     expected_data.length()));
}

TEST_F(Page_output_stream_suite, Write_on_next_page) {
  const int data_commited_with_next = BUFFER_PAGE_SIZE - 5;
  const int data_commited_with_write_alias = 10;
  int size;
  uint8 *ptr;

  ASSERT_TRUE(stream.Next((void **)&ptr, &size));
  stream.BackUp(size - data_commited_with_next);
  std::fill(ptr, ptr + data_commited_with_next, 11u);
  ASSERT_EQ(data_commited_with_next, stream.ByteCount());

  std::string expected_data(data_commited_with_write_alias, (char)12);
  ASSERT_TRUE(WriteDataOnSinglePage(&stream, expected_data.c_str(),
                                    expected_data.length()));

  ASSERT_EQ(data_commited_with_write_alias + data_commited_with_next,
            stream.ByteCount());

  const auto page = xpl::test::get_pages_from_stream(&stream);

  ASSERT_EQ(2, page.size());
  ASSERT_EQ(data_commited_with_next, page[0].second);
  for (int i = 0; i < data_commited_with_next; i++) {
    ASSERT_EQ(11u, page[0].first[i]);
  }

  for (int i = 0; i < data_commited_with_write_alias; i++) {
    ASSERT_EQ(12u, page[1].first[i]);
  }
}

}  // namespace tests
}  // namespace im
