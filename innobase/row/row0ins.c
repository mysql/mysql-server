/******************************************************
Insert into a table

(c) 1996 Innobase Oy

Created 4/20/1996 Heikki Tuuri
*******************************************************/

#include "row0ins.h"

#ifdef UNIV_NONINL
#include "row0ins.ic"
#endif

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

#define	ROW_INS_PREV	1
#define	ROW_INS_NEXT	2


/*********************************************************************
This prototype is copied from /mysql/sql/ha_innodb.cc.
Invalidates the MySQL query cache for the table.
NOTE that the exact prototype of this function has to be in
/innobase/row/row0ins.c! */
extern
void
innobase_invalidate_query_cache(
/*============================*/
	trx_t*	trx,		/* in: transaction which modifies the table */
	char*	full_name,	/* in: concatenation of database name, null
				char '\0', table name, null char'\0';
				NOTE that in Windows this is always
				in LOWER CASE! */
	ulint	full_name_len);	/* in: full name length where also the null
				chars count */


/*************************************************************************
Creates an insert node struct. */

ins_node_t*
ins_node_create(
/*============*/
					/* out, own: insert node struct */
	ulint		ins_type,	/* in: INS_VALUES, ... */
	dict_table_t*	table, 		/* in: table where to insert */
	mem_heap_t*	heap)		/* in: mem heap where created */
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

/***************************************************************
Creates an entry template for each index of a table. */
static
void
ins_node_create_entry_list(
/*=======================*/
	ins_node_t*	node)	/* in: row insert node */
{
	dict_index_t*	index;
	dtuple_t*	entry;

	ut_ad(node->entry_sys_heap);

	UT_LIST_INIT(node->entry_list);

	index = dict_table_get_first_index(node->table);
	
	while (index != NULL) {
		entry = row_build_index_entry(node->row, index,
							node->entry_sys_heap);
		UT_LIST_ADD_LAST(tuple_list, node->entry_list, entry);

		index = dict_table_get_next_index(index);
	}
}

/*********************************************************************
Adds system field buffers to a row. */
static
void
row_ins_alloc_sys_fields(
/*=====================*/
	ins_node_t*	node)	/* in: insert node */
{
	dtuple_t*	row;
	dict_table_t*	table;
	mem_heap_t*	heap;
	dict_col_t*	col;
	dfield_t*	dfield;
	ulint		len;
	byte*		ptr;

	row = node->row;
	table = node->table;
	heap = node->entry_sys_heap;

	ut_ad(row && table && heap);
	ut_ad(dtuple_get_n_fields(row) == dict_table_get_n_cols(table));

	/* 1. Allocate buffer for row id */

	col = dict_table_get_sys_col(table, DATA_ROW_ID);
	
	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));

	ptr = mem_heap_alloc(heap, DATA_ROW_ID_LEN);
				
	dfield_set_data(dfield, ptr, DATA_ROW_ID_LEN);

	node->row_id_buf = ptr;

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {

		/* 2. Fill in the dfield for mix id */

		col = dict_table_get_sys_col(table, DATA_MIX_ID);
	
		dfield = dtuple_get_nth_field(row, dict_col_get_no(col));

		len = mach_dulint_get_compressed_size(table->mix_id);
		ptr = mem_heap_alloc(heap, DATA_MIX_ID_LEN);
				
		mach_dulint_write_compressed(ptr, table->mix_id);
		dfield_set_data(dfield, ptr, len);
	}

	/* 3. Allocate buffer for trx id */

	col = dict_table_get_sys_col(table, DATA_TRX_ID);
	
	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = mem_heap_alloc(heap, DATA_TRX_ID_LEN);
				
	dfield_set_data(dfield, ptr, DATA_TRX_ID_LEN);

	node->trx_id_buf = ptr;

	/* 4. Allocate buffer for roll ptr */

	col = dict_table_get_sys_col(table, DATA_ROLL_PTR);
	
	dfield = dtuple_get_nth_field(row, dict_col_get_no(col));
	ptr = mem_heap_alloc(heap, DATA_ROLL_PTR_LEN);
				
	dfield_set_data(dfield, ptr, DATA_ROLL_PTR_LEN);
}

/*************************************************************************
Sets a new row to insert for an INS_DIRECT node. This function is only used
if we have constructed the row separately, which is a rare case; this
function is quite slow. */

void
ins_node_set_new_row(
/*=================*/
	ins_node_t*	node,	/* in: insert node */
	dtuple_t*	row)	/* in: new row (or first row) for the node */
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

/***********************************************************************
Does an insert operation by updating a delete-marked existing record
in the index. This situation can occur if the delete-marked record is
kept in the index for consistent reads. */
static
ulint
row_ins_sec_index_entry_by_modify(
/*==============================*/
				/* out: DB_SUCCESS or error code */
	ulint		mode,	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/* in: B-tree cursor */
	dtuple_t*	entry,	/* in: index entry to insert */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	big_rec_t*	dummy_big_rec;
	mem_heap_t*	heap;
	upd_t*		update;
	rec_t*		rec;
	ulint		err;
	
	rec = btr_cur_get_rec(cursor);
	
	ut_ad((cursor->index->type & DICT_CLUSTERED) == 0);
	ut_ad(rec_get_deleted_flag(rec));
	
	/* We know that in the alphabetical ordering, entry and rec are
	identified. But in their binary form there may be differences if
	there are char fields in them. Therefore we have to calculate the
	difference. */
	
	heap = mem_heap_create(1024);
	
	update = row_upd_build_sec_rec_difference_binary(cursor->index,
				entry, rec, thr_get_trx(thr), heap);
	if (mode == BTR_MODIFY_LEAF) {
		/* Try an optimistic updating of the record, keeping changes
		within the page */

		err = btr_cur_optimistic_update(BTR_KEEP_SYS_FLAG, cursor,
						update, 0, thr, mtr);
		if (err == DB_OVERFLOW || err == DB_UNDERFLOW) {
			err = DB_FAIL;
		}
	} else  {
		ut_a(mode == BTR_MODIFY_TREE);
		err = btr_cur_pessimistic_update(BTR_KEEP_SYS_FLAG, cursor,
					&dummy_big_rec, update, 0, thr, mtr);
	}

	mem_heap_free(heap);

	return(err);
}

