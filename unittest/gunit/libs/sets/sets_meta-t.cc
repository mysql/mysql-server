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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include <gtest/gtest.h>                // TEST
#include "bitset_boundary_container.h"  // Bitset_boundary_container
#include "bitset_interval_container.h"  // Bitset_interval_container
#include "mysql/sets/sets.h"            // Interval_container

// ==== Purpose ====
//
// Compile-time assertions on properties of the types of set classes and their
// iterators.
//
// For the various implemnetations of Boundary Sets, Interval Sets, and Nested
// Sets, check standard requirements related to copy/move semantics and
// default-constructibility.
//
// For the iterators and const iterators, check the same and also the iterator
// categories.
//
// All these types must be nothrow-default-constructible and nothrow-moveable.
// Throwing containers must be throwingly copyable, nonthrowing containers must
// be non-copyable, and views and iterators must be nothrow-copyable.
//
// Iterators in map types must be bidirectional but not random access. Iterators
// in vector boundary containers must be contiguous. Iterators in vector
// interval containers must be random access.

namespace {

using namespace mysql;

using My_set_traits = sets::Int_set_traits<int>;

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint
enum class Copyable { no, yes, nothrow };
enum class Default_constructible { no, yes, nothrow };
enum class Moveable { no, yes, nothrow };
// NOLINTEND(performance-enum-size)

template <class Test, Copyable copyable = Copyable::nothrow,
          Default_constructible default_constructible =
              Default_constructible::nothrow,
          Moveable moveable = Moveable::nothrow>
void assert_copyable_default_constructible_moveable() {
  constexpr bool expect_copyable = (copyable != Copyable::no);
  static_assert(std::is_copy_constructible_v<Test> == expect_copyable);
  static_assert(std::is_copy_assignable_v<Test> == expect_copyable);

  constexpr bool expect_nothrow_copyable = (copyable == Copyable::nothrow);
  static_assert(std::is_nothrow_copy_constructible_v<Test> ==
                expect_nothrow_copyable);
  static_assert(std::is_nothrow_copy_assignable_v<Test> ==
                expect_nothrow_copyable);

  constexpr bool expect_moveable = (moveable != Moveable::no);
  static_assert(std::is_move_constructible_v<Test> == expect_moveable);
  static_assert(std::is_move_assignable_v<Test> == expect_moveable);

  constexpr bool expect_nothrow_moveable = (moveable == Moveable::nothrow);
  static_assert(std::is_nothrow_move_constructible_v<Test> ==
                expect_nothrow_moveable);
  static_assert(std::is_nothrow_move_assignable_v<Test> ==
                expect_nothrow_moveable);

  constexpr bool expect_dc =
      (default_constructible != Default_constructible::no);
  static_assert(std::is_default_constructible_v<Test> == expect_dc);

  constexpr bool expect_nothrow_dc =
      (default_constructible == Default_constructible::nothrow);
  static_assert(std::is_nothrow_default_constructible_v<Test> ==
                expect_nothrow_dc);
}

template <class Test, class Iterator_concept, auto... Args>
void assert_container() {
  assert_copyable_default_constructible_moveable<Test, Args...>();
  assert_copyable_default_constructible_moveable<
      ranges::Range_iterator_type<Test>>();
  static_assert(
      std::same_as<
          iterators::Iterator_concept_tag<ranges::Range_iterator_type<Test>>,
          Iterator_concept>);
  static_assert(std::same_as<iterators::Iterator_concept_tag<
                                 ranges::Range_const_iterator_type<Test>>,
                             Iterator_concept>);
}

TEST(LibsSets, Meta) {
  // Throwing containers are throwingly copyable, nothrow-moveable, and
  // nothrow-default-constructible.
  assert_container<sets::throwing::Map_boundary_container<My_set_traits>,
                   std::bidirectional_iterator_tag, Copyable::yes>();
  assert_container<sets::throwing::Vector_boundary_container<My_set_traits>,
                   std::contiguous_iterator_tag, Copyable::yes>();
  assert_container<sets::throwing::Map_interval_container<My_set_traits>,
                   std::bidirectional_iterator_tag, Copyable::yes>();
  assert_container<sets::throwing::Vector_interval_container<My_set_traits>,
                   std::random_access_iterator_tag, Copyable::yes>();

  // Non-throwing containers are not copyable, but nothrow-moveable and
  // nothrow-default-constructible.
  assert_container<sets::Map_boundary_container<My_set_traits>,
                   std::bidirectional_iterator_tag, Copyable::no>();
  assert_container<sets::Vector_boundary_container<My_set_traits>,
                   std::contiguous_iterator_tag, Copyable::no>();
  assert_container<sets::Map_interval_container<My_set_traits>,
                   std::bidirectional_iterator_tag, Copyable::no>();
  assert_container<sets::Vector_interval_container<My_set_traits>,
                   std::random_access_iterator_tag, Copyable::no>();
  assert_container<
      sets::Map_nested_container<My_set_traits,
                                 sets::Map_interval_container<My_set_traits>>,
      std::bidirectional_iterator_tag, Copyable::no>();

  // Views are nothrow-copyable, nothrow-moveable, and
  // nothrow-default-constructible.
  assert_container<
      sets::Union_view<sets::Map_interval_container<My_set_traits>,
                       sets::Vector_interval_container<My_set_traits>>,
      std::forward_iterator_tag>();
  assert_container<
      sets::Intersection_view<sets::Map_interval_container<My_set_traits>,
                              sets::Vector_interval_container<My_set_traits>>,
      std::forward_iterator_tag>();
  assert_container<
      sets::Subtraction_view<sets::Map_interval_container<My_set_traits>,
                             sets::Vector_interval_container<My_set_traits>>,
      std::forward_iterator_tag>();

  assert_container<
      sets::Union_view<
          sets::throwing::Map_interval_container<My_set_traits>,
          sets::throwing::Vector_interval_container<My_set_traits>>,
      std::forward_iterator_tag>();
  assert_container<
      sets::Intersection_view<
          sets::throwing::Map_interval_container<My_set_traits>,
          sets::throwing::Vector_interval_container<My_set_traits>>,
      std::forward_iterator_tag>();
  assert_container<
      sets::Subtraction_view<
          sets::throwing::Map_interval_container<My_set_traits>,
          sets::throwing::Vector_interval_container<My_set_traits>>,
      std::forward_iterator_tag>();
}

}  // namespace
