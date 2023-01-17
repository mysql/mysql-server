/*****************************************************************************
Copyright (c) 1995, 2023, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/os0atomic.h
 Macros for using atomics

 Created 2012-09-23 Sunny Bains (Split from os0sync.h)
 *******************************************************/

#ifndef os0atomic_h
#define os0atomic_h

#include "univ.i"

/** barrier definitions for memory ordering */
#ifdef HAVE_IB_GCC_ATOMIC_THREAD_FENCE
#define HAVE_MEMORY_BARRIER
#define os_rmb __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define os_wmb __atomic_thread_fence(__ATOMIC_RELEASE)
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "GCC builtin __atomic_thread_fence() is used for memory barrier"

#elif defined(HAVE_IB_GCC_SYNC_SYNCHRONISE)
#define HAVE_MEMORY_BARRIER
#define os_rmb __sync_synchronize()
#define os_wmb __sync_synchronize()
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "GCC builtin __sync_synchronize() is used for memory barrier"

#elif defined(HAVE_IB_MACHINE_BARRIER_SOLARIS)
#define HAVE_MEMORY_BARRIER
#include <mbarrier.h>
#define os_rmb __machine_r_barrier()
#define os_wmb __machine_w_barrier()
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "Solaris memory ordering functions are used for memory barrier"

#elif defined(HAVE_WINDOWS_MM_FENCE) && defined(_WIN64)
#define HAVE_MEMORY_BARRIER
#include <mmintrin.h>
#define os_rmb _mm_lfence()
#define os_wmb _mm_sfence()
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "_mm_lfence() and _mm_sfence() are used for memory barrier"

#else
#define os_rmb
#define os_wmb
#define IB_MEMORY_BARRIER_STARTUP_MSG "Memory barrier is not used"
#endif

#endif /* !os0atomic_h */
