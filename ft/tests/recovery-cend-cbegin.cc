/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// run recovery on a log with an incomplete checkpoint 

#include "test.h"
#include "includes.h"

#define TESTDIR __SRCFILE__ ".dir"

static int 
run_test(void) {
    int r;

    // setup the test dir
    r = system("rm -rf " TESTDIR);
    CKERR(r);
    r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

    // create the log
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);
    LSN firstbegin = ZERO_LSN;
    r = toku_log_begin_checkpoint(logger, &firstbegin, TRUE, 0, 0); assert(r == 0);
    assert(firstbegin.lsn != ZERO_LSN.lsn);
    r = toku_log_end_checkpoint(logger, NULL, FALSE, firstbegin, 0, 0, 0); assert(r == 0);
    r = toku_log_begin_checkpoint(logger, NULL, TRUE, 0, 0); assert(r == 0);
    r = toku_logger_close(&logger); assert(r == 0);

    if (!verbose) {
        // redirect stderr
        int devnul = open(DEV_NULL_FILE, O_WRONLY);
        assert(devnul >= 0);
        r = toku_dup2(devnul, fileno(stderr)); assert(r == fileno(stderr));
        r = close(devnul); assert(r == 0);
    }

    // run recovery
    r = tokudb_recover(NULL,
		       NULL_prepared_txn_callback,
		       NULL_keep_cachetable_callback,
		       NULL_logger, TESTDIR, TESTDIR,
                       toku_builtin_compare_fun,
                       NULL, NULL, NULL,
                       0);
    assert(r == 0);

    r = system("rm -rf " TESTDIR);
    CKERR(r);

    return 0;
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    int r;
    r = run_test();
    return r;
}
