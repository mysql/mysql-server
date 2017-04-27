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

#ifndef MYSQL_PSI_RWLOCK_H
#define MYSQL_PSI_RWLOCK_H

/**
  @file include/mysql/psi/psi_rwlock.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_rwlock Rwlock Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
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
  @def PSI_RWLOCK_VERSION_1
  Performance Schema Rwlock Interface number for version 1.
  This version is supported.
*/
#define PSI_RWLOCK_VERSION_1 1

/**
  @def PSI_RWLOCK_VERSION_2
  Performance Schema Rwlock Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_RWLOCK_VERSION_2 2

/**
  @def PSI_CURRENT_RWLOCK_VERSION
  Performance Schema Rwlock Interface number for the most recent version.
  The most current version is @c PSI_RWLOCK_VERSION_1
*/
#define PSI_CURRENT_RWLOCK_VERSION 1

#ifndef USE_PSI_RWLOCK_2
#ifndef USE_PSI_RWLOCK_1
#define USE_PSI_RWLOCK_1
#endif /* USE_PSI_RWLOCK_1 */
#endif /* USE_PSI_RWLOCK_2 */

#ifdef USE_PSI_RWLOCK_1
#define HAVE_PSI_RWLOCK_1
#endif /* USE_PSI_RWLOCK_1 */

#ifdef USE_PSI_RWLOCK_2
#define HAVE_PSI_RWLOCK_2
#endif

/**
  Interface for an instrumented rwlock.
  This is an opaque structure.
*/
struct PSI_rwlock;
typedef struct PSI_rwlock PSI_rwlock;

/** Entry point for the performance schema interface. */
struct PSI_rwlock_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_RWLOCK_VERSION_1
    @sa PSI_RWLOCK_VERSION_2
    @sa PSI_CURRENT_RWLOCK_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_rwlock_bootstrap PSI_rwlock_bootstrap;

#ifdef HAVE_PSI_RWLOCK_1

/**
  Interface for an instrumented rwlock operation.
  This is an opaque structure.
*/
struct PSI_rwlock_locker;
typedef struct PSI_rwlock_locker PSI_rwlock_locker;

/**
  Operation performed on an instrumented rwlock.
  For basic READ / WRITE lock,
  operations are "READ" or "WRITE".
  For SX-locks, operations are "SHARED", "SHARED-EXCLUSIVE" or "EXCLUSIVE".
*/
enum PSI_rwlock_operation
{
  /** Read lock. */
  PSI_RWLOCK_READLOCK = 0,
  /** Write lock. */
  PSI_RWLOCK_WRITELOCK = 1,
  /** Read lock attempt. */
  PSI_RWLOCK_TRYREADLOCK = 2,
  /** Write lock attempt. */
  PSI_RWLOCK_TRYWRITELOCK = 3,

  /** Shared lock. */
  PSI_RWLOCK_SHAREDLOCK = 4,
  /** Shared Exclusive lock. */
  PSI_RWLOCK_SHAREDEXCLUSIVELOCK = 5,
  /** Exclusive lock. */
  PSI_RWLOCK_EXCLUSIVELOCK = 6,
  /** Shared lock attempt. */
  PSI_RWLOCK_TRYSHAREDLOCK = 7,
  /** Shared Exclusive lock attempt. */
  PSI_RWLOCK_TRYSHAREDEXCLUSIVELOCK = 8,
  /** Exclusive lock attempt. */
  PSI_RWLOCK_TRYEXCLUSIVELOCK = 9

};
typedef enum PSI_rwlock_operation PSI_rwlock_operation;

/**
  Rwlock information.
  @since PSI_RWLOCK_VERSION_1
  This structure is used to register an instrumented rwlock.
*/
struct PSI_rwlock_info_v1
{
  /**
    Pointer to the key assigned to the registered rwlock.
  */
  PSI_rwlock_key *m_key;
  /**
    The name of the rwlock to register.
  */
  const char *m_name;
  /**
    The flags of the rwlock to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};
typedef struct PSI_rwlock_info_v1 PSI_rwlock_info_v1;

/**
  State data storage for @c start_rwlock_rdwait_v1_t, @c
  start_rwlock_wrwait_v1_t.
  This structure provide temporary storage to a rwlock locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_rwlock_rdwait_v1_t
  @sa start_rwlock_wrwait_v1_t
*/
struct PSI_rwlock_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current operation. */
  enum PSI_rwlock_operation m_operation;
  /** Current rwlock. */
  struct PSI_rwlock *m_rwlock;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_wait;
};
typedef struct PSI_rwlock_locker_state_v1 PSI_rwlock_locker_state_v1;

