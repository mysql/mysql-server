/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test_named_db (void) {
    const char *n0 = __FILE__ "0.brt";
    CACHETABLE ct;
    BRT t0;
    int r;
    DBT k,v;

    if (verbose) printf("test_named_db\n");
    unlink(n0);
    toku_memory_check_all_free();
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);           assert(r==0);
    r = toku_open_brt(n0, "db1", 1, &t0, 1<<12, ct, null_txn, toku_default_compare_fun, null_db); assert(r==0);

    toku_brt_insert(t0, toku_fill_dbt(&k, "good", 5), toku_fill_dbt(&v, "day", 4), null_txn); assert(r==0);

    r = toku_close_brt(t0, 0, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
    toku_memory_check_all_free();

    toku_memory_check_all_free();
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);           assert(r==0);
    r = toku_open_brt(n0, "db1", 0, &t0, 1<<12, ct, null_txn, toku_default_compare_fun, null_db); assert(r==0);

    {
	brt_lookup_and_check_nodup(t0, "good", "day");
    }

    r = toku_close_brt(t0, 0, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
    toku_memory_check_all_free();
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    test_named_db();
    toku_malloc_cleanup();
    if (verbose) printf("test_named_db ok\n");
    return 0;
}
