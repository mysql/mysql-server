#include "test.h"

static int
put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_data = dest_data; src_key = src_key; src_data = src_data;

    lazy_assert(src_db != NULL && dest_db != NULL);

    dest_key->data = src_data->data;
    dest_key->size = src_data->size;
    dest_data->size = 0;
    
    return 0;
}

static void
run_test(void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_put(env, put_callback); assert_zero(r);

    r = env->open(env, ENVDIR, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *src_db = NULL;
    r = db_create(&src_db, env, 0); assert_zero(r);
    r = src_db->open(src_db, NULL, "0.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *dest_db = NULL;
    r = db_create(&dest_db, env, 0); assert_zero(r);
    r = dest_db->open(dest_db, NULL, "1.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DB_INDEXER *indexer = NULL;
    r = env->create_indexer(env, txn, &indexer, src_db, 1, &dest_db, NULL, 0); assert_zero(r);

    r = indexer->abort(indexer); assert_zero(r);

    r = txn->abort(txn); assert_zero(r);

    r = src_db->close(src_db, 0); assert_zero(r);
    r = dest_db->close(dest_db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;

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
    }

    r = system("rm -rf " ENVDIR); assert_zero(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    run_test();

    return 0;
}

