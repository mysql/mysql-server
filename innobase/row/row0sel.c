/*******************************************************
Select

(c) 1997 Innobase Oy

Created 12/19/1997 Heikki Tuuri
*******************************************************/

#include "row0sel.h"

#ifdef UNIV_NONINL
#include "row0sel.ic"
#endif

#include "dict0dict.h"
#include "dict0boot.h"
#include "trx0undo.h"
#include "trx0trx.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "mach0data.h"
#include "que0que.h"
#include "row0upd.h"
#include "row0row.h"
#include "row0vers.h"
#include "rem0cmp.h"
#include "lock0lock.h"
#include "eval0eval.h"
#include "pars0sym.h"
#include "pars0pars.h"
#include "row0mysql.h"

/* Maximum number of rows to prefetch; MySQL interface has another parameter */
#define SEL_MAX_N_PREFETCH	16

/* Number of rows fetched, after which to start prefetching; MySQL interface
has another parameter */
#define SEL_PREFETCH_LIMIT	1

/* When a select has accessed about this many pages, it returns control back
to que_run_threads: this is to allow canceling runaway queries */

#define SEL_COST_LIMIT	100

/* Flags for search shortcut */
#define SEL_FOUND	0
#define	SEL_EXHAUSTED	1
#define SEL_RETRY	2

/************************************************************************
Returns TRUE if the user-defined column values in a secondary index record
are the same as the corresponding columns in the clustered index record. */ 
static
ibool
row_sel_sec_rec_is_for_clust_rec(
/*=============================*/
	rec_t*		sec_rec,
	dict_index_t*	sec_index,
	rec_t*		clust_rec,
	dict_index_t*	clust_index)
{
	dict_col_t*	col;
	byte*		sec_field;
	ulint		sec_len;
	byte*		clust_field;
	ulint		clust_len;
	ulint		n;
	ulint		i;

	n = dict_index_get_n_ordering_defined_by_user(sec_index);

	for (i = 0; i < n; i++) {
		col = dict_field_get_col(
				dict_index_get_nth_field(sec_index, i));

		clust_field = rec_get_nth_field(clust_rec,
						dict_col_get_clust_pos(col),
						&clust_len);
		sec_field = rec_get_nth_field(sec_rec, i, &sec_len);

		if (sec_len != clust_len) {

			return(FALSE);
		}

		if (sec_len != UNIV_SQL_NULL
			&& ut_memcmp(sec_field, clust_field, sec_len) != 0) {

			return(FALSE);
		}
	}

	return(TRUE);
}

/*************************************************************************
Creates a select node struct. */

sel_node_t*
sel_node_create(
/*============*/
				/* out, own: select node struct */
	mem_heap_t*	heap)	/* in: memory heap where created */
{
	sel_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(sel_node_t));
	node->common.type = QUE_NODE_SELECT;
	node->state = SEL_NODE_OPEN;

	node->select_will_do_update = FALSE;
	node->latch_mode = BTR_SEARCH_LEAF;

	node->plans = NULL;
	
	return(node);
}

/*************************************************************************
Frees the memory private to a select node when a query graph is freed,
does not free the heap where the node was originally created. */

void
sel_node_free_private(
/*==================*/
	sel_node_t*	node)	/* in: select node struct */
{
	ulint	i;
	plan_t*	plan;

	if (node->plans != NULL) {
		for (i = 0; i < node->n_tables; i++) {
			plan = sel_node_get_nth_plan(node, i);

			btr_pcur_close(&(plan->pcur));
			btr_pcur_close(&(plan->clust_pcur));

			if (plan->old_vers_heap) {
				mem_heap_free(plan->old_vers_heap);
			}
		}
	}
}

/*************************************************************************
Evaluates the values in a select list. If there are aggregate functions,
their argument value is added to the aggregate total. */
UNIV_INLINE
void
sel_eval_select_list(
/*=================*/
	sel_node_t*	node)	/* in: select node */
{
	que_node_t*	exp;

	exp = node->select_list;

	while (exp) {
		eval_exp(exp);

		exp = que_node_get_next(exp);
	}
}

/*************************************************************************
Assigns the values in the select list to the possible into-variables in
SELECT ... INTO ... */
UNIV_INLINE
void
sel_assign_into_var_values(
/*=======================*/
	sym_node_t*	var,	/* in: first variable in a list of variables */
	sel_node_t*	node)	/* in: select node */
{
	que_node_t*	exp;

	if (var == NULL) {

		return;
	}

	exp = node->select_list;

	while (var) {
		ut_ad(exp);

		eval_node_copy_val(var->alias, exp);

		exp = que_node_get_next(exp);
		var = que_node_get_next(var);
	}
}

/*************************************************************************
Resets the aggregate value totals in the select list of an aggregate type
query. */
UNIV_INLINE
void
sel_reset_aggregate_vals(
/*=====================*/
	sel_node_t*	node)	/* in: select node */
{
	func_node_t*	func_node;

	ut_ad(node->is_aggregate);

	func_node = node->select_list;

	while (func_node) {
		eval_node_set_int_val(func_node, 0);

		func_node = que_node_get_next(func_node);
	}	

	node->aggregate_already_fetched = FALSE;
}

/*************************************************************************
Copies the input variable values when an explicit cursor is opened. */
UNIV_INLINE
void
row_sel_copy_input_variable_vals(
/*=============================*/
	sel_node_t*	node)	/* in: select node */
{
	sym_node_t*	var;

	var = UT_LIST_GET_FIRST(node->copy_variables);

	while (var) {
		eval_node_copy_val(var, var->alias);

		var->indirection = NULL;

		var = UT_LIST_GET_NEXT(col_var_list, var);
	}
}

/*************************************************************************
Fetches the column values from a record. */
static
void
row_sel_fetch_columns(
/*==================*/
	dict_index_t*	index,	/* in: record index */
	rec_t*		rec,	/* in: record in a clustered or non-clustered
				index */
	sym_node_t*	column)	/* in: first column in a column list, or
				NULL */
{
	dfield_t*	val;
	ulint		index_type;
	ulint		field_no;
	byte*		data;
	ulint		len;
	
	if (index->type & DICT_CLUSTERED) {
		index_type = SYM_CLUST_FIELD_NO;
	} else {
		index_type = SYM_SEC_FIELD_NO;
	}

	while (column) {
		field_no = column->field_nos[index_type];

		if (field_no != ULINT_UNDEFINED) {
	
			data = rec_get_nth_field(rec, field_no, &len);
			
			if (column->copy_val) {
				eval_node_copy_and_alloc_val(column, data,
									len);
			} else {
				val = que_node_get_val(column);
				dfield_set_data(val, data, len);
			}
		}

		column = UT_LIST_GET_NEXT(col_var_list, column);
	}
}

/*************************************************************************
Allocates a prefetch buffer for a column when prefetch is first time done. */
static
void
sel_col_prefetch_buf_alloc(
/*=======================*/
	sym_node_t*	column)	/* in: symbol table node for a column */
{
	sel_buf_t*	sel_buf;
	ulint		i;

	ut_ad(que_node_get_type(column) == QUE_NODE_SYMBOL);
	
	column->prefetch_buf = mem_alloc(SEL_MAX_N_PREFETCH
							* sizeof(sel_buf_t));
	for (i = 0; i < SEL_MAX_N_PREFETCH; i++) {
		sel_buf = column->prefetch_buf + i;

		sel_buf->data = NULL;

		sel_buf->val_buf_size = 0;
	}
}

/*************************************************************************
Frees a prefetch buffer for a column, including the dynamically allocated
memory for data stored there. */

void
sel_col_prefetch_buf_free(
/*======================*/
	sel_buf_t*	prefetch_buf)	/* in, own: prefetch buffer */
{
	sel_buf_t*	sel_buf;
	ulint		i;

	for (i = 0; i < SEL_MAX_N_PREFETCH; i++) {
		sel_buf = prefetch_buf + i;

		if (sel_buf->val_buf_size > 0) {

			mem_free(sel_buf->data);
		}
	}
}

/*************************************************************************
Pops the column values for a prefetched, cached row from the column prefetch
buffers and places them to the val fields in the column nodes. */
static
void
sel_pop_prefetched_row(
/*===================*/
	plan_t*	plan)	/* in: plan node for a table */
{
	sym_node_t*	column;
	sel_buf_t*	sel_buf;
	dfield_t*	val;
	byte*		data;
	ulint		len;
	ulint		val_buf_size;
	
	ut_ad(plan->n_rows_prefetched > 0);

	column = UT_LIST_GET_FIRST(plan->columns);

	while (column) {
		val = que_node_get_val(column);

		if (!column->copy_val) {
			/* We did not really push any value for the
			column */

			ut_ad(!column->prefetch_buf);
			ut_ad(que_node_get_val_buf_size(column) == 0);
#ifdef UNIV_DEBUG
			dfield_set_data(val, NULL, 0);
#endif
			goto next_col;
		}

		ut_ad(column->prefetch_buf);

		sel_buf = column->prefetch_buf + plan->first_prefetched;

		data = sel_buf->data;
		len = sel_buf->len;
		val_buf_size = sel_buf->val_buf_size;

		/* We must keep track of the allocated memory for
		column values to be able to free it later: therefore
		we swap the values for sel_buf and val */

		sel_buf->data = dfield_get_data(val);
		sel_buf->len = dfield_get_len(val);
		sel_buf->val_buf_size = que_node_get_val_buf_size(column);
		
		dfield_set_data(val, data, len);
		que_node_set_val_buf_size(column, val_buf_size);
next_col:
		column = UT_LIST_GET_NEXT(col_var_list, column);
	}

	plan->n_rows_prefetched--;

	plan->first_prefetched++;
}

