/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_COND_H
#define MYSQL_PSI_COND_H

/**
  @file include/mysql/psi/psi_cond.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_cond Cond Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_COND_VERSION_1
  Performance Schema Cond Interface number for version 1.
  This version is supported.
*/
#define PSI_COND_VERSION_1 1

/**
  @def PSI_COND_VERSION_2
  Performance Schema Cond Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_COND_VERSION_2 2

/**
  @def PSI_CURRENT_COND_VERSION
  Performance Schema Cond Interface number for the most recent version.
  The most current version is @c PSI_COND_VERSION_1
*/
#define PSI_CURRENT_COND_VERSION 1

#ifndef USE_PSI_COND_2
#ifndef USE_PSI_COND_1
#define USE_PSI_COND_1
#endif /* USE_PSI_COND_1 */
#endif /* USE_PSI_COND_2 */

#ifdef USE_PSI_COND_1
#define HAVE_PSI_COND_1
#endif /* USE_PSI_COND_1 */

#ifdef USE_PSI_COND_2
#define HAVE_PSI_COND_2
#endif

/**
  Interface for an instrumented condition.
  This is an opaque structure.
*/
struct PSI_cond;
typedef struct PSI_cond PSI_cond;

/** Entry point for the performance schema interface. */
struct PSI_cond_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_COND_VERSION_1
    @sa PSI_COND_VERSION_2
    @sa PSI_CURRENT_COND_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_cond_bootstrap PSI_cond_bootstrap;

#ifdef HAVE_PSI_COND_1

/**
  Interface for an instrumented condition operation.
  This is an opaque structure.
*/
struct PSI_cond_locker;
typedef struct PSI_cond_locker PSI_cond_locker;

/** Operation performed on an instrumented condition. */
enum PSI_cond_operation
{
  /** Wait. */
  PSI_COND_WAIT = 0,
  /** Wait, with timeout. */
  PSI_COND_TIMEDWAIT = 1
};
typedef enum PSI_cond_operation PSI_cond_operation;

/**
  Condition information.
  @since PSI_COND_VERSION_1
  This structure is used to register an instrumented cond.
*/
struct PSI_cond_info_v1
{
  /**
    Pointer to the key assigned to the registered cond.
  */
  PSI_cond_key *m_key;
  /**
    The name of the cond to register.
  */
  const char *m_name;
  /**
    The flags of the cond to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};
typedef struct PSI_cond_info_v1 PSI_cond_info_v1;

/**
  State data storage for @c start_cond_wait_v1_t.
  This structure provide temporary storage to a condition locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_cond_wait_v1_t
*/
struct PSI_cond_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current operation. */
  enum PSI_cond_operation m_operation;
  /** Current condition. */
  struct PSI_cond *m_cond;
  /** Current mutex. */
  struct PSI_mutex *m_mutex;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_wait;
};
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state_v1;

/**
  Cond registration API.
  @param category a category name (typically a plugin name)
  @param info an array of cond info to register
  @param count the size of the info array
*/
typedef void (*register_cond_v1_t)(const char *category,
                                   struct PSI_cond_info_v1 *info,
                                   int count);

/**
  Cond instrumentation initialisation API.
  @param key the registered key
  @param identity the address of the cond itself
  @return an instrumented cond
*/
typedef struct PSI_cond *(*init_cond_v1_t)(PSI_cond_key key,
                                           const void *identity);

/**
  Cond instrumentation destruction API.
  @param cond the rcond to destroy
*/
typedef void (*destroy_cond_v1_t)(struct PSI_cond *cond);

/**
  Record a condition instrumentation signal event.
  @param cond the cond instrumentation
*/
typedef void (*signal_cond_v1_t)(struct PSI_cond *cond);

/**
  Record a condition instrumentation broadcast event.
  @param cond the cond instrumentation
*/
typedef void (*broadcast_cond_v1_t)(struct PSI_cond *cond);

/**
  Record a condition instrumentation wait start event.
  @param state data storage for the locker
  @param cond the instrumented cond to lock
  @param op the operation to perform
  @param src_file the source file name
  @param src_line the source line number
  @return a cond locker, or NULL
*/
typedef struct PSI_cond_locker *(*start_cond_wait_v1_t)(
  struct PSI_cond_locker_state_v1 *state,
  struct PSI_cond *cond,
  struct PSI_mutex *mutex,
  enum PSI_cond_operation op,
  const char *src_file,
  uint src_line);

/**
  Record a condition instrumentation wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_cond_wait_v1_t)(struct PSI_cond_locker *locker, int rc);

/**
  Performance Schema Cond Interface, version 1.
  @since PSI_COND_VERSION_1
*/
struct PSI_cond_service_v1
{
  /** @sa register_cond_v1_t. */
  register_cond_v1_t register_cond;
  /** @sa init_cond_v1_t. */
  init_cond_v1_t init_cond;
  /** @sa destroy_cond_v1_t. */
  destroy_cond_v1_t destroy_cond;
  /** @sa signal_cond_v1_t. */
  signal_cond_v1_t signal_cond;
  /** @sa broadcast_cond_v1_t. */
  broadcast_cond_v1_t broadcast_cond;
  /** @sa start_cond_wait_v1_t. */
  start_cond_wait_v1_t start_cond_wait;
  /** @sa end_cond_wait_v1_t. */
  end_cond_wait_v1_t end_cond_wait;
};

#endif /* HAVE_PSI_COND_1 */

/* Export the required version */
#ifdef USE_PSI_COND_1
typedef struct PSI_cond_service_v1 PSI_cond_service_t;
typedef struct PSI_cond_info_v1 PSI_cond_info;
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state;
#else
typedef struct PSI_placeholder PSI_cond_service_t;
typedef struct PSI_placeholder PSI_cond_info;
typedef struct PSI_placeholder PSI_cond_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_cond_service_t *psi_cond_service;

/** @} (end of group psi_abi_cond) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_MUTEX_H */
