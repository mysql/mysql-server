/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <array>
#include <chrono>
#include <vector>

#include "sql/memory/aligned_atomic.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace memory {

struct Aligned_atomic_accessor_token {};

template <>
class Aligned_atomic_accessor<Aligned_atomic_accessor_token> {
 public:
  template <class T>
  static auto get_underlying(memory::Aligned_atomic<T> &atm) {
    return atm.m_underlying;
  }
};

using Aligned_atomic_accessor_ut =
    Aligned_atomic_accessor<Aligned_atomic_accessor_token>;

class AlignedAtomicTest : public ::testing::Test {
 protected:
  AlignedAtomicTest() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(AlignedAtomicTest, Class_template_test) {
  std::string str{"012345678"};
  memory::Aligned_atomic<int> atm{1};
  EXPECT_EQ(atm->load(), 1);
  atm->store(2);
  EXPECT_EQ((*atm).load(), 2);
  EXPECT_EQ(atm.size(), sizeof(std::atomic<int>));
  EXPECT_EQ(atm.allocated_size(), memory::cache_line_size());

  memory::Aligned_atomic<int> atm2{std::move(atm)};
  EXPECT_EQ(atm2->load(), 2);
  EXPECT_EQ(atm == nullptr, true);
  memory::Aligned_atomic<int> atm3;
  atm3 = std::move(atm2);
  EXPECT_EQ(atm3->load(), 2);
}

TEST_F(AlignedAtomicTest, MinimumCachelineFor) {
  EXPECT_EQ(memory::minimum_cacheline_for<char>(), memory::cache_line_size());
  EXPECT_EQ(memory::minimum_cacheline_for<int>(), memory::cache_line_size());
  EXPECT_EQ(memory::minimum_cacheline_for<std::atomic<bool>>(),
            memory::cache_line_size());
  EXPECT_EQ(memory::minimum_cacheline_for<std::atomic<int>>(),
            memory::cache_line_size());
}

TEST_F(AlignedAtomicTest, AlignedAllocation) {
  Aligned_atomic_accessor_ut accessor;
  memory::Aligned_atomic<int> atm1{1};
  EXPECT_EQ((unsigned long long)accessor.get_underlying(atm1) %
                memory::cache_line_size(),
            0);

  memory::Aligned_atomic<bool> atm2{true};
  EXPECT_EQ((unsigned long long)accessor.get_underlying(atm2) %
                memory::cache_line_size(),
            0);

  memory::Aligned_atomic<short> atm3{0};
  EXPECT_EQ((unsigned long long)accessor.get_underlying(atm3) %
                memory::cache_line_size(),
            0);
}

TEST_F(AlignedAtomicTest, AlignedAllocationArray) {
  Aligned_atomic_accessor_ut accessor;
  static const int array_size = 10;
  memory::Aligned_atomic<int> atm[array_size];

  for (int i = 0; i < array_size; i++)
    EXPECT_EQ((unsigned long long)accessor.get_underlying(atm[i]) %
                  memory::cache_line_size(),
              0);
}

}  // namespace memory
