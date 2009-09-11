/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static void
test_insert_delete_insert (int dup_mode) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    if (verbose) printf("test_insert_delete_insert:%d\n", dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.cursor.insert.delete.insert.brt";

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    int k = htonl(1), v = 2;
    DBT key, val;

    r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE); 
    assert(r == 0);

    r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), DB_SET); 
    assert(r == 0);
    toku_free(val.data);

    r = cursor->c_del(cursor, 0); 
    assert(r == 0);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_CURRENT);
    assert(r == DB_KEYEMPTY);
    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);

    r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE); 
    assert(r == 0);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_CURRENT);
    assert(r == 0);
    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *argv[]) {

    parse_args(argc, argv);
  
    test_insert_delete_insert(0);
    test_insert_delete_insert(DB_DUP + DB_DUPSORT);

    return 0;
}
