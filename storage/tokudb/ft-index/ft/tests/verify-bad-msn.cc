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

// generate fractal trees with a given height, fanout, and number of leaf elements per leaf.
// jam the child buffers with inserts.
// this code can be used as a template to build broken trees
//
// This program (copied from make-tree.c) creates a tree with bad msns by commenting out
// the setting of the msn:
//
// To correctly set msn per node:
//  - set in each non-leaf when message is injected into node (see insert_into_child_buffer())
//  - set in each leaf node (see append_leaf())
//  - set in root node (set test_make_tree())



#include <ft-cachetable-wrappers.h>
#include "test.h"

static FTNODE
make_node(FT_HANDLE brt, int height) {
    FTNODE node = NULL;
    int n_children = (height == 0) ? 1 : 0;
    toku_create_new_ftnode(brt, &node, height, n_children);
    if (n_children) BP_STATE(node,0) = PT_AVAIL;
    return node;
}

static void
append_leaf(FTNODE leafnode, void *key, size_t keylen, void *val, size_t vallen) {
    assert(leafnode->height == 0);

    DBT thekey; toku_fill_dbt(&thekey, key, keylen);
    DBT theval; toku_fill_dbt(&theval, val, vallen);

    // get an index that we can use to create a new leaf entry
    uint32_t idx = BLB_DATA(leafnode, 0)->omt_size();

    MSN msn = next_dummymsn();

    // apply an insert to the leaf node
    FT_MSG_S cmd = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &theval }} };
    toku_ft_bn_apply_cmd_once(BLB(leafnode, 0), &cmd, idx, NULL, TXNID_NONE, make_gc_info(false), NULL, NULL);

    // Create bad tree (don't do following):
    // leafnode->max_msn_applied_to_node = msn;

    // dont forget to dirty the node
    leafnode->dirty = 1;
}

static void
populate_leaf(FTNODE leafnode, int seq, int n, int *minkey, int *maxkey) {
    for (int i = 0; i < n; i++) {
        int k = htonl(seq + i);
        int v = seq + i;
        append_leaf(leafnode, &k, sizeof k, &v, sizeof v);
    }
    *minkey = htonl(seq);
    *maxkey = htonl(seq + n - 1);
}

static void
insert_into_child_buffer(FT_HANDLE brt, FTNODE node, int childnum, int minkey, int maxkey) {
    for (unsigned int val = htonl(minkey); val <= htonl(maxkey); val++) {
        MSN msn = next_dummymsn();
        unsigned int key = htonl(val);
        DBT thekey; toku_fill_dbt(&thekey, &key, sizeof key);
        DBT theval; toku_fill_dbt(&theval, &val, sizeof val);
        toku_ft_append_to_child_buffer(brt->ft->compare_fun, NULL, node, childnum, FT_INSERT, msn, xids_get_root_xids(), true, &thekey, &theval);

	// Create bad tree (don't do following):
	// node->max_msn_applied_to_node = msn;
    }
}

static FTNODE
make_tree(FT_HANDLE brt, int height, int fanout, int nperleaf, int *seq, int *minkey, int *maxkey) {
    FTNODE node;
    if (height == 0) {
        node = make_node(brt, 0);
        populate_leaf(node, *seq, nperleaf, minkey, maxkey);
        *seq += nperleaf;
    } else {
        node = make_node(brt, height);
        int minkeys[fanout], maxkeys[fanout];
        for (int childnum = 0; childnum < fanout; childnum++) {
            FTNODE child = make_tree(brt, height-1, fanout, nperleaf, seq, &minkeys[childnum], &maxkeys[childnum]);
            if (childnum == 0) {
                toku_ft_nonleaf_append_child(node, child, NULL);
            } else {
                int k = maxkeys[childnum-1]; // use the max of the left tree
                DBT pivotkey;
                toku_ft_nonleaf_append_child(node, child, toku_fill_dbt(&pivotkey, &k, sizeof k));
            }
            toku_unpin_ftnode(brt->ft, child);
            insert_into_child_buffer(brt, node, childnum, minkeys[childnum], maxkeys[childnum]);
        }
        *minkey = minkeys[0];
        *maxkey = maxkeys[0];
        for (int i = 1; i < fanout; i++) {
            if (memcmp(minkey, &minkeys[i], sizeof minkeys[i]) > 0)
                *minkey = minkeys[i];
            if (memcmp(maxkey, &maxkeys[i], sizeof maxkeys[i]) < 0)
                *maxkey = maxkeys[i];
        }
    }
    return node;
}

static UU() void
deleted_row(UU() DB *db, UU() DBT *key, UU() DBT *val) {
}

static void 
test_make_tree(int height, int fanout, int nperleaf, int do_verify) {
    int r;

    // cleanup
    const char *fname = TOKU_TEST_FILENAME;
    r = unlink(fname);
    assert(r == 0 || (r == -1 && errno == ENOENT));

    // create a cachetable
    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    // create the brt
    TOKUTXN null_txn = NULL;
    FT_HANDLE brt = NULL;
    r = toku_open_ft_handle(fname, 1, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r == 0);

    // make a tree
    int seq = 0, minkey, maxkey;
    FTNODE newroot = make_tree(brt, height, fanout, nperleaf, &seq, &minkey, &maxkey);

    // set the new root to point to the new tree
    toku_ft_set_new_root_blocknum(brt->ft, newroot->thisnodename);

    // Create bad tree (don't do following):
    // newroot->max_msn_applied_to_node = last_dummymsn(); // capture msn of last message injected into tree

    // unpin the new root
    toku_unpin_ftnode(brt->ft, newroot);

    if (do_verify) {
        r = toku_verify_ft(brt);
        assert(r != 0);
    }

    // flush to the file system
    r = toku_close_ft_handle_nolsn(brt, 0);     
    assert(r == 0);

    // shutdown the cachetable
    toku_cachetable_close(&ct);
}

static int
usage(void) {
    return 1;
}

int
test_main (int argc , const char *argv[]) {
    initialize_dummymsn();
    int height = 1;
    int fanout = 2;
    int nperleaf = 8;
    int do_verify = 1;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(arg, "--height") == 0 && i+1 < argc) {
            height = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--fanout") == 0 && i+1 < argc) {
            fanout = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--nperleaf") == 0 && i+1 < argc) {
            nperleaf = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--verify") == 0 && i+1 < argc) {
            do_verify = atoi(argv[++i]);
            continue;
        }
        return usage();
    }
    test_make_tree(height, fanout, nperleaf, do_verify);
    return 0;
}
