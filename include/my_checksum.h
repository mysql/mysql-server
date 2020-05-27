/*
   Copyright (c) 2020, Oracle and/or its affiliates.

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

#include <zlib.h>  // crc32
#include <cassert>
#include <cstdint>      // std::uint32_t
#include <limits>       // std::numeric_limits
#include <type_traits>  // std::is_convertible

#include "my_config.h"

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
  static_assert(std::is_convertible<uLong, ha_checksum>::value,
                "uLong cannot be converted to ha_checksum");
  assert(crc32_z(static_cast<ha_checksum>(crc), pos, length) <
         std::numeric_limits<ha_checksum>::max());
  return crc32_z(static_cast<ha_checksum>(crc), pos, length);
}
#endif  // MY_CHECKSUM_INCLUDED
