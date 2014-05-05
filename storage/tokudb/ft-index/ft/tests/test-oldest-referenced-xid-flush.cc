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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include "ft-cachetable-wrappers.h"
#include "ft-flusher.h"
#include "ft-flusher-internal.h"

static bool
dont_destroy_bn(void* UU(extra)) {
    return false;
}

static void merge_should_not_happen(struct flusher_advice* UU(fa),
                              FT UU(h),
                              FTNODE UU(parent),
                              int UU(childnum),
                              FTNODE UU(child),
                              void* UU(extra))
{
    assert(false);
}

static bool dont_recursively_flush(FTNODE UU(child), void* UU(extra)) {
    return false;
}

static int child_to_flush(FT UU(h), FTNODE parent, void* UU(extra)) {
    assert(parent->height == 2);
    assert(parent->n_children == 1);
    return 0;
}

static void dummy_update_status(FTNODE UU(child), int UU(dirtied), void* UU(extra)) {
}

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

static void test_oldest_referenced_xid_gets_propogated(void) {
    int r;
    CACHETABLE ct;
    FT_HANDLE t;
    BLOCKNUM grandchild_leaf_blocknum, child_nonleaf_blocknum, root_blocknum;

    toku_cachetable_create(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER);
    unlink("foo1.ft_handle");
    r = toku_open_ft_handle("foo1.ft_handle", 1, &t, NODESIZE, NODESIZE/2, TOKU_DEFAULT_COMPRESSION_METHOD, ct, nullptr, toku_builtin_compare_fun);
    assert(r==0);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    // This test flushes from a nonleaf root to a nonleaf child, without any leaf nodes.

    r = toku_testsetup_leaf(t, &grandchild_leaf_blocknum, 1, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &child_nonleaf_blocknum, 1, &grandchild_leaf_blocknum, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 2, &root_blocknum, 1, &child_nonleaf_blocknum, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_root(t, root_blocknum);
    assert(r==0);

    r = toku_testsetup_insert_to_nonleaf(
        t, 
        root_blocknum, 
        FT_INSERT,
        "a",
        2,
        NULL,
        0
        );

    // Verify that both the root and its child start with TXNID_NONE
    // for the oldest referenced xid

    // first verify the child
    FTNODE node = NULL;
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, t->ft);
    toku_pin_ftnode(
        t->ft,
        child_nonleaf_blocknum,
        toku_cachetable_hash(t->ft->cf, child_nonleaf_blocknum),
        &bfe,
        PL_WRITE_EXPENSIVE,
        &node,
        true
        );
    assert(node->height == 1);
    assert(node->n_children == 1);
    assert(BP_BLOCKNUM(node, 0).b == grandchild_leaf_blocknum.b);
    assert(node->oldest_referenced_xid_known == TXNID_NONE);
    toku_unpin_ftnode(t->ft, node);

    // now verify the root - keep it pinned so we can flush it below
    toku_pin_ftnode(
        t->ft, 
        root_blocknum,
        toku_cachetable_hash(t->ft->cf, root_blocknum),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(node->height == 2);
    assert(node->n_children == 1);
    assert(BP_BLOCKNUM(node, 0).b == child_nonleaf_blocknum.b);
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) > 0);
    assert(node->oldest_referenced_xid_known == TXNID_NONE);

    // set the root's oldest referenced xid to something special
    const TXNID flush_xid = 25000;
    node->oldest_referenced_xid_known = flush_xid;

    // do the flush
    struct flusher_advice fa;
    flusher_advice_init(
        &fa,
        child_to_flush,
        dont_destroy_bn,
        dont_recursively_flush,
        merge_should_not_happen,
        dummy_update_status,
        default_pick_child_after_split,
        NULL
        );
    toku_ft_flush_some_child(t->ft, node, &fa);

    // pin the child, verify that oldest referenced xid was
    // propogated from parent to child during the flush
    toku_pin_ftnode(
        t->ft, 
        child_nonleaf_blocknum,
        toku_cachetable_hash(t->ft->cf, child_nonleaf_blocknum),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(node->oldest_referenced_xid_known == flush_xid);

    toku_unpin_ftnode(t->ft, node);
    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    toku_cachetable_close(&ct);
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    default_parse_args(argc, argv);
    test_oldest_referenced_xid_gets_propogated();
    return 0;
}
