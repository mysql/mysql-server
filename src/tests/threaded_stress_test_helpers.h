/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef _THREADED_STRESS_TEST_HELPERS_H_
#define _THREADED_STRESS_TEST_HELPERS_H_

#include "test.h"

#include "rwlock.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>
#include <malloc.h>

static inline int32_t
myrandom_r(struct random_data *buf)
{
    int32_t x;
    int r = random_r(buf, &x);
    CKERR(r);
    return x;
}

volatile bool run_test; // should be volatile since we are communicating through this variable.

typedef struct arg *ARG;
typedef int (*operation_t)(DB_TXN *txn, ARG arg, void* operation_extra);

typedef int (*test_update_callback_f)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra);

enum stress_lock_type {
    STRESS_LOCK_NONE = 0,
    STRESS_LOCK_SHARED,
    STRESS_LOCK_EXCL
};

struct arg {
    int num_elements; // number of elements per DB
    DB **dbp; // array of DBs
    int num_DBs; // number of DBs
    DB_ENV* env; // environment used
    bool bounded_element_range; // true if elements in dictionary are bounded
                                // by num_elements, that is, all keys in each
                                // DB are in [0, num_elements)
                                // false otherwise
    int sleep_ms; // number of milliseconds to sleep between operations
    u_int32_t txn_type; // isolation level for txn running operation
    operation_t operation; // function that is the operation to be run
    void* operation_extra; // extra parameter passed to operation
    enum stress_lock_type lock_type; // states if operation must be exclusive, shared, or does not require locking
    bool crash_on_operation_failure; // true if we should crash if operation returns non-zero, false otherwise
    struct random_data *random_data; // state for random_r
    bool single_txn;
};

struct env_args {
    int node_size;
    int basement_node_size;
    int checkpointing_period;
    int cleaner_period;
    int cleaner_iterations;
    u_int64_t cachetable_size;
    char *envdir;
    test_update_callback_f update_function; // update callback function
};

struct cli_args {
    int num_elements; // number of elements per DB
    int num_DBs; // number of DBs
    int time_of_test; // how long test should run
    bool only_create; // true if want to only create DBs but not run stress
    bool only_stress; // true if DBs are already created and want to only run stress
    int update_broadcast_period_ms; // specific to test_stress3
    int num_ptquery_threads; // number of threads to run point queries
    bool do_test_and_crash; // true if we should crash after running stress test. For recovery tests.
    bool do_recover; // true if we should run recover
    int num_update_threads; // number of threads running updates
    bool crash_on_update_failure; 
    bool print_performance;
    bool print_thread_performance;
    int performance_period;
    u_int32_t update_txn_size; // for clients that do updates, specifies number of updates per txn
    u_int32_t key_size; // number of bytes in vals. Must be at least 4
    u_int32_t val_size; // number of bytes in vals. Must be at least 4
    struct env_args env_args; // specifies environment variables
    bool single_txn;
};

DB_TXN * const null_txn = 0;

static void arg_init(struct arg *arg, int num_elements, DB **dbp, DB_ENV *env, struct cli_args *cli_args) {
    arg->num_elements = num_elements;
    arg->dbp = dbp;
    arg->num_DBs = cli_args->num_DBs;
    arg->env = env;
    arg->bounded_element_range = true;
    arg->sleep_ms = 0;
    arg->lock_type = STRESS_LOCK_NONE;
    arg->txn_type = DB_TXN_SNAPSHOT;
    arg->crash_on_operation_failure = cli_args->crash_on_update_failure;
    arg->single_txn = cli_args->single_txn;
    arg->operation_extra = NULL;
}

struct worker_extra {
    struct arg* thread_arg;
    toku_pthread_mutex_t *operation_lock_mutex;
    struct rwlock *operation_lock;
    int64_t num_operations_completed;
    int64_t pad[4]; // pad to 64 bytes
};

static void lock_worker_op(struct worker_extra* we) {
    ARG arg = we->thread_arg;
    if (arg->lock_type != STRESS_LOCK_NONE) {
        if (0) toku_pthread_mutex_lock(we->operation_lock_mutex);
        if (arg->lock_type == STRESS_LOCK_SHARED) {
            rwlock_read_lock(we->operation_lock, we->operation_lock_mutex);
        } else if (arg->lock_type == STRESS_LOCK_EXCL) {
            rwlock_write_lock(we->operation_lock, we->operation_lock_mutex);
        } else {
            assert(false);
        }
        if (0) toku_pthread_mutex_unlock(we->operation_lock_mutex);
    }
}

