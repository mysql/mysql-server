/* Test log archive. */
#include <toku_portability.h>
#include <db.h>
#include <sys/stat.h>
#include "test.h"



int
test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    DB_ENV *env;
    DB *db;
    DB_TXN *txn;
    int r;

    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    CKERR(r);

    char **list;
    r=env->log_archive(env, &list, 0); 

    assert(list==0);
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);
    return 0;
}

