//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_BIT_UTILITIES_HPP
#define BOOST_CONTAINER_DETAIL_BIT_UTILITIES_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/assert.hpp>
#include <boost/cstdint.hpp>

//_BitScan{Forward,Reverse}[64] used by the bit helpers below. The 64-bit
//variants only exist on x64/ARM64; the 32-bit ones are also available on x86
//and ARM, where the 64-bit value is scanned as two 32-bit halves.
#if defined(BOOST_MSVC) && \
    (defined(_M_X64) || defined(_M_ARM64) || defined(_M_IX86) || defined(_M_ARM))
#  include <intrin.h>
#endif

namespace boost {
namespace container {
namespace dtl {

//! Count trailing zero bits of a 64-bit value.
//! Precondition: x != 0 (the result is undefined otherwise).
BOOST_CONTAINER_FORCEINLINE int unchecked_countr_zero(boost::uint64_t x)
{
#if defined(BOOST_MSVC) && (defined(_M_X64) || defined(_M_ARM64))
   unsigned long r;
   _BitScanForward64(&r, x);
   return (int)r;
#elif defined(BOOST_GCC) || defined(BOOST_CLANG)
   return (int)__builtin_ctzll(x);
#elif defined(BOOST_MSVC) && (defined(_M_IX86) || defined(_M_ARM))
   //32-bit MSVC targets lack _BitScanForward64: scan the two 32-bit halves.
   unsigned long r;
   if (_BitScanForward(&r, (unsigned long)x))
      return (int)r;
   _BitScanForward(&r, (unsigned long)(x >> 32));
   return (int)r + 32;
#else
   //O(1) branchless fallback for compilers without intrinsics, using
   //mod-37 perfect-hash table on 32-bit halves
   //(avoids 64-bit multiplies, friendlier to exotic 32-bit-only targets)
   static const unsigned char mod37[37] =
   { 32, 0, 1, 26, 2, 23, 27, 0, 3, 16, 24, 30, 28, 11, 0, 13, 4, 7, 17, 0
   , 25, 22, 31, 15, 29, 10, 12, 6, 0, 21, 14, 9, 5, 20, 8, 19, 18 };
   boost::uint32_t lo = (boost::uint32_t)x;
   if (lo)
      return mod37[((boost::uint32_t)(0u - lo) & lo) % 37u];
   boost::uint32_t hi = (boost::uint32_t)(x >> 32);
   return 32 + mod37[((boost::uint32_t)(0u - hi) & hi) % 37u];
#endif
}

//! Count trailing one bits of a 64-bit value.
//! Precondition: x != (uint64_t)-1 (the result is undefined otherwise).
BOOST_CONTAINER_FORCEINLINE int unchecked_countr_one(boost::uint64_t x)
{
   return unchecked_countr_zero(~x);
}

//! Count leading zero bits of a 64-bit value.
//! Precondition: x != 0 (the result is undefined otherwise).
BOOST_CONTAINER_FORCEINLINE int unchecked_countl_zero(boost::uint64_t x)
{
#if defined(BOOST_MSVC) && (defined(_M_X64) || defined(_M_ARM64))
   unsigned long r;
   _BitScanReverse64(&r, x);
   return (int)(63 - r);
#elif defined(BOOST_GCC) || defined(BOOST_CLANG)
   return (int)__builtin_clzll(x);
#elif defined(BOOST_MSVC) && (defined(_M_IX86) || defined(_M_ARM))
   //32-bit MSVC targets lack _BitScanReverse64: scan the two 32-bit halves.
   unsigned long r;
   if (_BitScanReverse(&r, (unsigned long)(x >> 32)))
      return 31 - (int)r;
   _BitScanReverse(&r, (unsigned long)x);
   return 63 - (int)r;
#else
   //O(1) branchless fallback for compilers without intrinsics, using
   //smear-right + mod-37 perfect-hash table on 32-bit halves
   static const unsigned char mod37[37] =
   { 32, 31, 6, 30, 9, 5, 0, 29, 16, 8, 2, 4, 21, 0, 19, 28, 25, 15, 0, 7
   , 10, 1, 17, 3, 22, 20, 26, 0, 11, 18, 23, 27, 12, 24, 13, 14, 0 };
   boost::uint32_t hi = (boost::uint32_t)(x >> 32);
   if (hi) {
      hi |= hi >> 1; hi |= hi >> 2; hi |= hi >> 4; hi |= hi >> 8; hi |= hi >> 16;
      return mod37[hi % 37u];
   }
   boost::uint32_t lo = (boost::uint32_t)x;
   lo |= lo >> 1; lo |= lo >> 2; lo |= lo >> 4; lo |= lo >> 8; lo |= lo >> 16;
   return 32 + mod37[lo % 37u];
#endif
}

//! Count the number of set bits of a 64-bit value.
BOOST_CONTAINER_FORCEINLINE int popcount(boost::uint64_t x)
{
#if defined(BOOST_GCC) || defined(BOOST_CLANG)
   return (int)__builtin_popcountll(x);
#elif defined(BOOST_MSVC) && defined(_M_X64)
   return (int)__popcnt64(x);
#else
   //Portable SWAR population count.
   x = x - ((x >> 1) & boost::uint64_t(0x5555555555555555));
   x = (x & boost::uint64_t(0x3333333333333333)) + ((x >> 2) & boost::uint64_t(0x3333333333333333));
   x = (x + (x >> 4)) & boost::uint64_t(0x0f0f0f0f0f0f0f0f);
   return (int)((x * boost::uint64_t(0x0101010101010101)) >> 56);
#endif
}

} //namespace dtl {
} //namespace container {
} //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif //BOOST_CONTAINER_DETAIL_BIT_UTILITIES_HPP
