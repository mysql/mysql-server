/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <memory.h>
#include <db.h>

static void
db_put (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    CKERR(r);
}

static void
test_cursor_current (void) {
    if (verbose) printf("test_cursor_current\n");

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.cursor.current.brt";
    int r;

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); CKERR(r);

    int k = 42, v = 42000;
    db_put(db, k, v);
    db_put(db, 43, 2000);
 
    DBC *cursor;

    r = db->cursor(db, null_txn, &cursor, 0); CKERR(r);

    DBT key, data; int kk, vv;

    r = cursor->c_del(cursor, 0);
    assert(r == EINVAL);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_CURRENT);
    assert(r == EINVAL);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_FIRST);
    CKERR(r);
    assert(key.size == sizeof kk);
    memcpy(&kk, key.data, sizeof kk);
    assert(kk == k);
    assert(data.size == sizeof vv);
    memcpy(&vv, data.data, data.size);
    assert(vv == v);
    toku_free(key.data); toku_free(data.data);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_CURRENT);
    CKERR(r);
    assert(key.size == sizeof kk);
    memcpy(&kk, key.data, sizeof kk);
    assert(kk == k);
    assert(data.size == sizeof vv);
    memcpy(&vv, data.data, data.size);
    assert(vv == v);
    toku_free(key.data); toku_free(data.data);

    r = cursor->c_del(cursor, 0); 
    CKERR(r);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_CURRENT);
    CKERR2(r,DB_KEYEMPTY);

    r = cursor->c_del(cursor, 0); 
    CKERR2(r,DB_KEYEMPTY);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_CURRENT);
    CKERR2(r,DB_KEYEMPTY);

    r = cursor->c_close(cursor); CKERR(r);

    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static void
db_get (DB *db, int k, int UU(v), int expectr) {
    DBT key, val;
    int r = db->get(db, 0, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == expectr);
}

static void
test_reopen (void) {
    if (verbose) printf("test_reopen\n");

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.cursor.current.brt";
    int r;

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); CKERR(r);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666); CKERR(r);

    db_get(db, 1, 1, DB_NOTFOUND);

    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
  
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    test_cursor_current();
    test_reopen();

    return 0;
}
