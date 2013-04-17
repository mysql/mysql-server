/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"
#include "includes.h"
#include <brt-cachetable-wrappers.h>

#include "brt-flusher.h"
#include "brt-flusher-internal.h"
#include "checkpoint.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
BRT t;
int fnamelen;
char *fname;

int curr_child_to_flush;
int num_flushes_called;

static int child_to_flush(struct brt_header* UU(h), BRTNODE parent, void* UU(extra)) {
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
        assert(FALSE);
    }
    return curr_child_to_flush;
}

static void update_status(BRTNODE UU(child), int UU(dirtied), void* UU(extra)) {
    num_flushes_called++;
}



static bool
dont_destroy_bn(void* UU(extra))
{
    return false;
}

static void merge_should_not_happen(struct flusher_advice* UU(fa),
                              struct brt_header* UU(h),
                              BRTNODE UU(parent),
                              int UU(childnum),
                              BRTNODE UU(child),
                              void* UU(extra))
{
    assert(FALSE);
}

static bool recursively_flush_should_not_happen(BRTNODE UU(child), void* UU(extra)) {
    assert(FALSE);
}

static bool always_flush(BRTNODE UU(child), void* UU(extra)) {
    return true;
}


static void
doit (void) {
    BLOCKNUM node_internal, node_root;
    BLOCKNUM node_leaf[2];
    int r;
    
    fnamelen = strlen(__FILE__) + 20;
    fname = toku_malloc(fnamelen);
    assert(fname!=0);

    snprintf(fname, fnamelen, "%s.brt", __FILE__);
    r = toku_brt_create_cachetable(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, NODESIZE, NODESIZE/2, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    toku_free(fname);

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

    char filler[900];
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
        BRT_INSERT, 
        "a",
        2,
        NULL,
        0
        );
    assert_zero(r);
    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_internal, 
        BRT_INSERT, 
        "z",
        2,
        NULL,
        0
        );
    assert_zero(r);
    
    //
    // now run a checkpoint to get everything clean
    //
    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);

    // now with setup done, start the test
    // test that if flush_some_child properly honors
    // what we say and flushes the child we pick
    BRTNODE node = NULL;
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
    flush_some_child(t->h, node, &fa);
    assert(num_flushes_called == 1);

    toku_pin_node_with_min_bfe(&node, node_internal, t);
    toku_assert_entire_node_in_memory(node);
    assert(node->dirty);
    assert(node->n_children == 2);
    // child 0 should have empty buffer because it flushed
    // child 1 should still have message in buffer
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) == 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) > 0);
    toku_unpin_brtnode(t->h, node);
    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(!node->dirty);
    curr_child_to_flush = 1;
    num_flushes_called = 0;
    flush_some_child(t->h, node, &fa);
    assert(num_flushes_called == 1);
    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(node->dirty);
    toku_assert_entire_node_in_memory(node);
    assert(node->n_children == 2);
    // both buffers should be empty now
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) == 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) == 0);
    // now let's do a flush with an empty buffer, make sure it is ok
    toku_unpin_brtnode(t->h, node);
    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(!node->dirty);
    curr_child_to_flush = 0;
    num_flushes_called = 0;
    flush_some_child(t->h, node, &fa);
    assert(num_flushes_called == 1);

    toku_pin_node_with_min_bfe(&node, node_internal, t);
    assert(!node->dirty); // nothing was flushed, so node better not be dirty
    toku_assert_entire_node_in_memory(node);
    assert(node->n_children == 2);
    // both buffers should be empty now
    assert(toku_bnc_n_entries(node->bp[0].ptr.u.nonleaf) == 0);
    assert(toku_bnc_n_entries(node->bp[1].ptr.u.nonleaf) == 0);
    toku_unpin_brtnode(t->h, node);

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
        flush_some_child(t->h, node, &fa);
        assert(num_flushes_called == 2);
    
        toku_pin_node_with_min_bfe(&node, node_internal, t);
        assert(!node->dirty); // nothing was flushed, so node better not be dirty
        toku_unpin_brtnode(t->h, node);
        toku_pin_node_with_min_bfe(&node, node_leaf[0], t);
        assert(!node->dirty); // nothing was flushed, so node better not be dirty
        toku_unpin_brtnode(t->h, node);
        toku_pin_node_with_min_bfe(&node, node_leaf[1], t);
        assert(!node->dirty); // nothing was flushed, so node better not be dirty
        toku_unpin_brtnode(t->h, node);
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
        BRT_INSERT, 
        "c",
        2,
        NULL,
        0
        );
    assert_zero(r);
    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);    
    toku_pin_node_with_min_bfe(&node, node_internal, t);
    for (int i = 0; i < 20; i++) {
        PAIR_ATTR attr;
        toku_brtnode_pe_callback(node, make_pair_attr(0xffffffff), &attr, NULL);
    }
    assert(BP_STATE(node,0) == PT_COMPRESSED);
    toku_unpin_brtnode(t->h, node);

    //now let's do the same test as above
    toku_pin_node_with_min_bfe(&node, node_root, t);
    toku_assert_entire_node_in_memory(node); // entire root is in memory
    curr_child_to_flush = 0;
    num_flushes_called = 0;
    flush_some_child(t->h, node, &fa);
    assert(num_flushes_called == 2);
    
    r = toku_close_brt_nolsn(t, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    toku_free(pivots[0]);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
