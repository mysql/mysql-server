/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static const char fname[]= __SRCFILE__ ".ft_handle";

static TOKUTXN const null_txn = 0;

static int
save_data (ITEMLEN UU(keylen), bytevec UU(key), ITEMLEN vallen, bytevec val, void *v, bool lock_only) {
    if (lock_only) return 0;
    assert(key!=NULL);
    void **vp = v;
    *vp = toku_memdup(val, vallen);
    return 0;
}


// Verify that different cursors return different data items when a DBT is initialized to all zeros (no flags)
// Note: The BRT test used to implement DBTs with per-cursor allocated space, but there isn't any such thing any more
// so this test is a little bit obsolete.
static void test_multiple_ft_cursor_dbts(int n) {
    if (verbose) printf("test_multiple_ft_cursors:%d\n", n);

    int r;
    CACHETABLE ct;
    FT_HANDLE brt;
    FT_CURSOR cursors[n];

    unlink(fname);

    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    int i;
    for (i=0; i<n; i++) {
	DBT kbt,vbt;
	char key[10],val[10];
	snprintf(key, sizeof key, "k%04d", i);
	snprintf(val, sizeof val, "v%04d", i);
	r = toku_ft_insert(brt,
			    toku_fill_dbt(&kbt, key, 1+strlen(key)),
			    toku_fill_dbt(&vbt, val, 1+strlen(val)),
			    0);
	assert(r == 0);
    }

    for (i=0; i<n; i++) {
        r = toku_ft_cursor(brt, &cursors[i], NULL, FALSE, FALSE);
        assert(r == 0);
    }

    void *ptrs[n];
    for (i=0; i<n; i++) {
	DBT kbt;
	char key[10];
	snprintf(key, sizeof key, "k%04d", i);
	r = toku_ft_cursor_get(cursors[i],
				toku_fill_dbt(&kbt, key, 1+strlen(key)),
				save_data,
				&ptrs[i],
				DB_SET);
	assert(r == 0);
    }

    for (i=0; i<n; i++) {
	int j;
	for (j=i+1; j<n; j++) {
	    assert(strcmp(ptrs[i],ptrs[j])!=0);
	}
    }

    for (i=0; i<n; i++) {
        r = toku_ft_cursor_close(cursors[i]);
        assert(r == 0);
	toku_free(ptrs[i]);
    }

    r = toku_close_ft_handle_nolsn(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_ft_cursor(void) {
    test_multiple_ft_cursor_dbts(1);
    test_multiple_ft_cursor_dbts(2);
    test_multiple_ft_cursor_dbts(3);
}


int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    test_ft_cursor();
    if (verbose) printf("test ok\n");
    return 0;
}