/***********************************************************************
Does an insert operation by delete unmarking and updating a delete marked
existing record in the index. This situation can occur if the delete marked
record is kept in the index for consistent reads. */
static
ulint
row_ins_clust_index_entry_by_modify(
/*================================*/
				/* out: DB_SUCCESS, DB_FAIL, or error code */
	ulint		mode,	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether mtr holds just a leaf
				latch or also a tree latch */
	btr_cur_t*	cursor,	/* in: B-tree cursor */
	big_rec_t**	big_rec,/* out: possible big rec vector of fields
				which have to be stored externally by the
				caller */
	dtuple_t*	entry,	/* in: index entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	mem_heap_t*	heap;
	rec_t*		rec;
	upd_t*		update;
	ulint		err;
	
	ut_ad(cursor->index->type & DICT_CLUSTERED);
	
	*big_rec = NULL;

	rec = btr_cur_get_rec(cursor);

	ut_ad(rec_get_deleted_flag(rec));	

	heap = mem_heap_create(1024);
	
	/* Build an update vector containing all the fields to be modified;
	NOTE that this vector may NOT contain system columns trx_id or
	roll_ptr */
	
	update = row_upd_build_difference_binary(cursor->index, entry, ext_vec,
			n_ext_vec, rec, thr_get_trx(thr), heap);
	if (mode == BTR_MODIFY_LEAF) {
		/* Try optimistic updating of the record, keeping changes
		within the page */

		err = btr_cur_optimistic_update(0, cursor, update, 0, thr,
								   mtr);
		if (err == DB_OVERFLOW || err == DB_UNDERFLOW) {
			err = DB_FAIL;
		}
	} else  {
		ut_a(mode == BTR_MODIFY_TREE);
		err = btr_cur_pessimistic_update(0, cursor, big_rec, update,
								0, thr, mtr);
	}
	
	mem_heap_free(heap);

	return(err);
}

/*************************************************************************
Returns TRUE if in a cascaded update/delete an ancestor node of node
updates (not DELETE, but UPDATE) table. */
static
ibool
row_ins_cascade_ancestor_updates_table(
/*===================================*/
				/* out: TRUE if an ancestor updates table */
	que_node_t*	node,	/* in: node in a query graph */
	dict_table_t*	table)	/* in: table */
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
	
/*************************************************************************
Returns the number of ancestor UPDATE or DELETE nodes of a
cascaded update/delete node. */
static
ulint
row_ins_cascade_n_ancestors(
/*========================*/
				/* out: number of ancestors */
	que_node_t*	node)	/* in: node in a query graph */
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
	
/**********************************************************************
Calculates the update vector node->cascade->update for a child table in
a cascaded update. */
static
ulint
row_ins_cascade_calc_update_vec(
/*============================*/
					/* out: number of fields in the
					calculated update vector; the value
					can also be 0 if no foreign key
					fields changed; the returned value
					is ULINT_UNDEFINED if the column
					type in the child table is too short
					to fit the new value in the parent
					table: that means the update fails */
	upd_node_t*	node,		/* in: update node of the parent
					table */
	dict_foreign_t*	foreign,	/* in: foreign key constraint whose
					type is != 0 */
	mem_heap_t*	heap)		/* in: memory heap to use as
					temporary storage */
{
	upd_node_t*	cascade		= node->cascade_node;
	dict_table_t*	table		= foreign->foreign_table;
	dict_index_t*	index		= foreign->foreign_index;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_table_t*	parent_table;
	dict_index_t*	parent_index;
	upd_t*		parent_update;
	upd_field_t*	parent_ufield;
	ulint		n_fields_updated;
	ulint           parent_field_no;
	dtype_t*	type;
	ulint		i;
	ulint		j;
	    	
	ut_a(node && foreign && cascade && table && index);

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
					dict_index_get_nth_col_no(
							parent_index, i));

		for (j = 0; j < parent_update->n_fields; j++) {
			parent_ufield = parent_update->fields + j;
		
			if (parent_ufield->field_no == parent_field_no) {

				/* A field in the parent index record is
				updated. Let us make the update vector
				field for the child table. */

 				ufield = update->fields + n_fields_updated;

				ufield->field_no =
					dict_table_get_nth_col_pos(table,
					dict_index_get_nth_col_no(index, i));
				ufield->exp = NULL;

				ufield->new_val = parent_ufield->new_val;

				type = dict_index_get_nth_type(index, i);

				/* Do not allow a NOT NULL column to be
				updated as NULL */

				if (ufield->new_val.len == UNIV_SQL_NULL
				    && (type->prtype & DATA_NOT_NULL)) {

				        return(ULINT_UNDEFINED);
				}

				/* If the new value would not fit in the
				column, do not allow the update */

				if (ufield->new_val.len != UNIV_SQL_NULL
				    && ufield->new_val.len
				       > dtype_get_len(type)) {

				        return(ULINT_UNDEFINED);
				}

				/* If the parent column type has a different
				length than the child column type, we may
				need to pad with spaces the new value of the
				child column */

				if (dtype_is_fixed_size(type)
				    && ufield->new_val.len != UNIV_SQL_NULL
				    && ufield->new_val.len
				       < dtype_get_fixed_size(type)) {

				        ufield->new_val.data =
						mem_heap_alloc(heap,
						  dtype_get_fixed_size(type));
					ufield->new_val.len = 
						dtype_get_fixed_size(type);
					ut_a(dtype_get_pad_char(type)
					     != ULINT_UNDEFINED);

					memset(ufield->new_val.data,
					       (byte)dtype_get_pad_char(type),
					       dtype_get_fixed_size(type));
					ut_memcpy(ufield->new_val.data,
						parent_ufield->new_val.data,
						parent_ufield->new_val.len);
				}

				ufield->extern_storage = FALSE;

				n_fields_updated++;
			}
		}
	}

	update->n_fields = n_fields_updated;

	return(n_fields_updated);
}

