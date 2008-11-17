/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
  \file elocks.c
  \brief Ephemeral locks

   The ydb big lock serializes access to the tokudb
   every call (including methods) into the tokudb library gets the lock 
   no internal function should invoke a method through an object */

#include "toku_portability.h"
#include "ydb-internal.h"
#include <assert.h>
#include <toku_pthread.h>
#include <sys/types.h>

static toku_pthread_mutex_t ydb_big_lock = TOKU_PTHREAD_MUTEX_INITIALIZER;

void toku_ydb_lock_init(void) {
    int r = toku_pthread_mutex_init(&ydb_big_lock, NULL); assert(r == 0);
}

void toku_ydb_lock_destroy(void) {
    int r = toku_pthread_mutex_destroy(&ydb_big_lock); assert(r == 0);
}

void toku_ydb_lock(void) {
    int r = toku_pthread_mutex_lock(&ydb_big_lock);   assert(r == 0);
}

void toku_ydb_unlock(void) {
    int r = toku_pthread_mutex_unlock(&ydb_big_lock); assert(r == 0);
}

