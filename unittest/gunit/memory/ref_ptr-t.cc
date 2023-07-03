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

#include <array>
#include <chrono>
#include <vector>

#include "sql/memory/ref_ptr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace memory {
namespace unittests {

class Ref_ptr_test : public ::testing::Test {
 protected:
  Ref_ptr_test() = default;
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Ref_ptr_test, Class_template_test) {
  std::string str{"012345678"};
  memory::Ref_ptr<std::string> ptr{str};
  EXPECT_EQ(ptr->length(), 9);
  EXPECT_EQ(*ptr, "012345678");

  memory::Ref_ptr<std::string> ptr2 = ptr;
  bool equal = (ptr == ptr2);
  EXPECT_EQ(equal, true);

  std::string str2{"012345678"};
  memory::Ref_ptr<std::string> ptr3{str2};
  equal = (ptr2 == ptr3);
  EXPECT_EQ(equal, false);

  ptr2.reset();
  equal = (ptr2 == nullptr);
  EXPECT_EQ(equal, true);
}

}  // namespace unittests
}  // namespace memory
