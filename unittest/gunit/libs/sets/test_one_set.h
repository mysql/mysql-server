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

#ifndef UNITTEST_LIBS_SETS_TEST_ONE_SET_H
#define UNITTEST_LIBS_SETS_TEST_ONE_SET_H

#include <gtest/gtest.h>                      // ASSERT_TRUE
#include <ranges>                             // view
#include <type_traits>                        // remove_cvref_t
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/meta/is_specialization.h"     // Is_specialization
#include "mysql/sets/sets.h"                  // make_complement_view
#include "set_assertions.h"                   // assert_equal_sets

namespace unittest::libs::sets {

// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Test_complement { no, yes };

/// Exercise set operations on one set.
///
/// @tparam test_complement If true, this function will (once) compute the
/// complement view of the given sets, and pass them recursively to itself.
///
/// @tparam Set_t Set type to test. It may be either a boundary set or an
/// interval set.
///
/// @tparam Truth_t Type of truth to compare with.
///
/// @param set The set to test
///
/// @param truth Expected contents of set, in the form of an
/// Bitset_interval_container.
template <Test_complement test_complement, class Truth_t, class Set_t>
void test_one_set(const Truth_t &truth, const Set_t &set) {
  static_assert(mysql::sets::Is_set<Truth_t>);
  static_assert(mysql::sets::Is_set<Set_t>);
  static_assert(std::forward_iterator<typename Truth_t::Iterator_t>);
  static_assert(std::forward_iterator<typename Set_t::Iterator_t>);

  bool is_empty = truth.empty();
  MY_SCOPED_TRACE("is_empty=", is_empty);

  // Size/emptiness
  ASSERT_EQ(set.empty(), is_empty);
  ASSERT_EQ((bool)set, !is_empty);
  ASSERT_EQ(set.size() == 0, is_empty);

  // Self-comparison
  assert_equal_sets(set, set);

  // Complement_view
  if constexpr (test_complement == Test_complement::yes) {
    auto complement = mysql::sets::make_complement_view(set);
    auto truth_complement = truth;
    static_assert(mysql::sets::Is_compatible_set<decltype(complement), Set_t>);
    static_assert(
        mysql::sets::Is_compatible_set<decltype(truth_complement), Set_t>);
    truth_complement.inplace_complement();
    ASSERT_EQ(set == complement, false);
    ASSERT_EQ(set != complement, true);
    ASSERT_EQ(complement, truth_complement);
    ASSERT_EQ(mysql::sets::is_subset(set, complement), is_empty);
    ASSERT_EQ(mysql::sets::is_superset(set, complement), complement.empty());
    ASSERT_EQ(mysql::sets::is_disjoint(set, complement), true);
    ASSERT_EQ(mysql::sets::is_intersecting(set, complement), false);
    ASSERT_EQ((mysql::sets::make_full_set_view_like<Set_t>()) ==
                  mysql::sets::make_union_view(set, complement),
              true);
    ASSERT_EQ((mysql::sets::make_empty_set_view_like<Set_t>()) ==
                  mysql::sets::make_intersection_view(set, complement),
              true);

    // Complement of complement gives the same object back.
    static_assert(mysql::sets::Is_compatible_set<
                  std::remove_cvref_t<
                      decltype(mysql::sets::make_complement_view(complement))>,
                  Set_t>);
    if constexpr (mysql::meta::Is_specialization<
                      Set_t, mysql::sets::Complement_view>) {
      // set is already a Complement_view. Then make_complement_view should
      // return a reference to its source.
      ASSERT_EQ(&set.source(), &mysql::sets::make_complement_view(set));
    } else if constexpr (!std::ranges::view<Set_t>) {
      // Set_t is a container. Then make_complement_view(complement) should
      // return a reference to set.
      ASSERT_EQ(&mysql::sets::make_complement_view(complement), &set);
    } else {
      // Set_t is a view. Then `complement` holds its source by-value, so it
      // can't return a reference to set. Instead of checking object identity,
      // we check that the sets are the same types and contain the same
      // elements.
      static_assert(
          std::same_as<decltype(mysql::sets::make_complement_view(complement)),
                       const Set_t &>);
      ASSERT_NE(&mysql::sets::make_complement_view(complement), &set);
      ASSERT_EQ(mysql::sets::make_complement_view(complement), set);
    }

    {
      MY_SCOPED_TRACE("complement");
      test_one_set<Test_complement::no>(truth_complement, complement);
    }
  }

  // Binary operation views with one nullptr argument (treated as empty).
  // (Extra parentheses required so the preprocessor doesn't think the comma
  // between template arguments separates macro arguments.)
  ASSERT_EQ((mysql::sets::Union_view<Set_t, Set_t>(&set, &set)), set);
  ASSERT_EQ((mysql::sets::Union_view<Set_t, Set_t>(&set, nullptr)), set);
  ASSERT_EQ((mysql::sets::Union_view<Set_t, Set_t>(nullptr, &set)), set);
  static_assert(
      mysql::sets::Is_compatible_set<mysql::sets::Union_view<Set_t, Set_t>,
                                     Set_t>);
  ASSERT_EQ((mysql::sets::Intersection_view<Set_t, Set_t>(&set, &set)), set);
  ASSERT_TRUE(
      (mysql::sets::Intersection_view<Set_t, Set_t>(&set, nullptr)).empty());
  ASSERT_TRUE(
      (mysql::sets::Intersection_view<Set_t, Set_t>(nullptr, &set)).empty());
  static_assert(mysql::sets::Is_compatible_set<
                mysql::sets::Intersection_view<Set_t, Set_t>, Set_t>);
  ASSERT_TRUE(
      (mysql::sets::Subtraction_view<Set_t, Set_t>(&set, &set)).empty());
  ASSERT_EQ((mysql::sets::Subtraction_view<Set_t, Set_t>(&set, nullptr)), set);
  ASSERT_TRUE(
      (mysql::sets::Subtraction_view<Set_t, Set_t>(nullptr, &set)).empty());
  static_assert(mysql::sets::Is_compatible_set<
                mysql::sets::Subtraction_view<Set_t, Set_t>, Set_t>);
}

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_TEST_ONE_SET_H
