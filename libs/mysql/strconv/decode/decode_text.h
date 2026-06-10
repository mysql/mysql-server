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

#ifndef MYSQL_STRCONV_DECODE_DECODE_TEXT_H
#define MYSQL_STRCONV_DECODE_DECODE_TEXT_H

/// @file
/// Experimental API header

#include <string_view>                          // string_view
#include "mysql/strconv/decode/decode.h"        // decode
#include "mysql/strconv/formats/text_format.h"  // Text_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Parse from a string into the given object in text format.
///
/// @param in_sv Input represented as `string_view`.
///
/// @param[out] out Reference to object to read into. In case of error, this may
/// be in a "half-parsed" state.
///
/// @return Parser that can be queried for success and error messages.
[[nodiscard]] Parser decode_text(const std::string_view &in_sv, auto &out) {
  return decode(Text_format{}, in_sv, out);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_DECODE_TEXT_H
