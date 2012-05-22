/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_NBMUTEX_H
#define TOKU_NBMUTEX_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

#include "rwlock.h"

//Use case:
// General purpose non blocking mutex with properties:
// 1. one writer at a time

// An external mutex must be locked when using these functions.  An alternate
// design would bury a mutex into the nb_mutex itself.  While this may
// increase parallelism at the expense of single thread performance, we
// are experimenting with a single higher level lock.

typedef struct nb_mutex *NB_MUTEX;
struct nb_mutex {
    struct rwlock lock;
};

// initialize an nb mutex
static __attribute__((__unused__))
void
nb_mutex_init(NB_MUTEX nb_mutex) {
    rwlock_init(&nb_mutex->lock);
}

// destroy a read write lock
static __attribute__((__unused__))
void
nb_mutex_destroy(NB_MUTEX nb_mutex) {
    rwlock_destroy(&nb_mutex->lock);
}

// obtain a write lock
// expects: mutex is locked
static inline void nb_mutex_lock(NB_MUTEX nb_mutex, 
        toku_mutex_t *mutex) {
    rwlock_write_lock(&nb_mutex->lock, mutex);
}

// release a write lock
// expects: mutex is locked

static inline void nb_mutex_write_unlock(NB_MUTEX nb_mutex) {
    rwlock_write_unlock(&nb_mutex->lock);
}

// returns: the number of writers who are waiting for the lock

static inline int nb_mutex_blocked_writers(NB_MUTEX nb_mutex) {
    return rwlock_blocked_writers(&nb_mutex->lock);
}

// returns: the number of writers

static inline int nb_mutex_writers(NB_MUTEX nb_mutex) {
    return rwlock_writers(&nb_mutex->lock);
}

// returns: the sum of the number of readers, pending readers, 
// writers, and pending writers
static inline int nb_mutex_users(NB_MUTEX nb_mutex) {
    return rwlock_users(&nb_mutex->lock);
}

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

