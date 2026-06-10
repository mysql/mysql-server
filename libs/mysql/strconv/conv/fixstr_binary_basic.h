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

#ifndef MYSQL_STRCONV_CONV_FIXSTR_BINARY_BASIC_H
#define MYSQL_STRCONV_CONV_FIXSTR_BINARY_BASIC_H

/// @file
/// Experimental API header

#include <string_view>                                   // string_view
#include "mysql/strconv/decode/parser.h"                 // Parser
#include "mysql/strconv/encode/string_target.h"          // Is_string_target
#include "mysql/strconv/formats/fixstr_binary_format.h"  // Fixstr_binary_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Parse a string in fixed-length format into @c out, advance the position, and
/// return the status. The length is not encoded in the input string; it is a
/// nonstatic member of the format.
///
/// @param format Type tag to identify that this relates to fixed-width binary
/// format
///
/// @param[in,out] parser Parser position and parser.
///
/// @param[out] out Reference to string_view, which will be altered to point
/// directly into the input string.
inline void decode_impl(const Fixstr_binary_format &format, Parser &parser,
                        std::string_view &out) {
  if (parser.remaining_size() < format.m_string_size) {
    parser.advance(parser.remaining_size());
    parser.set_parse_error("Expected more characters");
    return;
  }
  out = std::string_view(parser.pos(), format.m_string_size);
  parser.advance(format.m_string_size);
}

/// Parse a string in fixed-length format into @c out, advance the position, and
/// return the status. The length is not encoded in the input string; it is a
/// nonstatic member of the format.
///
/// @param format Type tag to identify that this relates to fixed-width binary
/// format
///
/// @param[in,out] parser Parser position and parser.
///
/// @param[out] out Output string wrapper to store into.
void decode_impl(const Fixstr_binary_format &format, Parser &parser,
                 Is_string_target auto &out) {
  std::string_view sv;
  if (parser.read(format, sv) != mysql::utils::Return_status::ok) return;
  out.write_raw(sv);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_FIXSTR_BINARY_BASIC_H
