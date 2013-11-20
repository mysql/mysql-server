/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// The Way Things Work:
//
// Threaded stress tests have the following properties:
// - One or more DBs
// - One or more threads performing some number of operations per txn.
// - Correctness tests use signed 4 byte keys and signed 4 byte values. They expect
// a table with all zeroes before running.
// - Performance tests should use 8 byte keys and 8+ byte values, where the values
// are some mixture of random uncompressible garbage and zeroes, depending how
// compressible we want the data. These tests want the table to be populated
// with keys in the range [0, table_size - 1] unless disperse_keys is true,
// then the keys are scrambled up in the integer key space.

#ifndef _THREADED_STRESS_TEST_HELPERS_H_
#define _THREADED_STRESS_TEST_HELPERS_H_

#include "toku_config.h"
#include "test.h"

#include <stdio.h>
#include <math.h>
#include <locale.h>

#include <db.h>
#include <memory.h>
#include <toku_race_tools.h>

#include <portability/toku_atomic.h>
#include <portability/toku_pthread.h>
#include <portability/toku_random.h>
#include <portability/toku_time.h>

#include <src/ydb-internal.h>

#include <ft/ybt.h>

#include <util/rwlock.h>
#include <util/kibbutz.h>

static const size_t min_val_size = sizeof(int32_t);
static const size_t min_key_size = sizeof(int32_t);

volatile bool run_test; // should be volatile since we are communicating through this variable.

typedef struct arg *ARG;
typedef int (*operation_t)(DB_TXN *txn, ARG arg, void *operation_extra, void *stats_extra);

// TODO: Properly define these in db.h so we don't have to copy them here
typedef int (*test_update_callback_f)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra);
typedef int (*test_generate_row_for_put_callback)(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_data);
typedef int (*test_generate_row_for_del_callback)(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, const DBT *src_key, const DBT *src_data);

enum stress_lock_type {
    STRESS_LOCK_NONE = 0,
    STRESS_LOCK_SHARED,
    STRESS_LOCK_EXCL
};

struct env_args {
    int node_size;
    int basement_node_size;
    int rollback_node_size;
    int checkpointing_period;
    int cleaner_period;
    int cleaner_iterations;
    int sync_period;
    uint64_t lk_max_memory;
    uint64_t cachetable_size;
    uint32_t num_bucket_mutexes;
    const char *envdir;
    test_update_callback_f update_function; // update callback function
    test_generate_row_for_put_callback generate_put_callback;
    test_generate_row_for_del_callback generate_del_callback;
};

enum perf_output_format {
    HUMAN = 0,
    CSV,
    TSV,
    NUM_OUTPUT_FORMATS
};

struct cli_args {
    int num_elements; // number of elements per DB
    int num_DBs; // number of DBs
    int num_seconds; // how long test should run
    int join_timeout; // how long to wait for threads to join before assuming deadlocks
    bool only_create; // true if want to only create DBs but not run stress
    bool only_stress; // true if DBs are already created and want to only run stress
    int update_broadcast_period_ms; // specific to test_stress3
    int num_ptquery_threads; // number of threads to run point queries
    bool do_test_and_crash; // true if we should crash after running stress test. For recovery tests.
    bool do_recover; // true if we should run recover
    int num_update_threads; // number of threads running updates
    int num_put_threads; // number of threads running puts
    int range_query_limit; // how many rows to look at for range queries
    bool serial_insert;
    bool interleave; // for insert benchmarks, whether to interleave separate threads' puts (or segregate them)
    bool crash_on_operation_failure; 
    bool print_performance;
    bool print_thread_performance;
    bool print_iteration_performance;
    enum perf_output_format perf_output_format;
    enum toku_compression_method compression_method; // the compression method to use on newly created DBs
    int performance_period;
    uint32_t txn_size; // specifies number of updates/puts/whatevers per txn
    uint32_t key_size; // number of bytes in vals. Must be at least 4
    uint32_t val_size; // number of bytes in vals. Must be at least 4
    double compressibility; // the row values should compress down to this fraction
    struct env_args env_args; // specifies environment variables
    bool single_txn;
    bool warm_cache; // warm caches before running stress_table
    bool blackhole; // all message injects are no-ops. helps measure txn/logging/locktree overhead.
    bool nolocktree; // use this flag to avoid the locktree on insertions
    bool unique_checks; // use uniqueness checking during insert. makes it slow.
    uint32_t sync_period; // background log fsync period
    bool nolog; // do not log. useful for testing in memory performance.
    bool nocrashstatus; // do not print engine status upon crash
    bool prelock_updates; // update threads perform serial updates on a prelocked range
    bool disperse_keys; // spread the keys out during a load (by reversing the bits in the loop index) to make a wide tree we can spread out random inserts into
    bool direct_io; // use direct I/O
    const char *print_engine_status; // print engine status rows matching a simple regex "a|b|c", matching strings where a or b or c is a subtring.
};

struct arg {
    DB **dbp; // array of DBs
    DB_ENV* env; // environment used
    bool bounded_element_range; // true if elements in dictionary are bounded
                                // by num_elements, that is, all keys in each
                                // DB are in [0, num_elements)
                                // false otherwise
    int sleep_ms; // number of milliseconds to sleep between operations
    uint32_t txn_flags; // isolation level for txn running operation
    operation_t operation; // function that is the operation to be run
    void* operation_extra; // extra parameter passed to operation
    enum stress_lock_type lock_type; // states if operation must be exclusive, shared, or does not require locking
    struct random_data *random_data; // state for random_r
    int thread_idx;
    int num_threads;
    struct cli_args *cli;
    bool do_prepare;
    bool prelock_updates;
    bool track_thread_performance;
};

static void arg_init(struct arg *arg, DB **dbp, DB_ENV *env, struct cli_args *cli_args) {
    arg->cli = cli_args;
    arg->dbp = dbp;
    arg->env = env;
    arg->bounded_element_range = true;
    arg->sleep_ms = 0;
    arg->lock_type = STRESS_LOCK_NONE;
    arg->txn_flags = DB_TXN_SNAPSHOT;
    arg->operation_extra = nullptr;
    arg->do_prepare = false;
    arg->prelock_updates = false;
    arg->track_thread_performance = true;
}

enum operation_type {
    OPERATION = 0,
    PUTS,
    PTQUERIES,
    NUM_OPERATION_TYPES
};

const char *operation_names[] = {
    "ops",
    "puts",
    "ptqueries",
    nullptr
};

static void increment_counter(void *extra, enum operation_type type, uint64_t inc) {
    invariant(type != OPERATION);
    int t = (int) type;
    invariant(extra);
    invariant(t >= 0 && t < (int) NUM_OPERATION_TYPES);
    uint64_t *CAST_FROM_VOIDP(counters, extra);
    counters[t] += inc;
}

struct perf_formatter {
    void (*header)(const struct cli_args *cli_args, const int num_threads);
    void (*iteration)(const struct cli_args *cli_args, const int current_time, uint64_t last_counters[][(int) NUM_OPERATION_TYPES], uint64_t *counters[], const int num_threads);
    void (*totals)(const struct cli_args *cli_args, uint64_t *counters[], const int num_threads);
};

static inline int
seconds_in_this_iteration(const int current_time, const int performance_period)
{
    const int iteration = (current_time + performance_period - 1) / performance_period;
    return current_time - ((iteration - 1) * performance_period);
}

static void
human_print_perf_header(const struct cli_args *UU(cli_args), const int UU(num_threads)) {}

static void
human_print_perf_iteration(const struct cli_args *cli_args, const int current_time, uint64_t last_counters[][(int) NUM_OPERATION_TYPES], uint64_t *counters[], const int num_threads)
{
    const int secondsthisiter = seconds_in_this_iteration(current_time, cli_args->performance_period);
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        uint64_t period_total = 0;
        printf("%4d %s", current_time, operation_names[op]);
        for (int i = strlen(operation_names[op]); i < 12; ++i) {
            printf(" ");
        }
        for (int t = 0; t < num_threads; ++t) {
            const uint64_t last = last_counters[t][op];
            const uint64_t current = counters[t][op];
            const uint64_t this_iter = current - last;
            if (cli_args->print_thread_performance) {
                const double persecond = (double) this_iter / secondsthisiter;
                printf("\t%'12" PRIu64 " (%'12.1lf/s)", this_iter, persecond);
            }
            period_total += this_iter;
            last_counters[t][op] = current;
        }
        const double totalpersecond = (double) period_total / secondsthisiter;
        printf("\tTotal %'12" PRIu64 " (%'12.1lf/s)\n", period_total, totalpersecond);
    }
    fflush(stdout);
}

static void
human_print_perf_totals(const struct cli_args *cli_args, uint64_t *counters[], const int num_threads)
{
    if (cli_args->print_iteration_performance) {
        printf("\n");
    }
    printf("Overall performance:\n");
    uint64_t overall_totals[(int) NUM_OPERATION_TYPES];
    ZERO_ARRAY(overall_totals);
    for (int t = 0; t < num_threads; ++t) {
        if (cli_args->print_thread_performance) {
            printf("Thread %4d: ", t + 1);
        }
        for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
            const uint64_t current = counters[t][op];
            if (cli_args->print_thread_performance) {
                const double persecond = (double) current / cli_args->num_seconds;
                printf("\t%s\t%'12" PRIu64 " (%'12.1lf/s)", operation_names[op], current, persecond);
            }
            overall_totals[op] += current;
        }
        if (cli_args->print_thread_performance) {
            printf("\n");
        }
    }
    printf("All threads: ");
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        const double totalpersecond = (double) overall_totals[op] / cli_args->num_seconds;
        printf("\t%s\t%'12" PRIu64 " (%'12.1lf/s)", operation_names[op], overall_totals[op], totalpersecond);
    }
    printf("\n");
}

static void
csv_print_perf_header(const struct cli_args *cli_args, const int num_threads)
{
    printf("seconds");
    if (cli_args->print_thread_performance) {
        for (int t = 1; t <= num_threads; ++t) {
            for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
                printf(",\"Thread %d %s\",\"Thread %d %s/s\"", t, operation_names[op], t, operation_names[op]);
            }
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        printf(",\"Total %s\",\"Total %s/s\"", operation_names[op], operation_names[op]);
    }
    printf("\n");
}

static void
csv_print_perf_iteration(const struct cli_args *cli_args, const int current_time, uint64_t last_counters[][(int) NUM_OPERATION_TYPES], uint64_t *counters[], const int num_threads)
{
    const int secondsthisiter = seconds_in_this_iteration(current_time, cli_args->performance_period);
    printf("%d", current_time);
    uint64_t period_totals[(int) NUM_OPERATION_TYPES];
    ZERO_ARRAY(period_totals);
    for (int t = 0; t < num_threads; ++t) {
        for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
            const uint64_t last = last_counters[t][op];
            const uint64_t current = counters[t][op];
            const uint64_t this_iter = current - last;
            if (cli_args->print_thread_performance) {
                const double persecond = (double) this_iter / secondsthisiter;
                printf(",%" PRIu64 ",%.1lf", this_iter, persecond);
            }
            period_totals[op] += this_iter;
            last_counters[t][op] = current;
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        const double totalpersecond = (double) period_totals[op] / secondsthisiter;
        printf(",%" PRIu64 ",%.1lf", period_totals[op], totalpersecond);
    }
    printf("\n");
    fflush(stdout);
}

static void
csv_print_perf_totals(const struct cli_args *cli_args, uint64_t *counters[], const int num_threads) {
    printf("overall");
    uint64_t overall_totals[(int) NUM_OPERATION_TYPES];
    ZERO_ARRAY(overall_totals);
    for (int t = 0; t < num_threads; ++t) {
        for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
            const uint64_t current = counters[t][op];
            if (cli_args->print_thread_performance) {
                const double persecond = (double) current / cli_args->num_seconds;
                printf(",%" PRIu64 ",%.1lf", current, persecond);
            }
            overall_totals[op] += current;
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        const double totalpersecond = (double) overall_totals[op] / cli_args->num_seconds;
        printf(",%" PRIu64 ",%.1lf", overall_totals[op], totalpersecond);
    }
    printf("\n");
}

static void
tsv_print_perf_header(const struct cli_args *cli_args, const int num_threads)
{
    printf("\"seconds\"");
    if (cli_args->print_thread_performance) {
        for (int t = 1; t <= num_threads; ++t) {
            for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
                printf("\t\"Thread %d %s\"\t\"Thread %d %s/s\"", t, operation_names[op], t, operation_names[op]);
            }
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        printf("\t\"Total %s\"\t\"Total %s/s\"", operation_names[op], operation_names[op]);
    }
    printf("\n");
}

