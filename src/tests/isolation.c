// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char *argv[]) {
    parse_args(argc, argv);
    int r;
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB_TXN *txna;
    r = env->txn_begin(env, NULL, &txna, 0);                                            CKERR(r);
    env->set_errfile(env, stderr);

    {
	DB_TXN *txnb;
	r = env->txn_begin(env, txna, &txnb, 0);                                        CKERR(r);
	r = txnb->commit(txnb, 0);                                                      CKERR(r);
    }
    {
	DB_TXN *txnc;
	r = env->txn_begin(env, txna, &txnc, DB_READ_UNCOMMITTED);                      CKERR(r);
	r = txnc->commit(txnc, 0);                                                      CKERR(r);
    }

    
    return 0;
}
