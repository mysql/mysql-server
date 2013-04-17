/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
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

DB* hot_db;
toku_mutex_t fops_lock;
toku_mutex_t hi_lock;
u_int32_t gid_count;
u_int8_t hi_gid[DB_GID_SIZE];


static int
hi_put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    lazy_assert(src_db != NULL && dest_db != NULL);

    if (dest_key->data) {
        toku_free(dest_key->data);
        dest_key->data = NULL;
    }
    if (dest_data->data) {
        toku_free(dest_data->data);
        dest_data->data = NULL;
    }
    dest_key->data = toku_xmemdup(src_key->data, src_key->size);
    dest_key->size = src_key->size;
    dest_data->data = toku_xmemdup(src_data->data, src_data->size);
    dest_data->size = src_data->size;
    
    return 0;
}

static int
hi_del_callback(DB *dest_db, DB *src_db, DBT *dest_key, const DBT *src_key, const DBT* UU(src_data)) {
    lazy_assert(src_db != NULL && dest_db != NULL);
    if (dest_key->data) {
        toku_free(dest_key->data);
        dest_key->data = NULL;
    }
    dest_key->data = toku_xmemdup(src_key->data, src_key->size);
    dest_key->size = src_key->size;
    
    return 0;
}


static int hi_inserts(DB_TXN* UU(txn), ARG arg, void* UU(operation_extra), void *stats_extra) {
    int r;
    DB_TXN* hi_txn = NULL;
    toku_mutex_lock(&fops_lock);
    DB_ENV* env = arg->env;
    DB* db = arg->dbp[0];
    u_int32_t flags[2];
    flags[0] = 0;
    flags[1] = 0;
    DBT dest_keys[2];
    DBT dest_vals[2];
    memset(dest_keys, 0, sizeof(dest_keys));
    memset(dest_vals, 0, sizeof(dest_vals));
    int i;
    r = env->txn_begin(env, NULL, &hi_txn, 0);
    CKERR(r);
    for (i = 0; i < 1000; i++) {
        DB* dbs[2];        
        toku_mutex_lock(&hi_lock);
        dbs[0] = db;
        dbs[1] = hot_db;
        int num_dbs = hot_db ? 2 : 1;
        // do a random insertion
        int rand_key = random() % arg->cli->num_elements;
        int rand_val = random();
        DBT key, val;
        dbt_init(&key, &rand_key, sizeof(rand_key)), 
        dbt_init(&val, &rand_val, sizeof(rand_val)), 
        r = env->put_multiple(
            env, 
            db, 
            hi_txn, 
            &key, 
            &val, 
            num_dbs, 
            dbs, 
            dest_keys, 
            dest_vals, 
            flags
            );
        toku_mutex_unlock(&hi_lock);
        if (r != 0) {
            goto cleanup;
        }
    }
cleanup:
    if (dest_keys[0].data) {
        toku_free(dest_keys[0].data);
    }
    if (dest_keys[1].data) {
        toku_free(dest_keys[1].data);
    }
    if (dest_vals[0].data) {
        toku_free(dest_vals[0].data);
    }
    if (dest_vals[1].data) {
        toku_free(dest_vals[1].data);
    }
    increment_counter(stats_extra, PUTS, i);
    gid_count++;
    u_int32_t *hi_gid_count_p = cast_to_typeof(hi_gid_count_p) hi_gid;  // make gcc --happy about -Wstrict-aliasing
    *hi_gid_count_p = gid_count;
    int rr = hi_txn->prepare(hi_txn, hi_gid);
    CKERR(rr);
    if (r || (random() % 2)) {
        rr = hi_txn->abort(hi_txn);
        CKERR(rr);
    }
    else {
        rr = hi_txn->commit(hi_txn, 0);
        CKERR(rr);
    }
    toku_mutex_unlock(&fops_lock);
    return r;
}

