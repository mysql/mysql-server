/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <array> /* std::array */
#include <new>   /* std::bad_alloc */

#include "storage/temptable/include/temptable/allocator.h" /* temptable::Allocator */
#include "storage/temptable/include/temptable/constants.h" /* temptable::ALLOCATOR_MAX_BLOCK_BYTES */

namespace temptable_allocator_unittest {

TEST(temptable_allocator, basic)
{
  {
    temptable::Allocator<uint64_t> allocator;

    constexpr size_t n_allocate = 128;
    std::array<uint64_t*, n_allocate> a;
    constexpr size_t n_elements = 16;

    for (size_t i = 0; i < n_allocate; ++i) {
      a[i] = allocator.allocate(n_elements);

      for (size_t j = 0; j < n_elements; ++j) {
        a[i][j] = 0xF00BA4C0FFEE1234;
      }
    }

    for (size_t i = 0; i < n_allocate; ++i) {
      allocator.deallocate(a[i], n_elements);
    }
  }
  temptable::Allocator<uint64_t>::end_thread();
}

TEST(temptable_allocator, edge)
{
  temptable::Allocator<uint8_t> allocator;

  using namespace temptable;

  EXPECT_EQ(nullptr, allocator.allocate(0));

#ifndef DBUG_OFF
  DBUG_SET("+d,temptable_allocator_oom");
  bool thrown = false;
  try {
    allocator.allocate(8_MiB);
  } catch (Result) {
    thrown = true;
  }
  EXPECT_EQ(true, thrown);
  DBUG_SET("-d,temptable_allocator_oom");
#endif /* DBUG_OFF */
}

TEST(temptable_allocator, block_size_cap)
{
  {
    temptable::Allocator<uint8_t> allocator;

    using namespace temptable;

    constexpr size_t alloc_size = 1_MiB;
    constexpr size_t n_allocate = ALLOCATOR_MAX_BLOCK_BYTES / alloc_size + 10;
    std::array<uint8_t*, n_allocate> a;

    for (size_t i = 0; i < n_allocate; ++i) {
      a[i] = allocator.allocate(alloc_size);
    }

    for (size_t i = 0; i < n_allocate; ++i) {
      allocator.deallocate(a[i], alloc_size);
    }
  }
  temptable::Allocator<uint8_t>::end_thread();
}

} /* namespace temptable_allocator_unittest */
