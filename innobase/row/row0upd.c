/******************************************************
Update of a row

(c) 1996 Innobase Oy

Created 12/27/1996 Heikki Tuuri
*******************************************************/

#include "row0upd.h"

#ifdef UNIV_NONINL
#include "row0upd.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "mach0data.h"
#include "trx0undo.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "que0que.h"
#include "row0ins.h"
#include "row0sel.h"
#include "row0row.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "log0log.h"
#include "pars0sym.h"
#include "eval0eval.h"


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

/***************************************************************
Checks if an update vector changes some of the first ordering fields of an
index record. This is only used in foreign key checks and we can assume
that index does not contain column prefixes. */
static
ibool
row_upd_changes_first_fields_binary(
/*================================*/
				/* out: TRUE if changes */
	dtuple_t*	entry,	/* in: old value of index entry */
	dict_index_t*	index,	/* in: index of entry */
	upd_t*		update,	/* in: update vector for the row */
	ulint		n);	/* in: how many first fields to check */


/*************************************************************************
Checks if index currently is mentioned as a referenced index in a foreign
key constraint. */
static
ibool
row_upd_index_is_referenced(
/*========================*/
				/* out: TRUE if referenced; NOTE that since
				we do not hold dict_operation_lock
				when leaving the function, it may be that
				the referencing table has been dropped when
				we leave this function: this function is only
				for heuristic use! */
	dict_index_t*	index,	/* in: index */
	trx_t*		trx)	/* in: transaction */
{
	dict_table_t*	table		= index->table;
	dict_foreign_t*	foreign;
	ibool		froze_data_dict	= FALSE;

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

			if (froze_data_dict) {
				row_mysql_unfreeze_data_dictionary(trx);
			}

			return(TRUE);
		}

		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}
	
	if (froze_data_dict) {
		row_mysql_unfreeze_data_dictionary(trx);
	}

	return(FALSE);
}

/*************************************************************************
Checks if possible foreign key constraints hold after a delete of the record
under pcur. NOTE that this function will temporarily commit mtr and lose the
pcur position! */
static
ulint
row_upd_check_references_constraints(
/*=================================*/
				/* out: DB_SUCCESS or an error code */
	upd_node_t*	node,	/* in: row update node */
	btr_pcur_t*	pcur,	/* in: cursor positioned on a record; NOTE: the
				cursor position is lost in this function! */
	dict_table_t*	table,	/* in: table in question */
	dict_index_t*	index,	/* in: index of the cursor */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_foreign_t*	foreign;
	mem_heap_t*	heap;
	dtuple_t*	entry;
	trx_t*		trx;
	rec_t*		rec;
	ulint		err;
	ibool		got_s_lock	= FALSE;

	if (UT_LIST_GET_FIRST(table->referenced_list) == NULL) {

		return(DB_SUCCESS);
	}

	trx = thr_get_trx(thr);

	rec = btr_pcur_get_rec(pcur);

	heap = mem_heap_create(500);

	entry = row_rec_to_index_entry(ROW_COPY_DATA, index, rec, heap);

	mtr_commit(mtr);	

	mtr_start(mtr);	
	
	if (trx->dict_operation_lock_mode == 0) {
		got_s_lock = TRUE;

		row_mysql_freeze_data_dictionary(trx);
	}
		
	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign) {
		/* Note that we may have an update which updates the index
		record, but does NOT update the first fields which are
		referenced in a foreign key constraint. Then the update does
		NOT break the constraint. */

		if (foreign->referenced_index == index
		    && (node->is_delete
		       || row_upd_changes_first_fields_binary(entry, index,
			    		node->update, foreign->n_fields))) {
			    				
			if (foreign->foreign_table == NULL) {
				dict_table_get(foreign->foreign_table_name,
									trx);
			}

			if (foreign->foreign_table) {
				mutex_enter(&(dict_sys->mutex));

				(foreign->foreign_table
				->n_foreign_key_checks_running)++;

				mutex_exit(&(dict_sys->mutex));
			}

			/* NOTE that if the thread ends up waiting for a lock
			we will release dict_operation_lock temporarily!
			But the counter on the table protects 'foreign' from
			being dropped while the check is running. */
			
			err = row_ins_check_foreign_constraint(FALSE, foreign,
							table, entry, thr);

			if (foreign->foreign_table) {
				mutex_enter(&(dict_sys->mutex));

				ut_a(foreign->foreign_table
				->n_foreign_key_checks_running > 0);

				(foreign->foreign_table
				->n_foreign_key_checks_running)--;

				mutex_exit(&(dict_sys->mutex));
			}

			if (err != DB_SUCCESS) {
				if (got_s_lock) {
					row_mysql_unfreeze_data_dictionary(
									trx);
				}

				mem_heap_free(heap);

				return(err);
			}
		}

		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}

	if (got_s_lock) {
		row_mysql_unfreeze_data_dictionary(trx);
	}

	mem_heap_free(heap);
	
	return(DB_SUCCESS);
}

/*************************************************************************
Creates an update node for a query graph. */

upd_node_t*
upd_node_create(
/*============*/
				/* out, own: update node */
	mem_heap_t*	heap)	/* in: mem heap where created */
{
	upd_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(upd_node_t));
	node->common.type = QUE_NODE_UPDATE;

	node->state = UPD_NODE_UPDATE_CLUSTERED;
	node->select_will_do_update = FALSE;
	node->in_mysql_interface = FALSE;

	node->row = NULL;
	node->ext_vec = NULL;
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

/*************************************************************************
Updates the trx id and roll ptr field in a clustered index record in database
recovery. */

