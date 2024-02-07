// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SERIALIZATION_PRIMITIVE_TYPE_CODEC_H
#define MYSQL_SERIALIZATION_PRIMITIVE_TYPE_CODEC_H

/// @file
/// Experimental API header

#include "mysql/serialization/serialization_types.h"

#include "my_byteorder.h"

#include "mysql/serialization/byte_order_helpers.h"
#include "mysql/serialization/variable_length_integers.h"

#include <bitset>
#include <iostream>
#include <limits>

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

template <class Type>
struct Primitive_type_codec;

template <Field_size field_size>
struct Byte_count_helper;

/// @brief Specialization of Byte_count_helper for 0 field_size. 0 means
/// default. Default choice for integer is variable-length encoding (1-9 bytes).
/// Default choice for a string is unlimited size (un-bounded string)
template <>
struct Byte_count_helper<0> {
  /// @brief Calculates bytes needed to encode the field, enabled for integer
  /// @tparam Type of the field, as represented in the code
  /// @param arg Variable we need to encode
  /// @return Bytes needed to encode the "arg" field
  template <class Type, typename Enabler = std::enable_if_t<
                            std::is_integral<std::decay_t<Type>>::value>>
  static size_t count_write_bytes(const Type &arg) {
    return detail::get_size_integer_varlen<Type>(arg);
  }

  /// @brief Calculates bytes needed to encode the field, enabled for
  /// non-integer types (float/double)
  /// @tparam Type of the field, as represented in the code
  /// @return Bytes needed to encode the "arg" field
  template <class Type, typename Enabler = std::enable_if_t<
                            std::is_floating_point<std::decay_t<Type>>::value>>
  static size_t count_write_bytes(const Type &, int = 0) {
    return sizeof(Type);
  }

  /// @brief Calculates bytes needed to encode the unbounded string
  /// @param arg String argument needed to be encoded
  /// @return Bytes needed to encode the "arg" field
  static size_t count_write_bytes(const std::string &arg) {
    return Byte_count_helper<0>::count_write_bytes(arg.length()) + arg.length();
  }
};

/// @brief Structure that contains methods to count the number of bytes needed
/// to encode a specific field
/// @tparam field_size The number of bytes requested by the user. 0 means
/// "default". For string, field_size is the boundary length of the string
template <Field_size field_size>
struct Byte_count_helper {
  /// Calculates bytes needed to encode the field
  /// @tparam Type of the field, as represented in the code
  /// @return Bytes needed to encode the field
  template <class Type, typename Enabler = std::enable_if_t<
                            std::is_integral<std::decay_t<Type>>::value>>
  static size_t count_write_bytes(const Type &) {
    return field_size;
  }

  /// Calculates bytes needed to encode the bounded string field (specialization
  /// of count_write_bytes for string type)
  /// @param arg String argument needed to be encoded
  /// @return Bytes needed to encode the arg
  static size_t count_write_bytes(const std::string &arg) {
    return Byte_count_helper<0>::count_write_bytes(arg.length()) + arg.length();
  }
};

/// @brief This class is to provide functionality to encode/decode the primitive
/// types into/out of defined stream, without boundary check
/// @tparam Type Type of the field being read / written
template <class Type>
struct Primitive_type_codec {
  /// @brief Writes field_size bytes stored in data into stream
  /// @tparam field_size Number of bytes stored into the stream
  /// @param[in,out] stream Output stream
  /// @param[in] data Value to write
  /// @return Number of bytes written
  template <Field_size field_size>
  static size_t write_bytes(unsigned char *stream, const Type &data);

  /// @brief Reads field_size bytes stored in the stream into data
  /// @tparam field_size Number of bytes stored in the stream
  /// @param[in] stream Input stream
  /// @param[in] stream_bytes Number of bytes in the stream
  /// @param[in,out] data Output value
  /// @return Number of bytes read or 0 on error
  template <Field_size field_size>
  static size_t read_bytes(const unsigned char *stream,
                           std::size_t stream_bytes, Type &data);

  /// @brief Returns number of bytes required to hold information
  /// Returns field_size or size of bytes required to write variable length data
  /// @tparam field_size Number of bytes stored in the stream
  /// @param[in] data Count bytes for this variable
  /// @return Number of bytes that will be read
  template <Field_size field_size>
  static size_t count_write_bytes(const Type &data) {
    return Byte_count_helper<field_size>::count_write_bytes(data);
  }
};

template <>
template <Field_size field_size>
size_t Primitive_type_codec<std::string>::write_bytes(unsigned char *stream,
                                                      const std::string &data) {
  if (data.length() > field_size && field_size != 0) {
    return 0;
  }
  std::size_t bytes_written = detail::write_varlen_bytes(stream, data.length());
  if (data.length() == 0) {
    return bytes_written;
  }
  memcpy(stream + bytes_written, data.c_str(), data.length());
  return data.length() + bytes_written;
}

template <>
template <Field_size field_size>
size_t Primitive_type_codec<std::string>::read_bytes(
    const unsigned char *stream, std::size_t stream_bytes, std::string &data) {
  uint64_t string_length = 0;
  std::size_t bytes_read =
      detail::read_varlen_bytes(stream, stream_bytes, string_length);
  if (bytes_read == 0 || (string_length > field_size && field_size != 0) ||
      stream_bytes < bytes_read + string_length) {
    return 0;
  }
  try {
    data.resize(string_length);
  } catch (std::bad_alloc &) {
    return 0;
  }
  memcpy(data.data(), stream + bytes_read, data.length());
  return data.length() + bytes_read;
}

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_PRIMITIVE_TYPE_CODEC_H
