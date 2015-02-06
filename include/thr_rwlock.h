#ifndef THR_RWLOCK_INCLUDED
#define THR_RWLOCK_INCLUDED

/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  MySQL rwlock implementation.

  There are two "layers":
  1) native_rw_*()
       Functions that map directly down to OS primitives.
       Windows    - SRWLock
       Other OSes - pthread
  2) mysql_rw*()
       Functions that include Performance Schema instrumentation.
       See include/mysql/psi/mysql_thread.h

  This file also includes rw_pr_*(), which implements a special
  version of rwlocks that prefer readers. The P_S version of these
  are mysql_prlock_*() - see include/mysql/psi/mysql_thread.h
*/

#include "my_global.h"
#include "my_thread.h"
#include "thr_cond.h"

C_MODE_START

#ifdef _WIN32
typedef struct st_my_rw_lock_t
{
  SRWLOCK srwlock;             /* native reader writer lock */
  BOOL have_exclusive_srwlock; /* used for unlock */
} native_rw_lock_t;
#else
typedef pthread_rwlock_t native_rw_lock_t;
#endif

static inline int native_rw_init(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  InitializeSRWLock(&rwp->srwlock);
  rwp->have_exclusive_srwlock = FALSE;
  return 0;
#else
  /* pthread_rwlockattr_t is not used in MySQL */
  return pthread_rwlock_init(rwp, NULL);
#endif
}

static inline int native_rw_destroy(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  return 0; /* no destroy function */
#else
  return pthread_rwlock_destroy(rwp);
#endif
}

static inline int native_rw_rdlock(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  AcquireSRWLockShared(&rwp->srwlock);
  return 0;
#else
  return pthread_rwlock_rdlock(rwp);
#endif
}

static inline int native_rw_tryrdlock(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  if (!TryAcquireSRWLockShared(&rwp->srwlock))
    return EBUSY;
  return 0;
#else
  return pthread_rwlock_tryrdlock(rwp);
#endif
}

static inline int native_rw_wrlock(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  AcquireSRWLockExclusive(&rwp->srwlock);
  rwp->have_exclusive_srwlock= TRUE;
  return 0;
#else
  return pthread_rwlock_wrlock(rwp);
#endif
}

static inline int native_rw_trywrlock(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  if (!TryAcquireSRWLockExclusive(&rwp->srwlock))
    return EBUSY;
  rwp->have_exclusive_srwlock= TRUE;
  return 0;
#else
  return pthread_rwlock_trywrlock(rwp);
#endif
}

static inline int native_rw_unlock(native_rw_lock_t *rwp)
{
#ifdef _WIN32
  if (rwp->have_exclusive_srwlock)
  {
    rwp->have_exclusive_srwlock= FALSE;
    ReleaseSRWLockExclusive(&rwp->srwlock);
  }
  else
    ReleaseSRWLockShared(&rwp->srwlock);
  return 0;
#else
  return pthread_rwlock_unlock(rwp);
#endif
}


/**
  Portable implementation of special type of read-write locks.

  These locks have two properties which are unusual for rwlocks:
  1) They "prefer readers" in the sense that they do not allow
     situations in which rwlock is rd-locked and there is a
     pending rd-lock which is blocked (e.g. due to pending
     request for wr-lock).
     This is a stronger guarantee than one which is provided for
     PTHREAD_RWLOCK_PREFER_READER_NP rwlocks in Linux.
     MDL subsystem deadlock detector relies on this property for
     its correctness.
  2) They are optimized for uncontended wr-lock/unlock case.
     This is scenario in which they are most oftenly used
     within MDL subsystem. Optimizing for it gives significant
     performance improvements in some of tests involving many
     connections.

  Another important requirement imposed on this type of rwlock
  by the MDL subsystem is that it should be OK to destroy rwlock
  object which is in unlocked state even though some threads might
  have not yet fully left unlock operation for it (of course there
  is an external guarantee that no thread will try to lock rwlock
  which is destroyed).
  Putting it another way the unlock operation should not access
  rwlock data after changing its state to unlocked.

  TODO/FIXME: We should consider alleviating this requirement as
  it blocks us from doing certain performance optimizations.
*/

typedef struct st_rw_pr_lock_t {
  /**
    Lock which protects the structure.
    Also held for the duration of wr-lock.
  */
  native_mutex_t lock;
  /**
    Condition variable which is used to wake-up
    writers waiting for readers to go away.
  */
  native_cond_t no_active_readers;
  /** Number of active readers. */
  uint active_readers;
  /** Number of writers waiting for readers to go away. */
  uint writers_waiting_readers;
  /** Indicates whether there is an active writer. */
  my_bool active_writer;
#ifdef SAFE_MUTEX
  /** Thread holding wr-lock (for debug purposes only). */
  my_thread_t writer_thread;
#endif
} rw_pr_lock_t;

extern int rw_pr_init(rw_pr_lock_t *);
extern int rw_pr_rdlock(rw_pr_lock_t *);
extern int rw_pr_wrlock(rw_pr_lock_t *);
extern int rw_pr_unlock(rw_pr_lock_t *);
extern int rw_pr_destroy(rw_pr_lock_t *);

static inline void
rw_pr_lock_assert_write_owner(const rw_pr_lock_t *rwlock __attribute__((unused)))
{
#ifdef SAFE_MUTEX
  DBUG_ASSERT(rwlock->active_writer &&
              my_thread_equal(my_thread_self(), rwlock->writer_thread));
#endif
}

static inline void
rw_pr_lock_assert_not_write_owner(const rw_pr_lock_t *rwlock __attribute__((unused)))
{
#ifdef SAFE_MUTEX
  DBUG_ASSERT(!rwlock->active_writer ||
              !my_thread_equal(my_thread_self(), rwlock->writer_thread));
#endif
}

C_MODE_END

#endif /* THR_RWLOCK_INCLUDED */
