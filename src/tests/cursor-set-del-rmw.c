#include "test.h"

// TODO

static void test_del_rmw(DB_ENV *env, DB *db, uint32_t t1_flags, uint32_t t2_flags, uint32_t c1_flags, uint32_t c2_flags, int expect_r) {
    int r;

    {
        DB_TXN *write_txn = NULL;
        r = env->txn_begin(env, NULL, &write_txn, 0); assert_zero(r);
        for (int i = 1; i <= 3; i++) {
            int k = htonl(i); int v = i;
            DBT key; dbt_init(&key, &k, sizeof k);
            DBT val; dbt_init(&val, &v, sizeof v);
            r = db->put(db, write_txn, &key, &val, DB_YESOVERWRITE); assert_zero(r);
        }
        r = write_txn->commit(write_txn, 0); assert_zero(r);
    }

    {
        DB_TXN *txn1 = NULL;
        r = env->txn_begin(env, NULL, &txn1, t1_flags); assert_zero(r);

        DB_TXN *txn2 = NULL;
        r = env->txn_begin(env, NULL, &txn2, t2_flags); assert_zero(r);

        DBC *c1 = NULL;
        r = db->cursor(db, txn1, &c1, c1_flags); assert_zero(r);

        DBC *c2 = NULL;
        r = db->cursor(db, txn2, &c2, c2_flags); assert_zero(r);

        r = c1->c_pre_acquire_range_lock(c1, db->dbt_neg_infty(), db->dbt_pos_infty()); assert_zero(r);

        int k = htonl(2);
        DBT key; dbt_init(&key, &k, sizeof k);
        r = db->del(db, txn1, &key, 0); assert_zero(r);
        
        k = htonl(1);
        DBT val; memset(&val, 0, sizeof val);
        r = c2->c_get(c2, &key, &val, DB_SET); assert(r == expect_r);

        r = c1->c_close(c1); assert_zero(r);
        r = c2->c_close(c2); assert_zero(r);

        r = txn1->commit(txn1, 0); assert_zero(r);
        r = txn2->commit(txn2, 0); assert_zero(r);
    }
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

    // t1: prelock read, del(2)
    // t2: set(1)
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_READ_UNCOMMITTED, 0, 0, 0);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_READ_COMMITTED,   0, 0, 0);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_TXN_SNAPSHOT,     0, 0, 0);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_SERIALIZABLE,     0, 0, 0);

    // t1: prelock write, del(2)
    // t2: set(1)
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_READ_UNCOMMITTED, DB_RMW, 0, 0);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_READ_COMMITTED,   DB_RMW, 0, 0);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_TXN_SNAPSHOT  ,   DB_RMW, 0, 0);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_SERIALIZABLE,     DB_RMW, 0, DB_LOCK_NOTGRANTED);

    // t1: prelock write, del(2)
    // t2: rmw set(1)
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_READ_UNCOMMITTED, DB_RMW, DB_RMW, DB_LOCK_NOTGRANTED);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_READ_COMMITTED,   DB_RMW, DB_RMW, DB_LOCK_NOTGRANTED);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_TXN_SNAPSHOT  ,   DB_RMW, DB_RMW, DB_LOCK_NOTGRANTED);
    test_del_rmw(env, db, DB_SERIALIZABLE, DB_SERIALIZABLE,     DB_RMW, DB_RMW, DB_LOCK_NOTGRANTED);


    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
    return 0;
}
