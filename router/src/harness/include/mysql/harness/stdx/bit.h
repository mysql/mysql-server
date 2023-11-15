/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_STDX_BIT_H_
#define MYSQL_HARNESS_STDX_BIT_H_

#include <climits>      // CHAR_BIT
#include <cstdint>      // uint64_t
#include <limits>       // numeric_limits
#include <type_traits>  // enable_if, is_unsigned

namespace stdx {

// implementation 'byteswap()' and 'bitops' from std c++20
//
// see:
// - http://wg21.link/P1272
// - http://wg21.link/P0553

// bswap() functions translate into
//
// - `bswap` on x86 with
//   - clang -O1 since 3.5
//   - gcc -O2 since 5.1
//   - msvc /O1 for all but 8-byte. _byteswap_uint64() exists, but isn't
//     constexpr
// - `rev` on armv6 and later with -O1
//   - clang
//   - gcc
//   - msvc /O1 for 4 byte

namespace impl {
// two implementations are provided:
//
// 1. std::enable_if_t<> preselects valid int-sizes for impl::bswap() which
//    selects impl::bswap_N() with an 'if'
// 2. impl::bswap() is automatically selected with std::if_enable_t<>
//
// implementation for
//
// - gcc [
//   2 byte: 1x rol
//   4 byte: 1x bswap
//   8 byte: 1x bswap
//   ]
// - clang [
//   2 byte: 1x rol
//   4 byte: 1x bswap
//   8 byte: 1x bswap
//   ]
// - msvc [
//   2 byte: 1x rol
//   4 byte: 1x bswap
//   8 byte: lots of shifts, or and ands
//   ]
//
// the GCC/Clang variant can use __builtin_bswap*() which translates to
// the right asm instructions on all platforms and optimization levels.
//
// The fallback variant with shift-or only gets translated to BSWAP
// on higher optimization levels.
//
// MSVC provides _byteswap_uint64() and friends as instrincts, but they aren't
// marked as constexpr.
//
template <class T>
constexpr std::enable_if_t<sizeof(T) == 1, T> bswap(T t) noexcept {
  return t;
}

template <class T>
constexpr std::enable_if_t<sizeof(T) == 2, T> bswap(T t) noexcept {
#if defined(__GNUC__)
  return __builtin_bswap16(t);
#else
  return (t & UINT16_C(0x00ff)) << (1 * 8) | (t & UINT16_C(0xff00)) >> (1 * 8);
#endif
}

// for all types that are 4 byte long
//
// unsigned long and unsigned int are both 4 byte on windows, but different
// types
template <class T>
constexpr std::enable_if_t<sizeof(T) == 4, T> bswap(T t) noexcept {
#if defined(__GNUC__)
  return __builtin_bswap32(t);
#else
  return (t & UINT32_C(0x0000'00ff)) << (3 * 8) |
         (t & UINT32_C(0x0000'ff00)) << (1 * 8) |
         (t & UINT32_C(0x00ff'0000)) >> (1 * 8) |
         (t & UINT32_C(0xff00'0000)) >> (3 * 8);
#endif
}

// for all types that are 8 byte long
//
// unsigned long and unsigned long long are both 8 byte on unixes, but different
// types
template <class T>
constexpr std::enable_if_t<sizeof(T) == 8, T> bswap(T t) noexcept {
#if defined(__GNUC__)
  return __builtin_bswap64(t);
#else
  return (t & UINT64_C(0x0000'0000'0000'00ff)) << (7 * 8) |
         (t & UINT64_C(0x0000'0000'0000'ff00)) << (5 * 8) |
         (t & UINT64_C(0x0000'0000'00ff'0000)) << (3 * 8) |
         (t & UINT64_C(0x0000'0000'ff00'0000)) << (1 * 8) |
         (t & UINT64_C(0x0000'00ff'0000'0000)) >> (1 * 8) |
         (t & UINT64_C(0x0000'ff00'0000'0000)) >> (3 * 8) |
         (t & UINT64_C(0x00ff'0000'0000'0000)) >> (5 * 8) |
         (t & UINT64_C(0xff00'0000'0000'0000)) >> (7 * 8);
#endif
}

}  // namespace impl

template <class IntegerType>
std::enable_if_t<std::is_integral<IntegerType>::value,
                 IntegerType> constexpr byteswap(IntegerType t) noexcept {
  return impl::bswap(static_cast<std::make_unsigned_t<IntegerType>>(t));
}

}  // namespace stdx
#endif
