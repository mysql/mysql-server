/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "includes.h"


#include "test.h"
static const char fname[]= __SRCFILE__ ".ft_handle";

static TOKUTXN const null_txn = 0;
CACHETABLE ct;
FT_HANDLE brt;
FT_CURSOR cursor;

static int test_ft_cursor_keycompare(DB *db __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}
int
test_main (int argc __attribute__((__unused__)), const char *argv[]  __attribute__((__unused__))) {
    int r;

    unlink(fname);

    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                               assert(r==0);
    r = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);   assert(r==0);
    r = toku_ft_cursor(brt, &cursor, NULL, false, false);               assert(r==0);

    int i;
    for (i=0; i<1000; i++) {
	char string[100];
	snprintf(string, sizeof(string), "%04d", i);
	DBT key,val;
	r = toku_ft_insert(brt, toku_fill_dbt(&key, string, 5), toku_fill_dbt(&val, string, 5), 0);       assert(r==0);
    }

    {
	struct check_pair pair = {5, "0000", 5, "0000", 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);    assert(r==0); assert(pair.call_count==1);
    }
    {
	struct check_pair pair = {5, "0001", 5, "0001", 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);    assert(r==0); assert(pair.call_count==1);
    }

    // This will invalidate due to the root counter bumping, but the OMT itself will still be valid.
    {
	DBT key, val;
	r = toku_ft_insert(brt, toku_fill_dbt(&key, "d", 2), toku_fill_dbt(&val, "w", 2), 0);   assert(r==0);
    }

    {
	struct check_pair pair = {5, "0002", 5, "0002", 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);    assert(r==0); assert(pair.call_count==1);
    }

    r = toku_ft_cursor_close(cursor);                                                           assert(r==0);
    r = toku_close_ft_handle_nolsn(brt, 0);                                                               assert(r==0);
    r = toku_cachetable_close(&ct);                                                              assert(r==0);
    return 0;
}
