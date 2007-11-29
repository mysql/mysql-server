/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

// DIR is defined in the Makefile

int main() {
    DB_ENV *dbenv;
    int r;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->set_data_dir(dbenv, DIR);
    assert(r == 0);

    r = dbenv->set_data_dir(dbenv, DIR);
    assert(r == 0);

    r = dbenv->open(dbenv, DIR, DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL, 0);
    CKERR(r);

#ifdef USE_TDB
    // According to the BDB man page, you may not call set_data_dir after doing the open.
    // Some versions of BDB don't actually check this or complain
    r = dbenv->set_data_dir(dbenv, "foo" DIR);
    assert(r == EINVAL);
#endif
    
    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}
