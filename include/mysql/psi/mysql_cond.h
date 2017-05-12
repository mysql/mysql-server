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

#ifndef MYSQL_COND_H
#define MYSQL_COND_H

/**
  @file include/mysql/psi/mysql_cond.h
  Instrumentation helpers for conditions.
*/

#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_cond.h"
#include "thr_cond.h"
#ifdef MYSQL_SERVER
#ifndef MYSQL_DYNAMIC_PLUGIN
#include "pfs_cond_provider.h"
#endif
#endif

#ifndef PSI_COND_CALL
#define PSI_COND_CALL(M) psi_cond_service->M
#endif

/**
  @defgroup psi_api_cond Cond Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  An instrumented cond structure.
  @sa mysql_cond_t
*/
struct st_mysql_cond
{
  /** The real condition */
  native_cond_t m_cond;
  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c mysql_cond_t interface.
  */
  struct PSI_cond *m_psi;
};

/**
  Type of an instrumented condition.
  @c mysql_cond_t is a drop-in replacement for @c native_cond_t.
  @sa mysql_cond_init
  @sa mysql_cond_wait
  @sa mysql_cond_timedwait
  @sa mysql_cond_signal
  @sa mysql_cond_broadcast
  @sa mysql_cond_destroy
*/
typedef struct st_mysql_cond mysql_cond_t;

#ifndef DISABLE_MYSQL_THREAD_H

/**
  @def mysql_cond_register(P1, P2, P3)
  Cond registration.
*/
#define mysql_cond_register(P1, P2, P3) inline_mysql_cond_register(P1, P2, P3)

/**
  @def mysql_cond_init(K, C)
  Instrumented cond_init.
  @c mysql_cond_init is a replacement for @c pthread_cond_init.
  Note that pthread_condattr_t is not supported in MySQL.
  @param C The cond to initialize
  @param K The PSI_cond_key for this instrumented cond

*/
#ifdef HAVE_PSI_COND_INTERFACE
#define mysql_cond_init(K, C) inline_mysql_cond_init(K, C)
#else
#define mysql_cond_init(K, C) inline_mysql_cond_init(C)
#endif

/**
  @def mysql_cond_destroy(C)
  Instrumented cond_destroy.
  @c mysql_cond_destroy is a drop-in replacement for @c pthread_cond_destroy.
*/
#define mysql_cond_destroy(C) inline_mysql_cond_destroy(C)

/**
  @def mysql_cond_wait(C)
  Instrumented cond_wait.
  @c mysql_cond_wait is a drop-in replacement for @c native_cond_wait.
*/
#if defined(SAFE_MUTEX) || defined(HAVE_PSI_COND_INTERFACE)
#define mysql_cond_wait(C, M) inline_mysql_cond_wait(C, M, __FILE__, __LINE__)
#else
#define mysql_cond_wait(C, M) inline_mysql_cond_wait(C, M)
#endif

/**
  @def mysql_cond_timedwait(C, M, W)
  Instrumented cond_timedwait.
  @c mysql_cond_timedwait is a drop-in replacement
  for @c native_cond_timedwait.
*/
#if defined(SAFE_MUTEX) || defined(HAVE_PSI_COND_INTERFACE)
#define mysql_cond_timedwait(C, M, W) \
  inline_mysql_cond_timedwait(C, M, W, __FILE__, __LINE__)
#else
#define mysql_cond_timedwait(C, M, W) inline_mysql_cond_timedwait(C, M, W)
#endif

/**
  @def mysql_cond_signal(C)
  Instrumented cond_signal.
  @c mysql_cond_signal is a drop-in replacement for @c pthread_cond_signal.
*/
#define mysql_cond_signal(C) inline_mysql_cond_signal(C)

/**
  @def mysql_cond_broadcast(C)
  Instrumented cond_broadcast.
  @c mysql_cond_broadcast is a drop-in replacement
  for @c pthread_cond_broadcast.
*/
#define mysql_cond_broadcast(C) inline_mysql_cond_broadcast(C)

static inline void
inline_mysql_cond_register(
#ifdef HAVE_PSI_COND_INTERFACE
  const char *category, PSI_cond_info *info, int count
#else
  const char *category MY_ATTRIBUTE((unused)),
  void *info MY_ATTRIBUTE((unused)),
  int count MY_ATTRIBUTE((unused))
#endif
  )
{
#ifdef HAVE_PSI_COND_INTERFACE
  PSI_COND_CALL(register_cond)(category, info, count);
#endif
}

