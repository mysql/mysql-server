/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef MY_CHECKSUM_INCLUDED
#define MY_CHECKSUM_INCLUDED

/**
  @file include/my_checksum.h
  Abstraction functions over zlib/intrinsics.
*/

#include <cassert>
#include <cstdint>      // std::uint32_t
#include <limits>       // std::numeric_limits
#include <type_traits>  // std::is_convertible

#include <zlib.h>  // crc32_z

#include "my_compiler.h"  // My_ATTRIBUTE
#include "my_config.h"

#ifdef HAVE_ARMV8_CRC32_INTRINSIC
#include <arm_acle.h>   // __crc32x
#include <asm/hwcap.h>  // HWCAP_CRC32
#include <sys/auxv.h>   // getauxval
#endif                  /* HAVE_ARMV8_CRC32_INTRINSIC */

namespace mycrc32 {
#ifdef HAVE_ARMV8_CRC32_INTRINSIC
const bool auxv_at_hwcap = (getauxval(AT_HWCAP) & HWCAP_CRC32);

// Some overloads for these since to allow us to work genericly
MY_ATTRIBUTE((target("+crc")))
inline std::uint32_t IntegerCrc32(std::uint32_t crc, std::uint8_t b) {
  return __crc32b(crc, b);
}

MY_ATTRIBUTE((target("+crc")))
inline std::uint32_t IntegerCrc32(std::uint32_t crc, std::uint16_t h) {
  return __crc32h(crc, h);
}

MY_ATTRIBUTE((target("+crc")))
inline std::uint32_t IntegerCrc32(std::uint32_t crc, std::uint32_t w) {
  return __crc32w(crc, w);
}

MY_ATTRIBUTE((target("+crc")))
inline std::uint32_t IntegerCrc32(std::uint32_t crc, std::uint64_t d) {
  return __crc32d(crc, d);
}
#else /* HAVE_ARMV8_CRC32_INTRINSIC */

template <class I>
inline std::uint32_t IntegerCrc32(std::uint32_t crc, I i) {
  unsigned char buf[sizeof(I)];
  memcpy(buf, &i, sizeof(I));
  crc = ~crc;
  crc = crc32_z(crc, buf, sizeof(I));
  return ~crc;
}

#endif /* HAVE_ARMV8_CRC32_INTRINSIC */

template <class PT>
inline std::uint32_t PunnedCrc32(std::uint32_t crc, const unsigned char *buf,
                                 size_t len) {
  crc = ~crc;
  const unsigned char *plast = buf + (len / sizeof(PT)) * sizeof(PT);
  const unsigned char *last = buf + len;
  for (; buf < plast; buf += sizeof(PT)) {
    PT pv;
    memcpy(&pv, buf, sizeof(PT));
    crc = IntegerCrc32(crc, pv);
  }

  for (; buf < last; ++buf) {
    crc = IntegerCrc32(crc, *buf);
  }

  return (~crc);
}
}  // namespace mycrc32

using ha_checksum = std::uint32_t;

/**
   Calculate a CRC32 checksum for a memoryblock.

   @param crc       Start value for crc.
   @param pos       Pointer to memory block.
   @param length    Length of the block.

   @returns Updated checksum.
*/
inline ha_checksum my_checksum(ha_checksum crc, const unsigned char *pos,
                               size_t length) {
#ifdef HAVE_ARMV8_CRC32_INTRINSIC
  if (mycrc32::auxv_at_hwcap)
    return mycrc32::PunnedCrc32<std::uint64_t>(crc, pos, length);
#endif  // HAVE_ARMV8_CRC32_INTRINSIC
  static_assert(std::is_convertible<uLong, ha_checksum>::value,
                "uLong cannot be converted to ha_checksum");
  assert(crc32_z(crc, pos, length) <= std::numeric_limits<ha_checksum>::max());
  return crc32_z(crc, pos, length);
}
#endif /* not defined(MY_CEHCKSUM_INCLUDED) */
