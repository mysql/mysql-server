/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
// test recovery of a clean shutdown

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
    
    // add begin checkpoint, end checkpoint
    LSN beginlsn;
    r = toku_log_begin_checkpoint(logger, &beginlsn, FALSE, 0); assert(r == 0);
    r = toku_log_end_checkpoint(logger, NULL, TRUE, beginlsn.lsn, 0, 0, 0); assert(r == 0);
    r = toku_logger_close(&logger); assert(r == 0);

    // add hello
    for (int i=0; i<2; i++) {
        r = toku_logger_create(&logger); assert(r == 0);
        r = toku_logger_open(TESTDIR, logger); assert(r == 0);
        BYTESTRING hello  = { strlen("hello"), "hello" };
        r = toku_log_comment(logger, NULL, TRUE, 0, hello);
        r = toku_logger_close(&logger); assert(r == 0);
    }

    // redirect stderr
    int devnul = open(DEV_NULL_FILE, O_WRONLY);
    assert(devnul>=0);
    r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // run recovery
    r = tokudb_recover(NULL,
		       NULL_prepared_txn_callback,
		       NULL_keep_cachetable_callback,
		       NULL_logger, TESTDIR, TESTDIR, 0, 0, 0, NULL, 0); 
    assert(r == 0);

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