static void unlock_worker_op(struct worker_extra* we) {
    ARG arg = we->thread_arg;
    if (arg->lock_type != STRESS_LOCK_NONE) {
        if (0) toku_pthread_mutex_lock(we->operation_lock_mutex);
        if (arg->lock_type == STRESS_LOCK_SHARED) {
            rwlock_read_unlock(we->operation_lock);
        } else if (arg->lock_type == STRESS_LOCK_EXCL) {
            rwlock_write_unlock(we->operation_lock);
        } else {
            assert(false);
        }
        if (0) toku_pthread_mutex_unlock(we->operation_lock_mutex);
    }
}

static void *worker(void *arg_v) {
    int r;
    struct worker_extra* we = arg_v;
    ARG arg = we->thread_arg;
    struct random_data random_data;
    memset(&random_data, 0, sizeof random_data);
    char *random_buf = toku_xmalloc(8);
    memset(random_buf, 0, 8);
    r = initstate_r(random(), random_buf, 8, &random_data);
    assert_zero(r);
    arg->random_data = &random_data;
    DB_ENV *env = arg->env;
    DB_TXN *txn = NULL;
    if (verbose) {
        printf("%lu starting %p\n", toku_pthread_self(), arg->operation);
    }
    if (arg->single_txn) {
        r = env->txn_begin(env, 0, &txn, arg->txn_type); CKERR(r);
    }
    while (run_test) {
        lock_worker_op(we);
        if (!arg->single_txn) {
            r = env->txn_begin(env, 0, &txn, arg->txn_type); CKERR(r);
        }
        r = arg->operation(txn, arg, arg->operation_extra);
        if (r == 0) {
            if (!arg->single_txn) {
                CHK(txn->commit(txn,0));
            }
        } else {
            if (arg->crash_on_operation_failure) {
                CKERR(r);
            } else {
                if (!arg->single_txn) {
                    CHK(txn->abort(txn));
                }
            }
        }
        unlock_worker_op(we);
        we->num_operations_completed++;
        if (arg->sleep_ms) {
            usleep(arg->sleep_ms * 1000);
        }
    }
    if (arg->single_txn) {
        CHK(txn->commit(txn, 0));
    }
    if (verbose)
        printf("%lu returning\n", toku_pthread_self());
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
    SCAN_CB_EXTRA cb_extra = arg_v;
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

    CHK(db->cursor(db, txn, &cursor, 0));
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
    }
    CHK(cursor->c_close(cursor));
    if (r == DB_NOTFOUND) {
        r = 0;
    }
    if (check_sum && e.curr_sum) {
        printf("e.curr_sum: %"PRId64" e.num_elements: %"PRId64" \n", e.curr_sum, e.num_elements);
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

static int UU() nop(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra)) {
    return 0;
}

static int UU() xmalloc_free_op(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra)) {
    size_t s = 256;
    void *p = toku_xmalloc(s);
    toku_free(p);
    return 0;
}

static int UU() malloc_free_op(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra)) {
    size_t s = 256;
    void *p = malloc(s);
    free(p);
    return 0;
}

static int UU() loader_op(DB_TXN* txn, ARG UU(arg), void* UU(operation_extra)) {
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
        u_int32_t loader_flags = (num == 0) ? 0 : LOADER_USE_PUTS;
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

static int UU() keyrange_op(DB_TXN *txn, ARG arg, void* UU(operation_extra)) {
    int r;
    // callback is designed to run on tests with one DB
    // no particular reason why, just the way it was 
    // originally done
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    int rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->num_elements;
    }
    DBT key;
    dbt_init(&key, &rand_key, sizeof rand_key);
    u_int64_t less,equal,greater;
    int is_exact;
    r = db->key_range64(db, txn, &key, &less, &equal, &greater, &is_exact);
    assert(r == 0);
    return r;
}

static int UU() verify_op(DB_TXN* UU(txn), ARG UU(arg), void* UU(operation_extra)) {
    int r;
    for (int i = 0; i < arg->num_DBs; i++) {
        DB* db = arg->dbp[i];
        r = db->verify_with_progress(db, NULL, NULL, 0, 0);
    }
    CKERR(r);
    return r;
}

