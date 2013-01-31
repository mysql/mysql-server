/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test0 (void) {
    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    const char *fname = TOKU_TEST_FILENAME;
    if (verbose) printf("%s:%d test0\n", __FILE__, __LINE__);
    
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    if (verbose) printf("%s:%d test0\n", __FILE__, __LINE__);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    //printf("%s:%d test0\n", __FILE__, __LINE__);
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
    r = toku_close_ft_handle_nolsn(t, 0);     assert(r==0);
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
    toku_cachetable_close(&ct);
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
