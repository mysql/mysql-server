/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "includes.h"
#include "../brttypes.h"
#include "../txn.h"

/* 
 * a test of the txn filenums to ignore utilities:
 *  - toku_txn_ignore_create()
 *  - toku_txn_ignore_free()
 *  - toku_txn_ignore_add()
 *  - toku_txn_ignore_delete()
 *  - toku_txn_ignore_contains()
 */

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    TOKUTXN txn = (TOKUTXN) toku_malloc(sizeof(struct tokutxn));

    int r;

    r = toku_txn_ignore_init(txn);             CKERR(r);
    
    FILENUM f1 = {1};
    FILENUM f2 = {2};
    FILENUM f3 = {3};
    FILENUM f4 = {4};
    FILENUM f5 = {5};
    FILENUM f6 = {6};
    FILENUM f7 = {7};
    FILENUM f8 = {8};
    FILENUM f9 = {9};

    r = toku_txn_ignore_add(txn, f1);          CKERR(r);
    r = toku_txn_ignore_add(txn, f3);          CKERR(r);
    r = toku_txn_ignore_add(txn, f5);          CKERR(r);
    r = toku_txn_ignore_add(txn, f7);          CKERR(r);
    r = toku_txn_ignore_add(txn, f9);          CKERR(r);
    r = toku_txn_ignore_remove(txn, f3);       CKERR(r);
    r = toku_txn_ignore_remove(txn, f2);       assert( r == ENOENT );

    r = toku_txn_ignore_contains(txn, f1);     CKERR(r);
    r = toku_txn_ignore_contains(txn, f2);     assert( r == ENOENT );
    r = toku_txn_ignore_contains(txn, f3);     assert( r == ENOENT );
    r = toku_txn_ignore_contains(txn, f4);     assert( r == ENOENT );
    r = toku_txn_ignore_contains(txn, f5);     CKERR(r);
    r = toku_txn_ignore_contains(txn, f6);     assert( r == ENOENT );
    r = toku_txn_ignore_contains(txn, f7);     CKERR(r);
    r = toku_txn_ignore_contains(txn, f8);     assert( r == ENOENT );
    r = toku_txn_ignore_contains(txn, f9);     CKERR(r);

    assert(txn->ignore_errors.fns_allocated == 8);
    assert(txn->ignore_errors.filenums.num == 4);


    r = toku_txn_ignore_add(txn, f2);          CKERR(r);
    r = toku_txn_ignore_add(txn, f3);          CKERR(r);
    r = toku_txn_ignore_add(txn, f4);          CKERR(r);
    r = toku_txn_ignore_add(txn, f6);          CKERR(r);
    r = toku_txn_ignore_add(txn, f8);          CKERR(r);

    TXN_IGNORE txni = &(txn->ignore_errors);  // test using code similar to that in txn.c
    assert(txni->fns_allocated == 16);
    assert(txni->filenums.num == 9);

    // check that dups are ignored
    for (int i=0;i<10;i++) {
        r = toku_txn_ignore_add(txn, f2);          CKERR(r);
    }
    assert(txn->ignore_errors.fns_allocated == 16);
    assert(txn->ignore_errors.filenums.num == 9);

    toku_txn_ignore_free(txn);

    toku_free(txn);

    return 0;
}
