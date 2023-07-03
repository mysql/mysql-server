/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

// [bitops.ret]
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, T> rotl(T x,
                                                               int s) noexcept {
  auto N = std::numeric_limits<T>::digits;
  auto r = s % N;

  if (0 == r) return x;

  return r > 0 ? ((x << r) | x >> (N - r)) : rotr(x, -r);
}

template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, T> rotr(T x,
                                                               int s) noexcept {
  auto N = std::numeric_limits<T>::digits;
  auto r = s % N;

  if (0 == r) return x;

  return r > 0 ? ((x >> r) | x << (N - r)) : rotl(x, -r);
}

namespace impl {
template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int>
countl_zero_linear(T x) noexcept {
  // O(N) N = digits
  //
  auto N = std::numeric_limits<T>::digits;

  if (x == 0) return N;

  int r{};
  for (; x != 0; ++r) {
    x >>= 1;
  }

  return N - r;
}

template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int>
countl_zero_logarithmic(T x) noexcept {
  // O(log(N)) N = digits
  //
  // x             = 0b0000'0100
  // mask[0]       = 0b1111'1111
  // shiftr[0]     = 1 * 4
  // r             = 0
  //
  // -- for-loop 1st round
  // mask[1]       = 0b1111'0000
  // x[1]          = 0b0100'0000
  // r[1]          = 4
  // shiftr        = 2
  // -- for-loop 2nd round
  // mask[2]       = 0b1100'0000
  // x[2]          = 0b0100'0000
  // r[2]          = 4
  // shiftr        = 1
  // -- for-loop 3rd round
  // mask[3]       = 0b1000'0000
  // x[3]          = 0b1000'0000
  // r[3]          = 5
  // shiftr        = 0
  auto N = std::numeric_limits<T>::digits;

  if (x == 0) return N;

  int r{};
  T mask = ~static_cast<T>(0);  // all bits
  int shiftr = sizeof(T) * 4;   // (sizeof(T) * 8) / 2

  for (; shiftr; shiftr >>= 1) {
    mask = mask << shiftr;
    if ((x & mask) == 0) {
      x <<= shiftr;
      r += shiftr;
    }
  }

  return r;
}

template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int>
countl_zero_builtin(T x) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  // __builtin_clz is undefined for x == 0
  if (x == 0) return std::numeric_limits<T>::digits;

  // pick the right builtin for the type
  if (sizeof(T) == sizeof(unsigned long long)) {
    return __builtin_clzll(x);
  } else if (sizeof(T) == sizeof(unsigned long)) {
    return __builtin_clzl(x);
  } else if (sizeof(T) == sizeof(unsigned int)) {
    return __builtin_clz(x);
  }
  // fallthrough for uint8_t and uint16_t
#endif

  return impl::countl_zero_logarithmic(x);
}

template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int>
countr_zero_linear(T x) noexcept {
  // O(N) N = digits
  //
  // x[0] = 0b0001'1000
  // x[1] = 0b0011'0000
  // x[2] = 0b0110'0000
  // x[3] = 0b1100'0000
  // x[4] = 0b1000'0000
  // x[5] = 0b0000'0000 -> 8 - 5 = 3
  //
  auto N = std::numeric_limits<T>::digits;

  if (x == 0) return N;

  int r{};
  for (; x != 0; ++r) {
    x <<= 1;
  }

  return N - r;
}

template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int>
countr_zero_logarithmic(T x) noexcept {
  auto N = std::numeric_limits<T>::digits;

  // simple case
  if (x == 0) return N;

  // O(log(N))
  //
  // x             = 0b0010'0000
  // mask[0]       = 0b1111'1111
  // shiftr[0]     = 1 * 4
  // r             = 0
  //
  // -- for-loop 1st round
  // mask[1]       = 0b0000'1111
  // x[1]          = 0b0000'0010
  // r[1]          = 4
  // shiftr        = 2
  // -- for-loop 2nd round
  // mask[2]       = 0b0000'0011
  // x[2]          = 0b0000'0010
  // r[2]          = 4
  // shiftr        = 1
  // -- for-loop 3rd round
  // mask[3]       = 0b0000'0001
  // x[3]          = 0b0000'0001
  // r[3]          = 5
  // shiftr        = 0
  T mask = ~static_cast<T>(0);  // all bits
  int shiftr = sizeof(T) * 4;   // (sizeof(T) * 8) / 2

  int r{};
  for (; shiftr; shiftr >>= 1) {
    mask = mask >> shiftr;
    if ((x & mask) == 0) {
      x >>= shiftr;
      r += shiftr;
    }
  }

  return r;
}

