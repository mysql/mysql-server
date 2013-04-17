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


volatile bool run_test; // should be volatile since we are communicating through this variable.

typedef struct arg *ARG;
typedef int (*operation_t)(DB_ENV *env, DB** dbp, DB_TXN *txn, ARG arg);

typedef int (*test_update_callback_f)(DB *, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra);

enum stress_lock_type {
    STRESS_LOCK_NONE = 0,
    STRESS_LOCK_SHARED,
    STRESS_LOCK_EXCL
};

struct arg {
    int n;
    DB **dbp;
    DB_ENV* env;
    bool fast;
    bool fwd;
    bool prefetch;
    bool bounded_update_range;
    int sleep_ms;
    enum stress_lock_type lock_type;
    u_int32_t txn_type;
    int *update_history_buffer;
    operation_t operation;
    toku_pthread_mutex_t *broadcast_lock_mutex;
    struct rwlock *broadcast_lock;
    int update_pad_frequency;
};

DB_TXN * const null_txn = 0;

static void arg_init(struct arg *arg, int n, DB **dbp, DB_ENV *env) {
    arg->n = n;
    arg->dbp = dbp;
    arg->env = env;
    arg->fast = true;
    arg->fwd = true;
    arg->prefetch = false; // setting this to TRUE causes thrashing, even with a cachetable size that is 400000. Must investigate
    arg->bounded_update_range = true;
    arg->sleep_ms = 0;
    arg->lock_type = STRESS_LOCK_NONE;
    arg->txn_type = DB_TXN_SNAPSHOT;
    arg->update_history_buffer = NULL;
    arg->update_pad_frequency = n/100; // bit arbitrary. Just want dictionary to grow and shrink so splits and merges occur
}

static void *worker(void *arg_v) {
    ARG arg = arg_v;
    DB_ENV *env = arg->env;
    DB** dbp = arg->dbp;
    DB_TXN *txn = NULL;
    while (run_test) {
        if (arg->lock_type != STRESS_LOCK_NONE) {
            toku_pthread_mutex_lock(arg->broadcast_lock_mutex);
            if (arg->lock_type == STRESS_LOCK_SHARED) {
                rwlock_read_lock(arg->broadcast_lock, arg->broadcast_lock_mutex);
            } else if (arg->lock_type == STRESS_LOCK_EXCL) {
                rwlock_write_lock(arg->broadcast_lock, arg->broadcast_lock_mutex);
            } else {
                assert(false);
            }
            toku_pthread_mutex_unlock(arg->broadcast_lock_mutex);
        }

        int r = env->txn_begin(env, 0, &txn, arg->txn_type); CKERR(r);
        r = arg->operation(env, dbp, txn, arg); CKERR(r);
        CHK(txn->commit(txn,0));

        toku_pthread_mutex_lock(arg->broadcast_lock_mutex);
        if (arg->lock_type == STRESS_LOCK_SHARED) {
            rwlock_read_unlock(arg->broadcast_lock);
        } else if (arg->lock_type == STRESS_LOCK_EXCL) {
            rwlock_write_unlock(arg->broadcast_lock);
        } else {
            assert(arg->lock_type == STRESS_LOCK_NONE);
        }
        toku_pthread_mutex_unlock(arg->broadcast_lock_mutex);

        if (arg->sleep_ms) {
            usleep(arg->sleep_ms * 1000);
        }
    }
    return arg;
}

