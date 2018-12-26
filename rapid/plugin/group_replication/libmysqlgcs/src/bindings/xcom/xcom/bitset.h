/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BITSET_H
#define BITSET_H

#ifdef __cplusplus
extern "C" {
#endif

bit_set *clone_bit_set(bit_set *orig);
bit_set *new_bit_set(uint32_t bits);
void bit_set_or(bit_set *x, bit_set const *y);
void bit_set_xor(bit_set *x, bit_set const *y);
void dbg_bit_set(bit_set *bs);
char *dbg_bitset(bit_set const *p, u_int nodes);
void free_bit_set(bit_set *bs);

#ifdef __cplusplus
}
#endif

#endif

