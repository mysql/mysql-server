/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"


#include <stdio.h>
#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <memory.h>

// ENVDIR is defined in the Makefile

int
test_main(int argc, const char** argv) {
    DB_ENV *dbenv;
    int r;
    if (argc == 2 && !strcmp(argv[1], "-v")) verbose = 1;
    
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->open(dbenv, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0666);
    assert(r == 0);

    r = dbenv->open(dbenv, ENVDIR, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0666);
#ifdef USE_TDB
    if (verbose) printf("r=%d\n", r);
    assert(r != 0);
#else
    if (verbose) printf("test_db_env_open_open_close.bdb skipped.  (BDB apparently does not follow the spec).\n");
    assert(r==0);
#endif    

    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}
