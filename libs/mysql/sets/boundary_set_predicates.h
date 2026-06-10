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

#ifndef MYSQL_SETS_BOUNDARY_SET_PREDICATES_H
#define MYSQL_SETS_BOUNDARY_SET_PREDICATES_H

/// @file
/// Experimental API header

#include "mysql/sets/boundary_set_meta.h"  // Is_boundary_set_over_traits
#include "mysql/sets/interval.h"           // Interval
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== contains_element ====

/// Return true if the element is contained in the Boundary set.
///
/// Complexity: The time of one invocation of set.upper_bound.
template <Is_boundary_set Boundary_set_t>
[[nodiscard]] constexpr bool contains_element(
    const Boundary_set_t &set,
    const typename Boundary_set_t::Element_t &element) {
  return set.upper_bound(element).is_endpoint();
}

// ==== is_subset ====

/// Return true if the left Interval is a subset of or equal to the right
/// Boundary_set.
///
/// Complexity: The time of one invocation of set2.upper_bound.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_subset(
    const Interval<Set_traits_t> &interval1,
    const Is_boundary_set_over_traits<Set_traits_t> auto &set2) {
  auto ub = set2.upper_bound(interval1.start());
  return ub.is_endpoint() && Set_traits_t::le(interval1.exclusive_end(), *ub);
}

/// Return true if the left Boundary set is a subset of or equal to the
/// right Interval.
///
/// Complexity: Constant.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_subset(
    const Is_boundary_set_over_traits<Set_traits_t> auto &set1,
    const Interval<Set_traits_t> &interval2) {
  return set1.empty() ||
         (Set_traits_t::le(interval2.start(), set1.front()) &&
          Set_traits_t::ge(interval2.exclusive_end(), set1.back()));
}

/// Return true if the left Boundary set is a subset of or equal to the right
/// Boundary_set.
///
/// Complexity: The number of iterations is linear in the size of the smallest
/// set. Each iteration requires an invocation of upper_bound in both sets.
template <Is_boundary_set Boundary_set1_t, Is_boundary_set Boundary_set2_t>
  requires Is_compatible_set<Boundary_set1_t, Boundary_set2_t>
[[nodiscard]] constexpr bool is_subset(const Boundary_set1_t &set1,
                                       const Boundary_set2_t &set2) {
  auto it1 = set1.begin();
  auto end1 = set1.end();
  auto it2 = set2.begin();
  auto end2 = set2.end();
  // Each iteration (except possibly the last one) visits a start-point in set2
  // and an end-point in set1. So the maximum possible number of iterations
  // can't exceed the number of elements in either set.
  while (it1 != end1) {
    // Invariant holding here: it1.start==true, and there are no elements in
    // it1-it2 up to it1.
    it2 = set2.upper_bound(it2, *it1);
    // Is it1 past the end of set2?
    if (it2 == end2) return false;
    // Is it1 outside intervals in set2?
    if (!it2.is_endpoint()) return false;

    // Invariant holding here: it2.start==false, and there are no elements in
    // it1-it2 up to it2.
    it1 = set1.upper_bound(it1, *it2);
    // Is it2 past the end of set1?
    if (it1 == end1) return true;
    // Is it1 ouside intervals in set2?
    if (it1.is_endpoint()) return false;
  }
  return true;
}

// ==== is_intersecting ====

/// Return true if the left Interval and the right Boundary set intersect
/// (overlap).
///
/// Complexity: The time of one invocation of set.upper_bound.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_intersecting(
    const Interval<Set_traits_t> &interval,
    const Is_boundary_set_over_traits<Set_traits_t> auto &set) {
  auto ub = set.upper_bound(interval.start());
  return ub.is_endpoint() ||
         (ub != set.end() && Set_traits_t::lt(*ub, interval.exclusive_end()));
}

/// Return true if the left Boundary set and the right Interval intersect
/// (overlap).
///
/// Complexity: The time of one invocation of set.upper_bound.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_intersecting(
    const Is_boundary_set_over_traits<Set_traits_t> auto &set,
    const Interval<Set_traits_t> &interval) {
  return is_intersecting(interval, set);
}

/// Return true if the two Boundary sets intersect (overlap).
///
/// Complexity: The number of iterations is linear in the size of the smallest
/// set. Each iteration requires an invocation of upper_bound in both sets.
template <Is_boundary_set Boundary_set1_t, Is_boundary_set Boundary_set2_t>
  requires Is_compatible_set<Boundary_set1_t, Boundary_set2_t>
[[nodiscard]] constexpr bool is_intersecting(const Boundary_set1_t &set1,
                                             const Boundary_set2_t &set2) {
  auto it1 = set1.begin();
  auto end1 = set1.end();
  auto it2 = set2.begin();
  auto end2 = set2.end();
  if (it1 == end1) return false;
  // Each iteration (except possibly the last one) visits a start-point in each
  // set. So the maximum possible number of iterations can't exceed the number
  // of elements in either set.
  while (true) {
    // Invariant holding here: there are no overlaps up to it1, and
    // it1.is_endpoint==false.
    it2 = set2.upper_bound(it2, *it1);
    // Is it1 past the end of set2?
    if (it2 == end2) return false;
    // Is it1 in the middle of an interval?
    if (it2.is_endpoint()) return true;

    // Invariant holding here: there are no overlaps up to it2, and
    // it2.is_endpoint==false.
    it1 = set1.upper_bound(it1, *it2);
    // Is it2 past the end of set1?
    if (it1 == end1) return false;
    // Is it1 in the middle of an interval?
    if (it1.is_endpoint()) return true;
  }
  return false;
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_PREDICATES_H