/*************************************************************************
Pushes the column values for a prefetched, cached row to the column prefetch
buffers from the val fields in the column nodes. */
UNIV_INLINE
void
sel_push_prefetched_row(
/*====================*/
	plan_t*	plan)	/* in: plan node for a table */
{
	sym_node_t*	column;
	sel_buf_t*	sel_buf;
	dfield_t*	val;
	byte*		data;
	ulint		len;
	ulint		pos;
	ulint		val_buf_size;

	if (plan->n_rows_prefetched == 0) {
		pos = 0;
		plan->first_prefetched = 0;
	} else {
		pos = plan->n_rows_prefetched;

		/* We have the convention that pushing new rows starts only
		after the prefetch stack has been emptied: */
		
		ut_ad(plan->first_prefetched == 0);
	}

	plan->n_rows_prefetched++;
	
	ut_ad(pos < SEL_MAX_N_PREFETCH);
	
	column = UT_LIST_GET_FIRST(plan->columns);

	while (column) {
		if (!column->copy_val) {
			/* There is no sense to push pointers to database
			page fields when we do not keep latch on the page! */

			goto next_col;
		}
		
		if (!column->prefetch_buf) {
			/* Allocate a new prefetch buffer */

			sel_col_prefetch_buf_alloc(column);
		}

		sel_buf = column->prefetch_buf + pos;

		val = que_node_get_val(column);

		data = dfield_get_data(val);
		len = dfield_get_len(val);
		val_buf_size = que_node_get_val_buf_size(column);

		/* We must keep track of the allocated memory for
		column values to be able to free it later: therefore
		we swap the values for sel_buf and val */

		dfield_set_data(val, sel_buf->data, sel_buf->len);
		que_node_set_val_buf_size(column, sel_buf->val_buf_size);
		
		sel_buf->data = data;
		sel_buf->len = len;
		sel_buf->val_buf_size = val_buf_size;
next_col:		
		column = UT_LIST_GET_NEXT(col_var_list, column);
	}
}

/*************************************************************************
Builds a previous version of a clustered index record for a consistent read */
static
ulint
row_sel_build_prev_vers(
/*====================*/
					/* out: DB_SUCCESS or error code */
	read_view_t*	read_view,	/* in: read view */
	plan_t*		plan,		/* in: plan node for table */
	rec_t*		rec,		/* in: record in a clustered index */
	rec_t**		old_vers,	/* out: old version, or NULL if the
					record does not exist in the view:
					i.e., it was freshly inserted
					afterwards */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	err;

	if (plan->old_vers_heap) {
		mem_heap_empty(plan->old_vers_heap);
	} else {
		plan->old_vers_heap = mem_heap_create(512);
	}
	
	err = row_vers_build_for_consistent_read(rec, mtr, plan->index,
					read_view, plan->old_vers_heap,
					old_vers);
	return(err);
}

/*************************************************************************
Tests the conditions which determine when the index segment we are searching
through has been exhausted. */
UNIV_INLINE
ibool
row_sel_test_end_conds(
/*===================*/
 			/* out: TRUE if row passed the tests */
	plan_t*	plan)	/* in: plan for the table; the column values must
			already have been retrieved and the right sides of
			comparisons evaluated */
{
	func_node_t*	cond;

	/* All conditions in end_conds are comparisons of a column to an
	expression */
	
	cond = UT_LIST_GET_FIRST(plan->end_conds);

	while (cond) {
		/* Evaluate the left side of the comparison, i.e., get the
		column value if there is an indirection */

		eval_sym(cond->args);

		/* Do the comparison */

		if (!eval_cmp(cond)) {

			return(FALSE);
		}

		cond = UT_LIST_GET_NEXT(cond_list, cond);
	}

	return(TRUE);
}

/*************************************************************************
Tests the other conditions. */
UNIV_INLINE
ibool
row_sel_test_other_conds(
/*=====================*/
			/* out: TRUE if row passed the tests */
	plan_t*	plan)	/* in: plan for the table; the column values must
			already have been retrieved */
{
	func_node_t*	cond;
	
	cond = UT_LIST_GET_FIRST(plan->other_conds);

	while (cond) {
		eval_exp(cond);

		if (!eval_node_get_ibool_val(cond)) {

			return(FALSE);
		}

		cond = UT_LIST_GET_NEXT(cond_list, cond);
	}

	return(TRUE);
}

/*************************************************************************
Retrieves the clustered index record corresponding to a record in a
non-clustered index. Does the necessary locking. */
static
ulint
row_sel_get_clust_rec(
/*==================*/
				/* out: DB_SUCCESS or error code */
	sel_node_t*	node,	/* in: select_node */
	plan_t*		plan,	/* in: plan node for table */
	rec_t*		rec,	/* in: record in a non-clustered index */
	que_thr_t*	thr,	/* in: query thread */
	rec_t**		out_rec,/* out: clustered record or an old version of
				it, NULL if the old version did not exist
				in the read view, i.e., it was a fresh
				inserted version */
	mtr_t*		mtr)	/* in: mtr used to get access to the
				non-clustered record; the same mtr is used to
				access the clustered index */
{
	dict_index_t*	index;
	rec_t*		clust_rec;
	rec_t*		old_vers;
	ulint		err;
	
	row_build_row_ref_fast(plan->clust_ref, plan->clust_map, rec);

	index = dict_table_get_first_index(plan->table);
	
	btr_pcur_open_with_no_init(index, plan->clust_ref, PAGE_CUR_LE,
				node->latch_mode, &(plan->clust_pcur),
				0, mtr);

	clust_rec = btr_pcur_get_rec(&(plan->clust_pcur));

	ut_ad(page_rec_is_user_rec(clust_rec));

	if (!node->read_view) {
		/* Try to place a lock on the index record */
		
		err = lock_clust_rec_read_check_and_lock(0, clust_rec, index,
						node->row_lock_mode, thr);
		if (err != DB_SUCCESS) {

			return(err);
		}
	} else {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		old_vers = NULL;

		if (!lock_clust_rec_cons_read_sees(clust_rec, index,
							node->read_view)) {

			err = row_sel_build_prev_vers(node->read_view, plan,
						clust_rec, &old_vers, mtr);
			if (err != DB_SUCCESS) {

				return(err);
			}

			clust_rec = old_vers;

			if (clust_rec == NULL) {
				*out_rec = clust_rec;

				return(DB_SUCCESS);
			}
		}

		/* If we had to go to an earlier version of row or the
		secondary index record is delete marked, then it may be that
		the secondary index record corresponding to clust_rec
		(or old_vers) is not rec; in that case we must ignore
		such row because in our snapshot rec would not have existed.
		Remember that from rec we cannot see directly which transaction
		id corresponds to it: we have to go to the clustered index
		record. A query where we want to fetch all rows where
		the secondary index value is in some interval would return
		a wrong result if we would not drop rows which we come to
		visit through secondary index records that would not really
		exist in our snapshot. */
		
		if ((old_vers || rec_get_deleted_flag(rec)) 
		    && !row_sel_sec_rec_is_for_clust_rec(rec, plan->index,
							clust_rec, index)) {
			clust_rec = NULL;
			*out_rec = clust_rec;

			return(DB_SUCCESS);
		}								
	}

	/* Fetch the columns needed in test conditions */
	
	row_sel_fetch_columns(index, clust_rec,
					UT_LIST_GET_FIRST(plan->columns));
	*out_rec = clust_rec;

	return(DB_SUCCESS);
}

/*************************************************************************
Sets a lock on a record. */
UNIV_INLINE
ulint
sel_set_rec_lock(
/*=============*/
				/* out: DB_SUCCESS or error code */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index */
	ulint		mode,	/* in: lock mode */
	que_thr_t*	thr)	/* in: query thread */	
{
	ulint	err;

	if (index->type & DICT_CLUSTERED) {
		err = lock_clust_rec_read_check_and_lock(0, rec, index, mode,
									thr);
	} else {
		err = lock_sec_rec_read_check_and_lock(0, rec, index, mode,
									thr);
	}

	return(err);
}

/*************************************************************************
Opens a pcur to a table index. */
static
void
row_sel_open_pcur(
/*==============*/
	sel_node_t*	node,		/* in: select node */
	plan_t*		plan,		/* in: table plan */
	ibool		search_latch_locked,
					/* in: TRUE if the thread currently
					has the search latch locked in
					s-mode */
	mtr_t*		mtr)		/* in: mtr */
{
	dict_index_t*	index;
	func_node_t*	cond;
	que_node_t*	exp;
	ulint		n_fields;
	ulint		has_search_latch = 0;	/* RW_S_LATCH or 0 */ 
	ulint		i;

	if (search_latch_locked) {
		has_search_latch = RW_S_LATCH;
	}

	index = plan->index;

	/* Calculate the value of the search tuple: the exact match columns
	get their expressions evaluated when we evaluate the right sides of
	end_conds */

	cond = UT_LIST_GET_FIRST(plan->end_conds);

	while (cond) {
		eval_exp(que_node_get_next(cond->args));
	
		cond = UT_LIST_GET_NEXT(cond_list, cond);
	}
	
	if (plan->tuple) {
		n_fields = dtuple_get_n_fields(plan->tuple);
	
		if (plan->n_exact_match < n_fields) {
			/* There is a non-exact match field which must be
			evaluated separately */
			
			eval_exp(plan->tuple_exps[n_fields - 1]);
		}
		
		for (i = 0; i < n_fields; i++) {
			exp = plan->tuple_exps[i];
	
			dfield_copy_data(dtuple_get_nth_field(plan->tuple, i),
							que_node_get_val(exp));
		}
	
		/* Open pcur to the index */
	
		btr_pcur_open_with_no_init(index, plan->tuple, plan->mode,
					node->latch_mode, &(plan->pcur),
					has_search_latch, mtr);
	} else {
		/* Open the cursor to the start or the end of the index
		(FALSE: no init) */

		btr_pcur_open_at_index_side(plan->asc, index, node->latch_mode,
						&(plan->pcur), FALSE, mtr);
	}

	ut_ad(plan->n_rows_prefetched == 0);
	ut_ad(plan->n_rows_fetched == 0);
	ut_ad(plan->cursor_at_end == FALSE);
 
	plan->pcur_is_open = TRUE;
}

