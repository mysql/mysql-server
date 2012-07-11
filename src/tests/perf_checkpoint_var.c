/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: test_stress1.c 39258 2012-01-27 13:51:58Z zardosht $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"

u_int64_t num_basements_decompressed;
u_int64_t num_buffers_decompressed;
u_int64_t num_basements_fetched;
u_int64_t num_buffers_fetched;
u_int64_t num_pivots_fetched;

static void checkpoint_callback_1(void * extra) {
    DB_ENV* env = cast_to_typeof(env) extra;
    u_int64_t old_num_basements_decompressed = num_basements_decompressed;
    u_int64_t old_num_buffers_decompressed = num_buffers_decompressed;
    u_int64_t old_num_basements_fetched = num_basements_fetched;
    u_int64_t old_num_buffers_fetched = num_buffers_fetched;
    u_int64_t old_num_pivots_fetched = num_pivots_fetched;

    num_basements_decompressed = 
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_WRITE");
        
    num_buffers_decompressed = 
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_WRITE");

    num_basements_fetched = 
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_WRITE");

    num_buffers_fetched = 
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_WRITE");

    num_pivots_fetched = 
        get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_QUERY") +
        get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_WRITE");
        
    printf("basements decompressed %" PRIu64 " \n", num_basements_decompressed - old_num_basements_decompressed);
    printf("buffers   decompressed %" PRIu64 " \n", num_buffers_decompressed- old_num_buffers_decompressed);
    printf("basements fetched      %" PRIu64 " \n", num_basements_fetched - old_num_basements_fetched);
    printf("buffers fetched        %" PRIu64 " \n", num_buffers_fetched - old_num_buffers_fetched);
    printf("pivots fetched         %" PRIu64 " \n", num_pivots_fetched - old_num_pivots_fetched);
    printf("************************************************************\n");
}

static void checkpoint_callback_2(void * extra) {
    DB_ENV* env = cast_to_typeof(env) extra;
    num_basements_decompressed = 
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_DECOMPRESSED_WRITE");
        
    num_buffers_decompressed = 
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_DECOMPRESSED_WRITE");
    
    num_basements_fetched = 
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_BASEMENTS_FETCHED_WRITE");
    
    num_buffers_fetched = 
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_NORMAL") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_AGGRESSIVE") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_MSG_BUFFER_FETCHED_WRITE");
    
    num_pivots_fetched = 
        get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_QUERY") +
        get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_PREFETCH") +
        get_engine_status_val(env, "FT_NUM_PIVOTS_FETCHED_WRITE");
}



//
// This test is a form of stress that does operations on a single dictionary:
// We create a dictionary bigger than the cachetable (around 4x greater).
// Then, we spawn a bunch of pthreads that do the following:
//  - scan dictionary forward with bulk fetch
//  - scan dictionary forward slowly
//  - scan dictionary backward with bulk fetch
//  - scan dictionary backward slowly
//  - Grow the dictionary with insertions
//  - do random point queries into the dictionary
// With the small cachetable, this should produce quite a bit of churn in reading in and evicting nodes.
// If the test runs to completion without crashing, we consider it a success. It also tests that snapshots
// work correctly by verifying that table scans sum their vals to 0.
//
// This does NOT test:
//  - splits and merges
//  - multiple DBs
//
// Variables that are interesting to tweak and run:
//  - small cachetable
//  - number of elements
//

static int checkpoint_var(DB_TXN *txn, ARG arg, void* operation_extra, void *stats_extra) {
    int db_index = random() % arg->cli->num_DBs;
    int r = 0;
    int val_size = *(int *)operation_extra;
    DB* db = arg->dbp[db_index];
    char data[val_size];
    memset(data, 0, sizeof(data));
    int i;
    for (i = 0; i < 10; i++) {
        // do point queries
        ptquery_and_maybe_check_op(db, txn, arg, FALSE);
    }
    increment_counter(stats_extra, PTQUERIES, i);
    for (i = 0; i < 20; i++) {
        // do a random insertion
        int rand_key = random() % arg->cli->num_elements;
        DBT key, val;
        r = db->put(
            db, 
            txn, 
            dbt_init(&key, &rand_key, sizeof(rand_key)), 
            dbt_init(&val, data, sizeof(data)), 
            0);
        if (r != 0) {
            goto cleanup;
        }
    }
cleanup:
    increment_counter(stats_extra, PUTS, i);
    return r;
}


static void
stress_table(DB_ENV* env, DB** dbp, struct cli_args *cli_args) {
    db_env_set_checkpoint_callback(checkpoint_callback_1, env);
    db_env_set_checkpoint_callback2(checkpoint_callback_2, env);
    //
    // the threads that we want:
    //   - some threads constantly updating random values
    //   - one thread doing table scan with bulk fetch
    //   - one thread doing table scan without bulk fetch
    //   - some threads doing random point queries
    //

    if (verbose) printf("starting creation of pthreads\n");
    int val_size = cli_args->val_size;
    const int num_threads = cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }
    for (int i = 0; i < num_threads; i++) {
        myargs[i].operation = checkpoint_var;
        myargs[i].operation_extra = &val_size;
    }
    run_workers(myargs, num_threads, cli_args->time_of_test, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args_for_perf();
    args.env_args.checkpointing_period = 30;
    args.num_DBs = 4;
    args.num_ptquery_threads = 4;
    args.crash_on_operation_failure = false;
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