static void
tsv_print_perf_iteration(const struct cli_args *cli_args, const int current_time, uint64_t last_counters[][(int) NUM_OPERATION_TYPES], uint64_t *counters[], const int num_threads)
{
    const int secondsthisiter = seconds_in_this_iteration(current_time, cli_args->performance_period);
    printf("%d", current_time);
    uint64_t period_totals[(int) NUM_OPERATION_TYPES];
    ZERO_ARRAY(period_totals);
    for (int t = 0; t < num_threads; ++t) {
        for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
            const uint64_t last = last_counters[t][op];
            const uint64_t current = counters[t][op];
            const uint64_t this_iter = current - last;
            if (cli_args->print_thread_performance) {
                const double persecond = (double) this_iter / secondsthisiter;
                printf("\t%" PRIu64 "\t%.1lf", this_iter, persecond);
            }
            period_totals[op] += this_iter;
            last_counters[t][op] = current;
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        const double totalpersecond = (double) period_totals[op] / secondsthisiter;
        printf("\t%" PRIu64 "\t%.1lf", period_totals[op], totalpersecond);
    }
    printf("\n");
    fflush(stdout);
}

static void
tsv_print_perf_totals(const struct cli_args *cli_args, uint64_t *counters[], const int num_threads) {
    printf("\"overall\"");
    uint64_t overall_totals[(int) NUM_OPERATION_TYPES];
    ZERO_ARRAY(overall_totals);
    for (int t = 0; t < num_threads; ++t) {
        for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
            const uint64_t current = counters[t][op];
            if (cli_args->print_thread_performance) {
                const double persecond = (double) current / cli_args->num_seconds;
                printf("\t%" PRIu64 "\t%.1lf", current, persecond);
            }
            overall_totals[op] += current;
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        const double totalpersecond = (double) overall_totals[op] / cli_args->num_seconds;
        printf("\t%" PRIu64 "\t%.1lf", overall_totals[op], totalpersecond);
    }
    printf("\n");
}

const struct perf_formatter perf_formatters[] = {
    [HUMAN] = {
        .header = human_print_perf_header,
        .iteration = human_print_perf_iteration,
        .totals = human_print_perf_totals
    },
    [CSV] = {
        .header = csv_print_perf_header,
        .iteration = csv_print_perf_iteration,
        .totals = csv_print_perf_totals
    },
    [TSV] = {
        .header = tsv_print_perf_header,
        .iteration = tsv_print_perf_iteration,
        .totals = tsv_print_perf_totals
    },
};

static int get_env_open_flags(struct cli_args *args) {
    int flags = DB_INIT_LOCK|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE;
    flags |= args->nolog ? 0 : DB_INIT_LOG;
    return flags;
}

static int get_put_flags(struct cli_args *args) {
    int flags = 0;
    flags |= args->nolocktree ? DB_PRELOCKED_WRITE : 0;
    flags |= args->unique_checks ? DB_NOOVERWRITE : 0;
    return flags;
}

static int get_commit_flags(struct cli_args *args) {
    int flags = 0;
    flags |= args->env_args.sync_period > 0 ? DB_TXN_NOSYNC : 0;
    return flags;
}

struct worker_extra {
    struct arg* thread_arg;
    toku_mutex_t *operation_lock_mutex;
    struct rwlock *operation_lock;
    uint64_t *counters;
    int64_t pad[4]; // pad to 64 bytes
};

static void lock_worker_op(struct worker_extra* we) {
    ARG arg = we->thread_arg;
    if (arg->lock_type != STRESS_LOCK_NONE) {
        toku_mutex_lock(we->operation_lock_mutex);
        if (arg->lock_type == STRESS_LOCK_SHARED) {
            rwlock_read_lock(we->operation_lock, we->operation_lock_mutex);
        } else if (arg->lock_type == STRESS_LOCK_EXCL) {
            rwlock_write_lock(we->operation_lock, we->operation_lock_mutex);
        } else {
            abort();
        }
        toku_mutex_unlock(we->operation_lock_mutex);
    }
}

static void unlock_worker_op(struct worker_extra* we) {
    ARG arg = we->thread_arg;
    if (arg->lock_type != STRESS_LOCK_NONE) {
        toku_mutex_lock(we->operation_lock_mutex);
        if (arg->lock_type == STRESS_LOCK_SHARED) {
            rwlock_read_unlock(we->operation_lock);
        } else if (arg->lock_type == STRESS_LOCK_EXCL) {
            rwlock_write_unlock(we->operation_lock);
        } else {
            abort();
        }
        toku_mutex_unlock(we->operation_lock_mutex);
    }
}

static void *worker(void *arg_v) {
    int r;
    struct worker_extra* CAST_FROM_VOIDP(we, arg_v);
    ARG arg = we->thread_arg;
    struct random_data random_data;
    ZERO_STRUCT(random_data);
    char *XCALLOC_N(8, random_buf);
    r = myinitstate_r(random(), random_buf, 8, &random_data);
    assert_zero(r);
    arg->random_data = &random_data;
    DB_ENV *env = arg->env;
    DB_TXN *txn = nullptr;
    if (verbose) {
        toku_pthread_t self = toku_pthread_self();
        uintptr_t intself = (uintptr_t) self;
        printf("%lu starting %p\n", (unsigned long) intself, arg->operation);
    }
    if (arg->cli->single_txn) {
        r = env->txn_begin(env, 0, &txn, arg->txn_flags); CKERR(r);
    }
    while (run_test) {
        lock_worker_op(we);
        if (!arg->cli->single_txn) {
            r = env->txn_begin(env, 0, &txn, arg->txn_flags); CKERR(r);
        }
        r = arg->operation(txn, arg, arg->operation_extra, we->counters);
        if (r==0 && !arg->cli->single_txn && arg->do_prepare) {
            uint8_t gid[DB_GID_SIZE];
            memset(gid, 0, DB_GID_SIZE);
            uint64_t gid_val = txn->id64(txn);
            uint64_t *gid_count_p = cast_to_typeof(gid_count_p) gid;  // make gcc --happy about -Wstrict-aliasing
            *gid_count_p = gid_val;
            int rr = txn->prepare(txn, gid);
            assert_zero(rr);
        }
        if (r == 0) {
            if (!arg->cli->single_txn) {
                int flags = get_commit_flags(arg->cli);
                int chk_r = txn->commit(txn, flags); CKERR(chk_r);
            }
        } else {
            if (arg->cli->crash_on_operation_failure) {
                CKERR(r);
            } else {
                if (!arg->cli->single_txn) {
                    { int chk_r = txn->abort(txn); CKERR(chk_r); }
                }
            }
        }
        unlock_worker_op(we);
        if (arg->track_thread_performance) {
            we->counters[OPERATION]++;
        }
        if (arg->sleep_ms) {
            usleep(arg->sleep_ms * 1000);
        }
    }
    if (arg->cli->single_txn) {
        int flags = get_commit_flags(arg->cli);
        int chk_r = txn->commit(txn, flags); CKERR(chk_r);
    }
    if (verbose) {
        toku_pthread_t self = toku_pthread_self();
        uintptr_t intself = (uintptr_t) self;
        printf("%lu returning\n", (unsigned long) intself);
    }
    toku_free(random_buf);
    return arg;
}

struct scan_cb_extra {
    bool fast;
    int curr_sum;
    int num_elements;
};

struct scan_op_extra {
    bool fast;
    bool fwd;
    bool prefetch;
};

static int
scan_cb(const DBT *key, const DBT *val, void *arg_v) {
    struct scan_cb_extra *CAST_FROM_VOIDP(cb_extra, arg_v);
    assert(key);
    assert(val);
    assert(cb_extra);
    assert(val->size >= sizeof(int));
    cb_extra->curr_sum += *(int *) val->data;
    cb_extra->num_elements++;
    return cb_extra->fast ? TOKUDB_CURSOR_CONTINUE : 0;
}

static int scan_op_and_maybe_check_sum(
    DB* db, 
    DB_TXN *txn, 
    struct scan_op_extra* sce, 
    bool check_sum
    ) 
{
    int r = 0;
    DBC* cursor = nullptr;

    struct scan_cb_extra e = {
        e.fast = sce->fast,
        e.curr_sum = 0,
        e.num_elements = 0,
    };

    { int chk_r = db->cursor(db, txn, &cursor, 0); CKERR(chk_r); }
    if (sce->prefetch) {
        r = cursor->c_set_bounds(cursor, db->dbt_neg_infty(), db->dbt_pos_infty(), true, 0);
        assert(r == 0);
    }
    while (r != DB_NOTFOUND) {
        if (sce->fwd) {
            r = cursor->c_getf_next(cursor, 0, scan_cb, &e);
        }
        else {
            r = cursor->c_getf_prev(cursor, 0, scan_cb, &e);
        }
        assert(r==0 || r==DB_NOTFOUND);
        if (!run_test) {
            // terminate early because this op takes a while under drd.
            // don't check the sum if we do this.
            check_sum = false;
            break;
        }
    }
    { int chk_r = cursor->c_close(cursor); CKERR(chk_r); }
    if (r == DB_NOTFOUND) {
        r = 0;
    }
    if (check_sum && e.curr_sum) {
        printf("e.curr_sum: %" PRId32 " e.num_elements: %" PRId32 " \n", e.curr_sum, e.num_elements);
        abort();
    }
    return r;
}

static int generate_row_for_put(
    DB *dest_db,
    DB *src_db,
    DBT_ARRAY *dest_keys,
    DBT_ARRAY *dest_vals,
    const DBT *src_key,
    const DBT *src_val
    )
{
    invariant(!src_db || src_db != dest_db);
    invariant(src_key->size >= sizeof(unsigned int));

    // Consistent pseudo random source.  Use checksum of key and val, and which db as seed
    
/*
    struct x1764 l;
    x1764_init(&l);
    x1764_add(&l, src_key->data, src_key->size);
    x1764_add(&l, src_val->data, src_val->size);
    x1764_add(&l, &dest_db, sizeof(dest_db)); //make it depend on which db
    unsigned int seed = x1764_finish(&l);
    */
    unsigned int seed = *(unsigned int*)src_key->data;

    struct random_data random_data;
    ZERO_STRUCT(random_data);
    char random_buf[8];
    {
        int r = myinitstate_r(seed, random_buf, 8, &random_data);
        assert_zero(r);
    }

    uint8_t num_outputs = 0;
    while (myrandom_r(&random_data) % 2) {
        num_outputs++;
        if (num_outputs > 8) {
            break;
        }
    }

    toku_dbt_array_resize(dest_keys, num_outputs);
    toku_dbt_array_resize(dest_vals, num_outputs);
    int sum = 0;
    for (uint8_t i = 0; i < num_outputs; i++) {
        DBT *dest_key = &dest_keys->dbts[i];
        DBT *dest_val = &dest_vals->dbts[i];

        invariant(dest_key->flags == DB_DBT_REALLOC);
        invariant(dest_val->flags == DB_DBT_REALLOC);

        if (dest_key->ulen < src_key->size) {
            dest_key->data = toku_xrealloc(dest_key->data, src_key->size);
            dest_key->ulen = src_key->size;
        }
        dest_key->size = src_key->size;
        if (dest_val->ulen < src_val->size) {
            dest_val->data = toku_xrealloc(dest_val->data, src_val->size);
            dest_val->ulen = src_val->size;
        }
        dest_val->size = src_val->size;
        memcpy(dest_key->data, src_key->data, src_key->size);
        ((uint8_t*)dest_key->data)[src_key->size-1] = i;  //Have different keys for each entry.

        memcpy(dest_val->data, src_val->data, src_val->size);
        invariant(dest_val->size >= sizeof(int));
        int number;
        if (i == num_outputs - 1) {
            // Make sum add to 0
            number = -sum;
        } else {
            // Keep track of sum
            number = myrandom_r(&random_data);
        }
        sum += number;
        *(int *) dest_val->data = number;
    }
    invariant(sum == 0);
    return 0;
}

// How Keys Work:
//
// Keys are either
// - 4 byte little endian non-negative integers
// - 8 byte little endian non-negative integers
// - 8 byte little endian non-negative integers, padded with zeroes.
//
// The comparison function treats the key as a 4 byte
// int if the key size is exactly 4, and it treats
// the key as an 8 byte int if the key size is 8 or more.

static int64_t random_bounded_key(struct random_data *random_data, ARG arg) {
// Effect: Returns a random key in the table, possible bounded by the number of elements.
    int64_t key = myrandom_r(random_data);
    if (arg->bounded_element_range && arg->cli->num_elements > 0) {
        key = key % arg->cli->num_elements;
    }
    return key;
}

