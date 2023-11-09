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

/// @defgroup GroupLibsMysqlUtils MySQL Libraries : Utilities
/// @ingroup GroupLibsMysql

#ifndef MYSQL_UTILS_BITOPS_BIT_OPERATIONS_H
#define MYSQL_UTILS_BITOPS_BIT_OPERATIONS_H

/// @file
/// Experimental API header
/// This file uses de Bruijn sequences to search for the first / last
/// set or unset bit in the number
/// https://en.wikipedia.org/wiki/De_Bruijn_sequence

#include <cstdint>

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils::bitops {

/// @brief Calculate trailing zeros (32 bit input)
/// @details Return the number of least-significant zeros before the first one,
/// in the binary representation of the number, or 0 if the number is 0.
/// Examples:
/// 0 -> 0
/// 1 -> 0
/// any odd number shifted left by N -> N
/// @details In case this function is fed with 0, it will return 0
/// @param value Tested value
/// @return Number of trailing zero bits in the value
inline int countr_zero(uint32_t value) {
  if (value == 0) {
    return 0;
  }
#if (defined(__clang__) || defined(__GNUC__)) && \
    not defined(CALCULATE_BITS_CUSTOM_IMPL)
  return __builtin_ctz(value);
#else
  static int32_t bit_count_lookup[64] = {
      32, -1, 2,  -1, 3,  -1, -1, -1, -1, 4,  -1, 17, 13, -1, -1, 7,
      0,  -1, -1, 5,  -1, -1, 27, 18, 29, 14, 24, -1, -1, 20, 8,  -1,
      31, 1,  -1, -1, -1, 16, 12, 6,  -1, -1, -1, 26, 28, 23, 19, -1,
      30, -1, 15, 11, -1, 25, 22, -1, -1, 10, -1, 21, 9,  -1, -1, -1};
  value &= (~value + 1);
  value *= 0x4279976b;
  return bit_count_lookup[value >> 26];
#endif
}

/// @brief Calculate trailing zeros (64 bit input)
/// @details In case this function is fed with 0, it will return 0
/// @param value Tested value
/// @return Number of trailing zero bits in the value
inline int countr_zero(uint64_t value) {
  if (value == 0) {
    return 0;
  }
#if defined(__clang__) || \
    defined(__GNUC__) && not defined(CALCULATE_BITS_CUSTOM_IMPL)
  return __builtin_ctzll(value);
#else
  uint32_t value_32 = value;
  if (value_32 == 0) {
    return 32 + countr_zero(static_cast<uint32_t>(value >> 32));
  }
  return countr_zero(value_32);
#endif
}

/// @brief Calculate trailing ones (64 bit input)
/// @param value Tested value
/// @return Number of trailing "one" bits in the value
inline int countr_one(uint32_t value) { return countr_zero(~value); }

/// @brief Calculate trailing ones (64 bit input)
/// @param value Tested value
/// @return Number of trailing "one" bits in the value
inline int countr_one(uint64_t value) { return countr_zero(~value); }

/// @brief Calculate leading zeros (32 bit input)
/// @details In case this function is fed with 0, it will return 31
/// @param value Tested value
/// @return Number of trailing zero bits in the value
inline int countl_zero(uint32_t value) {
  if (value == 0) {
    return sizeof(uint32_t) * 8 - 1;
  }
#if defined(__clang__) || \
    defined(__GNUC__) && not defined(CALCULATE_BITS_CUSTOM_IMPL)
  return __builtin_clz(value);
#else
  static const char de_bruijn_seq[32] = {
      0, 31, 9, 30, 3, 8,  13, 29, 2,  5,  7,  21, 12, 24, 28, 19,
      1, 10, 4, 14, 6, 22, 25, 20, 11, 15, 23, 26, 16, 27, 17, 18};
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value++;
  return de_bruijn_seq[value * 0x076be629 >> 27];
#endif
}

/// @brief Calculate leading zeros (64 bit input)
/// @details In case this function is fed with 0, it will return 63
/// @param value Tested value
/// @return Number of trailing zero bits in the value
inline int countl_zero(uint64_t value) {
  if (value == 0) {
    return sizeof(uint64_t) * 8 - 1;
  }
#if defined(__clang__) || \
    defined(__GNUC__) && not defined(CALCULATE_BITS_CUSTOM_IMPL)
  return __builtin_clzll(value);
#else
  uint32_t value_hi_32 = value >> 32;
  if (value_hi_32 == 0) {
    return 32 + countl_zero(static_cast<uint32_t>(value));
  }
  return countl_zero(value_hi_32);
#endif
}

/// @brief Calculates bit width of the value
/// @param value Value under consideration
/// @return Bit width of the "value" parameter
inline int bit_width(uint64_t value) {
  return sizeof(uint64_t) * 8 - countl_zero(value);
}

}  // namespace mysql::utils::bitops

/// @}

#endif  // MYSQL_UTILS_BITOPS_BIT_OPERATIONS_H
