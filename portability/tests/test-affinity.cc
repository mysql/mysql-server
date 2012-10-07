/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdio.h>
#include <toku_assert.h>
#include <toku_os.h>
#include "toku_affinity.h"

int main(void) {
    int r;
    toku_cpuset_t orig;
    TOKU_CPU_ZERO(&orig);
    r = toku_getaffinity(toku_os_getpid(), sizeof orig, &orig);
    if (r) {
        perror("toku_getaffinity");
        return r;
    }
    toku_cpuset_t set;
    TOKU_CPU_ZERO(&set);
    TOKU_CPU_SET(0, &set);
    r = toku_setaffinity(toku_os_getpid(), sizeof set, &set);
    if (r) {
        perror("toku_setaffinity");
        return r;
    }
    toku_cpuset_t chk;
    TOKU_CPU_ZERO(&chk);
    r = toku_getaffinity(toku_os_getpid(), sizeof chk, &chk);
    if (r) {
        perror("toku_getaffinity");
        return r;
    }

    // don't want to expose this api unless we use it somewhere
#if defined(HAVE_SCHED_GETAFFINITY) || defined(HAVE_CPUSET_GETAFFINITY)
    r = CPU_CMP(&set, &chk);
#else
    r = memcmp(&set, &chk, sizeof set);
#endif

    return r;
}
