/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef REFCONVERT_H
#define REFCONVERT_H

#include <assert.h>
#include "kernel_types.h"
#include "ndb_limits.h"

#define JAM_FILE_ID 217


/*
 * In multithreaded kernel, BlockNumber includes the main block
 * number in lower 9 bits and the instance in upper 7 bits.
 */

inline
BlockNumber blockToMain(Uint32 block){
  assert(block < (1 << 16));
  return (BlockNumber)(block & ((1 << NDBMT_BLOCK_BITS) - 1));
}

inline
BlockInstance blockToInstance(Uint32 block){
  assert(block < (1 << 16));
  return (BlockNumber)(block >> NDBMT_BLOCK_BITS);
}

inline
BlockNumber numberToBlock(Uint32 main, Uint32 instance)
{
  assert(main < (1 << NDBMT_BLOCK_BITS) && 
         instance < (1 << NDBMT_BLOCK_INSTANCE_BITS));
  return (BlockNumber)(main | (instance << NDBMT_BLOCK_BITS));
}

/**
 * Convert BlockReference to NodeId
 */
inline
NodeId refToNode(Uint32 ref){
  return (NodeId)(ref & ((1 << 16) - 1));
}

/**
 * Convert BlockReference to full 16-bit BlockNumber.
 */
inline
BlockNumber refToBlock(Uint32 ref){
  return (BlockNumber)(ref >> 16);
}

/**
 * Convert BlockReference to main BlockNumber.
 * Used in tests such as: refToMain(senderRef) == DBTC.
 */
inline
BlockNumber refToMain(Uint32 ref){
  return (BlockNumber)((ref >> 16) & ((1 << NDBMT_BLOCK_BITS) - 1));
}

/**
 * Convert BlockReference to BlockInstance.
 */
inline
BlockInstance refToInstance(Uint32 ref){
  return (BlockInstance)(ref >> (16 + NDBMT_BLOCK_BITS));
}

/**
 * Convert NodeId and BlockNumber to BlockReference
 */
inline 
BlockReference numberToRef(Uint32 block, Uint32 node){
  assert(node < (1 << 16) && block < (1 << 16));
  return (BlockReference)(node | (block << 16));
}

/**
 * Convert NodeId and block main and instance to BlockReference
 */
inline 
BlockReference numberToRef(Uint32 main, Uint32 instance, Uint32 node){
  assert(node < (1 << 16) && 
         main < (1 << NDBMT_BLOCK_BITS) && 
         instance < (1 << NDBMT_BLOCK_INSTANCE_BITS));
  return (BlockReference)(node | 
                          (main << 16) | 
                          (instance << (16 + NDBMT_BLOCK_BITS)));
}


#undef JAM_FILE_ID

#endif