template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int>
countr_zero_builtin(T x) noexcept {
  auto N = std::numeric_limits<T>::digits;

  // simple case, and __builtin_ctz() is undefined for x == 0
  if (x == 0) return N;
#if defined(__GNUC__) || defined(__clang__)
  if (sizeof(T) == sizeof(unsigned long long)) {
    return __builtin_ctzll(x);
  } else if (sizeof(T) == sizeof(unsigned long)) {
    return __builtin_ctzl(x);
  } else if (sizeof(T) == sizeof(unsigned int)) {
    return __builtin_ctz(x);
  }
#endif

  return impl::countr_zero_logarithmic(x);
}

#if 0
// only for reference.

/**
 * popcount.
 *
 * O(N), naive version.
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> popcount_linear(
    T v) noexcept {
  int cnt{};
  for (; v; v >>= 1) {
    cnt += v & 1;
  }
  return cnt;
}

/**
 * popcount.
 *
 * O(N), K&R version
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> popcount_linear_kr(
    T v) noexcept {
  // K&R version of popcount
  int cnt{};
  for (; v; ++cnt) {
    v &= v - 1;
  }
  return cnt;
}
#endif

/**
 * popcount.
 *
 * O(1) version, as fast as builtin popcount
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> popcount_constant(
    T v) noexcept {
  // non-looping, parallel version
  //
  // see: https://www.chessprogramming.org/Population_Count#SWAR-Popcount
  // see: Knuth Art of Computer Programming: Vol 4, Fascicle 1: Bitwise tricks
  static_assert(sizeof(T) <= 16,
                "implementation of popcount works for up to 128 bit only");

  // p-adic numbers
  const T bit_pattern_all = ~(static_cast<T>(0));
  const T bit_pattern_5555 = bit_pattern_all / 3;
  const T bit_pattern_3333 = bit_pattern_all / 0x0f * 3;
  const T bit_pattern_0f0f = bit_pattern_all / 0xff * 0x0f;
  const T bit_pattern_0101 = bit_pattern_all / 0xff;

  v = v - ((v >> 1) & bit_pattern_5555);
  v = (v & bit_pattern_3333) + ((v >> 2) & bit_pattern_3333);
  v = (v + (v >> 4)) & bit_pattern_0f0f;
  return static_cast<T>(v * bit_pattern_0101) >> (sizeof(T) - 1) * CHAR_BIT;
}

/*
 * Other links for faster popcounts:
 *
 * - http://0x80.pl/articles/index.html#population-count
 * - https://news.ycombinator.com/item?id=11277891
 */

/**
 * popcount.
 *
 * uses builtin's if available, falls back to popcount_constant() if not
 * available.
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> popcount_builtin(
    T v) noexcept {
#if defined(__clang__) || defined(__GNUC__)
  if (sizeof(T) == sizeof(unsigned long long)) {
    return __builtin_popcountll(v);
  } else if (sizeof(T) == sizeof(unsigned long)) {
    return __builtin_popcountl(v);
  } else {
    return __builtin_popcount(v);
  }
#endif

  return popcount_constant(v);
}
}  // namespace impl

template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> popcount(
    T v) noexcept {
  return impl::popcount_builtin(v);
}

/**
 * consecutive 0-bits starting from MSB.
 *
 * 0b0000'0000 = 8
 * 0b0000'0001 = 7
 * 0b0000'1110 = 4
 */
template <class T>
inline constexpr std::enable_if_t<std::is_unsigned<T>::value, int> countl_zero(
    T x) noexcept {
#if (defined(__clang_major__) && (__clang_major__ >= 9))
  // clang-9 can translate the linear bitshift version to the right
  // __builtin_clz*() itself in all optimization levels
  return impl::countl_zero_linear(x);
#else
  return impl::countl_zero_builtin(x);
#endif
}

/**
 * consecutive 0-bits starting from LSB (right).
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> countr_zero(
    T x) noexcept {
#if (defined(__clang_major__) && (__clang_major__ >= 9))
  // clang-9 can translate the linear bitshift version to the right
  // __builtin_clz*() itself in all optimization levels
  return impl::countr_zero_linear(x);
#else
  return impl::countr_zero_builtin(x);
#endif
}  // namespace stdx

/**
 * consecutive 1-bits starting from LSB (right).
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> countr_one(
    T x) noexcept {
  // countr_one(0b0000'0011) == 2
  // countr_zero(0b1111'1100) == 2
  return countr_zero(static_cast<T>(~x));
}

/**
 * consecutive 1-bits starting from LSB (right).
 */
template <class T>
constexpr std::enable_if_t<std::is_unsigned<T>::value, int> countl_one(
    T x) noexcept {
  // countl_one(0b0000'0011) == 6
  // countl_zero(0b1111'1100) == 6
  return countl_zero(static_cast<T>(~x));
}

}  // namespace stdx
#endif
