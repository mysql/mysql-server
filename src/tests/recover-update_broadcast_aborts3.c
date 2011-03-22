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
    unsigned int *k, *ov, v;
    assert(key->size == sizeof(*k));
    k = key->data;
    assert(old_val->size == sizeof(*ov));
    ov = old_val->data;
    assert(extra->size == 0);
    v = _u(*ov, _e(*k));

    if (should_update(*k)) {
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
        r = CHK(db->put(db, txn, keyp, valp, 0));
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db) {
    DBT extra;
    DBT *extrap = dbt_init(&extra, NULL, 0);
    int r = CHK(db->update_broadcast(db, txn, extrap, 0));
    return r;
}

DB_ENV *env;
DB *db;

static void checkpoint_callback_1(void * extra) {
    assert(extra == NULL);
    IN_TXN_ABORT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db));
        });
}

static void run_test(void)
{

    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    db_env_set_checkpoint_callback(checkpoint_callback_1, NULL);
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));

            CHK(do_inserts(txn_1, db));
        });

    CHK(env->txn_checkpoint(env, 0, 0, 0));

    toku_hard_crash_on_purpose();
}

static int verify_unchanged(void)
{
    int r = 0;
    DBT key, val;
    unsigned int i, *vp;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            for (i = 0; i < NUM_KEYS; ++i) {
                r = CHK(db->get(db, txn_1, keyp, valp, 0));
                assert(val.size == sizeof(*vp));
                vp = val.data;
                assert(*vp == _v(i));
            }
        });

    return r;
}

static void run_recover(void)
{
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    CHK(env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666));
    CHK(verify_unchanged());
    CHK(db->close(db, 0));
    CHK(env->close(env, 0));
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
