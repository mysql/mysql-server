/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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

} /* namespace temptable_allocator_unittest */
