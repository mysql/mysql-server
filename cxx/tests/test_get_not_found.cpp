/* This test ensures that a db->get DB_NOTFOUND does not cause an
   exception when exceptions are enabled, which is consistent with
   Berkeley DB */

#include <db_cxx.h>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#define DIR __FILE__ ".dir"
#define fname "foo.tdb"

void db_put(Db *db, int k, int v) {
    Dbt key(&k, sizeof k);
    Dbt val(&v, sizeof v);
    int r = db->put(0, &key, &val, 0); 
    assert(r == 0);
}

void db_del(Db *db, int k) {
    Dbt key(&k, sizeof k);
    int r = db->del(0, &key, 0); 
    assert(r == 0);
}

void db_get(Db *db, int k, int v, int expectr) {
    Dbt key(&k, sizeof k);
    Dbt val; val.set_data(&v);  val.set_ulen(sizeof v); val.set_flags(DB_DBT_USERMEM);
    int r = db->get(0, &key, &val, 0); 
    assert(r == expectr);
    if (r == 0) {
        assert(val.get_size() == sizeof v);
        int vv;
        memcpy(&vv, val.get_data(), val.get_size());
        assert(vv == v);
    }
}

DbEnv *env = NULL;
void reset_env (void) {
    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);
    if (env) delete env;
    env = new DbEnv(DB_CXX_NO_EXCEPTIONS);
    int r = env->open(DIR, DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777);
    assert(r == 0);
}

void test_not_found(void) {
    int r;
    reset_env();
    Db *db = new Db(env, DB_CXX_NO_EXCEPTIONS); assert(db);
    r = db->open(0, fname, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    db_put(db, 1, 2);
    db_get(db, 1, 2, 0);
    db_del(db, 1);
    db_get(db, 1, 0, DB_NOTFOUND);
    r = db->close(0); assert(r == 0);
    delete db;
}

void test_exception_not_found(void) {
    int r;
    
    reset_env();
    Db *db = new Db(env, 0); assert(db);
    r = db->open(0, fname, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    db_put(db, 1, 2);
    db_get(db, 1, 2, 0);
    db_del(db, 1);
    try {
        db_get(db, 1, 0, DB_NOTFOUND);
    } catch(...) {
        assert(0);
    }
    r = db->close(0); assert(r == 0);
    delete db;
}

int main(int argc, char *argv[]) {
    test_not_found();
    test_exception_not_found();
    if (env) delete env;
    return 0;
}
