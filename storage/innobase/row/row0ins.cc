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
@file row/row0ins.cc
Insert into a table

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#include "row0ins.h"

#ifdef UNIV_NONINL
#include "row0ins.ic"
#endif

#include "ha_prototypes.h"
#include "dict0dict.h"
#include "dict0boot.h"
#include "trx0rec.h"
#include "trx0undo.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "mach0data.h"
#include "que0que.h"
#include "row0upd.h"
#include "row0sel.h"
#include "row0row.h"
#include "row0log.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "log0log.h"
#include "eval0eval.h"
#include "data0data.h"
#include "usr0sess.h"
#include "buf0lru.h"
#include "fts0fts.h"
#include "fts0types.h"
#include "m_string.h"

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchonization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

/*********************************************************************//**
Creates an insert node struct.
@return	own: insert node struct */
UNIV_INTERN
ins_node_t*
ins_node_create(
/*============*/
	ulint		ins_type,	/*!< in: INS_VALUES, ... */
	dict_table_t*	table,		/*!< in: table where to insert */
	mem_heap_t*	heap)		/*!< in: mem heap where created */
{
	ins_node_t*	node;

	node = static_cast<ins_node_t*>(
		mem_heap_alloc(heap, sizeof(ins_node_t)));

	node->common.type = QUE_NODE_INSERT;

	node->ins_type = ins_type;

	node->state = INS_NODE_SET_IX_LOCK;
	node->table = table;
	node->index = NULL;
	node->entry = NULL;

	node->select = NULL;

	node->trx_id = 0;

	node->entry_sys_heap = mem_heap_create(128);

	node->magic_n = INS_NODE_MAGIC_N;

	return(node);
}

/***********************************************************//**
Creates an entry template for each index of a table. */
static
void
ins_node_create_entry_list(
/*=======================*/
	ins_node_t*	node)	/*!< in: row insert node */
{
	dict_index_t*	index;
	dtuple_t*	entry;

	ut_ad(node->entry_sys_heap);

	UT_LIST_INIT(node->entry_list);

	/* We will include all indexes (include those corrupted
	secondary indexes) in the entry list. Filteration of
	these corrupted index will be done in row_ins() */

	for (index = dict_table_get_first_index(node->table);
	     index != 0;
	     index = dict_table_get_next_index(index)) {

		entry = row_build_index_entry(
			node->row, NULL, index, node->entry_sys_heap);

		UT_LIST_ADD_LAST(tuple_list, node->entry_list, entry);
	}
}

/*****************************************************************//**
Adds system field buffers to a row. */
static
void
row_ins_alloc_sys_fields(
/*=====================*/
	ins_node_t*	node)	/*!< in: insert node */
{
	dtuple_t*		row;
	dict_table_t*		table;
	mem_heap_t*		heap;
	const dict_col_t*	col;
	dfield_t*		dfield;
	byte*			ptr;

	row = node->row;
	table = node->table;
	heap = node->entry_sys_heap;

	ut_ad(row && table && heap);
	ut_ad(dtuple_get_n_fields(row) == dict_table_get_n_cols(table));

	/* 1. Allocate buffer for row id */

	col = dict_table_get_sys_col(table, DATA_ROW_ID);

	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));

	ptr = static_cast<byte*>(mem_heap_zalloc(heap, DATA_ROW_ID_LEN));

	dfield_set_data(dfield, ptr, DATA_ROW_ID_LEN);

	node->row_id_buf = ptr;

	/* 3. Allocate buffer for trx id */

	col = dict_table_get_sys_col(table, DATA_TRX_ID);

	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = static_cast<byte*>(mem_heap_zalloc(heap, DATA_TRX_ID_LEN));

	dfield_set_data(dfield, ptr, DATA_TRX_ID_LEN);

	node->trx_id_buf = ptr;

	/* 4. Allocate buffer for roll ptr */

	col = dict_table_get_sys_col(table, DATA_ROLL_PTR);

	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = static_cast<byte*>(mem_heap_zalloc(heap, DATA_ROLL_PTR_LEN));

	dfield_set_data(dfield, ptr, DATA_ROLL_PTR_LEN);
}

/*********************************************************************//**
Sets a new row to insert for an INS_DIRECT node. This function is only used
if we have constructed the row separately, which is a rare case; this
function is quite slow. */
UNIV_INTERN
void
ins_node_set_new_row(
/*=================*/
	ins_node_t*	node,	/*!< in: insert node */
	dtuple_t*	row)	/*!< in: new row (or first row) for the node */
{
	node->state = INS_NODE_SET_IX_LOCK;
	node->index = NULL;
	node->entry = NULL;

	node->row = row;

	mem_heap_empty(node->entry_sys_heap);

	/* Create templates for index entries */

	ins_node_create_entry_list(node);

	/* Allocate from entry_sys_heap buffers for sys fields */

	row_ins_alloc_sys_fields(node);

	/* As we allocated a new trx id buf, the trx id should be written
	there again: */

	node->trx_id = 0;
}

/*******************************************************************//**
Does an insert operation by updating a delete-marked existing record
in the index. This situation can occur if the delete-marked record is
kept in the index for consistent reads.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_sec_index_entry_by_modify(
/*==============================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/*!< in: B-tree cursor */
	ulint**		offsets,/*!< in/out: offsets on cursor->page_cur.rec */
	mem_heap_t*	offsets_heap,
				/*!< in/out: memory heap that can be emptied */
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr; must be committed before
				latching any further pages */
{
	big_rec_t*	dummy_big_rec;
	upd_t*		update;
	rec_t*		rec;
	dberr_t		err;

	rec = btr_cur_get_rec(cursor);

	ut_ad(!dict_index_is_clust(cursor->index));
	ut_ad(rec_offs_validate(rec, cursor->index, *offsets));
	ut_ad(!entry->info_bits);

	/* We know that in the alphabetical ordering, entry and rec are
	identified. But in their binary form there may be differences if
	there are char fields in them. Therefore we have to calculate the
	difference. */

	update = row_upd_build_sec_rec_difference_binary(
		rec, cursor->index, *offsets, entry, heap);

	if (!rec_get_deleted_flag(rec, rec_offs_comp(*offsets))) {
		/* We should never insert in place of a record that
		has not been delete-marked. The only exception is when
		online CREATE INDEX copied the changes that we already
		made to the clustered index, and completed the
		secondary index creation before we got here. In this
		case, the change would already be there. The CREATE
		INDEX should be waiting for a MySQL meta-data lock
		upgrade at least until this INSERT or UPDATE
		returns. After that point, the TEMP_INDEX_PREFIX
		would be dropped from the index name in
		commit_inplace_alter_table(). */
		ut_a(update->n_fields == 0);
		ut_a(*cursor->index->name == TEMP_INDEX_PREFIX);
		ut_ad(!dict_index_is_online_ddl(cursor->index));
		return(DB_SUCCESS);
	}

	if (mode == BTR_MODIFY_LEAF) {
		/* Try an optimistic updating of the record, keeping changes
		within the page */

		/* TODO: pass only *offsets */
		err = btr_cur_optimistic_update(
			flags | BTR_KEEP_SYS_FLAG, cursor,
			offsets, &offsets_heap, update, 0, thr,
			thr_get_trx(thr)->id, mtr);
		switch (err) {
		case DB_OVERFLOW:
		case DB_UNDERFLOW:
		case DB_ZIP_OVERFLOW:
			err = DB_FAIL;
		default:
			break;
		}
	} else {
		ut_a(mode == BTR_MODIFY_TREE);
		if (buf_LRU_buf_pool_running_out()) {

			return(DB_LOCK_TABLE_FULL);
		}

		err = btr_cur_pessimistic_update(
			flags | BTR_KEEP_SYS_FLAG, cursor,
			offsets, &offsets_heap,
			heap, &dummy_big_rec, update, 0,
			thr, thr_get_trx(thr)->id, mtr);
		ut_ad(!dummy_big_rec);
	}

	return(err);
}

/*******************************************************************//**
Does an insert operation by delete unmarking and updating a delete marked
existing record in the index. This situation can occur if the delete marked
record is kept in the index for consistent reads.
@return	DB_SUCCESS, DB_FAIL, or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_clust_index_entry_by_modify(
/*================================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/*!< in: B-tree cursor */
	ulint**		offsets,/*!< out: offsets on cursor->page_cur.rec */
	mem_heap_t**	offsets_heap,
				/*!< in/out: pointer to memory heap that can
				be emptied, or NULL */
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	big_rec_t**	big_rec,/*!< out: possible big rec vector of fields
				which have to be stored externally by the
				caller */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr; must be committed before
				latching any further pages */
{
	const rec_t*	rec;
	const upd_t*	update;
	dberr_t		err;

	ut_ad(dict_index_is_clust(cursor->index));

	*big_rec = NULL;

	rec = btr_cur_get_rec(cursor);

	ut_ad(rec_get_deleted_flag(rec,
				   dict_table_is_comp(cursor->index->table)));

	/* Build an update vector containing all the fields to be modified;
	NOTE that this vector may NOT contain system columns trx_id or
	roll_ptr */

	update = row_upd_build_difference_binary(
		cursor->index, entry, rec, NULL, true,
		thr_get_trx(thr), heap);
	if (mode != BTR_MODIFY_TREE) {
		ut_ad((mode & ~BTR_ALREADY_S_LATCHED) == BTR_MODIFY_LEAF);

		/* Try optimistic updating of the record, keeping changes
		within the page */

		err = btr_cur_optimistic_update(
			flags, cursor, offsets, offsets_heap, update, 0, thr,
			thr_get_trx(thr)->id, mtr);
		switch (err) {
		case DB_OVERFLOW:
		case DB_UNDERFLOW:
		case DB_ZIP_OVERFLOW:
			err = DB_FAIL;
		default:
			break;
		}
	} else {
		if (buf_LRU_buf_pool_running_out()) {

			return(DB_LOCK_TABLE_FULL);

		}
		err = btr_cur_pessimistic_update(
			flags | BTR_KEEP_POS_FLAG,
			cursor, offsets, offsets_heap, heap,
			big_rec, update, 0, thr, thr_get_trx(thr)->id, mtr);
	}

	return(err);
}

