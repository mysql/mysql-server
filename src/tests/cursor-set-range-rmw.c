#include "test.h"

// verify that the DB_RMW flag on cursor create grabs write locks for cursor set operations

static void test_create_rmw(DB_ENV *env, DB *db, int k, uint32_t txn1_flags, uint32_t txn2_flags, int expect_r) {
    int r;

    DB_TXN *txn1 = NULL;
    r = env->txn_begin(env, NULL, &txn1, 0); assert_zero(r);

    DB_TXN *txn2 = NULL;
    r = env->txn_begin(env, NULL, &txn2, 0); assert_zero(r);

    DBC *c1 = NULL;
    r = db->cursor(db, txn1, &c1, txn1_flags); assert_zero(r);

    DBC *c2 = NULL;
    r = db->cursor(db, txn2, &c2, txn2_flags); assert_zero(r);

    DBT key; dbt_init(&key, &k, sizeof k);
    DBT val; memset(&val, 0, sizeof val);
    r = c1->c_get(c1, &key, &val, DB_SET); assert_zero(r);

    r = c2->c_get(c2, &key, &val, DB_SET); assert(r == expect_r);

    r = c1->c_close(c1); assert_zero(r);
    r = c2->c_close(c2); assert_zero(r);

    r = txn1->commit(txn1, 0); assert_zero(r);
    r = txn2->commit(txn2, 0); assert_zero(r);
}

// verify that the DB_RMW flag to the cursor set operations grabs write locks

static void test_set_rmw(DB_ENV *env, DB *db, int k, uint32_t txn1_flags, uint32_t txn2_flags, int expect_r) {
    int r;

    DB_TXN *txn1 = NULL;
    r = env->txn_begin(env, NULL, &txn1, 0); assert_zero(r);

    DB_TXN *txn2 = NULL;
    r = env->txn_begin(env, NULL, &txn2, 0); assert_zero(r);

    DBC *c1 = NULL;
    r = db->cursor(db, txn1, &c1, 0); assert_zero(r);

    DBC *c2 = NULL;
    r = db->cursor(db, txn2, &c2, 0); assert_zero(r);

    DBT key; dbt_init(&key, &k, sizeof k);
    DBT val; memset(&val, 0, sizeof val);
    r = c1->c_get(c1, &key, &val, DB_SET + txn1_flags); assert_zero(r);

    r = c2->c_get(c2, &key, &val, DB_SET + txn2_flags); assert(r == expect_r);

    r = c1->c_close(c1); assert_zero(r);
    r = c2->c_close(c2); assert_zero(r);

    r = txn1->commit(txn1, 0); assert_zero(r);
    r = txn2->commit(txn2, 0); assert_zero(r);
}

int test_main(int argc, char * const argv[]) {
    int r;

    char *env_dir = ENVDIR;
    char *db_filename = "rmwtest";

    parse_args(argc, argv);

    char rm_cmd[strlen(env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", env_dir);
    r = system(rm_cmd); assert_zero(r);

    r = toku_os_mkdir(env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert_zero(r);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);
    int env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    r = env->open(env, env_dir, env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert_zero(r);

    // create the db
    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);
    DB_TXN *create_txn = NULL;
    r = env->txn_begin(env, NULL, &create_txn, 0); assert_zero(r);
    r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert_zero(r);
    r = create_txn->commit(create_txn, 0); assert_zero(r);

    DB_TXN *write_txn = NULL;
    r = env->txn_begin(env, NULL, &write_txn, 0); assert_zero(r);

    int k = htonl(42); int v = 42;
    DBT key; dbt_init(&key, &k, sizeof k);
    DBT val; dbt_init(&val, &v, sizeof v);
    r = db->put(db, write_txn, &key, &val, DB_NOOVERWRITE); assert_zero(r);
    r = write_txn->commit(write_txn, 0); assert_zero(r);

    test_set_rmw(env, db, k, 0, 0, 0);
    test_set_rmw(env, db, k, 0, DB_RMW, DB_LOCK_NOTGRANTED);
    test_set_rmw(env, db, k, DB_RMW, 0, DB_LOCK_NOTGRANTED);
    test_set_rmw(env, db, k, DB_RMW, DB_RMW, DB_LOCK_NOTGRANTED);

    test_create_rmw(env, db, k, 0, 0, 0);
    test_create_rmw(env, db, k, 0, DB_RMW, DB_LOCK_NOTGRANTED);
    test_create_rmw(env, db, k, DB_RMW, 0, DB_LOCK_NOTGRANTED);
    test_create_rmw(env, db, k, DB_RMW, DB_RMW, DB_LOCK_NOTGRANTED);


    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
    return 0;
}
