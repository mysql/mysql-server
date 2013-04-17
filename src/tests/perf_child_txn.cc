/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: perf_nop.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"

// The intent of this test is to measure the performace of creating and destroying child 
// transactions. Child transactions should have less work associated with them. They
// are not added to the live root list and they should not be creating their own snapshots.
// Nevertheless, benchmarks like tpcc and sysbench create many child transactions
// for each root transaction, and do little work per child transaction

static int create_child_txn(DB_TXN* txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    DB_TXN* child_txn = NULL;
    DB_ENV* env = arg->env;
    int r = env->txn_begin(env, txn, &child_txn, arg->txn_type); 
    CKERR(r);
    r = child_txn->commit(child_txn, 0);
    CKERR(r);
    return 0;
}


static void
stress_table(DB_ENV* env, DB** dbp, struct cli_args *cli_args) {
    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
        myargs[i].operation = create_child_txn;
    }
    run_workers(myargs, num_threads, cli_args->num_seconds, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args_for_perf();
    parse_stress_test_args(argc, argv, &args);
    args.single_txn = true;
    stress_test_main(&args);
    return 0;
}