/*************************************************************************
Restores a stored pcur position to a table index. */
UNIV_INLINE
ibool
row_sel_restore_pcur_pos(
/*=====================*/
				/* out: TRUE if the cursor should be moved to
				the next record after we return from this
				function (moved to the previous, in the case
				of a descending cursor) without processing
				again the current cursor record */
	sel_node_t*	node,	/* in: select node */
	plan_t*		plan,	/* in: table plan */
	mtr_t*		mtr)	/* in: mtr */
{
	ibool	equal_position;
	ulint	relative_position;

	ut_ad(!plan->cursor_at_end);
	
	relative_position = btr_pcur_get_rel_pos(&(plan->pcur));

	equal_position = btr_pcur_restore_position(node->latch_mode,
							&(plan->pcur), mtr);

	/* If the cursor is traveling upwards, and relative_position is
	
	(1) BTR_PCUR_BEFORE: this is not allowed, as we did not have a lock
	yet on the successor of the page infimum;
	(2) BTR_PCUR_AFTER: btr_pcur_restore_position placed the cursor on the
	first record GREATER than the predecessor of a page supremum; we have
	not yet processed the cursor record: no need to move the cursor to the
	next record;
	(3) BTR_PCUR_ON: btr_pcur_restore_position placed the cursor on the
	last record LESS or EQUAL to the old stored user record; (a) if
	equal_position is FALSE, this means that the cursor is now on a record
	less than the old user record, and we must move to the next record;
	(b) if equal_position is TRUE, then if
	plan->stored_cursor_rec_processed is TRUE, we must move to the next
	record, else there is no need to move the cursor. */

	if (plan->asc) {
		if (relative_position == BTR_PCUR_ON) {

			if (equal_position) {

				return(plan->stored_cursor_rec_processed);
			}

			return(TRUE);
		}

		ut_ad(relative_position == BTR_PCUR_AFTER);

		return(FALSE);
	}

	/* If the cursor is traveling downwards, and relative_position is
	
	(1) BTR_PCUR_BEFORE: btr_pcur_restore_position placed the cursor on
	the last record LESS than the successor of a page infimum; we have not
	processed the cursor record: no need to move the cursor;
	(2) BTR_PCUR_AFTER: btr_pcur_restore_position placed the cursor on the
	first record GREATER than the predecessor of a page supremum; we have
	processed the cursor record: we should move the cursor to the previous
	record;
	(3) BTR_PCUR_ON: btr_pcur_restore_position placed the cursor on the
	last record LESS or EQUAL to the old stored user record; (a) if
	equal_position is FALSE, this means that the cursor is now on a record
	less than the old user record, and we need not move to the previous
	record; (b) if equal_position is TRUE, then if
	plan->stored_cursor_rec_processed is TRUE, we must move to the previous
	record, else there is no need to move the cursor. */

	if (relative_position == BTR_PCUR_BEFORE) {

		return(FALSE);
	}

	if (relative_position == BTR_PCUR_ON) {

		if (equal_position) {

			return(plan->stored_cursor_rec_processed);
		}

		return(FALSE);
	}

	ut_ad(relative_position == BTR_PCUR_AFTER);

	return(TRUE);
}

/*************************************************************************
Resets a plan cursor to a closed state. */
UNIV_INLINE
void
plan_reset_cursor(
/*==============*/
	plan_t*	plan)	/* in: plan */
{	
	plan->pcur_is_open = FALSE;
	plan->cursor_at_end = FALSE;	
	plan->n_rows_fetched = 0;
	plan->n_rows_prefetched = 0;
}
	
/*************************************************************************
Tries to do a shortcut to fetch a clustered index record with a unique key,
using the hash index if possible (not always). */
static
ulint
row_sel_try_search_shortcut(
/*========================*/
				/* out: SEL_FOUND, SEL_EXHAUSTED, SEL_RETRY */
	sel_node_t*	node,	/* in: select node for a consistent read */
	plan_t*		plan,	/* in: plan for a unique search in clustered
				index */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_index_t*	index;
	rec_t*		rec;

	index = plan->index;

	ut_ad(node->read_view);
	ut_ad(plan->unique_search);
	ut_ad(!plan->must_get_clust);
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
	
	row_sel_open_pcur(node, plan, TRUE, mtr);

	rec = btr_pcur_get_rec(&(plan->pcur));
	
	if (!page_rec_is_user_rec(rec)) {

		return(SEL_RETRY);
	}

	ut_ad(plan->mode == PAGE_CUR_GE);
	
	/* As the cursor is now placed on a user record after a search with
	the mode PAGE_CUR_GE, the up_match field in the cursor tells how many
	fields in the user record matched to the search tuple */ 

	if (btr_pcur_get_up_match(&(plan->pcur)) < plan->n_exact_match) {

		return(SEL_EXHAUSTED);
	}

	/* This is a non-locking consistent read: if necessary, fetch
	a previous version of the record */
			
	if (index->type & DICT_CLUSTERED) {
		if (!lock_clust_rec_cons_read_sees(rec, index,
							node->read_view)) {
			return(SEL_RETRY);
		}
	} else if (!lock_sec_rec_cons_read_sees(rec, index, node->read_view)) {

		return(SEL_RETRY);
	}

	/* Test deleted flag. Fetch the columns needed in test conditions. */
	
	row_sel_fetch_columns(index, rec, UT_LIST_GET_FIRST(plan->columns));

	if (rec_get_deleted_flag(rec)) {

		return(SEL_EXHAUSTED);
	}

	/* Test the rest of search conditions */
	
	if (!row_sel_test_other_conds(plan)) {

		return(SEL_EXHAUSTED);
	}

	ut_ad(plan->pcur.latch_mode == node->latch_mode);

	plan->n_rows_fetched++;

	return(SEL_FOUND);
}

