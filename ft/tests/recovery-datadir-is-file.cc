/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test recovery with no data directory 

#include "test.h"

static int 
run_test(void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert_zero(r);

    char testdir[TOKU_PATH_MAX+1];
    char testfile[TOKU_PATH_MAX+1];
    toku_path_join(testdir, 2, TOKU_TEST_FILENAME, "dir");
    toku_path_join(testfile, 2, TOKU_TEST_FILENAME, "file");

    // setup the test dir
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU); assert(r == 0);
    r = toku_os_mkdir(testdir, S_IRWXU); assert(r == 0);

    // create the log
    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_open(testdir, logger); assert(r == 0);
    BYTESTRING hello  = { (uint32_t) strlen("hello"), (char *) "hello" };
    toku_log_comment(logger, NULL, true, 0, hello);
    r = toku_logger_close(&logger); assert(r == 0);

    // redirect stderr
    int devnul = open(DEV_NULL_FILE, O_WRONLY);
    assert(devnul>=0);
    r = toku_dup2(devnul, fileno(stderr)); 	    assert(r==fileno(stderr));
    r = close(devnul);                      assert(r==0);

    // run recovery
    {
        char buf[TOKU_PATH_MAX+sizeof("touch ")];
        strcpy(buf, "touch ");
        strncat(buf, testfile, TOKU_PATH_MAX);
        r = system(buf); CKERR(r);
    }
    r = tokudb_recover(NULL,
		       NULL_prepared_txn_callback,
		       NULL_keep_cachetable_callback,
		       NULL_logger,
		       testfile, testdir, 0, 0, 0, NULL, 0); 
    assert(r != 0);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    return 0;
}

int
test_main(int UU(argc), const char *UU(argv[])) {
    int r;
    r = run_test();
    return r;
}
