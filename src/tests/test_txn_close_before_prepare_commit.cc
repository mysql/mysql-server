/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

// Recreate a mysqld crash by closing and opening a db within a transaction.
// The crash occurs when writing a dirty cachetable pair, so we insert one
// row.
static void
test_txn_close_before_prepare_commit (void) {

    { int chk_r = system("rm -rf " ENVDIR); CKERR(chk_r); }
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    env->set_errfile(env, stdout);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, NULL, "test.db", 0, DB_BTREE, DB_CREATE|DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    DB_TXN *txn = 0;
    r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

    DBT key, val;
    int k = 1, v = 1;
    r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    assert(r == 0);

    // Close before commit
    r = db->close(db, 0); assert(r == 0);

    uint8_t gid[DB_GID_SIZE];
    memset(gid, 1, DB_GID_SIZE);
    r = txn->prepare(txn, gid);   assert(r == 0);
    r = txn->commit(txn, 0); assert(r == 0);

    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int UU(argc), char UU(*const argv[])) {
    test_txn_close_before_prepare_commit();
    return 0;
}