/*************************************************************************
Reports a foreign key error associated with an update or a delete of a
parent table index entry. */
static
void
row_ins_foreign_report_err(
/*=======================*/
	const char*	errstr,		/* in: error string from the viewpoint
					of the parent table */
	que_thr_t*	thr,		/* in: query thread whose run_node
					is an update node */
	dict_foreign_t*	foreign,	/* in: foreign key constraint */
	rec_t*		rec,		/* in: a matching index record in the
					child table */
	dtuple_t*	entry)		/* in: index entry in the parent
					table */
{
	FILE*	ef	= dict_foreign_err_file;
	trx_t*	trx	= thr_get_trx(thr);

	mutex_enter(&dict_foreign_err_mutex);
	rewind(ef);
	ut_print_timestamp(ef);
	fputs(" Transaction:\n", ef);
	trx_print(ef, trx);

	fputs("Foreign key constraint fails for table ", ef);
	ut_print_name(ef, trx, foreign->foreign_table_name);
	fputs(":\n", ef);
	dict_print_info_on_foreign_key_in_create_format(ef, trx, foreign);
	putc('\n', ef);
	fputs(errstr, ef);
	fputs(" in parent table, in index ", ef);
	ut_print_name(ef, trx, foreign->referenced_index->name);
	if (entry) {
		fputs(" tuple:\n", ef);
		dtuple_print(ef, entry);
	}
	fputs("\nBut in child table ", ef);
	ut_print_name(ef, trx, foreign->foreign_table_name);
	fputs(", in index ", ef);
	ut_print_name(ef, trx, foreign->foreign_index->name);
	if (rec) {
		fputs(", there is a record:\n", ef);
		rec_print(ef, rec);
	} else {
		fputs(", the record is not available\n", ef);
	}
	putc('\n', ef);

	mutex_exit(&dict_foreign_err_mutex);
}

/*************************************************************************
Reports a foreign key error to dict_foreign_err_buf when we are trying
to add an index entry to a child table. Note that the adding may be the result
of an update, too. */
static
void
row_ins_foreign_report_add_err(
/*===========================*/
	trx_t*		trx,		/* in: transaction */
	dict_foreign_t*	foreign,	/* in: foreign key constraint */
	rec_t*		rec,		/* in: a record in the parent table:
					it does not match entry because we
					have an error! */
	dtuple_t*	entry)		/* in: index entry to insert in the
					child table */
{
	FILE*	ef	= dict_foreign_err_file;

	mutex_enter(&dict_foreign_err_mutex);
	rewind(ef);
	ut_print_timestamp(ef);
	fputs(" Transaction:\n", ef);
	trx_print(ef, trx);
	fputs("Foreign key constraint fails for table ", ef);
	ut_print_name(ef, trx, foreign->foreign_table_name);
	fputs(":\n", ef);
	dict_print_info_on_foreign_key_in_create_format(ef, trx, foreign);
	fputs("\nTrying to add in child table, in index ", ef);
	ut_print_name(ef, trx, foreign->foreign_index->name);
	if (entry) {
		fputs(" tuple:\n", ef);
		dtuple_print(ef, entry);
	}
	fputs("\nBut in parent table ", ef);
	ut_print_name(ef, trx, foreign->referenced_table_name);
	fputs(", in index ", ef);
	ut_print_name(ef, trx, foreign->referenced_index->name);
	fputs(",\nthe closest match we can find is record:\n", ef);
	if (rec && page_rec_is_supremum(rec)) {
		/* If the cursor ended on a supremum record, it is better
		to report the previous record in the error message, so that
		the user gets a more descriptive error message. */
		rec = page_rec_get_prev(rec);
	}

	if (rec) {
		rec_print(ef, rec);
	}
	putc('\n', ef);

	mutex_exit(&dict_foreign_err_mutex);
}

/*************************************************************************
Invalidate the query cache for the given table. */
static
void
row_ins_invalidate_query_cache(
/*===========================*/
	que_thr_t*	thr,		/* in: query thread whose run_node
					is an update node */
	const char*	name)		/* in: table name prefixed with
					database name and a '/' character */
{
	char*	buf;
	char*	ptr;
	ulint	len = strlen(name) + 1;

	buf = mem_strdupl(name, len);

	ptr = strchr(buf, '/');
	ut_a(ptr);
	*ptr = '\0';

	/* We call a function in ha_innodb.cc */
#ifndef UNIV_HOTBACKUP
	innobase_invalidate_query_cache(thr_get_trx(thr), buf, len);
#endif
	mem_free(buf);
}

