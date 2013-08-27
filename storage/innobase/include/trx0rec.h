/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0rec.h
Transaction undo log record

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rec_h
#define trx0rec_h

#include "univ.i"
#include "trx0types.h"
#include "row0types.h"
#include "mtr0mtr.h"
#include "dict0types.h"
#include "data0data.h"
#include "rem0types.h"

#ifndef UNIV_HOTBACKUP
# include "que0types.h"

/***********************************************************************//**
Copies the undo record to the heap.
@return	own: copy of undo log record */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_rec_copy(
/*==============*/
	const trx_undo_rec_t*	undo_rec,	/*!< in: undo log record */
	mem_heap_t*		heap);		/*!< in: heap where copied */
/**********************************************************************//**
Reads the undo log record type.
@return	record type */
UNIV_INLINE
ulint
trx_undo_rec_get_type(
/*==================*/
	const trx_undo_rec_t*	undo_rec);	/*!< in: undo log record */
/**********************************************************************//**
Reads from an undo log record the record compiler info.
@return	compiler info */
UNIV_INLINE
ulint
trx_undo_rec_get_cmpl_info(
/*=======================*/
	const trx_undo_rec_t*	undo_rec);	/*!< in: undo log record */
/**********************************************************************//**
Returns TRUE if an undo log record contains an extern storage field.
@return	TRUE if extern */
UNIV_INLINE
ibool
trx_undo_rec_get_extern_storage(
/*============================*/
	const trx_undo_rec_t*	undo_rec);	/*!< in: undo log record */
/**********************************************************************//**
Reads the undo log record number.
@return	undo no */
UNIV_INLINE
undo_no_t
trx_undo_rec_get_undo_no(
/*=====================*/
	const trx_undo_rec_t*	undo_rec);	/*!< in: undo log record */
/**********************************************************************//**
Returns the start of the undo record data area.
@return	offset to the data area */
UNIV_INLINE
ulint
trx_undo_rec_get_offset(
/*====================*/
	undo_no_t	undo_no)	/*!< in: undo no read from node */
	__attribute__((const));

/**********************************************************************//**
Returns the start of the undo record data area. */
#define trx_undo_rec_get_ptr(undo_rec, undo_no)		\
	((undo_rec) + trx_undo_rec_get_offset(undo_no))

/**********************************************************************//**
Reads from an undo log record the general parameters.
@return	remaining part of undo log record after reading these values */
UNIV_INTERN
byte*
trx_undo_rec_get_pars(
/*==================*/
	trx_undo_rec_t*	undo_rec,	/*!< in: undo log record */
	ulint*		type,		/*!< out: undo record type:
					TRX_UNDO_INSERT_REC, ... */
	ulint*		cmpl_info,	/*!< out: compiler info, relevant only
					for update type records */
	bool*		updated_extern,	/*!< out: true if we updated an
					externally stored fild */
	undo_no_t*	undo_no,	/*!< out: undo log record number */
	table_id_t*	table_id)	/*!< out: table id */
	__attribute__((nonnull));
/*******************************************************************//**
Builds a row reference from an undo log record.
@return	pointer to remaining part of undo record */
UNIV_INTERN
byte*
trx_undo_rec_get_row_ref(
/*=====================*/
	byte*		ptr,	/*!< in: remaining part of a copy of an undo log
				record, at the start of the row reference;
				NOTE that this copy of the undo log record must
				be preserved as long as the row reference is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/*!< in: clustered index */
	dtuple_t**	ref,	/*!< out, own: row reference */
	mem_heap_t*	heap);	/*!< in: memory heap from which the memory
				needed is allocated */
/*******************************************************************//**
Skips a row reference from an undo log record.
@return	pointer to remaining part of undo record */
UNIV_INTERN
byte*
trx_undo_rec_skip_row_ref(
/*======================*/
	byte*		ptr,	/*!< in: remaining part in update undo log
				record, at the start of the row reference */
	dict_index_t*	index);	/*!< in: clustered index */
/**********************************************************************//**
Reads from an undo log update record the system field values of the old
version.
@return	remaining part of undo log record after reading these values */
UNIV_INTERN
byte*
trx_undo_update_rec_get_sys_cols(
/*=============================*/
	byte*		ptr,		/*!< in: remaining part of undo
					log record after reading
					general parameters */
	trx_id_t*	trx_id,		/*!< out: trx id */
	roll_ptr_t*	roll_ptr,	/*!< out: roll ptr */
	ulint*		info_bits);	/*!< out: info bits state */
/*******************************************************************//**
Builds an update vector based on a remaining part of an undo log record.
@return remaining part of the record, NULL if an error detected, which
means that the record is corrupted */
UNIV_INTERN
byte*
trx_undo_update_rec_get_update(
/*===========================*/
	byte*		ptr,	/*!< in: remaining part in update undo log
				record, after reading the row reference
				NOTE that this copy of the undo log record must
				be preserved as long as the update vector is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint		type,	/*!< in: TRX_UNDO_UPD_EXIST_REC,
				TRX_UNDO_UPD_DEL_REC, or
				TRX_UNDO_DEL_MARK_REC; in the last case,
				only trx id and roll ptr fields are added to
				the update vector */
	trx_id_t	trx_id,	/*!< in: transaction id from this undorecord */
	roll_ptr_t	roll_ptr,/*!< in: roll pointer from this undo record */
	ulint		info_bits,/*!< in: info bits from this undo record */
	trx_t*		trx,	/*!< in: transaction */
	mem_heap_t*	heap,	/*!< in: memory heap from which the memory
				needed is allocated */
	upd_t**		upd);	/*!< out, own: update vector */