/**
  Rwlock registration API.
  @param category a category name (typically a plugin name)
  @param info an array of rwlock info to register
  @param count the size of the info array
*/
typedef void (*register_rwlock_v1_t)(const char *category,
                                     struct PSI_rwlock_info_v1 *info,
                                     int count);

/**
  Rwlock instrumentation initialisation API.
  @param key the registered rwlock key
  @param identity the address of the rwlock itself
  @return an instrumented rwlock
*/
typedef struct PSI_rwlock *(*init_rwlock_v1_t)(PSI_rwlock_key key,
                                               const void *identity);

/**
  Rwlock instrumentation destruction API.
  @param rwlock the rwlock to destroy
*/
typedef void (*destroy_rwlock_v1_t)(struct PSI_rwlock *rwlock);

/**
  Record a rwlock instrumentation read wait start event.
  @param state data storage for the locker
  @param rwlock the instrumented rwlock to lock
  @param op the operation to perform
  @param src_file the source file name
  @param src_line the source line number
  @return a rwlock locker, or NULL
*/
typedef struct PSI_rwlock_locker *(*start_rwlock_rdwait_v1_t)(
  struct PSI_rwlock_locker_state_v1 *state,
  struct PSI_rwlock *rwlock,
  enum PSI_rwlock_operation op,
  const char *src_file,
  uint src_line);

/**
  Record a rwlock instrumentation read wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_rwlock_rdwait_v1_t)(struct PSI_rwlock_locker *locker,
                                       int rc);

/**
  Record a rwlock instrumentation write wait start event.
  @param state data storage for the locker
  @param rwlock the instrumented rwlock to lock
  @param op the operation to perform
  @param src_file the source file name
  @param src_line the source line number
  @return a rwlock locker, or NULL
*/
typedef struct PSI_rwlock_locker *(*start_rwlock_wrwait_v1_t)(
  struct PSI_rwlock_locker_state_v1 *state,
  struct PSI_rwlock *rwlock,
  enum PSI_rwlock_operation op,
  const char *src_file,
  uint src_line);

/**
  Record a rwlock instrumentation write wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_rwlock_wrwait_v1_t)(struct PSI_rwlock_locker *locker,
                                       int rc);

/**
  Record a rwlock instrumentation unlock event.
  @param rwlock the rwlock instrumentation
*/
typedef void (*unlock_rwlock_v1_t)(struct PSI_rwlock *rwlock);

/**
  Performance Schema Rwlock Interface, version 1.
  @since PSI_RWLOCK_VERSION_1
*/
struct PSI_rwlock_service_v1
{
  /** @sa register_rwlock_v1_t. */
  register_rwlock_v1_t register_rwlock;
  /** @sa init_rwlock_v1_t. */
  init_rwlock_v1_t init_rwlock;
  /** @sa destroy_rwlock_v1_t. */
  destroy_rwlock_v1_t destroy_rwlock;
  /** @sa start_rwlock_rdwait_v1_t. */
  start_rwlock_rdwait_v1_t start_rwlock_rdwait;
  /** @sa end_rwlock_rdwait_v1_t. */
  end_rwlock_rdwait_v1_t end_rwlock_rdwait;
  /** @sa start_rwlock_wrwait_v1_t. */
  start_rwlock_wrwait_v1_t start_rwlock_wrwait;
  /** @sa end_rwlock_wrwait_v1_t. */
  end_rwlock_wrwait_v1_t end_rwlock_wrwait;
  /** @sa unlock_rwlock_v1_t. */
  unlock_rwlock_v1_t unlock_rwlock;
};

#endif /* HAVE_PSI_RWLOCK_1 */

/* Export the required version */
#ifdef USE_PSI_RWLOCK_1
typedef struct PSI_rwlock_service_v1 PSI_rwlock_service_t;
typedef struct PSI_rwlock_info_v1 PSI_rwlock_info;
typedef struct PSI_rwlock_locker_state_v1 PSI_rwlock_locker_state;
#else
typedef struct PSI_placeholder PSI_rwlock_service_t;
typedef struct PSI_placeholder PSI_rwlock_info;
typedef struct PSI_placeholder PSI_rwlock_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_rwlock_service_t *psi_rwlock_service;

/** @} (end of group psi_abi_rwlock) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_RWLOCK_H */
