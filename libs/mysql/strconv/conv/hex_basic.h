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

#ifndef MYSQL_STRCONV_CONV_HEX_BASIC_H
#define MYSQL_STRCONV_CONV_HEX_BASIC_H

/// @file
/// Experimental API header

#include <string_view>                           // string_view
#include "mysql/meta/is_specialization.h"        // Is_nontype_specialization
#include "mysql/strconv/decode/parser.h"         // Parser
#include "mysql/strconv/encode/string_target.h"  // Is_string_target
#include "mysql/strconv/formats/hex_format.h"    // Hex_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Format strings into hex format ====

/// Format the given char buffer in hex format.
///
/// @param format Type tag to identify that this relates to hex format.
///
/// @param[out] target Target object to which the string will be written.
///
/// @param sv Input string.
template <Is_string_target Target_t>
void encode_impl(
    const mysql::meta::Is_nontype_specialization<Hex_format> auto &format,
    Target_t &target, const std::string_view &sv) {
  if constexpr (Target_t::target_type == Target_type::counter) {
    target.advance(2 * sv.size());
  } else {
    for (unsigned char ch : sv) {
      target.write_char(format.int_to_hex(ch >> 4));
      target.write_char(format.int_to_hex(ch & 0xf));
    }
  }
}

// ==== Parse strings from hex format ====

/// Read into a single character from a string of two hex digits.
///
/// @param format Type tag to identify that this is hex format. Also holds the
/// minimum and maximum allowed length of the resulting plaintext string.
///
/// @param[in,out] parser reference to Parser
///
/// @param target String target object to write to.
void decode_impl(
    const mysql::meta::Is_nontype_specialization<Hex_format> auto &format,
    Parser &parser, Is_string_target auto &target) {
  if (parser.remaining_size() < 2) {
    parser.set_parse_error("Expected at least two hex digits");
    return;
  }
  auto hi = format.hex_to_int(*parser.upos());
  if (hi == -1) {
    parser.set_parse_error("Expected hex digit");
    return;
  }
  ++parser;

  auto lo = format.hex_to_int(*parser.upos());
  if (lo == -1) {
    parser.set_parse_error("Expected hex digit");
    return;
  }
  ++parser;

  target.write_char((hi << 4) + lo);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_HEX_BASIC_H
