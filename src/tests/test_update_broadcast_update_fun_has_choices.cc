/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test that an update broadcast can change and delete different values,
// or do nothing

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const unsigned int NUM_KEYS = 1000;

static inline bool should_insert(const unsigned int i) { return i % 2 == 0; }
static inline bool should_update(const unsigned int i) { return i % 3 == 0; }
static inline bool should_delete(const unsigned int i) { return (i % 5 == 0) && (i % 3 != 0); }

static inline unsigned int _v(const unsigned int i) { return 10 - i; }
static inline unsigned int _e(const unsigned int i) { return i + 4; }
static inline unsigned int _u(const unsigned int v, const unsigned int e) { return v * v * e; }

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) {
    unsigned int *k, *ov, e, v;
    assert(key->size == sizeof(*k));
    CAST_FROM_VOIDP(k, key->data);
    assert(should_insert(*k));
    assert(old_val->size == sizeof(*ov));
    CAST_FROM_VOIDP(ov, old_val->data);
    assert(extra->size == 0);
    if (should_update(*k)) {
        e = _e(*k);
        v = _u(*ov, e);

        {
            DBT newval;
            set_val(dbt_init(&newval, &v, sizeof(v)), set_extra);
        }
    } else if (should_delete(*k)) {
        set_val(NULL, set_extra);
    }

    return 0;
}

static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static int do_inserts(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i, v;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, &v, sizeof(v));
    for (i = 0; i < NUM_KEYS; ++i) {
        if (should_insert(i)) {
            v = _v(i);
            r = db->put(db, txn, keyp, valp, 0); CKERR(r);
        }
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db, uint32_t flags) {
    DBT extra;
    DBT *extrap = dbt_init(&extra, NULL, 0);
    int r = db->update_broadcast(db, txn, extrap, flags); CKERR(r);
    return r;
}

static int do_verify_results(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i, *vp;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);
    for (i = 0; i < NUM_KEYS; ++i) {
        r = db->get(db, txn, keyp, valp, 0);
        if (!should_insert(i) || should_delete(i)) {
            CKERR2(r, DB_NOTFOUND);
            r = 0;
        } else if (should_insert(i)) {
            CKERR(r);
            assert(val.size == sizeof(*vp));
            CAST_FROM_VOIDP(vp, val.data);
            if (should_update(i)) {
                assert(*vp == _u(_v(i), _e(i)));
            } else {
                assert(*vp == _v(i));
            }
        }
    }
    return r;
}

static void run_test(bool is_resetting) {
    DB *db;
    uint32_t update_flags = is_resetting ? DB_IS_RESETTING_OP : 0;

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            { int chk_r = db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }

            { int chk_r = do_inserts(txn_1, db); CKERR(chk_r); }
        });

    IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            { int chk_r = do_updates(txn_2, db, update_flags); CKERR(chk_r); }
        });

    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            { int chk_r = do_verify_results(txn_3, db); CKERR(chk_r); }
        });

    { int chk_r = db->close(db, 0); CKERR(chk_r); }
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test(true);
    run_test(false);
    cleanup();

    return 0;
}
