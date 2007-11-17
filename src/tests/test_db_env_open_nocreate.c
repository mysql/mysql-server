// Try to open an environment where the directory does not exist 
// Try when the dir exists but is not an initialized env

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

// DIR is defined in the Makefile
#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

int main() {
    DB_ENV *dbenv;
    int r;

    system("rm -rf " DIR);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);
    r = dbenv->open(dbenv, DIR, DB_PRIVATE|DB_INIT_MPOOL, 0);
    assert(r==ENOENT);
    dbenv->close(dbenv,0); // free memory

    system("rm -rf " DIR);
    mkdir(DIR, 0777);
    r = db_env_create(&dbenv, 0);
    assert(r == 0);
    r = dbenv->open(dbenv, DIR, DB_PRIVATE|DB_INIT_MPOOL, 0);
    assert(r==ENOENT);
    dbenv->close(dbenv,0); // free memory

    return 0;
}
