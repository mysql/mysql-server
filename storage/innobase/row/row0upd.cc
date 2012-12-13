/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0upd.cc
Update of a row

Created 12/27/1996 Heikki Tuuri
*******************************************************/

#include "row0upd.h"

#ifdef UNIV_NONINL
#include "row0upd.ic"
#endif

#include "ha_prototypes.h"
#include "dict0dict.h"
#include "trx0undo.h"
#include "rem0rec.h"
#ifndef UNIV_HOTBACKUP
#include "dict0boot.h"
#include "dict0crea.h"
#include "mach0data.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "que0que.h"
#include "row0ext.h"
#include "row0ins.h"
#include "row0log.h"
#include "row0row.h"
#include "row0sel.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "log0log.h"
#include "pars0sym.h"
#include "eval0eval.h"
#include "buf0lru.h"


/* What kind of latch and lock can we assume when the control comes to
   -------------------------------------------------------------------
an update node?
--------------
Efficiency of massive updates would require keeping an x-latch on a
clustered index page through many updates, and not setting an explicit
x-lock on clustered index records, as they anyway will get an implicit
x-lock when they are updated. A problem is that the read nodes in the
graph should know that they must keep the latch when passing the control
up to the update node, and not set any record lock on the record which
will be updated. Another problem occurs if the execution is stopped,
as the kernel switches to another query thread, or the transaction must
wait for a lock. Then we should be able to release the latch and, maybe,
acquire an explicit x-lock on the record.
	Because this seems too complicated, we conclude that the less
efficient solution of releasing all the latches when the control is
transferred to another node, and acquiring explicit x-locks, is better. */

/* How is a delete performed? If there is a delete without an
explicit cursor, i.e., a searched delete, there are at least
two different situations:
the implicit select cursor may run on (1) the clustered index or
on (2) a secondary index. The delete is performed by setting
the delete bit in the record and substituting the id of the
deleting transaction for the original trx id, and substituting a
new roll ptr for previous roll ptr. The old trx id and roll ptr
are saved in the undo log record. Thus, no physical changes occur
in the index tree structure at the time of the delete. Only
when the undo log is purged, the index records will be physically
deleted from the index trees.

The query graph executing a searched delete would consist of
a delete node which has as a subtree a select subgraph.
The select subgraph should return a (persistent) cursor
in the clustered index, placed on page which is x-latched.
The delete node should look for all secondary index records for
this clustered index entry and mark them as deleted. When is
the x-latch freed? The most efficient way for performing a
searched delete is obviously to keep the x-latch for several
steps of query graph execution. */

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchonization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

/***********************************************************//**
Checks if an update vector changes some of the first ordering fields of an
index record. This is only used in foreign key checks and we can assume
that index does not contain column prefixes.
@return	TRUE if changes */
static
ibool
row_upd_changes_first_fields_binary(
/*================================*/
	dtuple_t*	entry,	/*!< in: old value of index entry */
	dict_index_t*	index,	/*!< in: index of entry */
	const upd_t*	update,	/*!< in: update vector for the row */
	ulint		n);	/*!< in: how many first fields to check */


/*********************************************************************//**
Checks if index currently is mentioned as a referenced index in a foreign
key constraint.

NOTE that since we do not hold dict_operation_lock when leaving the
function, it may be that the referencing table has been dropped when
we leave this function: this function is only for heuristic use!

@return TRUE if referenced */
static
ibool
row_upd_index_is_referenced(
/*========================*/
	dict_index_t*	index,	/*!< in: index */
	trx_t*		trx)	/*!< in: transaction */
{
	dict_table_t*	table		= index->table;
	dict_foreign_t*	foreign;
	ibool		froze_data_dict	= FALSE;
	ibool		is_referenced	= FALSE;

	if (!UT_LIST_GET_FIRST(table->referenced_list)) {

		return(FALSE);
	}

	if (trx->dict_operation_lock_mode == 0) {
		row_mysql_freeze_data_dictionary(trx);
		froze_data_dict = TRUE;
	}

	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign) {
		if (foreign->referenced_index == index) {

			is_referenced = TRUE;
			goto func_exit;
		}

		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}

func_exit:
	if (froze_data_dict) {
		row_mysql_unfreeze_data_dictionary(trx);
	}

	return(is_referenced);
}

/*********************************************************************//**
Checks if possible foreign key constraints hold after a delete of the record
under pcur.

NOTE that this function will temporarily commit mtr and lose the
pcur position!

@return	DB_SUCCESS or an error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_check_references_constraints(
/*=================================*/
	upd_node_t*	node,	/*!< in: row update node */
	btr_pcur_t*	pcur,	/*!< in: cursor positioned on a record; NOTE: the
				cursor position is lost in this function! */
	dict_table_t*	table,	/*!< in: table in question */
	dict_index_t*	index,	/*!< in: index of the cursor */
	ulint*		offsets,/*!< in/out: rec_get_offsets(pcur.rec, index) */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dict_foreign_t*	foreign;
	mem_heap_t*	heap;
	dtuple_t*	entry;
	trx_t*		trx;
	const rec_t*	rec;
	ulint		n_ext;
	dberr_t		err;
	ibool		got_s_lock	= FALSE;

	if (UT_LIST_GET_FIRST(table->referenced_list) == NULL) {

		return(DB_SUCCESS);
	}

	trx = thr_get_trx(thr);

	rec = btr_pcur_get_rec(pcur);
	ut_ad(rec_offs_validate(rec, index, offsets));

	heap = mem_heap_create(500);

	entry = row_rec_to_index_entry(rec, index, offsets, &n_ext, heap);

	mtr_commit(mtr);

	DEBUG_SYNC_C("foreign_constraint_check_for_update");

	mtr_start(mtr);

	if (trx->dict_operation_lock_mode == 0) {
		got_s_lock = TRUE;

		row_mysql_freeze_data_dictionary(trx);
	}

run_again:
	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign) {
		/* Note that we may have an update which updates the index
		record, but does NOT update the first fields which are
		referenced in a foreign key constraint. Then the update does
		NOT break the constraint. */

		if (foreign->referenced_index == index
		    && (node->is_delete
			|| row_upd_changes_first_fields_binary(
				entry, index, node->update,
				foreign->n_fields))) {
			dict_table_t*	foreign_table = foreign->foreign_table;

			dict_table_t*	ref_table = NULL;

			if (foreign_table == NULL) {

				ref_table = dict_table_open_on_name(
					foreign->foreign_table_name_lookup,
					FALSE, FALSE, DICT_ERR_IGNORE_NONE);
			}

			if (foreign_table) {
				os_inc_counter(dict_sys->mutex,
					       foreign_table
					       ->n_foreign_key_checks_running);
			}

			/* NOTE that if the thread ends up waiting for a lock
			we will release dict_operation_lock temporarily!
			But the counter on the table protects 'foreign' from
			being dropped while the check is running. */

			err = row_ins_check_foreign_constraint(
				FALSE, foreign, table, entry, thr);

			if (foreign_table) {
				os_dec_counter(dict_sys->mutex,
					       foreign_table
					       ->n_foreign_key_checks_running);
			}

			if (ref_table != NULL) {
				dict_table_close(ref_table, FALSE, FALSE);
			}

			/* Some table foreign key dropped, try again */
			if (err == DB_DICT_CHANGED) {
				goto run_again;
			} else if (err != DB_SUCCESS) {
				goto func_exit;
			}
		}

		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}

	err = DB_SUCCESS;

func_exit:
	if (got_s_lock) {
		row_mysql_unfreeze_data_dictionary(trx);
	}

	mem_heap_free(heap);

	DEBUG_SYNC_C("foreign_constraint_check_for_update_done");

	return(err);
}

/*********************************************************************//**
Creates an update node for a query graph.
@return	own: update node */
UNIV_INTERN
upd_node_t*
upd_node_create(
/*============*/
	mem_heap_t*	heap)	/*!< in: mem heap where created */
{
	upd_node_t*	node;

	node = static_cast<upd_node_t*>(
		mem_heap_alloc(heap, sizeof(upd_node_t)));

	node->common.type = QUE_NODE_UPDATE;

	node->state = UPD_NODE_UPDATE_CLUSTERED;
	node->in_mysql_interface = FALSE;

	node->row = NULL;
	node->ext = NULL;
	node->upd_row = NULL;
	node->upd_ext = NULL;
	node->index = NULL;
	node->update = NULL;

	node->foreign = NULL;
	node->cascade_heap = NULL;
	node->cascade_node = NULL;

	node->select = NULL;

	node->heap = mem_heap_create(128);
	node->magic_n = UPD_NODE_MAGIC_N;

	node->cmpl_info = 0;

	return(node);
}
#endif /* !UNIV_HOTBACKUP */

/*********************************************************************//**
Updates the trx id and roll ptr field in a clustered index record in database
recovery. */
UNIV_INTERN
void
row_upd_rec_sys_fields_in_recovery(
/*===============================*/
	rec_t*		rec,	/*!< in/out: record */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		pos,	/*!< in: TRX_ID position in rec */
	trx_id_t	trx_id,	/*!< in: transaction id */
	roll_ptr_t	roll_ptr)/*!< in: roll ptr of the undo log record */
{
	ut_ad(rec_offs_validate(rec, NULL, offsets));

	if (page_zip) {
		page_zip_write_trx_id_and_roll_ptr(
			page_zip, rec, offsets, pos, trx_id, roll_ptr);
	} else {
		byte*	field;
		ulint	len;

		field = rec_get_nth_field(rec, offsets, pos, &len);
		ut_ad(len == DATA_TRX_ID_LEN);
#if DATA_TRX_ID + 1 != DATA_ROLL_PTR
# error "DATA_TRX_ID + 1 != DATA_ROLL_PTR"
#endif
		trx_write_trx_id(field, trx_id);
		trx_write_roll_ptr(field + DATA_TRX_ID_LEN, roll_ptr);
	}
}

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Sets the trx id or roll ptr field of a clustered index entry. */
UNIV_INTERN
void
row_upd_index_entry_sys_field(
/*==========================*/
	dtuple_t*	entry,	/*!< in/out: index entry, where the memory
				buffers for sys fields are already allocated:
				the function just copies the new values to
				them */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint		type,	/*!< in: DATA_TRX_ID or DATA_ROLL_PTR */
	ib_uint64_t	val)	/*!< in: value to write */
{
	dfield_t*	dfield;
	byte*		field;
	ulint		pos;

	ut_ad(dict_index_is_clust(index));

	pos = dict_index_get_sys_col_pos(index, type);

	dfield = dtuple_get_nth_field(entry, pos);
	field = static_cast<byte*>(dfield_get_data(dfield));

	if (type == DATA_TRX_ID) {
		trx_write_trx_id(field, val);
	} else {
		ut_ad(type == DATA_ROLL_PTR);
		trx_write_roll_ptr(field, val);
	}
}

