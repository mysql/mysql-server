/*
   Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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
  for (auto ptr{buf}; ptr < (buf + words); ++ptr) {
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
#pragma GCC optimize("unroll-loops","tree-loop-vectorize")
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
  for (auto ptr{buf}; ptr < (buf + middle); ++ptr) {
    // Use two separate 'Xor-streams'
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



inline
Uint32
rotateChecksum(const Uint32 sum, Uint32 byte_steps)
{
  assert(byte_steps > 0);
  assert(byte_steps < 4);

  const unsigned char *psum = static_cast<const unsigned char*>(static_cast<const void*>(&sum));
  Uint32 rot;
  unsigned char *prot = static_cast<unsigned char*>(static_cast<void*>(&rot));
  for (int i=0, j = byte_steps; i < 4; i ++, j = (j + 1) % 4)
  {
    prot[i] = psum[j];
  }
  return rot;
}

/**
 * @buf series of bytes for which the checksum has to be computed
 * @bytes size of buf in bytes
 * @sum checksum
 */
inline
Uint32
computeXorChecksumBytes(const unsigned char* buf, size_t bytes, Uint32 sum = 0)
{
  assert(bytes > 0);

  // For undoing rotate
  size_t rotate_back = (size_t)buf % sizeof(Uint32);
  /**
   * Number of bytes at the start of buf that are not word aligned.
   * Also the index to the original byte 0 in checksum word.
   */
  size_t rotate = (sizeof(Uint32) - rotate_back) % sizeof(Uint32);
  size_t words = (bytes > rotate) ? (bytes - rotate) / 4 : 0;

  // checksum buf[0..rotate-1] per byte
  if (rotate > 0)
  {
    unsigned char * psum = static_cast<unsigned char*>(static_cast<void*>(&sum));
    for (size_t i = 0; i < rotate && i < bytes; i ++ )
    {
      psum[i] ^= buf[i];
    }
  }

  // checksum buf[rotate..rotate+4*words-1] per word
  if (words > 0)
  {
    // Rotate sum to match alignment
    if (rotate > 0)
    {
      sum = rotateChecksum(sum, rotate);
    }

    sum = computeXorChecksum(static_cast<const Uint32*>(static_cast<const void*>(buf + rotate)),
                             words, sum);

    // Rotate back sum
    if (rotate > 0)
    {
      sum = rotateChecksum(sum, rotate_back);
    }
  }

  // checksum buf[rotate+4*words..bytes-1] per byte
  {
    unsigned char * psum = static_cast<unsigned char*>(static_cast<void*>(&sum));
    for (size_t i = rotate, j = rotate + 4 * words; j < bytes; j ++, i = (i + 1) %4)
    {
      psum[i] ^= buf[j];
    }
  }

  /**
   * Return checksum rotated such that it can be passed in as checksum for
   * next buffer. The 'next byte to XOR' can be memorised in the checksum
   * itself by rotating the checksum so that byte 0 is always next.
   */
  {
    size_t rotate_forward = bytes % 4;
    if (rotate_forward > 0)
    {
      sum = rotateChecksum(sum, rotate_forward);
    }
  }

  return sum;
}

#endif // CHECKSUM_HPP

