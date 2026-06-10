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
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface

namespace {

// ==== Basic examples ====
//
// This test case contains examples illustrating how to define new iterators
// using Iterator_interface.

using namespace mysql;

int int_array[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

// Example showing how to define an input iterator.
//
// Define `get` to return the current element, and `next` to advance to the next
// element.
class Basic_input_iterator
    : public iterators::Iterator_interface<Basic_input_iterator> {
 public:
  Basic_input_iterator() = default;
  explicit Basic_input_iterator(int position) : m_position(position) {}

  [[nodiscard]] int &get() const { return int_array[m_position]; }
  void next() { ++m_position; }

 private:
  int m_position{0};
};
static_assert(std::input_iterator<Basic_input_iterator>);

// Example showing how to define a forward iterator.
//
// Define `get` and `next` as for input_iterators, and define `is_equal` to
// compare iterators.
class Basic_forward_iterator
    : public iterators::Iterator_interface<Basic_forward_iterator> {
 public:
  Basic_forward_iterator() = default;
  explicit Basic_forward_iterator(int position) : m_position(position) {}

  [[nodiscard]] int &get() const { return int_array[m_position]; }
  void next() { ++m_position; }
  [[nodiscard]] bool is_equal(const Basic_forward_iterator &other) const {
    return other.m_position == m_position;
  }

 private:
  int m_position{0};
};
static_assert(std::forward_iterator<Basic_forward_iterator>);

// Example showing how to define a bidirectional iterator.
//
// Define `get`, `next`, and `is_equal` as for forward_iterators, and define
// `prev` to move back one step.
class Basic_bidirectional_iterator
    : public iterators::Iterator_interface<Basic_bidirectional_iterator> {
 public:
  Basic_bidirectional_iterator() = default;
  explicit Basic_bidirectional_iterator(int position) : m_position(position) {}

  [[nodiscard]] int &get() const { return int_array[m_position]; }
  void next() { ++m_position; }
  void prev() { --m_position; }
  [[nodiscard]] bool is_equal(const Basic_bidirectional_iterator &other) const {
    return other.m_position == m_position;
  }

 private:
  int m_position{0};
};
static_assert(std::bidirectional_iterator<Basic_bidirectional_iterator>);

// Example showing how to define a random access iterator.
//
// Define `get` as for the previous iterator types, but define `advance` instead
// of `next`/`prev` to move a given number of steps back or forth, and define
// `distance_from` instead of `is_equal` to compute the distance from another
// iterator to this one.
class Basic_random_access_iterator
    : public iterators::Iterator_interface<Basic_random_access_iterator> {
 public:
  Basic_random_access_iterator() = default;
  explicit Basic_random_access_iterator(int position) : m_position(position) {}

  [[nodiscard]] int &get() const { return int_array[m_position]; }
  void advance(std::ptrdiff_t delta) { m_position += delta; }
  [[nodiscard]] std::ptrdiff_t distance_from(
      const Basic_random_access_iterator &other) const {
    return m_position - other.m_position;
  }

 private:
  int m_position{0};
};
static_assert(std::random_access_iterator<Basic_random_access_iterator>);

// Example showing how to define a contiguous iterator.
//
// Define `advance` and `distance_from` as for random_access iterators, but
// define `get_pointer` instead of `get`, to return a pointer to the current
// element rather than a reference.
class Basic_contiguous_iterator
    : public iterators::Iterator_interface<Basic_contiguous_iterator> {
 public:
  Basic_contiguous_iterator() = default;
  explicit Basic_contiguous_iterator(int position) : m_position(position) {}

  [[nodiscard]] int *get_pointer() const { return int_array + m_position; }
  void advance(std::ptrdiff_t delta) { m_position += delta; }
  [[nodiscard]] std::ptrdiff_t distance_from(
      const Basic_contiguous_iterator &other) const {
    return m_position - other.m_position;
  }

 private:
  int m_position{0};
};
static_assert(std::contiguous_iterator<Basic_contiguous_iterator>);

// Verify that the iterators satisfy the necessary concepts.
TEST(LibsMysqlIteratorsBasic, Basic) {}

}  // namespace
