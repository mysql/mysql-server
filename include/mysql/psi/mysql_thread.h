/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_THREAD_H
#define MYSQL_THREAD_H

/**
  @file include/mysql/psi/mysql_thread.h
  Instrumentation helpers for mysys threads.
  This header file provides the necessary declarations
  to use the mysys thread API with the performance schema instrumentation.
  In some compilers (SunStudio), 'static inline' functions, when declared
  but not used, are not optimized away (because they are unused) by default,
  so that including a static inline function from a header file does
  create unwanted dependencies, causing unresolved symbols at link time.
  Other compilers, like gcc, optimize these dependencies by default.

  Since the instrumented APIs declared here are wrapper on top
  of my_thread / safemutex / etc APIs,
  including mysql/psi/mysql_thread.h assumes that
  the dependency on my_thread and safemutex already exists.
*/

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "my_thread.h"
#include "my_thread_local.h"
#include "mysql/psi/psi_thread.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_THREAD_CALL() as direct call. */
#include "pfs_thread_provider.h"  // IWYU pragma: keep
#endif

#ifndef PSI_THREAD_CALL
#define PSI_THREAD_CALL(M) psi_thread_service->M
#endif

/**
  @defgroup psi_api_thread Thread Instrumentation (API)
  @ingroup psi_api
  @{
*/

/**
  @def mysql_thread_register(P1, P2, P3)
  Thread registration.
*/
#define mysql_thread_register(P1, P2, P3) \
  inline_mysql_thread_register(P1, P2, P3)

/**
  @def mysql_thread_create(K, P1, P2, P3, P4)
  Instrumented my_thread_create.
  This function creates both the thread instrumentation and a thread.
  @c mysql_thread_create is a replacement for @c my_thread_create.
  The parameter P4 (or, if it is NULL, P1) will be used as the
  instrumented thread "identity".
  Providing a P1 / P4 parameter with a different value for each call
  will on average improve performances, since this thread identity value
  is used internally to randomize access to data and prevent contention.
  This is optional, and the improvement is not guaranteed, only statistical.
  @param K The PSI_thread_key for this instrumented thread
  @param P1 my_thread_create parameter 1
  @param P2 my_thread_create parameter 2
  @param P3 my_thread_create parameter 3
  @param P4 my_thread_create parameter 4
*/
#define mysql_thread_create(K, P1, P2, P3, P4) \
  inline_mysql_thread_create(K, 0, P1, P2, P3, P4)

/**
  @def mysql_thread_create_seq(K, S, P1, P2, P3, P4)
  Instrumented my_thread_create.
  @see mysql_thread_create.
  This forms takes an additional sequence number parameter,
  used to name threads "name-N" in the operating system.

  @param K The PSI_thread_key for this instrumented thread
  @param S The sequence number for this instrumented thread
  @param P1 my_thread_create parameter 1
  @param P2 my_thread_create parameter 2
  @param P3 my_thread_create parameter 3
  @param P4 my_thread_create parameter 4
*/
#define mysql_thread_create_seq(K, S, P1, P2, P3, P4) \
  inline_mysql_thread_create(K, S, P1, P2, P3, P4)

/**
  @def mysql_thread_set_psi_id(I)
  Set the thread identifier for the instrumentation.
  @param I The thread identifier
*/
#define mysql_thread_set_psi_id(I) inline_mysql_thread_set_psi_id(I)

/**
  @def mysql_thread_set_psi_THD(T)
  Set the thread sql session for the instrumentation.
  @param T The thread sql session
*/
#define mysql_thread_set_psi_THD(T) inline_mysql_thread_set_psi_THD(T)

static inline void inline_mysql_thread_register(const char *category
                                                [[maybe_unused]],
                                                PSI_thread_info *info
                                                [[maybe_unused]],
                                                int count [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(register_thread)(category, info, count);
#endif
}

static inline int inline_mysql_thread_create(
    PSI_thread_key key [[maybe_unused]],
    unsigned int sequence_number [[maybe_unused]], my_thread_handle *thread,
    const my_thread_attr_t *attr, my_start_routine start_routine, void *arg) {
  int result;
#ifdef HAVE_PSI_THREAD_INTERFACE
  result = PSI_THREAD_CALL(spawn_thread)(key, sequence_number, thread, attr,
                                         start_routine, arg);
#else
  result = my_thread_create(thread, attr, start_routine, arg);
#endif
  return result;
}

static inline void inline_mysql_thread_set_psi_id(my_thread_id id
                                                  [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  PSI_THREAD_CALL(set_thread_id)(psi, id);
#endif
}

#ifdef __cplusplus
class THD;
static inline void inline_mysql_thread_set_psi_THD(THD *thd [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  PSI_THREAD_CALL(set_thread_THD)(psi, thd);
#endif
}
#endif /* __cplusplus */

/**
  @def mysql_thread_set_peer_port()
  Set the remote (peer) port for the thread instrumentation.
  @param port peer port number
*/
static inline void mysql_thread_set_peer_port(uint port [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  PSI_THREAD_CALL(set_thread_peer_port)(psi, port);
#endif
}

/**
  @def mysql_thread_set_secondary_engine()
  Set the EXECUTION_ENGINE attribute for the thread instrumentation.
  @param secondary True for SECONDARY, false for PRIMARY.
*/
static inline void mysql_thread_set_secondary_engine(bool secondary
                                                     [[maybe_unused]]) {
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_thread_secondary_engine)(secondary);
#endif
}

/** @} (end of group psi_api_thread) */

#endif
