/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef REFCONVERT_H
#define REFCONVERT_H

#include <assert.h>
#include "kernel_types.h"

/*
 * In multithreaded kernel, BlockNumber includes the main block
 * number in lower 9 bits and the instance in upper 7 bits.
 */

inline
BlockNumber blockToMain(Uint32 block){
  assert(block < (1 << 16));
  return (BlockNumber)(block & ((1 << 9) - 1));
}

inline
BlockInstance blockToInstance(Uint32 block){
  assert(block < (1 << 16));
  return (BlockNumber)(block >> 9);
}

inline
BlockNumber numberToBlock(Uint32 main, Uint32 instance)
{
  assert(main < (1 << 9) && instance < (1 << (16 - 9)));
  return (BlockNumber)(main | (instance << 9));
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
  return (BlockNumber)((ref >> 16) & ((1 << 9) - 1));
}

/**
 * Convert BlockReference to BlockInstance.
 */
inline
BlockInstance refToInstance(Uint32 ref){
  return (BlockInstance)(ref >> (16 + 9));
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
  assert(node < (1 << 16) && main < (1 << 9) && instance < (1 << (16 - 9)));
  return (BlockReference)(node | (main << 16) | (instance << (16 + 9)));
}

#endif