/*********************************************************************//**
Returns TRUE if in a cascaded update/delete an ancestor node of node
updates (not DELETE, but UPDATE) table.
@return	TRUE if an ancestor updates table */
static
ibool
row_ins_cascade_ancestor_updates_table(
/*===================================*/
	que_node_t*	node,	/*!< in: node in a query graph */
	dict_table_t*	table)	/*!< in: table */
{
	que_node_t*	parent;

	for (parent = que_node_get_parent(node);
	     que_node_get_type(parent) == QUE_NODE_UPDATE;
	     parent = que_node_get_parent(parent)) {

		upd_node_t*	upd_node;

		upd_node = static_cast<upd_node_t*>(parent);

		if (upd_node->table == table && upd_node->is_delete == FALSE) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/*********************************************************************//**
Returns the number of ancestor UPDATE or DELETE nodes of a
cascaded update/delete node.
@return	number of ancestors */
static __attribute__((nonnull, warn_unused_result))
ulint
row_ins_cascade_n_ancestors(
/*========================*/
	que_node_t*	node)	/*!< in: node in a query graph */
{
	que_node_t*	parent;
	ulint		n_ancestors = 0;

	for (parent = que_node_get_parent(node);
	     que_node_get_type(parent) == QUE_NODE_UPDATE;
	     parent = que_node_get_parent(parent)) {

		n_ancestors++;
	}

	return(n_ancestors);
}

/******************************************************************//**
Calculates the update vector node->cascade->update for a child table in
a cascaded update.
@return number of fields in the calculated update vector; the value
can also be 0 if no foreign key fields changed; the returned value is
ULINT_UNDEFINED if the column type in the child table is too short to
fit the new value in the parent table: that means the update fails */
static __attribute__((nonnull, warn_unused_result))
ulint
row_ins_cascade_calc_update_vec(
/*============================*/
	upd_node_t*	node,		/*!< in: update node of the parent
					table */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint whose
					type is != 0 */
	mem_heap_t*	heap,		/*!< in: memory heap to use as
					temporary storage */
	trx_t*		trx,		/*!< in: update transaction */
	ibool*		fts_col_affected)/*!< out: is FTS column affected */
{
	upd_node_t*	cascade		= node->cascade_node;
	dict_table_t*	table		= foreign->foreign_table;
	dict_index_t*	index		= foreign->foreign_index;
	upd_t*		update;
	dict_table_t*	parent_table;
	dict_index_t*	parent_index;
	upd_t*		parent_update;
	ulint		n_fields_updated;
	ulint		parent_field_no;
	ulint		i;
	ulint		j;
	ibool		doc_id_updated = FALSE;
	ulint		doc_id_pos = 0;
	doc_id_t	new_doc_id = FTS_NULL_DOC_ID;

	ut_a(node);
	ut_a(foreign);
	ut_a(cascade);
	ut_a(table);
	ut_a(index);

	/* Calculate the appropriate update vector which will set the fields
	in the child index record to the same value (possibly padded with
	spaces if the column is a fixed length CHAR or FIXBINARY column) as
	the referenced index record will get in the update. */

	parent_table = node->table;
	ut_a(parent_table == foreign->referenced_table);
	parent_index = foreign->referenced_index;
	parent_update = node->update;

	update = cascade->update;

	update->info_bits = 0;
	update->n_fields = foreign->n_fields;

	n_fields_updated = 0;

	*fts_col_affected = FALSE;

	if (table->fts) {
		doc_id_pos = dict_table_get_nth_col_pos(
			table, table->fts->doc_col);
	}

	for (i = 0; i < foreign->n_fields; i++) {

		parent_field_no = dict_table_get_nth_col_pos(
			parent_table,
			dict_index_get_nth_col_no(parent_index, i));

		for (j = 0; j < parent_update->n_fields; j++) {
			const upd_field_t*	parent_ufield
				= &parent_update->fields[j];

			if (parent_ufield->field_no == parent_field_no) {

				ulint			min_size;
				const dict_col_t*	col;
				ulint			ufield_len;
				upd_field_t*		ufield;

				col = dict_index_get_nth_col(index, i);

				/* A field in the parent index record is
				updated. Let us make the update vector
				field for the child table. */

				ufield = update->fields + n_fields_updated;

				ufield->field_no
					= dict_table_get_nth_col_pos(
					table, dict_col_get_no(col));

				ufield->orig_len = 0;
				ufield->exp = NULL;

				ufield->new_val = parent_ufield->new_val;
				ufield_len = dfield_get_len(&ufield->new_val);

				/* Clear the "external storage" flag */
				dfield_set_len(&ufield->new_val, ufield_len);

				/* Do not allow a NOT NULL column to be
				updated as NULL */

				if (dfield_is_null(&ufield->new_val)
				    && (col->prtype & DATA_NOT_NULL)) {

					return(ULINT_UNDEFINED);
				}

				/* If the new value would not fit in the
				column, do not allow the update */

				if (!dfield_is_null(&ufield->new_val)
				    && dtype_get_at_most_n_mbchars(
					col->prtype, col->mbminmaxlen,
					col->len,
					ufield_len,
					static_cast<char*>(
						dfield_get_data(
							&ufield->new_val)))
				    < ufield_len) {

					return(ULINT_UNDEFINED);
				}

				/* If the parent column type has a different
				length than the child column type, we may
				need to pad with spaces the new value of the
				child column */

				min_size = dict_col_get_min_size(col);

				/* Because UNIV_SQL_NULL (the marker
				of SQL NULL values) exceeds all possible
				values of min_size, the test below will
				not hold for SQL NULL columns. */

				if (min_size > ufield_len) {

					byte*	pad;
					ulint	pad_len;
					byte*	padded_data;
					ulint	mbminlen;

					padded_data = static_cast<byte*>(
						mem_heap_alloc(
							heap, min_size));

					pad = padded_data + ufield_len;
					pad_len = min_size - ufield_len;

					memcpy(padded_data,
					       dfield_get_data(&ufield
							       ->new_val),
					       ufield_len);

					mbminlen = dict_col_get_mbminlen(col);

					ut_ad(!(ufield_len % mbminlen));
					ut_ad(!(min_size % mbminlen));

					if (mbminlen == 1
					    && dtype_get_charset_coll(
						    col->prtype)
					    == DATA_MYSQL_BINARY_CHARSET_COLL) {
						/* Do not pad BINARY columns */
						return(ULINT_UNDEFINED);
					}

					row_mysql_pad_col(mbminlen,
							  pad, pad_len);
					dfield_set_data(&ufield->new_val,
							padded_data, min_size);
				}

				/* Check whether the current column has
				FTS index on it */
				if (table->fts
				    && dict_table_is_fts_column(
					table->fts->indexes,
					dict_col_get_no(col))
					!= ULINT_UNDEFINED) {
					*fts_col_affected = TRUE;
				}

				/* If Doc ID is updated, check whether the
				Doc ID is valid */
				if (table->fts
				    && ufield->field_no == doc_id_pos) {
					doc_id_t	n_doc_id;

					n_doc_id =
						table->fts->cache->next_doc_id;

					new_doc_id = fts_read_doc_id(
						static_cast<const byte*>(
							dfield_get_data(
							&ufield->new_val)));

					if (new_doc_id <= 0) {
						fprintf(stderr,
							"InnoDB: FTS Doc ID "
							"must be larger than "
							"0 \n");
						return(ULINT_UNDEFINED);
					}

					if (new_doc_id < n_doc_id) {
						fprintf(stderr,
						       "InnoDB: FTS Doc ID "
						       "must be larger than "
						       IB_ID_FMT" for table",
						       n_doc_id -1);

						ut_print_name(stderr, trx,
							      TRUE,
							      table->name);

						putc('\n', stderr);
						return(ULINT_UNDEFINED);
					}

					*fts_col_affected = TRUE;
					doc_id_updated = TRUE;
				}

				n_fields_updated++;
			}
		}
	}

	/* Generate a new Doc ID if FTS index columns get updated */
	if (table->fts && *fts_col_affected) {
		if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
			doc_id_t	doc_id;
                        upd_field_t*	ufield;

			ut_ad(!doc_id_updated);
			ufield = update->fields + n_fields_updated;
			fts_get_next_doc_id(table, &trx->fts_next_doc_id);
			doc_id = fts_update_doc_id(table, ufield,
						   &trx->fts_next_doc_id);
			n_fields_updated++;
			fts_trx_add_op(trx, table, doc_id, FTS_INSERT, NULL);
		} else  {
			if (doc_id_updated) {
				ut_ad(new_doc_id);
				fts_trx_add_op(trx, table, new_doc_id,
					       FTS_INSERT, NULL);
			} else {
				fprintf(stderr, "InnoDB: FTS Doc ID must be "
					"updated along with FTS indexed "
					"column for table ");
				ut_print_name(stderr, trx, TRUE, table->name);
				putc('\n', stderr);
				return(ULINT_UNDEFINED);
			}
		}
	}

	update->n_fields = n_fields_updated;

	return(n_fields_updated);
}

/*********************************************************************//**
Set detailed error message associated with foreign key errors for
the given transaction. */
static
void
row_ins_set_detailed(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_foreign_t*	foreign)	/*!< in: foreign key constraint */
{
	ut_ad(!srv_read_only_mode);

	mutex_enter(&srv_misc_tmpfile_mutex);
	rewind(srv_misc_tmpfile);

	if (os_file_set_eof(srv_misc_tmpfile)) {
		ut_print_name(srv_misc_tmpfile, trx, TRUE,
			      foreign->foreign_table_name);
		dict_print_info_on_foreign_key_in_create_format(
			srv_misc_tmpfile, trx, foreign, FALSE);
		trx_set_detailed_error_from_file(trx, srv_misc_tmpfile);
	} else {
		trx_set_detailed_error(trx, "temp file operation failed");
	}

	mutex_exit(&srv_misc_tmpfile_mutex);
}

/*********************************************************************//**
Acquires dict_foreign_err_mutex, rewinds dict_foreign_err_file
and displays information about the given transaction.
The caller must release dict_foreign_err_mutex. */
static
void
row_ins_foreign_trx_print(
/*======================*/
	trx_t*	trx)	/*!< in: transaction */
{
	ulint	n_rec_locks;
	ulint	n_trx_locks;
	ulint	heap_size;

	if (srv_read_only_mode) {
		return;
	}

	lock_mutex_enter();
	n_rec_locks = lock_number_of_rows_locked(&trx->lock);
	n_trx_locks = UT_LIST_GET_LEN(trx->lock.trx_locks);
	heap_size = mem_heap_get_size(trx->lock.lock_heap);
	lock_mutex_exit();

	trx_sys_mutex_enter();

	mutex_enter(&dict_foreign_err_mutex);
	rewind(dict_foreign_err_file);
	ut_print_timestamp(dict_foreign_err_file);
	fputs(" Transaction:\n", dict_foreign_err_file);

	trx_print_low(dict_foreign_err_file, trx, 600,
		      n_rec_locks, n_trx_locks, heap_size);

	trx_sys_mutex_exit();

	ut_ad(mutex_own(&dict_foreign_err_mutex));
}

/*********************************************************************//**
Reports a foreign key error associated with an update or a delete of a
parent table index entry. */
static
void
row_ins_foreign_report_err(
/*=======================*/
	const char*	errstr,		/*!< in: error string from the viewpoint
					of the parent table */
	que_thr_t*	thr,		/*!< in: query thread whose run_node
					is an update node */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint */
	const rec_t*	rec,		/*!< in: a matching index record in the
					child table */
	const dtuple_t*	entry)		/*!< in: index entry in the parent
					table */
{
	if (srv_read_only_mode) {
		return;
	}

	FILE*	ef	= dict_foreign_err_file;
	trx_t*	trx	= thr_get_trx(thr);

	row_ins_set_detailed(trx, foreign);

	row_ins_foreign_trx_print(trx);

	fputs("Foreign key constraint fails for table ", ef);
	ut_print_name(ef, trx, TRUE, foreign->foreign_table_name);
	fputs(":\n", ef);
	dict_print_info_on_foreign_key_in_create_format(ef, trx, foreign,
							TRUE);
	putc('\n', ef);
	fputs(errstr, ef);
	fputs(" in parent table, in index ", ef);
	ut_print_name(ef, trx, FALSE, foreign->referenced_index->name);
	if (entry) {
		fputs(" tuple:\n", ef);
		dtuple_print(ef, entry);
	}
	fputs("\nBut in child table ", ef);
	ut_print_name(ef, trx, TRUE, foreign->foreign_table_name);
	fputs(", in index ", ef);
	ut_print_name(ef, trx, FALSE, foreign->foreign_index->name);
	if (rec) {
		fputs(", there is a record:\n", ef);
		rec_print(ef, rec, foreign->foreign_index);
	} else {
		fputs(", the record is not available\n", ef);
	}
	putc('\n', ef);

	mutex_exit(&dict_foreign_err_mutex);
}

/*********************************************************************//**
Reports a foreign key error to dict_foreign_err_file when we are trying
to add an index entry to a child table. Note that the adding may be the result
of an update, too. */
static
void
row_ins_foreign_report_add_err(
/*===========================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint */
	const rec_t*	rec,		/*!< in: a record in the parent table:
					it does not match entry because we
					have an error! */
	const dtuple_t*	entry)		/*!< in: index entry to insert in the
					child table */
{
	if (srv_read_only_mode) {
		return;
	}

	FILE*	ef	= dict_foreign_err_file;

	row_ins_set_detailed(trx, foreign);

	row_ins_foreign_trx_print(trx);

	fputs("Foreign key constraint fails for table ", ef);
	ut_print_name(ef, trx, TRUE, foreign->foreign_table_name);
	fputs(":\n", ef);
	dict_print_info_on_foreign_key_in_create_format(ef, trx, foreign,
							TRUE);
	fputs("\nTrying to add in child table, in index ", ef);
	ut_print_name(ef, trx, FALSE, foreign->foreign_index->name);
	if (entry) {
		fputs(" tuple:\n", ef);
		/* TODO: DB_TRX_ID and DB_ROLL_PTR may be uninitialized.
		It would be better to only display the user columns. */
		dtuple_print(ef, entry);
	}
	fputs("\nBut in parent table ", ef);
	ut_print_name(ef, trx, TRUE, foreign->referenced_table_name);
	fputs(", in index ", ef);
	ut_print_name(ef, trx, FALSE, foreign->referenced_index->name);
	fputs(",\nthe closest match we can find is record:\n", ef);
	if (rec && page_rec_is_supremum(rec)) {
		/* If the cursor ended on a supremum record, it is better
		to report the previous record in the error message, so that
		the user gets a more descriptive error message. */
		rec = page_rec_get_prev_const(rec);
	}

	if (rec) {
		rec_print(ef, rec, foreign->referenced_index);
	}
	putc('\n', ef);

	mutex_exit(&dict_foreign_err_mutex);
}

/*********************************************************************//**
Invalidate the query cache for the given table. */
static
void
row_ins_invalidate_query_cache(
/*===========================*/
	que_thr_t*	thr,		/*!< in: query thread whose run_node
					is an update node */
	const char*	name)		/*!< in: table name prefixed with
					database name and a '/' character */
{
	char*	buf;
	char*	ptr;
	ulint	len = strlen(name) + 1;

	buf = mem_strdupl(name, len);

	ptr = strchr(buf, '/');
	ut_a(ptr);
	*ptr = '\0';

	innobase_invalidate_query_cache(thr_get_trx(thr), buf, len);
	mem_free(buf);
}

/*********************************************************************//**
Perform referential actions or checks when a parent row is deleted or updated
and the constraint had an ON DELETE or ON UPDATE condition which was not
RESTRICT.
@return	DB_SUCCESS, DB_LOCK_WAIT, or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_foreign_check_on_constraint(
/*================================*/
	que_thr_t*	thr,		/*!< in: query thread whose run_node
					is an update node */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint whose
					type is != 0 */
	btr_pcur_t*	pcur,		/*!< in: cursor placed on a matching
					index record in the child table */
	dtuple_t*	entry,		/*!< in: index entry in the parent
					table */
	mtr_t*		mtr)		/*!< in: mtr holding the latch of pcur
					page */
{
	upd_node_t*	node;
	upd_node_t*	cascade;
	dict_table_t*	table		= foreign->foreign_table;
	dict_index_t*	index;
	dict_index_t*	clust_index;
	dtuple_t*	ref;
	mem_heap_t*	upd_vec_heap	= NULL;
	const rec_t*	rec;
	const rec_t*	clust_rec;
	const buf_block_t* clust_block;
	upd_t*		update;
	ulint		n_to_update;
	dberr_t		err;
	ulint		i;
	trx_t*		trx;
	mem_heap_t*	tmp_heap	= NULL;
	doc_id_t	doc_id = FTS_NULL_DOC_ID;
	ibool		fts_col_affacted = FALSE;

	ut_a(thr);
	ut_a(foreign);
	ut_a(pcur);
	ut_a(mtr);

	trx = thr_get_trx(thr);

	/* Since we are going to delete or update a row, we have to invalidate
	the MySQL query cache for table. A deadlock of threads is not possible
	here because the caller of this function does not hold any latches with
	the sync0mutex.h rank above the lock_sys_t::mutex. The query cache mutex
       	has a rank just above the lock_sys_t::mutex. */

	row_ins_invalidate_query_cache(thr, table->name);

	node = static_cast<upd_node_t*>(thr->run_node);

	if (node->is_delete && 0 == (foreign->type
				     & (DICT_FOREIGN_ON_DELETE_CASCADE
					| DICT_FOREIGN_ON_DELETE_SET_NULL))) {

		row_ins_foreign_report_err("Trying to delete",
					   thr, foreign,
					   btr_pcur_get_rec(pcur), entry);

		return(DB_ROW_IS_REFERENCED);
	}

	if (!node->is_delete && 0 == (foreign->type
				      & (DICT_FOREIGN_ON_UPDATE_CASCADE
					 | DICT_FOREIGN_ON_UPDATE_SET_NULL))) {

		/* This is an UPDATE */

		row_ins_foreign_report_err("Trying to update",
					   thr, foreign,
					   btr_pcur_get_rec(pcur), entry);

		return(DB_ROW_IS_REFERENCED);
	}

	if (node->cascade_node == NULL) {
		/* Extend our query graph by creating a child to current
		update node. The child is used in the cascade or set null
		operation. */

		node->cascade_heap = mem_heap_create(128);
		node->cascade_node = row_create_update_node_for_mysql(
			table, node->cascade_heap);
		que_node_set_parent(node->cascade_node, node);
	}

	/* Initialize cascade_node to do the operation we want. Note that we
	use the SAME cascade node to do all foreign key operations of the
	SQL DELETE: the table of the cascade node may change if there are
	several child tables to the table where the delete is done! */

	cascade = node->cascade_node;

	cascade->table = table;

	cascade->foreign = foreign;

	if (node->is_delete
	    && (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE)) {
		cascade->is_delete = TRUE;
	} else {
		cascade->is_delete = FALSE;

		if (foreign->n_fields > cascade->update_n_fields) {
			/* We have to make the update vector longer */

			cascade->update = upd_create(foreign->n_fields,
						     node->cascade_heap);
			cascade->update_n_fields = foreign->n_fields;
		}
	}

	/* We do not allow cyclic cascaded updating (DELETE is allowed,
	but not UPDATE) of the same table, as this can lead to an infinite
	cycle. Check that we are not updating the same table which is
	already being modified in this cascade chain. We have to check
	this also because the modification of the indexes of a 'parent'
	table may still be incomplete, and we must avoid seeing the indexes
	of the parent table in an inconsistent state! */

	if (!cascade->is_delete
	    && row_ins_cascade_ancestor_updates_table(cascade, table)) {

		/* We do not know if this would break foreign key
		constraints, but play safe and return an error */

		err = DB_ROW_IS_REFERENCED;

		row_ins_foreign_report_err(
			"Trying an update, possibly causing a cyclic"
			" cascaded update\n"
			"in the child table,", thr, foreign,
			btr_pcur_get_rec(pcur), entry);

		goto nonstandard_exit_func;
	}

	if (row_ins_cascade_n_ancestors(cascade) >= 15) {
		err = DB_ROW_IS_REFERENCED;

		row_ins_foreign_report_err(
			"Trying a too deep cascaded delete or update\n",
			thr, foreign, btr_pcur_get_rec(pcur), entry);

		goto nonstandard_exit_func;
	}

	index = btr_pcur_get_btr_cur(pcur)->index;

	ut_a(index == foreign->foreign_index);

	rec = btr_pcur_get_rec(pcur);

	tmp_heap = mem_heap_create(256);

	if (dict_index_is_clust(index)) {
		/* pcur is already positioned in the clustered index of
		the child table */

		clust_index = index;
		clust_rec = rec;
		clust_block = btr_pcur_get_block(pcur);
	} else {
		/* We have to look for the record in the clustered index
		in the child table */

		clust_index = dict_table_get_first_index(table);

		ref = row_build_row_ref(ROW_COPY_POINTERS, index, rec,
					tmp_heap);
		btr_pcur_open_with_no_init(clust_index, ref,
					   PAGE_CUR_LE, BTR_SEARCH_LEAF,
					   cascade->pcur, 0, mtr);

		clust_rec = btr_pcur_get_rec(cascade->pcur);
		clust_block = btr_pcur_get_block(cascade->pcur);

		if (!page_rec_is_user_rec(clust_rec)
		    || btr_pcur_get_low_match(cascade->pcur)
		    < dict_index_get_n_unique(clust_index)) {

			fputs("InnoDB: error in cascade of a foreign key op\n"
			      "InnoDB: ", stderr);
			dict_index_name_print(stderr, trx, index);

			fputs("\n"
			      "InnoDB: record ", stderr);
			rec_print(stderr, rec, index);
			fputs("\n"
			      "InnoDB: clustered record ", stderr);
			rec_print(stderr, clust_rec, clust_index);
			fputs("\n"
			      "InnoDB: Submit a detailed bug report to"
			      " http://bugs.mysql.com\n", stderr);
			ut_ad(0);
			err = DB_SUCCESS;

			goto nonstandard_exit_func;
		}
	}

	/* Set an X-lock on the row to delete or update in the child table */

	err = lock_table(0, table, LOCK_IX, thr);

	if (err == DB_SUCCESS) {
		/* Here it suffices to use a LOCK_REC_NOT_GAP type lock;
		we already have a normal shared lock on the appropriate
		gap if the search criterion was not unique */

		err = lock_clust_rec_read_check_and_lock_alt(
			0, clust_block, clust_rec, clust_index,
			LOCK_X, LOCK_REC_NOT_GAP, thr);
	}

	if (err != DB_SUCCESS) {

		goto nonstandard_exit_func;
	}

	if (rec_get_deleted_flag(clust_rec, dict_table_is_comp(table))) {
		/* This can happen if there is a circular reference of
		rows such that cascading delete comes to delete a row
		already in the process of being delete marked */
		err = DB_SUCCESS;

		goto nonstandard_exit_func;
	}

	if (table->fts) {
		doc_id = fts_get_doc_id_from_rec(table, clust_rec, tmp_heap);
	}

	if (node->is_delete
	    ? (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL)
	    : (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL)) {

		/* Build the appropriate update vector which sets
		foreign->n_fields first fields in rec to SQL NULL */

		update = cascade->update;

		update->info_bits = 0;
		update->n_fields = foreign->n_fields;
		UNIV_MEM_INVALID(update->fields,
				 update->n_fields * sizeof *update->fields);

		for (i = 0; i < foreign->n_fields; i++) {
			upd_field_t*	ufield = &update->fields[i];

			ufield->field_no = dict_table_get_nth_col_pos(
				table,
				dict_index_get_nth_col_no(index, i));
			ufield->orig_len = 0;
			ufield->exp = NULL;
			dfield_set_null(&ufield->new_val);

			if (table->fts && dict_table_is_fts_column(
				table->fts->indexes,
				dict_index_get_nth_col_no(index, i))
				!= ULINT_UNDEFINED) {
				fts_col_affacted = TRUE;
			}
		}

		if (fts_col_affacted) {
			fts_trx_add_op(trx, table, doc_id, FTS_DELETE, NULL);
		}
	} else if (table->fts && cascade->is_delete) {
		/* DICT_FOREIGN_ON_DELETE_CASCADE case */
		for (i = 0; i < foreign->n_fields; i++) {
			if (table->fts && dict_table_is_fts_column(
				table->fts->indexes,
				dict_index_get_nth_col_no(index, i))
				!= ULINT_UNDEFINED) {
				fts_col_affacted = TRUE;
			}
		}

		if (fts_col_affacted) {
			fts_trx_add_op(trx, table, doc_id, FTS_DELETE, NULL);
		}
	}

	if (!node->is_delete
	    && (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE)) {

		/* Build the appropriate update vector which sets changing
		foreign->n_fields first fields in rec to new values */

		upd_vec_heap = mem_heap_create(256);

		n_to_update = row_ins_cascade_calc_update_vec(
			node, foreign, upd_vec_heap, trx, &fts_col_affacted);

		if (n_to_update == ULINT_UNDEFINED) {
			err = DB_ROW_IS_REFERENCED;

			row_ins_foreign_report_err(
				"Trying a cascaded update where the"
				" updated value in the child\n"
				"table would not fit in the length"
				" of the column, or the value would\n"
				"be NULL and the column is"
				" declared as not NULL in the child table,",
				thr, foreign, btr_pcur_get_rec(pcur), entry);

			goto nonstandard_exit_func;
		}

		if (cascade->update->n_fields == 0) {

			/* The update does not change any columns referred
			to in this foreign key constraint: no need to do
			anything */

			err = DB_SUCCESS;

			goto nonstandard_exit_func;
		}

		/* Mark the old Doc ID as deleted */
		if (fts_col_affacted) {
			ut_ad(table->fts);
			fts_trx_add_op(trx, table, doc_id, FTS_DELETE, NULL);
		}
	}

	/* Store pcur position and initialize or store the cascade node
	pcur stored position */

	btr_pcur_store_position(pcur, mtr);

	if (index == clust_index) {
		btr_pcur_copy_stored_position(cascade->pcur, pcur);
	} else {
		btr_pcur_store_position(cascade->pcur, mtr);
	}

	mtr_commit(mtr);

	ut_a(cascade->pcur->rel_pos == BTR_PCUR_ON);

	cascade->state = UPD_NODE_UPDATE_CLUSTERED;

	err = row_update_cascade_for_mysql(thr, cascade,
					   foreign->foreign_table);

	if (foreign->foreign_table->n_foreign_key_checks_running == 0) {
		fprintf(stderr,
			"InnoDB: error: table %s has the counter 0"
			" though there is\n"
			"InnoDB: a FOREIGN KEY check running on it.\n",
			foreign->foreign_table->name);
	}

	/* Release the data dictionary latch for a while, so that we do not
	starve other threads from doing CREATE TABLE etc. if we have a huge
	cascaded operation running. The counter n_foreign_key_checks_running
	will prevent other users from dropping or ALTERing the table when we
	release the latch. */

	row_mysql_unfreeze_data_dictionary(thr_get_trx(thr));

	DEBUG_SYNC_C("innodb_dml_cascade_dict_unfreeze");

	row_mysql_freeze_data_dictionary(thr_get_trx(thr));

	mtr_start(mtr);

	/* Restore pcur position */

	btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	if (upd_vec_heap) {
		mem_heap_free(upd_vec_heap);
	}

	return(err);

nonstandard_exit_func:
	if (tmp_heap) {
		mem_heap_free(tmp_heap);
	}

	if (upd_vec_heap) {
		mem_heap_free(upd_vec_heap);
	}

	btr_pcur_store_position(pcur, mtr);

	mtr_commit(mtr);
	mtr_start(mtr);

	btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

	return(err);
}

