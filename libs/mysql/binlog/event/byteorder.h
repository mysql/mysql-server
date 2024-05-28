/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

  @brief The file contains functions to convert the byte encoding of integer
  values to and from little-endian and big-endian byte order.
*/

#ifndef MYSQL_BINLOG_EVENT_BYTEORDER_H
#define MYSQL_BINLOG_EVENT_BYTEORDER_H

#include <stdint.h>
#include "my_compiler.h"
#include "my_config.h"
#include "mysql/serialization/byte_order_helpers.h"
#ifndef STANDALONE_BINLOG
#define HAVE_MYSYS 1
#endif

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
  Converting a 16 bit integer from little-endian byte order to host byteorder

  @param x  16-bit integer in little endian byte order
  @return  16-bit integer in host byte order
*/
uint16_t inline le16toh(uint16_t x) {
#ifndef WORDS_BIGENDIAN
  return x;
#else
  return ((x >> 8) | (x << 8));
#endif
}
#endif

#if !defined(le32toh)
/**
  Converting a 32 bit integer from little-endian byte order to host byteorder

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

#if !defined(be32toh)
/**
  Converting a 32 bit integer from big-endian byte order to host byteorder

  @param x  32-bit integer in big endian byte order
  @return  32-bit integer in host byte order
*/
uint32_t inline be32toh(uint32_t x) {
#ifndef WORDS_BIGENDIAN
  return (((x >> 24) & 0xff) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) |
          ((x << 24) & 0xff000000));
#else
  return x;
#endif
}
#endif

#endif  // MYSQL_BINLOG_EVENT_BYTEORDER_H