void
row_upd_rec_sys_fields_in_recovery(
/*===============================*/
	rec_t*	rec,	/* in: record */
	ulint	pos,	/* in: TRX_ID position in rec */
	dulint	trx_id,	/* in: transaction id */
	dulint	roll_ptr)/* in: roll ptr of the undo log record */
{
	byte*	field;
	ulint	len;

	field = rec_get_nth_field(rec, pos, &len);
	ut_ad(len == DATA_TRX_ID_LEN);
	trx_write_trx_id(field, trx_id);

	field = rec_get_nth_field(rec, pos + 1, &len);
	ut_ad(len == DATA_ROLL_PTR_LEN);
	trx_write_roll_ptr(field, roll_ptr);
}

/*************************************************************************
Sets the trx id or roll ptr field of a clustered index entry. */

void
row_upd_index_entry_sys_field(
/*==========================*/
	dtuple_t*	entry,	/* in: index entry, where the memory buffers
				for sys fields are already allocated:
				the function just copies the new values to
				them */
	dict_index_t*	index,	/* in: clustered index */
	ulint		type,	/* in: DATA_TRX_ID or DATA_ROLL_PTR */
	dulint		val)	/* in: value to write */
{
	dfield_t*	dfield;
	byte*		field;
	ulint		pos;

	ut_ad(index->type & DICT_CLUSTERED);

	pos = dict_index_get_sys_col_pos(index, type);

	dfield = dtuple_get_nth_field(entry, pos);
	field = dfield_get_data(dfield);

	if (type == DATA_TRX_ID) {
		trx_write_trx_id(field, val);
	} else {
		ut_ad(type == DATA_ROLL_PTR);
		trx_write_roll_ptr(field, val);
	}
}

/***************************************************************
Returns TRUE if row update changes size of some field in index or if some
field to be updated is stored externally in rec or update. */

ibool
row_upd_changes_field_size_or_external(
/*===================================*/
				/* out: TRUE if the update changes the size of
				some field in index or the field is external
				in rec or update */
	rec_t*		rec,	/* in: record in index */
	dict_index_t*	index,	/* in: index */
	upd_t*		update)	/* in: update vector */
{
	upd_field_t*	upd_field;
	dfield_t*	new_val;
	ulint		old_len;
	ulint		new_len;
	ulint		n_fields;
	ulint		i;

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		upd_field = upd_get_nth_field(update, i);

		new_val = &(upd_field->new_val);
		new_len = new_val->len;

		if (new_len == UNIV_SQL_NULL) {
			new_len = dtype_get_sql_null_size(
					dict_index_get_nth_type(index, i));
		}

		old_len = rec_get_nth_field_size(rec, upd_field->field_no);
		
		if (old_len != new_len) {

			return(TRUE);
		}
		
		if (rec_get_nth_field_extern_bit(rec, upd_field->field_no)) {

			return(TRUE);
		}

		if (upd_field->extern_storage) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/***************************************************************
Replaces the new column values stored in the update vector to the record
given. No field size changes are allowed. This function is used only for
a clustered index */

void
row_upd_rec_in_place(
/*=================*/
	rec_t*	rec,	/* in/out: record where replaced */
	upd_t*	update)	/* in: update vector */
{
	upd_field_t*	upd_field;
	dfield_t*	new_val;
	ulint		n_fields;
	ulint		i;

	rec_set_info_bits(rec, update->info_bits);

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		upd_field = upd_get_nth_field(update, i);
		new_val = &(upd_field->new_val);
		
		rec_set_nth_field(rec, upd_field->field_no,
						dfield_get_data(new_val),
						dfield_get_len(new_val));
	}
}

/*************************************************************************
Writes into the redo log the values of trx id and roll ptr and enough info
to determine their positions within a clustered index record. */

byte*
row_upd_write_sys_vals_to_log(
/*==========================*/
				/* out: new pointer to mlog */
	dict_index_t*	index,	/* in: clustered index */
	trx_t*		trx,	/* in: transaction */
	dulint		roll_ptr,/* in: roll ptr of the undo log record */
	byte*		log_ptr,/* pointer to a buffer of size > 20 opened
				in mlog */
	mtr_t*		mtr __attribute__((unused))) /* in: mtr */
{
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(mtr);

	log_ptr += mach_write_compressed(log_ptr,
			dict_index_get_sys_col_pos(index, DATA_TRX_ID));

	trx_write_roll_ptr(log_ptr, roll_ptr);
	log_ptr += DATA_ROLL_PTR_LEN;	

	log_ptr += mach_dulint_write_compressed(log_ptr, trx->id);

	return(log_ptr);
}

/*************************************************************************
Parses the log data of system field values. */

byte*
row_upd_parse_sys_vals(
/*===================*/
			/* out: log data end or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	ulint*	pos,	/* out: TRX_ID position in record */
	dulint*	trx_id,	/* out: trx id */
	dulint*	roll_ptr)/* out: roll ptr */
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

	ptr = mach_dulint_parse_compressed(ptr, end_ptr, trx_id);

	return(ptr);
}

/***************************************************************
Writes to the redo log the new values of the fields occurring in the index. */

