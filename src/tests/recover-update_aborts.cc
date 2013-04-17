/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"

// verify recovery of an update log entry which changes values at keys

static const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
static const unsigned int NUM_KEYS = 100;

static inline BOOL should_update(const unsigned int k) { return k % 3 == 0; }

static inline unsigned int _v(const unsigned int k) { return 10 - k; }
static inline unsigned int _e(const unsigned int k) { return k + 4; }
static inline unsigned int _u(const unsigned int v, const unsigned int e) { return v * v * e; }

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra)
{
    unsigned int *k, *ov, *e, v;
    assert(key->size == sizeof(*k));
    CAST_FROM_VOIDP(k, key->data);
    assert(old_val->size == sizeof(*ov));
    CAST_FROM_VOIDP(ov, old_val->data);
    assert(extra->size == sizeof(*e));
    CAST_FROM_VOIDP(e, extra->data);
    v = _u(*ov, *e);

    {
        DBT newval;
        set_val(dbt_init(&newval, &v, sizeof(v)), set_extra);
    }

    return 0;
}

static int do_inserts(DB_TXN *txn, DB *db)
{
    int r = 0;
    DBT key, val;
    unsigned int i, v;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, &v, sizeof(v));
    for (i = 0; i < NUM_KEYS; ++i) {
        v = _v(i);
        r = db->put(db, txn, keyp, valp, 0);
        CKERR(r);
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, extra;
    unsigned int i, e;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *extrap = dbt_init(&extra, &e, sizeof(e));
    for (i = 0; i < NUM_KEYS; ++i) {
        if (should_update(i)) {
            e = _e(i);
            r = db->update(db, txn, keyp, extrap, 0);
            CKERR(r);
        }
    }
    return r;
}

static void run_test(void)
{
    DB_ENV *env;
    DB *db;

    { int chk_r = system("rm -rf " ENVDIR); CKERR(chk_r); }
    { int chk_r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            { int chk_r = db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }

            { int chk_r = do_inserts(txn_1, db); CKERR(chk_r); }
        });

    { int chk_r = env->txn_checkpoint(env, 0, 0, 0); CKERR(chk_r); }

    IN_TXN_ABORT(env, NULL, txn_2, 0, {
            { int chk_r = do_updates(txn_2, db); CKERR(chk_r); }
        });

    toku_hard_crash_on_purpose();
}

static int verify_unchanged(DB_ENV *env, DB *db)
{
    int r = 0;
    DBT key, val;
    unsigned int i, *vp;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            for (i = 0; i < NUM_KEYS; ++i) {
                r = db->get(db, txn_1, keyp, valp, 0);
                CKERR(r);
                assert(val.size == sizeof(*vp));
                CAST_FROM_VOIDP(vp, val.data);
                assert(*vp == _v(i));
            }
        });

    return r;
}

static void run_recover(void)
{
    DB_ENV *env;
    DB *db;

    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666); CKERR(chk_r); }
    { int chk_r = verify_unchanged(env, db); CKERR(chk_r); }
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static int usage(void)
{
    return 1;
}

int test_main(int argc, char * const argv[])
{
    BOOL do_test = FALSE;
    BOOL do_recover = FALSE;

    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose--;
            if (verbose < 0)
                verbose = 0;
            continue;
        }
        if (strcmp(arg, "--test") == 0) {
            do_test = TRUE;
            continue;
        }
        if (strcmp(arg, "--recover") == 0) {
            do_recover = TRUE;
            continue;
        }
        if (strcmp(arg, "--help") == 0) {
            return usage();
        }
    }

    if (do_test) {
        run_test();
    }
    if (do_recover) {
        run_recover();
    }

    return 0;
}
