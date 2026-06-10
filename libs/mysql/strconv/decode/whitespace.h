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

#ifndef MYSQL_STRCONV_DECODE_WHITESPACE_H
#define MYSQL_STRCONV_DECODE_WHITESPACE_H

/// @file
/// Experimental API header

#include "mysql/strconv/decode/parser.h"  // Parser

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Return true if the given character is a space character, i.e., ascii 9,
/// 10, 11, 12, 13, or 32.
///
/// This is derived from ctype_utf8mb3 and agrees with
/// my_isspace(&my_charset_utf8mb3_general_ci, x).
[[nodiscard]] constexpr bool is_space(int x) {
  return (x >= 9 && x <= 13) || x == 32;
}

/// Move the position forward until end or non-whitespace.
inline void skip_whitespace(Parser &parser) {
  while (!parser.is_sentinel() && is_space(*parser)) ++parser;
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_WHITESPACE_H
