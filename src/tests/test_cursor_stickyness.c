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



void db_put(DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    assert(r == 0);
}

void db_get(DB *db, int k) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == 0);
    int vv;
    assert(val.size == sizeof vv);
    memcpy(&vv, val.data, val.size);
    printf("do_search %d\n", htonl(vv));
    free(val.data);
}

void db_del(DB *db, int k) {
    DB_TXN * const null_txn = 0;
    DBT key;
    int r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
    assert(r == 0);
}

void expect_db_get(DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == 0);
    int vv;
    assert(val.size == sizeof vv);
    memcpy(&vv, val.data, val.size);
    assert(vv == v);
    free(val.data);
}

int cursor_get(DBC *cursor, int *k, int *v, int op) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), op);
    if (r == 0) {
        assert(key.size == sizeof *k); memcpy(k, key.data, key.size);
        assert(val.size == sizeof *v); memcpy(v, val.data, val.size);
    }
    if (key.data) free(key.data);
    if (val.data) free(val.data);
    return r;
}

void expect_cursor_get(DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
    assert(r == 0);
    assert(key.size == sizeof k);
    int kk;
    memcpy(&kk, key.data, key.size);
    assert(val.size == sizeof v);
    int vv;
    memcpy(&vv, val.data, val.size);
    if (kk != k || vv != v) printf("expect key %d got %d - %d %d\n", htonl(k), htonl(kk), htonl(v), htonl(vv));
    assert(kk == k);
    assert(vv == v);

    free(key.data);
    free(val.data);
}

void expect_cursor_set(DBC *cursor, int k, int expectr) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), DB_SET);
    assert(r == expectr);
    if (val.data) free(val.data);
}

void expect_cursor_get_both(DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_GET_BOTH);
    assert(r == 0);
}

void expect_cursor_get_current(DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_CURRENT);
    assert(r == 0);
    int kk, vv;
    assert(key.size == sizeof kk); memcpy(&kk, key.data, key.size); assert(kk == k);
    assert(val.size == sizeof vv); memcpy(&vv, val.data, val.size); assert(vv == v);
    free(key.data); free(val.data);
}


void test_cursor_sticky(int n, int dup_mode) {
    if (verbose) printf("test_cursor_sticky:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_cursor_sticky.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    int k, v;
    for (i=0; i<n; i++) {
        db_put(db, htonl(i), htonl(i));
    } 

    /* walk the tree */
    DBC *cursor;
    r = db->cursor(db, 0, &cursor, 0); assert(r == 0);
    for (i=0; i<n; i++) {
        r = cursor_get(cursor, &k, &v, DB_NEXT); assert(r == 0);
        assert(k == htonl(i)); assert(v == htonl(i));
    }

    r = cursor_get(cursor, &k, &v, DB_NEXT); assert(r == DB_NOTFOUND);

    r = cursor_get(cursor, &k, &v, DB_CURRENT); assert(r == 0); assert(k == htonl(n-1)); assert(v == htonl(n-1));

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}


int main(int argc, const char *argv[]) {
    int i;

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    for (i=1; i<65537; i *= 2) {
        test_cursor_sticky(i, 0);
    }
    return 0;
}
