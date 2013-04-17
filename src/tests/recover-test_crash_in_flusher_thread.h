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

toku_pthread_t checkpoint_tid;
static int cnt = 0;
static BOOL starting_a_chkpt = FALSE;

int state_to_crash = 0;

static void *do_checkpoint_and_crash(void *arg) {
    // first verify that checkpointed_data is correct;
    DB_ENV* env = arg;
    if (verbose) printf("starting a checkpoint\n");
    int r = env->txn_checkpoint(env, 0, 0, 0); assert(r==0);
    if (verbose) printf("completed a checkpoint, about to crash\n");
    toku_hard_crash_on_purpose();
    return arg;
}

static void ft_callback(int ft_state, void* extra) {
    cnt++;
        if (verbose) printf("ft_state!! %d\n", ft_state);
    if (cnt > 0 && !starting_a_chkpt && ft_state == state_to_crash) {
        starting_a_chkpt = TRUE;
        if (verbose) printf("ft_state %d\n", ft_state);
        int r = toku_pthread_create(&checkpoint_tid, NULL, do_checkpoint_and_crash, extra); 
        assert(r==0);
        usleep(2*1000*1000);
    }
}


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
    const int num_threads = 1;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], n, dbp, env, cli_args);
    }

    // make the guy that updates the db
    myargs[0].operation = update_op;
    //myargs[0].update_pad_frequency = 0;

    db_env_set_flusher_thread_callback(ft_callback, env);
    run_workers(myargs, num_threads, cli_args->time_of_test, true);
}

static int
run_recover_ft_test(int argc, char *const argv[]) {
    struct cli_args args = DEFAULT_ARGS;
    // make test time arbitrarily high because we expect a crash
    args.time_of_test = 1000000000;
    args.num_elements = 2000;
    // we want to induce a checkpoint
    args.checkpointing_period = 0;
    parse_stress_test_args(argc, argv, &args);
    if (args.do_test_and_crash) {
        stress_test_main(&args);
    }
    if (args.do_recover) {
        stress_recover(&args);
    }
    return 0;
}
