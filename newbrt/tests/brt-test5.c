/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test5 (void) {
    int r;
    BRT t;
    int limit=100000;
    int *values;
    int i;
    CACHETABLE ct;
    char fname[]= __FILE__ ".brt";
    toku_memory_check_all_free();
    MALLOC_N(limit,values);
    for (i=0; i<limit; i++) values[i]=-1;
    unlink(fname);
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);        assert(r==0);
    r = toku_open_brt(fname, 0, 1, &t, 1<<12, ct, null_txn, toku_default_compare_fun, null_db);   assert(r==0);
    for (i=0; i<limit/2; i++) {
	char key[100],val[100];
	int rk = random()%limit;
	int rv = random();
	if (i%1000==0 && verbose) { printf("w"); fflush(stdout); }
	values[rk] = rv;
	snprintf(key, 100, "key%d", rk);
	snprintf(val, 100, "val%d", rv);
	DBT k,v;
	toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
    }
    if (verbose) printf("\n");
    for (i=0; i<limit/2; i++) {
	int rk = random()%limit;
	if (values[rk]>=0) {
	    char key[100], valexpected[100];
	    DBT k;
	    if (i%1000==0 && verbose) { printf("r"); fflush(stdout); }
	    snprintf(key, 100, "key%d", rk);
	    snprintf(valexpected, 100, "val%d", values[rk]);
	    struct check_pair pair = {1+strlen(key), key, 1+strlen(valexpected), valexpected, 0};
	    r = toku_brt_lookup(t, toku_fill_dbt(&k, key, 1+strlen(key)), NULL, lookup_checkf, &pair);
	    assert(r==0);
	    assert(pair.call_count==1);
	}
    }
    if (verbose) printf("\n");
    toku_free(values);
    r = toku_close_brt(t, 0, 0);    assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
    toku_memory_check_all_free();
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    test5();
    toku_malloc_cleanup();
    if (verbose) printf("test ok\n");
    return 0;
}