static int64_t breverse(int64_t v)
// Effect: return the bits in i, reversed
// Notes: implementation taken from http://graphics.stanford.edu/~seander/bithacks.html#BitReverseObvious
// Rationale: just a hack to spread out the keys during loading, doesn't need to be fast but does need to be correct.
{
    uint64_t k = v; // r will be reversed bits of v; first get LSB of v
    int s = sizeof(v) * CHAR_BIT - 1; // extra shift needed at end

    for (v >>= 1; v; v >>= 1) {
        k <<= 1;
        k |= v & 1;
        s--;
    }
    k <<= s; // shift when v's highest bits are zero
    int64_t r = k;
    return r & ~(1ULL << 63);
}

static void
fill_key_buf(int64_t key, uint8_t *data, struct cli_args *args) {
// Effect: Fill data with a specific little-endian integer, 4 or 8 bytes long
//         depending on args->key_size, possibly padded with zeroes.
// Requires: *data is at least sizeof(uint64_t)
    if (args->disperse_keys) {
        key = breverse(key);
    }
    invariant(key >= 0);
    if (args->key_size == sizeof(int)) {
        const int key32 = key;
        memcpy(data, &key32, sizeof(key32));
    } else {
        invariant(args->key_size >= sizeof(key));
        memcpy(data, &key, sizeof(key));
        memset(data + sizeof(key), 0, args->key_size - sizeof(key));
    }
}

static void
fill_key_buf_random(struct random_data *random_data, uint8_t *data, ARG arg) {
// Effect: Fill data with a random, little-endian, 4 or 8 byte integer, possibly
// bounded by the size of the table, and padded with zeroes until key_size.
// Requires, Notes: see fill_key_buf()
    int64_t key = random_bounded_key(random_data, arg);
    fill_key_buf(key, data, arg->cli);
}

// How Vals Work:
//
// Values are either
// - 4 byte little endian integers
// - 4 byte little endian integers, padded with zeroes
// - X bytes random values, Y bytes zeroes, where X and Y
// are derived from the desired compressibility;
//
// Correctness tests use integer values, perf tests use random bytes.
// Both support padding out values > 4 bytes with zeroes.

static void
fill_val_buf(int64_t val, uint8_t *data, uint32_t val_size) {
// Effect, Requires, Notes: see fill_key_buf().
    if (val_size == sizeof(int)) {
        const int val32 = val;
        memcpy(data, &val32, sizeof(val32));
    } else {
        invariant(val_size >= sizeof(val));
        memcpy(data, &val, sizeof(val));
        memset(data + sizeof(val), 0, val_size - sizeof(val));
    }
}

// Fill array with compressibility*size 0s.
// 0.0<=compressibility<=1.0
// Compressibility is the fraction of size that will be 0s (e.g. approximate fraction that will be compressed away).
// The rest will be random data.
static void
fill_val_buf_random(struct random_data *random_data, uint8_t *data, struct cli_args *args) {
    invariant(args->val_size >= min_val_size);
    //Requires: The array was zeroed since the last time 'size' was changed.
    //Requires: compressibility is in range [0,1] indicating fraction that should be zeros.

    // Fill in the random bytes
    uint32_t num_random_bytes = (1 - args->compressibility) * args->val_size;
    if (num_random_bytes > 0) {
        uint32_t filled;
        for (filled = 0; filled + sizeof(uint64_t) <= num_random_bytes; filled += sizeof(uint64_t)) {
            *((uint64_t *) &data[filled]) = myrandom_r(random_data);
        }
        if (filled != num_random_bytes) {
            uint64_t last8 = myrandom_r(random_data);
            memcpy(&data[filled], &last8, num_random_bytes - filled);
        }
    }

    // Fill in the zero bytes
    if (num_random_bytes < args->val_size) {
        memset(data + num_random_bytes, 0, args->val_size - num_random_bytes);
    }
}

static int random_put_in_db(DB *db, DB_TXN *txn, ARG arg, bool ignore_errors, void *stats_extra) {
    int r = 0;
    uint8_t keybuf[arg->cli->key_size];
    uint8_t valbuf[arg->cli->val_size];

    DBT key, val;
    dbt_init(&key, keybuf, sizeof keybuf);
    dbt_init(&val, valbuf, sizeof valbuf);
    const int put_flags = get_put_flags(arg->cli);

    uint64_t puts_to_increment = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; ++i) {
        fill_key_buf_random(arg->random_data, keybuf, arg);
        fill_val_buf_random(arg->random_data, valbuf, arg->cli);
        r = db->put(db, txn, &key, &val, put_flags);
        if (!ignore_errors && r != 0) {
            goto cleanup;
        }
        puts_to_increment++;
        if (puts_to_increment == 100) {
            increment_counter(stats_extra, PUTS, puts_to_increment);
            puts_to_increment = 0;
        }
    }

cleanup:
    increment_counter(stats_extra, PUTS, puts_to_increment);
    return r;
}

static int UU() random_put_op(DB_TXN *txn, ARG arg, void *UU(operation_extra), void *stats_extra) {
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    return random_put_in_db(db, txn, arg, false, stats_extra);
}

static int UU() random_put_op_singledb(DB_TXN *txn, ARG arg, void *UU(operation_extra), void *stats_extra) {
    int db_index = arg->thread_idx%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    return random_put_in_db(db, txn, arg, false, stats_extra);
}

struct serial_put_extra {
    uint64_t current;
};

static int UU() serial_put_op(DB_TXN *txn, ARG arg, void *operation_extra, void *stats_extra) {
    struct serial_put_extra *CAST_FROM_VOIDP(extra, operation_extra);

    int db_index = arg->thread_idx % arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];

    int r = 0;
    uint8_t keybuf[arg->cli->key_size];
    uint8_t valbuf[arg->cli->val_size];

    DBT key, val;
    dbt_init(&key, keybuf, sizeof keybuf);
    dbt_init(&val, valbuf, sizeof valbuf);
    const int put_flags = get_put_flags(arg->cli);

    uint64_t puts_to_increment = 0;
    for (uint64_t i = 0; i < arg->cli->txn_size; ++i) {
        // TODO: Change perf_insert to pass a single serial_put_op_extra
        // to each insertion thread so they share the current key,
        // and use a sync fetch an add here. This way you can measure
        // the true performance of multiple threads appending unique
        // keys to the end of a tree.
        uint64_t k = extra->current++;
        fill_key_buf(k, keybuf, arg->cli);
        fill_val_buf_random(arg->random_data, valbuf, arg->cli);
        r = db->put(db, txn, &key, &val, put_flags);
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
    increment_counter(stats_extra, PUTS, puts_to_increment);
    return r;
}

struct loader_op_extra {
    struct scan_op_extra soe;
    int num_dbs;
};

static int UU() loader_op(DB_TXN* txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    struct loader_op_extra* CAST_FROM_VOIDP(extra, operation_extra);
    invariant(extra->num_dbs >= 1);
    DB_ENV* env = arg->env;
    int r;
    for (int num = 0; num < 2; num++) {
        DB *dbs_load[extra->num_dbs];
        uint32_t db_flags[extra->num_dbs];
        uint32_t dbt_flags[extra->num_dbs];
        for (int i = 0; i < extra->num_dbs; ++i) {
            db_flags[i] = 0;
            dbt_flags[i] = 0;
            r = db_create(&dbs_load[i], env, 0);
            assert(r == 0);
            char fname[100];
            sprintf(fname, "loader-db-%d", i);
            // TODO: Need to call before_db_open_hook() and after_db_open_hook()
            r = dbs_load[i]->open(dbs_load[i], txn, fname, nullptr, DB_BTREE, DB_CREATE, 0666);
            assert(r == 0);
        }
        DB_LOADER *loader;
        uint32_t loader_flags = (num == 0) ? 0 : LOADER_COMPRESS_INTERMEDIATES;
        r = env->create_loader(env, txn, &loader, dbs_load[0], extra->num_dbs, dbs_load, db_flags, dbt_flags, loader_flags);
        CKERR(r);

        DBT key, val;
        uint8_t keybuf[arg->cli->key_size];
        uint8_t valbuf[arg->cli->val_size];
        dbt_init(&key, keybuf, sizeof keybuf);
        dbt_init(&val, valbuf, sizeof valbuf);

        int sum = 0;
        const int num_elements = 1000;
        for (int i = 0; i < num_elements; i++) {
            fill_key_buf(i, keybuf, arg->cli);
            fill_val_buf_random(arg->random_data, valbuf, arg->cli);

            assert(val.size >= sizeof(int));
            if (i == num_elements - 1) {
                // Make sum add to 0
                *(int *) val.data = -sum;
            } else {
                // Keep track of sum
                sum += *(int *) val.data;
            }
            r = loader->put(loader, &key, &val); CKERR(r);
        }

        r = loader->close(loader); CKERR(r);

        for (int i = 0; i < extra->num_dbs; ++i) {
            r = scan_op_and_maybe_check_sum(dbs_load[i], txn, &extra->soe, true); CKERR(r);
            r = dbs_load[i]->close(dbs_load[i], 0); CKERR(r);
            char fname[100];
            sprintf(fname, "loader-db-%d", i);
            r = env->dbremove(env, txn, fname, nullptr, 0); CKERR(r);
        }
    }
    return 0;
}

static int UU() keyrange_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    // Pick a random DB, do a keyrange operation.
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];

    int r = 0;
    uint8_t keybuf[arg->cli->key_size];

    DBT key;
    dbt_init(&key, keybuf, sizeof keybuf);
    fill_key_buf_random(arg->random_data, keybuf, arg);

    uint64_t less,equal,greater;
    int is_exact;
    r = db->key_range64(db, txn, &key, &less, &equal, &greater, &is_exact);
    assert(r == 0);
    return r;
}

static void UU() get_key_after_bytes_callback(const DBT *UU(end_key), uint64_t UU(skipped), void *UU(extra)) {
    // nothing
}

static int UU() get_key_after_bytes_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    // Pick a random DB, do a get_key_after_bytes operation.
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];

    int r = 0;
    uint8_t keybuf[arg->cli->key_size];

    DBT start_key, end_key;
    dbt_init(&start_key, keybuf, sizeof keybuf);
    fill_key_buf_random(arg->random_data, keybuf, arg);
    uint64_t skip_len = myrandom_r(arg->random_data) % (2<<30);
    dbt_init(&end_key, nullptr, 0);

    r = db->get_key_after_bytes(db, txn, &start_key, skip_len, get_key_after_bytes_callback, nullptr, 0);
    return r;
}

static int verify_progress_callback(void *UU(extra), float UU(progress)) {
    if (!run_test) {
        return -1;
    }
    return 0;
}

static int UU() verify_op(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra), void *UU(stats_extra)) {
    int r = 0;
    for (int i = 0; i < arg->cli->num_DBs && run_test; i++) {
        DB* db = arg->dbp[i];
        r = db->verify_with_progress(db, verify_progress_callback, nullptr, 1, 0);
        if (!run_test) {
            r = 0;
        }
        CKERR(r);
    }
    return r;
}

static int UU() scan_op(DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    struct scan_op_extra* CAST_FROM_VOIDP(extra, operation_extra);
    for (int i = 0; run_test && i < arg->cli->num_DBs; i++) {
        int r = scan_op_and_maybe_check_sum(arg->dbp[i], txn, extra, true);
        assert_zero(r);
    }
    return 0;
}

static int UU() scan_op_no_check(DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    struct scan_op_extra* CAST_FROM_VOIDP(extra, operation_extra);
    for (int i = 0; run_test && i < arg->cli->num_DBs; i++) {
        int r = scan_op_and_maybe_check_sum(arg->dbp[i], txn, extra, false);
        assert_zero(r);
    }
    return 0;
}

struct scan_op_worker_info {
    DB *db;
    DB_TXN *txn;
    void *extra;
};

static void scan_op_worker(void *arg) {
    struct scan_op_worker_info *CAST_FROM_VOIDP(info, arg);
    struct scan_op_extra *CAST_FROM_VOIDP(extra, info->extra);
    int r = scan_op_and_maybe_check_sum(
            info->db,
            info->txn,
            extra,
            false
            );
    assert_zero(r);
    toku_free(info);
}

