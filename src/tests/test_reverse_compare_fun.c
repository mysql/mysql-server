/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* try a reverse compare function to verify that the database always uses the application's
   compare function */

#include <arpa/inet.h>
#include <assert.h>
#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "test.h"



int keycompare (const void *key1, unsigned int key1len, const void *key2, unsigned int key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	return -keycompare(key2,key2len,key1,key1len);
    }
}

int reverse_compare(DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return -keycompare(a->data, a->size, b->data, b->size);
}

void expect(DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
    CKERR(r);
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

void test_reverse_compare(int n) {
    if (verbose) printf("test_reverse_compare:%d\n", n);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/reverse.compare.db";

    int r;
    int i;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, 0);
    CKERR(r);
    r = db->set_pagesize(db, 4096);
    CKERR(r);
    r = db->set_bt_compare(db, reverse_compare);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);

    /* insert n unique keys {0, 1,  n-1} */
    for (i=0; i<n; i++) {
        DBT key, val;
        int k, v;
        k = htonl(i);
        dbt_init(&key, &k, sizeof k);
        v = i;
        dbt_init(&val, &v, sizeof v);
        r = db->put(db, null_txn, &key, &val, 0);
        CKERR(r);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    CKERR(r);
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, 0);
    CKERR(r);
    r = db->set_pagesize(db, 4096);
    CKERR(r);
    r = db->set_bt_compare(db, reverse_compare);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    CKERR(r);

    /* insert n unique keys {n, n+1,  2*n-1} */
    for (i=n; i<2*n; i++) {
        DBT key, val;
        int k, v;
        k = htonl(i);
        dbt_init(&key, &k, sizeof k);
        v = i;
        dbt_init(&val, &v, sizeof v);
        r = db->put(db, null_txn, &key, &val, 0);
        CKERR(r);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    CKERR(r);

    //for (i=0; i<2*n; i++) 
    for (i=2*n-1; i>=0; i--)
        expect(cursor, htonl(i), i);

    r = cursor->c_close(cursor);
    CKERR(r);

    r = db->close(db, 0);
    CKERR(r);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    int i;

    for (i = 1; i <= (1<<16); i *= 2) {
        test_reverse_compare(i);
    }
    return 0;
}
