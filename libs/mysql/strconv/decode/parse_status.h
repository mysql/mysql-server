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

#ifndef MYSQL_STRCONV_DECODE_PARSE_STATUS_H
#define MYSQL_STRCONV_DECODE_PARSE_STATUS_H

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlStrconv
/// @{

#include "mysql/strconv/encode/string_target.h"  // Is_string_target
#include "mysql/strconv/formats/format.h"        // Debug_format

namespace mysql::strconv::detail {

/// The status after parsing an object from a string.
enum class Parse_status : char {
  /// The object could be successfully parsed.
  ok,

  /// There was previously a parse error; the parser has backtracked to a
  /// non-error status; the information about the parse error is still stored in
  /// the Parse_position object.
  ok_backtracked_from_parse_error,

  /// The object could not be successfully parsed because the string was wrong.
  parse_error,

  /// An object was parsed successfully from a prefix of the string, but there
  /// were extra characters after the string.
  fullmatch_error,

  /// No parse error was found in the string, but an error occurred when storing
  /// into the output object - for example, out-of-memory.
  store_error
};

}  // namespace mysql::strconv::detail

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_PARSE_STATUS_H