static int UU() scan_op_no_check_parallel(DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    const int num_cores = toku_os_get_number_processors();
    const int num_workers = arg->cli->num_DBs < num_cores ? arg->cli->num_DBs : num_cores;
    KIBBUTZ kibbutz = toku_kibbutz_create(num_workers);
    for (int i = 0; run_test && i < arg->cli->num_DBs; i++) {
        struct scan_op_worker_info *XCALLOC(info);
        info->db = arg->dbp[i];
        info->txn = txn;
        info->extra = operation_extra;
        toku_kibbutz_enq(kibbutz, scan_op_worker, info);
    }
    toku_kibbutz_destroy(kibbutz);
    return 0;
}

static int dbt_do_nothing (DBT const *UU(key), DBT  const *UU(row), void *UU(context)) {
  return 0;
}

static int UU() ptquery_and_maybe_check_op(DB* db, DB_TXN *txn, ARG arg, bool check) {
    int r = 0;
    uint8_t keybuf[arg->cli->key_size];
    DBT key, val;
    dbt_init(&key, keybuf, sizeof keybuf);
    dbt_init(&val, nullptr, 0);
    fill_key_buf_random(arg->random_data, keybuf, arg);

    r = db->getf_set(
        db, 
        txn, 
        0, 
        &key, 
        dbt_do_nothing, 
        nullptr
        );
    if (check) {
        assert(r != DB_NOTFOUND);
    }
    r = 0;
    return r;
}

static int UU() ptquery_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *stats_extra) {
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    int r = ptquery_and_maybe_check_op(db, txn, arg, true);
    if (!r) {
        increment_counter(stats_extra, PTQUERIES, 1);
    }
    return r;
}

static int UU() ptquery_op_no_check(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *stats_extra) {
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    int r = ptquery_and_maybe_check_op(db, txn, arg, false);
    if (!r) {
        increment_counter(stats_extra, PTQUERIES, 1);
    }
    return r;
}

typedef void (*rangequery_row_cb)(DB *db, const DBT *key, const DBT *val, void *extra);
struct rangequery_cb_extra {
    int rows_read;

    // Call cb(db, key, value, cb_extra) on up to $limit rows.
    const int limit;
    const rangequery_row_cb cb;
    DB *const db;
    void *const cb_extra;
};

static int rangequery_cb(const DBT *key, const DBT *value, void *extra) {
    struct rangequery_cb_extra *CAST_FROM_VOIDP(info, extra);
    if (info->cb != nullptr) {
        info->cb(info->db, key, value, info->cb_extra);
    }
    if (++info->rows_read >= info->limit) {
        return 0;
    } else {
        return TOKUDB_CURSOR_CONTINUE;
    }
}

static void rangequery_db(DB *db, DB_TXN *txn, ARG arg, rangequery_row_cb cb, void *cb_extra) {
    const int limit = arg->cli->range_query_limit;

    int r;
    DBC *cursor;
    DBT start_key, end_key;
    uint8_t start_keybuf[arg->cli->key_size];
    uint8_t end_keybuf[arg->cli->key_size];
    dbt_init(&start_key, start_keybuf, sizeof start_keybuf);
    dbt_init(&end_key, end_keybuf, sizeof end_keybuf);
    const uint64_t start_k = random_bounded_key(arg->random_data, arg);
    fill_key_buf(start_k, start_keybuf, arg->cli);
    fill_key_buf(start_k + limit, end_keybuf, arg->cli);

    r = db->cursor(db, txn, &cursor, 0); CKERR(r);
    r = cursor->c_set_bounds(cursor, &start_key, &end_key, true, 0); CKERR(r);

    struct rangequery_cb_extra extra = {
        .rows_read = 0,
        .limit = limit,
        .cb = cb,
        .db = db,
        .cb_extra = cb_extra,
    };
    r = cursor->c_getf_set(cursor, 0, &start_key, rangequery_cb, &extra);
    while (r == 0 && extra.rows_read < extra.limit && run_test) {
        r = cursor->c_getf_next(cursor, 0, rangequery_cb, &extra);
    }

    r = cursor->c_close(cursor); CKERR(r);
}

static int UU() rangequery_op(DB_TXN *txn, ARG arg, void *UU(operation_extra), void *stats_extra) {
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB *db = arg->dbp[db_index];
    rangequery_db(db, txn, arg, nullptr, nullptr);
    increment_counter(stats_extra, PTQUERIES, 1);
    return 0;
}

static int UU() cursor_create_close_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    int db_index = arg->cli->num_DBs > 1 ? myrandom_r(arg->random_data)%arg->cli->num_DBs : 0;
    DB* db = arg->dbp[db_index];
    DBC* cursor = nullptr;
    int r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
    r = cursor->c_close(cursor); assert(r == 0);
    return 0;
}

#define MAX_RANDOM_VAL 10000

enum update_type {
    UPDATE_ADD_DIFF,
    UPDATE_NEGATE,
    UPDATE_WITH_HISTORY
};

struct update_op_extra {
    enum update_type type;
    int pad_bytes;
    union {
        struct {
            int diff;
        } d;
        struct {
            int expected;
            int new_val;
        } h;
    } u;
};

struct update_op_args {
    int *update_history_buffer;
    int update_pad_frequency;
};

static struct update_op_args UU() get_update_op_args(struct cli_args* cli_args, int* update_history_buffer) {
    struct update_op_args uoe;
    uoe.update_history_buffer = update_history_buffer;
    uoe.update_pad_frequency = cli_args->num_elements/100; // arbitrary
    return uoe;
}

static uint64_t update_count = 0;

static int update_op_callback(DB *UU(db), const DBT *UU(key),
                              const DBT *old_val,
                              const DBT *extra,
                              void (*set_val)(const DBT *new_val,
                                              void *set_extra),
                              void *set_extra)
{
    int old_int_val = 0;
    if (old_val) {
        old_int_val = *(int *) old_val->data;
    }
    assert(extra->size == sizeof(struct update_op_extra));
    struct update_op_extra *CAST_FROM_VOIDP(e, extra->data);

    int new_int_val;
    switch (e->type) {
    case UPDATE_ADD_DIFF:
        new_int_val = old_int_val + e->u.d.diff;
        break;
    case UPDATE_NEGATE:
        new_int_val = -old_int_val;
        break;
    case UPDATE_WITH_HISTORY:
        assert(old_int_val == e->u.h.expected);
        new_int_val = e->u.h.new_val;
        break;
    default:
        abort();
    }

    uint32_t val_size = sizeof(int) + e->pad_bytes;
    uint8_t valbuf[val_size];
    fill_val_buf(new_int_val, valbuf, val_size);

    DBT new_val;
    dbt_init(&new_val, valbuf, val_size);
    set_val(&new_val, set_extra);
    return 0;
}

static int UU() update_op2(DB_TXN* txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];

    int r = 0;
    DBT key, val;
    uint8_t keybuf[arg->cli->key_size];

    toku_sync_fetch_and_add(&update_count, 1);
    struct update_op_extra extra;
    ZERO_STRUCT(extra);
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    int curr_val_sum = 0;

    dbt_init(&key, keybuf, sizeof keybuf);
    dbt_init(&val, &extra, sizeof extra);

    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        fill_key_buf_random(arg->random_data, keybuf, arg);
        extra.u.d.diff = 1;
        curr_val_sum += extra.u.d.diff;
        r = db->update(
            db,
            txn,
            &key,
            &val,
            0
            );
        if (r != 0) {
            return r;
        }
        int *rkp = (int *) keybuf;
        int rand_key = *rkp;
        invariant(rand_key != (arg->cli->num_elements - rand_key));
        rand_key -= arg->cli->num_elements;
        fill_key_buf(rand_key, keybuf, arg->cli);
        extra.u.d.diff = -1;
        r = db->update(
            db,
            txn,
            &key,
            &val,
            0
            );
        if (r != 0) {
            return r;
        }
    }
    return r;
}

static int pre_acquire_write_lock(DB *db, DB_TXN *txn,
        const DBT *left_key, const DBT *right_key) {
    int r;
    DBC *cursor;

    r = db->cursor(db, txn, &cursor, DB_RMW);
    CKERR(r);
    int cursor_r = cursor->c_set_bounds(cursor, left_key, right_key, true, 0);
    r = cursor->c_close(cursor);
    CKERR(r);

    return cursor_r;
}

// take the given db and do an update on it
static int
UU() update_op_db(DB *db, DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    uint64_t old_update_count = toku_sync_fetch_and_add(&update_count, 1);
    struct update_op_args* CAST_FROM_VOIDP(op_args, operation_extra);
    struct update_op_extra extra;
    ZERO_STRUCT(extra);
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    if (op_args->update_pad_frequency) {
        if (old_update_count % (2*op_args->update_pad_frequency) == old_update_count%op_args->update_pad_frequency) {
            extra.pad_bytes = 100;
        }
    }

    int r = 0;
    DBT key, val;
    uint8_t keybuf[arg->cli->key_size];
    int update_key;
    int curr_val_sum = 0;
    const int update_flags = arg->cli->prelock_updates ? DB_PRELOCKED_WRITE : 0;

    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        if (arg->prelock_updates) {
            if (i == 0) {
                update_key = random_bounded_key(arg->random_data, arg);

                const int max_key_in_table = arg->cli->num_elements - 1;
                const bool range_wraps = (update_key + (int) arg->cli->txn_size - 1) > max_key_in_table;
                int left_key, right_key;
                DBT left_key_dbt, right_key_dbt;

                // acquire the range starting at the random key, plus txn_size - 1
                // elements, but lock no further than the end of the table. if the
                // range wraps around to the beginning we will handle it below.
                left_key = update_key;
                right_key = range_wraps ? max_key_in_table : (left_key + arg->cli->txn_size - 1);
                r = pre_acquire_write_lock(
                        db,
                        txn,
                        dbt_init(&left_key_dbt, &left_key, sizeof update_key),
                        dbt_init(&right_key_dbt, &right_key, sizeof right_key)
                        );
                if (r != 0) {
                    return r;
                }

                // check if the right end point wrapped around to the beginning
                // if so, lock from 0 to the right key, modded by table size.
                if (range_wraps) {
                    right_key = (left_key + arg->cli->txn_size - 1) - max_key_in_table;
                    invariant(right_key > 0);
                    left_key = 0;
                    r = pre_acquire_write_lock(
                            db,
                            txn,
                            dbt_init(&left_key_dbt, &left_key, sizeof update_key),
                            dbt_init(&right_key_dbt, &right_key, sizeof right_key)
                            );
                    if (r != 0) {
                        return r;
                    }
                }
            } else {
                update_key++;
                if (arg->bounded_element_range) {
                    update_key = update_key % arg->cli->num_elements;
                }
            }
            fill_key_buf(update_key, keybuf, arg->cli);
        } else {
            // just do a usual, random point update without locking first
            fill_key_buf_random(arg->random_data, keybuf, arg);
        }


        // the last update keeps the table's sum as zero
        // every other update except the last applies a random delta
        if (i == arg->cli->txn_size - 1) {
            extra.u.d.diff = -curr_val_sum;
        } else {
            extra.u.d.diff = myrandom_r(arg->random_data) % MAX_RANDOM_VAL;
            // just make every other value random
            if (i%2 == 0) {
                extra.u.d.diff = -extra.u.d.diff;
            }
            curr_val_sum += extra.u.d.diff;
        }

        dbt_init(&key, keybuf, sizeof keybuf);
        dbt_init(&val, &extra, sizeof extra);

        // do the update
        r = db->update(
            db,
            txn,
            &key,
            &val,
            update_flags
            );
        if (r != 0) {
            return r;
        }
    }

    return r;
}

// choose a random DB and do an update on it
static int
UU() update_op(DB_TXN *txn, ARG arg, void* operation_extra, void *stats_extra) {
    int db_index = myrandom_r(arg->random_data) % arg->cli->num_DBs;
    DB *db = arg->dbp[db_index];
    return update_op_db(db, txn, arg, operation_extra, stats_extra);
}

