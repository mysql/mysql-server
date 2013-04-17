/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include "includes.h"

// Here's a very simple implementation.
// It's not very fast at allocating or freeing.
// Previous implementation used next_fit, but now use first_fit since we are moving blocks around to reduce file size.

struct blockpair {
    u_int64_t offset;
    u_int64_t size;
};

struct block_allocator {
    u_int64_t reserve_at_beginning; // How much to reserve at the beginning
    u_int64_t alignment;            // Block alignment
    u_int64_t n_blocks; // How many blocks
    u_int64_t blocks_array_size; // How big is the blocks_array.  Must be >= n_blocks.
    struct blockpair *blocks_array; // These blocks are sorted by address.
    u_int64_t n_bytes_in_use; // including the reserve_at_beginning
};

void
block_allocator_validate (BLOCK_ALLOCATOR ba) {
    u_int64_t i;
    u_int64_t n_bytes_in_use = ba->reserve_at_beginning;
    for (i=0; i<ba->n_blocks; i++) {
	n_bytes_in_use += ba->blocks_array[i].size;
	if (i>0) {
	    assert(ba->blocks_array[i].offset >  ba->blocks_array[i-1].offset);
	    assert(ba->blocks_array[i].offset >= ba->blocks_array[i-1].offset + ba->blocks_array[i-1].size );
	}
    }
    assert(n_bytes_in_use == ba->n_bytes_in_use);
}

#if 0
#define VALIDATE(b) block_allocator_validate(b)
#else
#define VALIDATE(b) ((void)0)
#endif

#if 0
void
block_allocator_print (BLOCK_ALLOCATOR ba) {
    u_int64_t i;
    for (i=0; i<ba->n_blocks; i++) {
	printf("%" PRId64 ":%" PRId64 " ", ba->blocks_array[i].offset, ba->blocks_array[i].size);
    }
    printf("\n");
    VALIDATE(ba);
}
#endif

void
create_block_allocator (BLOCK_ALLOCATOR *ba, u_int64_t reserve_at_beginning, u_int64_t alignment) {
    BLOCK_ALLOCATOR XMALLOC(result);
    result->reserve_at_beginning = reserve_at_beginning;
    result->alignment = alignment;
    result->n_blocks = 0;
    result->blocks_array_size = 1;
    XMALLOC_N(result->blocks_array_size, result->blocks_array);
    result->n_bytes_in_use = reserve_at_beginning;
    *ba = result;
    VALIDATE(result);
}

void
destroy_block_allocator (BLOCK_ALLOCATOR *bap) {
    BLOCK_ALLOCATOR ba = *bap;
    *bap = 0;
    toku_free(ba->blocks_array);
    toku_free(ba);
}

static void
grow_blocks_array (BLOCK_ALLOCATOR ba) {
    if (ba->n_blocks >= ba->blocks_array_size) {
	ba->blocks_array_size *= 2;
	XREALLOC_N(ba->blocks_array_size, ba->blocks_array);
    }
}

void
block_allocator_alloc_block_at (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t offset) {
    assert(offset%ba->alignment == 0);
    u_int64_t i;
    VALIDATE(ba);
    assert(offset >= ba->reserve_at_beginning);
    grow_blocks_array(ba);
    // Just do a linear search for the block
    ba->n_bytes_in_use += size;
    for (i=0; i<ba->n_blocks; i++) {
	if (ba->blocks_array[i].offset > offset) {
	    // allocate it in that slot
	    // Don't do error checking, since we require that the blocks don't overlap.
	    // Slide everything over
	    memmove(ba->blocks_array+i+1, ba->blocks_array+i, (ba->n_blocks - i)*sizeof(struct blockpair));
	    ba->blocks_array[i].offset = offset;
	    ba->blocks_array[i].size   = size;
	    ba->n_blocks++;
	    VALIDATE(ba);
	    return;
	}
    }
    // Goes at the end
    ba->blocks_array[ba->n_blocks].offset = offset;
    ba->blocks_array[ba->n_blocks].size   = size;
    ba->n_blocks++;
    VALIDATE(ba);
}

