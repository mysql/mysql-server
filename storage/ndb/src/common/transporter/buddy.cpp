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

#include "buddy.hpp"

void Chunk256::setFree(bool free){
  // Bit 0 of allocationTimeStamp represents if the segment is free or not
  Uint32 offMask = 0x0; // A mask to set the 0 bit to 0
  allocationTimeStamp = 0x0;
  if(free) 
    // Set this bit to 0, if segment should be free
    allocationTimeStamp = allocationTimeStamp & offMask;
}

bool Chunk256::getFree(){
  Uint32 offMask = 0x0; 
  return ((allocationTimeStamp | offMask) == offMask ? true : false);
}

void Chunk256::setAllocationTimeStamp(Uint32 cTime){
  // Bits 1-31 of allocationTimeStamp represent the allocation time for segment
  
  // printf("\nSet allocation time. Current time %d", cTime);
  Uint32 onMask = 0x80000000; // A mask to set the 0 bit to 1
  allocationTimeStamp = 0x0;
  allocationTimeStamp = onMask | cTime;
}

Uint32 Chunk256::getAllocationTimeStamp(){
  Uint32 onMask = 0x80000000;
  allocationTimeStamp = allocationTimeStamp ^ onMask;
  printf("\nGet allocation time. Time is %d", allocationTimeStamp);
  return allocationTimeStamp;
};

bool BuddyMemory::allocate(int nChunksToAllocate) {
  
  // Allocate the memory block needed. This memory is deallocated in the
  // destructor of TransporterRegistry.

  printf("\nAllocating %d chunks...", nChunksToAllocate);

  startOfMemoryBlock = (Uint32*) malloc(256 * nChunksToAllocate);

  if (startOfMemoryBlock == NULL)
    return false;
  
  // Allocate the array of 256-byte chunks
  chunk = new Chunk256[nChunksToAllocate];
  
  // Initialize the chunk-array. Every 8 kB segment consists of 32 chunks.
  // Set all chunks to free and set the prev and next pointer 
  for (int i=0; i < nChunksToAllocate; i++) {
    chunk[i].setFree(true);
    if (i%32 == 0) {  
      // The first chunk in every segment will point to the prev and next segment
      chunk[i].prevSegmentOfSameSize = i-32;
      chunk[i].nextSegmentOfSameSize = i + 32;
      chunk[0].prevSegmentOfSameSize = END_OF_CHUNK_LIST;
      chunk[totalNoOfChunks-32].nextSegmentOfSameSize = END_OF_CHUNK_LIST;
    } else {
      // The rest of the chunks in the segments have undefined prev and next pointers
      chunk[i].prevSegmentOfSameSize = UNDEFINED_CHUNK;
      chunk[i].nextSegmentOfSameSize = UNDEFINED_CHUNK;
    }
  }
  
  // Initialize the freeSegment-pointers
  for (int i=0; i<sz_MAX; i++)
    freeSegment[i] = UNDEFINED_CHUNK;
     
  // There are only 8 kB segments at startup
  freeSegment[sz_8192] = 0;

  for (int i=0; i<sz_MAX; i++)
    printf("\nPointers: %d", freeSegment[i]);

  return true;
}


