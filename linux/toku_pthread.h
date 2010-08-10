/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#ifndef _TOKU_PTHREAD_H
#define _TOKU_PTHREAD_H

#include <pthread.h>
#include <time.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

typedef pthread_attr_t toku_pthread_attr_t;
typedef pthread_t toku_pthread_t;
typedef pthread_mutexattr_t toku_pthread_mutexattr_t;
typedef pthread_mutex_t toku_pthread_mutex_t;
typedef pthread_condattr_t toku_pthread_condattr_t;
typedef pthread_cond_t toku_pthread_cond_t;
typedef pthread_rwlock_t toku_pthread_rwlock_t;
typedef pthread_rwlockattr_t  toku_pthread_rwlockattr_t;
typedef pthread_key_t toku_pthread_key_t;
typedef struct timespec toku_timespec_t;

static inline int
toku_pthread_rwlock_init(toku_pthread_rwlock_t *__restrict rwlock, const toku_pthread_rwlockattr_t *__restrict attr) {
    return pthread_rwlock_init(rwlock, attr);
}

static inline int
toku_pthread_rwlock_destroy(toku_pthread_rwlock_t *rwlock) {
    return pthread_rwlock_destroy(rwlock);
}

static inline int
toku_pthread_rwlock_rdlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_rwlock_rdlock(rwlock);
}

static inline int
toku_pthread_rwlock_rdunlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_rwlock_unlock(rwlock);
}

static inline int
toku_pthread_rwlock_wrlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_rwlock_wrlock(rwlock);
}

static inline int
toku_pthread_rwlock_wrunlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_rwlock_unlock(rwlock);
}

int 
toku_pthread_yield(void) __attribute__((__visibility__("default")));

static inline int toku_pthread_attr_init(toku_pthread_attr_t *attr) {
    return pthread_attr_init(attr);
}

static inline int 
toku_pthread_attr_destroy(toku_pthread_attr_t *attr) {
    return pthread_attr_destroy(attr);
}

static inline int 
toku_pthread_attr_getstacksize(toku_pthread_attr_t *attr, size_t *stacksize) {
    return pthread_attr_getstacksize(attr, stacksize);
}

static inline int 
toku_pthread_attr_setstacksize(toku_pthread_attr_t *attr, size_t stacksize) {
    return pthread_attr_setstacksize(attr, stacksize);
}

static inline int 
toku_pthread_create(toku_pthread_t *thread, const toku_pthread_attr_t *attr, void *(*start_function)(void *), void *arg) {
    return pthread_create(thread, attr, start_function, arg);
}

static inline int 
toku_pthread_join(toku_pthread_t thread, void **value_ptr) {
    return pthread_join(thread, value_ptr);
}

static inline toku_pthread_t 
toku_pthread_self(void) {
    return pthread_self();
}

#define TOKU_PTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline int 
toku_pthread_mutex_init(toku_pthread_mutex_t *mutex, const toku_pthread_mutexattr_t *attr) {
    return pthread_mutex_init(mutex, attr);
}

static inline int 
toku_pthread_mutex_destroy(toku_pthread_mutex_t *mutex) {
    return pthread_mutex_destroy(mutex);
}

static inline int 
toku_pthread_mutex_lock(toku_pthread_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

static inline int 
toku_pthread_mutex_trylock(toku_pthread_mutex_t *mutex) {
    return pthread_mutex_trylock(mutex);
}

static inline int 
toku_pthread_mutex_unlock(toku_pthread_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

static inline int 
toku_pthread_cond_init(toku_pthread_cond_t *cond, const toku_pthread_condattr_t *attr) {
    return pthread_cond_init(cond, attr);
}

static inline int 
toku_pthread_cond_destroy(toku_pthread_cond_t *cond) {
    return pthread_cond_destroy(cond);
}

static inline int 
toku_pthread_cond_wait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex) {
    return pthread_cond_wait(cond, mutex);
}

static inline int 
toku_pthread_cond_timedwait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex, toku_timespec_t *wakeup_at) {
    return pthread_cond_timedwait(cond, mutex, wakeup_at);
}

static inline int 
toku_pthread_cond_signal(toku_pthread_cond_t *cond) {
    return pthread_cond_signal(cond);
}

static inline int 
toku_pthread_cond_broadcast(toku_pthread_cond_t *cond) {
    return pthread_cond_broadcast(cond);
}

static inline int 
toku_pthread_key_create(toku_pthread_key_t *key, void (*destroyf)(void *)) {
    return pthread_key_create(key, destroyf);
}

static inline int 
toku_pthread_key_delete(toku_pthread_key_t key) {
    return pthread_key_delete(key);
}

static inline void *
toku_pthread_getspecific(toku_pthread_key_t key) {
    return pthread_getspecific(key);
}

static inline int 
toku_pthread_setspecific(toku_pthread_key_t key, void *data) {
    return pthread_setspecific(key, data);
}

// Fair readers/writer locks.  These are fair (meaning first-come first-served.  No reader starvation, and no writer starvation).  And they are
// probably faster than the linux readers/writer locks (pthread_rwlock_t).
struct toku_fair_rwlock_waiter_state; // this structure is used internally.
typedef struct toku_fair_rwlock_s {
    toku_pthread_mutex_t                  mutex;
    int                                   state; // 0 means no locks, + is number of readers locked, -1 is a writer
    struct toku_fair_rwlock_waiter_state *waiters_head, *waiters_tail;
} toku_fair_rwlock_t;

int toku_fair_rwlock_init (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_destroy (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_rdlock (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_wrlock (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_unlock (toku_fair_rwlock_t *rwlock);


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
