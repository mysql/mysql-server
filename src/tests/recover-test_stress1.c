/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: test_stress1.c 35324 2011-10-04 01:48:45Z zardosht $"
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

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    int n = cli_args->num_elements;

    //
    // the threads that we want:
    //   - one thread constantly updating random values
    //   - one thread doing table scan with bulk fetch
    //   - one thread doing table scan without bulk fetch
    //   - one thread doing random point queries
    //

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = 5 + cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], n, dbp, env);
    }

    // make the forward fast scanner
    myargs[0].fast = TRUE;
    myargs[0].fwd = TRUE;
    myargs[0].operation = scan_op;

    // make the forward slow scanner
    myargs[1].fast = FALSE;
    myargs[1].fwd = TRUE;
    myargs[1].operation = scan_op;

    // make the backward fast scanner
    myargs[2].fast = TRUE;
    myargs[2].fwd = FALSE;
    myargs[2].operation = scan_op;

    // make the backward slow scanner
    myargs[3].fast = FALSE;
    myargs[3].fwd = FALSE;
    myargs[3].operation = scan_op;

    // make the guy that updates the db
    myargs[4].operation = update_op;

    // make the guy that does point queries
    for (int i = 5; i < num_threads; i++) {
        myargs[i].operation = ptquery_op;
    }

    int num_seconds = random() % cli_args->time_of_test;
    run_workers(myargs, num_threads, num_seconds, true);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = DEFAULT_ARGS;
    args.checkpointing_period = 1;
    parse_stress_test_args(argc, argv, &args);
    if (args.do_test_and_crash) {
        stress_test_main(&args);
    }
    if (args.do_recover) {
        stress_recover(&args);
    }
    return 0;
}
