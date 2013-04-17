/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
/*
 * Test that different compression methods can be used.
 */

#include <db.h>
#include "test.h"

static const int VAL_SIZE = 248;
static const int NUM_ROWS = 1 << 12;

static int
insert(DB_ENV *env, DB *db, void *UU(extra))
{
    char val[VAL_SIZE];
    memset(val, 0, sizeof val);
    DB_TXN *txn;
    int r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
    for (int i = 0; i < NUM_ROWS; ++i) {
        DBT k, v;
        *((int *) val) = i;
        r = db->put(db, txn, dbt_init(&k, &i, sizeof i), dbt_init(&v, val, sizeof val), 0);
        CKERR(r);
    }
    r = txn->commit(txn, 0);
    CKERR(r);
    return 0;
}

static int
lookup(DB_ENV *env, DB *db, void *UU(extra))
{
    DB_TXN *txn;
    int r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
    for (int i = 0; i < NUM_ROWS; ++i) {
        DBT k, v;
        r = db->get(db, txn, dbt_init(&k, &i, sizeof i), dbt_init(&v, NULL, 0), 0);
        CKERR(r);
        assert(v.size == (size_t) VAL_SIZE);
        assert(*(int *) v.data == i);
    }
    r = txn->commit(txn, 0);
    CKERR(r);
    return 0;
}

typedef int (*db_callback)(DB_ENV *env, DB *db, void *extra);
static int
with_open_db(db_callback cb, void *cb_extra, bool set_method, enum toku_compression_method method)
{
    DB_ENV *env;
    DB *db;
    int r;
    r = db_env_create(&env, 0);
    CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = db_create(&db, env, 0);
    CKERR(r);
    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0);
        CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
        if (set_method) {
            r = db->set_compression_method(db, method);
            CKERR(r);
        }
        r = txn->commit(txn, 0);
        CKERR(r);
    }

    {
        enum toku_compression_method saved_method;
        r = db->get_compression_method(db, &saved_method);
        CKERR(r);
        assert(saved_method == method);
    }

    int cr = cb(env, db, cb_extra);

    r = db->close(db, 0);
    CKERR(r);
    r = env->close(env, 0);
    CKERR(r);

    return cr;
}

static void
run_test(enum toku_compression_method method)
{
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    r = with_open_db(insert, NULL, true, method);
    CKERR(r);
    r = with_open_db(lookup, NULL, false, method);
    CKERR(r);
}

int
test_main(int argc, char *const argv[])
{
    parse_args(argc, argv);
    run_test(TOKU_NO_COMPRESSION);
    run_test(TOKU_ZLIB_METHOD);
    run_test(TOKU_QUICKLZ_METHOD);
    run_test(TOKU_LZMA_METHOD);
    return 0;
}
