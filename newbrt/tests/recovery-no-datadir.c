// test recovery with no data directory 

#include "test.h"
#include "includes.h"

#define TESTDIR "dir." __FILE__ 

static int 
run_test(void) {
    int r;

    // setup the test dir
    system("rm -rf " TESTDIR);
    r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

    // create the log
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);
    BYTESTRING hello  = { strlen("hello"), "hello" };
    r = toku_log_comment(logger, NULL, TRUE, 0, hello);
    r = toku_logger_close(&logger); assert(r == 0);

    // redirect stderr
    int devnul = open("/dev/null", O_WRONLY);
    assert(devnul>=0);
    r = dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // run recovery
    r = tokudb_recover("/junk", TESTDIR, 0, 0, NULL, NULL, NULL, NULL, 0); 
    assert(r != 0);
    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