/*********************************************************************//**
Sets a shared lock on a record. Used in locking possible duplicate key
records and also in checking foreign key constraints.
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, or error code */
static
dberr_t
row_ins_set_shared_rec_lock(
/*========================*/
	ulint			type,	/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP type lock */
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: record */
	dict_index_t*		index,	/*!< in: index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	que_thr_t*		thr)	/*!< in: query thread */
{
	dberr_t	err;

	ut_ad(rec_offs_validate(rec, index, offsets));

	if (dict_index_is_clust(index)) {
		err = lock_clust_rec_read_check_and_lock(
			0, block, rec, index, offsets, LOCK_S, type, thr);
	} else {
		err = lock_sec_rec_read_check_and_lock(
			0, block, rec, index, offsets, LOCK_S, type, thr);
	}

	return(err);
}

/*********************************************************************//**
Sets a exclusive lock on a record. Used in locking possible duplicate key
records
@return	DB_SUCCESS, DB_SUCCESS_LOCKED_REC, or error code */
static
dberr_t
row_ins_set_exclusive_rec_lock(
/*===========================*/
	ulint			type,	/*!< in: LOCK_ORDINARY, LOCK_GAP, or
					LOCK_REC_NOT_GAP type lock */
	const buf_block_t*	block,	/*!< in: buffer block of rec */
	const rec_t*		rec,	/*!< in: record */
	dict_index_t*		index,	/*!< in: index */
	const ulint*		offsets,/*!< in: rec_get_offsets(rec, index) */
	que_thr_t*		thr)	/*!< in: query thread */
{
	dberr_t	err;

	ut_ad(rec_offs_validate(rec, index, offsets));

	if (dict_index_is_clust(index)) {
		err = lock_clust_rec_read_check_and_lock(
			0, block, rec, index, offsets, LOCK_X, type, thr);
	} else {
		err = lock_sec_rec_read_check_and_lock(
			0, block, rec, index, offsets, LOCK_X, type, thr);
	}

	return(err);
}

/***************************************************************//**
Checks if foreign key constraint fails for an index entry. Sets shared locks
which lock either the success or the failure of the constraint. NOTE that
the caller must have a shared latch on dict_operation_lock.
@return	DB_SUCCESS, DB_NO_REFERENCED_ROW, or DB_ROW_IS_REFERENCED */
UNIV_INTERN
dberr_t
row_ins_check_foreign_constraint(
/*=============================*/
	ibool		check_ref,/*!< in: TRUE if we want to check that
				the referenced table is ok, FALSE if we
				want to check the foreign key table */
	dict_foreign_t*	foreign,/*!< in: foreign constraint; NOTE that the
				tables mentioned in it must be in the
				dictionary cache if they exist at all */
	dict_table_t*	table,	/*!< in: if check_ref is TRUE, then the foreign
				table, else the referenced table */
	dtuple_t*	entry,	/*!< in: index entry for index */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t		err;
	upd_node_t*	upd_node;
	dict_table_t*	check_table;
	dict_index_t*	check_index;
	ulint		n_fields_cmp;
	btr_pcur_t	pcur;
	int		cmp;
	ulint		i;
	mtr_t		mtr;
	trx_t*		trx		= thr_get_trx(thr);
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

run_again:
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_S));
#endif /* UNIV_SYNC_DEBUG */

	err = DB_SUCCESS;

	if (trx->check_foreigns == FALSE) {
		/* The user has suppressed foreign key checks currently for
		this session */
		goto exit_func;
	}

	/* If any of the foreign key fields in entry is SQL NULL, we
	suppress the foreign key check: this is compatible with Oracle,
	for example */

	for (i = 0; i < foreign->n_fields; i++) {
		if (UNIV_SQL_NULL == dfield_get_len(
			    dtuple_get_nth_field(entry, i))) {

			goto exit_func;
		}
	}

	if (que_node_get_type(thr->run_node) == QUE_NODE_UPDATE) {
		upd_node = static_cast<upd_node_t*>(thr->run_node);

		if (!(upd_node->is_delete) && upd_node->foreign == foreign) {
			/* If a cascaded update is done as defined by a
			foreign key constraint, do not check that
			constraint for the child row. In ON UPDATE CASCADE
			the update of the parent row is only half done when
			we come here: if we would check the constraint here
			for the child row it would fail.

			A QUESTION remains: if in the child table there are
			several constraints which refer to the same parent
			table, we should merge all updates to the child as
			one update? And the updates can be contradictory!
			Currently we just perform the update associated
			with each foreign key constraint, one after
			another, and the user has problems predicting in
			which order they are performed. */

			goto exit_func;
		}
	}

	if (check_ref) {
		check_table = foreign->referenced_table;
		check_index = foreign->referenced_index;
	} else {
		check_table = foreign->foreign_table;
		check_index = foreign->foreign_index;
	}

	if (check_table == NULL
	    || check_table->ibd_file_missing
	    || check_index == NULL) {

		if (!srv_read_only_mode && check_ref) {
			FILE*	ef = dict_foreign_err_file;

			row_ins_set_detailed(trx, foreign);

			row_ins_foreign_trx_print(trx);

			fputs("Foreign key constraint fails for table ", ef);
			ut_print_name(ef, trx, TRUE,
				      foreign->foreign_table_name);
			fputs(":\n", ef);
			dict_print_info_on_foreign_key_in_create_format(
				ef, trx, foreign, TRUE);
			fputs("\nTrying to add to index ", ef);
			ut_print_name(ef, trx, FALSE,
				      foreign->foreign_index->name);
			fputs(" tuple:\n", ef);
			dtuple_print(ef, entry);
			fputs("\nBut the parent table ", ef);
			ut_print_name(ef, trx, TRUE,
				      foreign->referenced_table_name);
			fputs("\nor its .ibd file does"
			      " not currently exist!\n", ef);
			mutex_exit(&dict_foreign_err_mutex);

			err = DB_NO_REFERENCED_ROW;
		}

		goto exit_func;
	}

	if (check_table != table) {
		/* We already have a LOCK_IX on table, but not necessarily
		on check_table */

		err = lock_table(0, check_table, LOCK_IS, thr);

		if (err != DB_SUCCESS) {

			goto do_possible_lock_wait;
		}
	}

	mtr_start(&mtr);

	/* Store old value on n_fields_cmp */

	n_fields_cmp = dtuple_get_n_fields_cmp(entry);

	dtuple_set_n_fields_cmp(entry, foreign->n_fields);

	btr_pcur_open(check_index, entry, PAGE_CUR_GE,
		      BTR_SEARCH_LEAF, &pcur, &mtr);

	/* Scan index records and check if there is a matching record */

	do {
		const rec_t*		rec = btr_pcur_get_rec(&pcur);
		const buf_block_t*	block = btr_pcur_get_block(&pcur);

		if (page_rec_is_infimum(rec)) {

			continue;
		}

		offsets = rec_get_offsets(rec, check_index,
					  offsets, ULINT_UNDEFINED, &heap);

		if (page_rec_is_supremum(rec)) {

			err = row_ins_set_shared_rec_lock(LOCK_ORDINARY, block,
							  rec, check_index,
							  offsets, thr);
			switch (err) {
			case DB_SUCCESS_LOCKED_REC:
			case DB_SUCCESS:
				continue;
			default:
				goto end_scan;
			}
		}

		cmp = cmp_dtuple_rec(entry, rec, offsets);

		if (cmp == 0) {
			if (rec_get_deleted_flag(rec,
						 rec_offs_comp(offsets))) {
				err = row_ins_set_shared_rec_lock(
					LOCK_ORDINARY, block,
					rec, check_index, offsets, thr);
				switch (err) {
				case DB_SUCCESS_LOCKED_REC:
				case DB_SUCCESS:
					break;
				default:
					goto end_scan;
				}
			} else {
				/* Found a matching record. Lock only
				a record because we can allow inserts
				into gaps */

				err = row_ins_set_shared_rec_lock(
					LOCK_REC_NOT_GAP, block,
					rec, check_index, offsets, thr);

				switch (err) {
				case DB_SUCCESS_LOCKED_REC:
				case DB_SUCCESS:
					break;
				default:
					goto end_scan;
				}

				if (check_ref) {
					err = DB_SUCCESS;

					goto end_scan;
				} else if (foreign->type != 0) {
					/* There is an ON UPDATE or ON DELETE
					condition: check them in a separate
					function */

					err = row_ins_foreign_check_on_constraint(
						thr, foreign, &pcur, entry,
						&mtr);
					if (err != DB_SUCCESS) {
						/* Since reporting a plain
						"duplicate key" error
						message to the user in
						cases where a long CASCADE
						operation would lead to a
						duplicate key in some
						other table is very
						confusing, map duplicate
						key errors resulting from
						FK constraints to a
						separate error code. */

						if (err == DB_DUPLICATE_KEY) {
							err = DB_FOREIGN_DUPLICATE_KEY;
						}

						goto end_scan;
					}

					/* row_ins_foreign_check_on_constraint
					may have repositioned pcur on a
					different block */
					block = btr_pcur_get_block(&pcur);
				} else {
					row_ins_foreign_report_err(
						"Trying to delete or update",
						thr, foreign, rec, entry);

					err = DB_ROW_IS_REFERENCED;
					goto end_scan;
				}
			}
		} else {
			ut_a(cmp < 0);

			err = row_ins_set_shared_rec_lock(
				LOCK_GAP, block,
				rec, check_index, offsets, thr);

			switch (err) {
			case DB_SUCCESS_LOCKED_REC:
			case DB_SUCCESS:
				if (check_ref) {
					err = DB_NO_REFERENCED_ROW;
					row_ins_foreign_report_add_err(
						trx, foreign, rec, entry);
				} else {
					err = DB_SUCCESS;
				}
			default:
				break;
			}

			goto end_scan;
		}
	} while (btr_pcur_move_to_next(&pcur, &mtr));

	if (check_ref) {
		row_ins_foreign_report_add_err(
			trx, foreign, btr_pcur_get_rec(&pcur), entry);
		err = DB_NO_REFERENCED_ROW;
	} else {
		err = DB_SUCCESS;
	}

