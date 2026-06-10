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

#ifndef MYSQL_STRCONV_CONV_TEXT_BASIC_H
#define MYSQL_STRCONV_CONV_TEXT_BASIC_H

/// @file
/// Experimental API header

#include <charconv>                             // to_chars
#include <concepts>                             // unsigned_integral
#include <type_traits>                          // conditional
#include "mysql/math/int_pow.h"                 // int_log
#include "mysql/strconv/decode/parser.h"        // Parser
#include "mysql/strconv/formats/text_format.h"  // Text_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Format integers and strings into text format ====

/// Compute the length of a text-formatted integer.
///
/// @tparam Value_t Type of the value.
///
/// @param format Type tag to identify that this relates to text format.
///
/// @param[out] counter String_counter object to which the string will be
/// written.
///
/// @param value The value to compute the text representation length for.
template <std::integral Value_t>
constexpr void encode_impl(const Text_format &format [[maybe_unused]],
                           String_counter &counter, Value_t value) {
  if constexpr (std::same_as<Value_t, bool>) {
    // Special case for bool because make_unsigned is not defined for bool.
    counter.advance(1);
  } else {
    using Unsigned_value_t = std::make_unsigned_t<Value_t>;
    Unsigned_value_t unsigned_value{};
    if constexpr (std::signed_integral<Value_t>) {
      if (value < 0) {
        // Minus sign, then the positive part.
        counter.advance(1);
        // Negating MIN_INT, or casting a negative signed value to unsigned,
        // are both undefined behavior. Instead, invert the bits, cast to
        // unsigned, and add 1. This sequence of arithmetic operations is
        // equivalent to negation, and all steps are defined.
        unsigned_value = Unsigned_value_t(~value) + Unsigned_value_t(1);
      } else {
        unsigned_value = Unsigned_value_t(value);
      }
    } else {
      unsigned_value = value;
    }
    counter.advance(1 +
                    mysql::math::int_log<Unsigned_value_t(10)>(unsigned_value));
  }
}

/// Write a text-formatted integer to a String_writer.
///
/// @tparam Value_t Type of the value.
///
/// @param format Type tag to identify that this relates to text format.
///
/// @param[out] writer String_writer object to which the string will be written.
///
/// @param value The value to write.
template <std::integral Value_t>
constexpr void encode_impl(const Text_format &format [[maybe_unused]],
                           String_writer &writer, Value_t value) {
  // to_chars is not defined for bool, so cast bool to char.
  using Store_t =
      std::conditional_t<std::same_as<Value_t, bool>, char, Value_t>;
  auto ret = std::to_chars(writer.pos(), writer.end(), Store_t(value));
  // It is required that the caller provides a sufficient buffer. This is
  // guaranteed by this library, so we assert it.
  assert(ret.ec == std::errc());
  writer.advance(ret.ptr - writer.pos());
}

/// Format the given string_view in text format, by copying the string data
/// literally to the output buffer.
///
/// Note that neither the string length nor any delimiter is written to the
/// output, so a parser cannot in general compute the end in case the written
/// string is followed by other text. Therefore there is no corresponding
/// `decode_impl` function.
///
/// @param format Type tag to identify that this relates to text format.
///
/// @param[out] target Target object to which the string will be written.
///
/// @param sv Input string.
void encode_impl(const Text_format &format [[maybe_unused]],
                 Is_string_target auto &target, const std::string_view &sv) {
  target.write_raw(sv);
}

// ==== Parse integers from text format ====

/// Parse an integer in text format into out.
///
/// @param format Type tag to identify that this relates to text format.
///
/// @param[in,out] parser Parser position.
///
/// @param[out] out Destination value.
///
/// The possible error states are:
///
/// - not found: The position was at end-of-string, or the first character was a
///   non-number.
///
/// - parse_error: The number was out of range.
template <std::integral Value_t>
void decode_impl(const Text_format &format [[maybe_unused]], Parser &parser,
                 Value_t &out) {
  Value_t value;
  auto ret = std::from_chars(parser.pos(), parser.end(), value);
  switch (ret.ec) {
    case std::errc::invalid_argument:
      parser.set_parse_error("Expected number");
      break;
    case std::errc::result_out_of_range:
      parser.set_parse_error("Number out of range");
      break;
    default:
      assert(ret.ec == std::errc());
      out = value;
      parser += (ret.ptr - parser.pos());
      break;
  }
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_TEXT_BASIC_H