static int UU() update_with_history_op(DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    struct update_op_args* CAST_FROM_VOIDP(op_args, operation_extra);
    assert(arg->bounded_element_range);
    assert(op_args->update_history_buffer);

    int r = 0;
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];

    struct update_op_extra extra;
    ZERO_STRUCT(extra);
    extra.type = UPDATE_WITH_HISTORY;
    uint64_t old_update_count = toku_sync_fetch_and_add(&update_count, 1);
    extra.pad_bytes = 0;
    if (op_args->update_pad_frequency) {
        if (old_update_count % (2*op_args->update_pad_frequency) != old_update_count%op_args->update_pad_frequency) {
            extra.pad_bytes = 500;
        }
    }

    DBT key, val;
    uint8_t keybuf[arg->cli->key_size];
    int rand_key;
    int curr_val_sum = 0;

    dbt_init(&key, keybuf, sizeof keybuf);
    dbt_init(&val, &extra, sizeof extra);

    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        fill_key_buf_random(arg->random_data, keybuf, arg);
        int *rkp = (int *) keybuf;
        rand_key = *rkp;
        invariant(rand_key < arg->cli->num_elements);
        if (i < arg->cli->txn_size - 1) {
            extra.u.h.new_val = myrandom_r(arg->random_data) % MAX_RANDOM_VAL;
            // just make every other value random
            if (i % 2 == 0) {
                extra.u.h.new_val = -extra.u.h.new_val;
            }
            curr_val_sum += extra.u.h.new_val;
        } else {
            // the last update should ensure the sum stays zero
            extra.u.h.new_val = -curr_val_sum;
        }
        extra.u.h.expected = op_args->update_history_buffer[rand_key];
        op_args->update_history_buffer[rand_key] = extra.u.h.new_val;
        r = db->update(
            db,
            txn,
            &key,
            &val,
            0
            );
        if (r != 0) {
            return r;
        }
    }

    return r;
}

static int UU() update_broadcast_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    struct update_op_extra extra;
    ZERO_STRUCT(extra);
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    extra.type = UPDATE_NEGATE;
    extra.pad_bytes = 0;
    DBT val;
    int r = db->update_broadcast(db, txn, dbt_init(&val, &extra, sizeof extra), 0);
    CKERR(r);
    return r;
}

static int hot_progress_callback(void *UU(extra), float UU(progress)) {
    return run_test ? 0 : 1;
}

static int UU() hot_op(DB_TXN *UU(txn), ARG UU(arg), void* UU(operation_extra), void *UU(stats_extra)) {
    int r;
    for (int i = 0; run_test && i < arg->cli->num_DBs; i++) {
        DB* db = arg->dbp[i];
        r = db->hot_optimize(db, NULL, NULL, hot_progress_callback, nullptr);
        if (run_test) {
            CKERR(r);
        }
    }
    return 0;
}

static void
get_ith_table_name(char *buf, size_t len, int i) {
    snprintf(buf, len, "main%d", i);
}

DB_TXN * const null_txn = 0;

// For each line of engine status output, look for lines that contain substrings
// that match any of the strings in the pattern string. The pattern string contains
// 0 or more strings separated by the '|' character, kind of like a regex.
static void print_matching_engine_status_rows(DB_ENV *env, const char *pattern) {
    uint64_t num_rows;
    env->get_engine_status_num_rows(env, &num_rows);
    uint64_t buf_size = num_rows * 128;
    const char *row;
    char *row_r;

    char *pattern_copy = toku_xstrdup(pattern);
    int num_patterns = 1;
    for (char *p = pattern_copy; *p != '\0'; p++) {
        if (*p == '|') {
            *p = '\0';
            num_patterns++;
        }
    }

    char *XMALLOC_N(buf_size, buf);
    int r = env->get_engine_status_text(env, buf, buf_size);
    invariant_zero(r);

    for (row = strtok_r(buf, "\n", &row_r); row != nullptr; row = strtok_r(nullptr, "\n", &row_r)) {
        const char *p = pattern_copy;
        for (int i = 0; i < num_patterns; i++, p += strlen(p) + 1) {
            if (strstr(row, p) != nullptr) {
                fprintf(stderr, "%s\n", row);
            }
        }
    }

    toku_free(pattern_copy);
    toku_free(buf);
    fflush(stderr);
}

// TODO: stuff like this should be in a generalized header somwhere
static inline int
intmin(const int a, const int b)
{
    if (a < b) {
        return a;
    }
    return b;
}

struct test_time_extra {
    DB_ENV *env;
    int num_seconds;
    bool crash_at_end;
    struct worker_extra *wes;
    int num_wes;
    struct cli_args *cli_args;
};

static void *test_time(void *arg) {
    struct test_time_extra* CAST_FROM_VOIDP(tte, arg);
    DB_ENV *env = tte->env;
    int num_seconds = tte->num_seconds;
    const struct perf_formatter *perf_formatter = &perf_formatters[tte->cli_args->perf_output_format];

    //
    // if num_Seconds is set to 0, run indefinitely
    //
    if (num_seconds == 0) {
        num_seconds = INT32_MAX;
    }
    uint64_t last_counter_values[tte->num_wes][(int) NUM_OPERATION_TYPES];
    ZERO_ARRAY(last_counter_values);
    uint64_t *counters[tte->num_wes];
    for (int t = 0; t < tte->num_wes; ++t) {
        counters[t] = tte->wes[t].counters;
    }
    if (verbose) {
        printf("Sleeping for %d seconds\n", num_seconds);
    }
    for (int i = 0; i < num_seconds; ) {
        struct timeval tv[2];
        const int sleeptime = intmin(tte->cli_args->performance_period, num_seconds - i);
        int r = gettimeofday(&tv[0], nullptr);
        assert_zero(r);
        usleep(sleeptime*1000*1000);
        r = gettimeofday(&tv[1], nullptr);
        assert_zero(r);
        int actual_sleeptime = tv[1].tv_sec - tv[0].tv_sec;
        if (abs(actual_sleeptime - sleeptime) <= 1) {
            // Close enough, no need to alarm the user, and we didn't check nsec.
            i += sleeptime;
        } else {
            if (verbose) {
                printf("tried to sleep %d secs, actually slept %d secs\n", sleeptime, actual_sleeptime);
            }
            i += actual_sleeptime;
        }
        if (tte->cli_args->print_performance && tte->cli_args->print_iteration_performance) {
            perf_formatter->iteration(tte->cli_args, i, last_counter_values, counters, tte->num_wes);
        }
        if (tte->cli_args->print_engine_status != nullptr) {
            print_matching_engine_status_rows(env, tte->cli_args->print_engine_status);
        }
    }

    if (verbose) {
        printf("should now end test\n");
    }
    toku_sync_bool_compare_and_swap(&run_test, true, false); // make this atomic to make valgrind --tool=drd happy.
    if (verbose) {
        printf("run_test %d\n", run_test);
    }
    if (tte->crash_at_end) {
        toku_hard_crash_on_purpose();
    }
    return arg;
}

struct sleep_and_crash_extra {
    toku_mutex_t mutex;
    toku_cond_t cond;
    int seconds;
    bool is_setup;
    bool threads_have_joined;
};

static void *sleep_and_crash(void *extra) {
    sleep_and_crash_extra *e = static_cast<sleep_and_crash_extra *>(extra);
    toku_mutex_lock(&e->mutex);
    struct timeval tv;
    toku_timespec_t ts;
    gettimeofday(&tv, nullptr);
    ts.tv_sec = tv.tv_sec + e->seconds;
    ts.tv_nsec = 0;
    e->is_setup = true;
    if (verbose) {
        printf("Waiting %d seconds for other threads to join.\n", e->seconds);
        fflush(stdout);
    }
    int r = toku_cond_timedwait(&e->cond, &e->mutex, &ts);
    toku_mutex_assert_locked(&e->mutex);
    if (r == ETIMEDOUT) {
        invariant(!e->threads_have_joined);
        if (verbose) {
            printf("Some thread didn't join on time, crashing.\n");
            fflush(stdout);
        }
        toku_crash_and_dump_core_on_purpose();
    } else {
        assert(r == 0);
        assert(e->threads_have_joined);
        if (verbose) {
            printf("Other threads joined on time, exiting cleanly.\n");
        }
    }
    toku_mutex_unlock(&e->mutex);
    return nullptr;
}

static int run_workers(
    struct arg *thread_args,
    int num_threads,
    uint32_t num_seconds,
    bool crash_at_end,
    struct cli_args* cli_args
    )
{
    int r;
    const struct perf_formatter *perf_formatter = &perf_formatters[cli_args->perf_output_format];
    toku_mutex_t mutex = ZERO_MUTEX_INITIALIZER;
    toku_mutex_init(&mutex, nullptr);
    struct rwlock rwlock;
    rwlock_init(&rwlock);
    toku_pthread_t tids[num_threads];
    toku_pthread_t time_tid;
    if (cli_args->print_performance) {
        perf_formatter->header(cli_args, num_threads);
    }
    // allocate worker_extra's on cache line boundaries
    struct worker_extra *XMALLOC_N_ALIGNED(64, num_threads, worker_extra);
    struct test_time_extra tte;
    tte.env = thread_args[0].env;
    tte.num_seconds = num_seconds;
    tte.crash_at_end = crash_at_end;
    tte.wes = worker_extra;
    tte.num_wes = num_threads;
    tte.cli_args = cli_args;
    run_test = true;
    for (int i = 0; i < num_threads; ++i) {
        thread_args[i].thread_idx = i;
        thread_args[i].num_threads = num_threads;
        worker_extra[i].thread_arg = &thread_args[i];
        worker_extra[i].operation_lock = &rwlock;
        worker_extra[i].operation_lock_mutex = &mutex;
        XCALLOC_N((int) NUM_OPERATION_TYPES, worker_extra[i].counters);
        TOKU_DRD_IGNORE_VAR(worker_extra[i].counters);
        { int chk_r = toku_pthread_create(&tids[i], nullptr, worker, &worker_extra[i]); CKERR(chk_r); }
        if (verbose)
            printf("%lu created\n", (unsigned long) tids[i]);
    }
    { int chk_r = toku_pthread_create(&time_tid, nullptr, test_time, &tte); CKERR(chk_r); }
    if (verbose)
        printf("%lu created\n", (unsigned long) time_tid);

    void *ret;
    r = toku_pthread_join(time_tid, &ret); assert_zero(r);
    if (verbose) printf("%lu joined\n", (unsigned long) time_tid);

    {
        // Set an alarm that will kill us if it takes too long to join all the
        // threads (i.e. there is some runaway thread).
        struct sleep_and_crash_extra sac_extra;
        ZERO_STRUCT(sac_extra);
        toku_mutex_init(&sac_extra.mutex, nullptr);
        toku_cond_init(&sac_extra.cond, nullptr);
        sac_extra.seconds = cli_args->join_timeout;
        sac_extra.is_setup = false;
        sac_extra.threads_have_joined = false;

        toku_mutex_lock(&sac_extra.mutex);
        toku_pthread_t sac_thread;
        r = toku_pthread_create(&sac_thread, nullptr, sleep_and_crash, &sac_extra);
        assert_zero(r);
        // Wait for sleep_and_crash thread to get set up, spinning is ok, this should be quick.
        while (!sac_extra.is_setup) {
            toku_mutex_unlock(&sac_extra.mutex);
            r = toku_pthread_yield();
            assert_zero(r);
            toku_mutex_lock(&sac_extra.mutex);
        }
        toku_mutex_unlock(&sac_extra.mutex);

        // Timeout thread has started, join everyone
        for (int i = 0; i < num_threads; ++i) {
            r = toku_pthread_join(tids[i], &ret); assert_zero(r);
            if (verbose)
                printf("%lu joined\n", (unsigned long) tids[i]);
        }

        // Signal timeout thread not to crash.
        toku_mutex_lock(&sac_extra.mutex);
        sac_extra.threads_have_joined = true;
        toku_cond_signal(&sac_extra.cond);
        toku_mutex_unlock(&sac_extra.mutex);
        r = toku_pthread_join(sac_thread, nullptr);
        assert_zero(r);
        toku_cond_destroy(&sac_extra.cond);
        toku_mutex_destroy(&sac_extra.mutex);
    }

    if (cli_args->print_performance) {
        uint64_t *counters[num_threads];
        for (int i = 0; i < num_threads; ++i) {
            counters[i] = worker_extra[i].counters;
        }
        perf_formatter->totals(cli_args, counters, num_threads);
    }

    for (int i = 0; i < num_threads; ++i) {
        toku_free(worker_extra[i].counters);
    }
    if (verbose)
        printf("ending test, pthreads have joined\n");
    rwlock_destroy(&rwlock);
    toku_mutex_destroy(&mutex);
    toku_free(worker_extra);
    return r;
}

// Pre-open hook
static void do_nothing_before_db_open(DB *UU(db), int UU(idx)) { }
// Requires: DB is created (allocated) but not opened. idx is the index
//           into the DBs array.
static void (*before_db_open_hook)(DB *db, int idx) = do_nothing_before_db_open;

// Post-open hook
typedef void (*reopen_db_fn)(DB *db, int idx, struct cli_args *cli_args);
static DB *do_nothing_after_db_open(DB_ENV *UU(env), DB *db, int UU(idx), reopen_db_fn UU(reopen), struct cli_args *UU(cli_args)) { return db; }
// Requires: DB is opened and is the 'idx' db in the DBs array.
// Note: Reopen function may be used to open a db if the given one was closed.
// Returns: An opened db.
static DB *(*after_db_open_hook)(DB_ENV *env, DB *db, int idx, reopen_db_fn reopen, struct cli_args *cli_args) = do_nothing_after_db_open;

