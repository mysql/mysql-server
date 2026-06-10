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

#ifndef MYSQL_STRCONV_DECODE_DECODE_H
#define MYSQL_STRCONV_DECODE_DECODE_H

/// @file
/// Experimental API header

#include <new>                                   // bad_alloc
#include <string_view>                           // string_view
#include "mysql/strconv/decode/parse_options.h"  // Is_parse_options_nocheck
#include "mysql/strconv/decode/parser.h"         // Parser
#include "mysql/strconv/decode/repeat.h"         // Repeat
#include "mysql/strconv/encode/string_target.h"  // Is_string_target
#include "mysql/strconv/formats/format.h"        // Is_format
#include "mysql/utils/concat.h"                  // concat

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Parse from a string into the given object, according to the parse options.
///
/// @param opt Parse options, which should include the format, and may include
/// a number of repetitions.
///
/// @param in_sv Input represented as `string_view`.
///
/// @param[out] out Reference to object to read into. In case of error, this may
/// be in a "half-parsed" state.
///
/// @return Parser that can be queried for success and error messages.
[[nodiscard]] Parser decode(const Is_parse_options_nocheck auto &opt,
                            const std::string_view &in_sv, auto &out) {
  Parser parser(in_sv);
  auto ret = parser.read(opt, out);
  if (ret == mysql::utils::Return_status::ok && !parser.is_sentinel())
    parser.set_fullmatch_error();
  return parser;
}

/// Test for success when parsing (decoding) a *string* from a string, without
/// producing output.
///
/// If this succeeds, it is guaranteed that a subsequent invocation of
/// `decode` will not produce a parse_error. But note that it is possible
/// for it to fail with an out-of-memory error.
///
/// This is an API wrapper for objects/formats for which `decode_impl(opt,
/// pos, Is_string_target)` has been implemented.
///
/// @param opt Parse options, which should include the format, and may include a
/// number of repetitions.
///
/// @param in_sv Input represented as `string_view`.
///
/// @return Parser that can be queried for success and error messages.
[[nodiscard]] Parser test_decode(const Is_parse_options_nocheck auto &opt,
                                 const std::string_view &in_sv) {
  detail::Constructible_string_counter string_counter;
  return decode(opt, in_sv, string_counter);
}

/// Compute the output length when parsing a *string* from a string.
///
/// If this succeeds, it is guaranteed that a subsequent invocation of
/// `decode` will not produce a parse_error. But note that it is possible
/// for it to fail with an out-of-memory error.
///
/// This is an API wrapper for objects/formats for which `decode_impl(opt,
/// pos, Is_string_target)` has been implemented.
///
/// @param opt Parse options, which should include the format, and may include a
/// number of repetitions.
///
/// @param in_sv Input represented as `string_view`.
///
/// @return The length, or -1 on parse error.
[[nodiscard]] std::ptrdiff_t compute_decoded_length(
    const Is_parse_options_nocheck auto &opt, const std::string_view &in_sv) {
  detail::Constructible_string_counter string_counter;
  Parser ret = decode(opt, in_sv, string_counter);
  if (!ret.is_ok()) return -1;
  return string_counter.size();
}

/// Parse from a string into an Is_out_str object.
///
/// This is an API wrapper for objects/formats for which `decode_impl(opt,
/// pos, Is_string_target)` has been implemented.
///
/// @param opt Parse options, which should include the format, and may include a
/// number of repetitions.
///
/// @param in_sv Input represented as `string_view`.
///
/// @param[out] out_str Output string wrapper to read into. In case of errors,
/// this is unchanged.
///
/// @return Parser that can be queried for success and error messages.
[[nodiscard]] Parser decode(const Is_parse_options_nocheck auto &opt,
                            const std::string_view &in_sv,
                            const Is_out_str auto &out_str) {
  using mysql::utils::Return_status;
  Parser parser(in_sv);
  auto ret = parser.read_to_out_str(opt, out_str);
  if (ret == Return_status::ok && !parser.is_sentinel())
    parser.set_fullmatch_error();
  return parser;
}

// ==== decode(..., std::string): string->string decoding ====

/// Parse from a string into an std::string object.
///
/// This is an API wrapper for objects/formats for which `decode_impl(opt,
/// pos, Is_string_target)` has been implemented.
///
/// @param opt Parse options, which should include the format, and may include
/// a number of repetitions.
///
/// @param in_sv Input represented as `string_view`.
///
/// @param[out] out Reference to string to read into. In case of errors, this is
/// unchanged.
///
/// @return Parser that can be queried for success and error messages.
[[nodiscard]] Parser decode(
    const Is_parse_options_nocheck auto &opt, const std::string_view &in_sv,
    mysql::meta::Is_specialization<std::basic_string> auto &out) {
  return decode(opt, in_sv, out_str_growable(out));
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_DECODE_H