typedef struct scan_cb_extra *SCAN_CB_EXTRA;
struct scan_cb_extra {
    bool fast;
    int64_t curr_sum;
    int64_t num_elements;
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

static int scan_op_and_maybe_check_sum(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg, bool check_sum) {
    int r = 0;
    DB* db = *dbp;
    DBC* cursor = NULL;

    struct scan_cb_extra e;
    e.fast = arg->fast;
    e.curr_sum = 0;
    e.num_elements = 0;

    CHK(db->cursor(db, txn, &cursor, 0));
    if (arg->prefetch) {
        r = cursor->c_pre_acquire_range_lock(cursor, db->dbt_neg_infty(), db->dbt_pos_infty());
        assert(r == 0);
    }
    while (r != DB_NOTFOUND) {
        if (arg->fwd) {
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

static int UU() loader_op(DB_ENV *env, DB** UU(dbp), DB_TXN* txn, ARG UU(arg)) {
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
            int rand_key = random();
            int rand_val = random();
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

static int UU() keyrange_op(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg) {
    int r;
    DB* db = *dbp;
    int rand_key = random();
    if (arg->bounded_update_range) {
        rand_key = rand_key % arg->n;
    }
    DBT key;
    dbt_init(&key, &rand_key, sizeof rand_key);
    u_int64_t less,equal,greater;
    int is_exact;
    r = db->key_range64(db, txn, &key, &less, &equal, &greater, &is_exact);
    assert(r == 0);
    return r;
}

static int UU() scan_op(DB_ENV *env, DB **dbp, DB_TXN *txn, ARG arg) {
    return scan_op_and_maybe_check_sum(env, dbp, txn, arg, true);
}

static int UU() scan_op_no_check(DB_ENV *env, DB **dbp, DB_TXN *txn, ARG arg) {
    return scan_op_and_maybe_check_sum(env, dbp, txn, arg, false);
}

static int UU() ptquery_and_maybe_check_op(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg, BOOL check) {
    int r;
    DB* db = *dbp;
    int rand_key = random();
    if (arg->bounded_update_range) {
        rand_key = rand_key % arg->n;
    }
    DBT key, val;
    memset(&val, 0, sizeof val);
    dbt_init(&key, &rand_key, sizeof rand_key);
    r = db->get(db, txn, &key, &val, 0);
    if (check) assert(r != DB_NOTFOUND);
    r = 0;
    return r;
}

static int UU() ptquery_op(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg) {
    return ptquery_and_maybe_check_op(env, dbp, txn, arg, TRUE);
}

static int UU() ptquery_op_no_check(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg) {
    return ptquery_and_maybe_check_op(env, dbp, txn, arg, FALSE);
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

static int UU()update_op2(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg) {
    int r;
    DB* db = *dbp;
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    int rand_key2;
    update_count++;
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    for (u_int32_t i = 0; i < 500; i++) {
        rand_key = random();
        if (arg->bounded_update_range) {
            rand_key = rand_key % (arg->n/2);
        }
        rand_key2 = arg->n - rand_key;
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
        CKERR(r);
        extra.u.d.diff = -1;
        r = db->update(
            db,
            txn,
            dbt_init(&key, &rand_key2, sizeof rand_key),
            dbt_init(&val, &extra, sizeof extra),
            0
            );
        CKERR(r);
    }
    return r;
}

static int UU()update_op(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg) {
    int r;
    DB* db = *dbp;
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    update_count++;
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = UPDATE_ADD_DIFF;
    extra.pad_bytes = 0;
    if (arg->update_pad_frequency) {
        if (update_count % (2*arg->update_pad_frequency) == update_count%arg->update_pad_frequency) {
            extra.pad_bytes = 100;
        }
        
    }
    for (u_int32_t i = 0; i < 1000; i++) {
        rand_key = random();
        if (arg->bounded_update_range) {
            rand_key = rand_key % arg->n;
        }
        extra.u.d.diff = random() % MAX_RANDOM_VAL;
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
        CKERR(r);
    }
    //
    // now put in one more to ensure that the sum stays 0
    //
    extra.u.d.diff = -curr_val_sum;
    rand_key = random();
    if (arg->bounded_update_range) {
        rand_key = rand_key % arg->n;
    }
    r = db->update(
        db,
        txn,
        dbt_init(&key, &rand_key, sizeof rand_key),
        dbt_init(&val, &extra, sizeof extra),
        0
        );
    CKERR(r);

    return r;
}

static int UU() update_with_history_op(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG arg) {
    assert(arg->bounded_update_range);
    assert(arg->update_history_buffer);
    int r;
    DB* db = *dbp;
    int curr_val_sum = 0;
    DBT key, val;
    int rand_key;
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    extra.type = UPDATE_WITH_HISTORY;
    update_count++;
    extra.pad_bytes = 0;
    if (arg->update_pad_frequency) {
        if (update_count % (2*arg->update_pad_frequency) != update_count%arg->update_pad_frequency) {
            extra.pad_bytes = 500;
        }
        
    }
    for (u_int32_t i = 0; i < 1000; i++) {
        rand_key = random() % arg->n;
        extra.u.h.new = random() % MAX_RANDOM_VAL;
        // just make every other value random
        if (i%2 == 0) {
            extra.u.h.new = -extra.u.h.new;
        }
        curr_val_sum += extra.u.h.new;
        extra.u.h.expected = arg->update_history_buffer[rand_key];
        arg->update_history_buffer[rand_key] = extra.u.h.new;
        r = db->update(
            db,
            txn,
            dbt_init(&key, &rand_key, sizeof rand_key),
            dbt_init(&val, &extra, sizeof extra),
            0
            );
        CKERR(r);
    }
    //
    // now put in one more to ensure that the sum stays 0
    //
    extra.u.h.new = -curr_val_sum;
    rand_key = random();
    if (arg->bounded_update_range) {
        rand_key = rand_key % arg->n;
    }
    extra.u.h.expected = arg->update_history_buffer[rand_key];
    arg->update_history_buffer[rand_key] = extra.u.h.new;
    r = db->update(
        db,
        txn,
        dbt_init(&key, &rand_key, sizeof rand_key),
        dbt_init(&val, &extra, sizeof extra),
        0
        );
    CKERR(r);

    return r;
}

static int UU() update_broadcast_op(DB_ENV *UU(env), DB **dbp, DB_TXN *txn, ARG UU(arg)) {
    struct update_op_extra extra;
    memset(&extra, 0, sizeof(extra));
    DB* db = *dbp;
    extra.type = UPDATE_NEGATE;
    extra.pad_bytes = 0;
    DBT val;
    int r = db->update_broadcast(db, txn, dbt_init(&val, &extra, sizeof extra), 0);
    CKERR(r);
    return r;
}

static int UU() remove_and_recreate_me(DB_ENV *env, DB **dbp, DB_TXN *UU(txn), ARG UU(arg)) {
    int r;
    r = (*dbp)->close(*dbp, 0); CKERR(r);
    
    r = env->dbremove(env, null_txn, "main", NULL, 0);  
    CKERR(r);
    
    r = db_create(dbp, env, 0);
    assert(r == 0);
    r = (*dbp)->open(*dbp, null_txn, "main", NULL, DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    return 0;
}

struct test_time_extra {
    int num_seconds;
    bool crash_at_end;
};

static void *test_time(void *arg) {
    struct test_time_extra* tte = arg;
    int num_seconds = tte->num_seconds;
    
    //
    // if num_Seconds is set to 0, run indefinitely
    //
    if (num_seconds != 0) {
	if (verbose) printf("Sleeping for %d seconds\n", num_seconds);
        usleep(num_seconds*1000*1000);
        if (verbose) printf("should now end test\n");
        __sync_bool_compare_and_swap(&run_test, true, false); // make this atomic to make valgrind --tool=drd happy.
        if (tte->crash_at_end) {
            toku_hard_crash_on_purpose();
        }
    }
    return arg;
}

static int run_workers(struct arg *thread_args, int num_threads, u_int32_t num_seconds, bool crash_at_end) {
    int r;
    toku_pthread_mutex_t mutex;
    toku_pthread_mutex_init(&mutex, NULL);
    struct rwlock rwlock;
    rwlock_init(&rwlock);
    toku_pthread_t tids[num_threads];
    toku_pthread_t time_tid;
    struct test_time_extra tte;
    tte.num_seconds = num_seconds;
    tte.crash_at_end = crash_at_end;
    run_test = true;
    for (int i = 0; i < num_threads; ++i) {
        thread_args[i].broadcast_lock = &rwlock;
        thread_args[i].broadcast_lock_mutex = &mutex;
        CHK(toku_pthread_create(&tids[i], NULL, worker, &thread_args[i]));
    }
    CHK(toku_pthread_create(&time_tid, NULL, test_time, &tte));

    void *ret;
    r = toku_pthread_join(time_tid, &ret); assert_zero(r);
    for (int i = 0; i < num_threads; ++i) {
        r = toku_pthread_join(tids[i], &ret); assert_zero(r);
    }
    rwlock_destroy(&rwlock);
    if (verbose) printf("ending test, pthreads have joined\n");
    toku_pthread_mutex_destroy(&mutex);
    return r;
}


static int create_table(DB_ENV **env_res, DB **db_res,
                        int (*bt_compare)(DB *, const DBT *, const DBT *),
                        u_int32_t cachesize,
                        u_int32_t checkpointing_period,
                        u_int32_t pagesize,
                        u_int32_t readpagesize) {
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r=env->set_default_bt_compare(env, bt_compare); CKERR(r);
    r = env->set_cachesize(env, 0, cachesize, 1); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, checkpointing_period);
    CKERR(r);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, pagesize);
    assert(r == 0);
    r = db->set_readpagesize(db, readpagesize);
    assert(r == 0);
    r = db->open(db, null_txn, "main", NULL, DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    *env_res = env;
    *db_res = db;
    return r;
}

static int fill_table_from_fun(DB *db, int num_elements, int max_bufsz,
                               void (*callback)(int idx, void *extra,
                                                void *key, int *keysz,
                                                void *val, int *valsz),
                               void *extra) {
    int r = 0;
    for (long i = 0; i < num_elements; ++i) {
        char keybuf[max_bufsz], valbuf[max_bufsz];
        int keysz, valsz;
        callback(i, extra, keybuf, &keysz, valbuf, &valsz);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, keybuf, keysz), dbt_init(&val, valbuf, valsz), 0);
        assert(r == 0);
    }
    return r;
}

static void int_element_callback(int idx, void *UU(extra), void *keyv, int *keysz, void *valv, int *valsz) {
    int *key = keyv, *val = valv;
    *key = idx;
    *val = idx;
    *keysz = sizeof(int);
    *valsz = sizeof(int);
}

static int fill_table_with_ints(DB *db, int num_elements) __attribute__((unused));
static int fill_table_with_ints(DB *db, int num_elements) {
    return fill_table_from_fun(db, num_elements, sizeof(int), int_element_callback, NULL);
}

static void zero_element_callback(int idx, void *UU(extra), void *keyv, int *keysz, void *valv, int *valsz) {
    int *key = keyv, *val = valv;
    *key = idx;
    *val = 0;
    *keysz = sizeof(int);
    *valsz = sizeof(int);
}

static int fill_table_with_zeroes(DB *db, int num_elements) {
    return fill_table_from_fun(db, num_elements, sizeof(int), zero_element_callback, NULL);
}

static int fill_table_from_array(DB *db, int num_elements, void *array, size_t element_size) __attribute__((unused));
static int fill_table_from_array(DB *db, int num_elements, void *array, size_t element_size) {
    int r = 0;
    char *a = array;
    for (char *p = a; p < a + num_elements * element_size; p += element_size) {
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, p, element_size), dbt_init(&val, p, element_size), 0);
        assert(r == 0);
    }
    return r;
}

static int open_table(DB_ENV **env_res, DB **db_res,
                      int (*bt_compare)(DB *, const DBT *, const DBT *),
                      u_int64_t cachesize,
                      u_int32_t checkpointing_period,
                      test_update_callback_f f) {
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_redzone(env, 0); CKERR(r);
    r=env->set_default_bt_compare(env, bt_compare); CKERR(r);
    env->set_update(env, f);
    // set the cache size to 10MB
    r = env->set_cachesize(env, cachesize / (1 << 30), cachesize % (1 << 30), 1); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
    r=env->open(env, ENVDIR, DB_RECOVER|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->checkpointing_set_period(env, checkpointing_period);
    CKERR(r);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->open(db, null_txn, "main", NULL, DB_BTREE, 0, 0666);
    assert(r == 0);

    *env_res = env;
    *db_res = db;
    return r;
}

static int close_table(DB_ENV *env, DB *db) {
    int r;
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
    return r;
}

struct cli_args {
    int num_elements;
    int time_of_test;
    int node_size;
    int basement_node_size;
    u_int64_t cachetable_size;
    bool only_create;
    bool only_stress;
    int checkpointing_period;
    int update_broadcast_period_ms;
    int num_ptquery_threads;
    test_update_callback_f update_function;
    bool do_test_and_crash;
    bool do_recover;
};

static const struct cli_args DEFAULT_ARGS = {
    .num_elements = 150000,
    .time_of_test = 180,
    .node_size = 4096,
    .basement_node_size = 1024,
    .cachetable_size = 300000,
    .only_create = false,
    .only_stress = false,
    .checkpointing_period = 10,
    .update_broadcast_period_ms = 2000,
    .num_ptquery_threads = 1,
    .update_function = update_op_callback,
    .do_test_and_crash = false,
    .do_recover = false
};

static inline void parse_stress_test_args (int argc, char *const argv[], struct cli_args *args) {
    const char *argv0=argv[0];
    while (argc>1) {
        int resultcode=0;
        if (strcmp(argv[1], "-v")==0) {
            verbose++;
        } 
        else if (strcmp(argv[1], "-q")==0) {
            verbose=0;
        } 
        else if (strcmp(argv[1], "-h")==0) {
        do_usage:
            fprintf(stderr, "Usage:\n%s [-h|-v|-q] [OPTIONS] [--only_create|--only_stress]\n", argv0);
            fprintf(stderr, "OPTIONS are among:\n");
            fprintf(stderr, "\t--num_elements             INT (default %d)\n", DEFAULT_ARGS.num_elements);
            fprintf(stderr, "\t--num_seconds              INT (default %ds)\n", DEFAULT_ARGS.time_of_test);
            fprintf(stderr, "\t--node_size                INT (default %d bytes)\n", DEFAULT_ARGS.node_size);
            fprintf(stderr, "\t--basement_node_size       INT (default %d bytes)\n", DEFAULT_ARGS.basement_node_size);
            fprintf(stderr, "\t--cachetable_size          INT (default %ld bytes)\n", DEFAULT_ARGS.cachetable_size);
            fprintf(stderr, "\t--checkpointing_period     INT (default %ds)\n",      DEFAULT_ARGS.checkpointing_period);
            fprintf(stderr, "\t--update_broadcast_period  INT (default %dms)\n",     DEFAULT_ARGS.update_broadcast_period_ms);
            fprintf(stderr, "\t--num_ptquery_threads      INT (default %d threads)\n", DEFAULT_ARGS.num_ptquery_threads);
            exit(resultcode);
        }
        else if (strcmp(argv[1], "--num_elements") == 0) {
            argc--;
            argv++;
            args->num_elements = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_seconds") == 0) {
            argc--;
            argv++;
            args->time_of_test = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--node_size") == 0) {
            argc--;
            argv++;
            args->node_size = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--basement_node_size") == 0) {
            argc--;
            argv++;
            args->basement_node_size = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--cachetable_size") == 0) {
            argc--;
            argv++;
            args->cachetable_size = strtoll(argv[1], NULL, 0);
        }
        else if (strcmp(argv[1], "--checkpointing_period") == 0) {
            argc--;
            argv++;
            args->checkpointing_period = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--update_broadcast_period") == 0) {
            argc--;
            argv++;
            args->update_broadcast_period_ms = atoi(argv[1]);
        }
        else if (strcmp(argv[1], "--num_ptquery_threads") == 0) {
            argc--;
            argv++;
            args->num_ptquery_threads = atoi(argv[1]);
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

static void
stress_test_main(struct cli_args *args)
{
    DB_ENV* env = NULL;
    DB* db = NULL;
    if (!args->only_stress) {
        create_table(
            &env,
            &db,
            int_dbt_cmp,
            args->cachetable_size,
            args->checkpointing_period,
            args->node_size,
            args->basement_node_size
            );
        CHK(fill_table_with_zeroes(db, args->num_elements));
        CHK(close_table(env, db));
    }
    if (!args->only_create) {
        CHK(open_table(&env,
                       &db,
                       int_dbt_cmp,
                       args->cachetable_size, //cachetable size
                       args->checkpointing_period, // checkpoint period
                       args->update_function));
        stress_table(env, &db, args);
        CHK(close_table(env, db));
    }
}

static void
UU() stress_recover(struct cli_args *args) {
    DB_ENV* env = NULL;
    DB* db = NULL;
    CHK(open_table(&env,
                   &db,
                   int_dbt_cmp,
                   args->cachetable_size, //cachetable size
                   args->checkpointing_period, // checkpoint period
                   args->update_function));

    DB_TXN* txn = NULL;    
    struct arg recover_args;
    arg_init(&recover_args, args->num_elements, &db, env);
    int r = env->txn_begin(env, 0, &txn, recover_args.txn_type);
    CKERR(r);
    r = scan_op_and_maybe_check_sum(env, &db, txn, &recover_args, true);
    CKERR(r);
    CHK(txn->commit(txn,0));
    CHK(close_table(env, db));
}

#endif
