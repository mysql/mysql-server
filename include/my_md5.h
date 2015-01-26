#ifndef MY_MD5_INCLUDED
#define MY_MD5_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "m_string.h"
#include "my_md5_size.h"

/*
  Wrapper function for MD5 implementation.
*/
#ifdef __cplusplus
extern "C" {
#endif

void compute_md5_hash(char *digest, const char *buf, int len);

/*
  Convert an array of bytes to a hexadecimal representation.

  Used to generate a hexadecimal representation of a message digest.
*/
static inline void array_to_hex(char *to, const unsigned char *str, uint len)
{
  const unsigned char *str_end= str + len;
  for (; str != str_end; ++str)
  {
    *to++= _dig_vec_lower[((uchar) *str) >> 4];
    *to++= _dig_vec_lower[((uchar) *str) & 0x0F];
  }
}

#ifdef __cplusplus
}
#endif

#endif /* MY_MD5_INCLUDED */
