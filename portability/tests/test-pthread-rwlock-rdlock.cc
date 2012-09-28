/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <test.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>

int test_main(int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__))) {
    toku_pthread_rwlock_t rwlock;
    ZERO_STRUCT(rwlock);

    toku_pthread_rwlock_init(&rwlock, NULL);
    toku_pthread_rwlock_rdlock(&rwlock);
    toku_pthread_rwlock_rdlock(&rwlock);
    toku_pthread_rwlock_rdunlock(&rwlock);
    toku_pthread_rwlock_rdunlock(&rwlock);
    toku_pthread_rwlock_destroy(&rwlock);

    return 0;
}

