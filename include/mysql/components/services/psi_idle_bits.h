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

#ifndef COMPONENTS_SERVICES_PSI_IDLE_BITS_H
#define COMPONENTS_SERVICES_PSI_IDLE_BITS_H

#include "my_inttypes.h"
#include "my_macros.h"

C_MODE_START

/**
  @file
  Performance schema instrumentation interface.

  @defgroup psi_abi_idle Idle Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

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
typedef struct PSI_idle_locker *(*start_idle_wait_v1_t)(
  struct PSI_idle_locker_state_v1 *state, const char *src_file, uint src_line);

/**
  Record an idle instrumentation wait end event.
  @param locker a thread locker for the running thread
*/
typedef void (*end_idle_wait_v1_t)(struct PSI_idle_locker *locker);

typedef struct PSI_idle_locker_state_v1 PSI_idle_locker_state;

/** @} (end of group psi_abi_idle) */

C_MODE_END

#endif /* COMPONENTS_SERVICES_PSI_IDLE_BITS_H */
