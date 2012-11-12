/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: test_stress1.cc 46157 2012-07-25 20:49:56Z yfogel $"
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
// This test is a micro stress test that does multithreaded updates on a fixed size table.
// There is also a thread that scans the table with bulk fetch, ensuring the sum is zero.
//
// This test is targetted at stressing the locktree, hence the small table and many update threads.
//

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = 1 + cli_args->num_update_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }
    struct scan_op_extra soe[1];

    // make the forward fast scanner
    soe[0].fast = true;
    soe[0].fwd = true;
    soe[0].prefetch = false;
    myargs[0].operation_extra = &soe[0];
    myargs[0].operation = scan_op;

    // make the threads that update the db
    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    for (int i = 1; i < 1 + cli_args->num_update_threads; ++i) {
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_op;
        myargs[i].do_prepare = false;
        // the first three threads will prelock ranges before
        // doing sequential updates. the rest of the threads
        // will take point write locks on update as usual.
        // this ensures both ranges and points are stressed.
        myargs[i].prelock_updates = i < 4 ? true : false;
    }

    run_workers(myargs, num_threads, cli_args->time_of_test, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    // default args for first, then parse any overrides
    args.num_update_threads = 8;
    args.num_elements = 512;
    args.txn_size = 16;
    parse_stress_test_args(argc, argv, &args);

    // we expect to get lock_notgranted op failures, and we
    // don't want the overhead of fsync on small txns
    args.crash_on_operation_failure = false;
    args.nosync = true;
    stress_test_main(&args);
    return 0;
}
