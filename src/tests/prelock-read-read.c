#include "test.h"

// verify that prelocking read ranges on multiple transactions do not conflict

static int prelock_range(DBC *cursor, int left, int right) {
    DBT key_left; dbt_init(&key_left, &left, sizeof left);
    DBT key_right; dbt_init(&key_right, &right, sizeof right);
    int r = cursor->c_pre_acquire_range_lock(cursor, &key_left, &key_right);
    return r;
}

static void test_read_read(DB_ENV *env, DB *db, uint32_t iso_flags, int expect_r) {
    int r;

    DB_TXN *txn_a = NULL;
    r = env->txn_begin(env, NULL, &txn_a, iso_flags); assert_zero(r);
    DB_TXN *txn_b = NULL;
    r = env->txn_begin(env, NULL, &txn_b, iso_flags); assert_zero(r);

    DBC *cursor_a = NULL;
    r = db->cursor(db, txn_a, &cursor_a, 0); assert_zero(r);
    DBC *cursor_b = NULL;
    r = db->cursor(db, txn_b, &cursor_b, 0); assert_zero(r);

    r = prelock_range(cursor_a, htonl(10), htonl(100)); assert_zero(r);
    r = prelock_range(cursor_b, htonl(50), htonl(200)); assert(r == expect_r);

    r = cursor_a->c_close(cursor_a); assert_zero(r);
    r = cursor_b->c_close(cursor_b); assert_zero(r);

    r = txn_a->commit(txn_a, 0); assert_zero(r);
    r = txn_b->commit(txn_b, 0); assert_zero(r);
}

int test_main(int argc, char * const argv[]) {
    int r;

    char *env_dir = ENVDIR;
    char *db_filename = "prelocktest";

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

    test_read_read(env, db, DB_READ_UNCOMMITTED, 0);
    test_read_read(env, db, DB_READ_UNCOMMITTED, 0);
    test_read_read(env, db, DB_SERIALIZABLE, 0);
 
    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
    return 0;
}
