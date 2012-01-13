#ident "$Id: test-del-inorder.c 32975 2011-07-11 23:42:51Z leifwalsh $"
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

BOOL checkpoint_called;
BOOL checkpoint_callback_called;
toku_pthread_t checkpoint_tid;


// callback functions for flush_some_child
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

static int child_to_flush(struct brt_header* UU(h), BRTNODE parent, void* UU(extra)) {
    assert(parent->height == 1);
    assert(parent->n_children == 1);
    return 0;
}

static void dummy_update_status(BRTNODE UU(child), int UU(dirtied), void* UU(extra)) {
}


static void checkpoint_callback(void* UU(extra)) {
    usleep(1*1024*1024);
    checkpoint_callback_called = TRUE;
}


static void *do_checkpoint(void *arg) {
    // first verify that checkpointed_data is correct;
    if (verbose) printf("starting a checkpoint\n");
    int r = toku_checkpoint(ct, NULL, checkpoint_callback, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r);
    if (verbose) printf("completed a checkpoint\n");
    return arg;
}


static void flusher_callback(int state, void* extra) {
    BOOL after_child_pin = *(BOOL *)extra;
    if (verbose) {
        printf("state %d\n", state);
    }
    if ((state == ft_flush_before_child_pin && !after_child_pin) ||
        (state == ft_flush_after_child_pin && after_child_pin)) {
        checkpoint_called = TRUE;
        int r = toku_pthread_create(&checkpoint_tid, NULL, do_checkpoint, NULL); 
        assert_zero(r);
        while (!checkpoint_callback_called) {
            usleep(1*1024*1024);
        }
    }
}

static void
doit (BOOL after_child_pin) {
    BLOCKNUM node_leaf, node_root;

    int r;
    checkpoint_called = FALSE;
    checkpoint_callback_called = FALSE;

    toku_flusher_thread_set_callback(flusher_callback, &after_child_pin);
    
    r = toku_brt_create_cachetable(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink("foo.brt");
    r = toku_open_brt("foo.brt", 1, &t, NODESIZE, NODESIZE/2, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaf, 1, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &node_root, 1, &node_leaf, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);


    r = toku_testsetup_insert_to_nonleaf(
        t, 
        node_root, 
        BRT_INSERT,
        "a",
        2,
        NULL,
        0
        );

    // at this point, we have inserted a message into
    // the root, and we wish to flush it, the leaf
    // should be empty

    struct flusher_advice fa;
    flusher_advice_init(
        &fa,
        child_to_flush,
        dont_destroy_bn,
        recursively_flush_should_not_happen,
        merge_should_not_happen,
        dummy_update_status,
        default_pick_child_after_split,
        NULL
        );
    
    BRTNODE node = NULL;
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, t->h);
    toku_pin_brtnode_off_client_thread(
        t->h, 
        node_root,
        toku_cachetable_hash(t->h->cf, node_root),
        &bfe,
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(node->n_children == 1);
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) > 0);

    // do the flush
    flush_some_child(t->h, node, &fa);
    assert(checkpoint_callback_called);

    // now let's pin the root again and make sure it is flushed
    toku_pin_brtnode_off_client_thread(
        t->h, 
        node_root,
        toku_cachetable_hash(t->h->cf, node_root),
        &bfe,
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(node->n_children == 1);
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) == 0);
    toku_unpin_brtnode(t, node);

    void *ret;
    r = toku_pthread_join(checkpoint_tid, &ret); 
    assert_zero(r);

    //
    // now the dictionary has been checkpointed
    // copy the file to something with a new name,
    // open it, and verify that the state of what is
    // checkpointed is what we expect
    //

    r = system("cp foo.brt bar.brt ");
    assert_zero(r);

    BRT c_brt;
    r = toku_open_brt("bar.brt", 0, &c_brt, NODESIZE, NODESIZE/2, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    //
    // now pin the root, verify that we have a message in there, and that it is clean
    //
    fill_bfe_for_full_read(&bfe, c_brt->h);
    toku_pin_brtnode_off_client_thread(
        c_brt->h, 
        node_root,
        toku_cachetable_hash(c_brt->h->cf, node_root),
        &bfe,
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(!node->dirty);
    assert(node->n_children == 1);
    if (after_child_pin) {
        assert(toku_bnc_nbytesinbuf(BNC(node, 0)) == 0);
    }
    else {
        assert(toku_bnc_nbytesinbuf(BNC(node, 0)) > 0);
    }
    toku_unpin_brtnode_off_client_thread(c_brt->h, node);

    toku_pin_brtnode_off_client_thread(
        c_brt->h, 
        node_leaf,
        toku_cachetable_hash(c_brt->h->cf, node_root),
        &bfe,
        0,
        NULL,
        &node
        );
    assert(node->height == 0);
    assert(!node->dirty);
    assert(node->n_children == 1);
    if (after_child_pin) {
        assert(BLB_NBYTESINBUF(node,0) > 0);
    }
    else {
        assert(BLB_NBYTESINBUF(node,0) == 0);
    }
    toku_unpin_brtnode_off_client_thread(c_brt->h, node);

    struct check_pair pair1 = {2, "a", 0, NULL, 0};
    DBT k;
    r = toku_brt_lookup(c_brt, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair1);
    assert(r==0);


    r = toku_close_brt(t, 0);    assert(r==0);
    r = toku_close_brt(c_brt, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit(FALSE);
    doit(TRUE);
    return 0;
}
