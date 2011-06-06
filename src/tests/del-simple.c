#include "test.h"

// verify that deletes on single dictionaries delete the correct key and create the correct lock

static void
verify_locked(DB_ENV *env, DB *db, int k) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);
    DBT key; dbt_init(&key, &k, sizeof k);
    r = db->del(db, txn, &key, DB_DELETE_ANY); assert(r == DB_LOCK_NOTGRANTED);
    r = txn->abort(txn); assert_zero(r);
}

static void
verify_empty(DB_ENV *env, DB *db) {
    int r;
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);
    int i;
    for (i = 0; ; i++) {
        DBT key; memset(&key, 0, sizeof key);
        DBT val; memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0)
            break;
    }
    assert_zero(i);
    r = cursor->c_close(cursor); assert_zero(r);
    r = txn->commit(txn, 0); assert_zero(r);
}

static void
verify_del(DB_ENV *env, DB *db, int nrows) {
    int r;
    DB_TXN *deltxn = NULL;
    r = env->txn_begin(env, NULL, &deltxn, 0); assert_zero(r);
    for (int i = 0; i < nrows; i++) {
        int k = htonl(i);
        DBT key; dbt_init(&key, &k, sizeof k);
        r = db->del(db, deltxn, &key, DB_DELETE_ANY); assert_zero(r);
        verify_locked(env, db, k);
    }
    r = deltxn->commit(deltxn, 0); assert_zero(r);
    verify_empty(env, db);
}

static void
test_del(int nrows) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->open(env, ENVDIR, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "test.db", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = htonl(i);
        int v = i;
        DBT key; dbt_init(&key, &k, sizeof k);
        DBT val; dbt_init(&val, &v, sizeof v);
        r = db->put(db, txn, &key, &val, 0); assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);

    verify_del(env, db, nrows);

    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;
    int nrows = 2;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(arg, "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
    }

    r = system("rm -rf " ENVDIR); assert_zero(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    test_del(nrows);

    return 0;
}