/***********************************************************//**
Returns TRUE if row update changes size of some field in index or if some
field to be updated is stored externally in rec or update.
@return TRUE if the update changes the size of some field in index or
the field is external in rec or update */
UNIV_INTERN
ibool
row_upd_changes_field_size_or_external(
/*===================================*/
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	const upd_t*	update)	/*!< in: update vector */
{
	const upd_field_t*	upd_field;
	const dfield_t*		new_val;
	ulint			old_len;
	ulint			new_len;
	ulint			n_fields;
	ulint			i;

	ut_ad(rec_offs_validate(NULL, index, offsets));
	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		upd_field = upd_get_nth_field(update, i);

		new_val = &(upd_field->new_val);
		new_len = dfield_get_len(new_val);

		if (dfield_is_null(new_val) && !rec_offs_comp(offsets)) {
			/* A bug fixed on Dec 31st, 2004: we looked at the
			SQL NULL size from the wrong field! We may backport
			this fix also to 4.0. The merge to 5.0 will be made
			manually immediately after we commit this to 4.1. */

			new_len = dict_col_get_sql_null_size(
				dict_index_get_nth_col(index,
						       upd_field->field_no),
				0);
		}

		old_len = rec_offs_nth_size(offsets, upd_field->field_no);

		if (rec_offs_comp(offsets)
		    && rec_offs_nth_sql_null(offsets,
					     upd_field->field_no)) {
			/* Note that in the compact table format, for a
			variable length field, an SQL NULL will use zero
			bytes in the offset array at the start of the physical
			record, but a zero-length value (empty string) will
			use one byte! Thus, we cannot use update-in-place
			if we update an SQL NULL varchar to an empty string! */

			old_len = UNIV_SQL_NULL;
		}

		if (dfield_is_ext(new_val) || old_len != new_len
		    || rec_offs_nth_extern(offsets, upd_field->field_no)) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/***********************************************************//**
Returns true if row update contains disowned external fields.
@return true if the update contains disowned external fields. */
UNIV_INTERN
bool
row_upd_changes_disowned_external(
/*==============================*/
	const upd_t*	update)	/*!< in: update vector */
{
	const upd_field_t*	upd_field;
	const dfield_t*		new_val;
	ulint			new_len;
	ulint                   n_fields;
	ulint			i;

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		const byte*	field_ref;

		upd_field = upd_get_nth_field(update, i);
		new_val = &(upd_field->new_val);
		new_len = dfield_get_len(new_val);

		if (!dfield_is_ext(new_val)) {
			continue;
		}

		ut_ad(new_len >= BTR_EXTERN_FIELD_REF_SIZE);

		field_ref = static_cast<const byte*>(dfield_get_data(new_val))
			    + new_len - BTR_EXTERN_FIELD_REF_SIZE;

		if (field_ref[BTR_EXTERN_LEN] & BTR_EXTERN_OWNER_FLAG) {
			return(true);
		}
	}

	return(false);
}
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Replaces the new column values stored in the update vector to the
record given. No field size changes are allowed. This function is
usually invoked on a clustered index. The only use case for a
secondary index is row_ins_sec_index_entry_by_modify() or its
counterpart in ibuf_insert_to_index_page(). */
UNIV_INTERN
void
row_upd_rec_in_place(
/*=================*/
	rec_t*		rec,	/*!< in/out: record where replaced */
	dict_index_t*	index,	/*!< in: the index the record belongs to */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	const upd_t*	update,	/*!< in: update vector */
	page_zip_des_t*	page_zip)/*!< in: compressed page with enough space
				available, or NULL */
{
	const upd_field_t*	upd_field;
	const dfield_t*		new_val;
	ulint			n_fields;
	ulint			i;

	ut_ad(rec_offs_validate(rec, index, offsets));

	if (rec_offs_comp(offsets)) {
		rec_set_info_bits_new(rec, update->info_bits);
	} else {
		rec_set_info_bits_old(rec, update->info_bits);
	}

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
#ifdef UNIV_BLOB_DEBUG
		btr_blob_dbg_t	b;
		const byte*	field_ref	= NULL;
#endif /* UNIV_BLOB_DEBUG */

		upd_field = upd_get_nth_field(update, i);
		new_val = &(upd_field->new_val);
		ut_ad(!dfield_is_ext(new_val) ==
		      !rec_offs_nth_extern(offsets, upd_field->field_no));
#ifdef UNIV_BLOB_DEBUG
		if (dfield_is_ext(new_val)) {
			ulint	len;
			field_ref = rec_get_nth_field(rec, offsets, i, &len);
			ut_a(len != UNIV_SQL_NULL);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);
			field_ref += len - BTR_EXTERN_FIELD_REF_SIZE;

			b.ref_page_no = page_get_page_no(page_align(rec));
			b.ref_heap_no = page_rec_get_heap_no(rec);
			b.ref_field_no = i;
			b.blob_page_no = mach_read_from_4(
				field_ref + BTR_EXTERN_PAGE_NO);
			ut_a(b.ref_field_no >= index->n_uniq);
			btr_blob_dbg_rbt_delete(index, &b, "upd_in_place");
		}
#endif /* UNIV_BLOB_DEBUG */

		rec_set_nth_field(rec, offsets, upd_field->field_no,
				  dfield_get_data(new_val),
				  dfield_get_len(new_val));

#ifdef UNIV_BLOB_DEBUG
		if (dfield_is_ext(new_val)) {
			b.blob_page_no = mach_read_from_4(
				field_ref + BTR_EXTERN_PAGE_NO);
			b.always_owner = b.owner = !(field_ref[BTR_EXTERN_LEN]
						     & BTR_EXTERN_OWNER_FLAG);
			b.del = rec_get_deleted_flag(
				rec, rec_offs_comp(offsets));

			btr_blob_dbg_rbt_insert(index, &b, "upd_in_place");
		}
