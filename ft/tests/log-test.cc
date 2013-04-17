/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"
#if defined(HAVE_LIMITS_H)
# include <limits.h>
#endif
#if defined(HAVE_SYS_SYSLIMITS_H)
# include <sys/syslimits.h>
#endif

#ifndef dname
#define dname __SRCFILE__ ".dir"
#endif
#define rmrf "rm -rf " dname "/"

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int r;
    long long lognum;
    char logname[PATH_MAX];
    r = system(rmrf);
    CKERR(r);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);
    r = toku_logger_find_next_unused_log_file(dname,&lognum);
    assert(r==0 && lognum==0LL);

    mode_t mode = S_IRWXU + S_IRWXG + S_IRWXO;
    sprintf(logname, dname "/log01.tokulog%d", TOKU_LOG_VERSION);
    r = open(logname, O_WRONLY + O_CREAT + O_BINARY, mode); assert(r>=0);
    r = close(r); assert(r==0);

    r = toku_logger_find_next_unused_log_file(dname,&lognum);
    assert(r==0 && lognum==2LL);
    
    sprintf(logname, dname "/log123456789012345.tokulog%d", TOKU_LOG_VERSION);
    r = open(logname, O_WRONLY + O_CREAT + O_BINARY, mode); assert(r>=0);
    r = close(r); assert(r==0);
    r = toku_logger_find_next_unused_log_file(dname,&lognum);
    assert(r==0 && lognum==123456789012346LL);

    sprintf(logname, dname "/log3.tokulog%d", TOKU_LOG_VERSION);
    r = open(logname, O_WRONLY + O_CREAT + O_BINARY, mode); assert(r>=0);
    r = close(r); assert(r==0);
    r = toku_logger_find_next_unused_log_file(dname,&lognum);
    assert(r==0 && lognum==123456789012346LL);

    r = system(rmrf);
    CKERR(r);

    return 0;
}

