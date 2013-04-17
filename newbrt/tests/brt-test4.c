/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"
#include "toku_time.h"

static const char fname[]= __FILE__ ".brt";

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test4 (int nodesize, int count, int memcheck) {
    BRT t;
    int r;
    struct timeval t0,t1;
    int i;
    CACHETABLE ct;
    gettimeofday(&t0, 0);
    unlink(fname);
    toku_memory_check=memcheck;
    toku_memory_check_all_free();
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);         assert(r==0);
    r = toku_open_brt(fname, 1, &t, nodesize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);
    for (i=0; i<count; i++) {
	char key[100],val[100];
	int rv = random();
	DBT k,v;
	snprintf(key,100,"hello%d",rv);
	snprintf(val,100,"there%d",i);
	toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
    }
    r = toku_verify_brt(t); assert(r==0);
    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
    toku_memory_check_all_free();
    gettimeofday(&t1, 0);
    {
	double diff = toku_tdiff(&t1, &t0);
	if (verbose) printf("random insertions: blocksize=%d %d insertions in %.3f seconds, %.2f insertions/second\n", nodesize, count, diff, count/diff);
    }
}

static void brt_blackbox_test (void) {
    test4(2048, 1<<14, 1);

    if (0) {

	if (verbose) printf("test4 slow\n");
	test4(2048, 1<<15, 1);

	//if (verbose) toku_pma_show_stats();

	test4(1<<15, 1024, 1);

	test4(1<<18, 1<<20, 0);

	// Once upon a time srandom(8) caused this test to fail.
	srandom(8); test4(2048, 1<<15, 1);
    }
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    brt_blackbox_test();
    toku_malloc_cleanup();
    if (verbose) printf("test ok\n");
    return 0;
}