/*******************************************************************//**
Builds a partial row from an update undo log record, for purge.
It contains the columns which occur as ordering in any index of the table.
Any missing columns are indicated by col->mtype == DATA_MISSING.
@return	pointer to remaining part of undo record */
UNIV_INTERN
byte*
trx_undo_rec_get_partial_row(
/*=========================*/
	byte*		ptr,	/*!< in: remaining part in update undo log
				record of a suitable type, at the start of
				the stored index columns;
				NOTE that this copy of the undo log record must
				be preserved as long as the partial row is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/*!< in: clustered index */
	dtuple_t**	row,	/*!< out, own: partial row */
	ibool		ignore_prefix, /*!< in: flag to indicate if we
				expect blob prefixes in undo. Used
				only in the assertion. */
	mem_heap_t*	heap)	/*!< in: memory heap from which the memory
				needed is allocated */
	__attribute__((nonnull, warn_unused_result));
/***********************************************************************//**
Writes information to an undo log about an insert, update, or a delete marking
of a clustered index record. This information is used in a rollback of the
transaction and in consistent reads that must look to the history of this
transaction.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
trx_undo_report_row_operation(
/*==========================*/
	ulint		flags,		/*!< in: if BTR_NO_UNDO_LOG_FLAG bit is
					set, does nothing */
	ulint		op_type,	/*!< in: TRX_UNDO_INSERT_OP or
					TRX_UNDO_MODIFY_OP */
	que_thr_t*	thr,		/*!< in: query thread */
	dict_index_t*	index,		/*!< in: clustered index */
	const dtuple_t*	clust_entry,	/*!< in: in the case of an insert,
					index entry to insert into the
					clustered index, otherwise NULL */
	const upd_t*	update,		/*!< in: in the case of an update,
					the update vector, otherwise NULL */
	ulint		cmpl_info,	/*!< in: compiler info on secondary
					index updates */
	const rec_t*	rec,		/*!< in: case of an update or delete
					marking, the record in the clustered
					index, otherwise NULL */
	const ulint*	offsets,	/*!< in: rec_get_offsets(rec) */
	roll_ptr_t*	roll_ptr)	/*!< out: rollback pointer to the
					inserted undo log record,
					0 if BTR_NO_UNDO_LOG
					flag was specified */
	__attribute__((nonnull(3,4,10), warn_unused_result));
/******************************************************************//**
Copies an undo record to heap. This function can be called if we know that
the undo log record exists.
@return	own: copy of the record */
UNIV_INTERN
trx_undo_rec_t*
trx_undo_get_undo_rec_low(
/*======================*/
	roll_ptr_t	roll_ptr,	/*!< in: roll pointer to record */
	mem_heap_t*	heap)		/*!< in: memory heap where copied */
	__attribute__((nonnull, warn_unused_result));
/*******************************************************************//**
Build a previous version of a clustered index record. The caller must
hold a latch on the index page of the clustered index record.
@retval true if previous version was built, or if it was an insert
or the table has been rebuilt
@retval false if the previous version is earlier than purge_view,
which means that it may have been removed */
UNIV_INTERN
bool
trx_undo_prev_version_build(
/*========================*/
	const rec_t*	index_rec,/*!< in: clustered index record in the
				index tree */
	mtr_t*		index_mtr,/*!< in: mtr which contains the latch to
				index_rec page and purge_view */
	const rec_t*	rec,	/*!< in: version of a clustered index record */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint*		offsets,/*!< in/out: rec_get_offsets(rec, index) */
	mem_heap_t*	heap,	/*!< in: memory heap from which the memory
				needed is allocated */
	rec_t**		old_vers)/*!< out, own: previous version, or NULL if
				rec is the first inserted version, or if
				history data has been deleted */
	__attribute__((nonnull));
#endif /* !UNIV_HOTBACKUP */
/***********************************************************//**
Parses a redo log record of adding an undo log record.
@return	end of log record or NULL */
UNIV_INTERN
byte*
trx_undo_parse_add_undo_rec(
/*========================*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	page_t*	page);	/*!< in: page or NULL */
/***********************************************************//**
Parses a redo log record of erasing of an undo page end.
@return	end of log record or NULL */
UNIV_INTERN
byte*
trx_undo_parse_erase_page_end(
/*==========================*/
	byte*	ptr,	/*!< in: buffer */
	byte*	end_ptr,/*!< in: buffer end */
	page_t*	page,	/*!< in: page or NULL */
	mtr_t*	mtr);	/*!< in: mtr or NULL */

#ifndef UNIV_HOTBACKUP

/* Types of an undo log record: these have to be smaller than 16, as the
compilation info multiplied by 16 is ORed to this value in an undo log
record */

#define	TRX_UNDO_INSERT_REC	11	/* fresh insert into clustered index */
#define	TRX_UNDO_UPD_EXIST_REC	12	/* update of a non-delete-marked
					record */
#define	TRX_UNDO_UPD_DEL_REC	13	/* update of a delete marked record to
					a not delete marked record; also the
					fields of the record can change */
#define	TRX_UNDO_DEL_MARK_REC	14	/* delete marking of a record; fields
					do not change */
#define	TRX_UNDO_CMPL_INFO_MULT	16	/* compilation info is multiplied by
					this and ORed to the type above */
#define	TRX_UNDO_UPD_EXTERN	128	/* This bit can be ORed to type_cmpl
					to denote that we updated external
					storage fields: used by purge to
					free the external storage */

/* Operation type flags used in trx_undo_report_row_operation */
#define	TRX_UNDO_INSERT_OP		1
#define	TRX_UNDO_MODIFY_OP		2

#ifndef UNIV_NONINL
#include "trx0rec.ic"
#endif

#endif /* !UNIV_HOTBACKUP */

#endif /* trx0rec_h */
