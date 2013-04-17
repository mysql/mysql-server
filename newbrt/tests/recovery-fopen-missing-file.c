// test recovery with a fopen that references a missing file

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
    BYTESTRING iname  = { strlen("missing_tokudb_file"), "missing_tokudb_file" };
    FILENUM filenum = {42};
    uint32_t treeflags = 0;
    r = toku_log_fopen(logger, NULL, TRUE, iname, filenum, treeflags);
    r = toku_logger_close(&logger); assert(r == 0);

    // run recovery
    r = tokudb_recover(TESTDIR, TESTDIR, 0, 0, 0); 
    assert(r == 0);
    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
