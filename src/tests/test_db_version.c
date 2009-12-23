/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <db.h>



int
test_main (int argc, char *argv[]) {
    const char *v;
    int major, minor, patch;
    parse_args(argc, argv);
    v = db_version(0, 0, 0);
    assert(v!=0);
    v = db_version(&major, &minor, &patch);
    assert(major==DB_VERSION_MAJOR);
    assert(minor==DB_VERSION_MINOR);
    assert(patch==DB_VERSION_PATCH);
    if (verbose) {
        printf("%d.%d.%d\n", major, minor, patch);
        printf("%s\n", v);
    }
    return 0;
}