void
row_upd_index_write_log(
/*====================*/
	upd_t*	update,	/* in: update vector */
	byte*	log_ptr,/* in: pointer to mlog buffer: must contain at least
			MLOG_BUF_MARGIN bytes of free space; the buffer is
			closed within this function */
	mtr_t*	mtr)	/* in: mtr into whose log to write */
{
	upd_field_t*	upd_field;
	dfield_t*	new_val;
	ulint		len;
	ulint		n_fields;
	byte*		buf_end;
	ulint		i;

	n_fields = upd_get_n_fields(update);

	buf_end = log_ptr + MLOG_BUF_MARGIN;
	
	mach_write_to_1(log_ptr, update->info_bits);
	log_ptr++;
	log_ptr += mach_write_compressed(log_ptr, n_fields);
	
	for (i = 0; i < n_fields; i++) {

		ut_ad(MLOG_BUF_MARGIN > 30);

		if (log_ptr + 30 > buf_end) {
			mlog_close(mtr, log_ptr);
			
			log_ptr = mlog_open(mtr, MLOG_BUF_MARGIN);
			buf_end = log_ptr + MLOG_BUF_MARGIN;
		}

		upd_field = upd_get_nth_field(update, i);

		new_val = &(upd_field->new_val);

		len = new_val->len;

		log_ptr += mach_write_compressed(log_ptr, upd_field->field_no);
		log_ptr += mach_write_compressed(log_ptr, len);

		if (len != UNIV_SQL_NULL) {
			if (log_ptr + len < buf_end) {
				ut_memcpy(log_ptr, new_val->data, len);

				log_ptr += len;
			} else {
				mlog_close(mtr, log_ptr);
			
				mlog_catenate_string(mtr, new_val->data, len);

				log_ptr = mlog_open(mtr, MLOG_BUF_MARGIN);
				buf_end = log_ptr + MLOG_BUF_MARGIN;
			}
		}
	}

	mlog_close(mtr, log_ptr);
}

/*************************************************************************
Parses the log data written by row_upd_index_write_log. */

byte*
row_upd_index_parse(
/*================*/
				/* out: log data end or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	mem_heap_t*	heap,	/* in: memory heap where update vector is
				built */
	upd_t**		update_out)/* out: update vector */
{
	upd_t*		update;
	upd_field_t*	upd_field;
	dfield_t*	new_val;
	ulint		len;
	ulint		n_fields;
	byte*		buf;
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
		upd_field = upd_get_nth_field(update, i);
		new_val = &(upd_field->new_val);

		ptr = mach_parse_compressed(ptr, end_ptr,
						&(upd_field->field_no));
		if (ptr == NULL) {

			return(NULL);
		}

		ptr = mach_parse_compressed(ptr, end_ptr, &len);

		if (ptr == NULL) {

			return(NULL);
		}

		new_val->len = len;

		if (len != UNIV_SQL_NULL) {

			if (end_ptr < ptr + len) {

				return(NULL);
			} else {
				buf = mem_heap_alloc(heap, len);
				ut_memcpy(buf, ptr, len);

				ptr += len;

				new_val->data = buf;
			}
		}
	}

	*update_out = update;

	return(ptr);
}

/*******************************************************************
Returns TRUE if ext_vec contains i. */
static
ibool
upd_ext_vec_contains(
/*=================*/
				/* out: TRUE if i is in ext_vec */
	ulint*	ext_vec,	/* in: array of indexes or NULL */
	ulint	n_ext_vec,	/* in: number of numbers in ext_vec */
	ulint	i)		/* in: a number */
{
	ulint	j;

	if (ext_vec == NULL) {

		return(FALSE);
	}

	for (j = 0; j < n_ext_vec; j++) {
		if (ext_vec[j] == i) {

			return(TRUE);
		}
	}

	return(FALSE);
}
	
/*******************************************************************
Builds an update vector from those fields which in a secondary index entry
differ from a record that has the equal ordering fields. NOTE: we compare
the fields as binary strings! */

upd_t*
row_upd_build_sec_rec_difference_binary(
/*====================================*/
				/* out, own: update vector of differing
				fields */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t*		rec,	/* in: secondary index record */
	mem_heap_t*	heap)	/* in: memory heap from which allocated */
{
	upd_field_t*	upd_field;
	dfield_t*	dfield;
	byte*		data;
	ulint		len;
	upd_t*		update;
	ulint		n_diff;
	ulint		i;

	/* This function is used only for a secondary index */
	ut_a(0 == (index->type & DICT_CLUSTERED));

	update = upd_create(dtuple_get_n_fields(entry), heap);

	n_diff = 0;

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {

		data = rec_get_nth_field(rec, i, &len);

		dfield = dtuple_get_nth_field(entry, i);

		/* NOTE that it may be that len != dfield_get_len(dfield) if we
		are updating in a character set and collation where strings of
		different length can be equal in an alphabetical comparison,
		and also in the case where we have a column prefix index
		and the last characters in the index field are spaces; the
		latter case probably caused the assertion failures reported at
		row0upd.c line 713 in versions 4.0.14 - 4.0.16. */

		/* NOTE: we compare the fields as binary strings!
		(No collation) */

		if (!dfield_data_is_binary_equal(dfield, len, data)) {

			upd_field = upd_get_nth_field(update, n_diff);

			dfield_copy(&(upd_field->new_val), dfield);

			upd_field_set_field_no(upd_field, i, index);

			upd_field->extern_storage = FALSE;

			n_diff++;
		}
	}

	update->n_fields = n_diff;

	return(update);
}

/*******************************************************************
Builds an update vector from those fields, excluding the roll ptr and
trx id fields, which in an index entry differ from a record that has
the equal ordering fields. NOTE: we compare the fields as binary strings! */

