/* A simple case to see if recovery works. */

#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "test.h"

static void test (void) {
    int r;
    system("rm -rf " ENVDIR);
    r=mkdir(ENVDIR, 0777);                                                     assert(r==0);

    DB_ENV *env;
    DB_TXN *tid;
    DB     *db;
    DBT key,data;

    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, 0777); CKERR(r);

    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, 0777);               CKERR(r);
    r=tid->commit(tid, 0);                                                     assert(r==0);

    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db->put(db, tid, dbt_init(&key, "a", 2), dbt_init(&data, "b", 2), 0);    assert(r==0);
    r=tid->commit(tid, 0);                                                     assert(r==0);

    r=db->close(db, 0);                                                        assert(r==0);
    r=env->close(env, 0);                                                      assert(r==0);

    unlink(ENVDIR "/foo.db");

    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD|DB_RECOVER, 0777); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, 0, 0777);                       CKERR(r);
    r=db->get(db, tid, dbt_init(&key, "a", 2), dbt_init_malloc(&data), 0);     assert(r==0); 
    r=tid->commit(tid, 0);                                                     assert(r==0);
    free(data.data);
    
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test();
    return 0;
}
    