static int UU() scan_op(DB_TXN *txn, ARG UU(arg), void* operation_extra) {
    struct scan_op_extra* extra = operation_extra;
    for (int i = 0; i < arg->num_DBs; i++) {
        int r = scan_op_and_maybe_check_sum(arg->dbp[i], txn, extra, true);
        assert_zero(r);
    }
    return 0;
}

static int UU() scan_op_no_check(DB_TXN *txn, ARG arg, void* operation_extra) {
    struct scan_op_extra* extra = operation_extra;
    for (int i = 0; i < arg->num_DBs; i++) {
        int r = scan_op_and_maybe_check_sum(arg->dbp[i], txn, extra, false);
        assert_zero(r);
    }
    return 0;
}

static int UU() ptquery_and_maybe_check_op(DB* db, DB_TXN *txn, ARG arg, BOOL check) {
    int r;
    int rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->num_elements;
    }
    DBT key, val;
    memset(&val, 0, sizeof val);
    dbt_init(&key, &rand_key, sizeof rand_key);
    r = db->get(db, txn, &key, &val, 0);
    if (check) assert(r != DB_NOTFOUND);
    r = 0;
    return r;
}

static int UU() ptquery_op(DB_TXN *txn, ARG arg, void* UU(operation_extra)) {
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    return ptquery_and_maybe_check_op(db, txn, arg, TRUE);
}

static int UU() ptquery_op_no_check(DB_TXN *txn, ARG arg, void* UU(operation_extra)) {
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    return ptquery_and_maybe_check_op(db, txn, arg, FALSE);
}

static int UU() cursor_create_close_op(DB_TXN *txn, ARG arg, void* UU(operation_extra)) {
    int db_index = arg->num_DBs > 1 ? random()%arg->num_DBs : 0;
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
            int new;
        } h;
    } u;
};

struct update_op_args {
    int *update_history_buffer;
    u_int32_t update_txn_size;
    int update_pad_frequency;
};

static struct update_op_args UU() get_update_op_args(struct cli_args* cli_args, int* update_history_buffer) {    
    struct update_op_args uoe;
    uoe.update_history_buffer = update_history_buffer;
    uoe.update_pad_frequency = cli_args->num_elements/100; // arbitrary
    uoe.update_txn_size = cli_args->update_txn_size;
    return uoe;
}

static u_int64_t update_count = 0;

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
    struct update_op_extra *e = extra->data;

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
        new_int_val = e->u.h.new;
        break;
    default:
        assert(FALSE);
    }

    DBT new_val;
    u_int32_t data_size = sizeof(int) + e->pad_bytes;
    char* data [data_size];
    memset(data, 0, data_size);
    memcpy(data, &new_int_val, sizeof(new_int_val));
    set_val(dbt_init(&new_val, data, data_size), set_extra);
    return 0;
}