upd_t*
row_upd_build_difference_binary(
/*============================*/
				/* out, own: update vector of differing
				fields, excluding roll ptr and trx id */
	dict_index_t*	index,	/* in: clustered index */
	dtuple_t*	entry,	/* in: entry to insert */
	ulint*		ext_vec,/* in: array containing field numbers of
				externally stored fields in entry, or NULL */
	ulint		n_ext_vec,/* in: number of fields in ext_vec */
	rec_t*		rec,	/* in: clustered index record */
	mem_heap_t*	heap)	/* in: memory heap from which allocated */
{
	upd_field_t*	upd_field;
	dfield_t*	dfield;
	byte*		data;
	ulint		len;
	upd_t*		update;
	ulint		n_diff;
	ulint		roll_ptr_pos;
	ulint		trx_id_pos;
	ibool		extern_bit;
	ulint		i;

	/* This function is used only for a clustered index */
	ut_a(index->type & DICT_CLUSTERED);

	update = upd_create(dtuple_get_n_fields(entry), heap);

	n_diff = 0;

	roll_ptr_pos = dict_index_get_sys_col_pos(index, DATA_ROLL_PTR);
	trx_id_pos = dict_index_get_sys_col_pos(index, DATA_TRX_ID);

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {

		data = rec_get_nth_field(rec, i, &len);

		dfield = dtuple_get_nth_field(entry, i);

		/* NOTE: we compare the fields as binary strings!
		(No collation) */

		if (i == trx_id_pos || i == roll_ptr_pos) {

			goto skip_compare;
		}

		extern_bit = rec_get_nth_field_extern_bit(rec, i);
		
		if (extern_bit != upd_ext_vec_contains(ext_vec, n_ext_vec, i)
		    || !dfield_data_is_binary_equal(dfield, len, data)) {

			upd_field = upd_get_nth_field(update, n_diff);

			dfield_copy(&(upd_field->new_val), dfield);

			upd_field_set_field_no(upd_field, i, index);

			if (upd_ext_vec_contains(ext_vec, n_ext_vec, i)) {
				upd_field->extern_storage = TRUE;
			} else {
				upd_field->extern_storage = FALSE;
			}
				
			n_diff++;
		}
skip_compare:
		;
	}

	update->n_fields = n_diff;

	return(update);
}

/***************************************************************
Replaces the new column values stored in the update vector to the index entry
given. */

void
row_upd_index_replace_new_col_vals_index_pos(
/*=========================================*/
	dtuple_t*	entry,	/* in/out: index entry where replaced */
	dict_index_t*	index,	/* in: index; NOTE that this may also be a
				non-clustered index */
	upd_t*		update,	/* in: an update vector built for the index so
				that the field number in an upd_field is the
				index position */
	mem_heap_t*	heap)	/* in: memory heap to which we allocate and
				copy the new values, set this as NULL if you
				do not want allocation */
{
	dict_field_t*	field;
	upd_field_t*	upd_field;
	dfield_t*	dfield;
	dfield_t*	new_val;
	ulint		j;
	ulint		i;
	dtype_t*	cur_type;

	ut_ad(index);

	dtuple_set_info_bits(entry, update->info_bits);

	for (j = 0; j < dict_index_get_n_fields(index); j++) {

	        field = dict_index_get_nth_field(index, j);

		for (i = 0; i < upd_get_n_fields(update); i++) {

		        upd_field = upd_get_nth_field(update, i);

			if (upd_field->field_no == j) {

			        dfield = dtuple_get_nth_field(entry, j);

				new_val = &(upd_field->new_val);

				dfield_set_data(dfield, new_val->data,
								new_val->len);
				if (heap && new_val->len != UNIV_SQL_NULL) {
				        dfield->data = mem_heap_alloc(heap,
								new_val->len);
					ut_memcpy(dfield->data, new_val->data,
								new_val->len);
				}

				if (field->prefix_len > 0
			            && new_val->len != UNIV_SQL_NULL) {

				/* For prefix keys get the storage length
				for the prefix_len characters. */

				  cur_type = dict_col_get_type(
					dict_field_get_col(field));

				  dfield->len = 
				    innobase_get_at_most_n_mbchars(
				      dtype_get_charset_coll(cur_type->prtype),
					field->prefix_len,
					new_val->len,new_val->data);
				}
			}
		}
	}
}

/***************************************************************
Replaces the new column values stored in the update vector to the index entry
given. */

void
row_upd_index_replace_new_col_vals(
/*===============================*/
	dtuple_t*	entry,	/* in/out: index entry where replaced */
	dict_index_t*	index,	/* in: index; NOTE that this may also be a
				non-clustered index */
	upd_t*		update,	/* in: an update vector built for the
				CLUSTERED index so that the field number in
				an upd_field is the clustered index position */
	mem_heap_t*	heap)	/* in: memory heap to which we allocate and
				copy the new values, set this as NULL if you
				do not want allocation */
{
	dict_field_t*	field;
	upd_field_t*	upd_field;
	dfield_t*	dfield;
	dfield_t*	new_val;
	ulint		j;
	ulint		i;
	dtype_t*	cur_type;

	ut_ad(index);

	dtuple_set_info_bits(entry, update->info_bits);

	for (j = 0; j < dict_index_get_n_fields(index); j++) {

	        field = dict_index_get_nth_field(index, j);

		for (i = 0; i < upd_get_n_fields(update); i++) {

		        upd_field = upd_get_nth_field(update, i);

			if (upd_field->field_no == field->col->clust_pos) {

			        dfield = dtuple_get_nth_field(entry, j);

				new_val = &(upd_field->new_val);

				dfield_set_data(dfield, new_val->data,
								new_val->len);
				if (heap && new_val->len != UNIV_SQL_NULL) {
				        dfield->data = mem_heap_alloc(heap,
								new_val->len);
					ut_memcpy(dfield->data, new_val->data,
								new_val->len);
				}

				if (field->prefix_len > 0
			            && new_val->len != UNIV_SQL_NULL) {

				/* For prefix keys get the storage length
				for the prefix_len characters. */

				cur_type = dict_col_get_type(
					dict_field_get_col(field));

				  dfield->len = 
				    innobase_get_at_most_n_mbchars(
				      dtype_get_charset_coll(cur_type->prtype),
					field->prefix_len,
					new_val->len,new_val->data);
				}
			}
		}
	}
}

/***************************************************************
Checks if an update vector changes an ordering field of an index record.
This function is fast if the update vector is short or the number of ordering
fields in the index is small. Otherwise, this can be quadratic.
NOTE: we compare the fields as binary strings! */

