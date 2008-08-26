/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
  \file elocks.c
  \brief Ephemeral locks

   The ydb big lock serializes access to the tokudb
   every call (including methods) into the tokudb library gets the lock 
   no internal function should invoke a method through an object */

#include <assert.h>
#include <pthread.h>
#include <sys/types.h>

#ifdef __CYGWIN__
#include <windows.h>
#include <winbase.h>
CRITICAL_SECTION ydb_big_lock;

void toku_ydb_lock(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized=1;
        InitializeCriticalSection(&ydb_big_lock);
    }
    EnterCriticalSection(&ydb_big_lock);
}

void toku_ydb_unlock(void) {
    LeaveCriticalSection(&ydb_big_lock);
}

#else //Not Cygwin
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
static pthread_mutex_t ydb_big_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
static pthread_mutex_t ydb_big_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

void toku_ydb_lock(void) {
    int r = pthread_mutex_lock(&ydb_big_lock);   assert(r == 0);
}

void toku_ydb_unlock(void) {
    int r = pthread_mutex_unlock(&ydb_big_lock); assert(r == 0);
}
#endif

