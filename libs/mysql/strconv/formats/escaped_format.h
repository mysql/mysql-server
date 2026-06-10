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

#ifndef MYSQL_STRCONV_FORMATS_ESCAPED_FORMAT_H
#define MYSQL_STRCONV_FORMATS_ESCAPED_FORMAT_H

/// @file
/// Experimental API header

#include <array>                           // array
#include <cstddef>                         // size_t
#include <string_view>                     // string_view
#include "mysql/strconv/formats/format.h"  // Format_base

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Helpers types to define format variants ====

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint

/// Whether output string should be enclosed in quote characters: "foo" vs foo.
enum class With_quotes { no, yes };

/// Whether ascii 128..255 should be preserved or escaped: backslash-xff vs
/// ascii 255.
enum class Preserve_high_characters { no, yes };

/// Whether ascii 7..13 should use hex instead of mnemonics: backslash-x0a vs
/// backslash-n.
enum class Numeric_control_characters { no, yes };

// NOLINTEND(performance-enum-size)

// ==== Format class ====

/// Format class to encode ascii strings.
///
/// @tparam quote_char_tp The character that surrounds a string, e.g. double
/// quote.
///
/// @tparam escape_char_tp The character that begins an escape sequence, e.g.
/// backslash.
///
/// @tparam preserve_high_characters_tp Whether ascii 128..255 should be
/// preserved or escaped: backslash-xff vs ascii 255.
///
/// @tparam numeric_control_characters_tp Whether ascii 7..13 should use hex
/// instead of mnemonics: backslash-x0a vs backslash-n.
template <char quote_char_tp = '"', char escape_char_tp = '\\',
          Preserve_high_characters preserve_high_characters_tp =
              Preserve_high_characters::no,
          Numeric_control_characters numeric_control_characters_tp =
              Numeric_control_characters::no>
  requires(quote_char_tp >= 32 && (unsigned)quote_char_tp < 128 &&
           escape_char_tp >= 32 && (unsigned)escape_char_tp < 128)
class Escaped_format : public Format_base {
 public:
  static constexpr char quote_char = quote_char_tp;
  static constexpr char escape_char = escape_char_tp;
  static constexpr Preserve_high_characters preserve_high_characters =
      preserve_high_characters_tp;
  static constexpr Numeric_control_characters numeric_control_characters =
      numeric_control_characters_tp;

  using Table_t = std::array<std::string_view, 256>;

  /// Construct a new Format object.
  ///
  /// @param with_quotes Indicates whether output strings should be enclosed in
  /// quote characters.
  explicit Escaped_format(With_quotes with_quotes = With_quotes::no)
      : m_with_quotes(with_quotes) {}

  /// Return the conversion table for this format.
  ///
  /// For a character `c` between 0 and 255, element `c` is the possibly-escaped
  /// form of character `c`, as a std::string_view.
  static const Table_t &table() {
    // Build the table on first invocation of this function.
    const static auto &tbl = build_table();
    return tbl;
  }

  /// Indicates whether output strings should be enclosed in quote characters.
  With_quotes m_with_quotes;

 private:
  /// Construct and return the table.
  //
  // Todo: Make this constexpr when we have C++23.
  static const Table_t &build_table() {
    char hex[] = "0123456789abcdef";
    for (int i = 0; i < 256; ++i) {
      auto &sv = m_table[i];
      auto &data = m_data_table[i];
      std::size_t size;
      if (i < 32 || (preserve_high_characters == Preserve_high_characters::no &&
                     i >= 128)) {
        data[0] = escape_char;
        if (numeric_control_characters == Numeric_control_characters::no &&
            i >= '\a' && i <= '\r') {
          // the special escapes, backslash followed by a, b, t, n, v, f, or r
          data[1] = "abtnvfr"[i - '\a'];
          size = 2;
        } else {
          // hex escapes like backslash-x01
          data[1] = 'x';
          data[2] = hex[i >> 4];
          data[3] = hex[i & 0xf];
          size = 4;
        }
      } else {
        if (i == escape_char || i == quote_char) {
          // escaped control characters: backslash-quote and backslash-backslash
          data[0] = escape_char;
          data[1] = i;
          size = 2;
        } else {
          // un-escaped character
          data[0] = i;
          size = 1;
        }
      }
      sv = std::string_view(data.data(), size);
    }
    return m_table;
  }

  /// Element 'c' is a string_view over character 'c' escaped.
  static inline Table_t m_table;

  /// Element 'c' is a character array containing the string data for
  /// character 'c' escaped.
  static inline std::array<std::array<char, 4>, 256> m_data_table;
};  // class Escaped_format

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_FORMATS_ESCAPED_FORMAT_H
