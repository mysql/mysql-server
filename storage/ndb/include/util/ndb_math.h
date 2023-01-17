/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MATH_H
#define NDB_MATH_H

#include <type_traits>
#include "util/require.h"

/**
 * Greatest common divisor, gcd.
 * Arguments should be positive integers.
 */

template<typename Int>
inline Int gcd(Int x, Int y)
{
  do {
    Int t = y;
    y = x % y;
    x = t;
  } while (y != 0);
  return x;
}

/**
 * Least common multiple, lcm.
 * Arguments should be positive integers.
 * Result may be overflowed.
 */

template<typename Int>
inline Int lcm(Int x, Int y)
{
  return (x / gcd(x, y)) * y;
}

/**
 * Integer division rounding up.
 */

template<typename T>
static constexpr inline T ndb_ceil_div(const T p, const T q)
{
  static_assert(std::is_integral_v<T>,
                "Integral type required for ndb_ceil_div().");
  if (p == 0)
  {
    return 0;
  }
#if defined(__GNUC__) && (__GNUC__ <= 8)
  // Nothing, broken build: calling non-constexpr function require_failed().
#else
  if constexpr (std::is_signed_v<T>)
  {
    // Negative values not supported
    require(p >= 0);
    require(q >= 0);
  }
#endif
  return 1 + (p - 1) / q;
}

#endif
