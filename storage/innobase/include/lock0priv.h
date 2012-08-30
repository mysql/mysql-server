/*****************************************************************************

Copyright (c) 2007, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/lock0priv.h
Lock module internal structures and methods.

Created July 12, 2007 Vasil Dimov
*******************************************************/

#ifndef lock0priv_h
#define lock0priv_h

#ifndef LOCK_MODULE_IMPLEMENTATION
/* If you need to access members of the structures defined in this
file, please write appropriate functions that retrieve them and put
those functions in lock/ */
#error Do not include lock0priv.h outside of the lock/ module
#endif

#include "univ.i"
#include "dict0types.h"
#include "hash0hash.h"
#include "trx0types.h"
#include "ut0lst.h"

/** A table lock */
struct lock_table_t {
	dict_table_t*	table;		/*!< database table in dictionary
					cache */
	UT_LIST_NODE_T(lock_t)
			locks;		/*!< list of locks on the same
					table */
};

/** Record lock for a page */
struct lock_rec_t {
	ulint	space;			/*!< space id */
	ulint	page_no;		/*!< page number */
	ulint	n_bits;			/*!< number of bits in the lock
					bitmap; NOTE: the lock bitmap is
					placed immediately after the
					lock struct */
};

/** Lock struct; protected by lock_sys->mutex */
struct lock_t {
	trx_t*		trx;		/*!< transaction owning the
					lock */
	UT_LIST_NODE_T(lock_t)
			trx_locks;	/*!< list of the locks of the
					transaction */
	ulint		type_mode;	/*!< lock type, mode, LOCK_GAP or
					LOCK_REC_NOT_GAP,
					LOCK_INSERT_INTENTION,
					wait flag, ORed */
	hash_node_t	hash;		/*!< hash chain node for a record
					lock */
	dict_index_t*	index;		/*!< index for a record lock */
	union {
		lock_table_t	tab_lock;/*!< table lock */
		lock_rec_t	rec_lock;/*!< record lock */
	} un_member;			/*!< lock details */
};

/*********************************************************************//**
Gets the type of a lock.
@return	LOCK_TABLE or LOCK_REC */
UNIV_INLINE
ulint
lock_get_type_low(
/*==============*/
	const lock_t*	lock);	/*!< in: lock */

/*********************************************************************//**
Gets the previous record lock set on a record.
@return	previous lock on the same record, NULL if none exists */
UNIV_INTERN
const lock_t*
lock_rec_get_prev(
/*==============*/
	const lock_t*	in_lock,/*!< in: record lock */
	ulint		heap_no);/*!< in: heap number of the record */

/*********************************************************************//**
Cancels a waiting lock request and releases possible other transactions
waiting behind it. */
UNIV_INTERN
void
lock_cancel_waiting_and_release(
/*============================*/
	lock_t*	lock);	/*!< in/out: waiting lock request */

/*********************************************************************//**
Checks if some transaction has an implicit x-lock on a record in a clustered
index.
@return	transaction id of the transaction which has the x-lock, or 0 */
UNIV_INLINE
trx_id_t
lock_clust_rec_some_has_impl(
/*=========================*/
	const rec_t*		rec,	/*!< in: user record */
	const dict_index_t*	index,	/*!< in: clustered index */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
	__attribute__((nonnull, warn_unused_result));

#ifndef UNIV_NONINL
#include "lock0priv.ic"
#endif

#endif /* lock0priv_h */
