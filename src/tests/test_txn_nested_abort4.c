#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <db.h>
#include "test.h"

DB *db;
DB_ENV *env;
void setup_db(void) {
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    int r;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_data_dir(env, ENVDIR);
    r = env->set_lg_dir(env, ENVDIR);
    r = env->open(env, 0, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, 0777); 
    CKERR(r);

    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }
}

void close_db(void) {
    int r;
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

void test_txn_abort(int insert, int secondnum) {
    setup_db();

    DBT key, val;
    int r;


    DB_TXN *parent = NULL, *child = NULL;

    int i = 1;
    r = env->txn_begin(env, 0, &parent, 0); CKERR(r);

    //Insert something as a child
    r = env->txn_begin(env, parent, &child, 0); CKERR(r);
    i = 1;
    r = db->put(db, child, dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
    CKERR(r);
    r = child->commit(child,DB_TXN_NOSYNC); 
    child = NULL;


    //delete it as a child
    r = env->txn_begin(env, parent, &child, 0); CKERR(r);
    i = secondnum;
    if (insert) {
        r = db->put(db, child, dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
        CKERR(r);
    }
    else { // delete
        r = db->del(db, child, dbt_init(&key, &i, sizeof i), DB_DELETE_ANY); 
#if USE_TDB
        CKERR(r);
#else
        CKERR2(r, (secondnum==1 ? 0 : DB_NOTFOUND));
#endif
    }
    r = child->commit(child,DB_TXN_NOSYNC); 
    child = NULL;

    r = parent->abort(parent);
    CKERR(r);
    parent = NULL;


    {
        DB_TXN *txn = NULL;
        /* walk the db, should be empty */
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); CKERR(r);
        memset(&key, 0, sizeof key);
        memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_FIRST); 
        CKERR2(r, DB_NOTFOUND);
        r = cursor->c_close(cursor); CKERR(r);
        r = txn->commit(txn, 0);
    }

}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    test_txn_abort(1, 0);
    test_txn_abort(0, 0);
    test_txn_abort(1, 1);
    test_txn_abort(0, 1);
    return 0;
}
