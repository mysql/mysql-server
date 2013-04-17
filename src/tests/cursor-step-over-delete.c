#include <db.h>
#include <sys/stat.h>
#include "test.h"

static DB_ENV *env;
static DB *db;
DB_TXN *txn;

static void
setup (void) {
    system("rm -rf " ENVDIR);
    int r;
    r=mkdir(ENVDIR, 0777);       CKERR(r);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, 0777); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, 0777); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);
}

static void
shutdown (void) {
    int r;
    r= db->close(db, 0); CKERR(r);
    r= env->close(env, 0); CKERR(r);
}

static void
doit (void) {
    DBT key,data;
    int r;
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&data, "a", 2), DB_YESOVERWRITE);
    r=db->put(db, txn, dbt_init(&key, "b", 2), dbt_init(&data, "b", 2), DB_YESOVERWRITE);
    r=db->put(db, txn, dbt_init(&key, "c", 2), dbt_init(&data, "c", 2), DB_YESOVERWRITE);
    r=txn->commit(txn, 0);    assert(r==0);
    
    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    r=db->del(db, txn, dbt_init(&key, "b", 2),  0); assert(r==0);
    r=txn->commit(txn, 0);    assert(r==0);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);    
    DBC *dbc;
    r = db->cursor(db, txn, &dbc, 0);                           assert(r==0);
    memset(&key,  0, sizeof(key));
    memset(&data, 0, sizeof(data));
    r = dbc->c_get(dbc, &key, &data, DB_FIRST);                 assert(r==0);
    assert(strcmp(key.data, "a")==0);
    assert(strcmp(data.data, "a")==0);
    r = dbc->c_get(dbc, &key, &data, DB_NEXT);                  assert(r==0);
    assert(strcmp(key.data, "c")==0);
    assert(strcmp(data.data, "c")==0);
    r = dbc->c_close(dbc);                                      assert(r==0);
    r=txn->commit(txn, 0);    assert(r==0);
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);

    setup();
    doit();
    shutdown();

    return 0;
}
