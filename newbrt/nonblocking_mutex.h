/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_NBMUTEX_H
#define TOKU_NBMUTEX_H
#ident "$Id: rwlock.h 32279 2011-06-29 13:51:57Z bkuszmaul $"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

//Use case:

//Use case:
// General purpose non blocking mutex with properties:
// 1. one writer at a time

// An external mutex must be locked when using these functions.  An alternate
// design would bury a mutex into the nb_mutex itself.  While this may
// increase parallelism at the expense of single thread performance, we
// are experimenting with a single higher level lock.

typedef struct nb_mutex *NB_MUTEX;
struct nb_mutex {
    int writer;                  // the number of writers
    int want_write;              // the number of blocked writers
    toku_pthread_cond_t wait_write;
};

// initialize a read write lock

static __attribute__((__unused__))
void
nb_mutex_init(NB_MUTEX nb_mutex) {
    int r;
    nb_mutex->writer = nb_mutex->want_write = 0;
    r = toku_pthread_cond_init(&nb_mutex->wait_write, 0); assert(r == 0);
}

// destroy a read write lock

static __attribute__((__unused__))
void
nb_mutex_destroy(NB_MUTEX nb_mutex) {
    int r;
    assert(nb_mutex->writer == 0 && nb_mutex->want_write == 0);
    r = toku_pthread_cond_destroy(&nb_mutex->wait_write); assert(r == 0);
}


// obtain a write lock
// expects: mutex is locked

static inline void nb_mutex_write_lock(NB_MUTEX nb_mutex, toku_pthread_mutex_t *mutex) {
    if (nb_mutex->writer) {
        nb_mutex->want_write++;
        while (nb_mutex->writer) {
            int r = toku_pthread_cond_wait(&nb_mutex->wait_write, mutex); assert(r == 0);
        }
        nb_mutex->want_write--;
    }
    nb_mutex->writer++;
}

// release a write lock
// expects: mutex is locked

static inline void nb_mutex_write_unlock(NB_MUTEX nb_mutex) {
    assert(nb_mutex->writer == 1);
    nb_mutex->writer--;
    if (nb_mutex->want_write) {
        int r = toku_pthread_cond_signal(&nb_mutex->wait_write); assert(r == 0);
    } 
}

// returns: the number of writers who are waiting for the lock

static inline int nb_mutex_blocked_writers(NB_MUTEX nb_mutex) {
    return nb_mutex->want_write;
}

// returns: the number of writers

static inline int nb_mutex_writers(NB_MUTEX nb_mutex) {
    return nb_mutex->writer;
}

// returns: the sum of the number of readers, pending readers, writers, and
// pending writers

static inline int nb_mutex_users(NB_MUTEX nb_mutex) {
    return nb_mutex->writer + nb_mutex->want_write;
}

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

