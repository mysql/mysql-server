/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef REFCONVERT_H
#define REFCONVERT_H

#include <assert.h>
#include "kernel_types.h"
#include "ndb_limits.h"

#define JAM_FILE_ID 217

#define NDBMT_REAL_BLOCK_BITS 6
#define FIRST_BLOCK 244

/*
 * In multithreaded kernel, BlockNumber includes the main block
 * number in lower 9 bits and the instance in upper 7 bits.
 *
 * The new functions that end with Wide support more than 128
 * instances. This is is handled by using a part of the bits
 * in the main part. The main part can be up to 9 bits, so
 * by restricting these to only in reality support 6 bits we
 * free up 3 bits for more instances supported. At the same time
 * we want all the code that is used to handle signals that never
 * goes to higher instance numbers than 128 to not be distracted
 * by this.
 *
 * This is solved by ensuring that we get those 3 bits by some
 * mathematical engineering. This provides backward compatible
 * block references as long as no more than 128 instances are
 * used.
 *
 * If block numbers were starting at 0 and was able to go up to
 * 63 it would be easy enough to simply remove 3 bits from the
 * block number and use those for high instance bits. What we
 * conclude now is that block numbers instead are placed between
 * 244 and 307. To handle this we transform the numbers by
 * adding 512 - 244 and after that do a bitwise AND with 511.
 * This means that 244 to 307 is transformed to 0 to 63. Thus
 * we have then transformed things to where we can simply get
 * the high instance bits by moving them down 6 bits and then
 * multiplying by 128 (by shifting 7 steps to the left).
 *
 * We introduce a unit test to ensure that the calculations
 * are correct.
 */

inline BlockNumber blockToMain_old(Uint32 block) {
  assert(block < (1 << 16));
  return (BlockNumber)(block & ((1 << NDBMT_BLOCK_BITS) - 1));
}

inline BlockInstance blockToInstance_old(Uint32 block) {
  assert(block < (1 << 16));
  return (BlockNumber)(block >> NDBMT_BLOCK_BITS);
}

inline BlockNumber numberToBlock_old(Uint32 main, Uint32 instance) {
  assert(main < (1 << NDBMT_BLOCK_BITS) &&
         instance < (1 << NDBMT_BLOCK_INSTANCE_BITS));
  return (BlockNumber)(main | (instance << NDBMT_BLOCK_BITS));
}

inline BlockReference numberToRef_old(Uint32 main, Uint32 instance,
                                      Uint32 node) {
  assert(node < (1 << 16) && main < (1 << NDBMT_BLOCK_BITS) &&
         instance < (1 << NDBMT_BLOCK_INSTANCE_BITS));
  return (BlockReference)(node | (main << 16) |
                          (instance << (16 + NDBMT_BLOCK_BITS)));
}

inline BlockNumber blockToMain(Uint32 block) {
  assert(block < (1 << 16));
  assert(FIRST_BLOCK + 12 == 256);
  Uint32 block_part = block & ((1 << NDBMT_BLOCK_BITS) - 1);
  if (unlikely(block_part < FIRST_BLOCK)) {
    if (block_part == 0) {
      return 0;
    }
    block_part--;
  }
  Uint32 transformer = (1 << NDBMT_BLOCK_BITS) - FIRST_BLOCK;
  block = (block_part + transformer) & ((1 << NDBMT_BLOCK_BITS) - 1);
  block &= ((1 << NDBMT_REAL_BLOCK_BITS) - 1);
  block = block + FIRST_BLOCK;
  return block;
}

/**
 * Supports up to 1024 instances by limiting the number of main blocks
 * to 64. This means that we are allowed to create new blocks in the
 * range from 244 == BACKUP up to 307. At the moment the highest kernel
 * block is TRPMAN at 266.
 */
inline BlockInstance blockToInstance(Uint32 block) {
  assert(block < (1 << 16));
  Uint32 instance = (BlockNumber)(block >> NDBMT_BLOCK_BITS);
  Uint32 block_part = block & ((1 << NDBMT_BLOCK_BITS) - 1);
  if (unlikely(block_part < FIRST_BLOCK)) {
    if (block_part == 0) {
      return instance;
    }
    block_part--;
  }
  Uint32 transformer = (1 << NDBMT_BLOCK_BITS) - FIRST_BLOCK;
  block = (block + transformer) & ((1 << NDBMT_BLOCK_BITS) - 1);
  Uint32 instance_upper_bits = block >> NDBMT_REAL_BLOCK_BITS;
  instance_upper_bits <<= NDBMT_BLOCK_INSTANCE_BITS;
  instance += instance_upper_bits;
  return instance;
}

inline BlockNumber numberToBlock(Uint32 main, Uint32 instance) {
  assert(main >= FIRST_BLOCK);
  assert(main < FIRST_BLOCK + (1 << NDBMT_REAL_BLOCK_BITS));
  assert(instance < NDBMT_MAX_INSTANCES);
  Uint32 low_instance_bits = instance & ((1 << NDBMT_BLOCK_INSTANCE_BITS) - 1);
  Uint32 high_instance_bits = instance >> NDBMT_BLOCK_INSTANCE_BITS;
  Uint32 base_block = main - FIRST_BLOCK;
  Uint32 block_part =
      base_block + (high_instance_bits << NDBMT_REAL_BLOCK_BITS);
  block_part += FIRST_BLOCK;
  block_part &= ((1 << NDBMT_BLOCK_BITS) - 1);
  if (unlikely(block_part < FIRST_BLOCK)) {
    block_part++;  // We are not using block_part == 0 */
    assert(block_part != FIRST_BLOCK);
  }
  return (BlockNumber)(block_part | (low_instance_bits << NDBMT_BLOCK_BITS));
}

/**
 * Convert BlockReference to NodeId
 */
inline NodeId refToNode(Uint32 ref) { return (NodeId)(ref & ((1 << 16) - 1)); }

/**
 * Convert BlockReference to full 16-bit BlockNumber.
 */
inline BlockNumber refToBlock(Uint32 ref) { return (BlockNumber)(ref >> 16); }

/**
 * Convert BlockReference to main BlockNumber.
 * Used in tests such as: refToMain(senderRef) == DBTC.
 */
inline BlockNumber refToMain(Uint32 ref) { return blockToMain(ref >> 16); }

/**
 * Convert BlockReference to BlockInstance.
 */
inline BlockInstance refToInstance(Uint32 ref) {
  return blockToInstance(ref >> 16);
}

/**
 * Convert NodeId and BlockNumber to BlockReference
 */
inline BlockReference numberToRef(Uint32 block, Uint32 node) {
  assert(node < (1 << 16) && block < (1 << 16));
  return (BlockReference)(node | (block << 16));
}

/**
 * Convert NodeId and block main and instance to BlockReference
 */
inline BlockReference numberToRef(Uint32 main, Uint32 instance, Uint32 node) {
  return (BlockReference)(node | (numberToBlock(main, instance) << 16));
}

#undef JAM_FILE_ID

#endif
