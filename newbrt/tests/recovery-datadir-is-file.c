#ident "$Id$"
// test recovery with no data directory 

#include "test.h"
#include "includes.h"

#define TESTDIR __FILE__ ".dir"
#define TESTFILE __FILE__ ".dir.bogus"

static int 
run_test(void) {
    int r;

    // setup the test dir
    r = system("rm -rf " TESTDIR " " TESTFILE);
    CKERR(r);
    r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

    // create the log
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);
    BYTESTRING hello  = { strlen("hello"), "hello" };
    r = toku_log_comment(logger, NULL, TRUE, 0, hello);
    r = toku_logger_close(&logger); assert(r == 0);

    // redirect stderr
    int devnul = open(DEV_NULL_FILE, O_WRONLY);
    assert(devnul>=0);
    r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // run recovery
    r = system("touch " TESTFILE); CKERR(r);
    r = tokudb_recover(NULL,
		       NULL_keep_zombie_callback,
		       NULL_prepared_txn_callback,
		       NULL_keep_cachetable_callback,
		       NULL_setup_db_callback,
		       NULL_close_db_callback,
		       NULL_logger,
		       TESTFILE, TESTDIR, 0, 0, 0, NULL, 0); 
    assert(r != 0);

    r = system("rm -rf " TESTDIR " " TESTFILE); CKERR(r);

    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