/*************************************************************************
Performs a select step. */
static
ulint
row_sel(
/*====*/
				/* out: DB_SUCCESS or error code */
	sel_node_t*	node,	/* in: select node */
	que_thr_t*	thr)	/* in: query thread */
{
	dict_index_t*	index;
	plan_t*		plan;
	mtr_t		mtr;
	ibool		moved;
	rec_t*		rec;
	rec_t*		old_vers;
	rec_t*		clust_rec;
	ibool		search_latch_locked;
	ibool		consistent_read;
	
		/* The following flag becomes TRUE when we are doing a
		consistent read from a non-clustered index and we must look
		at the clustered index to find out the previous delete mark
		state of the non-clustered record: */

	ibool		cons_read_requires_clust_rec	= FALSE;
	ulint		cost_counter			= 0;
	ibool		cursor_just_opened;
	ibool		must_go_to_next;
	ibool		leaf_contains_updates 		= FALSE;
					/* TRUE if select_will_do_update is
					TRUE and the current clustered index
					leaf page has been updated during
					the current mtr: mtr must be committed
					at the same time as the leaf x-latch
					is released */
	ibool		mtr_has_extra_clust_latch 	= FALSE;
					/* TRUE if the search was made using
					a non-clustered index, and we had to
					access the clustered record: now &mtr
					contains a clustered index latch, and
					&mtr must be committed before we move
					to the next non-clustered record */
	ulint		found_flag;
	ulint		err;
	
	ut_ad(thr->run_node == node);

	search_latch_locked = FALSE;

	if (node->read_view) {
		/* In consistent reads, we try to do with the hash index and
		not to use the buffer page get. This is to reduce memory bus
		load resulting from semaphore operations. The search latch
		will be s-locked when we access an index with a unique search
		condition, but not locked when we access an index with a
		less selective search condition. */

		consistent_read = TRUE;
	} else {
		consistent_read = FALSE;
	}

table_loop:
	/* TABLE LOOP
	   ----------
	This is the outer major loop in calculating a join. We come here when
	node->fetch_table changes, and after adding a row to aggregate totals
	and, of course, when this function is called. */

	ut_ad(leaf_contains_updates == FALSE);
	ut_ad(mtr_has_extra_clust_latch == FALSE);

	plan = sel_node_get_nth_plan(node, node->fetch_table);
	index = plan->index;

	if (plan->n_rows_prefetched > 0) {
		sel_pop_prefetched_row(plan);

		goto next_table_no_mtr;
	}

	if (plan->cursor_at_end) {
		/* The cursor has already reached the result set end: no more
		rows to process for this table cursor, as also the prefetch
		stack was empty */

		ut_ad(plan->pcur_is_open);

		goto table_exhausted_no_mtr;
	}

	/* Open a cursor to index, or restore an open cursor position */
	
	mtr_start(&mtr);

	if (consistent_read && plan->unique_search && !plan->pcur_is_open
						&& !plan->must_get_clust) {
		if (!search_latch_locked) {
			rw_lock_s_lock(&btr_search_latch);

			search_latch_locked = TRUE;
		} else if (btr_search_latch.writer_is_wait_ex) {

			/* There is an x-latch request waiting: release the
			s-latch for a moment; as an s-latch here is often
			kept for some 10 searches before being released,
			a waiting x-latch request would block other threads
			from acquiring an s-latch for a long time, lowering
			performance significantly in multiprocessors. */

			rw_lock_s_unlock(&btr_search_latch);
			rw_lock_s_lock(&btr_search_latch);
		}

		found_flag = row_sel_try_search_shortcut(node, plan, &mtr);

		if (found_flag == SEL_FOUND) {

			goto next_table;

		} else if (found_flag == SEL_EXHAUSTED) {

			goto table_exhausted;
		}
		
		ut_ad(found_flag == SEL_RETRY);

		plan_reset_cursor(plan);

		mtr_commit(&mtr);
		mtr_start(&mtr);
	}

	if (search_latch_locked) {
		rw_lock_s_unlock(&btr_search_latch);

		search_latch_locked = FALSE;
	}

	if (!plan->pcur_is_open) {
		/* Evaluate the expressions to build the search tuple and
		open the cursor */

		row_sel_open_pcur(node, plan, search_latch_locked, &mtr);

		cursor_just_opened = TRUE;

		/* A new search was made: increment the cost counter */
		cost_counter++;
	} else {
		/* Restore pcur position to the index */

		must_go_to_next = row_sel_restore_pcur_pos(node, plan, &mtr);

		cursor_just_opened = FALSE;

		if (must_go_to_next) {
			/* We have already processed the cursor record: move
			to the next */
		
			goto next_rec;
		}
	}
	
rec_loop:
	/* RECORD LOOP
	   -----------
	In this loop we use pcur and try to fetch a qualifying row, and
	also fill the prefetch buffer for this table if n_rows_fetched has
	exceeded a threshold. While we are inside this loop, the following
	holds:
	(1) &mtr is started,
	(2) pcur is positioned and open.

	NOTE that if cursor_just_opened is TRUE here, it means that we came
	to this point right after row_sel_open_pcur. */
	
	ut_ad(mtr_has_extra_clust_latch == FALSE);

	rec = btr_pcur_get_rec(&(plan->pcur));
	
	/* PHASE 1: Set a lock if specified */

	if (!node->asc && cursor_just_opened
		&& (rec != page_get_supremum_rec(buf_frame_align(rec)))) {

		/* When we open a cursor for a descending search, we must set
		a next-key lock on the successor record: otherwise it would
		be possible to insert new records next to the cursor position,
		and it might be that these new records should appear in the
		search result set, resulting in the phantom problem. */
		
		if (!consistent_read) {
			err = sel_set_rec_lock(page_rec_get_next(rec), index,
						node->row_lock_mode, thr);
			if (err != DB_SUCCESS) {
				/* Note that in this case we will store in pcur
				the PREDECESSOR of the record we are waiting
				the lock for */
				
				goto lock_wait_or_error;
			}
		}
	}

	if (rec == page_get_infimum_rec(buf_frame_align(rec))) {

		/* The infimum record on a page cannot be in the result set,
		and neither can a record lock be placed on it: we skip such
		a record. We also increment the cost counter as we may have
		processed yet another page of index. */

		cost_counter++;

		goto next_rec;
	}

	if (!consistent_read) {
		/* Try to place a lock on the index record */	

		err = sel_set_rec_lock(rec, index, node->row_lock_mode, thr);

		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}
	}

	if (rec == page_get_supremum_rec(buf_frame_align(rec))) {

		/* A page supremum record cannot be in the result set: skip
		it now when we have placed a possible lock on it */

		goto next_rec;
	}

	ut_ad(page_rec_is_user_rec(rec));

	if (cost_counter > SEL_COST_LIMIT) {
		
		/* Now that we have placed the necessary locks, we can stop
		for a while and store the cursor position; NOTE that if we
		would store the cursor position BEFORE placing a record lock,
		it might happen that the cursor would jump over some records
		that another transaction could meanwhile insert adjacent to
		the cursor: this would result in the phantom problem. */

		goto stop_for_a_while;
	}
	
	/* PHASE 2: Check a mixed index mix id if needed */

	if (plan->unique_search && cursor_just_opened) {

		ut_ad(plan->mode == PAGE_CUR_GE);
	
		/* As the cursor is now placed on a user record after a search
		with the mode PAGE_CUR_GE, the up_match field in the cursor
		tells how many fields in the user record matched to the search
		tuple */ 

		if (btr_pcur_get_up_match(&(plan->pcur))
						< plan->n_exact_match) {
			goto table_exhausted;
		}

		/* Ok, no need to test end_conds or mix id */

	} else if (plan->mixed_index) {
	    	/* We have to check if the record in a mixed cluster belongs
	    	to this table */

	 	if (!dict_is_mixed_table_rec(plan->table, rec)) {

	    		goto next_rec;
	    	}
	}

	/* We are ready to look at a possible new index entry in the result
	set: the cursor is now placed on a user record */

	/* PHASE 3: Get previous version in a consistent read */

	if (consistent_read) {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		if (index->type & DICT_CLUSTERED) {
			
			if (!lock_clust_rec_cons_read_sees(rec, index,
							node->read_view)) {

				err = row_sel_build_prev_vers(node->read_view,
							plan, rec, &old_vers,
							&mtr);
				if (err != DB_SUCCESS) {

					goto lock_wait_or_error;
				}

				if (old_vers == NULL) {
					row_sel_fetch_columns(index, rec,
					    UT_LIST_GET_FIRST(plan->columns));

					if (!row_sel_test_end_conds(plan)) {

						goto table_exhausted;
					}

					goto next_rec;
				}

				rec = old_vers;
			}
		} else if (!lock_sec_rec_cons_read_sees(rec, index,
							node->read_view)) {
			cons_read_requires_clust_rec = TRUE;
		}
	}

	/* PHASE 4: Test search end conditions and deleted flag */

	/* Fetch the columns needed in test conditions */
	
	row_sel_fetch_columns(index, rec, UT_LIST_GET_FIRST(plan->columns));

	/* Test the selection end conditions: these can only contain columns
	which already are found in the index, even though the index might be
	non-clustered */

	if (plan->unique_search && cursor_just_opened) {

		/* No test necessary: the test was already made above */

	} else if (!row_sel_test_end_conds(plan)) {

		goto table_exhausted;
	}

	if (rec_get_deleted_flag(rec) && !cons_read_requires_clust_rec) {

		/* The record is delete marked: we can skip it if this is
		not a consistent read which might see an earlier version
		of a non-clustered index record */

		if (plan->unique_search) {
			
			goto table_exhausted;
		}
		
		goto next_rec;
	}

	/* PHASE 5: Get the clustered index record, if needed and if we did
	not do the search using the clustered index */

	if (plan->must_get_clust || cons_read_requires_clust_rec) {

		/* It was a non-clustered index and we must fetch also the
		clustered index record */

		err = row_sel_get_clust_rec(node, plan, rec, thr, &clust_rec,
									&mtr);
		mtr_has_extra_clust_latch = TRUE;
		
		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}

		/* Retrieving the clustered record required a search:
		increment the cost counter */

		cost_counter++;

		if (clust_rec == NULL) {
			/* The record did not exist in the read view */
			ut_ad(consistent_read);

			goto next_rec;
		}

		if (rec_get_deleted_flag(clust_rec)) {

			/* The record is delete marked: we can skip it */

			goto next_rec;
		}

		if (node->can_get_updated) {

			btr_pcur_store_position(&(plan->clust_pcur), &mtr);
		}
	}	

	/* PHASE 6: Test the rest of search conditions */
	
	if (!row_sel_test_other_conds(plan)) {

		if (plan->unique_search) {
			
			goto table_exhausted;
		}

		goto next_rec;
	}

	/* PHASE 7: We found a new qualifying row for the current table; push
	the row if prefetch is on, or move to the next table in the join */
	
	plan->n_rows_fetched++;

	ut_ad(plan->pcur.latch_mode == node->latch_mode);

	if (node->select_will_do_update) {
		/* This is a searched update and we can do the update in-place,
		saving CPU time */

		row_upd_in_place_in_select(node, thr, &mtr);

		leaf_contains_updates = TRUE;

		/* When the database is in the online backup mode, the number
		of log records for a single mtr should be small: increment the
		cost counter to ensure it */
		
		cost_counter += 1 + (SEL_COST_LIMIT / 8);

		if (plan->unique_search) {

			goto table_exhausted;			
		}

		goto next_rec;
	}	

	if ((plan->n_rows_fetched <= SEL_PREFETCH_LIMIT)
				|| plan->unique_search || plan->no_prefetch) {

		/* No prefetch in operation: go to the next table */
	
		goto next_table;
	}

	sel_push_prefetched_row(plan);

	if (plan->n_rows_prefetched == SEL_MAX_N_PREFETCH) {

		/* The prefetch buffer is now full */
		
		sel_pop_prefetched_row(plan);

		goto next_table;
	}

next_rec:	
	ut_ad(!search_latch_locked);

	if (mtr_has_extra_clust_latch) {

		/* We must commit &mtr if we are moving to the next
		non-clustered index record, because we could break the
		latching order if we would access a different clustered
		index page right away without releasing the previous. */

		goto commit_mtr_for_a_while;
	}
	
	if (leaf_contains_updates
		&& btr_pcur_is_after_last_on_page(&(plan->pcur), &mtr)) {

		/* We must commit &mtr if we are moving to a different page,
		because we have done updates to the x-latched leaf page, and
		the latch would be released in btr_pcur_move_to_next, without
		&mtr getting committed there */

		ut_ad(node->asc);

		goto commit_mtr_for_a_while;
	}

	if (node->asc) {
		moved = btr_pcur_move_to_next(&(plan->pcur), &mtr);
	} else {
		moved = btr_pcur_move_to_prev(&(plan->pcur), &mtr);
	}

	if (!moved) {
		
		goto table_exhausted;
	}

	cursor_just_opened = FALSE;

	/* END OF RECORD LOOP
	   ------------------ */
	goto rec_loop;

next_table:
	/* We found a record which satisfies the conditions: we can move to
	the next table or return a row in the result set */

	ut_ad(btr_pcur_is_on_user_rec(&(plan->pcur), &mtr));
	
	if (plan->unique_search && !node->can_get_updated) {

		plan->cursor_at_end = TRUE;
	} else {
		ut_ad(!search_latch_locked);

		plan->stored_cursor_rec_processed = TRUE;

		btr_pcur_store_position(&(plan->pcur), &mtr);
	}

	mtr_commit(&mtr);

	leaf_contains_updates = FALSE;
	mtr_has_extra_clust_latch = FALSE;

next_table_no_mtr:
	/* If we use 'goto' to this label, it means that the row was popped
	from the prefetched rows stack, and &mtr is already committed */
	
	if (node->fetch_table + 1 == node->n_tables) {

		sel_eval_select_list(node);

		if (node->is_aggregate) {

			goto table_loop;			
		}

		sel_assign_into_var_values(node->into_list, node);
		
		thr->run_node = que_node_get_parent(node);

		if (search_latch_locked) {
			rw_lock_s_unlock(&btr_search_latch);
		}
		
		return(DB_SUCCESS);
	}

	node->fetch_table++;

	/* When we move to the next table, we first reset the plan cursor:
	we do not care about resetting it when we backtrack from a table */
	
	plan_reset_cursor(sel_node_get_nth_plan(node, node->fetch_table));
	
	goto table_loop;

table_exhausted:
	/* The table cursor pcur reached the result set end: backtrack to the
	previous table in the join if we do not have cached prefetched rows */	

	plan->cursor_at_end = TRUE;

	mtr_commit(&mtr);

	leaf_contains_updates = FALSE;
	mtr_has_extra_clust_latch = FALSE;
	
	if (plan->n_rows_prefetched > 0) {
		/* The table became exhausted during a prefetch */
	
		sel_pop_prefetched_row(plan);

		goto next_table_no_mtr;
	}

table_exhausted_no_mtr:
	if (node->fetch_table == 0) {

		if (node->is_aggregate && !node->aggregate_already_fetched) {

			node->aggregate_already_fetched = TRUE;

			sel_assign_into_var_values(node->into_list, node);

			thr->run_node = que_node_get_parent(node);

			if (search_latch_locked) {
				rw_lock_s_unlock(&btr_search_latch);
			}
		
			return(DB_SUCCESS);
		}

		node->state = SEL_NODE_NO_MORE_ROWS;
		
		thr->run_node = que_node_get_parent(node);

		if (search_latch_locked) {
			rw_lock_s_unlock(&btr_search_latch);
		}
		
		return(DB_SUCCESS);
	}

	node->fetch_table--;

	goto table_loop;

