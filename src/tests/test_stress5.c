/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: test_stress1.c 35109 2011-09-27 18:41:25Z leifwalsh $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"


static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    int n = cli_args->num_elements;

    //
    // do insertions and queries with a loader lying around doing stuff
    //

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = 4 + cli_args->num_update_threads + cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], n, dbp, env, cli_args);
    }
    // make the forward fast scanner
    myargs[0].fast = TRUE;
    myargs[0].fwd = TRUE;
    myargs[0].operation = scan_op;

    // make the backward slow scanner
    myargs[1].fast = FALSE;
    myargs[1].fwd = FALSE;
    myargs[1].operation = scan_op;

    // make the guy that updates the db
    myargs[2].operation = loader_op;
    myargs[3].operation = keyrange_op;

    for (int i = 4; i < 4 + cli_args->num_update_threads; ++i) {
        myargs[i].operation = update_op;
    }

    // make the guy that does point queries
    for (int i = 4 + cli_args->num_update_threads; i < num_threads; i++) {
        myargs[i].operation = ptquery_op;
    }
    run_workers(myargs, num_threads, cli_args->time_of_test, false);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = DEFAULT_ARGS;
    // let's make default checkpointing period really slow
    args.checkpointing_period = 1;
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
