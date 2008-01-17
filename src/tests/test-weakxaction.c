/* Find out about weak transactions.
 *  User A does a transaction.
 *  User B does somethign without a transaction, and it conflicts.
 */

#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main (int argc, char *argv[]) {
    DB_ENV *env;
    DB *db;
    int r;
    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    r = db_env_create (&env, 0); assert(r==0);
    r = env->open(env, DIR, DB_CREATE | DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_TXN | DB_INIT_LOCK, 0777); assert(r==0);
        r = db_create(&db, env, 0);
    assert(r==0);
    r = db->open(db, 0, "numbers.db", 0, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, 0);
    assert(r==0);

    DB_TXN *x1, *x2;
    r = env->txn_begin(env, 0, &x1, 0); assert(r==0);
    r = env->txn_begin(env, 0, &x2, DB_TXN_NOWAIT); assert(r==0);
    DBT k1,k2,v1,v2;
    memset(&k1, 0, sizeof(DBT));
    memset(&k2, 0, sizeof(DBT));
    memset(&v1, 0, sizeof(DBT));
    memset(&v2, 0, sizeof(DBT));
    k2.data = k1.data = "hello";
    k2.size = k1.size = 6;
    v1.data = "there";
    v1.size = 6;
    r = db->put(db, x1, &k1, &v1, 0); assert(r==0);
    r = db->get(db, x2, &k2, &v2, 0); assert(r==DB_LOCK_DEADLOCK || r==DB_LOCK_NOTGRANTED);
    r = x1->commit(x1, 0);         assert(r==0);
    r = x2->commit(x2, 0);         assert(r==0);
    r = db->close(db, 0);          assert(r==0);
    r = env->close(env, 0);        assert(r==0);
    return 0;
}
