/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MYSQL_RWLOCK_H
#define MYSQL_RWLOCK_H

/**
  @file include/mysql/psi/mysql_rwlock.h
  Instrumentation helpers for rwlock.
*/

#include "thr_rwlock.h"
#include "mysql/psi/psi_rwlock.h"
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN
#include "pfs_rwlock_provider.h"
#endif
#endif

#ifndef PSI_RWLOCK_CALL
#define PSI_RWLOCK_CALL(M) psi_rwlock_service->M
#endif

/**
  @defgroup psi_api_rwlock Rwlock Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def mysql_prlock_assert_write_owner(M)
  Drop-in replacement
  for @c rw_pr_lock_assert_write_owner.
*/
#ifdef SAFE_MUTEX
#define mysql_prlock_assert_write_owner(M) \
  rw_pr_lock_assert_write_owner(&(M)->m_prlock)
#else
#define mysql_prlock_assert_write_owner(M) { }
#endif

/**
  @def mysql_prlock_assert_not_write_owner(M)
  Drop-in replacement
  for @c rw_pr_lock_assert_not_write_owner.
*/
#ifdef SAFE_MUTEX
#define mysql_prlock_assert_not_write_owner(M) \
  rw_pr_lock_assert_not_write_owner(&(M)->m_prlock)
#else
#define mysql_prlock_assert_not_write_owner(M) { }
#endif

/**
  An instrumented rwlock structure.
  @sa mysql_rwlock_t
*/
struct st_mysql_rwlock
{
  /** The real rwlock */
  native_rw_lock_t m_rwlock;
  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c mysql_rwlock_t interface.
  */
  struct PSI_rwlock *m_psi;
};

/**
  An instrumented prlock structure.
  @sa mysql_prlock_t
*/
struct st_mysql_prlock
{
  /** The real prlock */
  rw_pr_lock_t m_prlock;
  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c mysql_rwlock_t interface.
  */
  struct PSI_rwlock *m_psi;
};

/**
  Type of an instrumented rwlock.
  @c mysql_rwlock_t is a drop-in replacement for @c pthread_rwlock_t.
  @sa mysql_rwlock_init
  @sa mysql_rwlock_rdlock
  @sa mysql_rwlock_tryrdlock
  @sa mysql_rwlock_wrlock
  @sa mysql_rwlock_trywrlock
  @sa mysql_rwlock_unlock
  @sa mysql_rwlock_destroy
*/
typedef struct st_mysql_rwlock mysql_rwlock_t;

/**
  Type of an instrumented prlock.
  A prlock is a read write lock that 'prefers readers' (pr).
  @c mysql_prlock_t is a drop-in replacement for @c rw_pr_lock_t.
  @sa mysql_prlock_init
  @sa mysql_prlock_rdlock
  @sa mysql_prlock_wrlock
  @sa mysql_prlock_unlock
  @sa mysql_prlock_destroy
*/
typedef struct st_mysql_prlock mysql_prlock_t;

#ifndef DISABLE_MYSQL_THREAD_H

/**
  @def mysql_rwlock_register(P1, P2, P3)
  Rwlock registration.
*/
#define mysql_rwlock_register(P1, P2, P3) \
  inline_mysql_rwlock_register(P1, P2, P3)

/**
  @def mysql_rwlock_init(K, RW)
  Instrumented rwlock_init.
  @c mysql_rwlock_init is a replacement for @c pthread_rwlock_init.
  Note that pthread_rwlockattr_t is not supported in MySQL.
  @param K The PSI_rwlock_key for this instrumented rwlock
  @param RW The rwlock to initialize
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_rwlock_init(K, RW) inline_mysql_rwlock_init(K, RW)
#else
  #define mysql_rwlock_init(K, RW) inline_mysql_rwlock_init(RW)
#endif

/**
  @def mysql_prlock_init(K, RW)
  Instrumented rw_pr_init.
  @c mysql_prlock_init is a replacement for @c rw_pr_init.
  @param K The PSI_rwlock_key for this instrumented prlock
  @param RW The prlock to initialize
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_prlock_init(K, RW) inline_mysql_prlock_init(K, RW)
#else
  #define mysql_prlock_init(K, RW) inline_mysql_prlock_init(RW)
#endif

/**
  @def mysql_rwlock_destroy(RW)
  Instrumented rwlock_destroy.
  @c mysql_rwlock_destroy is a drop-in replacement
  for @c pthread_rwlock_destroy.
*/
#define mysql_rwlock_destroy(RW) inline_mysql_rwlock_destroy(RW)

