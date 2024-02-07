/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include <concepts>  // integral
#include <cstdint>   // UINT64_C

namespace stdx {

// implementation 'byteswap()' from std c++23
//
// see:
// - http://wg21.link/P1272

// byteswap() functions translate into
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

template <std::integral T>
constexpr T byteswap(T num) noexcept {
  static_assert(std::has_unique_object_representations_v<decltype(num)>,
                "T may not have padding bits");

  if constexpr (sizeof(T) == 1) {
    return num;
#if defined(__GNUC__)
  } else if constexpr (sizeof(T) == 2) {
    return __builtin_bswap16(num);
  } else if constexpr (sizeof(T) == 4) {
    return __builtin_bswap32(num);
  } else if constexpr (sizeof(T) == 8) {
    return __builtin_bswap64(num);
#endif
  } else if constexpr (sizeof(T) == 2) {
    return (num & UINT16_C(0x00ff)) << (1 * 8) |
           (num & UINT16_C(0xff00)) >> (1 * 8);
  } else if constexpr (sizeof(T) == 4) {
    return (num & UINT32_C(0x0000'00ff)) << (3 * 8) |
           (num & UINT32_C(0x0000'ff00)) << (1 * 8) |
           (num & UINT32_C(0x00ff'0000)) >> (1 * 8) |
           (num & UINT32_C(0xff00'0000)) >> (3 * 8);
  } else if constexpr (sizeof(T) == 8) {
    return (num & UINT64_C(0x0000'0000'0000'00ff)) << (7 * 8) |
           (num & UINT64_C(0x0000'0000'0000'ff00)) << (5 * 8) |
           (num & UINT64_C(0x0000'0000'00ff'0000)) << (3 * 8) |
           (num & UINT64_C(0x0000'0000'ff00'0000)) << (1 * 8) |
           (num & UINT64_C(0x0000'00ff'0000'0000)) >> (1 * 8) |
           (num & UINT64_C(0x0000'ff00'0000'0000)) >> (3 * 8) |
           (num & UINT64_C(0x00ff'0000'0000'0000)) >> (5 * 8) |
           (num & UINT64_C(0xff00'0000'0000'0000)) >> (7 * 8);
  } else {
    static_assert(sizeof(num) == 0,
                  "byteswap not implemented for integral types of this size");
  }
}

}  // namespace stdx

#endif
