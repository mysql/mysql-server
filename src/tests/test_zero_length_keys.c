/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <db.h>

#include "test.h"
#ifndef DIR
#define DIR "./test.dir"
#endif

void walk(DB *db) {
    int r;
    DB_TXN * const null_txn = 0;

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    DBT key; memset(&key, 0, sizeof key); key.flags = DB_DBT_REALLOC;
    DBT val; memset(&val, 0, sizeof val); val.flags = DB_DBT_REALLOC;
    int i;
    for (i=0; ; i++) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0) 
            break;
        if (verbose) printf("%d %d %d\n", i, key.size, val.size);
        if (i == 0) assert(key.size == 0);
    }
    assert(i != 0);
    r = cursor->c_close(cursor); assert(r == 0);

    if (key.data) free(key.data);
    if (val.data) free(val.data);
}

void test_insert_zero_length(int n, int dup_mode) {
    if (verbose) printf("test_insert_zero_length:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_insert.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        char k[n]; memset(k, i, n);
        char v[n]; memset(v, i, n);
        DBT key;
        DBT val;
        r = db->put(db, null_txn, dbt_init(&key, &k, i), dbt_init(&val, &v, i), 0);
        if (r != 0) {
            if (verbose) printf("db->put %d %d = %d\n", n, n, r);
            assert(r == 0);
        }
    }

    walk(db);

    r = db->close(db, 0); assert(r == 0);
}

void test_insert_zero_length_keys(int n, int dup_mode) {
    if (verbose) printf("test_insert_zero_length_keys:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_insert.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        char k[n]; memset(k, i, n);
        char v[n]; memset(v, i, n);
        DBT key;
        DBT val;
        r = db->put(db, null_txn, dbt_init(&key, &k, 0), dbt_init(&val, &v, i), DB_YESOVERWRITE);
        if (r != 0) {
            if (verbose) printf("db->put %d %d = %d\n", n, n, r);
            assert(r == 0);
        }
    }

    walk(db);

    r = db->close(db, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    test_insert_zero_length(32, 0);
    test_insert_zero_length_keys(32, 0);
    test_insert_zero_length_keys(32, DB_DUP+DB_DUPSORT);

    return 0;
}
