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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#include <string.h>

#include "portability/toku_assert.h"

#include "ft/serialize/block_allocator_strategy.h"

static uint64_t _align(uint64_t value, uint64_t ba_alignment) {
    return ((value + ba_alignment - 1) / ba_alignment) * ba_alignment;
}

static uint64_t _roundup_to_power_of_two(uint64_t value) {
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
           uint64_t max_padding) {
    if (n_blocks == 1) {
        // won't enter loop, can't underflow the direction < 0 case
        return nullptr;
    }

    struct block_allocator::blockpair *bp = &blocks_array[0];
    for (uint64_t n_spaces_to_check = n_blocks - 1; n_spaces_to_check > 0;
         n_spaces_to_check--, bp++) {
        // Consider the space after bp
        uint64_t padded_alignment = max_padding != 0 ? _align(max_padding, alignment) : alignment;
        uint64_t possible_offset = _align(bp->offset + bp->size, padded_alignment);
        if (possible_offset + size <= bp[1].offset) { // bp[1] is always valid since bp < &blocks_array[n_blocks-1]
            invariant(bp - blocks_array < (int64_t) n_blocks);
            return bp;
        }
    }
    return nullptr;
}

static struct block_allocator::blockpair *
_first_fit_bw(struct block_allocator::blockpair *blocks_array,
           uint64_t n_blocks, uint64_t size, uint64_t alignment,
           uint64_t max_padding, struct block_allocator::blockpair *blocks_array_limit) {
    if (n_blocks == 1) {
        // won't enter loop, can't underflow the direction < 0 case
        return nullptr;
    }

    struct block_allocator::blockpair *bp = &blocks_array[-1];
    for (uint64_t n_spaces_to_check = n_blocks - 1; n_spaces_to_check > 0;
         n_spaces_to_check--, bp--) {
        // Consider the space after bp
        uint64_t padded_alignment = max_padding != 0 ? _align(max_padding, alignment) : alignment;
        uint64_t possible_offset = _align(bp->offset + bp->size, padded_alignment);
        if (&bp[1] < blocks_array_limit && possible_offset + size <= bp[1].offset) {
            invariant(blocks_array - bp < (int64_t) n_blocks);
            return bp;
        }
    }
    return nullptr;
}

struct block_allocator::blockpair *
block_allocator_strategy::first_fit(struct block_allocator::blockpair *blocks_array,
                                    uint64_t n_blocks, uint64_t size, uint64_t alignment) {
    return _first_fit(blocks_array, n_blocks, size, alignment, 0);
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

static uint64_t padded_fit_alignment = 4096;

// TODO: These compiler specific directives should be abstracted in a portability header
//       portability/toku_compiler.h?
__attribute__((__constructor__))
static void determine_padded_fit_alignment_from_env(void) {
    // TODO: Should be in portability as 'toku_os_getenv()?'
    const char *s = getenv("TOKU_BA_PADDED_FIT_ALIGNMENT");
    if (s != nullptr && strlen(s) > 0) {
        const int64_t alignment = strtoll(s, nullptr, 10);
        if (alignment <= 0) {
            fprintf(stderr, "tokuft: error: block allocator padded fit alignment found in environment (%s), "
                            "but it's out of range (should be an integer > 0). defaulting to %" PRIu64 "\n",
                            s, padded_fit_alignment);
        } else {
            padded_fit_alignment = _roundup_to_power_of_two(alignment);
            fprintf(stderr, "tokuft: setting block allocator padded fit alignment to %" PRIu64 "\n",
                    padded_fit_alignment);
        }
    }
}

// First fit into a block that is oversized by up to max_padding.
// The hope is that if we purposefully waste a bit of space at allocation
// time we'll be more likely to reuse this block later.
struct block_allocator::blockpair *
block_allocator_strategy::padded_fit(struct block_allocator::blockpair *blocks_array,
                                     uint64_t n_blocks, uint64_t size, uint64_t alignment) {
    return _first_fit(blocks_array, n_blocks, size, alignment, padded_fit_alignment);
}

static double hot_zone_threshold = 0.85;

// TODO: These compiler specific directives should be abstracted in a portability header
//       portability/toku_compiler.h?
__attribute__((__constructor__))
static void determine_hot_zone_threshold_from_env(void) {
    // TODO: Should be in portability as 'toku_os_getenv()?'
    const char *s = getenv("TOKU_BA_HOT_ZONE_THRESHOLD");
    if (s != nullptr && strlen(s) > 0) {
        const double hot_zone = strtod(s, nullptr);
        if (hot_zone < 1 || hot_zone > 99) {
            fprintf(stderr, "tokuft: error: block allocator hot zone threshold found in environment (%s), "
                            "but it's out of range (should be an integer 1 through 99). defaulting to 85\n", s);
            hot_zone_threshold = 85 / 100;
        } else {
            fprintf(stderr, "tokuft: setting block allocator hot zone threshold to %s\n", s);
            hot_zone_threshold = hot_zone / 100;
        }
    }
}

struct block_allocator::blockpair *
block_allocator_strategy::heat_zone(struct block_allocator::blockpair *blocks_array,
                                    uint64_t n_blocks, uint64_t size, uint64_t alignment,
                                    uint64_t heat) {
    if (heat > 0) {
        struct block_allocator::blockpair *bp, *boundary_bp;

        // Hot allocation. Find the beginning of the hot zone.
        boundary_bp = &blocks_array[n_blocks - 1];
        uint64_t highest_offset = _align(boundary_bp->offset + boundary_bp->size, alignment);
        uint64_t hot_zone_offset = static_cast<uint64_t>(hot_zone_threshold * highest_offset);

        boundary_bp = std::lower_bound(blocks_array, blocks_array + n_blocks, hot_zone_offset);
        uint64_t blocks_in_zone = (blocks_array + n_blocks) - boundary_bp;
        uint64_t blocks_outside_zone = boundary_bp - blocks_array;
        invariant(blocks_in_zone + blocks_outside_zone == n_blocks);

        if (blocks_in_zone > 0) {
            // Find the first fit in the hot zone, going forward.
            bp = _first_fit(boundary_bp, blocks_in_zone, size, alignment, 0);
            if (bp != nullptr) {
                return bp;
            }
        }
        if (blocks_outside_zone > 0) {
            // Find the first fit in the cold zone, going backwards.
            bp = _first_fit_bw(boundary_bp, blocks_outside_zone, size, alignment, 0, &blocks_array[n_blocks]);
            if (bp != nullptr) {
                return bp;
            }
        }
    } else {
        // Cold allocations are simply first-fit from the beginning.
        return _first_fit(blocks_array, n_blocks, size, alignment, 0);
    }
    return nullptr;
}
