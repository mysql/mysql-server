/*****************************************************************************

Copyright (c) 2013, 2014, Oracle and/or its affiliates. All Rights Reserved.

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

/**
Initializes the synchronization data structures. */
void
sync_check_init();

/**
Enable sync order checking. */
void
sync_check_enable();

/**
Frees the resources in synchronization data structures. */
void
sync_check_close();

/**
Check if it is OK to acquire the latch.
@param latch - latch type */
void
sync_check_lock(const latch_t* latch);

/**
Check if it is OK to acquire the latch.
@param latch - latch type
@param level - the level of the mutex */
void
sync_check_lock(const latch_t* latch, latch_level_t level);

/**
Check if it is OK to re-acquire the lock. */
void
sync_check_relock(const latch_t* latch);

/**
Removes a latch from the thread level array if it is found there.
@param latch - to unlock */
void
sync_check_unlock(const latch_t* latch);

/**
Checks if the level array for the current thread contains a
mutex or rw-latch at the specified level.
@param level - to find
@return	a matching latch, or NULL if not found */
const latch_t*
sync_check_find(latch_level_t level);

/**
Checks that the level array for the current thread is empty. Terminate
iteration if the functor returns true.
@param functor - called for each element.
@return true if the functor returns true */
bool
sync_check_iterate(sync_check_functor_t& functor);

/**
Get the sync level for a latch name.
@param name - latch name
@return SYNC_UNKNOWN - if not found. */
latch_level_t
sync_latch_get_level(const char* name);

/**
Get the latch name from a sync level.
@param level - latch level to look for
@return 0 if not found. */
const char*
sync_latch_get_name(latch_level_t level)
	__attribute__((warn_unused_result));

#ifdef UNIV_PFS_MUTEX
/**
Get the sync level for a latch name.
@param name - latch name to look for
@return SYNC_UNKNOWN - if not found. */
mysql_pfs_key_t
sync_latch_get_pfs_key(const char* name);
#endif /* UNIV_PFS_MUTEX */

/**
Add the latch meta data of Latch level is SYNC_NO_ORDER_CHECK.
@param name		Latch name
@param key		Performance schema key */
void
sync_latch_add_no_check(
	const char*		name
#ifdef UNIV_PFS_MUTEX
	,mysql_pfs_key_t	key
#endif /* UNIV_PFS_MUTEX */
	);

#ifdef UNIV_PFS_MUTEX
/** Wrapper around latch_add() - PFS version. */
#define SYNC_LATCH_ADD(m, n)	sync_latch_add_no_check((m), (n))
#else
/** Wrapper around latch_add() */
#define SYNC_LATCH_ADD(m, n)	sync_latch_add_no_check((m))
#endif /* UNIV_PFS_MUTEX */

#endif /* !sync0debug_h */
