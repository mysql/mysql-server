/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."

// verify that key_range64 can deal with >2G number of keys.
// create a height 1 tree with 1 key in each subtree.
// artificially set the key estimates in the subtrees to huge (2**31).

#include "includes.h"
#include "test.h"

static BRTNODE
make_node(BRT brt, int height) {
    BRTNODE node = NULL;
    int n_children = (height == 0) ? 1 : 0;
    toku_create_new_brtnode(brt, &node, height, n_children);
    if (n_children) BP_STATE(node,0) = PT_AVAIL;
    return node;
}

static void
append_leaf(BRTNODE leafnode, void *key, size_t keylen, void *val, size_t vallen) {
    assert(leafnode->height == 0);

    DBT thekey; toku_fill_dbt(&thekey, key, keylen);
    DBT theval; toku_fill_dbt(&theval, val, vallen);

    // get an index that we can use to create a new leaf entry
    uint32_t idx = toku_omt_size(BLB_BUFFER(leafnode, 0));

    // apply an insert to the leaf node
    MSN msn = next_dummymsn();
    BRT_MSG_S cmd = { BRT_INSERT, msn, xids_get_root_xids(), .u.id = { &thekey, &theval } };
    brt_leaf_apply_cmd_once(BLB(leafnode, 0), &BP_SUBTREE_EST(leafnode,0), &cmd, idx, NULL, NULL, NULL);

    // dont forget to dirty the node
    leafnode->dirty = 1;
}

static void
populate_leaf(BRTNODE leafnode, int seq, int n, int *minkey, int *maxkey) {
    for (int i = 0; i < n; i++) {
        int k = htonl(seq + i);
        int v = seq + i;
        append_leaf(leafnode, &k, sizeof k, &v, sizeof v);
    }
    *minkey = htonl(seq);
    *maxkey = htonl(seq + n - 1);
}

static BRTNODE
make_tree(BRT brt, int height, int fanout, int nperleaf, int *seq, int *minkey, int *maxkey, uint64_t subtree_size) {
    BRTNODE node;
    if (height == 0) {
        node = make_node(brt, 0);
        populate_leaf(node, *seq, nperleaf, minkey, maxkey);
        *seq += nperleaf;
    } else {
        node = make_node(brt, height);
        int minkeys[fanout], maxkeys[fanout];
        for (int childnum = 0; childnum < fanout; childnum++) {
            BRTNODE child = make_tree(brt, height-1, fanout, nperleaf, seq, &minkeys[childnum], &maxkeys[childnum], subtree_size);
            if (childnum == 0) 
                toku_brt_nonleaf_append_child(node, child, NULL, 0);
            else {
                int k = minkeys[childnum]; // use the min key of the right subtree, which creates a broken tree
                struct kv_pair *pivotkey = kv_pair_malloc(&k, sizeof k, NULL, 0);
                toku_brt_nonleaf_append_child(node, child, pivotkey, sizeof k);
            }
            BP_SUBTREE_EST(node,childnum) = make_subtree_estimates(subtree_size, subtree_size, 0, FALSE);
            toku_unpin_brtnode(brt, child);
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

static void 
test_make_tree(int height, int fanout, int nperleaf, uint64_t subtree_size) {
    int r;

    // cleanup
    char fname[]= __FILE__ ".brt";
    r = unlink(fname);
    assert(r == 0 || (r == -1 && errno == ENOENT));

    // create a cachetable
    CACHETABLE ct = NULL;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r == 0);

    // create the brt
    TOKUTXN null_txn = NULL;
    DB *null_db = NULL;
    BRT brt = NULL;
    r = toku_open_brt(fname, 1, &brt, 1024, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r == 0);

    // make a tree
    int seq = 0, minkey, maxkey;
    BRTNODE newroot = make_tree(brt, height, fanout, nperleaf, &seq, &minkey, &maxkey, subtree_size);

    // discard the old root block
    u_int32_t fullhash = 0;
    CACHEKEY *rootp;
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

    // set the new root to point to the new tree
    *rootp = newroot->thisnodename;

    // unpin the new root
    toku_unpin_brtnode(brt, newroot);

    // test the key range estimate
    uint64_t less, equal, greater;
    int k = htonl(0);
    DBT key; toku_fill_dbt(&key, &k, sizeof k);
    r = toku_brt_keyrange(brt, &key, &less, &equal, &greater); assert_zero(r);
    assert(less == 0 && equal == 1 && greater == subtree_size);

    // flush to the file system
    r = toku_close_brt(brt, 0);     
    assert(r == 0);

    // shutdown the cachetable
    r = toku_cachetable_close(&ct);
    assert(r == 0);
}

static int
usage(void) {
    return 1;
}

int
test_main (int argc , const char *argv[]) {
    int height = 1;
    int fanout = 2;
    int nperleaf = 1;
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
        return usage();
    }
    test_make_tree(height, fanout, nperleaf, 0);
    test_make_tree(height, fanout, nperleaf, 1ULL << 30);
    test_make_tree(height, fanout, nperleaf, 1ULL << 31);
    test_make_tree(height, fanout, nperleaf, 1ULL << 32);
    
    return 0;
}
