/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef BLOCK_ALLOCATOR_H
#define  BLOCK_ALLOCATOR_H

#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "fttypes.h"


#define BLOCK_ALLOCATOR_ALIGNMENT 4096
// How much must be reserved at the beginning for the block?
//  The actual header is 8+4+4+8+8_4+8+ the length of the db names + 1 pointer for each root.
//  So 4096 should be enough.
#define BLOCK_ALLOCATOR_HEADER_RESERVE 4096
#if (BLOCK_ALLOCATOR_HEADER_RESERVE % BLOCK_ALLOCATOR_ALIGNMENT) != 0
#error
#endif

// Block allocator.
// Overview: A block allocator manages the allocation of variable-sized blocks.
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


#define BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE (2*BLOCK_ALLOCATOR_HEADER_RESERVE)

typedef struct block_allocator *BLOCK_ALLOCATOR;

void create_block_allocator (BLOCK_ALLOCATOR * ba, uint64_t reserve_at_beginning, uint64_t alignment);
// Effect: Create a block allocator, in which the first RESERVE_AT_BEGINNING bytes are not put into a block.
//  All blocks be start on a multiple of ALIGNMENT.
//  Aborts if we run out of memory.
// Parameters
//  ba (OUT):                        Result stored here.
//  reserve_at_beginning (IN)        Size of reserved block at beginning.  This size does not have to be aligned.
//  alignment (IN)                   Block alignment.

void destroy_block_allocator (BLOCK_ALLOCATOR *ba);
// Effect: Destroy a block allocator at *ba.
//  Also, set *ba=NULL.
// Rationale:  If there was only one copy of the pointer, this kills that copy too.
// Paramaters:
//  ba (IN/OUT):


void block_allocator_alloc_block_at (BLOCK_ALLOCATOR ba, uint64_t size, uint64_t offset);
// Effect: Allocate a block of the specified size at a particular offset.
//  Aborts if anything goes wrong.
//  The performance of this function may be as bad as Theta(N), where N is the number of blocks currently in use.
// Usage note: To allocate several blocks (e.g., when opening a BRT),  use block_allocator_alloc_blocks_at().
// Requires: The resulting block may not overlap any other allocated block.
//  And the offset must be a multiple of the block alignment.
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  size (IN):   The size of the block.
//  offset (IN): The location of the block.


struct block_allocator_blockpair {
    uint64_t offset;
    uint64_t size;
};
void block_allocator_alloc_blocks_at (BLOCK_ALLOCATOR ba, uint64_t n_blocks, struct block_allocator_blockpair *pairs);
// Effect: Take pairs in any order, and add them all, as if we did block_allocator_alloc_block() on each pair.
//  This should run in time O(N + M log M) where N is the number of blocks in ba, and M is the number of new blocks.
// Modifies: pairs (sorts them).

void block_allocator_alloc_block (BLOCK_ALLOCATOR ba, uint64_t size, uint64_t *offset);
// Effect: Allocate a block of the specified size at an address chosen by the allocator.
//  Aborts if anything goes wrong.
//  The block address will be a multiple of the alignment.
// Parameters:
//  ba (IN/OUT):  The block allocator.   (Modifies ba.)
//  size (IN):    The size of the block.  (The size does not have to be aligned.)
//  offset (OUT): The location of the block.

void block_allocator_free_block (BLOCK_ALLOCATOR ba, uint64_t offset);
// Effect: Free the block at offset.
// Requires: There must be a block currently allocated at that offset.
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  offset (IN): The offset of the block.


uint64_t block_allocator_block_size (BLOCK_ALLOCATOR ba, uint64_t offset);
// Effect: Return the size of the block that starts at offset.
// Requires: There must be a block currently allocated at that offset.
// Parameters:
//  ba (IN/OUT): The block allocator.  (Modifies ba.)
//  offset (IN): The offset of the block.

void block_allocator_validate (BLOCK_ALLOCATOR ba);
// Effect: Check to see if the block allocator is OK.  This may take a long time.
// Usage Hints: Probably only use this for unit tests.

void block_allocator_print (BLOCK_ALLOCATOR ba);
// Effect: Print information about the block allocator.
// Rationale: This is probably useful only for debugging.

uint64_t block_allocator_allocated_limit (BLOCK_ALLOCATOR ba);
// Effect: Return the unallocated block address of "infinite" size.
//  That is, return the smallest address that is above all the allocated blocks.
// Rationale: When writing the root FIFO we don't know how big the block is.
//  So we start at the "infinite" block, write the fifo, and then
//  allocate_block_at of the correct size and offset to account for the root FIFO.

int block_allocator_get_nth_block_in_layout_order (BLOCK_ALLOCATOR ba, uint64_t b, uint64_t *offset, uint64_t *size);
// Effect: Consider the blocks in sorted order.  The reserved block at the beginning is number 0.  The next one is number 1 and so forth.
//  Return the offset and size of the block with that number.
//  Return 0 if there is a block that big, return nonzero if b is too big.
// Rationale: This is probably useful only for tests.

void block_allocator_get_unused_statistics(BLOCK_ALLOCATOR ba, TOKU_DB_FRAGMENTATION report);
// Effect:  Fill in report to indicate how the file is used.
// Requires: 
//  report->file_size_bytes is filled in
//  report->data_bytes is filled in
//  report->checkpoint_bytes_additional is filled in

void block_allocator_merge_blockpairs_into (uint64_t d,       struct block_allocator_blockpair dst[/*d*/],
				       uint64_t s, const struct block_allocator_blockpair src[/*s*/]);
// Effect: Merge dst[d] and src[s] into dst[d+s], merging in place.
//   Initially dst and src hold sorted arrays (sorted by increasing offset).
//   Finally dst contains all d+s elements sorted in order.
// Requires: 
//   dst and src are sorted.
//   dst must be large enough.
//   No blocks may overlap.
// Rationale: This is exposed so it can be tested by a glass box tester.  Otherwise it would be static (file-scope) function inside block_allocator.c


#endif