static int UU()update_op2(DB_TXN* txn, ARG arg, void* operation_extra) {
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    int rand_key2;
    struct update_op_args* op_args = operation_extra;
    update_count++;
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    for (u_int32_t i = 0; i < op_args->update_txn_size; i++) {
        rand_key = myrandom_r(arg->random_data);
        if (arg->bounded_element_range) {
            rand_key = rand_key % (arg->num_elements/2);
        }
        rand_key2 = arg->num_elements - rand_key;
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

static int UU()update_op(DB_TXN *txn, ARG arg, void* operation_extra) {
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    update_count++;
    struct update_op_args* op_args = operation_extra;
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    if (op_args->update_pad_frequency) {
        if (update_count % (2*op_args->update_pad_frequency) == update_count%op_args->update_pad_frequency) {
            extra.pad_bytes = 100;
        }
    }
    for (u_int32_t i = 0; i < op_args->update_txn_size; i++) {
        rand_key = myrandom_r(arg->random_data);
        if (arg->bounded_element_range) {
            rand_key = rand_key % arg->num_elements;
        }
        extra.u.d.diff = myrandom_r(arg->random_data) % MAX_RANDOM_VAL;
        // just make every other value random
        if (i%2 == 0) {
            extra.u.d.diff = -extra.u.d.diff;
        }
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
    }
    //
    // now put in one more to ensure that the sum stays 0
    //
    extra.u.d.diff = -curr_val_sum;
    rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->num_elements;
    }
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

static int UU() update_with_history_op(DB_TXN *txn, ARG arg, void* operation_extra) {
    struct update_op_args* op_args = operation_extra;
    assert(arg->bounded_element_range);
    assert(op_args->update_history_buffer);
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = UPDATE_WITH_HISTORY;
    update_count++;
    extra.pad_bytes = 0;
    if (op_args->update_pad_frequency) {
        if (update_count % (2*op_args->update_pad_frequency) != update_count%op_args->update_pad_frequency) {
            extra.pad_bytes = 500;
        }
        
    }
    for (u_int32_t i = 0; i < op_args->update_txn_size; i++) {
        rand_key = myrandom_r(arg->random_data) % arg->num_elements;
        extra.u.h.new = myrandom_r(arg->random_data) % MAX_RANDOM_VAL;
        // just make every other value random
        if (i%2 == 0) {
            extra.u.h.new = -extra.u.h.new;
        }
        curr_val_sum += extra.u.h.new;
        extra.u.h.expected = op_args->update_history_buffer[rand_key];
        op_args->update_history_buffer[rand_key] = extra.u.h.new;
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
    extra.u.h.new = -curr_val_sum;
    rand_key = myrandom_r(arg->random_data);
    if (arg->bounded_element_range) {
        rand_key = rand_key % arg->num_elements;
    }
    extra.u.h.expected = op_args->update_history_buffer[rand_key];
    op_args->update_history_buffer[rand_key] = extra.u.h.new;
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

static int UU() update_broadcast_op(DB_TXN *txn, ARG arg, void* UU(operation_extra)) {
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    extra.type = UPDATE_NEGATE;
    extra.pad_bytes = 0;
    DBT val;
    int r = db->update_broadcast(db, txn, dbt_init(&val, &extra, sizeof extra), 0);
    CKERR(r);
    return r;
}

static int UU() hot_op(DB_TXN *UU(txn), ARG UU(arg), void* UU(operation_extra)) {
    int r;
    for (int i = 0; i < arg->num_DBs; i++) {
        DB* db = arg->dbp[i];
        r = db->hot_optimize(db, NULL, NULL);
        CKERR(r);
    }
    return 0;
}

static int UU() remove_and_recreate_me(DB_TXN *UU(txn), ARG arg, void* UU(operation_extra)) {
    int r;
    int db_index = myrandom_r(arg->random_data)%arg->num_DBs;
    DB* db = arg->dbp[db_index];
    r = (db)->close(db, 0); CKERR(r);
    
    char name[30];
    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), "main%d", db_index);

    r = arg->env->dbremove(arg->env, null_txn, name, NULL, 0);  
    CKERR(r);
    
    r = db_create(&(arg->dbp[db_index]), arg->env, 0);
    assert(r == 0);
    r = arg->dbp[db_index]->open(arg->dbp[db_index], null_txn, name, NULL, DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    return 0;
}

static int UU() truncate_me(DB_TXN *txn, ARG UU(arg), void* UU(operation_extra)) {
    int r;
    for ( int i = 0; i < arg->num_DBs; i++) {
        u_int32_t row_count = 0;
        r = (*arg->dbp)->truncate(*arg->dbp, txn, &row_count, 0);
        assert(r == 0);
    }
    return 0;
}



struct test_time_extra {
    int num_seconds;
    bool crash_at_end;
    struct worker_extra *wes;
    int num_wes;
    bool print_performance;
    bool print_thread_performance;
    int performance_period;
};

static void *test_time(void *arg) {
    struct test_time_extra* tte = arg;
    int num_seconds = tte->num_seconds;

    //
    // if num_Seconds is set to 0, run indefinitely
    //
    if (num_seconds == 0) {
        num_seconds = INT32_MAX;
    }
    if (verbose) {
        printf("Sleeping for %d seconds\n", num_seconds);
    }
    int64_t num_operations_completed_total[tte->num_wes];
    memset(num_operations_completed_total, 0, sizeof num_operations_completed_total);
    for (int i = 0; i < num_seconds; i += tte->performance_period) {
        usleep(tte->performance_period*1000*1000);
        int total_operations_in_period = 0;
        for (int we = 0; we < tte->num_wes; ++we) {
            int last = num_operations_completed_total[we];
            int current = __sync_fetch_and_add(&tte->wes[we].num_operations_completed, 0);
            if (tte->print_thread_performance) {
                printf("Thread %d Iteration %d Operations %d\n", we, i, current - last);
            }
            total_operations_in_period += (current - last);
            num_operations_completed_total[we] = current;
        }
        if (tte->print_performance) {
            printf("Iteration %d Total_Operations %d\n", i, total_operations_in_period);
        }
    }
    int total_operations_in_test = 0;
    for (int we = 0; we < tte->num_wes; ++we) {
        int current = __sync_fetch_and_add(&tte->wes[we].num_operations_completed, 0);
        if (tte->print_thread_performance) {
        printf("TOTAL Thread %d Operations %d\n", we, current);
        }
        total_operations_in_test += current;
    }
    if (tte->print_performance) {
        printf("Total_Operations %d\n", total_operations_in_test);
    }

    if (verbose) {
        printf("should now end test\n");
    }
    __sync_bool_compare_and_swap(&run_test, true, false); // make this atomic to make valgrind --tool=drd happy.
    if (verbose) {
        printf("run_test %d\n", run_test);
    }
    if (tte->crash_at_end) {
        toku_hard_crash_on_purpose();
    }
    return arg;
}

static int run_workers(
    struct arg *thread_args, 
    int num_threads, 
    u_int32_t num_seconds, 
    bool crash_at_end,
    struct cli_args* cli_args
    ) 
{
    int r;
    toku_pthread_mutex_t mutex;
    toku_pthread_mutex_init(&mutex, NULL);
    struct rwlock rwlock;
    rwlock_init(&rwlock);
    toku_pthread_t tids[num_threads];
    toku_pthread_t time_tid;
    struct worker_extra *worker_extra = (struct worker_extra *)
        memalign(64, num_threads * sizeof (struct worker_extra)); // allocate worker_extra's on cache line boundaries
    struct test_time_extra tte;
    tte.num_seconds = num_seconds;
    tte.crash_at_end = crash_at_end;
    tte.wes = worker_extra;
    tte.num_wes = num_threads;
    tte.print_performance = cli_args->print_performance;
    tte.print_thread_performance = cli_args->print_thread_performance;
    tte.performance_period = cli_args->performance_period;
    run_test = true;
    for (int i = 0; i < num_threads; ++i) {
        worker_extra[i].thread_arg = &thread_args[i];
        worker_extra[i].operation_lock = &rwlock;
        worker_extra[i].operation_lock_mutex = &mutex;
        worker_extra[i].num_operations_completed = 0;
        CHK(toku_pthread_create(&tids[i], NULL, worker, &worker_extra[i]));
        if (verbose) 
            printf("%lu created\n", tids[i]);
    }
    CHK(toku_pthread_create(&time_tid, NULL, test_time, &tte));
    if (verbose) 
        printf("%lu created\n", time_tid);

    void *ret;
    r = toku_pthread_join(time_tid, &ret); assert_zero(r);
    if (verbose) printf("%lu joined\n", time_tid);
    for (int i = 0; i < num_threads; ++i) {
        r = toku_pthread_join(tids[i], &ret); assert_zero(r);
        if (verbose) 
            printf("%lu joined\n", tids[i]);
    }
    if (verbose) 
        printf("ending test, pthreads have joined\n");
    rwlock_destroy(&rwlock);
    toku_pthread_mutex_destroy(&mutex);
    toku_free(worker_extra);
    return r;
}


static int create_tables(DB_ENV **env_res, DB **db_res, int num_DBs,
                        int (*bt_compare)(DB *, const DBT *, const DBT *),
                        struct env_args env_args
) {
    int r;

    char rmcmd[32 + strlen(env_args.envdir)]; sprintf(rmcmd, "rm -rf %s", env_args.envdir);
    r = system(rmcmd);
    CKERR(r);
    r = toku_os_mkdir(env_args.envdir, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, bt_compare); CKERR(r);
    r = env->set_cachesize(env, env_args.cachetable_size / (1 << 30), env_args.cachetable_size % (1 << 30), 1); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
    r = env->open(env, env_args.envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, env_args.checkpointing_period); CKERR(r);
    r = env->cleaner_set_period(env, env_args.cleaner_period); CKERR(r);
    r = env->cleaner_set_iterations(env, env_args.cleaner_iterations); CKERR(r);
    *env_res = env;


    for (int i = 0; i < num_DBs; i++) {
        DB *db;
        char name[30];
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), "main%d", i);
        r = db_create(&db, env, 0);
        CKERR(r);
        r = db->set_flags(db, 0);
        CKERR(r);
        r = db->set_pagesize(db, env_args.node_size);
        CKERR(r);
        r = db->set_readpagesize(db, env_args.basement_node_size);
        CKERR(r);
        r = db->open(db, null_txn, name, NULL, DB_BTREE, DB_CREATE, 0666);
        CKERR(r);
        db_res[i] = db;
    }
    return r;
}

static int fill_table_from_fun(DB *db, int num_elements, int key_bufsz, int val_bufsz,
                               void (*callback)(int idx, void *extra,
                                                void *key, int *keysz,
                                                void *val, int *valsz),
                               void *extra) {
    int r = 0;
    for (long i = 0; i < num_elements; ++i) {
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
            null_txn, 
            dbt_init(&key, keybuf, key_bufsz),
            dbt_init(&val, valbuf, val_bufsz), 
            0
            );
        assert(r == 0);
    }
    return r;
}

