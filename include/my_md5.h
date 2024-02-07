#ifndef MY_MD5_INCLUDED
#define MY_MD5_INCLUDED

/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#include <sys/types.h>

#include "dig_vec.h"
#include "my_inttypes.h"

/**
  @file include/my_md5.h
  Wrapper function for MD5 implementation.
*/

extern int compute_md5_hash(char *digest, const char *buf, size_t len);

/*
  Convert an array of bytes to a hexadecimal representation.

  Used to generate a hexadecimal representation of a message digest.
*/
static inline void array_to_hex(char *to, const unsigned char *str, uint len) {
  const unsigned char *str_end = str + len;
  for (; str != str_end; ++str) {
    *to++ = dig_vec_lower[((uchar)*str) >> 4];
    *to++ = dig_vec_lower[((uchar)*str) & 0x0F];
  }
}

#endif /* MY_MD5_INCLUDED */
