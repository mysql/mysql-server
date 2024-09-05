/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* See http://code.google.com/p/googletest/wiki/Primer */

#include <gtest/gtest.h>

#include "storage/innobase/include/ut0bitset.h"

namespace innodb_ut0bitset_unittest {

TEST(ut0bitset, find_set_handles_misaligned_ranges) {
  alignas(64) byte base_data[1000] = {};
  for (int a = 0; a < 8; ++a) {
    for (int len = 0; len < 10; ++len) {
      Bitset bs(base_data + a, len);
      for (int b = 0; b < len * 8; ++b) {
        assert(!bs.test(b));
        base_data[a + b / 8] ^= 1U << b % 8;
        assert(bs.test(b));
        for (int q = 0; q < len * 8; ++q) {
          auto found = bs.find_set(q);
          if (q <= b) {
            ASSERT_EQ(found, b);
          } else {
            ASSERT_EQ(found, bs.NOT_FOUND);
          }
        }
        base_data[a + b / 8] ^= 1U << b % 8;
      }
    }
  }
}

}  // namespace innodb_ut0bitset_unittest
