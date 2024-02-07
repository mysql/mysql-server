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

#ifndef MYSQL_UTILS_IS_SPECIALIZATION_H
#define MYSQL_UTILS_IS_SPECIALIZATION_H

/// @file
/// Experimental API header

#include <type_traits>

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// @brief Helper struct used to determine at compile time
/// whether a given type T is a template specialization of the Primary
template <typename Test, template <typename...> class Primary>
struct Is_specialization : std::false_type {};

/// @brief Helper struct used to determine at compile time
/// whether a given type T is a template specialization of the Primary
template <template <typename...> class Primary, typename... Args>
struct Is_specialization<Primary<Args...>, Primary> : std::true_type {};

}  // namespace mysql::utils

/// @}

#endif  // MYSQL_UTILS_IS_SPECIALIZATION_H
