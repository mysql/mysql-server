/*****************************************************************************

Copyright (c) 2011, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0log.h
Modification log for online index creation and online table rebuild

Created 2011-05-26 Marko Makela
*******************************************************/

#ifndef row0log_h
#define row0log_h

#include "univ.i"
#include "mtr0types.h"
#include "row0types.h"
#include "rem0types.h"
#include "data0types.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"

/******************************************************//**
Allocate the row log for an index and flag the index
for online creation.
@retval true if success, false if not */
UNIV_INTERN
bool
row_log_allocate(
/*=============*/
	dict_index_t*	index,	/*!< in/out: index */
	dict_table_t*	table,	/*!< in/out: new table being rebuilt,
				or NULL when creating a secondary index */
	bool		same_pk,/*!< in: whether the definition of the
				PRIMARY KEY has remained the same */
	const dtuple_t*	add_cols,
				/*!< in: default values of
				added columns, or NULL */
	const ulint*	col_map)/*!< in: mapping of old column
				numbers to new ones, or NULL if !table */
	__attribute__((nonnull(1), warn_unused_result));

/******************************************************//**
Free the row log for an index that was being created online. */
UNIV_INTERN
void
row_log_free(
/*=========*/
	row_log_t*&	log)	/*!< in,own: row log */
	__attribute__((nonnull));

/******************************************************//**
Free the row log for an index on which online creation was aborted. */
UNIV_INLINE
void
row_log_abort_sec(
/*==============*/
	dict_index_t*	index)	/*!< in/out: index (x-latched) */
	__attribute__((nonnull));

/******************************************************//**
Try to log an operation to a secondary index that is
(or was) being created.
@retval	true if the operation was logged or can be ignored
@retval	false if online index creation is not taking place */
UNIV_INLINE
bool
row_log_online_op_try(
/*==================*/
	dict_index_t*	index,	/*!< in/out: index, S or X latched */
	const dtuple_t* tuple,	/*!< in: index tuple */
	trx_id_t	trx_id)	/*!< in: transaction ID for insert,
				or 0 for delete */
	__attribute__((nonnull, warn_unused_result));
/******************************************************//**
Logs an operation to a secondary index that is (or was) being created. */
UNIV_INTERN
void
row_log_online_op(
/*==============*/
	dict_index_t*	index,	/*!< in/out: index, S or X latched */
	const dtuple_t*	tuple,	/*!< in: index tuple */
	trx_id_t	trx_id)	/*!< in: transaction ID for insert,
				or 0 for delete */
	UNIV_COLD __attribute__((nonnull));

/******************************************************//**
Gets the error status of the online index rebuild log.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_log_table_get_error(
/*====================*/
	const dict_index_t*	index)	/*!< in: clustered index of a table
					that is being rebuilt online */
	__attribute__((nonnull, warn_unused_result));

/******************************************************//**
Logs a delete operation to a table that is being rebuilt.
This will be merged in row_log_table_apply_delete(). */
UNIV_INTERN
void
row_log_table_delete(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index) */
	bool		purge,	/*!< in: true=purging BLOBs */
	trx_id_t	trx_id)	/*!< in: DB_TRX_ID of the record before
				it was deleted */
	UNIV_COLD __attribute__((nonnull));

/******************************************************//**
Logs an update operation to a table that is being rebuilt.
This will be merged in row_log_table_apply_update(). */
UNIV_INTERN
void
row_log_table_update(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index) */
	const dtuple_t*	old_pk)	/*!< in: row_log_table_get_pk()
				before the update */
	UNIV_COLD __attribute__((nonnull(1,2,3)));

/******************************************************//**
Constructs the old PRIMARY KEY and DB_TRX_ID,DB_ROLL_PTR
of a table that is being rebuilt.
@return tuple of PRIMARY KEY,DB_TRX_ID,DB_ROLL_PTR in the rebuilt table,
or NULL if the PRIMARY KEY definition does not change */
UNIV_INTERN
const dtuple_t*
row_log_table_get_pk(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index),
				or NULL */
	mem_heap_t**	heap)	/*!< in/out: memory heap where allocated */
	UNIV_COLD __attribute__((nonnull(1,2,4), warn_unused_result));

/******************************************************//**
Logs an insert to a table that is being rebuilt.
This will be merged in row_log_table_apply_insert(). */
UNIV_INTERN
void
row_log_table_insert(
/*=================*/
	const rec_t*	rec,	/*!< in: clustered index leaf page record,
				page X-latched */
	dict_index_t*	index,	/*!< in/out: clustered index, S-latched
				or X-latched */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec,index) */
	UNIV_COLD __attribute__((nonnull));
/******************************************************//**
Notes that a BLOB is being freed during online ALTER TABLE. */
UNIV_INTERN
void
row_log_table_blob_free(
/*====================*/
	dict_index_t*	index,	/*!< in/out: clustered index, X-latched */
	ulint		page_no)/*!< in: starting page number of the BLOB */
	UNIV_COLD __attribute__((nonnull));
/******************************************************//**
Notes that a BLOB is being allocated during online ALTER TABLE. */
UNIV_INTERN
void
row_log_table_blob_alloc(
/*=====================*/
	dict_index_t*	index,	/*!< in/out: clustered index, X-latched */
	ulint		page_no)/*!< in: starting page number of the BLOB */
	UNIV_COLD __attribute__((nonnull));
/******************************************************//**
Apply the row_log_table log to a table upon completing rebuild.
@return DB_SUCCESS, or error code on failure */
UNIV_INTERN
dberr_t
row_log_table_apply(
/*================*/
	que_thr_t*	thr,	/*!< in: query graph */
	dict_table_t*	old_table,
				/*!< in: old table */
	struct TABLE*	table)	/*!< in/out: MySQL table
				(for reporting duplicates) */
	__attribute__((nonnull, warn_unused_result));

/******************************************************//**
Get the latest transaction ID that has invoked row_log_online_op()
during online creation.
@return latest transaction ID, or 0 if nothing was logged */
UNIV_INTERN
trx_id_t
row_log_get_max_trx(
/*================*/
	dict_index_t*	index)	/*!< in: index, must be locked */
	__attribute__((nonnull, warn_unused_result));

/******************************************************//**
Merge the row log to the index upon completing index creation.
@return DB_SUCCESS, or error code on failure */
UNIV_INTERN
dberr_t
row_log_apply(
/*==========*/
	trx_t*		trx,	/*!< in: transaction (for checking if
				the operation was interrupted) */
	dict_index_t*	index,	/*!< in/out: secondary index */
	struct TABLE*	table)	/*!< in/out: MySQL table
				(for reporting duplicates) */
	__attribute__((nonnull, warn_unused_result));

#ifndef UNIV_NONINL
#include "row0log.ic"
#endif

#endif /* row0log.h */
