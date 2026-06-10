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

#ifndef MYSQL_SETS_NESTED_SET_CONST_VIEWS_H
#define MYSQL_SETS_NESTED_SET_CONST_VIEWS_H

/// @file
/// Experimental API header

#include "mysql/iterators/empty_sequence_iterator.h"  // Empty_sequence_iterator
#include "mysql/sets/base_const_views.h"              // Empty_set_view
#include "mysql/sets/nested_set_category.h"           // Nested_set_category_tag
#include "mysql/sets/nested_set_interface.h"          // Nested_set_interface
#include "mysql/sets/nested_set_meta.h"               // Is_nested_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Gives the mapped type for a nested Empty_set_view.
///
/// @tparam Set_traits_t Nested set traits.
template <Is_nested_set_traits Set_traits_t>
using Empty_nested_mapped_type =
    Empty_set_view<typename Set_traits_t::Mapped_category_t,
                   typename Set_traits_t::Mapped_traits_t>;

/// Gives the iterator type for a nested Empty_set_view.
///
/// @tparam Set_traits_t Nested set traits.
template <Is_nested_set_traits Set_traits_t>
using Empty_nested_iterator_type = mysql::iterators::Empty_sequence_iterator<
    std::pair<const typename Set_traits_t::Key_t,
              Empty_nested_mapped_type<Set_traits_t>>>;

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// View over an empty Nested set
///
/// @tparam Set_traits_tp Nested set traits.
template <Is_nested_set_traits Set_traits_tp>
class Empty_set_view<Nested_set_category_tag, Set_traits_tp>
    : public Nested_view_interface<
          Empty_set_view<Nested_set_category_tag, Set_traits_tp>,
          detail::Empty_nested_iterator_type<Set_traits_tp>,
          detail::Empty_nested_iterator_type<Set_traits_tp>, Set_traits_tp> {
 public:
  using Key_t = typename Set_traits_tp::Key_t;
  using Mapped_t = detail::Empty_nested_mapped_type<Set_traits_tp>;
  using Iterator_t = detail::Empty_nested_iterator_type<Set_traits_tp>;

  [[nodiscard]] constexpr auto begin() const { return Iterator_t(); }

  [[nodiscard]] constexpr auto end() const { return Iterator_t(); }

  [[nodiscard]] constexpr Iterator_t find(const Key_t & /*key*/) const {
    return Iterator_t();
  }

  [[nodiscard]] constexpr Iterator_t find(Iterator_t & /*cursor*/,
                                          const Key_t & /*key*/) const {
    return Iterator_t();
  }

};  // class Empty_set_view

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_NESTED_SET_CONST_VIEWS_H
