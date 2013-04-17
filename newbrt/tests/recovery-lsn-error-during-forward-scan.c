// force a bad LSN during the forward scan.  recovery should fail.

#include "test.h"
#include "includes.h"

#define TESTDIR "dir." __FILE__ 

static void recover_callback_at_turnaround(void *UU(arg)) {
    // change the LSN in the first log entry of log 2.  this will cause an LSN error during the forward scan.
    int r;
    FILE *f = fopen("log000000000002.tokulog", "r+b"); assert(f);
    r = fseek(f, 025, SEEK_SET); assert(r == 0);
    char c = 100;
    size_t n = fwrite(&c, sizeof c, 1, f); assert(n == sizeof c);
    r = fclose(f); assert(r == 0);
}

static int 
run_test(void) {
    int r;

    // setup the test dir
    system("rm -rf " TESTDIR);
    r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

    // log 1 has the checkpoint
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);

    LSN beginlsn;
    r = toku_log_begin_checkpoint(logger, &beginlsn, TRUE, 0); assert(r == 0);
    r = toku_log_end_checkpoint(logger, NULL, TRUE, beginlsn.lsn, 0); assert(r == 0);

    r = toku_logger_close(&logger); assert(r == 0);

    // log 2 has hello
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);

    BYTESTRING hello  = { strlen("hello"), "hello" };
    r = toku_log_comment(logger, NULL, TRUE, 0, hello); assert(r == 0);

    r = toku_logger_close(&logger); assert(r == 0);

    // log 3 has there
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(TESTDIR, logger); assert(r == 0);

    BYTESTRING there  = { strlen("there"), "there" };
    r = toku_log_comment(logger, NULL, TRUE, 0, there); assert(r == 0);

    r = toku_logger_close(&logger); assert(r == 0);

    // redirect stderr
    int devnul = open(DEV_NULL_FILE, O_WRONLY);
    assert(devnul>=0);
    r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // delete log 2 at the turnaround to force
    toku_recover_set_callback(recover_callback_at_turnaround, NULL);

    // run recovery
    r = tokudb_recover(TESTDIR, TESTDIR, 0, 0, NULL, NULL, 0); 
    assert(r != 0);
    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
