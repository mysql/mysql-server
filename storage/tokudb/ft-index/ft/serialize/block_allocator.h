/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db.h>

#include "portability/toku_pthread.h"
#include "portability/toku_stdint.h"

// Block allocator.
//
// A block allocator manages the allocation of variable-sized blocks.
// The translation of block numbers to addresses is handled elsewhere.
// The allocation of block numbers is handled elsewhere.
//
// When creating a block allocator we also specify a certain-sized
// block at the beginning that is preallocated (and cannot be allocated or freed)
//
// We can allocate blocks of a particular size at a particular location.
// We can allocate blocks of a particular size at a location chosen by the allocator.
// We can free blocks.
// We can determine the size of a block.

class block_allocator {
public:
    static const size_t BLOCK_ALLOCATOR_ALIGNMENT = 4096;

    // How much must be reserved at the beginning for the block?
    //  The actual header is 8+4+4+8+8_4+8+ the length of the db names + 1 pointer for each root.
    //  So 4096 should be enough.
    static const size_t BLOCK_ALLOCATOR_HEADER_RESERVE = 4096;
    
    static_assert(BLOCK_ALLOCATOR_HEADER_RESERVE % BLOCK_ALLOCATOR_ALIGNMENT == 0,
                  "block allocator header must have proper alignment");

    static const size_t BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE = BLOCK_ALLOCATOR_HEADER_RESERVE * 2;

    enum allocation_strategy {
        BA_STRATEGY_FIRST_FIT = 1,
        BA_STRATEGY_BEST_FIT,
        BA_STRATEGY_PADDED_FIT,
        BA_STRATEGY_HEAT_ZONE
    };

    struct blockpair {
        uint64_t offset;
        uint64_t size;
        blockpair(uint64_t o, uint64_t s) :
            offset(o), size(s) {
        }
        int operator<(const struct blockpair &rhs) const {
            return offset < rhs.offset;
        }
        int operator<(const uint64_t &o) const {
            return offset < o;
        }
    };

    // Effect: Create a block allocator, in which the first RESERVE_AT_BEGINNING bytes are not put into a block.
    //         The default allocation strategy is first fit (BA_STRATEGY_FIRST_FIT)
    //  All blocks be start on a multiple of ALIGNMENT.
    //  Aborts if we run out of memory.
    // Parameters
    //  reserve_at_beginning (IN)        Size of reserved block at beginning.  This size does not have to be aligned.
    //  alignment (IN)                   Block alignment.
    void create(uint64_t reserve_at_beginning, uint64_t alignment);

    // Effect: Create a block allocator, in which the first RESERVE_AT_BEGINNING bytes are not put into a block.
    //         The default allocation strategy is first fit (BA_STRATEGY_FIRST_FIT)
    //         The allocator is initialized to contain `n_blocks' of blockpairs, taken from `pairs'
    //  All blocks be start on a multiple of ALIGNMENT.
    //  Aborts if we run out of memory.
    // Parameters
    //  pairs,                           unowned array of pairs to copy
    //  n_blocks,                        Size of pairs array
    //  reserve_at_beginning (IN)        Size of reserved block at beginning.  This size does not have to be aligned.
    //  alignment (IN)                   Block alignment.
    void create_from_blockpairs(uint64_t reserve_at_beginning, uint64_t alignment,
                                struct blockpair *pairs, uint64_t n_blocks);

    // Effect: Destroy this block allocator
    void destroy();

    // Effect: Set the allocation strategy that the allocator should use
    // Requires: No other threads are operating on this block allocator
    void set_strategy(enum allocation_strategy strategy);

    // Effect: Allocate a block of the specified size at an address chosen by the allocator.
    //  Aborts if anything goes wrong.
    //  The block address will be a multiple of the alignment.
    // Parameters:
    //  size (IN):    The size of the block.  (The size does not have to be aligned.)
    //  offset (OUT): The location of the block.
    //  heat (IN):    A higher heat means we should be prepared to free this block soon (perhaps in the next checkpoint)
    //                Heat values are lexiographically ordered (like integers), but their specific values are arbitrary
    void alloc_block(uint64_t size, uint64_t heat, uint64_t *offset);

    // Effect: Free the block at offset.
    // Requires: There must be a block currently allocated at that offset.
    // Parameters:
    //  offset (IN): The offset of the block.
    void free_block(uint64_t offset);

    // Effect: Return the size of the block that starts at offset.
    // Requires: There must be a block currently allocated at that offset.
    // Parameters:
    //  offset (IN): The offset of the block.
    uint64_t block_size(uint64_t offset);

    // Effect: Check to see if the block allocator is OK.  This may take a long time.
    // Usage Hints: Probably only use this for unit tests.
    // TODO: Private?
    void validate() const;

    // Effect: Return the unallocated block address of "infinite" size.
    //  That is, return the smallest address that is above all the allocated blocks.
    uint64_t allocated_limit() const;

    // Effect: Consider the blocks in sorted order.  The reserved block at the beginning is number 0.  The next one is number 1 and so forth.
    //  Return the offset and size of the block with that number.
    //  Return 0 if there is a block that big, return nonzero if b is too big.
    // Rationale: This is probably useful only for tests.
    int get_nth_block_in_layout_order(uint64_t b, uint64_t *offset, uint64_t *size);

    // Effect:  Fill in report to indicate how the file is used.
    // Requires: 
    //  report->file_size_bytes is filled in
    //  report->data_bytes is filled in
    //  report->checkpoint_bytes_additional is filled in
    void get_unused_statistics(TOKU_DB_FRAGMENTATION report);

    // Effect: Fill in report->data_bytes with the number of bytes in use
    //         Fill in report->data_blocks with the number of blockpairs in use
    //         Fill in unused statistics using this->get_unused_statistics()
    // Requires:
    //  report->file_size is ignored on return
    //  report->checkpoint_bytes_additional is ignored on return
    void get_statistics(TOKU_DB_FRAGMENTATION report);

    // Block allocator tracing.
    // - Enabled by setting TOKU_BA_TRACE_PATH to the file that the trace file
    //   should be written to.
    // - Trace may be replayed by ba_trace_replay tool in tools/ directory
    //   eg: "cat mytracefile | ba_trace_replay"
    static void maybe_initialize_trace();
    static void maybe_close_trace();

private:
    void _create_internal(uint64_t reserve_at_beginning, uint64_t alignment);
    void grow_blocks_array_by(uint64_t n_to_add);
    void grow_blocks_array();
    int64_t find_block(uint64_t offset);
    struct blockpair *choose_block_to_alloc_after(size_t size, uint64_t heat);

    // Tracing
    toku_mutex_t _trace_lock;
    void _trace_create(void);
    void _trace_create_from_blockpairs(void);
    void _trace_destroy(void);
    void _trace_alloc(uint64_t size, uint64_t heat, uint64_t offset);
    void _trace_free(uint64_t offset);

    // How much to reserve at the beginning
    uint64_t _reserve_at_beginning;
    // Block alignment
    uint64_t _alignment;
    // How many blocks
    uint64_t _n_blocks;
    // How big is the blocks_array.  Must be >= n_blocks.
    uint64_t _blocks_array_size;
    // These blocks are sorted by address.
    struct blockpair *_blocks_array;
    // Including the reserve_at_beginning
    uint64_t _n_bytes_in_use;
    // The allocation strategy are we using
    enum allocation_strategy _strategy;
};