static void zero_element_callback(int idx, void *UU(extra), void *keyv, int *keysz, void *valv, int *valsz) {
    int *key = keyv, *val = valv;
    *key = idx;
    *val = 0;
    *keysz = sizeof(int);
    *valsz = sizeof(int);
}

static int fill_tables_with_zeroes(DB **dbs, int num_DBs, int num_elements, u_int32_t key_size, u_int32_t val_size) {
    for (int i = 0; i < num_DBs; i++) {
        assert(key_size >= sizeof(int));
        assert(val_size >= sizeof(int));
        int r = fill_table_from_fun(
            dbs[i], 
            num_elements, 
            key_size, 
            val_size, 
            zero_element_callback, 
            NULL
            );
        CKERR(r);
    }
    return 0;
}

static int open_tables(DB_ENV **env_res, DB **db_res, int num_DBs,
                      int (*bt_compare)(DB *, const DBT *, const DBT *),
                      struct env_args env_args) {
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_default_bt_compare(env, bt_compare); CKERR(r);
    env->set_update(env, env_args.update_function);
    // set the cache size to 10MB
    r = env->set_cachesize(env, env_args.cachetable_size / (1 << 30), env_args.cachetable_size % (1 << 30), 1); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
    r = env->open(env, env_args.envdir, DB_RECOVER|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, env_args.checkpointing_period); CKERR(r);
    r = env->cleaner_set_period(env, env_args.cleaner_period); CKERR(r);
    r = env->cleaner_set_iterations(env, env_args.cleaner_iterations); CKERR(r);
    *env_res = env;

    
    for (int i = 0; i < num_DBs; i++) {
        DB *db;
        char name[30];
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), "main%d", i);
        r = db_create(&db, env, 0);
        CKERR(r);
        r = db->open(db, null_txn, name, NULL, DB_BTREE, 0, 0666);
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
    .checkpointing_period = 10,
    .cleaner_period = 1,
    .cleaner_iterations = 1,
    .cachetable_size = 300000,
    .envdir = ENVDIR,
    .update_function = update_op_callback,
};

