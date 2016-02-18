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

#ifndef MYSQL_PSI_IDLE_H
#define MYSQL_PSI_IDLE_H

/**
  @file include/mysql/psi/psi_idle.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_idle Idle Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_global.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_IDLE_VERSION_1
  Performance Schema Idle Interface number for version 1.
  This version is supported.
*/
#define PSI_IDLE_VERSION_1 1

/**
  @def PSI_IDLE_VERSION_2
  Performance Schema Idle Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_IDLE_VERSION_2 2

/**
  @def PSI_CURRENT_IDLE_VERSION
  Performance Schema Idle Interface number for the most recent version.
  The most current version is @c PSI_IDLE_VERSION_1
*/
#define PSI_CURRENT_IDLE_VERSION 1

#ifndef USE_PSI_IDLE_2
#ifndef USE_PSI_IDLE_1
#define USE_PSI_IDLE_1
#endif /* USE_PSI_IDLE_1 */
#endif /* USE_PSI_IDLE_2 */

#ifdef USE_PSI_IDLE_1
#define HAVE_PSI_IDLE_1
#endif /* USE_PSI_IDLE_1 */

#ifdef USE_PSI_IDLE_2
#define HAVE_PSI_IDLE_2
#endif /* USE_PSI_IDLE_2 */

/** Entry point for the performance schema interface. */
struct PSI_idle_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_IDLE_VERSION_1
    @sa PSI_IDLE_VERSION_2
    @sa PSI_CURRENT_IDLE_VERSION
  */
  void* (*get_interface)(int version);
};
typedef struct PSI_idle_bootstrap PSI_idle_bootstrap;

#ifdef HAVE_PSI_IDLE_1

/**
  Interface for an instrumented idle operation.
  This is an opaque structure.
*/
struct PSI_idle_locker;
typedef struct PSI_idle_locker PSI_idle_locker;

/**
  State data storage for @c start_idle_wait_v1_t.
  This structure provide temporary storage to an idle locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_idle_wait_v1_t.
*/
struct PSI_idle_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_wait;
};
typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state_v1;

/**
  Record an idle instrumentation wait start event.
  @param state data storage for the locker
  @param src_file the source file name
  @param src_line the source line number
  @return an idle locker, or NULL
*/
typedef struct PSI_idle_locker* (*start_idle_wait_v1_t)
  (struct PSI_idle_locker_state_v1 *state, const char *src_file, uint src_line);

/**
  Record an idle instrumentation wait end event.
  @param locker a thread locker for the running thread
*/
typedef void (*end_idle_wait_v1_t)
  (struct PSI_idle_locker *locker);

/**
  Performance Schema Idle Interface, version 1.
  @since PSI_IDLE_VERSION_1
*/
struct PSI_idle_service_v1
{
  /** @sa start_idle_wait_v1_t. */
  start_idle_wait_v1_t start_idle_wait;
  /** @sa end_idle_wait_v1_t. */
  end_idle_wait_v1_t end_idle_wait;
};

#endif /* HAVE_PSI_IDLE_1 */

/* Export the required version */
#ifdef USE_PSI_IDLE_1
typedef struct PSI_idle_service_v1 PSI_idle_service_t;
typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state;
#else
typedef struct PSI_placeholder PSI_idle_service_t;
typedef struct PSI_placeholder PSI_idle_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_idle_service_t *psi_idle_service;

/** @} (end of group psi_abi_idle) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_IDLE_H */