bool BuddyMemory::getSegment(Uint32 size, Segment * dst) {
  
  // The no of chunks the user asked for
  Uint32 nChunksAskedFor = ceil((double(size)/double(256)));
  int segm;

  printf("\n%d chunks asked for", nChunksAskedFor);

  // It may be that the closest segment size above 
  // nChunksAskedFor*256 is not a size that is available in 
  // the freeSegment-list, i.e. it may not be of FreeSegmentSize.
  int nChunksToAllocate = nChunksAskedFor;

  // Find the FreeSegmentSize closest above nChunksAskedFor
  if ((nChunksToAllocate != 1) && (nChunksToAllocate % 2 != 0))
    nChunksToAllocate++;

  printf("\n%d chunks to allocate", nChunksToAllocate);
  int segmSize = logTwoPlus(nChunksToAllocate) - 1;
  if (size-pow(2,segmSize) > 256) 
    segmSize ++;
  printf("\nSegment size: %f", pow(2,int(8+segmSize)));

  while ((segmSize <= sz_GET_MAX) && (freeSegment[segmSize] == UNDEFINED_CHUNK))
    segmSize++;

  segm = freeSegment[segmSize];
  if (segm != UNDEFINED_CHUNK){
    // Free segment of asked size or larger is found
    
    // Remove the found segment from the freeSegment-list
    removeFromFreeSegmentList(segmSize, segm);

    // Set all chunks to allocated (not free) and set the allocation time
    // for the segment we are about to allocate
    for (int i = segm; i <= segm+nChunksToAllocate; i++) {
      chunk[i].setFree(false);
      chunk[i].setAllocationTimeStamp(currentTime);
    }

    // Before returning the segment, check if it is larger than the segment asked for
    if (nChunksAskedFor < nChunksToAllocate) 
      release(nChunksAskedFor, nChunksToAllocate - nChunksAskedFor - 1);
    
    Segment segment;
    segment.segmentAddress = startOfMemoryBlock+(segm * 256);
    segment.segmentSize = 256 * nChunksAskedFor;
    segment.releaseId = segm;

    printf("\nSegment: segment address = %d, segment size = %d, release Id = %d", 
	   segment.segmentAddress, segment.segmentSize, segment.releaseId);  

    return true;
  }
  printf("\nNo segments of asked size or larger are found");
  return false;
}

void BuddyMemory::removeFromFreeSegmentList(int sz, int index) {
  // Remove the segment from the freeSegment list

  printf("\nRemoving segment from list...");
  if (index != UNDEFINED_CHUNK) {
    Chunk256 prevChunk;
    Chunk256 nextChunk;
    int prevChunkIndex = chunk[index].prevSegmentOfSameSize;
    int nextChunkIndex = chunk[index].nextSegmentOfSameSize;
  
    if (prevChunkIndex == END_OF_CHUNK_LIST) {
      if (nextChunkIndex == END_OF_CHUNK_LIST)
	// We are about to remove the only element in the list
	freeSegment[sz] = UNDEFINED_CHUNK;
      else {
	// We are about to remove the first element in the list
	nextChunk = chunk[nextChunkIndex];
	nextChunk.prevSegmentOfSameSize = END_OF_CHUNK_LIST;
	freeSegment[sz] = nextChunkIndex;
      }
    } else {
      if (nextChunkIndex == END_OF_CHUNK_LIST) {
	// We are about to remove the last element in the list
	prevChunk = chunk[prevChunkIndex];
	prevChunk.nextSegmentOfSameSize = END_OF_CHUNK_LIST;
      } else {
	// We are about to remove an element in the middle of the list
	prevChunk = chunk[prevChunkIndex];
	nextChunk = chunk[nextChunkIndex];
	prevChunk.nextSegmentOfSameSize = nextChunkIndex;
	nextChunk.prevSegmentOfSameSize = prevChunkIndex;
      }
    }
  }
  for (int i=0; i<sz_MAX; i++)
    printf("\nPointers: %d", freeSegment[i]);
}

