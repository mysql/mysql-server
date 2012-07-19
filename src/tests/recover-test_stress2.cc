/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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


static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_update_threads;
    struct arg myargs[num_threads];
    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    // make the guy that updates the db
    for (int i = 0; i < 0 + cli_args->num_update_threads; ++i) {
        arg_init(&myargs[i], dbp, env, cli_args);
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_op;
    }


    int num_seconds = random() % cli_args->time_of_test;
    run_workers(myargs, num_threads, num_seconds, true, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    args.env_args.checkpointing_period = 1;
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