/**
  @def mysql_prlock_destroy(RW)
  Instrumented rw_pr_destroy.
  @c mysql_prlock_destroy is a drop-in replacement
  for @c rw_pr_destroy.
*/
#define mysql_prlock_destroy(RW) inline_mysql_prlock_destroy(RW)

/**
  @def mysql_rwlock_rdlock(RW)
  Instrumented rwlock_rdlock.
  @c mysql_rwlock_rdlock is a drop-in replacement
  for @c pthread_rwlock_rdlock.
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_rwlock_rdlock(RW) \
    inline_mysql_rwlock_rdlock(RW, __FILE__, __LINE__)
#else
  #define mysql_rwlock_rdlock(RW) \
    inline_mysql_rwlock_rdlock(RW)
#endif

/**
  @def mysql_prlock_rdlock(RW)
  Instrumented rw_pr_rdlock.
  @c mysql_prlock_rdlock is a drop-in replacement
  for @c rw_pr_rdlock.
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_prlock_rdlock(RW) \
    inline_mysql_prlock_rdlock(RW, __FILE__, __LINE__)
#else
  #define mysql_prlock_rdlock(RW) \
    inline_mysql_prlock_rdlock(RW)
#endif

/**
  @def mysql_rwlock_wrlock(RW)
  Instrumented rwlock_wrlock.
  @c mysql_rwlock_wrlock is a drop-in replacement
  for @c pthread_rwlock_wrlock.
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_rwlock_wrlock(RW) \
    inline_mysql_rwlock_wrlock(RW, __FILE__, __LINE__)
#else
  #define mysql_rwlock_wrlock(RW) \
    inline_mysql_rwlock_wrlock(RW)
#endif

/**
  @def mysql_prlock_wrlock(RW)
  Instrumented rw_pr_wrlock.
  @c mysql_prlock_wrlock is a drop-in replacement
  for @c rw_pr_wrlock.
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_prlock_wrlock(RW) \
    inline_mysql_prlock_wrlock(RW, __FILE__, __LINE__)
#else
  #define mysql_prlock_wrlock(RW) \
    inline_mysql_prlock_wrlock(RW)
#endif

/**
  @def mysql_rwlock_tryrdlock(RW)
  Instrumented rwlock_tryrdlock.
  @c mysql_rwlock_tryrdlock is a drop-in replacement
  for @c pthread_rwlock_tryrdlock.
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_rwlock_tryrdlock(RW) \
    inline_mysql_rwlock_tryrdlock(RW, __FILE__, __LINE__)
#else
  #define mysql_rwlock_tryrdlock(RW) \
    inline_mysql_rwlock_tryrdlock(RW)
#endif

/**
  @def mysql_rwlock_trywrlock(RW)
  Instrumented rwlock_trywrlock.
  @c mysql_rwlock_trywrlock is a drop-in replacement
  for @c pthread_rwlock_trywrlock.
*/
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  #define mysql_rwlock_trywrlock(RW) \
    inline_mysql_rwlock_trywrlock(RW, __FILE__, __LINE__)
#else
  #define mysql_rwlock_trywrlock(RW) \
    inline_mysql_rwlock_trywrlock(RW)
#endif

/**
  @def mysql_rwlock_unlock(RW)
  Instrumented rwlock_unlock.
  @c mysql_rwlock_unlock is a drop-in replacement
  for @c pthread_rwlock_unlock.
*/
#define mysql_rwlock_unlock(RW) inline_mysql_rwlock_unlock(RW)

