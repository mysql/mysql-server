/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test2 (int memcheck, int limit) {
    BRT t;
    int r;
    int i;
    CACHETABLE ct;
    char fname[]= __FILE__ "2.brt";
    toku_memory_check=memcheck;
    if (verbose) printf("%s:%d checking\n", __FILE__, __LINE__);
    
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 1024, ct, null_txn, toku_builtin_compare_fun, null_db);
    if (verbose) printf("%s:%d did setup\n", __FILE__, __LINE__);
    assert(r==0);
    for (i=0; i<limit; i++) { // 4096
	DBT k,v;
	char key[100],val[100];
	snprintf(key,100,"hello%d",i);
	snprintf(val,100,"there%d",i);
	r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
	assert(r==0);
	r = toku_verify_brt(t); assert(r==0);
	//printf("%s:%d did insert %d\n", __FILE__, __LINE__, i);
	if (0) {
	    r = toku_brt_flush(t);
	    assert(r==0);
	}
    }
    if (verbose) printf("%s:%d inserted\n", __FILE__, __LINE__);
    r = toku_verify_brt(t); assert(r==0);
    r = toku_close_brt(t, 0);              assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
    
    if (verbose) printf("test2 ok\n");
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
//    if (verbose) printf("test2 checking memory\n");
//    test2(1);
    if (verbose) printf("test2 faster\n");
    test2(0, 2);
    test2(0, 27);
    test2(0, 212);
    test2(0, 4096);
    
    if (verbose) printf("test1 ok\n");
    return 0;
}
