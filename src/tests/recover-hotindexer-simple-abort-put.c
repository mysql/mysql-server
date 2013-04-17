/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include "test.h"

static int
put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_data = dest_data; src_key = src_key; src_data = src_data;

    lazy_assert(src_db != NULL && dest_db != NULL);

    dest_key->data = toku_xmemdup(src_data->data, src_data->size);
    dest_key->size = src_data->size;
    dest_data->size = 0;
    
    return 0;
}

int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

static void
run_test(void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_put(env, put_callback); assert_zero(r);

    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));

    DB *src_db = NULL;
    r = db_create(&src_db, env, 0); assert_zero(r);
    r = src_db->open(src_db, NULL, "0.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *dest_db = NULL;
    r = db_create(&dest_db, env, 0); assert_zero(r);
    r = dest_db->open(dest_db, NULL, "1.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_TXN* index_txn = NULL;
    r = env->txn_begin(env, NULL, &index_txn , 0); assert_zero(r);
    DB_TXN* put_txn = NULL;
    r = env->txn_begin(env, NULL, &put_txn , 0); assert_zero(r);

    DBT key,data;
    r = src_db->put(
        src_db, 
        put_txn,
        dbt_init(&key,  "hello", 6),
        dbt_init(&data, "there", 6),
        0
        );

    DB_INDEXER *indexer = NULL;
    r = env->create_indexer(env, index_txn, &indexer, src_db, 1, &dest_db, NULL, 0); assert_zero(r);
    r = indexer->build(indexer); assert_zero(r);        
    r = indexer->close(indexer); assert_zero(r);
    r = index_txn->abort(index_txn); assert_zero(r);

    r = env->txn_checkpoint(env, 0, 0, 0);
    assert_zero(r);

    toku_hard_crash_on_purpose();
}

static void
run_recover(void) {
    DB_ENV *env;
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    CHK(env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(env->close(env, 0));
}

int
test_main(int argc, char * const argv[]) {
    BOOL do_test = FALSE;
    BOOL do_recover = FALSE;

    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "--test") == 0) {
            do_test = TRUE;
            continue;
        }
        if (strcmp(arg, "--recover") == 0) {
            do_recover = TRUE;
            continue;
        }
    }

    if (do_test) {
        int r;
        r = system("rm -rf " ENVDIR); assert_zero(r);
        r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);
        run_test();
    }
    if (do_recover) {
        run_recover();
    }

    return 0;
}

