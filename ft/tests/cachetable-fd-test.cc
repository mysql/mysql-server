/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"


static void
cachetable_fd_test (void) {
    const int test_limit = 1;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert_zero(r);
    char fname1[TOKU_PATH_MAX+1];
    unlink(toku_path_join(fname1, 2, TOKU_TEST_FILENAME, "test1.dat"));
    CACHEFILE cf;
    r = toku_cachetable_openf(&cf, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    int fd1 = toku_cachefile_get_fd(cf); assert(fd1 >= 0);

    // test set to good fd succeeds
    char fname2[TOKU_PATH_MAX+1];
    unlink(toku_path_join(fname2, 2, TOKU_TEST_FILENAME, "test2.dat"));
    int fd2 = open(fname2, O_RDWR | O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd2 >= 0 && fd1 != fd2);
    r = toku_cachefile_set_fd(cf, fd2, fname2); assert(r == 0);
    assert(toku_cachefile_get_fd(cf) == fd2);

    // test set to bogus fd fails
    int fd3 = open(DEV_NULL_FILE, O_RDWR); assert(fd3 >= 0);
    r = close(fd3); assert(r == 0);
    r = toku_cachefile_set_fd(cf, fd3, DEV_NULL_FILE); assert(r != 0);
    assert(toku_cachefile_get_fd(cf) == fd2);

    // test the filenum functions
    FILENUM fn = toku_cachefile_filenum(cf);
    CACHEFILE newcf = 0;
    r = toku_cachefile_of_filenum(ct, fn, &newcf);
    assert(r == 0 && cf == newcf);

    // test a bogus filenum
    fn.fileid++;
    r = toku_cachefile_of_filenum(ct, fn, &newcf);
    assert(r == ENOENT);

    toku_cachefile_close(&cf, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    toku_os_initialize_settings(verbose);

    cachetable_fd_test();
    return 0;
}
