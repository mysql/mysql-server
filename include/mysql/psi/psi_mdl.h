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

#ifndef MYSQL_PSI_MDL_H
#define MYSQL_PSI_MDL_H

/**
  @file include/mysql/psi/psi_mdl.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_mdl Metadata Lock Instrumentation (ABI)
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
  @def PSI_MDL_VERSION_1
  Performance Schema Metadata Lock Interface number for version 1.
  This version is supported.
*/
#define PSI_MDL_VERSION_1 1

/**
  @def PSI_MDL_VERSION_2
  Performance Schema Metadata Lock Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_MDL_VERSION_2 2

/**
  @def PSI_CURRENT_MDL_VERSION
  Performance Schema Metadata Lock Interface number for the most recent version.
  The most current version is @c PSI_MDL_VERSION_1
*/
#define PSI_CURRENT_MDL_VERSION 1

#ifndef USE_PSI_MDL_2
#ifndef USE_PSI_MDL_1
#define USE_PSI_MDL_1
#endif /* USE_PSI_MDL_1 */
#endif /* USE_PSI_MDL_2 */

#ifdef USE_PSI_MDL_1
#define HAVE_PSI_MDL_1
#endif /* USE_PSI_MDL_1 */

#ifdef USE_PSI_MDL_2
#define HAVE_PSI_MDL_2
#endif /* USE_PSI_MDL_2 */

/** Entry point for the performance schema interface. */
struct PSI_mdl_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_MDL_VERSION_1
    @sa PSI_MDL_VERSION_2
    @sa PSI_CURRENT_MDL_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_mdl_bootstrap PSI_mdl_bootstrap;

#ifdef HAVE_PSI_MDL_1

struct MDL_key;

/** @sa enum_mdl_type. */
typedef int opaque_mdl_type;

/** @sa enum_mdl_duration. */
typedef int opaque_mdl_duration;

/** @sa MDL_wait::enum_wait_status. */
typedef int opaque_mdl_status;

/**
  Interface for an instrumented metadata lock.
  This is an opaque structure.
*/
struct PSI_metadata_lock;
typedef struct PSI_metadata_lock PSI_metadata_lock;

/**
  Interface for an instrumented MDL operation.
  This is an opaque structure.
*/
struct PSI_metadata_locker;
typedef struct PSI_metadata_locker PSI_metadata_locker;

/**
  State data storage for @c start_metadata_wait_v1_t.
  This structure provide temporary storage to a metadata locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa start_metadata_wait_v1_t
*/
struct PSI_metadata_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current metadata lock. */
  struct PSI_metadata_lock *m_metadata_lock;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_wait;
};
typedef struct PSI_metadata_locker_state_v1 PSI_metadata_locker_state_v1;

typedef PSI_metadata_lock *(*create_metadata_lock_v1_t)(
  void *identity,
  const struct MDL_key *key,
  opaque_mdl_type mdl_type,
  opaque_mdl_duration mdl_duration,
  opaque_mdl_status mdl_status,
  const char *src_file,
  uint src_line);

typedef void (*set_metadata_lock_status_v1_t)(PSI_metadata_lock *lock,
                                              opaque_mdl_status mdl_status);

typedef void (*destroy_metadata_lock_v1_t)(PSI_metadata_lock *lock);

typedef struct PSI_metadata_locker *(*start_metadata_wait_v1_t)(
  struct PSI_metadata_locker_state_v1 *state,
  struct PSI_metadata_lock *mdl,
  const char *src_file,
  uint src_line);

typedef void (*end_metadata_wait_v1_t)(struct PSI_metadata_locker *locker,
                                       int rc);

/**
  Performance Schema Metadata Lock Interface, version 1.
  @since PSI_TRANSACTION_VERSION_1
*/
struct PSI_mdl_service_v1
{
  create_metadata_lock_v1_t create_metadata_lock;
  set_metadata_lock_status_v1_t set_metadata_lock_status;
  destroy_metadata_lock_v1_t destroy_metadata_lock;
  start_metadata_wait_v1_t start_metadata_wait;
  end_metadata_wait_v1_t end_metadata_wait;
};

#endif /* HAVE_PSI_MDL_1 */

/* Export the required version */
#ifdef USE_PSI_MDL_1
typedef struct PSI_mdl_service_v1 PSI_mdl_service_t;
typedef struct PSI_metadata_locker_state_v1 PSI_metadata_locker_state;
#else
typedef struct PSI_placeholder PSI_mdl_service_t;
typedef struct PSI_placeholder PSI_metadata_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_mdl_service_t *psi_mdl_service;

/** @} (end of group psi_abi_mdl) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_MDL_H */