#endif /* UNIV_BLOB_DEBUG */
	}

	if (page_zip) {
		page_zip_write_rec(page_zip, rec, index, offsets, 0);
	}
}

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Writes into the redo log the values of trx id and roll ptr and enough info
to determine their positions within a clustered index record.
@return	new pointer to mlog */
UNIV_INTERN
byte*
row_upd_write_sys_vals_to_log(
/*==========================*/
	dict_index_t*	index,	/*!< in: clustered index */
	trx_id_t	trx_id,	/*!< in: transaction id */
	roll_ptr_t	roll_ptr,/*!< in: roll ptr of the undo log record */
	byte*		log_ptr,/*!< pointer to a buffer of size > 20 opened
				in mlog */
	mtr_t*		mtr __attribute__((unused))) /*!< in: mtr */
{
	ut_ad(dict_index_is_clust(index));
	ut_ad(mtr);

	log_ptr += mach_write_compressed(log_ptr,
					 dict_index_get_sys_col_pos(
						 index, DATA_TRX_ID));

	trx_write_roll_ptr(log_ptr, roll_ptr);
	log_ptr += DATA_ROLL_PTR_LEN;

	log_ptr += mach_ull_write_compressed(log_ptr, trx_id);

	return(log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/*********************************************************************//**
Parses the log data of system field values.
@return	log data end or NULL */
UNIV_INTERN
byte*
row_upd_parse_sys_vals(
/*===================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	ulint*		pos,	/*!< out: TRX_ID position in record */
	trx_id_t*	trx_id,	/*!< out: trx id */
	roll_ptr_t*	roll_ptr)/*!< out: roll ptr */
{
	ptr = mach_parse_compressed(ptr, end_ptr, pos);

	if (ptr == NULL) {

		return(NULL);
	}

	if (end_ptr < ptr + DATA_ROLL_PTR_LEN) {

		return(NULL);
	}

	*roll_ptr = trx_read_roll_ptr(ptr);
	ptr += DATA_ROLL_PTR_LEN;

	ptr = mach_ull_parse_compressed(ptr, end_ptr, trx_id);

	return(ptr);
}

#ifndef UNIV_HOTBACKUP
/***********************************************************//**
Writes to the redo log the new values of the fields occurring in the index. */
UNIV_INTERN
void
row_upd_index_write_log(
/*====================*/
	const upd_t*	update,	/*!< in: update vector */
	byte*		log_ptr,/*!< in: pointer to mlog buffer: must
				contain at least MLOG_BUF_MARGIN bytes
				of free space; the buffer is closed
				within this function */
	mtr_t*		mtr)	/*!< in: mtr into whose log to write */
{
	const upd_field_t*	upd_field;
	const dfield_t*		new_val;
	ulint			len;
	ulint			n_fields;
	byte*			buf_end;
	ulint			i;

	n_fields = upd_get_n_fields(update);

	buf_end = log_ptr + MLOG_BUF_MARGIN;

	mach_write_to_1(log_ptr, update->info_bits);
	log_ptr++;
	log_ptr += mach_write_compressed(log_ptr, n_fields);

	for (i = 0; i < n_fields; i++) {

#if MLOG_BUF_MARGIN <= 30
# error "MLOG_BUF_MARGIN <= 30"
#endif

		if (log_ptr + 30 > buf_end) {
			mlog_close(mtr, log_ptr);

			log_ptr = mlog_open(mtr, MLOG_BUF_MARGIN);
			buf_end = log_ptr + MLOG_BUF_MARGIN;
		}

		upd_field = upd_get_nth_field(update, i);

		new_val = &(upd_field->new_val);

		len = dfield_get_len(new_val);

		log_ptr += mach_write_compressed(log_ptr, upd_field->field_no);
		log_ptr += mach_write_compressed(log_ptr, len);

		if (len != UNIV_SQL_NULL) {
			if (log_ptr + len < buf_end) {
				memcpy(log_ptr, dfield_get_data(new_val), len);

				log_ptr += len;
			} else {
				mlog_close(mtr, log_ptr);

				mlog_catenate_string(
					mtr,
					static_cast<byte*>(
						dfield_get_data(new_val)),
					len);

				log_ptr = mlog_open(mtr, MLOG_BUF_MARGIN);
				buf_end = log_ptr + MLOG_BUF_MARGIN;
			}
		}
	}

	mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/*********************************************************************//**
Parses the log data written by row_upd_index_write_log.
@return	log data end or NULL */
UNIV_INTERN
byte*
row_upd_index_parse(
/*================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	mem_heap_t*	heap,	/*!< in: memory heap where update vector is
				built */
	upd_t**		update_out)/*!< out: update vector */
{
	upd_t*		update;
	upd_field_t*	upd_field;
	dfield_t*	new_val;
	ulint		len;
	ulint		n_fields;
	ulint		info_bits;
	ulint		i;

	if (end_ptr < ptr + 1) {

		return(NULL);
	}

	info_bits = mach_read_from_1(ptr);
	ptr++;
	ptr = mach_parse_compressed(ptr, end_ptr, &n_fields);

	if (ptr == NULL) {

		return(NULL);
	}

	update = upd_create(n_fields, heap);
	update->info_bits = info_bits;

	for (i = 0; i < n_fields; i++) {
		ulint	field_no;
		upd_field = upd_get_nth_field(update, i);
		new_val = &(upd_field->new_val);

		ptr = mach_parse_compressed(ptr, end_ptr, &field_no);

		if (ptr == NULL) {

			return(NULL);
		}

		upd_field->field_no = field_no;

		ptr = mach_parse_compressed(ptr, end_ptr, &len);

		if (ptr == NULL) {

			return(NULL);
		}

		if (len != UNIV_SQL_NULL) {

			if (end_ptr < ptr + len) {

				return(NULL);
			}

			dfield_set_data(new_val,
					mem_heap_dup(heap, ptr, len), len);
			ptr += len;
		} else {
			dfield_set_null(new_val);
		}
	}

	*update_out = update;

	return(ptr);
}

#ifndef UNIV_HOTBACKUP
/***************************************************************//**
Builds an update vector from those fields which in a secondary index entry
differ from a record that has the equal ordering fields. NOTE: we compare
the fields as binary strings!
@return	own: update vector of differing fields */
UNIV_INTERN
upd_t*
row_upd_build_sec_rec_difference_binary(
/*====================================*/
	const rec_t*	rec,	/*!< in: secondary index record */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec, index) */
	const dtuple_t*	entry,	/*!< in: entry to insert */
	mem_heap_t*	heap)	/*!< in: memory heap from which allocated */
{
	upd_field_t*	upd_field;
	const dfield_t*	dfield;
	const byte*	data;
	ulint		len;
	upd_t*		update;
	ulint		n_diff;
	ulint		i;

	/* This function is used only for a secondary index */
	ut_a(!dict_index_is_clust(index));
	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(rec_offs_n_fields(offsets) == dtuple_get_n_fields(entry));
	ut_ad(!rec_offs_any_extern(offsets));

	update = upd_create(dtuple_get_n_fields(entry), heap);

	n_diff = 0;

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {

		data = rec_get_nth_field(rec, offsets, i, &len);

		dfield = dtuple_get_nth_field(entry, i);

		/* NOTE that it may be that len != dfield_get_len(dfield) if we
		are updating in a character set and collation where strings of
		different length can be equal in an alphabetical comparison,
		and also in the case where we have a column prefix index
		and the last characters in the index field are spaces; the
		latter case probably caused the assertion failures reported at
		row0upd.cc line 713 in versions 4.0.14 - 4.0.16. */

		/* NOTE: we compare the fields as binary strings!
		(No collation) */

		if (!dfield_data_is_binary_equal(dfield, len, data)) {

			upd_field = upd_get_nth_field(update, n_diff);

			dfield_copy(&(upd_field->new_val), dfield);

			upd_field_set_field_no(upd_field, i, index, NULL);

			n_diff++;
		}
	}

	update->n_fields = n_diff;

	return(update);
}

/***************************************************************//**
Builds an update vector from those fields, excluding the roll ptr and
trx id fields, which in an index entry differ from a record that has
the equal ordering fields. NOTE: we compare the fields as binary strings!
@return own: update vector of differing fields, excluding roll ptr and
trx id */
UNIV_INTERN
const upd_t*
row_upd_build_difference_binary(
/*============================*/
	dict_index_t*	index,	/*!< in: clustered index */
	const dtuple_t*	entry,	/*!< in: entry to insert */
	const rec_t*	rec,	/*!< in: clustered index record */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec,index), or NULL */
	bool		no_sys,	/*!< in: skip the system columns
				DB_TRX_ID and DB_ROLL_PTR */
	trx_t*		trx,	/*!< in: transaction */
	mem_heap_t*	heap)	/*!< in: memory heap from which allocated */
{
	upd_field_t*	upd_field;
	const dfield_t*	dfield;
	const byte*	data;
	ulint		len;
	upd_t*		update;
	ulint		n_diff;
	ulint		trx_id_pos;
	ulint		i;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	rec_offs_init(offsets_);

	/* This function is used only for a clustered index */
	ut_a(dict_index_is_clust(index));

	update = upd_create(dtuple_get_n_fields(entry), heap);

	n_diff = 0;

	trx_id_pos = dict_index_get_sys_col_pos(index, DATA_TRX_ID);
	ut_ad(dict_index_get_sys_col_pos(index, DATA_ROLL_PTR)
	      == trx_id_pos + 1);

	if (!offsets) {
		offsets = rec_get_offsets(rec, index, offsets_,
					  ULINT_UNDEFINED, &heap);
	} else {
		ut_ad(rec_offs_validate(rec, index, offsets));
	}

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {

		data = rec_get_nth_field(rec, offsets, i, &len);

		dfield = dtuple_get_nth_field(entry, i);

		/* NOTE: we compare the fields as binary strings!
		(No collation) */

		if (no_sys && (i == trx_id_pos || i == trx_id_pos + 1)) {

			continue;
		}

		if (!dfield_is_ext(dfield)
		    != !rec_offs_nth_extern(offsets, i)
		    || !dfield_data_is_binary_equal(dfield, len, data)) {

			upd_field = upd_get_nth_field(update, n_diff);

			dfield_copy(&(upd_field->new_val), dfield);

			upd_field_set_field_no(upd_field, i, index, trx);

			n_diff++;
		}
	}

	update->n_fields = n_diff;

	return(update);
}

/***********************************************************//**
Fetch a prefix of an externally stored column.  This is similar
to row_ext_lookup(), but the row_ext_t holds the old values
of the column and must not be poisoned with the new values.
@return	BLOB prefix */
static
byte*
row_upd_ext_fetch(
/*==============*/
	const byte*	data,		/*!< in: 'internally' stored part of the
					field containing also the reference to
					the external part */
	ulint		local_len,	/*!< in: length of data, in bytes */
	ulint		zip_size,	/*!< in: nonzero=compressed BLOB
					page size, zero for uncompressed
					BLOBs */
	ulint*		len,		/*!< in: length of prefix to fetch;
					out: fetched length of the prefix */
	mem_heap_t*	heap)		/*!< in: heap where to allocate */
{
	byte*	buf = static_cast<byte*>(mem_heap_alloc(heap, *len));

	*len = btr_copy_externally_stored_field_prefix(
		buf, *len, zip_size, data, local_len);

	/* We should never update records containing a half-deleted BLOB. */
	ut_a(*len);

	return(buf);
}

/***********************************************************//**
Replaces the new column value stored in the update vector in
the given index entry field. */
static
void
row_upd_index_replace_new_col_val(
/*==============================*/
	dfield_t*		dfield,	/*!< in/out: data field
					of the index entry */
	const dict_field_t*	field,	/*!< in: index field */
	const dict_col_t*	col,	/*!< in: field->col */
	const upd_field_t*	uf,	/*!< in: update field */
	mem_heap_t*		heap,	/*!< in: memory heap for allocating
					and copying the new value */
	ulint			zip_size)/*!< in: compressed page
					 size of the table, or 0 */
{
	ulint		len;
	const byte*	data;

	dfield_copy_data(dfield, &uf->new_val);

	if (dfield_is_null(dfield)) {
		return;
	}

	len = dfield_get_len(dfield);
	data = static_cast<const byte*>(dfield_get_data(dfield));

	if (field->prefix_len > 0) {
		ibool		fetch_ext = dfield_is_ext(dfield)
			&& len < (ulint) field->prefix_len
			+ BTR_EXTERN_FIELD_REF_SIZE;

		if (fetch_ext) {
			ulint	l = len;

			len = field->prefix_len;

			data = row_upd_ext_fetch(data, l, zip_size,
						 &len, heap);
		}

		len = dtype_get_at_most_n_mbchars(col->prtype,
						  col->mbminmaxlen,
						  field->prefix_len, len,
						  (const char*) data);

		dfield_set_data(dfield, data, len);

		if (!fetch_ext) {
			dfield_dup(dfield, heap);
		}

		return;
	}

	switch (uf->orig_len) {
		byte*	buf;
	case BTR_EXTERN_FIELD_REF_SIZE:
		/* Restore the original locally stored
		part of the column.  In the undo log,
		InnoDB writes a longer prefix of externally
		stored columns, so that column prefixes
		in secondary indexes can be reconstructed. */
		dfield_set_data(dfield,
				data + len - BTR_EXTERN_FIELD_REF_SIZE,
				BTR_EXTERN_FIELD_REF_SIZE);
		dfield_set_ext(dfield);
		/* fall through */
	case 0:
		dfield_dup(dfield, heap);
		break;
	default:
		/* Reconstruct the original locally
		stored part of the column.  The data
		will have to be copied. */
		ut_a(uf->orig_len > BTR_EXTERN_FIELD_REF_SIZE);
		buf = static_cast<byte*>(mem_heap_alloc(heap, uf->orig_len));

		/* Copy the locally stored prefix. */
		memcpy(buf, data,
		       uf->orig_len - BTR_EXTERN_FIELD_REF_SIZE);

		/* Copy the BLOB pointer. */
		memcpy(buf + uf->orig_len - BTR_EXTERN_FIELD_REF_SIZE,
		       data + len - BTR_EXTERN_FIELD_REF_SIZE,
		       BTR_EXTERN_FIELD_REF_SIZE);

		dfield_set_data(dfield, buf, uf->orig_len);
		dfield_set_ext(dfield);
		break;
	}
}

/***********************************************************//**
Replaces the new column values stored in the update vector to the index entry
given. */
UNIV_INTERN
void
row_upd_index_replace_new_col_vals_index_pos(
/*=========================================*/
	dtuple_t*	entry,	/*!< in/out: index entry where replaced;
				the clustered index record must be
				covered by a lock or a page latch to
				prevent deletion (rollback or purge) */
	dict_index_t*	index,	/*!< in: index; NOTE that this may also be a
				non-clustered index */
	const upd_t*	update,	/*!< in: an update vector built for the index so
				that the field number in an upd_field is the
				index position */
	ibool		order_only,
				/*!< in: if TRUE, limit the replacement to
				ordering fields of index; note that this
				does not work for non-clustered indexes. */
	mem_heap_t*	heap)	/*!< in: memory heap for allocating and
				copying the new values */
{
	ulint		i;
	ulint		n_fields;
	const ulint	zip_size	= dict_table_zip_size(index->table);

	ut_ad(index);

	dtuple_set_info_bits(entry, update->info_bits);

	if (order_only) {
		n_fields = dict_index_get_n_unique(index);
	} else {
		n_fields = dict_index_get_n_fields(index);
	}

	for (i = 0; i < n_fields; i++) {
		const dict_field_t*	field;
		const dict_col_t*	col;
		const upd_field_t*	uf;

		field = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(field);
		uf = upd_get_field_by_field_no(update, i);

		if (uf) {
			row_upd_index_replace_new_col_val(
				dtuple_get_nth_field(entry, i),
				field, col, uf, heap, zip_size);
		}
	}
}

/***********************************************************//**
Replaces the new column values stored in the update vector to the index entry
given. */
UNIV_INTERN
void
row_upd_index_replace_new_col_vals(
/*===============================*/
	dtuple_t*	entry,	/*!< in/out: index entry where replaced;
				the clustered index record must be
				covered by a lock or a page latch to
				prevent deletion (rollback or purge) */
	dict_index_t*	index,	/*!< in: index; NOTE that this may also be a
				non-clustered index */
	const upd_t*	update,	/*!< in: an update vector built for the
				CLUSTERED index so that the field number in
				an upd_field is the clustered index position */
	mem_heap_t*	heap)	/*!< in: memory heap for allocating and
				copying the new values */
{
	ulint			i;
	const dict_index_t*	clust_index
		= dict_table_get_first_index(index->table);
	const ulint		zip_size
		= dict_table_zip_size(index->table);

	dtuple_set_info_bits(entry, update->info_bits);

	for (i = 0; i < dict_index_get_n_fields(index); i++) {
		const dict_field_t*	field;
		const dict_col_t*	col;
		const upd_field_t*	uf;

		field = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(field);
		uf = upd_get_field_by_field_no(
			update, dict_col_get_clust_pos(col, clust_index));

		if (uf) {
			row_upd_index_replace_new_col_val(
				dtuple_get_nth_field(entry, i),
				field, col, uf, heap, zip_size);
		}
	}
}

/***********************************************************//**
Replaces the new column values stored in the update vector. */
UNIV_INTERN
void
row_upd_replace(
/*============*/
	dtuple_t*		row,	/*!< in/out: row where replaced,
					indexed by col_no;
					the clustered index record must be
					covered by a lock or a page latch to
					prevent deletion (rollback or purge) */
	row_ext_t**		ext,	/*!< out, own: NULL, or externally
					stored column prefixes */
	const dict_index_t*	index,	/*!< in: clustered index */
	const upd_t*		update,	/*!< in: an update vector built for the
					clustered index */
	mem_heap_t*		heap)	/*!< in: memory heap */
{
	ulint			col_no;
	ulint			i;
	ulint			n_cols;
	ulint			n_ext_cols;
	ulint*			ext_cols;
	const dict_table_t*	table;

	ut_ad(row);
	ut_ad(ext);
	ut_ad(index);
	ut_ad(dict_index_is_clust(index));
	ut_ad(update);
	ut_ad(heap);

	n_cols = dtuple_get_n_fields(row);
	table = index->table;
	ut_ad(n_cols == dict_table_get_n_cols(table));

	ext_cols = static_cast<ulint*>(
		mem_heap_alloc(heap, n_cols * sizeof *ext_cols));

	n_ext_cols = 0;

	dtuple_set_info_bits(row, update->info_bits);

	for (col_no = 0; col_no < n_cols; col_no++) {

		const dict_col_t*	col
			= dict_table_get_nth_col(table, col_no);
		const ulint		clust_pos
			= dict_col_get_clust_pos(col, index);
		dfield_t*		dfield;

		if (UNIV_UNLIKELY(clust_pos == ULINT_UNDEFINED)) {

			continue;
		}

		dfield = dtuple_get_nth_field(row, col_no);

		for (i = 0; i < upd_get_n_fields(update); i++) {

			const upd_field_t*	upd_field
				= upd_get_nth_field(update, i);

			if (upd_field->field_no != clust_pos) {

				continue;
			}

			dfield_copy_data(dfield, &upd_field->new_val);
			break;
		}

		if (dfield_is_ext(dfield) && col->ord_part) {
			ext_cols[n_ext_cols++] = col_no;
		}
	}

	if (n_ext_cols) {
		*ext = row_ext_create(n_ext_cols, ext_cols, table->flags, row,
				      heap);
	} else {
		*ext = NULL;
	}
}

/***********************************************************//**
Checks if an update vector changes an ordering field of an index record.

This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic.
NOTE: we compare the fields as binary strings!
@return TRUE if update vector changes an ordering field in the index record */
UNIV_INTERN
ibool
row_upd_changes_ord_field_binary_func(
/*==================================*/
	dict_index_t*	index,	/*!< in: index of the record */
	const upd_t*	update,	/*!< in: update vector for the row; NOTE: the
				field numbers in this MUST be clustered index
				positions! */
#ifdef UNIV_DEBUG
	const que_thr_t*thr,	/*!< in: query thread */
#endif /* UNIV_DEBUG */
	const dtuple_t*	row,	/*!< in: old value of row, or NULL if the
				row and the data values in update are not
				known when this function is called, e.g., at
				compile time */
	const row_ext_t*ext)	/*!< NULL, or prefixes of the externally
				stored columns in the old row */
{
	ulint			n_unique;
	ulint			i;
	const dict_index_t*	clust_index;

	ut_ad(index);
	ut_ad(update);
	ut_ad(thr);
	ut_ad(thr->graph);
	ut_ad(thr->graph->trx);

	n_unique = dict_index_get_n_unique(index);

	clust_index = dict_table_get_first_index(index->table);

	for (i = 0; i < n_unique; i++) {

		const dict_field_t*	ind_field;
		const dict_col_t*	col;
		ulint			col_no;
		const upd_field_t*	upd_field;
		const dfield_t*		dfield;
		dfield_t		dfield_ext;
		ulint			dfield_len;
		const byte*		buf;

		ind_field = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(ind_field);
		col_no = dict_col_get_no(col);

		upd_field = upd_get_field_by_field_no(
			update, dict_col_get_clust_pos(col, clust_index));

		if (upd_field == NULL) {
			continue;
		}

		if (row == NULL) {
			ut_ad(ext == NULL);
			return(TRUE);
		}

		dfield = dtuple_get_nth_field(row, col_no);

		/* This treatment of column prefix indexes is loosely
		based on row_build_index_entry(). */

		if (UNIV_LIKELY(ind_field->prefix_len == 0)
		    || dfield_is_null(dfield)) {
			/* do nothing special */
		} else if (ext) {
			/* Silence a compiler warning without
			silencing a Valgrind error. */
			dfield_len = 0;
			UNIV_MEM_INVALID(&dfield_len, sizeof dfield_len);
			/* See if the column is stored externally. */
			buf = row_ext_lookup(ext, col_no, &dfield_len);

			ut_ad(col->ord_part);

			if (UNIV_LIKELY_NULL(buf)) {
				if (UNIV_UNLIKELY(buf == field_ref_zero)) {
					/* The externally stored field
					was not written yet. This
					record should only be seen by
					recv_recovery_rollback_active(),
					when the server had crashed before
					storing the field. */
					ut_ad(thr->graph->trx->is_recovered);
					ut_ad(trx_is_recv(thr->graph->trx));
					return(TRUE);
				}

				goto copy_dfield;
			}
		} else if (dfield_is_ext(dfield)) {
			dfield_len = dfield_get_len(dfield);
			ut_a(dfield_len > BTR_EXTERN_FIELD_REF_SIZE);
			dfield_len -= BTR_EXTERN_FIELD_REF_SIZE;
			ut_a(dict_index_is_clust(index)
			     || ind_field->prefix_len <= dfield_len);

			buf = static_cast<byte*>(dfield_get_data(dfield));
copy_dfield:
			ut_a(dfield_len > 0);
			dfield_copy(&dfield_ext, dfield);
			dfield_set_data(&dfield_ext, buf, dfield_len);
			dfield = &dfield_ext;
		}

		if (!dfield_datas_are_binary_equal(
			    dfield, &upd_field->new_val,
			    ind_field->prefix_len)) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/***********************************************************//**
Checks if an update vector changes an ordering field of an index record.
NOTE: we compare the fields as binary strings!
@return TRUE if update vector may change an ordering field in an index
record */
UNIV_INTERN
ibool
row_upd_changes_some_index_ord_field_binary(
/*========================================*/
	const dict_table_t*	table,	/*!< in: table */
	const upd_t*		update)	/*!< in: update vector for the row */
{
	upd_field_t*	upd_field;
	dict_index_t*	index;
	ulint		i;

	index = dict_table_get_first_index(table);

	for (i = 0; i < upd_get_n_fields(update); i++) {

		upd_field = upd_get_nth_field(update, i);

		if (dict_field_get_col(dict_index_get_nth_field(
					       index, upd_field->field_no))
		    ->ord_part) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/***********************************************************//**
Checks if an FTS Doc ID column is affected by an UPDATE.
@return whether the Doc ID column is changed */
UNIV_INTERN
bool
row_upd_changes_doc_id(
/*===================*/
	dict_table_t*	table,		/*!< in: table */
	upd_field_t*	upd_field)	/*!< in: field to check */
{
	ulint		col_no;
	dict_index_t*	clust_index;
	fts_t*		fts = table->fts;

	clust_index = dict_table_get_first_index(table);

	/* Convert from index-specific column number to table-global
	column number. */
	col_no = dict_index_get_nth_col_no(clust_index, upd_field->field_no);

	return(col_no == fts->doc_col);
}
/***********************************************************//**
Checks if an FTS indexed column is affected by an UPDATE.
@return offset within fts_t::indexes if FTS indexed column updated else
ULINT_UNDEFINED */
UNIV_INTERN
ulint
row_upd_changes_fts_column(
/*=======================*/
	dict_table_t*	table,		/*!< in: table */
	upd_field_t*	upd_field)	/*!< in: field to check */
{
	ulint		col_no;
	dict_index_t*	clust_index;
	fts_t*		fts = table->fts;

	clust_index = dict_table_get_first_index(table);

	/* Convert from index-specific column number to table-global
	column number. */
	col_no = dict_index_get_nth_col_no(clust_index, upd_field->field_no);

	return(dict_table_is_fts_column(fts->indexes, col_no));
}

/***********************************************************//**
Checks if an update vector changes some of the first ordering fields of an
index record. This is only used in foreign key checks and we can assume
that index does not contain column prefixes.
@return	TRUE if changes */
static
ibool
row_upd_changes_first_fields_binary(
/*================================*/
	dtuple_t*	entry,	/*!< in: index entry */
	dict_index_t*	index,	/*!< in: index of entry */
	const upd_t*	update,	/*!< in: update vector for the row */
	ulint		n)	/*!< in: how many first fields to check */
{
	ulint		n_upd_fields;
	ulint		i, j;
	dict_index_t*	clust_index;

	ut_ad(update && index);
	ut_ad(n <= dict_index_get_n_fields(index));

	n_upd_fields = upd_get_n_fields(update);
	clust_index = dict_table_get_first_index(index->table);

	for (i = 0; i < n; i++) {

		const dict_field_t*	ind_field;
		const dict_col_t*	col;
		ulint			col_pos;

		ind_field = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(ind_field);
		col_pos = dict_col_get_clust_pos(col, clust_index);

		ut_a(ind_field->prefix_len == 0);

		for (j = 0; j < n_upd_fields; j++) {

			upd_field_t*	upd_field
				= upd_get_nth_field(update, j);

			if (col_pos == upd_field->field_no
			    && !dfield_datas_are_binary_equal(
				    dtuple_get_nth_field(entry, i),
				    &upd_field->new_val, 0)) {

				return(TRUE);
			}
		}
	}

	return(FALSE);
}

/*********************************************************************//**
Copies the column values from a record. */
UNIV_INLINE
void
row_upd_copy_columns(
/*=================*/
	rec_t*		rec,	/*!< in: record in a clustered index */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	sym_node_t*	column)	/*!< in: first column in a column list, or
				NULL */
{
	byte*	data;
	ulint	len;

	while (column) {
		data = rec_get_nth_field(rec, offsets,
					 column->field_nos[SYM_CLUST_FIELD_NO],
					 &len);
		eval_node_copy_and_alloc_val(column, data, len);

		column = UT_LIST_GET_NEXT(col_var_list, column);
	}
}

/*********************************************************************//**
Calculates the new values for fields to update. Note that row_upd_copy_columns
must have been called first. */
UNIV_INLINE
void
row_upd_eval_new_vals(
/*==================*/
	upd_t*	update)	/*!< in/out: update vector */
{
	que_node_t*	exp;
	upd_field_t*	upd_field;
	ulint		n_fields;
	ulint		i;

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		upd_field = upd_get_nth_field(update, i);

		exp = upd_field->exp;

		eval_exp(exp);

		dfield_copy_data(&(upd_field->new_val), que_node_get_val(exp));
	}
}

/***********************************************************//**
Stores to the heap the row on which the node->pcur is positioned. */
static
void
row_upd_store_row(
/*==============*/
	upd_node_t*	node)	/*!< in: row update node */
{
	dict_index_t*	clust_index;
	rec_t*		rec;
	mem_heap_t*	heap		= NULL;
	row_ext_t**	ext;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	const ulint*	offsets;
	rec_offs_init(offsets_);

	ut_ad(node->pcur->latch_mode != BTR_NO_LATCHES);

	if (node->row != NULL) {
		mem_heap_empty(node->heap);
	}

	clust_index = dict_table_get_first_index(node->table);

	rec = btr_pcur_get_rec(node->pcur);

	offsets = rec_get_offsets(rec, clust_index, offsets_,
				  ULINT_UNDEFINED, &heap);

	if (dict_table_get_format(node->table) >= UNIV_FORMAT_B) {
		/* In DYNAMIC or COMPRESSED format, there is no prefix
		of externally stored columns in the clustered index
		record. Build a cache of column prefixes. */
		ext = &node->ext;
	} else {
		/* REDUNDANT and COMPACT formats store a local
		768-byte prefix of each externally stored column.
		No cache is needed. */
		ext = NULL;
		node->ext = NULL;
	}

	node->row = row_build(ROW_COPY_DATA, clust_index, rec, offsets,
			      NULL, NULL, NULL, ext, node->heap);
	if (node->is_delete) {
		node->upd_row = NULL;
		node->upd_ext = NULL;
	} else {
		node->upd_row = dtuple_copy(node->row, node->heap);
		row_upd_replace(node->upd_row, &node->upd_ext,
				clust_index, node->update, node->heap);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/***********************************************************//**
Updates a secondary index entry of a row.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_sec_index_entry(
/*====================*/
	upd_node_t*	node,	/*!< in: row update node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	mtr_t			mtr;
	const rec_t*		rec;
	btr_pcur_t		pcur;
	mem_heap_t*		heap;
	dtuple_t*		entry;
	dict_index_t*		index;
	btr_cur_t*		btr_cur;
	ibool			referenced;
	dberr_t			err	= DB_SUCCESS;
	trx_t*			trx	= thr_get_trx(thr);
	ulint			mode;
	enum row_search_result	search_result;

	ut_ad(trx->id);

	index = node->index;

	referenced = row_upd_index_is_referenced(index, trx);

	heap = mem_heap_create(1024);

	/* Build old index entry */
	entry = row_build_index_entry(node->row, node->ext, index, heap);
	ut_a(entry);

	log_free_check();

#ifdef UNIV_DEBUG
	/* Work around Bug#14626800 ASSERTION FAILURE IN DEBUG_SYNC().
	Once it is fixed, remove the 'ifdef', 'if' and this comment. */
	if (!trx->ddl) {
		DEBUG_SYNC_C_IF_THD(trx->mysql_thd,
				    "before_row_upd_sec_index_entry");
	}
#endif /* UNIV_DEBUG */

	mtr_start(&mtr);

	if (*index->name == TEMP_INDEX_PREFIX) {
		/* The index->online_status may change if the
		index->name starts with TEMP_INDEX_PREFIX (meaning
		that the index is or was being created online). It is
		protected by index->lock. */

		mtr_s_lock(dict_index_get_lock(index), &mtr);

		switch (dict_index_get_online_status(index)) {
		case ONLINE_INDEX_COMPLETE:
			/* This is a normal index. Do not log anything.
			Perform the update on the index tree directly. */
			break;
		case ONLINE_INDEX_CREATION:
			/* Log a DELETE and optionally INSERT. */
			row_log_online_op(index, entry, 0);

			if (!node->is_delete) {
				mem_heap_empty(heap);
				entry = row_build_index_entry(
					node->upd_row, node->upd_ext,
					index, heap);
				ut_a(entry);
				row_log_online_op(index, entry, trx->id);
			}
			/* fall through */
		case ONLINE_INDEX_ABORTED:
		case ONLINE_INDEX_ABORTED_DROPPED:
			mtr_commit(&mtr);
			goto func_exit;
		}

		/* We can only buffer delete-mark operations if there
		are no foreign key constraints referring to the index.
		Insert/Change buffering is block for temp-table
		and so no point in removing entry from these buffers
		if not present in buffer-pool */
		mode = (referenced || dict_table_is_temporary(index->table))
			? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
			: BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
			| BTR_DELETE_MARK;
	} else {
		/* For secondary indexes,
		index->online_status==ONLINE_INDEX_CREATION unless
		index->name starts with TEMP_INDEX_PREFIX. */
		ut_ad(!dict_index_is_online_ddl(index));

		/* We can only buffer delete-mark operations if there
		are no foreign key constraints referring to the index.
		Insert/Change buffering is block for temp-table
		and so no point in removing entry from these buffers
		if not present in buffer-pool */
		mode = (referenced || dict_table_is_temporary(index->table))
			? BTR_MODIFY_LEAF
			: BTR_MODIFY_LEAF | BTR_DELETE_MARK;
	}

	/* Set the query thread, so that ibuf_insert_low() will be
	able to invoke thd_get_trx(). */
	btr_pcur_get_btr_cur(&pcur)->thr = thr;

	search_result = row_search_index_entry(index, entry, mode,
					       &pcur, &mtr);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	rec = btr_cur_get_rec(btr_cur);

	switch (search_result) {
	case ROW_NOT_DELETED_REF:	/* should only occur for BTR_DELETE */
		ut_error;
		break;
	case ROW_BUFFERED:
		/* Entry was delete marked already. */
		break;

	case ROW_NOT_FOUND:
		if (*index->name == TEMP_INDEX_PREFIX) {
			/* When online CREATE INDEX copied the update
			that we already made to the clustered index,
			and completed the secondary index creation
			before we got here, the old secondary index
			record would not exist. The CREATE INDEX
			should be waiting for a MySQL meta-data lock
			upgrade at least until this UPDATE
			returns. After that point, the
			TEMP_INDEX_PREFIX would be dropped from the
			index name in commit_inplace_alter_table(). */
			break;
		}

		fputs("InnoDB: error in sec index entry update in\n"
		      "InnoDB: ", stderr);
		dict_index_name_print(stderr, trx, index);
		fputs("\n"
		      "InnoDB: tuple ", stderr);
		dtuple_print(stderr, entry);
		fputs("\n"
		      "InnoDB: record ", stderr);
		rec_print(stderr, rec, index);
		putc('\n', stderr);
		trx_print(stderr, trx, 0);
		fputs("\n"
		      "InnoDB: Submit a detailed bug report"
		      " to http://bugs.mysql.com\n", stderr);
		ut_ad(0);
		break;
	case ROW_FOUND:
		/* Delete mark the old index record; it can already be
		delete marked if we return after a lock wait in
		row_ins_sec_index_entry() below */
		if (!rec_get_deleted_flag(
			    rec, dict_table_is_comp(index->table))) {
			err = btr_cur_del_mark_set_sec_rec(
				0, btr_cur, TRUE, thr, &mtr);

			if (err == DB_SUCCESS && referenced) {

				ulint*	offsets;

				offsets = rec_get_offsets(
					rec, index, NULL, ULINT_UNDEFINED,
					&heap);

				/* NOTE that the following call loses
				the position of pcur ! */
				err = row_upd_check_references_constraints(
					node, &pcur, index->table,
					index, offsets, thr, &mtr);
			}
		}
		break;
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	if (node->is_delete || err != DB_SUCCESS) {

		goto func_exit;
	}

	mem_heap_empty(heap);

	/* Build a new index entry */
	entry = row_build_index_entry(node->upd_row, node->upd_ext,
				      index, heap);
	ut_a(entry);

	/* Insert new index entry */
	err = row_ins_sec_index_entry(index, entry, thr);

func_exit:
	mem_heap_free(heap);

	return(err);
}

/***********************************************************//**
Updates the secondary index record if it is changed in the row update or
deletes it if this is a delete.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_sec_step(
/*=============*/
	upd_node_t*	node,	/*!< in: row update node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ut_ad((node->state == UPD_NODE_UPDATE_ALL_SEC)
	      || (node->state == UPD_NODE_UPDATE_SOME_SEC));
	ut_ad(!dict_index_is_clust(node->index));

	if (node->state == UPD_NODE_UPDATE_ALL_SEC
	    || row_upd_changes_ord_field_binary(node->index, node->update,
						thr, node->row, node->ext)) {
		return(row_upd_sec_index_entry(node, thr));
	}

	return(DB_SUCCESS);
}

#ifdef UNIV_DEBUG
# define row_upd_clust_rec_by_insert_inherit(rec,offsets,entry,update)	\
	row_upd_clust_rec_by_insert_inherit_func(rec,offsets,entry,update)
#else /* UNIV_DEBUG */
# define row_upd_clust_rec_by_insert_inherit(rec,offsets,entry,update)	\
	row_upd_clust_rec_by_insert_inherit_func(entry,update)
#endif /* UNIV_DEBUG */
/*******************************************************************//**
Mark non-updated off-page columns inherited when the primary key is
updated. We must mark them as inherited in entry, so that they are not
freed in a rollback. A limited version of this function used to be
called btr_cur_mark_dtuple_inherited_extern().
@return TRUE if any columns were inherited */
static __attribute__((warn_unused_result))
ibool
row_upd_clust_rec_by_insert_inherit_func(
/*=====================================*/
#ifdef UNIV_DEBUG
	const rec_t*	rec,	/*!< in: old record, or NULL */
	const ulint*	offsets,/*!< in: rec_get_offsets(rec), or NULL */
#endif /* UNIV_DEBUG */
	dtuple_t*	entry,	/*!< in/out: updated entry to be
				inserted into the clustered index */
	const upd_t*	update)	/*!< in: update vector */
{
	ibool	inherit	= FALSE;
	ulint	i;

	ut_ad(!rec == !offsets);
	ut_ad(!rec || rec_offs_any_extern(offsets));

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {
		dfield_t*	dfield	= dtuple_get_nth_field(entry, i);
		byte*		data;
		ulint		len;

		ut_ad(!offsets
		      || !rec_offs_nth_extern(offsets, i)
		      == !dfield_is_ext(dfield)
		      || upd_get_field_by_field_no(update, i));
		if (!dfield_is_ext(dfield)
		    || upd_get_field_by_field_no(update, i)) {
			continue;
		}

#ifdef UNIV_DEBUG
		if (UNIV_LIKELY(rec != NULL)) {
			const byte* rec_data
				= rec_get_nth_field(rec, offsets, i, &len);
			ut_ad(len == dfield_get_len(dfield));
			ut_ad(len != UNIV_SQL_NULL);
			ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);

			rec_data += len - BTR_EXTERN_FIELD_REF_SIZE;

			/* The pointer must not be zero. */
			ut_ad(memcmp(rec_data, field_ref_zero,
				     BTR_EXTERN_FIELD_REF_SIZE));
			/* The BLOB must be owned. */
			ut_ad(!(rec_data[BTR_EXTERN_LEN]
				& BTR_EXTERN_OWNER_FLAG));
		}
#endif /* UNIV_DEBUG */

		len = dfield_get_len(dfield);
		ut_a(len != UNIV_SQL_NULL);
		ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);

		data = static_cast<byte*>(dfield_get_data(dfield));

		data += len - BTR_EXTERN_FIELD_REF_SIZE;
		/* The pointer must not be zero. */
		ut_a(memcmp(data, field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));
		/* The BLOB must be owned. */
		ut_a(!(data[BTR_EXTERN_LEN] & BTR_EXTERN_OWNER_FLAG));

		data[BTR_EXTERN_LEN] |= BTR_EXTERN_INHERITED_FLAG;
		/* The BTR_EXTERN_INHERITED_FLAG only matters in
		rollback. Purge will always free the extern fields of
		a delete-marked row. */

		inherit = TRUE;
	}

	return(inherit);
}

/***********************************************************//**
Marks the clustered index record deleted and inserts the updated version
of the record to the index. This function should be used when the ordering
fields of the clustered index record change. This should be quite rare in
database applications.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_clust_rec_by_insert(
/*========================*/
	upd_node_t*	node,	/*!< in/out: row update node */
	dict_index_t*	index,	/*!< in: clustered index of the record */
	que_thr_t*	thr,	/*!< in: query thread */
	ibool		referenced,/*!< in: TRUE if index may be referenced in
				a foreign key constraint */
	mtr_t*		mtr)	/*!< in/out: mtr; gets committed here */
{
	mem_heap_t*	heap;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	trx_t*		trx;
	dict_table_t*	table;
	dtuple_t*	entry;
	dberr_t		err;
	ibool		change_ownership	= FALSE;
	rec_t*		rec;
	ulint*		offsets			= NULL;

	ut_ad(node);
	ut_ad(dict_index_is_clust(index));

	trx = thr_get_trx(thr);
	table = node->table;
	pcur = node->pcur;
	btr_cur	= btr_pcur_get_btr_cur(pcur);

	heap = mem_heap_create(1000);

	entry = row_build_index_entry(node->upd_row, node->upd_ext,
				      index, heap);
	ut_a(entry);

	row_upd_index_entry_sys_field(entry, index, DATA_TRX_ID, trx->id);

	switch (node->state) {
	default:
		ut_error;
	case UPD_NODE_INSERT_BLOB:
		/* A lock wait occurred in row_ins_clust_index_entry() in
		the previous invocation of this function. Mark the
		off-page columns in the entry inherited. */

		change_ownership = row_upd_clust_rec_by_insert_inherit(
			NULL, NULL, entry, node->update);
		ut_a(change_ownership);
		/* fall through */
	case UPD_NODE_INSERT_CLUSTERED:
		/* A lock wait occurred in row_ins_clust_index_entry() in
		the previous invocation of this function. */
		break;
	case UPD_NODE_UPDATE_CLUSTERED:
		/* This is the first invocation of the function where
		we update the primary key.  Delete-mark the old record
		in the clustered index and prepare to insert a new entry. */
		rec = btr_cur_get_rec(btr_cur);
		offsets = rec_get_offsets(rec, index, NULL,
					  ULINT_UNDEFINED, &heap);
		ut_ad(page_rec_is_user_rec(rec));

		err = btr_cur_del_mark_set_clust_rec(
			btr_cur_get_block(btr_cur), rec, index, offsets,
			thr, mtr);
		if (err != DB_SUCCESS) {
err_exit:
			mtr_commit(mtr);
			mem_heap_free(heap);
			return(err);
		}

		/* If the the new row inherits externally stored
		fields (off-page columns a.k.a. BLOBs) from the
		delete-marked old record, mark them disowned by the
		old record and owned by the new entry. */

		if (rec_offs_any_extern(offsets)) {
			change_ownership = row_upd_clust_rec_by_insert_inherit(
				rec, offsets, entry, node->update);

			if (change_ownership) {
				btr_pcur_store_position(pcur, mtr);
			}
		}

		if (referenced) {
			/* NOTE that the following call loses
			the position of pcur ! */

			err = row_upd_check_references_constraints(
				node, pcur, table, index, offsets, thr, mtr);

			if (err != DB_SUCCESS) {
				goto err_exit;
			}
		}
	}

	mtr_commit(mtr);

	err = row_ins_clust_index_entry(
		index, entry, thr,
		node->upd_ext ? node->upd_ext->n_ext : 0);
	node->state = change_ownership
		? UPD_NODE_INSERT_BLOB
		: UPD_NODE_INSERT_CLUSTERED;

	if (err == DB_SUCCESS && change_ownership) {
		/* Mark the non-updated fields disowned by the old record. */

		/* NOTE: this transaction has an x-lock on the record
		and therefore other transactions cannot modify the
		record when we have no latch on the page. In addition,
		we assume that other query threads of the same
		transaction do not modify the record in the meantime.
		Therefore we can assert that the restoration of the
		cursor succeeds. */

		mtr_start(mtr);

		if (!btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur, mtr)) {
			ut_error;
		}

		rec = btr_cur_get_rec(btr_cur);
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		ut_ad(page_rec_is_user_rec(rec));
		ut_ad(rec_get_deleted_flag(rec, rec_offs_comp(offsets)));

		btr_cur_disown_inherited_fields(
			btr_cur_get_page_zip(btr_cur),
			rec, index, offsets, node->update, mtr);

		/* It is not necessary to call row_log_table for
		this, because during online table rebuild, purge will
		not free any BLOBs in the table, whether or not they
		are owned by the clustered index record. */

		mtr_commit(mtr);
	}

	mem_heap_free(heap);

	return(err);
}

/***********************************************************//**
Updates a clustered index record of a row when the ordering fields do
not change.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_clust_rec(
/*==============*/
	upd_node_t*	node,	/*!< in: row update node */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint*		offsets,/*!< in: rec_get_offsets() on node->pcur */
	mem_heap_t**	offsets_heap,
				/*!< in/out: memory heap, can be emptied */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr; gets committed here */
{
	mem_heap_t*	heap		= NULL;
	big_rec_t*	big_rec		= NULL;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	dberr_t		err;
	const dtuple_t*	rebuilt_old_pk	= NULL;

	ut_ad(node);
	ut_ad(dict_index_is_clust(index));

	pcur = node->pcur;
	btr_cur = btr_pcur_get_btr_cur(pcur);

	ut_ad(btr_cur_get_index(btr_cur) == index);
	ut_ad(!rec_get_deleted_flag(btr_cur_get_rec(btr_cur),
				    dict_table_is_comp(index->table)));
	ut_ad(rec_offs_validate(btr_cur_get_rec(btr_cur), index, offsets));

	if (dict_index_is_online_ddl(index)) {
		rebuilt_old_pk = row_log_table_get_pk(
			btr_cur_get_rec(btr_cur), index, offsets, &heap);
	}

	/* Try optimistic updating of the record, keeping changes within
	the page; we do not check locks because we assume the x-lock on the
	record to update */

	if (node->cmpl_info & UPD_NODE_NO_SIZE_CHANGE) {
		err = btr_cur_update_in_place(
			BTR_NO_LOCKING_FLAG, btr_cur,
			offsets, node->update,
			node->cmpl_info, thr, thr_get_trx(thr)->id, mtr);
	} else {
		err = btr_cur_optimistic_update(
			BTR_NO_LOCKING_FLAG, btr_cur,
			&offsets, offsets_heap, node->update,
			node->cmpl_info, thr, thr_get_trx(thr)->id, mtr);
	}

	if (err == DB_SUCCESS && dict_index_is_online_ddl(index)) {
		row_log_table_update(btr_cur_get_rec(btr_cur),
				     index, offsets, rebuilt_old_pk);
	}

	mtr_commit(mtr);

	if (UNIV_LIKELY(err == DB_SUCCESS)) {

		goto func_exit;
	}

	if (buf_LRU_buf_pool_running_out()) {

		err = DB_LOCK_TABLE_FULL;
		goto func_exit;
	}
	/* We may have to modify the tree structure: do a pessimistic descent
	down the index tree */

	mtr_start(mtr);

	/* NOTE: this transaction has an s-lock or x-lock on the record and
	therefore other transactions cannot modify the record when we have no
	latch on the page. In addition, we assume that other query threads of
	the same transaction do not modify the record in the meantime.
	Therefore we can assert that the restoration of the cursor succeeds. */

	ut_a(btr_pcur_restore_position(BTR_MODIFY_TREE, pcur, mtr));

	ut_ad(!rec_get_deleted_flag(btr_pcur_get_rec(pcur),
				    dict_table_is_comp(index->table)));

	if (!heap) {
		heap = mem_heap_create(1024);
	}

	err = btr_cur_pessimistic_update(
		BTR_NO_LOCKING_FLAG | BTR_KEEP_POS_FLAG, btr_cur,
		&offsets, offsets_heap, heap, &big_rec,
		node->update, node->cmpl_info,
		thr, thr_get_trx(thr)->id, mtr);
	if (big_rec) {
		ut_a(err == DB_SUCCESS);
		/* Write out the externally stored
		columns while still x-latching
		index->lock and block->lock. Allocate
		pages for big_rec in the mtr that
		modified the B-tree, but be sure to skip
		any pages that were freed in mtr. We will
		write out the big_rec pages before
		committing the B-tree mini-transaction. If
		the system crashes so that crash recovery
		will not replay the mtr_commit(&mtr), the
		big_rec pages will be left orphaned until
		the pages are allocated for something else.

		TODO: If the allocation extends the tablespace, it
		will not be redo logged, in either mini-transaction.
		Tablespace extension should be redo-logged in the
		big_rec mini-transaction, so that recovery will not
		fail when the big_rec was written to the extended
		portion of the file, in case the file was somehow
		truncated in the crash. */

		DEBUG_SYNC_C("before_row_upd_extern");
		err = btr_store_big_rec_extern_fields(
			index, btr_cur_get_block(btr_cur),
			btr_cur_get_rec(btr_cur), offsets,
			big_rec, mtr, BTR_STORE_UPDATE);
		DEBUG_SYNC_C("after_row_upd_extern");
		/* If writing big_rec fails (for example, because of
		DB_OUT_OF_FILE_SPACE), the record will be corrupted.
		Even if we did not update any externally stored
		columns, our update could cause the record to grow so
		that a non-updated column was selected for external
		storage. This non-update would not have been written
		to the undo log, and thus the record cannot be rolled
		back.

		However, because we have not executed mtr_commit(mtr)
		yet, the update will not be replayed in crash
		recovery, and the following assertion failure will
		effectively "roll back" the operation. */
		ut_a(err == DB_SUCCESS);
	}

	if (err == DB_SUCCESS && dict_index_is_online_ddl(index)) {
		row_log_table_update(btr_cur_get_rec(btr_cur),
				     index, offsets, rebuilt_old_pk);
	}

	mtr_commit(mtr);
func_exit:
	if (heap) {
		mem_heap_free(heap);
	}

	if (big_rec) {
		dtuple_big_rec_free(big_rec);
	}

	return(err);
}

/***********************************************************//**
Delete marks a clustered index record.
@return	DB_SUCCESS if operation successfully completed, else error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_del_mark_clust_rec(
/*=======================*/
	upd_node_t*	node,	/*!< in: row update node */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint*		offsets,/*!< in/out: rec_get_offsets() for the
				record under the cursor */
	que_thr_t*	thr,	/*!< in: query thread */
	ibool		referenced,
				/*!< in: TRUE if index may be referenced in
				a foreign key constraint */
	mtr_t*		mtr)	/*!< in: mtr; gets committed here */
{
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	dberr_t		err;

	ut_ad(node);
	ut_ad(dict_index_is_clust(index));
	ut_ad(node->is_delete);

	pcur = node->pcur;
	btr_cur = btr_pcur_get_btr_cur(pcur);

	/* Store row because we have to build also the secondary index
	entries */

	row_upd_store_row(node);

	/* Mark the clustered index record deleted; we do not have to check
	locks, because we assume that we have an x-lock on the record */

	err = btr_cur_del_mark_set_clust_rec(
		btr_cur_get_block(btr_cur), btr_cur_get_rec(btr_cur),
		index, offsets, thr, mtr);
	if (err == DB_SUCCESS && referenced) {
		/* NOTE that the following call loses the position of pcur ! */

		err = row_upd_check_references_constraints(
			node, pcur, index->table, index, offsets, thr, mtr);
	}

	mtr_commit(mtr);

	return(err);
}

/***********************************************************//**
Updates the clustered index record.
@return DB_SUCCESS if operation successfully completed, DB_LOCK_WAIT
in case of a lock wait, else error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd_clust_step(
/*===============*/
	upd_node_t*	node,	/*!< in: row update node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	ibool		success;
	dberr_t		err;
	mtr_t		mtr;
	rec_t*		rec;
	mem_heap_t*	heap	= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets;
	ibool		referenced;
	rec_offs_init(offsets_);

	index = dict_table_get_first_index(node->table);

	referenced = row_upd_index_is_referenced(index, thr_get_trx(thr));

	pcur = node->pcur;

	/* We have to restore the cursor to its position */

	mtr_start(&mtr);

	/* If the restoration does not succeed, then the same
	transaction has deleted the record on which the cursor was,
	and that is an SQL error. If the restoration succeeds, it may
	still be that the same transaction has successively deleted
	and inserted a record with the same ordering fields, but in
	that case we know that the transaction has at least an
	implicit x-lock on the record. */

	ut_a(pcur->rel_pos == BTR_PCUR_ON);

	ulint	mode;

#ifdef UNIV_DEBUG
	/* Work around Bug#14626800 ASSERTION FAILURE IN DEBUG_SYNC().
	Once it is fixed, remove the 'ifdef', 'if' and this comment. */
	if (!thr_get_trx(thr)->ddl) {
		DEBUG_SYNC_C_IF_THD(
			thr_get_trx(thr)->mysql_thd,
			"innodb_row_upd_clust_step_enter");
	}
#endif /* UNIV_DEBUG */

	if (dict_index_is_online_ddl(index)) {
		ut_ad(node->table->id != DICT_INDEXES_ID);
		mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
		mtr_s_lock(dict_index_get_lock(index), &mtr);
	} else {
		mode = BTR_MODIFY_LEAF;
	}

	success = btr_pcur_restore_position(mode, pcur, &mtr);

	if (!success) {
		err = DB_RECORD_NOT_FOUND;

		mtr_commit(&mtr);

		return(err);
	}

	/* If this is a row in SYS_INDEXES table of the data dictionary,
	then we have to free the file segments of the index tree associated
	with the index */

	if (node->is_delete && node->table->id == DICT_INDEXES_ID) {

		ut_ad(!dict_index_is_online_ddl(index));

		dict_drop_index_tree_step(btr_pcur_get_rec(pcur), &mtr);

		mtr_commit(&mtr);

		mtr_start(&mtr);

		success = btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur,
						    &mtr);
		if (!success) {
			err = DB_ERROR;

			mtr_commit(&mtr);

			return(err);
		}
	}

	rec = btr_pcur_get_rec(pcur);
	offsets = rec_get_offsets(rec, index, offsets_,
				  ULINT_UNDEFINED, &heap);

	if (!node->has_clust_rec_x_lock) {
		err = lock_clust_rec_modify_check_and_lock(
			0, btr_pcur_get_block(pcur),
			rec, index, offsets, thr);
		if (err != DB_SUCCESS) {
			mtr_commit(&mtr);
			goto exit_func;
		}
	}

	/* NOTE: the following function calls will also commit mtr */

	if (node->is_delete) {
		err = row_upd_del_mark_clust_rec(
			node, index, offsets, thr, referenced, &mtr);

		if (err == DB_SUCCESS) {
			node->state = UPD_NODE_UPDATE_ALL_SEC;
			node->index = dict_table_get_next_index(index);
		}

		goto exit_func;
	}

	/* If the update is made for MySQL, we already have the update vector
	ready, else we have to do some evaluation: */

	if (UNIV_UNLIKELY(!node->in_mysql_interface)) {
		/* Copy the necessary columns from clust_rec and calculate the
		new values to set */
		row_upd_copy_columns(rec, offsets,
				     UT_LIST_GET_FIRST(node->columns));
		row_upd_eval_new_vals(node->update);
	}

	if (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE) {

		err = row_upd_clust_rec(
			node, index, offsets, &heap, thr, &mtr);
		goto exit_func;
	}

	row_upd_store_row(node);

	if (row_upd_changes_ord_field_binary(index, node->update, thr,
					     node->row, node->ext)) {

		/* Update causes an ordering field (ordering fields within
		the B-tree) of the clustered index record to change: perform
		the update by delete marking and inserting.

		TODO! What to do to the 'Halloween problem', where an update
		moves the record forward in index so that it is again
		updated when the cursor arrives there? Solution: the
		read operation must check the undo record undo number when
		choosing records to update. MySQL solves now the problem
		externally! */

		err = row_upd_clust_rec_by_insert(
			node, index, thr, referenced, &mtr);

		if (err != DB_SUCCESS) {

			goto exit_func;
		}

		node->state = UPD_NODE_UPDATE_ALL_SEC;
	} else {
		err = row_upd_clust_rec(
			node, index, offsets, &heap, thr, &mtr);

		if (err != DB_SUCCESS) {

			goto exit_func;
		}

		node->state = UPD_NODE_UPDATE_SOME_SEC;
	}

	node->index = dict_table_get_next_index(index);