ibool
row_upd_changes_ord_field_binary(
/*=============================*/
				/* out: TRUE if update vector changes
				an ordering field in the index record;
				NOTE: the fields are compared as binary
				strings */
	dtuple_t*	row,	/* in: old value of row, or NULL if the
				row and the data values in update are not
				known when this function is called, e.g., at
				compile time */
	dict_index_t*	index,	/* in: index of the record */
	upd_t*		update)	/* in: update vector for the row; NOTE: the
				field numbers in this MUST be clustered index
				positions! */
{
	upd_field_t*	upd_field;
	dict_field_t*	ind_field;
	dict_col_t*	col;
	ulint		n_unique;
	ulint		n_upd_fields;
	ulint		col_pos;
	ulint		col_no;
	ulint		i, j;
	
	ut_ad(update && index);

	n_unique = dict_index_get_n_unique(index);
	n_upd_fields = upd_get_n_fields(update);

	for (i = 0; i < n_unique; i++) {

		ind_field = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(ind_field);
		col_pos = dict_col_get_clust_pos(col);
		col_no = dict_col_get_no(col);

		for (j = 0; j < n_upd_fields; j++) {

			upd_field = upd_get_nth_field(update, j);

			/* Note that if the index field is a column prefix
			then it may be that row does not contain an externally
			stored part of the column value, and we cannot compare
			the datas */

			if (col_pos == upd_field->field_no
			    && (row == NULL
			        || ind_field->prefix_len > 0
				|| !dfield_datas_are_binary_equal(
					dtuple_get_nth_field(row, col_no),
						&(upd_field->new_val)))) {
				return(TRUE);
			}
		}
	}

	return(FALSE);
}

/***************************************************************
Checks if an update vector changes an ordering field of an index record.
NOTE: we compare the fields as binary strings! */

ibool
row_upd_changes_some_index_ord_field_binary(
/*========================================*/
				/* out: TRUE if update vector may change
				an ordering field in an index record */
	dict_table_t*	table,	/* in: table */
	upd_t*		update)	/* in: update vector for the row */
{
	upd_field_t*	upd_field;
	dict_index_t*	index;
	ulint		i;
	
	index = dict_table_get_first_index(table);
	
	for (i = 0; i < upd_get_n_fields(update); i++) {

		upd_field = upd_get_nth_field(update, i);

		if (dict_field_get_col(dict_index_get_nth_field(index,
						upd_field->field_no))
		    ->ord_part) {

		    	return(TRUE);
		}
	}
	
	return(FALSE);
}

/***************************************************************
Checks if an update vector changes some of the first ordering fields of an
index record. This is only used in foreign key checks and we can assume
that index does not contain column prefixes. */
static
ibool
row_upd_changes_first_fields_binary(
/*================================*/
				/* out: TRUE if changes */
	dtuple_t*	entry,	/* in: index entry */
	dict_index_t*	index,	/* in: index of entry */
	upd_t*		update,	/* in: update vector for the row */
	ulint		n)	/* in: how many first fields to check */
{
	upd_field_t*	upd_field;
	dict_field_t*	ind_field;
	dict_col_t*	col;
	ulint		n_upd_fields;
	ulint		col_pos;
	ulint		i, j;
	
	ut_a(update && index);
	ut_a(n <= dict_index_get_n_fields(index));
	
	n_upd_fields = upd_get_n_fields(update);

	for (i = 0; i < n; i++) {

		ind_field = dict_index_get_nth_field(index, i);
		col = dict_field_get_col(ind_field);
		col_pos = dict_col_get_clust_pos(col);

		ut_a(ind_field->prefix_len == 0);

		for (j = 0; j < n_upd_fields; j++) {

			upd_field = upd_get_nth_field(update, j);

			if (col_pos == upd_field->field_no
			    && !dfield_datas_are_binary_equal(
					     dtuple_get_nth_field(entry, i),
					     &(upd_field->new_val))) {
				return(TRUE);
			}
		}
	}

	return(FALSE);
}

/*************************************************************************
Copies the column values from a record. */
UNIV_INLINE
void
row_upd_copy_columns(
/*=================*/
	rec_t*		rec,	/* in: record in a clustered index */
	sym_node_t*	column)	/* in: first column in a column list, or
				NULL */
{
	byte*	data;
	ulint	len;

	while (column) {
		data = rec_get_nth_field(rec,
					column->field_nos[SYM_CLUST_FIELD_NO],
									&len);
		eval_node_copy_and_alloc_val(column, data, len);

		column = UT_LIST_GET_NEXT(col_var_list, column);
	}
}

/*************************************************************************
Calculates the new values for fields to update. Note that row_upd_copy_columns
must have been called first. */
UNIV_INLINE
void
row_upd_eval_new_vals(
/*==================*/
	upd_t*	update)	/* in: update vector */
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

/***************************************************************
Stores to the heap the row on which the node->pcur is positioned. */
static
void
row_upd_store_row(
/*==============*/
	upd_node_t*	node)	/* in: row update node */
{
	dict_index_t*	clust_index;
	upd_t*		update;
	rec_t*		rec;
	
	ut_ad(node->pcur->latch_mode != BTR_NO_LATCHES);

	if (node->row != NULL) {
		mem_heap_empty(node->heap);
		node->row = NULL;
	}
	
	clust_index = dict_table_get_first_index(node->table);

	rec = btr_pcur_get_rec(node->pcur);
	
	node->row = row_build(ROW_COPY_DATA, clust_index, rec, node->heap);

	node->ext_vec = mem_heap_alloc(node->heap, sizeof(ulint)
				                    * rec_get_n_fields(rec));
	if (node->is_delete) {
		update = NULL;
	} else {
		update = node->update;
	}
	
	node->n_ext_vec = btr_push_update_extern_fields(node->ext_vec,
								rec, update);
}

