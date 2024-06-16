/*
   Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "my_config.h"

#define N (1024 * 1024)
#define S 65537
/* The number S must be relative prime to N. */

uint32_t bm[N * 4];

uint32_t bms[N][4];
uint32_t len[N];
uint32_t pos[N];

typedef uint32_t Uint32;
#define MEMCOPY_NO_WORDS(to, from, no_of_words) \
  memcpy((to), (void *)(from), (size_t)(no_of_words << 2));

/****************************************************************************/
static void getbits(const Uint32 *src, Uint32 bit_pos, Uint32 *dst,
                    Uint32 count) {
  Uint32 val;

  /* Move to start word in src. */
  src += bit_pos >> 5;
  bit_pos &= 31;

  /*
    If word-aligned, copy word-for-word is faster and avoids edge
    cases with undefined bitshift operations.
  */
  if (bit_pos == 0) {
    MEMCOPY_NO_WORDS(dst, src, count >> 5);
    src += count >> 5;
    dst += count >> 5;
    count &= 31;
  } else {
    while (count >= 32) {
      /*
        Get bits 0-X from first source word.
        Get bits (X+1)-31 from second source word.
        Handle endian so that we store bit 0 in the first byte, and bit 31 in
        the last byte, so that we don't waste space on 32-bit aligning the
        bitmap.
      */
#ifdef WORDS_BIGENDIAN
      Uint32 firstpart_len = 32 - bit_pos;
      val = *src++ & (((Uint32)1 << firstpart_len) - 1);
      val |= *src & ((Uint32)0xffffffff << firstpart_len);
#else
      val = *src++ >> bit_pos;
      val |= *src << (32 - bit_pos);
#endif
      *dst++ = val;
      count -= 32;
    }
  }

  /* Handle any partial word at the end. */
  if (count > 0) {
    if (bit_pos + count <= 32) {
      /* Last part is wholly contained in one source word. */
#ifdef WORDS_BIGENDIAN
      val = *src >> (32 - (bit_pos + count));
#else
      val = *src >> bit_pos;
#endif
    } else {
      /* Need to assemble last part from two source words. */
#ifdef WORDS_BIGENDIAN
      Uint32 firstpart_len = 32 - bit_pos;
      val = *src++ & (((Uint32)1 << firstpart_len) - 1);
      val |= (*src >> (32 - count)) & ((Uint32)0xffffffff << firstpart_len);
#else
      val = *src++ >> bit_pos;
      val |= *src << (32 - bit_pos);
#endif
    }
    /* Mask off any unused bits. */
    *dst = val & (((Uint32)1 << count) - 1);
  }
}

static void setbits(const Uint32 *src, Uint32 *dst, Uint32 bit_pos,
                    Uint32 count) {
  Uint32 val;

  /* Move to start word in dst. */

  dst += bit_pos >> 5;
  bit_pos &= 31;

#ifdef WORDS_BIGENDIAN
  Uint32 low_mask = ((Uint32)0xffffffff) << (32 - bit_pos);
  Uint32 high_mask = ~low_mask;
#else
  Uint32 low_mask = (((Uint32)1) << bit_pos) - 1;
  Uint32 high_mask = ~low_mask;
#endif

  if (bit_pos == 0) {
    MEMCOPY_NO_WORDS(dst, src, count >> 5);
    src += count >> 5;
    dst += count >> 5;
    count &= 31;
  } else {
    while (count >= 32) {
      val = *src++;
#ifdef WORDS_BIGENDIAN
      *dst = (*dst & low_mask) | (val & high_mask);
      dst++;
      *dst = (*dst & high_mask) | (val & low_mask);
#else
      *dst = (*dst & low_mask) | (val << bit_pos);
      dst++;
      *dst = (*dst & high_mask) | (val >> (32 - bit_pos));
#endif
      count -= 32;
    }
  }

  /* Handle any partial word at the end. */
  if (count > 0) {
    val = *src;
    if (bit_pos + count <= 32) {
      /* Remaining part fits in one word of destination. */
      Uint32 end_mask = (((Uint32)1) << count) - 1;
#ifdef WORDS_BIGENDIAN
      Uint32 shift = (32 - (bit_pos + count));
      *dst = (*dst & ~(end_mask << shift)) | ((val & end_mask) << shift);
#else
      *dst = (*dst & ~(end_mask << bit_pos)) | ((val & end_mask) << bit_pos);
#endif
    } else {
      /* Need to split the remaining part across two destination words. */
#ifdef WORDS_BIGENDIAN
      *dst = (*dst & low_mask) | (val & high_mask);
      dst++;
      Uint32 shift = 32 - count;
      Uint32 end_mask = ((((Uint32)1) << (bit_pos + count - 32)) - 1)
                        << (32 - bit_pos);
      *dst = (*dst & ~(end_mask << shift)) | ((val & end_mask) << shift);
#else
      *dst = (*dst & low_mask) | (val << bit_pos);
      dst++;
      Uint32 end_mask = (((Uint32)1) << (count + bit_pos - 32)) - 1;
      *dst = (*dst & ~end_mask) | ((val >> (32 - bit_pos)) & end_mask);
#endif
    }
  }
}
/****************************************************************************/

/* Set up a bunch of test bit fields. */
void fill(void) {
  uint32_t i, j;
  uint32_t p = 0;

  for (i = 0; i < N; i++) {
    memset(bms[i], 0, sizeof(bms[i]));
    pos[i] = p;
    do len[i] = rand() % 128;
    while (!len[i]);
    p += len[i];
    for (j = 0; j < len[i]; j++)
      if (rand() % 2) bms[i][j >> 5] |= (((uint32_t)1) << (j & 31));
  }
}

void write(void) {
  uint32_t i, idx;

  for (i = 0, idx = 0; i < N; i++, idx += S) {
    if (idx >= N) idx -= N;
    setbits(&(bms[idx][0]), &(bm[0]), pos[idx], len[idx]);
  }
}

void read(void) {
  uint32_t buf[4];
  uint32_t i;

  for (i = 0; i < N; i++) {
    getbits(&(bm[0]), pos[i], &(buf[0]), len[i]);
    assert(0 == memcmp(buf, bms[i], ((len[i] + 31) >> 5) << 2));
  }
}

int main(int argc, char *argv[]) {
  uint32_t i;

  srand(1);
  fill();
  write();
  read();

  exit(0);
  return 0;
}