stop_for_a_while:
	/* Return control for a while to que_run_threads, so that runaway
	queries can be canceled. NOTE that when we come here, we must, in a
	locking read, have placed the necessary (possibly waiting request)
	record lock on the cursor record or its successor: when we reposition
	the cursor, this record lock guarantees that nobody can meanwhile have
	inserted new records which should have appeared in the result set,
	which would result in the phantom problem. */ 

	ut_ad(!search_latch_locked);

	plan->stored_cursor_rec_processed = FALSE;
	btr_pcur_store_position(&(plan->pcur), &mtr);

	mtr_commit(&mtr);
		
	ut_ad(sync_thread_levels_empty_gen(TRUE));

	return(DB_SUCCESS);

commit_mtr_for_a_while:
	/* Stores the cursor position and commits &mtr; this is used if
	&mtr may contain latches which would break the latching order if
	&mtr would not be committed and the latches released. */ 

	plan->stored_cursor_rec_processed = TRUE;

	ut_ad(!search_latch_locked);
	btr_pcur_store_position(&(plan->pcur), &mtr);

	mtr_commit(&mtr);

	leaf_contains_updates = FALSE;
	mtr_has_extra_clust_latch = FALSE;
	
	ut_ad(sync_thread_levels_empty_gen(TRUE));

	goto table_loop;

lock_wait_or_error:
	/* See the note at stop_for_a_while: the same holds for this case */

	ut_ad(!btr_pcur_is_before_first_on_page(&(plan->pcur), &mtr)
							|| !node->asc);
	ut_ad(!search_latch_locked);

	plan->stored_cursor_rec_processed = FALSE;
	btr_pcur_store_position(&(plan->pcur), &mtr);
	
	mtr_commit(&mtr);
		
	ut_ad(sync_thread_levels_empty_gen(TRUE));

	return(err);
}

/**************************************************************************
Performs a select step. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
row_sel_step(
/*=========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	ulint		i_lock_mode;
	sym_node_t*	table_node;
	sel_node_t*	node;
	ulint		err;

	ut_ad(thr);
	
	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_SELECT);

	/* If this is a new time this node is executed (or when execution
	resumes after wait for a table intention lock), set intention locks
	on the tables, or assign a read view */

	if (node->into_list && (thr->prev_node == que_node_get_parent(node))) {

		node->state = SEL_NODE_OPEN;
	}

	if (node->state == SEL_NODE_OPEN) {

		/* It may be that the current session has not yet started
		its transaction, or it has been committed: */

		trx_start_if_not_started(thr_get_trx(thr));

		plan_reset_cursor(sel_node_get_nth_plan(node, 0));

		if (node->consistent_read) {
			/* Assign a read view for the query */
			node->read_view = trx_assign_read_view(
							thr_get_trx(thr));
		} else {
			if (node->set_x_locks) {
				i_lock_mode = LOCK_IX;
			} else {
				i_lock_mode = LOCK_IS;
			}
	
			table_node = node->table_list;
	
			while (table_node) {
				err = lock_table(0, table_node->table,
							i_lock_mode, thr);
				if (err != DB_SUCCESS) {
	
					que_thr_handle_error(thr, DB_ERROR,
								NULL, 0);
					return(NULL);
				}
	
				table_node = que_node_get_next(table_node);
			}
		}
	
		/* If this is an explicit cursor, copy stored procedure
		variable values, so that the values cannot change between
		fetches (currently, we copy them also for non-explicit
		cursors) */

		if (node->explicit_cursor &&
				UT_LIST_GET_FIRST(node->copy_variables)) {

			row_sel_copy_input_variable_vals(node);
		}
		
		node->state = SEL_NODE_FETCH;
		node->fetch_table = 0;

		if (node->is_aggregate) {
			/* Reset the aggregate total values */
			sel_reset_aggregate_vals(node);
		}
	}

	err = row_sel(node, thr);

	/* NOTE! if queries are parallelized, the following assignment may
	have problems; the assignment should be made only if thr is the
	only top-level thr in the graph: */
	
	thr->graph->last_sel_node = node;

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */

	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
		/* SQL error detected */
		printf("SQL error %lu\n", err);

		que_thr_handle_error(thr, DB_ERROR, NULL, 0);

		return(NULL);
	}

	return(thr);
} 

/**************************************************************************
Performs a fetch for a cursor. */

que_thr_t*
fetch_step(
/*=======*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	sel_node_t*	sel_node;
	fetch_node_t*	node;

	ut_ad(thr);
	
	node = thr->run_node;
	sel_node = node->cursor_def;
	
	ut_ad(que_node_get_type(node) == QUE_NODE_FETCH);

	if (thr->prev_node != que_node_get_parent(node)) {

		if (sel_node->state != SEL_NODE_NO_MORE_ROWS) {
			
			sel_assign_into_var_values(node->into_list, sel_node);
		}

		thr->run_node = que_node_get_parent(node);

		return(thr);
	}

	/* Make the fetch node the parent of the cursor definition for
	the time of the fetch, so that execution knows to return to this
	fetch node after a row has been selected or we know that there is
	no row left */
		
	sel_node->common.parent = node;
	
	if (sel_node->state == SEL_NODE_CLOSED) {
		/* SQL error detected */
		printf("SQL error %lu\n", DB_ERROR);

		que_thr_handle_error(thr, DB_ERROR, NULL, 0);

		return(NULL);
	}

	thr->run_node = sel_node;

	return(thr);
} 

/***************************************************************
Prints a row in a select result. */

que_thr_t*
row_printf_step(
/*============*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	row_printf_node_t*	node;
	sel_node_t*		sel_node;
	que_node_t*		arg;

	ut_ad(thr);
	
	node = thr->run_node;
	
	sel_node = node->sel_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_ROW_PRINTF);

	if (thr->prev_node == que_node_get_parent(node)) {
	
		/* Reset the cursor */
		sel_node->state = SEL_NODE_OPEN;

		/* Fetch next row to print */

		thr->run_node = sel_node;
		
		return(thr);
	}

	if (sel_node->state != SEL_NODE_FETCH) {

		ut_ad(sel_node->state == SEL_NODE_NO_MORE_ROWS);

		/* No more rows to print */

		thr->run_node = que_node_get_parent(node);
	
		return(thr);
	}

	arg = sel_node->select_list;

	while (arg) {
		dfield_print_also_hex(que_node_get_val(arg));

		printf(" ::: ");

		arg = que_node_get_next(arg);
	}

	printf("\n");

	/* Fetch next row to print */

	thr->run_node = sel_node;

	return(thr);
} 

/********************************************************************
Converts a key value stored in MySQL format to an Innobase dtuple.
The last field of the key value may be just a prefix of a fixed length
field: hence the parameter key_len. */

void
row_sel_convert_mysql_key_to_innobase(
/*==================================*/
	dtuple_t*	tuple,		/* in: tuple where to build;
					NOTE: we assume that the type info
					in the tuple is already according
					to index! */
	byte*		buf,		/* in: buffer to use in field
					conversions */
	dict_index_t*	index,		/* in: index of the key value */
	byte*		key_ptr,	/* in: MySQL key value */
	ulint		key_len)	/* in: MySQL key value length */
{
	dfield_t*	dfield;
	ulint		offset;
	ulint		len;
	byte*		key_end;
	ulint		n_fields = 0;
	
	UT_NOT_USED(index);

	key_end = key_ptr + key_len;

	/* Permit us to access any field in the tuple (ULINT_MAX): */
	
	dtuple_set_n_fields(tuple, ULINT_MAX);

	dfield = dtuple_get_nth_field(tuple, 0);

	if (dfield_get_type(dfield)->mtype == DATA_SYS) {
		/* A special case: we are looking for a position in a
		generated clustered index: the first and the only
		ordering column is ROW_ID */

		ut_a(key_len == DATA_ROW_ID_LEN);

		dfield_set_data(dfield, key_ptr, DATA_ROW_ID_LEN);
					
		dtuple_set_n_fields(tuple, 1);

		return;
	}

  	while (key_ptr < key_end) {
		offset = 0;
		len = dfield_get_type(dfield)->len;

		n_fields++;    		

    		if (!(dfield_get_type(dfield)->prtype & DATA_NOT_NULL)) {
    			/* The first byte in the field tells if this is
    			an SQL NULL value */
    			
    			offset = 1;

			if (*key_ptr != 0) {
      				dfield_set_data(dfield, NULL, UNIV_SQL_NULL);

      				goto next_part;
      			}
      		}

		row_mysql_store_col_in_innobase_format(
				dfield, buf, key_ptr + offset, len,
					dfield_get_type(dfield)->mtype,
					dfield_get_type(dfield)->prtype
							& DATA_UNSIGNED);
	next_part:
    		key_ptr += (offset + len);

		if (key_ptr > key_end) {
			/* The last field in key was not a complete
			field but a prefix of it */

			ut_ad(dfield_get_len(dfield) != UNIV_SQL_NULL);
			
			dfield_set_data(dfield, buf,
					len - (ulint)(key_ptr - key_end));
		}

		buf += len;
    		
		dfield++;
  	}

 	/* We set the length of tuple to n_fields: we assume that
	the memory area allocated for it is big enough (usually
	bigger than n_fields). */
 	
 	dtuple_set_n_fields(tuple, n_fields);
}

/******************************************************************
Stores the row id to the prebuilt struct. */
UNIV_INLINE
void
row_sel_store_row_id_to_prebuilt(
/*=============================*/
	row_prebuilt_t*	prebuilt,	/* in: prebuilt */
	rec_t*		index_rec,	/* in: record */
	dict_index_t*	index)		/* in: index of the record */
{
	byte*	data;
	ulint	len;

	data = rec_get_nth_field(index_rec,
			dict_index_get_sys_col_pos(index, DATA_ROW_ID), &len);

	ut_a(len == DATA_ROW_ID_LEN);

	ut_memcpy(prebuilt->row_id, data, len);
}