static inline u_int64_t
align (u_int64_t value, BLOCK_ALLOCATOR ba)
// Effect: align a value by rounding up.
{
    return ((value+ba->alignment-1)/ba->alignment)*ba->alignment;
}

void
block_allocator_alloc_block (BLOCK_ALLOCATOR ba, u_int64_t size, u_int64_t *offset) {
    grow_blocks_array(ba);
    ba->n_bytes_in_use += size;
    if (ba->n_blocks==0) {
	assert(ba->n_bytes_in_use == ba->reserve_at_beginning + size); // we know exactly how many are in use
	ba->blocks_array[0].offset = align(ba->reserve_at_beginning, ba);
	ba->blocks_array[0].size  = size;
	*offset = ba->blocks_array[0].offset;
	ba->n_blocks++;
	return;
    }
    // Implement first fit.    
    {
	u_int64_t end_of_reserve = align(ba->reserve_at_beginning, ba);
	if (end_of_reserve + size <= ba->blocks_array[0].offset ) {
	    // Check to see if the space immediately after the reserve is big enough to hold the new block.
	    struct blockpair *bp = &ba->blocks_array[0];
	    memmove(bp+1, bp, (ba->n_blocks)*sizeof(struct blockpair));
	    bp[0].offset = end_of_reserve;
	    bp[0].size   = size;
	    ba->n_blocks++;
	    *offset = end_of_reserve;
	    VALIDATE(ba);
	    return;
	}
    }
    for (u_int64_t blocknum = 0; blocknum +1 < ba->n_blocks; blocknum ++) {
	// Consider the space after blocknum
	struct blockpair *bp = &ba->blocks_array[blocknum];
	u_int64_t this_offset = bp[0].offset;
	u_int64_t this_size   = bp[0].size;
	u_int64_t answer_offset = align(this_offset + this_size, ba);
	if (answer_offset + size > bp[1].offset) continue; // The block we want doesn't fit after this block.
	// It fits, so allocate it here.
	memmove(bp+2, bp+1, (ba->n_blocks - blocknum -1)*sizeof(struct blockpair));
	bp[1].offset = answer_offset;
	bp[1].size   = size;
	ba->n_blocks++;
	*offset = answer_offset;
	VALIDATE(ba);
	return;
    }
    // It didn't fit anywhere, so fit it on the end.
    struct blockpair *bp = &ba->blocks_array[ba->n_blocks];
    u_int64_t answer_offset = align(bp[-1].offset+bp[-1].size, ba);
    bp->offset = answer_offset;
    bp->size   = size;
    ba->n_blocks++;
    *offset = answer_offset;
    VALIDATE(ba);
}

static int64_t
find_block (BLOCK_ALLOCATOR ba, u_int64_t offset)
// Find the index in the blocks array that has a particular offset.  Requires that the block exist.
// Use binary search so it runs fast.
{
    VALIDATE(ba);
    if (ba->n_blocks==1) {
	assert(ba->blocks_array[0].offset == offset);
	return 0;
    }
    u_int64_t lo = 0;
    u_int64_t hi = ba->n_blocks;
    while (1) {
	assert(lo<hi); // otherwise no such block exists.
	u_int64_t mid = (lo+hi)/2;
	u_int64_t thisoff = ba->blocks_array[mid].offset;
	//printf("lo=%" PRId64 " hi=%" PRId64 " mid=%" PRId64 "  thisoff=%" PRId64 " offset=%" PRId64 "\n", lo, hi, mid, thisoff, offset);
	if (thisoff < offset) {
	    lo = mid+1;
	} else if (thisoff > offset) {
	    hi = mid;
	} else {
	    return mid;
	}
    }
}

void
block_allocator_free_block (BLOCK_ALLOCATOR ba, u_int64_t offset) {
    VALIDATE(ba);
    int64_t bn = find_block(ba, offset);
    assert(bn>=0); // we require that there is a block with that offset.  Might as well abort if no such block exists.
    ba->n_bytes_in_use -= ba->blocks_array[bn].size;
    memmove(&ba->blocks_array[bn], &ba->blocks_array[bn+1], (ba->n_blocks-bn-1) * sizeof(struct blockpair));
    ba->n_blocks--;
    VALIDATE(ba);
}

