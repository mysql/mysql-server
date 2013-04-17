#ifndef _TOKU_PTHREAD_H
#define _TOKU_PTHREAD_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <toku_time.h>
#include <windows.h>

#if defined(ETIMEDOUT)
#error
#endif
#define ETIMEDOUT (WAIT_TIMEOUT)

// pthread types

typedef struct toku_pthread_attr {
    SIZE_T stacksize;
} toku_pthread_attr_t;

typedef struct toku_pthread {
    HANDLE handle;
    DWORD id;
    void *(*f)(void *);
    void *arg;
    void *ret;
} *toku_pthread_t;

typedef struct toku_pthread_rwlockattr *toku_pthread_rwlockattr_t;

typedef struct toku_pthread_mutexattr *toku_pthread_mutexattr_t;

typedef struct toku_pthread_mutex {
    CRITICAL_SECTION section;
    BOOL initialized;
} toku_pthread_mutex_t;

typedef struct toku_pthread_rwlock toku_pthread_rwlock_t;

#define TOKU_PTHREAD_MUTEX_INITIALIZER { 0, 0 }

typedef struct toku_pthread_condattr *toku_pthread_condattr_t;

// WINNT >= 6 supports condition variables.  For now, we use a couple of events.
#define TOKU_PTHREAD_COND_NEVENTS 2

typedef struct toku_pthread_cond {
#if 0
    CONDITION_VARIABLE wcv;
#else
    HANDLE events[TOKU_PTHREAD_COND_NEVENTS];
    int waiters;
#endif
} toku_pthread_cond_t;

// pthread interface

int toku_pthread_yield(void);

int toku_pthread_attr_init(toku_pthread_attr_t *);
int toku_pthread_attr_destroy(toku_pthread_attr_t *);
int toku_pthread_attr_getstacksize(toku_pthread_attr_t *, size_t *stacksize);
int toku_pthread_attr_setstacksize(toku_pthread_attr_t *, size_t stacksize);

int toku_pthread_create(toku_pthread_t *thread, const toku_pthread_attr_t *attr, void *(*start_function)(void *), void *arg);

int toku_pthread_join(toku_pthread_t thread, void **value_ptr);

toku_pthread_t toku_pthread_self(void);


int toku_pthread_mutex_init(toku_pthread_mutex_t *mutex, const toku_pthread_mutexattr_t *attr);

int toku_pthread_mutex_destroy(toku_pthread_mutex_t *mutex);

int toku_pthread_mutex_lock(toku_pthread_mutex_t *mutex);

int toku_pthread_mutex_trylock(toku_pthread_mutex_t *mutex);

int toku_pthread_mutex_unlock(toku_pthread_mutex_t *mutex);

int toku_pthread_rwlock_init(toku_pthread_rwlock_t *restrict rwlock, const toku_pthread_rwlockattr_t *restrict attr);
int toku_pthread_rwlock_destroy(toku_pthread_rwlock_t *rwlock);
int toku_pthread_rwlock_rdlock(toku_pthread_rwlock_t *rwlock);
int toku_pthread_rwlock_rdunlock(toku_pthread_rwlock_t *rwlock);
int toku_pthread_rwlock_wrunlock(toku_pthread_rwlock_t *rwlock);
int toku_pthread_rwlock_wrlock(toku_pthread_rwlock_t *rwlock);

int toku_pthread_cond_init(toku_pthread_cond_t *cond, const toku_pthread_condattr_t *attr);

int toku_pthread_cond_destroy(toku_pthread_cond_t *cond);

int toku_pthread_cond_wait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex);

int toku_pthread_cond_timedwait(toku_pthread_cond_t *cond, toku_pthread_mutex_t *mutex, toku_timespec_t *wakeup_at);

int toku_pthread_cond_signal(toku_pthread_cond_t *cond);

int toku_pthread_cond_broadcast(toku_pthread_cond_t *cond);

#include <toku_assert.h>
#include "../newbrt/rwlock.h"
struct toku_pthread_rwlock {
    struct rwlock        rwlock;
    toku_pthread_mutex_t mutex;
    BOOL                 initialized;
};


#if defined(__cplusplus)
};
#endif

#endif
