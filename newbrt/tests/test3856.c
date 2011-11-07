/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

// it used to be the case that we copied the left and right keys of a
// range to be prelocked but never freed them, this test checks that they
// are freed (as of this time, this happens in destroy_bfe_for_prefetch)

#include "test.h"

#include "includes.h"

static const char fname[]= __FILE__ ".brt";

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;
static int const nodesize = 1<<12, basementnodesize = 1<<9;
static int const count = 1000;

static int
string_cmp(DB* UU(db), const DBT *a, const DBT *b)
{
    return strcmp(a->data, b->data);
}

static int
found(ITEMLEN UU(keylen), bytevec key, ITEMLEN UU(vallen), bytevec UU(val), void *UU(extra))
{
    assert(key != NULL);
    return 0;
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {

    CACHETABLE ct;
    BRT t;

    int r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, nodesize, basementnodesize, ct, null_txn, string_cmp, null_db); assert(r==0);

    for (int i = 0; i < count; ++i) {
        char key[100],val[100];
        DBT k,v;
        snprintf(key, 100, "hello%d", i);
        snprintf(val, 100, "there%d", i);
        r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
        assert(r==0);
    }
    r = toku_close_brt(t, 0); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    r = toku_open_brt(fname, 1, &t, nodesize, basementnodesize, ct, null_txn, string_cmp, null_db); assert(r == 0);

    for (int n = 0; n < count/100; ++n) {
        int i = n * 100;
        BRT_CURSOR c;
        char lkey[100],rkey[100];
        DBT lk, rk;
        r = toku_brt_cursor(t, &c, null_txn, FALSE); assert(r == 0);
        snprintf(lkey, 100, "hello%d", i);
        snprintf(rkey, 100, "hello%d", i + 100);
        toku_brt_cursor_set_range_lock(c, toku_fill_dbt(&lk, lkey, 1+strlen(lkey)),
                                       toku_fill_dbt(&rk, rkey, 1+strlen(rkey)),
                                       FALSE, FALSE);
        r = toku_brt_cursor_set(c, &lk, found, NULL); assert(r == 0);
        for (int j = 0; j < 100; ++j) {
            r = toku_brt_cursor_next(c, found, NULL); assert(r == 0);
        }
        r = toku_brt_cursor_close(c); assert(r == 0);
    }

    r = toku_close_brt(t, 0); assert(r == 0);
    r = toku_cachetable_close(&ct), assert(r == 0);

    return 0;
}
