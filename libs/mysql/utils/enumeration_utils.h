// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

#ifndef MYSQL_UTILS_ENUMERATION_UTILS_INCLUDED
#define MYSQL_UTILS_ENUMERATION_UTILS_INCLUDED

/// @file
/// Experimental API header

#include <mysql/utils/return_status.h>
#include <string>
#include <utility>

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// @brief Helper function that converts enum type to underlying integer type
/// @note This function may be removed after switching to C++23
/// @tparam Enum_type Type of the enumeration parameter that gets converted into
/// the underlying type value
template <typename Enum_type>
constexpr inline decltype(auto) to_underlying(Enum_type enum_value) {
  static_assert(
      std::is_enum<Enum_type>::value,
      "to_underlying conversion function called with non-enumeration argument");
  using EnumValueType = std::underlying_type_t<Enum_type>;
  return static_cast<EnumValueType>(enum_value);
}

/// @brief Template function that returns maximum *valid* constant that can
/// appear in the enumeration type. It must be specialized for each
/// enumeration type serialized
/// @tparam Enum_type Type of the enumeration that will be returned
/// @return Last valid enumeration constant within Enum_type
template <typename Enum_type>
constexpr inline Enum_type enum_max();

/// @brief Helper function that converts value of enumeration underlying type
///        into enumeration type constant
/// @tparam Enum_type Type of the enumeration that Integral_type parameter is
/// converted into
/// @tparam Integral_type Type of the enumeration parameter that gets converted
/// into the Enum_type
template <typename Enum_type, typename Integral_type>
constexpr inline std::pair<Enum_type, Return_status> to_enumeration(
    Integral_type value) {
  static_assert(std::is_enum_v<Enum_type>,
                "to_enumeration conversion requested for non-enumeration type");
  static_assert(std::is_integral_v<Integral_type>,
                "to_enumeration conversion requested from non-integral type");
  if (value > to_underlying(enum_max<Enum_type>())) {
    return std::make_pair(enum_max<Enum_type>(), Return_status::error);
  }
  return std::make_pair(static_cast<Enum_type>(value), Return_status::ok);
}

}  // namespace mysql::utils

/// @}

#endif  // MYSQL_UTILS_ENUMERATION_UTILS_INCLUDED
