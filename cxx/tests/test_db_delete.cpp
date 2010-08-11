#include <stdio.h>
#include <assert.h>
#include <db_cxx.h>
#include <stdlib.h>
#include <memory.h>

#define DIR  __FILE__ ".dir"
#define FNAME "test.tdb"

void test_new_delete() {
    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);
    DbEnv env(0);
    { int r = env.set_redzone(0); assert(r==0); }
    { int r = env.open(DIR, DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0); }
    Db *db = new Db(&env, 0); assert(db != 0);
    delete db;
}

void test_new_open_delete() {
    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);
    DbEnv env(0);
    { int r = env.set_redzone(0); assert(r==0); }
    { int r = env.open(DIR, DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0); }
    Db *db = new Db(&env, 0); assert(db != 0);
    { int r = db->open(NULL, FNAME, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0); }
    { int r = db->close(0); assert(r == 0); }
    delete db;
}

int main() {
    test_new_delete();
    test_new_open_delete();
    return 0;
}
