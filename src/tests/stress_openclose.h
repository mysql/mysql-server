/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: ft-refcount-stress.c 44373 2012-06-08 20:33:40Z esmet $"

#include <toku_pthread.h>
#include "test.h"
#include "threaded_stress_test_helpers.h"

// set this to true for the recovery version of this stress test
// the way this works is to include this header and set 
// crash_at_end = true;
static bool stress_openclose_crash_at_end;

//
// Stress test ft reference counting
//
// Three things keep a fractal tree in memory by holding a reference:
//  - open ft handle
//  - live txn that did a write op
//  - checkpoint
//
// To stress reference counting, we would like threads which:
//  - take checkpoints at random intervals
//  - update random values, random point queries for auditing
//      * sometimes close handle before commit.
//  - close random dictionaries
//
// Here's how we can do it:
//
// A bunch of threads randomly choose from N buckets. Each bucket
// has a DB, an is_open bit, and a lock.
// - in a single txn, each thread will do some small number of
// queries or updates on randomb uckets, opening the dbs if
// they were closed and possibly closing afterwards.
// - this should stress both open db handles and various txns
// references dbs simultaneously.
// 
// and all while this is happening, throw in scanners, updaters,
// and query threads that all assert the contents of these dbs
// is correct, even after recovery.

#define verbose_printf(...)         \
    do {                            \
        if (verbose) {              \
            printf(__VA_ARGS__);    \
            fflush(stdout);         \
        }                           \
    } while (0)

// a bunch of buckets with dbs, a lock, and an is_open bit
// threads will choose buckets randomly for update, query,
// and then maybe open/close the bucket's db.
struct db_bucket {
    DB_ENV *env;
    DB *db;
    bool is_open;
    toku_mutex_t mutex;
};
static struct db_bucket *buckets;
static int num_buckets;

// each operation can do at most this many operations in one txn
static int
choose_random_iteration_count(ARG arg) {
    const int max_iteration_count = 8;
    int k = myrandom_r(arg->random_data) % max_iteration_count;
    return k + 1;
}

// open the ith db in the array, asserting success
static void
open_ith_db(DB_ENV *env, DB **db, int i) {
    char name[30];
    memset(name, 0, sizeof(name));
    get_ith_table_name(name, sizeof(name), i);
    int r = db_create(db, env, 0);
    CKERR(r);
    r = (*db)->open(*db, null_txn, name, NULL, DB_BTREE, 0, 0666);
    CKERR(r);
}

// debugging counter to maintain the invariant that open_buckets <= num_buckets
static int open_buckets;

// choose and lock a random bucket, possibly opening a db
static struct db_bucket *
lock_and_maybe_open_some_db(ARG arg) {
    int k = myrandom_r(arg->random_data) % num_buckets;
    struct db_bucket *bucket = &buckets[k];
    toku_mutex_lock(&bucket->mutex);
    if (!bucket->is_open) {
        // choose a random DB from 0..k-1 to associate with this bucket
        // then, mark the bucket as open.
        int i = myrandom_r(arg->random_data) % num_buckets;
        open_ith_db(bucket->env, &bucket->db, i);
        bucket->is_open = true;
        assert(__sync_fetch_and_add(&open_buckets, 1) < num_buckets);
        verbose_printf("opened db %d in bucket %d\n", i, k);
    }
    return bucket;
}

// release the lock on a bucket, possibly closing its db
static void
unlock_and_maybe_close_db(struct db_bucket *bucket, ARG arg) {
    static const int p = 5;
    int k = ((unsigned) myrandom_r(arg->random_data)) % 100;
    // we should close with probability approximately p / 100
    assert(bucket->is_open);
    if (false && k <= p) {
        DB *db = bucket->db;
        int r = db->close(db, 0);
        CKERR(r);
        bucket->is_open = false;
        int old_open_buckets = __sync_fetch_and_sub(&open_buckets, 1);
        assert(old_open_buckets > 0);
        verbose_printf("decided to close a bucket's db before unlocking\n");
    }
    toku_mutex_unlock(&bucket->mutex);
}

// scan some dbs, verifying the correct sum.
static int
scan_some_dbs(DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    int r = 0;
    verbose_printf("scanning some dbs\n");
    struct scan_op_extra* extra = cast_to_typeof(extra) operation_extra;
    // scan every db, one by one, and verify that the contents are correct
    for (int i = 0; r == 0 && run_test && i < arg->cli->num_DBs; i++) {
        struct db_bucket *bucket = lock_and_maybe_open_some_db(arg);
        const bool check_sum = true;
        r = scan_op_and_maybe_check_sum(bucket->db, txn, extra, check_sum);
        invariant(r == 0 || r == DB_LOCK_NOTGRANTED);
        unlock_and_maybe_close_db(bucket, arg);
    }
    return r;
}

