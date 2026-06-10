/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>

#include "sql/hash.h"

#include "unittest/gunit/test_utils.h"

namespace hash_unittest {

using my_testing::Server_initializer;

class HashTest : public ::testing::Test {
 protected:
  void SetUp() override { initializer.SetUp(); }
  void TearDown() override { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};

TEST_F(HashTest, HashTestAll) {
  std::string s1{"test"};
  std::string s2{"test"};
  std::string s3{"test"};
  uint64_t num10 = 10;
  uint64_t num20 = 20;
  EXPECT_EQ(HashCString(s1.c_str()), HashCString(s2.c_str()));
  EXPECT_EQ(HashString(s1), HashString(s2));
  EXPECT_EQ(HashString(s1), HashCString(s2.c_str()));
  EXPECT_NE(HashNumber(num10), HashNumber(num20));
  EXPECT_NE(HashNumber(num20), HashString(s3));
  EXPECT_EQ(CombineCommutativeSigs(HashNumber(num20), HashString(s3)),
            CombineCommutativeSigs(HashString(s3), HashNumber(num20)));
  EXPECT_NE(CombineNonCommutativeSigs(HashNumber(num10), HashString(s3)),
            CombineNonCommutativeSigs(HashString(s3), HashNumber(num10)));
}

}  // namespace hash_unittest
