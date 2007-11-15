#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <db.h>

#define DIR "dir.test_db_dup"

int main() {
    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.dup.brt";
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);
    r=chdir(DIR);       assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    r = db->close(db, 0);
    assert(r == 0);

    /* verify dup flags match */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r != 0);
    r = db->close(db, 0);
    assert(r == 0);

    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);
    r = db->close(db, 0);
    assert(r == 0);

    /* verify nodesize match */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);
    r = db->close(db, 0);
    assert(r == 0);

    r=chdir("..");       assert(r==0);

    return 0;
}
