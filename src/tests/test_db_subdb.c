#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <db.h>

// DIR is defined in the Makefile

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main() {
    DB_ENV * env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.db";
    int r;

    system("rm -rf " DIR);

    r=mkdir(DIR, 0777); assert(r==0);

    r=db_env_create(&env, 0);   assert(r==0);
    // Note: without DB_INIT_MPOOL the BDB library will fail on db->open().
    r=env->open(env, DIR, DB_INIT_MPOOL|DB_PRIVATE|DB_CREATE|DB_INIT_LOG, 0777); assert(r==0);

    r = db_create(&db, env, 0);
    assert(r == 0);

    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    
    r = db->close(db, 0);
    assert(r == 0);

    return 0;
}