/**
  @def mysql_prlock_unlock(RW)
  Instrumented rw_pr_unlock.
  @c mysql_prlock_unlock is a drop-in replacement
  for @c rw_pr_unlock.
*/
#define mysql_prlock_unlock(RW) inline_mysql_prlock_unlock(RW)

static inline void inline_mysql_rwlock_register(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  const char *category,
  PSI_rwlock_info *info,
  int count
#else
  const char *category MY_ATTRIBUTE ((unused)),
  void *info MY_ATTRIBUTE ((unused)),
  int count MY_ATTRIBUTE ((unused))
#endif
)
{
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  PSI_RWLOCK_CALL(register_rwlock)(category, info, count);
#endif
}

static inline int inline_mysql_rwlock_init(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  PSI_rwlock_key key,
#endif
  mysql_rwlock_t *that)
{
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  that->m_psi= PSI_RWLOCK_CALL(init_rwlock)(key, &that->m_rwlock);
#else
  that->m_psi= NULL;
#endif
  return native_rw_init(&that->m_rwlock);
}

#ifndef DISABLE_MYSQL_PRLOCK_H
static inline int inline_mysql_prlock_init(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  PSI_rwlock_key key,
#endif
  mysql_prlock_t *that)
{
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  that->m_psi= PSI_RWLOCK_CALL(init_rwlock)(key, &that->m_prlock);
#else
  that->m_psi= NULL;
#endif
  return rw_pr_init(&that->m_prlock);
}
#endif

static inline int inline_mysql_rwlock_destroy(
  mysql_rwlock_t *that)
{
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    PSI_RWLOCK_CALL(destroy_rwlock)(that->m_psi);
    that->m_psi= NULL;
  }
#endif
  return native_rw_destroy(&that->m_rwlock);
}

#ifndef DISABLE_MYSQL_PRLOCK_H
static inline int inline_mysql_prlock_destroy(
  mysql_prlock_t *that)
{
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    PSI_RWLOCK_CALL(destroy_rwlock)(that->m_psi);
    that->m_psi= NULL;
  }
#endif
  return rw_pr_destroy(&that->m_prlock);
}
#endif

static inline int inline_mysql_rwlock_rdlock(
  mysql_rwlock_t *that
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  , const char *src_file, uint src_line
#endif
  )
{
  int result;

#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_rwlock_locker *locker;
    PSI_rwlock_locker_state state;
    locker= PSI_RWLOCK_CALL(start_rwlock_rdwait)(&state, that->m_psi,
                                          PSI_RWLOCK_READLOCK, src_file, src_line);

    /* Instrumented code */
    result= native_rw_rdlock(&that->m_rwlock);

    /* Instrumentation end */
    if (locker != NULL)
      PSI_RWLOCK_CALL(end_rwlock_rdwait)(locker, result);

    return result;
  }
#endif

  /* Non instrumented code */
  result= native_rw_rdlock(&that->m_rwlock);

  return result;
}

#ifndef DISABLE_MYSQL_PRLOCK_H
static inline int inline_mysql_prlock_rdlock(
  mysql_prlock_t *that
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  , const char *src_file, uint src_line
#endif
  )
{
  int result;

#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_rwlock_locker *locker;
    PSI_rwlock_locker_state state;
    locker= PSI_RWLOCK_CALL(start_rwlock_rdwait)(&state, that->m_psi,
                                          PSI_RWLOCK_READLOCK, src_file, src_line);

    /* Instrumented code */
    result= rw_pr_rdlock(&that->m_prlock);

    /* Instrumentation end */
    if (locker != NULL)
      PSI_RWLOCK_CALL(end_rwlock_rdwait)(locker, result);

    return result;
  }
#endif

  /* Non instrumented code */
  result= rw_pr_rdlock(&that->m_prlock);

  return result;
}
#endif

