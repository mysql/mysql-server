/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <db.h>

#include "test.h"

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

DBT *dbt_init_malloc(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_MALLOC;
    return dbt;
}

void db_put(DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    assert(r == 0);
}

void test_db_current() {
    if (verbose) printf("test_db_current\n");

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.db.current.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    
    /* insert <1,1> */
    int k = 1, v = 1;
    db_put(db, k, v);
    
    DBC *cursor;

    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    DBT key, data; int kk, vv;

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_CURRENT);
    assert(r == EINVAL);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_FIRST);
    assert(r == 0);
    assert(key.size == sizeof kk);
    memcpy(&kk, key.data, sizeof kk);
    assert(kk == 1);
    assert(data.size == sizeof vv);
    memcpy(&vv, data.data, data.size);
    assert(vv == 1);
    free(key.data); free(data.data);

    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&data), DB_CURRENT);
    assert(r == 0);
    assert(key.size == sizeof kk);
    memcpy(&kk, key.data, sizeof kk);
    assert(kk == 1);
    assert(data.size == sizeof vv);
    memcpy(&vv, data.data, data.size);
    assert(vv == 1);
    free(key.data); free(data.data);

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    test_db_current();

    return 0;
}
