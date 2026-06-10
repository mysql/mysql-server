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

#ifndef UNITTEST_LIBS_SETS_SET_ASSERTIONS_H
#define UNITTEST_LIBS_SETS_SET_ASSERTIONS_H

#include <gtest/gtest.h>                      // TEST
#include <bit>                                // countr_zero
#include <cassert>                            // assert
#include <iterator>                           // forward_iterator
#include "bitset_boundary_container.h"        // Bitset_boundary_container
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE

/// Copy src to dst, and don't check the return status.
///
/// This is useful in test code that assumes there are no out-of-memory
/// conditions, when the test code needs to be generic and work for both
/// `assign` functions that return a status and `assign` functions that return
/// void.
///
/// @param dst Object to overwrite
///
/// @param src Parameters to dst.assign
///
/// @return Same as dst.assign, but without the [[nodiscard]] attribute.
auto assign_nocheck(auto &dst, const auto &src) { return dst.assign(src); }
template <class Lhs, class Rhs>
  requires std::is_assignable_v<Lhs &, const Rhs &>
void assign_nocheck(Lhs &dst, const Rhs &src) {
  dst = src;
}

namespace unittest::libs::sets {

/// Assume that the two sets (of the same category and traits) are equal, and
/// exercise all predicaets operations whose result is therefore known.
///
/// @param set1 First set.
///
/// @param set2 Second set.
void assert_equal_sets(const auto &set1, const auto &set2) {
  bool is_empty = set1.empty();
  auto test_one_way = [&is_empty](const auto &s1, const auto &s2) {
    // emptiness
    ASSERT_EQ(s1.empty(), is_empty);
    ASSERT_EQ(!s1, is_empty);
    ASSERT_EQ((bool)s1, !is_empty);

    // equality
    assert(mysql::sets::is_equal(s1, s2));
    ASSERT_TRUE(mysql::sets::is_equal(s1, s2));
    ASSERT_TRUE(mysql::sets::operator==(s1, s2));
    ASSERT_FALSE(mysql::sets::operator!=(s1, s2));

    // Boolean set predicates
    ASSERT_TRUE(mysql::sets::is_subset(s1, s2));
    ASSERT_TRUE(mysql::sets::is_superset(s1, s2));
    ASSERT_EQ(mysql::sets::is_disjoint(s1, s2), is_empty);
    ASSERT_EQ(mysql::sets::is_intersecting(s1, s2), !is_empty);
  };
  test_one_way(set1, set2);
  if (static_cast<const void *>(&set1) != static_cast<const void *>(&set2)) {
    MY_SCOPED_TRACE("reverse compare");
    test_one_way(set2, set1);
  }
}

/// Exercise set operations (read-only) on the two given sets.
///
/// User should pass two sets and two truths. It is not required that the sets
/// and truths are compatible - they may have different categories. But it is
/// expected that any operation applied on the two tested sets has the same
/// result as the operation applied on the truths.
///
/// @tparam Truth_t Type of "truths", the sets to compare with.
///
/// @tparam Set1_t Type of the first set to test.
///
/// @tparam Set2_t Type of the second set to test.
///
/// @param truth1 First reference set.
///
/// @param truth2 Second reference set.
///
/// @param set1 First set, expressed as a Set1_t.
///
/// @param set2 Second set, expressed as a Set2_t.
template <class Truth_t, class Set1_t, class Set2_t>
void test_binary_predicates(const Truth_t &truth1, const Truth_t &truth2,
                            const Set1_t &set1, const Set2_t &set2) {
  // Set comparison
  auto is_equal = mysql::sets::is_equal(truth1, truth2);
  ASSERT_EQ(mysql::sets::operator==(set1, set2), is_equal);
  ASSERT_EQ(mysql::sets::operator!=(set1, set2), !is_equal);
  ASSERT_EQ(mysql::sets::is_equal(set1, set2), is_equal);

  ASSERT_EQ(mysql::sets::is_subset(set1, set2),
            mysql::sets::is_subset(truth1, truth2));
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  ASSERT_EQ(mysql::sets::is_subset(set2, set1),
            mysql::sets::is_subset(truth2, truth1));

  ASSERT_EQ(mysql::sets::is_superset(set1, set2),
            mysql::sets::is_superset(truth1, truth2));
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  ASSERT_EQ(mysql::sets::is_superset(set2, set1),
            mysql::sets::is_superset(truth2, truth1));

  ASSERT_EQ(mysql::sets::is_intersecting(set1, set2),
            mysql::sets::is_intersecting(truth1, truth2));
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  ASSERT_EQ(mysql::sets::is_intersecting(set2, set1),
            mysql::sets::is_intersecting(truth2, truth1));

  ASSERT_EQ(mysql::sets::is_disjoint(set1, set2),
            mysql::sets::is_disjoint(truth1, truth2));
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  ASSERT_EQ(mysql::sets::is_disjoint(set2, set1),
            mysql::sets::is_disjoint(truth2, truth1));
}

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_SET_ASSERTIONS_H