static void open_db_for_create(DB *db, int idx, struct cli_args *cli_args) {
    int r;
    char name[30];
    memset(name, 0, sizeof(name));
    get_ith_table_name(name, sizeof(name), idx);
    r = db->set_flags(db, 0); CKERR(r);
    r = db->set_pagesize(db, cli_args->env_args.node_size); CKERR(r);
    r = db->set_readpagesize(db, cli_args->env_args.basement_node_size); CKERR(r);
    r = db->set_compression_method(db, cli_args->compression_method); CKERR(r);
    const int flags = DB_CREATE | (cli_args->blackhole ? DB_BLACKHOLE : 0);
    r = db->open(db, null_txn, name, nullptr, DB_BTREE, flags, 0666); CKERR(r);
}

static void open_db(DB *db, int idx, struct cli_args *cli_args) {
    int r;
    char name[30];
    memset(name, 0, sizeof(name));
    get_ith_table_name(name, sizeof(name), idx);
    const int flags = DB_CREATE | (cli_args->blackhole ? DB_BLACKHOLE : 0);
    r = db->open(db, null_txn, name, nullptr, DB_BTREE, flags, 0666); CKERR(r);
}

static int create_tables(DB_ENV **env_res, DB **db_res, int num_DBs,
                        int (*bt_compare)(DB *, const DBT *, const DBT *),
                        struct cli_args *cli_args
) {
    int r;
    struct env_args env_args = cli_args->env_args;

    char rmcmd[32 + strlen(env_args.envdir)]; sprintf(rmcmd, "rm -rf %s", env_args.envdir);
    r = system(rmcmd);
    CKERR(r);
    r = toku_os_mkdir(env_args.envdir, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    DB_ENV *env;
    db_env_set_num_bucket_mutexes(env_args.num_bucket_mutexes);
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, bt_compare); CKERR(r);
    r = env->set_lk_max_memory(env, env_args.lk_max_memory); CKERR(r);
    r = env->set_cachesize(env, env_args.cachetable_size / (1 << 30), env_args.cachetable_size % (1 << 30), 1); CKERR(r);
    r = env->set_lg_bsize(env, env_args.rollback_node_size); CKERR(r);
    if (env_args.generate_put_callback) {
        r = env->set_generate_row_callback_for_put(env, env_args.generate_put_callback); 
        CKERR(r);
    }
    else {
        r = env->set_generate_row_callback_for_put(env, generate_row_for_put); 
        CKERR(r);
    }
    if (env_args.generate_del_callback) {
        r = env->set_generate_row_callback_for_del(env, env_args.generate_del_callback); 
        CKERR(r);
    }
    int env_flags = get_env_open_flags(cli_args);
    r = env->open(env, env_args.envdir, env_flags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, env_args.checkpointing_period); CKERR(r);
    r = env->cleaner_set_period(env, env_args.cleaner_period); CKERR(r);
    r = env->cleaner_set_iterations(env, env_args.cleaner_iterations); CKERR(r);
    env->change_fsync_log_period(env, env_args.sync_period);
    *env_res = env;

    for (int i = 0; i < num_DBs; i++) {
        DB *db;
        r = db_create(&db, env, 0); CKERR(r);
        before_db_open_hook(db, i);
        open_db_for_create(db, i, cli_args);
        db_res[i] = after_db_open_hook(env, db, i, open_db_for_create, cli_args);
    }
    return r;
}

static void report_overall_fill_table_progress(struct cli_args *args, int num_rows) {
    // for sanitary reasons we'd like to prevent two threads
    // from printing the same performance report twice.
    static bool reporting;

    // when was the first time measurement taken?
    static uint64_t t0;
    static int rows_inserted;

    // when was the last report? what was its progress?
    static uint64_t last_report;
    static double last_progress;
    if (t0 == 0) {
        t0 = toku_current_time_microsec();
        last_report = t0;
    }

    uint64_t rows_so_far = toku_sync_add_and_fetch(&rows_inserted, num_rows);
    double progress = rows_so_far / (args->num_elements * args->num_DBs * 1.0);
    if (progress > (last_progress + .01)) {
        uint64_t t1 = toku_current_time_microsec();
        const uint64_t minimum_report_period = 5 * 1000000;
        if (t1 > last_report + minimum_report_period
                && toku_sync_bool_compare_and_swap(&reporting, 0, 1) == 0) {
            double inserts_per_sec = (rows_so_far*1000000) / ((t1 - t0) * 1.0);
            printf("fill tables: %ld%% complete, %.2lf rows/sec\n",
                    (long)(progress * 100), inserts_per_sec);
            last_progress = progress;
            last_report = t1;
            reporting = false;
        }
    }
}

static void fill_single_table(DB_ENV *env, DB *db, struct cli_args *args, bool fill_with_zeroes) {
    const int min_size_for_loader = 1 * 1000 * 1000;
    const int puts_per_txn = 10 * 1000;;

    int r = 0;
    DB_TXN *txn = nullptr;
    DB_LOADER *loader = nullptr;
    struct random_data random_data;
    char random_buf[8];
    memset(&random_data, 0, sizeof(random_data));
    memset(random_buf, 0, 8);
    r = myinitstate_r(random(), random_buf, 8, &random_data); CKERR(r);

    uint8_t keybuf[args->key_size], valbuf[args->val_size];
    memset(keybuf, 0, sizeof keybuf);
    memset(valbuf, 0, sizeof valbuf);
    DBT key, val;
    dbt_init(&key, keybuf, args->key_size);
    dbt_init(&val, valbuf, args->val_size);

    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    if (args->num_elements >= min_size_for_loader) {
        uint32_t db_flags = DB_PRELOCKED_WRITE;
        uint32_t dbt_flags = 0;
        r = env->create_loader(env, txn, &loader, db, 1, &db, &db_flags, &dbt_flags, 0); CKERR(r);
    }

    for (int i = 0; i < args->num_elements; i++) {
        fill_key_buf(i, keybuf, args);

        // Correctness tests map every key to zeroes. Perf tests fill
        // values with random bytes, based on compressibility.
        if (fill_with_zeroes) {
            fill_val_buf(0, valbuf, args->val_size);
        } else {
            fill_val_buf_random(&random_data, valbuf, args);
        }

        r = loader ? loader->put(loader, &key, &val) :
                     db->put(db, txn, &key, &val, DB_PRELOCKED_WRITE);
        CKERR(r);

        if (i > 0 && i % puts_per_txn == 0) {
            if (verbose) {
                report_overall_fill_table_progress(args, puts_per_txn);
            }
            // begin a new txn if we're not using the loader,
            if (loader == nullptr) {
                r = txn->commit(txn, 0); CKERR(r);
                r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
            }
        }
    }

    if (loader) {
        r = loader->close(loader); CKERR(r);
    }
    r = txn->commit(txn, 0); CKERR(r);
}

struct fill_table_worker_info {
    struct cli_args *args;
    DB_ENV *env;
    DB *db;
    bool fill_with_zeroes;
};

static void fill_table_worker(void *arg) {
    struct fill_table_worker_info *CAST_FROM_VOIDP(info, arg);
    fill_single_table(info->env, info->db, info->args, info->fill_with_zeroes);
    toku_free(info);
}

static int fill_tables_default(DB_ENV *env, DB **dbs, struct cli_args *args, bool fill_with_zeroes) {
    const int num_cores = toku_os_get_number_processors();
    // Use at most cores / 2 worker threads, since we want some other cores to
    // be used for internal engine work (ie: flushes, loader threads, etc).
    const int max_num_workers = (num_cores + 1) / 2;
    const int num_workers = args->num_DBs < max_num_workers ? args->num_DBs : max_num_workers;
    KIBBUTZ kibbutz = toku_kibbutz_create(num_workers);
    for (int i = 0; i < args->num_DBs; i++) {
        struct fill_table_worker_info *XCALLOC(info);
        info->env = env;
        info->db = dbs[i];
        info->args = args;
        info->fill_with_zeroes = fill_with_zeroes;
        toku_kibbutz_enq(kibbutz, fill_table_worker, info);
    }
    toku_kibbutz_destroy(kibbutz);
    return 0;
}

// fill_tables() is called when the tables are first created.
// set this function if you want custom table contents.
static int (*fill_tables)(DB_ENV *env, DB **dbs, struct cli_args *args, bool fill_with_zeroes) = fill_tables_default;

static void do_xa_recovery(DB_ENV* env) {
    DB_PREPLIST preplist[1];
    long num_recovered= 0;
    int r = 0;
    r = env->txn_recover(env, preplist, 1, &num_recovered, DB_NEXT);
    while(r==0 && num_recovered > 0) {
        DB_TXN* recovered_txn = preplist[0].txn;
        if (verbose) {
            printf("recovering transaction with id %" PRIu64 " \n", recovered_txn->id64(recovered_txn));
        }
        if (random() % 2 == 0) {
            int rr = recovered_txn->commit(recovered_txn, 0);
            CKERR(rr);
        }
        else {
            int rr = recovered_txn->abort(recovered_txn);
            CKERR(rr);
        }
        r = env->txn_recover(env, preplist, 1, &num_recovered, DB_NEXT);
    }
}

static int open_tables(DB_ENV **env_res, DB **db_res, int num_DBs,
                      int (*bt_compare)(DB *, const DBT *, const DBT *),
                      struct cli_args *cli_args) {
    int r;
    struct env_args env_args = cli_args->env_args;

    DB_ENV *env;
    db_env_set_num_bucket_mutexes(env_args.num_bucket_mutexes);
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, bt_compare); CKERR(r);
    r = env->set_lk_max_memory(env, env_args.lk_max_memory); CKERR(r);
    env->set_update(env, env_args.update_function);
    r = env->set_cachesize(env, env_args.cachetable_size / (1 << 30), env_args.cachetable_size % (1 << 30), 1); CKERR(r);
    r = env->set_lg_bsize(env, env_args.rollback_node_size); CKERR(r);
    if (env_args.generate_put_callback) {
        r = env->set_generate_row_callback_for_put(env, env_args.generate_put_callback);
        CKERR(r);
    }
    else {
        r = env->set_generate_row_callback_for_put(env, generate_row_for_put);
        CKERR(r);
    }
    if (env_args.generate_del_callback) {
        r = env->set_generate_row_callback_for_del(env, env_args.generate_del_callback);
        CKERR(r);
    }
    int env_flags = get_env_open_flags(cli_args);
    r = env->open(env, env_args.envdir, DB_RECOVER | env_flags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    do_xa_recovery(env);
    r = env->checkpointing_set_period(env, env_args.checkpointing_period); CKERR(r);
    r = env->cleaner_set_period(env, env_args.cleaner_period); CKERR(r);
    r = env->cleaner_set_iterations(env, env_args.cleaner_iterations); CKERR(r);
    env->change_fsync_log_period(env, env_args.sync_period);
    *env_res = env;

    for (int i = 0; i < num_DBs; i++) {
        DB *db;
        r = db_create(&db, env, 0); CKERR(r);
        before_db_open_hook(db, i);
        open_db(db, i, cli_args);
        db_res[i] = after_db_open_hook(env, db, i, open_db, cli_args);
    }
    return r;
}

static int close_tables(DB_ENV *env, DB**  dbs, int num_DBs) {
    int r;
    for (int i = 0; i < num_DBs; i++) {
        r = dbs[i]->close(dbs[i], 0); CKERR(r);
    }
    r = env->close(env, 0); CKERR(r);
    return r;
}

static const struct env_args DEFAULT_ENV_ARGS = {
    .node_size = 4096,
    .basement_node_size = 1024,
    .rollback_node_size = 4096,
    .checkpointing_period = 10,
    .cleaner_period = 1,
    .cleaner_iterations = 1,
    .sync_period = 0,
    .lk_max_memory = 1L * 1024 * 1024 * 1024,
    .cachetable_size = 300000,
    .num_bucket_mutexes = 1024,
    .envdir = nullptr,
    .update_function = update_op_callback,
    .generate_put_callback = nullptr,
    .generate_del_callback = nullptr,
};

static const struct env_args DEFAULT_PERF_ENV_ARGS = {
    .node_size = 4*1024*1024,
    .basement_node_size = 128*1024,
    .rollback_node_size = 4*1024*1024,
    .checkpointing_period = 60,
    .cleaner_period = 1,
    .cleaner_iterations = 5,
    .sync_period = 0,
    .lk_max_memory = 1L * 1024 * 1024 * 1024,
    .cachetable_size = 1<<30,
    .num_bucket_mutexes = 1024 * 1024,
    .envdir = nullptr,
    .update_function = nullptr,
    .generate_put_callback = nullptr,
    .generate_del_callback = nullptr,
};