/*************************************************************************
Perform referential actions or checks when a parent row is deleted or updated
and the constraint had an ON DELETE or ON UPDATE condition which was not
RESTRICT. */
static
ulint
row_ins_foreign_check_on_constraint(
/*================================*/
					/* out: DB_SUCCESS, DB_LOCK_WAIT,
					or error code */
	que_thr_t*	thr,		/* in: query thread whose run_node
					is an update node */
	dict_foreign_t*	foreign,	/* in: foreign key constraint whose
					type is != 0 */
	btr_pcur_t*	pcur,		/* in: cursor placed on a matching
					index record in the child table */
	dtuple_t*	entry,		/* in: index entry in the parent
					table */
	mtr_t*		mtr)		/* in: mtr holding the latch of pcur
					page */
{
	upd_node_t*	node;
	upd_node_t*	cascade;
	dict_table_t*	table		= foreign->foreign_table;
	dict_index_t*	index;
	dict_index_t*	clust_index;
	dtuple_t*	ref;
	mem_heap_t*	tmp_heap;
	mem_heap_t*	upd_vec_heap	= NULL;
	rec_t*		rec;
	rec_t*		clust_rec;
	upd_t*		update;
	ulint		n_to_update;
	ulint		err;
	ulint		i;
	trx_t*		trx;

	
	ut_a(thr && foreign && pcur && mtr);

	trx = thr_get_trx(thr);

	/* Since we are going to delete or update a row, we have to invalidate
	the MySQL query cache for table */

	row_ins_invalidate_query_cache(thr, table->name);

	node = thr->run_node;

	if (node->is_delete && 0 == (foreign->type &
			(DICT_FOREIGN_ON_DELETE_CASCADE
			 | DICT_FOREIGN_ON_DELETE_SET_NULL))) {

		row_ins_foreign_report_err("Trying to delete",
					thr, foreign,
					btr_pcur_get_rec(pcur), entry);

	        return(DB_ROW_IS_REFERENCED);
	}

	if (!node->is_delete && 0 == (foreign->type &
			(DICT_FOREIGN_ON_UPDATE_CASCADE
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
"Trying an update, possibly causing a cyclic cascaded update\n"
"in the child table,", thr, foreign, btr_pcur_get_rec(pcur), entry);

		goto nonstandard_exit_func;
	}

	if (row_ins_cascade_n_ancestors(cascade) >= 15) {
		err = DB_ROW_IS_REFERENCED;

		row_ins_foreign_report_err(
(char*)"Trying a too deep cascaded delete or update\n",
			thr, foreign, btr_pcur_get_rec(pcur), entry);

		goto nonstandard_exit_func;
	}

	index = btr_pcur_get_btr_cur(pcur)->index;

	ut_a(index == foreign->foreign_index);
	
	rec = btr_pcur_get_rec(pcur);

	if (index->type & DICT_CLUSTERED) {
		/* pcur is already positioned in the clustered index of
		the child table */
	
		clust_index = index;
		clust_rec = rec;
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

		mem_heap_free(tmp_heap);

		clust_rec = btr_pcur_get_rec(cascade->pcur);

		if (!page_rec_is_user_rec(clust_rec)
		    || btr_pcur_get_low_match(cascade->pcur)
		       < dict_index_get_n_unique(clust_index)) {

			fputs(
			"InnoDB: error in cascade of a foreign key op\n"
			"InnoDB: ", stderr);
			dict_index_name_print(stderr, trx, index);

			fputs("\n"
				"InnoDB: record ", stderr);
			rec_print(stderr, rec);
			fputs("\n"
				"InnoDB: clustered record ", stderr);
			rec_print(stderr, clust_rec);
			fputs("\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n", stderr);

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
		
		err = lock_clust_rec_read_check_and_lock(0, clust_rec,
				clust_index, LOCK_X, LOCK_REC_NOT_GAP, thr);
	}
	
	if (err != DB_SUCCESS) {

		goto nonstandard_exit_func;
	}

	if (rec_get_deleted_flag(clust_rec)) {
		/* This can happen if there is a circular reference of
		rows such that cascading delete comes to delete a row
		already in the process of being delete marked */
		err = DB_SUCCESS;		

		goto nonstandard_exit_func;
	}

	if ((node->is_delete
	    && (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL))
	   || (!node->is_delete
	    && (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL))) {
	    	
		/* Build the appropriate update vector which sets
		foreign->n_fields first fields in rec to SQL NULL */

		update = cascade->update;

		update->info_bits = 0;
		update->n_fields = foreign->n_fields;
		
		for (i = 0; i < foreign->n_fields; i++) {
			(update->fields + i)->field_no
				= dict_table_get_nth_col_pos(table,
					dict_index_get_nth_col_no(index, i));
			(update->fields + i)->exp = NULL;
			(update->fields + i)->new_val.len = UNIV_SQL_NULL;
			(update->fields + i)->new_val.data = NULL;
			(update->fields + i)->extern_storage = FALSE;
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
"Trying a cascaded update where the updated value in the child\n"
"table would not fit in the length of the column, or the value would\n"
"be NULL and the column is declared as not NULL in the child table,",
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
	mtr_start(mtr);

	/* Restore pcur position */
	
	btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

	if (upd_vec_heap) {
	        mem_heap_free(upd_vec_heap);
	}

	return(err);

nonstandard_exit_func:

	if (upd_vec_heap) {
	        mem_heap_free(upd_vec_heap);
	}

	btr_pcur_store_position(pcur, mtr);

	mtr_commit(mtr);
	mtr_start(mtr);

	btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

	return(err);
}

/*************************************************************************
Sets a shared lock on a record. Used in locking possible duplicate key
records and also in checking foreign key constraints. */
static
ulint
row_ins_set_shared_rec_lock(
/*========================*/
				/* out: DB_SUCCESS or error code */
	ulint		type, 	/* in: LOCK_ORDINARY, LOCK_GAP, or
				LOCK_REC_NOT_GAP type lock */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index */
	que_thr_t*	thr)	/* in: query thread */	
{
	ulint	err;

	if (index->type & DICT_CLUSTERED) {
		err = lock_clust_rec_read_check_and_lock(0, rec, index, LOCK_S,
								type, thr);
	} else {
		err = lock_sec_rec_read_check_and_lock(0, rec, index, LOCK_S,
								type, thr);
	}

	return(err);
}
	
/*******************************************************************
Checks if foreign key constraint fails for an index entry. Sets shared locks
which lock either the success or the failure of the constraint. NOTE that
the caller must have a shared latch on dict_operation_lock. */

ulint
row_ins_check_foreign_constraint(
/*=============================*/
				/* out: DB_SUCCESS,
				DB_NO_REFERENCED_ROW,
				or DB_ROW_IS_REFERENCED */
	ibool		check_ref,/* in: TRUE if we want to check that
				the referenced table is ok, FALSE if we
				want to to check the foreign key table */
	dict_foreign_t*	foreign,/* in: foreign constraint; NOTE that the
				tables mentioned in it must be in the
				dictionary cache if they exist at all */
	dict_table_t*	table,	/* in: if check_ref is TRUE, then the foreign
				table, else the referenced table */
	dtuple_t*	entry,	/* in: index entry for index */
	que_thr_t*	thr)	/* in: query thread */
{
  	upd_node_t*  	upd_node;
	dict_table_t*	check_table;
	dict_index_t*	check_index;
	ulint		n_fields_cmp;
	ibool		unique_search;
	rec_t*		rec;
	btr_pcur_t	pcur;
	ibool		moved;
	int		cmp;
	ulint		err;
	ulint		i;
	mtr_t		mtr;
	trx_t*		trx = thr_get_trx(thr);

run_again:
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */

	err = DB_SUCCESS;

	if (trx->check_foreigns == FALSE) {
		/* The user has suppressed foreign key checks currently for
		this session */

		return(DB_SUCCESS);
	}

	/* If any of the foreign key fields in entry is SQL NULL, we
	suppress the foreign key check: this is compatible with Oracle,
	for example */

	for (i = 0; i < foreign->n_fields; i++) {
		if (UNIV_SQL_NULL == dfield_get_len(
                                         dtuple_get_nth_field(entry, i))) {

			return(DB_SUCCESS);
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
		
		        return(DB_SUCCESS);
		}
	}

	if (check_ref) {
		check_table = foreign->referenced_table;
		check_index = foreign->referenced_index;
	} else {
		check_table = foreign->foreign_table;
		check_index = foreign->foreign_index;
	}

	if (check_table == NULL) {
		if (check_ref) {
			FILE*	ef = dict_foreign_err_file;
			mutex_enter(&dict_foreign_err_mutex);
			rewind(ef);
			ut_print_timestamp(ef);
			fputs(" Transaction:\n", ef);
			trx_print(ef, trx);
			fputs("Foreign key constraint fails for table ", ef);
			ut_print_name(ef, trx, foreign->foreign_table_name);
			fputs(":\n", ef);
			dict_print_info_on_foreign_key_in_create_format(ef,
					trx, foreign);
			fputs("\nTrying to add to index ", ef);
			ut_print_name(ef, trx, foreign->foreign_index->name);
			fputs(" tuple:\n", ef);
			dtuple_print(ef, entry);
			fputs("\nBut the parent table ", ef);
			ut_print_name(ef, trx, foreign->referenced_table_name);
			fputs(" does not currently exist!\n", ef);
			mutex_exit(&dict_foreign_err_mutex);

			return(DB_NO_REFERENCED_ROW);
		}

		return(DB_SUCCESS);
	}

	ut_a(check_table && check_index);

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

	if (dict_index_get_n_unique(check_index) <= foreign->n_fields) {
		/* We can just set a LOCK_REC_NOT_GAP type lock */
	
		unique_search = TRUE;
	} else {
		unique_search = FALSE;
	}

	btr_pcur_open(check_index, entry, PAGE_CUR_GE,
					BTR_SEARCH_LEAF, &pcur, &mtr);

	/* Scan index records and check if there is a matching record */

	for (;;) {
		rec = btr_pcur_get_rec(&pcur);

		if (rec == page_get_infimum_rec(buf_frame_align(rec))) {

			goto next_rec;
		}
		
		if (rec == page_get_supremum_rec(buf_frame_align(rec))) {
		
			err = row_ins_set_shared_rec_lock(LOCK_ORDINARY, rec,
							check_index, thr);
			if (err != DB_SUCCESS) {

				break;
			}

			goto next_rec;
		}

		cmp = cmp_dtuple_rec(entry, rec);

		if (cmp == 0) {
			if (rec_get_deleted_flag(rec)) {
				err = row_ins_set_shared_rec_lock(
							LOCK_ORDINARY,
							rec, check_index, thr);
				if (err != DB_SUCCESS) {

					break;
				}
			} else {
				/* Found a matching record */
				
				if (unique_search) {
					err = row_ins_set_shared_rec_lock(
							LOCK_REC_NOT_GAP,
							rec, check_index, thr);
				} else {
					err = row_ins_set_shared_rec_lock(
							LOCK_ORDINARY,
							rec, check_index, thr);
				}
				
				if (err != DB_SUCCESS) {

					break;
				}

				if (check_ref) {			
					err = DB_SUCCESS;

					break;
				} else if (foreign->type != 0) {
					/* There is an ON UPDATE or ON DELETE
					condition: check them in a separate
					function */

					err =
					  row_ins_foreign_check_on_constraint(
						thr, foreign, &pcur, entry,
									&mtr);
					if (err != DB_SUCCESS) {

						break;
					}
				} else {
					row_ins_foreign_report_err(
						"Trying to delete or update",
						thr, foreign, rec, entry);

					err = DB_ROW_IS_REFERENCED;
					break;
				}
			}
		}

		if (cmp < 0) {
			err = row_ins_set_shared_rec_lock(LOCK_GAP,
						rec, check_index, thr);
			if (err != DB_SUCCESS) {

				break;
			}

			if (check_ref) {			
				err = DB_NO_REFERENCED_ROW;
				row_ins_foreign_report_add_err(
					trx, foreign, rec, entry);
			} else {
				err = DB_SUCCESS;
			}

			break;
		}

		ut_a(cmp == 0);
next_rec:
		moved = btr_pcur_move_to_next(&pcur, &mtr);

		if (!moved) {
			if (check_ref) {			
				rec = btr_pcur_get_rec(&pcur);
				row_ins_foreign_report_add_err(
					trx, foreign, rec, entry);
				err = DB_NO_REFERENCED_ROW;
			} else {
				err = DB_SUCCESS;
			}

			break;
		}
	}

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

	return(err);
}

/*******************************************************************
Checks if foreign key constraints fail for an index entry. If index
is not mentioned in any constraint, this function does nothing,
Otherwise does searches to the indexes of referenced tables and
sets shared locks which lock either the success or the failure of
a constraint. */
static
ulint
row_ins_check_foreign_constraints(
/*==============================*/
				/* out: DB_SUCCESS or error code */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry for index */
	que_thr_t*	thr)	/* in: query thread */
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
									trx);
			}

			if (0 == trx->dict_operation_lock_mode) {
				got_s_lock = TRUE;

				row_mysql_freeze_data_dictionary(trx);
			}

			err = row_ins_check_foreign_constraint(TRUE, foreign,
						table, entry, thr);
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

/*******************************************************************
Checks if a unique key violation to rec would occur at the index entry
insert. */
static
ibool
row_ins_dupl_error_with_rec(
/*========================*/
				/* out: TRUE if error */
	rec_t*		rec,	/* in: user record; NOTE that we assume
				that the caller already has a record lock on
				the record! */
	dtuple_t*	entry,	/* in: entry to insert */
	dict_index_t*	index)	/* in: index */
{
	ulint	matched_fields;
	ulint	matched_bytes;
	ulint	n_unique;
	ulint   i;
	
	n_unique = dict_index_get_n_unique(index);

	matched_fields = 0;
	matched_bytes = 0;

	cmp_dtuple_rec_with_match(entry, rec, &matched_fields, &matched_bytes);

	if (matched_fields < n_unique) {

	        return(FALSE);
	}

	/* In a unique secondary index we allow equal key values if they
	contain SQL NULLs */

	if (!(index->type & DICT_CLUSTERED)) {

	        for (i = 0; i < n_unique; i++) {
	                if (UNIV_SQL_NULL == dfield_get_len(
                                         dtuple_get_nth_field(entry, i))) {

	                        return(FALSE);
	                }
	        }
	}

	if (!rec_get_deleted_flag(rec)) {

	        return(TRUE);
	}

	return(FALSE);
}	

/*******************************************************************
Scans a unique non-clustered index at a given index entry to determine
whether a uniqueness violation has occurred for the key value of the entry.
Set shared locks on possible duplicate records. */
static
ulint
row_ins_scan_sec_index_for_duplicate(
/*=================================*/
				/* out: DB_SUCCESS, DB_DUPLICATE_KEY, or
				DB_LOCK_WAIT */
	dict_index_t*	index,	/* in: non-clustered unique index */
	dtuple_t*	entry,	/* in: index entry */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint		n_unique;
	ulint		i;
	int		cmp;
	ulint		n_fields_cmp;
	rec_t*		rec;
	btr_pcur_t	pcur;
	ulint		err		= DB_SUCCESS;
	ibool		moved;
	mtr_t		mtr;

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

	/* Scan index records and check if there is a duplicate */

	for (;;) {
		rec = btr_pcur_get_rec(&pcur);

		if (rec == page_get_infimum_rec(buf_frame_align(rec))) {

			goto next_rec;
		}
				
		/* Try to place a lock on the index record */

		err = row_ins_set_shared_rec_lock(LOCK_ORDINARY, rec, index,
									thr);

		if (err != DB_SUCCESS) {

			break;
		}

		if (rec == page_get_supremum_rec(buf_frame_align(rec))) {
		
			goto next_rec;
		}

		cmp = cmp_dtuple_rec(entry, rec);

		if (cmp == 0) {
			if (row_ins_dupl_error_with_rec(rec, entry, index)) {
				err = DB_DUPLICATE_KEY;

				thr_get_trx(thr)->error_info = index;

				break;
			}
		}

		if (cmp < 0) {
			break;
		}

		ut_a(cmp == 0);
next_rec:
		moved = btr_pcur_move_to_next(&pcur, &mtr);

		if (!moved) {
			break;
		}
	}

	mtr_commit(&mtr);

	/* Restore old value */
	dtuple_set_n_fields_cmp(entry, n_fields_cmp);

	return(err);
}

/*******************************************************************
Checks if a unique key violation error would occur at an index entry
insert. Sets shared locks on possible duplicate records. Works only
for a clustered index! */
static
ulint
row_ins_duplicate_error_in_clust(
/*=============================*/
				/* out: DB_SUCCESS if no error,
				DB_DUPLICATE_KEY if error, DB_LOCK_WAIT if we
				have to wait for a lock on a possible
				duplicate record */
	btr_cur_t*	cursor,	/* in: B-tree cursor */
	dtuple_t*	entry,	/* in: entry to insert */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	ulint	err;
	rec_t*	rec;
	page_t*	page;
	ulint	n_unique;
	trx_t*	trx	= thr_get_trx(thr);

	UT_NOT_USED(mtr);
	
	ut_a(cursor->index->type & DICT_CLUSTERED);
	ut_ad(cursor->index->type & DICT_UNIQUE);

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
		page = buf_frame_align(rec);

		if (rec != page_get_infimum_rec(page)) {

			/* We set a lock on the possible duplicate: this
			is needed in logical logging of MySQL to make
			sure that in roll-forward we get the same duplicate
			errors as in original execution */
		
			err = row_ins_set_shared_rec_lock(LOCK_REC_NOT_GAP,
						rec, cursor->index, thr);
			if (err != DB_SUCCESS) {
					
				return(err);
			}

			if (row_ins_dupl_error_with_rec(rec, entry,
							cursor->index)) {
				trx->error_info = cursor->index;
				return(DB_DUPLICATE_KEY);
			}
		}
	}

	if (cursor->up_match >= n_unique) {

		rec = page_rec_get_next(btr_cur_get_rec(cursor));
		page = buf_frame_align(rec);

		if (rec != page_get_supremum_rec(page)) {

			err = row_ins_set_shared_rec_lock(LOCK_REC_NOT_GAP,
						rec, cursor->index, thr);
			if (err != DB_SUCCESS) {
					
				return(err);
			}

			if (row_ins_dupl_error_with_rec(rec, entry,
							cursor->index)) {
				trx->error_info = cursor->index;
				return(DB_DUPLICATE_KEY);
			}
		}

		ut_a(!(cursor->index->type & DICT_CLUSTERED));
						/* This should never happen */
	}

	return(DB_SUCCESS);
}

/*******************************************************************
Checks if an index entry has long enough common prefix with an existing
record so that the intended insert of the entry must be changed to a modify of
the existing record. In the case of a clustered index, the prefix must be
n_unique fields long, and in the case of a secondary index, all fields must be
equal. */
UNIV_INLINE
ulint
row_ins_must_modify(
/*================*/
				/* out: 0 if no update, ROW_INS_PREV if
				previous should be updated; currently we
				do the search so that only the low_match
				record can match enough to the search tuple,
				not the next record */
	btr_cur_t*	cursor)	/* in: B-tree cursor */
{
	ulint	enough_match;
	rec_t*	rec;
	page_t*	page;
	
	/* NOTE: (compare to the note in row_ins_duplicate_error) Because node
	pointers on upper levels of the B-tree may match more to entry than
	to actual user records on the leaf level, we have to check if the
	candidate record is actually a user record. In a clustered index
	node pointers contain index->n_unique first fields, and in the case
	of a secondary index, all fields of the index. */

	enough_match = dict_index_get_n_unique_in_tree(cursor->index);
	
	if (cursor->low_match >= enough_match) {

		rec = btr_cur_get_rec(cursor);
		page = buf_frame_align(rec);

		if (rec != page_get_infimum_rec(page)) {

			return(ROW_INS_PREV);
		}
	}

	return(0);
}

/*******************************************************************
Tries to insert an index entry to an index. If the index is clustered
and a record with the same unique key is found, the other record is
necessarily marked deleted by a committed transaction, or a unique key
violation error occurs. The delete marked record is then updated to an
existing record, and we must write an undo log record on the delete
marked record. If the index is secondary, and a record with exactly the
same fields is found, the other record is necessarily marked deleted.
It is then unmarked. Otherwise, the entry is just inserted to the index. */

ulint
row_ins_index_entry_low(
/*====================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, DB_FAIL
				if pessimistic retry needed, or error code */
	ulint		mode,	/* in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE,
				depending on whether we wish optimistic or
				pessimistic descent down the index tree */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	que_thr_t*	thr)	/* in: query thread */
{
	btr_cur_t	cursor;
	ulint		ignore_sec_unique	= 0;
	ulint		modify = 0; /* remove warning */
	rec_t*		insert_rec;
	rec_t*		rec;
	rec_t*		first_rec;
	ulint		err;
	ulint		n_unique;
	big_rec_t*	big_rec			= NULL;
	mtr_t		mtr;
	
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
				&cursor, 0, &mtr);

	if (cursor.flag == BTR_CUR_INSERT_TO_IBUF) {
		/* The insertion was made to the insert buffer already during
		the search: we are done */

		err = DB_SUCCESS;

		goto function_exit;
	}	
					
	first_rec = page_rec_get_next(page_get_infimum_rec(
			buf_frame_align(btr_cur_get_rec(&cursor))));

	if (!page_rec_is_supremum(first_rec)) {
		ut_a((rec_get_n_fields(first_rec))
					== dtuple_get_n_fields(entry));
	}

	n_unique = dict_index_get_n_unique(index);

	if (index->type & DICT_UNIQUE && (cursor.up_match >= n_unique
					 || cursor.low_match >= n_unique)) {

		if (index->type & DICT_CLUSTERED) {			 
			/* Note that the following may return also
			DB_LOCK_WAIT */

			err = row_ins_duplicate_error_in_clust(&cursor,
							entry, thr, &mtr);
			if (err != DB_SUCCESS) {

				goto function_exit;
			}
		} else {
			mtr_commit(&mtr);
			err = row_ins_scan_sec_index_for_duplicate(index,
								entry, thr);
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
					PAGE_CUR_LE, mode | BTR_INSERT,
					&cursor, 0, &mtr);
		}		
	}

	modify = row_ins_must_modify(&cursor);

	if (modify != 0) {
		/* There is already an index entry with a long enough common
		prefix, we must convert the insert into a modify of an
		existing record */

		if (modify == ROW_INS_NEXT) {
			rec = page_rec_get_next(btr_cur_get_rec(&cursor));

			btr_cur_position(index, rec, &cursor);
		}

		if (index->type & DICT_CLUSTERED) {
			err = row_ins_clust_index_entry_by_modify(mode,
							&cursor, &big_rec,
							entry,
							ext_vec, n_ext_vec,
							thr, &mtr);
		} else {
			err = row_ins_sec_index_entry_by_modify(mode, &cursor,
								entry,
								thr, &mtr);
		}
		
	} else {
		if (mode == BTR_MODIFY_LEAF) {
			err = btr_cur_optimistic_insert(0, &cursor, entry,
					&insert_rec, &big_rec, thr, &mtr);
		} else {
			ut_a(mode == BTR_MODIFY_TREE);
			err = btr_cur_pessimistic_insert(0, &cursor, entry,
					&insert_rec, &big_rec, thr, &mtr);
		}

		if (err == DB_SUCCESS) {
			if (ext_vec) {
				rec_set_field_extern_bits(insert_rec,
						ext_vec, n_ext_vec, &mtr);
			}
		}
	}