exit_func:
	if (heap) {
		mem_heap_free(heap);
	}
	return(err);
}

/***********************************************************//**
Updates the affected index records of a row. When the control is transferred
to this node, we assume that we have a persistent cursor which was on a
record, and the position of the cursor is stored in the cursor.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_upd(
/*====*/
	upd_node_t*	node,	/*!< in: row update node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t		err	= DB_SUCCESS;

	ut_ad(node && thr);

	if (UNIV_LIKELY(node->in_mysql_interface)) {

		/* We do not get the cmpl_info value from the MySQL
		interpreter: we must calculate it on the fly: */

		if (node->is_delete
		    || row_upd_changes_some_index_ord_field_binary(
			    node->table, node->update)) {
			node->cmpl_info = 0;
		} else {
			node->cmpl_info = UPD_NODE_NO_ORD_CHANGE;
		}
	}

	switch (node->state) {
	case UPD_NODE_UPDATE_CLUSTERED:
	case UPD_NODE_INSERT_CLUSTERED:
	case UPD_NODE_INSERT_BLOB:
		log_free_check();
		err = row_upd_clust_step(node, thr);

		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	if (node->index == NULL
	    || (!node->is_delete
		&& (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE))) {

		return(DB_SUCCESS);
	}

#ifdef UNIV_DEBUG
	/* Work around Bug#14626800 ASSERTION FAILURE IN DEBUG_SYNC().
	Once it is fixed, remove the 'ifdef', 'if' and this comment. */
	if (!thr_get_trx(thr)->ddl) {
		DEBUG_SYNC_C_IF_THD(thr_get_trx(thr)->mysql_thd,
				    "after_row_upd_clust");
	}
#endif /* UNIV_DEBUG */

	DBUG_EXECUTE_IF("row_upd_skip_sec", node->index = NULL;);

	do {
		/* Skip corrupted index */
		dict_table_skip_corrupt_index(node->index);

		if (!node->index) {
			break;
		}

		if (node->index->type != DICT_FTS) {
			err = row_upd_sec_step(node, thr);

			if (err != DB_SUCCESS) {

				return(err);
			}
		}

		node->index = dict_table_get_next_index(node->index);
	} while (node->index != NULL);

	ut_ad(err == DB_SUCCESS);

	/* Do some cleanup */

	if (node->row != NULL) {
		node->row = NULL;
		node->ext = NULL;
		node->upd_row = NULL;
		node->upd_ext = NULL;
		mem_heap_empty(node->heap);
	}

	node->state = UPD_NODE_UPDATE_CLUSTERED;

	return(err);
}

