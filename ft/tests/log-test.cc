/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int r;
    long long lognum;
    char logname[TOKU_PATH_MAX+1];
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);    assert(r==0);
    r = toku_logger_find_next_unused_log_file(TOKU_TEST_FILENAME,&lognum);
    assert(r==0 && lognum==0LL);

    mode_t mode = S_IRWXU + S_IRWXG + S_IRWXO;
    sprintf(logname, "%s/log01.tokulog%d", TOKU_TEST_FILENAME, TOKU_LOG_VERSION);
    r = open(logname, O_WRONLY + O_CREAT + O_BINARY, mode); assert(r>=0);
    r = close(r); assert(r==0);

    r = toku_logger_find_next_unused_log_file(TOKU_TEST_FILENAME,&lognum);
    assert(r==0 && lognum==2LL);
    
    sprintf(logname, "%s/log123456789012345.tokulog%d", TOKU_TEST_FILENAME, TOKU_LOG_VERSION);
    r = open(logname, O_WRONLY + O_CREAT + O_BINARY, mode); assert(r>=0);
    r = close(r); assert(r==0);
    r = toku_logger_find_next_unused_log_file(TOKU_TEST_FILENAME,&lognum);
    assert(r==0 && lognum==123456789012346LL);

    sprintf(logname, "%s/log3.tokulog%d", TOKU_TEST_FILENAME, TOKU_LOG_VERSION);
    r = open(logname, O_WRONLY + O_CREAT + O_BINARY, mode); assert(r>=0);
    r = close(r); assert(r==0);
    r = toku_logger_find_next_unused_log_file(TOKU_TEST_FILENAME,&lognum);
    assert(r==0 && lognum==123456789012346LL);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    return 0;
}