/***************************************************************
Updates a secondary index entry of a row. */
static
ulint
row_upd_sec_index_entry(
/*====================*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	upd_node_t*	node,	/* in: row update node */
	que_thr_t*	thr)	/* in: query thread */
{
	ibool		check_ref;
	ibool		found;
	dict_index_t*	index;
	dtuple_t*	entry;
	btr_pcur_t	pcur;
	btr_cur_t*	btr_cur;
	mem_heap_t*	heap;
	rec_t*		rec;
	ulint		err	= DB_SUCCESS;
	mtr_t		mtr;
	
	index = node->index;
	
	check_ref = row_upd_index_is_referenced(index, thr_get_trx(thr));

	heap = mem_heap_create(1024);

	/* Build old index entry */
	entry = row_build_index_entry(node->row, index, heap);

	log_free_check();
	mtr_start(&mtr);
	
	found = row_search_index_entry(index, entry, BTR_MODIFY_LEAF, &pcur,
									&mtr);
	btr_cur = btr_pcur_get_btr_cur(&pcur);

	rec = btr_cur_get_rec(btr_cur);

	if (!found) {
		fputs("InnoDB: error in sec index entry update in\n"
			"InnoDB: ", stderr);
		dict_index_name_print(stderr, index);
		fputs("\n"
			"InnoDB: tuple ", stderr);
		dtuple_print(stderr, entry);
		fputs("\n"
			"InnoDB: record ", stderr);
		rec_print(stderr, rec);
		putc('\n', stderr);

		trx_print(stderr, thr_get_trx(thr));

		fputs("\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n", stderr);
	} else {
 	  	/* Delete mark the old index record; it can already be
          	delete marked if we return after a lock wait in
          	row_ins_index_entry below */

	  	if (!rec_get_deleted_flag(rec)) {
			err = btr_cur_del_mark_set_sec_rec(0, btr_cur, TRUE,
								thr, &mtr);
			if (err == DB_SUCCESS && check_ref) {
			    	
				/* NOTE that the following call loses
				the position of pcur ! */
				err = row_upd_check_references_constraints(
							node,
							&pcur, index->table,
							index, thr, &mtr);
				if (err != DB_SUCCESS) {

					goto close_cur;
				}
			}

	  	}
	}
close_cur:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	if (node->is_delete || err != DB_SUCCESS) {

		mem_heap_free(heap);	

        	return(err);
	}

	/* Build a new index entry */
	row_upd_index_replace_new_col_vals(entry, index, node->update, NULL);

	/* Insert new index entry */
	err = row_ins_index_entry(index, entry, NULL, 0, thr);

	mem_heap_free(heap);	

        return(err);
}

/***************************************************************
Updates secondary index record if it is changed in the row update. This
should be quite rare in database applications. */
UNIV_INLINE
ulint
row_upd_sec_step(
/*=============*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	upd_node_t*	node,	/* in: row update node */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err;

	ut_ad((node->state == UPD_NODE_UPDATE_ALL_SEC)
				|| (node->state == UPD_NODE_UPDATE_SOME_SEC));
	ut_ad(!(node->index->type & DICT_CLUSTERED));
	
	if (node->state == UPD_NODE_UPDATE_ALL_SEC
	    || row_upd_changes_ord_field_binary(node->row, node->index,
							   node->update)) {
		err = row_upd_sec_index_entry(node, thr);

		return(err);
	}

	return(DB_SUCCESS);
}

/***************************************************************
Marks the clustered index record deleted and inserts the updated version
of the record to the index. This function should be used when the ordering
fields of the clustered index record change. This should be quite rare in
database applications. */
static
ulint
row_upd_clust_rec_by_insert(
/*========================*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	upd_node_t*	node,	/* in: row update node */
	dict_index_t*	index,	/* in: clustered index of the record */
	que_thr_t*	thr,	/* in: query thread */
	ibool		check_ref,/* in: TRUE if index may be referenced in
				a foreign key constraint */
	mtr_t*		mtr)	/* in: mtr; gets committed here */
{
	mem_heap_t*	heap;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	trx_t*		trx;
	dict_table_t*	table;
	dtuple_t*	entry;
	ulint		err;
	
	ut_ad(node);
	ut_ad(index->type & DICT_CLUSTERED);

	trx = thr_get_trx(thr);
	table = node->table;
	pcur = node->pcur;
	btr_cur	= btr_pcur_get_btr_cur(pcur);
	
	if (node->state != UPD_NODE_INSERT_CLUSTERED) {

		err = btr_cur_del_mark_set_clust_rec(BTR_NO_LOCKING_FLAG,
						btr_cur, TRUE, thr, mtr);
		if (err != DB_SUCCESS) {
			mtr_commit(mtr);

			return(err);
		}

		/* Mark as not-owned the externally stored fields which the new
		row inherits from the delete marked record: purge should not
		free those externally stored fields even if the delete marked
		record is removed from the index tree, or updated. */

		btr_cur_mark_extern_inherited_fields(btr_cur_get_rec(btr_cur),
							node->update, mtr);
		if (check_ref) {
			/* NOTE that the following call loses
			the position of pcur ! */
			err = row_upd_check_references_constraints(node,
							pcur, table,
							index, thr, mtr);
			if (err != DB_SUCCESS) {
				mtr_commit(mtr);

				return(err);
			}
		}

	} 

	mtr_commit(mtr);

	node->state = UPD_NODE_INSERT_CLUSTERED;

	heap = mem_heap_create(500);
	
	entry = row_build_index_entry(node->row, index, heap);

	row_upd_index_replace_new_col_vals(entry, index, node->update, NULL);
	
	row_upd_index_entry_sys_field(entry, index, DATA_TRX_ID, trx->id);
	
	/* If we return from a lock wait, for example, we may have
	extern fields marked as not-owned in entry (marked in the
	if-branch above). We must unmark them. */
	
	btr_cur_unmark_dtuple_extern_fields(entry, node->ext_vec,
							node->n_ext_vec);
	/* We must mark non-updated extern fields in entry as inherited,
	so that a possible rollback will not free them */
	
	btr_cur_mark_dtuple_inherited_extern(entry, node->ext_vec,
						node->n_ext_vec,
						node->update);
	
	err = row_ins_index_entry(index, entry, node->ext_vec,
						node->n_ext_vec, thr);
	mem_heap_free(heap);
	
	return(err);
}

