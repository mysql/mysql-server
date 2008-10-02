/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "includes.h"

int main (int argc, char *argv[]) {
    assert(argc==2);

    int r=tokudb_recover(".", argv[1]);
    if (r!=0) {
	fprintf(stderr, "Recovery failed\n");
	exit(1);
    }
    toku_malloc_cleanup();
    return 0;
}
