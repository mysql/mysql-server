/******************************************************
Transaction undo log record

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0rec_h
#define trx0rec_h

#include "univ.i"
#include "trx0types.h"
#include "row0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "dict0types.h"
#include "que0types.h"
#include "data0data.h"
#include "rem0types.h"

/***************************************************************************
Copies the undo record to the heap. */
UNIV_INLINE
trx_undo_rec_t*
trx_undo_rec_copy(
/*==============*/
					/* out, own: copy of undo log record */
	trx_undo_rec_t*	undo_rec,	/* in: undo log record */
	mem_heap_t*	heap);		/* in: heap where copied */
/**************************************************************************
Reads the undo log record type. */
UNIV_INLINE
ulint
trx_undo_rec_get_type(
/*==================*/
					/* out: record type */
	trx_undo_rec_t*	undo_rec);	/* in: undo log record */
/**************************************************************************
Reads from an undo log record the record compiler info. */
UNIV_INLINE
ulint
trx_undo_rec_get_cmpl_info(
/*=======================*/
					/* out: compiler info */
	trx_undo_rec_t*	undo_rec);	/* in: undo log record */
/**************************************************************************
Returns TRUE if an undo log record contains an extern storage field. */
UNIV_INLINE
ibool
trx_undo_rec_get_extern_storage(
/*============================*/
					/* out: TRUE if extern */
	trx_undo_rec_t*	undo_rec);	/* in: undo log record */
/**************************************************************************
Reads the undo log record number. */
UNIV_INLINE
dulint
trx_undo_rec_get_undo_no(
/*=====================*/
					/* out: undo no */
	trx_undo_rec_t*	undo_rec);	/* in: undo log record */
/**************************************************************************
Reads from an undo log record the general parameters. */

byte*
trx_undo_rec_get_pars(
/*==================*/
					/* out: remaining part of undo log
					record after reading these values */
	trx_undo_rec_t*	undo_rec,	/* in: undo log record */
	ulint*		type,		/* out: undo record type:
					TRX_UNDO_INSERT_REC, ... */
	ulint*		cmpl_info,	/* out: compiler info, relevant only
					for update type records */
	ibool*		updated_extern,	/* out: TRUE if we updated an
					externally stored fild */
	dulint*		undo_no,	/* out: undo log record number */
	dulint*		table_id);	/* out: table id */
/***********************************************************************
Builds a row reference from an undo log record. */

byte*
trx_undo_rec_get_row_ref(
/*=====================*/
				/* out: pointer to remaining part of undo
				record */
	byte*		ptr,	/* in: remaining part of a copy of an undo log
				record, at the start of the row reference;
				NOTE that this copy of the undo log record must
				be preserved as long as the row reference is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/* in: clustered index */
	dtuple_t**	ref,	/* out, own: row reference */
	mem_heap_t*	heap);	/* in: memory heap from which the memory
				needed is allocated */
/***********************************************************************
Skips a row reference from an undo log record. */

byte*
trx_undo_rec_skip_row_ref(
/*======================*/
				/* out: pointer to remaining part of undo
				record */
	byte*		ptr,	/* in: remaining part in update undo log
				record, at the start of the row reference */
	dict_index_t*	index);	/* in: clustered index */
/**************************************************************************
Reads from an undo log update record the system field values of the old
version. */

byte*
trx_undo_update_rec_get_sys_cols(
/*=============================*/
				/* out: remaining part of undo log
				record after reading these values */
	byte*	ptr,		/* in: remaining part of undo log
				record after reading general
				parameters */
	dulint*	trx_id,		/* out: trx id */
	dulint*	roll_ptr,	/* out: roll ptr */
	ulint*	info_bits);	/* out: info bits state */
/***********************************************************************
Builds an update vector based on a remaining part of an undo log record. */

byte*
trx_undo_update_rec_get_update(
/*===========================*/
				/* out: remaining part of the record,
				NULL if an error detected, which means that
				the record is corrupted */
	byte*		ptr,	/* in: remaining part in update undo log
				record, after reading the row reference
				NOTE that this copy of the undo log record must
				be preserved as long as the update vector is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/* in: clustered index */
	ulint		type,	/* in: TRX_UNDO_UPD_EXIST_REC,
				TRX_UNDO_UPD_DEL_REC, or
				TRX_UNDO_DEL_MARK_REC; in the last case,
				only trx id and roll ptr fields are added to
				the update vector */
	dulint		trx_id,	/* in: transaction id from this undorecord */
	dulint		roll_ptr,/* in: roll pointer from this undo record */
	ulint		info_bits,/* in: info bits from this undo record */
	trx_t*		trx,	/* in: transaction */
	mem_heap_t*	heap,	/* in: memory heap from which the memory
				needed is allocated */
	upd_t**		upd);	/* out, own: update vector */
/***********************************************************************
Builds a partial row from an update undo log record. It contains the
columns which occur as ordering in any index of the table. */

