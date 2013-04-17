/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: perf_checkpoint_var.cc 47109 2012-08-22 18:22:29Z zardosht $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"

static int perf_read(DB_TXN *txn, ARG arg, void* operation_extra, void *stats_extra) {
    int db_index = *(int *)operation_extra;
    DB* db = arg->dbp[db_index];

    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        ptquery_and_maybe_check_op(db, txn, arg, true);
        increment_counter(stats_extra, PTQUERIES, 1);
    }
    return 0;
}

static int perf_write(DB_TXN *txn, ARG arg, void* operation_extra, void *stats_extra) {
    int db_index = *(int *)operation_extra;
    DB* db = arg->dbp[db_index];
    return random_put_in_db(db, txn, arg, true, stats_extra);
}


static void
stress_table(DB_ENV* env, DB** dbp, struct cli_args *cli_args) {
    //
    // the threads that we want:
    //   - some threads constantly updating random values
    //   - one thread doing table scan with bulk fetch
    //   - one thread doing table scan without bulk fetch
    //   - some threads doing random point queries
    //

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_ptquery_threads + cli_args->num_update_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }

    const int num_update_threads = cli_args->num_update_threads;
    int upd_thread_ids[num_update_threads];
    for (int i = 0; i < cli_args->num_update_threads; ++i) {
        upd_thread_ids[i] = i % cli_args->num_DBs;
        myargs[i].operation_extra = &upd_thread_ids[i];
        myargs[i].operation = perf_write;
    }

    const int num_ptquery_threads = cli_args->num_ptquery_threads;
    int ptq_thread_ids[num_ptquery_threads];
    for (int i = cli_args->num_update_threads; i < num_threads; i++) {
        ptq_thread_ids[i] = i % cli_args->num_DBs;
        myargs[i].operation_extra = &ptq_thread_ids[i];
        myargs[i].operation = perf_read;
    }

    run_workers(myargs, num_threads, cli_args->time_of_test, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args_for_perf();
    args.env_args.checkpointing_period = 30;
    args.num_DBs = 1;
    args.num_ptquery_threads = 1;
    args.num_update_threads = 1;
    args.crash_on_operation_failure = false;
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
