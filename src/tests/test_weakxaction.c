/* Find out about weak transactions.
 *  User A does a transaction.
 *  User B does somethign without a transaction, and it conflicts.
 */

#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "test.h"

void test_autotxn(u_int32_t env_flags, u_int32_t db_flags) {
    DB_ENV *env;
    DB *db;
    int r;
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);
    r = db_env_create (&env, 0);           CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_flags(env, env_flags, 1); CKERR(r);
    r = env->open(env, ENVDIR, 
                  DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | 
                  DB_INIT_LOG | DB_INIT_TXN | DB_INIT_LOCK, 0777); CKERR(r);
    r = db_create(&db, env, 0);
    CKERR(r);
    {
	DB_TXN *x = NULL;
	if (env_flags==0 && db_flags==0) {
	    r = env->txn_begin(env, 0, &x, 0); CKERR(r);
	}
	r = db->open(db, x, "numbers.db", 0, DB_BTREE, DB_CREATE | db_flags, 0);
	if (env_flags==0 && db_flags==0) {
	    r = x->commit(x, 0); CKERR(r);
	}
	CKERR(r);
    }

    DB_TXN *x1, *x2 = NULL;
    r = env->txn_begin(env, 0, &x1, DB_TXN_NOWAIT); CKERR(r);
    #ifdef USE_BDB
        r = env->txn_begin(env, 0, &x2, DB_TXN_NOWAIT); CKERR(r);
    #endif
    DBT k1,k2,v1,v2;
    memset(&k1, 0, sizeof(DBT));
    memset(&k2, 0, sizeof(DBT));
    memset(&v1, 0, sizeof(DBT));
    memset(&v2, 0, sizeof(DBT));
    k2.data = k1.data = "hello";
    k2.size = k1.size = 6;
    v1.data = "there";
    v1.size = 6;
    r = db->put(db, x1, &k1, &v1, 0); CKERR(r);
    r = db->get(db, x2, &k2, &v2, 0); assert(r==DB_LOCK_DEADLOCK || r==DB_LOCK_NOTGRANTED);
    r = x1->commit(x1, 0);         CKERR(r);
    #ifdef USE_BDB
        r = x2->commit(x2, 0);     assert(r==0);
    #endif
    r = db->close(db, 0);          CKERR(r);
    r = env->close(env, 0);        assert(r==0);
}

int main (int argc, char *argv[])  {
    test_autotxn(DB_AUTO_COMMIT, DB_AUTO_COMMIT); 
    test_autotxn(0,              DB_AUTO_COMMIT); 
    test_autotxn(DB_AUTO_COMMIT, 0); 
    test_autotxn(0,              0); 
    return 0;
}
