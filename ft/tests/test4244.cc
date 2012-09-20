/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"
#include "includes.h"
#include <ft-cachetable-wrappers.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
FT_HANDLE t;
int fnamelen;
char *fname;

static void
doit (void) {
    BLOCKNUM node_leaf, node_internal, node_root;

    int r;
    
    fnamelen = strlen(__SRCFILE__) + 20;
    XMALLOC_N(fnamelen, fname);

    snprintf(fname, fnamelen, "%s.ft_handle", __SRCFILE__);
    toku_cachetable_create(&ct, 500*1024*1024, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, NODESIZE, NODESIZE/2, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    toku_free(fname);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &node_leaf, 1, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &node_internal, 1, &node_leaf, 0, 0);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &node_root, 1, &node_internal, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, node_root);
    assert(r==0);

    // make a 1MB val
    uint32_t big_val_size = 1000000;
    char* XCALLOC_N(big_val_size, big_val);
    DBT k,v;
    memset(&k, 0, sizeof(k));
    memset(&v, 0, sizeof(v));
    for (int i = 0; i < 100; i++) {
        toku_ft_insert(t,
                       toku_fill_dbt(&k, "hello", 6),
                       toku_fill_dbt(&v, big_val, big_val_size),
                       null_txn);
    }
    toku_free(big_val);


    // at this point, we have inserted 100MB of messages, if bug exists,
    // then node_internal should be huge
    // we pin it and verify that it is not
    FTNODE node;
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, t->ft);
    toku_pin_ftnode_off_client_thread(
        t->ft, 
        node_internal,
        toku_cachetable_hash(t->ft->cf, node_internal),
        &bfe,
        PL_WRITE_EXPENSIVE, 
        0,
        NULL,
        &node
        );
    assert(node->n_children == 1);
    // simply assert that the buffer is less than 50MB,
    // we inserted 100MB of data in there.
    assert(toku_bnc_nbytesinbuf(BNC(node, 0)) < 50*1000*1000);
    toku_unpin_ftnode_off_client_thread(t->ft, node);

    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    toku_cachetable_close(&ct);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
