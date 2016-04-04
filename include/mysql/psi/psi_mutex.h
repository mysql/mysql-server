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

#ifndef MYSQL_PSI_MUTEX_H
#define MYSQL_PSI_MUTEX_H

/**
  @file include/mysql/psi/psi_mutex.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_mutex Mutex Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_global.h"
#include "psi_base.h"

/*
  MAINTAINER:
  The following pattern:
    typedef struct XYZ XYZ;
  is not needed in C++, but required for C.
*/

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_MUTEX_VERSION_1
  Performance Schema Mutex Interface number for version 1.
  This version is supported.
*/
#define PSI_MUTEX_VERSION_1 1

/**
  @def PSI_MUTEX_VERSION_2
  Performance Schema Mutex Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_MUTEX_VERSION_2 2

/**
  @def PSI_CURRENT_MUTEX_VERSION
  Performance Schema Mutex Interface number for the most recent version.
  The most current version is @c PSI_MUTEX_VERSION_1
*/
#define PSI_CURRENT_MUTEX_VERSION 1

#ifndef USE_PSI_MUTEX_2
#ifndef USE_PSI_MUTEX_1
#define USE_PSI_MUTEX_1
#endif /* USE_PSI_MUTEX_1 */
#endif /* USE_PSI_MUTEX_2 */

#ifdef USE_PSI_MUTEX_1
#define HAVE_PSI_MUTEX_1
#endif /* USE_PSI_MUTEX_1 */

#ifdef USE_PSI_MUTEX_2
#define HAVE_PSI_MUTEX_2
#endif

/**
  Interface for an instrumented mutex.
  This is an opaque structure.
*/
struct PSI_mutex;
typedef struct PSI_mutex PSI_mutex;

/** Entry point for the performance schema interface. */
struct PSI_mutex_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_MUTEX_VERSION_1
    @sa PSI_MUTEX_VERSION_2
    @sa PSI_CURRENT_MUTEX_VERSION
  */
  void* (*get_interface)(int version);
};
typedef struct PSI_mutex_bootstrap PSI_mutex_bootstrap;

#ifdef HAVE_PSI_MUTEX_1

/**
  Interface for an instrumented mutex operation.
  This is an opaque structure.
*/
struct PSI_mutex_locker;
typedef struct PSI_mutex_locker PSI_mutex_locker;

/** Operation performed on an instrumented mutex. */
enum PSI_mutex_operation
{
  /** Lock. */
  PSI_MUTEX_LOCK= 0,
  /** Lock attempt. */
  PSI_MUTEX_TRYLOCK= 1
};
typedef enum PSI_mutex_operation PSI_mutex_operation;

/**
  Mutex information.
  @since PSI_MUTEX_VERSION_1
  This structure is used to register an instrumented mutex.
*/
struct PSI_mutex_info_v1
{
  /**
    Pointer to the key assigned to the registered mutex.
  */
  PSI_mutex_key *m_key;
  /**
    The name of the mutex to register.
  */
  const char *m_name;
  /**
    The flags of the mutex to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
  int m_volatility;
};
typedef struct PSI_mutex_info_v1 PSI_mutex_info_v1;

/**
  State data storage for @c start_mutex_wait_v1_t.
  This structure provide temporary storage to a mutex locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_mutex_wait_v1_t
*/
struct PSI_mutex_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current operation. */
  enum PSI_mutex_operation m_operation;
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
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state_v1;

/**
  Mutex registration API.
  @param category a category name (typically a plugin name)
  @param info an array of mutex info to register
  @param count the size of the info array
*/
typedef void (*register_mutex_v1_t)
  (const char *category, struct PSI_mutex_info_v1 *info, int count);

/**
  Mutex instrumentation initialisation API.
  @param key the registered mutex key
  @param identity the address of the mutex itself
  @return an instrumented mutex
*/
typedef struct PSI_mutex* (*init_mutex_v1_t)
  (PSI_mutex_key key, const void *identity);

/**
  Mutex instrumentation destruction API.
  @param mutex the mutex to destroy
*/
typedef void (*destroy_mutex_v1_t)(struct PSI_mutex *mutex);

/**
  Record a mutex instrumentation unlock event.
  @param mutex the mutex instrumentation
*/
typedef void (*unlock_mutex_v1_t)
  (struct PSI_mutex *mutex);

/**
  Record a mutex instrumentation wait start event.
  @param state data storage for the locker
  @param mutex the instrumented mutex to lock
  @param op the operation to perform
  @param src_file the source file name
  @param src_line the source line number
  @return a mutex locker, or NULL
*/
typedef struct PSI_mutex_locker* (*start_mutex_wait_v1_t)
  (struct PSI_mutex_locker_state_v1 *state,
   struct PSI_mutex *mutex,
   enum PSI_mutex_operation op,
   const char *src_file, uint src_line);

/**
  Record a mutex instrumentation wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_mutex_wait_v1_t)
  (struct PSI_mutex_locker *locker, int rc);

/**
  Performance Schema Mutex Interface, version 1.
  @since PSI_MUTEX_VERSION_1
*/
struct PSI_mutex_service_v1
{
  /** @sa register_mutex_v1_t. */
  register_mutex_v1_t register_mutex;
  /** @sa init_mutex_v1_t. */
  init_mutex_v1_t init_mutex;
  /** @sa destroy_mutex_v1_t. */
  destroy_mutex_v1_t destroy_mutex;
  /** @sa start_mutex_wait_v1_t. */
  start_mutex_wait_v1_t start_mutex_wait;
  /** @sa end_mutex_wait_v1_t. */
  end_mutex_wait_v1_t end_mutex_wait;
  /** @sa unlock_mutex_v1_t. */
  unlock_mutex_v1_t unlock_mutex;
};

#endif /* HAVE_PSI_MUTEX_1 */

/**
  @typedef PSI_mutex_info
  The mutex information structure for the current version.
*/

/* Export the required version */
#ifdef USE_PSI_MUTEX_1
typedef struct PSI_mutex_service_v1 PSI_mutex_service_t;
typedef struct PSI_mutex_info_v1 PSI_mutex_info;
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state;
#else
typedef struct PSI_placeholder PSI_mutex_service_t;
typedef struct PSI_placeholder PSI_mutex_info;
typedef struct PSI_placeholder PSI_mutex_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_mutex_service_t *psi_mutex_service;

/** @} (end of group psi_abi_mutex) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_MUTEX_H */

