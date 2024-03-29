/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/span.h"

#include <array>
#include <forward_list>
#include <initializer_list>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>

static_assert(stdx::detail::is_sized_range<std::vector<int>>::value);
static_assert(stdx::detail::is_sized_range<std::string>::value);
static_assert(stdx::detail::is_sized_range<std::initializer_list<int>>::value);
static_assert(stdx::detail::is_sized_range<std::list<int>>::value);
static_assert(!stdx::detail::is_sized_range<std::forward_list<int>>::value);

static_assert(stdx::detail::is_contiguous_range<std::vector<int>>::value);
static_assert(stdx::detail::is_contiguous_range<std::string>::value);
static_assert(
    stdx::detail::is_contiguous_range<std::initializer_list<int>>::value);
static_assert(!stdx::detail::is_contiguous_range<std::list<int>>::value);
static_assert(
    !stdx::detail::is_contiguous_range<std::forward_list<int>>::value);

static_assert(stdx::detail::is_compatible_range<std::vector<int>, int>::value);
static_assert(stdx::detail::is_compatible_range<std::string, char>::value);
static_assert(stdx::detail::is_compatible_range<std::initializer_list<int>,
                                                const int>::value);
static_assert(!stdx::detail::is_compatible_range<std::list<int>, int>::value);
static_assert(
    !stdx::detail::is_compatible_range<std::forward_list<int>, int>::value);

TEST(SpanTest, default_constructor) {
  constexpr stdx::span<int> spn;

  static_assert(spn.empty());
}

TEST(SpanTest, construct_from_array) {
  int ar[]{0, 2};

  stdx::span<int, 2> spn(ar);

  EXPECT_EQ(spn.data(), std::addressof(ar[0]));
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_std_array) {
  std::array<int, 2> ar{0, 2};

  stdx::span<int, 2> spn(ar);

  EXPECT_EQ(spn.data(), ar.data());
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_std_array_const) {
  static constexpr const std::array<int, 2> ar{0, 2};

  constexpr stdx::span<const int, 2> spn(ar);

  EXPECT_EQ(spn.data(), ar.data());
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_std_vector) {
  std::vector<int> ar{0, 2};

  stdx::span<int, 2> spn(ar);

  EXPECT_EQ(spn.data(), ar.data());
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_std_vector_const) {
  const std::vector<int> ar{0, 2};

  stdx::span<const int, 2> spn(ar);

  EXPECT_EQ(spn.data(), ar.data());
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_initializer_list_dynamic) {
  // the underlying type is const.
  auto il = {0, 2};

  stdx::span<const int> spn(il);

  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_array_dynamic) {
  int ar[]{0, 2};

  stdx::span<int> spn(ar);

  EXPECT_EQ(spn.data(), std::data(ar));
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_std_array_dynamic) {
  std::array<int, 2> ar{0, 2};

  stdx::span<int> spn(ar);

  EXPECT_EQ(spn.data(), ar.data());
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);
}

TEST(SpanTest, construct_from_std_vector_dynamic) {
  std::vector<int> ar{0, 2};

  stdx::span<int> spn(ar);

  EXPECT_EQ(spn.data(), ar.data());
  EXPECT_EQ(spn.size(), 2);
  EXPECT_EQ(spn[0], 0);
  EXPECT_EQ(spn[1], 2);

  // can be written.
  spn[0] = 1;

  // and iterated.
  EXPECT_THAT(spn, ::testing::ElementsAreArray({1, 2}));
}

TEST(SpanTest, subspan_0_1) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto sub = spn.subspan(0, 1);

  EXPECT_EQ(sub.data(), ar.data());
  EXPECT_EQ(sub.size(), 1);
  EXPECT_EQ(sub[0], 0);
}

TEST(SpanTest, subspan_1_1) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto sub = spn.subspan(1, 1);

  EXPECT_EQ(sub.data(), ar.data() + 1);
  EXPECT_EQ(sub.size(), 1);
  EXPECT_EQ(sub[0], 1);
}

