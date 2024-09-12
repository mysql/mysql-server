/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>

#include <array>
#include <deque>
#include <forward_list>
#include <initializer_list>
#include <list>
#include <set>
#include <unordered_set>
#include <vector>

#include "mysql/harness/utility/string.h"

#include "unittest/gunit/benchmark.h"

using mysql_harness::join;

template <typename T>
class JoinTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(JoinTest);

TYPED_TEST_P(JoinTest, many) {
  TypeParam c_array[]{
      "abc",
      "def",
  };

  EXPECT_EQ(join(c_array, "-"), "abc-def");
  EXPECT_EQ(join(std::array<TypeParam, 2>{"abc", "def"}, "-"), "abc-def");
  EXPECT_EQ(join(std::initializer_list<const char *>{"abc", "def"}, "-"),
            "abc-def");

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
  TypeParam c_array[1]{
      "abc",
  };
  EXPECT_EQ(join(c_array, "-"), "abc");

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

REGISTER_TYPED_TEST_SUITE_P(JoinTest, many, one, none);

using JoinTestTypes =
    ::testing::Types<std::string, const char *, std::string_view>;
INSTANTIATE_TYPED_TEST_SUITE_P(Spec, JoinTest, JoinTestTypes);

namespace {

using namespace std::string_view_literals;

template <class T, size_t N>
constexpr auto init_bench_data() {
  std::array<T, N> data;

  for (auto &el : data) {
    el = "fuzbuzshnuzz";
  }

  return data;
}

auto bench_ar_sv = init_bench_data<std::string_view, 1024>();
auto bench_ar_cs = init_bench_data<const char *, 1024>();
auto bench_ar_s = init_bench_data<std::string, 1024>();

void BenchJoinStdArrayStringView(size_t iter) {
  while ((iter--) != 0) {
    std::string joined = join(bench_ar_sv, ", ");
  }
}

void BenchJoinStdArrayCString(size_t iter) {
  while ((iter--) != 0) {
    std::string joined = join(bench_ar_cs, ", ");
  }
}

void BenchJoinStdArrayStdString(size_t iter) {
  while ((iter--) != 0) {
    std::string joined = join(bench_ar_s, ", ");
  }
}

}  // namespace

BENCHMARK(BenchJoinStdArrayStdString)
BENCHMARK(BenchJoinStdArrayStringView)
BENCHMARK(BenchJoinStdArrayCString)

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
