/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "Copyright (c) 2012 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef __TOKU_SPINLOCK_H__
#define __TOKU_SPINLOCK_H__

#include <config.h>

#include "toku_assert.h"

#ifdef HAVE_LIBKERN_OSATOMIC_H

#include <libkern/OSAtomic.h>
typedef OSSpinLock toku_spinlock_t;
static inline void
toku_spin_init(toku_spinlock_t *lock __attribute__((unused)),
               int pshared __attribute__((unused))) {
}

static inline void
toku_spin_destroy(toku_spinlock_t *lock __attribute__((unused))) {

}

static inline void
toku_spin_lock(toku_spinlock_t *lock) {
    OSSpinLockLock(lock);
}

static inline void
toku_spin_unlock(toku_spinlock_t *lock) {
    OSSpinLockUnlock(lock);
}

#else

#ifdef HAVE_PTHREAD_H

#include <pthread.h>

typedef pthread_spinlock_t toku_spinlock_t;

static inline void
toku_spin_init(toku_spinlock_t *lock, int pshared) {
    int r = pthread_spin_init(lock, pshared);
    assert_zero(r);
}

static inline void
toku_spin_destroy(toku_spinlock_t *lock) {
    int r = pthread_spin_destroy(lock);
    assert_zero(r);
}

static inline void
toku_spin_lock(toku_spinlock_t *lock) {
    int r = pthread_spin_lock(lock);
    assert_zero(r);
}

static inline void
toku_spin_unlock(toku_spinlock_t *lock) {
    int r = pthread_spin_unlock(lock);
    assert_zero(r);
}

#endif

#endif

#endif /* __TOKU_SPINLOCK_H__ */
