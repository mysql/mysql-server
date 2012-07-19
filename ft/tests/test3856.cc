/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

// it used to be the case that we copied the left and right keys of a
// range to be prelocked but never freed them, this test checks that they
// are freed (as of this time, this happens in destroy_bfe_for_prefetch)

#include "test.h"

#include "includes.h"

static const char fname[]= __SRCFILE__ ".ft_handle";

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;
static int const nodesize = 1<<12, basementnodesize = 1<<9;
static const enum toku_compression_method compression_method = TOKU_DEFAULT_COMPRESSION_METHOD;
static int const count = 1000;

static int
string_cmp(DB* UU(db), const DBT *a, const DBT *b)
{
    return strcmp((char*)a->data, (char*)b->data);
}

static int
found(ITEMLEN UU(keylen), bytevec key, ITEMLEN UU(vallen), bytevec UU(val), void *UU(extra), bool lock_only)
{
    assert(key != NULL && !lock_only);
    return 0;
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {

    CACHETABLE ct;
    FT_HANDLE t;

    int r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, nodesize, basementnodesize, compression_method, ct, null_txn, string_cmp); assert(r==0);

    for (int i = 0; i < count; ++i) {
        char key[100],val[100];
        DBT k,v;
        snprintf(key, 100, "hello%d", i);
        snprintf(val, 100, "there%d", i);
        r = toku_ft_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
        assert(r==0);
    }
    r = toku_close_ft_handle_nolsn(t, 0); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    r = toku_open_ft_handle(fname, 1, &t, nodesize, basementnodesize, compression_method, ct, null_txn, string_cmp); assert(r == 0);

    for (int n = 0; n < count/100; ++n) {
        int i = n * 100;
        FT_CURSOR c;
        char lkey[100],rkey[100];
        DBT lk, rk;
        r = toku_ft_cursor(t, &c, null_txn, FALSE, FALSE); assert(r == 0);
        snprintf(lkey, 100, "hello%d", i);
        snprintf(rkey, 100, "hello%d", i + 100);
        toku_ft_cursor_set_range_lock(c, toku_fill_dbt(&lk, lkey, 1+strlen(lkey)),
                                       toku_fill_dbt(&rk, rkey, 1+strlen(rkey)),
                                       FALSE, FALSE);
        r = toku_ft_cursor_set(c, &lk, found, NULL); assert(r == 0);
        for (int j = 0; j < 100; ++j) {
            r = toku_ft_cursor_next(c, found, NULL); assert(r == 0);
        }
        r = toku_ft_cursor_close(c); assert(r == 0);
    }

    r = toku_close_ft_handle_nolsn(t, 0); assert(r == 0);
    r = toku_cachetable_close(&ct), assert(r == 0);

    return 0;
}