static struct cli_args UU() get_default_args(void) {
    struct cli_args DEFAULT_ARGS = {
        .num_elements = 150000,
        .num_DBs = 1,
        .num_seconds = 180,
        .join_timeout = 3600,
        .only_create = false,
        .only_stress = false,
        .update_broadcast_period_ms = 2000,
        .num_ptquery_threads = 1,
        .do_test_and_crash = false,
        .do_recover = false,
        .num_update_threads = 1,
        .num_put_threads = 1,
        .range_query_limit = 100,
        .serial_insert = false,
        .interleave = false,
        .crash_on_operation_failure = true,
        .print_performance = false,
        .print_thread_performance = true,
        .print_iteration_performance = true,
        .perf_output_format = HUMAN,
        .compression_method = TOKU_DEFAULT_COMPRESSION_METHOD,
        .performance_period = 1,
        .txn_size = 1000,
        .key_size = min_key_size,
        .val_size = min_val_size,
        .compressibility = 1.0,
        .env_args = DEFAULT_ENV_ARGS,
        .single_txn = false,
        .warm_cache = false,
        .blackhole = false,
        .nolocktree = false,
        .unique_checks = false,
        .sync_period = 0,
        .nolog = false,
        .nocrashstatus = false,
        .prelock_updates = false,
        .disperse_keys = false,
        .direct_io = false,
        };
    DEFAULT_ARGS.env_args.envdir = TOKU_TEST_FILENAME;
    return DEFAULT_ARGS;
}

static struct cli_args UU() get_default_args_for_perf(void) {
    struct cli_args args = get_default_args();
    args.num_elements = 1000000; //default of 1M
    args.env_args = DEFAULT_PERF_ENV_ARGS;
    args.env_args.envdir = TOKU_TEST_FILENAME;
    return args;
}

union val_type {
    int32_t     i32;
    int64_t     i64;
    uint32_t    u32;
    uint64_t    u64;
    bool        b;
    double      d;
    const char *s;
};

struct arg_type;

typedef bool (*match_fun)(struct arg_type *type, char *const argv[]);
typedef int  (*parse_fun)(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]);
typedef void  (*help_fun)(struct arg_type *type, int width_name, int width_type);

struct type_description {
    const char           *type_name;
    const match_fun       matches;
    const parse_fun       parse;
    const help_fun        help;
};

struct arg_type {
    const char              *name;
    struct type_description *description;
    union val_type           default_val;
    void                    *target;
    const char              *help_suffix;
    union val_type           min;
    union val_type           max;
};

#define DEFINE_NUMERIC_HELP(typename, format, member, MIN, MAX) \
static inline void \
help_##typename(struct arg_type *type, int width_name, int width_type) { \
    invariant(!strncmp("--", type->name, strlen("--"))); \
    fprintf(stderr, "\t%-*s  %-*s  ", width_name, type->name, width_type, type->description->type_name); \
    fprintf(stderr, "(default %" format "%s", type->default_val.member, type->help_suffix); \
    if (type->min.member != MIN) { \
        fprintf(stderr, ", min %" format "%s", type->min.member, type->help_suffix); \
    } \
    if (type->max.member != MAX) { \
        fprintf(stderr, ", max %" format "%s", type->max.member, type->help_suffix); \
    } \
    fprintf(stderr, ")\n"); \
}

DEFINE_NUMERIC_HELP(int32, PRId32, i32, INT32_MIN, INT32_MAX)
DEFINE_NUMERIC_HELP(int64, PRId64, i64, INT64_MIN, INT64_MAX)
DEFINE_NUMERIC_HELP(uint32, PRIu32, u32, 0, UINT32_MAX)
DEFINE_NUMERIC_HELP(uint64, PRIu64, u64, 0, UINT64_MAX)
DEFINE_NUMERIC_HELP(double, ".2lf",  d, -HUGE_VAL, HUGE_VAL)
static inline void
help_bool(struct arg_type *type, int width_name, int width_type) {
    invariant(strncmp("--", type->name, strlen("--")));
    const char *default_value = type->default_val.b ? "yes" : "no";
    fprintf(stderr, "\t--[no-]%-*s  %-*s  (default %s)\n",
            width_name - (int)strlen("--[no-]"), type->name,
            width_type, type->description->type_name,
            default_value);
}

static inline void
help_string(struct arg_type *type, int width_name, int width_type) {
    invariant(!strncmp("--", type->name, strlen("--")));
    const char *default_value = type->default_val.s ? type->default_val.s : "";
    fprintf(stderr, "\t%-*s  %-*s  (default '%s')\n",
            width_name, type->name,
            width_type, type->description->type_name,
            default_value);
}

static inline bool
match_name(struct arg_type *type, char *const argv[]) {
    invariant(!strncmp("--", type->name, strlen("--")));
    return !strcmp(argv[1], type->name);
}

static inline bool
match_bool(struct arg_type *type, char *const argv[]) {
    invariant(strncmp("--", type->name, strlen("--")));
    const char *string = argv[1];
    if (strncmp(string, "--", strlen("--"))) {
        return false;
    }
    string += strlen("--");
    if (!strncmp(string, "no-", strlen("no-"))) {
        string += strlen("no-");
    }
    return !strcmp(string, type->name);
}

static inline int
parse_bool(struct arg_type *type, int *extra_args_consumed, int UU(argc), char *const argv[]) {
    const char *string = argv[1];
    if (!strncmp(string, "--no-", strlen("--no-"))) {
        *((bool *)type->target) = false;
    }
    else {
        *((bool *)type->target) = true;
    }
    *extra_args_consumed = 0;
    return 0;
}

static inline int
parse_string(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]) {
    if (argc < 2) {
        return EINVAL;
    }
    *((const char **)type->target) = argv[2];
    *extra_args_consumed = 1;
    return 0;
}

static inline int
parse_uint64(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]) {
    // Already verified name.

    if (argc < 2) {
        return EINVAL;
    }
    if (*argv[2] == '\0') {
       return EINVAL;
    }

    char *endptr;
    unsigned long long int result = strtoull(argv[2], &endptr, 0);
    if (*endptr != '\0') {
        return EINVAL;
    }
    if (result < type->min.u64 || result > type->max.u64) {
        return ERANGE;
    }
    *((uint64_t*)type->target) = result;
    *extra_args_consumed = 1;
    return 0;
}

static inline int
parse_int64(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]) {
    // Already verified name.

    if (argc < 2) {
        return EINVAL;
    }
    if (*argv[2] == '\0') {
       return EINVAL;
    }

    char *endptr;
    long long int result = strtoll(argv[2], &endptr, 0);
    if (*endptr != '\0') {
        return EINVAL;
    }
    if (result < type->min.i64 || result > type->max.i64) {
        return ERANGE;
    }
    *((int64_t*)type->target) = result;
    *extra_args_consumed = 1;
    return 0;
}

static inline int
parse_uint32(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]) {
    // Already verified name.

    if (argc < 2) {
        return EINVAL;
    }
    if (*argv[2] == '\0') {
       return EINVAL;
    }

    char *endptr;
    unsigned long int result = strtoul(argv[2], &endptr, 0);
    if (*endptr != '\0') {
        return EINVAL;
    }
    if (result < type->min.u32 || result > type->max.u32) {
        return ERANGE;
    }
    *((int32_t*)type->target) = result;
    *extra_args_consumed = 1;
    return 0;
}

static inline int
parse_int32(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]) {
    // Already verified name.

    if (argc < 2) {
        return EINVAL;
    }
    if (*argv[2] == '\0') {
       return EINVAL;
    }

    char *endptr;
    long int result = strtol(argv[2], &endptr, 0);
    if (*endptr != '\0') {
        return EINVAL;
    }
    if (result < type->min.i32 || result > type->max.i32) {
        return ERANGE;
    }
    *((int32_t*)type->target) = result;
    *extra_args_consumed = 1;
    return 0;
}

static inline int
parse_double(struct arg_type *type, int *extra_args_consumed, int argc, char *const argv[]) {
    // Already verified name.

    if (argc < 2) {
        return EINVAL;
    }
    if (*argv[2] == '\0') {
       return EINVAL;
    }

    char *endptr;
    double result = strtod(argv[2], &endptr);
    if (*endptr != '\0') {
        return EINVAL;
    }
    if (result < type->min.d || result > type->max.d) {
        return ERANGE;
    }
    *((double*)type->target) = result;
    *extra_args_consumed = 1;
    return 0;
}

// Common case (match_name).
#define DECLARE_TYPE_DESCRIPTION(typename) \
    struct type_description type_##typename = { \
        .type_name = #typename, \
        .matches = match_name, \
        .parse = parse_##typename, \
        .help = help_##typename \
    }
DECLARE_TYPE_DESCRIPTION(int32);
DECLARE_TYPE_DESCRIPTION(uint32);
DECLARE_TYPE_DESCRIPTION(int64);
DECLARE_TYPE_DESCRIPTION(uint64);
DECLARE_TYPE_DESCRIPTION(double);
DECLARE_TYPE_DESCRIPTION(string);

// Bools use their own match function so they are declared manually.
struct type_description type_bool = {
    .type_name = "bool",
    .matches = match_bool,
    .parse = parse_bool,
    .help = help_bool
};

#define ARG_MATCHES(type, rest...) type->description->matches(type, rest)
#define ARG_PARSE(type, rest...) type->description->parse(type, rest)
#define ARG_HELP(type, rest...) type->description->help(type, rest)

static inline void
do_usage(const char *argv0, int n, struct arg_type types[/*n*/]) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t%s [-h|--help]\n", argv0);
    fprintf(stderr, "\t%s [OPTIONS]\n", argv0);
    fprintf(stderr, "\n");
    fprintf(stderr, "OPTIONS are among:\n");
    fprintf(stderr, "\t-q|--quiet\n");
    fprintf(stderr, "\t-v|--verbose\n");
    for (int i = 0; i < n; i++) {
        struct arg_type *type = &types[i];
        ARG_HELP(type, 35, 6);
    }
}

static inline void parse_stress_test_args (int argc, char *const argv[], struct cli_args *args) {
    struct cli_args default_args = *args;
    const char *argv0=argv[0];

#define MAKE_ARG(name_string, type, member, variable, suffix, min_val, max_val) { \
    .name=(name_string), \
    .description=&(type), \
    .default_val={.member=default_args.variable}, \
    .target=&(args->variable), \
    .help_suffix=(suffix), \
    .min={.member=min_val}, \
    .max={.member=max_val}, \
}
#define MAKE_LOCAL_ARG(name_string, type, member, default, variable, suffix, min_val, max_val) { \
    .name=(name_string), \
    .description=&(type), \
    .default_val={.member=default}, \
    .target=&(variable), \
    .help_suffix=(suffix), \
    .min={.member=min_val}, \
    .max={.member=max_val}, \
}
#define UINT32_ARG(name_string, variable, suffix) \
        MAKE_ARG(name_string, type_uint32, u32, variable, suffix, 0, UINT32_MAX)
#define UINT32_ARG_R(name_string, variable, suffix, min, max) \
        MAKE_ARG(name_string, type_uint32, u32, variable, suffix, min, max)
#define UINT64_ARG(name_string, variable, suffix) \
        MAKE_ARG(name_string, type_uint64, u64, variable, suffix, 0, UINT64_MAX)
#define INT32_ARG_NONNEG(name_string, variable, suffix) \
        MAKE_ARG(name_string, type_int32, i32, variable, suffix, 0, INT32_MAX)
#define INT32_ARG_R(name_string, variable, suffix, min, max) \
        MAKE_ARG(name_string, type_int32, i32, variable, suffix, min, max)
#define DOUBLE_ARG_R(name_string, variable, suffix, min, max) \
        MAKE_ARG(name_string, type_double, d, variable, suffix, min, max)
#define BOOL_ARG(name_string, variable) \
        MAKE_ARG(name_string, type_bool, b, variable, "", false, false)
#define STRING_ARG(name_string, variable) \
        MAKE_ARG(name_string, type_string, s, variable, "", "", "")
