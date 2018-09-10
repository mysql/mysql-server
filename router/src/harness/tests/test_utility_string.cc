/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <gmock/gmock.h>

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <set>
#include <unordered_set>
#include <vector>

#include "mysql/harness/utility/string.h"

using mysql_harness::join;

template <typename T>
class JoinTest : public ::testing::Test {};

TYPED_TEST_CASE_P(JoinTest);

TYPED_TEST_P(JoinTest, many) {
  EXPECT_EQ(join(std::array<TypeParam, 2>{"abc", "def"}, "-"), "abc-def");
  EXPECT_EQ(join(std::deque<TypeParam>{"abc", "def"}, "-"), "abc-def");
  EXPECT_EQ(join(std::forward_list<TypeParam>{"abc", "def"}, "-"), "abc-def");
  EXPECT_EQ(join(std::list<TypeParam>{"abc", "def"}, "-"), "abc-def");

#if 0
  // - a std::set<const char *> comparse pointer-addresses, iteration may
  // returned in any order a
  // - std::unordered_set<const char *> has no ordering
  EXPECT_EQ(join(std::set<TypeParam>{"abc", "def"}, "-"), "abc-def");
  EXPECT_EQ(join(std::unordered_set<TypeParam>{"abc", "def"}, "-"), "abc-def");
#endif
  EXPECT_EQ(join(std::vector<TypeParam>{"abc", "def"}, "-"), "abc-def");
}

TYPED_TEST_P(JoinTest, one) {
  EXPECT_EQ(join(std::array<TypeParam, 1>{"abc"}, "-"), "abc");
  EXPECT_EQ(join(std::deque<TypeParam>{"abc"}, "-"), "abc");
  EXPECT_EQ(join(std::forward_list<TypeParam>{"abc"}, "-"), "abc");
  EXPECT_EQ(join(std::list<TypeParam>{"abc"}, "-"), "abc");
  EXPECT_EQ(join(std::set<TypeParam>{"abc"}, "-"), "abc");
  EXPECT_EQ(join(std::unordered_set<TypeParam>{"abc"}, "-"), "abc");
  EXPECT_EQ(join(std::vector<TypeParam>{"abc"}, "-"), "abc");
}

TYPED_TEST_P(JoinTest, none) {
  EXPECT_EQ(join(std::array<TypeParam, 0>{}, "-"), "");
  EXPECT_EQ(join(std::deque<TypeParam>{}, "-"), "");
  EXPECT_EQ(join(std::forward_list<TypeParam>{}, "-"), "");
  EXPECT_EQ(join(std::list<TypeParam>{}, "-"), "");
  EXPECT_EQ(join(std::set<TypeParam>{}, "-"), "");
  EXPECT_EQ(join(std::unordered_set<TypeParam>{}, "-"), "");
  EXPECT_EQ(join(std::vector<TypeParam>{}, "-"), "");
}

REGISTER_TYPED_TEST_CASE_P(JoinTest, many, one, none);

using JoinTestTypes = ::testing::Types<std::string, const char *>;
INSTANTIATE_TYPED_TEST_CASE_P(Spec, JoinTest, JoinTestTypes);

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
