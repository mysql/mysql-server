/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: recover-test_stress1.c 39258 2012-01-27 13:51:58Z zardosht $"
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

u_int64_t time_til_crash;
u_int64_t start_time;

static uint64_t get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static void checkpoint_callback2(void* UU(extra)) {
    u_int64_t curr_time = get_tnow();
    u_int64_t time_diff = curr_time - start_time;
    if ((time_diff/1000000ULL) > time_til_crash) {
        toku_hard_crash_on_purpose();
    }
}

static int manual_checkpoint(DB_TXN *UU(txn), ARG UU(arg), void* operation_extra, void *UU(stats_extra)) {
    DB_ENV* env = operation_extra;
    int r = env->txn_checkpoint(env,0,0,0);
    assert_zero(r);
    return 0;
}

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    //
    // the threads that we want:
    //   - one thread constantly updating random values
    //   - one thread doing table scan with bulk fetch
    //   - one thread doing table scan without bulk fetch
    //   - one thread doing random point queries
    //

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = 5 + cli_args->num_update_threads + cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }
    struct scan_op_extra soe[4];

    // make the forward fast scanner
    soe[0].fast = TRUE;
    soe[0].fwd = TRUE;
    soe[0].prefetch = FALSE;
    myargs[0].operation_extra = &soe[0];
    myargs[0].operation = scan_op;

    // make the forward slow scanner
    soe[1].fast = FALSE;
    soe[1].fwd = TRUE;
    soe[1].prefetch = FALSE;
    myargs[1].operation_extra = &soe[1];
    myargs[1].operation = scan_op;

    // make the backward fast scanner
    soe[2].fast = TRUE;
    soe[2].fwd = FALSE;
    soe[2].prefetch = FALSE;
    myargs[2].operation_extra = &soe[2];
    myargs[2].operation = scan_op;

    // make the backward slow scanner
    soe[3].fast = FALSE;
    soe[3].fwd = FALSE;
    soe[3].prefetch = FALSE;
    myargs[3].operation_extra = &soe[3];
    myargs[3].operation = scan_op;

    // make something for checkpoints
    myargs[4].operation = manual_checkpoint;
    myargs[4].sleep_ms = 30*1000; // do checkpoints every 30 seconds
    myargs[4].operation_extra = env;

    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    // make the guy that updates the db
    for (int i = 5; i < 5 + cli_args->num_update_threads; ++i) {
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_op;
    }

    // make the guy that does point queries
    for (int i = 5 + cli_args->num_update_threads; i < num_threads; i++) {
        myargs[i].operation = ptquery_op;
    }

    db_env_set_checkpoint_callback2(checkpoint_callback2, NULL);
    time_til_crash = random() % cli_args->time_of_test;
    start_time = get_tnow();
    run_workers(myargs, num_threads, INT32_MAX, true, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    args.env_args.checkpointing_period = 0;
    parse_stress_test_args(argc, argv, &args);
    if (args.do_test_and_crash) {
        stress_test_main(&args);
    }
    if (args.do_recover) {
        stress_recover(&args);
    }
    return 0;
}
