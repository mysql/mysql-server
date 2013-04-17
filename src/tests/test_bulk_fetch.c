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
    assert(a->size == sizeof(u_int64_t));
    assert(b->size == sizeof(u_int64_t));
    u_int64_t* expected = (u_int64_t *)c;
    assert(*expected == *(u_int64_t *)a->data);
    assert(*expected == *(u_int64_t *)b->data);
}

static int
verify_fwd_fast(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    u_int64_t* expected = (u_int64_t *)c;
    *expected = *expected + 1;
    return TOKUDB_CURSOR_CONTINUE;
}

static int
verify_fwd_slow(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    u_int64_t* expected = (u_int64_t *)c;
    *expected = *expected + 1;
    return 0;
}

static int
verify_bwd_fast(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    u_int64_t* expected = (u_int64_t *)c;
    *expected = *expected - 1;
    return TOKUDB_CURSOR_CONTINUE;
}

static int
verify_bwd_slow(DBT const *a, DBT  const *b, void *c) {
    verify_val(a,b,c);
    u_int64_t* expected = (u_int64_t *)c;
    *expected = *expected - 1;
    return 0;
}

u_int64_t num_pivots_fetched_prefetch;
u_int64_t num_basements_decompressed_aggressive;
u_int64_t num_basements_decompressed_prefetch;
u_int64_t num_basements_fetched_aggressive;
u_int64_t num_basements_fetched_prefetch;

static void
init_eng_stat_vars(DB_ENV* env) {
    ENGINE_STATUS engstat;
    int r = env->get_engine_status(env, &engstat, NULL, 0);
    CKERR(r);
    num_pivots_fetched_prefetch = engstat.num_pivots_fetched_prefetch;
    num_basements_decompressed_aggressive = engstat.num_basements_decompressed_aggressive;
    num_basements_decompressed_prefetch = engstat.num_basements_decompressed_prefetch;
    num_basements_fetched_aggressive = engstat.num_basements_fetched_aggressive;
    num_basements_fetched_prefetch = engstat.num_basements_fetched_prefetch;
}

static void
check_eng_stat_vars_unchanged(DB_ENV* env) {
    ENGINE_STATUS engstat;
    int r = env->get_engine_status(env, &engstat, NULL, 0);
    CKERR(r);
    assert(num_pivots_fetched_prefetch == engstat.num_pivots_fetched_prefetch);
    assert(num_basements_decompressed_aggressive == engstat.num_basements_decompressed_aggressive);
    assert(num_basements_decompressed_prefetch == engstat.num_basements_decompressed_prefetch);
    assert(num_basements_fetched_aggressive == engstat.num_basements_fetched_aggressive);
    assert(num_basements_fetched_prefetch == engstat.num_basements_fetched_prefetch);
}

static void
print_relevant_eng_stat_vars(DB_ENV* env) {
    ENGINE_STATUS engstat;
    int r = env->get_engine_status(env, &engstat, NULL, 0);
    CKERR(r);
    printf("num_pivots_fetched_prefetch %"PRId64" \n", engstat.num_pivots_fetched_prefetch);
    printf("num_basements_decompressed_aggressive %"PRId64" \n", engstat.num_basements_decompressed_aggressive);
    printf("num_basements_decompressed_prefetch %"PRId64" \n", engstat.num_basements_decompressed_prefetch);
    printf("num_basements_fetched_aggressive %"PRId64" \n", engstat.num_basements_fetched_aggressive);
    printf("num_basements_fetched_prefetch %"PRId64" \n", engstat.num_basements_fetched_prefetch);
}

static void
test_bulk_fetch (u_int64_t n, BOOL prelock, BOOL disable_prefetching) {
    if (verbose) printf("test_rand_insert:%"PRId64" \n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.bulk_fetch.brt";
    int r;
    

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r=env->set_default_bt_compare(env, int64_dbt_cmp); CKERR(r);
    r = env->set_cachesize(env, 0, (u_int32_t)n, 1); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->set_readpagesize(db, 1024);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    u_int64_t keys[n];
    u_int64_t i;
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
    u_int32_t flags = disable_prefetching ? DBC_DISABLE_PREFETCHING : 0;
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
    CKERR(r);
    if (prelock) {
        r = cursor->c_pre_acquire_range_lock(
            cursor,
            db->dbt_neg_infty(),
            db->dbt_pos_infty()
            );
        CKERR(r);
    }
    u_int64_t expected = 0;
    while (r != DB_NOTFOUND) {
        r = cursor->c_getf_next(cursor, 0, verify_fwd_fast, &expected);
        assert(r==0 || r==DB_NOTFOUND);
    }
    r = cursor->c_close(cursor); CKERR(r);
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }

    // verify slow
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
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
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }

    // now do backwards
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
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
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }

    // verify slow
    if (disable_prefetching) {
        init_eng_stat_vars(env);
    }
    r = db->cursor(db, NULL, &cursor, flags);
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
    if (disable_prefetching) {
        check_eng_stat_vars_unchanged(env);
    }
    if (verbose) {
        print_relevant_eng_stat_vars(env);
    }


    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_bulk_fetch(10000, FALSE, TRUE);
    test_bulk_fetch(10000, TRUE, TRUE);
    test_bulk_fetch(10000, FALSE, FALSE);
    test_bulk_fetch(10000, TRUE, FALSE);
    return 0;
}
