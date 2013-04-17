#ifndef _TOKU_PTHREAD_H
#define _TOKU_PTHREAD_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "pthread.h"
#include <toku_time.h>

#define USE_PTHREADS_WIN32_RWLOCKS 0
int toku_pthread_win32_init(void);
int toku_pthread_win32_destroy(void);

typedef pthread_attr_t          toku_pthread_attr_t;
typedef pthread_t               toku_pthread_t;
typedef pthread_mutexattr_t     toku_pthread_mutexattr_t;
typedef pthread_mutex_t         toku_pthread_mutex_t;
typedef pthread_condattr_t      toku_pthread_condattr_t;
typedef pthread_cond_t          toku_pthread_cond_t;
#if USE_PTHREADS_WIN32_RWLOCKS
typedef pthread_rwlock_t        toku_pthread_rwlock_t;
typedef pthread_rwlockattr_t    toku_pthread_rwlockattr_t;
#else
#include "../newbrt/rwlock.h"
typedef struct toku_pthread_rwlock_struct {
    struct rwlock rwlock;
    toku_pthread_mutex_t mutex;
} toku_pthread_rwlock_t;
typedef struct toku_pthread_rwlockattr_struct {
} toku_pthread_rwlockattr_t;
#endif
//typedef struct timespec         toku_timespec_t; //Already defined in toku_time.h