static inline int inline_mysql_rwlock_wrlock(
  mysql_rwlock_t *that
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  , const char *src_file, uint src_line
#endif
  )
{
  int result;

#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_rwlock_locker *locker;
    PSI_rwlock_locker_state state;
    locker= PSI_RWLOCK_CALL(start_rwlock_wrwait)(&state, that->m_psi,
                                          PSI_RWLOCK_WRITELOCK, src_file, src_line);

    /* Instrumented code */
    result= native_rw_wrlock(&that->m_rwlock);

    /* Instrumentation end */
    if (locker != NULL)
      PSI_RWLOCK_CALL(end_rwlock_wrwait)(locker, result);

    return result;
  }
#endif

  /* Non instrumented code */
  result= native_rw_wrlock(&that->m_rwlock);

  return result;
}

#ifndef DISABLE_MYSQL_PRLOCK_H
static inline int inline_mysql_prlock_wrlock(
  mysql_prlock_t *that
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  , const char *src_file, uint src_line
#endif
  )
{
  int result;

#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_rwlock_locker *locker;
    PSI_rwlock_locker_state state;
    locker= PSI_RWLOCK_CALL(start_rwlock_wrwait)(&state, that->m_psi,
                                          PSI_RWLOCK_WRITELOCK, src_file, src_line);

    /* Instrumented code */
    result= rw_pr_wrlock(&that->m_prlock);

    /* Instrumentation end */
    if (locker != NULL)
      PSI_RWLOCK_CALL(end_rwlock_wrwait)(locker, result);

    return result;
  }
#endif

  /* Non instrumented code */
  result= rw_pr_wrlock(&that->m_prlock);

  return result;
}
#endif

static inline int inline_mysql_rwlock_tryrdlock(
  mysql_rwlock_t *that
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  , const char *src_file, uint src_line
#endif
  )
{
  int result;

#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_rwlock_locker *locker;
    PSI_rwlock_locker_state state;
    locker= PSI_RWLOCK_CALL(start_rwlock_rdwait)(&state, that->m_psi,
                                          PSI_RWLOCK_TRYREADLOCK, src_file, src_line);

    /* Instrumented code */
    result= native_rw_tryrdlock(&that->m_rwlock);

    /* Instrumentation end */
    if (locker != NULL)
      PSI_RWLOCK_CALL(end_rwlock_rdwait)(locker, result);

    return result;
  }
#endif

  /* Non instrumented code */
  result= native_rw_tryrdlock(&that->m_rwlock);

  return result;
}

static inline int inline_mysql_rwlock_trywrlock(
  mysql_rwlock_t *that
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  , const char *src_file, uint src_line
#endif
  )
{
  int result;

#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_rwlock_locker *locker;
    PSI_rwlock_locker_state state;
    locker= PSI_RWLOCK_CALL(start_rwlock_wrwait)(&state, that->m_psi,
                                          PSI_RWLOCK_TRYWRITELOCK, src_file, src_line);

    /* Instrumented code */
    result= native_rw_trywrlock(&that->m_rwlock);

    /* Instrumentation end */
    if (locker != NULL)
      PSI_RWLOCK_CALL(end_rwlock_wrwait)(locker, result);

    return result;
  }
#endif

  /* Non instrumented code */
  result= native_rw_trywrlock(&that->m_rwlock);

  return result;
}

static inline int inline_mysql_rwlock_unlock(
  mysql_rwlock_t *that)
{
  int result;
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
    PSI_RWLOCK_CALL(unlock_rwlock)(that->m_psi);
#endif
  result= native_rw_unlock(&that->m_rwlock);
  return result;
}

#ifndef DISABLE_MYSQL_PRLOCK_H
static inline int inline_mysql_prlock_unlock(
  mysql_prlock_t *that)
{
  int result;
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (that->m_psi != NULL)
    PSI_RWLOCK_CALL(unlock_rwlock)(that->m_psi);
#endif
  result= rw_pr_unlock(&that->m_prlock);
  return result;
}
#endif

#endif /* DISABLE_MYSQL_THREAD_H */

/** @} (end of group psi_api_rwlock) */

#endif

