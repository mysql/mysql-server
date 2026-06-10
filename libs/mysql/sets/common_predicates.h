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

#ifndef MYSQL_SETS_COMMON_PREDICATES_H
#define MYSQL_SETS_COMMON_PREDICATES_H

/// @file
/// Experimental API header

#include "mysql/sets/meta.h"                       // Has_fast_size
#include "mysql/sets/set_categories.h"             // Is_set_category
#include "mysql/sets/set_categories_and_traits.h"  // Is_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Return true if two sets are equal, which must be of the same Set category
/// and Set traits and be *iterator-defined*. See Is_iterator_defined_set for
/// details.
///
/// Complexity: the number of value comparisons is linear in the size of the
/// smallest set.
template <Is_set Set1_t, Is_set Set2_t>
  requires Is_compatible_set<Set1_t, Set2_t> && Is_iterator_defined_set<Set1_t>
[[nodiscard]] constexpr bool operator==(const Set1_t &set1,
                                        const Set2_t &set2) {
  if constexpr (std::same_as<Set1_t, Set2_t>) {
    // Self-comparison
    if (&set1 == &set2) return true;
  }
  if constexpr (Has_fast_size<Set1_t> && Has_fast_size<Set2_t>) {
    // If it is fast to compute size, do that first, and compare elements only
    // if the sizes are equal. This case typically occurs when the operands are
    // containers.
    auto size1 = set1.size();
    auto size2 = set2.size();
    if (size1 != size2) return false;

    auto it2 = set2.begin();
    for (auto &&value1 : set1) {
      if (value1 != *it2) return false;
      assert(it2 != set2.end());
      ++it2;
    }
  } else {
    // If it is not fast to compute size, compare elements, and check if one
    // sequence ends before the other. This case typically occurs when the
    // operands are views.
    auto it2 = set2.begin();
    for (auto &&value1 : set1) {
      if (it2 == set2.end()) return false;
      if (value1 != *it2) return false;
      ++it2;
    }
    if (it2 != set2.end()) return false;
  }
  return true;
}

/// Return true if the two sets are not equal.
template <Is_set Set1_t, Is_set Set2_t>
  requires Is_compatible_set<Set1_t, Set2_t>
[[nodiscard]] constexpr bool operator!=(const Set1_t &set1,
                                        const Set2_t &set2) {
  return !(set1 == set2);
}

/// Return true if the two sets are equal, which must be of the same Set
/// category and Set traits.
///
/// This is alternative syntax to operator==. If both the operands and the
/// calling context are outside the mysql::sets namespace, name lookup will not
/// find the operator. It can be qualified by the namespace using the syntax
/// mysql::sets::operator==(a, b). We provide this equivalent alternative in
/// case anyone thinks that looks funny, and also because the syntax is
/// analogous to other set predicates such as is_subset etc.
template <Is_set Set1_t, Is_set Set2_t>
  requires Is_compatible_set<Set1_t, Set2_t>
[[nodiscard]] constexpr bool is_equal(const Set1_t &set1, const Set2_t &set2) {
  return set1 == set2;
}

/// Return true if the left object is a superset of or equal to the right
/// object.
///
/// This just delegates work to is_subset, and is defined for any types for
/// which is_subset is defined.
[[nodiscard]] constexpr bool is_superset(const auto &lhs, const auto &rhs) {
  return is_subset(rhs, lhs);
}

/// Return true if the two objects are disjoint (have nothing in common).  The
/// objects may be intervals, boundary sets, or interval sets.
///
/// This just delegates work to is_intersecting, and is defined for any types
/// for which is_intersecting is defined.
[[nodiscard]] constexpr bool is_disjoint(const auto &lhs, const auto &rhs) {
  return !is_intersecting(lhs, rhs);
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_COMMON_PREDICATES_H
