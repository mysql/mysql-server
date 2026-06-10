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

#ifndef MYSQL_STRCONV_CONV_BINARY_BASIC_H
#define MYSQL_STRCONV_CONV_BINARY_BASIC_H

/// @file
/// Experimental API header

#include "mysql/serialization/variable_length_integers.h"  // read_varlen_bytes_unsigned
#include "mysql/strconv/decode/parser.h"                   // Parser
#include "mysql/strconv/encode/string_target.h"            // Is_string_target
#include "mysql/strconv/formats/binary_format.h"           // Binary_format
#include "mysql/strconv/formats/fixstr_binary_format.h"  // Fixstr_binary_format
#include "mysql/strconv/formats/format.h"                // Format_base
#include "mysql/utils/return_status.h"                   // Return_status

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Format integers and strings into binary format ====

/// Format an integer in variable-length, binary format.
///
/// The format is the varint format defined in the `serialization` library.
///
/// @tparam Target_t Type of `target`.
///
/// @param format Type tag to identify that this relates to
/// binary format with variable-length integers.
///
/// @param[out] target Target object to which the string will be written.
///
/// @param value The value to write.
template <Is_string_target Target_t>
void encode_impl(const Binary_format &format [[maybe_unused]], Target_t &target,
                 const std::integral auto &value) {
  if constexpr (Target_t::target_type == Target_type::counter) {
    target.advance(
        mysql::serialization::detail::get_size_integer_varlen(value));
  } else {
    target.advance(
        mysql::serialization::detail::write_varlen_bytes(target.upos(), value));
  }
}

/// Format a string using variable-length, binary format for the length.
///
/// The format is the length followed by the string data. The length is in the
/// varint format defined in the `serialization` library.
///
/// @param format Type tag to identify that this relates to variable-length,
/// binary format.
///
/// @param[out] target Target object to which the string will be written.
///
/// @param sv string_view to write.
void encode_impl(const Binary_format &format, Is_string_target auto &target,
                 const std::string_view &sv) {
  target.write(format, sv.size());
  target.write_raw(sv);
}

// ==== Parse integers from binary format ====

/// Parse an integral in variable-width integer format type into @c out,
/// advance the position, and return the status.
///
/// The format is as given by the `serialization` library.
///
/// @param format Type tag to identify that this relates to binary format with
/// variable-width integers.
///
/// @param[in,out] parser Parser position and parser.
///
/// @param[out] out Variable of integral type to read into.
///
/// The possible error states are:
///
/// - not found: The position was at end-of-string, or the first character was a
///   non-number.
///
/// - parse_error: The number was out of range.
void decode_impl(const Binary_format &format [[maybe_unused]], Parser &parser,
                 std::integral auto &out) {
  if (parser.remaining_size() < 1) {
    parser.set_parse_error("Expected integer");
    return;
  }
  auto length = mysql::serialization::detail::read_varlen_bytes(
      parser.upos(), parser.remaining_size(), out);
  if (length == 0) {
    parser.set_parse_error("Expected integer");
    return;
  }
  parser.advance(length);
}

// ==== Parse strings from binary format ====

/// Parse a string in binary format, using variable-length integer format for
/// the length; store the result by making the given `std::string_view &` point
/// directly into the input string.
///
/// @param format Type tag to identify that this relates to binary format.
///
/// @param[in,out] parser Parser position and parser.
///
/// @param[out] out Reference to `std::string_view` which will be set to point
/// directly into the input string.
///
/// The possible error states are:
///
/// - not found: The position was at end-of-string, or the first character was a
/// non-number, or the remaining input is shorter than the string length.
///
/// - parse_error: The number was out of range.
inline void decode_impl(const Binary_format &format [[maybe_unused]],
                        Parser &parser, std::string_view &out) {
  uint64_t size{0};
  if (parser.read(format, size) != mysql::utils::Return_status::ok) return;
  if (parser.read(Fixstr_binary_format{size}, out) !=
      mysql::utils::Return_status::ok)
    return;
}

/// Parse a string in binary format, using variable-length integer format for
/// the length; make the given `std::string_ref &` refer to the corresponding
/// segment of the input buffer; advance the position; and return the status.
///
/// @param format Type tag to identify that this relates to binary format.
///
/// @param[in,out] parser Parser position and parser.
///
/// @param[out] out Destination string_view which will be set to point directly
/// into the input string.
///
/// The possible error states are:
///
/// - not found: The position was at end-of-string, or the first character was a
///   non-number.
///
/// - parse_error: The number was out of range.
inline void decode_impl(const Binary_format &format, Parser &parser,
                        Is_string_target auto &out) {
  std::string_view sv;
  if (parser.read(format, sv) != mysql::utils::Return_status::ok) return;
  out.write_raw(sv);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_BINARY_BASIC_H