static inline int
inline_mysql_cond_init(
#ifdef HAVE_PSI_COND_INTERFACE
  PSI_cond_key key,
#endif
  mysql_cond_t *that)
{
#ifdef HAVE_PSI_COND_INTERFACE
  that->m_psi = PSI_COND_CALL(init_cond)(key, &that->m_cond);
#else
  that->m_psi = NULL;
#endif
  return native_cond_init(&that->m_cond);
}

static inline int
inline_mysql_cond_destroy(mysql_cond_t *that)
{
#ifdef HAVE_PSI_COND_INTERFACE
  if (that->m_psi != NULL)
  {
    PSI_COND_CALL(destroy_cond)(that->m_psi);
    that->m_psi = NULL;
  }
#endif
  return native_cond_destroy(&that->m_cond);
}

static inline int
inline_mysql_cond_wait(mysql_cond_t *that,
                       mysql_mutex_t *mutex
#if defined(SAFE_MUTEX) || defined(HAVE_PSI_COND_INTERFACE)
                       ,
                       const char *src_file,
                       uint src_line
#endif
                       )
{
  int result;

#ifdef HAVE_PSI_COND_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_cond_locker *locker;
    PSI_cond_locker_state state;
    locker = PSI_COND_CALL(start_cond_wait)(
      &state, that->m_psi, mutex->m_psi, PSI_COND_WAIT, src_file, src_line);

    /* Instrumented code */
    result = my_cond_wait(&that->m_cond,
                          &mutex->m_mutex
#ifdef SAFE_MUTEX
                          ,
                          src_file,
                          src_line
#endif
                          );

    /* Instrumentation end */
    if (locker != NULL)
    {
      PSI_COND_CALL(end_cond_wait)(locker, result);
    }

    return result;
  }
#endif

  /* Non instrumented code */
  result = my_cond_wait(&that->m_cond,
                        &mutex->m_mutex
#ifdef SAFE_MUTEX
                        ,
                        src_file,
                        src_line
#endif
                        );

  return result;
}

static inline int
inline_mysql_cond_timedwait(mysql_cond_t *that,
                            mysql_mutex_t *mutex,
                            const struct timespec *abstime
#if defined(SAFE_MUTEX) || defined(HAVE_PSI_COND_INTERFACE)
                            ,
                            const char *src_file,
                            uint src_line
#endif
                            )
{
  int result;

#ifdef HAVE_PSI_COND_INTERFACE
  if (that->m_psi != NULL)
  {
    /* Instrumentation start */
    PSI_cond_locker *locker;
    PSI_cond_locker_state state;
    locker = PSI_COND_CALL(start_cond_wait)(&state,
                                            that->m_psi,
                                            mutex->m_psi,
                                            PSI_COND_TIMEDWAIT,
                                            src_file,
                                            src_line);

    /* Instrumented code */
    result = my_cond_timedwait(&that->m_cond,
                               &mutex->m_mutex,
                               abstime
#ifdef SAFE_MUTEX
                               ,
                               src_file,
                               src_line
#endif
                               );

    /* Instrumentation end */
    if (locker != NULL)
    {
      PSI_COND_CALL(end_cond_wait)(locker, result);
    }

    return result;
  }
#endif

  /* Non instrumented code */
  result = my_cond_timedwait(&that->m_cond,
                             &mutex->m_mutex,
                             abstime
#ifdef SAFE_MUTEX
                             ,
                             src_file,
                             src_line
#endif
                             );

  return result;
}

static inline int
inline_mysql_cond_signal(mysql_cond_t *that)
{
  int result;
#ifdef HAVE_PSI_COND_INTERFACE
  if (that->m_psi != NULL)
  {
    PSI_COND_CALL(signal_cond)(that->m_psi);
  }
#endif
  result = native_cond_signal(&that->m_cond);
  return result;
}

static inline int
inline_mysql_cond_broadcast(mysql_cond_t *that)
{
  int result;
#ifdef HAVE_PSI_COND_INTERFACE
  if (that->m_psi != NULL)
  {
    PSI_COND_CALL(broadcast_cond)(that->m_psi);
  }
#endif
  result = native_cond_broadcast(&that->m_cond);
  return result;
}

#endif /* DISABLE_MYSQL_THREAD_H */

/** @} (end of group psi_api_cond) */

#endif
