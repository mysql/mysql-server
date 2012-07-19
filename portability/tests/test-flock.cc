/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <toku_assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include "toku_portability.h"

int main(void) {
    int fd = toku_os_lock_file(__FILE__);
    assert(fd != -1);
    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0) {
        int fd2 = toku_os_lock_file(__FILE__);
        assert(fd2 == -1);
	return 0;
    } else {
        int status;
        pid_t wpid = waitpid(-1, &status, 0);
	assert(wpid == pid);
	assert(status == 0);
    }

    int r = toku_os_unlock_file(fd);
    assert(r == 0);

    return 0;
}
