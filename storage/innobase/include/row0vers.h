/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0vers.h
Row versions

Created 2/6/1997 Heikki Tuuri
*******************************************************/

#ifndef row0vers_h
#define row0vers_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "rem0types.h"
#include "mtr0mtr.h"
#include "dict0mem.h"

// Forward declaration
class ReadView;

/*****************************************************************//**
Finds out if an active transaction has inserted or modified a secondary
index record.
@return 0 if committed, else the active transaction id;
NOTE that this function can return false positives but never false
negatives. The caller must confirm all positive results by calling
trx_is_active() while holding lock_sys->mutex. */
trx_t*
row_vers_impl_x_locked(
/*===================*/
	const rec_t*	rec,	/*!< in: record in a secondary index */
	dict_index_t*	index,	/*!< in: the secondary index */
	const ulint*	offsets);/*!< in: rec_get_offsets(rec, index) */
/*****************************************************************//**
Finds out if we must preserve a delete marked earlier version of a clustered
index record, because it is >= the purge view.
@param[in]	trx_id		transaction id in the version
@param[in]	name		table name
@param[in,out]	mtr		mini transaction  holding the latch on the
				clustered index record; it will also hold
				 the latch on purge_view
@return TRUE if earlier version should be preserved */
ibool
row_vers_must_preserve_del_marked(
/*==============================*/
	trx_id_t		trx_id,
	const table_name_t&	name,
	mtr_t*			mtr);

/*****************************************************************//**
Finds out if a version of the record, where the version >= the current
purge view, should have ientry as its secondary index entry. We check
if there is any not delete marked version of the record where the trx
id >= purge view, and the secondary index entry == ientry; exactly in
this case we return TRUE.
@return TRUE if earlier version should have */
ibool
row_vers_old_has_index_entry(
/*=========================*/
	ibool		also_curr,/*!< in: TRUE if also rec is included in the
				versions to search; otherwise only versions
				prior to it are searched */
	const rec_t*	rec,	/*!< in: record in the clustered index; the
				caller must have a latch on the page */
	mtr_t*		mtr,	/*!< in: mtr holding the latch on rec; it will
				also hold the latch on purge_view */
	dict_index_t*	index,	/*!< in: the secondary index */
	const dtuple_t*	ientry,	/*!< in: the secondary index entry */
	roll_ptr_t	roll_ptr,/*!< in: roll_ptr for the purge record */
	trx_id_t	trx_id);/*!< in: transaction ID on the purging record */

/*****************************************************************//**
Constructs the version of a clustered index record which a consistent
read should see. We assume that the trx id stored in rec is such that
the consistent read should not see rec in its present version.
@return DB_SUCCESS or DB_MISSING_HISTORY */
dberr_t
row_vers_build_for_consistent_read(
/*===============================*/
	const rec_t*	rec,	/*!< in: record in a clustered index; the
				caller must have a latch on the page; this
				latch locks the top of the stack of versions
				of this records */
	mtr_t*		mtr,	/*!< in: mtr holding the latch on rec; it will
				also hold the latch on purge_view */
	dict_index_t*	index,	/*!< in: the clustered index */
	ulint**		offsets,/*!< in/out: offsets returned by
				rec_get_offsets(rec, index) */
	ReadView*	view,	/*!< in: the consistent read view */
	mem_heap_t**	offset_heap,/*!< in/out: memory heap from which
				the offsets are allocated */
	mem_heap_t*	in_heap,/*!< in: memory heap from which the memory for
				*old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	rec_t**		old_vers,/*!< out, own: old version, or NULL
				if the history is missing or the record
				does not exist in the view, that is,
				it was freshly inserted afterwards */
	const dtuple_t**vrow);  /*!< out: reports virtual column info if any */

/*****************************************************************//**
Constructs the last committed version of a clustered index record,
which should be seen by a semi-consistent read. */
void
row_vers_build_for_semi_consistent_read(
/*====================================*/
	const rec_t*	rec,	/*!< in: record in a clustered index; the
				caller must have a latch on the page; this
				latch locks the top of the stack of versions
				of this records */
	mtr_t*		mtr,	/*!< in: mtr holding the latch on rec */
	dict_index_t*	index,	/*!< in: the clustered index */
	ulint**		offsets,/*!< in/out: offsets returned by
				rec_get_offsets(rec, index) */
	mem_heap_t**	offset_heap,/*!< in/out: memory heap from which
				the offsets are allocated */
	mem_heap_t*	in_heap,/*!< in: memory heap from which the memory for
				*old_vers is allocated; memory for possible
				intermediate versions is allocated and freed
				locally within the function */
	const rec_t**	old_vers,/*!< out: rec, old version, or NULL if the
				record does not exist in the view, that is,
				it was freshly inserted afterwards */
	const dtuple_t**vrow);	/*!< out: holds virtual column info if any
				is updated in the view */

#include "row0vers.ic"

#endif
