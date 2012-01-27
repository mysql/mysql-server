/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
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
//  - update existing values in the dictionary with db->put(DB_YESOVERWRITE)
//  - do random point queries into the dictionary
// With the small cachetable, this should produce quite a bit of churn in reading in and evicting nodes.
// If the test runs to completion without crashing, we consider it a success.
//
// This test differs from stress2 in that it verifies the last value on an update.
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
    const int num_threads = 4 + cli_args->num_update_threads + cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], n, dbp, env, cli_args);
    }

    struct scan_op_extra soe[4];

    // make the forward fast scanner
    soe[0].fast = TRUE;
    soe[0].fwd = TRUE;
    soe[0].prefetch = FALSE;
    myargs[0].operation_extra = &soe[0];
    myargs[0].operation = scan_op_no_check;

    // make the forward slow scanner
    soe[1].fast = FALSE;
    soe[1].fwd = TRUE;
    soe[1].prefetch = FALSE;
    myargs[1].operation_extra = &soe[1];
    myargs[1].operation = scan_op_no_check;

    // make the backward fast scanner
    soe[2].fast = TRUE;
    soe[2].fwd = FALSE;
    soe[2].prefetch = FALSE;
    myargs[2].operation_extra = &soe[2];
    myargs[2].operation = scan_op_no_check;

    // make the backward slow scanner
    soe[3].fast = FALSE;
    soe[3].fwd = FALSE;
    soe[3].prefetch = FALSE;
    myargs[3].operation_extra = &soe[3];
    myargs[3].operation = scan_op_no_check;

    struct update_op_args uoe[cli_args->num_update_threads];
    // make the guy that updates the db
    for (int i = 4; i < 4 + cli_args->num_update_threads; ++i) {
        int* update_history_buffer = toku_xmalloc(n * (sizeof uoe[i-4].update_history_buffer[0]));
        uoe[i-4] = get_update_op_args(cli_args,update_history_buffer);
        memset(uoe[i-4].update_history_buffer, 0, n * (sizeof uoe[i-4].update_history_buffer[0]));
        myargs[i].operation = update_with_history_op;
    }

    // make the guys that do point queries
    for (int i = 4 + cli_args->num_update_threads; i < num_threads; i++) {
        myargs[i].operation = ptquery_op;
    }

    run_workers(myargs, num_threads, cli_args->time_of_test, false, cli_args);

    for (int i = 0; i < cli_args->num_update_threads; ++i) {
        toku_free(uoe[i].update_history_buffer);
    }
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
