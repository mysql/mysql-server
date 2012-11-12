/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* The goal of this test.  Make sure that inserts stay behind deletes. */


#include "test.h"


static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

enum { NODESIZE = 1024, KSIZE=NODESIZE-100, TOKU_PSIZE=20 };

CACHETABLE ct;
FT_HANDLE t;
int fnamelen;
char *fname;

static void
doit (void) {
    BLOCKNUM nodea,nodeb;

    int r;
    
    fnamelen = strlen(__SRCFILE__) + 20;
    XMALLOC_N(fnamelen, fname);

    snprintf(fname, fnamelen, "%s.ft_handle", __SRCFILE__);
    toku_cachetable_create(&ct, 16*1024, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, NODESIZE, NODESIZE, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    toku_free(fname);

    toku_testsetup_initialize();  // must precede any other toku_testsetup calls

    r = toku_testsetup_leaf(t, &nodea, 1, NULL, NULL);
    assert(r==0);

    r = toku_testsetup_nonleaf(t, 1, &nodeb, 1, &nodea, 0, 0);
    assert(r==0);

    r = toku_testsetup_insert_to_nonleaf(t, nodeb, FT_DELETE_ANY, "hello", 6, 0, 0);
    assert(r==0);

    r = toku_testsetup_root(t, nodeb);
    assert(r==0);
    
    DBT k,v;
    toku_ft_insert(t,
                   toku_fill_dbt(&k, "hello", 6),
                   toku_fill_dbt(&v, "there", 6),
                   null_txn);

    memset(&v, 0, sizeof(v));
    struct check_pair pair = {6, "hello", 6, "there", 0};
    r = toku_ft_lookup(t, &k, lookup_checkf, &pair);
    assert(r==0);
    assert(pair.call_count == 1);

    r = toku_close_ft_handle_nolsn(t, 0);    assert(r==0);
    toku_cachetable_close(&ct);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    doit();
    return 0;
}
