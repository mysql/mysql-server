/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."

// generate a tree with misrouted messages in the child buffers.
// check that brt verify finds them.


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
    uint32_t idx = toku_omt_size(BLB_BUFFER(leafnode, 0));

    // apply an insert to the leaf node
    MSN msn = next_dummymsn();
    FT_MSG_S cmd = { FT_INSERT, msn, xids_get_root_xids(), .u={.id = { &thekey, &theval }} };
    toku_ft_bn_apply_cmd_once(BLB(leafnode,0), &cmd, idx, NULL, TXNID_NONE, make_gc_info(false), NULL, NULL);

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
    int k = htonl(maxkey);
    maxkey = htonl(k+1);
    for (unsigned int val = htonl(minkey); val <= htonl(maxkey); val++) {
        unsigned int key = htonl(val);
        DBT thekey; toku_fill_dbt(&thekey, &key, sizeof key);
        DBT theval; toku_fill_dbt(&theval, &val, sizeof val);
	MSN msn = next_dummymsn();
        toku_ft_append_to_child_buffer(brt->ft->compare_fun, NULL, node, childnum, FT_INSERT, msn, xids_get_root_xids(), true, &thekey, &theval);
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
    char fname[]= __SRCFILE__ ".ft_handle";
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

    // discard the old root block
    // set the new root to point to the new tree
    toku_ft_set_new_root_blocknum(brt->ft, newroot->thisnodename);

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
    int height = 1;
    int fanout = 2;
    int nperleaf = 8;
    int do_verify = 1;
    initialize_dummymsn();
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
