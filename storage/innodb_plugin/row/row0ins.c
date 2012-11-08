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
@file row/row0ins.c
Insert into a table

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#ifdef __WIN__
/* error LNK2001: unresolved external symbol _debug_sync_C_callback_ptr */
# define DEBUG_SYNC_C(dummy) ((void) 0)
#else
# include "my_global.h" /* HAVE_* */
# include "m_string.h" /* for my_sys.h */
# include "my_sys.h" /* DEBUG_SYNC_C */
#endif

#include "row0ins.h"

#ifdef UNIV_NONINL
#include "row0ins.ic"
#endif

#include "ha_prototypes.h"
#include "dict0dict.h"
#include "dict0boot.h"
#include "trx0undo.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "mach0data.h"
#include "que0que.h"
#include "row0upd.h"
#include "row0sel.h"
#include "row0row.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "log0log.h"
#include "eval0eval.h"
#include "data0data.h"
#include "usr0sess.h"
#include "buf0lru.h"

#define	ROW_INS_PREV	1
#define	ROW_INS_NEXT	2

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

	node = mem_heap_alloc(heap, sizeof(ins_node_t));

	node->common.type = QUE_NODE_INSERT;

	node->ins_type = ins_type;

	node->state = INS_NODE_SET_IX_LOCK;
	node->table = table;
	node->index = NULL;
	node->entry = NULL;

	node->select = NULL;

	node->trx_id = ut_dulint_zero;

	node->entry_sys_heap = mem_heap_create(128);

	node->magic_n = INS_NODE_MAGIC_N;

	return(node);
}

