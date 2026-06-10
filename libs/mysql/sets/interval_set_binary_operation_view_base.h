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

#ifndef MYSQL_SETS_INTERVAL_SET_BINARY_OPERATION_VIEW_BASE_H
#define MYSQL_SETS_INTERVAL_SET_BINARY_OPERATION_VIEW_BASE_H

/// @file
/// Experimental API header

#include "mysql/sets/interval_set_interface.h"     // Interval_set_interface
#include "mysql/sets/interval_set_meta.h"          // Is_interval_set
#include "mysql/sets/set_categories_and_traits.h"  // Is_compatible_set

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Common base class for the specializations of Union_view, Intersection_view,
/// and Subtraction_view for interval sets.
///
/// This provides forward iterators.
///
/// @tparam Source1_tp Type of first boundary set.
///
/// @tparam Source2_tp Type of second boundary set.
///
/// @tparam operation_tp The type of operation.
template <Is_interval_set Source1_tp, Is_interval_set Source2_tp,
          Binary_operation operation_tp>
  requires Is_compatible_set<Source1_tp, Source2_tp>
class Interval_set_binary_operation_view_base
    : public Interval_set_interface<
          Interval_set_binary_operation_view_base<Source1_tp, Source2_tp,
                                                  operation_tp>,
          Binary_operation_view_type<operation_tp,
                                     typename Source1_tp::Boundary_set_t,
                                     typename Source2_tp::Boundary_set_t>> {
 public:
  using Source1_t = Source1_tp;
  using Source2_t = Source2_tp;
  static constexpr auto operation = operation_tp;
  using This_t = Interval_set_binary_operation_view_base<Source1_t, Source2_t,
                                                         operation_tp>;
  using Set_traits_t = typename Source1_t::Set_traits_t;
  using Boundary_set_binary_operation_view_t =
      Binary_operation_view_type<operation_tp,
                                 typename Source1_tp::Boundary_set_t,
                                 typename Source2_tp::Boundary_set_t>;

  /// Construct a new, empty view.
  Interval_set_binary_operation_view_base() = default;

  /// Construct a new view over the two given interval set sources.
  ///
  /// @param source1 The first source.
  ///
  /// @param source2 The second source.
  explicit Interval_set_binary_operation_view_base(const Source1_t &source1,
                                                   const Source2_t &source2)
      : m_boundaries(source1.boundaries(), source2.boundaries()) {}

  /// Construct a new view over the two given interval set sources.
  ///
  /// Use this constructor if one of the sources may be empty and you do not
  /// have an object representing it; then pass nullptr for that source.
  ///
  /// @param source1 The first source, or nullptr for empty set.
  ///
  /// @param source2 The second source, or nullptr for empty set.
  explicit Interval_set_binary_operation_view_base(const Source1_t *source1,
                                                   const Source2_t *source2)
      : m_boundaries(source1 != nullptr ? &source1->boundaries() : nullptr,
                     source2 != nullptr ? &source2->boundaries() : nullptr) {}

  // Default rule-of-5.
  Interval_set_binary_operation_view_base(
      const Interval_set_binary_operation_view_base &) noexcept = default;
  Interval_set_binary_operation_view_base(
      Interval_set_binary_operation_view_base &&) noexcept = default;
  Interval_set_binary_operation_view_base &operator=(
      const Interval_set_binary_operation_view_base &) noexcept = default;
  Interval_set_binary_operation_view_base &operator=(
      Interval_set_binary_operation_view_base &&) noexcept = default;
  ~Interval_set_binary_operation_view_base() = default;

  /// Return the boundary set.
  [[nodiscard]] const auto &boundaries() const { return m_boundaries; }

 private:
  /// Boundary binary operation view over the boundary sets of the
  /// given interval sets.
  Boundary_set_binary_operation_view_t m_boundaries;
};  // class Interval_set_binary_operation_view_base

}  // namespace mysql::sets::detail

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_SET_BINARY_OPERATION_VIEW_BASE_H
