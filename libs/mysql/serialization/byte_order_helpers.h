// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_SERIALIZATION_BYTE_ORDER_HELPERS_H
#define MYSQL_SERIALIZATION_BYTE_ORDER_HELPERS_H

#include <bitset>
#include <sstream>
#include <vector>
#include "my_byteorder.h"

#ifdef HAVE_ENDIAN_CONVERSION_MACROS
#include <endian.h>
#endif

/// @file
/// Experimental API header
/// Conversions between different number representations. MySQL writes
/// binary data in LE, therefore this header defines the following conversions:
/// host -> LE (for writing)
/// LE -> host (for reading)

#if !defined(le64toh)
/**
  Converting a 64 bit integer from little-endian byte order to host byteorder

  @param x  64-bit integer in little endian byte order
  @return  64-bit integer in host byte order
*/
uint64_t inline le64toh(uint64_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  x = ((x << 8) & 0xff00ff00ff00ff00ULL) | ((x >> 8) & 0x00ff00ff00ff00ffULL);
  x = ((x << 16) & 0xffff0000ffff0000ULL) | ((x >> 16) & 0x0000ffff0000ffffULL);
  return (x << 32) | (x >> 32);
#endif
}
#endif

#if !defined(htole64)
/**
  Converting a 64 bit integer from host's byte order to little-endian byte
  order.

  @param x  64-bit integer in host's byte order
  @return  64-bit integer in little endian byte order
*/

uint64_t inline htole64(uint64_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  x = ((x << 8) & 0xff00ff00ff00ff00ULL) | ((x >> 8) & 0x00ff00ff00ff00ffULL);
  x = ((x << 16) & 0xffff0000ffff0000ULL) | ((x >> 16) & 0x0000ffff0000ffffULL);
  return (x << 32) | (x >> 32);
#endif
}
#endif

#endif  // MYSQL_SERIALIZATION_BYTE_ORDER_HELPERS_H