end_scan:
	btr_pcur_close(&pcur);

	mtr_commit(&mtr);

	/* Restore old value */
	dtuple_set_n_fields_cmp(entry, n_fields_cmp);

do_possible_lock_wait:
	if (err == DB_LOCK_WAIT) {
		bool		verified = false;

		trx->error_state = err;

		que_thr_stop_for_mysql(thr);

		lock_wait_suspend_thread(thr);

		if (check_table->to_be_dropped) {
			/* The table is being dropped. We shall timeout
			this operation */
			err = DB_LOCK_WAIT_TIMEOUT;
			goto exit_func;
		}

		/* We had temporarily released dict_operation_lock in
		above lock sleep wait, now we have the lock again, and
		we will need to re-check whether the foreign key has been
		dropped */
		for (const dict_foreign_t* check_foreign = UT_LIST_GET_FIRST(
			table->referenced_list);
		     check_foreign;
		     check_foreign = UT_LIST_GET_NEXT(
                                referenced_list, check_foreign)) {
			if (check_foreign == foreign) {
				verified = true;
			}
		}

		if (!verified) {
			err = DB_DICT_CHANGED;
		} else if (trx->error_state == DB_SUCCESS) {
			goto run_again;
		} else {
			err = trx->error_state;
		}
	}

exit_func:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
}

