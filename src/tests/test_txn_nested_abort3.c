#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <db.h>
#include "test.h"

void test_txn_abort() {

    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    int r;
    int i;
    DB_ENV *env;
    DBT key, val;
    DB_TXN* txn_all = NULL;
    DB_TXN* txn_stmt = NULL;
    DB_TXN* txn_sp = NULL;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->set_data_dir(env, ENVDIR);
    r = env->set_lg_dir(env, ENVDIR);
    r = env->open(env, 0, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, 0777); 
    CKERR(r);

    DB *db = NULL;
    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }


    r = env->txn_begin(env, 0, &txn_all, 0); CKERR(r);

    r = env->txn_begin(env, txn_all, &txn_stmt, 0); CKERR(r);
    i = 1;
    r = db->put(db, txn_stmt, dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
    CKERR(r);
    r = txn_stmt->commit(txn_stmt,DB_TXN_NOSYNC); 
    txn_stmt = NULL;
    
    r = env->txn_begin(env, txn_all, &txn_sp, 0); CKERR(r);
    
    r = env->txn_begin(env, txn_sp, &txn_stmt, 0); CKERR(r);
    r = db->del(db, txn_stmt, dbt_init(&key, &i, sizeof i), 0);
    CKERR(r);


    r = txn_stmt->commit(txn_stmt,DB_TXN_NOSYNC); 
    txn_stmt = NULL;


    r = txn_all->abort(txn_all);
    CKERR(r);


    {
        /* walk the db, should be empty */
        DB_TXN *txn = 0;
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

    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    test_txn_abort();
    return 0;
}
