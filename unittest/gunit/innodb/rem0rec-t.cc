/* Copyright (c) 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "page0page.h"
#include "rem0rec.h"
#include "ut0new.h"

namespace innodb_rem0rec_unittest {

TEST(Rem0rec, CompactNextDisplacementBoundaries) {
  struct Test_case {
    uint16_t field_value;
    bool valid;
  };

  constexpr auto first_valid_positive = REC_N_NEW_EXTRA_BYTES + 1;
  constexpr auto last_valid_negative =
      static_cast<uint16_t>(-REC_N_NEW_EXTRA_BYTES - 1);
  constexpr auto first_invalid_negative =
      static_cast<uint16_t>(-REC_N_NEW_EXTRA_BYTES);

  constexpr Test_case test_cases[] = {
      {0, false},
      {REC_N_NEW_EXTRA_BYTES, false},
      {first_valid_positive, true},
      {(1U << 15) - 1, true},
      {1U << 15, true},
      {last_valid_negative, true},
      {first_invalid_negative, false},
      {UINT16_MAX, false},
  };

  for (const auto &test_case : test_cases) {
    EXPECT_EQ(test_case.valid,
              rec_is_valid_next_displacement(test_case.field_value))
        << test_case.field_value;
  }
}

class Rem0recPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_page = static_cast<page_t *>(
        ut::aligned_zalloc(UNIV_PAGE_SIZE, UNIV_PAGE_SIZE));
    ASSERT_NE(m_page, nullptr);
  }

  void TearDown() override { ut::aligned_free(m_page); }

  page_t *m_page{};
};

TEST_F(Rem0recPageTest, TryGetNextOffsRejectsShortDisplacements) {
  constexpr uint16_t rec_offset = 1024;
  constexpr auto last_valid_negative =
      static_cast<uint16_t>(-REC_N_NEW_EXTRA_BYTES - 1);
  constexpr auto first_invalid_negative =
      static_cast<uint16_t>(-REC_N_NEW_EXTRA_BYTES);
  auto *rec = m_page + rec_offset;

  mach_write_to_2(rec - REC_NEXT, 1);
  EXPECT_FALSE(rec_try_get_next_offs(rec, true).has_value());

  mach_write_to_2(rec - REC_NEXT, REC_N_NEW_EXTRA_BYTES);
  EXPECT_FALSE(rec_try_get_next_offs(rec, true).has_value());

  mach_write_to_2(rec - REC_NEXT, REC_N_NEW_EXTRA_BYTES + 1);
  EXPECT_TRUE(rec_try_get_next_offs(rec, true).has_value());

  mach_write_to_2(rec - REC_NEXT, last_valid_negative);
  EXPECT_TRUE(rec_try_get_next_offs(rec, true).has_value());

  mach_write_to_2(rec - REC_NEXT, first_invalid_negative);
  EXPECT_FALSE(rec_try_get_next_offs(rec, true).has_value());

  mach_write_to_2(rec - REC_NEXT, UINT16_MAX);
  EXPECT_FALSE(rec_try_get_next_offs(rec, true).has_value());
}

TEST_F(Rem0recPageTest, NextOffsetMustNotExceedHeapTop) {
  constexpr uint16_t rec_offset = 512;
  constexpr uint16_t heap_top = 1024;
  auto *rec = m_page + rec_offset;

  page_header_set_field(m_page, nullptr, PAGE_HEAP_TOP, heap_top);

  for (const bool comp : {false, true}) {
    SCOPED_TRACE(comp);
    const auto encode_target = [=](uint16_t target) {
      return comp ? target - rec_offset : target;
    };

    mach_write_to_2(rec - REC_NEXT, encode_target(heap_top - 1));
    EXPECT_TRUE(page_rec_try_get_next_offs(rec, comp).has_value());

    mach_write_to_2(rec - REC_NEXT, encode_target(heap_top));
    EXPECT_TRUE(page_rec_try_get_next_offs(rec, comp).has_value());

    mach_write_to_2(rec - REC_NEXT, encode_target(heap_top + 1));
    EXPECT_FALSE(page_rec_try_get_next_offs(rec, comp).has_value());
  }
}

}  // namespace innodb_rem0rec_unittest
