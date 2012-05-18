#ident "$Id: test-del-inorder.c 32975 2011-07-11 23:42:51Z leifwalsh $"
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"
#include "includes.h"
#include <ft-cachetable-wrappers.h>
#include "ft-flusher.h"
#include "ft-flusher-internal.h"
#include "checkpoint.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
FT_HANDLE t;

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
                              FT UU(h),
                              FTNODE UU(parent),
                              int UU(childnum),
                              FTNODE UU(child),
                              void* UU(extra))
{
    assert(FALSE);
}

static bool recursively_flush_should_not_happen(FTNODE UU(child), void* UU(extra)) {
    assert(FALSE);
}

static int child_to_flush(FT UU(h), FTNODE parent, void* UU(extra)) {
    assert(parent->height == 1);
    assert(parent->n_children == 1);
    return 0;
}

static void dummy_update_status(FTNODE UU(child), int UU(dirtied), void* UU(extra)) {
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
    BOOL after_split = *(BOOL *)extra;
    if (verbose) {
        printf("state %d\n", state);
    }
    if ((state == flt_flush_before_split && !after_split) ||
        (state == flt_flush_during_split && after_split)) {
        checkpoint_called = TRUE;
        int r = toku_pthread_create(&checkpoint_tid, NULL, do_checkpoint, NULL); 
        assert_zero(r);
        while (!checkpoint_callback_called) {
            usleep(1*1024*1024);
        }
    }
}

static void
doit (BOOL after_split) {
    BLOCKNUM node_leaf, node_root;

    int r;
    checkpoint_called = FALSE;
    checkpoint_callback_called = FALSE;

    toku_flusher_thread_set_callback(flusher_callback, &after_split);
    
    r = toku_create_cachetable(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink("foo4.ft_handle");
    unlink("bar4.ft_handle");
    // note the basement node size is 5 times the node size
    // this is done to avoid rebalancing when writing a leaf
    // node to disk
    r = toku_open_ft_handle("foo4.ft_handle", 1, &t, NODESIZE, 5*NODESIZE, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaf, 1, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &node_root, 1, &node_leaf, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);

    char dummy_val[NODESIZE-50];
    memset(dummy_val, 0, sizeof(dummy_val));
    r = toku_testsetup_insert_to_leaf(
        t,
        node_leaf,
        "a",
        2,
        dummy_val,
        sizeof(dummy_val)
        );
    assert_zero(r);
    r = toku_testsetup_insert_to_leaf(
        t,
        node_leaf,
        "z",
        2,
        dummy_val,
        sizeof(dummy_val)
        );
    assert_zero(r);


    // at this point, we have inserted two leafentries into
    // the leaf, that should be big enough such that a split
    // will happen    
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
    
    FTNODE node = NULL;
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, t->h);
    toku_pin_ftnode_off_client_thread(
        t->h, 
        node_root,
        toku_cachetable_hash(t->h->cf, node_root),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(node->n_children == 1);

    // do the flush
    flush_some_child(t->h, node, &fa);
    assert(checkpoint_callback_called);

    // now let's pin the root again and make sure it is has split
    toku_pin_ftnode_off_client_thread(
        t->h, 
        node_root,
        toku_cachetable_hash(t->h->cf, node_root),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(node->n_children == 2);
    toku_unpin_ftnode(t->h, node);

    void *ret;
    r = toku_pthread_join(checkpoint_tid, &ret); 
    assert_zero(r);

    //
    // now the dictionary has been checkpointed
    // copy the file to something with a new name,
    // open it, and verify that the state of what is
    // checkpointed is what we expect
    //

    r = system("cp foo4.ft_handle bar4.ft_handle ");
    assert_zero(r);

    FT_HANDLE c_ft;
    // note the basement node size is 5 times the node size
    // this is done to avoid rebalancing when writing a leaf
    // node to disk
    r = toku_open_ft_handle("bar4.ft_handle", 0, &c_ft, NODESIZE, 5*NODESIZE, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    //
    // now pin the root, verify that we have a message in there, and that it is clean
    //
    fill_bfe_for_full_read(&bfe, c_ft->h);
    toku_pin_ftnode_off_client_thread(
        c_ft->h, 
        node_root,
        toku_cachetable_hash(c_ft->h->cf, node_root),
        &bfe,
        TRUE, 
        0,
        NULL,
        &node
        );
    assert(node->height == 1);
    assert(!node->dirty);
    BLOCKNUM left_child, right_child;
    if (after_split) {
        assert(node->n_children == 2);
        left_child = BP_BLOCKNUM(node,0);
        assert(left_child.b == node_leaf.b);
        right_child = BP_BLOCKNUM(node,1);
    }
    else {
        assert(node->n_children == 1);
        left_child = BP_BLOCKNUM(node,0);
        assert(left_child.b == node_leaf.b);
    }
    toku_unpin_ftnode_off_client_thread(c_ft->h, node);

    // now let's verify the leaves are what we expect
    if (after_split) {
        toku_pin_ftnode_off_client_thread(
            c_ft->h, 
            left_child,
            toku_cachetable_hash(c_ft->h->cf, left_child),
            &bfe,
            TRUE, 
            0,
            NULL,
            &node
            );
        assert(node->height == 0);
        assert(!node->dirty);
        assert(node->n_children == 1);
        assert(toku_omt_size(BLB_BUFFER(node,0)) == 1);
        toku_unpin_ftnode_off_client_thread(c_ft->h, node);

        toku_pin_ftnode_off_client_thread(
            c_ft->h, 
            right_child,
            toku_cachetable_hash(c_ft->h->cf, right_child),
            &bfe,
            TRUE, 
            0,
            NULL,
            &node
            );
        assert(node->height == 0);
        assert(!node->dirty);
        assert(node->n_children == 1);
        assert(toku_omt_size(BLB_BUFFER(node,0)) == 1);
        toku_unpin_ftnode_off_client_thread(c_ft->h, node);
    }
    else {
        toku_pin_ftnode_off_client_thread(
            c_ft->h, 
            left_child,
            toku_cachetable_hash(c_ft->h->cf, left_child),
            &bfe,
            TRUE, 
            0,
            NULL,
            &node
            );
        assert(node->height == 0);
        assert(!node->dirty);
        assert(node->n_children == 1);
        assert(toku_omt_size(BLB_BUFFER(node,0)) == 2);
        toku_unpin_ftnode_off_client_thread(c_ft->h, node);
    }


    DBT k;
    struct check_pair pair1 = {2, "a", sizeof(dummy_val), dummy_val, 0};
    r = toku_ft_lookup(c_ft, toku_fill_dbt(&k, "a", 2), lookup_checkf, &pair1);
    assert(r==0);
    struct check_pair pair2 = {2, "z", sizeof(dummy_val), dummy_val, 0};
    r = toku_ft_lookup(c_ft, toku_fill_dbt(&k, "z", 2), lookup_checkf, &pair2);
    assert(r==0);


    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    r = toku_close_ft_handle_nolsn(c_ft, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    default_parse_args(argc, argv);
    doit(FALSE);
    doit(TRUE);
    return 0;
}
