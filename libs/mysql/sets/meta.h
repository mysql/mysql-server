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

#ifndef MYSQL_SETS_META_H
#define MYSQL_SETS_META_H

/// @file
/// Experimental API header

#include <type_traits>  // false_type

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Helper to implement Has_fast_size. This is true if Set_t::disable_fast_size
/// is a constexpr bool member with value true; false if the member is absent or
/// false.
template <class Set_t>
concept Has_disabled_fast_size = Set_t::disable_fast_size;

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// Determines if the given type has "fast" size computations.
///
/// Set types can declare themselves as having "slow" size computations by
/// defining a constexpr bool member variable named `disable_fast_size` and
/// having the value `true`.
///
/// Typically, containers that store the size explicitly should *not* define
/// disable_fast_size, whereas views over set operations, which may compute the
/// size by iterating over the set, *should* define disable_fast_size.
///
/// For sets that *have* fast size operations, operations such as set equality
/// can implement optimizations, by comparing the size first, and compare the
/// full set element-by-element only for same-sized sets.
template <class Set_t>
concept Has_fast_size = (!detail::Has_disabled_fast_size<Set_t>);

}  // namespace mysql::sets

namespace mysql::sets {

/// Customization point that set container types can use to indicate that they
/// support noexcept move-semantics for full-set-copy operations between the
/// given types. Note that this can be defined for different, compatible types.
/// This will be invoked with cvref-removed `Source_t` and `Target_t`, and
/// should indicate if move-semantics is enabled for `Source_t&&` and
/// `Target_t`.
template <class Source_t, class Target_t>
struct Enable_donate_set : public std::false_type {};

/// Enable move-semantics when moving one set to itself.
template <class Set_t>
struct Enable_donate_set<Set_t, Set_t> : public std::true_type {};

/// True if move-semantics has been declared as enabled for full-set-copy
/// operations for the given operand types, with cvref removed from `Source_t`.
template <class Source_t, class Target_t>
concept Can_donate_set_unqualified =
    Enable_donate_set<std::remove_cvref_t<Source_t>, Target_t>::value;

/// True if move-semantics has been declared as enabled for full-set-copy
/// operations for the given operand types, and `Source_t` is an rvalue
/// reference type.
template <class Source_t, class Target_t>
concept Can_donate_set = Can_donate_set_unqualified<Source_t, Target_t> &&
                         std::is_rvalue_reference_v<Source_t>;

/// Customization point that set container types can use to indicate that they
/// support noexcept move-semantics to copy parts of a set during
/// inplace_union/inplace_intersect/inplace_subtract operations between the
/// given types. Note that this can be defined for different, compatible types.
/// This will be invoked with cvref-removed `Source_t` and `Target_t`, and
/// should indicate if move-semantics is enabled for `Source_t&&` and
/// `Target_t`.
template <class Source_t, class Target_t>
struct Enable_donate_set_elements : public std::false_type {};

/// True if move-semantics has been declared as enabled for
/// inplace_union/inplace_intersect/inplace_subtract operations for the given
/// operand types, with cvref removed from `Source_t`.
template <class Source_t, class Target_t>
concept Can_donate_set_elements_unqualified =
    Enable_donate_set_elements<std::remove_cvref_t<Source_t>, Target_t>::value;

/// True if move-semantics has been declared as enabled for
/// inplace_union/inplace_intersect/inplace_subtract operations for the given
/// operand types, and `Source_t` is an rvalue reference type.
template <class Source_t, class Target_t>
concept Can_donate_set_elements =
    Can_donate_set_elements_unqualified<Source_t, Target_t> &&
    std::is_rvalue_reference_v<Source_t>;

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_META_H
