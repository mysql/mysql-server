/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#ifndef _TOKU_PTHREAD_H
#define _TOKU_PTHREAD_H

#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include "toku_assert.h"

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

#define TOKU_PTHREAD_DEBUG 0

typedef struct toku_mutex {
    pthread_mutex_t pmutex;
    bool locked;
#if TOKU_PTHREAD_DEBUG
    pthread_t owner; // = pthread_self(); // for debugging
#endif
} toku_mutex_t;

static inline void
toku_mutex_init(toku_mutex_t *mutex, const toku_pthread_mutexattr_t *attr) {
    int r = pthread_mutex_init(&mutex->pmutex, attr);
    assert_zero(r);
    mutex->locked = false;
}

static inline void
toku_mutex_destroy(toku_mutex_t *mutex) {
    assert(!mutex->locked);
    int r = pthread_mutex_destroy(&mutex->pmutex);
    assert_zero(r);
}

static inline void
toku_mutex_lock(toku_mutex_t *mutex) {
    int r = pthread_mutex_lock(&mutex->pmutex);
    assert_zero(r);
    assert(!mutex->locked);
    mutex->locked = true;
#if TOKU_PTHREAD_DEBUG
    mutex->owner = pthread_self();
#endif
}

static inline void
toku_mutex_unlock(toku_mutex_t *mutex) {
    assert(mutex->locked);
    mutex->locked = false;
#if TOKU_PTHREAD_DEBUG
    mutex->owner = 0;
#endif
    int r = pthread_mutex_unlock(&mutex->pmutex);
    assert_zero(r);
}

static inline bool
toku_mutex_is_locked(toku_mutex_t *mutex) {
    return mutex->locked;
}

typedef struct toku_cond {
    pthread_cond_t pcond;
} toku_cond_t;

static inline void
toku_cond_init(toku_cond_t *cond, const toku_pthread_condattr_t *attr) {
    int r = pthread_cond_init(&cond->pcond, attr);
    assert_zero(r);
}

static inline void
toku_cond_destroy(toku_cond_t *cond) {
    int r = pthread_cond_destroy(&cond->pcond);
    assert_zero(r);
}

static inline void
toku_cond_wait(toku_cond_t *cond, toku_mutex_t *mutex) {
    assert(mutex->locked);
    mutex->locked = false;
#if TOKU_PTHREAD_DEBUG
    mutex->owner = 0;
#endif
    int r = pthread_cond_wait(&cond->pcond, &mutex->pmutex);
    assert_zero(r);
    assert(!mutex->locked);
    mutex->locked = true;
#if TOKU_PTHREAD_DEBUG
    mutex->owner = pthread_self();
#endif
}

static inline int 
toku_cond_timedwait(toku_cond_t *cond, toku_mutex_t *mutex, toku_timespec_t *wakeup_at) {
    assert(mutex->locked);
    mutex->locked = false;
#if TOKU_PTHREAD_DEBUG
    mutex->owner = 0;
#endif
    int r = pthread_cond_timedwait(&cond->pcond, &mutex->pmutex, wakeup_at);
    assert(!mutex->locked);
    mutex->locked = true;
#if TOKU_PTHREAD_DEBUG
    mutex->owner = pthread_self();
#endif
    return r;
}

static inline void 
toku_cond_signal(toku_cond_t *cond) {
    pthread_cond_signal(&cond->pcond);
}

static inline void
toku_cond_broadcast(toku_cond_t *cond) {
    pthread_cond_broadcast(&cond->pcond);
}

int 
toku_pthread_yield(void) __attribute__((__visibility__("default")));

static inline toku_pthread_t 
toku_pthread_self(void) {
    return pthread_self();
}

#if 1
static inline void
toku_pthread_rwlock_init(toku_pthread_rwlock_t *__restrict rwlock, const toku_pthread_rwlockattr_t *__restrict attr) {
    int r = pthread_rwlock_init(rwlock, attr);
    assert_zero(r);
}

static inline void
toku_pthread_rwlock_destroy(toku_pthread_rwlock_t *rwlock) {
    int r = pthread_rwlock_destroy(rwlock);
    assert_zero(r);
}

static inline void
toku_pthread_rwlock_rdlock(toku_pthread_rwlock_t *rwlock) {
    int r = pthread_rwlock_rdlock(rwlock);
    assert_zero(r);
}

static inline void
toku_pthread_rwlock_rdunlock(toku_pthread_rwlock_t *rwlock) {
    int r = pthread_rwlock_unlock(rwlock);
    assert_zero(r);
}

static inline void
toku_pthread_rwlock_wrlock(toku_pthread_rwlock_t *rwlock) {
    int r = pthread_rwlock_wrlock(rwlock);
    assert_zero(r);
}

static inline void
toku_pthread_rwlock_wrunlock(toku_pthread_rwlock_t *rwlock) {
    int r = pthread_rwlock_unlock(rwlock);
    assert_zero(r);
}
#endif

static inline int 
toku_pthread_create(toku_pthread_t *thread, const toku_pthread_attr_t *attr, void *(*start_function)(void *), void *arg) {
    return pthread_create(thread, attr, start_function, arg);
}

static inline int 
toku_pthread_join(toku_pthread_t thread, void **value_ptr) {
    return pthread_join(thread, value_ptr);
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

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif
