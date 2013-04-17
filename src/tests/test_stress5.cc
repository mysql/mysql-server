/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
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
    //
    // do insertions and queries with a loader lying around doing stuff
    //

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = 4 + cli_args->num_update_threads + cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }
    struct scan_op_extra soe[2];

    // make the forward fast scanner
    soe[0].fast = true;
    soe[0].fwd = true;
    soe[0].prefetch = false;
    myargs[0].operation_extra = &soe[0];
    myargs[0].operation = scan_op;
    myargs[0].txn_flags = DB_TXN_SNAPSHOT | DB_TXN_READ_ONLY;

    // make the forward slow scanner
    soe[1].fast = false;
    soe[1].fwd = true;
    soe[1].prefetch = false;
    myargs[1].operation_extra = &soe[1];
    myargs[1].operation = scan_op;

    // make the guy that updates the db
    myargs[2].operation = loader_op;
    myargs[3].operation = keyrange_op;

    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    // make the guy that updates the db
    for (int i = 4; i < 4 + cli_args->num_update_threads; ++i) {
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_op;
    }

    // make the guy that does point queries
    for (int i = 4 + cli_args->num_update_threads; i < num_threads; i++) {
        myargs[i].operation = ptquery_op;
    }
    run_workers(myargs, num_threads, cli_args->num_seconds, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    // let's make default checkpointing period really slow
    args.env_args.checkpointing_period = 1;
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
