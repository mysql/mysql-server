// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;

    DB_TXN* txna = NULL;
    DB_TXN* txnb = NULL;
    DB_TXN* txnc = NULL;
    DBC* c;
    r = env->txn_begin(env, NULL, &txna, 0);                                        CKERR(r);

    r = db_create(&db, env, 0);                                                     CKERR(r);
    r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);

    DBT key,val;
    r = db->put(db, txna, dbt_init(&key, "a", 4), dbt_init(&val, "a", 4), 0);       CKERR(r);

    r = env->txn_begin(env, NULL, &txnb, DB_TXN_SNAPSHOT);                                        CKERR(r);
    r = env->txn_begin(env, NULL, &txnc, DB_READ_COMMITTED);                                       CKERR(r);
    r = db->cursor(db, txna, &c, 0); CKERR(r);
    r = c->c_close(c); CKERR(r);
    c = NULL;
    r = txna->commit(txna, 0);                                                      CKERR(r);

    r = db->cursor(db, txnb, &c, 0); assert(r == TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    r = db->cursor(db, txnc, &c, 0); assert(r == TOKUDB_MVCC_DICTIONARY_TOO_NEW);


    r = txnb->commit(txnb, 0);                                                      CKERR(r);
    r = txnc->commit(txnc, 0);                                                      CKERR(r);

    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    
    return 0;
}
