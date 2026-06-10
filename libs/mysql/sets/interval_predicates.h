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

#ifndef MYSQL_SETS_INTERVAL_PREDICATES_H
#define MYSQL_SETS_INTERVAL_PREDICATES_H

/// @file
/// Experimental API header

#include "mysql/sets/boundary_set_meta.h"  // Is_boundary_set_over_traits
#include "mysql/sets/interval.h"           // Interval

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Return true if the element is contained in the Interval.
///
/// Complexity: Constant.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool contains_element(
    const Interval<Set_traits_t> &interval,
    const typename Set_traits_t::Element_t &element) {
  return Set_traits_t::ge(element, interval.start()) &&
         Set_traits_t::lt(element, interval.exclusive_end());
}

/// Return true if the left Interval is a subset of or equal to the right
/// Interval.
///
/// Complexity: Constant.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_subset(
    const Interval<Set_traits_t> &interval1,
    const Interval<Set_traits_t> &interval2) {
  return Set_traits_t::ge(interval1.start(), interval2.start()) &&
         Set_traits_t::le(interval1.exclusive_end(), interval2.exclusive_end());
}

/// Return true if the two Interval objects intersect (overlap).
///
/// Complexity: Constant.
template <Is_bounded_set_traits Set_traits_t>
[[nodiscard]] constexpr bool is_intersecting(
    const Interval<Set_traits_t> &interval1,
    const Interval<Set_traits_t> &interval2) {
  return Set_traits_t::gt(interval1.exclusive_end(), interval2.start()) &&
         Set_traits_t::lt(interval1.start(), interval2.exclusive_end());
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_PREDICATES_H
