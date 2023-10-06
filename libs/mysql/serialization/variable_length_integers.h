// Copyright (c) 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SERIALIZATION_VARIABLE_LENGTH_INTEGERS_H
#define MYSQL_SERIALIZATION_VARIABLE_LENGTH_INTEGERS_H

/// @file
/// Experimental API header
/// @details This file contains low-level internal functions used to store/load
/// variable-length integers to/from the memory
///
/// Please refer to the readme.md of the mysql_serialization library to find
/// more information about the format

#include "my_byteorder.h"
#include "mysql/utils/bit_operations.h"

#include <limits.h>
#include <bitset>
#include <iostream>
#include <limits>
#include <type_traits>

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization::detail {

/// @brief Calculates a number of bytes necessary to store data
/// @tparam Type Integer type
/// @param data A Number to be stored into the memory
/// @details When bits_in_number == N, the output buffer uses N bits, plus
/// a prefix of ceil(N / 7) bits; except in the case N>56, where we save one
/// bit and use an 8-bit prefix only. And the number of bytes required for
/// M bits is ceil(M / 8). In integer arithmetic,
/// ceil(X / Y) == (X + Y - 1) / Y, and X / 2^K == X >> K.
/// So the following algorithm would work:
///   if (bits_in_number > 56) return 9;
///   bits_needed = bits_in_number + (bits_in_number + 6) / 7;
///   return (bits_needed + 7) >> 3;
/// The code here is an optimization: by altering the numbers to
///   bits_needed = bits_in_number + (bits_in_number + 7) / 8;
/// the intermediate value changes, but the end result does not change, except
/// in the case where bits_in_number > 56, where this formula gives the correct
/// number. This is faster because we remove the need for a branch
/// (the `if` statement), and we can use >> instead of /.
/// @return A number of bytes necessary to store data
template <class Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == false>>
size_t get_size_integer_varlen_unsigned(const Type &data) {
  int bits_in_number = utils::bitops::bit_width(static_cast<uint64_t>(data));
  int bits_needed = bits_in_number + ((bits_in_number + 7) >> 3);
  return (bits_needed + 7) >> 3;
}

/// @copydoc get_size_integer_varlen_unsigned
/// @details Version for signed integers
template <class Type, typename = std::enable_if_t<std::is_signed_v<Type>>>
size_t get_size_integer_varlen_signed(const Type &data) {
  uint64_t data_cpy = data;
  Type sign_mask = Type(data_cpy) >> (sizeof(data) * 8 - 1);
  return get_size_integer_varlen_unsigned((data_cpy ^ sign_mask) << 1);
}

/// @copydoc get_size_integer_varlen_unsigned
/// @details Enabled for unsigned integers
template <typename Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == false>>
size_t get_size_integer_varlen(const Type &data) {
  return get_size_integer_varlen_unsigned(data);
}

/// @copydoc get_size_integer_varlen_unsigned
/// @details Enabled for signed integers
template <typename Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == true>>
size_t get_size_integer_varlen(const Type &data, int = 0) {
  return get_size_integer_varlen_signed(data);
}

/// @brief Writes variable-length integer to the stream
/// @param[in] stream Encoded data stream
/// @param[out] data Integer to write
/// @return Number of bytes written to the stream
template <typename Type>
size_t write_varlen_bytes_unsigned(unsigned char *stream, const Type &data) {
  uint64_t data_cpy = data;
  int byte_count = get_size_integer_varlen_unsigned(data);
  stream[0] = ((1 << (byte_count - 1)) - 1) |
              static_cast<uint8_t>(data_cpy << byte_count);
  // memcpy won't accept 0 bytes
  if (byte_count == 1) {
    return byte_count;
  }
  // If byte_count <= 8, shift right by 8 - byte_count.
  // If byte_count == 9, shift right by 8 - 9 + 1 = 0.
  data_cpy >>= (8 - byte_count + ((byte_count + 7) >> 4));
  // reverse endianess for BIG ENDIAN archs
  data_cpy = htole64(data_cpy);
  memcpy(&stream[1], &data_cpy, byte_count - 1);
  return byte_count;
}

/// @copydoc write_varlen_bytes_unsigned
/// @details Version for signed integers
template <typename Type>
size_t write_varlen_bytes_signed(unsigned char *stream, const Type &data) {
  // convert negatives into positive numbers
  // sign_mask is 0 if data >= 0 and ~0 if data < 0
  Type sign_mask = (data >> (sizeof(Type) * 8 - 1));
  uint64_t data_cpy = (data ^ sign_mask);
  // insert sign bit as least significant bit
  data_cpy = (data_cpy << 1) | (sign_mask & 1);
  return write_varlen_bytes_unsigned(stream, data_cpy);
}

/// @copydoc write_varlen_bytes_unsigned
/// @details Enabled for unsigned integers
template <typename Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == false>>
size_t write_varlen_bytes(unsigned char *stream, const Type &data) {
  return write_varlen_bytes_unsigned(stream, data);
}

/// @copydoc write_varlen_bytes_unsigned
/// @details Enabled for signed integers
template <typename Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == true>>
size_t write_varlen_bytes(unsigned char *stream, const Type &data, int = 0) {
  return write_varlen_bytes_signed(stream, data);
}

/// @brief Reads variable-length integer from the stream
/// @param[in] stream Encoded data
/// @param[in] stream_bytes Number of bytes in the stream
/// @param[out] data Result value
/// @return Number of bytes read from the stream or 0 on error. Error occurs
/// if the stream ends before or in the middle of the encoded numbers.
template <typename Type>
size_t read_varlen_bytes_unsigned(const unsigned char *stream,
                                  std::size_t stream_bytes, Type &data) {
  if (stream_bytes == 0) {
    return stream_bytes;
  }
  uint8_t first_byte = stream[0];
  std::size_t num_bytes = utils::bitops::countr_one(uint32_t(first_byte)) + 1;
  if (num_bytes > stream_bytes) {
    return 0;
  }
  Type data_cpy = first_byte >> num_bytes;
  if (num_bytes == 1) {
    data = data_cpy;
    return num_bytes;
  }
  uint64_t data_tmp = 0;
  memcpy(&data_tmp, &stream[1], num_bytes - 1);
  data_tmp = le64toh(data_tmp);
  // If num_bytes <= 8, shift left by 8 - num_bytes.
  // If num_bytes == 9, shift left by 8 - 9 + 1 = 0.
  data_tmp <<= (8 - num_bytes + ((num_bytes + 7) >> 4));
  if (data_tmp > std::numeric_limits<Type>::max()) {
    return 0;
  }
  data_cpy |= data_tmp;
  data = data_cpy;
  return num_bytes;
}

/// @copydoc read_varlen_bytes_unsigned
template <typename Type>
size_t read_varlen_bytes_signed(const unsigned char *stream,
                                std::size_t stream_bytes, Type &data) {
  using Type_unsigned = std::make_unsigned_t<Type>;
  Type_unsigned data_tmp = 0;
  std::size_t num_bytes =
      read_varlen_bytes_unsigned(stream, stream_bytes, data_tmp);
  // 0 if positive, ~0 if negative
  Type_unsigned sign_mask = -static_cast<Type>(data_tmp & 1);
  // the result if it is nonnegative, or -(result + 1) if it is negative.
  data_tmp = data_tmp >> 1;
  // the result
  data_tmp = data_tmp ^ sign_mask;
  data = Type(data_tmp);
  return num_bytes;
}

/// @copydoc read_varlen_bytes_unsigned
template <typename Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == false>>
size_t read_varlen_bytes(const unsigned char *stream, std::size_t stream_bytes,
                         Type &data) {
  return read_varlen_bytes_unsigned(stream, stream_bytes, data);
}

/// @copydoc read_varlen_bytes_unsigned
template <typename Type,
          typename = std::enable_if_t<std::is_signed_v<Type> == true>>
size_t read_varlen_bytes(const unsigned char *stream, std::size_t stream_bytes,
                         Type &data, int = 0) {
  return read_varlen_bytes_signed(stream, stream_bytes, data);
}

}  // namespace mysql::serialization::detail

/// @}

#endif  // MYSQL_SERIALIZATION_VARIABLE_LENGTH_INTEGERS_H
