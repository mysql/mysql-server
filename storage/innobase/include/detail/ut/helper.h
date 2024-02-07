/*****************************************************************************

Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/detail/ut/helper.h
    Small helper functions. Exists mostly because ut0ut.h is not usable from
    every translation unit.
 */

#ifndef detail_ut_helper_h
#define detail_ut_helper_h

namespace ut {
namespace detail {

/** Calculates the smallest multiple of m that is not smaller than n
    when m is a power of two. In other words, rounds n up to m * k.
    @param n in: number to round up
    @param m in: alignment, must be a power of two
    @return n rounded up to the smallest possible integer multiple of m
 */
constexpr size_t calc_align(size_t n, size_t m) {
  // This is a copy pasta from ut0ut.h but consuming that header from
  // this file bursts the build into flames. Let's at least stick to the
  // similar name.
  return (n + (m - 1)) & ~(m - 1);
}

/** Calculates the biggest multiple of m that is not bigger than n
    when m is a power of two.  In other words, rounds n down to m * k.
    @param n in: number to round down
    @param m in: alignment, must be a power of two
    @return n rounded down to the biggest possible integer multiple of m */
constexpr size_t pow2_round(size_t n, size_t m) {
  // This is a copy pasta from ut0ut.h but consuming that header from
  // this file bursts the build into flames. Let's at least stick to the
  // similar name.
  return (n & ~(m - 1));
}

/** Calculates the next multiple of m that is bigger or equal to n.
    @param n in: number to find the next multiple of in terms of m
    @param m in: alignment, must be a power of two
    @return next next multiple of m bigger or equal than n */
constexpr size_t round_to_next_multiple(size_t n, size_t m) {
  return pow2_round(n + (m - 1), m);
}

}  // namespace detail
}  // namespace ut

#endif
