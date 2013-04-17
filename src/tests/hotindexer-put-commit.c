#include "test.h"
#include "ydb.h"
#include "toku_pthread.h"

// this test reproduces the rollback log corruption that occurs when hot indexing runs concurrent with a long commit.
// the concurrent operation occurs when the commit periodically releases the ydb lock which allows the hot indexer
// to run.  the hot indexer erroneously append to the rollback log that is in the process of being committed.

static int
put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_data = dest_data; src_key = src_key; src_data = src_data;

    lazy_assert(src_db != NULL && dest_db != NULL);

    if (dest_key->flags == DB_DBT_REALLOC) {
        dest_key->data = toku_realloc(dest_key->data, src_key->size);
        memcpy(dest_key->data, src_key->data, src_key->size);
        dest_key->size = src_key->size;
    }
    if (dest_data->flags == DB_DBT_REALLOC) {
        dest_data->data = toku_realloc(dest_data->data, src_data->size);
        memcpy(dest_data->data, src_data->data, src_data->size);
        dest_data->size = src_data->size;
    }
    
    return 0;
}

struct indexer_arg {
    DB_ENV *env;
    DB *src_db;
    int n_dest_db;
    DB **dest_db;
};

static void *
indexer_thread(void *arg) {
    struct indexer_arg *indexer_arg = (struct indexer_arg *) arg;
    DB_ENV *env = indexer_arg->env;
    int r;
    
    DB_TXN *indexer_txn = NULL;
    r = env->txn_begin(env, NULL, &indexer_txn, 0); assert_zero(r);
        
    DB_INDEXER *indexer = NULL;
    r = env->create_indexer(env, indexer_txn, &indexer, indexer_arg->src_db, indexer_arg->n_dest_db, indexer_arg->dest_db, NULL, 0); assert_zero(r);
    
    if (verbose) fprintf(stderr, "build start\n");
    r = indexer->build(indexer); assert_zero(r);
    if (verbose) fprintf(stderr, "build end\n");
        
    r = indexer->close(indexer); assert_zero(r);
        
    r = indexer_txn->commit(indexer_txn, 0); assert_zero(r);

    return arg;
}

static void
verify_full(DB_ENV *env, DB *db, int n) {
    int r;
    
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0); assert_zero(r);

    DBC *cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0); assert_zero(r);

    int i = 0;
    DBT key; dbt_init_realloc(&key);
    DBT val; dbt_init_realloc(&val);
    while (1) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r == DB_NOTFOUND)
            break;
        int k;
        assert(key.size == sizeof k);
        memcpy(&k, key.data, key.size);
        assert(k == (int) htonl(i));
        int v;
        assert(val.size == sizeof v);
        memcpy(&v, val.data, val.size);
        assert(v == i);
        i++;
    }
    assert(i == n);
    toku_free(key.data);
    toku_free(val.data);
    
    r = cursor->c_close(cursor); assert_zero(r);

    r = txn->commit(txn, 0); assert_zero(r);
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

    // insert some
    int n = 246723;
    for (int i = 0; i < n; i++) {
        int k = htonl(i);
        int v = i;
        DBT key; dbt_init(&key, &k, sizeof k);
        DBT val; dbt_init(&val, &v, sizeof v);
        r = src_db->put(src_db, txn, &key, &val, 0); assert_zero(r);
    }

    // run the indexer
    struct indexer_arg indexer_arg = { env, src_db, 1, &dest_db };
    toku_pthread_t pid;
    r = toku_pthread_create(&pid, NULL, indexer_thread, &indexer_arg); assert_zero(r);

    if (verbose) fprintf(stderr, "commit start\n");
    r = txn->commit(txn, 0); assert_zero(r);
    if (verbose) fprintf(stderr, "commit end\n");

    void *ret;
    r = toku_pthread_join(pid, &ret); assert_zero(r);

    verify_full(env, src_db, n);
    verify_full(env, dest_db, n);

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