/***************************************************************//**
Checks if foreign key constraints fail for an index entry. If index
is not mentioned in any constraint, this function does nothing,
Otherwise does searches to the indexes of referenced tables and
sets shared locks which lock either the success or the failure of
a constraint.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_check_foreign_constraints(
/*==============================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry for index */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dict_foreign_t*	foreign;
	dberr_t		err;
	trx_t*		trx;
	ibool		got_s_lock	= FALSE;

	trx = thr_get_trx(thr);

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	DEBUG_SYNC_C_IF_THD(thr_get_trx(thr)->mysql_thd,
			    "foreign_constraint_check_for_ins");

	while (foreign) {
		if (foreign->foreign_index == index) {
			dict_table_t*	ref_table = NULL;
			dict_table_t*	foreign_table = foreign->foreign_table;
			dict_table_t*	referenced_table
						= foreign->referenced_table;

			if (referenced_table == NULL) {

				ref_table = dict_table_open_on_name(
					foreign->referenced_table_name_lookup,
					FALSE, FALSE, DICT_ERR_IGNORE_NONE);
			}

			if (0 == trx->dict_operation_lock_mode) {
				got_s_lock = TRUE;

				row_mysql_freeze_data_dictionary(trx);
			}

			if (referenced_table) {
				os_inc_counter(dict_sys->mutex,
					       foreign_table
					       ->n_foreign_key_checks_running);
			}

			/* NOTE that if the thread ends up waiting for a lock
			we will release dict_operation_lock temporarily!
			But the counter on the table protects the referenced
			table from being dropped while the check is running. */

			err = row_ins_check_foreign_constraint(
				TRUE, foreign, table, entry, thr);

			DBUG_EXECUTE_IF("row_ins_dict_change_err",
					err = DB_DICT_CHANGED;);

			if (referenced_table) {
				os_dec_counter(dict_sys->mutex,
					       foreign_table
					       ->n_foreign_key_checks_running);
			}

			if (got_s_lock) {
				row_mysql_unfreeze_data_dictionary(trx);
			}

			if (ref_table != NULL) {
				dict_table_close(ref_table, FALSE, FALSE);
			}

			if (err != DB_SUCCESS) {

				return(err);
			}
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	return(DB_SUCCESS);
}

/***************************************************************//**
Checks if a unique key violation to rec would occur at the index entry
insert.
@return	TRUE if error */
static
ibool
row_ins_dupl_error_with_rec(
/*========================*/
	const rec_t*	rec,	/*!< in: user record; NOTE that we assume
				that the caller already has a record lock on
				the record! */
	const dtuple_t*	entry,	/*!< in: entry to insert */
	dict_index_t*	index,	/*!< in: index */
	const ulint*	offsets)/*!< in: rec_get_offsets(rec, index) */
{
	ulint	matched_fields;
	ulint	matched_bytes;
	ulint	n_unique;
	ulint	i;

	ut_ad(rec_offs_validate(rec, index, offsets));

	n_unique = dict_index_get_n_unique(index);

	matched_fields = 0;
	matched_bytes = 0;

	cmp_dtuple_rec_with_match(entry, rec, offsets,
				  &matched_fields, &matched_bytes);

	if (matched_fields < n_unique) {

		return(FALSE);
	}

	/* In a unique secondary index we allow equal key values if they
	contain SQL NULLs */

	if (!dict_index_is_clust(index)) {

		for (i = 0; i < n_unique; i++) {
			if (dfield_is_null(dtuple_get_nth_field(entry, i))) {

				return(FALSE);
			}
		}
	}

	return(!rec_get_deleted_flag(rec, rec_offs_comp(offsets)));
}

/***************************************************************//**
Scans a unique non-clustered index at a given index entry to determine
whether a uniqueness violation has occurred for the key value of the entry.
Set shared locks on possible duplicate records.
@return	DB_SUCCESS, DB_DUPLICATE_KEY, or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_scan_sec_index_for_duplicate(
/*=================================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	dict_index_t*	index,	/*!< in: non-clustered unique index */
	dtuple_t*	entry,	/*!< in: index entry */
	que_thr_t*	thr,	/*!< in: query thread */
	bool		s_latch,/*!< in: whether index->lock is being held */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	mem_heap_t*	offsets_heap)
				/*!< in/out: memory heap that can be emptied */
{
	ulint		n_unique;
	int		cmp;
	ulint		n_fields_cmp;
	btr_pcur_t	pcur;
	dberr_t		err		= DB_SUCCESS;
	ulint		allow_duplicates;
	ulint*		offsets		= NULL;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(s_latch == rw_lock_own(&index->lock, RW_LOCK_S));
#endif /* UNIV_SYNC_DEBUG */

	n_unique = dict_index_get_n_unique(index);

	/* If the secondary index is unique, but one of the fields in the
	n_unique first fields is NULL, a unique key violation cannot occur,
	since we define NULL != NULL in this case */

	for (ulint i = 0; i < n_unique; i++) {
		if (UNIV_SQL_NULL == dfield_get_len(
			    dtuple_get_nth_field(entry, i))) {

			return(DB_SUCCESS);
		}
	}

	/* Store old value on n_fields_cmp */

	n_fields_cmp = dtuple_get_n_fields_cmp(entry);

	dtuple_set_n_fields_cmp(entry, n_unique);

	btr_pcur_open(index, entry, PAGE_CUR_GE,
		      s_latch
		      ? BTR_SEARCH_LEAF | BTR_ALREADY_S_LATCHED
		      : BTR_SEARCH_LEAF,
		      &pcur, mtr);

	allow_duplicates = thr_get_trx(thr)->duplicates;

	/* Scan index records and check if there is a duplicate */

	do {
		const rec_t*		rec	= btr_pcur_get_rec(&pcur);
		const buf_block_t*	block	= btr_pcur_get_block(&pcur);

		if (page_rec_is_infimum(rec)) {

			continue;
		}

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &offsets_heap);

		if (flags & BTR_NO_LOCKING_FLAG) {
			/* Set no locks when applying log
			in online table rebuild. */
		} else if (allow_duplicates) {

			/* If the SQL-query will update or replace
			duplicate key we will take X-lock for
			duplicates ( REPLACE, LOAD DATAFILE REPLACE,
			INSERT ON DUPLICATE KEY UPDATE). */

			err = row_ins_set_exclusive_rec_lock(
				LOCK_ORDINARY, block,
				rec, index, offsets, thr);
		} else {

			err = row_ins_set_shared_rec_lock(
				LOCK_ORDINARY, block,
				rec, index, offsets, thr);
		}

		switch (err) {
		case DB_SUCCESS_LOCKED_REC:
			err = DB_SUCCESS;
		case DB_SUCCESS:
			break;
		default:
			goto end_scan;
		}

		if (page_rec_is_supremum(rec)) {

			continue;
		}

		cmp = cmp_dtuple_rec(entry, rec, offsets);

		if (cmp == 0) {
			if (row_ins_dupl_error_with_rec(rec, entry,
							index, offsets)) {
				err = DB_DUPLICATE_KEY;

				thr_get_trx(thr)->error_info = index;

				goto end_scan;
			}
		} else {
			ut_a(cmp < 0);
			goto end_scan;
		}
	} while (btr_pcur_move_to_next(&pcur, mtr));

end_scan:
	/* Restore old value */
	dtuple_set_n_fields_cmp(entry, n_fields_cmp);

	return(err);
}

/** Checks for a duplicate when the table is being rebuilt online.
@retval DB_SUCCESS		when no duplicate is detected
@retval DB_SUCCESS_LOCKED_REC	when rec is an exact match of entry or
a newer version of entry (the entry should not be inserted)
@retval DB_DUPLICATE_KEY	when entry is a duplicate of rec */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_duplicate_online(
/*=====================*/
	ulint		n_uniq,	/*!< in: offset of DB_TRX_ID */
	const dtuple_t*	entry,	/*!< in: entry that is being inserted */
	const rec_t*	rec,	/*!< in: clustered index record */
	ulint*		offsets)/*!< in/out: rec_get_offsets(rec) */
{
	ulint	fields	= 0;
	ulint	bytes	= 0;

	/* During rebuild, there should not be any delete-marked rows
	in the new table. */
	ut_ad(!rec_get_deleted_flag(rec, rec_offs_comp(offsets)));
	ut_ad(dtuple_get_n_fields_cmp(entry) == n_uniq);

	/* Compare the PRIMARY KEY fields and the
	DB_TRX_ID, DB_ROLL_PTR. */
	cmp_dtuple_rec_with_match_low(
		entry, rec, offsets, n_uniq + 2, &fields, &bytes);

	if (fields < n_uniq) {
		/* Not a duplicate. */
		return(DB_SUCCESS);
	}

	if (fields == n_uniq + 2) {
		/* rec is an exact match of entry. */
		ut_ad(bytes == 0);
		return(DB_SUCCESS_LOCKED_REC);
	}

	return(DB_DUPLICATE_KEY);
}