/***********************************************************//**
Updates a row in a table. This is a high-level function used in SQL execution
graphs.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
row_upd_step(
/*=========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	upd_node_t*	node;
	sel_node_t*	sel_node;
	que_node_t*	parent;
	dberr_t		err		= DB_SUCCESS;
	trx_t*		trx;

	ut_ad(thr);

	trx = thr_get_trx(thr);

	trx_start_if_not_started_xa(trx);

	node = static_cast<upd_node_t*>(thr->run_node);

	sel_node = node->select;

	parent = que_node_get_parent(node);

	ut_ad(que_node_get_type(node) == QUE_NODE_UPDATE);

	if (thr->prev_node == parent) {
		node->state = UPD_NODE_SET_IX_LOCK;
	}

	if (node->state == UPD_NODE_SET_IX_LOCK) {

		if (!node->has_clust_rec_x_lock) {
			/* It may be that the current session has not yet
			started its transaction, or it has been committed: */

			err = lock_table(0, node->table, LOCK_IX, thr);

			if (err != DB_SUCCESS) {

				goto error_handling;
			}
		}

		node->state = UPD_NODE_UPDATE_CLUSTERED;

		if (node->searched_update) {
			/* Reset the cursor */
			sel_node->state = SEL_NODE_OPEN;

			/* Fetch a row to update */

			thr->run_node = sel_node;

			return(thr);
		}
	}

	/* sel_node is NULL if we are in the MySQL interface */

	if (sel_node && (sel_node->state != SEL_NODE_FETCH)) {

		if (!node->searched_update) {
			/* An explicit cursor should be positioned on a row
			to update */

			ut_error;

			err = DB_ERROR;

			goto error_handling;
		}

		ut_ad(sel_node->state == SEL_NODE_NO_MORE_ROWS);

		/* No more rows to update, or the select node performed the
		updates directly in-place */

		thr->run_node = parent;

		return(thr);
	}

	/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */

	err = row_upd(node, thr);

error_handling:
	trx->error_state = err;

	if (err != DB_SUCCESS) {
		return(NULL);
	}

	/* DO THE TRIGGER ACTIONS HERE */

	if (node->searched_update) {
		/* Fetch next row to update */

		thr->run_node = sel_node;
	} else {
		/* It was an explicit cursor update */

		thr->run_node = parent;
	}

	node->state = UPD_NODE_UPDATE_CLUSTERED;

	return(thr);
}
#endif /* !UNIV_HOTBACKUP */