u_int64_t
block_allocator_block_size (BLOCK_ALLOCATOR ba, u_int64_t offset) {
    int64_t bn = find_block(ba, offset);
    assert(bn>=0); // we require that there is a block with that offset.  Might as well abort if no such block exists.
    return ba->blocks_array[bn].size;
}

u_int64_t
block_allocator_allocated_limit (BLOCK_ALLOCATOR ba) {
    if (ba->n_blocks==0) return ba->reserve_at_beginning;
    else {
	struct blockpair *last = &ba->blocks_array[ba->n_blocks-1];
	return last->offset + last->size;
    }
}

int
block_allocator_get_nth_block_in_layout_order (BLOCK_ALLOCATOR ba, u_int64_t b, u_int64_t *offset, u_int64_t *size)
// Effect: Consider the blocks in sorted order.  The reserved block at the beginning is number 0.  The next one is number 1 and so forth.
// Return the offset and size of the block with that number.
// Return 0 if there is a block that big, return nonzero if b is too big.
{
    if (b==0) {
	*offset=0;
	*size  =ba->reserve_at_beginning;
	return  0;
    } else if (b > ba->n_blocks) {
	return -1;
    } else {
	*offset=ba->blocks_array[b-1].offset;
	*size  =ba->blocks_array[b-1].size;
	return 0;
    }
}

void
block_allocator_get_unused_statistics(BLOCK_ALLOCATOR ba, TOKU_DB_FRAGMENTATION report) {
    //Requires: report->file_size_bytes is filled in
    //Requires: report->data_bytes is filled in
    //Requires: report->checkpoint_bytes_additional is filled in

    assert(ba->n_bytes_in_use == report->data_bytes + report->checkpoint_bytes_additional);

    report->unused_bytes         = 0;
    report->unused_blocks        = 0;
    report->largest_unused_block = 0;
    if (ba->n_blocks > 0) {
        //Deal with space before block 0 and after reserve:
        {
            struct blockpair *bp = &ba->blocks_array[0];
            assert(bp->offset >= align(ba->reserve_at_beginning, ba));
            uint64_t free_space = bp->offset - align(ba->reserve_at_beginning, ba);
            if (free_space > 0) {
                report->unused_bytes += free_space;
                report->unused_blocks++;
                if (free_space > report->largest_unused_block) {
                    report->largest_unused_block = free_space;
                }
            }
        }

        //Deal with space between blocks:
        for (u_int64_t blocknum = 0; blocknum +1 < ba->n_blocks; blocknum ++) {
            // Consider the space after blocknum
            struct blockpair *bp = &ba->blocks_array[blocknum];
            uint64_t this_offset = bp[0].offset;
            uint64_t this_size   = bp[0].size;
            uint64_t end_of_this_block = align(this_offset+this_size, ba);
            uint64_t next_offset = bp[1].offset;
            uint64_t free_space  = next_offset - end_of_this_block;
            if (free_space > 0) {
                report->unused_bytes += free_space;
                report->unused_blocks++;
                if (free_space > report->largest_unused_block) {
                    report->largest_unused_block = free_space;
                }
            }
        }

        //Deal with space after last block
        {
            struct blockpair *bp = &ba->blocks_array[ba->n_blocks-1];
            uint64_t this_offset = bp[0].offset;
            uint64_t this_size   = bp[0].size;
            uint64_t end_of_this_block = align(this_offset+this_size, ba);
            if (end_of_this_block < report->file_size_bytes) {
                uint64_t free_space  = report->file_size_bytes - end_of_this_block;
                assert(free_space > 0);
                report->unused_bytes += free_space;
                report->unused_blocks++;
                if (free_space > report->largest_unused_block) {
                    report->largest_unused_block = free_space;
                }
            }
        }
    }
    else {
        //No blocks.  Just the reserve.
        uint64_t end_of_this_block = align(ba->reserve_at_beginning, ba);
        if (end_of_this_block < report->file_size_bytes) {
            uint64_t free_space  = report->file_size_bytes - end_of_this_block;
            assert(free_space > 0);
            report->unused_bytes += free_space;
            report->unused_blocks++;
            if (free_space > report->largest_unused_block) {
                report->largest_unused_block = free_space;
            }
        }
    }
}

