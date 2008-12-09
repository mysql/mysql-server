/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <stdio.h>
#include <assert.h>
#include <toku_portability.h>
#include <db.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// ENVDIR is defined in the Makefile

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    DB_ENV *dbenv;
    int r;

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->set_tmp_dir(dbenv, ".");
    assert(r == 0);

    r = dbenv->set_tmp_dir(dbenv, ".");
    assert(r == 0);
    
    r = dbenv->open(dbenv, ENVDIR, DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL, 0);
    CKERR(r);

#ifdef USE_TDB
    // According to the BDB man page, you may not call set_tmp_dir after doing the open.
    // Some versions of BDB don't actually check this or complain
    r = dbenv->set_tmp_dir(dbenv, ".");
    assert(r == EINVAL);
#endif
    

    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}
