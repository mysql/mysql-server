/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test recovery of "hello" comments

#include "test.h"
#include "includes.h"

#define TESTDIR __SRCFILE__ ".dir"

static int 
run_test(void) {
    // leave this many bytes in file
    const int magic_begin_end_checkpoint_sz = 8 // "tokulogg" magic 8 byte header
                                             +4 // version
                                             +toku_log_begin_checkpoint_overhead
                                             +toku_log_end_checkpoint_overhead;

    int r;
    int trim = 1;
    toku_struct_stat st;

    while ( 1 ) {
        // setup the test dir
        r = system("rm -rf " TESTDIR);
        CKERR(r);
        r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

        // create the log
        TOKULOGGER logger;
        BYTESTRING hello  = { (uint32_t) strlen("hello"), (char *) "hello" };
        BYTESTRING world  = { (uint32_t) strlen("world"), (char *) "world" };
        BYTESTRING there  = { (uint32_t) strlen("there"), (char *) "there" };
        r = toku_logger_create(&logger); assert(r == 0);
        r = toku_logger_open(TESTDIR, logger); assert(r == 0);
        LSN beginlsn;
        // all logs must contain a valid checkpoint
        r = toku_log_begin_checkpoint(logger, &beginlsn, true, 0, 0); assert(r == 0);
        r = toku_log_end_checkpoint(logger, NULL, true, beginlsn, 0, 0, 0); assert(r == 0);
        r = toku_log_comment(logger, NULL, true, 0, hello); assert(r == 0);
        r = toku_log_comment(logger, NULL, true, 0, world); assert(r == 0);
        r = toku_log_begin_checkpoint(logger, &beginlsn, true, 0, 0); assert(r == 0);
        r = toku_log_end_checkpoint(logger, NULL, true, beginlsn, 0, 0, 0); assert(r == 0);
        r = toku_log_comment(logger, NULL, true, 0, hello); assert(r == 0);
        r = toku_log_comment(logger, NULL, true, 0, there); assert(r == 0);
        r = toku_logger_close(&logger); assert(r == 0);

        // redirect stderr
        int devnul = open(DEV_NULL_FILE, O_WRONLY);
        assert(devnul>=0);
        r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
        r = close(devnul);                      assert(r==0);

        char fname[256];
        sprintf(fname, "%s/%s%d", TESTDIR, "log000000000000.tokulog", TOKU_LOG_VERSION);

        r = toku_stat(fname, &st); assert(r==0);
        if ( st.st_size - trim > magic_begin_end_checkpoint_sz ) {
            r = truncate(fname, st.st_size - trim);
            CKERR(r);
        }
        else
            break;
        // run recovery
        r = tokudb_recover(NULL,
			   NULL_prepared_txn_callback,
			   NULL_keep_cachetable_callback,
			   NULL_logger,
			   TESTDIR, TESTDIR, 0, 0, 0, NULL, 0); 
        assert(r == 0);
        
        trim += 1;
    }
    r = system("rm -rf " TESTDIR);
    CKERR(r);
    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
