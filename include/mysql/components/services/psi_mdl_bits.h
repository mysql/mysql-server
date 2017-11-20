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

#ifndef COMPONENTS_SERVICES_PSI_MDL_BITS_H
#define COMPONENTS_SERVICES_PSI_MDL_BITS_H

#include "my_inttypes.h"
#include "my_macros.h"

C_MODE_START

/**
  @file
  Performance schema instrumentation interface.

  @defgroup psi_abi_mdl Metadata Lock Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

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

typedef struct PSI_metadata_locker_state_v1 PSI_metadata_locker_state;

/** @} (end of group psi_abi_mdl) */

C_MODE_END

#endif /* COMPONENTS_SERVICES_PSI_MDL_BITS_H */