/***********************************************************//**
Creates an entry template for each index of a table. */
UNIV_INTERN
void
ins_node_create_entry_list(
/*=======================*/
	ins_node_t*	node)	/*!< in: row insert node */
{
	dict_index_t*	index;
	dtuple_t*	entry;

	ut_ad(node->entry_sys_heap);

	UT_LIST_INIT(node->entry_list);

	index = dict_table_get_first_index(node->table);

	while (index != NULL) {
		entry = row_build_index_entry(node->row, NULL, index,
					      node->entry_sys_heap);
		UT_LIST_ADD_LAST(tuple_list, node->entry_list, entry);

		index = dict_table_get_next_index(index);
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

	ptr = mem_heap_zalloc(heap, DATA_ROW_ID_LEN);

	dfield_set_data(dfield, ptr, DATA_ROW_ID_LEN);

	node->row_id_buf = ptr;

	/* 3. Allocate buffer for trx id */

	col = dict_table_get_sys_col(table, DATA_TRX_ID);

	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = mem_heap_zalloc(heap, DATA_TRX_ID_LEN);

	dfield_set_data(dfield, ptr, DATA_TRX_ID_LEN);

	node->trx_id_buf = ptr;

	/* 4. Allocate buffer for roll ptr */

	col = dict_table_get_sys_col(table, DATA_ROLL_PTR);

	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = mem_heap_zalloc(heap, DATA_ROLL_PTR_LEN);

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

	node->trx_id = ut_dulint_zero;
}

/*******************************************************************//**
Does an insert operation by updating a delete-marked existing record
in the index. This situation can occur if the delete-marked record is
kept in the index for consistent reads.
@return	DB_SUCCESS or error code */
static
ulint
row_ins_sec_index_entry_by_modify(
/*==============================*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/*!< in: B-tree cursor */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr; must be committed before
				latching any further pages */
{
	big_rec_t*	dummy_big_rec;
	mem_heap_t*	heap;
	upd_t*		update;
	rec_t*		rec;
	ulint		err;

	rec = btr_cur_get_rec(cursor);

	ut_ad(!dict_index_is_clust(cursor->index));
	ut_ad(rec_get_deleted_flag(rec,
				   dict_table_is_comp(cursor->index->table)));

	/* We know that in the alphabetical ordering, entry and rec are
	identified. But in their binary form there may be differences if
	there are char fields in them. Therefore we have to calculate the
	difference. */

	heap = mem_heap_create(1024);

	update = row_upd_build_sec_rec_difference_binary(
		cursor->index, entry, rec, thr_get_trx(thr), heap);
	if (mode == BTR_MODIFY_LEAF) {
		/* Try an optimistic updating of the record, keeping changes
		within the page */

		err = btr_cur_optimistic_update(BTR_KEEP_SYS_FLAG, cursor,
						update, 0, thr, mtr);
		switch (err) {
		case DB_OVERFLOW:
		case DB_UNDERFLOW:
		case DB_ZIP_OVERFLOW:
			err = DB_FAIL;
		}
	} else {
		ut_a(mode == BTR_MODIFY_TREE);
		if (buf_LRU_buf_pool_running_out()) {

			err = DB_LOCK_TABLE_FULL;

			goto func_exit;
		}

		err = btr_cur_pessimistic_update(BTR_KEEP_SYS_FLAG, cursor,
						 &heap, &dummy_big_rec, update,
						 0, thr, mtr);
		ut_ad(!dummy_big_rec);
	}
func_exit:
	mem_heap_free(heap);

	return(err);
}

/*******************************************************************//**
Does an insert operation by delete unmarking and updating a delete marked
existing record in the index. This situation can occur if the delete marked
record is kept in the index for consistent reads.
@return	DB_SUCCESS, DB_FAIL, or error code */
static
ulint
row_ins_clust_index_entry_by_modify(
/*================================*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/*!< in: B-tree cursor */
	mem_heap_t**	heap,	/*!< in/out: pointer to memory heap, or NULL */
	big_rec_t**	big_rec,/*!< out: possible big rec vector of fields
				which have to be stored externally by the
				caller */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr; must be committed before
				latching any further pages */
{
	rec_t*		rec;
	upd_t*		update;
	ulint		err;

	ut_ad(dict_index_is_clust(cursor->index));

	*big_rec = NULL;

	rec = btr_cur_get_rec(cursor);

	ut_ad(rec_get_deleted_flag(rec,
				   dict_table_is_comp(cursor->index->table)));

	if (!*heap) {
		*heap = mem_heap_create(1024);
	}

	/* Build an update vector containing all the fields to be modified;
	NOTE that this vector may NOT contain system columns trx_id or
	roll_ptr */

	update = row_upd_build_difference_binary(cursor->index, entry, rec,
						 thr_get_trx(thr), *heap);
	if (mode == BTR_MODIFY_LEAF) {
		/* Try optimistic updating of the record, keeping changes
		within the page */

		err = btr_cur_optimistic_update(0, cursor, update, 0, thr,
						mtr);
		switch (err) {
		case DB_OVERFLOW:
		case DB_UNDERFLOW:
		case DB_ZIP_OVERFLOW:
			err = DB_FAIL;
		}
	} else {
		ut_a(mode == BTR_MODIFY_TREE);
		if (buf_LRU_buf_pool_running_out()) {

			return(DB_LOCK_TABLE_FULL);

		}
		err = btr_cur_pessimistic_update(
			BTR_KEEP_POS_FLAG, cursor, heap, big_rec, update,
			0, thr, mtr);
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
	upd_node_t*	upd_node;

	parent = que_node_get_parent(node);

	while (que_node_get_type(parent) == QUE_NODE_UPDATE) {

		upd_node = parent;

		if (upd_node->table == table && upd_node->is_delete == FALSE) {

			return(TRUE);
		}

		parent = que_node_get_parent(parent);

		ut_a(parent);
	}

	return(FALSE);
}

/*********************************************************************//**
Returns the number of ancestor UPDATE or DELETE nodes of a
cascaded update/delete node.
@return	number of ancestors */
static
ulint
row_ins_cascade_n_ancestors(
/*========================*/
	que_node_t*	node)	/*!< in: node in a query graph */
{
	que_node_t*	parent;
	ulint		n_ancestors = 0;

	parent = que_node_get_parent(node);

	while (que_node_get_type(parent) == QUE_NODE_UPDATE) {
		n_ancestors++;

		parent = que_node_get_parent(parent);

		ut_a(parent);
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
static
ulint
row_ins_cascade_calc_update_vec(
/*============================*/
	upd_node_t*	node,		/*!< in: update node of the parent
					table */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint whose
					type is != 0 */
	mem_heap_t*	heap)		/*!< in: memory heap to use as
					temporary storage */
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
					col->prtype,
					col->mbminlen, col->mbmaxlen,
					col->len,
					ufield_len,
					dfield_get_data(&ufield->new_val))
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

					char*		pad_start;
					const char*	pad_end;
					char*		padded_data
						= mem_heap_alloc(
							heap, min_size);
					pad_start = padded_data + ufield_len;
					pad_end = padded_data + min_size;

					memcpy(padded_data,
					       dfield_get_data(&ufield
							       ->new_val),
					       dfield_get_len(&ufield
							      ->new_val));

					switch (UNIV_EXPECT(col->mbminlen,1)) {
					default:
						ut_error;
						return(ULINT_UNDEFINED);
					case 1:
						if (UNIV_UNLIKELY
						    (dtype_get_charset_coll(
							    col->prtype)
						     == DATA_MYSQL_BINARY_CHARSET_COLL)) {
							/* Do not pad BINARY
							columns. */
							return(ULINT_UNDEFINED);
						}

						/* space=0x20 */
						memset(pad_start, 0x20,
						       pad_end - pad_start);
						break;
					case 2:
						/* space=0x0020 */
						ut_a(!(ufield_len % 2));
						ut_a(!(min_size % 2));
						do {
							*pad_start++ = 0x00;
							*pad_start++ = 0x20;
						} while (pad_start < pad_end);
						break;
					}

					dfield_set_data(&ufield->new_val,
							padded_data, min_size);
				}

				n_fields_updated++;
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
	FILE*	ef	= dict_foreign_err_file;
	trx_t*	trx	= thr_get_trx(thr);

	row_ins_set_detailed(trx, foreign);

	mutex_enter(&dict_foreign_err_mutex);
	rewind(ef);
	ut_print_timestamp(ef);
	fputs(" Transaction:\n", ef);
	trx_print(ef, trx, 600);

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
	FILE*	ef	= dict_foreign_err_file;

	row_ins_set_detailed(trx, foreign);

	mutex_enter(&dict_foreign_err_mutex);
	rewind(ef);
	ut_print_timestamp(ef);
	fputs(" Transaction:\n", ef);
	trx_print(ef, trx, 600);
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
static
ulint
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
	ulint		err;
	ulint		i;
	trx_t*		trx;
	mem_heap_t*	tmp_heap	= NULL;

	ut_a(thr);
	ut_a(foreign);
	ut_a(pcur);
	ut_a(mtr);

	trx = thr_get_trx(thr);

	/* Since we are going to delete or update a row, we have to invalidate
	the MySQL query cache for table. A deadlock of threads is not possible
	here because the caller of this function does not hold any latches with
	the sync0sync.h rank above the kernel mutex. The query cache mutex has
	a rank just above the kernel mutex. */

	row_ins_invalidate_query_cache(thr, table->name);

	node = thr->run_node;

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

		tmp_heap = mem_heap_create(256);

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
		}
	}

	if (!node->is_delete
	    && (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE)) {

		/* Build the appropriate update vector which sets changing
		foreign->n_fields first fields in rec to new values */

		upd_vec_heap = mem_heap_create(256);

		n_to_update = row_ins_cascade_calc_update_vec(node, foreign,
							      upd_vec_heap);
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
enum db_err
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
	enum db_err	err;

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
enum db_err
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
	enum db_err	err;

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
ulint
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
	upd_node_t*	upd_node;
	dict_table_t*	check_table;
	dict_index_t*	check_index;
	ulint		n_fields_cmp;
	btr_pcur_t	pcur;
	int		cmp;
	ulint		err;
	ulint		i;
	mtr_t		mtr;
	trx_t*		trx		= thr_get_trx(thr);
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

run_again:
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_SHARED));
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
		upd_node = thr->run_node;

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

	if (check_table == NULL || check_table->ibd_file_missing
	    || check_index == NULL) {
		if (check_ref) {
			FILE*	ef = dict_foreign_err_file;

			row_ins_set_detailed(trx, foreign);

			mutex_enter(&dict_foreign_err_mutex);
			rewind(ef);
			ut_print_timestamp(ef);
			fputs(" Transaction:\n", ef);
			trx_print(ef, trx, 600);
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
		trx->error_state = err;

		que_thr_stop_for_mysql(thr);

		srv_suspend_mysql_thread(thr);

		if (trx->error_state == DB_SUCCESS) {

			goto run_again;
		}

		err = trx->error_state;
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
static
ulint
row_ins_check_foreign_constraints(
/*==============================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry for index */
	que_thr_t*	thr)	/*!< in: query thread */
{
	dict_foreign_t*	foreign;
	ulint		err;
	trx_t*		trx;
	ibool		got_s_lock	= FALSE;

	trx = thr_get_trx(thr);

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign) {
		if (foreign->foreign_index == index) {

			if (foreign->referenced_table == NULL) {
				dict_table_get(foreign->referenced_table_name,
					       FALSE);
			}

			if (0 == trx->dict_operation_lock_mode) {
				got_s_lock = TRUE;

				row_mysql_freeze_data_dictionary(trx);
			}

			if (foreign->referenced_table) {
				mutex_enter(&(dict_sys->mutex));

				(foreign->referenced_table
				 ->n_foreign_key_checks_running)++;

				mutex_exit(&(dict_sys->mutex));
			}

			/* NOTE that if the thread ends up waiting for a lock
			we will release dict_operation_lock temporarily!
			But the counter on the table protects the referenced
			table from being dropped while the check is running. */

			err = row_ins_check_foreign_constraint(
				TRUE, foreign, table, entry, thr);

			if (foreign->referenced_table) {
				mutex_enter(&(dict_sys->mutex));

				ut_a(foreign->referenced_table
				     ->n_foreign_key_checks_running > 0);
				(foreign->referenced_table
				 ->n_foreign_key_checks_running)--;

				mutex_exit(&(dict_sys->mutex));
			}

			if (got_s_lock) {
				row_mysql_unfreeze_data_dictionary(trx);
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
			if (UNIV_SQL_NULL == dfield_get_len(
				    dtuple_get_nth_field(entry, i))) {

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
static
ulint
row_ins_scan_sec_index_for_duplicate(
/*=================================*/
	dict_index_t*	index,	/*!< in: non-clustered unique index */
	dtuple_t*	entry,	/*!< in: index entry */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint		n_unique;
	ulint		i;
	int		cmp;
	ulint		n_fields_cmp;
	btr_pcur_t	pcur;
	ulint		err		= DB_SUCCESS;
	ulint		allow_duplicates;
	mtr_t		mtr;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	n_unique = dict_index_get_n_unique(index);

	/* If the secondary index is unique, but one of the fields in the
	n_unique first fields is NULL, a unique key violation cannot occur,
	since we define NULL != NULL in this case */

	for (i = 0; i < n_unique; i++) {
		if (UNIV_SQL_NULL == dfield_get_len(
			    dtuple_get_nth_field(entry, i))) {

			return(DB_SUCCESS);
		}
	}

	mtr_start(&mtr);

	/* Store old value on n_fields_cmp */

	n_fields_cmp = dtuple_get_n_fields_cmp(entry);

	dtuple_set_n_fields_cmp(entry, dict_index_get_n_unique(index));

	btr_pcur_open(index, entry, PAGE_CUR_GE, BTR_SEARCH_LEAF, &pcur, &mtr);

	allow_duplicates = thr_get_trx(thr)->duplicates;

	/* Scan index records and check if there is a duplicate */

	do {
		const rec_t*		rec	= btr_pcur_get_rec(&pcur);
		const buf_block_t*	block	= btr_pcur_get_block(&pcur);

		if (page_rec_is_infimum(rec)) {

			continue;
		}

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);

		if (allow_duplicates) {

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
	} while (btr_pcur_move_to_next(&pcur, &mtr));

end_scan:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	mtr_commit(&mtr);

	/* Restore old value */
	dtuple_set_n_fields_cmp(entry, n_fields_cmp);

	return(err);
}

/***************************************************************//**
Checks if a unique key violation error would occur at an index entry
insert. Sets shared locks on possible duplicate records. Works only
for a clustered index!
@return DB_SUCCESS if no error, DB_DUPLICATE_KEY if error,
DB_LOCK_WAIT if we have to wait for a lock on a possible duplicate
record */
static
ulint
row_ins_duplicate_error_in_clust(
/*=============================*/
	btr_cur_t*	cursor,	/*!< in: B-tree cursor */
	const dtuple_t*	entry,	/*!< in: entry to insert */
	que_thr_t*	thr,	/*!< in: query thread */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint	err;
	rec_t*	rec;
	ulint	n_unique;
	trx_t*	trx		= thr_get_trx(thr);
	mem_heap_t*heap		= NULL;
	ulint	offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*	offsets		= offsets_;
	rec_offs_init(offsets_);

	UT_NOT_USED(mtr);

	ut_a(dict_index_is_clust(cursor->index));
	ut_ad(dict_index_is_unique(cursor->index));

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
				trx->error_info = cursor->index;
				err = DB_DUPLICATE_KEY;
				goto func_exit;
			}
		}

		ut_a(!dict_index_is_clust(cursor->index));
		/* This should never happen */
	}

	err = DB_SUCCESS;
func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
}

/***************************************************************//**
Checks if an index entry has long enough common prefix with an existing
record so that the intended insert of the entry must be changed to a modify of
the existing record. In the case of a clustered index, the prefix must be
n_unique fields long, and in the case of a secondary index, all fields must be
equal.
@return 0 if no update, ROW_INS_PREV if previous should be updated;
currently we do the search so that only the low_match record can match
enough to the search tuple, not the next record */
UNIV_INLINE
ulint
row_ins_must_modify(
/*================*/
	btr_cur_t*	cursor)	/*!< in: B-tree cursor */
{
	ulint	enough_match;
	rec_t*	rec;

	/* NOTE: (compare to the note in row_ins_duplicate_error) Because node
	pointers on upper levels of the B-tree may match more to entry than
	to actual user records on the leaf level, we have to check if the
	candidate record is actually a user record. In a clustered index
	node pointers contain index->n_unique first fields, and in the case
	of a secondary index, all fields of the index. */

	enough_match = dict_index_get_n_unique_in_tree(cursor->index);

	if (cursor->low_match >= enough_match) {

		rec = btr_cur_get_rec(cursor);

		if (!page_rec_is_infimum(rec)) {

			return(ROW_INS_PREV);
		}
	}

	return(0);
}

/***************************************************************//**
Tries to insert an index entry to an index. If the index is clustered
and a record with the same unique key is found, the other record is
necessarily marked deleted by a committed transaction, or a unique key
violation error occurs. The delete marked record is then updated to an
existing record, and we must write an undo log record on the delete
marked record. If the index is secondary, and a record with exactly the
same fields is found, the other record is necessarily marked deleted.
It is then unmarked. Otherwise, the entry is just inserted to the index.
@return DB_SUCCESS, DB_LOCK_WAIT, DB_FAIL if pessimistic retry needed,
or error code */
static
ulint
row_ins_index_entry_low(
/*====================*/
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	que_thr_t*	thr)	/*!< in: query thread */
{
	btr_cur_t	cursor;
	ulint		ignore_sec_unique	= 0;
	ulint		modify = 0; /* remove warning */
	rec_t*		insert_rec;
	rec_t*		rec;
	ulint*		offsets;
	ulint		err;
	ulint		n_unique;
	big_rec_t*	big_rec			= NULL;
	mtr_t		mtr;
	mem_heap_t*	heap			= NULL;

	log_free_check();

	mtr_start(&mtr);

	cursor.thr = thr;

	/* Note that we use PAGE_CUR_LE as the search mode, because then
	the function will return in both low_match and up_match of the
	cursor sensible values */

	if (!(thr_get_trx(thr)->check_unique_secondary)) {
		ignore_sec_unique = BTR_IGNORE_SEC_UNIQUE;
	}

	btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
				    mode | BTR_INSERT | ignore_sec_unique,
				    &cursor, 0, __FILE__, __LINE__, &mtr);

	if (cursor.flag == BTR_CUR_INSERT_TO_IBUF) {
		/* The insertion was made to the insert buffer already during
		the search: we are done */

		err = DB_SUCCESS;

		goto function_exit;
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

	if (dict_index_is_unique(index) && (cursor.up_match >= n_unique
					    || cursor.low_match >= n_unique)) {

		if (dict_index_is_clust(index)) {
			/* Note that the following may return also
			DB_LOCK_WAIT */

			err = row_ins_duplicate_error_in_clust(
				&cursor, entry, thr, &mtr);
			if (err != DB_SUCCESS) {

				goto function_exit;
			}
		} else {
			mtr_commit(&mtr);
			err = row_ins_scan_sec_index_for_duplicate(
				index, entry, thr);
			mtr_start(&mtr);

			if (err != DB_SUCCESS) {

				goto function_exit;
			}

			/* We did not find a duplicate and we have now
			locked with s-locks the necessary records to
			prevent any insertion of a duplicate by another
			transaction. Let us now reposition the cursor and
			continue the insertion. */

			btr_cur_search_to_nth_level(index, 0, entry,
						    PAGE_CUR_LE,
						    mode | BTR_INSERT,
						    &cursor, 0,
						    __FILE__, __LINE__, &mtr);
		}
	}

	modify = row_ins_must_modify(&cursor);

	if (modify != 0) {
		/* There is already an index entry with a long enough common
		prefix, we must convert the insert into a modify of an
		existing record */

		if (modify == ROW_INS_NEXT) {
			rec = page_rec_get_next(btr_cur_get_rec(&cursor));

			btr_cur_position(index, rec,
					 btr_cur_get_block(&cursor),&cursor);
		}

		if (dict_index_is_clust(index)) {
			err = row_ins_clust_index_entry_by_modify(
				mode, &cursor, &heap, &big_rec, entry,
				thr, &mtr);

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

				rec = btr_cur_get_rec(&cursor);
				offsets = rec_get_offsets(
					rec, index, NULL,
					ULINT_UNDEFINED, &heap);

				DEBUG_SYNC_C("before_row_ins_upd_extern");
				err = btr_store_big_rec_extern_fields(
					index, btr_cur_get_block(&cursor),
					rec, offsets, big_rec, &mtr,
					BTR_STORE_INSERT_UPDATE);
				DEBUG_SYNC_C("after_row_ins_upd_extern");
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
				goto stored_big_rec;
			}
		} else {
			ut_ad(!n_ext);
			err = row_ins_sec_index_entry_by_modify(
				mode, &cursor, entry, thr, &mtr);
		}
	} else {
		if (mode == BTR_MODIFY_LEAF) {
			err = btr_cur_optimistic_insert(
				0, &cursor, entry, &insert_rec, &big_rec,
				n_ext, thr, &mtr);
		} else {
			ut_a(mode == BTR_MODIFY_TREE);
			if (buf_LRU_buf_pool_running_out()) {

				err = DB_LOCK_TABLE_FULL;

				goto function_exit;
			}

			err = btr_cur_optimistic_insert(
				0, &cursor, entry, &insert_rec, &big_rec,
				n_ext, thr, &mtr);

			if (err == DB_FAIL) {
				err = btr_cur_pessimistic_insert(
					0, &cursor, entry, &insert_rec,
					&big_rec, n_ext, thr, &mtr);
			}
		}
	}

function_exit:
	mtr_commit(&mtr);

	if (UNIV_LIKELY_NULL(big_rec)) {
		DBUG_EXECUTE_IF(
			"row_ins_extern_checkpoint",
			log_make_checkpoint_at(IB_ULONGLONG_MAX, TRUE););

		mtr_start(&mtr);

		DEBUG_SYNC_C("before_row_ins_extern_latch");
		btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
					    BTR_MODIFY_TREE, &cursor, 0,
					    __FILE__, __LINE__, &mtr);
		rec = btr_cur_get_rec(&cursor);
		offsets = rec_get_offsets(rec, index, NULL,
					  ULINT_UNDEFINED, &heap);

		DEBUG_SYNC_C("before_row_ins_extern");
		err = btr_store_big_rec_extern_fields(
			index, btr_cur_get_block(&cursor),
			rec, offsets, big_rec, &mtr, BTR_STORE_INSERT);
		DEBUG_SYNC_C("after_row_ins_extern");

stored_big_rec:
		if (modify) {
			dtuple_big_rec_free(big_rec);
		} else {
			dtuple_convert_back_big_rec(index, entry, big_rec);
		}

		mtr_commit(&mtr);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
}

/***************************************************************//**
Inserts an index entry to index. Tries first optimistic, then pessimistic
descent down the tree. If the entry matches enough to a delete marked record,
performs the insert by updating or delete unmarking the delete marked
record.
@return	DB_SUCCESS, DB_LOCK_WAIT, DB_DUPLICATE_KEY, or some other error code */
UNIV_INTERN
ulint
row_ins_index_entry(
/*================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in/out: index entry to insert */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	ibool		foreign,/*!< in: TRUE=check foreign key constraints
				(foreign=FALSE only during CREATE INDEX) */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint	err;

	if (foreign && UT_LIST_GET_FIRST(index->table->foreign_list)) {
		err = row_ins_check_foreign_constraints(index->table, index,
							entry, thr);
		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	/* Try first optimistic descent to the B-tree */

	err = row_ins_index_entry_low(BTR_MODIFY_LEAF, index, entry,
				      n_ext, thr);
	if (err != DB_FAIL) {

		return(err);
	}

	/* Try then pessimistic descent to the B-tree */

	err = row_ins_index_entry_low(BTR_MODIFY_TREE, index, entry,
				      n_ext, thr);
	return(err);
}

/***********************************************************//**
Sets the values of the dtuple fields in entry from the values of appropriate
columns in row. */
static
void
row_ins_index_entry_set_vals(
/*=========================*/
	dict_index_t*	index,	/*!< in: index */
	dtuple_t*	entry,	/*!< in: index entry to make */
	const dtuple_t*	row)	/*!< in: row */
{
	ulint	n_fields;
	ulint	i;

	ut_ad(entry && row);

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
				col->prtype, col->mbminlen, col->mbmaxlen,
				ind_field->prefix_len,
				len, dfield_get_data(row_field));

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
static
ulint
row_ins_index_entry_step(
/*=====================*/
	ins_node_t*	node,	/*!< in: row insert node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint	err;

	ut_ad(dtuple_check_typed(node->row));

	row_ins_index_entry_set_vals(node->index, node->entry, node->row);

	ut_ad(dtuple_check_typed(node->entry));

	err = row_ins_index_entry(node->index, node->entry, 0, TRUE, thr);

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
	dulint	row_id;

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
static
ulint
row_ins(
/*====*/
	ins_node_t*	node,	/*!< in: row insert node */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint	err;

	ut_ad(node && thr);

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
		err = row_ins_index_entry_step(node, thr);

		if (err != DB_SUCCESS) {

			return(err);
		}

		node->index = dict_table_get_next_index(node->index);
		node->entry = UT_LIST_GET_NEXT(tuple_list, node->entry);
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
	ulint		err;

	ut_ad(thr);

	trx = thr_get_trx(thr);

	trx_start_if_not_started(trx);

	node = thr->run_node;

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

		/* It may be that the current session has not yet started
		its transaction, or it has been committed: */

		if (UT_DULINT_EQ(trx->id, node->trx_id)) {
			/* No need to do IX-locking */

			goto same_trx;
		}

		err = lock_table(0, node->table, LOCK_IX, thr);

		if (err != DB_SUCCESS) {

			goto error_handling;
		}

		node->trx_id = trx->id;
same_trx:
		node->state = INS_NODE_ALLOC_ROW_ID;

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
