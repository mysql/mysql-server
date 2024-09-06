// Copyright (c) 2024, Oracle and/or its affiliates.
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

#ifndef MYSQL_ABI_HELPERS_FIELD_H
#define MYSQL_ABI_HELPERS_FIELD_H

/// @file
/// Experimental API header

#include <cstdlib>  // std::size_t

/// @addtogroup GroupLibsMysqlAbiHelpers
/// @{

namespace mysql::abi_helpers {

/// A type code and a value that is either a 64 bit integer, a boolean, or a
/// bounded-length string.
template <class Type_enum_tp>
  requires requires { std::is_enum_v<Type_enum_tp>; }
class Field {
 public:
  using Type_enum_t = Type_enum_tp;

  /// @brief The type of the field
  Type_enum_t m_type;

  /// @brief The data of the field
  union {
    long long m_int;
    bool m_bool;
    char *m_string;
  } m_data;
};

}  // namespace mysql::abi_helpers

// addtogroup GroupLibsMysqlAbiHelpers
/// @}

#endif  // ifndef MYSQL_ABI_HELPERS_FIELD_H
