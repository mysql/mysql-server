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

#ifndef MYSQL_SERIALIZATION_VARIABLE_LENGTH_INTEGERS_H
#define MYSQL_SERIALIZATION_VARIABLE_LENGTH_INTEGERS_H

/// @file
/// Experimental API header
/// @details This file contains low-level internal functions used to store/load
/// variable-length integers to/from the memory
///
/// Please refer to the readme.md of the mysql_serialization library to find
/// more information about the format

#include <bit>
#include <concepts>
#include <limits>
#include "my_byteorder.h"

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization::detail {

/// @brief Calculates the number of bytes necessary to store data
/// @tparam Type Integer type
/// @param data The number to be stored into the memory
/// @return The number of bytes necessary to store data.
size_t get_size_integer_varlen_unsigned(
    const std::unsigned_integral auto &data) {
  // @details When bit_width(data) == N, the output buffer uses:
  // * 1 byte, if N==0;
  // * 1 + ceil((N-1)/7) bytes, if 1<=N<=63;
  // * 9 bytes, if N==64.
  // For the case 1<=N<=63, the function follows a straight line. It
  // is a little above that line when N==0 and a little below that
  // line when N==63. Therefore, it can be approximated by a line with
  // slightly lower slope.  The slope 575/4096 gives correct results
  // for all values between 0 and 64, inclusive, and can be computed
  // with just 1 multiplication and 1 shift.
  int bits_in_number = std::bit_width(data);
  return ((bits_in_number * 575) >> 12) + 1;
}

/// @copydoc get_size_integer_varlen_unsigned
/// @details Version for signed integers
size_t get_size_integer_varlen_signed(const std::signed_integral auto &data) {
  // sign_mask = (data < 0) ? ~0 : 0
  auto sign_mask = data >> (sizeof(data) * 8 - 1);
  return get_size_integer_varlen_unsigned(uint64_t(data ^ sign_mask) << 1);
}

/// @copydoc get_size_integer_varlen_unsigned
/// @details Enabled for unsigned integers
size_t get_size_integer_varlen(const std::unsigned_integral auto &data) {
  return get_size_integer_varlen_unsigned(data);
}

/// @copydoc get_size_integer_varlen_unsigned
/// @details Enabled for signed integers
size_t get_size_integer_varlen(const std::signed_integral auto &data) {
  return get_size_integer_varlen_signed(data);
}

/// @brief Writes variable-length integer to the stream
/// @param[in] stream Encoded data stream
/// @param[out] data Integer to write
/// @return Number of bytes written to the stream
size_t write_varlen_bytes_unsigned(unsigned char *stream,
                                   const std::unsigned_integral auto &data) {
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
size_t write_varlen_bytes_signed(unsigned char *stream,
                                 const std::signed_integral auto &data) {
  // convert negatives into positive numbers
  // sign_mask is 0 if data >= 0 and ~0 if data < 0
  auto sign_mask = (data >> (sizeof(data) * 8 - 1));
  uint64_t data_cpy = (data ^ sign_mask);
  // insert sign bit as least significant bit
  data_cpy = (data_cpy << 1) | (sign_mask & 1);
  return write_varlen_bytes_unsigned(stream, data_cpy);
}

/// @copydoc write_varlen_bytes_unsigned
/// @details Enabled for unsigned integers
size_t write_varlen_bytes(unsigned char *stream,
                          const std::unsigned_integral auto &data) {
  return write_varlen_bytes_unsigned(stream, data);
}

/// @copydoc write_varlen_bytes_unsigned
/// @details Enabled for signed integers
size_t write_varlen_bytes(unsigned char *stream,
                          const std::signed_integral auto &data) {
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
                                  std::size_t stream_bytes, Type &data)
  requires std::unsigned_integral<Type>
{
  if (stream_bytes == 0) {
    return stream_bytes;
  }
  uint8_t first_byte = stream[0];
  std::size_t num_bytes = std::countr_one(first_byte) + 1;
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
                                std::size_t stream_bytes, Type &data)
  requires std::signed_integral<Type>
{
  using Type_unsigned = std::make_unsigned_t<Type>;
  Type_unsigned data_tmp = 0;
  std::size_t num_bytes =
      read_varlen_bytes_unsigned(stream, stream_bytes, data_tmp);
  // 0 if positive, ~0 if negative
  // static_cast is needed to avoid compilation warning on Windows.
  Type_unsigned sign_mask = -static_cast<Type>(data_tmp & 1);
  // the result if it is nonnegative, or -(result + 1) if it is negative.
  data_tmp = data_tmp >> 1;
  // the result
  data_tmp = data_tmp ^ sign_mask;
  data = Type(data_tmp);
  return num_bytes;
}

/// @copydoc read_varlen_bytes_unsigned
size_t read_varlen_bytes(const unsigned char *stream, std::size_t stream_bytes,
                         std::unsigned_integral auto &data) {
  return read_varlen_bytes_unsigned(stream, stream_bytes, data);
}

/// @copydoc read_varlen_bytes_unsigned
size_t read_varlen_bytes(const unsigned char *stream, std::size_t stream_bytes,
                         std::signed_integral auto &data) {
  return read_varlen_bytes_signed(stream, stream_bytes, data);
}

}  // namespace mysql::serialization::detail

/// @}

#endif  // MYSQL_SERIALIZATION_VARIABLE_LENGTH_INTEGERS_H
