/*
   Copyright (C) 2003-2006 MySQL AB
    Use is subject to license terms.

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

#ifndef BUDDY_H
#define BUDDY_H

#include <ndb_global.h>

typedef unsigned int Uint32;
typedef unsigned short Uint16;
typedef unsigned long long Uint64;

//
const int UNDEFINED_CHUNK = -2; // XXX Set to hex

//
const int END_OF_CHUNK_LIST = -1; // XXX Set to hex

// A timeout (no of seconds) for the memory segments in the TransporterRegistry 
// memory pool. If a segment has been occupied (free=false) for a longer period 
// than this timeout, it will be released.
const int ALLOCATION_TIMEOUT = 10000;

// Free segments should always be as large as possible
// and are only allowed to be in any of these sizes
enum FreeSegmentSize {
  sz_256  = 0,
  sz_512  = 1,
  sz_1024 = 2,
  sz_2048 = 3,
  sz_4096 = 4,
  sz_8192 = 5,
  sz_16384 = 6,
  sz_32768 = 7,
  sz_65536 = 8,
  sz_131072 = 9,
  sz_GET_MAX = 5,
  sz_MAX = 9
};

struct Segment;

class BuddyMemory {
public:

  // Return true if there is at least 8 kB memory available
  bool memoryAvailable();

  // 
  bool allocate(int nChunksToAllocate);

  // Remove the segment from the freeSegment list
  void removeFromFreeSegmentList(int sz, int index);
  
  // Release the segment of size
  void release(int releaseId, int size);
  
  // Add a segment to the freeSegment list
  void addToFreeSegmentList(int sz, int index);

  bool getSegment(Uint32 size, Segment * dst);

  void refreshTime(Uint32 time);
  
  //Calculate log2(arg) + 1
  Uint32 logTwoPlus(Uint32 arg);
  
  // The current time
  Uint32 currentTime;
  
  // Pointer to the first free segment of size FreeSegmentSize
  Uint32 freeSegment[sz_MAX];
  
  // Start address of the memory block allocated
  Uint32* startOfMemoryBlock;
  
  // Total number of 256 byte chunks.
  Uint32 totalNoOfChunks;
  
  // Array of 256-byte chunks
  struct Chunk256* chunk;
};

struct Segment {
  Uint32 segmentSize;    // Size of the segment in no of words
  Uint16 index;          // Index in the array of SegmentListElements
  Uint16 releaseId;      // Unique no used when releasing the segment
                         // Undefined if Long_signal.deallocIndicator==0
  union {
    Uint32* segmentAddress; // Address to the memory segment
    Uint64  _padding_NOT_TO_BE_USED_;
  };
};

struct Chunk256 {
  Uint32 allocationTimeStamp;   // Bit 0 represents if the segment is free or not
                                // Bit 1-31 is the allocation time for the segment
                                // Bit 1-31 are undefined if the segment is free 
  Uint32 nextSegmentOfSameSize; // Undefined if allocated. 
                                // The first chunk in a free segment has a valid 
                                // next-pointer. In the rest of the chunks 
                                // belonging to the segment it is UNDEFINED_CHUNK.
  Uint32 prevSegmentOfSameSize; // Undefined if allocated
                                // The first chunk in a free segment has a valid 
                                // prev-pointer. In the rest of the chunks 
                                // belonging to the segment it is UNDEFINED_CHUNK.

  void setFree(bool free);

  bool getFree();

  void setAllocationTimeStamp(Uint32 cTime);

  Uint32 getAllocationTimeStamp();  
};

// inline void Chunk256::setFree(bool free){
//   // Bit 0 of allocationTimeStamp represents if the segment is free or not
//   allocationTimeStamp = 0x0;

//   printf("\nSet free segment");
//   Uint32 offMask = 0x0; // A mask to set the 0 bit to 0
//   if(free) 
//     // Set this bit to 0, if segment should be free
//     allocationTimeStamp = allocationTimeStamp & offMask;
// }

// inline bool Chunk256::getFree(){
//   // Get free segment

//   allocationTimeStamp = 0x0;
//   Uint32 offMask = 0x0; 

//   printf("\nGet free segment"); 
//   return ((allocationTimeStamp | offMask) == offMask ? true : false);
// }

// inline void Chunk256::setAllocationTimeStamp(Uint32 cTime){
//   // Bits 1-31 of allocationTimeStamp represent the allocation time for segment

//   Uint32 onMask = 0x80000000; // A mask to set the 0 bit to 1
//   allocationTimeStamp = 0x0;

//   printf("\nSet allocation time");
  
//   allocationTimeStamp = onMask | cTime;
// }

// inline Uint32 Chunk256::getAllocationTimeStamp(){

//   Uint32 onMask = 0x80000000; // A mask to set the 0 bit to 1
//   allocationTimeStamp = 0x0;

//   printf("\nGet allocation time");
//   allocationTimeStamp = allocationTimeStamp ^ onMask;
//   return allocationTimeStamp;
// };

#endif
