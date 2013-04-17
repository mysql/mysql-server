/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef _THREADED_STRESS_TEST_HELPERS_H_
#define _THREADED_STRESS_TEST_HELPERS_H_

#include <config.h>
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <sys/stat.h>
#include <db.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include <math.h>

#include <memory.h>
#include <toku_race_tools.h>
#include <portability/toku_atomic.h>
#include <portability/toku_pthread.h>
#include <portability/toku_random.h>
#include <portability/toku_time.h>

#include <util/rwlock.h>
#include <util/kibbutz.h>

#include <ft/ybt.h>

using namespace toku;

#if !defined(HAVE_MEMALIGN)
# if defined(HAVE_VALLOC)
static void *
memalign(size_t UU(alignment), size_t size)
{
    return valloc(size);
}
# else
#  error "no suitable aligned malloc available (checked memalign and valloc)"
# endif
#endif

volatile bool run_test; // should be volatile since we are communicating through this variable.

typedef struct arg *ARG;
typedef int (*operation_t)(DB_TXN *txn, ARG arg, void *operation_extra, void *stats_extra);

typedef int (*test_update_callback_f)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra);
typedef int (*test_generate_row_for_put_callback)(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data);
typedef int (*test_generate_row_for_del_callback)(DB *dest_db, DB *src_db, DBT *dest_key, const DBT *src_key, const DBT *src_data);

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
#if 0
    GNUPLOT,
#endif
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
    bool serial_insert;
    bool interleave; // for insert benchmarks, whether to interleave
                     // separate threads' puts (or segregate them)
    bool crash_on_operation_failure; 
    bool print_performance;
    bool print_thread_performance;
    bool print_iteration_performance;
    enum perf_output_format perf_output_format;
    int performance_period;
    uint32_t txn_size; // specifies number of updates/puts/whatevers per txn
    uint32_t key_size; // number of bytes in vals. Must be at least 4
    uint32_t val_size; // number of bytes in vals. Must be at least 4
    double compressibility; // how much of each key/val (as a fraction in [0,1]) can be compressed away
                            // First 4-8 bytes of key may be ignored
    struct env_args env_args; // specifies environment variables
    bool single_txn;
    bool warm_cache; // warm caches before running stress_table
    bool blackhole; // all message injects are no-ops. helps measure txn/logging/locktree overhead.
    bool nolocktree; // use this flag to avoid the locktree on insertions
    bool unique_checks; // use uniqueness checking during insert. makes it slow.
    bool nosync; // do not fsync on txn commit. useful for testing in memory performance.
    bool nolog; // do not log. useful for testing in memory performance.
    bool nocrashstatus; // do not print engine status upon crash
    bool prelock_updates; // update threads perform serial updates on a prelocked range
    bool disperse_keys; // spread the keys out during a load (by reversing the bits in the loop index) to make a wide tree we can spread out random inserts into
};

struct arg {
    DB **dbp; // array of DBs
    DB_ENV* env; // environment used
    bool bounded_element_range; // true if elements in dictionary are bounded
                                // by num_elements, that is, all keys in each
                                // DB are in [0, num_elements)
                                // false otherwise
    int sleep_ms; // number of milliseconds to sleep between operations
    uint32_t txn_type; // isolation level for txn running operation
    operation_t operation; // function that is the operation to be run
    void* operation_extra; // extra parameter passed to operation
    enum stress_lock_type lock_type; // states if operation must be exclusive, shared, or does not require locking
    struct random_data *random_data; // state for random_r
    int thread_idx;
    int num_threads;
    struct cli_args *cli;
    bool do_prepare;
    bool prelock_updates;
};

DB_TXN * const null_txn = 0;

static void arg_init(struct arg *arg, DB **dbp, DB_ENV *env, struct cli_args *cli_args) {
    arg->cli = cli_args;
    arg->dbp = dbp;
    arg->env = env;
    arg->bounded_element_range = true;
    arg->sleep_ms = 0;
    arg->lock_type = STRESS_LOCK_NONE;
    arg->txn_type = DB_TXN_SNAPSHOT;
    arg->operation_extra = NULL;
    arg->do_prepare = false;
    arg->prelock_updates = false;
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
    NULL
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

#if 0
static void
gnuplot_print_perf_header(const struct cli_args *cli_args, const int num_threads)
{
    printf("set terminal postscript solid color\n");
    printf("set output \"foo.eps\"\n");
    printf("set xlabel \"seconds\"\n");
    printf("set xrange [0:*]\n");
    printf("set ylabel \"X/s\"\n");
    printf("plot ");
    if (cli_args->print_thread_performance) {
        for (int t = 1; t <= num_threads; ++t) {
            for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
                const int col = (2 * ((t - 1) * (int) NUM_OPERATION_TYPES + op)) + 2;
                //printf("'-' u 1:%d w lines t \"Thread %d %s\", ", col, t, operation_names[op]);
                printf("'-' u 1:%d w lines t \"Thread %d %s/s\", ", col + 1, t, operation_names[op]);
            }
        }
    }
    for (int op = 0; op < (int) NUM_OPERATION_TYPES; ++op) {
        const int col = (2 * (num_threads * (int) NUM_OPERATION_TYPES + op)) + 2;
        //printf("'-' u 1:%d w lines t \"Total %s\", ", col);
        printf("'-' u 1:%d w lines t \"Total %s/s\"%s", col + 1, operation_names[op], op == ((int) NUM_OPERATION_TYPES - 1) ? "\n" : ", ");
    }
}

