/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#define _GNU_SOURCE 1
#include <config.h>
#include <toku_pthread.h>

int toku_pthread_yield(void) {
#if defined(HAVE_PTHREAD_YIELD)
# if defined(PTHREAD_YIELD_RETURNS_INT)
    return pthread_yield();
# elif defined(PTHREAD_YIELD_RETURNS_VOID)
    pthread_yield();
    return 0;
# else
#  error "don't know what pthread_yield() returns"
# endif
#elif defined(HAVE_PTHREAD_YIELD_NP)
    pthread_yield_np();
    return 0;
#else
# error "cannot find pthread_yield or pthread_yield_np"
#endif
}
