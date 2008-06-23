/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"

uint64_t lorange = 0;
uint64_t hirange = 1<<24;

void test_key_size_limit(int dup_mode) {
    if (verbose > 1) printf("%s:%d\n", __FUNCTION__, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = ENVDIR "/" "test.rand.insert.brt";
    int r;

    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    uint32_t lo = lorange, mi = 0, hi = hirange;
    uint32_t bigest = 0;
    while (lo <= hi) {
        mi = lo + (hi - lo) / 2;
        assert(lo <= mi && mi <= hi);
        uint32_t ks = mi;
        if (verbose > 1) printf("trying %u %u %u ks=%u\n", lo, mi, hi, ks);
        k = realloc(k, ks); assert(k);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        uint32_t vs = sizeof (uint32_t);
        v = realloc(v, vs); assert(v);
        memset(v, 0, vs);
        memcpy(v, &vs, sizeof vs);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, k, ks), dbt_init(&val, v, vs), DB_YESOVERWRITE);
        if (r == 0) {
            bigest = mi;
            lo = mi+1;
        } else {
            if (verbose > 1) printf("%u too big\n", ks);
            hi = mi-1;
        }
    }
    free(k);
    free(v);
    assert(bigest > 0);
    if (verbose && bigest >= 0) printf("%s bigest %d\n", __FUNCTION__, bigest);

    r = db->close(db, 0);
    assert(r == 0);
}

void test_data_size_limit(int dup_mode) {
    if (verbose > 1) printf("%s:%d\n", __FUNCTION__, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = ENVDIR "/" "test.rand.insert.brt";
    int r;

    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    uint32_t lo = lorange, mi = 0, hi = hirange;
    uint32_t bigest = 0;
    while (lo <= hi) {
        mi = lo + (hi - lo) / 2;
        assert(lo <= mi && mi <= hi);
        uint32_t ks = sizeof (uint32_t);
        if (verbose > 1) printf("trying %u %u %u ks=%u\n", lo, mi, hi, ks);
        k = realloc(k, ks); assert(k);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        uint32_t vs = mi;
        v = realloc(v, vs); assert(v);
        memset(v, 0, vs);
        memcpy(v, &vs, sizeof vs);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, k, ks), dbt_init(&val, v, vs), DB_YESOVERWRITE);
        if (r == 0) {
            bigest = mi;
            lo = mi+1;
        } else {
            if (verbose > 1) printf("%u too big\n", vs);
            hi = mi-1;
        }
    }
    free(k);
    free(v);
    if (verbose && bigest > 0) printf("%s bigest %d\n", __FUNCTION__, bigest);

    r = db->close(db, 0);
    assert(r == 0);
}

int main(int argc, const char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-lorange") == 0) {
            if (i+1 >= argc)
                return 1;
            lorange = strtoull(argv[++i], 0, 10);
            if (lorange > UINT_MAX)
                return 2;
            continue;
        }
        if (strcmp(arg, "-hirange") == 0) {
            if (i+1 >= argc) 
                return 1;
            hirange = strtoull(argv[++i], 0, 10);
            if (hirange > UINT_MAX)
                return 2;
            continue;
        }
    }

    test_key_size_limit(0);
    test_data_size_limit(0);
    test_key_size_limit(DB_DUP + DB_DUPSORT);
    test_data_size_limit(DB_DUP + DB_DUPSORT);

    return 0;
}