/***************************************************************
Updates a clustered index record of a row when the ordering fields do
not change. */
static
ulint
row_upd_clust_rec(
/*==============*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	upd_node_t*	node,	/* in: row update node */
	dict_index_t*	index,	/* in: clustered index */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr; gets committed here */
{
	big_rec_t*	big_rec	= NULL;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ulint		err;
	
	ut_ad(node);
	ut_ad(index->type & DICT_CLUSTERED);

	pcur = node->pcur;
	btr_cur = btr_pcur_get_btr_cur(pcur);

	ut_ad(FALSE == rec_get_deleted_flag(btr_pcur_get_rec(pcur)));
	
	/* Try optimistic updating of the record, keeping changes within
	the page; we do not check locks because we assume the x-lock on the
	record to update */

	if (node->cmpl_info & UPD_NODE_NO_SIZE_CHANGE) {
		err = btr_cur_update_in_place(BTR_NO_LOCKING_FLAG,
						btr_cur, node->update,
						node->cmpl_info, thr, mtr);
	} else {
		err = btr_cur_optimistic_update(BTR_NO_LOCKING_FLAG,
						btr_cur, node->update,
						node->cmpl_info, thr, mtr);
	}

	mtr_commit(mtr);
	
	if (err == DB_SUCCESS) {

		return(err);
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

	ut_ad(FALSE == rec_get_deleted_flag(btr_pcur_get_rec(pcur)));
	
	err = btr_cur_pessimistic_update(BTR_NO_LOCKING_FLAG, btr_cur,
					&big_rec, node->update,
					node->cmpl_info, thr, mtr);
	mtr_commit(mtr);

	if (err == DB_SUCCESS && big_rec) {
		mtr_start(mtr);
		ut_a(btr_pcur_restore_position(BTR_MODIFY_TREE, pcur, mtr));
	
		err = btr_store_big_rec_extern_fields(index,
						btr_cur_get_rec(btr_cur),
						big_rec, mtr);		
		mtr_commit(mtr);
	}

	if (big_rec) {
		dtuple_big_rec_free(big_rec);
	}
		
	return(err);
}

/***************************************************************
Delete marks a clustered index record. */
static
ulint
row_upd_del_mark_clust_rec(
/*=======================*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code */
	upd_node_t*	node,	/* in: row update node */
	dict_index_t*	index,	/* in: clustered index */
	que_thr_t*	thr,	/* in: query thread */
	ibool		check_ref,/* in: TRUE if index may be referenced in
				a foreign key constraint */
	mtr_t*		mtr)	/* in: mtr; gets committed here */
{
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ulint		err;
	
	ut_ad(node);
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(node->is_delete);

	pcur = node->pcur;
	btr_cur = btr_pcur_get_btr_cur(pcur);

	/* Store row because we have to build also the secondary index
	entries */
	
	row_upd_store_row(node);

	/* Mark the clustered index record deleted; we do not have to check
	locks, because we assume that we have an x-lock on the record */

	err = btr_cur_del_mark_set_clust_rec(BTR_NO_LOCKING_FLAG,
						btr_cur, TRUE, thr, mtr);
	if (err == DB_SUCCESS && check_ref) {
		/* NOTE that the following call loses the position of pcur ! */

		err = row_upd_check_references_constraints(node,
							pcur, index->table,
							index, thr, mtr);
		if (err != DB_SUCCESS) {
			mtr_commit(mtr);

			return(err);
		}
	}

	mtr_commit(mtr);
	
	return(err);
}

/***************************************************************
Updates the clustered index record. */
static
ulint
row_upd_clust_step(
/*===============*/
				/* out: DB_SUCCESS if operation successfully
				completed, DB_LOCK_WAIT in case of a lock wait,
				else error code */
	upd_node_t*	node,	/* in: row update node */
	que_thr_t*	thr)	/* in: query thread */
{
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	ibool		success;
	ibool		check_ref;
	ulint		err;
	mtr_t*		mtr;
	mtr_t		mtr_buf;
	
	index = dict_table_get_first_index(node->table);

	check_ref = row_upd_index_is_referenced(index, thr_get_trx(thr));

	pcur = node->pcur;

	/* We have to restore the cursor to its position */
	mtr = &mtr_buf;

	mtr_start(mtr);
	
	/* If the restoration does not succeed, then the same
	transaction has deleted the record on which the cursor was,
	and that is an SQL error. If the restoration succeeds, it may
	still be that the same transaction has successively deleted
	and inserted a record with the same ordering fields, but in
	that case we know that the transaction has at least an
	implicit x-lock on the record. */
	
	ut_a(pcur->rel_pos == BTR_PCUR_ON);

	success = btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur, mtr);

	if (!success) {
		err = DB_RECORD_NOT_FOUND;

		mtr_commit(mtr);

		return(err);
	}

	/* If this is a row in SYS_INDEXES table of the data dictionary,
	then we have to free the file segments of the index tree associated
	with the index */

	if (node->is_delete
	    && ut_dulint_cmp(node->table->id, DICT_INDEXES_ID) == 0) {

		dict_drop_index_tree(btr_pcur_get_rec(pcur), mtr);

		mtr_commit(mtr);

		mtr_start(mtr);

		success = btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur,
									mtr);
		if (!success) {
			err = DB_ERROR;

			mtr_commit(mtr);

			return(err);
		}
	} 

	if (!node->has_clust_rec_x_lock) {
		err = lock_clust_rec_modify_check_and_lock(0,
						btr_pcur_get_rec(pcur),
						index, thr);
		if (err != DB_SUCCESS) {
			mtr_commit(mtr);

			return(err);
		}
	}

	/* NOTE: the following function calls will also commit mtr */

	if (node->is_delete) {
		err = row_upd_del_mark_clust_rec(node, index, thr, check_ref,
									mtr);
		if (err != DB_SUCCESS) {

			return(err);
		}

		node->state = UPD_NODE_UPDATE_ALL_SEC;
		node->index = dict_table_get_next_index(index);

		return(err);
	}
	
	/* If the update is made for MySQL, we already have the update vector
	ready, else we have to do some evaluation: */
 
	if (!node->in_mysql_interface) {
		/* Copy the necessary columns from clust_rec and calculate the
		new values to set */

		row_upd_copy_columns(btr_pcur_get_rec(pcur),
					UT_LIST_GET_FIRST(node->columns));
		row_upd_eval_new_vals(node->update);
	}
		
	if (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE) {

		err = row_upd_clust_rec(node, index, thr, mtr);

		return(err);
	}
	
	row_upd_store_row(node);

	if (row_upd_changes_ord_field_binary(node->row, index, node->update)) {

		/* Update causes an ordering field (ordering fields within
		the B-tree) of the clustered index record to change: perform
		the update by delete marking and inserting.

		TODO! What to do to the 'Halloween problem', where an update
		moves the record forward in index so that it is again
		updated when the cursor arrives there? Solution: the
		read operation must check the undo record undo number when
		choosing records to update. MySQL solves now the problem
		externally! */

		err = row_upd_clust_rec_by_insert(node, index, thr, check_ref,
									mtr);
		if (err != DB_SUCCESS) {

			return(err);
		}

		node->state = UPD_NODE_UPDATE_ALL_SEC;
	} else {
		err = row_upd_clust_rec(node, index, thr, mtr);

		if (err != DB_SUCCESS) {

			return(err);
		}

		node->state = UPD_NODE_UPDATE_SOME_SEC;
	}

	node->index = dict_table_get_next_index(index);

	return(err);
}

