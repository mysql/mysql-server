/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include <memory>

#include <gtest/gtest.h>

#include "sql/cmp_varlen_keys.h"
#include "sql/sort_param.h"

namespace filesort_mergechunk_unittest {

class FileSortMergeChunkTest : public ::testing::Test {
 public:
  void SetUp() override {
    memset(&m_sort_field, 0, sizeof(m_sort_field));
    m_sort_field.is_varlen = true;
    m_param.init_for_unittest(make_array(&m_sort_field, 1));
    m_mcg = std::make_unique<Merge_chunk_greater>(&m_param);
  }

 protected:
  st_sort_field m_sort_field;
  Sort_param m_param;
  std::unique_ptr<Merge_chunk_greater> m_mcg;
};

TEST_F(FileSortMergeChunkTest, BasicCompareOperations) {
  EXPECT_TRUE(m_param.using_varlen_keys());

  uchar chunk_a_buf[10];
  uchar chunk_b_buf[10];

  Merge_chunk chunk_a;
  Merge_chunk chunk_b;
  chunk_a.set_buffer_start(chunk_a_buf);
  chunk_b.set_buffer_start(chunk_b_buf);
  chunk_a.init_current_key();
  chunk_b.init_current_key();

  int4store(chunk_a_buf + 4, 1 + VARLEN_PREFIX);
  int4store(chunk_b_buf + 4, 1 + VARLEN_PREFIX);
  chunk_a_buf[4 + VARLEN_PREFIX] = 1;
  chunk_b_buf[4 + VARLEN_PREFIX] = 1;

  EXPECT_FALSE((*m_mcg)(&chunk_a, &chunk_b));
  EXPECT_FALSE((*m_mcg)(&chunk_b, &chunk_a));

  chunk_a_buf[4 + VARLEN_PREFIX] = 0;
  EXPECT_FALSE((*m_mcg)(&chunk_a, &chunk_b));
  EXPECT_TRUE((*m_mcg)(&chunk_b, &chunk_a));
}

}  // namespace filesort_mergechunk_unittest
