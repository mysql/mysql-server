/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
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

bool checkpoint_called;
bool checkpoint_callback_called;
toku_pthread_t checkpoint_tid;


// callback functions for toku_ft_flush_some_child
static bool
dont_destroy_bn(void* UU(extra))
{
    return false;
}

static bool recursively_flush_should_not_happen(FTNODE UU(child), void* UU(extra)) {
    assert(false);
}

static int child_to_flush(FT UU(h), FTNODE parent, void* UU(extra)) {
    assert(parent->height == 1);
    assert(parent->n_children == 2);
    return 0;
}

static void dummy_update_status(FTNODE UU(child), int UU(dirtied), void* UU(extra)) {
}


static void checkpoint_callback(void* UU(extra)) {
    usleep(1*1024*1024);
    checkpoint_callback_called = true;
}


static void *do_checkpoint(void *arg) {
    // first verify that checkpointed_data is correct;
    if (verbose) printf("starting a checkpoint\n");
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    int r = toku_checkpoint(cp, NULL, checkpoint_callback, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);
    if (verbose) printf("completed a checkpoint\n");
    return arg;
}


static void flusher_callback(int state, void* extra) {
    int desired_state = *(int *)extra;
    if (verbose) {
        printf("state %d\n", state);
    }
    if (state == desired_state) {
        checkpoint_called = true;
        int r = toku_pthread_create(&checkpoint_tid, NULL, do_checkpoint, NULL); 
        assert_zero(r);
        while (!checkpoint_callback_called) {
            usleep(1*1024*1024);
        }
    }
}

