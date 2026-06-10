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

#ifndef MYSQL_STRCONV_CONV_TEXT_PARSER_H
#define MYSQL_STRCONV_CONV_TEXT_PARSER_H

/// @file
/// Experimental API header

#include <cstddef>                                 // std::size_t
#include <string_view>                             // string_view
#include "mysql/strconv/decode/parser.h"           // Parser
#include "mysql/strconv/encode/string_target.h"    // Is_string_target
#include "mysql/strconv/formats/escaped_format.h"  // Escaped_format
#include "mysql/strconv/formats/text_format.h"     // Text_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// ==== Format Parser into text format ====

/// Enable encode(Text_format, Parse_result). This produces an error message
/// that describes the Parse_result object to humans.
///
/// @param format Type tag to identify that this relates to text format.
///
/// @param target String target to which the string will be appended.
///
/// @param obj Object to write.
void encode_impl(const Text_format &format, Is_string_target auto &target,
                 const Parser &obj) {
  // For errors, we include a quote of up to 48 characters, of which 6 are
  // "[HERE]". Of the remaining ones, 1/3 are the prefix and 2/3 are the suffix.
  static constexpr std::size_t max_len = 48;
  static constexpr std::string_view here("[HERE]");
  static constexpr std::size_t max_prefix_len = (max_len - here.size()) / 3;
  static constexpr std::size_t max_suffix_len =
      max_len - max_prefix_len - here.size();
  static constexpr std::string_view ellipsis("...");
  detail::Parse_result_internals internals(obj);

  // Generate the string
  // "after N characters, up to [HERE] in \"...prefix[HERE]".
  // (Without the ellipsis, in case prefix is short.)
  auto prefix = [&](std::size_t position) {
    // Use the text "after N characters" since it is non-ambiguous. The
    // alternative "at position N" may be ambiguous since some software uses
    // 0-based positions and some uses 1-based positions.
    target.concat(format, " after ", position, " characters, marked by ", here,
                  " in: \"");
    // Prefix
    if (position > max_prefix_len) {
      // Truncate to the left
      std::size_t length = max_prefix_len - ellipsis.size();
      target.write(format, ellipsis);
      target.write(Escaped_format{}, {obj.begin() + position - length, length});
    } else {
      target.write(Escaped_format{}, {obj.begin(), position});
    }
    // "[HERE]"
    target.write(format, here);
  };

  // Generate the string "suffix...\"". (Without the ellipsis, in case the
  // suffix is short.)
  auto suffix = [&](const char *position) {
    std::size_t remaining_size = obj.end() - position;
    if (remaining_size > max_suffix_len) {
      // Truncate to the right
      target.write(Escaped_format{},
                   {position, max_suffix_len - ellipsis.size()});
      target.write(format, ellipsis);
    } else {
      target.write(Escaped_format{}, {position, remaining_size});
    }
    target.write(format, "\"");
  };

  if (obj.is_ok()) {
    target.write(format, "OK");
  } else if (obj.is_store_error()) {
    // There was an error storing the object. Just print the message, no
    // position.
    target.write(format, internals.message());
  } else {
    // There was a parse error. Print the message, the position, and some
    // context from the string.
    // First say what is wrong.
    std::size_t position{0};
    if (obj.is_fullmatch_error() &&
        internals.parse_error_position() < std::ptrdiff_t(obj.int_pos())) {
      target.write(format, "Expected end of string");
      position = obj.int_pos();
    } else {
      if (internals.message_form() ==
          detail::Parse_result_internals::Message_form::expected_string) {
        target.write(format, "Expected ");
        target.write(Escaped_format(With_quotes::yes), internals.message());
      } else {
        target.write(format, internals.message());
      }
      position = internals.parse_error_position();
    }
    // Then say where it is wrong.
    if (position == 0) {
      target.write(format, " at the beginning of the string: \"");
    } else {
      // " after N characters, up to [HERE] in \"...prefix[HERE]"
      prefix(position);
    }
    // "suffix...\""
    suffix(obj.begin() + position);
  }
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_TEXT_PARSER_H
