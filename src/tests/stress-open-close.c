/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: ft-refcount-stress.c 44373 2012-06-08 20:33:40Z esmet $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

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
// N DB handles will map to M dictionaries and counters
// [db1, count] [db2, count] ... [dbM, count]
// 
// update_thread {
//   db = lock_and_maybe_open_random_db()
//   db->update(db, touch);
//   touch_count(db);
//   maybe_close(db);
//   unlock(db);
// }
// query_thread {
//   db = lock_and_maybe_open_random_db()
//   db->get(db, val);
//   assert(val == get_count(db));
//   unlock(db);
// }

struct db_counter {
    const DB *db;
    toku_mutex_t mutex;
    int count;
};

static int
choose_random_iteration_count(ARG arg) {
    // each operation can do at most this many operations in one txn
    const int max_iteration_count = 4;
    int k = myrandom_r(arg->random_data) % max_iteration_count;
    return k + 1;
}

static DB *
lock_and_maybe_open_some_db(ARG arg) {
    int k = myrandom_r(arg->random_data) % arg->cli->num_DBs];
    DB *db = arg->dbp[k];
}

static int 
touch_some_dbs(DB_TXN *txn, ARG arg, void *op_extra, void *UU(stats_extra)) {
    printf("touching some dbs\n");
    struct db_counter *counters = op_extra;
    const int iterations = choose_random_iteration_count(arg);
    return 0;
}

static int 
verify_some_dbs(DB_TXN *txn, ARG arg, void *op_extra, void *UU(stats_extra)) {
    printf("verifying some dbs\n");
    struct db_counter *counters = op_extra;
    const int iterations = choose_random_iteration_count(arg);
    return 0;
}

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    const int num_updaters = cli_args->num_update_threads;
    const int num_verifiers = cli_args->num_ptquery_threads;
    const int num_threads = num_updaters + num_verifiers;

    // each thread gets access to this array of db counters, from 
    // which they can choose a random db to either touch or verify
    struct db_counter counters[cli_args->num_DBs];
    for (int i = 0; i < cli_args->num_DBs; i++) {
        counters[i] = { 
            .is_open = true,
            .db = dbp[i], 
            .count = 0, 
            .mutex = PTHREAD_MUTEX_INITIALIZER,
        };
    }

    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
        // first i threads are updaters, rest are verifiers
        if (i < num_updaters) {
            myargs[i].operation = touch_some_dbs;
        } else {
            myargs[i].operation = verify_some_dbs;
        }
        myargs[i].operation_extra = &counters;
    }

    run_workers(myargs, num_threads, cli_args->time_of_test, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
