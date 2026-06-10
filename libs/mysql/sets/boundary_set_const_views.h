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

#ifndef MYSQL_SETS_BOUNDARY_SET_CONST_VIEWS_H
#define MYSQL_SETS_BOUNDARY_SET_CONST_VIEWS_H

/// @file
/// Experimental API header
///
/// This file defines *constant boundary set views*. These are boundary
/// sets whose values are specified at compile time, including:
///
/// - The empty set, represented by the class Empty_boundary_view
///   and produced by the factory function make_empty_boundary_view.
///
/// - The "full" set, containing all values in the Set traits, represented
///   by the class Full_boundary_view and produced by the factory
///   function make_full_boundary_view.
///
/// - Arbitrary sets, represented by the class
///   Const_boundary_view. The boundary values are given as template
///   arguments.

#include <algorithm>                             // upper_bound
#include <array>                                 // array
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/meta/is_same_ignore_const.h"     // Is_same_ignore_const
#include "mysql/sets/base_const_views.h"         // Empty_set_view
#include "mysql/sets/boundary_set_category.h"    // Boundary_set_category_tag
#include "mysql/sets/boundary_set_interface.h"   // Boundary_view_interface
#include "mysql/sets/ordered_set_traits_interface.h"  // Int_set_traits
#include "mysql/sets/set_traits.h"                    // Is_bounded_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Contiguous iterator over a Const_boundary_view.
///
/// @tparam Set_traits_tp Bounded set traits for the set type.
///
/// @tparam boundaries Values of all boundaries. This must have an even number
/// of elements.
template <Is_bounded_set_traits Set_traits_tp,
          typename Set_traits_tp::Element_t... boundaries>
class Const_boundary_view_iterator
    : public mysql::iterators::Iterator_interface<
          Const_boundary_view_iterator<Set_traits_tp, boundaries...>> {
 public:
  using Set_traits_t = Set_traits_tp;
  using Element_t = Set_traits_t::Element_t;
  static constexpr auto size = sizeof...(boundaries);
  using Array_t = std::array<Element_t, size>;
  using Iterator_category_t = std::contiguous_iterator_tag;

  static_assert(size % 2 == 0, "Const_boundary_view size must be even.");

  /// Construct an iterator to the beginning.
  Const_boundary_view_iterator() noexcept = default;

  /// Construct an iterator to the std::next(begin(), position).
  explicit Const_boundary_view_iterator(int position) : m_position(position) {}

  /// Return the boundary at the current position.
  [[nodiscard]] constexpr const Element_t *get_pointer() const {
    return array().data() + m_position;
  }

  /// Move the position forward by the given number of steps.
  constexpr void advance(std::ptrdiff_t delta) { m_position += delta; }

  /// Return the distance from the other iterator to this one.
  [[nodiscard]] constexpr std::ptrdiff_t distance_from(
      const Const_boundary_view_iterator &other) const {
    return m_position - other.m_position;
  }

  /// Return true if the current position is an endpoint.
  [[nodiscard]] constexpr bool is_endpoint() const {
    return (m_position & 1) == 1;
  }

  /// Return an array of the values.
  [[nodiscard]] static const Array_t &array() {
    static const Array_t ret{boundaries...};
    return ret;
  }

 private:
  // Index to the current position within the array.
  int m_position{0};
};  // class Const_boundary_view_iterator

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// Boundary set view for which the values are defined at compile-time.
///
/// @tparam Set_traits_tp Bounded set traits for the set type.
///
/// @tparam boundaries Values of all boundaries. This must have an even number
/// of elements.
template <Is_bounded_set_traits Set_traits_tp,
          typename Set_traits_tp::Element_t... boundaries>
class Const_boundary_view
    : public Boundary_view_interface<
          Const_boundary_view<Set_traits_tp, boundaries...>,
          detail::Const_boundary_view_iterator<Set_traits_tp, boundaries...>,
          detail::Const_boundary_view_iterator<Set_traits_tp, boundaries...>,
          Set_traits_tp> {
  using This_t = Const_boundary_view<Set_traits_tp, boundaries...>;

 public:
  using Set_traits_t = Set_traits_tp;
  using Element_t = typename Set_traits_t::Element_t;
  using Less_t = typename Set_traits_t::Less_t;
  using Iterator_t =
      detail::Const_boundary_view_iterator<Set_traits_tp, boundaries...>;
  using Const_iterator_t = Iterator_t;

  /// @return Iterator to the beginning.
  [[nodiscard]] auto begin() const { return Iterator_t(); }

  /// @return Iterator to the end.
  [[nodiscard]] auto end() const { return Iterator_t(Iterator_t::size); }

  /// Only for internal use by the CRTP base class.
  ///
  /// Return the upper bound for the given element in this object.
  template <class Iter_t>
  [[nodiscard]] static constexpr Iter_t upper_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const Iter_t &hint,
      const Element_t &element) {
    return std::upper_bound(hint, self.end(), element, Less_t());
  }

  /// Only for internal use by the CRTP base class.
  ///
  /// Return the lower bound for the given element in this object.
  template <class Iter_t>
  [[nodiscard]] static constexpr Iter_t lower_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const Iter_t &hint,
      const Element_t &element) {
    return std::lower_bound(hint, self.end(), element, Less_t());
  }
};  // class Const_boundary_view

/// View over the empty Boundary set for the given Set traits.
template <Is_bounded_set_traits Set_traits_tp>
class Empty_set_view<Boundary_set_category_tag, Set_traits_tp>
    : public Const_boundary_view<Set_traits_tp> {};

/// View over the Boundary set containing the full range of values for
/// the given Set traits.
template <Is_bounded_set_traits Set_traits_tp>
class Full_set_view<Boundary_set_category_tag, Set_traits_tp>
    : public Const_boundary_view<Set_traits_tp, Set_traits_tp::min(),
                                 Set_traits_tp::max_exclusive()> {};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_CONST_VIEWS_H
