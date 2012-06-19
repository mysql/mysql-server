/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: ft-refcount-stress.c 44373 2012-06-08 20:33:40Z esmet $"

#include <toku_pthread.h>
#include "test.h"
#include "threaded_stress_test_helpers.h"

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
// - this should stress both open db handles and v arious txns
// references dbs simultaneously.
// 
// update_thread {
//   for i in random() % small_int
//      db = lock_and_maybe_open_random_db()
//      db->update(db, touch);
//      maybe_close(db);
//      unlock(db);
// }
// query_thread {
//   for i in random() % small_int
//      db = lock_and_maybe_open_random_db()
//      db->get(db, val);
//      maybe_close(db);
//      unlock(db);
// }

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

static void
init_dbt(DBT *dbt, void *data, int len) {
    memset(dbt, 0, sizeof(DBT));
    dbt->data = data;
    dbt->size = len;
    dbt->ulen = len;
    dbt->flags = DB_DBT_USERMEM;
}

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
        printf("opened db %d in bucket %d\n", i, k);
    }
    assert(bucket->is_open);
    assert(__sync_fetch_and_add(&open_buckets, 1) < num_buckets);
    return bucket;
}

// release the lock on a bucket, possibly closing its db
static void
unlock_and_maybe_close_db(struct db_bucket *bucket, ARG arg) {
    static const int p = 5;
    int k = ((unsigned) myrandom_r(arg->random_data)) % 100;
    // we should close with probability approximately p / 100
    assert(bucket->is_open);
    if (k <= p) {
        DB *db = bucket->db;
        int r = db->close(db, 0);
        CKERR(r);
        bucket->is_open = false;
        printf("decided to close a bucket's db before unlocking\n");
    }
    assert(__sync_fetch_and_add(&open_buckets, -1) > 0);
    toku_mutex_unlock(&bucket->mutex);
}

// put a value of 1 for the key or increment what's there
static int 
increment(DB *db, const DBT *key, const DBT *old_val, const DBT *extra, 
         void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    assert(db);
    assert(key);
    assert(extra && extra->size == 0);
    DBT new_val;
    int k = 1;
    if (old_val) {
        assert(old_val->size == sizeof(int));
        k = *(int *) old_val->data;
        k++;
    }
    init_dbt(&new_val, &k, sizeof(int));
    set_val(&new_val, set_extra);
    return 0;
}

static const int max_key = 10000;

// read the one and only row for a db with the bucket.
// should be holding the bucket's lock.
static int
read_row(DB_TXN *txn, struct db_bucket *bucket, ARG arg) {
    int k, v;
    DBT key, value;
    k = myrandom_r(arg->random_data) % max_key;
    init_dbt(&key, &k, sizeof(int));
    init_dbt(&value, &v, sizeof(int));
    DB *db = bucket->db;
    int r = db->get(db, txn, &key, &value, 0);
    invariant(r == 0 || r == DB_NOTFOUND || r == DB_LOCK_NOTGRANTED);
    return k;
}

// update a random row, should be holding the bucket's lock
static void
update_row(DB_TXN *txn, struct db_bucket *bucket, ARG arg) {
    DBT key, extra;
    int k = myrandom_r(arg->random_data) % max_key;
    init_dbt(&key, &k, sizeof(int));
    init_dbt(&extra, NULL, 0);
    DB *db = bucket->db;
    int r = db->update(db, txn, &key, &extra, DB_TXN_SNAPSHOT);
    invariant(r == 0 || r == DB_LOCK_NOTGRANTED);
}

// touch a couple of dbs in some buckets with a txn
static int 
touch_some_dbs(DB_TXN *txn, ARG arg, void *UU(op_extra), void *UU(stats_extra)) {
    const int iterations = choose_random_iteration_count(arg);
    for (int i = 0; i < iterations; i++) {
        struct db_bucket *bucket = lock_and_maybe_open_some_db(arg);
        update_row(txn, bucket, arg);
        unlock_and_maybe_close_db(bucket, arg);
    }
    return 0;
}

// query a couple of dbs, asserting that the value read is equal
// to the value in the dbs bucket, while holding the bucket lock
static int 
query_some_dbs(DB_TXN *txn, ARG arg, void *UU(op_extra), void *UU(stats_extra)) {
    printf("querying some dbs\n");
    const int iterations = choose_random_iteration_count(arg);
    for (int i = 0; i < iterations; i++) {
        struct db_bucket *bucket = lock_and_maybe_open_some_db(arg);
        int k = read_row(txn, bucket, arg);
        assert(k >= 0);
        unlock_and_maybe_close_db(bucket, arg);
    }
    return 0;
}

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    const int update_threads = cli_args->num_update_threads;
    const int query_threads = cli_args->num_ptquery_threads;
    const int total_threads = update_threads + query_threads;

    num_buckets = cli_args->num_DBs;

    printf("stressing %d tables using %d update threads, %d query threads\n",
            num_buckets, update_threads, query_threads);

    // each thread gets access to this array of db buckets, from 
    // which they can choose a random db to either touch or query
    buckets = toku_malloc(sizeof(struct db_bucket) * num_buckets);
    for (int i = 0; i < num_buckets; i++) {
        struct db_bucket bucket = {
            .is_open = true,
            .db = dbp[i], 
            .env = env,
        };
        buckets[i] = bucket;
        toku_mutex_init(&buckets[i].mutex, NULL);
    }

    struct arg myargs[total_threads];
    for (int i = 0; i < total_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
        // first i threads are updaters, rest are verifiers
        if (i < update_threads) {
            myargs[i].operation = touch_some_dbs;
        } else {
            myargs[i].operation = query_some_dbs;
        }
    }

    // run all of the query and update workers. they may randomly open
    // and close the dbs in each db_bucket to be some random dictionary,
    // so when they're done we'll have to clean up the mess so this
    // stress test can exit gracefully expecting db[i] = the ith db
    run_workers(myargs, total_threads, cli_args->time_of_test, false, cli_args);

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

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    parse_stress_test_args(argc, argv, &args);
    args.env_args.update_function = increment;
    // checkpointing is a part of the ref count, so do it often
    args.env_args.checkpointing_period = 5;
    stress_test_main(&args);
    return 0;
}
