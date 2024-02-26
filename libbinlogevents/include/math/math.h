/* Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/// @addtogroup Replication
/// @{
///  @file math.h

#ifndef MYSQL_MATH_MATH_H_
#define MYSQL_MATH_MATH_H_

#include <type_traits>  // std::enable_if

namespace mysqlns::math {

/// Return x+y, limited to the given maximum.
///
/// @note This works even when x+y would exceed the maximum for the
/// datatype.
///
/// @tparam T Data type. Must be an unsigned integral datatype.
///
/// @param x The first term.
///
/// @param y The second term.
///
/// @param maximum The maximum allowed value.
///
/// @return The smallest of (x + y) and (maximum), computed as if
/// using infinite precision arithmetic.
template <typename T, std::enable_if_t<std::is_integral<T>::value &&
                                           std::is_unsigned<T>::value,
                                       bool> = true>
constexpr T add_bounded(const T x, const T y, const T maximum) {
  if (y >= maximum || maximum - y < x) return maximum;
  return x + y;
}

/// Return x*y, limited to the given maximum.
///
/// @note This works even when x * y would exceed the maximum for any of the
/// data type.
///
/// @tparam T Data type for the first factor, the maximum, and the
/// result.  This must be an unsigned integral.
///
/// @tparam T2 datatype for the second factor.  This can be any
/// arithmetic type, including floating point types.
///
/// @param x The first factor.
///
/// @param y The second factor.
///
/// @param maximum The maximum allowed value.
///
/// @return The smallest of (x + y) and (maximum), computed as if
/// using infinite precision arithmetic; or 0 if y is negative.
template <
    typename T, typename T2,
    std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value &&
                         std::is_arithmetic<T2>::value,
                     bool> = true>
constexpr T multiply_bounded(const T x, const T2 y, const T maximum) {
  if (y <= 0) return 0;
  if (y > 1 && static_cast<T>(maximum / y) < x) return maximum;
  return static_cast<T>(x * y);
}

/// Return ceil(x / y), where x and y are unsigned integer types
template <typename T, std::enable_if_t<std::is_integral<T>::value &&
                                           std::is_unsigned<T>::value,
                                       bool> = true>
constexpr T ceil_div(const T x, const T y) {
  return (x + y - 1) / y;
}

}  // namespace mysqlns::math

/// @} (end of group Replication)

#endif  // MYSQL_MATH_MATH_H_
