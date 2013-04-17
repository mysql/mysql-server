#include "test.h"

// insert enough rows with a child txn to force a rollinclude log entry

static void
populate(DB_ENV *env, DB *db, int nrows) {
    int r;
    DB_TXN *parent = NULL;
    r = env->txn_begin(env, NULL, &parent, 0); assert_zero(r);

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, parent, &txn, 0); assert_zero(r);

    // populate
    for (int i = 0; i < nrows; i++) {
        int k = htonl(i);
        char kk[4096]; // 4 KB key
        memset(kk, 0, sizeof kk);
        memcpy(kk, &k, sizeof k);
        DBT key = { .data = &kk, .size = sizeof kk };
        DBT val = { .data = NULL, .size = 0 };
        r = db->put(db, txn, &key, &val, 0);
        assert_zero(r);
    }

    r = txn->commit(txn, 0); assert_zero(r);
    r = txn->commit(parent, 0); assert_zero(r);
}

static void
run_test(int nrows) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->open(env, ENVDIR, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "0.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert_zero(r);

    populate(env, db, nrows);

    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;
    int nrows = 1024; // = 4 MB / 4KB assumes 4 MB rollback nodes

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

    run_test(nrows);

    return 0;
}