// update a couple of dbs in some buckets with a txn
static int 
update_some_dbs(DB_TXN *txn, ARG arg, void *op_extra, void *stats_extra) {
    int r = 0;
    verbose_printf("updating some dbs\n");
    const int iterations = choose_random_iteration_count(arg);
    for (int i = 0; r == 0 && run_test && i < iterations; i++) {
        struct db_bucket *bucket = lock_and_maybe_open_some_db(arg);
        // does an update operation on this bucket's db
        r = update_op_db(bucket->db, txn, arg, op_extra, stats_extra);
        invariant(r == 0 || r == DB_LOCK_NOTGRANTED);
        unlock_and_maybe_close_db(bucket, arg);
    }
    return r;
}

// point query a couple of dbs in some buckets with a txn
static int 
ptquery_some_dbs(DB_TXN *txn, ARG arg, void *UU(op_extra), void *UU(stats_extra)) {
    int r = 0;
    verbose_printf("querying some dbs\n");
    const int iterations = choose_random_iteration_count(arg);
    for (int i = 0; r == 0 && run_test && i < iterations; i++) {
        struct db_bucket *bucket = lock_and_maybe_open_some_db(arg);
        // does a point query on a random key for this bucket's db
        const bool check_sum = true;
        r = ptquery_and_maybe_check_op(bucket->db, txn, arg, check_sum);
        invariant(r == 0 || r == DB_LOCK_NOTGRANTED);
        unlock_and_maybe_close_db(bucket, arg);
    }
    return r;
}

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    const int update_threads = cli_args->num_update_threads;
    const int query_threads = cli_args->num_ptquery_threads;
    const int total_threads = update_threads + query_threads + 1;

    struct arg myargs[total_threads];
    for (int i = 0; i < total_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }

    struct scan_op_extra soe[4];

    // make the forward fast scanner
    soe[0].fast = TRUE;
    soe[0].fwd = TRUE;
    soe[0].prefetch = FALSE;
    myargs[0].operation_extra = &soe[0];
    myargs[0].operation = scan_some_dbs;

    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    // make the guy that updates the db
    for (int i = 1; i < 1 + update_threads; ++i) {
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_some_dbs;
        myargs[i].do_prepare = true;
    }
    // make the guy that does point queries
    for (int i = 1 + update_threads; i < total_threads; i++) {
        myargs[i].operation = ptquery_some_dbs;
        myargs[i].do_prepare = true;
    }

    num_buckets = cli_args->num_DBs;
    open_buckets = num_buckets;
    // each thread gets access to this array of db buckets, from 
    // which they can choose a random db to either touch or query
    XMALLOC_N(num_buckets, buckets);
    for (int i = 0; i < num_buckets; i++) {
        struct db_bucket bucket = {
            .env = env,
            .db = dbp[i], 
            .is_open = true
        };
        buckets[i] = bucket;
        toku_mutex_init(&buckets[i].mutex, NULL);
    }
    // run all of the query and update workers. they may randomly open
    // and close the dbs in each db_bucket to be some random dictionary,
    // so when they're done we'll have to clean up the mess so this
    // stress test can exit gracefully expecting db[i] = the ith db
    //verbose_printf("stressing %d tables using %d update threads, %d query threads\n",
    //        num_buckets, update_threads, query_threads);
    verbose_printf("stressing %d tables using %d update threads\n",
            num_buckets, update_threads);
    // stress_openclose_crash_at_end should be changed to true or false, 
    // depending if this test is for recovery or not.
    const bool crash_at_end = stress_openclose_crash_at_end;
    run_workers(myargs, total_threads, cli_args->time_of_test, crash_at_end, cli_args);
    
    // the stress test is now complete. get ready for shutdown/close.
    //
    // make sure that every db in the original array is opened
    // as it was when it was passed in.
    for (int i = 0; i < num_buckets; i++) {
        // close whatever is open
        if (buckets[i].is_open) {
            DB *db = buckets[i].db; 
            int r = db->close(db, 0);
            CKERR(r);
        }
        // put the correct db back, then save the pointer
        // into the dbp array we were given
        open_ith_db(env, &buckets[i].db, i);
        dbp[i] = buckets[i].db;
    }

    toku_free(buckets);
}
