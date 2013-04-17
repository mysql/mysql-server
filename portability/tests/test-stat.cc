/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <toku_assert.h>
#include <toku_portability.h>

static void test_stat(const char *dirname, int result, int ex_errno) {
    int r;
    toku_struct_stat buf;
    r = toku_stat(dirname, &buf);
    //printf("stat %s %d %d\n", dirname, r, errno); fflush(stdout);
    assert(r==result);
    if (r!=0) assert(get_maybe_error_errno() == ex_errno);
}

int main(void) {
    int r;

    test_stat(".", 0, 0);
    test_stat("./", 0, 0);

    r = system("rm -rf testdir"); assert(r==0);
    test_stat("testdir", -1, ENOENT);
    test_stat("testdir/", -1, ENOENT);
    test_stat("testdir/foo", -1, ENOENT);
    test_stat("testdir/foo/", -1, ENOENT);
    r = toku_os_mkdir("testdir", S_IRWXU);
    assert(r == 0);
    test_stat("testdir/foo", -1, ENOENT);
    test_stat("testdir/foo/", -1, ENOENT);
    r = system("touch testdir/foo"); assert(r==0);
    test_stat("testdir/foo", 0, 0);
    test_stat("testdir/foo/", -1, ENOTDIR);

    test_stat("testdir", 0, 0);

    test_stat("./testdir", 0, 0);

    test_stat("./testdir/", 0, 0);

    test_stat("/", 0, 0);

    test_stat("/usr", 0, 0);
    test_stat("/usr/", 0, 0);

    return 0;
}
