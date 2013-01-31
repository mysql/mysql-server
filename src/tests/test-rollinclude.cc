/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"

// insert enough rows with a child txn to force a rollinclude log entry

static void
populate(DB_ENV *env, DB *db, int nrows) {
    int r;
    DB_TXN *parent = NULL;
    r = env->txn_begin(env, NULL, &parent, 0); assert_zero(r);

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, parent, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = htonl(i);
        char kk[4096]; // 4 KB key
        memset(kk, 0, sizeof kk);
        memcpy(kk, &k, sizeof k);
        DBT key = { .data = &kk, .size = sizeof kk };
        DBT val = { .data = NULL, .size = 0 };
        r = db->put(db, txn, &key, &val, 0);
        assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);
    r = parent->commit(parent, 0); assert_zero(r);
}

static void
run_test(int nrows) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "0.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert_zero(r);

    populate(env, db, nrows);

    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;
    int nrows = 1024; // = 4 MB / 4KB assumes 4 MB rollback nodes

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(arg, "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
    }

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    run_test(nrows);

    return 0;
}

