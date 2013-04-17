/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "$Id: perf_insert_multiple.cc 50140 2012-11-19 19:18:30Z esmet $"

#include <db.h>
#include <portability/toku_atomic.h>

#include "test.h"
#include "threaded_stress_test_helpers.h"

//
// This test tries to emulate iibench at the ydb layer. There is one
// unique index with an auto-increment key, plus several non-unique
// secondary indexes with random keys.
//

struct iibench_op_extra {
    uint64_t autoincrement;
};

static uint64_t next_autoincrement(struct iibench_op_extra *info) {
    return toku_sync_fetch_and_add(&info->autoincrement, 1);
}

static int UU() iibench_put_op(DB_TXN *txn, ARG arg, void *operation_extra, void *stats_extra) {
    const int num_dbs = arg->cli->num_DBs;
    DB **dbs = arg->dbp;
    DB_ENV *env = arg->env;
    DBT mult_key_dbt[num_dbs];
    DBT mult_put_dbt[num_dbs];
    uint32_t mult_put_flags[num_dbs];
    memset(mult_key_dbt, 0, sizeof(mult_key_dbt));
    memset(mult_put_dbt, 0, sizeof(mult_put_dbt));
    memset(mult_put_flags, 0, sizeof(mult_put_dbt));
    // The first DB is a unique autoincrement index.
    mult_put_flags[0] = DB_NOOVERWRITE;

    int r = 0;
    uint8_t valbuf[arg->cli->val_size];
    ZERO_ARRAY(valbuf);

    uint64_t puts_to_increment = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; ++i) {
        struct iibench_op_extra *CAST_FROM_VOIDP(info, operation_extra);
        uint64_t key = i == 0 ? next_autoincrement(info) : randu64(arg->random_data);
        fill_zeroed_array(valbuf, arg->cli->val_size, arg->random_data, arg->cli->compressibility);
        DBT key_dbt, val_dbt;
        dbt_init(&key_dbt, &key, sizeof key);
        dbt_init(&val_dbt, valbuf, sizeof valbuf);
        r = env->put_multiple(
            env, 
            dbs[0], // source db.
            txn, 
            &key_dbt, // source db key
            &val_dbt, // source db value
            num_dbs, // total number of dbs
            dbs, // array of dbs
            mult_key_dbt, // array of keys
            mult_put_dbt, // array of values
            mult_put_flags // array of flags
            );
        if (r != 0) {
            goto cleanup;
        }
        puts_to_increment++;
        if (puts_to_increment == 100) {
            increment_counter(stats_extra, PUTS, puts_to_increment);
            puts_to_increment = 0;
        }
    }
cleanup:
    return r;
}

static void
stress_table(DB_ENV* env, DB** UU(dbp), struct cli_args *cli_args) {
    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_put_threads;
    struct iibench_op_extra iib_extra = {
        .autoincrement = 0
    };
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
        myargs[i].operation = iibench_put_op;
        myargs[i].operation_extra = &iib_extra;
    }
    // TODO: set generate row for put callback
    (void) env;

    const bool crash_at_end = false;
    run_workers(myargs, num_threads, cli_args->num_seconds, crash_at_end, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args_for_perf();
    args.num_elements = 0;  // want to start with empty DBs
    parse_stress_test_args(argc, argv, &args);
    // when there are multiple threads, its valid for two of them to
    // generate the same key and one of them fail with DB_LOCK_NOTGRANTED
    if (args.num_put_threads > 1) {
        args.crash_on_operation_failure = false;
    }
    // TODO: Use a more complex, more expensive comparison function that
    // unpacks keys and walks a descriptor to satisfy the comparison.
    stress_test_main_with_cmp(&args, stress_uint64_dbt_cmp);
    return 0;
}
