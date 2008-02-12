#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <db.h>

#include "test.h"

int main(int argc, char *argv[]) {
    int r;
    DB_ENV *env;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    r = db_env_create(&env, 0); 
    assert(r == 0);

    r = env->open(env, DIR, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, 0777); 
    assert(r == 0);

    DB_TXN *txn;
    r = env->txn_begin(env, 0, &txn, 0);
    assert(r == 0);

    r = txn->commit(txn, 0); 
    assert(r == 0);
    
r = env->close(env, 0); 
    assert(r == 0);
    return 0;
}
