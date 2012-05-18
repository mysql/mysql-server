/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test1 (void) {
    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    char fname[]= __SRCFILE__ ".ft_handle";
    DBT k,v;
    
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    r = toku_ft_insert(t, toku_fill_dbt(&k, "hello", 6), toku_fill_dbt(&v, "there", 6), null_txn);
    assert(r==0);
    {
	struct check_pair pair = {6, "hello", 6, "there", 0};
	r = toku_ft_lookup(t, toku_fill_dbt(&k, "hello", 6), lookup_checkf, &pair);
	assert(r==0);
	assert(pair.call_count==1);
    }
    r = toku_close_ft_handle_nolsn(t, 0);              assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
    
    if (verbose) printf("test1 ok\n");
}
int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
     if (verbose) printf("test1\n");
    test1();
    
    if (verbose) printf("test1 ok\n");
    return 0;
}
