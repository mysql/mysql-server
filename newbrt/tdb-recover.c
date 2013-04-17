/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "includes.h"

int main (int argc, const char *argv[]) {
    const char *data_dir, *log_dir;
    if (argc==3) {
	data_dir = argv[1];
	log_dir  = argv[2];
    } else if (argc==2) {
	data_dir = log_dir = argv[1];
    } else {
	printf("Usage: %s <datadir> [ <logdir> ]\n", argv[0]);
	exit(1);
    }

    int r=tokudb_recover(data_dir, log_dir);
    if (r!=0) {
	fprintf(stderr, "Recovery failed\n");
	exit(1);
    }
    toku_malloc_cleanup();
    return 0;
}