#define LOCAL_STRING_ARG(name_string, variable, default) \
        MAKE_LOCAL_ARG(name_string, type_string, s, default, variable, "", "", "")

    const char *perf_format_s = nullptr;
    const char *compression_method_s = nullptr;
    const char *print_engine_status_s = nullptr;
    struct arg_type arg_types[] = {
        INT32_ARG_NONNEG("--num_elements",            num_elements,                  ""),
        INT32_ARG_NONNEG("--num_DBs",                 num_DBs,                       ""),
        INT32_ARG_NONNEG("--num_seconds",             num_seconds,                   "s"),
        INT32_ARG_NONNEG("--node_size",               env_args.node_size,            " bytes"),
        INT32_ARG_NONNEG("--basement_node_size",      env_args.basement_node_size,   " bytes"),
        INT32_ARG_NONNEG("--rollback_node_size",      env_args.rollback_node_size,   " bytes"),
        INT32_ARG_NONNEG("--checkpointing_period",    env_args.checkpointing_period, "s"),
        INT32_ARG_NONNEG("--cleaner_period",          env_args.cleaner_period,       "s"),
        INT32_ARG_NONNEG("--cleaner_iterations",      env_args.cleaner_iterations,   ""),
        INT32_ARG_NONNEG("--sync_period",             env_args.sync_period,          "ms"),
        INT32_ARG_NONNEG("--update_broadcast_period", update_broadcast_period_ms,    "ms"),
        INT32_ARG_NONNEG("--num_ptquery_threads",     num_ptquery_threads,           " threads"),
        INT32_ARG_NONNEG("--num_put_threads",         num_put_threads,               " threads"),
        INT32_ARG_NONNEG("--num_update_threads",      num_update_threads,            " threads"),
        INT32_ARG_NONNEG("--range_query_limit",       range_query_limit,             " rows"),

        UINT32_ARG("--txn_size",                      txn_size,                      " rows"),
        UINT32_ARG("--num_bucket_mutexes",            env_args.num_bucket_mutexes,   " mutexes"),

        INT32_ARG_R("--join_timeout",                 join_timeout,                  "s", 1, INT32_MAX),
        INT32_ARG_R("--performance_period",           performance_period,            "s", 1, INT32_MAX),

        UINT64_ARG("--cachetable_size",               env_args.cachetable_size,      " bytes"),
        UINT64_ARG("--lk_max_memory",                 env_args.lk_max_memory,        " bytes"),

        DOUBLE_ARG_R("--compressibility",             compressibility,               "", 0.0, 1.0),

        //TODO: when outputting help.. skip min/max that is min/max of data range.
        UINT32_ARG_R("--key_size",                    key_size,                      " bytes", min_key_size, UINT32_MAX),
        UINT32_ARG_R("--val_size",                    val_size,                      " bytes", min_val_size, UINT32_MAX),

        BOOL_ARG("serial_insert",                     serial_insert),
        BOOL_ARG("interleave",                        interleave),
        BOOL_ARG("crash_on_operation_failure",        crash_on_operation_failure),
        BOOL_ARG("single_txn",                        single_txn),
        BOOL_ARG("warm_cache",                        warm_cache),
        BOOL_ARG("print_performance",                 print_performance),
        BOOL_ARG("print_thread_performance",          print_thread_performance),
        BOOL_ARG("print_iteration_performance",       print_iteration_performance),
        BOOL_ARG("only_create",                       only_create),
        BOOL_ARG("only_stress",                       only_stress),
        BOOL_ARG("test",                              do_test_and_crash),
        BOOL_ARG("recover",                           do_recover),
        BOOL_ARG("blackhole",                         blackhole),
        BOOL_ARG("nolocktree",                        nolocktree),
        BOOL_ARG("unique_checks",                     unique_checks),
        BOOL_ARG("nolog",                             nolog),
        BOOL_ARG("nocrashstatus",                     nocrashstatus),
        BOOL_ARG("prelock_updates",                   prelock_updates),
        BOOL_ARG("disperse_keys",                     disperse_keys),
        BOOL_ARG("direct_io",                         direct_io),

        STRING_ARG("--envdir",                        env_args.envdir),

        LOCAL_STRING_ARG("--perf_format",             perf_format_s,                "human"),
        LOCAL_STRING_ARG("--compression_method",      compression_method_s,         "quicklz"),
        LOCAL_STRING_ARG("--print_engine_status",     print_engine_status_s,        nullptr),
        //TODO(add --quiet, -v, -h)
    };
#undef UINT32_ARG
#undef UINT32_ARG_R
#undef UINT64_ARG
#undef DOUBLE_ARG_R
#undef BOOL_ARG
#undef STRING_ARG
#undef MAKE_ARG

    int num_arg_types = sizeof(arg_types) / sizeof(arg_types[0]);

    int resultcode = 0;
    while (argc > 1) {
        if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--verbose")) {
            verbose++;
            argv++;
            argc--;
        }
        else if (!strcmp(argv[1], "-q") || !strcmp(argv[1], "--quiet")) {
            verbose = 0;
            argv++;
            argc--;
        }
        else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            fprintf(stderr, "HELP INVOKED\n");
            do_usage(argv0, num_arg_types, arg_types);
            exit(0);
        }
        else {
            bool found = false;
            for (int i = 0; i < num_arg_types; i++) {
                struct arg_type *type = &arg_types[i];
                if (ARG_MATCHES(type, argv)) {
                    int extra_args_consumed;
                    resultcode = ARG_PARSE(type, &extra_args_consumed, argc, argv);
                    if (resultcode) {
                        fprintf(stderr, "ERROR PARSING [%s]\n", argv[1]);
                        do_usage(argv0, num_arg_types, arg_types);
                        exit(resultcode);
                    }
                    found = true;
                    argv += extra_args_consumed + 1;
                    argc -= extra_args_consumed + 1;
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "COULD NOT PARSE [%s]\n", argv[1]);
                do_usage(argv0, num_arg_types, arg_types);
                exit(EINVAL);
            }
        }
    }
    args->print_engine_status = print_engine_status_s;
    if (compression_method_s != nullptr) {
        if (strcmp(compression_method_s, "quicklz") == 0) {
            args->compression_method = TOKU_QUICKLZ_METHOD;
        } else if (strcmp(compression_method_s, "zlib") == 0) {
            args->compression_method = TOKU_ZLIB_WITHOUT_CHECKSUM_METHOD;
        } else if (strcmp(compression_method_s, "lzma") == 0) {
            args->compression_method = TOKU_LZMA_METHOD;
        } else if (strcmp(compression_method_s, "none") == 0) {
            args->compression_method = TOKU_NO_COMPRESSION;
        } else {
            fprintf(stderr, "valid values for --compression_method are \"quicklz\", \"zlib\", \"lzma\" and \"none\"\n");
            do_usage(argv0, num_arg_types, arg_types);
            exit(EINVAL);
        }
    }
    if (perf_format_s != nullptr) {
        if (!strcmp(perf_format_s, "human")) {
            args->perf_output_format = HUMAN;
        } else if (!strcmp(perf_format_s, "csv")) {
            args->perf_output_format = CSV;
        } else if (!strcmp(perf_format_s, "tsv")) {
            args->perf_output_format = TSV;
        } else {
            fprintf(stderr, "valid values for --perf_format are \"human\", \"csv\", and \"tsv\"\n");
            do_usage(argv0, num_arg_types, arg_types);
            exit(EINVAL);
        }
    }
    if (args->only_create && args->only_stress) {
        fprintf(stderr, "used --only_stress and --only_create\n");
        do_usage(argv0, num_arg_types, arg_types);
        exit(EINVAL);
    }
}

static void
stress_table(DB_ENV *, DB **, struct cli_args *);

static int
stress_dbt_cmp_legacy(const DBT *a, const DBT *b) {
    int x = *(int *) a->data;
    int y = *(int *) b->data;
    if (x < y) {
        return -1;
    } else if (x > y) {
        return +1;
    } else {
        return 0;
    }
}

static int
stress_dbt_cmp(const DBT *a, const DBT *b) {
    // Keys are only compared by their first 8 bytes,
    // interpreted as a little endian 64 bit integers.
    // The rest of the key is just padding.
    uint64_t x = *(uint64_t *) a->data;
    uint64_t y = *(uint64_t *) b->data;
    if (x < y) {
        return -1;
    } else if (x > y) {
        return +1;
    } else {
        return 0;
    }
}

static int
stress_cmp(DB *db, const DBT *a, const DBT *b) {
    assert(db && a && b);
    assert(a->size == b->size);

    if (a->size == sizeof(int)) {
        // Legacy comparison: keys must be >= 4 bytes
        return stress_dbt_cmp_legacy(a, b);
    } else {
        // Modern comparison: keys must be >= 8 bytes
        invariant(a->size >= sizeof(uint64_t));
        return stress_dbt_cmp(a, b);
    }
}

static void
do_warm_cache(DB_ENV *env, DB **dbs, struct cli_args *args)
{
    struct scan_op_extra soe;
    soe.fast = true;
    soe.fwd = true;
    soe.prefetch = true;
    struct arg scan_arg;
    arg_init(&scan_arg, dbs, env, args);
    scan_arg.operation_extra = &soe;
    scan_arg.operation = scan_op_no_check;
    scan_arg.lock_type = STRESS_LOCK_NONE;
    DB_TXN* txn = nullptr;
    // don't take serializable read locks when scanning.
    int r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT); CKERR(r);
    // make sure the scan doesn't terminate early
    run_test = true;
    // warm up each DB in parallel
    scan_op_no_check_parallel(txn, &scan_arg, &soe, nullptr);
    r = txn->commit(txn,0); CKERR(r);
}

static void
UU() stress_recover(struct cli_args *args) {
    DB_ENV* env = nullptr;
    DB* dbs[args->num_DBs];
    memset(dbs, 0, sizeof(dbs));
    { int chk_r = open_tables(&env,
                              dbs,
                              args->num_DBs,
                              stress_cmp,
                              args); CKERR(chk_r); }

    DB_TXN* txn = nullptr;
    struct arg recover_args;
    arg_init(&recover_args, dbs, env, args);
    int r = env->txn_begin(env, 0, &txn, recover_args.txn_flags);
    CKERR(r);
    struct scan_op_extra soe = {
        .fast = true,
        .fwd = true,
        .prefetch = false
    };
    // make sure the scan doesn't terminate early
    run_test = true;
    r = scan_op(txn, &recover_args, &soe, nullptr);
    CKERR(r);
    { int chk_r = txn->commit(txn,0); CKERR(chk_r); }
    { int chk_r = close_tables(env, dbs, args->num_DBs); CKERR(chk_r); }
}

static void
open_and_stress_tables(struct cli_args *args, bool fill_with_zeroes, int (*cmp)(DB *, const DBT *, const DBT *))
{
    if ((args->key_size < 8 && args->key_size != 4) ||
        (args->val_size < 8 && args->val_size != 4)) {
        fprintf(stderr, "The only valid key/val sizes are 4, 8, and > 8.\n");
        return;
    }

    { char *loc = setlocale(LC_NUMERIC, "en_US.UTF-8"); assert(loc); }
    DB_ENV* env = nullptr;
    DB* dbs[args->num_DBs];
    memset(dbs, 0, sizeof(dbs));
    db_env_enable_engine_status(args->nocrashstatus ? false : true);
    db_env_set_direct_io(args->direct_io ? true : false);
    if (!args->only_stress) {
        create_tables(
            &env,
            dbs,
            args->num_DBs,
            cmp,
            args
            );
        { int chk_r = fill_tables(env, dbs, args, fill_with_zeroes); CKERR(chk_r); }
        { int chk_r = close_tables(env, dbs, args->num_DBs); CKERR(chk_r); }
    }
    if (!args->only_create) {
        { int chk_r = open_tables(&env,
                                  dbs,
                                  args->num_DBs,
                                  cmp,
                                  args); CKERR(chk_r); }
        if (args->warm_cache) {
            do_warm_cache(env, dbs, args);
        }
        stress_table(env, dbs, args);
        { int chk_r = close_tables(env, dbs, args->num_DBs); CKERR(chk_r); }
    }
}

static void
UU() stress_test_main(struct cli_args *args) {
    // Begin the test with fixed size values equal to zero.
    // This is important for correctness testing.
    open_and_stress_tables(args, true, stress_cmp);
}

static void
UU() perf_test_main(struct cli_args *args) {
    // Do not begin the test by creating a table of all zeroes.
    // We want to control the row size and its compressibility.
    open_and_stress_tables(args, false, stress_cmp);
}

static void
UU() perf_test_main_with_cmp(struct cli_args *args, int (*cmp)(DB *, const DBT *, const DBT *)) {
    // Do not begin the test by creating a table of all zeroes.
    // We want to control the row size and its compressibility.
    open_and_stress_tables(args, false, cmp);
}

#endif
