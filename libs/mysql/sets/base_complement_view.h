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

#ifndef MYSQL_SETS_BASE_COMPLEMENT_VIEW_H
#define MYSQL_SETS_BASE_COMPLEMENT_VIEW_H

/// @file
/// Experimental API header

#include <ranges>       // enable_view
#include <type_traits>  // is_pointer_v

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets {

// Primary template for views over the complement operation.
///
/// This can be specialized to specific set categories, e.g. boundary sets,
/// interval sets, and nested sets.
template <class>
class Complement_view;

// ==== Factory functions ====

/// Return the Complement_view over the argument.
///
/// @tparam Source_t Type of source.
///
/// @param source Source.
template <class Source_t>
[[nodiscard]] auto make_complement_view(const Source_t &source) {
  static_assert(!std::is_pointer_v<Source_t>);
  return Complement_view<Source_t>(source);
}

/// Make complement-of-complement return the original set.
///
/// @tparam Source_t Type of complemented source.
///
/// @param complement Source.
template <class Source_t>
[[nodiscard]] decltype(auto) make_complement_view(
    const Complement_view<Source_t> &complement) {
  return complement.source();
}

}  // namespace mysql::sets

// ==== Declarations of view ====

/// @cond DOXYGEN_DOES_NOT_UNDERSTAND_THIS
template <class Source_t>
constexpr bool
    std::ranges::enable_view<mysql::sets::Complement_view<Source_t>> = true;
/// @endcond

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_BASE_COMPLEMENT_VIEW_H
