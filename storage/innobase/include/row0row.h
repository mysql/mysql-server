/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/row0row.h
General row routines

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#ifndef row0row_h
#define row0row_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "mtr0mtr.h"
#include "rem0types.h"
#include "read0types.h"
#include "row0types.h"
#include "btr0types.h"

/*********************************************************************//**
Gets the offset of the DB_TRX_ID field, in bytes relative to the origin of
a clustered index record.
@return	offset of DATA_TRX_ID */
UNIV_INLINE
ulint
row_get_trx_id_offset(
/*==================*/
	const dict_index_t*	index,	/*!< in: clustered index */
	const ulint*		offsets)/*!< in: record offsets */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Reads the trx id field from a clustered index record.
@return	value of the field */
UNIV_INLINE
trx_id_t
row_get_rec_trx_id(
/*===============*/
	const rec_t*		rec,	/*!< in: record */
	const dict_index_t*	index,	/*!< in: clustered index */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Reads the roll pointer field from a clustered index record.
@return	value of the field */
UNIV_INLINE
roll_ptr_t
row_get_rec_roll_ptr(
/*=================*/
	const rec_t*		rec,	/*!< in: record */
	const dict_index_t*	index,	/*!< in: clustered index */
	const ulint*		offsets)/*!< in: rec_get_offsets(rec, index) */
	__attribute__((nonnull, warn_unused_result));
/*****************************************************************//**
When an insert or purge to a table is performed, this function builds
the entry to be inserted into or purged from an index on the table.
@return index entry which should be inserted or purged, or NULL if the
externally stored columns in the clustered index record are
unavailable and ext != NULL */
UNIV_INTERN
dtuple_t*
row_build_index_entry(
/*==================*/
	const dtuple_t*	row,	/*!< in: row which should be
				inserted or purged */
	row_ext_t*	ext,	/*!< in: externally stored column prefixes,
				or NULL */
	dict_index_t*	index,	/*!< in: index on the table */
	mem_heap_t*	heap);	/*!< in: memory heap from which the memory for
				the index entry is allocated */
/*******************************************************************//**
An inverse function to row_build_index_entry. Builds a row from a
record in a clustered index.
@return	own: row built; see the NOTE below! */
UNIV_INTERN
dtuple_t*
row_build(
/*======*/
	ulint			type,	/*!< in: ROW_COPY_POINTERS or
					ROW_COPY_DATA; the latter
					copies also the data fields to
					heap while the first only
					places pointers to data fields
					on the index page, and thus is
					more efficient */
	const dict_index_t*	index,	/*!< in: clustered index */
	const rec_t*		rec,	/*!< in: record in the clustered
					index; NOTE: in the case
					ROW_COPY_POINTERS the data
					fields in the row will point
					directly into this record,
					therefore, the buffer page of
					this record must be at least
					s-latched and the latch held
					as long as the row dtuple is used! */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec,index)
					or NULL, in which case this function
					will invoke rec_get_offsets() */
	const dict_table_t*	col_table,
					/*!< in: table, to check which
					externally stored columns
					occur in the ordering columns
					of an index, or NULL if
					index->table should be
					consulted instead; the user
					columns in this table should be
					the same columns as in index->table */
	row_ext_t**		ext,	/*!< out, own: cache of
					externally stored column
					prefixes, or NULL */
	mem_heap_t*		heap);	/*!< in: memory heap from which
					the memory needed is allocated */
/*******************************************************************//**
Converts an index record to a typed data tuple.
@return index entry built; does not set info_bits, and the data fields
in the entry will point directly to rec */
UNIV_INTERN
dtuple_t*
row_rec_to_index_entry_low(
/*=======================*/
	const rec_t*		rec,	/*!< in: record in the index */
	const dict_index_t*	index,	/*!< in: index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	ulint*			n_ext,	/*!< out: number of externally
					stored columns */
	mem_heap_t*		heap);	/*!< in: memory heap from which
					the memory needed is allocated */
/*******************************************************************//**
Converts an index record to a typed data tuple. NOTE that externally
stored (often big) fields are NOT copied to heap.
@return	own: index entry built; see the NOTE below! */
UNIV_INTERN
dtuple_t*
row_rec_to_index_entry(
/*===================*/
	ulint			type,	/*!< in: ROW_COPY_DATA, or
					ROW_COPY_POINTERS: the former
					copies also the data fields to
					heap as the latter only places
					pointers to data fields on the
					index page */
	const rec_t*		rec,	/*!< in: record in the index;
					NOTE: in the case
					ROW_COPY_POINTERS the data
					fields in the row will point
					directly into this record,
					therefore, the buffer page of
					this record must be at least
					s-latched and the latch held
					as long as the dtuple is used! */
	const dict_index_t*	index,	/*!< in: index */
	ulint*			offsets,/*!< in/out: rec_get_offsets(rec) */
	ulint*			n_ext,	/*!< out: number of externally
					stored columns */
	mem_heap_t*		heap);	/*!< in: memory heap from which
					the memory needed is allocated */
