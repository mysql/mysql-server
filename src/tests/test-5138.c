/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// test that with full optimizations including the "last IPO pass" and
// static linking doesn't break lzma

#include "test.h"

int test_main(int argc, char * const argv[]) {
    int r;
    parse_args(argc, argv);
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);
    CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL|DB_CREATE|DB_THREAD|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB *db;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0);
    CKERR(r);
    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_compression_method(db, TOKU_LZMA_METHOD);
    CKERR(r);
    r = db->open(db, txn, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);

    DBT key, val;
    unsigned int i;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, &i, sizeof(i));
    for (i = 0; i < 1000; ++i) {
        r = db->put(db, txn, keyp, valp, 0);
        CKERR(r);
    }

    r = txn->commit(txn, 0);
    CKERR(r);

    r = db->close(db, 0);
    CKERR(r);
    r = env->close(env, 0);
    CKERR(r);

    return 0;
}