static int hi_create_index(DB_TXN* UU(txn), ARG arg, void* UU(operation_extra), void* UU(stats_extra)) {
    int r;
    DB_TXN* hi_txn = NULL;
    DB_ENV* env = arg->env;
    DB* db = arg->dbp[0];
    DB_INDEXER* indexer = NULL;
    r = env->txn_begin(env, NULL, &hi_txn, 0);
    CKERR(r);
    toku_mutex_lock(&hi_lock);
    assert(hot_db == NULL);
    db_create(&hot_db, env, 0);
    CKERR(r);
    r = hot_db->set_flags(hot_db, 0);
    CKERR(r);
    r = hot_db->set_pagesize(hot_db, arg->cli->env_args.node_size);
    CKERR(r);
    r = hot_db->set_readpagesize(hot_db, arg->cli->env_args.basement_node_size);
    CKERR(r);
    r = hot_db->open(hot_db, NULL, "hotindex_db", NULL, DB_BTREE, DB_CREATE | DB_IS_HOT_INDEX, 0666);
    CKERR(r);
    u_int32_t db_flags = 0;
    u_int32_t indexer_flags = 0;
    
    r = env->create_indexer(
        env,
        hi_txn,
        &indexer,
        arg->dbp[0],
        1,
        &hot_db,
        &db_flags,
        indexer_flags
        );
    CKERR(r);
    toku_mutex_unlock(&hi_lock);
    
    r = indexer->build(indexer);
    CKERR(r);

    toku_mutex_lock(&hi_lock);
    r = indexer->close(indexer);
    CKERR(r);
    toku_mutex_unlock(&hi_lock);

    r = hi_txn->commit(hi_txn, 0);
    hi_txn = NULL;
    CKERR(r);

    // now do a scan to make sure hot index is good
    DB_TXN* scan_txn = NULL;
    DBC* main_cursor = NULL;
    DBC* hi_cursor = NULL;
    r = env->txn_begin(env, NULL, &scan_txn, DB_TXN_SNAPSHOT);
    CKERR(r);
    r = db->cursor(db, scan_txn, &main_cursor, 0);
    CKERR(r);
    r = db->cursor(hot_db, scan_txn, &hi_cursor, 0);
    CKERR(r);
    DBT key1, key2, val1, val2;
    memset(&key1, 0, sizeof key1);
    memset(&val1, 0, sizeof val1);
    memset(&key2, 0, sizeof key2);
    memset(&val2, 0, sizeof val2);
    while(r != DB_NOTFOUND) {
        // get next from both cursors and assert they are equal
        int r1 = main_cursor->c_get(
            main_cursor, 
            &key1, 
            &val1, 
            DB_NEXT
            );
        int r2 = hi_cursor->c_get(
            hi_cursor, 
            &key2, 
            &val2, 
            DB_NEXT
            );
        assert(r1 == r2);
        r = r1;
        if (r != DB_NOTFOUND) {
            assert(key1.size == key2.size);
            assert(val1.size == val2.size);
            assert(memcmp(key1.data, key2.data, key1.size) == 0);
            assert(memcmp(val1.data, val2.data, val1.size) == 0);
        }
    }
    CKERR2(r, DB_NOTFOUND);
    r = main_cursor->c_close(main_cursor);
    r = hi_cursor->c_close(hi_cursor);
    CKERR(r);
    r = scan_txn->commit(scan_txn, 0);
    CKERR(r);

    // grab lock and close hot_db, set it to NULL
    toku_mutex_lock(&hi_lock);
    r = hot_db->close(hot_db, 0);
    CKERR(r);
    hot_db = NULL;
    toku_mutex_unlock(&hi_lock);

    toku_mutex_lock(&fops_lock);
    r = env->dbremove(env, NULL, "hotindex_db", NULL, 0);
    toku_mutex_unlock(&fops_lock);
    CKERR(r);
    return 0;
}

//
// purpose of this stress test is to do a bunch of splitting and merging
// and run db->verify periodically to make sure the db is in a 
// a good state
//
static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = 2;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }
    myargs[0].operation = hi_inserts;
    myargs[1].operation = hi_create_index;

    run_workers(myargs, num_threads, cli_args->time_of_test, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    gid_count = 0;
    memset(hi_gid, 0, sizeof(hi_gid));
    toku_mutex_init(&hi_lock, NULL);
    toku_mutex_init(&fops_lock, NULL);
    hot_db = NULL;
    struct cli_args args = get_default_args();
    // let's make default checkpointing period really slow
    args.num_ptquery_threads = 0;
    parse_stress_test_args(argc, argv, &args);
    args.num_DBs = 1;
    args.crash_on_operation_failure = FALSE;
    args.env_args.generate_del_callback = hi_del_callback;
    args.env_args.generate_put_callback = hi_put_callback;
    stress_test_main(&args);
    toku_mutex_destroy(&hi_lock);
    toku_mutex_destroy(&fops_lock);
    return 0;
}
