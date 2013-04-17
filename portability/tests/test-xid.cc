/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <config.h>
#include <stdio.h>
#include <toku_stdint.h>
#include <unistd.h>
#include <toku_assert.h>
#include "toku_os.h"
#if defined(HAVE_SYSCALL_H)
# include <syscall.h>
#elif defined(HAVE_SYS_SYSCALL_H)
# include <sys/syscall.h>
# if !defined(__NR_gettid) && defined(SYS_gettid)
#  define __NR_gettid SYS_gettid
# endif
#endif

static int gettid(void) {
    return syscall(__NR_gettid);
}

int main(void) {
    assert(toku_os_getpid() == getpid());
    assert(toku_os_gettid() == gettid());
    return 0;
}