static const struct env_args DEFAULT_PERF_ENV_ARGS = {
    .node_size = 4*1024*1024,
    .basement_node_size = 128*1024,
    .checkpointing_period = 60,
    .cleaner_period = 1,
    .cleaner_iterations = 5,
    .cachetable_size = 1<<30,
    .envdir = ENVDIR,
    .update_function = NULL,
};

#define MIN_VAL_SIZE sizeof(int)
#define MIN_KEY_SIZE sizeof(int)
static struct cli_args UU() get_default_args(void) {
    struct cli_args DEFAULT_ARGS = {
        .num_elements = 150000,
        .num_DBs = 1,
        .time_of_test = 180,
        .only_create = false,
        .only_stress = false,
        .update_broadcast_period_ms = 2000,
        .num_ptquery_threads = 1,
        .do_test_and_crash = false,
        .do_recover = false,
        .num_update_threads = 1,
        .crash_on_update_failure = true,
        .print_performance = false,
        .print_thread_performance = false,
        .performance_period = 1,
        .update_txn_size = 1000,
        .key_size = MIN_KEY_SIZE,
        .val_size = MIN_VAL_SIZE,
        .env_args = DEFAULT_ENV_ARGS,
        .single_txn = false,
        };
    return DEFAULT_ARGS;
}

