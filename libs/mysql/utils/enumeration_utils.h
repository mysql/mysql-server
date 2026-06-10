// Copyright (c) 2023, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_UTILS_ENUMERATION_UTILS_H
#define MYSQL_UTILS_ENUMERATION_UTILS_H

/// @file
/// Experimental API header

#include <string>
#include <utility>
#include "mysql/utils/return_status.h"

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// true if Enum_t is an enumeration type (scoped or not).
///
/// This is equivalent to is_enum_v, but as a concept, is syntactically valid in
/// more contexts.
template <class Enum_t>
concept Is_enum = std::is_enum_v<Enum_t>;

/// @brief Helper function that converts enum type to underlying integer type
/// @note This function may be removed after switching to C++23
/// @tparam Enum_type Type of the enumeration parameter that gets converted into
/// the underlying type value
template <Is_enum Enum_type>
constexpr decltype(auto) to_underlying(Enum_type enum_value) {
  using EnumValueType = std::underlying_type_t<Enum_type>;
  return static_cast<EnumValueType>(enum_value);
}

/// @brief Template function that returns maximum *valid* constant that can
/// appear in the enumeration type. It must be specialized for each
/// enumeration type serialized
/// @tparam Enum_type Type of the enumeration that will be returned
/// @return Last valid enumeration constant within Enum_type
template <Is_enum Enum_type>
constexpr Enum_type enum_max();

/// @brief Helper function that converts value of enumeration underlying type
///        into enumeration type constant
/// @tparam Enum_type Type of the enumeration that Integral_type parameter is
/// converted into
template <Is_enum Enum_type>
constexpr std::pair<Enum_type, Return_status> to_enumeration(
    std::integral auto value) {
  if (value > to_underlying(enum_max<Enum_type>())) {
    return std::make_pair(enum_max<Enum_type>(), Return_status::error);
  }
  return std::make_pair(static_cast<Enum_type>(value), Return_status::ok);
}

}  // namespace mysql::utils

/// @}

#endif  // MYSQL_UTILS_ENUMERATION_UTILS_H