/******************************************************************
Stores a non-SQL-NULL field in the MySQL format. */
UNIV_INLINE
void
row_sel_field_store_in_mysql_format(
/*================================*/
	byte*	dest,	/* in/out: buffer where to store; NOTE that BLOBs
			are not in themselves stored here: the caller must
			allocate and copy the BLOB into buffer before, and pass
			the pointer to the BLOB in 'data' */
	ulint	col_len,/* in: MySQL column length */
	byte*	data,	/* in: data to store */
	ulint	len,	/* in: length of the data */
	ulint	type,	/* in: data type */
	ulint	is_unsigned)/* in: != 0 if an unsigned integer type */
{
	byte*	ptr;

	ut_ad(len != UNIV_SQL_NULL);

	if (type == DATA_INT) {
		/* Convert integer data from Innobase to a little-endian
		format, sign bit restored to normal */

		ptr = dest + len;

		for (;;) {
			ptr--;
			*ptr = *data;
			if (ptr == dest) {
				break;
			}
			data++;
		}

		if (!is_unsigned) {
			dest[len - 1] = (byte) (dest[len - 1] ^ 128);
		}

		ut_ad(col_len == len);
	} else if (type == DATA_VARCHAR || type == DATA_VARMYSQL
						|| type == DATA_BINARY) {
		/* Store the length of the data to the first two bytes of
		dest; does not do anything yet because MySQL has
		no real vars! */
		
		dest = row_mysql_store_var_len(dest, len);
		ut_memcpy(dest, data, len);

		/* Pad with trailing spaces */
		memset(dest + len, ' ', col_len - len); 

		/* ut_ad(col_len >= len + 2); No real var implemented in
		MySQL yet! */
		
	} else if (type == DATA_BLOB) {
		/* Store a pointer to the BLOB buffer to dest: the BLOB was
		already copied to the buffer in row_sel_store_mysql_rec */

		row_mysql_store_blob_ref(dest, col_len, data, len);
	} else {
		ut_memcpy(dest, data, len);
		ut_ad(col_len == len);
	}
}

/******************************************************************
Convert a row in the Innobase format to a row in the MySQL format.
Note that the template in prebuilt may advise us to copy only a few
columns to mysql_rec, other columns are left blank. All columns may not
be needed in the query. */
static
void
row_sel_store_mysql_rec(
/*====================*/
	byte*		mysql_rec,	/* out: row in the MySQL format */
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct */
	rec_t*		rec)		/* in: Innobase record in the index
					which was described in prebuilt's
					template */
{
	mysql_row_templ_t*	templ;
	mem_heap_t*		extern_field_heap	= NULL;
	byte*			data;
	ulint			len;
	byte*			blob_buf;
	ulint			i;
	
	ut_ad(prebuilt->mysql_template);

	if (prebuilt->blob_heap != NULL) {
		mem_heap_free(prebuilt->blob_heap);
		prebuilt->blob_heap = NULL;
	}

	/* Mark all columns as not SQL NULL */

	memset(mysql_rec, '\0', prebuilt->null_bitmap_len);

	for (i = 0; i < prebuilt->n_template; i++) {

		templ = prebuilt->mysql_template + i;

		data = rec_get_nth_field(rec, templ->rec_field_no, &len);

		if (rec_get_nth_field_extern_bit(rec, templ->rec_field_no)) {
			/* Copy an externally stored field to the temporary
			heap */

			if (prebuilt->trx->has_search_latch) {
				rw_lock_s_unlock(&btr_search_latch);
				prebuilt->trx->has_search_latch = FALSE;
			}

			extern_field_heap = mem_heap_create(UNIV_PAGE_SIZE);

			data = btr_rec_copy_externally_stored_field(rec,
					templ->rec_field_no, &len,
					extern_field_heap);

			ut_a(len != UNIV_SQL_NULL);
		}

		if (len != UNIV_SQL_NULL) {
			if (templ->type == DATA_BLOB) {

				/* Copy the BLOB data to the BLOB
				heap of prebuilt */

				if (prebuilt->blob_heap == NULL) {
					prebuilt->blob_heap =
						mem_heap_create(len);
				}

				blob_buf = mem_heap_alloc(prebuilt->blob_heap,
									len);
				ut_memcpy(blob_buf, data, len);

				data = blob_buf;
			}
		
			row_sel_field_store_in_mysql_format(
				mysql_rec + templ->mysql_col_offset,
				templ->mysql_col_len, data, len,
				templ->type, templ->is_unsigned);

			if (extern_field_heap) {
 				mem_heap_free(extern_field_heap);
				extern_field_heap = NULL;
 			}
		} else {
			mysql_rec[templ->mysql_null_byte_offset] |=
					(byte) (templ->mysql_null_bit_mask);
		}
	} 
}

/*************************************************************************
Builds a previous version of a clustered index record for a consistent read */
static
ulint
row_sel_build_prev_vers_for_mysql(
/*==============================*/
					/* out: DB_SUCCESS or error code */
	read_view_t*	read_view,	/* in: read view */
	dict_index_t*	clust_index,	/* in: clustered index */
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct */
	rec_t*		rec,		/* in: record in a clustered index */
	rec_t**		old_vers,	/* out: old version, or NULL if the
					record does not exist in the view:
					i.e., it was freshly inserted
					afterwards */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	err;

	if (prebuilt->old_vers_heap) {
		mem_heap_empty(prebuilt->old_vers_heap);
	} else {
		prebuilt->old_vers_heap = mem_heap_create(200);
	}
	
	err = row_vers_build_for_consistent_read(rec, mtr, clust_index,
					read_view, prebuilt->old_vers_heap,
					old_vers);
	return(err);
}

/*************************************************************************
Retrieves the clustered index record corresponding to a record in a
non-clustered index. Does the necessary locking. Used in the MySQL
interface. */
static
ulint
row_sel_get_clust_rec_for_mysql(
/*============================*/
				/* out: DB_SUCCESS or error code */
	row_prebuilt_t*	prebuilt,/* in: prebuilt struct in the handle */
	dict_index_t*	sec_index,/* in: secondary index where rec resides */
	rec_t*		rec,	/* in: record in a non-clustered index */
	que_thr_t*	thr,	/* in: query thread */
	rec_t**		out_rec,/* out: clustered record or an old version of
				it, NULL if the old version did not exist
				in the read view, i.e., it was a fresh
				inserted version */
	mtr_t*		mtr)	/* in: mtr used to get access to the
				non-clustered record; the same mtr is used to
				access the clustered index */
{
	dict_index_t*	clust_index;
	rec_t*		clust_rec;
	rec_t*		old_vers;
	ulint		err;
	trx_t*		trx;

	*out_rec = NULL;
	
	row_build_row_ref_in_tuple(prebuilt->clust_ref, sec_index, rec);

	clust_index = dict_table_get_first_index(sec_index->table);
	
	btr_pcur_open_with_no_init(clust_index, prebuilt->clust_ref,
			PAGE_CUR_LE, BTR_SEARCH_LEAF,
			prebuilt->clust_pcur, 0, mtr);

	clust_rec = btr_pcur_get_rec(prebuilt->clust_pcur);

	ut_ad(page_rec_is_user_rec(clust_rec));

	if (prebuilt->select_lock_type != LOCK_NONE) {
		/* Try to place a lock on the index record */
		
		err = lock_clust_rec_read_check_and_lock(0, clust_rec,
					clust_index,
					prebuilt->select_lock_type, thr);
		if (err != DB_SUCCESS) {

			return(err);
		}
	} else {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		trx = thr_get_trx(thr);

		old_vers = NULL;
		
		if (!lock_clust_rec_cons_read_sees(clust_rec, clust_index,
							trx->read_view)) {

			err = row_sel_build_prev_vers_for_mysql(
					trx->read_view, clust_index,
					prebuilt, clust_rec,
					&old_vers, mtr);
						
			if (err != DB_SUCCESS) {

				return(err);
			}

			clust_rec = old_vers;
		}

		/* If we had to go to an earlier version of row or the
		secondary index record is delete marked, then it may be that
		the secondary index record corresponding to clust_rec
		(or old_vers) is not rec; in that case we must ignore
		such row because in our snapshot rec would not have existed.
		Remember that from rec we cannot see directly which transaction
		id corrsponds to it: we have to go to the clustered index
		record. A query where we want to fetch all rows where
		the secondary index value is in some interval would return
		a wrong result if we would not drop rows which we come to
		visit through secondary index records that would not really
		exist in our snapshot. */
		
		if (clust_rec && (old_vers || rec_get_deleted_flag(rec))
		    && !row_sel_sec_rec_is_for_clust_rec(rec, sec_index,
						clust_rec, clust_index)) {
			clust_rec = NULL;
		}
	}

	*out_rec = clust_rec;

	if (prebuilt->select_lock_type == LOCK_X) {
		/* We may use the cursor in update: store its position */
		
		btr_pcur_store_position(prebuilt->clust_pcur, mtr);
	}

	return(DB_SUCCESS);
}

/************************************************************************
Restores cursor position after it has been stored. We have to take into
account that the record cursor was positioned on can have been deleted.
Then we may have to move the cursor one step up or down. */
static
ibool
sel_restore_position_for_mysql(
/*===========================*/
					/* out: TRUE if we may need to
					process the record the cursor is
					now positioned on (i.e. we should
					not go to the next record yet) */
	ulint		latch_mode,	/* in: latch mode wished in
					restoration */
	btr_pcur_t*	pcur,		/* in: cursor whose position
					has been stored */
	ibool		moves_up,	/* in: TRUE if the cursor moves up
					in the index */
	mtr_t*		mtr)		/* in: mtr; CAUTION: may commit
					mtr temporarily! */
{
	ibool	success;
	ulint	relative_position;

	relative_position = pcur->rel_pos;
	
	success = btr_pcur_restore_position(latch_mode, pcur, mtr);

	if (relative_position == BTR_PCUR_ON) {
		if (success) {
			return(FALSE);
		}

		if (moves_up) {
			btr_pcur_move_to_next(pcur, mtr);

			return(TRUE);
		}

		return(TRUE);
	}

	if (relative_position == BTR_PCUR_AFTER) {
		if (moves_up) {
			return(TRUE);
		}
					
		if (btr_pcur_is_on_user_rec(pcur, mtr)) {
			btr_pcur_move_to_prev(pcur, mtr);
		}

		return(TRUE);
	}

	ut_ad(relative_position == BTR_PCUR_BEFORE);
	
	if (moves_up && btr_pcur_is_on_user_rec(pcur, mtr)) {
		btr_pcur_move_to_next(pcur, mtr);
	}

	return(TRUE);
}

/************************************************************************
Pops a cached row for MySQL from the fetch cache. */
UNIV_INLINE
void
row_sel_pop_cached_row_for_mysql(
/*=============================*/
	byte*		buf,		/* in/out: buffer where to copy the
					row */
	row_prebuilt_t*	prebuilt)	/* in: prebuilt struct */
{
	ut_ad(prebuilt->n_fetch_cached > 0);

	ut_memcpy(buf, prebuilt->fetch_cache[prebuilt->fetch_cache_first],
						prebuilt->mysql_row_len);
	prebuilt->n_fetch_cached--;
	prebuilt->fetch_cache_first++;

	if (prebuilt->n_fetch_cached == 0) {
		prebuilt->fetch_cache_first = 0;
	}
}