/** Checks for a duplicate when the table is being rebuilt online.
@retval DB_SUCCESS		when no duplicate is detected
@retval DB_SUCCESS_LOCKED_REC	when rec is an exact match of entry or
a newer version of entry (the entry should not be inserted)
@retval DB_DUPLICATE_KEY	when entry is a duplicate of rec */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_duplicate_error_in_clust_online(
/*====================================*/
	ulint		n_uniq,	/*!< in: offset of DB_TRX_ID */
	const dtuple_t*	entry,	/*!< in: entry that is being inserted */
	const btr_cur_t*cursor,	/*!< in: cursor on insert position */
	ulint**		offsets,/*!< in/out: rec_get_offsets(rec) */
	mem_heap_t**	heap)	/*!< in/out: heap for offsets */
{
	dberr_t		err	= DB_SUCCESS;
	const rec_t*	rec	= btr_cur_get_rec(cursor);

	if (cursor->low_match >= n_uniq && !page_rec_is_infimum(rec)) {
		*offsets = rec_get_offsets(rec, cursor->index, *offsets,
					   ULINT_UNDEFINED, heap);
		err = row_ins_duplicate_online(n_uniq, entry, rec, *offsets);
		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	rec = page_rec_get_next_const(btr_cur_get_rec(cursor));

	if (cursor->up_match >= n_uniq && !page_rec_is_supremum(rec)) {
		*offsets = rec_get_offsets(rec, cursor->index, *offsets,
					   ULINT_UNDEFINED, heap);
		err = row_ins_duplicate_online(n_uniq, entry, rec, *offsets);
	}

	return(err);
}

/***************************************************************//**
Checks if a unique key violation error would occur at an index entry
insert. Sets shared locks on possible duplicate records. Works only
for a clustered index!
@retval DB_SUCCESS if no error
@retval DB_DUPLICATE_KEY if error,
@retval DB_LOCK_WAIT if we have to wait for a lock on a possible duplicate
record
@retval DB_SUCCESS_LOCKED_REC if an exact match of the record was found
in online table rebuild (flags & (BTR_KEEP_SYS_FLAG | BTR_NO_LOCKING_FLAG)) */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_duplicate_error_in_clust(
/*=============================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	btr_cur_t*	cursor,	/*!< in: B-tree cursor */
	const dtuple_t*	entry,	/*!< in: entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dberr_t	err;
	rec_t*	rec;
	ulint	n_unique;
	trx_t*	trx		= thr_get_trx(thr);
	mem_heap_t*heap		= NULL;
	ulint	offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*	offsets		= offsets_;
	rec_offs_init(offsets_);

	UT_NOT_USED(mtr);

	ut_ad(dict_index_is_clust(cursor->index));

	/* NOTE: For unique non-clustered indexes there may be any number
	of delete marked records with the same value for the non-clustered
	index key (remember multiversioning), and which differ only in
	the row refererence part of the index record, containing the
	clustered index key fields. For such a secondary index record,
	to avoid race condition, we must FIRST do the insertion and after
	that check that the uniqueness condition is not breached! */

	/* NOTE: A problem is that in the B-tree node pointers on an
	upper level may match more to the entry than the actual existing
	user records on the leaf level. So, even if low_match would suggest
	that a duplicate key violation may occur, this may not be the case. */

	n_unique = dict_index_get_n_unique(cursor->index);

	if (cursor->low_match >= n_unique) {

		rec = btr_cur_get_rec(cursor);

		if (!page_rec_is_infimum(rec)) {
			offsets = rec_get_offsets(rec, cursor->index, offsets,
						  ULINT_UNDEFINED, &heap);

			/* We set a lock on the possible duplicate: this
			is needed in logical logging of MySQL to make
			sure that in roll-forward we get the same duplicate
			errors as in original execution */

			if (trx->duplicates) {

				/* If the SQL-query will update or replace
				duplicate key we will take X-lock for
				duplicates ( REPLACE, LOAD DATAFILE REPLACE,
				INSERT ON DUPLICATE KEY UPDATE). */

				err = row_ins_set_exclusive_rec_lock(
					LOCK_REC_NOT_GAP,
					btr_cur_get_block(cursor),
					rec, cursor->index, offsets, thr);
			} else {

				err = row_ins_set_shared_rec_lock(
					LOCK_REC_NOT_GAP,
					btr_cur_get_block(cursor), rec,
					cursor->index, offsets, thr);
			}

			switch (err) {
			case DB_SUCCESS_LOCKED_REC:
			case DB_SUCCESS:
				break;
			default:
				goto func_exit;
			}

			if (row_ins_dupl_error_with_rec(
				    rec, entry, cursor->index, offsets)) {
duplicate:
				trx->error_info = cursor->index;
				err = DB_DUPLICATE_KEY;
				goto func_exit;
			}
		}
	}

	if (cursor->up_match >= n_unique) {

		rec = page_rec_get_next(btr_cur_get_rec(cursor));

		if (!page_rec_is_supremum(rec)) {
			offsets = rec_get_offsets(rec, cursor->index, offsets,
						  ULINT_UNDEFINED, &heap);

			if (trx->duplicates) {

				/* If the SQL-query will update or replace
				duplicate key we will take X-lock for
				duplicates ( REPLACE, LOAD DATAFILE REPLACE,
				INSERT ON DUPLICATE KEY UPDATE). */

				err = row_ins_set_exclusive_rec_lock(
					LOCK_REC_NOT_GAP,
					btr_cur_get_block(cursor),
					rec, cursor->index, offsets, thr);
			} else {

				err = row_ins_set_shared_rec_lock(
					LOCK_REC_NOT_GAP,
					btr_cur_get_block(cursor),
					rec, cursor->index, offsets, thr);
			}

			switch (err) {
			case DB_SUCCESS_LOCKED_REC:
			case DB_SUCCESS:
				break;
			default:
				goto func_exit;
			}

			if (row_ins_dupl_error_with_rec(
				    rec, entry, cursor->index, offsets)) {
				goto duplicate;
			}
		}

		/* This should never happen */
		ut_error;
	}

	err = DB_SUCCESS;
func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
}

/***************************************************************//**
Checks if an index entry has long enough common prefix with an
existing record so that the intended insert of the entry must be
changed to a modify of the existing record. In the case of a clustered
index, the prefix must be n_unique fields long. In the case of a
secondary index, all fields must be equal.  InnoDB never updates
secondary index records in place, other than clearing or setting the
delete-mark flag. We could be able to update the non-unique fields
of a unique secondary index record by checking the cursor->up_match,
but we do not do so, because it could have some locking implications.
@return TRUE if the existing record should be updated; FALSE if not */
UNIV_INLINE
ibool
row_ins_must_modify_rec(
/*====================*/
	const btr_cur_t*	cursor)	/*!< in: B-tree cursor */
{
	/* NOTE: (compare to the note in row_ins_duplicate_error_in_clust)
	Because node pointers on upper levels of the B-tree may match more
	to entry than to actual user records on the leaf level, we
	have to check if the candidate record is actually a user record.
	A clustered index node pointer contains index->n_unique first fields,
	and a secondary index node pointer contains all index fields. */

	return(cursor->low_match
	       >= dict_index_get_n_unique_in_tree(cursor->index)
	       && !page_rec_is_infimum(btr_cur_get_rec(cursor)));
}

/***************************************************************//**
Tries to insert an entry into a clustered index, ignoring foreign key
constraints. If a record with the same unique key is found, the other
record is necessarily marked deleted by a committed transaction, or a
unique key violation error occurs. The delete marked record is then
updated to an existing record, and we must write an undo log record on
the delete marked record.
@retval DB_SUCCESS on success
@retval DB_LOCK_WAIT on lock wait when !(flags & BTR_NO_LOCKING_FLAG)
@retval DB_FAIL if retry with BTR_MODIFY_TREE is needed
@return error code */
UNIV_INTERN
dberr_t
row_ins_clust_index_entry_low(
/*==========================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/*!< in: clustered index */
	ulint		n_uniq,	/*!< in: 0 or index->n_uniq */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	que_thr_t*	thr)	/*!< in: query thread */
{
	btr_cur_t	cursor;
	ulint*		offsets		= NULL;
	dberr_t		err;
	big_rec_t*	big_rec		= NULL;
	mtr_t		mtr;
	mem_heap_t*	offsets_heap	= NULL;

	ut_ad(dict_index_is_clust(index));
	ut_ad(!dict_index_is_unique(index)
	      || n_uniq == dict_index_get_n_unique(index));
	ut_ad(!n_uniq || n_uniq == dict_index_get_n_unique(index));

	mtr_start(&mtr);

	if (mode == BTR_MODIFY_LEAF && dict_index_is_online_ddl(index)) {
		mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
		mtr_s_lock(dict_index_get_lock(index), &mtr);
	}

	cursor.thr = thr;

	/* Note that we use PAGE_CUR_LE as the search mode, because then
	the function will return in both low_match and up_match of the
	cursor sensible values */

	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE, mode,
				    &cursor, 0, __FILE__, __LINE__, &mtr);

#ifdef UNIV_DEBUG
	{
		page_t*	page = btr_cur_get_page(&cursor);
		rec_t*	first_rec = page_rec_get_next(
			page_get_infimum_rec(page));

		ut_ad(page_rec_is_supremum(first_rec)
		      || rec_get_n_fields(first_rec, index)
		      == dtuple_get_n_fields(entry));
	}
#endif

	if (n_uniq && (cursor.up_match >= n_uniq
		       || cursor.low_match >= n_uniq)) {

		if (flags
		    == (BTR_CREATE_FLAG | BTR_NO_LOCKING_FLAG
			| BTR_NO_UNDO_LOG_FLAG | BTR_KEEP_SYS_FLAG)) {
			/* Set no locks when applying log
			in online table rebuild. Only check for duplicates. */
			err = row_ins_duplicate_error_in_clust_online(
				n_uniq, entry, &cursor,
				&offsets, &offsets_heap);

			switch (err) {
			case DB_SUCCESS:
				break;
			default:
				ut_ad(0);
				/* fall through */
			case DB_SUCCESS_LOCKED_REC:
			case DB_DUPLICATE_KEY:
				thr_get_trx(thr)->error_info = cursor.index;
			}
		} else {
			/* Note that the following may return also
			DB_LOCK_WAIT */

			err = row_ins_duplicate_error_in_clust(
				flags, &cursor, entry, thr, &mtr);
		}

		if (err != DB_SUCCESS) {
err_exit:
			mtr_commit(&mtr);
			goto func_exit;
		}
	}

	if (row_ins_must_modify_rec(&cursor)) {
		/* There is already an index entry with a long enough common
		prefix, we must convert the insert into a modify of an
		existing record */
		mem_heap_t*	entry_heap	= mem_heap_create(1024);

		err = row_ins_clust_index_entry_by_modify(
			flags, mode, &cursor, &offsets, &offsets_heap,
			entry_heap, &big_rec, entry, thr, &mtr);

		rec_t*		rec		= btr_cur_get_rec(&cursor);

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

			TODO: If the allocation extends the
			tablespace, it will not be redo
			logged, in either mini-transaction.
			Tablespace extension should be
			redo-logged in the big_rec
			mini-transaction, so that recovery
			will not fail when the big_rec was
			written to the extended portion of the
			file, in case the file was somehow
			truncated in the crash. */

			DEBUG_SYNC_C_IF_THD(
				thr_get_trx(thr)->mysql_thd,
				"before_row_ins_upd_extern");
			err = btr_store_big_rec_extern_fields(
				index, btr_cur_get_block(&cursor),
				rec, offsets, big_rec, &mtr,
				BTR_STORE_INSERT_UPDATE);
			DEBUG_SYNC_C_IF_THD(
				thr_get_trx(thr)->mysql_thd,
				"after_row_ins_upd_extern");
			/* If writing big_rec fails (for
			example, because of DB_OUT_OF_FILE_SPACE),
			the record will be corrupted. Even if
			we did not update any externally
			stored columns, our update could cause
			the record to grow so that a
			non-updated column was selected for
			external storage. This non-update
			would not have been written to the
			undo log, and thus the record cannot
			be rolled back.

			However, because we have not executed
			mtr_commit(mtr) yet, the update will
			not be replayed in crash recovery, and
			the following assertion failure will
			effectively "roll back" the operation. */
			ut_a(err == DB_SUCCESS);
			dtuple_big_rec_free(big_rec);
		}

		if (dict_index_is_online_ddl(index)) {
			row_log_table_insert(rec, index, offsets);
		}

		mtr_commit(&mtr);
		mem_heap_free(entry_heap);
	} else {
		rec_t*	insert_rec;

		if (mode != BTR_MODIFY_TREE) {
			ut_ad((mode & ~BTR_ALREADY_S_LATCHED)
			      == BTR_MODIFY_LEAF);
			err = btr_cur_optimistic_insert(
				flags, &cursor, &offsets, &offsets_heap,
				entry, &insert_rec, &big_rec,
				n_ext, thr, &mtr);
		} else {
			if (buf_LRU_buf_pool_running_out()) {

				err = DB_LOCK_TABLE_FULL;
				goto err_exit;
			}

			err = btr_cur_optimistic_insert(
				flags, &cursor,
				&offsets, &offsets_heap,
				entry, &insert_rec, &big_rec,
				n_ext, thr, &mtr);

			if (err == DB_FAIL) {
				err = btr_cur_pessimistic_insert(
					flags, &cursor,
					&offsets, &offsets_heap,
					entry, &insert_rec, &big_rec,
					n_ext, thr, &mtr);
			}
		}

		if (UNIV_LIKELY_NULL(big_rec)) {
			mtr_commit(&mtr);

			/* Online table rebuild could read (and
			ignore) the incomplete record at this point.
			If online rebuild is in progress, the
			row_ins_index_entry_big_rec() will write log. */

			DBUG_EXECUTE_IF(
				"row_ins_extern_checkpoint",
				log_make_checkpoint_at(
					IB_ULONGLONG_MAX, TRUE););
			err = row_ins_index_entry_big_rec(
				entry, big_rec, offsets, &offsets_heap, index,
				thr_get_trx(thr)->mysql_thd,
				__FILE__, __LINE__);
			dtuple_convert_back_big_rec(index, entry, big_rec);
		} else {
			if (err == DB_SUCCESS
			    && dict_index_is_online_ddl(index)) {
				row_log_table_insert(
					insert_rec, index, offsets);
			}

			mtr_commit(&mtr);
		}
	}

