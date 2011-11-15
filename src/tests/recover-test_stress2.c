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


static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    int n = cli_args->num_elements;

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_update_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], n, dbp, env);
        // make the guy that updates the db
        myargs[i].operation = update_op;
    }


    int num_seconds = random() % cli_args->time_of_test;
    run_workers(myargs, num_threads, num_seconds, true);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = DEFAULT_ARGS;
    args.checkpointing_period = 1;
    args.num_elements = 2000;
    parse_stress_test_args(argc, argv, &args);
    if (args.do_test_and_crash) {
        stress_test_main(&args);
    }
    if (args.do_recover) {
        stress_recover(&args);
    }
    return 0;
}