typedef int (__cdecl *toku_pthread_win32_rwlock_init_func) (pthread_rwlock_t *lock, const pthread_rwlockattr_t *attr);
typedef int (__cdecl *toku_pthread_win32_rwlock_destroy_func) (pthread_rwlock_t *lock);
typedef int (__cdecl *toku_pthread_win32_rwlock_rdlock_func) (pthread_rwlock_t *rwlock);
typedef int (__cdecl *toku_pthread_win32_rwlock_wrlock_func) (pthread_rwlock_t *rwlock);
typedef int (__cdecl *toku_pthread_win32_rwlock_unlock_func) (pthread_rwlock_t *rwlock);
typedef int (__cdecl *toku_pthread_win32_attr_init_func) (pthread_attr_t *attr);
typedef int (__cdecl *toku_pthread_win32_attr_destroy_func) (pthread_attr_t *attr);
typedef int (__cdecl *toku_pthread_win32_attr_getstacksize_func)(pthread_attr_t *attr, size_t *stacksize);
typedef int (__cdecl *toku_pthread_win32_attr_setstacksize_func)(pthread_attr_t *attr, size_t *stacksize);
typedef int (__cdecl *toku_pthread_win32_create_func) (pthread_t *thread, const pthread_attr_t *attr, void *(*start_function)(void *), void *arg);
typedef int (__cdecl *toku_pthread_win32_join_func) (pthread_t thread, void **value_ptr);
typedef pthread_t (__cdecl *toku_pthread_win32_self_func)(void);
typedef int (__cdecl *toku_pthread_win32_mutex_init_func) (pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
typedef int (__cdecl *toku_pthread_win32_mutex_destroy_func) (pthread_mutex_t *mutex);
typedef int (__cdecl *toku_pthread_win32_mutex_lock_func) (pthread_mutex_t *mutex);
typedef int (__cdecl *toku_pthread_win32_mutex_trylock_func) (pthread_mutex_t *mutex);
typedef int (__cdecl *toku_pthread_win32_mutex_unlock_func) (pthread_mutex_t *mutex);
typedef int (__cdecl *toku_pthread_win32_cond_init_func) (pthread_cond_t *cond, const pthread_condattr_t *attr);
typedef int (__cdecl *toku_pthread_win32_cond_destroy_func) (pthread_cond_t *cond);
typedef int (__cdecl *toku_pthread_win32_cond_wait_func) (pthread_cond_t *cond, pthread_mutex_t *mutex);
typedef int (__cdecl *toku_pthread_win32_cond_timedwait_func) (pthread_cond_t *cond, pthread_mutex_t *mutex, toku_timespec_t *wakeup_at);
typedef int (__cdecl *toku_pthread_win32_cond_signal_func) (pthread_cond_t *cond);
typedef int (__cdecl *toku_pthread_win32_cond_broadcast_func) (pthread_cond_t *cond);

typedef struct toku_pthread_win32_funcs_struct {
    toku_pthread_win32_attr_init_func         pthread_attr_init;
    toku_pthread_win32_attr_destroy_func      pthread_attr_destroy;
    toku_pthread_win32_attr_getstacksize_func pthread_attr_getstacksize;
    toku_pthread_win32_attr_setstacksize_func pthread_attr_setstacksize;

    toku_pthread_win32_mutex_init_func        pthread_mutex_init;
    toku_pthread_win32_mutex_destroy_func     pthread_mutex_destroy;
    toku_pthread_win32_mutex_lock_func        pthread_mutex_lock;
    toku_pthread_win32_mutex_trylock_func     pthread_mutex_trylock;
    toku_pthread_win32_mutex_unlock_func      pthread_mutex_unlock;

    toku_pthread_win32_cond_init_func         pthread_cond_init;
    toku_pthread_win32_cond_destroy_func      pthread_cond_destroy;
    toku_pthread_win32_cond_wait_func         pthread_cond_wait;
    toku_pthread_win32_cond_timedwait_func    pthread_cond_timedwait;
    toku_pthread_win32_cond_signal_func       pthread_cond_signal;
    toku_pthread_win32_cond_broadcast_func    pthread_cond_broadcast;

    toku_pthread_win32_rwlock_init_func       pthread_rwlock_init;
    toku_pthread_win32_rwlock_destroy_func    pthread_rwlock_destroy;
    toku_pthread_win32_rwlock_rdlock_func     pthread_rwlock_rdlock;
    toku_pthread_win32_rwlock_wrlock_func     pthread_rwlock_wrlock;
    toku_pthread_win32_rwlock_unlock_func     pthread_rwlock_unlock;

    toku_pthread_win32_create_func            pthread_create;
    toku_pthread_win32_join_func              pthread_join;
    toku_pthread_win32_self_func              pthread_self;
} toku_pthread_win32_funcs;

extern toku_pthread_win32_funcs pthread_win32;


int toku_pthread_yield(void);


static inline
int toku_pthread_attr_init(toku_pthread_attr_t *attr) {
    return pthread_win32.pthread_attr_init(attr);
}

static inline
int toku_pthread_attr_destroy(toku_pthread_attr_t *attr) {
    return pthread_win32.pthread_attr_destroy(attr);
}

static inline
int toku_pthread_attr_getstacksize(toku_pthread_attr_t *attr, size_t *stacksize) {
    return pthread_win32.pthread_attr_getstacksize(attr, stacksize);
}

static inline
int toku_pthread_attr_setstacksize(toku_pthread_attr_t *attr, size_t stacksize) {
    return pthread_win32.pthread_attr_setstacksize(attr, stacksize);
}

static inline
int toku_pthread_create(toku_pthread_t *thread, const toku_pthread_attr_t *attr, void *(*start_function)(void *), void *arg) {
    return pthread_win32.pthread_create(thread, attr, start_function, arg);
}

static inline
int toku_pthread_join(toku_pthread_t thread, void **value_ptr) {
    return pthread_win32.pthread_join(thread, value_ptr);
}

static inline
toku_pthread_t toku_pthread_self(void) {
    return pthread_win32.pthread_self();
}

#define TOKU_PTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline
int toku_pthread_mutex_init(toku_pthread_mutex_t *mutex, const toku_pthread_mutexattr_t *attr) {
    return pthread_win32.pthread_mutex_init(mutex, attr);
}

static inline
int toku_pthread_mutex_destroy(toku_pthread_mutex_t *mutex) {
    return pthread_win32.pthread_mutex_destroy(mutex);
}

static inline
int toku_pthread_mutex_lock(toku_pthread_mutex_t *mutex) {
    return pthread_win32.pthread_mutex_lock(mutex);
}

int toku_pthread_mutex_trylock(toku_pthread_mutex_t *mutex);

static inline
int toku_pthread_mutex_unlock(toku_pthread_mutex_t *mutex) {
    return pthread_win32.pthread_mutex_unlock(mutex);
}

static inline
int toku_pthread_cond_init(toku_pthread_cond_t *cond, const toku_pthread_condattr_t *attr) {
    return pthread_win32.pthread_cond_init(cond, attr);
}

static inline 
int toku_pthread_cond_destroy(toku_pthread_cond_t *cond) {
    return pthread_win32.pthread_cond_destroy(cond);
}

static inline
int toku_pthread_cond_wait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex) {
    return pthread_win32.pthread_cond_wait(cond, mutex);
}

