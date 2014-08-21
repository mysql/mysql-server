/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include <pthread.h>
#include <time.h>
#include <stdint.h>

#include "toku_assert.h"

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

#ifndef TOKU_PTHREAD_DEBUG
# define TOKU_PTHREAD_DEBUG 0
#endif

typedef struct toku_mutex {
    pthread_mutex_t pmutex;
#if TOKU_PTHREAD_DEBUG
    pthread_t owner; // = pthread_self(); // for debugging
    bool locked;
    bool valid;
#endif
} toku_mutex_t;

typedef struct toku_mutex_aligned {
    toku_mutex_t aligned_mutex __attribute__((__aligned__(64)));
} toku_mutex_aligned_t;

// Different OSes implement mutexes as different amounts of nested structs.
// C++ will fill out all missing values with zeroes if you provide at least one zero, but it needs the right amount of nesting.
#if defined(__FreeBSD__)
# define ZERO_MUTEX_INITIALIZER {0}
#elif defined(__APPLE__)
# define ZERO_MUTEX_INITIALIZER {{0}}
#else // __linux__, at least
# define ZERO_MUTEX_INITIALIZER {{{0}}}
#endif

#if TOKU_PTHREAD_DEBUG
# define TOKU_MUTEX_INITIALIZER { .pmutex = PTHREAD_MUTEX_INITIALIZER, .owner = 0, .locked = false, .valid = true }
#else
# define TOKU_MUTEX_INITIALIZER { .pmutex = PTHREAD_MUTEX_INITIALIZER }
#endif

// Darwin doesn't provide adaptive mutexes
#if defined(__APPLE__)
# define TOKU_MUTEX_ADAPTIVE PTHREAD_MUTEX_DEFAULT
# if TOKU_PTHREAD_DEBUG
#  define TOKU_ADAPTIVE_MUTEX_INITIALIZER { .pmutex = PTHREAD_MUTEX_INITIALIZER, .owner = 0, .locked = false, .valid = true }
# else
#  define TOKU_ADAPTIVE_MUTEX_INITIALIZER { .pmutex = PTHREAD_MUTEX_INITIALIZER }
# endif
#else // __FreeBSD__, __linux__, at least
# define TOKU_MUTEX_ADAPTIVE PTHREAD_MUTEX_ADAPTIVE_NP
# if TOKU_PTHREAD_DEBUG
#  define TOKU_ADAPTIVE_MUTEX_INITIALIZER { .pmutex = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP, .owner = 0, .locked = false, .valid = true }
# else
#  define TOKU_ADAPTIVE_MUTEX_INITIALIZER { .pmutex = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP }
# endif
#endif

static inline void
toku_mutex_init(toku_mutex_t *mutex, const toku_pthread_mutexattr_t *attr) {
    int r = pthread_mutex_init(&mutex->pmutex, attr);
    assert_zero(r);
#if TOKU_PTHREAD_DEBUG
    mutex->locked = false;
    invariant(!mutex->valid);
    mutex->valid = true;
    mutex->owner = 0;
#endif
}

static inline void
toku_mutexattr_init(toku_pthread_mutexattr_t *attr) {
    int r = pthread_mutexattr_init(attr);
    assert_zero(r);
}

static inline void
toku_mutexattr_settype(toku_pthread_mutexattr_t *attr, int type) {
    int r = pthread_mutexattr_settype(attr, type);
    assert_zero(r);
}

static inline void
toku_mutexattr_destroy(toku_pthread_mutexattr_t *attr) {
    int r = pthread_mutexattr_destroy(attr);
    assert_zero(r);
}

static inline void
toku_mutex_destroy(toku_mutex_t *mutex) {
#if TOKU_PTHREAD_DEBUG
    invariant(mutex->valid);
    mutex->valid = false;
    invariant(!mutex->locked);
#endif
    int r = pthread_mutex_destroy(&mutex->pmutex);
    assert_zero(r);
}

static inline void
toku_mutex_lock(toku_mutex_t *mutex) {
    int r = pthread_mutex_lock(&mutex->pmutex);
    assert_zero(r);
#if TOKU_PTHREAD_DEBUG
    invariant(mutex->valid);
    invariant(!mutex->locked);
    invariant(mutex->owner == 0);
    mutex->locked = true;
    mutex->owner = pthread_self();
#endif
}

