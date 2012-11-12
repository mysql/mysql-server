/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../ft/recover ../dir.test_log2.c.tdb

#include "ft-ops.h"
#include "recover.h"

static int recovery_main(int argc, const char *const argv[]);

int
main(int argc, const char *const argv[]) {
    {
	int rr = toku_ft_layer_init();
	assert(rr==0);
    }
    int r = recovery_main(argc, argv);
    toku_ft_layer_destroy();
    return r;
}

int recovery_main (int argc, const char *const argv[]) {
    const char *data_dir, *log_dir;
    if (argc==3) {
	data_dir = argv[1];
	log_dir  = argv[2];
    } else if (argc==2) {
	data_dir = log_dir = argv[1];
    } else {
	printf("Usage: %s <datadir> [ <logdir> ]\n", argv[0]);
	return(1);
    }

    int r = tokudb_recover(NULL,
			   NULL_prepared_txn_callback,
			   NULL_keep_cachetable_callback,
			   NULL_logger,
			   data_dir, log_dir, NULL, NULL, NULL, NULL, 0);
    if (r!=0) {
	fprintf(stderr, "Recovery failed\n");
	return(1);
    }
    return 0;
}
