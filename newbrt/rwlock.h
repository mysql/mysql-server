/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_RWLOCK_H
#define TOKU_RWLOCK_H
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


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
    toku_pthread_cond_t wait_read;
    int writer;                  // the number of writers
    int want_write;              // the number of blocked writers
    toku_pthread_cond_t wait_write;
};

// initialize a read write lock

static __attribute__((__unused__))
void
rwlock_init(RWLOCK rwlock) {
    int r;
    rwlock->reader = rwlock->want_read = 0;
    r = toku_pthread_cond_init(&rwlock->wait_read, 0); assert(r == 0);
    rwlock->writer = rwlock->want_write = 0;
    r = toku_pthread_cond_init(&rwlock->wait_write, 0); assert(r == 0);
}

// destroy a read write lock

static __attribute__((__unused__))
void
rwlock_destroy(RWLOCK rwlock) {
    int r;
    assert(rwlock->reader == 0 && rwlock->want_read == 0);
    assert(rwlock->writer == 0 && rwlock->want_write == 0);
    r = toku_pthread_cond_destroy(&rwlock->wait_read); assert(r == 0);
    r = toku_pthread_cond_destroy(&rwlock->wait_write); assert(r == 0);
}

// obtain a read lock
// expects: mutex is locked

static inline void rwlock_read_lock(RWLOCK rwlock, toku_pthread_mutex_t *mutex) {
    if (rwlock->writer || rwlock->want_write) {
        rwlock->want_read++;
        while (rwlock->writer || rwlock->want_write) {
            int r = toku_pthread_cond_wait(&rwlock->wait_read, mutex); assert(r == 0);
        }
        rwlock->want_read--;
    }
    rwlock->reader++;
}


// TODO: #1398  Get rid of this hack.
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY

// preferentially obtain a read lock (ignore request for write lock)
// expects: mutex is locked

static inline void rwlock_prefer_read_lock(RWLOCK rwlock, toku_pthread_mutex_t *mutex) {
    if (rwlock->reader)
	rwlock->reader++;
    else
	rwlock_read_lock(rwlock, mutex);
}
#endif


// release a read lock
// expects: mutex is locked

static inline void rwlock_read_unlock(RWLOCK rwlock) {
    rwlock->reader--;
    if (rwlock->reader == 0 && rwlock->want_write) {
        int r = toku_pthread_cond_signal(&rwlock->wait_write); assert(r == 0);
    }
}

// obtain a write lock
// expects: mutex is locked

static inline void rwlock_write_lock(RWLOCK rwlock, toku_pthread_mutex_t *mutex) {
    if (rwlock->reader || rwlock->writer) {
        rwlock->want_write++;
        while (rwlock->reader || rwlock->writer) {
            int r = toku_pthread_cond_wait(&rwlock->wait_write, mutex); assert(r == 0);
        }
        rwlock->want_write--;
    }
    rwlock->writer++;
}

// release a write lock
// expects: mutex is locked

static inline void rwlock_write_unlock(RWLOCK rwlock) {
    rwlock->writer--;
    if (rwlock->writer == 0) {
        if (rwlock->want_write) {
            int r = toku_pthread_cond_signal(&rwlock->wait_write); assert(r == 0);
        } else if (rwlock->want_read) {
            int r = toku_pthread_cond_broadcast(&rwlock->wait_read); assert(r == 0);
        }
    }
}

// returns: the number of readers

static inline int rwlock_readers(RWLOCK rwlock) {
    return rwlock->reader;
}

// returns: the number of writers

static inline int rwlock_writers(RWLOCK rwlock) {
    return rwlock->writer;
}

// returns: the sum of the number of readers, pending readers, writers, and
// pending writers

static inline int rwlock_users(RWLOCK rwlock) {
    return rwlock->reader + rwlock->want_read + rwlock->writer + rwlock->want_write;
}

#endif

