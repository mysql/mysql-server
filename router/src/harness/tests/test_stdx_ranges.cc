/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/ranges.h"

#include <array>
#include <forward_list>
#include <initializer_list>
#include <iterator>
#include <list>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>

#include "mysql/harness/stdx/type_traits.h"

using ::testing::ElementsAre;
using ::testing::FieldsAre;

static_assert(
    std::is_same_v<
        stdx::indirectly_readable_traits<std::vector<int>>::value_type, int>);
static_assert(
    std::is_same_v<
        stdx::indirectly_readable_traits<std::array<int, 1>>::value_type, int>);
static_assert(
    std::is_same_v<stdx::indirectly_readable_traits<int[]>::value_type, int>);

static_assert(stdx::impl::has_reference<std::list<int>>::value);
static_assert(std::is_same_v<stdx::iter_reference_t<std::list<int>>,
                             std::list<int>::reference>);
static_assert(std::is_same_v<stdx::iter_reference_t<std::list<int> &>,
                             std::list<int>::reference>);
static_assert(std::is_same_v<stdx::iter_reference_t<std::list<int> &>,
                             std::list<int>::reference>);
static_assert(std::is_same_v<stdx::iter_reference_t<const std::list<int> &>,
                             std::list<int>::reference>);
static_assert(std::is_same_v<stdx::ranges::range_reference_t<std::list<int>>,
                             std::list<int>::reference>);
static_assert(
    std::is_same_v<stdx::ranges::range_reference_t<const std::list<int> &>,
                   std::list<int>::const_reference>);
static_assert(
    std::is_same_v<stdx::ranges::range_value_t<const std::list<int> &>, int>);
static_assert(std::is_same_v<stdx::ranges::range_value_t<std::list<int>>, int>);

template <typename T>
class EnumerateTest : public ::testing::Test {};

using EnumerateTypes =
    ::testing::Types<int[], std::vector<int>, std::array<int, 3>,
                     std::list<int>, std::forward_list<int>,
                     std::initializer_list<int>>;
TYPED_TEST_SUITE(EnumerateTest, EnumerateTypes);

TYPED_TEST(EnumerateTest, enumerate_empty) {
  if constexpr (std::is_same_v<TypeParam, int[]> ||
                std::is_same_v<TypeParam, std::array<int, 3>>) {
    GTEST_SKIP() << "can't have zero elements.";
  } else {
    TypeParam v{};

    auto it = stdx::views::enumerate(v);
    EXPECT_FALSE(it.begin() != it.end());
  }
}

TYPED_TEST(EnumerateTest, enumerate_ref) {
  TypeParam v{1, 3, 5};

  EXPECT_THAT(
      stdx::views::enumerate(v),
      ::testing::ElementsAre(std::make_tuple(0, 1), std::make_tuple(1, 3),
                             std::make_tuple(2, 5)));
}

TYPED_TEST(EnumerateTest, enumerate_some_constref) {
  if constexpr (std::is_same_v<TypeParam, int[]>) {
    GTEST_SKIP() << "missing overload";
  } else {
    EXPECT_THAT(
        stdx::views::enumerate(TypeParam{1, 3, 5}),
        ::testing::ElementsAre(std::make_tuple(0, 1), std::make_tuple(1, 3),
                               std::make_tuple(2, 5)));
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
