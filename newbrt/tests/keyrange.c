/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

#include <unistd.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test_flat (void) {
    char fname[]= __FILE__ ".brt";
    u_int64_t limit=30000;
    unlink(fname);
    CACHETABLE ct;
    int r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                                assert(r==0);
    BRT t;
    r = toku_open_brt(fname, 0, 1, &t, 1<<12, ct, null_txn, toku_default_compare_fun, null_db);   assert(r==0);
    u_int64_t i;
    for (i=0; i<limit; i++) {
	char key[100],val[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k,v;
	r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v,val, 1+strlen(val)), null_txn);
	assert(r == 0);
    }
    // flatten it.
    for (i=0; i<limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k,v;
	r = toku_brt_lookup(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_init_dbt(&v));
	assert(r==0);
    }
    for (i=0; i<limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k;
	u_int64_t less,equal,greater;
	r = toku_brt_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(less==(u_int64_t)i);
	assert(equal==1);
	assert(less+equal+greater == limit);
    }
    for (i=0; i<1+limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i);
	DBT k;
	u_int64_t less,equal,greater;
	r = toku_brt_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(less==(u_int64_t)i);
	assert(equal==0);
	assert(less+equal+greater == limit);
    }
    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct);    assert(r==0);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    test_flat();
    toku_malloc_cleanup();
    if (verbose) printf("test ok\n");
    return 0;
}