/************************************************************************
Pushes a row for MySQL to the fetch cache. */
UNIV_INLINE
void
row_sel_push_cache_row_for_mysql(
/*=============================*/
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct */
	rec_t*		rec)		/* in: record to push */
{
	ulint	i;

	ut_ad(prebuilt->n_fetch_cached < MYSQL_FETCH_CACHE_SIZE);

	if (prebuilt->fetch_cache[0] == NULL) {
		/* Allocate memory for the fetch cache */

		for (i = 0; i < MYSQL_FETCH_CACHE_SIZE; i++) {
			prebuilt->fetch_cache[i] = mem_alloc(
						prebuilt->mysql_row_len);
		}
	}

	ut_ad(prebuilt->fetch_cache_first == 0);

	row_sel_store_mysql_rec(
			prebuilt->fetch_cache[prebuilt->n_fetch_cached],
			prebuilt, rec);

	prebuilt->n_fetch_cached++;
}
	
/*************************************************************************
Tries to do a shortcut to fetch a clustered index record with a unique key,
using the hash index if possible (not always). We assume that the search
mode is PAGE_CUR_GE, it is a consistent read, trx has already a read view,
btr search latch has been locked in S-mode. */
static
ulint
row_sel_try_search_shortcut_for_mysql(
/*==================================*/
				/* out: SEL_FOUND, SEL_EXHAUSTED, SEL_RETRY */
	rec_t**		out_rec,/* out: record if found */
	row_prebuilt_t*	prebuilt,/* in: prebuilt struct */
	mtr_t*		mtr)	/* in: started mtr */
{
	dict_index_t*	index		= prebuilt->index;
	dtuple_t*	search_tuple	= prebuilt->search_tuple;
	btr_pcur_t*	pcur		= prebuilt->pcur;
	trx_t*		trx		= prebuilt->trx;
	rec_t*		rec;
	
	ut_ad(index->type & DICT_CLUSTERED);

	btr_pcur_open_with_no_init(index, search_tuple, PAGE_CUR_GE,
					BTR_SEARCH_LEAF, pcur,
					RW_S_LATCH, mtr);
	rec = btr_pcur_get_rec(pcur);
	
	if (!page_rec_is_user_rec(rec)) {

		return(SEL_RETRY);
	}

	/* As the cursor is now placed on a user record after a search with
	the mode PAGE_CUR_GE, the up_match field in the cursor tells how many
	fields in the user record matched to the search tuple */ 

	if (btr_pcur_get_up_match(pcur) < dtuple_get_n_fields(search_tuple)) {

		return(SEL_EXHAUSTED);
	}

	/* This is a non-locking consistent read: if necessary, fetch
	a previous version of the record */
			
	if (!lock_clust_rec_cons_read_sees(rec, index, trx->read_view)) {

		return(SEL_RETRY);
	}

	if (rec_get_deleted_flag(rec)) {

		return(SEL_EXHAUSTED);
	}

	*out_rec = rec;
	
	return(SEL_FOUND);
}

/************************************************************************
Searches for rows in the database. This is used in the interface to
MySQL. This function opens a cursor, and also implements fetch next
and fetch prev. NOTE that if we do a search with a full key value
from a unique index (ROW_SEL_EXACT), then we will not store the cursor
position and fetch next or fetch prev must not be tried to the cursor! */

ulint
row_search_for_mysql(
/*=================*/
					/* out: DB_SUCCESS,
					DB_RECORD_NOT_FOUND, 
					DB_END_OF_INDEX, or DB_DEADLOCK */
	byte*		buf,		/* in/out: buffer for the fetched
					row in the MySQL format */
	ulint		mode,		/* in: search mode PAGE_CUR_L, ... */
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct for the
					table handle; this contains the info
					of search_tuple, index; if search
					tuple contains 0 fields then we
					position the cursor at the start or
					the end of the index, depending on
					'mode' */
	ulint		match_mode,	/* in: 0 or ROW_SEL_EXACT or
					ROW_SEL_EXACT_PREFIX */ 
	ulint		direction)	/* in: 0 or ROW_SEL_NEXT or
					ROW_SEL_PREV; NOTE: if this is != 0,
					then prebuilt must have a pcur
					with stored position! In opening of a
					cursor 'direction' should be 0. */
{
	dict_index_t*	index		= prebuilt->index;
	dtuple_t*	search_tuple	= prebuilt->search_tuple;
	btr_pcur_t*	pcur		= prebuilt->pcur;
	trx_t*		trx		= prebuilt->trx;
	dict_index_t*	clust_index;
	que_thr_t*	thr;
	rec_t*		rec;
	rec_t*		index_rec;
	rec_t*		clust_rec;
	rec_t*		old_vers;
	ulint		err;
	ibool		moved;
	ibool		cons_read_requires_clust_rec;
	ibool		was_lock_wait;
	ulint		ret;
	ulint		shortcut;
	ibool		unique_search_from_clust_index	= FALSE;
	ibool		mtr_has_extra_clust_latch 	= FALSE;
	ibool		moves_up 			= FALSE;
	ulint		cnt				= 0;
	mtr_t		mtr;
	
	ut_ad(index && pcur && search_tuple);
	ut_ad(trx->mysql_thread_id == os_thread_get_curr_id());
	
	ut_ad(sync_thread_levels_empty_gen(FALSE));
	
/*	printf("Match mode %lu\n search tuple ", match_mode);
	dtuple_print(search_tuple);
	
	printf("N tables locked %lu\n", trx->mysql_n_tables_locked);
*/
	if (direction == 0) {
		trx->op_info = "starting index read";
	
		prebuilt->n_rows_fetched = 0;
		prebuilt->n_fetch_cached = 0;
		prebuilt->fetch_cache_first = 0;

		if (prebuilt->sel_graph == NULL) {
			/* Build a dummy select query graph */
			row_prebuild_sel_graph(prebuilt);
		}
	} else {
		trx->op_info = "fetching rows";

		if (prebuilt->n_rows_fetched == 0) {
			prebuilt->fetch_direction = direction;
		}

		if (direction != prebuilt->fetch_direction) {
			if (prebuilt->n_fetch_cached > 0) {
				ut_a(0);
				/* TODO: scrollable cursor: restore cursor to
				the place of the latest returned row,
				or better: prevent caching for a scroll
				cursor! */
			}
		
			prebuilt->n_rows_fetched = 0;
			prebuilt->n_fetch_cached = 0;
			prebuilt->fetch_cache_first = 0;

		} else if (prebuilt->n_fetch_cached > 0) {
			row_sel_pop_cached_row_for_mysql(buf, prebuilt);

			prebuilt->n_rows_fetched++;

			srv_n_rows_read++;
			trx->op_info = "";

			return(DB_SUCCESS);
		}

		if (prebuilt->fetch_cache_first > 0
		    && prebuilt->fetch_cache_first < MYSQL_FETCH_CACHE_SIZE) {

		    	/* The previous returned row was popped from the fetch
		    	cache, but the cache was not full at the time of the
		    	popping: no more rows can exist in the result set */
		    
			trx->op_info = "";
		    	return(DB_RECORD_NOT_FOUND);
		}
		
		prebuilt->n_rows_fetched++;

		if (prebuilt->n_rows_fetched > 1000000000) {
			/* Prevent wrap-over */
			prebuilt->n_rows_fetched = 500000000;
		}

		mode = pcur->search_mode;
	}

	mtr_start(&mtr);

	if (match_mode == ROW_SEL_EXACT && index->type & DICT_UNIQUE
		&& index->type & DICT_CLUSTERED
		&& dtuple_get_n_fields(search_tuple)
				== dict_index_get_n_unique(index)) {

		if (direction == ROW_SEL_NEXT) {
			/* MySQL sometimes seems to do fetch next even
			if the search condition is unique; we do not store
			pcur position in this case, so we cannot
			restore cursor position, and must return
 			immediately */

 			mtr_commit(&mtr);

			/* printf("%s record not found 1\n", index->name); */
	
			trx->op_info = "";
			return(DB_RECORD_NOT_FOUND);
		}

		ut_a(direction == 0);	/* We cannot do fetch prev, as we have
					not stored the cursor position */
		mode = PAGE_CUR_GE;

		unique_search_from_clust_index = TRUE;

		if (trx->mysql_n_tables_locked == 0
					&& !prebuilt->sql_stat_start) {

			/* This is a SELECT query done as a consistent read,
			and the read view has already been allocated:
			let us try a search shortcut through the hash
			index */
			
			if (!trx->has_search_latch) {
				rw_lock_s_lock(&btr_search_latch);
				trx->has_search_latch = TRUE;

			} else if (btr_search_latch.writer_is_wait_ex) {
			        /* There is an x-latch request waiting:
				release the s-latch for a moment to reduce
				starvation */

			        rw_lock_s_unlock(&btr_search_latch);
			        rw_lock_s_lock(&btr_search_latch);
			}

			shortcut = row_sel_try_search_shortcut_for_mysql(&rec,
							prebuilt, &mtr);
			if (shortcut == SEL_FOUND) {
				row_sel_store_mysql_rec(buf, prebuilt, rec);
	
 				mtr_commit(&mtr);

 				/* printf("%s shortcut\n", index->name); */

				srv_n_rows_read++;

				trx->op_info = "";
				return(DB_SUCCESS);
			
			} else if (shortcut == SEL_EXHAUSTED) {

 				mtr_commit(&mtr);

				/* printf("%s record not found 2\n",
							index->name); */
				trx->op_info = "";
				return(DB_RECORD_NOT_FOUND);
			}

			mtr_commit(&mtr);
			mtr_start(&mtr);
		}
	}
	
	if (trx->has_search_latch) {
		rw_lock_s_unlock(&btr_search_latch);
		trx->has_search_latch = FALSE;
	}			

	/* Note that if the search mode was GE or G, then the cursor
	naturally moves upward (in fetch next) in alphabetical order,
	otherwise downward */
	
	if (direction == 0) {
		if (mode == PAGE_CUR_GE || mode == PAGE_CUR_G) {
			moves_up = TRUE;
		}
	} else if (direction == ROW_SEL_NEXT) {
		moves_up = TRUE;
	}

	thr = que_fork_get_first_thr(prebuilt->sel_graph);

	que_thr_move_to_run_state_for_mysql(thr, trx);

	clust_index = dict_table_get_first_index(index->table);

	if (direction != 0) {		
		moved = sel_restore_position_for_mysql(BTR_SEARCH_LEAF, pcur,
							moves_up, &mtr);
		if (!moved) {
			goto next_rec;
		}

	} else if (dtuple_get_n_fields(search_tuple) > 0) {

		btr_pcur_open_with_no_init(index, search_tuple, mode,
					BTR_SEARCH_LEAF,
					pcur, 0, &mtr);
	} else {
		if (mode == PAGE_CUR_G) {
			btr_pcur_open_at_index_side(TRUE, index,
					BTR_SEARCH_LEAF, pcur, FALSE, &mtr);
		} else if (mode == PAGE_CUR_L) {
			btr_pcur_open_at_index_side(FALSE, index,
					BTR_SEARCH_LEAF, pcur, FALSE, &mtr);
		}
	}

	if (!prebuilt->sql_stat_start) {
		/* No need to set an intention lock or assign a read view */

	} else if (prebuilt->select_lock_type == LOCK_NONE) {
		/* This is a consistent read */
		trx_start_if_not_started(trx);
	
		/* Assign a read view for the query */

		trx_assign_read_view(trx);
		prebuilt->sql_stat_start = FALSE;
	} else {			
		trx_start_if_not_started(trx);

		if (prebuilt->select_lock_type == LOCK_S) {		
			err = lock_table(0, index->table, LOCK_IS, thr);
		} else {
			err = lock_table(0, index->table, LOCK_IX, thr);
		}

		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}
		prebuilt->sql_stat_start = FALSE;
	}

	/*-------------------------------------------------------------*/
