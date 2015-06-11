/*****************************************************************************

Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/sync0debug.h
Debug checks for latches, header file

Created 2012-08-21 Sunny Bains
*******************************************************/

#ifndef sync0debug_h
#define sync0debug_h

#include "univ.i"
#include "sync0types.h"

/** Initializes the synchronization data structures. */
void
sync_check_init();

/** Frees the resources in synchronization data structures. */
void
sync_check_close();

#ifdef UNIV_DEBUG
/** Enable sync order checking. */
void
sync_check_enable();

/** Check if it is OK to acquire the latch.
@param[in]	latch	latch type */
void
sync_check_lock_validate(const latch_t* latch);

/** Note that the lock has been granted
@param[in]	latch	latch type */
void
sync_check_lock_granted(const latch_t* latch);

/** Check if it is OK to acquire the latch.
@param[in]	latch	latch type
@param[in]	level	the level of the mutex */
void
sync_check_lock(const latch_t* latch, latch_level_t level);

/**
Check if it is OK to re-acquire the lock. */
void
sync_check_relock(const latch_t* latch);

/** Removes a latch from the thread level array if it is found there.
@param[in]	latch	to unlock */
void
sync_check_unlock(const latch_t* latch);

/** Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@param[in]	level	to find
@return	a matching latch, or NULL if not found */
const latch_t*
sync_check_find(latch_level_t level);

/** Checks that the level array for the current thread is empty.
Terminate iteration if the functor returns true.
@param[in,out]	 functor	called for each element.
@return true if the functor returns true */
bool
sync_check_iterate(sync_check_functor_t& functor);

/** Acquires the debug mutex. We cannot use the mutex defined in sync0sync,
because the debug mutex is also acquired in sync0arr while holding the OS
mutex protecting the sync array, and the ordinary mutex_enter might
recursively call routines in sync0arr, leading to a deadlock on the OS
mutex. */
void
rw_lock_debug_mutex_enter();

/** Releases the debug mutex. */
void
rw_lock_debug_mutex_exit();

#endif /* UNIV_DEBUG */

#endif /* !sync0debug_h */
