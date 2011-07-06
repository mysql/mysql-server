/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."

// generate a tree with a single leaf node containing duplicate keys
// check that brt verify finds them

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
populate_leaf(BRTNODE leafnode, int k, int v) {
    append_leaf(leafnode, &k, sizeof k, &v, sizeof v);
}

static void 
test_dup_in_leaf(int do_verify) {
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

    // discard the old root block
    u_int32_t fullhash = 0;
    CACHEKEY *rootp;
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

    BRTNODE newroot = make_node(brt, 0);
    populate_leaf(newroot, htonl(2), 1);
    populate_leaf(newroot, htonl(2), 2);

    // set the new root to point to the new tree
    *rootp = newroot->thisnodename;

    // unpin the new root
    toku_unpin_brtnode(brt, newroot);

    if (do_verify) {
        r = toku_verify_brt(brt);
        assert(r != 0);
    }

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
        if (strcmp(arg, "--verify") == 0 && i+1 < argc) {
            do_verify = atoi(argv[++i]);
            continue;
        }
        return usage();
    }
    test_dup_in_leaf(do_verify);
    return 0;
}
