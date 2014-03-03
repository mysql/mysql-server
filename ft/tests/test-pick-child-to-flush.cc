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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"

#include <ft-cachetable-wrappers.h>

#include "ft-flusher.h"
#include "ft-flusher-internal.h"
#include "checkpoint.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
FT_HANDLE t;
const char *fname = TOKU_TEST_FILENAME;

int curr_child_to_flush;
int num_flushes_called;

static int child_to_flush(FT UU(h), FTNODE parent, void* UU(extra)) {
    // internal node has 2 children
    if (parent->height == 1) {
        assert(parent->n_children == 2);
        return curr_child_to_flush;
    }
    // root has 1 child
    else if (parent->height == 2) {
        assert(parent->n_children == 1);
        return 0;
    }
    else {
        assert(false);
    }
    return curr_child_to_flush;
}

static void update_status(FTNODE UU(child), int UU(dirtied), void* UU(extra)) {
    num_flushes_called++;
}



static bool
dont_destroy_bn(void* UU(extra))
{
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

static bool recursively_flush_should_not_happen(FTNODE UU(child), void* UU(extra)) {
    assert(false);
}

static bool always_flush(FTNODE UU(child), void* UU(extra)) {
    return true;
}


static void
doit (void) {
    BLOCKNUM node_internal, node_root;
    BLOCKNUM node_leaf[2];
    int r;
    
    toku_cachetable_create(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, NODESIZE, NODESIZE/2, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaf[0], 1, NULL, NULL);
    assert(r==0);
    r = toku_testsetup_leaf(t, &node_leaf[1], 1, NULL, NULL);
    assert(r==0);

    char* pivots[1];
    pivots[0] = toku_strdup("kkkkk");
    int pivot_len = 6;
    r = toku_testsetup_nonleaf(t, 1, &node_internal, 2, node_leaf, pivots, &pivot_len);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 2, &node_root, 1, &node_internal, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);

    char filler[900-2*bn_data::HEADER_LENGTH];
    memset(filler, 0, sizeof(filler));
    // now we insert filler data so that a merge does not happen
    r = toku_testsetup_insert_to_leaf (
        t, 
        node_leaf[0], 
        "b", // key
        2, // keylen
        filler, 
        sizeof(filler)
        );
    assert(r==0);
    r = toku_testsetup_insert_to_leaf (
        t, 
        node_leaf[1], 
        "y", // key
        2, // keylen
        filler, 
        sizeof(filler)
        );
    assert(r==0);

    // make buffers in internal node non-empty
    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_internal, 
        FT_INSERT, 
        "a",
        2,
        NULL,
        0
        );
    assert_zero(r);
    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_internal, 
        FT_INSERT, 
        "z",
        2,
        NULL,
        0
        );
    assert_zero(r);
    
    //
    // now run a checkpoint to get everything clean
    //
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);

    // now with setup done, start the test
    // test that if toku_ft_flush_some_child properly honors
    // what we say and flushes the child we pick
    FTNODE node = NULL;
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    toku_assert_entire_node_in_memory(node);
    assert(node->n_children == 2);
    assert(!node->dirty);
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) > 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) > 0);

    struct flusher_advice fa;
    flusher_advice_init(
        &fa,
        child_to_flush,
        dont_destroy_bn,
        recursively_flush_should_not_happen,
        merge_should_not_happen,
        update_status,
	default_pick_child_after_split,
        NULL
        );
    curr_child_to_flush = 0;
    num_flushes_called = 0;
    toku_ft_flush_some_child(t->ft, node, &fa);
    assert(num_flushes_called == 1);

    toku_pin_node_with_min_bfe(&node, node_internal, t);
    toku_assert_entire_node_in_memory(node);
    assert(node->dirty);
    assert(node->n_children == 2);
    // child 0 should have empty buffer because it flushed
    // child 1 should still have message in buffer
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) == 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) > 0);
    toku_unpin_ftnode(t->ft, node);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(!node->dirty);
    curr_child_to_flush = 1;
    num_flushes_called = 0;
    toku_ft_flush_some_child(t->ft, node, &fa);
    assert(num_flushes_called == 1);
    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(node->dirty);
    toku_assert_entire_node_in_memory(node);
    assert(node->n_children == 2);
    // both buffers should be empty now
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) == 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) == 0);
    // now let's do a flush with an empty buffer, make sure it is ok
    toku_unpin_ftnode(t->ft, node);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(!node->dirty);
    curr_child_to_flush = 0;
    num_flushes_called = 0;
    toku_ft_flush_some_child(t->ft, node, &fa);
    assert(num_flushes_called == 1);

    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(node->dirty); // nothing was flushed, but since we were trying to flush to a leaf, both become dirty
    toku_assert_entire_node_in_memory(node);
    assert(node->n_children == 2);
    // both buffers should be empty now
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) == 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) == 0);
    toku_unpin_ftnode(t->ft, node);

    // now let's start a flush from the root, that always recursively flushes    
    flusher_advice_init(
        &fa,
        child_to_flush,
        dont_destroy_bn,
        always_flush,
        merge_should_not_happen,
        update_status,
	default_pick_child_after_split,
        NULL
        );
    // use a for loop so to get us down both paths
    for (int i = 0; i < 2; i++) {
        toku_pin_node_with_min_bfe(&node, node_root, t);
        toku_assert_entire_node_in_memory(node); // entire root is in memory
        curr_child_to_flush = i;
        num_flushes_called = 0;
        toku_ft_flush_some_child(t->ft, node, &fa);
        assert(num_flushes_called == 2);
    
        toku_pin_node_with_min_bfe(&node, node_internal, t);
        assert(node->dirty);
        toku_unpin_ftnode(t->ft, node);
        toku_pin_node_with_min_bfe(&node, node_leaf[0], t);
        assert(node->dirty);
        toku_unpin_ftnode(t->ft, node);
        toku_pin_node_with_min_bfe(&node, node_leaf[1], t);
        if (i == 0) {
            assert(!node->dirty);
        }
        else {
            assert(node->dirty);
        }
        toku_unpin_ftnode(t->ft, node);
    }

    // now one more test to show a bug was fixed
    // if there is nothing to flush from parent to child,
    // and child is not fully in memory, we used to crash
    // so, to make sure that is fixed, let's get internal to not
    // be fully in memory, and make sure the above test works
    
    // a hack to get internal compressed
    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_internal, 
        FT_INSERT, 
        "c",
        2,
        NULL,
        0
        );
    assert_zero(r);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    for (int i = 0; i < 20; i++) {
        toku_ftnode_pe_callback(node, make_pair_attr(0xffffffff), t->ft, def_pe_finalize_impl, nullptr);
    }
    assert(BP_STATE(node,0) == PT_COMPRESSED);
    toku_unpin_ftnode(t->ft, node);

    //now let's do the same test as above
    toku_pin_node_with_min_bfe(&node, node_root, t);
    toku_assert_entire_node_in_memory(node); // entire root is in memory
    curr_child_to_flush = 0;
    num_flushes_called = 0;
    toku_ft_flush_some_child(t->ft, node, &fa);
    assert(num_flushes_called == 2);
    
    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    toku_cachetable_close(&ct);

    toku_free(pivots[0]);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
