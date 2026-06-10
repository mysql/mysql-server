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

#ifndef MYSQL_SETS_BASE_BINARY_OPERATION_VIEWS_H
#define MYSQL_SETS_BASE_BINARY_OPERATION_VIEWS_H

/// @file
/// Experimental API header

#include <ranges>                         // enable_view
#include <type_traits>                    // is_pointer_v
#include "mysql/sets/binary_operation.h"  // Binary_operation

/// @addtogroup GroupLibsMysqlSets
/// @{
///
/// This file defines the primary template classes and factory functions for
/// `Union_view`, `Intersection_view`, and `Subtraction_view`. Each set category
/// may specialize the primary template to classes of the category. The operands
/// may be of different types, but must have the same Set traits.
///
/// This file also declares that the views satisfy `std::ranges::view`. And it
/// contains a helper enum type that lists the three set operations.

// ==== Primary templates ====

namespace mysql::sets {

/// Primary template for views over unions of two sets operations.
///
/// This can be specialized to specific set categories, e.g. boundary sets,
/// interval sets, and nested sets.
template <class Source1_tp, class Source2_tp>
class Union_view;

/// Primary template for views over intersections of two sets operations.
///
/// This can be specialized to specific set categories, e.g. boundary sets,
/// interval sets, and nested sets.
template <class Source1_tp, class Source2_tp>
class Intersection_view;

/// Primary template for views over subtractions of two sets operations.
///
/// This can be specialized to specific set categories, e.g. boundary sets,
/// interval sets, and nested sets.
template <class Source1_tp, class Source2_tp>
class Subtraction_view;

// ==== Parameterization by Binary_operation ====

/// For a Binary_operation and two operand sets, gives the corresponding
/// Union_view, Intersection_view, or Subtraction_view class.
//
// The casts to int are workarounds for what appears to be a bug in MSVC 19.29
// (VS16.11). It gives the error "error C2677: binary '==': no global operator
// found which takes type 'mysql::sets::Binary_operation' (or there is no
// acceptable conversion)".
//
// @todo Remove the casts when we drop support for the buggy compiler version.
template <Binary_operation binary_operation, class Source1_t, class Source2_t>
using Binary_operation_view_type = std::conditional_t<
    (int)binary_operation == (int)Binary_operation::op_union,
    Union_view<Source1_t, Source2_t>,
    std::conditional_t<
        (int)binary_operation == (int)Binary_operation::op_intersection,
        Intersection_view<Source1_t, Source2_t>,
        std::conditional_t<(int)binary_operation ==
                               (int)Binary_operation::op_subtraction,
                           Subtraction_view<Source1_t, Source2_t>,
                           /*impossible*/ void>>>;

// ==== Factory functions ====

/// Return the Union_view, Intersection_view, or Subtraction_view over the
/// arguments, according to the given Binary_operation.
///
/// @tparam binary_operation Type of operation.
///
/// @tparam Source1_t Type of first operand.
///
/// @tparam Source2_t Type of second operand.
///
/// @param source1 First operand.
///
/// @param source2 Second operand.
template <Binary_operation binary_operation, class Source1_t, class Source2_t>
[[nodiscard]] auto make_binary_operation_view(const Source1_t &source1,
                                              const Source2_t &source2) {
  static_assert(!std::is_pointer_v<Source1_t> && !std::is_pointer_v<Source2_t>);
  return Binary_operation_view_type<binary_operation, Source1_t, Source2_t>(
      source1, source2);
}

/// Return the Union_view over the arguments.
///
/// @tparam Source1_t Type of first operand.
///
/// @tparam Source2_t Type of second operand.
///
/// @param source1 First operand.
///
/// @param source2 Second operand.
template <class Source1_t, class Source2_t>
[[nodiscard]] auto make_union_view(const Source1_t &source1,
                                   const Source2_t &source2) {
  return make_binary_operation_view<Binary_operation::op_union>(source1,
                                                                source2);
}

/// Return the Intersection_view over the arguments.
///
/// @tparam Source1_t Type of first operand.
///
/// @tparam Source2_t Type of second operand.
///
/// @param source1 First operand.
///
/// @param source2 Second operand.
template <class Source1_t, class Source2_t>
[[nodiscard]] auto make_intersection_view(const Source1_t &source1,
                                          const Source2_t &source2) {
  return make_binary_operation_view<Binary_operation::op_intersection>(source1,
                                                                       source2);
}

/// Return the Subtraction_view over the arguments.
///
/// @tparam Source1_t Type of first operand.
///
/// @tparam Source2_t Type of second operand.
///
/// @param source1 First operand.
///
/// @param source2 Second operand.
template <class Source1_t, class Source2_t>
[[nodiscard]] auto make_subtraction_view(const Source1_t &source1,
                                         const Source2_t &source2) {
  return make_binary_operation_view<Binary_operation::op_subtraction>(source1,
                                                                      source2);
}

}  // namespace mysql::sets

// ==== Declarations of std::ranges::enable_view ====

/// @cond DOXYGEN_DOES_NOT_UNDERSTAND_THIS
/// Declare that Union_view is a view.
template <class Source1_t, class Source2_t>
constexpr bool
    std::ranges::enable_view<mysql::sets::Union_view<Source1_t, Source2_t>> =
        true;

/// Declare that Intersection_view is a view.
template <class Source1_t, class Source2_t>
constexpr bool std::ranges::enable_view<
    mysql::sets::Intersection_view<Source1_t, Source2_t>> = true;

/// Declare that Subtraction_view is a view.
template <class Source1_t, class Source2_t>
constexpr bool std::ranges::enable_view<
    mysql::sets::Subtraction_view<Source1_t, Source2_t>> = true;
/// @endcond

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BASE_BINARY_OPERATION_VIEWS_H
