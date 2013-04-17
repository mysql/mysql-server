/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"


#include <stdio.h>

#include <db.h>

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    DB_ENV *env;
    DB *db;
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, ENVDIR, DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_INIT_LOG, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_pagesize(db, 10000);
    CKERR(r);

    const char * const fname = "test.change_pagesize";
    r = db->open(db, NULL, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    DB_TXN* txn;
    r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
    for (uint64_t i = 0; i < 10000; i++) {
        DBT key, val;
        uint64_t k = i;
        uint64_t v = i;
        dbt_init(&key, &k, sizeof k);
        dbt_init(&val, &v, sizeof v);
        db->put(db, txn, &key, &val, DB_PRELOCKED_WRITE); // adding DB_PRELOCKED_WRITE just to make the test go faster
    }
    r = txn->commit(txn, 0);
    CKERR(r);

    // now we change the pagesize. In 6.1.0, this would eventually cause a crash
    r = db->change_pagesize(db, 1024);
    CKERR(r);

    r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
    for (uint64_t i = 0; i < 10000; i++) {
        DBT key, val;
        uint64_t k = 10000+i;
        uint64_t v = i;
        dbt_init(&key, &k, sizeof k);
        dbt_init(&val, &v, sizeof v);
        db->put(db, txn, &key, &val, DB_PRELOCKED_WRITE); // adding DB_PRELOCKED_WRITE just to make the test go faster
    }
    r = txn->commit(txn, 0);
    CKERR(r);

    r = db->close(db, 0);
    CKERR(r);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
