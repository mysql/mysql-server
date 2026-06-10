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

#ifndef MYSQL_SETS_BASE_CONST_VIEWS_H
#define MYSQL_SETS_BASE_CONST_VIEWS_H

/// @file
/// Experimental API header

#include <ranges>                                  // enable_view
#include "mysql/sets/set_categories.h"             // Is_set_category
#include "mysql/sets/set_categories_and_traits.h"  // Is_set
#include "mysql/sets/set_traits.h"                 // Is_bounded_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// ==== Empty sets ====

/// Forward declaration of primary template for views over empty sets.
///
/// This may be specialized to set categories for which we need it.
template <Is_set_category, Is_set_traits>
class Empty_set_view;

/// Return a reference to a singleton object representing the view containing
/// the empty set, for the given Set category and Set traits.
template <Is_set_category Set_category_t, Is_set_traits Set_traits_t>
[[nodiscard]] auto &make_empty_set_view() {
  static Empty_set_view<Set_category_t, Set_traits_t> ret;
  return ret;
}

/// Return the result of make_empty_set_view for the set category and set traits
/// of the given set type.
template <Is_set Set_t>
[[nodiscard]] auto &make_empty_set_view_like() {
  return make_empty_set_view<typename Set_t::Set_category_t,
                             typename Set_t::Set_traits_t>();
}

// ==== Full sets ====

/// Forward declaration of primary template for views over "full" sets, i.e.,
/// the complement of the empty set.
///
/// This may be specialized to set categories for which we need it. Note that
/// not all set categories can have "full sets".
template <Is_set_category, Is_set_traits>
class Full_set_view;

/// Return a reference to a singleton object representing the view containing
/// the empty set, for the given Set category and Set traits.
template <Is_set_category Set_category_t, Is_bounded_set_traits Set_traits_t>
[[nodiscard]] auto &make_full_set_view() {
  static Full_set_view<Set_category_t, Set_traits_t> ret;
  return ret;
}

/// Return the result of make_full_set_view for the set category and set traits
/// of the given set type.
template <Is_set Set_t>
[[nodiscard]] auto &make_full_set_view_like() {
  return make_full_set_view<typename Set_t::Set_category_t,
                            typename Set_t::Set_traits_t>();
}

}  // namespace mysql::sets

// ==== Declarations of view ====

/// @cond DOXYGEN_DOES_NOT_UNDERSTAND_THIS
/// Declare that Empty_view is a view.
template <class Set_category_t, class Set_traits_t>
constexpr bool std::ranges::enable_view<
    mysql::sets::Empty_set_view<Set_category_t, Set_traits_t>> = true;

/// Declare that Full_view is a view.
template <class Set_category_t, class Set_traits_t>
constexpr bool std::ranges::enable_view<
    mysql::sets::Full_set_view<Set_category_t, Set_traits_t>> = true;
/// @endcond

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BASE_CONST_VIEWS_H
