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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "bndata.h"

static void
le_add_to_bn(bn_data* bn, uint32_t idx, const  char *key, int keysize, const char *val, int valsize)
{
    LEAFENTRY r = NULL;
    uint32_t size_needed = LE_CLEAN_MEMSIZE(valsize);
    void *maybe_free = nullptr;
    bn->get_space_for_insert(
        idx, 
        key,
        keysize,
        size_needed,
        &r,
        &maybe_free
        );
    if (maybe_free) {
        toku_free(maybe_free);
    }
    resource_assert(r);
    r->type = LE_CLEAN;
    r->u.clean.vallen = valsize;
    memcpy(r->u.clean.val, val, valsize);
}

static void
le_overwrite(bn_data* bn, uint32_t idx, const  char *key, int keysize, const char *val, int valsize) {
    LEAFENTRY r = NULL;
    uint32_t size_needed = LE_CLEAN_MEMSIZE(valsize);
    void *maybe_free = nullptr;
    bn->get_space_for_overwrite(
        idx, 
        key,
        keysize,
        size_needed, // old_le_size
        size_needed,
        &r,
        &maybe_free
        );
    if (maybe_free) {
        toku_free(maybe_free);
    }
    resource_assert(r);
    r->type = LE_CLEAN;
    r->u.clean.vallen = valsize;
    memcpy(r->u.clean.val, val, valsize);
}


class bndata_bugfix_test {
public:
    void
    run_test(void) {
        //    struct ft_handle source_ft;
        struct ftnode sn;
    
        // just copy this code from a previous test
        // don't care what it does, just want to get a node up and running
        sn.flags = 0x11223344;
        sn.thisnodename.b = 20;
        sn.layout_version = FT_LAYOUT_VERSION;
        sn.layout_version_original = FT_LAYOUT_VERSION;
        sn.height = 0;
        sn.n_children = 2;
        sn.dirty = 1;
        sn.oldest_referenced_xid_known = TXNID_NONE;
        MALLOC_N(sn.n_children, sn.bp);
        MALLOC_N(1, sn.childkeys);
        toku_memdup_dbt(&sn.childkeys[0], "b", 2);
        sn.totalchildkeylens = 2;
        BP_STATE(&sn,0) = PT_AVAIL;
        BP_STATE(&sn,1) = PT_AVAIL;
        set_BLB(&sn, 0, toku_create_empty_bn());
        set_BLB(&sn, 1, toku_create_empty_bn());
        le_add_to_bn(BLB_DATA(&sn, 0), 0, "a", 2, "aval", 5);
        le_add_to_bn(BLB_DATA(&sn, 0), 1, "b", 2, "bval", 5);
        le_add_to_bn(BLB_DATA(&sn, 1), 0, "x", 2, "xval", 5);
    
    
    
        // now this is the test. If I keep getting space for overwrite
        // like crazy, it should expose the bug
        bn_data* bnd = BLB_DATA(&sn, 0);
        size_t old_size = bnd->m_buffer_mempool.size;
        if (verbose) printf("frag size: %zu\n", bnd->m_buffer_mempool.frag_size);
        if (verbose) printf("size: %zu\n", bnd->m_buffer_mempool.size);
        for (uint32_t i = 0; i < 1000000; i++) {
            le_overwrite(bnd, 0, "a", 2, "aval", 5);
        }
        if (verbose) printf("frag size: %zu\n", bnd->m_buffer_mempool.frag_size);
        if (verbose) printf("size: %zu\n", bnd->m_buffer_mempool.size);
        size_t new_size = bnd->m_buffer_mempool.size;
        // just a crude test to make sure we did not grow unbounded.
        // if this assert ever fails, revisit the code and see what is going
        // on. It may be that some algorithm has changed.
        assert(new_size < 5*old_size);
    
    
        for (int i = 0; i < sn.n_children-1; ++i) {
            toku_free(sn.childkeys[i].data);
        }
        for (int i = 0; i < sn.n_children; i++) {
            destroy_basement_node(BLB(&sn, i));
        }
        toku_free(sn.bp);
        toku_free(sn.childkeys);
    }
};

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    bndata_bugfix_test t;
    t.run_test();
    return 0;
}
