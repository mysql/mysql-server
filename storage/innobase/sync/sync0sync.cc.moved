/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file sync/sync0sync.cc
Mutex, the basic synchronization primitive

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#include "univ.i"
#include "sync0rw.h"
#include "sync0sync.h"

/** The number of iterations in the mutex_spin_wait() spin loop.
Intended for performance monitoring. */
UNIV_INTERN mutex_counter_t	mutex_spin_round_count;

/** The number of mutex_spin_wait() calls.  Intended for
performance monitoring. */
UNIV_INTERN mutex_counter_t	mutex_spin_wait_count;

/** The number of OS waits in mutex_spin_wait().  Intended for
performance monitoring. */
UNIV_INTERN mutex_counter_t	mutex_os_wait_count;

/**
Prints wait info of the sync system.
@param file - where to print */
static
void
sync_print_wait_info(FILE* file)
{
	fprintf(file,
		"Mutex spin waits "UINT64PF", rounds "UINT64PF", "
		"OS waits "UINT64PF"\n"
		"RW-shared spins "UINT64PF", rounds "UINT64PF", "
		"OS waits "UINT64PF"\n"
		"RW-excl spins "UINT64PF", rounds "UINT64PF", "
		"OS waits "UINT64PF"\n",
		(ib_uint64_t) mutex_spin_wait_count,
		(ib_uint64_t) mutex_spin_round_count,
		(ib_uint64_t) mutex_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_s_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_s_os_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_wait_count,
		(ib_uint64_t) rw_lock_stats.rw_x_spin_round_count,
		(ib_uint64_t) rw_lock_stats.rw_x_os_wait_count);

	fprintf(file,
		"Spin rounds per wait: %.2f mutex, %.2f RW-shared, "
		"%.2f RW-excl\n",
		(double) mutex_spin_round_count_get() /
		(mutex_spin_wait_count_get() ? mutex_spin_wait_count_get() : 1),
		(double) rw_lock_stats.rw_s_spin_round_count /
		(rw_lock_stats.rw_s_spin_wait_count
		 ? rw_lock_stats.rw_s_spin_wait_count : 1),
		(double) rw_lock_stats.rw_x_spin_round_count /
		(rw_lock_stats.rw_x_spin_wait_count
		 ? rw_lock_stats.rw_x_spin_wait_count : 1));
}

/**
Prints info of the sync system. */
UNIV_INTERN
void
sync_print(
/*=======*/
	FILE*	file)				/*!< in/out: where to print */
{
#ifdef UNIV_SYNC_DEBUG
	mutex_list_print_info(file);

	rw_lock_list_print_info(file);
#endif /* UNIV_SYNC_DEBUG */

	sync_array_print(file);

	sync_print_wait_info(file);
}

