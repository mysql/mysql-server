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

#ifndef MYSQL_SETS_BOUNDARY_SET_BINARY_OPERATION_VIEW_BASE_H
#define MYSQL_SETS_BOUNDARY_SET_BINARY_OPERATION_VIEW_BASE_H

/// @file
/// Experimental API header

#include "mysql/meta/is_same_ignore_const.h"  // Is_same_ignore_const
#include "mysql/sets/boundary_set_binary_operation_iterator.h"  // Boundary_set_binary_operation_iterator
#include "mysql/sets/boundary_set_interface.h"     // Boundary_view_interface
#include "mysql/sets/boundary_set_meta.h"          // Is_boundary_set
#include "mysql/sets/optional_view_source_set.h"   // Optional_view_source_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Common base class for the specializations of Union_view, Intersection_view,
/// and Subtraction_view for boundary sets.
///
/// This provides forward iterators.
///
/// @tparam Source1_tp Type of the left boundary set.
///
/// @tparam Source2_tp Type of the left boundary set.
///
/// @tparam operation_tp The type of binary set operation.
template <Is_boundary_set Source1_tp, Is_boundary_set Source2_tp,
          Binary_operation operation_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Boundary_set_binary_operation_view_base
    : public Boundary_view_interface<Boundary_set_binary_operation_view_base<
                                         Source1_tp, Source2_tp, operation_tp>,
                                     Boundary_set_binary_operation_iterator<
                                         Source1_tp, Source2_tp, operation_tp>,
                                     Boundary_set_binary_operation_iterator<
                                         Source1_tp, Source2_tp, operation_tp>,
                                     typename Source1_tp::Set_traits_t> {
 public:
  using Source1_t = Source1_tp;
  using Source2_t = Source2_tp;
  using Opt_source1_t = Optional_view_source_set<Source1_t>;
  using Opt_source2_t = Optional_view_source_set<Source2_t>;
  static constexpr auto operation = operation_tp;
  using This_t = Boundary_set_binary_operation_view_base<Source1_t, Source2_t,
                                                         operation_tp>;
  using Iterator1_t = mysql::ranges::Range_const_iterator_type<Source1_t>;
  using Iterator2_t = mysql::ranges::Range_const_iterator_type<Source2_t>;
  using Set_traits_t = typename Source1_t::Set_traits_t;
  using Element_t = Set_traits_t::Element_t;
  using Iterator_t =
      Boundary_set_binary_operation_iterator<Source1_t, Source2_t, operation>;
  static constexpr bool disable_fast_size = true;

  /// Construct a new, empty view.
  Boundary_set_binary_operation_view_base() noexcept = default;

  /// Construct a new view over the given boundary set sources.
  ///
  /// @param source1 The first source.
  ///
  /// @param source2 The second source.
  Boundary_set_binary_operation_view_base(const Source1_t &source1,
                                          const Source2_t &source2) noexcept
      : m_source1(source1), m_source2(source2) {}

  /// Construct a new view over the given boundary set sources.
  ///
  /// Use this constructor if one of the sources may be empty and you do not
  /// have an object representing it; then pass nullptr for that source.
  ///
  /// @param source1 The first source, or nullptr for empty set.
  ///
  /// @param source2 The second source, or nullptr for empty set.
  Boundary_set_binary_operation_view_base(const Source1_t *source1,
                                          const Source2_t *source2) noexcept
      : m_source1(source1), m_source2(source2) {}

  // Default rule-of-5.

  /// @return iterator to the first output boundary.
  [[nodiscard]] auto begin() const {
    return Iterator_t(m_source1.pointer(), m_source2.pointer(),
                      m_source1.begin(), m_source2.begin());
  }

  /// @return iterator to sentinel in the output set.
  [[nodiscard]] auto end() const {
    return Iterator_t(m_source1.pointer(), m_source2.pointer(), m_source1.end(),
                      m_source2.end());
  }

  /// For internal use by the CTRP base class only.
  ///
  /// @return lower bound for the given element in the given
  /// Boundary_set_binary_operation_view_base object.
  template <class Iter_t>
  [[nodiscard]] static constexpr Iter_t lower_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const Iter_t &hint,
      const Element_t &element) {
    return Iter_t(self.m_source1.pointer(), self.m_source2.pointer(),
                  self.m_source1.lower_bound(hint.position1(), element),
                  self.m_source2.lower_bound(hint.position2(), element));
  }

  /// For internal use by the CTRP base class only.
  ///
  /// @return upper bound for the given element in the given
  /// Boundary_set_binary_operation_view_base object.
  template <class Iter_t>
  [[nodiscard]] static constexpr Iter_t upper_bound_impl(
      mysql::meta::Is_same_ignore_const<This_t> auto &self, const Iter_t &hint,
      const Element_t &element) {
    return Iter_t(self.m_source1.pointer(), self.m_source2.pointer(),
                  self.m_source1.upper_bound(hint.position1(), element),
                  self.m_source2.upper_bound(hint.position2(), element));
  }

  /// Return pointer to the first boundary set operand.
  [[nodiscard]] const Source1_t *source1() const { return m_source1.pointer(); }

  /// Return pointer to the second boundary set operand.
  [[nodiscard]] const Source2_t *source2() const { return m_source2.pointer(); }

 private:
  /// Pointer to the first boundary set operand.
  Opt_source1_t m_source1{};

  /// Pointer to the second boundary set operand.
  Opt_source2_t m_source2{};
};  // class Boundary_set_binary_operation_view_base over boundary sets

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BOUNDARY_SET_BINARY_OPERATION_VIEW_BASE_H
