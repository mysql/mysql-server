/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

/**
  @file
  plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h
  @author Neha Kumari

  @brief The file contains functions to convert the byte encoding of integer
  values to and from little-endian and big-endian byte order.
*/

#ifndef BYTEORDER_INCLUDED
#define BYTEORDER_INCLUDED

#include <stdint.h>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/mysql_gcs.h"

/*
  Methods for reading and storing in machine independent
  format (low byte first).
*/

/*
  Checking le16toh is required because the machine may have the header
  but the functions might not be defined if the version of glibc < 2.9
*/
#ifdef HAVE_ENDIAN_CONVERSION_MACROS
#include <endian.h>
#endif

#if !defined(le16toh)
/**
  Converting a 16 bit integer from little-endian byte order to host byteorder.

  @param x  16-bit integer in little endian byte order
  @return  16-bit integer in host byte order
*/

uint16_t inline le16toh(uint16_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  return static_cast<uint16_t>((x >> 8) | (x << 8));
#endif
}
#endif

#if !defined(le32toh)
/**
  Converting a 32 bit integer from little-endian byte order to host byteorder.

  @param x  32-bit integer in little endian byte order
  @return  32-bit integer in host byte order
*/

uint32_t inline le32toh(uint32_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  return (((x >> 24) & 0xff) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) |
          ((x << 24) & 0xff000000));
#endif
}
#endif

#if !defined(le64toh)
/**
  Converting a 64 bit integer from little-endian byte order to host byteorder.

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

#if !defined(htole16)
/**
  Converting a 16 bit integer from host's byte order to little-endian byte
  order.

  @param x  16-bit integer in host byte order
  @return  16-bit integer in little endian byte order
*/

uint16_t inline htole16(uint16_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  return static_cast<uint16_t>((x >> 8) | (x << 8));
#endif
}
#endif

#if !defined(htole32)
/**
  Converting a 32 bit integer from host's byte order to little-endian byte
  order.

  @param x  32-bit integer in host byte order
  @return  32-bit integer in little endian byte order
*/

uint32_t inline htole32(uint32_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  return (((x >> 24) & 0xff) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) |
          ((x << 24) & 0xff000000));
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

#endif  // BYTEORDER_INCLUDED
