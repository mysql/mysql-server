/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This test verifies that db->dbremove and db->truncate can be called
// during a call to db->hot_optimize without causing problems.  See
// #4356.

#include "test.h"

const size_t VALSIZE = 100;
// If a full leaf is 4 MB and a full internal node has 16 children, then
// a height 1 tree can have at most 64 MB of data (ish).  100 MB should
// easily be enough to force a height 2 tree, which should be enough for
// this test.
const int NUM_ROWS = 1024 * 1024;

static DB_ENV *env;
static DB *db;

static void
setup(void)
{
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);
    CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = db_create(&db, env, 0);
    CKERR(r);
    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0);
        CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
        r = txn->commit(txn, 0);
        CKERR(r);
    }
    if (verbose) { printf("Inserting data.\n"); }
    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0);
        CKERR(r);
        {
            int k;
            char v[VALSIZE];
            memset(v, 0, sizeof v);
            DBT key, val;
            dbt_init(&key, &k, sizeof k);
            dbt_init(&val, v, sizeof v);
            for (k = 0; k < NUM_ROWS; ++k) {
                r = db->put(db, txn, &key, &val, 0);
                CKERR(r);
            }
        }
        r = txn->commit(txn, 0);
        CKERR(r);
    }
}

static void
finish(void)
{
    int r;
    r = db->close(db, 0);
    CKERR(r);
    r = env->close(env, 0);
    CKERR(r);
}

typedef enum {
    REMOVE_4356,
    TRUNCATE_4356
} operation_4356_t;

struct progress_extra_4356 {
    operation_4356_t op;
    bool ran_operation;
};

static int
progress_4356(void *extra, float progress)
{
    int r = 0;
    struct progress_extra_4356 *e = extra;
    if (!e->ran_operation && progress > 0.5) {
        if (e->op == REMOVE_4356) {
            DB_TXN *txn;
            r = env->txn_begin(env, 0, &txn, 0);
            CKERR(r);
            if (verbose) { printf("Running remove.\n"); }
            r = env->dbremove(env, txn, "foo.db", NULL, 0);
            CKERR2(r, EINVAL);  // cannot remove a db with an open handle
            if (verbose) { printf("Completed remove.\n"); }
            r = txn->abort(txn);
            CKERR(r);
        } else if (e->op == TRUNCATE_4356) {
            DB_TXN *txn;
            r = env->txn_begin(env, 0, &txn, 0);
            CKERR(r);
            u_int32_t row_count = 0;
            if (verbose) { printf("Running truncate.\n"); }
            r = db->truncate(db, txn, &row_count, 0);
            CKERR2(r, DB_LOCK_NOTGRANTED);
            if (verbose) { printf("Completed truncate.\n"); }
            r = txn->abort(txn);
            CKERR(r);
        } else {
            assert(false);
        }
        e->ran_operation = true;
    }
    return r;
}

static void
run_test(operation_4356_t op)
{
    int r;
    setup();
    struct progress_extra_4356 extra;
    extra.op = op;
    extra.ran_operation = false;
    if (verbose) { printf("Running HOT.\n"); }
    r = db->hot_optimize(db, progress_4356, &extra);
    CKERR(r);
    if (verbose) { printf("Completed HOT.\n"); }
    finish();
}

int
test_main(int argc, char * const argv[])
{
    parse_args(argc, argv);
    if (verbose) { printf("Running remove test.\n"); }
    run_test(REMOVE_4356);
    if (verbose) { printf("Running truncate test.\n"); }
    run_test(TRUNCATE_4356);
    return 0;
}
