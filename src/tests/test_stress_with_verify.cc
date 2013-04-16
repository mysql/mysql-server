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


//
// purpose of this stress test is to do a bunch of splitting and merging
// and run db->verify periodically to make sure the db is in a 
// a good state
//

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
    // make the forward fast scanner
    struct scan_op_extra soe;
    soe.fast = true;
    soe.fwd = true;
    soe.prefetch = false;
    myargs[0].operation_extra = &soe;
    myargs[0].lock_type = STRESS_LOCK_SHARED;
    myargs[0].operation = scan_op;

    // make the backward slow scanner
    myargs[1].lock_type = STRESS_LOCK_EXCL;
    myargs[1].sleep_ms = 3000; // maybe make this a runtime param at some point
    myargs[1].operation = verify_op;

    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    for (int i = 2; i < 2 + cli_args->num_update_threads; ++i) {
        myargs[i].lock_type = STRESS_LOCK_SHARED;
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_op;
    }

    // make the guy that does point queries
    for (int i = 2 + cli_args->num_update_threads; i < num_threads; i++) {
        myargs[i].lock_type = STRESS_LOCK_SHARED;
        myargs[i].operation = ptquery_op;
    }
    run_workers(myargs, num_threads, cli_args->num_seconds, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    // let's make default checkpointing period really slow
    args.env_args.checkpointing_period = 1;
    args.num_elements= 2000; // make default of small num elements to 
    args.num_ptquery_threads = 0;
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
