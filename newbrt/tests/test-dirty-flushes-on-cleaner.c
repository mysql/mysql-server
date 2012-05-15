/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "$Id$"
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"
#include "includes.h"
#include <brt-cachetable-wrappers.h>
#include "brt-flusher.h"
#include "checkpoint.h"


static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
BRT brt;
int fnamelen;
char *fname;

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
doit (void) {
    BLOCKNUM node_leaf;
    BLOCKNUM node_internal, node_root;

    int r;
    
    fnamelen = strlen(__SRCFILE__) + 20;
    fname = toku_malloc(fnamelen);
    assert(fname!=0);

    snprintf(fname, fnamelen, "%s.brt", __SRCFILE__);
    r = toku_brt_create_cachetable(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &brt, NODESIZE, NODESIZE/2, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    toku_free(fname);

    brt->update_fun = update_func;
    brt->h->update_fun = update_func;
    
    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    char* pivots[1];
    pivots[0] = toku_strdup("kkkkk");
    int pivot_len = 6;

    r = toku_testsetup_leaf(brt, &node_leaf, 2, pivots, &pivot_len);
    assert(r==0);

    r = toku_testsetup_nonleaf(brt, 1, &node_internal, 1, &node_leaf, 0, 0);
    assert(r==0);

    r = toku_testsetup_nonleaf(brt, 2, &node_root, 1, &node_internal, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(brt, node_root);
    assert(r==0);

    //
    // at this point we have created a tree with a root, an internal node,
    // and two leaf nodes, the pivot being "kkkkk"
    //

    // now we insert a row into each leaf node
    r = toku_testsetup_insert_to_leaf (
        brt, 
        node_leaf, 
        "a", // key
        2, // keylen
        "aa", 
        3
        );
    assert(r==0);
    r = toku_testsetup_insert_to_leaf (
        brt, 
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
        brt, 
        node_leaf, 
        "b", // key
        2, // keylen
        filler, 
        sizeof(filler)
        );
    assert(r==0);
    r = toku_testsetup_insert_to_leaf (
        brt, 
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
            brt, 
            node_internal, 
            BRT_DELETE_ANY, 
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
        brt, 
        node_root, 
        BRT_UPDATE_BROADCAST_ALL, 
        NULL, 
        0, 
        NULL, 
        0
        );
    assert(r==0);

    // now lock and release the leaf node to make sure it is what we expect it to be.
    BRTNODE node = NULL;
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, brt->h);
    toku_pin_brtnode_off_client_thread(
        brt->h, 
        node_leaf,
        toku_cachetable_hash(brt->h->cf, node_leaf),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    assert(node->dirty);
    assert(node->n_children == 2);
    assert(BP_STATE(node,0) == PT_AVAIL);
    assert(BP_STATE(node,1) == PT_AVAIL);
    toku_unpin_brtnode_off_client_thread(brt->h, node);

    // now do a lookup on one of the keys, this should bring a leaf node up to date 
    DBT k;
    struct check_pair pair = {2, "a", 0, NULL, 0};
    r = toku_brt_lookup(brt, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair);
    assert(r==0);

    //
    // pin the leaf one more time
    // and make sure that one basement
    // node is in memory and another is
    // on disk
    //
    fill_bfe_for_min_read(&bfe, brt->h);
    toku_pin_brtnode_off_client_thread(
        brt->h, 
        node_leaf,
        toku_cachetable_hash(brt->h->cf, node_leaf),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    assert(node->dirty);
    assert(node->n_children == 2);
    assert(BP_STATE(node,0) == PT_AVAIL);
    assert(BP_STATE(node,1) == PT_AVAIL);
    toku_unpin_brtnode_off_client_thread(brt->h, node);
    
    //
    // now let us induce a clean on the internal node
    //    
    fill_bfe_for_min_read(&bfe, brt->h);
    toku_pin_brtnode_off_client_thread(
        brt->h, 
        node_internal,
        toku_cachetable_hash(brt->h->cf, node_internal),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    assert(node->dirty);

    // we expect that this flushes its buffer, that
    // a merge is not done, and that the lookup
    // of values "a" and "z" still works
    r = toku_brtnode_cleaner_callback(
        node,
        node_internal,
        toku_cachetable_hash(brt->h->cf, node_internal),
        brt->h
        );

    // verify that node_internal's buffer is empty
    fill_bfe_for_min_read(&bfe, brt->h);
    toku_pin_brtnode_off_client_thread(
        brt->h, 
        node_internal,
        toku_cachetable_hash(brt->h->cf, node_internal),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    // check that buffers are empty
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) == 0);
    toku_unpin_brtnode_off_client_thread(brt->h, node);
    
    //
    // now run a checkpoint to get everything clean,
    // and to get the rebalancing to happen
    //
    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);

    // check that lookups on the two keys is still good
    struct check_pair pair1 = {2, "a", 0, NULL, 0};
    r = toku_brt_lookup(brt, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair1);
    assert(r==0);
    struct check_pair pair2 = {2, "z", 0, NULL, 0};
    r = toku_brt_lookup(brt, toku_fill_dbt(&k, "z", 2), lookup_checkf, &pair2);
    assert(r==0);


    r = toku_close_brt_nolsn(brt, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    toku_free(pivots[0]);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    default_parse_args(argc, argv);
    doit();
    return 0;
}
