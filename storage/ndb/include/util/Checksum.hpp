/*
   Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CHECKSUM_HPP
#define CHECKSUM_HPP

#include <assert.h>
#include <ndb_types.h>
#include <stddef.h>


/**
 * We want the compiler to vectorize the xor-loop using SSE extensions.
 * Preferably quad-word-aligned memory access instructions should be
 * used as well.
 *
 * Benchmarking and assembly inspection has proved that Clang produce
 * vectorized code with the standard BUILD_TYPE=Release/RelWithDebInfo
 * compiler options.
 *
 * GCC need the 'vectorize' compiler pragma to be specified, see below
 *
 * Clang is also able to use quad-word-aligned memory access, but this
 * comes with some initial setup overhead in finding the aligned start.
 * This is why we only do this for larger memory blocks.
 *
 * No alignment improvements were found when testing with GCC.
 * Thus we left this out for this platform, in favour of a simpler implementation.
 *
 * Overall, Clang generated code seems to be ~10-20% faster than GCC.
 * When calculating checksum on memory objects fitting in the L1 cache,
 * Clang code peaked out at ~120Gb/2, while GCC reached ~100Gb/s.
 *
 * The performance improvement of getting the code vectorized is ~8x!
 * (i7-10875H CPU, @5.1 GHz)
 */

#if defined(__clang__)

/**
 * Defines the xorChecksum algorithm as a template as that guarantee inlining
 * of the code, contrary to an inlined function, where 'inline' is just a hint.
 *
 * Note that even if the C++ code sums up from a simple loop over Uint32's,
 * the compiler may (and should!) generate 128-bit wide SSE instructions out
 * of this.
 */
template <class T>
T xorChecksum(const T *const buf, const size_t words, T sum)
{
  T tmp = 0;
  for (const T *ptr = buf; ptr < (buf + words); ++ptr) {
    tmp ^= *ptr;
  }
  return sum ^ tmp;
}

/**
 * computeXorChecksumAligned16: Compute checksum on 16-byte aligned memory.
 *
 * Knowing that the address is aligned, the compiler can generate code using
 * the more efficient quad-word-aligned SSE-instructions. It seems
 * like Clang doesn't take advantage of the assume_aligned hint, unless the
 * code is in a non-inlined function.
 *
 * We only use this aligned variant of XorChecksum for large memory blocks.
 * Thus, the overhead for the extra function call should be relatively small.
 */
Uint32 computeXorChecksumAligned16(const Uint32 *buf, const size_t words,
                                   const Uint32 sum);

inline
Uint32
computeXorChecksum(const Uint32 *const buf, const size_t words, Uint32 sum = 0)
{
  if (words < 256) {  // Decided by empirical experiments
    return xorChecksum(buf, words, sum);
  } else {
    /**
     * For larger memory blocks there is a ~20% performance gain using aligned
     * memory accesses. Blocks has to be large enough to compensate for the
     * extra overhead of finding the aligned start (below).
     */
    unsigned int i;
    for (i = 0; ((size_t)(buf+i) % 16) != 0; ++i)
      sum ^= buf[i];  // Advance to a 16 byte aligned address

    return computeXorChecksumAligned16(buf+i, words-i, sum);
  }
}

#else

// For all non-Clang compilers:

#if defined(__GNUC__)
#pragma GCC push_options
// Specifying 'unroll and vectorize', improve GCC generated code by ~8x.
#pragma GCC optimize("unroll-loops","tree-vectorize")
#endif

/**
 * GCC generated code had too much reuse of the same XXM registers. Thus, it
 * didn't take advantage of that most processors are able to do multiple
 * operations in parallel, if they are independent. We fix this by having
 * two parallel 'Xor-streams' in the loop below.
 */
inline
Uint32
computeXorChecksum(const Uint32 *const buf, const size_t words,
                   const Uint32 sum = 0)
{
  Uint32 tmp0 = 0;
  Uint32 tmp1 = 0;
  const Uint32 middle = words / 2;
  for (const Uint32 *ptr = buf; ptr < (buf + middle); ++ptr) {
    // Use two seperate 'Xor-streams'
    tmp0 ^= *ptr;
    tmp1 ^= *(ptr+middle);
  }
  // Handle any odd trailing word
  if ((words % 2) != 0) {
    tmp0 ^= *(buf + words-1);
  }
  return sum ^ tmp0 ^ tmp1;
}

#if defined(__GNUC__)
#pragma GCC pop_options
#endif

#endif


#endif // CHECKSUM_HPP