TEST(SpanTest, first) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto sub = spn.first(2);

  EXPECT_EQ(sub.data(), ar.data());
  EXPECT_EQ(sub.size(), 2);
  EXPECT_EQ(sub[0], 0);
  EXPECT_EQ(sub[1], 1);
}

TEST(SpanTest, subspan_template_from_dynamic_extent) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  // Extent is dynamic
  EXPECT_EQ(spn.extent, stdx::dynamic_extent);

  {
    // Count is !dynamic
    auto sub = spn.subspan<1, 2>();

    // if Count is !dynamic, Extent is Count
    EXPECT_EQ(sub.extent, 2);

    EXPECT_EQ(sub.data(), ar.data() + 1);
    ASSERT_EQ(sub.size(), 2);
    EXPECT_EQ(sub[0], 1);
    EXPECT_EQ(sub[1], 2);
  }

  {
    // Count is dynamic
    auto sub = spn.subspan<1>();

    EXPECT_EQ(sub.extent, stdx::dynamic_extent);

    EXPECT_EQ(sub.data(), ar.data() + 1);
    ASSERT_EQ(sub.size(), 2);
    EXPECT_EQ(sub[0], 1);
    EXPECT_EQ(sub[1], 2);
  }
}

TEST(SpanTest, subspan_template) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int, 3> spn(ar);

  // Extent is !dynamic
  EXPECT_EQ(spn.extent, 3);

  {
    // Count is !dynamic
    auto sub = spn.subspan<1, 2>();

    // if Count is !dynamic, Extent is Count
    EXPECT_EQ(sub.extent, 2);

    EXPECT_EQ(sub.data(), ar.data() + 1);
    ASSERT_EQ(sub.size(), 2);
    EXPECT_EQ(sub[0], 1);
    EXPECT_EQ(sub[1], 2);
  }

  {
    // Count is dynamic
    auto sub = spn.subspan<1>();

    // Extent - Offset
    EXPECT_EQ(sub.extent, 2);

    EXPECT_EQ(sub.data(), ar.data() + 1);
    ASSERT_EQ(sub.size(), 2);
    EXPECT_EQ(sub[0], 1);
    EXPECT_EQ(sub[1], 2);
  }
}

TEST(SpanTest, first_template) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto sub = spn.first<2>();

  EXPECT_EQ(sub.data(), ar.data());
  EXPECT_EQ(sub.size(), 2);
  EXPECT_EQ(sub[0], 0);
  EXPECT_EQ(sub[1], 1);
}

TEST(SpanTest, last) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto sub = spn.last(1);

  EXPECT_EQ(sub.data(), ar.data() + 2);
  EXPECT_EQ(sub.size(), 1);
  EXPECT_EQ(sub[0], 2);
}

TEST(SpanTest, last_template) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto sub = spn.last<1>();

  EXPECT_EQ(sub.data(), ar.data() + 2);
  EXPECT_EQ(sub.size(), 1);
  EXPECT_EQ(sub[0], 2);
}

TEST(SpanTest, front) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  EXPECT_EQ(spn.front(), 0);
}

TEST(SpanTest, back) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  EXPECT_EQ(spn.back(), 2);
}

TEST(SpanTest, reverse) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  EXPECT_EQ(*spn.rbegin(), 2);
}

TEST(SpanTest, as_writable_bytes) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto writable_bytes_span = stdx::as_writable_bytes(spn);
  EXPECT_EQ(writable_bytes_span.size(), sizeof(int) * 3);

  SCOPED_TRACE("// change the first integer");
  writable_bytes_span[0] = std::byte{0xff};

  EXPECT_NE(ar[0], 0);
}

TEST(SpanTest, as_bytes) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<int> spn(ar);

  auto bytes_span = stdx::as_bytes(spn);
  EXPECT_EQ(bytes_span.size(), sizeof(int) * 3);
}

TEST(SpanTest, as_bytes_const) {
  std::vector<int> ar{0, 1, 2};

  stdx::span<const int> spn(ar);

  auto bytes_span = stdx::as_bytes(spn);
  EXPECT_EQ(bytes_span.size(), sizeof(int) * 3);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
