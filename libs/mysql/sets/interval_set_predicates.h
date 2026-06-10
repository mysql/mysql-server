// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_SETS_INTERVAL_SET_PREDICATES_H
#define MYSQL_SETS_INTERVAL_SET_PREDICATES_H

/// @file
/// Experimental API header
#include "mysql/sets/interval.h"                   // Interval
#include "mysql/sets/interval_set_meta.h"          // Is_interval_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== contains_element ====

/// Return true if the element is contained in the Interval set.
///
/// Complexity: The time of one invocation of set.boundaries().upper_bound.
template <Is_interval_set Interval_set_t>
[[nodiscard]] constexpr bool contains_element(
    const Interval_set_t &set,
    const typename Interval_set_t::Element_t &element) {
  return contains_element(set.boundaries(), element);
}

// ==== is_subset ====

/// Return true if the left Interval is a subset of or equal to the right
/// Interval set.
///
/// Complexity: The time of one invocation of set2.upper_bound.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_subset(
    const Interval<Set_traits_t> &interval1,
    const Is_interval_set_over_traits<Set_traits_t> auto &set2) {
  return is_subset(interval1, set2.boundaries());
}

/// Return true if the left Interval set is a subset of or equal to the
/// right Interval.
///
/// Complexity: Constant.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_subset(
    const Is_interval_set_over_traits<Set_traits_t> auto &set1,
    const Interval<Set_traits_t> &interval2) {
  return is_subset(set1.boundaries(), interval2);
}

/// Return true if the left Interval set is a subset of or equal to the
/// right Interval set.
///
/// Complexity: The number of iterations is linear in the size of the smallest
/// set. Each iteration requires an invocation of upper_bound in both sets.
template <Is_interval_set Interval_set1_tp, Is_interval_set Interval_set2_tp>
  requires Is_compatible_set<Interval_set1_tp, Interval_set2_tp>
[[nodiscard]] constexpr bool is_subset(const Interval_set1_tp &set1,
                                       const Interval_set2_tp &set2) {
  return is_subset(set1.boundaries(), set2.boundaries());
}

// ==== is_intersecting ====

/// Return true if the left Interval and the right Interval set
/// intersect.
///
/// Complexity: The time of one invocation of set.upper_bound.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_intersecting(
    const Interval<Set_traits_t> &interval,
    const Is_interval_set_over_traits<Set_traits_t> auto &set) {
  return is_intersecting(interval, set.boundaries());
}

/// Return true if the left Interval set and the right Interval
/// intersect.
///
/// Complexity: The time of one invocation of set.upper_bound.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_intersecting(
    const Is_interval_set_over_traits<Set_traits_t> auto &set,
    const Interval<Set_traits_t> &interval) {
  return is_intersecting(set.boundaries(), interval);
}

/// Return true if the two Interval sets intersect (overlap).
///
/// Complexity: The number of iterations is linear in the size of the smallest
/// set. Each iteration requires an invocation of upper_bound in both sets.
template <Is_interval_set Interval_set1_tp, Is_interval_set Interval_set2_tp>
  requires Is_compatible_set<Interval_set1_tp, Interval_set2_tp>
[[nodiscard]] constexpr bool is_intersecting(const Interval_set1_tp &set1,
                                             const Interval_set2_tp &set2) {
  return is_intersecting(set1.boundaries(), set2.boundaries());
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_PREDICATES_H
