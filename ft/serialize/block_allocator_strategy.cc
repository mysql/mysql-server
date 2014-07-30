/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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
  Copyright (C) 2007-2014 Tokutek, Inc.

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

#include <algorithm>

#include "portability/toku_assert.h"

#include "ft/serialize/block_allocator_strategy.h"

static uint64_t _align(uint64_t value, uint64_t ba_alignment) {
    return ((value + ba_alignment - 1) / ba_alignment) * ba_alignment;
}

static uint64_t _next_power_of_two(uint64_t value) {
    uint64_t r = 4096;
    while (r < value) {
        r *= 2;
        invariant(r > 0);
    }
    return r;
}

// First fit block allocation
static struct block_allocator::blockpair *
_first_fit(struct block_allocator::blockpair *blocks_array,
           uint64_t n_blocks, uint64_t size, uint64_t alignment,
           bool forward, uint64_t max_padding) {
    if (n_blocks == 1) {
        // won't enter loop, can't underflow the direction < 0 case
        return nullptr;
    }

    for (uint64_t n_spaces_to_check = n_blocks - 1,
                  blocknum = forward ? 0 : n_blocks - 2;
         n_spaces_to_check > 0;
         n_spaces_to_check--, forward ? blocknum++ : blocknum--) {
        invariant(blocknum < n_blocks);
        // Consider the space after blocknum
        struct block_allocator::blockpair *bp = &blocks_array[blocknum];
        uint64_t padded_alignment = max_padding != 0 ? _align(max_padding, alignment) : alignment;
        uint64_t possible_offset = _align(bp->offset + bp->size, padded_alignment);
        if (possible_offset + size <= bp[1].offset) {
            return bp;
        }
    }
    return nullptr;
}

struct block_allocator::blockpair *
block_allocator_strategy::first_fit(struct block_allocator::blockpair *blocks_array,
                                    uint64_t n_blocks, uint64_t size, uint64_t alignment) {
    return _first_fit(blocks_array, n_blocks, size, alignment, true, 0);
}

// Best fit block allocation
struct block_allocator::blockpair *
block_allocator_strategy::best_fit(struct block_allocator::blockpair *blocks_array,
                                   uint64_t n_blocks, uint64_t size, uint64_t alignment) {
    struct block_allocator::blockpair *best_bp = nullptr;
    uint64_t best_hole_size = 0;
    for (uint64_t blocknum = 0; blocknum + 1 < n_blocks; blocknum++) {
        // Consider the space after blocknum
        struct block_allocator::blockpair *bp = &blocks_array[blocknum];
        uint64_t possible_offset = _align(bp->offset + bp->size, alignment);
        uint64_t possible_end_offset = possible_offset + size;
        if (possible_end_offset <= bp[1].offset) {
            // It fits here. Is it the best fit?
            uint64_t hole_size = bp[1].offset - possible_end_offset;
            if (best_bp == nullptr || hole_size < best_hole_size) {
                best_hole_size = hole_size;
                best_bp = bp;
            }
        }
    }
    return best_bp;
}

// First fit into a block that is oversized by up to max_padding.
// The hope is that if we purposefully waste a bit of space at allocation
// time we'll be more likely to reuse this block later.
struct block_allocator::blockpair *
block_allocator_strategy::padded_fit(struct block_allocator::blockpair *blocks_array,
                                     uint64_t n_blocks, uint64_t size, uint64_t alignment) {
    static const uint64_t absolute_max_padding = 128 * 1024;
    static const uint64_t desired_fragmentation_divisor = 10;
    uint64_t desired_padding = size / desired_fragmentation_divisor;
    desired_padding = std::min(_next_power_of_two(desired_padding), absolute_max_padding);
    return _first_fit(blocks_array, n_blocks, size, alignment, true, desired_padding);
}

struct block_allocator::blockpair *
block_allocator_strategy::heat_zone(struct block_allocator::blockpair *blocks_array,
                                    uint64_t n_blocks, uint64_t size, uint64_t alignment,
                                    uint64_t heat) {
    if (heat > 0) {
        const double hot_zone_threshold = 0.85;

        // Hot allocation. Find the beginning of the hot zone.
        struct block_allocator::blockpair *bp = &blocks_array[n_blocks - 1];
        uint64_t highest_offset = _align(bp->offset + bp->size, alignment);
        uint64_t hot_zone_offset = static_cast<uint64_t>(hot_zone_threshold * highest_offset);

        bp = std::lower_bound(blocks_array, blocks_array + n_blocks, hot_zone_offset);
        uint64_t blocks_in_zone = (blocks_array + n_blocks) - bp;
        uint64_t blocks_outside_zone = bp - blocks_array;
        invariant(blocks_in_zone + blocks_outside_zone == n_blocks);

        if (blocks_in_zone > 0) {
            // Find the first fit in the hot zone, going forward.
            bp = _first_fit(bp, blocks_in_zone, size, alignment, true, 0);
            if (bp != nullptr) {
                return bp;
            }
        }
        if (blocks_outside_zone > 0) {
            // Find the first fit in the cold zone, going backwards.
            bp = _first_fit(bp, blocks_outside_zone, size, alignment, false, 0);
            if (bp != nullptr) {
                return bp;
            }
        }
    } else {
        // Cold allocations are simply first-fit from the beginning.
        return _first_fit(blocks_array, n_blocks, size, alignment, true, 0);
    }
    return nullptr;
}
