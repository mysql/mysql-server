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

#ifndef MYSQL_STRCONV_ENCODE_CONCAT_H
#define MYSQL_STRCONV_ENCODE_CONCAT_H

/// @file
/// Experimental API header

#include "mysql/strconv/encode/concat_object.h"  // Concat_object
#include "mysql/strconv/encode/encode.h"         // encode
#include "mysql/strconv/encode/string_target.h"  // String_target
#include "mysql/strconv/formats/format.h"        // Is_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

template <std::size_t index = 0, class... Args_t>
void encode_impl(const Is_format auto &format, Is_string_target auto &target,
                 const Concat_object<Args_t...> &concat_object) {
  if constexpr (index < sizeof...(Args_t)) {
    target.write(format, std::get<index>(concat_object.m_args));
    encode_impl<index + 1>(format, target, concat_object);
  }
}

/// Compute the length of the concatenation of the string representations of the
/// objects.
///
/// @tparam Format_t Type of format.
///
/// @tparam Args_t Types of objects.
///
/// @param format Format object.
///
/// @param args Objects.
template <Is_format Format_t, class... Args_t>
auto concat_length(const Format_t &format, const Args_t &...args) noexcept {
  return compute_encoded_length(format, Concat_object<Args_t...>(args...));
}

/// Concatenate the string representations of the objects into the output
/// string wrapper.
///
/// @tparam Format_t Type of format.
///
/// @tparam Out_str_t Type of output string wrapper.
///
/// @tparam Args_t Types of objects.
///
/// @param format Format object.
///
/// @param out Output String Wrapper in which the result will be saved.
///
/// @param args Objects.
///
/// @return Return_status::ok on success, Return_status::error on out-of-memory.
template <Is_format Format_t, Is_out_str Out_str_t, class... Args_t>
auto concat(const Format_t &format, const Out_str_t &out,
            const Args_t &...args) noexcept {
  return encode(format, out, Concat_object<Args_t...>(args...));
}

/// Concatenate the string representations of the objects and return a
/// std::optional<std::string> object holding the result.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @tparam Format_t Type of format.
///
/// @tparam Args_t Types of objects.
///
/// @param format Format object.
///
/// @param args Objects.
///
/// @return std::optional holding a String_t object on success, or no value on
/// out-of-memory.
template <class String_t = std::string, Is_format Format_t, class... Args_t>
auto concat(const Format_t &format, const Args_t &...args) noexcept {
  return encode<String_t>(format, Concat_object<Args_t...>(args...));
}

namespace throwing {
/// Concatenate the string representations of the objects and return a
/// std::string object holding the result.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @tparam Format_t Type of format.
///
/// @tparam Args_t Types of objects.
///
/// @param format Format object.
///
/// @param args Objects.
///
/// @return String_t object holding the result.
///
/// @throws bad_alloc if an out-of-memory condition occurred.
template <class String_t = std::string, Is_format Format_t, class... Args_t>
auto concat(const Format_t &format, const Args_t &...args) {
  return mysql::strconv::throwing::encode<String_t>(
      format, Concat_object<Args_t...>(args...));
}
}  // namespace throwing

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_CONCAT_H
