/* -*- mode: C; c-basic-offset: 4 -*- */
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

static int checkpoint_var(DB_TXN *txn, ARG arg, void* operation_extra) {
    int db_index = random()%arg->num_DBs;
    int r = 0;
    int val_size = *(int *)operation_extra;
    DB* db = arg->dbp[db_index];
    char data[val_size];
    memset(data, 0, sizeof(data));
    for (int i = 0; i < 10; i++) {
        // do point queries
        ptquery_and_maybe_check_op(db, txn, arg, FALSE);        
    }
    for (int i = 0; i < 20; i++) {
        // do a random insertion
        int rand_key = random() % arg->num_elements;
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
    return r;
}


static void
stress_table(DB_ENV* env, DB** dbp, struct cli_args *cli_args) {
    int n = cli_args->num_elements;
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
        arg_init(&myargs[i], n, dbp, env, cli_args);
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
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
