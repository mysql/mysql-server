/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include <db.h>
#include <assert.h>

#include "test.h"

int
test_main (int argc, const char *argv[]) {
    const char *v;
    int major, minor, patch;
    parse_args(argc, argv);
    v = db_version(0, 0, 0);
    assert(v!=0);
    v = db_version(&major, &minor, &patch);
    assert(major==DB_VERSION_MAJOR);
    assert(minor==DB_VERSION_MINOR);
    assert(patch==DB_VERSION_PATCH);
    if (verbose)
        printf("%d.%d.%d %s\n", major, minor, patch, v);
    return 0;
}