static void
doit (int state) {
    BLOCKNUM node_root;
    BLOCKNUM node_leaves[2];

    int r;
    checkpoint_called = false;
    checkpoint_callback_called = false;

    toku_flusher_thread_set_callback(flusher_callback, &state);
    
    toku_cachetable_create(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER);
    unlink("foo3.ft_handle");
    unlink("bar3.ft_handle");
    // note the basement node size is 5 times the node size
    // this is done to avoid rebalancing when writing a leaf
    // node to disk
    r = toku_open_ft_handle("foo3.ft_handle", 1, &t, NODESIZE, 5*NODESIZE, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaves[0], 1, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_leaf(t, &node_leaves[1], 1, NULL, NULL);
    assert(r==0);

    char* pivots[1];
    pivots[0] = toku_strdup("kkkkk");
    int pivot_len = 6;

    r = toku_testsetup_nonleaf(t, 1, &node_root, 2, node_leaves, pivots, &pivot_len);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);

    char dummy_val[NODESIZE/2-50];
    memset(dummy_val, 0, sizeof(dummy_val));
    r = toku_testsetup_insert_to_leaf(
        t,
        node_leaves[0],
        "a",
        2,
        NULL,
        0
        );
    assert_zero(r);
    r = toku_testsetup_insert_to_leaf(
        t,
        node_leaves[1],
        "x",
        2,
        dummy_val,
        sizeof(dummy_val)
        );
    assert_zero(r);
    r = toku_testsetup_insert_to_leaf(
        t,
        node_leaves[1],
        "y",
        2,
        dummy_val,
        sizeof(dummy_val)
        );
    assert_zero(r);
    r = toku_testsetup_insert_to_leaf(
        t,
        node_leaves[1],
        "z",
        2,
        NULL,
        0
        );
    assert_zero(r);


    // at this point, we have inserted two leafentries,
    // one in each leaf node. A flush should invoke a merge
    struct flusher_advice fa;
    flusher_advice_init(
        &fa,
        child_to_flush,
        dont_destroy_bn,
        recursively_flush_should_not_happen,
        default_merge_child,
        dummy_update_status,
        default_pick_child_after_split,
        NULL
        );

    // hack to get merge going
    FTNODE node = NULL;
    toku_pin_node_with_min_bfe(&node, node_leaves[0], t);
    BLB_SEQINSERT(node, node->n_children-1) = false;
    toku_unpin_ftnode(t->ft, node);
    toku_pin_node_with_min_bfe(&node, node_leaves[1], t);
    BLB_SEQINSERT(node, node->n_children-1) = false;
    toku_unpin_ftnode(t->ft, node);

    
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, t->ft);
    toku_pin_ftnode_off_client_thread(
        t->ft, 
        node_root,
        toku_cachetable_hash(t->ft->cf, node_root),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(node->n_children == 2);

    // do the flush
    toku_ft_flush_some_child(t->ft, node, &fa);
    assert(checkpoint_callback_called);

    // now let's pin the root again and make sure it is has rebalanced
    toku_pin_ftnode_off_client_thread(
        t->ft, 
        node_root,
        toku_cachetable_hash(t->ft->cf, node_root),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(node->n_children == 2);
    toku_unpin_ftnode(t->ft, node);

    void *ret;
    r = toku_pthread_join(checkpoint_tid, &ret); 
    assert_zero(r);

    //
    // now the dictionary has been checkpointed
    // copy the file to something with a new name,
    // open it, and verify that the state of what is
    // checkpointed is what we expect
    //

    r = system("cp foo3.ft_handle bar3.ft_handle ");
    assert_zero(r);

    FT_HANDLE c_ft;
    // note the basement node size is 5 times the node size
    // this is done to avoid rebalancing when writing a leaf
    // node to disk
    r = toku_open_ft_handle("bar3.ft_handle", 0, &c_ft, NODESIZE, 5*NODESIZE, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    //
    // now pin the root, verify that the state is what we expect
    //
    fill_bfe_for_full_read(&bfe, c_ft->ft);
    toku_pin_ftnode_off_client_thread(
        c_ft->ft, 
        node_root,
        toku_cachetable_hash(c_ft->ft->cf, node_root),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(!node->dirty);
    BLOCKNUM left_child, right_child;

    assert(node->n_children == 2);
    left_child = BP_BLOCKNUM(node,0);
    right_child = BP_BLOCKNUM(node,1);

    toku_unpin_ftnode_off_client_thread(c_ft->ft, node);

    // now let's verify the leaves are what we expect
    toku_pin_ftnode_off_client_thread(
        c_ft->ft, 
        left_child,
        toku_cachetable_hash(c_ft->ft->cf, left_child),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 0);
    assert(!node->dirty);
    assert(node->n_children == 1);
    assert(toku_omt_size(BLB_BUFFER(node,0)) == 2);
    toku_unpin_ftnode_off_client_thread(c_ft->ft, node);
    
    toku_pin_ftnode_off_client_thread(
        c_ft->ft, 
        right_child,
        toku_cachetable_hash(c_ft->ft->cf, right_child),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 0);
    assert(!node->dirty);
    assert(node->n_children == 1);
    assert(toku_omt_size(BLB_BUFFER(node,0)) == 2);
    toku_unpin_ftnode_off_client_thread(c_ft->ft, node);


    DBT k;
    struct check_pair pair1 = {2, "a", 0, NULL, 0};
    r = toku_ft_lookup(c_ft, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair1);
    assert(r==0);
    struct check_pair pair2 = {2, "x", sizeof(dummy_val), dummy_val, 0};
    r = toku_ft_lookup(c_ft, toku_fill_dbt(&k, "x", 2), lookup_checkf, &pair2);
    assert(r==0);
    struct check_pair pair3 = {2, "y", sizeof(dummy_val), dummy_val, 0};
    r = toku_ft_lookup(c_ft, toku_fill_dbt(&k, "y", 2), lookup_checkf, &pair3);
    assert(r==0);
    struct check_pair pair4 = {2, "z", 0, NULL, 0};
    r = toku_ft_lookup(c_ft, toku_fill_dbt(&k, "z", 2), lookup_checkf, &pair4);
    assert(r==0);


    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    r = toku_close_ft_handle_nolsn(c_ft, 0);    assert(r==0);
    toku_cachetable_close(&ct);
    toku_free(pivots[0]);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    default_parse_args(argc, argv);
    doit(ft_flush_aflter_rebalance);
    return 0;
}
