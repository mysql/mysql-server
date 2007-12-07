/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"



void db_put(DB *db, int k, int v) {
    DBT key, val;
    int r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    assert(r == 0);
}

void expect_db_del(DB *db, int k, int flags, int expectr) {
    DBT key;
    int r = db->del(db, 0, dbt_init(&key, &k, sizeof k), flags);
    assert(r == expectr);
}

void expect_db_get(DB *db, int k, int expectr) {
    DBT key, val;
    int r = db->get(db, 0, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == expectr);
}

void test_db_delete(int n, int dup_mode) {
    if (verbose) printf("test_db_delete:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.db.delete.brt";
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n/2 <i, i> pairs */
    int i;
    for (i=0; i<n/2; i++) 
        db_put(db, htonl(i), i);

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n/2 <i, i> pairs */
    for (i=n/2; i<n; i++) 
        db_put(db, htonl(i), i);

    for (i=0; i<n; i++) {
        expect_db_del(db, htonl(i), 0, 0);

        expect_db_get(db, htonl(i), DB_NOTFOUND);
    }

    expect_db_del(db, htonl(n), 0, DB_NOTFOUND);
#if USE_TDB
    expect_db_del(db, htonl(n), DB_DELETE_ANY, 0);
#endif
#if USE_BDB && defined(DB_DELETE_ANY)
    expect_db_del(db, htonl(n), DB_DELETE_ANY, EINVAL);
#endif

    r = db->close(db, 0);
    assert(r == 0);
}

void test_db_get_datasize0() {
    if (verbose) printf("test_db_get_datasize0\n");

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test.db_delete.brt";
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int k = 0;
    db_put(db, k, 0);

    DBT key, val; 
    r = db->get(db, 0, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == 0);
    free(val.data);

    r = db->close(db, 0);
    assert(r == 0);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    test_db_get_datasize0();

    test_db_delete(0, 0);

    int i;
    for (i = 1; i <= (1<<16); i *= 2) {
        test_db_delete(i, 0);
    }

    return 0;
}
