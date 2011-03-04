// test that an update, if called on a nonexistent key, will call back
// into update_function with the right arguments, and allows it to set a
// new value

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const int to_insert[] = { 0, 0, 1, 1, 1, 0, 0, 1, 1, 1 };
const int to_update[] = { 0, 1, 1, 1, 0, 0, 1, 0, 1, 0 };

static inline BOOL should_insert(const unsigned int i) { return to_insert[i]; }
static inline BOOL should_update(const unsigned int i) { return to_update[i]; }
static inline unsigned int _v(const unsigned int i) { return 10 - i; }
static inline unsigned int _e(const unsigned int i) { return i + 4; }
static inline unsigned int _u(const unsigned int v, const unsigned int e) { return v * v * e; }

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) {
    unsigned int *k, *ov, *e, v;
    assert(key->size == sizeof(*k));
    k = key->data;
    assert(extra->size == sizeof(*e));
    e = extra->data;
    if (!should_insert(*k)) {
        assert(old_val == NULL);
        v = _u(_v(*k), *e);
    } else {
        assert(old_val->size == sizeof(*ov));
        ov = old_val->data;
        v = _u(*ov, *e);
    }

    {
        DBT newval;
        set_val(dbt_init(&newval, &v, sizeof(v)), set_extra);
    }

    return 0;
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static int do_inserts(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, val;
    unsigned int i, v;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, &v, sizeof(v));
    for (i = 0; i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
        if (should_insert(i)) {
            v = _v(i);
            r = CHK(db->put(db, txn, keyp, valp, 0));
        }
    }
    return r;
}

static int do_updates(DB_TXN *txn, DB *db) {
    int r = 0;
    DBT key, extra;
    unsigned int i, e;
    const DBT *keyp = dbt_init(&key, &i, sizeof(i));
    const DBT *extrap = dbt_init(&extra, &e, sizeof(e));
    for (i = 0; i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
        if (should_update(i)) {
            e = _e(i);  // E I O
            r = CHK(db->update(db, txn, keyp, extrap, 0));
        }
    }
    return r;
}

static void chk_updated(const unsigned int k, const unsigned int v) {
    if (should_update(k)) {
        assert(v == _u(_v(k), _e(k)));
    } else {
        assert(v == _v(k));
    }
}

static void chk_original(const unsigned int k, const unsigned int v) {
    assert(v == _v(k));
}

static int do_verify_results(DB_TXN *txn, DB *db, void (*check_val)(const unsigned int k, const unsigned int v), BOOL after_update) {
    int r = 0;
    DBT key, val;
    unsigned int i, v;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, &v, sizeof(v));
    for (i = 0; i < (sizeof(to_insert) / sizeof(to_insert[0])); ++i) {
        r = db->get(db, txn, keyp, valp, 0);
        if (should_insert(i) || (after_update && should_update(i))) {
            CHK(r);
            assert(val.size == sizeof(v));
            v = *(unsigned int *) val.data;

            check_val(i, v);
        } else {
            CHK2(r, DB_NOTFOUND);
            r = 0;
        }
    }
    return r;
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));

            CHK(do_inserts(txn_1, db));

            IN_TXN_ABORT(env, txn_1, txn_11, 0, {
                    CHK(do_verify_results(txn_11, db, chk_original, FALSE));
                });
        });

    IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db));
        });

    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(do_verify_results(txn_3, db, chk_updated, TRUE));
        });

    CHK(db->close(db, 0));

    cleanup();

    return 0;
}
