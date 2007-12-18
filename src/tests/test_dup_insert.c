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
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    assert(r == 0);
}

void expect(DBC *cursor, int k, int v) {
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

/* verify that key insertions are stored in insert order */
void test_insert(int n, int dup_mode) {
    if (verbose) printf("test_insert:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_insert.brt";
    int r;
    int i;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int values[n];
    for (i=0; i<n; i++)
        values[i] = htonl((i << 16) + (random() & 0xffff));
    int sortvalues[n];
    for (i=0; i<n; i++)
        sortvalues[i] = values[i];
    int mycmp(const void *a, const void *b) {
        return memcmp(a, b, sizeof (int));
    }
    qsort(sortvalues, n, sizeof sortvalues[0], mycmp);

    /* insert n-1 unique keys {0, 1,  n-1} - {n/2} */
    for (i=0; i<n; i++) {
        if (i == n/2)
            continue;
        int k = htonl(i);
        int v = values[i];
        db_put(db, k, v);
    }

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = values[i];
        db_put(db, k, v);
    } 

    /* verify lookups */
    for (i=0; i<n; i++) {
        int k = htonl(i);
        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        if (i == n/2) {
            if (dup_mode & DB_DUPSORT)
                assert(vv == sortvalues[0]);
            else if (dup_mode & DB_DUP)
                assert(vv == values[0]);
            else
                assert(vv == values[n-1]);
        } else
            assert(vv == values[i]);
        free(val.data);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n/2; i++)
        expect(cursor, htonl(i), values[i]);

    if (dup_mode & DB_DUPSORT) {
        for (i=0; i<n; i++) 
            expect(cursor, htonl(n/2), sortvalues[i]);
    } else if (dup_mode & DB_DUP) {
        for (i=0; i<n; i++)
            expect(cursor, htonl(n/2), values[i]);
    } else {
        expect(cursor, htonl(n/2), values[n-1]);
    }

    for (i=(n/2)+1; i<n; i++)
        expect(cursor, htonl(i), values[i]);

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
}

/* verify dup keys are buffered in order in non-leaf nodes */
void test_nonleaf_insert(int n, int dup_mode) {
    if (verbose) printf("test_nonleaf_insert:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/" "test_nonleaf_insert.brt";
    int r;
    int i;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int values[n];
    for (i=0; i<n; i++)
        values[i] = htonl((i << 16) + (random() & 0xffff));
    int sortvalues[n];
    for (i=0; i<n; i++)
        sortvalues[i] = values[i];
    int mycmp(const void *a, const void *b) {
        return memcmp(a, b, sizeof (int));
    }
    qsort(sortvalues, n, sizeof sortvalues[0], mycmp);

    /* insert n-1 unique keys {0, 1,  n-1} - {n/2} */
    for (i=0; i<n; i++) {
        if (i == n/2)
            continue;
        int k = htonl(i);
        int v = values[i];
        db_put(db, k, v);
    }

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

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = values[i];
        db_put(db, k, v);
    } 

   /* verify lookups */
    for (i=0; i<n; i++) {
        int k = htonl(i);
        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        if (i == n/2) {
            if (dup_mode & DB_DUPSORT)
                assert(vv == sortvalues[0]);
            else if (dup_mode & DB_DUP)
                assert(vv == values[0]);
            else
                assert(vv == values[n-1]);
        } else
            assert(vv == values[i]);
        free(val.data);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n/2; i++)
        expect(cursor, htonl(i), values[i]);

    if (dup_mode & DB_DUPSORT) {
        for (i=0; i<n; i++) 
            expect(cursor, htonl(n/2), sortvalues[i]);
    } else if (dup_mode & DB_DUP) {
        for (i=0; i<n; i++)
            expect(cursor, htonl(n/2), values[i]);
    } else {
        expect(cursor, htonl(n/2), values[n-1]);
    }

    for (i=(n/2)+1; i<n; i++)
        expect(cursor, htonl(i), values[i]);

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
}

int main(int argc, const char *argv[]) {
    int i;

    parse_args(argc, argv);
  
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    
    /* nodup tests */
    for (i = 1; i <= (1<<16); i *= 2) {
        test_insert(i, 0);
        test_nonleaf_insert(i, 0);
    }

    /* dup tests */
#if USE_TDB
    printf("%s:%d:WARNING:tokudb does not support DB_DUP\n", __FILE__, __LINE__);
#else
    for (i = 1; i <= (1<<16); i *= 2) {
        test_insert(i, DB_DUP);
        test_nonleaf_insert(i, DB_DUP);
    }
#endif

    /* dupsort tests */
    for (i = 1; i <= (1<<16); i *= 2) {
        test_insert(i, DB_DUP + DB_DUPSORT);
        test_nonleaf_insert(i, DB_DUP + DB_DUPSORT);
    }

    return 0;
}
