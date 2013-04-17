/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BLOCK_ALLOCATOR_H
#define  BLOCK_ALLOCATOR_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "brttypes.h"

#define BLOCK_ALLOCATOR_ALIGNMENT 4096
// How much must be reserved at the beginning for the block?
//  The actual header is 8+4+4+8+8_4+8+ the length of the db names + 1 pointer for each root.
//  So 4096 should be enough.
#define BLOCK_ALLOCATOR_HEADER_RESERVE 4096

// A block allocator manages the allocation of variable-sized blocks.
// The translation of block numbers to addresses is handled elsewhere.
// The allocation of block numbers is handled elsewhere.

// We can create a block allocator.
// When creating a block allocator we also specify a certain-sized
// block at the beginning that is preallocated (and cannot be allocated
// or freed)

// We can allocate blocks of a particular size at a particular location.
// We can allocate blocks of a particular size at a location chosen by the allocator.
// We can free blocks.
// We can determine the size of a block.

typedef struct block_allocator *BLOCK_ALLOCATOR;

void
create_block_allocator (BLOCK_ALLOCATOR * ba, u_int64_t reserve_at_beginning, u_int64_t alignment);
// Effect: Create a block allocator, in which the first RESERVE_AT_BEGINNING bytes are not put into a block.
//  All blocks be start on a multiple of ALIGNMENT.
//  Aborts if we run out of memory.
// Parameters
//  ba (OUT):                        Result stored here.
//  reserve_at_beginning (IN)        Size of reserved block at beginning.
//  alignment (IN)                   Block alignment.

void
destroy_block_allocator (BLOCK_ALLOCATOR *ba);
// Effect: Destroy a block allocator at *ba.
//  Also, set *ba=NULL.
// Rationale:  If there was only one copy of the pointer, this kills that copy too.
// Paramaters:
//  ba (IN/OUT):


void
block_allocator_alloc_block_at (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t offset);
// Effect: Allocate a block of the specified size at a particular offset.
//  Aborts if anything goes wrong.
// Requires: The resulting block may not overlap any other allocated block.
//  And the offset must be a multiple of the block alignment.
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  size (IN):   The size of the block.
//  offset (IN): The location of the block.
//

void
block_allocator_alloc_block (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t *offset);
// Effect: Allocate a block of the specified size at an address chosen by the allocator.
//  Aborts if anything goes wrong.
//  The block address will be a multiple of the alignment.
// Parameters:
//  ba (IN/OUT):  The block allocator.   (Modifies ba.)
//  size (IN):    The size of the block.
//  offset (OUT): The location of the block.

void
block_allocator_free_block (BLOCK_ALLOCATOR ba, u_int64_t offset);
// Effect: Free the block at offset.
// Requires: There must be a block currently allocated at that offset.
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  offset (IN): The offset of the block.


u_int64_t
block_allocator_block_size (BLOCK_ALLOCATOR ba, u_int64_t offset);
// Effect: Return the size of the block that starts at offset.
// Requires: There must be a block currently allocated at that offset.
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  offset (IN): The offset of the block.

void
block_allocator_validate (BLOCK_ALLOCATOR ba);
// Effect: Check to see if the block allocator is OK.  This may take a long time.
// Usage Hints: Probably only use this for unit tests.

void
block_allocator_print (BLOCK_ALLOCATOR ba);

u_int64_t
block_allocator_allocated_limit (BLOCK_ALLOCATOR ba);
// Effect: Return the unallocated block address of "infinite" size.
//  That is, return the smallest address that is above all the allocated blocks.
// Rationale: When writing the root FIFO we don't know how big the block is.
//  So we start at the "infinite" block, write the fifo, and then
//  allocate_block_at of the correct size and offset to account for the root FIFO.

#endif
