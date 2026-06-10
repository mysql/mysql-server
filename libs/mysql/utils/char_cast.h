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

#ifndef MYSQL_UTILS_CHAR_CAST_H
#define MYSQL_UTILS_CHAR_CAST_H

/// @file
/// Experimental API header

#include <cstddef>                   // byte
#include <utility>                   // forward
#include "mysql/meta/is_charlike.h"  // Is_charlike

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

// cast a reference to an Is_charlike type, to char &.
template <mysql::meta::Is_charlike Char_t = char>
Char_t &char_cast(mysql::meta::Is_charlike auto &ref) {
  return reinterpret_cast<Char_t &>(ref);
}

// cast a const reference to an Is_charlike type, to const char &.
template <mysql::meta::Is_charlike Char_t = char>
const Char_t &char_cast(const mysql::meta::Is_charlike auto &ref) {
  return reinterpret_cast<const Char_t &>(ref);
}

// cast a pointer to an Is_charlike type, to char *.
template <mysql::meta::Is_charlike Char_t = char>
Char_t *char_cast(mysql::meta::Is_charlike auto *ptr) {
  return reinterpret_cast<Char_t *>(ptr);
}

// cast a const pointer to an Is_charlike type, to const char *.
template <mysql::meta::Is_charlike Char_t = char>
const Char_t *char_cast(const mysql::meta::Is_charlike auto *ptr) {
  return reinterpret_cast<const Char_t *>(ptr);
}

/// Shorthand for `char_cast<unsigned char>`.
template <class Type_t>
decltype(auto) uchar_cast(Type_t &&value) {
  return char_cast<unsigned char>(std::forward<Type_t>(value));
}

/// Shorthand for `char_cast<std::byte>`.
template <class Type_t>
decltype(auto) byte_cast(Type_t &&value) {
  return char_cast<std::byte>(std::forward<Type_t>(value));
}

}  // namespace mysql::utils

// addtogroup GroupLibsMysqlUtils
/// @}

#endif  // ifndef MYSQL_UTILS_CHAR_CAST_H