void BuddyMemory::release(int releaseId, int size) {

  int nChunksToRelease = (size == 0 ? 1 : ceil(double(size)/double(256)));
  //nChunksToRelease = ceil(double(size)/double(256));
  int startChunk = releaseId;
  int endChunk = releaseId + nChunksToRelease - 1;

  printf("\n%d chunks to release (initially)", nChunksToRelease);
 
  // Set the chunks we are about to release to free
  for (int i = startChunk; i <= endChunk; i++){
    chunk[i].setFree(true);
  }

  // Look at the chunks before the segment we are about to release
  for (int i = releaseId-1; i >= 0; i--) {
    if (!chunk[i].getFree())
      break;
    else {
      startChunk = i;
      nChunksToRelease++;
      // Look at the next-pointer. If it is valid, we have a 
      // chunk that is the start of a free segment. Remove it 
      // from the freeSegment-list.
      if (chunk[i].nextSegmentOfSameSize != UNDEFINED_CHUNK)
	removeFromFreeSegmentList(size, i);
    }
  }
  
  // Look at the chunks after the segment we are about to release
  for (int i = endChunk+1; i <= totalNoOfChunks; i++) {
    if (!chunk[i].getFree())
      break;
    else {
      endChunk = i;
      nChunksToRelease++;
      // Look at the next-pointer. If it is valid, we have a 
      // chunk that is the start of a free segment. Remove it
      // from the free segment list
      if (chunk[i].nextSegmentOfSameSize != UNDEFINED_CHUNK)
	removeFromFreeSegmentList(size, i);
    }
  }
 
  // We have the start and end indexes and total no of free chunks. 
  // Separate the chunks into segments that can be added to the
  // freeSegments-list.
  int restChunk = 0;
  int segmSize; 
  
   printf("\n%d chunks to release (finally)", nChunksToRelease);
  
  segmSize = logTwoPlus(nChunksToRelease) - 1;
  if (segmSize > sz_MAX) {
    segmSize = sz_MAX;
  }
   
  nChunksToRelease = pow(2,segmSize);
  addToFreeSegmentList(nChunksToRelease*256, startChunk);
}

void BuddyMemory::addToFreeSegmentList(int sz, int index) {
  // Add a segment to the freeSegment list
 
  printf("\nAsked to add segment of size %d", sz);

  // Get an index in freeSegment list corresponding to sz size
  int segmSize = logTwoPlus(sz) - 1;
  if (sz - pow(2,segmSize) >= 256) 
    segmSize ++;
  sz = segmSize - 8; 
 
  int nextSegm = freeSegment[sz];

  printf("\nAdding a segment of size %f", pow(2,(8 + sz)));

  freeSegment[sz] = index;
  if (nextSegm == UNDEFINED_CHUNK) {
    // We are about to add a segment to an empty list
    chunk[index].prevSegmentOfSameSize = END_OF_CHUNK_LIST;
    chunk[index].nextSegmentOfSameSize = END_OF_CHUNK_LIST;
  }
  else {
    // Add the segment first in the list
    chunk[index].prevSegmentOfSameSize = END_OF_CHUNK_LIST;
    chunk[index].nextSegmentOfSameSize = nextSegm;
    chunk[nextSegm].prevSegmentOfSameSize = index;
  }

 for (int i=0; i<sz_MAX; i++)
    printf("\nPointers: %d", freeSegment[i]);

}

Uint32 BuddyMemory::logTwoPlus(Uint32 arg) {
  // Calculate log2(arg) + 1
            
  Uint32 resValue;

  arg = arg | (arg >> 8);
  arg = arg | (arg >> 4);
  arg = arg | (arg >> 2);
  arg = arg | (arg >> 1);
  resValue = (arg & 0x5555) + ((arg >> 1) & 0x5555);
  resValue = (resValue & 0x3333) + ((resValue >> 2) & 0x3333);
  resValue = resValue + (resValue >> 4);
  resValue = (resValue & 0xf) + ((resValue >> 8) & 0xf);

  return resValue;
}

bool BuddyMemory::memoryAvailable() {
  // Return true if there is at least 8 kB memory available
  for (int i = sz_8192; i < sz_MAX; i++)
    if (freeSegment[i] != UNDEFINED_CHUNK)
      return true;
  return false;	  
}


void BuddyMemory::refreshTime(Uint32 time) {
  if (time - currentTime > 1000) { 
    // Update current time
    currentTime = time;
    // Go through the chunk-list every second and release 
    // any chunks that have been allocated for too long
    for (int i=0; i<totalNoOfChunks; i++) {
      if ((!chunk[i].getFree()) && 
	  (currentTime-chunk[i].getAllocationTimeStamp() > ALLOCATION_TIMEOUT)) {
	release(i, 256);
	printf("\nChunks hve been allocated for too long");
      }
    } 
  }
}
