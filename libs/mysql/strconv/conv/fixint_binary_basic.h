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

#ifndef MYSQL_STRCONV_CONV_FIXINT_BINARY_BASIC_H
#define MYSQL_STRCONV_CONV_FIXINT_BINARY_BASIC_H

/// @file
/// Experimental API header

#include <concepts>                                      // integral
#include "my_byteorder.h"                                // uint8korr
#include "mysql/strconv/decode/parser.h"                 // Parser
#include "mysql/strconv/encode/string_target.h"          // Is_string_target
#include "mysql/strconv/formats/fixint_binary_format.h"  // Fixint_binary_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Format integers into fixed-length binary format ====

/// Format an integer in fixed-length, binary format.
///
/// The format is 8-byte, little endian.
///
/// @tparam Target_t Type of `target`.
///
/// @param format Type tag to identify that this relates to binary format with
/// fixed-length integers.
///
/// @param[out] target Target object to which the string will be written.
///
/// @param value The value to write.
template <Is_string_target Target_t>
void encode_impl(const Fixint_binary_format &format [[maybe_unused]],
                 Target_t &target, const std::integral auto &value) {
  if constexpr (Target_t::target_type == Target_type::counter) {
    target.advance(8);
  } else {
    int8store(target.pos(), value);
    target.advance(8);
  }
}

// ==== Parse integers from fixed-length binary format ====

/// Parse an integral in fixed-width integer format type into @c out, advance
/// the position, and return the status.
///
/// The format is 8 byte, little-endian.
///
/// @tparam Value_t Unsigned integral type to read.
///
/// @param format Type tag to identify that this relates to binary format with
/// fixed-width integers.
///
/// @param[in,out] parser Parser position and parser.
///
/// @param[out] out Destination value.
///
/// The possible error states are:
///
/// - not found: The position was at end-of-string, or the first character was a
/// non-number.
///
/// - parse_error: The number was out of range.
template <std::integral Value_t>
void decode_impl(const Fixint_binary_format &format [[maybe_unused]],
                 Parser &parser, Value_t &out) {
  if (parser.remaining_size() < 8) {
    parser.set_parse_error("Expected 8-byte unsigned integer");
    return;
  }
  if constexpr (std::unsigned_integral<Value_t>) {
    uint64_t tmp = uint8korr(parser.pos());
    if (tmp > uint64_t(std::numeric_limits<Value_t>::max())) {
      parser.set_parse_error("Unsigned integer out of range");
      return;
    }
    out = Value_t(tmp);
  } else {
    int64_t tmp = sint8korr(parser.pos());
    if (tmp > int64_t(std::numeric_limits<Value_t>::max()) ||
        tmp < int64_t(std::numeric_limits<Value_t>::min())) {
      parser.set_parse_error("Signed integer out of range");
      return;
    }
    out = Value_t(tmp);
  }
  parser.advance(8);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_FIXINT_BINARY_BASIC_H
