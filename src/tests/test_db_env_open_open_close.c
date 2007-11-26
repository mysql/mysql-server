#include <stdio.h>
#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

// DIR is defined in the Makefile

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main() {
    DB_ENV *dbenv;
    int r;

    system("rm -rf " DIR);
    r=mkdir(DIR, 0777); assert(r==0);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->open(dbenv, DIR, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0666);
    assert(r == 0);

    r = dbenv->open(dbenv, DIR, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0666);
#ifdef USE_TDB
    assert(r != 0);
#else
    printf("test_db_env_open_open_close.bdb skipped.  (BDB apparently does not follow the spec).\n");
    assert(r==0);
#endif    

    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}
