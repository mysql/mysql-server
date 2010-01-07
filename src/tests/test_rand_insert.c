/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static void
test_rand_insert (int n, int dup_mode) {
    if (verbose) printf("test_rand_insert:%d %d\n", n, dup_mode);

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
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    unsigned int keys[n];
    int i;
    for (i=0; i<n; i++)
        keys[i] = htonl(random());

    /* insert n/2 <random(), i> pairs */
    for (i=0; i<n/2; i++) {
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    } 

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n/2 <random(), i> pairs */
    for (i=n/2; i<n; i++) {
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    } 

    for (i=0; i<n; i++) {
        DBT key, val;
        r = db->get(db, 0, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init_malloc(&val), 0);
        assert(r == 0);
        int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        if (vv != i) assert(keys[vv] == keys[i]);
        else assert(vv == i);
        toku_free(val.data);
    }

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *argv[]) {
    parse_args(argc, argv);

    int i;
    for (i = 1; i <= 2048; i *= 2) {
        test_rand_insert(i, 0);
    }

    return 0;
}
