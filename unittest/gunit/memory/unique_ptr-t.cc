/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/memory/unique_ptr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace memory {
namespace unittests {

class Unique_ptr_test : public ::testing::Test {
 protected:
  Unique_ptr_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Unique_ptr_test, Array_template_test) {
  auto ptr = memory::make_unique<char[]>(10);

  ptr[0] = '0';
  ptr[1] = '1';
  ptr[2] = '2';
  ptr[3] = '3';
  ptr[4] = '4';
  ptr[5] = '5';
  ptr[6] = '6';
  ptr[7] = '7';
  ptr[8] = '8';
  ptr[9] = '\0';

  EXPECT_EQ(ptr[2], '2');
  EXPECT_EQ(*ptr, '0');
  EXPECT_EQ(!ptr, false);

  EXPECT_EQ(*ptr.get(), '0');
  EXPECT_EQ(ptr.size(), 10);

  ptr.reserve(20);
  EXPECT_EQ(ptr.size(), 20);
  ptr[9] = '9';
  ptr[10] = '0';
  ptr[11] = '1';
  ptr[12] = '2';
  ptr[13] = '3';
  ptr[14] = '4';
  ptr[15] = '5';
  ptr[16] = '6';
  ptr[17] = '7';
  ptr[18] = '8';
  ptr[19] = '\0';

  EXPECT_EQ(ptr[12], '2');
  EXPECT_EQ(*ptr.get(), '0');
  EXPECT_EQ(ptr.size(), 20);

  char *underlying = ptr.release();
  EXPECT_EQ(!ptr, true);
  delete[] underlying;

  auto ptr2 = memory::make_unique<char[]>(10);
  bool equal = (ptr == ptr2);
  EXPECT_EQ(equal, false);
}

TEST_F(Unique_ptr_test, Class_template_test) {
  auto ptr = memory::make_unique<std::string>("012345678");

  EXPECT_EQ(ptr->length(), 9);
  EXPECT_EQ(*ptr, "012345678");
  EXPECT_EQ(!ptr, false);

  EXPECT_EQ(*ptr.get(), "012345678");
  EXPECT_EQ(ptr.size(), sizeof(std::string));

  std::string *underlying = ptr.release();
  EXPECT_EQ(!ptr, true);
  delete underlying;

  auto ptr2 = memory::make_unique<std::string>("012345678");
  bool equal = (ptr == ptr2);
  EXPECT_EQ(equal, false);
}

}  // namespace unittests
}  // namespace memory
