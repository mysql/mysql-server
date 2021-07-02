/* Copyright (c) 2003, 2021, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "base64.h"

/**
  @file mysys/base64.cc
*/

#ifdef MAIN

#define require(b)                                             \
  {                                                            \
    if (!(b)) {                                                \
      printf("Require failed at %s:%d\n", __FILE__, __LINE__); \
      abort();                                                 \
    }                                                          \
  }

int main(void) {
  int i;
  size_t j;
  size_t k, l;
  size_t dst_len;
  size_t needed_length;
  char *src;
  char *s;
  char *str;
  char *dst;
  const char *end_ptr;
  size_t src_len;

  for (i = 0; i <= 500; i++) {
    /* Create source data */
    if (i == 500) {
#if (SIZEOF_VOIDP == 8)
      printf("Test case for base64 max event length: 2119594243\n");
      src_len = 2119594243;
#else
      printf("Test case for base64 max event length: 536870912\n");
      src_len = 536870912;
#endif
    } else
      src_len = rand() % 1000 + 1;

    src = (char *)malloc(src_len);
    s = src;

    require(src);
    for (j = 0; j < src_len; j++) {
      char c = rand();
      *s++ = c;
    }

    /* Encode */
    needed_length = base64_needed_encoded_length(src_len);
    str = (char *)malloc(needed_length);
    require(str);
    for (k = 0; k < needed_length; k++)
      str[k] = 0xff; /* Fill memory to check correct NUL termination */
    require(base64_encode(src, src_len, str) == 0);
    require(needed_length == strlen(str) + 1);

    /* Decode */
    dst = (char *)malloc(base64_needed_decoded_length(strlen(str)));
    require(dst);
    dst_len = base64_decode(str, strlen(str), dst, &end_ptr, 0);
    require(dst_len == src_len);

    if (memcmp(src, dst, src_len) != 0) {
      printf("       --------- src ---------   --------- dst ---------\n");
      for (k = 0; k < src_len; k += 8) {
        printf("%.4x   ", (uint)k);
        for (l = 0; l < 8 && k + l < src_len; l++) {
          unsigned char c = src[k + l];
          printf("%.2x ", (unsigned)c);
        }

        printf("  ");

        for (l = 0; l < 8 && k + l < dst_len; l++) {
          unsigned char c = dst[k + l];
          printf("%.2x ", (unsigned)c);
        }
        printf("\n");
      }
      printf("src length: %.8x, dst length: %.8x\n", (uint)src_len,
             (uint)dst_len);
      require(0);
    }
    free(src);
    free(str);
    free(dst);
  }
  printf("Test succeeded.\n");
  return 0;
}

#endif
