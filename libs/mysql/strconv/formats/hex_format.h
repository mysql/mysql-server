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

#ifndef MYSQL_STRCONV_FORMATS_HEX_FORMAT_H
#define MYSQL_STRCONV_FORMATS_HEX_FORMAT_H

/// @file
/// Experimental API header

#include <array>                           // array
#include <cassert>                         // assert
#include "mysql/strconv/formats/format.h"  // Format_base

/// @addtogroup GroupLibsMysqlStrconv
/// @{

// ==== Conversion tables in namespace detail ====

namespace mysql::strconv::detail {

/// Conversion table with 16 elements, where element i is lowercase hex for i.
static inline constexpr std::array<unsigned char, 16> int_to_hex_lower{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

/// Conversion table with 16 elements, where element i is uppercase hex i.
static inline constexpr std::array<unsigned char, 16> int_to_hex_upper{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/// Policy for using uppercase/lowercase when parsing hex.
// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Hex_parse_case {
  /// Accept only lowercase
  lower,
  /// Accept only uppercase
  upper,
  /// Accept lowercase or uppercase
  flexible
};

/// Return a conversion table with 256 elements, where elements for hex chars
/// is the corresponding integer between 0 and 15, and other elements are -1.
template <Hex_parse_case hex_parse_case_tp>
class Hex_to_int {
 public:
  /// Return the table.
  static auto &table() {
    // build table on first invocation
    static auto &ret{build_table()};
    return ret;
  }

 private:
  /// Fill and return the conversion table.
  //
  // Todo: Make this constexpr when we have C++23
  static auto &build_table() {
    /// The conversion table.
    static std::array<int, 256> table;

    // Initialize all elements to -1
    for (int i = 0; i != 256; ++i) table[i] = -1;
    // Set elements corresponding to lowercase hex to the converted value.
    if (hex_parse_case_tp != Hex_parse_case::upper) {
      for (int i = 0; i != 16; ++i) table[int_to_hex_lower[i]] = i;
    }
    // Set elements corresponding to uppercase hex to the converted value.
    if (hex_parse_case_tp != Hex_parse_case::lower) {
      for (int i = 0; i != 16; ++i) table[int_to_hex_upper[i]] = i;
    }

    return table;
  }
};  // class Hex_to_int

}  // namespace mysql::strconv::detail

namespace mysql::strconv {

// ==== Helper type to define format variants ====

/// Policy for using uppercase/lowercase in hex conversion.
// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Hex_case {
  // Formatter uses lowercase, parser accepts lowercase or uppercase
  lower,
  // Formatter uses uppercase, parser accepts lowercase or uppercase
  upper,
  // Formatter uses lowercase, parser accepts lowercase only
  lower_only,
  // Formatter uses uppercase, parser accepts uppercase only
  upper_only
};

// ==== Format class ====

/// Format tag to identify hex format when encoding and decoding strings.
///
/// This also holds two member variables that bound the parsed string length
/// (impacting only decode, not encode).
template <Hex_case hex_case_tp = Hex_case::lower>
struct Hex_format : public Format_base {
  static constexpr Hex_case hex_case = hex_case_tp;

  /// Construct a new Hex_format with no bounds on the
  Hex_format() = default;

  /// Return the hex digit for a given integer in the range 0..15.
  static int int_to_hex(int half_byte) {
    assert(half_byte >= 0);
    assert(half_byte < 16);
    return (hex_case == Hex_case::lower_only || hex_case == Hex_case::lower
                ? detail::int_to_hex_lower
                : detail::int_to_hex_upper)[half_byte];
  }

  /// Return the numeric value between 0 and 15 for a given hex character, or -1
  /// if the character is not hex. The behavior is undefined if the hex
  /// character is not in the range 0 to 255.
  static int hex_to_int(int hex_char) {
    assert(hex_char >= 0);
    assert(hex_char < 256);
    return (detail::Hex_to_int < hex_case == Hex_case::lower_only
                ? detail::Hex_parse_case::lower
            : hex_case == Hex_case::upper_only
                ? detail::Hex_parse_case::upper
                : detail::Hex_parse_case::flexible > ::table())[hex_char];
  }
};  // class Hex_format

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_FORMATS_HEX_FORMAT_H
