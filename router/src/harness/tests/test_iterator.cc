/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "utilities.h"

////////////////////////////////////////
// Standard include files
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

////////////////////////////////////////
// Third-party include files
#include <gtest/gtest.h>

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

using mysql_harness::utility::make_range;

TEST(TestIterator, TestIterator) {
  static const char *array[] = {
      "one",
      "two",
      "three",
  };
  const size_t array_length = sizeof(array) / sizeof(*array);
  const char **ptr = array;

  auto range = make_range(array, array_length);
  for (auto elem : range) {
    EXPECT_EQ(elem, *ptr);
    EXPECT_LT(ptr - array, static_cast<long>(sizeof(array) / sizeof(*array)));
    ++ptr;
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
