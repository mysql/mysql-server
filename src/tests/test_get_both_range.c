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

DBT *dbt_init_user(DBT *d, void *uptr, int ulen) {
    memset(d, 0, sizeof *d);
    d->data = uptr;
    d->ulen = ulen;
    d->flags = DB_DBT_USERMEM;
    return d;
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
    if (kk != k || vv != v) printf("expect key %d got %d - %d %d\n", ntohl(k), ntohl(kk), ntohl(v), ntohl(vv));
    assert(kk == k);
    assert(vv == v);

    free(key.data);
    free(val.data);
}

void expect_cursor_set(DBC *cursor, int k) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), DB_SET);
    assert(r == 0);
    free(val.data);
}

void expect_cursor_get_both_range(DBC *cursor, int k, int v, int expectr) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_GET_BOTH_RANGE);
    assert(r == expectr);
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

void test_get_both(int n, int dup_mode, int op) {
    if (verbose) printf("test_get_both_range:%d %d %d\n", n, dup_mode, op);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = ENVDIR "/" "test_icdi_search.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    /* insert n unique kv pairs */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(10*i);
        int v = htonl(0);
        db_put(db, k, v);
    } 

    if (dup_mode) {
        /* insert a bunch of duplicate kv pairs */
        for (i=1; i<n; i++) {
            int k = htonl(10*(n/2));
            int v = htonl(10*i);
            db_put(db, k, v);
        }
    }

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);
    for (i=0; i<10*n; i++) {
        int k = htonl(i);
        int j;
        for (j=0; j<10*n; j++) {
            int v = htonl(j);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), op);
            if (r == 0) {
                assert((i % 10) == 0);
                int kk, vv;
                r = cursor->c_get(cursor, dbt_init_user(&key, &kk, sizeof kk), dbt_init_user(&val, &vv, sizeof vv), DB_CURRENT);
                assert(r == 0);
                assert(key.size == sizeof kk);
                kk = htonl(kk);
                assert(val.size == sizeof vv);
                vv = htonl(vv);
                if (verbose > 1) printf("%d %d -> %d %d\n", i, j, kk, vv);
                assert(kk == i);
                assert(vv == ((j+9)/10)*10);
            } else if (r == DB_NOTFOUND) {
                if ((i%10) != 0 || j > 0)
                    ;
                else
                    printf("nf %d %d\n", i, j);
            } else
                assert(0);
        }
    }
    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}


int main(int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-v")) {
            verbose++;
            continue;
        }
    }
  
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    for (i=1; i <= 256; i *= 2) {
        test_get_both(i, 0, DB_GET_BOTH);
        test_get_both(i, 0, DB_GET_BOTH_RANGE);
        test_get_both(i, DB_DUP + DB_DUPSORT, DB_GET_BOTH_RANGE);
    }

    return 0;
}
