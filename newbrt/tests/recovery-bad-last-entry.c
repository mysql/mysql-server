// test recovery of "hello" comments

#include "test.h"
#include "includes.h"

#define TESTDIR "dir." __FILE__ 

static const int magic_begin_end_checkpoint_sz = 77;  // leave this many bytes in file

static int 
run_test(void) {
    int r;
    int trim = 1;
    struct stat st;

    while ( 1 ) {
        // setup the test dir
        system("rm -rf " TESTDIR);
        r = toku_os_mkdir(TESTDIR, S_IRWXU); assert(r == 0);

        // create the log
        TOKULOGGER logger;
        BYTESTRING hello  = { strlen("hello"), "hello" };
        BYTESTRING world  = { strlen("world"), "world" };
        BYTESTRING there  = { strlen("there"), "there" };
        r = toku_logger_create(&logger); assert(r == 0);
        r = toku_logger_open(TESTDIR, logger); assert(r == 0);
        LSN beginlsn;
        // all logs must contain a valid checkpoint
        r = toku_log_begin_checkpoint(logger, &beginlsn, TRUE, 0); assert(r == 0);
        r = toku_log_end_checkpoint(logger, NULL, TRUE, beginlsn.lsn, 0); assert(r == 0);

        r = toku_log_comment(logger, NULL, TRUE, 0, hello); assert(r == 0);
        r = toku_log_comment(logger, NULL, TRUE, 0, world); assert(r == 0);
        r = toku_log_begin_checkpoint(logger, &beginlsn, TRUE, 0); assert(r == 0);
        r = toku_log_end_checkpoint(logger, NULL, TRUE, beginlsn.lsn, 0); assert(r == 0);
        r = toku_log_comment(logger, NULL, TRUE, 0, hello); assert(r == 0);
        r = toku_log_comment(logger, NULL, TRUE, 0, there); assert(r == 0);
        r = toku_logger_close(&logger); assert(r == 0);

        // redirect stderr
        int devnul = open(DEV_NULL_FILE, O_WRONLY);
        assert(devnul>=0);
        r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
        r = close(devnul);                      assert(r==0);

        char fname[256];
        sprintf(fname, "%s/%s", TESTDIR, "log000000000000.tokulog");

        r = toku_stat(fname, &st); assert(r==0);
        if ( st.st_size - trim > magic_begin_end_checkpoint_sz )
            truncate(fname, st.st_size - trim);
        else
            break;
        // run recovery
        r = tokudb_recover(TESTDIR, TESTDIR, 0, 0, NULL, NULL, 0); 
        assert(r == 0);
        
        trim += 1;
    }
    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
