/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#ifndef _TOKU_PTHREAD_H
#define _TOKU_PTHREAD_H

#include <pthread.h>
#include <time.h>
#include <stdint.h>

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
    // Try to put enough state into STATE so that in many cases, a compare-and-swap will work.
    // The 64-bit state bits are as follows:
    //    bit 0:      1 iff someone has exclusive ownership of the entire state.  (This is a spin lock).
    //    bit 1:      1 iff someone holds a write lock.
    //    bit 2:      1 iff the queue is not empty (if the queue is not empty, then you cannot use the fast path.)
    //    bits 3--31  how many read locks are held.
    uint64_t volatile                     state;
    // The waiters each provide a condition variable.  This is the mutex they are all using.
    // If anyone holds this mutex, they must set the RWS_MUTEXED bit first.  Then they grab the mutex.  Then they clear the bit indicating that they don't have the mutex.  No one else can change anything while the RWS_MUTEXED bit is set.
    toku_pthread_mutex_t                  mutex;
    struct toku_fair_rwlock_waiter_state *waiters_head, *waiters_tail;
} toku_fair_rwlock_t;

#define RWS_WLOCK_MASK 1LL

#define RWS_QCOUNT_OFF  1
#define RWS_QCOUNT_LEN  31
#define RWS_QCOUNT_INCR 2LL
#define RWS_QCOUNT_MASK (((1L<<RWS_QCOUNT_LEN)-1)<<RWS_QCOUNT_OFF)

#define RWS_RCOUNT_OFF  (RWS_QCOUNT_OFF+RWS_QCOUNT_LEN)
#define RWS_RCOUNT_LEN  31
#define RWS_RCOUNT_INCR (1LL<<32)

static inline int s_get_wlock(uint64_t s) {
    return (s&RWS_WLOCK_MASK)!=0;
}
static inline unsigned int s_get_qcount(uint64_t s) {
    return (s>>RWS_QCOUNT_OFF)&((1LL<<RWS_QCOUNT_LEN)-1);
}
static inline unsigned int s_get_rcount(uint64_t s) {
    return (s>>RWS_RCOUNT_OFF)&((1LL<<RWS_RCOUNT_LEN)-1);
}

static inline uint64_t s_set_wlock (uint64_t s) {
    return s | RWS_WLOCK_MASK;
}
static inline uint64_t s_clear_wlock (uint64_t s) {
    return s & ~RWS_WLOCK_MASK;
}
static inline uint64_t s_incr_qcount (uint64_t s) {
    //printf("%s:%d (%s) s=%lx, get_qcount=%d 1u<<%d=%u\n", __FILE__, __LINE__, __FUNCTION__, s, s_get_qcount(s), RWS_QCOUNT_LEN, 1u<<RWS_QCOUNT_LEN);
    //assert(s_get_qcount(s)+1 < (1u<<RWS_QCOUNT_LEN));
    return s+RWS_QCOUNT_INCR;
}
static inline uint64_t s_decr_qcount (uint64_t s) {
    //assert(s_get_qcount(s) > 0);
    return s-RWS_QCOUNT_INCR;
}
static inline uint64_t s_incr_rcount (uint64_t s) {
    //assert(s_get_rcount(s)+1 < (1u<<RWS_RCOUNT_LEN));
    return s+RWS_RCOUNT_INCR;
}
static inline uint64_t s_decr_rcount (uint64_t s) {
    //assert(s_get_rcount(s) > 0);
    return s-RWS_RCOUNT_INCR;
}

int toku_fair_rwlock_init (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_destroy (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_rdlock_slow (toku_fair_rwlock_t *rwlock);  // this is the slow internal version that grabs the mutex.
// Inline the fast path to avoid function call overhead.
static inline int toku_fair_rwlock_rdlock (toku_fair_rwlock_t *rwlock) {
    uint64_t s = rwlock->state;
 START:
    s = rwlock->state;
    if (0==(s&(RWS_QCOUNT_MASK | RWS_WLOCK_MASK))) goto C1;
    //if (s_get_qcount(s)==0 && !s_get_wlock(s)) goto C1;
    else goto ML;
 C1:
    if (__sync_bool_compare_and_swap(&rwlock->state, s, s_incr_rcount(s))) goto DONE;
    else goto START;
 DONE:
    return 0;
 ML:
    return toku_fair_rwlock_rdlock_slow(rwlock);
}
int toku_fair_rwlock_wrlock_slow (toku_fair_rwlock_t *rwlock);
// Inline the fast path to avoid function call overhead.
static inline int toku_fair_rwlock_wrlock (toku_fair_rwlock_t *rwlock) {
    uint64_t s;
 START:
    s = rwlock->state;
    if (s_get_qcount(s)==0 && !s_get_wlock(s) && s_get_rcount(s)==0) goto C1;
    else goto ML;
 C1:
    if (__sync_bool_compare_and_swap(&rwlock->state, s, s_set_wlock(s))) goto DONE;
    else goto START;
 DONE:
    return 0;
 ML:
    return toku_fair_rwlock_wrlock_slow(rwlock);
}	
int toku_fair_rwlock_unlock_r_slow (toku_fair_rwlock_t *rwlock);
int toku_fair_rwlock_unlock_w_slow (toku_fair_rwlock_t *rwlock);
static inline int toku_fair_rwlock_unlock (toku_fair_rwlock_t *rwlock) {
    uint64_t s;
    s = rwlock->state;
    if (s_get_wlock(s)) {
	goto wSTART0; // we already have s.
    wSTART:
	s = rwlock->state;
	goto wSTART0;
    wSTART0:
	if (s_get_qcount(s)==0) goto wC1;
	else goto wML;
    wC1:
	if (__sync_bool_compare_and_swap(&rwlock->state, s, s_clear_wlock(s))) goto wDONE;
	else goto wSTART;
    wDONE:
	return 0;
    wML:
	return toku_fair_rwlock_unlock_w_slow (rwlock);
    } else {
	goto rSTART0; // we already have s.
    rSTART:
	s = rwlock->state;
	goto rSTART0;
    rSTART0:
	if (s_get_rcount(s)>1 || s_get_qcount(s)==0) goto rC1;
	else goto rML;
    rC1:
	if (__sync_bool_compare_and_swap(&rwlock->state, s, s_decr_rcount(s))) goto rDONE;
	else goto rSTART;
    rDONE:
	return 0;
    rML:
	return toku_fair_rwlock_unlock_r_slow (rwlock);
    }
}	
int fcall_nop(int);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
