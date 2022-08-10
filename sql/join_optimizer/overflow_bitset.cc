/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/overflow_bitset.h"

#include <stdint.h>
#include <string.h>

#include "my_alloc.h"
#include "template_utils.h"

void OverflowBitset::InitOverflow(MEM_ROOT *mem_root, size_t capacity) {
  size_t num_blocks = (capacity + 63) / 64;
  m_ext = pointer_cast<Ext *>(
      mem_root->Alloc(sizeof(Ext) + sizeof(uint64_t) * (num_blocks - 1)));
  m_ext->m_num_blocks = num_blocks;
  memset(m_ext->m_bits, 0, sizeof(uint64_t) * num_blocks);
  assert(!is_inline());
}

MutableOverflowBitset OverflowBitset::OrOverflow(MEM_ROOT *mem_root,
                                                 OverflowBitset a,
                                                 OverflowBitset b) {
  assert(!a.is_inline());
  assert(!b.is_inline());
  assert(a.capacity() == b.capacity());
  MutableOverflowBitset ret{mem_root, a.capacity()};
  for (unsigned i = 0; i < a.m_ext->m_num_blocks; ++i) {
    ret.m_ext->m_bits[i] = a.m_ext->m_bits[i] | b.m_ext->m_bits[i];
  }
  return ret;
}

MutableOverflowBitset OverflowBitset::AndOverflow(MEM_ROOT *mem_root,
                                                  OverflowBitset a,
                                                  OverflowBitset b) {
  assert(!a.is_inline());
  assert(!b.is_inline());
  assert(a.capacity() == b.capacity());
  MutableOverflowBitset ret{mem_root, a.capacity()};
  for (unsigned i = 0; i < a.m_ext->m_num_blocks; ++i) {
    ret.m_ext->m_bits[i] = a.m_ext->m_bits[i] & b.m_ext->m_bits[i];
  }
  return ret;
}

MutableOverflowBitset OverflowBitset::XorOverflow(MEM_ROOT *mem_root,
                                                  OverflowBitset a,
                                                  OverflowBitset b) {
  assert(!a.is_inline());
  assert(!b.is_inline());
  assert(a.capacity() == b.capacity());
  MutableOverflowBitset ret{mem_root, a.capacity()};
  for (unsigned i = 0; i < a.m_ext->m_num_blocks; ++i) {
    ret.m_ext->m_bits[i] = a.m_ext->m_bits[i] ^ b.m_ext->m_bits[i];
  }
  return ret;
}

void MutableOverflowBitset::ClearBitsOverflow(int begin_bit_num,
                                              int end_bit_num) {
  assert(!is_inline());
  assert(begin_bit_num >= 0);
  assert(end_bit_num >= 0);
  assert(begin_bit_num <= end_bit_num);
  assert(static_cast<size_t>(begin_bit_num) <= capacity());
  assert(static_cast<size_t>(end_bit_num) <= capacity());

  if (begin_bit_num / 64 == end_bit_num / 64) {
    // Begin and end are in the same block.
    m_ext->m_bits[begin_bit_num / 64] &=
        ~BitsBetween(begin_bit_num % 64, end_bit_num % 64);
    return;
  }

  // Schematically, where x is untouched bits and 0 are the bits to clear
  // (shown here with 8-bit blocks instead of 64 for brevity):
  //
  // xxxxx000 [ 00000000 00000000 ... ] 00000xxx
  if (begin_bit_num % 64 != 0) {
    m_ext->m_bits[begin_bit_num / 64] &= ~BitsBetween(begin_bit_num % 64, 64);
    begin_bit_num += 64 - (begin_bit_num % 64);
  }
  if (end_bit_num % 64 != 0) {
    m_ext->m_bits[end_bit_num / 64] &= ~BitsBetween(0, end_bit_num % 64);
    end_bit_num &= ~63;
  }
  for (int block_num = begin_bit_num / 64; block_num < end_bit_num / 64;
       ++block_num) {
    m_ext->m_bits[block_num] = 0;
  }
}

bool OverlapsOverflow(OverflowBitset a, OverflowBitset b) {
  assert(!a.is_inline());
  assert(!b.is_inline());
  assert(a.capacity() == b.capacity());
  for (unsigned i = 0; i < a.m_ext->m_num_blocks; ++i) {
    if (Overlaps(a.m_ext->m_bits[i], b.m_ext->m_bits[i])) {
      return true;
    }
  }
  return false;
}

bool IsSubsetOverflow(OverflowBitset a, OverflowBitset b) {
  assert(!a.is_inline());
  assert(!b.is_inline());
  assert(a.capacity() == b.capacity());
  for (unsigned i = 0; i < a.m_ext->m_num_blocks; ++i) {
    if (!IsSubset(a.m_ext->m_bits[i], b.m_ext->m_bits[i])) {
      return false;
    }
  }
  return true;
}

int PopulationCountOverflow(OverflowBitset x) {
  assert(!x.is_inline());
  int count = 0;
  for (unsigned i = 0; i < x.m_ext->m_num_blocks; ++i) {
    count += PopulationCount(x.m_ext->m_bits[i]);
  }
  return count;
}