/***************************************************************
Updates the affected index records of a row. When the control is transferred
to this node, we assume that we have a persistent cursor which was on a
record, and the position of the cursor is stored in the cursor. */
static
ulint
row_upd(
/*====*/
				/* out: DB_SUCCESS if operation successfully
				completed, else error code or DB_LOCK_WAIT */
	upd_node_t*	node,	/* in: row update node */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint	err	= DB_SUCCESS;
	
	ut_ad(node && thr);

	if (node->in_mysql_interface) {
	
		/* We do not get the cmpl_info value from the MySQL
		interpreter: we must calculate it on the fly: */
		
		if (node->is_delete ||
			row_upd_changes_some_index_ord_field_binary(
					node->table, node->update)) {
			node->cmpl_info = 0; 
		} else {
			node->cmpl_info = UPD_NODE_NO_ORD_CHANGE;
		}
	}

	if (node->state == UPD_NODE_UPDATE_CLUSTERED
				|| node->state == UPD_NODE_INSERT_CLUSTERED) {

		err = row_upd_clust_step(node, thr);
		
		if (err != DB_SUCCESS) {

			goto function_exit;
		}
	}

	if (!node->is_delete && (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {

		goto function_exit;
	}

	while (node->index != NULL) {
		err = row_upd_sec_step(node, thr);

		if (err != DB_SUCCESS) {

			goto function_exit;
		}

		node->index = dict_table_get_next_index(node->index);
        }

function_exit:
	if (err == DB_SUCCESS) {
		/* Do some cleanup */

		if (node->row != NULL) {
			node->row = NULL;
			node->n_ext_vec = 0;
			mem_heap_empty(node->heap);
		}

		node->state = UPD_NODE_UPDATE_CLUSTERED;
	}

        return(err);
}

/***************************************************************
Updates a row in a table. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
row_upd_step(
/*=========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	upd_node_t*	node;
	sel_node_t*	sel_node;
	que_node_t*	parent;
	ulint		err		= DB_SUCCESS;
	trx_t*		trx;

	ut_ad(thr);
	
	trx = thr_get_trx(thr);

	trx_start_if_not_started(trx);

	node = thr->run_node;
	
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

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */
	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
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

/*************************************************************************
Performs an in-place update for the current clustered index record in
select. */

void
row_upd_in_place_in_select(
/*=======================*/
	sel_node_t*	sel_node,	/* in: select node */
	que_thr_t*	thr,		/* in: query thread */
	mtr_t*		mtr)		/* in: mtr */
{
	upd_node_t*	node;
	btr_pcur_t*	pcur;
	btr_cur_t*	btr_cur;
	ulint		err;

	ut_ad(sel_node->select_will_do_update);
	ut_ad(sel_node->latch_mode == BTR_MODIFY_LEAF);
	ut_ad(sel_node->asc);

	node = que_node_get_parent(sel_node);

	ut_ad(que_node_get_type(node) == QUE_NODE_UPDATE);

	pcur = node->pcur;
	btr_cur = btr_pcur_get_btr_cur(pcur);

	/* Copy the necessary columns from clust_rec and calculate the new
	values to set */

	row_upd_copy_columns(btr_pcur_get_rec(pcur),
					UT_LIST_GET_FIRST(node->columns));
	row_upd_eval_new_vals(node->update);

	ut_ad(FALSE == rec_get_deleted_flag(btr_pcur_get_rec(pcur)));
	
	ut_ad(node->cmpl_info & UPD_NODE_NO_SIZE_CHANGE);
	ut_ad(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE);
	ut_ad(node->select_will_do_update);

	err = btr_cur_update_in_place(BTR_NO_LOCKING_FLAG, btr_cur,
						node->update, node->cmpl_info,
						thr, mtr);
	ut_ad(err == DB_SUCCESS);
}
