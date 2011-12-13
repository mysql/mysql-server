#ident "$Id: test-del-inorder.c 32975 2011-07-11 23:42:51Z leifwalsh $"
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"
#include "includes.h"
#include <brt-cachetable-wrappers.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
BRT t;
int fnamelen;
char *fname;

static void
doit (void) {
    BLOCKNUM node_leaf, node_internal, node_root;

    int r;
    
    fnamelen = strlen(__FILE__) + 20;
    fname = toku_malloc(fnamelen);
    assert(fname!=0);

    snprintf(fname, fnamelen, "%s.brt", __FILE__);
    r = toku_brt_create_cachetable(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, NODESIZE, NODESIZE/2, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);
    toku_free(fname);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaf);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &node_internal, 1, &node_leaf, 0, 0);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &node_root, 1, &node_internal, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);

    // make a 1MB val
    u_int32_t big_val_size = 1000000;
    char* big_val = toku_xmalloc(big_val_size);
    memset(big_val, 0, big_val_size);
    DBT k,v;
    memset(&k, 0, sizeof(k));
    memset(&v, 0, sizeof(v));
    for (int i = 0; i < 100; i++) {
        r = toku_brt_insert(t,
                            toku_fill_dbt(&k, "hello", 6),
                            toku_fill_dbt(&v, big_val, big_val_size),
                            null_txn);
        assert(r==0);
    }
    toku_free(big_val);


    // at this point, we have inserted 100MB of messages, if bug exists,
    // then node_internal should be huge
    // we pin it and verify that it is not
    BRTNODE node;
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, t->h);
    toku_pin_brtnode_off_client_thread(
        t->h, 
        node_internal,
        toku_cachetable_hash(t->h->cf, node_internal),
        &bfe,
        0,
        NULL,
        &node
        );
    assert(node->n_children == 1);
    // simply assert that the buffer is less than 50MB,
    // we inserted 100MB of data in there.
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) < 50*1000*1000);
    toku_unpin_brtnode_off_client_thread(t->h, node);

    r = toku_close_brt(t, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
