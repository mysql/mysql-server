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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_SETS_INT_SET_TRAITS_H
#define MYSQL_SETS_INT_SET_TRAITS_H

/// @file
/// Experimental API header

#include <compare>                                    // strong_ordering
#include <limits>                                     // numeric_limits
#include "mysql/sets/ordered_set_traits_interface.h"  // Ordered_set_traits_interface
#include "mysql/sets/set_traits.h"  // Is_discrete_metric_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

/// Set traits for integral types.
///
/// @tparam Element_tp The integral type.
///
/// @tparam min_tp The minimum value. Default is the minimum value for the type
/// (negative for signed types, 0 for unsigned types).
///
/// @tparam max_exclusive_tp The maximum value. Default is the maximum value for
/// the type.
///
/// @note This reserves std::numeric_limits<Element_tp>::max() for *exclusive*
/// endpoints, so that value cannot be represented in the set.
template <std::integral Element_tp,
          Element_tp min_tp = std::numeric_limits<Element_tp>::min(),
          Element_tp max_exclusive_tp = std::numeric_limits<Element_tp>::max()>
struct Int_set_traits
    : public Ordered_set_traits_interface<
          Int_set_traits<Element_tp, min_tp, max_exclusive_tp>, Element_tp,
          std::make_unsigned_t<Element_tp>> {
  using Base_t = Ordered_set_traits_interface<
      Int_set_traits<Element_tp, min_tp, max_exclusive_tp>, Element_tp,
      std::make_unsigned_t<Element_tp>>;
  using Element_t = Base_t::Element_t;
  using Difference_t = Base_t::Difference_t;

  /// Return the minimum allowed value.
  [[nodiscard]] static constexpr Element_t min() { return min_tp; }

  /// @return The maximum allowed value for exclusive endpoints.
  ///
  /// @note The set can store values that are strictly smaller than this number.
  [[nodiscard]] static constexpr Element_t max_exclusive() {
    return max_exclusive_tp;
  }

  /// @return true if @c left < @c right.
  [[nodiscard]] static constexpr bool lt_impl(const Element_t &left,
                                              const Element_t &right) {
    return left < right;
  }

  /// @return true if @c left < @c right.
  [[nodiscard]] static constexpr std::strong_ordering cmp_impl(
      const Element_t &left, const Element_t &right) {
    return left <=> right;
  }

  /// @return @c element + 1.
  [[nodiscard]] static constexpr Element_t next(const Element_t &element) {
    return element + 1;
  }

  /// @return @c element - 1.
  [[nodiscard]] static constexpr Element_t prev(const Element_t &element) {
    return element - 1;
  }
};  // struct Int_set_traits

static_assert(Is_discrete_metric_set_traits<Int_set_traits<int>>);

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INT_SET_TRAITS_H
