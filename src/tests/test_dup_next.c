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



DBT *dbt_init_zero(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    return dbt;
}

void db_put(DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
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

void expect_cursor_set(DBC *cursor, int k, int expectv) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init_zero(&val), DB_SET); 
    assert(r == 0);
    assert(val.size == sizeof expectv);
    int vv;
    memcpy(&vv, val.data, val.size);
    assert(expectv == vv);
}

int expect_cursor_get(DBC *cursor, int expectk, int expectv, int op) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_zero(&key), dbt_init_zero(&val), op);
    if (r == 0) {
        assert(key.size == sizeof expectk);
        int kk;
        memcpy(&kk, key.data, key.size);
        assert(val.size == sizeof expectv);
        int vv;
        memcpy(&vv, val.data, val.size);
        if (kk != expectk || vv != expectv) printf("expect key %d got %d - %d %d\n", htonl(expectk), htonl(kk), htonl(expectv), htonl(vv));
        assert(kk == expectk);
        assert(vv == expectv);
    }
    return r;
}

void test_dup_next(int n, int dup_mode, int bracket_dups) {
    if (verbose) printf("test_dup_next:%d %d %d\n", n, dup_mode, bracket_dups);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_dup_next.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    db_put(db, 0, 0);
    if (bracket_dups) db_put(db, 2, 0);

    int i;
    for (i=0; i<n; i++) {
        int k = htonl(1);
        int v = htonl(i);
        db_put(db, k, v);
    } 

    /* use a cursor to iterate through the duplicates */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);

    r = expect_cursor_get(cursor, htonl(1), htonl(0), DB_NEXT_DUP); assert(r == EINVAL);

    expect_cursor_set(cursor, htonl(1), htonl(0));
    
    for (i=1; i<n; i++) {
        r = expect_cursor_get(cursor, htonl(1), htonl(i), DB_NEXT_DUP); assert(r == 0);
    }

    r = expect_cursor_get(cursor, htonl(1), htonl(i), DB_NEXT_DUP); assert(r == DB_NOTFOUND);

    r = expect_cursor_get(cursor, htonl(1), htonl(i-1), DB_CURRENT); assert(r == 0);

    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}

int main(int argc, const char *argv[]) {
    int i;

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    for (i = 1; i <= 65536; i *= 2) {
        test_dup_next(i, DB_DUP + DB_DUPSORT, 0);
        test_dup_next(i, DB_DUP + DB_DUPSORT, 1);
    }

    return 0;
}
