/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "cachetable.h"
#include "key.h"
#include "log-internal.h"
#include "log_header.h"
#include "toku_assert.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

int main (int argc, char *argv[]) {
    assert(argc==2);

    int r=tokudb_recover(argv[1], argv[1]);
    if (r!=0) {
	fprintf(stderr, "Recovery failed\n");
	exit(1);
    }
    return 0;
}
