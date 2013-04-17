#include "test.h"

// verify that a db->put with NOOVERWRITE grabs a write lock not a read lock.
// we use two transactions.  the first transaction tries to put with NOOVERWRITE
// and finds that the key already exists.  it now holds a write lock on the key.
// the second transaction trys to put the same key with NOOVERWRITE and gets
// LOCK_NOTGRANTED.  the second transaction can not put the key until the first
// transaction commits.

int test_main(int argc, char * const argv[]) {
    int r;

    char *env_dir = ENVDIR;
    char *db_filename = "replacetest";

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

    DB_TXN *txn1 = NULL;
    r = env->txn_begin(env, NULL, &txn1, 0); assert_zero(r);

    DB_TXN *txn2 = NULL;
    r = env->txn_begin(env, NULL, &txn2, 0); assert_zero(r);

    r = db->put(db, txn1, &key, &val, DB_NOOVERWRITE); assert(r == DB_KEYEXIST);
    r = db->put(db, txn2, &key, &val, DB_NOOVERWRITE); assert(r == DB_LOCK_NOTGRANTED);
    r = db->put(db, txn1, &key, &val, DB_YESOVERWRITE); assert_zero(r);
    r = db->put(db, txn2, &key, &val, DB_YESOVERWRITE); assert(r == DB_LOCK_NOTGRANTED);
    r = txn1->commit(txn1, 0); assert_zero(r);
    r = db->put(db, txn2, &key, &val, DB_YESOVERWRITE); assert_zero(r);
    r = txn2->commit(txn2, 0); assert_zero(r);

    r = db->close(db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
    return 0;
}