function_exit:
	mtr_commit(&mtr);

	if (big_rec) {
		mtr_start(&mtr);
	
		btr_cur_search_to_nth_level(index, 0, entry, PAGE_CUR_LE,
					BTR_MODIFY_TREE, &cursor, 0, &mtr);

		err = btr_store_big_rec_extern_fields(index,
						btr_cur_get_rec(&cursor), 
						big_rec, &mtr);
		if (modify) {
			dtuple_big_rec_free(big_rec);
		} else {
			dtuple_convert_back_big_rec(index, entry, big_rec);
		}

		mtr_commit(&mtr);
	}

	return(err);
}

/*******************************************************************
Inserts an index entry to index. Tries first optimistic, then pessimistic
descent down the tree. If the entry matches enough to a delete marked record,
performs the insert by updating or delete unmarking the delete marked
record. */

ulint
row_ins_index_entry(
/*================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT,
				DB_DUPLICATE_KEY, or some other error code */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	if (UT_LIST_GET_FIRST(index->table->foreign_list)) {
		err = row_ins_check_foreign_constraints(index->table, index,
								entry, thr);
		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	/* Try first optimistic descent to the B-tree */

	err = row_ins_index_entry_low(BTR_MODIFY_LEAF, index, entry,
						ext_vec, n_ext_vec, thr);
	if (err != DB_FAIL) {

		return(err);
	}

	/* Try then pessimistic descent to the B-tree */

	err = row_ins_index_entry_low(BTR_MODIFY_TREE, index, entry,
						ext_vec, n_ext_vec, thr);
	return(err);
}

/***************************************************************
Sets the values of the dtuple fields in entry from the values of appropriate
columns in row. */
static
void
row_ins_index_entry_set_vals(
/*=========================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: index entry to make */
	dtuple_t*	row)	/* in: row */
{
	dict_field_t*	ind_field;
	dfield_t*	field;
	dfield_t*	row_field;
	ulint		n_fields;
	ulint		i;

	ut_ad(entry && row);

	n_fields = dtuple_get_n_fields(entry);

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(entry, i);
		ind_field = dict_index_get_nth_field(index, i);

		row_field = dtuple_get_nth_field(row, ind_field->col->ind);

		/* Check column prefix indexes */
		if (ind_field->prefix_len > 0
		    && dfield_get_len(row_field) != UNIV_SQL_NULL
		    && dfield_get_len(row_field) > ind_field->prefix_len) {
		    
		        field->len = ind_field->prefix_len;
		} else {
		        field->len = row_field->len;
		}

		field->data = row_field->data;
	}
}

