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

#ifndef MYSQL_STRCONV_ENCODE_ENCODE_TEXT_H
#define MYSQL_STRCONV_ENCODE_ENCODE_TEXT_H

/// @file
/// Experimental API header

#include "mysql/strconv/encode/concat.h"        // concat
#include "mysql/strconv/encode/encode.h"        // compute_encoded_length
#include "mysql/strconv/formats/text_format.h"  // Text_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Return the string length of the object, using Text_format.
///
/// @note If you plan to allocate memory for a null-terminated string, remember
/// to add 1 byte for the trailing '\0'.
template <class Object_t>
[[nodiscard]] std::size_t compute_encoded_length_text(const Object_t &object) {
  return compute_encoded_length(Text_format{}, object);
}

/// Write the string representation of the object to the given string output
/// wrapper, using Text_format.
///
/// This overload is for the case that the output wrapper's resize policy is
/// `fixed`, i.e., the caller guarantees that the output buffer has enough space
/// to store the output. Therefore, this function cannot fail and does not have
/// the `[[nodiscard]]` attribute.
///
/// @tparam Out_str_t Type of output string wrapper to write to.
///
/// @tparam Object_t Type of object.
///
/// param[in,out] out_str Output string wrapper to write to.
///
/// param object Object to write.
///
/// @return Return_status::ok on success, Return_status::error on allocation
/// failure.
// Doxygen complains that the parameters are duplicated (maybe mixing up
// overloads with different constraints). We work around that by using param
// instead of @param.
template <Is_out_str_fixed Out_str_t, class Object_t>
auto encode_text(Out_str_t out_str, const Object_t &object) {
  return encode(Text_format{}, out_str, object);
}

/// Write the string representation of the object to the given string output
/// wrapper, using Text_format.
///
/// This overload is for the case that the output wrapper's resize policy is
/// `growable`. Therefore, this function can fail to allocate memory and has the
/// `[[nodiscard]]` attribute.
///
/// @tparam Out_str_t Output string wrapper to write to.
///
/// @tparam Object_t Type of object.
///
/// @param[in,out] out_str Output string wrapper to write to.
///
/// @param object Object to write.
///
/// @return Return_status::ok on success, Return_status::error on allocation
/// failure.
template <Is_out_str_growable Out_str_t, class Object_t>
[[nodiscard]] auto encode_text(Out_str_t out_str, const Object_t &object) {
  return encode(Text_format{}, out_str, object);
}

namespace throwing {

/// Return an std::string object holding the string representation of the
/// object, using Text_format.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @tparam Object_t Type of object.
///
/// @param object Object to write.
///
/// @return String representation of object.
///
/// @throws bad_alloc if an out-of-memory condition occurs.
template <class String_t = std::string, class Object_t>
[[nodiscard]] String_t encode_text(const Object_t &object) {
  return mysql::strconv::throwing::encode<String_t>(Text_format{}, object);
}

/// Return an std::string object holding the concatenated string representations
/// of the objects, using Text_format.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @tparam Objects_t Types of objects.
///
/// @param objects Objects to write.
///
/// @return Concatenated string representations of objects.
///
/// @throws bad_alloc if an out-of-memory condition occurs.
template <class String_t = std::string, class... Objects_t>
[[nodiscard]] String_t concat_text(const Objects_t &...objects) {
  return mysql::strconv::throwing::concat<String_t>(Text_format{}, objects...);
}

}  // namespace throwing

/// Return an std::optional<std::string> object holding the string
/// representation of the object, using Text_format.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @tparam Object_t Type of object.
///
/// @param object Object to write.
///
/// @returns std::optional<std::string> object holding the string representation
/// of @c object, or holding no value if an out-of-memory condition occurred.
template <class String_t = std::string, class Object_t>
[[nodiscard]] auto encode_text(const Object_t &object) {
  return encode<String_t>(Text_format{}, object);
}

/// Return an std::optional<std::string> object holding the concatenated string
/// representations of the objects, using Text_format.
///
/// @tparam String_t Output string type. Defaults to std::string.
///
/// @tparam Objects_t Types of objects.
///
/// @param objects Objects to write.
///
/// @returns std::optional<std::string> object holding the concatenated string
/// representations of @c objects, or holding no value if an out-of-memory
/// condition occurred.
template <class String_t = std::string, class... Objects_t>
[[nodiscard]] auto concat_text(const Objects_t &...objects) {
  return concat<String_t>(Text_format{}, objects...);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_ENCODE_TEXT_H
