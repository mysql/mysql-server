/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"


// create a brt and put n rows into it
// write the brt to the file
// verify the rows in the brt
static void test_sub_block(int n) {
    if (verbose) printf("%s:%d %d\n", __FUNCTION__, __LINE__, n);

    const char fname[]= __SRCFILE__ ".ft_handle";
    const int nodesize = 4*1024*1024;
    const int basementnodesize = 128*1024;
    const enum toku_compression_method compression_method = TOKU_DEFAULT_COMPRESSION_METHOD;

    TOKUTXN const null_txn = 0;

    int error;
    CACHETABLE ct;
    FT_HANDLE brt;
    int i;

    unlink(fname);

    error = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(error == 0);

    error = toku_open_ft_handle(fname, true, &brt, nodesize, basementnodesize, compression_method, ct, null_txn, toku_builtin_compare_fun);
    assert(error == 0);

    // insert keys 0, 1, 2, .. (n-1)
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
        int v = i;
	DBT key, val;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_ft_insert(brt, &key, &val, 0);
        assert(error == 0);
    }

    // write to the file
    error = toku_close_ft_handle_nolsn(brt, 0);
    assert(error == 0);

    // verify the brt by walking a cursor through the rows
    error = toku_open_ft_handle(fname, false, &brt, nodesize, basementnodesize, compression_method, ct, null_txn, toku_builtin_compare_fun);
    assert(error == 0);

    FT_CURSOR cursor;
    error = toku_ft_cursor(brt, &cursor, NULL, false, false);
    assert(error == 0);

    for (i=0; ; i++) {
        int k = htonl(i);
        int v = i;
	struct check_pair pair = {sizeof k, &k, sizeof v, &v, 0};	
        error = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (error != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
    }
    assert(i == n);

    error = toku_ft_cursor_close(cursor);
    assert(error == 0);

    error = toku_close_ft_handle_nolsn(brt, 0);
    assert(error == 0);

    error = toku_cachetable_close(&ct);
    assert(error == 0);
}

int test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    const int meg = 1024*1024;
    const int row = 32;
    const int rowspermeg = meg/row;

    test_sub_block(1);
    test_sub_block(rowspermeg-1);
    int i;
    for (i=1; i<8; i++)
        test_sub_block(rowspermeg*i);
    
    if (verbose) printf("test ok\n");
    return 0;
}