/***************************************************************
Inserts a single index entry to the table. */
static
ulint
row_ins_index_entry_step(
/*=====================*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	ins_node_t*	node,	/* in: row insert node */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad(dtuple_check_typed(node->row));
	
	row_ins_index_entry_set_vals(node->index, node->entry, node->row);
	
	ut_ad(dtuple_check_typed(node->entry));

	err = row_ins_index_entry(node->index, node->entry, NULL, 0, thr);

	return(err);
}

/***************************************************************
Allocates a row id for row and inits the node->index field. */
UNIV_INLINE
void
row_ins_alloc_row_id_step(
/*======================*/
	ins_node_t*	node)	/* in: row insert node */
{
	dulint	row_id;
	
	ut_ad(node->state == INS_NODE_ALLOC_ROW_ID);
	
	if (dict_table_get_first_index(node->table)->type & DICT_UNIQUE) {

		/* No row id is stored if the clustered index is unique */

		return;
	}
	
	/* Fill in row id value to row */

	row_id = dict_sys_get_new_row_id();

	dict_sys_write_row_id(node->row_id_buf, row_id);
}

/***************************************************************
Gets a row to insert from the values list. */
UNIV_INLINE
void
row_ins_get_row_from_values(
/*========================*/
	ins_node_t*	node)	/* in: row insert node */
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

/***************************************************************
Gets a row to insert from the select list. */
UNIV_INLINE
void
row_ins_get_row_from_select(
/*========================*/
	ins_node_t*	node)	/* in: row insert node */
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
	
/***************************************************************
Inserts a row to a table. */

ulint
row_ins(
/*====*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	ins_node_t*	node,	/* in: row insert node */
	que_thr_t*	thr)	/* in: query thread */
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

/***************************************************************
Inserts a row to a table. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
row_ins_step(
/*=========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
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
	IX lock on the table and reset the possible select node. */

	if (node->state == INS_NODE_SET_IX_LOCK) {

		/* It may be that the current session has not yet started
		its transaction, or it has been committed: */
		
		if (UT_DULINT_EQ(trx->id, node->trx_id)) {
			/* No need to do IX-locking or write trx id to buf */

			goto same_trx;
		}	

		trx_write_trx_id(node->trx_id_buf, trx->id);

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