static void
gnuplot_print_perf_iteration(const struct cli_args *cli_args, const int current_time, uint64_t last_counters[][(int) NUM_OPERATION_TYPES], uint64_t *counters[], const int num_threads)
{
    tsv_print_perf_iteration(cli_args, current_time, last_counters, counters, num_threads);
}

static void
gnuplot_print_perf_totals(const struct cli_args *UU(cli_args), uint64_t *UU(counters[]), const int UU(num_threads))
{
    printf("e\n");
}
#endif

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
#if 0
    [GNUPLOT] = {
        .header = gnuplot_print_perf_header,
        .iteration = gnuplot_print_perf_iteration,
        .totals = gnuplot_print_perf_totals
    }
#endif
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
    flags |= args->nosync ? DB_TXN_NOSYNC : 0;
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
            assert(false);
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
            assert(false);
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
    DB_TXN *txn = NULL;
    if (verbose) {
        toku_pthread_t self = toku_pthread_self();
        uintptr_t intself = (uintptr_t) self;
        printf("%lu starting %p\n", (unsigned long) intself, arg->operation);
    }
    if (arg->cli->single_txn) {
        r = env->txn_begin(env, 0, &txn, arg->txn_type); CKERR(r);
    }
    while (run_test) {
        lock_worker_op(we);
        if (!arg->cli->single_txn) {
            r = env->txn_begin(env, 0, &txn, arg->txn_type); CKERR(r);
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
        we->counters[OPERATION]++;
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

typedef struct scan_cb_extra *SCAN_CB_EXTRA;
struct scan_cb_extra {
    bool fast;
    int64_t curr_sum;
    int64_t num_elements;
};

struct scan_op_extra {
    bool fast;
    bool fwd;
    bool prefetch;
};

static int
scan_cb(const DBT *a, const DBT *b, void *arg_v) {
    SCAN_CB_EXTRA CAST_FROM_VOIDP(cb_extra, arg_v);
    assert(a);
    assert(b);
    assert(cb_extra);
    assert(b->size >= sizeof(int));
    cb_extra->curr_sum += *(int *)b->data;
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
    DBC* cursor = NULL;

    struct scan_cb_extra e;
    e.fast = sce->fast;
    e.curr_sum = 0;
    e.num_elements = 0;

    { int chk_r = db->cursor(db, txn, &cursor, 0); CKERR(chk_r); }
    if (sce->prefetch) {
        r = cursor->c_pre_acquire_range_lock(cursor, db->dbt_neg_infty(), db->dbt_pos_infty());
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
        printf("e.curr_sum: %" PRId64 " e.num_elements: %" PRId64 " \n", e.curr_sum, e.num_elements);
        assert(false);
    }
    return r;
}

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db), 
    DBT *dest_key, 
    DBT *dest_val, 
    const DBT *src_key, 
    const DBT *src_val
    ) 
{    
    dest_key->data = src_key->data;
    dest_key->size = src_key->size;
    dest_key->flags = 0;
    dest_val->data = src_val->data;
    dest_val->size = src_val->size;
    dest_val->flags = 0;
    return 0;
}

static int UU() nop(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra), void *UU(stats_extra)) {
    return 0;
}

static int UU() xmalloc_free_op(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra), void *UU(stats_extra)) {
    size_t s = 256;
    void *p = toku_xmalloc(s);
    toku_free(p);
    return 0;
}

#if DONT_DEPRECATE_MALLOC
static int UU() malloc_free_op(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra), void *UU(stats_extra)) {
    size_t s = 256;
    void *p = malloc(s);
    free(p);
    return 0;
}
#endif

// Fill array with compressibility*size 0s.
// 0.0<=compressibility<=1.0
// Compressibility is the fraction of size that will be 0s (e.g. approximate fraction that will be compressed away).
// The rest will be random data.
static void
fill_zeroed_array(uint8_t *data, uint32_t size, struct random_data *random_data, double compressibility) {
    //Requires: The array was zeroed since the last time 'size' was changed.
    //Requires: compressibility is in range [0,1] indicating fraction that should be zeros.

    uint32_t num_random_bytes = (1 - compressibility) * size;

    if (num_random_bytes > 0) {
        uint32_t filled;
        for (filled = 0; filled + sizeof(uint64_t) <= num_random_bytes; filled += sizeof(uint64_t)) {
            *((uint64_t *) &data[filled]) = randu64(random_data);
        }
        if (filled != num_random_bytes) {
            uint64_t last8 = randu64(random_data);
            memcpy(&data[filled], &last8, num_random_bytes - filled);
        }
    }
}

static inline size_t
size_t_max(size_t a, size_t b) {
    return (a > b) ? a : b;
}

