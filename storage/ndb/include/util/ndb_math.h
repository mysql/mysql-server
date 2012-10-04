/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MATH_H
#define NDB_MATH_H

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

#endif
