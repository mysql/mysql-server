/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BLOCK_ALLOCATOR_H
#define  BLOCK_ALLOCATOR_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "brttypes.h"

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
create_block_allocator (BLOCK_ALLOCATOR * ba, u_int64_t reserve_at_beginning);
// Effect: Create a block allocator, in which the first RESERVE_AT_BEGINNING bytes are not put into a block.
//  Aborts if we run out of memory.
// Parameters
//  ba (OUT):                        Result stored here.
//  reserve_at_beginning (IN)        Size of reserved block at beginning.

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
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  size (IN):   The size of the block.
//  offset (IN): The location of the block.
//

void
block_allocator_alloc_block (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t *offset);
// Effect: Allocate a block of the specified size at an address chosen by the allocator.
//  Aborts if anything goes wrong.
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

#endif
