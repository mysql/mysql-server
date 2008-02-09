/* This test ensures that a db->get DB_NOTFOUND does not cause an
   exception when exceptions are enabled, which is consistent with
   Berkeley DB */

#include <db_cxx.h>
#include <assert.h>

const char *test_file_name = "test_get_not_found.tdb";

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

void test_not_found() {
    int r;

    unlink(test_file_name);
    Db *db = new Db(0, DB_CXX_NO_EXCEPTIONS); assert(db);
    r = db->open(0, test_file_name, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    db_put(db, 1, 2);
    db_get(db, 1, 2, 0);
    db_del(db, 1);
    db_get(db, 1, 0, DB_NOTFOUND);
    r = db->close(0); assert(r == 0);
    delete db;
}

void test_exception_not_found() {
    int r;
    
    unlink(test_file_name);
    Db *db = new Db(0, 0); assert(db);
    r = db->open(0, test_file_name, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
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
    return 0;
}
