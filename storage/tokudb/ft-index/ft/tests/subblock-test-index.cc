/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
// test the sub block index function
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include "test.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "serialize/sub_block.h"

static void
test_sub_block_index(void) {
    if (verbose)
        printf("%s:%d\n", __FUNCTION__, __LINE__);
    
    const int n_sub_blocks = max_sub_blocks;
    struct sub_block sub_block[n_sub_blocks];
    
    size_t max_offset = 0;
    for (int i = 0 ; i < n_sub_blocks; i++) {
        size_t size = i+1;
        sub_block_init(&sub_block[i]);
        sub_block[i].uncompressed_size = size;
        max_offset += size;
    }
    
    int offset_to_sub_block[max_offset];
    for (int i = 0; i < (int) max_offset; i++)
        offset_to_sub_block[i] = -1;

    size_t start_offset = 0;
    for (int i = 0; i < n_sub_blocks; i++) {
        size_t size = sub_block[i].uncompressed_size;
        for (int j = 0; j < (int) (start_offset + size); j++) {
            if (offset_to_sub_block[j] == -1)
                offset_to_sub_block[j] = i;
        }
        start_offset += size;
    }

    int r;
    for (size_t offset = 0; offset < max_offset; offset++) {
        r = get_sub_block_index(n_sub_blocks, sub_block, offset);
        if (verbose)
            printf("%s:%d %u %d\n", __FUNCTION__, __LINE__, (unsigned int) offset, r);
        assert(0 <= r && r < n_sub_blocks);
        assert(r == offset_to_sub_block[offset]);
    }
    
    r = get_sub_block_index(n_sub_blocks, sub_block, max_offset);
    assert(r == -1);
}

int
test_main (int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0)
            verbose++;
    }
    test_sub_block_index();
    return 0;
}
