// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                         // TEST
#include <iterator>                              // bidirectional_iterator_tag
#include "mysql/debugging/my_scoped_trace.h"     // MY_SCOPED_TRACE
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/iterators/meta.h"                // Iterator_concept_tag
#include "mysql/ranges/projection_views.h"       // Key_view

namespace {

// ==== Key_view/Key_iterator and Mapped_view/Mapped_iterator ====
//
// Test that Key_view and Mapped_view provide what we expect.
//
// Requirements for each of the classes Key_view<S> / Mapped_view<S> /
// Projection_view<N, S>:
//
// R1. The class shall provide a view over component 0/1/N (respectively) of
//     each element in the range S.
//
// R2. The iterators shall satisfy the same standard iterator concept as the
//     iterators for S, except if S satisfies std::contiguous_iterator, the
//     iterator shall satisfy std::random_access_iterator.
//
// R3. The returned type can be an lvalue reference or a non-reference; which
//     one depends on both the tuple and the element, as follows:
//     R3.1. If the element type is a non-reference, the return type is an
//           lvalue reference if the tuple is an lvalue reference, and it is a
//           non-reference otherwise. We test this in the following cases:
//           R3.1.1. The tuple is a non-reference: tuple<T> -> T.
//           R3.1.2. The tuple is an lvalue reference: tuple<T>& -> T&.
//           R3.1.3. The tuple is an rvalue reference: tuple<T>&& -> T.
//     R3.2. If the element type is an lvalue reference, the returned type is an
//           lvalue reference. We test this in the following cases:
//           R3.2.1. The tuple is a non-reference: tuple<T&> -> T&.
//           R3.2.2. The tuple is an lvalue reference: tuple<T&>& -> T&.
//           R3.2.3. The tuple is an rvalue reference: tuple<T&>&& -> T&.
//     It is not supported to have elements of rvalue reference types.
//
// R4. The `size`/`empty` members shall call `S::size`/`S::empty` if the
//     function exists, and compute the value based on the begin/end iterators
//     otherwise.

using namespace mysql;

// ==== R1, R2: Basic properties, and iterator category ====

template <class Iterator_tag_t>
void test_key_view_and_mapped_view(const auto &pairs, const auto &keys,
                                   const auto &mapped) {
  // R1.
  auto key_view = ranges::make_key_view(pairs);
  ASSERT_TRUE(std::equal(key_view.begin(), key_view.end(), keys.begin()));
  ASSERT_EQ(key_view.size(), keys.size());

  // R2.
  using Key_iterator_t = ranges::Range_iterator_type<decltype(key_view)>;
  static_assert(std::same_as<iterators::Iterator_concept_tag<Key_iterator_t>,
                             Iterator_tag_t>);

  // R1.
  auto mapped_view = ranges::make_mapped_view(pairs);
  ASSERT_TRUE(
      std::equal(mapped_view.begin(), mapped_view.end(), mapped.begin()));
  ASSERT_EQ(mapped_view.size(), mapped.size());

  // R2.
  using Mapped_iterator_t = ranges::Range_iterator_type<decltype(mapped_view)>;
  static_assert(std::same_as<iterators::Iterator_concept_tag<Mapped_iterator_t>,
                             Iterator_tag_t>);
}

TEST(LibsMysqlIteratorsProjectionView, Basic) {
  std::vector<int> expected_key_vector{{0, 10, 20}};
  std::vector<int> expected_mapped_vector{{1, 11, 21}};

  {
    MY_SCOPED_TRACE("map");

    // R1 + R2.
    std::map<int, int> source_map{{0, 1}, {10, 11}, {20, 21}};
    test_key_view_and_mapped_view<std::bidirectional_iterator_tag>(
        source_map, expected_key_vector, expected_mapped_vector);

    // R3.1.2.
    [[maybe_unused]] auto key_it =
        ranges::make_key_iterator(source_map.begin());
    static_assert(std::same_as<decltype(*key_it), const int &>);

    auto mapped_it = ranges::make_mapped_iterator(source_map.begin());
    *mapped_it = 3;
    ASSERT_EQ(source_map[0], 3);
  }

  {
    MY_SCOPED_TRACE("vector");

    // R1 + R2.
    std::vector<std::tuple<int, int, int>> source_vector{
        {0, 1, 2}, {10, 11, 12}, {20, 21, 22}};
    test_key_view_and_mapped_view<std::random_access_iterator_tag>(
        source_vector, expected_key_vector, expected_mapped_vector);

    // R3.1.2.
    auto key_it = ranges::make_key_iterator(source_vector.begin());
    *key_it = 3;
    ASSERT_EQ(std::get<0>(source_vector[0]), 3);

    auto mapped_it = ranges::make_mapped_iterator(source_vector.begin());
    *mapped_it = 4;
    ASSERT_EQ(std::get<1>(source_vector[0]), 4);

    auto third_it = ranges::make_projection_iterator<2>(source_vector.begin());
    *third_it = 5;
    ASSERT_EQ(std::get<2>(source_vector[0]), 5);
  }
}

// ==== R3: returns reference or non-reference ====

// warns that make_pair can throw, but it can't
// NOLINTNEXTLINE(cert-err58-cpp)
std::pair<int, int> value_pair(0, 0);
std::pair<int &, int &> lvalref_pair(value_pair.first, value_pair.second);

// R3.1.1
class It_val_tuple_val_elem
    : public iterators::Iterator_interface<It_val_tuple_val_elem> {
 public:
  [[nodiscard]] std::pair<int, int> get() const { return value_pair; }
  void next() {}
  [[nodiscard]] bool is_equal(const It_val_tuple_val_elem &) const {
    return true;
  }
};

static_assert(std::forward_iterator<It_val_tuple_val_elem>);
static_assert(
    std::same_as<decltype(*It_val_tuple_val_elem{}), std::pair<int, int>>);
static_assert(
    std::same_as<decltype(*ranges::make_key_iterator(It_val_tuple_val_elem{})),
                 int>);

// R3.1.2
class It_lvalref_tuple_val_elem
    : public iterators::Iterator_interface<It_lvalref_tuple_val_elem> {
 public:
  [[nodiscard]] std::pair<int, int> &get() const { return value_pair; }
  void next() {}
  [[nodiscard]] bool is_equal(const It_lvalref_tuple_val_elem &) const {
    return true;
  }
};

static_assert(std::forward_iterator<It_lvalref_tuple_val_elem>);
static_assert(std::same_as<decltype(*It_lvalref_tuple_val_elem{}),
                           std::pair<int, int> &>);
static_assert(std::same_as<
              decltype(*ranges::make_key_iterator(It_lvalref_tuple_val_elem{})),
              int &>);

// R3.1.3
class It_rvalref_tuple_val_elem
    : public iterators::Iterator_interface<It_rvalref_tuple_val_elem> {
 public:
  [[nodiscard]] std::pair<int, int> &&get() const {
    return std::move(value_pair);
  }
  void next() {}
  [[nodiscard]] bool is_equal(const It_rvalref_tuple_val_elem &) const {
    return true;
  }
};

static_assert(std::forward_iterator<It_rvalref_tuple_val_elem>);
static_assert(std::same_as<decltype(*It_rvalref_tuple_val_elem{}),
                           std::pair<int, int> &&>);
static_assert(std::same_as<
              decltype(*ranges::make_key_iterator(It_rvalref_tuple_val_elem{})),
              int>);

// R3.2.1
class It_val_tuple_lvalref_elem
    : public iterators::Iterator_interface<It_val_tuple_lvalref_elem> {
 public:
  [[nodiscard]] std::pair<int &, int &> get() const { return lvalref_pair; }
  void next() {}
  [[nodiscard]] bool is_equal(const It_val_tuple_lvalref_elem &) const {
    return true;
  }
};

static_assert(std::forward_iterator<It_val_tuple_lvalref_elem>);
static_assert(std::same_as<decltype(*It_val_tuple_lvalref_elem{}),
                           std::pair<int &, int &>>);
static_assert(std::same_as<
              decltype(*ranges::make_key_iterator(It_val_tuple_lvalref_elem{})),
              int &>);

// R3.2.2
class It_lvalref_tuple_lvalref_elem
    : public iterators::Iterator_interface<It_lvalref_tuple_lvalref_elem> {
 public:
  [[nodiscard]] std::pair<int &, int &> &get() const { return lvalref_pair; }
  void next() {}
  [[nodiscard]] bool is_equal(const It_lvalref_tuple_lvalref_elem &) const {
    return true;
  }
};

static_assert(std::forward_iterator<It_lvalref_tuple_lvalref_elem>);
static_assert(std::same_as<decltype(*It_lvalref_tuple_lvalref_elem{}),
                           std::pair<int &, int &> &>);
static_assert(std::same_as<decltype(*ranges::make_key_iterator(
                               It_lvalref_tuple_lvalref_elem{})),
                           int &>);

// R3.2.3
class It_rvalref_tuple_lvalref_elem
    : public iterators::Iterator_interface<It_rvalref_tuple_lvalref_elem> {
 public:
  [[nodiscard]] std::pair<int &, int &> &&get() const {
    return std::move(lvalref_pair);
  }
  void next() {}
  [[nodiscard]] bool is_equal(const It_rvalref_tuple_lvalref_elem &) const {
    return true;
  }
};

static_assert(std::forward_iterator<It_rvalref_tuple_lvalref_elem>);
static_assert(std::same_as<decltype(*It_rvalref_tuple_lvalref_elem{}),
                           std::pair<int &, int &> &&>);
static_assert(std::same_as<decltype(*ranges::make_key_iterator(
                               It_rvalref_tuple_lvalref_elem{})),
                           int &>);

// ==== R4. calls to size ====

int size_calls{0};

class My_pair_range {
 public:
  [[nodiscard]] auto begin() const { return m_map.begin(); }
  [[nodiscard]] auto end() const { return m_map.end(); }
  [[nodiscard]] auto size() const {
    ++size_calls;
    return m_map.size();
  }

 private:
  std::map<int, int> m_map;
};

TEST(LibsMysqlIteratorsProjectionView, OverrideSizeFunction) {
  My_pair_range pair_range;
  auto mapped_view = ranges::make_mapped_view(pair_range);
  // Verify that Mapped_view::size invokes the size function of My_pair_range,
  // instead of computing the size from std::ranges::distance
  auto old_size_calls = size_calls;
  ASSERT_EQ(mapped_view.size(), 0);
  ASSERT_EQ(size_calls, old_size_calls + 1);
}

}  // namespace