static struct cli_args UU() get_default_args_for_perf(void) {
    struct cli_args args = get_default_args();
    args.num_elements = 1000000; //default of 1M
    args.print_performance = true;
    args.env_args = DEFAULT_PERF_ENV_ARGS;
    return args;
}

static inline void parse_stress_test_args (int argc, char *const argv[], struct cli_args *args) {
    struct cli_args default_args = *args;
    const char *argv0=argv[0];
    while (argc>1) {
        int resultcode=0;
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0) {
            verbose++;
        } 
        else if (strcmp(argv[1], "-q")==0) {
            verbose=0;
        } 
        else if (strcmp(argv[1], "-h")==0) {
        do_usage:
            fprintf(stderr, "Usage:\n%s [-h|-v|-q] [OPTIONS] [--only_create|--only_stress]\n", argv0);
            fprintf(stderr, "OPTIONS are among:\n");
            fprintf(stderr, "\t--num_elements                  INT (default %d)\n", default_args.num_elements);
            fprintf(stderr, "\t--num_DBs                       INT (default %d)\n", default_args.num_DBs);
            fprintf(stderr, "\t--num_seconds                   INT (default %ds)\n", default_args.time_of_test);
            fprintf(stderr, "\t--node_size                     INT (default %d bytes)\n", default_args.env_args.node_size);
            fprintf(stderr, "\t--basement_node_size            INT (default %d bytes)\n", default_args.env_args.basement_node_size);
            fprintf(stderr, "\t--cachetable_size               INT (default %ld bytes)\n", default_args.env_args.cachetable_size);
            fprintf(stderr, "\t--checkpointing_period          INT (default %ds)\n",      default_args.env_args.checkpointing_period);
            fprintf(stderr, "\t--cleaner_period                INT (default %ds)\n",      default_args.env_args.cleaner_period);
            fprintf(stderr, "\t--cleaner_iterations            INT (default %ds)\n",      default_args.env_args.cleaner_iterations);
            fprintf(stderr, "\t--update_broadcast_period       INT (default %dms)\n",     default_args.update_broadcast_period_ms);
            fprintf(stderr, "\t--num_ptquery_threads           INT (default %d threads)\n", default_args.num_ptquery_threads);
            fprintf(stderr, "\t--num_update_threads            INT (default %d threads)\n", default_args.num_update_threads);
            fprintf(stderr, "\t--update_txn_size               INT (default %d rows)\n", default_args.update_txn_size);
            fprintf(stderr, "\t--key_size                      INT (default %d, minimum %ld)\n", default_args.key_size, MIN_KEY_SIZE);
            fprintf(stderr, "\t--val_size                      INT (default %d, minimum %ld)\n", default_args.val_size, MIN_VAL_SIZE);
            fprintf(stderr, "\t--[no-]crash_on_update_failure  BOOL (default %s)\n", default_args.crash_on_update_failure ? "yes" : "no");
            fprintf(stderr, "\t--print_performance             \n");
            fprintf(stderr, "\t--print_thread_performance      \n");
            fprintf(stderr, "\t--performance_period            INT (default %d)\n", default_args.performance_period);
            exit(resultcode);
        }
        else if (strcmp(argv[1], "--num_elements") == 0) {
            argc--; argv++;
            args->num_elements = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_DBs") == 0) {
            argc--; argv++;
            args->num_DBs = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_seconds") == 0) {
            argc--; argv++;
            args->time_of_test = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--node_size") == 0) {
            argc--; argv++;
            args->env_args.node_size = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--basement_node_size") == 0) {
            argc--; argv++;
            args->env_args.basement_node_size = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--cachetable_size") == 0) {
            argc--; argv++;
            args->env_args.cachetable_size = strtoll(argv[1], NULL, 0);
        }
        else if (strcmp(argv[1], "--checkpointing_period") == 0) {
            argc--; argv++;
            args->env_args.checkpointing_period = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--cleaner_period") == 0) {
            argc--; argv++;
            args->env_args.cleaner_period = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--cleaner_iterations") == 0) {
            argc--; argv++;
            args->env_args.cleaner_iterations = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--update_broadcast_period") == 0) {
            argc--; argv++;
            args->update_broadcast_period_ms = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_ptquery_threads") == 0 || strcmp(argv[1], "--num_threads") == 0) {
            argc--; argv++;
            args->num_ptquery_threads = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_update_threads") == 0) {
            argc--; argv++;
            args->num_update_threads = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--crash_on_update_failure") == 0) {
            args->crash_on_update_failure = true;
        }
        else if (strcmp(argv[1], "--no-crash_on_update_failure") == 0) {
            args->crash_on_update_failure = false;
        }
        else if (strcmp(argv[1], "--print_performance") == 0) {
            args->print_performance = true;
        }
        else if (strcmp(argv[1], "--print_thread_performance") == 0) {
            args->print_thread_performance = true;
        }
        else if (strcmp(argv[1], "--performance_period") == 0) {
            argc--; argv++;
            args->performance_period = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--update_txn_size") == 0) {
            argc--; argv++;
            args->update_txn_size = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--key_size") == 0) {
            argc--; argv++;
            args->key_size = atoi(argv[1]);
            assert(args->key_size >= MIN_KEY_SIZE);
        }
        else if (strcmp(argv[1], "--val_size") == 0) {
            argc--; argv++;
            args->val_size = atoi(argv[1]);
            assert(args->val_size >= MIN_VAL_SIZE);
        }
        else if (strcmp(argv[1], "--only_create") == 0) {
            args->only_create = true;
        }
        else if (strcmp(argv[1], "--only_stress") == 0) {
            args->only_stress = true;
        }
        else if (strcmp(argv[1], "--test") == 0) {
            args->do_test_and_crash = true;
        }
        else if (strcmp(argv[1], "--recover") == 0) {
            args->do_recover = true;
        }
        else if (strcmp(argv[1], "--envdir") == 0 && argc > 1) {
            argc--; argv++;
            args->env_args.envdir = argv[1];
        }
        else if (strcmp(argv[1], "--single_txn") == 0) {
            args->single_txn = true;
        }
        else {
            resultcode=1;
            goto do_usage;
        }
        argc--;
        argv++;
    }
    if (args->only_create && args->only_stress) {
        goto do_usage;
    }
}

