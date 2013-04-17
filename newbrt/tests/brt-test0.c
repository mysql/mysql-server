/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test0 (void) {
    BRT t;
    int r;
    CACHETABLE ct;
    char fname[]= __FILE__ "0.brt";
    if (verbose) printf("%s:%d test0\n", __FILE__, __LINE__);
    toku_memory_check=1;
    
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    if (verbose) printf("%s:%d test0\n", __FILE__, __LINE__);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 1024, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);
    //printf("%s:%d test0\n", __FILE__, __LINE__);
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
    r = toku_close_brt(t, 0);     assert(r==0);
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
    r = toku_cachetable_close(&ct);
    assert(r==0);
    
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    if (verbose) printf("test0 A\n");
    test0();
    if (verbose) printf("test0 B\n");
    test0(); /* Make sure it works twice. */
    
    if (verbose) printf("test0 ok\n");
    return 0;
}