static inline int
toku_mutex_trylock(toku_mutex_t *mutex) {
    int r = pthread_mutex_trylock(&mutex->pmutex);
#if TOKU_PTHREAD_DEBUG
    if (r == 0) {
        invariant(mutex->valid);
        invariant(!mutex->locked);
        invariant(mutex->owner == 0);
        mutex->locked = true;
        mutex->owner = pthread_self();
    }
#endif
    return r;
}

static inline void
toku_mutex_unlock(toku_mutex_t *mutex) {
#if TOKU_PTHREAD_DEBUG
    invariant(mutex->owner == pthread_self());
    invariant(mutex->valid);
    invariant(mutex->locked);
    mutex->locked = false;
    mutex->owner = 0;
#endif
    int r = pthread_mutex_unlock(&mutex->pmutex);
    assert_zero(r);
}

#if TOKU_PTHREAD_DEBUG
static inline void
toku_mutex_assert_locked(const toku_mutex_t *mutex) {
    invariant(mutex->locked);
    invariant(mutex->owner == pthread_self());
}
#else
static inline void
toku_mutex_assert_locked(const toku_mutex_t *mutex __attribute__((unused))) {
}
#endif

// asserting that a mutex is unlocked only makes sense
// if the calling thread can guaruntee that no other threads
// are trying to lock this mutex at the time of the assertion
// 
// a good example of this is a tree with mutexes on each node.
// when a node is locked the caller knows that no other threads
// can be trying to lock its childrens' mutexes. the children
// are in one of two fixed states: locked or unlocked.
#if TOKU_PTHREAD_DEBUG
static inline void
toku_mutex_assert_unlocked(toku_mutex_t *mutex) {
    invariant(mutex->owner == 0);
    invariant(!mutex->locked);
}
#else
static inline void
toku_mutex_assert_unlocked(toku_mutex_t *mutex __attribute__((unused))) {
}
#endif

typedef struct toku_cond {
    pthread_cond_t pcond;
} toku_cond_t;

// Different OSes implement mutexes as different amounts of nested structs.
// C++ will fill out all missing values with zeroes if you provide at least one zero, but it needs the right amount of nesting.
#if defined(__FreeBSD__)
# define ZERO_COND_INITIALIZER {0}
#elif defined(__APPLE__)
# define ZERO_COND_INITIALIZER {{0}}
#else // __linux__, at least
# define ZERO_COND_INITIALIZER {{{0}}}
#endif
#define TOKU_COND_INITIALIZER {PTHREAD_COND_INITIALIZER}

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
#if TOKU_PTHREAD_DEBUG
    invariant(mutex->locked);
    mutex->locked = false;
    mutex->owner = 0;
#endif
    int r = pthread_cond_wait(&cond->pcond, &mutex->pmutex);
    assert_zero(r);
#if TOKU_PTHREAD_DEBUG
    invariant(!mutex->locked);
    mutex->locked = true;
    mutex->owner = pthread_self();
#endif
}

static inline int 
toku_cond_timedwait(toku_cond_t *cond, toku_mutex_t *mutex, toku_timespec_t *wakeup_at) {
#if TOKU_PTHREAD_DEBUG
    invariant(mutex->locked);
    mutex->locked = false;
    mutex->owner = 0;
#endif
    int r = pthread_cond_timedwait(&cond->pcond, &mutex->pmutex, wakeup_at);
#if TOKU_PTHREAD_DEBUG
    invariant(!mutex->locked);
    mutex->locked = true;
    mutex->owner = pthread_self();
#endif
    return r;
}

static inline void 
toku_cond_signal(toku_cond_t *cond) {
    int r = pthread_cond_signal(&cond->pcond);
    assert_zero(r);
}

static inline void
toku_cond_broadcast(toku_cond_t *cond) {
    int r =pthread_cond_broadcast(&cond->pcond);
    assert_zero(r);
}

int 
toku_pthread_yield(void) __attribute__((__visibility__("default")));

static inline toku_pthread_t 
toku_pthread_self(void) {
    return pthread_self();
}

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

static inline int 
toku_pthread_create(toku_pthread_t *thread, const toku_pthread_attr_t *attr, void *(*start_function)(void *), void *arg) {
    return pthread_create(thread, attr, start_function, arg);
}

static inline int 
toku_pthread_join(toku_pthread_t thread, void **value_ptr) {
    return pthread_join(thread, value_ptr);
}

static inline int 
toku_pthread_detach(toku_pthread_t thread) {
    return pthread_detach(thread);
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
