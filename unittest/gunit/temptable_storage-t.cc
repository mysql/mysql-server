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
//#include <array> /* std::array */
//#include <new>   /* std::bad_alloc */

#include "storage/temptable/include/temptable/allocator.h" /* temptable::Allocator */
#include "storage/temptable/include/temptable/storage.h" /* temptable::Storage */

namespace temptable_storage_unittest {

TEST(temptable_storage, iterate) {
  {
    temptable::Allocator<uint8_t> allocator;
    temptable::Storage storage(&allocator);

    storage.element_size(sizeof(uint64_t));

    for (uint64_t i = 0; i < 10000; ++i) {
      *static_cast<uint64_t*>(storage.allocate_back()) = i;
    }

    uint64_t i = 0;
    for (auto it = storage.begin(); it != storage.end(); ++it, ++i) {
      EXPECT_EQ(i, *static_cast<uint64_t*>(*it));
    }

    i = storage.size();
    auto it = storage.end();
    for (; it != storage.begin();) {
      --it;
      --i;
      EXPECT_EQ(i, *static_cast<uint64_t*>(*it));
    }
    EXPECT_EQ(0u, i);
  }
  temptable::Allocator<uint8_t>::end_thread();
}

} /* namespace temptable_storage_unittest */
