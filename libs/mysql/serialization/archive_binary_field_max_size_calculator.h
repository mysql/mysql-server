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

#ifndef MYSQL_SERIALIZATION_ARCHIVE_BINARY_FIELD_MAX_SIZE_CALCULATOR_H
#define MYSQL_SERIALIZATION_ARCHIVE_BINARY_FIELD_MAX_SIZE_CALCULATOR_H

#include <sstream>

#include "mysql/serialization/primitive_type_codec.h"
#include "mysql/serialization/serializable_type_traits.h"
#include "mysql/serialization/serialization_types.h"

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief helper structure created for partial specialization of get_max_size
/// function of Archive_binary, default version
template <class T, Field_size S>
struct Archive_binary_field_max_size_calculator {
  /// @brief Calculates maximum encoded size of the fixed-length integer and
  /// floating point fields
  /// @return maximum encoded byte length
  static constexpr std::size_t get_max_size() { return S; }
};

/// @brief helper structure created for partial specialization of get_max_size
/// function of Archive_binary, specialization for defined size equal to 0.
/// 0 means default maximum size: for std::string default maximum size is
/// unlimited, for variable length integer - sizeof(Type)+1, for double Type
/// it equals to sizeof(Type)
/// For unlimited strings, get_max_size is disabled (code won't compile)
/// @tparam T type of field
template <class T>
struct Archive_binary_field_max_size_calculator<T, 0> {
  /// @brief Calculates maximum encoded size of the
  /// variable-length integer fields
  /// @return maximum encoded byte length
  template <typename = std::enable_if_t<is_string_type<T>() == false>>
  static constexpr std::size_t get_max_size() {
    return sizeof(T) + is_integral_type<T>();  // + 1 for variable length
  }
};

/// @brief helper structure created for partial specialization of get_max_size
/// function of Archive_binary, version for std::string fields
/// @tparam S Bound size for the string field
template <Field_size S>
struct Archive_binary_field_max_size_calculator<std::string, S> {
  /// @brief Calculates maximum encoded size of the bounded
  /// std::string field
  /// @return maximum encoded byte length
  static constexpr std::size_t get_max_size() {
    return S + Archive_binary_field_max_size_calculator<uint64_t,
                                                        0>::get_max_size();
  }
};

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_ARCHIVE_BINARY_FIELD_MAX_SIZE_CALCULATOR_H