static int random_put_in_db(DB *db, DB_TXN *txn, ARG arg, bool ignore_errors, void *stats_extra) {
    int r = 0;
    uint8_t rand_key_b[size_t_max(arg->cli->key_size, sizeof(uint64_t))];
    uint64_t *rand_key_key = cast_to_typeof(rand_key_key) rand_key_b;
    uint16_t *rand_key_i = cast_to_typeof(rand_key_i) rand_key_b;
    ZERO_ARRAY(rand_key_b);
    uint8_t valbuf[arg->cli->val_size];
    ZERO_ARRAY(valbuf);

    uint64_t puts_to_increment = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; ++i) {
        rand_key_key[0] = randu64(arg->random_data);
        if (arg->cli->interleave) {
            rand_key_i[3] = arg->thread_idx;
        } else {
            rand_key_i[0] = arg->thread_idx;
        }
        fill_zeroed_array(valbuf, arg->cli->val_size, arg->random_data, arg->cli->compressibility);
        DBT key, val;
        dbt_init(&key, &rand_key_b, sizeof rand_key_b);
        dbt_init(&val, valbuf, sizeof valbuf);
        int flags = get_put_flags(arg->cli);
        r = db->put(db, txn, &key, &val, flags);
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

static int UU() random_put_multiple_op(DB_TXN *txn, ARG arg, void *UU(operation_extra), void *stats_extra) {
    const int num_dbs = arg->cli->num_DBs;
    DB **dbs = arg->dbp;
    DB_ENV *env = arg->env;
    DBT mult_key_dbt[num_dbs];
    DBT mult_put_dbt[num_dbs];
    uint32_t mult_put_flags[num_dbs];
    memset(mult_key_dbt, 0, sizeof(mult_key_dbt));
    memset(mult_put_dbt, 0, sizeof(mult_put_dbt));
    memset(mult_put_flags, 0, sizeof(mult_put_dbt));

    int r = 0;
    uint8_t rand_key_b[size_t_max(arg->cli->key_size, sizeof(uint64_t))];
    uint64_t *rand_key_key = cast_to_typeof(rand_key_key) rand_key_b;
    uint16_t *rand_key_i = cast_to_typeof(rand_key_i) rand_key_b;
    ZERO_ARRAY(rand_key_b);
    uint8_t valbuf[arg->cli->val_size];
    ZERO_ARRAY(valbuf);

    uint64_t puts_to_increment = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; ++i) {
        rand_key_key[0] = randu64(arg->random_data);
        if (arg->cli->interleave) {
            rand_key_i[3] = arg->thread_idx;
        } else {
            rand_key_i[0] = arg->thread_idx;
        }
        fill_zeroed_array(valbuf, arg->cli->val_size, arg->random_data, arg->cli->compressibility);
        DBT key, val;
        dbt_init(&key, &rand_key_b, sizeof rand_key_b);
        dbt_init(&val, valbuf, sizeof valbuf);
        r = env->put_multiple(
            env, 
            dbs[0], // source db. this is arbitrary.
            txn, 
            &key, // source db key
            &val, // source db value
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
    uint8_t rand_key_b[size_t_max(arg->cli->key_size, sizeof(uint64_t))];
    uint64_t *rand_key_key = cast_to_typeof(rand_key_key) rand_key_b;
    uint16_t *rand_key_i = cast_to_typeof(rand_key_i) rand_key_b;
    ZERO_ARRAY(rand_key_b);
    uint8_t valbuf[arg->cli->val_size];
    ZERO_ARRAY(valbuf);

    uint64_t puts_to_increment = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; ++i) {
        rand_key_key[0] = extra->current++;
        fill_zeroed_array(valbuf, arg->cli->val_size, arg->random_data, arg->cli->compressibility);
        DBT key, val;
        dbt_init(&key, &rand_key_b, sizeof rand_key_b);
        dbt_init(&val, valbuf, sizeof valbuf);
        int flags = get_put_flags(arg->cli);
        r = db->put(db, txn, &key, &val, flags);
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

static int UU() loader_op(DB_TXN* txn, ARG UU(arg), void* UU(operation_extra), void *UU(stats_extra)) {
    DB_ENV* env = arg->env;
    int r;
    for (int num = 0; num < 2; num++) {
        DB *db_load;
        uint32_t db_flags = 0;
        uint32_t dbt_flags = 0;
        r = db_create(&db_load, env, 0);
        assert(r == 0);
        r = db_load->open(db_load, txn, "loader-db", NULL, DB_BTREE, DB_CREATE, 0666);
        assert(r == 0);
        DB_LOADER *loader;
        uint32_t loader_flags = (num == 0) ? 0 : LOADER_COMPRESS_INTERMEDIATES;
        r = env->create_loader(env, txn, &loader, db_load, 1, &db_load, &db_flags, &dbt_flags, loader_flags);
        CKERR(r);

        for (int i = 0; i < 1000; i++) {
            DBT key, val;
            int rand_key = i;
            int rand_val = myrandom_r(arg->random_data);
            dbt_init(&key, &rand_key, sizeof(rand_key));
            dbt_init(&val, &rand_val, sizeof(rand_val));
            r = loader->put(loader, &key, &val); CKERR(r);
        }

        r = loader->close(loader); CKERR(r);
        r = db_load->close(db_load, 0); CKERR(r);
        r = env->dbremove(env, txn, "loader-db", NULL, 0); CKERR(r);
    }
    return 0;
}

static int UU() keyrange_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    int r;
    // callback is designed to run on tests with one DB
    // no particular reason why, just the way it was 
    // originally done
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    int rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->cli->num_elements;
    }
    DBT key;
    dbt_init(&key, &rand_key, sizeof rand_key);
    uint64_t less,equal,greater;
    int is_exact;
    r = db->key_range64(db, txn, &key, &less, &equal, &greater, &is_exact);
    assert(r == 0);
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

static int UU() scan_op(DB_TXN *txn, ARG UU(arg), void* operation_extra, void *UU(stats_extra)) {
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
    KIBBUTZ kibbutz = toku_kibbutz_create(num_cores);
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
    int r;
    int rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->cli->num_elements;
    }
    DBT key, val;
    dbt_init(&key, &rand_key, sizeof rand_key);
    dbt_init(&val, NULL, 0);
    r = db->getf_set(
        db, 
        txn, 
        0, 
        &key, 
        dbt_do_nothing, 
        NULL
        );
    if (check) assert(r != DB_NOTFOUND);
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

static int UU() cursor_create_close_op(DB_TXN *txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    int db_index = arg->cli->num_DBs > 1 ? myrandom_r(arg->random_data)%arg->cli->num_DBs : 0;
    DB* db = arg->dbp[db_index];
    DBC* cursor = NULL;
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
        old_int_val = *(int*)old_val->data;
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
        assert(false);
    }

    DBT new_val;
    uint32_t data_size = sizeof(int) + e->pad_bytes;
    char* data [data_size];
    ZERO_ARRAY(data);
    memcpy(data, &new_int_val, sizeof(new_int_val));
    set_val(dbt_init(&new_val, data, data_size), set_extra);
    return 0;
}

static int UU()update_op2(DB_TXN* txn, ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    int rand_key2;
    toku_sync_fetch_and_add(&update_count, 1);
    struct update_op_extra extra;
    ZERO_STRUCT(extra);
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        rand_key = myrandom_r(arg->random_data);
        if (arg->bounded_element_range) {
            rand_key = rand_key % (arg->cli->num_elements/2);
        }
        rand_key2 = arg->cli->num_elements - rand_key;
        assert(rand_key != rand_key2);
        extra.u.d.diff = 1;
        curr_val_sum += extra.u.d.diff;
        r = db->update(
            db,
            txn,
            dbt_init(&key, &rand_key, sizeof rand_key),
            dbt_init(&val, &extra, sizeof extra),
            0
            );
        if (r != 0) {
            return r;
        }
        extra.u.d.diff = -1;
        r = db->update(
            db,
            txn,
            dbt_init(&key, &rand_key2, sizeof rand_key),
            dbt_init(&val, &extra, sizeof extra),
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
    int cursor_r = cursor->c_pre_acquire_range_lock(cursor, left_key, right_key);
    r = cursor->c_close(cursor);
    CKERR(r);

    return cursor_r;
}

// take the given db and do an update on it
static int
UU() update_op_db(DB *db, DB_TXN *txn, ARG arg, void* operation_extra, void *UU(stats_extra)) {
    int r = 0;
    int curr_val_sum = 0;
    DBT key, val;
    int update_key;
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
    const int update_flags = arg->cli->prelock_updates ? DB_PRELOCKED_WRITE : 0;
    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        if (arg->prelock_updates) {
            if (i == 0) {
                update_key = myrandom_r(arg->random_data);
                if (arg->bounded_element_range) {
                    update_key = update_key % arg->cli->num_elements;
                }

                const uint32_t max_key_in_table = arg->cli->num_elements - 1;
                const bool range_wraps = (update_key + arg->cli->txn_size - 1) > max_key_in_table;
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
            }
        } else {
            // just do a usual, random point update without locking first
            update_key = myrandom_r(arg->random_data);
        }

        if (arg->bounded_element_range) {
            update_key = update_key % arg->cli->num_elements;
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

        // do the update
        r = db->update(
            db,
            txn,
            dbt_init(&key, &update_key, sizeof update_key),
            dbt_init(&val, &extra, sizeof extra),
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
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
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
    for (uint32_t i = 0; i < arg->cli->txn_size; i++) {
        rand_key = myrandom_r(arg->random_data) % arg->cli->num_elements;
        extra.u.h.new_val = myrandom_r(arg->random_data) % MAX_RANDOM_VAL;
        // just make every other value random
        if (i%2 == 0) {
            extra.u.h.new_val = -extra.u.h.new_val;
        }
        curr_val_sum += extra.u.h.new_val;
        extra.u.h.expected = op_args->update_history_buffer[rand_key];
        op_args->update_history_buffer[rand_key] = extra.u.h.new_val;
        r = db->update(
            db,
            txn,
            dbt_init(&key, &rand_key, sizeof rand_key),
            dbt_init(&val, &extra, sizeof extra),
            0
            );
        if (r != 0) {
            return r;
        }
    }
    //
    // now put in one more to ensure that the sum stays 0
    //
    extra.u.h.new_val = -curr_val_sum;
    rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->cli->num_elements;
    }
    extra.u.h.expected = op_args->update_history_buffer[rand_key];
    op_args->update_history_buffer[rand_key] = extra.u.h.new_val;
    r = db->update(
        db,
        txn,
        dbt_init(&key, &rand_key, sizeof rand_key),
        dbt_init(&val, &extra, sizeof extra),
        0
        );
    if (r != 0) {
        return r;
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
        r = db->hot_optimize(db, hot_progress_callback, NULL);
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

static int UU() remove_and_recreate_me(DB_TXN *UU(txn), ARG arg, void* UU(operation_extra), void *UU(stats_extra)) {
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->cli->num_DBs;
    DB* db = arg->dbp[db_index];
    r = (db)->close(db, 0); CKERR(r);

    char name[30];
    ZERO_ARRAY(name);
    get_ith_table_name(name, sizeof(name), db_index);

    r = arg->env->dbremove(arg->env, null_txn, name, NULL, 0);
    CKERR(r);

    r = db_create(&(arg->dbp[db_index]), arg->env, 0);
    assert(r == 0);
    r = arg->dbp[db_index]->open(arg->dbp[db_index], null_txn, name, NULL, DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    return 0;
}

static inline int
intmin(const int a, const int b)
{
    if (a < b) {
        return a;
    }
    return b;
}

struct test_time_extra {
    int num_seconds;
    bool crash_at_end;
    struct worker_extra *wes;
    int num_wes;
    struct cli_args *cli_args;
};

static void *test_time(void *arg) {
    struct test_time_extra* CAST_FROM_VOIDP(tte, arg);
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

static void crashing_alarm_handler(int sig) {
    assert(sig == SIGALRM);
    toku_hard_crash_on_purpose();
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
    toku_mutex_init(&mutex, NULL);
    struct rwlock rwlock;
    rwlock_init(&rwlock);
    toku_pthread_t tids[num_threads];
    toku_pthread_t time_tid;
    if (cli_args->print_performance) {
        perf_formatter->header(cli_args, num_threads);
    }
    struct worker_extra *worker_extra = (struct worker_extra *)
        memalign(64, num_threads * sizeof (struct worker_extra)); // allocate worker_extra's on cache line boundaries
    struct test_time_extra tte;
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
        { int chk_r = toku_pthread_create(&tids[i], NULL, worker, &worker_extra[i]); CKERR(chk_r); }
        if (verbose)
            printf("%lu created\n", (unsigned long) tids[i]);
    }
    { int chk_r = toku_pthread_create(&time_tid, NULL, test_time, &tte); CKERR(chk_r); }
    if (verbose)
        printf("%lu created\n", (unsigned long) time_tid);

    void *ret;
    r = toku_pthread_join(time_tid, &ret); assert_zero(r);
    if (verbose) printf("%lu joined\n", (unsigned long) time_tid);
    sighandler_t old_alarm = signal(SIGALRM, crashing_alarm_handler);
    assert(old_alarm != SIG_ERR);
    // Set an alarm that will kill us if it takes too long to join all the
    // threads (i.e. there is some runaway thread).
    unsigned int remaining = alarm(cli_args->join_timeout);
    assert_zero(remaining);
    for (int i = 0; i < num_threads; ++i) {
        r = toku_pthread_join(tids[i], &ret); assert_zero(r);
        if (verbose)
            printf("%lu joined\n", (unsigned long) tids[i]);
    }
    // All threads joined, deschedule the alarm.
    remaining = alarm(0);
    assert(remaining > 0);

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
    *env_res = env;

    for (int i = 0; i < num_DBs; i++) {
        DB *db;
        char name[30];
        memset(name, 0, sizeof(name));
        get_ith_table_name(name, sizeof(name), i);
        r = db_create(&db, env, 0);
        CKERR(r);
        r = db->set_flags(db, 0);
        CKERR(r);
        r = db->set_pagesize(db, env_args.node_size);
        CKERR(r);
        r = db->set_readpagesize(db, env_args.basement_node_size);
        CKERR(r);
        const int flags = DB_CREATE | (cli_args->blackhole ? DB_BLACKHOLE : 0);
        r = db->open(db, null_txn, name, NULL, DB_BTREE, flags, 0666);
        CKERR(r);
        db_res[i] = db;
    }
    return r;
}

static int fill_table_from_fun(DB_ENV *env, DB *db, int num_elements, int key_bufsz, int val_bufsz,
                               void (*callback)(int idx, void *extra,
                                                void *key, int *keysz,
                                                void *val, int *valsz),
                               void *extra, void (*progress_cb)(int num_rows)) {
    DB_TXN *txn = nullptr;
    const int puts_per_txn = 100000;
    int r = 0;
    for (long i = 0; i < num_elements; ++i) {
        if (txn == nullptr) {
            r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        }
        char keybuf[key_bufsz], valbuf[val_bufsz];
        memset(keybuf, 0, sizeof(keybuf));
        memset(valbuf, 0, sizeof(valbuf));
        int keysz, valsz;
        callback(i, extra, keybuf, &keysz, valbuf, &valsz);
        // let's make sure the data stored fits in the buffers we passed in
        assert(keysz <= key_bufsz);
        assert(valsz <= val_bufsz);
        DBT key, val;
        // make size of data what is specified w/input parameters
        // note that key and val have sizes of
        // key_bufsz and val_bufsz, which were passed into this
        // function, not what was stored by the callback
        r = db->put(
            db,
            txn,
            dbt_init(&key, keybuf, key_bufsz),
            dbt_init(&val, valbuf, val_bufsz),
            // don't bother taking locks in the locktree
            DB_PRELOCKED_WRITE
            );
        assert(r == 0);
        if (i > 0 && i % puts_per_txn == 0) {
            // don't bother fsyncing to disk.
            // the caller can checkpoint if they want.
            r = txn->commit(txn, DB_TXN_NOSYNC); CKERR(r);
            txn = nullptr;
            if (verbose) {
                progress_cb(puts_per_txn);
            }
        }
    }
    if (txn) {
        r = txn->commit(txn, DB_TXN_NOSYNC);
        invariant_zero(r);
    }
    return r;
}

static uint32_t breverse(uint32_t v)
// Effect: return the bits in i, reversed
// Notes: implementation taken from http://graphics.stanford.edu/~seander/bithacks.html#BitReverseObvious
// Rationale: just a hack to spread out the keys during loading, doesn't need to be fast but does need to be correct.
{
    uint32_t r = v; // r will be reversed bits of v; first get LSB of v
    int s = sizeof(v) * CHAR_BIT - 1; // extra shift needed at end

    for (v >>= 1; v; v >>= 1) {
        r <<= 1;
        r |= v & 1;
        s--;
    }
    r <<= s; // shift when v's highest bits are zero
    return r;
}

static void zero_element_callback(int idx, void *extra, void *keyv, int *keysz, void *valv, int *valsz) {
    const bool *disperse_keys = static_cast<bool *>(extra);
    int *CAST_FROM_VOIDP(key, keyv);
    int *CAST_FROM_VOIDP(val, valv);
    if (*disperse_keys) {
        *key = static_cast<int>(breverse(idx));
    } else {
        *key = idx;
    }
    *val = 0;
    *keysz = sizeof(int);
    *valsz = sizeof(int);
}

struct fill_table_worker_info {
    DB_ENV *env;
    DB *db;
    int num_elements;
    uint32_t key_size;
    uint32_t val_size;
    bool disperse_keys;
    void (*progress_cb)(int num_rows);
};

static void fill_table_worker(void *arg) {
    struct fill_table_worker_info *CAST_FROM_VOIDP(info, arg);
    int r = fill_table_from_fun(
        info->env,
        info->db,
        info->num_elements,
        info->key_size,
        info->val_size,
        zero_element_callback,
        &info->disperse_keys,
        info->progress_cb
        );
    invariant_zero(r);
    toku_free(info);
}

static int num_tables_to_fill = 1;
static int rows_per_table = 1;

static void report_overall_fill_table_progress(int num_rows) {
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
        t0 = toku_current_time_usec();
        last_report = t0;
    }

    uint64_t rows_so_far = toku_sync_add_and_fetch(&rows_inserted, num_rows);
    double progress = rows_so_far /
        (rows_per_table * num_tables_to_fill * 1.0);
    if (progress > (last_progress + .01)) {
        uint64_t t1 = toku_current_time_usec();
        // report no more often than once every 5 seconds, for less output.
        // there is a race condition. it is probably harmless.
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

static int fill_tables_with_zeroes(DB_ENV *env, DB **dbs, int num_DBs, int num_elements, uint32_t key_size, uint32_t val_size, bool disperse_keys) {
    // set the static globals that the progress reporter uses
    num_tables_to_fill = num_DBs;
    rows_per_table = num_elements;

    const int num_cores = toku_os_get_number_processors();
    KIBBUTZ kibbutz = toku_kibbutz_create(num_cores);
    for (int i = 0; i < num_DBs; i++) {
        assert(key_size >= sizeof(int));
        assert(val_size >= sizeof(int));
        struct fill_table_worker_info *XCALLOC(info);
        info->env = env;
        info->db = dbs[i];
        info->num_elements = num_elements;
        info->key_size = key_size;
        info->val_size = val_size;
        info->disperse_keys = disperse_keys;
        info->progress_cb = report_overall_fill_table_progress;
        toku_kibbutz_enq(kibbutz, fill_table_worker, info);
    }
    toku_kibbutz_destroy(kibbutz);
    return 0;
}

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

    /* create the dup database file */
    DB_ENV *env;
    db_env_set_num_bucket_mutexes(env_args.num_bucket_mutexes);
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, bt_compare); CKERR(r);
    r = env->set_lk_max_memory(env, env_args.lk_max_memory); CKERR(r);
    env->set_update(env, env_args.update_function);
    // set the cache size to 10MB
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
    *env_res = env;

    for (int i = 0; i < num_DBs; i++) {
        DB *db;
        char name[30];
        memset(name, 0, sizeof(name));
        get_ith_table_name(name, sizeof(name), i);
        r = db_create(&db, env, 0);
        CKERR(r);
        const int flags = cli_args->blackhole ? DB_BLACKHOLE : 0;
        r = db->open(db, null_txn, name, NULL, DB_BTREE, flags, 0666);
        CKERR(r);
        db_res[i] = db;
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
    .lk_max_memory = 1L * 1024 * 1024 * 1024,
    .cachetable_size = 300000,
    .num_bucket_mutexes = 1024,
    .envdir = ENVDIR,
    .update_function = update_op_callback,
    .generate_put_callback = NULL,
    .generate_del_callback = NULL,
};

static const struct env_args DEFAULT_PERF_ENV_ARGS = {
    .node_size = 4*1024*1024,
    .basement_node_size = 128*1024,
    .rollback_node_size = 4*1024*1024,
    .checkpointing_period = 60,
    .cleaner_period = 1,
    .cleaner_iterations = 5,
    .lk_max_memory = 1L * 1024 * 1024 * 1024,
    .cachetable_size = 1<<30,
    .num_bucket_mutexes = 1024 * 1024,
    .envdir = ENVDIR,
    .update_function = NULL,
    .generate_put_callback = NULL,
    .generate_del_callback = NULL,
};

#define MIN_VAL_SIZE sizeof(int)
#define MIN_KEY_SIZE sizeof(int)
#define MIN_COMPRESSIBILITY (0.0)
#define MAX_COMPRESSIBILITY (1.0)
static struct cli_args UU() get_default_args(void) {
    struct cli_args DEFAULT_ARGS = {
        .num_elements = 150000,
        .num_DBs = 1,
        .num_seconds = 180,
        .join_timeout = 600,
        .only_create = false,
        .only_stress = false,
        .update_broadcast_period_ms = 2000,
        .num_ptquery_threads = 1,
        .do_test_and_crash = false,
        .do_recover = false,
        .num_update_threads = 1,
        .num_put_threads = 1,
        .serial_insert = false,
        .interleave = false,
        .crash_on_operation_failure = true,
        .print_performance = false,
        .print_thread_performance = true,
        .print_iteration_performance = true,
        .perf_output_format = HUMAN,
        .performance_period = 1,
        .txn_size = 1000,
        .key_size = MIN_KEY_SIZE,
        .val_size = MIN_VAL_SIZE,
        .compressibility = 1.0,
        .env_args = DEFAULT_ENV_ARGS,
        .single_txn = false,
        .warm_cache = false,
        .blackhole = false,
        .nolocktree = false,
        .unique_checks = false,
        .nosync = false,
        .nolog = false,
        .nocrashstatus = false,
        .prelock_updates = false,
        .disperse_keys = false,
        };
    return DEFAULT_ARGS;
}

static struct cli_args UU() get_default_args_for_perf(void) {
    struct cli_args args = get_default_args();
    args.num_elements = 1000000; //default of 1M
    //args.print_performance = true;
    args.env_args = DEFAULT_PERF_ENV_ARGS;
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
//TODO fix
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
//    fprintf(stderr, "\t--compressibility               DOUBLE (default %.2f, minimum %.2f, maximum %.2f)\n",
//            default_args.compressibility, MIN_COMPRESSIBILITY, MAX_COMPRESSIBILITY);
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

    const char *perf_format_s = NULL;
    struct arg_type arg_types[] = {
        INT32_ARG_NONNEG("--num_elements",            num_elements,                  ""),
        INT32_ARG_NONNEG("--num_DBs",                 num_DBs,                       ""),
        INT32_ARG_NONNEG("--num_seconds",             num_seconds,                   "s"),
        INT32_ARG_NONNEG("--join_timeout",            join_timeout,                  "s"),
        INT32_ARG_NONNEG("--node_size",               env_args.node_size,            " bytes"),
        INT32_ARG_NONNEG("--basement_node_size",      env_args.basement_node_size,   " bytes"),
        INT32_ARG_NONNEG("--rollback_node_size",      env_args.rollback_node_size,   " bytes"),
        INT32_ARG_NONNEG("--checkpointing_period",    env_args.checkpointing_period, "s"),
        INT32_ARG_NONNEG("--cleaner_period",          env_args.cleaner_period,       "s"),
        INT32_ARG_NONNEG("--cleaner_iterations",      env_args.cleaner_iterations,   ""),
        INT32_ARG_NONNEG("--update_broadcast_period", update_broadcast_period_ms,    "ms"),
        INT32_ARG_NONNEG("--num_ptquery_threads",     num_ptquery_threads,           " threads"),
        INT32_ARG_NONNEG("--num_put_threads",         num_put_threads,               " threads"),
        INT32_ARG_NONNEG("--num_update_threads",      num_update_threads,            " threads"),

        UINT32_ARG("--txn_size",                      txn_size,                      " rows"),
        UINT32_ARG("--num_bucket_mutexes",            env_args.num_bucket_mutexes,   " mutexes"),

        INT32_ARG_R("--performance_period",           performance_period,            "s", 1, INT32_MAX),

        // TODO: John thinks the cachetable size should be in megabytes
        // and that lock memory should be in kilobytes. Is it worth the
        // inconsitency for a bit of convenience when running tests?
        UINT64_ARG("--cachetable_size",               env_args.cachetable_size,      " bytes"),
        UINT64_ARG("--lk_max_memory",                 env_args.lk_max_memory,        " bytes"),

        DOUBLE_ARG_R("--compressibility",             compressibility,               "", 0.0, 1.0),

        //TODO: when outputting help.. skip min/max that is min/max of data range.
        UINT32_ARG_R("--key_size",                    key_size,                      " bytes", MIN_KEY_SIZE, UINT32_MAX),
        UINT32_ARG_R("--val_size",                    val_size,                      " bytes", MIN_VAL_SIZE, UINT32_MAX),

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
        BOOL_ARG("nosync",                            nosync),
        BOOL_ARG("nolog",                             nolog),
        BOOL_ARG("nocrashstatus",                     nocrashstatus),
        BOOL_ARG("prelock_updates",                   prelock_updates),
        BOOL_ARG("disperse_keys",                     disperse_keys),

        STRING_ARG("--envdir",                        env_args.envdir),

        LOCAL_STRING_ARG("--perf_format",             perf_format_s,                 "human"),
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
    if (perf_format_s != NULL) {
        if (!strcmp(perf_format_s, "human")) {
            args->perf_output_format = HUMAN;
        } else if (!strcmp(perf_format_s, "csv")) {
            args->perf_output_format = CSV;
        } else if (!strcmp(perf_format_s, "tsv")) {
            args->perf_output_format = TSV;
#if 0
        } else if (!strcmp(perf_format_s, "gnuplot")) {
            args->perf_output_format = GNUPLOT;
#endif
        } else {
            fprintf(stderr, "valid values for --perf_format are \"human\", \"csv\", and \"tsv\"\n");
            //fprintf(stderr, "valid values for --perf_format are \"human\", \"csv\", \"tsv\", and \"gnuplot\"\n");
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
UU() stress_int_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
    assert(db && a && b);
    assert(a->size >= sizeof(int));
    assert(b->size >= sizeof(int));

    int x = *(int *) a->data;
    int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static int
UU() stress_uint64_dbt_cmp(DB *db, const DBT *a, const DBT *b) {
    assert(db && a && b);
    assert(a->size >= sizeof(uint64_t));
    assert(b->size >= sizeof(uint64_t));

    uint64_t x = *(uint64_t *) a->data;
    uint64_t y = *(uint64_t *) b->data;

    if (x < y) {
        return -1;
    }
    if (x > y) {
        return +1;
    }
    return 0;
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
    DB_TXN* txn = NULL;
    // don't take serializable read locks when scanning.
    int r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT); CKERR(r);
    // make sure the scan doesn't terminate early
    run_test = true;
    // warm up each DB in parallel
    scan_op_no_check_parallel(txn, &scan_arg, &soe, NULL);
    r = txn->commit(txn,0); CKERR(r);
}

static void
UU() stress_test_main_with_cmp(struct cli_args *args, int (*bt_compare)(DB *, const DBT *, const DBT *))
{
    { char *loc = setlocale(LC_NUMERIC, "en_US.UTF-8"); assert(loc); }
    DB_ENV* env = NULL;
    DB* dbs[args->num_DBs];
    memset(dbs, 0, sizeof(dbs));
    db_env_enable_engine_status(args->nocrashstatus ? false : true);
    if (!args->only_stress) {
        create_tables(
            &env,
            dbs,
            args->num_DBs,
            bt_compare,
            args
            );
        { int chk_r = fill_tables_with_zeroes(env, dbs, args->num_DBs, args->num_elements, args->key_size, args->val_size, args->disperse_keys); CKERR(chk_r); }
        { int chk_r = close_tables(env, dbs, args->num_DBs); CKERR(chk_r); }
    }
    if (!args->only_create) {
        { int chk_r = open_tables(&env,
                                  dbs,
                                  args->num_DBs,
                                  bt_compare,
                                  args); CKERR(chk_r); }
        if (args->warm_cache) {
            do_warm_cache(env, dbs, args);
        }
        stress_table(env, dbs, args);
        { int chk_r = close_tables(env, dbs, args->num_DBs); CKERR(chk_r); }
    }
}

static void
UU() stress_test_main(struct cli_args *args)
{
    stress_test_main_with_cmp(args, stress_int_dbt_cmp);
}

static void
UU() stress_recover(struct cli_args *args) {
    DB_ENV* env = NULL;
    DB* dbs[args->num_DBs];
    memset(dbs, 0, sizeof(dbs));
    { int chk_r = open_tables(&env,
                              dbs,
                              args->num_DBs,
                              stress_int_dbt_cmp,
                              args); CKERR(chk_r); }

    DB_TXN* txn = NULL;
    struct arg recover_args;
    arg_init(&recover_args, dbs, env, args);
    int r = env->txn_begin(env, 0, &txn, recover_args.txn_type);
    CKERR(r);
    struct scan_op_extra soe;
    soe.fast = true;
    soe.fwd = true;
    soe.prefetch = false;
    // make sure the scan doesn't terminate early
    run_test = true;
    r = scan_op(txn, &recover_args, &soe, NULL);
    CKERR(r);
    { int chk_r = txn->commit(txn,0); CKERR(chk_r); }
    { int chk_r = close_tables(env, dbs, args->num_DBs); CKERR(chk_r); }
}

#endif
