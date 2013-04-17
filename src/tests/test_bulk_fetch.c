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
verify_val(DBT const *a, DBT  const *b, void *c) {
    assert(a->size == sizeof(int));
    assert(b->size == sizeof(int));
    int* expected = (int *)c;
    assert(*expected == *(int *)a->data);
    assert(*expected == *(int *)b->data);
}

static int
verify_fwd_fast(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    int* expected = (int *)c;
    *expected = *expected + 1;
    return TOKUDB_CURSOR_CONTINUE;
}

static int
verify_fwd_slow(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    int* expected = (int *)c;
    *expected = *expected + 1;
    return 0;
}

static int
verify_bwd_fast(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    int* expected = (int *)c;
    *expected = *expected - 1;
    return TOKUDB_CURSOR_CONTINUE;
}

static int
verify_bwd_slow(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    int* expected = (int *)c;
    *expected = *expected - 1;
    return 0;
}



static void
test_bulk_fetch (int n, BOOL prelock) {
    if (verbose) printf("test_rand_insert:%d \n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.bulk_fetch.brt";
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r=env->set_default_bt_compare(env, int_dbt_cmp); CKERR(r);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int keys[n];
    int i;
    for (i=0; i<n; i++) {
        keys[i] = i;
    }
    
    for (i=0; i<n; i++) {
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    } 

    //
    // data inserted, now verify that using TOKUDB_CURSOR_CONTINUE in the callback works
    //
    DBC* cursor;

    // verify fast
    r = db->cursor(db, NULL, &cursor, 0);
    CKERR(r);
    if (prelock) {
        r = cursor->c_pre_acquire_range_lock(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty()
            );
        CKERR(r);
    }
    int expected = 0;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_next(cursor, 0, verify_fwd_fast, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);

    // verify slow
    r = db->cursor(db, NULL, &cursor, 0);
    CKERR(r);
    if (prelock) {
        r = cursor->c_pre_acquire_range_lock(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty()
            );
        CKERR(r);
    }
    expected = 0;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_next(cursor, 0, verify_fwd_slow, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);

    // now do backwards
    r = db->cursor(db, NULL, &cursor, 0);
    CKERR(r);
    if (prelock) {
        r = cursor->c_pre_acquire_range_lock(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty()
            );
        CKERR(r);
    }
    expected = n-1;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_prev(cursor, 0, verify_bwd_fast, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);

    // verify slow
    r = db->cursor(db, NULL, &cursor, 0);
    CKERR(r);
    if (prelock) {
        r = cursor->c_pre_acquire_range_lock(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty()
            );
        CKERR(r);
    }
    expected = n-1;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_prev(cursor, 0, verify_bwd_slow, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);


    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_bulk_fetch(10000, FALSE);
    test_bulk_fetch(10000, TRUE);
    return 0;
}
