/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2008 Tokutek Inc.  All rights reserved."

/* Insert N duplicates into a BRT.
 * Delete them with a single delete.
 * Close the BRT.
 * Check to see that the BRT is empty.
 */

#include "includes.h"
#include "test.h"

#include <unistd.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test_delete_all (void) {
    char fname[]= __FILE__ ".brt";
    u_int32_t limit =200;
    unlink(fname);
    CACHETABLE ct;
    int r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                                assert(r==0);
    BRT t;
    r = toku_brt_create(&t); assert(r==0);
    r = toku_brt_set_flags(t, TOKU_DB_DUP + TOKU_DB_DUPSORT); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, fname, fname, 0, 1, 1, ct, null_txn, (DB*)0); assert(r==0);
    u_int32_t i;
    for (i=0; i<limit; i++) {
	char key[100];
	char val[100];
	snprintf(key, 100, "%03u", limit/2);
	snprintf(val, 100, "%03u", i);
	DBT k,v;
	r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v,val, 1+strlen(val)), null_txn);
	assert(r == 0);
    }

    //printf("Initial insert done\n"); toku_dump_brt(stdout, t);

    // Now reopen the DB to force non-leaf buffering.
    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct);    assert(r==0);
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                                assert(r==0);

    r = toku_brt_create(&t); assert(r==0);
    r = toku_brt_open(t, fname, fname, 0, 0, 0, ct, null_txn, (DB*)0); assert(r==0);

    // Don't do a dump here, because that will warm the cachetable.  We want subsequent inserts to be buffered at the root.

    // Insert some more stuff
    if (1) {
	u_int32_t j;
	for (j=0; j<1; j++) {
	    char key[100];
	    char val[100];
	    snprintf(key, 100, "%03u", limit/2);
	    snprintf(val, 100, "%03u", limit+j);
	    DBT k,v;
	    r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v,val, 1+strlen(val)), null_txn);
	    assert(r == 0);
	}
    }

    // Don't do a dump here, because that will warm the cachetable.

    // Delete everything
    {
	char key[100];
	DBT k;
	snprintf(key, 100, "%03u", limit/2);
	r = toku_brt_delete(t, toku_fill_dbt(&k, key, 1+strlen(key)), null_txn);
	assert(r == 0);
    }

    //printf("Deleted\n"); toku_dump_brt(stdout, t);

    // Now use a cursor to see if it is all empty
    {
	BRT_CURSOR cursor = 0;
	r = toku_brt_cursor(t, &cursor, 0); assert(r==0);
	DBT kbt, vbt;
	toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
	toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
	r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_FIRST, null_txn);
	assert(r == DB_NOTFOUND);

	r = toku_brt_cursor_close(cursor);
	assert(r==0);
    }

    //printf("Looked\n"); toku_dump_brt(stdout, t);

    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct);    assert(r==0);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    test_delete_all();
    if (verbose) printf("test ok\n");
    return 0;
}

