/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// test that an update doesn't infringe on other txns started with
// TXN_SNAPSHOT, when the update deletes elements

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const int to_delete[] = { 0, 1, 1, 1, 0, 0, 1, 0, 1, 0 };

static inline unsigned int _v(const unsigned int i) { return 10 - i; }

static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) {
    assert(extra->size == 0);

    set_val(NULL, set_extra);

    return 0;
}

static void setup (void) {
    { int chk_r = system("rm -rf " ENVDIR); CKERR(chk_r); }
    { int chk_r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
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
    for (i = 0; i < (sizeof(to_delete) / sizeof(to_delete[0])); ++i) {
        v = _v(i);
        r = db->put(db, txn, keyp, valp, 0); CKERR(r);
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, extra;
    unsigned int i;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *extrap = dbt_init(&extra, NULL, 0);
    for (i = 0; i < (sizeof(to_delete) / sizeof(to_delete[0])); ++i) {
        if (to_delete[i] == 1) {
            r = db->update(db, txn, keyp, extrap, 0); CKERR(r);
        }
    }
    return r;
}

static void chk_original(const unsigned int k, const unsigned int v) {
    assert(v == _v(k));
}

static int do_verify_results(DB_TXN *txn, DB *db, void (*check_val)(const unsigned int k, const unsigned int v), BOOL already_deleted) {
    int r = 0;
    DBT key, val;
    unsigned int i, *vp;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);
    for (i = 0; i < (sizeof(to_delete) / sizeof(to_delete[0])); ++i) {
        r = db->get(db, txn, keyp, valp, 0);
        if (already_deleted && to_delete[i]) {
            CKERR2(r, DB_NOTFOUND);
            r = 0;
        } else {
            CKERR(r);
            assert(val.size == sizeof(*vp));
            vp = cast_to_typeof(vp) val.data;
            check_val(i, *vp);
        }
    }
    return r;
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            { int chk_r = db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }

            { int chk_r = do_inserts(txn_1, db); CKERR(chk_r); }

            IN_TXN_COMMIT(env, txn_1, txn_11, 0, {
                    { int chk_r = do_verify_results(txn_11, db, chk_original, FALSE); CKERR(chk_r); }
                });
        });

    {
        DB_TXN *txn_2, *txn_3;
        { int chk_r = env->txn_begin(env, NULL, &txn_2, DB_TXN_SNAPSHOT); CKERR(chk_r); }
        { int chk_r = do_verify_results(txn_2, db, chk_original, FALSE); CKERR(chk_r); }
        { int chk_r = env->txn_begin(env, NULL, &txn_3, 0); CKERR(chk_r); }
        { int chk_r = do_updates(txn_3, db); CKERR(chk_r); }
        { int chk_r = do_verify_results(txn_2, db, chk_original, FALSE); CKERR(chk_r); }
        { int chk_r = do_verify_results(txn_3, db, chk_original, TRUE); CKERR(chk_r); }
        { int chk_r = txn_2->abort(txn_2); CKERR(chk_r); }
        { int chk_r = txn_3->abort(txn_3); CKERR(chk_r); }
    }

    IN_TXN_COMMIT(env, NULL, txn_4, 0, {
            { int chk_r = do_verify_results(txn_4, db, chk_original, FALSE); CKERR(chk_r); }
        });

    { int chk_r = db->close(db, 0); CKERR(chk_r); }

    cleanup();

    return 0;
}