func_exit:
	if (offsets_heap) {
		mem_heap_free(offsets_heap);
	}

	return(err);
}

/***************************************************************//**
Starts a mini-transaction and checks if the index will be dropped.
@return true if the index is to be dropped */
static __attribute__((nonnull, warn_unused_result))
bool
row_ins_sec_mtr_start_and_check_if_aborted(
/*=======================================*/
	mtr_t*		mtr,	/*!< out: mini-transaction */
	dict_index_t*	index,	/*!< in/out: secondary index */
	bool		check,	/*!< in: whether to check */
	ulint		search_mode)
				/*!< in: flags */
{
	ut_ad(!dict_index_is_clust(index));

	mtr_start(mtr);

	if (!check) {
		return(false);
	}

	if (search_mode & BTR_ALREADY_S_LATCHED) {
		mtr_s_lock(dict_index_get_lock(index), mtr);
	} else {
		mtr_x_lock(dict_index_get_lock(index), mtr);
	}

	switch (index->online_status) {
	case ONLINE_INDEX_ABORTED:
	case ONLINE_INDEX_ABORTED_DROPPED:
		ut_ad(*index->name == TEMP_INDEX_PREFIX);
		return(true);
	case ONLINE_INDEX_COMPLETE:
		return(false);
	case ONLINE_INDEX_CREATION:
		break;
	}

	ut_error;
	return(true);
}

/***************************************************************//**
Tries to insert an entry into a secondary index. If a record with exactly the
same fields is found, the other record is necessarily marked deleted.
It is then unmarked. Otherwise, the entry is just inserted to the index.
@retval DB_SUCCESS on success
@retval DB_LOCK_WAIT on lock wait when !(flags & BTR_NO_LOCKING_FLAG)
@retval DB_FAIL if retry with BTR_MODIFY_TREE is needed
@return error code */
UNIV_INTERN
dberr_t
row_ins_sec_index_entry_low(
/*========================*/
	ulint		flags,	/*!< in: undo logging and locking flags */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/*!< in: secondary index */
	mem_heap_t*	offsets_heap,
				/*!< in/out: memory heap that can be emptied */
	mem_heap_t*	heap,	/*!< in/out: memory heap */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	trx_id_t	trx_id,	/*!< in: PAGE_MAX_TRX_ID during
				row_log_table_apply(), or 0 */
	que_thr_t*	thr)	/*!< in: query thread */
{
	btr_cur_t	cursor;
	ulint		search_mode	= mode | BTR_INSERT;
	dberr_t		err		= DB_SUCCESS;
	ulint		n_unique;
	mtr_t		mtr;
	ulint*		offsets	= NULL;

	ut_ad(!dict_index_is_clust(index));
	ut_ad(mode == BTR_MODIFY_LEAF || mode == BTR_MODIFY_TREE);

	cursor.thr = thr;
	ut_ad(thr_get_trx(thr)->id);
	mtr_start(&mtr);

	/* Ensure that we acquire index->lock when inserting into an
	index with index->online_status == ONLINE_INDEX_COMPLETE, but
	could still be subject to rollback_inplace_alter_table().
	This prevents a concurrent change of index->online_status.
	The memory object cannot be freed as long as we have an open
	reference to the table, or index->table->n_ref_count > 0. */
	const bool check = *index->name == TEMP_INDEX_PREFIX;
	if (check) {
		DEBUG_SYNC_C("row_ins_sec_index_enter");
		if (mode == BTR_MODIFY_LEAF) {
			search_mode |= BTR_ALREADY_S_LATCHED;
			mtr_s_lock(dict_index_get_lock(index), &mtr);
		} else {
			mtr_x_lock(dict_index_get_lock(index), &mtr);
		}

		if (row_log_online_op_try(
			    index, entry, thr_get_trx(thr)->id)) {
			goto func_exit;
		}
	}

	/* Note that we use PAGE_CUR_LE as the search mode, because then
	the function will return in both low_match and up_match of the
	cursor sensible values */

	if (!thr_get_trx(thr)->check_unique_secondary) {
		search_mode |= BTR_IGNORE_SEC_UNIQUE;
	}

	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
				    search_mode,
				    &cursor, 0, __FILE__, __LINE__, &mtr);

	if (cursor.flag == BTR_CUR_INSERT_TO_IBUF) {
		/* The insert was buffered during the search: we are done */
		goto func_exit;
	}

#ifdef UNIV_DEBUG
	{
		page_t*	page = btr_cur_get_page(&cursor);
		rec_t*	first_rec = page_rec_get_next(
			page_get_infimum_rec(page));

		ut_ad(page_rec_is_supremum(first_rec)
		      || rec_get_n_fields(first_rec, index)
		      == dtuple_get_n_fields(entry));
	}
#endif

	n_unique = dict_index_get_n_unique(index);

	if (dict_index_is_unique(index)
	    && (cursor.low_match >= n_unique || cursor.up_match >= n_unique)) {
		mtr_commit(&mtr);

		DEBUG_SYNC_C("row_ins_sec_index_unique");

		if (row_ins_sec_mtr_start_and_check_if_aborted(
			    &mtr, index, check, search_mode)) {
			goto func_exit;
		}

		err = row_ins_scan_sec_index_for_duplicate(
			flags, index, entry, thr, check, &mtr, offsets_heap);

		if (err != DB_SUCCESS) {
			goto func_exit;
		}

		mtr_commit(&mtr);

		if (row_ins_sec_mtr_start_and_check_if_aborted(
			    &mtr, index, check, search_mode)) {
			goto func_exit;
		}

		/* We did not find a duplicate and we have now
		locked with s-locks the necessary records to
		prevent any insertion of a duplicate by another
		transaction. Let us now reposition the cursor and
		continue the insertion. */

		btr_cur_search_to_nth_level(
			index, 0, entry, PAGE_CUR_LE,
			search_mode & ~(BTR_INSERT | BTR_IGNORE_SEC_UNIQUE),
			&cursor, 0, __FILE__, __LINE__, &mtr);
	}

	if (row_ins_must_modify_rec(&cursor)) {
		/* There is already an index entry with a long enough common
		prefix, we must convert the insert into a modify of an
		existing record */
		offsets = rec_get_offsets(
			btr_cur_get_rec(&cursor), index, offsets,
			ULINT_UNDEFINED, &offsets_heap);

		err = row_ins_sec_index_entry_by_modify(
			flags, mode, &cursor, &offsets,
			offsets_heap, heap, entry, thr, &mtr);
	} else {
		rec_t*		insert_rec;
		big_rec_t*	big_rec;

		if (mode == BTR_MODIFY_LEAF) {
			err = btr_cur_optimistic_insert(
				flags, &cursor, &offsets, &offsets_heap,
				entry, &insert_rec,
				&big_rec, 0, thr, &mtr);
		} else {
			ut_ad(mode == BTR_MODIFY_TREE);
			if (buf_LRU_buf_pool_running_out()) {

				err = DB_LOCK_TABLE_FULL;
				goto func_exit;
			}

			err = btr_cur_optimistic_insert(
				flags, &cursor,
				&offsets, &offsets_heap,
				entry, &insert_rec,
				&big_rec, 0, thr, &mtr);
			if (err == DB_FAIL) {
				err = btr_cur_pessimistic_insert(
					flags, &cursor,
					&offsets, &offsets_heap,
					entry, &insert_rec,
					&big_rec, 0, thr, &mtr);
			}
		}

		if (err == DB_SUCCESS && trx_id) {
			page_update_max_trx_id(
				btr_cur_get_block(&cursor),
				btr_cur_get_page_zip(&cursor),
				trx_id, &mtr);
		}

		ut_ad(!big_rec);
	}

func_exit:
	mtr_commit(&mtr);
	return(err);
}

/***************************************************************//**
Tries to insert the externally stored fields (off-page columns)
of a clustered index entry.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
UNIV_INTERN
dberr_t
row_ins_index_entry_big_rec_func(
/*=============================*/
	const dtuple_t*		entry,	/*!< in/out: index entry to insert */
	const big_rec_t*	big_rec,/*!< in: externally stored fields */
	ulint*			offsets,/*!< in/out: rec offsets */
	mem_heap_t**		heap,	/*!< in/out: memory heap */
	dict_index_t*		index,	/*!< in: index */
	const char*		file,	/*!< in: file name of caller */
#ifndef DBUG_OFF
	const void*		thd,	/*!< in: connection, or NULL */
#endif /* DBUG_OFF */
	ulint			line)	/*!< in: line number of caller */
{
	mtr_t		mtr;
	btr_cur_t	cursor;
	rec_t*		rec;
	dberr_t		error;

	ut_ad(dict_index_is_clust(index));

	DEBUG_SYNC_C_IF_THD(thd, "before_row_ins_extern_latch");

	mtr_start(&mtr);
	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
				    BTR_MODIFY_TREE, &cursor, 0,
				    file, line, &mtr);
	rec = btr_cur_get_rec(&cursor);
	offsets = rec_get_offsets(rec, index, offsets,
				  ULINT_UNDEFINED, heap);

	DEBUG_SYNC_C_IF_THD(thd, "before_row_ins_extern");
	error = btr_store_big_rec_extern_fields(
		index, btr_cur_get_block(&cursor),
		rec, offsets, big_rec, &mtr, BTR_STORE_INSERT);
	DEBUG_SYNC_C_IF_THD(thd, "after_row_ins_extern");

	if (error == DB_SUCCESS
	    && dict_index_is_online_ddl(index)) {
		row_log_table_insert(rec, index, offsets);
	}

	mtr_commit(&mtr);

	return(error);
}

