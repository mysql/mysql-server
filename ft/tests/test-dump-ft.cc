/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// Test the toku_dump_ft() call that is available in the debugger.
#include <stdio.h>

#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

int
test_main(int argc, const char *argv[]) {
    default_parse_args (argc, argv);
    const char *n = TOKU_TEST_FILENAME;
    int r;
    FT_HANDLE t;
    CACHETABLE ct;
    FILE *f = fopen("test-dump-ft.out", "w");
    unlink(n);
    assert(f);
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(n, 1, &t, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);
    int i;
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	DBT k,v;
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	toku_ft_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
    }
    r = toku_dump_ft(f, t); assert(r==0);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
    fclose(f);
    toku_os_recursive_delete("test-dump-ft.out");
    return 0;
}