static inline
int toku_pthread_cond_timedwait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex, toku_timespec_t *wakeup_at) {
    return pthread_win32.pthread_cond_timedwait(cond, mutex, wakeup_at);
}

static inline
int toku_pthread_cond_signal(toku_pthread_cond_t *cond) {
    return pthread_win32.pthread_cond_signal(cond);
}

static inline
int toku_pthread_cond_broadcast(toku_pthread_cond_t *cond) {
    return pthread_win32.pthread_cond_broadcast(cond);
}

#if USE_PTHREADS_WIN32_RWLOCKS
static inline int
toku_pthread_rwlock_init(toku_pthread_rwlock_t *__restrict rwlock, const toku_pthread_rwlockattr_t *__restrict attr) {
    return pthread_win32.pthread_rwlock_init(rwlock, attr);
}

static inline int
toku_pthread_rwlock_destroy(toku_pthread_rwlock_t *rwlock) {
    return pthread_win32.pthread_rwlock_destroy(rwlock);
}

static inline int
toku_pthread_rwlock_rdlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_win32.pthread_rwlock_rdlock(rwlock);
}

static inline int
toku_pthread_rwlock_rdunlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_win32.pthread_rwlock_unlock(rwlock);
}

static inline int
toku_pthread_rwlock_wrlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_win32.pthread_rwlock_wrlock(rwlock);
}

static inline int
toku_pthread_rwlock_wrunlock(toku_pthread_rwlock_t *rwlock) {
    return pthread_win32.pthread_rwlock_unlock(rwlock);
}
#else
static inline int
toku_pthread_rwlock_init(toku_pthread_rwlock_t *__restrict rwlock, const toku_pthread_rwlockattr_t *__restrict attr) {
    int r = 0;
    if (attr!=NULL) r = EINVAL;
    if (r==0)
        r = toku_pthread_mutex_init(&rwlock->mutex, NULL);
    if (r==0)
        rwlock_init(&rwlock->rwlock);
    return r;
}

static inline int
toku_pthread_rwlock_destroy(toku_pthread_rwlock_t *rwlock) {
    int r = 0;
    rwlock_destroy(&rwlock->rwlock);
    r = toku_pthread_mutex_destroy(&rwlock->mutex);
    return r;
}

static inline int
toku_pthread_rwlock_rdlock(toku_pthread_rwlock_t *rwlock) {
    int r = 0;
    r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    //We depend on recursive read locks.
    rwlock_prefer_read_lock(&rwlock->rwlock, &rwlock->mutex);
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return r;
}

static inline int
toku_pthread_rwlock_rdunlock(toku_pthread_rwlock_t *rwlock) {
    int r = 0;
    r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    rwlock_read_unlock(&rwlock->rwlock);
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return r;
}

static inline int
toku_pthread_rwlock_wrlock(toku_pthread_rwlock_t *rwlock) {
    int r = 0;
    r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    rwlock_write_lock(&rwlock->rwlock, &rwlock->mutex);
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return r;
}

static inline int
toku_pthread_rwlock_wrunlock(toku_pthread_rwlock_t *rwlock) {
    int r = 0;
    r = toku_pthread_mutex_lock(&rwlock->mutex);
    assert(r==0);
    rwlock_write_unlock(&rwlock->rwlock);
    r = toku_pthread_mutex_unlock(&rwlock->mutex);
    assert(r==0);
    return r;
}
#endif

int
initialize_pthread_functions();

int
pthread_functions_free();


#if defined(__cplusplus)
};
#endif

#endif
