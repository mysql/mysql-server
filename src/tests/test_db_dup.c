#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <db.h>


// DIR is defined in the Makefile

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main() {
    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = DIR "/test.dup.db";
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, DB_DUP);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);

    /* verify dup flags match */
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);

    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, DB_DUP);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);

    /* verify nodesize match */
    r = db_create(&db, null_env, 0);
    CKERR(r);
    r = db->set_flags(db, DB_DUP);
    CKERR(r);
    r = db->set_pagesize(db, 4096);
    CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);

    return 0;
}
