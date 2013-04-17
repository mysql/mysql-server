/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <stdio.h>
#include <assert.h>
#include <toku_portability.h>
#include <db.h>

int
test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    DB_ENV *dbenv;
    int r;

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    r = dbenv->close(dbenv, 0);
    assert(r == 0);

    return 0;
}
