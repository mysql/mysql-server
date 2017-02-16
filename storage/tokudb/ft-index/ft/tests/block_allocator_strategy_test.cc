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

#include "ft/tests/test.h"

#include "ft/serialize/block_allocator_strategy.h"

static const uint64_t alignment = 4096;

static void test_first_vs_best_fit(void) {
    struct block_allocator::blockpair pairs[] = {
        block_allocator::blockpair(1 * alignment, 6 * alignment),
        // hole between 7x align -> 8x align
        block_allocator::blockpair(8 * alignment, 4 * alignment),
        // hole between 12x align -> 16x align
        block_allocator::blockpair(16 * alignment, 1 * alignment),
        block_allocator::blockpair(17 * alignment, 2 * alignment),
        // hole between 19 align -> 21x align
        block_allocator::blockpair(21 * alignment, 2 * alignment),
    };
    const uint64_t n_blocks = sizeof(pairs) / sizeof(pairs[0]);
    
    block_allocator::blockpair *bp;

    // first fit
    bp = block_allocator_strategy::first_fit(pairs, n_blocks, 100, alignment);
    assert(bp == &pairs[0]);
    bp = block_allocator_strategy::first_fit(pairs, n_blocks, 4096, alignment);
    assert(bp == &pairs[0]);
    bp = block_allocator_strategy::first_fit(pairs, n_blocks, 3 * 4096, alignment);
    assert(bp == &pairs[1]);
    bp = block_allocator_strategy::first_fit(pairs, n_blocks, 5 * 4096, alignment);
    assert(bp == nullptr);

    // best fit
    bp = block_allocator_strategy::best_fit(pairs, n_blocks, 100, alignment);
    assert(bp == &pairs[0]);
    bp = block_allocator_strategy::best_fit(pairs, n_blocks, 4100, alignment);
    assert(bp == &pairs[3]);
    bp = block_allocator_strategy::best_fit(pairs, n_blocks, 3 * 4096, alignment);
    assert(bp == &pairs[1]);
    bp = block_allocator_strategy::best_fit(pairs, n_blocks, 5 * 4096, alignment);
    assert(bp == nullptr);
}

static void test_padded_fit(void) {
    struct block_allocator::blockpair pairs[] = {
        block_allocator::blockpair(1 * alignment, 1 * alignment),
        // 4096 byte hole after bp[0]
        block_allocator::blockpair(3 * alignment, 1 * alignment),
        // 8192 byte hole after bp[1]
        block_allocator::blockpair(6 * alignment, 1 * alignment),
        // 16384 byte hole after bp[2]
        block_allocator::blockpair(11 * alignment, 1 * alignment),
        // 32768 byte hole after bp[3]
        block_allocator::blockpair(17 * alignment, 1 * alignment),
        // 116kb hole after bp[4]
        block_allocator::blockpair(113 * alignment, 1 * alignment),
        // 256kb hole after bp[5]
        block_allocator::blockpair(371 * alignment, 1 * alignment),
    };
    const uint64_t n_blocks = sizeof(pairs) / sizeof(pairs[0]);
    
    block_allocator::blockpair *bp;

    // padding for a 100 byte allocation will be < than standard alignment,
    // so it should fit in the first 4096 byte hole.
    bp = block_allocator_strategy::padded_fit(pairs, n_blocks, 4000, alignment);
    assert(bp == &pairs[0]);

    // Even padded, a 12kb alloc will fit in a 16kb hole
    bp = block_allocator_strategy::padded_fit(pairs, n_blocks, 3 * alignment, alignment);
    assert(bp == &pairs[2]);

    // would normally fit in the 116kb hole but the padding will bring it over
    bp = block_allocator_strategy::padded_fit(pairs, n_blocks, 116 * alignment, alignment);
    assert(bp == &pairs[5]);

    bp = block_allocator_strategy::padded_fit(pairs, n_blocks, 127 * alignment, alignment);
    assert(bp == &pairs[5]);
}

int test_main(int argc, const char *argv[]) {
    (void) argc;
    (void) argv;

    test_first_vs_best_fit();
    test_padded_fit();

    return 0;
}