/*******************************************************************//**
Builds from a secondary index record a row reference with which we can
search the clustered index record.
@return	own: row reference built; see the NOTE below! */
UNIV_INTERN
dtuple_t*
row_build_row_ref(
/*==============*/
	ulint		type,	/*!< in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
				the former copies also the data fields to
				heap, whereas the latter only places pointers
				to data fields on the index page */
	dict_index_t*	index,	/*!< in: secondary index */
	const rec_t*	rec,	/*!< in: record in the index;
				NOTE: in the case ROW_COPY_POINTERS
				the data fields in the row will point
				directly into this record, therefore,
				the buffer page of this record must be
				at least s-latched and the latch held
				as long as the row reference is used! */
	mem_heap_t*	heap);	/*!< in: memory heap from which the memory
				needed is allocated */
/*******************************************************************//**
Builds from a secondary index record a row reference with which we can
search the clustered index record. */
UNIV_INTERN
void
row_build_row_ref_in_tuple(
/*=======================*/
	dtuple_t*		ref,	/*!< in/out: row reference built;
					see the NOTE below! */
	const rec_t*		rec,	/*!< in: record in the index;
					NOTE: the data fields in ref
					will point directly into this
					record, therefore, the buffer
					page of this record must be at
					least s-latched and the latch
					held as long as the row
					reference is used! */
	const dict_index_t*	index,	/*!< in: secondary index */
	ulint*			offsets,/*!< in: rec_get_offsets(rec, index)
					or NULL */
	trx_t*			trx);	/*!< in: transaction */
/*******************************************************************//**
Builds from a secondary index record a row reference with which we can
search the clustered index record. */
UNIV_INLINE
void
row_build_row_ref_fast(
/*===================*/
	dtuple_t*	ref,	/*!< in/out: typed data tuple where the
				reference is built */
	const ulint*	map,	/*!< in: array of field numbers in rec
				telling how ref should be built from
				the fields of rec */
	const rec_t*	rec,	/*!< in: record in the index; must be
				preserved while ref is used, as we do
				not copy field values to heap */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/***************************************************************//**
Searches the clustered index record for a row, if we have the row
reference.
@return	TRUE if found */
UNIV_INTERN
ibool
row_search_on_row_ref(
/*==================*/
	btr_pcur_t*		pcur,	/*!< out: persistent cursor, which must
					be closed by the caller */
	ulint			mode,	/*!< in: BTR_MODIFY_LEAF, ... */
	const dict_table_t*	table,	/*!< in: table */
	const dtuple_t*		ref,	/*!< in: row reference */
	mtr_t*			mtr);	/*!< in/out: mtr */
/*********************************************************************//**
Fetches the clustered index record for a secondary index record. The latches
on the secondary index record are preserved.
@return	record or NULL, if no record found */
UNIV_INTERN
rec_t*
row_get_clust_rec(
/*==============*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF, ... */
	const rec_t*	rec,	/*!< in: record in a secondary index */
	dict_index_t*	index,	/*!< in: secondary index */
	dict_index_t**	clust_index,/*!< out: clustered index */
	mtr_t*		mtr);	/*!< in: mtr */

/** Result of row_search_index_entry */
enum row_search_result {
	ROW_FOUND = 0,		/*!< the record was found */
	ROW_NOT_FOUND,		/*!< record not found */
	ROW_BUFFERED,		/*!< one of BTR_INSERT, BTR_DELETE, or
				BTR_DELETE_MARK was specified, the
				secondary index leaf page was not in
				the buffer pool, and the operation was
				enqueued in the insert/delete buffer */
	ROW_NOT_DELETED_REF	/*!< BTR_DELETE was specified, and
				row_purge_poss_sec() failed */
};

/***************************************************************//**
Searches an index record.
@return	whether the record was found or buffered */
UNIV_INTERN
enum row_search_result
row_search_index_entry(
/*===================*/
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	entry,	/*!< in: index entry */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF, ... */
	btr_pcur_t*	pcur,	/*!< in/out: persistent cursor, which must
				be closed by the caller */
	mtr_t*		mtr);	/*!< in: mtr */


#define ROW_COPY_DATA		1
#define ROW_COPY_POINTERS	2

/* The allowed latching order of index records is the following:
(1) a secondary index record ->
(2) the clustered index record ->
(3) rollback segment data for the clustered index record.

No new latches may be obtained while the kernel mutex is reserved.
However, the kernel mutex can be reserved while latches are owned. */

/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) using
"dict_field" and writes the result to "buf".
Not more than "buf_size" bytes are written to "buf".
The result is always NUL-terminated (provided buf_size is positive) and the
number of bytes that were written to "buf" is returned (including the
terminating NUL).
@return	number of bytes that were written */
UNIV_INTERN
ulint
row_raw_format(
/*===========*/
	const char*		data,		/*!< in: raw data */
	ulint			data_len,	/*!< in: raw data length
						in bytes */
	const dict_field_t*	dict_field,	/*!< in: index field */
	char*			buf,		/*!< out: output buffer */
	ulint			buf_size);	/*!< in: output buffer size
						in bytes */

#ifndef UNIV_NONINL
#include "row0row.ic"
#endif

#endif
