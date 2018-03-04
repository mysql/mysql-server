/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <assert.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

bit_set *new_bit_set(uint32_t bits) {
  bit_set *bs = malloc(sizeof(*bs));
  bs->bits.bits_len = howmany_words(bits, MASK_BITS);
  bs->bits.bits_val = malloc(bs->bits.bits_len * sizeof(*bs->bits.bits_val));
  BIT_ZERO(bs);
  return bs;
}

bit_set *clone_bit_set(bit_set *orig) {
  if (!orig) return orig;
  {
    bit_set *bs = malloc(sizeof(*bs));
    bs->bits.bits_len = orig->bits.bits_len;
    bs->bits.bits_val = malloc(bs->bits.bits_len * sizeof(*bs->bits.bits_val));
    memcpy(bs->bits.bits_val, orig->bits.bits_val,
           bs->bits.bits_len * sizeof(*bs->bits.bits_val));
    return bs;
  }
}

void free_bit_set(bit_set *bs) {
  free(bs->bits.bits_val);
  free(bs);
}
/* purecov: begin deadcode */
void dbg_bit_set(bit_set *bs) {
  unsigned int i = 0;

  GET_GOUT;

  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE))
    return;

  for (i = 0;
       i < bs->bits.bits_len * sizeof(*bs->bits.bits_val) * BITS_PER_BYTE;
       i++) {
    NPUT(BIT_ISSET(i, bs), d);
  }
  PRINT_GOUT;
  FREE_GOUT;
}

void bit_set_or(bit_set *x, bit_set const *y) {
  unsigned int i = 0;
  assert(x->bits.bits_len == y->bits.bits_len);
  for (i = 0; i < x->bits.bits_len; i++) {
    x->bits.bits_val[i] |= y->bits.bits_val[i];
  }
}

#if 0
void bit_set_and(bit_set *x, bit_set const *y)
{
  unsigned int i = 0;
  assert(x->bits.bits_len == y->bits.bits_len);
  for(i = 0; i < x->bits.bits_len ; i++){
    x->bits.bits_val[i] &= y->bits.bits_val[i];
  }
}
#endif

#if 0
void bit_set_xor(bit_set *x, bit_set const *y)
{
  unsigned int i = 0;
  assert(x->bits.bits_len == y->bits.bits_len);
  for(i = 0; i < x->bits.bits_len ; i++){
    x->bits.bits_val[i] ^= y->bits.bits_val[i];
  }
}
#endif

#if 0
void bit_set_not(bit_set *x)
{
  unsigned int i = 0;
  for(i = 0; i < x->bits.bits_len ; i++){
    x->bits.bits_val[i] = ~x->bits.bits_val[i];
  }
}
#endif

/* Debug a bit set */
char *dbg_bitset(bit_set const *p, u_int nodes) {
  u_int i = 0;
  GET_NEW_GOUT;
  if (!p) {
    STRLIT("p == 0 ");
  } else {
    STRLIT("{");
    for (i = 0; i < nodes; i++) {
      NPUT(BIT_ISSET(i, p), d);
    }
    STRLIT("} ");
  }
  RET_GOUT;
}

#ifdef ETEST
int main() {
  bit_set *bs = new_bit_set(64);
  BIT_SET(16, bs);
  XDBG("%X" NEWLINE, bs->bits.bits_val[0]);
  dbg_bit_set(bs);
  BIT_CLR(16, bs);
  XDBG("%X" NEWLINE, bs->bits.bits_val[0]);
  dbg_bit_set(bs);
  BIT_XOR(16, bs);
  XDBG("%X" NEWLINE, bs->bits.bits_val[0]);
  dbg_bit_set(bs);
  BIT_SET(33, bs);
  XDBG("%X" NEWLINE, bs->bits.bits_val[1]);
  dbg_bit_set(bs);
  BIT_CLR(33, bs);
  XDBG("%X" NEWLINE, bs->bits.bits_val[1]);
  dbg_bit_set(bs);
  BIT_XOR(33, bs);
  XDBG("%X" NEWLINE, bs->bits.bits_val[1]);
  dbg_bit_set(bs);
  bit_set_not(bs);
  dbg_bit_set(bs);
  return 0;
}
#endif
/* purecov: end */
