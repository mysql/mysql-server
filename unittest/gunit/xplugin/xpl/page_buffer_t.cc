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

#include "plugin/x/ngs/include/ngs/protocol/page_buffer.h"

namespace ngs {
namespace tests {

class Page_buffer_testsuite : public ::testing::Test {
 public:
  const ngs::Pool_config m_default_pool_config = {0, 0, BUFFER_PAGE_SIZE};
  Page_pool m_pool{m_default_pool_config};
  Page_buffer m_sut{m_pool};
};

class Visitor_stat : public Page_visitor {
 public:
  bool visit(const char *, ssize_t size) override {
    m_tot_size += size;
    m_page_count++;
    return true;
  }

  ssize_t m_page_count = 0;
  ssize_t m_tot_size = 0;
};

TEST_F(Page_buffer_testsuite, not_used_object_has_no_pages_to_visit) {
  Visitor_stat stats;
  m_sut.visit(&stats);

  ASSERT_EQ(0, stats.m_page_count);
  ASSERT_EQ(0, stats.m_tot_size);
}

TEST_F(Page_buffer_testsuite, one_page_used_still_no_data_in_it) {
  Visitor_stat stats;
  ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
  m_sut.visit(&stats);

  ASSERT_EQ(0, stats.m_page_count);
  ASSERT_EQ(0, stats.m_tot_size);
}

TEST_F(
    Page_buffer_testsuite,
    tries_to_get_multiple_still_it_puts_not_data_thus_its_always_on_first_page) {
  const int k_pages_to_use = 10;
  Visitor_stat stats;

  ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
  auto *page = m_sut.get_current_page();
  for (int i = 1; i < k_pages_to_use; ++i) {
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    ASSERT_EQ(page, m_sut.get_current_page());
  }
  m_sut.visit(&stats);

  ASSERT_EQ(0, stats.m_page_count);
  ASSERT_EQ(0, stats.m_tot_size);
}  // namespace tests

TEST_F(Page_buffer_testsuite,
       multiple_page_used_has_same_amount_of_visited_pages_preserve_used_size) {
  const int k_pages_to_use = 10;
  int tot = 0;
  Visitor_stat stats;

  for (int i = 0; i < k_pages_to_use; ++i) {
    const int k_data_on_page = 10 * (i + 1);
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    m_sut.get_current_page()->data_length = k_data_on_page;
    tot += k_data_on_page;
  }
  m_sut.visit(&stats);

  ASSERT_EQ(k_pages_to_use, stats.m_page_count);
  ASSERT_EQ(tot, stats.m_tot_size);
}

TEST_F(Page_buffer_testsuite, multiple_page_with_data_still_reset_clears_them) {
  const int k_pages_to_use = 10;
  const int k_data_on_page = 5;
  Visitor_stat stats;

  for (int i = 0; i < k_pages_to_use; ++i) {
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    m_sut.get_current_page()->data_length = k_data_on_page;
  }
  m_sut.visit(&stats);

  ASSERT_EQ(k_pages_to_use, stats.m_page_count);
  ASSERT_EQ(k_data_on_page * k_pages_to_use, stats.m_tot_size);
  stats = Visitor_stat();

  m_sut.reset();
  m_sut.visit(&stats);
  ASSERT_EQ(0, stats.m_page_count);
  ASSERT_EQ(0, stats.m_tot_size);
}

TEST_F(Page_buffer_testsuite,
       multiple_page_with_data_and_restore_to_backuped_state) {
  const int k_pages_to_use = 10;
  int tot = 0;
  Visitor_stat stats;

  for (int i = 0; i < k_pages_to_use; ++i) {
    const int k_data_on_page = 10 * (i + 1);
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    m_sut.get_current_page()->data_length = k_data_on_page;
    tot += k_data_on_page;
  }
  const int k_total_before_backup = tot;

  m_sut.backup();

  for (int i = 0; i < k_pages_to_use; ++i) {
    const int k_data_on_page = 10 * (i + 1);
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    m_sut.get_current_page()->data_length = k_data_on_page;
    tot += k_data_on_page;
  }
  m_sut.visit(&stats);

  ASSERT_EQ(2 * k_pages_to_use, stats.m_page_count);
  ASSERT_EQ(tot, stats.m_tot_size);

  m_sut.restore();
  stats = Visitor_stat();
  m_sut.visit(&stats);

  ASSERT_EQ(k_total_before_backup, stats.m_tot_size);
  ASSERT_EQ(k_pages_to_use, stats.m_page_count);
}

TEST_F(
    Page_buffer_testsuite,
    multiple_page_with_data_and_restore_to_backuped_state_in_middle_of_page) {
  const int k_pages_to_use = 10;
  int tot = 0;
  Visitor_stat stats;

  for (int i = 0; i < k_pages_to_use; ++i) {
    const int k_data_on_page = 10 * (i + 1);
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    m_sut.get_current_page()->data_length = k_data_on_page;
    tot += k_data_on_page;
  }
  const int k_total_before_backup = tot;

  m_sut.backup();

  m_sut.get_current_page()->data_length += 100;
  tot += 100;

  for (int i = 0; i < k_pages_to_use; ++i) {
    const int k_data_on_page = 10 * (i + 1);
    ASSERT_TRUE(m_sut.move_to_next_page_if_not_empty());
    m_sut.get_current_page()->data_length = k_data_on_page;
    tot += k_data_on_page;
  }
  m_sut.visit(&stats);

  ASSERT_EQ(2 * k_pages_to_use, stats.m_page_count);
  ASSERT_EQ(tot, stats.m_tot_size);

  m_sut.restore();
  stats = Visitor_stat();
  m_sut.visit(&stats);

  ASSERT_EQ(k_total_before_backup, stats.m_tot_size);
  ASSERT_EQ(k_pages_to_use, stats.m_page_count);
}

}  // namespace tests
}  // namespace ngs
