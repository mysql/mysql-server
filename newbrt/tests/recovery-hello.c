// test recovery of "hello" comments

#include "test.h"
#include "includes.h"

#define TESTDIR "dir." __FILE__ 

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
    BYTESTRING hello  = { strlen("hello"), "hello" };
    r = toku_log_comment(logger, NULL, TRUE, 0, hello); assert(r == 0);
    LSN beginlsn;
    r = toku_log_begin_checkpoint(logger, &beginlsn, TRUE, 0); assert(r == 0);
    r = toku_log_end_checkpoint(logger, NULL, TRUE, beginlsn.lsn, 0, 0, 0); assert(r == 0);
    r = toku_log_comment(logger, NULL, TRUE, 0, hello); assert(r == 0);
    BYTESTRING there  = { strlen("there"), "there" };
    r = toku_log_comment(logger, NULL, TRUE, 0, there); assert(r == 0);
    r = toku_logger_close(&logger); assert(r == 0);

    // redirect stderr
    int devnul = open(DEV_NULL_FILE, O_WRONLY);
    assert(devnul>=0);
    r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // run recovery
    r = tokudb_recover(TESTDIR, TESTDIR, 0, 0, NULL, 0); 
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
