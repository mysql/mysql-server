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

    // put begin and end checkpoint into separate log files
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);
    LSN beginlsn;
    r = toku_log_begin_checkpoint(logger, &beginlsn, FALSE, 0); assert(r == 0);
    r = toku_logger_close(&logger); assert(r == 0);

    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);
    r = toku_log_end_checkpoint(logger, NULL, TRUE, beginlsn.lsn, 0, 0, 0); assert(r == 0);
    r = toku_logger_close(&logger); assert(r == 0);

    // run recovery
    r = tokudb_recover(TESTDIR, TESTDIR, 0, 0, NULL, NULL, 0); 
    assert(r == 0);
    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
