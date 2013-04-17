/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_RWLOCK_H
#define TOKU_RWLOCK_H
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>


//Use case:
// A read lock is acquired by threads that get and pin an entry in the
// cachetable. A write lock is acquired by the writer thread when an entry
// is evicted from the cachetable and is being written storage.

//Use case:
// General purpose reader writer lock with properties:
// 1. multiple readers, no writers
// 2. one writer at a time
// 3. pending writers have priority over pending readers

// An external mutex must be locked when using these functions.  An alternate
// design would bury a mutex into the rwlock itself.  While this may
// increase parallelism at the expense of single thread performance, we
// are experimenting with a single higher level lock.

typedef struct rwlock *RWLOCK;
struct rwlock {
    int reader;                  // the number of readers
    int want_read;                // the number of blocked readers
    toku_cond_t wait_read;
    int writer;                  // the number of writers
    int want_write;              // the number of blocked writers
    toku_cond_t wait_write;
    toku_cond_t* wait_users_go_to_zero;
};

// returns: the sum of the number of readers, pending readers, writers, and
// pending writers

static inline int rwlock_users(RWLOCK rwlock) {
    return rwlock->reader + rwlock->want_read + rwlock->writer + rwlock->want_write;
}

// initialize a read write lock

static __attribute__((__unused__))
void
rwlock_init(RWLOCK rwlock) {
    rwlock->reader = rwlock->want_read = 0;
    toku_cond_init(&rwlock->wait_read, 0);
    rwlock->writer = rwlock->want_write = 0;
    toku_cond_init(&rwlock->wait_write, 0);
    rwlock->wait_users_go_to_zero = NULL;
}

// destroy a read write lock

static __attribute__((__unused__))
void
rwlock_destroy(RWLOCK rwlock) {
    assert(rwlock->reader == 0 && rwlock->want_read == 0);
    assert(rwlock->writer == 0 && rwlock->want_write == 0);
    toku_cond_destroy(&rwlock->wait_read);
    toku_cond_destroy(&rwlock->wait_write);
}

// obtain a read lock
// expects: mutex is locked

static inline void rwlock_read_lock(RWLOCK rwlock, toku_mutex_t *mutex) {
    assert(!rwlock->wait_users_go_to_zero);
    if (rwlock->writer || rwlock->want_write) {
        rwlock->want_read++;
        while (rwlock->writer || rwlock->want_write) {
            toku_cond_wait(&rwlock->wait_read, mutex);
        }
        rwlock->want_read--;
    }
    rwlock->reader++;
}

// release a read lock
// expects: mutex is locked

static inline void rwlock_read_unlock(RWLOCK rwlock) {
    assert(rwlock->reader > 0);
    assert(rwlock->writer == 0);
    rwlock->reader--;
    if (rwlock->reader == 0 && rwlock->want_write) {
        toku_cond_signal(&rwlock->wait_write);
    }
    if (rwlock->wait_users_go_to_zero && rwlock_users(rwlock) == 0) {
        toku_cond_signal(rwlock->wait_users_go_to_zero);
    }
}

// obtain a write lock
// expects: mutex is locked

static inline void rwlock_write_lock(RWLOCK rwlock, toku_mutex_t *mutex) {
    assert(!rwlock->wait_users_go_to_zero);
    if (rwlock->reader || rwlock->writer) {
        rwlock->want_write++;
        while (rwlock->reader || rwlock->writer) {
            toku_cond_wait(&rwlock->wait_write, mutex);
        }
        rwlock->want_write--;
    }
    rwlock->writer++;
}

// release a write lock
// expects: mutex is locked

static inline void rwlock_write_unlock(RWLOCK rwlock) {
    assert(rwlock->reader == 0);
    assert(rwlock->writer == 1);
    rwlock->writer--;
    if (rwlock->want_write) {
        toku_cond_signal(&rwlock->wait_write);
    } else if (rwlock->want_read) {
        toku_cond_broadcast(&rwlock->wait_read);
    }    
    if (rwlock->wait_users_go_to_zero && rwlock_users(rwlock) == 0) {
        toku_cond_signal(rwlock->wait_users_go_to_zero);
    }
}

// returns: the number of readers

static inline int rwlock_readers(RWLOCK rwlock) {
    return rwlock->reader;
}

// returns: the number of readers who are waiting for the lock

static inline int rwlock_blocked_readers(RWLOCK rwlock) {
    return rwlock->want_read;
}

// returns: the number of writers who are waiting for the lock

static inline int rwlock_blocked_writers(RWLOCK rwlock) {
    return rwlock->want_write;
}

// returns: the number of writers

static inline int rwlock_writers(RWLOCK rwlock) {
    return rwlock->writer;
}

static inline void rwlock_wait_for_users(
    RWLOCK rwlock, 
    toku_mutex_t *mutex
    ) 
{
    assert(!rwlock->wait_users_go_to_zero);
    toku_cond_t cond;
    toku_cond_init(&cond, NULL);
    while (rwlock_users(rwlock) > 0) {
        rwlock->wait_users_go_to_zero = &cond;
        toku_cond_wait(&cond, mutex);
    }
    rwlock->wait_users_go_to_zero = NULL;
    toku_cond_destroy(&cond);
}


#endif

