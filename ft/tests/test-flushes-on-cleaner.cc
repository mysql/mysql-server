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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"

#include <ft-cachetable-wrappers.h>
#include "ft-flusher.h"
#include "cachetable/checkpoint.h"


static TOKUTXN const null_txn = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
FT_HANDLE ft;
const char *fname = TOKU_TEST_FILENAME;

static int update_func(
    DB* UU(db),
    const DBT* key,
    const DBT* old_val, 
    const DBT* UU(extra),
    void (*set_val)(const DBT *new_val, void *set_extra),
    void *set_extra)
{
    DBT new_val;
    assert(old_val->size > 0);
    if (verbose) {
        printf("applying update to %s\n", (char *)key->data);
    }
    toku_init_dbt(&new_val);
    set_val(&new_val, set_extra);
    return 0;
}


static void
doit (bool keep_other_bn_in_memory) {
    BLOCKNUM node_leaf;
    BLOCKNUM node_internal, node_root;

    int r;
    
    toku_cachetable_create(&ct, 500*1024*1024, ZERO_LSN, nullptr);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &ft, NODESIZE, NODESIZE/2, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    ft->options.update_fun = update_func;
    ft->ft->update_fun = update_func;
    
    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    char* pivots[1];
    pivots[0] = toku_strdup("kkkkk");
    int pivot_len = 6;

    r = toku_testsetup_leaf(ft, &node_leaf, 2, pivots, &pivot_len);
    assert(r==0);

    r = toku_testsetup_nonleaf(ft, 1, &node_internal, 1, &node_leaf, 0, 0);
    assert(r==0);

    r = toku_testsetup_nonleaf(ft, 2, &node_root, 1, &node_internal, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(ft, node_root);
    assert(r==0);

    //
    // at this point we have created a tree with a root, an internal node,
    // and two leaf nodes, the pivot being "kkkkk"
    //

    // now we insert a row into each leaf node
    r = toku_testsetup_insert_to_leaf (
        ft, 
        node_leaf, 
        "a", // key
        2, // keylen
        "aa", 
        3
        );
    assert(r==0);
    r = toku_testsetup_insert_to_leaf (
        ft, 
        node_leaf, 
        "z", // key
        2, // keylen
        "zz", 
        3
        );
    assert(r==0);
    char filler[400];
    memset(filler, 0, sizeof(filler));
    // now we insert filler data so that the rebalance
    // keeps it at two nodes
    r = toku_testsetup_insert_to_leaf (
        ft, 
        node_leaf, 
        "b", // key
        2, // keylen
        filler, 
        sizeof(filler)
        );
    assert(r==0);
    r = toku_testsetup_insert_to_leaf (
        ft, 
        node_leaf, 
        "y", // key
        2, // keylen
        filler, 
        sizeof(filler)
        );
    assert(r==0);

    //
    // now insert a bunch of dummy delete messages
    // into the internal node, to get its cachepressure size up    
    //
    for (int i = 0; i < 100000; i++) {
        r = toku_testsetup_insert_to_nonleaf (
            ft, 
            node_internal, 
            FT_DELETE_ANY, 
            "jj", // this key does not exist, so its message application should be a no-op
            3, 
            NULL, 
            0
            );
        assert(r==0);
    }

    //
    // now insert a broadcast message into the root
    //
    r = toku_testsetup_insert_to_nonleaf (
        ft, 
        node_root, 
        FT_UPDATE_BROADCAST_ALL, 
        NULL, 
        0, 
        NULL, 
        0
        );
    assert(r==0);

    //
    // now run a checkpoint to get everything clean
    //
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);
    // now lock and release the leaf node to make sure it is what we expect it to be.
    FTNODE node = NULL;
    ftnode_fetch_extra bfe;
    bfe.create_for_min_read(ft->ft);
    toku_pin_ftnode(
        ft->ft, 
        node_leaf,
        toku_cachetable_hash(ft->ft->cf, node_leaf),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(!node->dirty);
    assert(node->n_children == 2);
    // a hack to get the basement nodes evicted
    for (int i = 0; i < 20; i++) {
        toku_ftnode_pe_callback(node, make_pair_attr(0xffffffff), ft->ft, def_pe_finalize_impl, nullptr);
    }
    // this ensures that when we do the lookups below,
    // that the data is read off disk
    assert(BP_STATE(node,0) == PT_ON_DISK);
    assert(BP_STATE(node,1) == PT_ON_DISK);
    toku_unpin_ftnode(ft->ft, node);

    // now do a lookup on one of the keys, this should bring a leaf node up to date 
    DBT k;
    struct check_pair pair = {2, "a", 0, NULL, 0};
    r = toku_ft_lookup(ft, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair);
    assert(r==0);

    if (keep_other_bn_in_memory) {
        //
        // pin the leaf one more time
        // and make sure that one basement
        // both basement nodes are in memory,
        // but only one should have broadcast message
        // applied.
        //
        bfe.create_for_full_read(ft->ft);
    }
    else {
        //
        // pin the leaf one more time
        // and make sure that one basement
        // node is in memory and another is
        // on disk
        //
        bfe.create_for_min_read(ft->ft);
    }
    toku_pin_ftnode(
        ft->ft, 
        node_leaf,
        toku_cachetable_hash(ft->ft->cf, node_leaf),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(!node->dirty);
    assert(node->n_children == 2);
    assert(BP_STATE(node,0) == PT_AVAIL);
    if (keep_other_bn_in_memory) {
        assert(BP_STATE(node,1) == PT_AVAIL);
    }
    else {
        assert(BP_STATE(node,1) == PT_ON_DISK);
    }
    toku_unpin_ftnode(ft->ft, node);
    
    //
    // now let us induce a clean on the internal node
    //    
    bfe.create_for_min_read(ft->ft);
    toku_pin_ftnode(
        ft->ft, 
        node_internal,
        toku_cachetable_hash(ft->ft->cf, node_internal),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    assert(!node->dirty);

    // we expect that this flushes its buffer, that
    // a merge is not done, and that the lookup
    // of values "a" and "z" still works
    r = toku_ftnode_cleaner_callback(
        node,
        node_internal,
        toku_cachetable_hash(ft->ft->cf, node_internal),
        ft->ft
        );

    // verify that node_internal's buffer is empty
    bfe.create_for_min_read(ft->ft);
    toku_pin_ftnode(
        ft->ft, 
        node_internal,
        toku_cachetable_hash(ft->ft->cf, node_internal),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        &node,
        true
        );
    // check that buffers are empty
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) == 0);
    toku_unpin_ftnode(ft->ft, node);
    
    //
    // now run a checkpoint to get everything clean,
    // and to get the rebalancing to happen
    //
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);

    // check that lookups on the two keys is still good
    struct check_pair pair1 = {2, "a", 0, NULL, 0};
    r = toku_ft_lookup(ft, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair1);
    assert(r==0);
    struct check_pair pair2 = {2, "z", 0, NULL, 0};
    r = toku_ft_lookup(ft, toku_fill_dbt(&k, "z", 2), lookup_checkf, &pair2);
    assert(r==0);


    r = toku_close_ft_handle_nolsn(ft, 0);    assert(r==0);
    toku_cachetable_close(&ct);

    toku_free(pivots[0]);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    default_parse_args(argc, argv);
    doit(false);
    doit(true);
    return 0;
}