static void
stress_table(DB_ENV *, DB **, struct cli_args *);

static int
stress_int_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
  assert(db && a && b);
  assert(a->size >= sizeof(int));
  assert(b->size >= sizeof(int));

  int x = *(int *) a->data;
  int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}


static void
stress_test_main(struct cli_args *args)
{
    DB_ENV* env = NULL;
    DB* dbs[args->num_DBs];
    memset(dbs, 0, sizeof(dbs));
    if (!args->only_stress) {
        create_tables(
            &env,
            dbs,
            args->num_DBs,
            stress_int_dbt_cmp,
            args->env_args
            );
        CHK(fill_tables_with_zeroes(dbs, args->num_DBs, args->num_elements, args->key_size, args->val_size));
        CHK(close_tables(env, dbs, args->num_DBs));
    }
    if (!args->only_create) {
        CHK(open_tables(&env,
                       dbs,
                       args->num_DBs,
                       stress_int_dbt_cmp,
                       args->env_args));
        stress_table(env, dbs, args);
        CHK(close_tables(env, dbs, args->num_DBs));
    }
}

static void
UU() stress_recover(struct cli_args *args) {
    DB_ENV* env = NULL;
    DB* dbs[args->num_DBs];
    memset(dbs, 0, sizeof(dbs));
    CHK(open_tables(&env,
                   dbs,
                   args->num_DBs,
                   stress_int_dbt_cmp,
                   args->env_args));

    DB_TXN* txn = NULL;    
    struct arg recover_args;
    arg_init(&recover_args, args->num_elements, dbs, env, args);
    int r = env->txn_begin(env, 0, &txn, recover_args.txn_type);
    CKERR(r);
    struct scan_op_extra soe;
    soe.fast = TRUE;
    soe.fwd = TRUE;
    soe.prefetch = FALSE;
    r = scan_op(txn, &recover_args, &soe);
    CKERR(r);
    CHK(txn->commit(txn,0));
    CHK(close_tables(env, dbs, args->num_DBs));
}

#endif
