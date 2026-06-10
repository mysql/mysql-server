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

#ifndef MYSQL_SETS_NESTED_SET_PREDICATES_H
#define MYSQL_SETS_NESTED_SET_PREDICATES_H

/// @file
/// Experimental API header

#include <concepts>                                // same_as
#include "mysql/sets/meta.h"                       // Has_fast_size
#include "mysql/sets/nested_set_meta.h"            // Is_nested_set_traits
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set
#include "mysql/sets/set_traits.h"                 // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Return true if the two pairs are equal, for pairs in which the second
/// components have different types but the same set category and traits.
///
/// Thus, when two nested sets with the same traits (but possibly different
/// types) are compared, it will run the algorithm in common_predicates.h
/// in the outer set; in each iteration invoke this
template <class Key_t, Is_set Mapped1_t, Is_set Mapped2_t>
  requires(Is_compatible_set<Mapped1_t, Mapped2_t> &&
           !std::same_as<Mapped1_t, Mapped2_t>)
[[nodiscard]] constexpr bool operator==(
    const std::pair<const Key_t, Mapped1_t> &left,
    const std::pair<const Key_t, Mapped2_t> &right) {
  return (left.first == right.first) && (left.second == right.second);
}

/// Return true if the value is contained in the Nested set.
template <Is_nested_set Nested_set_t>
[[nodiscard]] constexpr bool contains_element(
    const Nested_set_t &set, const typename Nested_set_t::Key_t &key,
    const auto &...mapped) {
  auto it = set.find(key);
  if (it == set.end()) return false;
  return contains_element(it->second, mapped...);
}

/// Return true if the left Nested set is a subset of or equal to the
/// right Nested_set.
template <Is_nested_set Nested_set1_t, Is_nested_set Nested_set2_t>
  requires Is_compatible_set<Nested_set1_t, Nested_set2_t>
[[nodiscard]] constexpr bool is_subset(const Nested_set1_t &set1,
                                       const Nested_set2_t &set2) {
  if constexpr (Has_fast_size<Nested_set1_t> && Has_fast_size<Nested_set2_t>) {
    if (set1.size() > set2.size()) return false;
  }
  for (auto &&[key, mapped1] : set1) {
    auto it2 = set2.find(key);
    if (it2 == set2.end()) return false;
    if (!is_subset(mapped1, it2->second)) return false;
  }
  return true;
}

/// Return true if the two Nested sets intersect (overlap).
template <Is_nested_set Nested_set1_t, Is_nested_set Nested_set2_t>
  requires Is_compatible_set<Nested_set1_t, Nested_set2_t>
[[nodiscard]] constexpr bool is_intersecting(const Nested_set1_t &set1,
                                             const Nested_set2_t &set2) {
  auto cursor1 = set1.begin();
  auto cursor2 = set2.begin();
  while (cursor1 != set1.end()) {
    // Invariant: the intersection up to cursor1 is empty, and cursor1 is not
    // end.
    auto it2 = set2.find(cursor2, cursor1->first);
    if (it2 != set2.end()) {
      if (is_intersecting(cursor1->second, it2->second)) return true;
    }
    if (cursor2 == set2.end()) return false;
    // Invariant: the intersection up to cursor2 is empty, and cursor2 is not
    // end.
    auto it1 = set1.find(cursor1, cursor2->first);
    if (it1 != set1.end()) {
      if (is_intersecting(it1->second, cursor2->second)) return true;
    }
  }
  return false;
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_PREDICATES_H