byte*
trx_undo_rec_get_partial_row(
/*=========================*/
				/* out: pointer to remaining part of undo
				record */
	byte*		ptr,	/* in: remaining part in update undo log
				record of a suitable type, at the start of
				the stored index columns;
				NOTE that this copy of the undo log record must
				be preserved as long as the partial row is
				used, as we do NOT copy the data in the
				record! */
	dict_index_t*	index,	/* in: clustered index */
	dtuple_t**	row,	/* out, own: partial row */
	mem_heap_t*	heap);	/* in: memory heap from which the memory
				needed is allocated */
/***************************************************************************
Writes information to an undo log about an insert, update, or a delete marking
of a clustered index record. This information is used in a rollback of the
transaction and in consistent reads that must look to the history of this
transaction. */

ulint
trx_undo_report_row_operation(
/*==========================*/
					/* out: DB_SUCCESS or error code */
	ulint		flags,		/* in: if BTR_NO_UNDO_LOG_FLAG bit is
					set, does nothing */
	ulint		op_type,	/* in: TRX_UNDO_INSERT_OP or
					TRX_UNDO_MODIFY_OP */
	que_thr_t*	thr,		/* in: query thread */
	dict_index_t*	index,		/* in: clustered index */
	dtuple_t*	clust_entry,	/* in: in the case of an insert,
					index entry to insert into the
					clustered index, otherwise NULL */
	upd_t*		update,		/* in: in the case of an update,
					the update vector, otherwise NULL */
	ulint		cmpl_info,	/* in: compiler info on secondary
					index updates */
	rec_t*		rec,		/* in: case of an update or delete
					marking, the record in the clustered
					index, otherwise NULL */
	dulint*		roll_ptr);	/* out: rollback pointer to the
					inserted undo log record,
					ut_dulint_zero if BTR_NO_UNDO_LOG
					flag was specified */
/**********************************************************************
Copies an undo record to heap. This function can be called if we know that
the undo log record exists. */

trx_undo_rec_t*
trx_undo_get_undo_rec_low(
/*======================*/
					/* out, own: copy of the record */
	dulint		roll_ptr,	/* in: roll pointer to record */
	mem_heap_t*	heap);		/* in: memory heap where copied */
/**********************************************************************
Copies an undo record to heap. */

ulint
trx_undo_get_undo_rec(
/*==================*/
					/* out: DB_SUCCESS, or
					DB_MISSING_HISTORY if the undo log
					has been truncated and we cannot
					fetch the old version; NOTE: the
					caller must have latches on the
					clustered index page and purge_view */
	dulint		roll_ptr,	/* in: roll pointer to record */
	dulint		trx_id,		/* in: id of the trx that generated
					the roll pointer: it points to an
					undo log of this transaction */
	trx_undo_rec_t** undo_rec,	/* out, own: copy of the record */
	mem_heap_t*	heap);		/* in: memory heap where copied */
/***********************************************************************
Build a previous version of a clustered index record. This function checks
that the caller has a latch on the index page of the clustered index record
and an s-latch on the purge_view. This guarantees that the stack of versions
is locked. */

ulint
trx_undo_prev_version_build(
/*========================*/
				/* out: DB_SUCCESS, or DB_MISSING_HISTORY if
				the previous version is not >= purge_view,
				which means that it may have been removed,
				DB_ERROR if corrupted record */
	rec_t*		index_rec,/* in: clustered index record in the
				index tree */
	mtr_t*		index_mtr,/* in: mtr which contains the latch to
				index_rec page and purge_view */
	rec_t*		rec,	/* in: version of a clustered index record */
	dict_index_t*	index,	/* in: clustered index */
	mem_heap_t*	heap,	/* in: memory heap from which the memory
				needed is allocated */
	rec_t**		old_vers);/* out, own: previous version, or NULL if
				rec is the first inserted version, or if
				history data has been deleted */
/***************************************************************
Parses a redo log record of adding an undo log record. */

byte*
trx_undo_parse_add_undo_rec(
/*========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */
/***************************************************************
Parses a redo log record of erasing of an undo page end. */

byte*
trx_undo_parse_erase_page_end(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */

/* Types of an undo log record: these have to be smaller than 16, as the
compilation info multiplied by 16 is ORed to this value in an undo log
record */
#define TRX_UNDO_INSERT_REC	11	/* fresh insert into clustered index */
#define TRX_UNDO_UPD_EXIST_REC	12	/* update of a non-delete-marked
					record */
#define	TRX_UNDO_UPD_DEL_REC	13	/* update of a delete marked record to
					a not delete marked record; also the
					fields of the record can change */
#define TRX_UNDO_DEL_MARK_REC	14	/* delete marking of a record; fields
					do not change */
#define	TRX_UNDO_CMPL_INFO_MULT	16	/* compilation info is multiplied by
					this and ORed to the type above */
#define TRX_UNDO_UPD_EXTERN	128	/* This bit can be ORed to type_cmpl
					to denote that we updated external
					storage fields: used by purge to
					free the external storage */

/* Operation type flags used in trx_undo_report_row_operation */
#define TRX_UNDO_INSERT_OP	1
#define TRX_UNDO_MODIFY_OP	2

#ifndef UNIV_NONINL
#include "trx0rec.ic"
#endif

#endif 