rec_loop:
	cons_read_requires_clust_rec = FALSE;

	rec = btr_pcur_get_rec(pcur);
/*
	printf("Using index %s cnt %lu ", index->name, cnt);
	printf("; Page no %lu\n",
			buf_frame_get_page_no(buf_frame_align(rec)));
	rec_print(rec);
*/
	if (rec == page_get_infimum_rec(buf_frame_align(rec))) {

		/* The infimum record on a page cannot be in the result set,
		and neither can a record lock be placed on it: we skip such
		a record. */

		goto next_rec;
	}
				
	if (prebuilt->select_lock_type != LOCK_NONE) {
		/* Try to place a lock on the index record */	

		err = sel_set_rec_lock(rec, index, prebuilt->select_lock_type,
									thr);
		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}
	}

	if (rec == page_get_supremum_rec(buf_frame_align(rec))) {

		/* A page supremum record cannot be in the result set: skip
		it now when we have placed a possible lock on it */		
		
		goto next_rec;
	}

	ut_ad(page_rec_is_user_rec(rec));

	if (unique_search_from_clust_index && btr_pcur_get_up_match(pcur)
					== dtuple_get_n_fields(search_tuple)) {
		/* The record matches enough */

		ut_ad(mode == PAGE_CUR_GE);
	
	} else if (match_mode == ROW_SEL_EXACT) {
		/* Test if the index record matches completely to search_tuple
		in prebuilt: if not, then we return with DB_RECORD_NOT_FOUND */

		/* printf("Comparing rec and search tuple\n"); */
		
		if (0 != cmp_dtuple_rec(search_tuple, rec)) {

			btr_pcur_store_position(pcur, &mtr);

			ret = DB_RECORD_NOT_FOUND;
 			/* printf("%s record not found 3\n", index->name); */
			
			goto normal_return;
		}

	} else if (match_mode == ROW_SEL_EXACT_PREFIX) {

		if (!cmp_dtuple_is_prefix_of_rec(search_tuple, rec)) {
			
			btr_pcur_store_position(pcur, &mtr);

			ret = DB_RECORD_NOT_FOUND;
 			/* printf("%s record not found 4\n", index->name); */

			goto normal_return;
		}
	}
		
	/* We are ready to look at a possible new index entry in the result
	set: the cursor is now placed on a user record */

	/* Get the right version of the row in a consistent read */

	if (prebuilt->select_lock_type == LOCK_NONE) {

		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		cons_read_requires_clust_rec = FALSE;

		if (index == clust_index) {
			
			if (!lock_clust_rec_cons_read_sees(rec, index,
							trx->read_view)) {

				err = row_sel_build_prev_vers_for_mysql(
						trx->read_view, clust_index,
						prebuilt, rec,
						&old_vers, &mtr);
						
				if (err != DB_SUCCESS) {

					goto lock_wait_or_error;
				}

				if (old_vers == NULL) {
					/* The row did not exist yet in
					the read view */

					goto next_rec;
				}

				rec = old_vers;
			}
		} else if (!lock_sec_rec_cons_read_sees(rec, index,
							trx->read_view)) {
			/* We are looking into a non-clustered index,
			and to get the right version of the record we
			have to look also into the clustered index: this
			is necessary, because we can only get the undo
			information via the clustered index record. */
			
			cons_read_requires_clust_rec = TRUE;
		}
	}

	if (rec_get_deleted_flag(rec) && !cons_read_requires_clust_rec) {

		/* The record is delete marked: we can skip it if this is
		not a consistent read which might see an earlier version
		of a non-clustered index record */
		
		goto next_rec;
	}

	/* Get the clustered index record if needed and if we did
	not do the search using the clustered index */

	index_rec = rec;

	if (index != clust_index && (cons_read_requires_clust_rec
				|| prebuilt->need_to_access_clustered)) {

		/* It was a non-clustered index and we must fetch also the
		clustered index record */

		mtr_has_extra_clust_latch = TRUE;
		
		err = row_sel_get_clust_rec_for_mysql(prebuilt, index, rec,
							thr, &clust_rec, &mtr);
		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}

		if (clust_rec == NULL) {
			/* The record did not exist in the read view */
			ut_ad(prebuilt->select_lock_type == LOCK_NONE);

			goto next_rec;
		}

		if (rec_get_deleted_flag(clust_rec)) {

			/* The record is delete marked: we can skip it */

			goto next_rec;
		}
		
		if (prebuilt->need_to_access_clustered) {
		        rec = clust_rec;
		}
	}

	/* We found a qualifying row */
	
	if (prebuilt->n_rows_fetched >= MYSQL_FETCH_CACHE_THRESHOLD
			&& !prebuilt->templ_contains_blob
			&& prebuilt->select_lock_type == LOCK_NONE
			&& !prebuilt->clust_index_was_generated
	                && prebuilt->template_type
	                                 != ROW_MYSQL_DUMMY_TEMPLATE) {

		/* Inside an update, for example, we do not cache rows,
		since we may use the cursor position to do the actual
		update, that is why we require ...lock_type == LOCK_NONE */

		row_sel_push_cache_row_for_mysql(prebuilt, rec);

		if (prebuilt->n_fetch_cached == MYSQL_FETCH_CACHE_SIZE) {
			
			goto got_row;
		}

		goto next_rec;
	} else {
		if (prebuilt->template_type == ROW_MYSQL_DUMMY_TEMPLATE) {
			ut_memcpy(buf + 4, rec - rec_get_extra_size(rec),
						rec_get_size(rec));
			mach_write_to_4(buf, rec_get_extra_size(rec) + 4);
		} else {
			row_sel_store_mysql_rec(buf, prebuilt, rec);
		}

		if (prebuilt->clust_index_was_generated) {
			row_sel_store_row_id_to_prebuilt(prebuilt, index_rec,
									index);
		}
	}
got_row:
	/* TODO: should we in every case store the cursor position, even
	if this is just a join, for example? */

	if (!unique_search_from_clust_index
				|| prebuilt->select_lock_type == LOCK_X) {

		/* Inside an update always store the cursor position */

		btr_pcur_store_position(pcur, &mtr);
	}

	ret = DB_SUCCESS;

	goto normal_return;
	/*-------------------------------------------------------------*/	
next_rec:
	if (mtr_has_extra_clust_latch) {
		/* We must commit mtr if we are moving to the next
		non-clustered index record, because we could break the
		latching order if we would access a different clustered
		index page right away without releasing the previous. */

		btr_pcur_store_position(pcur, &mtr);

		mtr_commit(&mtr);
		mtr_has_extra_clust_latch = FALSE;
	
		mtr_start(&mtr);
		moved = sel_restore_position_for_mysql(BTR_SEARCH_LEAF, pcur,
							moves_up, &mtr);
		if (moved) {
			cnt++;

			goto rec_loop;
		}
	}

	if (moves_up) {		
		moved = btr_pcur_move_to_next(pcur, &mtr);
	} else {
		moved = btr_pcur_move_to_prev(pcur, &mtr);
	}

	if (!moved) {
		btr_pcur_store_position(pcur, &mtr);

		if (match_mode != 0) {
			ret = DB_RECORD_NOT_FOUND;
		} else {
			ret = DB_END_OF_INDEX;
		}

		goto normal_return;
	}

	cnt++;

	goto rec_loop;
	/*-------------------------------------------------------------*/
lock_wait_or_error:
	btr_pcur_store_position(pcur, &mtr);

	mtr_commit(&mtr);
	mtr_has_extra_clust_latch = FALSE;
		
	trx->error_state = err;

	/* The following is a patch for MySQL */

	que_thr_stop_for_mysql(thr);

	was_lock_wait = row_mysql_handle_errors(&err, trx, thr, NULL);
	
	if (was_lock_wait) {
		mtr_start(&mtr);

		sel_restore_position_for_mysql(BTR_SEARCH_LEAF, pcur,
							moves_up, &mtr);
		mode = pcur->search_mode;

		goto rec_loop;
	}

	/* printf("Using index %s cnt %lu ret value %lu err\n", index->name,
							cnt, err); */
	trx->op_info = "";

	return(err);

normal_return:
	que_thr_stop_for_mysql_no_error(thr, trx);

	mtr_commit(&mtr);

	if (prebuilt->n_fetch_cached > 0) {
		row_sel_pop_cached_row_for_mysql(buf, prebuilt);

		ret = DB_SUCCESS;
	}

	/* printf("Using index %s cnt %lu ret value %lu\n", index->name,
							cnt, err); */
	if (ret == DB_SUCCESS) {
		srv_n_rows_read++;
	}

	trx->op_info = "";

	return(ret);
}
