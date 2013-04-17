/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test1 (void) {
    BRT t;
    int r;
    CACHETABLE ct;
    char fname[]= __FILE__ "1.brt";
    DBT k,v;
    toku_memory_check=1;
    
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 1024, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);
    r = toku_brt_insert(t, toku_fill_dbt(&k, "hello", 6), toku_fill_dbt(&v, "there", 6), null_txn);
    assert(r==0);
    {
	struct check_pair pair = {6, "hello", 6, "there", 0};
	r = toku_brt_lookup(t, toku_fill_dbt(&k, "hello", 6), lookup_checkf, &pair);
	assert(r==0);
	assert(pair.call_count==1);
    }
    r = toku_close_brt(t, 0);              assert(r==0);
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
