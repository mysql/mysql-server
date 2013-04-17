/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


u_int64_t lorange = 0;
u_int64_t hirange = 1<<24;

static void
test_key_size_limit (int dup_mode) {
    if (verbose > 1) printf("%s:%d\n", __FUNCTION__, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.rand.insert.brt";
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    u_int32_t lo = lorange, mi = 0, hi = hirange;
    u_int32_t bigest = 0;
    while (lo <= hi) {
        mi = lo + (hi - lo) / 2;
        assert(lo <= mi && mi <= hi);
        u_int32_t ks = mi;
        if (verbose > 1) printf("trying %u %u %u ks=%u\n", lo, mi, hi, ks);
        k = toku_realloc(k, ks); assert(k);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        u_int32_t vs = sizeof (u_int32_t);
        v = toku_realloc(v, vs); assert(v);
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
    toku_free(k);
    toku_free(v);
    assert(bigest > 0);
    if (verbose) printf("%s bigest %u\n", __FUNCTION__, bigest);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

static void
test_data_size_limit (int dup_mode) {
    if (verbose > 1) printf("%s:%d\n", __FUNCTION__, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.rand.insert.brt";
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    u_int32_t lo = lorange, mi = 0, hi = hirange;
    u_int32_t bigest = 0;
    while (lo <= hi) {
        mi = lo + (hi - lo) / 2;
        assert(lo <= mi && mi <= hi);
        u_int32_t ks = sizeof (u_int32_t);
        if (verbose > 1) printf("trying %u %u %u ks=%u\n", lo, mi, hi, ks);
        k = toku_realloc(k, ks); assert(k);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        u_int32_t vs = mi;
        v = toku_realloc(v, vs); assert(v);
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
    toku_free(k);
    toku_free(v);
    if (verbose && bigest > 0) printf("%s bigest %u\n", __FUNCTION__, bigest);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *argv[]) {
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
            if (lorange > ULLONG_MAX)
                return 2;
            continue;
        }
        if (strcmp(arg, "-hirange") == 0) {
            if (i+1 >= argc) 
                return 1;
            hirange = strtoull(argv[++i], 0, 10);
            if (hirange > ULLONG_MAX)
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