/***************************************************************//**
Inserts an entry into a clustered index. Tries first optimistic,
then pessimistic descent down the tree. If the entry matches enough
to a delete marked record, performs the insert by updating or delete
unmarking the delete marked record.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DUPLICATE_KEY, or some other error code */
UNIV_INTERN
dberr_t
row_ins_clust_index_entry(
/*======================*/
	dict_index_t*	index,	/*!< in: clustered index */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	ulint		n_ext)	/*!< in: number of externally stored columns */
{
	dberr_t	err;
	ulint	n_uniq;

	if (UT_LIST_GET_FIRST(index->table->foreign_list)) {
		err = row_ins_check_foreign_constraints(
			index->table, index, entry, thr);
		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	n_uniq = dict_index_is_unique(index) ? index->n_uniq : 0;

	/* Try first optimistic descent to the B-tree */

	log_free_check();

	err = row_ins_clust_index_entry_low(
		0, BTR_MODIFY_LEAF, index, n_uniq, entry, n_ext, thr);
	if (err != DB_FAIL) {

		return(err);
	}

	/* Try then pessimistic descent to the B-tree */

	log_free_check();

	return(row_ins_clust_index_entry_low(
		       0, BTR_MODIFY_TREE, index, n_uniq, entry, n_ext, thr));
}

/***************************************************************//**
Inserts an entry into a secondary index. Tries first optimistic,
then pessimistic descent down the tree. If the entry matches enough
to a delete marked record, performs the insert by updating or delete
unmarking the delete marked record.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DUPLICATE_KEY, or some other error code */
UNIV_INTERN
dberr_t
row_ins_sec_index_entry(
/*====================*/
	dict_index_t*	index,	/*!< in: secondary index */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t		err;
	mem_heap_t*	offsets_heap;
	mem_heap_t*	heap;

	if (UT_LIST_GET_FIRST(index->table->foreign_list)) {
		err = row_ins_check_foreign_constraints(index->table, index,
							entry, thr);
		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	ut_ad(thr_get_trx(thr)->id);

	offsets_heap = mem_heap_create(1024);
	heap = mem_heap_create(1024);

	/* Try first optimistic descent to the B-tree */

	log_free_check();

	err = row_ins_sec_index_entry_low(
		0, BTR_MODIFY_LEAF, index, offsets_heap, heap, entry, 0, thr);
	if (err == DB_FAIL) {
		mem_heap_empty(heap);

		/* Try then pessimistic descent to the B-tree */

		log_free_check();

		err = row_ins_sec_index_entry_low(
			0, BTR_MODIFY_TREE, index,
			offsets_heap, heap, entry, 0, thr);
	}

	mem_heap_free(heap);
	mem_heap_free(offsets_heap);
	return(err);
}

/***************************************************************//**
Inserts an index entry to index. Tries first optimistic, then pessimistic
descent down the tree. If the entry matches enough to a delete marked record,
performs the insert by updating or delete unmarking the delete marked
record.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DUPLICATE_KEY, or some other error code */
static
dberr_t
row_ins_index_entry(
/*================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	que_thr_t*	thr)	/*!< in: query thread */
{
	if (dict_index_is_clust(index)) {
		return(row_ins_clust_index_entry(index, entry, thr, 0));
	} else {
		return(row_ins_sec_index_entry(index, entry, thr));
	}
}

/***********************************************************//**
Sets the values of the dtuple fields in entry from the values of appropriate
columns in row. */
static __attribute__((nonnull))
void
row_ins_index_entry_set_vals(
/*=========================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry to make */
	const dtuple_t*	row)	/*!< in: row */
{
	ulint	n_fields;
	ulint	i;

	n_fields = dtuple_get_n_fields(entry);

	for (i = 0; i < n_fields; i++) {
		dict_field_t*	ind_field;
		dfield_t*	field;
		const dfield_t*	row_field;
		ulint		len;

		field = dtuple_get_nth_field(entry, i);
		ind_field = dict_index_get_nth_field(index, i);
		row_field = dtuple_get_nth_field(row, ind_field->col->ind);
		len = dfield_get_len(row_field);

		/* Check column prefix indexes */
		if (ind_field->prefix_len > 0
		    && dfield_get_len(row_field) != UNIV_SQL_NULL) {

			const	dict_col_t*	col
				= dict_field_get_col(ind_field);

			len = dtype_get_at_most_n_mbchars(
				col->prtype, col->mbminmaxlen,
				ind_field->prefix_len,
				len,
				static_cast<const char*>(
					dfield_get_data(row_field)));

			ut_ad(!dfield_is_ext(row_field));
		}

		dfield_set_data(field, dfield_get_data(row_field), len);
		if (dfield_is_ext(row_field)) {
			ut_ad(dict_index_is_clust(index));
			dfield_set_ext(field);
		}
	}
}

/***********************************************************//**
Inserts a single index entry to the table.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins_index_entry_step(
/*=====================*/
	ins_node_t*	node,	/*!< in: row insert node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t	err;

	ut_ad(dtuple_check_typed(node->row));

	row_ins_index_entry_set_vals(node->index, node->entry, node->row);

	ut_ad(dtuple_check_typed(node->entry));

	err = row_ins_index_entry(node->index, node->entry, thr);

#ifdef UNIV_DEBUG
	/* Work around Bug#14626800 ASSERTION FAILURE IN DEBUG_SYNC().
	Once it is fixed, remove the 'ifdef', 'if' and this comment. */
	if (!thr_get_trx(thr)->ddl) {
		DEBUG_SYNC_C_IF_THD(thr_get_trx(thr)->mysql_thd,
				    "after_row_ins_index_entry_step");
	}
#endif /* UNIV_DEBUG */

	return(err);
}

/***********************************************************//**
Allocates a row id for row and inits the node->index field. */
UNIV_INLINE
void
row_ins_alloc_row_id_step(
/*======================*/
	ins_node_t*	node)	/*!< in: row insert node */
{
	row_id_t	row_id;

	ut_ad(node->state == INS_NODE_ALLOC_ROW_ID);

	if (dict_index_is_unique(dict_table_get_first_index(node->table))) {

		/* No row id is stored if the clustered index is unique */

		return;
	}

	/* Fill in row id value to row */

	row_id = dict_sys_get_new_row_id();

	dict_sys_write_row_id(node->row_id_buf, row_id);
}

/***********************************************************//**
Gets a row to insert from the values list. */
UNIV_INLINE
void
row_ins_get_row_from_values(
/*========================*/
	ins_node_t*	node)	/*!< in: row insert node */
{
	que_node_t*	list_node;
	dfield_t*	dfield;
	dtuple_t*	row;
	ulint		i;

	/* The field values are copied in the buffers of the select node and
	it is safe to use them until we fetch from select again: therefore
	we can just copy the pointers */

	row = node->row;

	i = 0;
	list_node = node->values_list;

	while (list_node) {
		eval_exp(list_node);

		dfield = dtuple_get_nth_field(row, i);
		dfield_copy_data(dfield, que_node_get_val(list_node));

		i++;
		list_node = que_node_get_next(list_node);
	}
}

/***********************************************************//**
Gets a row to insert from the select list. */
UNIV_INLINE
void
row_ins_get_row_from_select(
/*========================*/
	ins_node_t*	node)	/*!< in: row insert node */
{
	que_node_t*	list_node;
	dfield_t*	dfield;
	dtuple_t*	row;
	ulint		i;

	/* The field values are copied in the buffers of the select node and
	it is safe to use them until we fetch from select again: therefore
	we can just copy the pointers */

	row = node->row;

	i = 0;
	list_node = node->select->select_list;

	while (list_node) {
		dfield = dtuple_get_nth_field(row, i);
		dfield_copy_data(dfield, que_node_get_val(list_node));

		i++;
		list_node = que_node_get_next(list_node);
	}
}

/***********************************************************//**
Inserts a row to a table.
@return DB_SUCCESS if operation successfully completed, else error
code or DB_LOCK_WAIT */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_ins(
/*====*/
	ins_node_t*	node,	/*!< in: row insert node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dberr_t	err;

	if (node->state == INS_NODE_ALLOC_ROW_ID) {

		row_ins_alloc_row_id_step(node);

		node->index = dict_table_get_first_index(node->table);
		node->entry = UT_LIST_GET_FIRST(node->entry_list);

		if (node->ins_type == INS_SEARCHED) {

			row_ins_get_row_from_select(node);

		} else if (node->ins_type == INS_VALUES) {

			row_ins_get_row_from_values(node);
		}

		node->state = INS_NODE_INSERT_ENTRIES;
	}

	ut_ad(node->state == INS_NODE_INSERT_ENTRIES);

	while (node->index != NULL) {
		if (node->index->type != DICT_FTS) {
			err = row_ins_index_entry_step(node, thr);

			if (err != DB_SUCCESS) {

				return(err);
			}
		}

		node->index = dict_table_get_next_index(node->index);
		node->entry = UT_LIST_GET_NEXT(tuple_list, node->entry);

		DBUG_EXECUTE_IF(
			"row_ins_skip_sec",
			node->index = NULL; node->entry = NULL; break;);

		/* Skip corrupted secondary index and its entry */
		while (node->index && dict_index_is_corrupted(node->index)) {

			node->index = dict_table_get_next_index(node->index);
			node->entry = UT_LIST_GET_NEXT(tuple_list, node->entry);
		}
	}

	ut_ad(node->entry == NULL);

	node->state = INS_NODE_ALLOC_ROW_ID;

	return(DB_SUCCESS);
}

/***********************************************************//**
Inserts a row to a table. This is a high-level function used in SQL execution
graphs.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
row_ins_step(
/*=========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	ins_node_t*	node;
	que_node_t*	parent;
	sel_node_t*	sel_node;
	trx_t*		trx;
	dberr_t		err;

	ut_ad(thr);

	trx = thr_get_trx(thr);

	trx_start_if_not_started_xa(trx);

	node = static_cast<ins_node_t*>(thr->run_node);

	ut_ad(que_node_get_type(node) == QUE_NODE_INSERT);

	parent = que_node_get_parent(node);
	sel_node = node->select;

	if (thr->prev_node == parent) {
		node->state = INS_NODE_SET_IX_LOCK;
	}

	/* If this is the first time this node is executed (or when
	execution resumes after wait for the table IX lock), set an
	IX lock on the table and reset the possible select node. MySQL's
	partitioned table code may also call an insert within the same
	SQL statement AFTER it has used this table handle to do a search.
	This happens, for example, when a row update moves it to another
	partition. In that case, we have already set the IX lock on the
	table during the search operation, and there is no need to set
	it again here. But we must write trx->id to node->trx_id_buf. */

	trx_write_trx_id(node->trx_id_buf, trx->id);

	if (node->state == INS_NODE_SET_IX_LOCK) {

		node->state = INS_NODE_ALLOC_ROW_ID;

		/* It may be that the current session has not yet started
		its transaction, or it has been committed: */

		if (trx->id == node->trx_id) {
			/* No need to do IX-locking */

			goto same_trx;
		}

		err = lock_table(0, node->table, LOCK_IX, thr);

		if (err != DB_SUCCESS) {

			goto error_handling;
		}

		node->trx_id = trx->id;
same_trx:
		if (node->ins_type == INS_SEARCHED) {
			/* Reset the cursor */
			sel_node->state = SEL_NODE_OPEN;

			/* Fetch a row to insert */

			thr->run_node = sel_node;

			return(thr);
		}
	}

	if ((node->ins_type == INS_SEARCHED)
	    && (sel_node->state != SEL_NODE_FETCH)) {

		ut_ad(sel_node->state == SEL_NODE_NO_MORE_ROWS);

		/* No more rows to insert */
		thr->run_node = parent;

		return(thr);
	}

	/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */

	err = row_ins(node, thr);

error_handling:
	trx->error_state = err;

	if (err != DB_SUCCESS) {
		/* err == DB_LOCK_WAIT or SQL error detected */
		return(NULL);
	}

	/* DO THE TRIGGER ACTIONS HERE */

	if (node->ins_type == INS_SEARCHED) {
		/* Fetch a row to insert */

		thr->run_node = sel_node;
	} else {
		thr->run_node = que_node_get_parent(node);
	}

	return(thr);
}
