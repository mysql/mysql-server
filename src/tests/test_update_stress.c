// stress test for update.  10M 8-byte keys should be 2, maybe 3 levels of
// treeness, makes sure flushes work

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const unsigned int NUM_KEYS = 10000000;

static inline BOOL should_update(const unsigned int i) { return i % 3 == 0; }
static inline unsigned int _v(const unsigned int i) { return 10 - i; }
static inline unsigned int _e(const unsigned int i) { return i + 4; }
static inline unsigned int _u(const unsigned int v, const unsigned int e) { return v + 2 * e; }

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *old_val, const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                      void *set_extra),
                      void *set_extra) {
    unsigned int *k, *ov, *e, v;
    assert(key->size == sizeof(*k));
    k = key->data;
    assert(old_val->size == sizeof(*ov));
    ov = old_val->data;
    assert(extra->size == sizeof(*e));
    e = extra->data;
    v = _u(*ov, *e);

    {
        DBT newval;
        set_val(dbt_init(&newval, &v, sizeof(v)), set_extra);
    }

    return 0;
}

static int
int_cmp(DB *UU(db), const DBT *a, const DBT *b) {
    unsigned int *ap, *bp;
    assert(a->size == sizeof(*ap));
    ap = a->data;
    assert(b->size == sizeof(*bp));
    bp = b->data;
    return (*ap > *bp) - (*ap < *bp);
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    CHK(env->set_default_bt_compare(env, int_cmp));
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
    for (i = 0; i < NUM_KEYS; ++i) {
        v = _v(i);
        r = CHK(db->put(db, txn, keyp, valp, 0));
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

static int do_verify_results(DB_TXN *txn, DB *db, void (*check_val)(const unsigned int k, const unsigned int v)) {
    int r = 0;
    DBT key, val;
    unsigned int i, *vp;
    DBT *keyp = dbt_init(&key, &i, sizeof(i));
    DBT *valp = dbt_init(&val, NULL, 0);
    for (i = 0; i < NUM_KEYS; ++i) {
        r = CHK(db->get(db, txn, keyp, valp, 0));
        assert(val.size == sizeof(*vp));
        vp = val.data;
        check_val(i, *vp);
    }
    return r;
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));

            CHK(do_inserts(txn_1, db));
        });

    IN_TXN_ABORT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db));

            IN_TXN_COMMIT(env, txn_2, txn_21, 0, {
                    CHK(do_verify_results(txn_21, db, chk_updated));
                });
        });

    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(do_verify_results(txn_3, db, chk_original));
        });

    CHK(db->close(db, 0));

    cleanup();

    return 0;
}
