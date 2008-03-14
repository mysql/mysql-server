/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "brt.h"
#include "key.h"
#include "pma.h"
#include "brt-internal.h"
#include "memory.h"
#include "toku_assert.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "test.h"

static const char fname[]= __FILE__ ".brt";

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

// Verify that different cursors return different data items when a DBT is initialized to all zeros (no flags)
static void test_multiple_brt_cursor_dbts(int n, DB *db) {
    if (verbose) printf("test_multiple_brt_cursors:%d %p\n", n, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursors[n];

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, toku_default_compare_fun, db);
    assert(r==0);

    int i;
    for (i=0; i<n; i++) {
	DBT kbt,vbt;
	char key[10],val[10];
	snprintf(key, sizeof key, "k%04d", i);
	snprintf(key, sizeof val, "k%04d", i);
	r = toku_brt_insert(brt,
			    toku_fill_dbt(&kbt, key, strlen(key)),
			    toku_fill_dbt(&vbt, val, strlen(val)),
			    0);
	assert(r == 0);
    }

    for (i=0; i<n; i++) {
        r = toku_brt_cursor(brt, &cursors[i]);
        assert(r == 0);
    }

    void *ptrs[n];
    for (i=0; i<n; i++) {
	DBT kbt, vbt;
	char key[10];
	snprintf(key, sizeof key, "k%04d", i);
	r = toku_brt_cursor_get(cursors[i],
				toku_fill_dbt(&kbt, key, strlen(key)),
				toku_init_dbt(&vbt),
				DB_SET,
				null_txn);
	assert(r == 0);
	ptrs[i] = vbt.data;
    }

    for (i=0; i<n; i++) {
	int j;
	for (j=i+1; j<n; j++) {
	    assert(ptrs[i]!=ptrs[j]);
	}
    }

    for (i=0; i<n; i++) {
        r = toku_brt_cursor_close(cursors[i]);
        assert(r == 0);
    }

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor(DB *db) {
    test_multiple_brt_cursor_dbts(1, db);
    test_multiple_brt_cursor_dbts(2, db);
    test_multiple_brt_cursor_dbts(3, db);
}


int main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    DB a_db;
    DB *db = &a_db;
    test_brt_cursor(db);

    toku_malloc_cleanup();
    if (verbose) printf("test ok\n");
    return 0;
}
