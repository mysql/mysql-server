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
#include "read0read.h"
#include "buf0lru.h"

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
are alphabetically the same as the corresponding columns in the clustered
index record.
NOTE: the comparison is NOT done as a binary comparison, but character
fields are compared with collation! */
static
ibool
row_sel_sec_rec_is_for_clust_rec(
/*=============================*/
					/* out: TRUE if the secondary
					record is equal to the corresponding
					fields in the clustered record,
					when compared with collation */
	rec_t*		sec_rec,	/* in: secondary index record */
	dict_index_t*	sec_index,	/* in: secondary index */
	rec_t*		clust_rec,	/* in: clustered index record */
	dict_index_t*	clust_index)	/* in: clustered index */
{
	dict_field_t*	ifield;
        dict_col_t*     col;
        byte*           sec_field;
        ulint           sec_len;
        byte*           clust_field;
        ulint           clust_len;
        ulint           n;
        ulint           i;
	dtype_t*	cur_type;
	mem_heap_t*	heap		= NULL;
	ulint		clust_offsets_[REC_OFFS_NORMAL_SIZE];
	ulint		sec_offsets_[REC_OFFS_SMALL_SIZE];
	ulint*		clust_offs	= clust_offsets_;
	ulint*		sec_offs	= sec_offsets_;
	ibool		is_equal	= TRUE;

	*clust_offsets_ = (sizeof clust_offsets_) / sizeof *clust_offsets_;
	*sec_offsets_ = (sizeof sec_offsets_) / sizeof *sec_offsets_;

	clust_offs = rec_get_offsets(clust_rec, clust_index, clust_offs,
						ULINT_UNDEFINED, &heap);
	sec_offs = rec_get_offsets(sec_rec, sec_index, sec_offs,
						ULINT_UNDEFINED, &heap);

        n = dict_index_get_n_ordering_defined_by_user(sec_index);

        for (i = 0; i < n; i++) {
		ifield = dict_index_get_nth_field(sec_index, i);
                col = dict_field_get_col(ifield);
                
		clust_field = rec_get_nth_field(clust_rec, clust_offs,
                                                dict_col_get_clust_pos(col),
                                                &clust_len);
		sec_field = rec_get_nth_field(sec_rec, sec_offs, i, &sec_len);

		if (ifield->prefix_len > 0
		    && clust_len != UNIV_SQL_NULL) {

			cur_type = dict_col_get_type(
				dict_field_get_col(ifield));

			clust_len = dtype_get_at_most_n_mbchars(
				cur_type,
				ifield->prefix_len,
				clust_len, (char*) clust_field);
		}

                if (0 != cmp_data_data(dict_col_get_type(col),
                                        clust_field, clust_len,
                                        sec_field, sec_len)) {
			is_equal = FALSE;
			goto func_exit;
                }
        }

func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(is_equal);
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
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	sym_node_t*	column)	/* in: first column in a column list, or
				NULL */
{
	dfield_t*	val;
	ulint		index_type;
	ulint		field_no;
	byte*		data;
	ulint		len;
	
	ut_ad(rec_offs_validate(rec, index, offsets));

	if (index->type & DICT_CLUSTERED) {
		index_type = SYM_CLUST_FIELD_NO;
	} else {
		index_type = SYM_SEC_FIELD_NO;
	}

	while (column) {
		field_no = column->field_nos[index_type];

		if (field_no != ULINT_UNDEFINED) {
	
			data = rec_get_nth_field(rec, offsets, field_no, &len);
			
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
	ulint**		offsets,	/* in/out: offsets returned by
					rec_get_offsets(rec, plan->index) */
	mem_heap_t**	offset_heap,	/* in/out: memory heap from which
					the offsets are allocated */
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
					offsets, read_view, offset_heap,
					plan->old_vers_heap, old_vers);
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
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	*out_rec = NULL;

	offsets = rec_get_offsets(rec,
				btr_pcur_get_btr_cur(&plan->pcur)->index,
				offsets, ULINT_UNDEFINED, &heap);
	
	row_build_row_ref_fast(plan->clust_ref, plan->clust_map, rec, offsets);

	index = dict_table_get_first_index(plan->table);
	
	btr_pcur_open_with_no_init(index, plan->clust_ref, PAGE_CUR_LE,
				node->latch_mode, &(plan->clust_pcur),
				0, mtr);

	clust_rec = btr_pcur_get_rec(&(plan->clust_pcur));

	/* Note: only if the search ends up on a non-infimum record is the
	low_match value the real match to the search tuple */

	if (!page_rec_is_user_rec(clust_rec)
            || btr_pcur_get_low_match(&(plan->clust_pcur))
	       < dict_index_get_n_unique(index)) {
	
		ut_a(rec_get_deleted_flag(rec, plan->table->comp));
		ut_a(node->read_view);

		/* In a rare case it is possible that no clust rec is found
		for a delete-marked secondary index record: if in row0umod.c
		in row_undo_mod_remove_clust_low() we have already removed
		the clust rec, while purge is still cleaning and removing
		secondary index records associated with earlier versions of
		the clustered index record. In that case we know that the
		clustered index record did not exist in the read view of
		trx. */

		goto func_exit;
	}

	offsets = rec_get_offsets(clust_rec, index, offsets,
						ULINT_UNDEFINED, &heap);

	if (!node->read_view) {
		/* Try to place a lock on the index record */
        
		/* If innodb_locks_unsafe_for_binlog option is used, 
		we lock only the record, i.e., next-key locking is
		not used. */
		ulint	lock_type;

		if (srv_locks_unsafe_for_binlog) {
			lock_type = LOCK_REC_NOT_GAP;
		} else {
			lock_type = LOCK_ORDINARY;
		}

		err = lock_clust_rec_read_check_and_lock(0,
				clust_rec, index, offsets,
				node->row_lock_mode, lock_type, thr);

		if (err != DB_SUCCESS) {

			goto err_exit;
		}
	} else {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		old_vers = NULL;

		if (!lock_clust_rec_cons_read_sees(clust_rec, index, offsets,
							node->read_view)) {

			err = row_sel_build_prev_vers(node->read_view, plan,
						clust_rec, &offsets, &heap,
						&old_vers, mtr);
			if (err != DB_SUCCESS) {

				goto err_exit;
			}

			clust_rec = old_vers;

			if (clust_rec == NULL) {
				goto func_exit;
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
		
		if ((old_vers || rec_get_deleted_flag(rec, plan->table->comp))
		    && !row_sel_sec_rec_is_for_clust_rec(rec, plan->index,
							clust_rec, index)) {
			goto func_exit;
		}
	}

	/* Fetch the columns needed in test conditions */

	row_sel_fetch_columns(index, clust_rec, offsets,
					UT_LIST_GET_FIRST(plan->columns));
	*out_rec = clust_rec;
func_exit:
	err = DB_SUCCESS;
err_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
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
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	ulint		mode,	/* in: lock mode */
	ulint		type, 	/* in: LOCK_ORDINARY, LOCK_GAP, or LOC_REC_NOT_GAP */
	que_thr_t*	thr)	/* in: query thread */	
{
	trx_t*	trx;
	ulint	err;

	trx = thr_get_trx(thr);	

	if (UT_LIST_GET_LEN(trx->trx_locks) > 10000) {
		if (buf_LRU_buf_pool_running_out()) {
			
			return(DB_LOCK_TABLE_FULL);
		}
	}

	if (index->type & DICT_CLUSTERED) {
		err = lock_clust_rec_read_check_and_lock(0,
					rec, index, offsets, mode, type, thr);
	} else {
		err = lock_sec_rec_read_check_and_lock(0,
					rec, index, offsets, mode, type, thr);
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
static
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

		ut_ad(relative_position == BTR_PCUR_AFTER
		      || relative_position == BTR_PCUR_AFTER_LAST_IN_TREE);

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

	if (relative_position == BTR_PCUR_BEFORE
	    || relative_position == BTR_PCUR_BEFORE_FIRST_IN_TREE) {

		return(FALSE);
	}

	if (relative_position == BTR_PCUR_ON) {

		if (equal_position) {

			return(plan->stored_cursor_rec_processed);
		}

		return(FALSE);
	}

	ut_ad(relative_position == BTR_PCUR_AFTER
		      || relative_position == BTR_PCUR_AFTER_LAST_IN_TREE);

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
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	ulint		ret;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	index = plan->index;

	ut_ad(node->read_view);
	ut_ad(plan->unique_search);
	ut_ad(!plan->must_get_clust);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
#endif /* UNIV_SYNC_DEBUG */
	
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
			
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

	if (index->type & DICT_CLUSTERED) {
		if (!lock_clust_rec_cons_read_sees(rec, index, offsets,
							node->read_view)) {
			ret = SEL_RETRY;
			goto func_exit;
		}
	} else if (!lock_sec_rec_cons_read_sees(rec, index, node->read_view)) {

		ret = SEL_RETRY;
		goto func_exit;
	}

	/* Test deleted flag. Fetch the columns needed in test conditions. */

	row_sel_fetch_columns(index, rec, offsets,
				UT_LIST_GET_FIRST(plan->columns));

	if (rec_get_deleted_flag(rec, plan->table->comp)) {

		ret = SEL_EXHAUSTED;
		goto func_exit;
	}

	/* Test the rest of search conditions */
	
	if (!row_sel_test_other_conds(plan)) {

		ret = SEL_EXHAUSTED;
		goto func_exit;
	}

	ut_ad(plan->pcur.latch_mode == node->latch_mode);

	plan->n_rows_fetched++;
	ret = SEL_FOUND;
func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(ret);
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
	mem_heap_t*	heap				= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets				= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

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
		&& !page_rec_is_supremum(rec)) {

		/* When we open a cursor for a descending search, we must set
		a next-key lock on the successor record: otherwise it would
		be possible to insert new records next to the cursor position,
		and it might be that these new records should appear in the
		search result set, resulting in the phantom problem. */
		
		if (!consistent_read) {

			/* If innodb_locks_unsafe_for_binlog option is used,
			we lock only the record, i.e., next-key locking is
			not used. */

			rec_t*	next_rec = page_rec_get_next(rec);
			ulint	lock_type;
			offsets = rec_get_offsets(next_rec, index, offsets,
						ULINT_UNDEFINED, &heap);

			if (srv_locks_unsafe_for_binlog) {
				lock_type = LOCK_REC_NOT_GAP;
			} else {
				lock_type = LOCK_ORDINARY;
			}

			err = sel_set_rec_lock(next_rec, index, offsets,
					node->row_lock_mode, lock_type, thr);

			if (err != DB_SUCCESS) {
				/* Note that in this case we will store in pcur
				the PREDECESSOR of the record we are waiting
				the lock for */
				
				goto lock_wait_or_error;
			}
		}
	}

	if (page_rec_is_infimum(rec)) {

		/* The infimum record on a page cannot be in the result set,
		and neither can a record lock be placed on it: we skip such
		a record. We also increment the cost counter as we may have
		processed yet another page of index. */

		cost_counter++;

		goto next_rec;
	}

	if (!consistent_read) {
		/* Try to place a lock on the index record */	

		/* If innodb_locks_unsafe_for_binlog option is used,
		we lock only the record, i.e., next-key locking is
		not used. */

		ulint	lock_type;
		offsets = rec_get_offsets(rec, index, offsets,
						ULINT_UNDEFINED, &heap);

		if (srv_locks_unsafe_for_binlog) {
			lock_type = LOCK_REC_NOT_GAP;
		} else {
			lock_type = LOCK_ORDINARY;
		}

		err = sel_set_rec_lock(rec, index, offsets,
					node->row_lock_mode, lock_type, thr);

		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}
	}

	if (page_rec_is_supremum(rec)) {

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

	cons_read_requires_clust_rec = FALSE;
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

	if (consistent_read) {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		if (index->type & DICT_CLUSTERED) {
			
			if (!lock_clust_rec_cons_read_sees(rec, index, offsets,
							node->read_view)) {

				err = row_sel_build_prev_vers(node->read_view,
							plan, rec,
							&offsets, &heap,
							&old_vers, &mtr);
				if (err != DB_SUCCESS) {

					goto lock_wait_or_error;
				}

				if (old_vers == NULL) {
					offsets = rec_get_offsets(
						rec, index, offsets,
						ULINT_UNDEFINED, &heap);
					row_sel_fetch_columns(index, rec,
					    offsets,
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
	
	row_sel_fetch_columns(index, rec, offsets,
					UT_LIST_GET_FIRST(plan->columns));

	/* Test the selection end conditions: these can only contain columns
	which already are found in the index, even though the index might be
	non-clustered */

	if (plan->unique_search && cursor_just_opened) {

		/* No test necessary: the test was already made above */

	} else if (!row_sel_test_end_conds(plan)) {

		goto table_exhausted;
	}

	if (rec_get_deleted_flag(rec, plan->table->comp)
			&& !cons_read_requires_clust_rec) {

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

		if (rec_get_deleted_flag(clust_rec, plan->table->comp)) {

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

		err = DB_SUCCESS;
		goto func_exit;
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
		err = DB_SUCCESS;

		if (node->is_aggregate && !node->aggregate_already_fetched) {

			node->aggregate_already_fetched = TRUE;

			sel_assign_into_var_values(node->into_list, node);

			thr->run_node = que_node_get_parent(node);

			if (search_latch_locked) {
				rw_lock_s_unlock(&btr_search_latch);
			}
		
			goto func_exit;
		}

		node->state = SEL_NODE_NO_MORE_ROWS;
		
		thr->run_node = que_node_get_parent(node);

		if (search_latch_locked) {
			rw_lock_s_unlock(&btr_search_latch);
		}
		
		goto func_exit;
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
	err = DB_SUCCESS;
	goto func_exit;

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

func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
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
		fprintf(stderr, "SQL error %lu\n", (ulong) err);

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
		fprintf(stderr, "SQL error %lu\n", (ulong)DB_ERROR);

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

		fputs(" ::: ", stderr);

		arg = que_node_get_next(arg);
	}

	putc('\n', stderr);

	/* Fetch next row to print */

	thr->run_node = sel_node;

	return(thr);
} 

/********************************************************************
Converts a key value stored in MySQL format to an Innobase dtuple. The last
field of the key value may be just a prefix of a fixed length field: hence
the parameter key_len. But currently we do not allow search keys where the
last field is only a prefix of the full key field len and print a warning if
such appears. A counterpart of this function is
ha_innobase::store_key_val_for_row() in ha_innodb.cc. */

void
row_sel_convert_mysql_key_to_innobase(
/*==================================*/
	dtuple_t*	tuple,		/* in: tuple where to build;
					NOTE: we assume that the type info
					in the tuple is already according
					to index! */
	byte*		buf,		/* in: buffer to use in field
					conversions */
	ulint		buf_len,	/* in: buffer length */
	dict_index_t*	index,		/* in: index of the key value */
	byte*		key_ptr,	/* in: MySQL key value */
	ulint		key_len,	/* in: MySQL key value length */
	trx_t*		trx)		/* in: transaction */
{
	byte*		original_buf	= buf;
	byte*		original_key_ptr = key_ptr;
	dict_field_t*	field;
	dfield_t*	dfield;
	ulint		data_offset;
	ulint		data_len;
	ulint		data_field_len;
	ibool		is_null;
	byte*		key_end;
	ulint		n_fields = 0;
	ulint		type;
	
	/* For documentation of the key value storage format in MySQL, see
	ha_innobase::store_key_val_for_row() in ha_innodb.cc. */

	key_end = key_ptr + key_len;

	/* Permit us to access any field in the tuple (ULINT_MAX): */
	
	dtuple_set_n_fields(tuple, ULINT_MAX);

	dfield = dtuple_get_nth_field(tuple, 0);
	field = dict_index_get_nth_field(index, 0);

	if (dfield_get_type(dfield)->mtype == DATA_SYS) {
		/* A special case: we are looking for a position in the
		generated clustered index which InnoDB automatically added
		to a table with no primary key: the first and the only
		ordering column is ROW_ID which InnoDB stored to the key_ptr
		buffer. */

		ut_a(key_len == DATA_ROW_ID_LEN);

		dfield_set_data(dfield, key_ptr, DATA_ROW_ID_LEN);
					
		dtuple_set_n_fields(tuple, 1);

		return;
	}

	while (key_ptr < key_end) {

		ut_a(dict_col_get_type(field->col)->mtype
		     == dfield_get_type(dfield)->mtype);

		data_offset = 0;
		is_null = FALSE;

    		if (!(dfield_get_type(dfield)->prtype & DATA_NOT_NULL)) {
    			/* The first byte in the field tells if this is
    			an SQL NULL value */
    			
			data_offset = 1;

 			if (*key_ptr != 0) {
      				dfield_set_data(dfield, NULL, UNIV_SQL_NULL);

				is_null = TRUE;
      			}
      		}

		type = dfield_get_type(dfield)->mtype;

		/* Calculate data length and data field total length */
		
		if (type == DATA_BLOB) {
			/* The key field is a column prefix of a BLOB or
			TEXT */

			ut_a(field->prefix_len > 0);

			/* MySQL stores the actual data length to the first 2
			bytes after the optional SQL NULL marker byte. The
			storage format is little-endian, that is, the most
			significant byte at a higher address. In UTF-8, MySQL
			seems to reserve field->prefix_len bytes for
			storing this field in the key value buffer, even
			though the actual value only takes data_len bytes
			from the start. */

			data_len = key_ptr[data_offset]
				   + 256 * key_ptr[data_offset + 1];
			data_field_len = data_offset + 2 + field->prefix_len;

			data_offset += 2;

			/* Now that we know the length, we store the column
			value like it would be a fixed char field */

		} else if (field->prefix_len > 0) {
			/* Looks like MySQL pads unused end bytes in the
			prefix with space. Therefore, also in UTF-8, it is ok
			to compare with a prefix containing full prefix_len
			bytes, and no need to take at most prefix_len / 3
			UTF-8 characters from the start.
			If the prefix is used as the upper end of a LIKE
			'abc%' query, then MySQL pads the end with chars
			0xff. TODO: in that case does it any harm to compare
			with the full prefix_len bytes. How do characters
			0xff in UTF-8 behave? */

		        data_len = field->prefix_len;
			data_field_len = data_offset + data_len;
		} else {
			data_len = dfield_get_type(dfield)->len;
			data_field_len = data_offset + data_len;
		}

 		if (dtype_get_mysql_type(dfield_get_type(dfield))
					== DATA_MYSQL_TRUE_VARCHAR
		    && dfield_get_type(dfield)->mtype != DATA_INT) {
			/* In a MySQL key value format, a true VARCHAR is
			always preceded by 2 bytes of a length field.
			dfield_get_type(dfield)->len returns the maximum
			'payload' len in bytes. That does not include the
			2 bytes that tell the actual data length.

			We added the check != DATA_INT to make sure we do
			not treat MySQL ENUM or SET as a true VARCHAR! */

			data_len += 2;
			data_field_len += 2;
		}

		/* Storing may use at most data_len bytes of buf */
		
		if (!is_null) {
		        row_mysql_store_col_in_innobase_format(
					dfield,
					buf,
					FALSE, /* MySQL key value format col */
					key_ptr + data_offset,
					data_len,
					index->table->comp);
			buf += data_len;
		}

    		key_ptr += data_field_len;

		if (key_ptr > key_end) {
			/* The last field in key was not a complete key field
			but a prefix of it.

		        Print a warning about this! HA_READ_PREFIX_LAST does
			not currently work in InnoDB with partial-field key
			value prefixes. Since MySQL currently uses a padding
			trick to calculate LIKE 'abc%' type queries there
			should never be partial-field prefixes in searches. */

		        ut_print_timestamp(stderr);
			
			fputs(
  "  InnoDB: Warning: using a partial-field key prefix in search.\n"
  "InnoDB: ", stderr);
			dict_index_name_print(stderr, trx, index);
			fprintf(stderr, ". Last data field length %lu bytes,\n"
  "InnoDB: key ptr now exceeds key end by %lu bytes.\n"
  "InnoDB: Key value in the MySQL format:\n",
					  (ulong) data_field_len,
					  (ulong) (key_ptr - key_end));
			fflush(stderr);
			ut_print_buf(stderr, original_key_ptr, key_len);
			fprintf(stderr, "\n");

			if (!is_null) {
			        dfield->len -= (ulint)(key_ptr - key_end);
			}
		}

		n_fields++;    		
    		field++;
		dfield++;
  	}

	ut_a(buf <= original_buf + buf_len);

 	/* We set the length of tuple to n_fields: we assume that the memory
	area allocated for it is big enough (usually bigger than n_fields). */
 	
 	dtuple_set_n_fields(tuple, n_fields);
}

/******************************************************************
Stores the row id to the prebuilt struct. */
static
void
row_sel_store_row_id_to_prebuilt(
/*=============================*/
	row_prebuilt_t*	prebuilt,	/* in: prebuilt */
	rec_t*		index_rec,	/* in: record */
	dict_index_t*	index,		/* in: index of the record */
	const ulint*	offsets)	/* in: rec_get_offsets
					(index_rec, index) */
{
	byte*	data;
	ulint	len;

	ut_ad(rec_offs_validate(index_rec, index, offsets));

	data = rec_get_nth_field(index_rec, offsets,
			dict_index_get_sys_col_pos(index, DATA_ROW_ID), &len);

	if (len != DATA_ROW_ID_LEN) {
	        fprintf(stderr,
"InnoDB: Error: Row id field is wrong length %lu in ", (ulong) len);
		dict_index_name_print(stderr, prebuilt->trx, index);
		fprintf(stderr, "\n"
"InnoDB: Field number %lu, record:\n",
		    (ulong) dict_index_get_sys_col_pos(index, DATA_ROW_ID));
		rec_print_new(stderr, index_rec, offsets);
		putc('\n', stderr);
		ut_error;
	}

	ut_memcpy(prebuilt->row_id, data, len);
}

/******************************************************************
Stores a non-SQL-NULL field in the MySQL format. The counterpart of this
function is row_mysql_store_col_in_innobase_format() in row0mysql.c. */
static
void
row_sel_field_store_in_mysql_format(
/*================================*/
	byte*	dest,	/* in/out: buffer where to store; NOTE that BLOBs
			are not in themselves stored here: the caller must
			allocate and copy the BLOB into buffer before, and pass
			the pointer to the BLOB in 'data' */
	const mysql_row_templ_t* templ,	/* in: MySQL column template.
			Its following fields are referenced:
			type, is_unsigned, mysql_col_len, mbminlen, mbmaxlen */
	byte*	data,	/* in: data to store */
	ulint	len)	/* in: length of the data */
{
	byte*	ptr;
	byte*	field_end;
	byte*	pad_ptr;

	ut_ad(len != UNIV_SQL_NULL);

	if (templ->type == DATA_INT) {
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

		if (!templ->is_unsigned) {
			dest[len - 1] = (byte) (dest[len - 1] ^ 128);
		}

		ut_ad(templ->mysql_col_len == len);
	} else if (templ->type == DATA_VARCHAR
	           || templ->type == DATA_VARMYSQL
		   || templ->type == DATA_BINARY) {

		field_end = dest + templ->mysql_col_len;

		if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR) {
			/* This is a >= 5.0.3 type true VARCHAR. Store the
			length of the data to the first byte or the first
			two bytes of dest. */
		
			dest = row_mysql_store_true_var_len(dest, len,
						templ->mysql_length_bytes);
		}

		/* Copy the actual data */
		ut_memcpy(dest, data, len);
		
		/* Pad with trailing spaces. We pad with spaces also the
		unused end of a >= 5.0.3 true VARCHAR column, just in case
		MySQL expects its contents to be deterministic. */
			
		pad_ptr = dest + len;

		ut_ad(templ->mbminlen <= templ->mbmaxlen);

		/* We handle UCS2 charset strings differently. */
		if (templ->mbminlen == 2) {
			/* A space char is two bytes, 0x0020 in UCS2 */

			if (len & 1) {
				/* A 0x20 has been stripped from the column.
				Pad it back. */
				
				if (pad_ptr < field_end) {
					*pad_ptr = 0x20;
					pad_ptr++;
				}
			}
			
			/* Pad the rest of the string with 0x0020 */

			while (pad_ptr < field_end) {
				*pad_ptr = 0x00;
				pad_ptr++;
				*pad_ptr = 0x20;
				pad_ptr++;
			}
		} else {
			ut_ad(templ->mbminlen == 1);
			/* space=0x20 */

			memset(pad_ptr, 0x20, field_end - pad_ptr);
		}
	} else if (templ->type == DATA_BLOB) {
		/* Store a pointer to the BLOB buffer to dest: the BLOB was
		already copied to the buffer in row_sel_store_mysql_rec */

		row_mysql_store_blob_ref(dest, templ->mysql_col_len, data,
									len);
	} else if (templ->type == DATA_MYSQL) {
		memcpy(dest, data, len);

		ut_ad(templ->mysql_col_len >= len);
		ut_ad(templ->mbmaxlen >= templ->mbminlen);

		ut_ad(templ->mbmaxlen > templ->mbminlen
			|| templ->mysql_col_len == len);
		/* The following assertion would fail for old tables
		containing UTF-8 ENUM columns due to Bug #9526. */
		ut_ad(!templ->mbmaxlen
			|| !(templ->mysql_col_len % templ->mbmaxlen));
		ut_ad(len * templ->mbmaxlen >= templ->mysql_col_len);

		if (templ->mbminlen != templ->mbmaxlen) {
			/* Pad with spaces. This undoes the stripping
			done in row0mysql.ic, function
			row_mysql_store_col_in_innobase_format(). */

			memset(dest + len, 0x20, templ->mysql_col_len - len);
		}
	} else {
		ut_ad(templ->type == DATA_CHAR
			|| templ->type == DATA_FIXBINARY
			/*|| templ->type == DATA_SYS_CHILD
			|| templ->type == DATA_SYS*/
			|| templ->type == DATA_FLOAT
			|| templ->type == DATA_DOUBLE
			|| templ->type == DATA_DECIMAL);
		ut_ad(templ->mysql_col_len == len);

		memcpy(dest, data, len);
	}
}

/******************************************************************
Convert a row in the Innobase format to a row in the MySQL format.
Note that the template in prebuilt may advise us to copy only a few
columns to mysql_rec, other columns are left blank. All columns may not
be needed in the query. */
static
ibool
row_sel_store_mysql_rec(
/*====================*/
					/* out: TRUE if success, FALSE if
					could not allocate memory for a BLOB
					(though we may also assert in that
					case) */
	byte*		mysql_rec,	/* out: row in the MySQL format */
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct */
	rec_t*		rec,		/* in: Innobase record in the index
					which was described in prebuilt's
					template */
	const ulint*	offsets)	/* in: array returned by
					rec_get_offsets() */
{
	mysql_row_templ_t*	templ;
	mem_heap_t*		extern_field_heap	= NULL;
	byte*			data;
	ulint			len;
	ulint			i;
	
	ut_ad(prebuilt->mysql_template);
	ut_ad(rec_offs_validate(rec, NULL, offsets));

	if (UNIV_LIKELY_NULL(prebuilt->blob_heap)) {
		mem_heap_free(prebuilt->blob_heap);
		prebuilt->blob_heap = NULL;
	}

	for (i = 0; i < prebuilt->n_template; i++) {

		templ = prebuilt->mysql_template + i;

		data = rec_get_nth_field(rec, offsets,
					templ->rec_field_no, &len);

		if (UNIV_UNLIKELY(rec_offs_nth_extern(offsets,
						templ->rec_field_no))) {

			/* Copy an externally stored field to the temporary
			heap */

			ut_a(!prebuilt->trx->has_search_latch);

			extern_field_heap = mem_heap_create(UNIV_PAGE_SIZE);

			/* NOTE: if we are retrieving a big BLOB, we may
			already run out of memory in the next call, which
			causes an assert */

			data = btr_rec_copy_externally_stored_field(rec,
					offsets, templ->rec_field_no, &len,
					extern_field_heap);

			ut_a(len != UNIV_SQL_NULL);
		}

		if (len != UNIV_SQL_NULL) {
			if (UNIV_UNLIKELY(templ->type == DATA_BLOB)) {

				ut_a(prebuilt->templ_contains_blob);

				/* A heuristic test that we can allocate the
				memory for a big BLOB. We have a safety margin
				of 1000000 bytes. Since the test takes some
				CPU time, we do not use it for small BLOBs. */

				if (UNIV_UNLIKELY(len > 2000000)
				    && UNIV_UNLIKELY(!ut_test_malloc(
							len + 1000000))) {

					ut_print_timestamp(stderr);
					fprintf(stderr,
"  InnoDB: Warning: could not allocate %lu + 1000000 bytes to retrieve\n"
"InnoDB: a big column. Table name ", (ulong) len);
					ut_print_name(stderr,
						prebuilt->trx,
						prebuilt->table->name);
					putc('\n', stderr);

					if (extern_field_heap) {
						mem_heap_free(
							extern_field_heap);
					}
					return(FALSE);
				}

				/* Copy the BLOB data to the BLOB heap of
				prebuilt */

				if (prebuilt->blob_heap == NULL) {
					prebuilt->blob_heap =
						mem_heap_create(len);
				}

				data = memcpy(mem_heap_alloc(
						prebuilt->blob_heap, len),
						data, len);
			}
		
			row_sel_field_store_in_mysql_format(
				mysql_rec + templ->mysql_col_offset,
				templ, data, len);

			/* Cleanup */
			if (extern_field_heap) {
 				mem_heap_free(extern_field_heap);
				extern_field_heap = NULL;
 			}
			
			if (templ->mysql_null_bit_mask) {
				/* It is a nullable column with a non-NULL
				value */
				mysql_rec[templ->mysql_null_byte_offset] &=
					~(byte) (templ->mysql_null_bit_mask);
			}
		} else {
		        /* MySQL seems to assume the field for an SQL NULL
		        value is set to zero or space. Not taking this into
			account caused seg faults with NULL BLOB fields, and
		        bug number 154 in the MySQL bug database: GROUP BY
		        and DISTINCT could treat NULL values inequal. */
			int	pad_char;

			mysql_rec[templ->mysql_null_byte_offset] |=
					(byte) (templ->mysql_null_bit_mask);
			switch (templ->type) {
			case DATA_VARCHAR:
			case DATA_BINARY:
			case DATA_VARMYSQL:
				if (templ->mysql_type
				    == DATA_MYSQL_TRUE_VARCHAR) {
					/* This is a >= 5.0.3 type
					true VARCHAR.  Zero the field. */
					pad_char = 0x00;
					break;
				}
				/* Fall through */
			case DATA_CHAR:
			case DATA_FIXBINARY:
			case DATA_MYSQL:
			        /* MySQL pads all string types (except
				BLOB, TEXT and true VARCHAR) with space. */
				if (UNIV_UNLIKELY(templ->mbminlen == 2)) {
					/* Treat UCS2 as a special case. */
					data = mysql_rec
						+ templ->mysql_col_offset;
					len = templ->mysql_col_len;
					/* There are two UCS2 bytes per char,
					so the length has to be even. */
					ut_a(!(len & 1));
					/* Pad with 0x0020. */
					while (len) {
						*data++ = 0x00;
						*data++ = 0x20;
						len -= 2;
					}
					continue;
				}
				pad_char = 0x20;
				break;
			default:
				pad_char = 0x00;
				break;
			}

			ut_ad(!pad_char || templ->mbminlen == 1);
			memset(mysql_rec + templ->mysql_col_offset,
					pad_char, templ->mysql_col_len);
		}
	} 

	return(TRUE);
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
	ulint**		offsets,	/* in/out: offsets returned by
					rec_get_offsets(rec, clust_index) */
	mem_heap_t**	offset_heap,	/* in/out: memory heap from which
					the offsets are allocated */
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
					offsets, read_view, offset_heap,
					prebuilt->old_vers_heap, old_vers);
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
	rec_t*		rec,	/* in: record in a non-clustered index; if
				this is a locking read, then rec is not
				allowed to be delete-marked, and that would
				not make sense either */
	que_thr_t*	thr,	/* in: query thread */
	rec_t**		out_rec,/* out: clustered record or an old version of
				it, NULL if the old version did not exist
				in the read view, i.e., it was a fresh
				inserted version */
	ulint**		offsets,/* out: offsets returned by
				rec_get_offsets(out_rec, clust_index) */
	mem_heap_t**	offset_heap,/* in/out: memory heap from which
				the offsets are allocated */
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
	trx = thr_get_trx(thr);
	
	row_build_row_ref_in_tuple(prebuilt->clust_ref, sec_index, rec, trx);

	clust_index = dict_table_get_first_index(sec_index->table);
	
	btr_pcur_open_with_no_init(clust_index, prebuilt->clust_ref,
			PAGE_CUR_LE, BTR_SEARCH_LEAF,
			prebuilt->clust_pcur, 0, mtr);

	clust_rec = btr_pcur_get_rec(prebuilt->clust_pcur);

	prebuilt->clust_pcur->trx_if_known = trx;

	/* Note: only if the search ends up on a non-infimum record is the
	low_match value the real match to the search tuple */

	if (!page_rec_is_user_rec(clust_rec)
	    || btr_pcur_get_low_match(prebuilt->clust_pcur)
	       < dict_index_get_n_unique(clust_index)) {
	
		/* In a rare case it is possible that no clust rec is found
		for a delete-marked secondary index record: if in row0umod.c
		in row_undo_mod_remove_clust_low() we have already removed
		the clust rec, while purge is still cleaning and removing
		secondary index records associated with earlier versions of
		the clustered index record. In that case we know that the
		clustered index record did not exist in the read view of
		trx. */

		if (!rec_get_deleted_flag(rec, sec_index->table->comp)
		    || prebuilt->select_lock_type != LOCK_NONE) {
		        ut_print_timestamp(stderr);
			fputs("  InnoDB: error clustered record"
				" for sec rec not found\n"
				"InnoDB: ", stderr);
			dict_index_name_print(stderr, trx, sec_index);
			fputs("\n"
				"InnoDB: sec index record ", stderr);
			rec_print(stderr, rec, sec_index);
			fputs("\n"
				"InnoDB: clust index record ", stderr);
			rec_print(stderr, clust_rec, clust_index);
			putc('\n', stderr);
			trx_print(stderr, trx, 600);

			fputs("\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n", stderr);
		}

		clust_rec = NULL;

		goto func_exit;
	}

	*offsets = rec_get_offsets(clust_rec, clust_index, *offsets,
					ULINT_UNDEFINED, offset_heap);

	if (prebuilt->select_lock_type != LOCK_NONE) {
		/* Try to place a lock on the index record; we are searching
		the clust rec with a unique condition, hence
		we set a LOCK_REC_NOT_GAP type lock */
		
		err = lock_clust_rec_read_check_and_lock(0, clust_rec,
					clust_index, *offsets,
					prebuilt->select_lock_type,
					LOCK_REC_NOT_GAP, thr);
		if (err != DB_SUCCESS) {

			goto err_exit;
		}
	} else {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		old_vers = NULL;

		/* If the isolation level allows reading of uncommitted data,
		then we never look for an earlier version */

		if (trx->isolation_level > TRX_ISO_READ_UNCOMMITTED
		    && !lock_clust_rec_cons_read_sees(clust_rec, clust_index,
						*offsets, trx->read_view)) {
		
			/* The following call returns 'offsets' associated with
			'old_vers' */
			err = row_sel_build_prev_vers_for_mysql(
					trx->read_view, clust_index,
					prebuilt, clust_rec,
					offsets, offset_heap,
					&old_vers, mtr);
						
			if (err != DB_SUCCESS) {

				goto err_exit;
			}

			clust_rec = old_vers;
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
		
		if (clust_rec && (old_vers
			|| rec_get_deleted_flag(rec, sec_index->table->comp))
		    && !row_sel_sec_rec_is_for_clust_rec(rec, sec_index,
						clust_rec, clust_index)) {
			clust_rec = NULL;
		} else {
#ifdef UNIV_SEARCH_DEBUG
			ut_a(clust_rec == NULL ||
			    row_sel_sec_rec_is_for_clust_rec(rec, sec_index,
						clust_rec, clust_index));
#endif		
		}
	}

func_exit:
	*out_rec = clust_rec;

	if (prebuilt->select_lock_type != LOCK_NONE) {
		/* We may use the cursor in unlock: store its position */
		
		btr_pcur_store_position(prebuilt->clust_pcur, mtr);
	}

	err = DB_SUCCESS;
err_exit:
	return(err);
}

/************************************************************************
Restores cursor position after it has been stored. We have to take into
account that the record cursor was positioned on may have been deleted.
Then we may have to move the cursor one step up or down. */
static
ibool
sel_restore_position_for_mysql(
/*===========================*/
					/* out: TRUE if we may need to
					process the record the cursor is
					now positioned on (i.e. we should
					not go to the next record yet) */
	ibool*		same_user_rec,	/* out: TRUE if we were able to restore
					the cursor on a user record with the
					same ordering prefix in in the
					B-tree index */
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

	*same_user_rec = success;

	if (relative_position == BTR_PCUR_ON) {
		if (success) {
			return(FALSE);
		}

		if (moves_up) {
			btr_pcur_move_to_next(pcur, mtr);
		}

		return(TRUE);
	}

	if (relative_position == BTR_PCUR_AFTER
	    || relative_position == BTR_PCUR_AFTER_LAST_IN_TREE) {

		if (moves_up) {
			return(TRUE);
		}
					
		if (btr_pcur_is_on_user_rec(pcur, mtr)) {
			btr_pcur_move_to_prev(pcur, mtr);
		}

		return(TRUE);
	}

	ut_ad(relative_position == BTR_PCUR_BEFORE
	     || relative_position == BTR_PCUR_BEFORE_FIRST_IN_TREE);
	
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
	ulint			i;
	mysql_row_templ_t*	templ;
	byte*			cached_rec;
        ut_ad(prebuilt->n_fetch_cached > 0);
	ut_ad(prebuilt->mysql_prefix_len <= prebuilt->mysql_row_len);
	
	if (UNIV_UNLIKELY(prebuilt->keep_other_fields_on_keyread))
	{
		/* Copy cache record field by field, don't touch fields that 
		are not covered by current key */
		cached_rec = 
			prebuilt->fetch_cache[prebuilt->fetch_cache_first];

		for (i = 0; i < prebuilt->n_template; i++) {
			templ = prebuilt->mysql_template + i;
			ut_memcpy(
				buf + templ->mysql_col_offset, 
				cached_rec + templ->mysql_col_offset,
				templ->mysql_col_len);
			/* Copy NULL bit of the current field from cached_rec 
			to buf */
			if (templ->mysql_null_bit_mask)
			{
				buf[templ->mysql_null_byte_offset] ^=
				  (buf[templ->mysql_null_byte_offset] ^
				   cached_rec[templ->mysql_null_byte_offset]) &
				  (byte)templ->mysql_null_bit_mask;
			}
		}
	}
	else
	{
		ut_memcpy(buf, prebuilt->fetch_cache[prebuilt->fetch_cache_first],
				prebuilt->mysql_prefix_len);
	}
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
	rec_t*		rec,		/* in: record to push */
	const ulint*	offsets)	/* in: rec_get_offsets() */
{
	byte*	buf;
	ulint	i;

	ut_ad(prebuilt->n_fetch_cached < MYSQL_FETCH_CACHE_SIZE);
	ut_ad(rec_offs_validate(rec, NULL, offsets));
	ut_a(!prebuilt->templ_contains_blob);

	if (prebuilt->fetch_cache[0] == NULL) {
		/* Allocate memory for the fetch cache */

		for (i = 0; i < MYSQL_FETCH_CACHE_SIZE; i++) {

			/* A user has reported memory corruption in these
			buffers in Linux. Put magic numbers there to help
			to track a possible bug. */
			
			buf = mem_alloc(prebuilt->mysql_row_len + 8);

			prebuilt->fetch_cache[i] = buf + 4;
				
			mach_write_to_4(buf, ROW_PREBUILT_FETCH_MAGIC_N);
			mach_write_to_4(buf + 4 + prebuilt->mysql_row_len,
					ROW_PREBUILT_FETCH_MAGIC_N);
		}
	}

	ut_ad(prebuilt->fetch_cache_first == 0);

	if (UNIV_UNLIKELY(!row_sel_store_mysql_rec(
			prebuilt->fetch_cache[prebuilt->n_fetch_cached],
			prebuilt, rec, offsets))) {
		ut_error;
	}

	prebuilt->n_fetch_cached++;
}

/*************************************************************************
Tries to do a shortcut to fetch a clustered index record with a unique key,
using the hash index if possible (not always). We assume that the search
mode is PAGE_CUR_GE, it is a consistent read, there is a read view in trx,
btr search latch has been locked in S-mode. */
static
ulint
row_sel_try_search_shortcut_for_mysql(
/*==================================*/
				/* out: SEL_FOUND, SEL_EXHAUSTED, SEL_RETRY */
	rec_t**		out_rec,/* out: record if found */
	row_prebuilt_t*	prebuilt,/* in: prebuilt struct */
	ulint**		offsets,/* in/out: for rec_get_offsets(*out_rec) */
	mem_heap_t**	heap,	/* in/out: heap for rec_get_offsets() */
	mtr_t*		mtr)	/* in: started mtr */
{
	dict_index_t*	index		= prebuilt->index;
	dtuple_t*	search_tuple	= prebuilt->search_tuple;
	btr_pcur_t*	pcur		= prebuilt->pcur;
	trx_t*		trx		= prebuilt->trx;
	rec_t*		rec;
	
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(!prebuilt->templ_contains_blob);
	
	btr_pcur_open_with_no_init(index, search_tuple, PAGE_CUR_GE,
					BTR_SEARCH_LEAF, pcur,
#ifndef UNIV_SEARCH_DEBUG
					RW_S_LATCH,
#else
					0,
#endif
					mtr);
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

	*offsets = rec_get_offsets(rec, index, *offsets,
					ULINT_UNDEFINED, heap);

	if (!lock_clust_rec_cons_read_sees(rec, index,
				*offsets, trx->read_view)) {

		return(SEL_RETRY);
	}

	if (rec_get_deleted_flag(rec, index->table->comp)) {

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
					DB_END_OF_INDEX, DB_DEADLOCK,
					DB_LOCK_TABLE_FULL, DB_CORRUPTION,
					or DB_TOO_BIG_RECORD */
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
	ibool		comp 		= index->table->comp;
	dtuple_t*	search_tuple	= prebuilt->search_tuple;
	btr_pcur_t*	pcur		= prebuilt->pcur;
	trx_t*		trx		= prebuilt->trx;
	dict_index_t*	clust_index;
	que_thr_t*	thr;
	rec_t*		rec;
	rec_t*		result_rec;
	rec_t*		clust_rec;
	rec_t*		old_vers;
	ulint		err				= DB_SUCCESS;
	ibool		unique_search			= FALSE;
	ibool		unique_search_from_clust_index	= FALSE;
	ibool		mtr_has_extra_clust_latch 	= FALSE;
	ibool		moves_up 			= FALSE;
	ibool		set_also_gap_locks		= TRUE;
					/* if the query is a plain
					locking SELECT, and the isolation
					level is <= TRX_ISO_READ_COMMITTED,
					then this is set to FALSE */
#ifdef UNIV_SEARCH_DEBUG
	ulint		cnt				= 0;
#endif /* UNIV_SEARCH_DEBUG */
	ulint		next_offs;
	ibool		same_user_rec;
	mtr_t		mtr;
	mem_heap_t*	heap				= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets				= offsets_;

	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(index && pcur && search_tuple);
	ut_ad(trx->mysql_thread_id == os_thread_get_curr_id());

	if (UNIV_UNLIKELY(prebuilt->table->ibd_file_missing)) {
	        ut_print_timestamp(stderr);
	        fprintf(stderr, "  InnoDB: Error:\n"
"InnoDB: MySQL is trying to use a table handle but the .ibd file for\n"
"InnoDB: table %s does not exist.\n"
"InnoDB: Have you deleted the .ibd file from the database directory under\n"
"InnoDB: the MySQL datadir, or have you used DISCARD TABLESPACE?\n"
"InnoDB: Look from\n"
"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/innodb-troubleshooting.html\n"
"InnoDB: how you can resolve the problem.\n",
				prebuilt->table->name);

		return(DB_ERROR);
	}

	if (UNIV_UNLIKELY(prebuilt->magic_n != ROW_PREBUILT_ALLOCATED)) {
		fprintf(stderr,
		"InnoDB: Error: trying to free a corrupt\n"
		"InnoDB: table handle. Magic n %lu, table name ",
		(ulong) prebuilt->magic_n);
		ut_print_name(stderr, trx, prebuilt->table->name);
		putc('\n', stderr);

		mem_analyze_corruption((byte*)prebuilt);

		ut_error;
	}

	if (trx->n_mysql_tables_in_use == 0
	    && UNIV_UNLIKELY(prebuilt->select_lock_type == LOCK_NONE)) {
		/* Note that if MySQL uses an InnoDB temp table that it
		created inside LOCK TABLES, then n_mysql_tables_in_use can
		be zero; in that case select_lock_type is set to LOCK_X in
		::start_stmt. */

/* August 19, 2005 by Heikki: temporarily disable this error print until the
cursor lock count is done correctly. See bugs #12263 and #12456!

		fputs(
"InnoDB: Error: MySQL is trying to perform a SELECT\n"
"InnoDB: but it has not locked any tables in ::external_lock()!\n",
                      stderr);
		trx_print(stderr, trx, 600);
                fputc('\n', stderr);
*/

	}

/*	fprintf(stderr, "Match mode %lu\n search tuple ", (ulong) match_mode);
	dtuple_print(search_tuple);
	
	fprintf(stderr, "N tables locked %lu\n", trx->mysql_n_tables_locked);
*/
	/*-------------------------------------------------------------*/
	/* PHASE 0: Release a possible s-latch we are holding on the
	adaptive hash index latch if there is someone waiting behind */

	if (UNIV_UNLIKELY(btr_search_latch.writer != RW_LOCK_NOT_LOCKED)
	    && trx->has_search_latch) {

		/* There is an x-latch request on the adaptive hash index:
		release the s-latch to reduce starvation and wait for
		BTR_SEA_TIMEOUT rounds before trying to keep it again over
		calls from MySQL */

		rw_lock_s_unlock(&btr_search_latch);
		trx->has_search_latch = FALSE;

		trx->search_latch_timeout = BTR_SEA_TIMEOUT;
	}
	
	/* Reset the new record lock info if we srv_locks_unsafe_for_binlog
	is set. Then we are able to remove the record locks set here on an
	individual row. */

	if (srv_locks_unsafe_for_binlog
	    && prebuilt->select_lock_type != LOCK_NONE) {

		trx_reset_new_rec_lock_info(trx);
	}

	/*-------------------------------------------------------------*/
	/* PHASE 1: Try to pop the row from the prefetch cache */

	if (UNIV_UNLIKELY(direction == 0)) {
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

		if (UNIV_UNLIKELY(direction != prebuilt->fetch_direction)) {
			if (UNIV_UNLIKELY(prebuilt->n_fetch_cached > 0)) {
				ut_error;
				/* TODO: scrollable cursor: restore cursor to
				the place of the latest returned row,
				or better: prevent caching for a scroll
				cursor! */
			}
		
			prebuilt->n_rows_fetched = 0;
			prebuilt->n_fetch_cached = 0;
			prebuilt->fetch_cache_first = 0;

		} else if (UNIV_LIKELY(prebuilt->n_fetch_cached > 0)) {
			row_sel_pop_cached_row_for_mysql(buf, prebuilt);

			prebuilt->n_rows_fetched++;

			srv_n_rows_read++;
			err = DB_SUCCESS;
			goto func_exit;
		}

		if (prebuilt->fetch_cache_first > 0
		    && prebuilt->fetch_cache_first < MYSQL_FETCH_CACHE_SIZE) {

		    	/* The previous returned row was popped from the fetch
		    	cache, but the cache was not full at the time of the
		    	popping: no more rows can exist in the result set */

			err = DB_RECORD_NOT_FOUND;
			goto func_exit;
		}
		
		prebuilt->n_rows_fetched++;

		if (prebuilt->n_rows_fetched > 1000000000) {
			/* Prevent wrap-over */
			prebuilt->n_rows_fetched = 500000000;
		}

		mode = pcur->search_mode;
	}

	/* In a search where at most one record in the index may match, we
	can use a LOCK_REC_NOT_GAP type record lock when locking a non-delete-
	marked matching record.

	Note that in a unique secondary index there may be different delete-
	marked versions of a record where only the primary key values differ:
	thus in a secondary index we must use next-key locks when locking
	delete-marked records. */
	
	if (match_mode == ROW_SEL_EXACT
	    && index->type & DICT_UNIQUE
	    && dtuple_get_n_fields(search_tuple)
					== dict_index_get_n_unique(index)
	    && (index->type & DICT_CLUSTERED
		 || !dtuple_contains_null(search_tuple))) {

		/* Note above that a UNIQUE secondary index can contain many
		rows with the same key value if one of the columns is the SQL
		null. A clustered index under MySQL can never contain null
		columns because we demand that all the columns in primary key
		are non-null. */

		unique_search = TRUE;

		/* Even if the condition is unique, MySQL seems to try to
		retrieve also a second row if a primary key contains more than
		1 column. Return immediately if this is not a HANDLER
		command. */

		if (UNIV_UNLIKELY(direction != 0 &&
				!prebuilt->used_in_HANDLER)) {
        
			err = DB_RECORD_NOT_FOUND;
			goto func_exit;
		}
	}

	mtr_start(&mtr);

	/*-------------------------------------------------------------*/
	/* PHASE 2: Try fast adaptive hash index search if possible */

	/* Next test if this is the special case where we can use the fast
	adaptive hash index to try the search. Since we must release the
	search system latch when we retrieve an externally stored field, we
	cannot use the adaptive hash index in a search in the case the row
	may be long and there may be externally stored fields */

	if (UNIV_UNLIKELY(direction == 0)
	    && unique_search
	    && index->type & DICT_CLUSTERED
	    && !prebuilt->templ_contains_blob
	    && !prebuilt->used_in_HANDLER
	    && (prebuilt->mysql_row_len < UNIV_PAGE_SIZE / 8)) {

		mode = PAGE_CUR_GE;

		unique_search_from_clust_index = TRUE;

		if (trx->mysql_n_tables_locked == 0
		    && prebuilt->select_lock_type == LOCK_NONE
		    && trx->isolation_level > TRX_ISO_READ_UNCOMMITTED
		    && trx->read_view) {

			/* This is a SELECT query done as a consistent read,
			and the read view has already been allocated:
			let us try a search shortcut through the hash
			index.
			NOTE that we must also test that
			mysql_n_tables_locked == 0, because this might
			also be INSERT INTO ... SELECT ... or
			CREATE TABLE ... SELECT ... . Our algorithm is
			NOT prepared to inserts interleaved with the SELECT,
			and if we try that, we can deadlock on the adaptive
			hash index semaphore! */

#ifndef UNIV_SEARCH_DEBUG			
			if (!trx->has_search_latch) {
				rw_lock_s_lock(&btr_search_latch);
				trx->has_search_latch = TRUE;
			}
#endif
			switch (row_sel_try_search_shortcut_for_mysql(&rec,
					prebuilt, &offsets, &heap, &mtr)) {
			case SEL_FOUND:
#ifdef UNIV_SEARCH_DEBUG
				ut_a(0 == cmp_dtuple_rec(search_tuple,
							rec, offsets));
#endif 
				if (!row_sel_store_mysql_rec(buf, prebuilt,
							rec, offsets)) {
 					err = DB_TOO_BIG_RECORD;

					/* We let the main loop to do the
					error handling */
 					goto shortcut_fails_too_big_rec;
				}
	
 				mtr_commit(&mtr);

				/* ut_print_name(stderr, index->name);
				fputs(" shortcut\n", stderr); */

				srv_n_rows_read++;
				
				if (trx->search_latch_timeout > 0
				    && trx->has_search_latch) {

					trx->search_latch_timeout--;

			        	rw_lock_s_unlock(&btr_search_latch);
					trx->has_search_latch = FALSE;
				}    	
				
				/* NOTE that we do NOT store the cursor
				position */
				err = DB_SUCCESS;
				goto func_exit;

			case SEL_EXHAUSTED:
 				mtr_commit(&mtr);

				/* ut_print_name(stderr, index->name);
				fputs(" record not found 2\n", stderr); */

				if (trx->search_latch_timeout > 0
				    && trx->has_search_latch) {

					trx->search_latch_timeout--;

			        	rw_lock_s_unlock(&btr_search_latch);
					trx->has_search_latch = FALSE;
				}

				/* NOTE that we do NOT store the cursor
				position */

				err = DB_RECORD_NOT_FOUND;
				goto func_exit;
			}
shortcut_fails_too_big_rec:
			mtr_commit(&mtr);
			mtr_start(&mtr);
		}
	}

	/*-------------------------------------------------------------*/
	/* PHASE 3: Open or restore index cursor position */

	if (trx->has_search_latch) {
		rw_lock_s_unlock(&btr_search_latch);
		trx->has_search_latch = FALSE;
	}			

	trx_start_if_not_started(trx);

	if (trx->isolation_level <= TRX_ISO_READ_COMMITTED
	    && prebuilt->select_lock_type != LOCK_NONE
	    && trx->mysql_query_str) {

		/* Scan the MySQL query string; check if SELECT is the first
	        word there */
		ibool	success;

		dict_accept(*trx->mysql_query_str, "SELECT", &success);

		if (success) {
			/* It is a plain locking SELECT and the isolation
			level is low: do not lock gaps */

			set_also_gap_locks = FALSE;
		}
	}
	
	/* Note that if the search mode was GE or G, then the cursor
	naturally moves upward (in fetch next) in alphabetical order,
	otherwise downward */
	
	if (UNIV_UNLIKELY(direction == 0)) {
		if (mode == PAGE_CUR_GE || mode == PAGE_CUR_G) {
			moves_up = TRUE;
		}
	} else if (direction == ROW_SEL_NEXT) {
		moves_up = TRUE;
	}

	thr = que_fork_get_first_thr(prebuilt->sel_graph);

	que_thr_move_to_run_state_for_mysql(thr, trx);

	clust_index = dict_table_get_first_index(index->table);

	if (UNIV_LIKELY(direction != 0)) {
		if (!sel_restore_position_for_mysql(&same_user_rec,
						BTR_SEARCH_LEAF,
						pcur, moves_up, &mtr)) {
			goto next_rec;
		}

	} else if (dtuple_get_n_fields(search_tuple) > 0) {

		btr_pcur_open_with_no_init(index, search_tuple, mode,
					BTR_SEARCH_LEAF,
					pcur, 0, &mtr);

		pcur->trx_if_known = trx;
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

		if (trx->read_view == NULL
		    && prebuilt->select_lock_type == LOCK_NONE) {

			fputs(
"InnoDB: Error: MySQL is trying to perform a consistent read\n"
"InnoDB: but the read view is not assigned!\n", stderr);
			trx_print(stderr, trx, 600);
                        fputc('\n', stderr);
			ut_a(0);
		}
	} else if (prebuilt->select_lock_type == LOCK_NONE) {
		/* This is a consistent read */	
		/* Assign a read view for the query */

		trx_assign_read_view(trx);
		prebuilt->sql_stat_start = FALSE;
	} else {
		ulint	lock_mode;
		if (prebuilt->select_lock_type == LOCK_S) {		
			lock_mode = LOCK_IS;
		} else {
			lock_mode = LOCK_IX;
		}
		err = lock_table(0, index->table, lock_mode, thr);

		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}
		prebuilt->sql_stat_start = FALSE;
	}

rec_loop:
	/*-------------------------------------------------------------*/
	/* PHASE 4: Look for matching records in a loop */
	
	rec = btr_pcur_get_rec(pcur);
	ut_ad(!!page_rec_is_comp(rec) == comp);
#ifdef UNIV_SEARCH_DEBUG
/*
	fputs("Using ", stderr);
	dict_index_name_print(stderr, index);
	fprintf(stderr, " cnt %lu ; Page no %lu\n", cnt,
			buf_frame_get_page_no(buf_frame_align(rec)));
	rec_print(rec);
*/
#endif /* UNIV_SEARCH_DEBUG */

	if (page_rec_is_infimum(rec)) {

		/* The infimum record on a page cannot be in the result set,
		and neither can a record lock be placed on it: we skip such
		a record. */

		goto next_rec;
	}

	if (page_rec_is_supremum(rec)) {

		if (set_also_gap_locks
		    && !srv_locks_unsafe_for_binlog
		    && prebuilt->select_lock_type != LOCK_NONE) {

			/* Try to place a lock on the index record */

			/* If innodb_locks_unsafe_for_binlog option is used,
			we do not lock gaps. Supremum record is really
			a gap and therefore we do not set locks there. */
			
			offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);
			err = sel_set_rec_lock(rec, index, offsets,
					prebuilt->select_lock_type,
					LOCK_ORDINARY, thr);

			if (err != DB_SUCCESS) {

				goto lock_wait_or_error;
			}
		}
		/* A page supremum record cannot be in the result set: skip
		it now that we have placed a possible lock on it */
		
		goto next_rec;
	}

	/*-------------------------------------------------------------*/
	/* Do sanity checks in case our cursor has bumped into page
	corruption */
	
	if (comp) {
		next_offs = rec_get_next_offs(rec, TRUE);
		if (UNIV_UNLIKELY(next_offs < PAGE_NEW_SUPREMUM)) {

			goto wrong_offs;
		}
	} else {
		next_offs = rec_get_next_offs(rec, FALSE);
		if (UNIV_UNLIKELY(next_offs < PAGE_OLD_SUPREMUM)) {

			goto wrong_offs;
		}
	}

	if (UNIV_UNLIKELY(next_offs >= UNIV_PAGE_SIZE - PAGE_DIR)) {

wrong_offs:
		if (srv_force_recovery == 0 || moves_up == FALSE) {
			ut_print_timestamp(stderr);
			buf_page_print(buf_frame_align(rec));
			fprintf(stderr,
"\nInnoDB: rec address %p, first buffer frame %p\n"
"InnoDB: buffer pool high end %p, buf block fix count %lu\n",
				rec, buf_pool->frame_zero,
				buf_pool->high_end,
				(ulong)buf_block_align(rec)->buf_fix_count);
			fprintf(stderr,
"InnoDB: Index corruption: rec offs %lu next offs %lu, page no %lu,\n"
"InnoDB: ",
				(ulong) ut_align_offset(rec, UNIV_PAGE_SIZE),
				(ulong) next_offs,
				(ulong) buf_frame_get_page_no(rec));
			dict_index_name_print(stderr, trx, index);
			fputs(". Run CHECK TABLE. You may need to\n"
"InnoDB: restore from a backup, or dump + drop + reimport the table.\n",
			      stderr);
		
			err = DB_CORRUPTION;

			goto lock_wait_or_error;
		} else {
			/* The user may be dumping a corrupt table. Jump
			over the corruption to recover as much as possible. */

			fprintf(stderr,
"InnoDB: Index corruption: rec offs %lu next offs %lu, page no %lu,\n"
"InnoDB: ",
			   (ulong) ut_align_offset(rec, UNIV_PAGE_SIZE),
			   (ulong) next_offs,
			   (ulong) buf_frame_get_page_no(rec));
			dict_index_name_print(stderr, trx, index);
			fputs(". We try to skip the rest of the page.\n",
				stderr);

			btr_pcur_move_to_last_on_page(pcur, &mtr);

			goto next_rec;
		}
	}
	/*-------------------------------------------------------------*/

	/* Calculate the 'offsets' associated with 'rec' */

	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

	if (UNIV_UNLIKELY(srv_force_recovery > 0)) {
		if (!rec_validate(rec, offsets)
		|| !btr_index_rec_validate(rec, index, FALSE)) {
			fprintf(stderr,
"InnoDB: Index corruption: rec offs %lu next offs %lu, page no %lu,\n"
"InnoDB: ",
			   (ulong) ut_align_offset(rec, UNIV_PAGE_SIZE),
			   (ulong) next_offs,
			   (ulong) buf_frame_get_page_no(rec));
			dict_index_name_print(stderr, trx, index);
			fputs(". We try to skip the record.\n",
				stderr);

			goto next_rec;
		}
	}

	/* Note that we cannot trust the up_match value in the cursor at this
	place because we can arrive here after moving the cursor! Thus
	we have to recompare rec and search_tuple to determine if they
	match enough. */

	if (match_mode == ROW_SEL_EXACT) {
		/* Test if the index record matches completely to search_tuple
		in prebuilt: if not, then we return with DB_RECORD_NOT_FOUND */

		/* fputs("Comparing rec and search tuple\n", stderr); */
		
		if (0 != cmp_dtuple_rec(search_tuple, rec, offsets)) {

			if (set_also_gap_locks
			    && !srv_locks_unsafe_for_binlog
			    && prebuilt->select_lock_type != LOCK_NONE) {

				/* Try to place a gap lock on the index 
				record only if innodb_locks_unsafe_for_binlog
				option is not set */

				err = sel_set_rec_lock(rec, index, offsets,
						prebuilt->select_lock_type,
						LOCK_GAP, thr);

				if (err != DB_SUCCESS) {

					goto lock_wait_or_error;
				}
			}

			btr_pcur_store_position(pcur, &mtr);

			err = DB_RECORD_NOT_FOUND;
			/* ut_print_name(stderr, index->name);
			fputs(" record not found 3\n", stderr); */
			
			goto normal_return;
		}

	} else if (match_mode == ROW_SEL_EXACT_PREFIX) {

		if (!cmp_dtuple_is_prefix_of_rec(search_tuple, rec, offsets)) {
			
			if (set_also_gap_locks
			    && !srv_locks_unsafe_for_binlog
			    && prebuilt->select_lock_type != LOCK_NONE) {

				/* Try to place a gap lock on the index 
				record only if innodb_locks_unsafe_for_binlog
				option is not set */

				err = sel_set_rec_lock(rec, index, offsets,
						prebuilt->select_lock_type,
						LOCK_GAP, thr);

				if (err != DB_SUCCESS) {

					goto lock_wait_or_error;
				}
			}

			btr_pcur_store_position(pcur, &mtr);

			err = DB_RECORD_NOT_FOUND;
			/* ut_print_name(stderr, index->name);
			fputs(" record not found 4\n", stderr); */

			goto normal_return;
		}
	}

	/* We are ready to look at a possible new index entry in the result
	set: the cursor is now placed on a user record */

	if (prebuilt->select_lock_type != LOCK_NONE) {
		/* Try to place a lock on the index record; note that delete
		marked records are a special case in a unique search. If there
		is a non-delete marked record, then it is enough to lock its
		existence with LOCK_REC_NOT_GAP. */

		/* If innodb_locks_unsafe_for_binlog option is used,
		we lock only the record, i.e., next-key locking is
		not used. */

		ulint	lock_type;

		if (!set_also_gap_locks
		    || srv_locks_unsafe_for_binlog
		    || (unique_search && !UNIV_UNLIKELY(rec_get_deleted_flag(
								rec, comp)))) {

			goto no_gap_lock;
		} else {
			lock_type = LOCK_ORDINARY;
		}

		/* If we are doing a 'greater or equal than a primary key
		value' search from a clustered index, and we find a record
		that has that exact primary key value, then there is no need
		to lock the gap before the record, because no insert in the
		gap can be in our search range. That is, no phantom row can
		appear that way.

		An example: if col1 is the primary key, the search is WHERE
		col1 >= 100, and we find a record where col1 = 100, then no
		need to lock the gap before that record. */

		if (index == clust_index
		    && mode == PAGE_CUR_GE
		    && direction == 0
		    && dtuple_get_n_fields_cmp(search_tuple)
		       == dict_index_get_n_unique(index)
		    && 0 == cmp_dtuple_rec(search_tuple, rec, offsets)) {
no_gap_lock:
			lock_type = LOCK_REC_NOT_GAP;
		}

		err = sel_set_rec_lock(rec, index, offsets,
					prebuilt->select_lock_type,
					lock_type, thr);

		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}
	} else {
		/* This is a non-locking consistent read: if necessary, fetch
		a previous version of the record */

		if (trx->isolation_level == TRX_ISO_READ_UNCOMMITTED) {

			/* Do nothing: we let a non-locking SELECT read the
			latest version of the record */
		
		} else if (index == clust_index) {
			  
			/* Fetch a previous version of the row if the current
			one is not visible in the snapshot; if we have a very
			high force recovery level set, we try to avoid crashes
			by skipping this lookup */

			if (UNIV_LIKELY(srv_force_recovery < 5)
                            && !lock_clust_rec_cons_read_sees(rec, index,
						offsets, trx->read_view)) {

				/* The following call returns 'offsets'
				associated with 'old_vers' */
				err = row_sel_build_prev_vers_for_mysql(
						trx->read_view, clust_index,
						prebuilt, rec,
						&offsets, &heap,
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

			ut_ad(index != clust_index);

			goto requires_clust_rec;
		}
	}

	/* NOTE that at this point rec can be an old version of a clustered
	index record built for a consistent read. We cannot assume after this
	point that rec is on a buffer pool page. Functions like
	page_rec_is_comp() cannot be used! */

	if (UNIV_UNLIKELY(rec_get_deleted_flag(rec, comp))) {

		/* The record is delete-marked: we can skip it */

		if (srv_locks_unsafe_for_binlog
	    	    && prebuilt->select_lock_type != LOCK_NONE) {

			/* No need to keep a lock on a delete-marked record
			if we do not want to use next-key locking. */

			row_unlock_for_mysql(prebuilt, TRUE);
			
			trx_reset_new_rec_lock_info(trx);
		}
		
		goto next_rec;
	}

	/* Get the clustered index record if needed, if we did not do the
	search using the clustered index. */

	if (index != clust_index && prebuilt->need_to_access_clustered) {

requires_clust_rec:
		/* We use a 'goto' to the preceding label if a consistent
		read of a secondary index record requires us to look up old
		versions of the associated clustered index record. */

		ut_ad(rec_offs_validate(rec, index, offsets));

		/* It was a non-clustered index and we must fetch also the
		clustered index record */

		mtr_has_extra_clust_latch = TRUE;

		/* The following call returns 'offsets' associated with
		'clust_rec'. Note that 'clust_rec' can be an old version
		built for a consistent read. */
		
		err = row_sel_get_clust_rec_for_mysql(prebuilt, index, rec,
							thr, &clust_rec,
							&offsets, &heap, &mtr);
		if (err != DB_SUCCESS) {

			goto lock_wait_or_error;
		}

		if (clust_rec == NULL) {
			/* The record did not exist in the read view */
			ut_ad(prebuilt->select_lock_type == LOCK_NONE);

			goto next_rec;
		}

		if (UNIV_UNLIKELY(rec_get_deleted_flag(clust_rec, comp))) {

			/* The record is delete marked: we can skip it */

			if (srv_locks_unsafe_for_binlog
	    		    && prebuilt->select_lock_type != LOCK_NONE) {

				/* No need to keep a lock on a delete-marked
				record if we do not want to use next-key
				locking. */

				row_unlock_for_mysql(prebuilt, TRUE);
			
				trx_reset_new_rec_lock_info(trx);
			}

			goto next_rec;
		}
		
		if (prebuilt->need_to_access_clustered) {

		        result_rec = clust_rec;

			ut_ad(rec_offs_validate(result_rec, clust_index,
								offsets));
		} else {
			/* We used 'offsets' for the clust rec, recalculate
			them for 'rec' */
			offsets = rec_get_offsets(rec, index, offsets,
						ULINT_UNDEFINED, &heap);
			result_rec = rec;
		}
	} else {
		result_rec = rec;
	}

	/* We found a qualifying record 'result_rec'. At this point,
	'offsets' are associated with 'result_rec'. */

	ut_ad(rec_offs_validate(result_rec,
				result_rec != rec ? clust_index : index,
				offsets));

	if ((match_mode == ROW_SEL_EXACT
		|| prebuilt->n_rows_fetched >= MYSQL_FETCH_CACHE_THRESHOLD)
			&& prebuilt->select_lock_type == LOCK_NONE
			&& !prebuilt->templ_contains_blob
			&& !prebuilt->clust_index_was_generated
			&& !prebuilt->used_in_HANDLER
	                && prebuilt->template_type
	                                 != ROW_MYSQL_DUMMY_TEMPLATE) {

		/* Inside an update, for example, we do not cache rows,
		since we may use the cursor position to do the actual
		update, that is why we require ...lock_type == LOCK_NONE.
		Since we keep space in prebuilt only for the BLOBs of
		a single row, we cannot cache rows in the case there
		are BLOBs in the fields to be fetched. In HANDLER we do
		not cache rows because there the cursor is a scrollable
		cursor. */

		row_sel_push_cache_row_for_mysql(prebuilt, result_rec,
								offsets);
		if (prebuilt->n_fetch_cached == MYSQL_FETCH_CACHE_SIZE) {
			
			goto got_row;
		}

		goto next_rec;
	} else {
		if (prebuilt->template_type == ROW_MYSQL_DUMMY_TEMPLATE) {
			memcpy(buf + 4, result_rec
						- rec_offs_extra_size(offsets),
					rec_offs_size(offsets));
			mach_write_to_4(buf,
					rec_offs_extra_size(offsets) + 4);
		} else {
			if (!row_sel_store_mysql_rec(buf, prebuilt,
							result_rec, offsets)) {
				err = DB_TOO_BIG_RECORD;

				goto lock_wait_or_error;
			}
		}

		if (prebuilt->clust_index_was_generated) {
			if (result_rec != rec) {
				offsets = rec_get_offsets(
						rec, index, offsets,
						ULINT_UNDEFINED, &heap);
			}
			row_sel_store_row_id_to_prebuilt(prebuilt, rec,
							index, offsets);
		}
	}

	/* From this point on, 'offsets' are invalid. */

got_row:
	/* We have an optimization to save CPU time: if this is a consistent
	read on a unique condition on the clustered index, then we do not
	store the pcur position, because any fetch next or prev will anyway
	return 'end of file'. Exceptions are locking reads and the MySQL 
	HANDLER command where the user can move the cursor with PREV or NEXT 
	even after a unique search. */

	if (!unique_search_from_clust_index
	    || prebuilt->select_lock_type != LOCK_NONE
	    || prebuilt->used_in_HANDLER) {

		/* Inside an update always store the cursor position */

		btr_pcur_store_position(pcur, &mtr);
	}

	err = DB_SUCCESS;

	goto normal_return;

next_rec:
	/*-------------------------------------------------------------*/
	/* PHASE 5: Move the cursor to the next index record */

	if (UNIV_UNLIKELY(mtr_has_extra_clust_latch)) {
		/* We must commit mtr if we are moving to the next
		non-clustered index record, because we could break the
		latching order if we would access a different clustered
		index page right away without releasing the previous. */

		btr_pcur_store_position(pcur, &mtr);

		mtr_commit(&mtr);
		mtr_has_extra_clust_latch = FALSE;
	
		mtr_start(&mtr);
		if (sel_restore_position_for_mysql(&same_user_rec,
						BTR_SEARCH_LEAF,
						pcur, moves_up, &mtr)) {
#ifdef UNIV_SEARCH_DEBUG
			cnt++;
#endif /* UNIV_SEARCH_DEBUG */

			goto rec_loop;
		}
	}

	if (moves_up) {		
		if (UNIV_UNLIKELY(!btr_pcur_move_to_next(pcur, &mtr))) {
not_moved:
			btr_pcur_store_position(pcur, &mtr);

			if (match_mode != 0) {
				err = DB_RECORD_NOT_FOUND;
			} else {
				err = DB_END_OF_INDEX;
			}

			goto normal_return;
		}
	} else {
		if (UNIV_UNLIKELY(!btr_pcur_move_to_prev(pcur, &mtr))) {
			goto not_moved;
		}
	}

#ifdef UNIV_SEARCH_DEBUG
	cnt++;
#endif /* UNIV_SEARCH_DEBUG */

	goto rec_loop;

lock_wait_or_error:
	/*-------------------------------------------------------------*/

	btr_pcur_store_position(pcur, &mtr);

	mtr_commit(&mtr);
	mtr_has_extra_clust_latch = FALSE;
		
	trx->error_state = err;

	/* The following is a patch for MySQL */

	que_thr_stop_for_mysql(thr);

	thr->lock_state = QUE_THR_LOCK_ROW;

	if (row_mysql_handle_errors(&err, trx, thr, NULL)) {
		/* It was a lock wait, and it ended */

		thr->lock_state = QUE_THR_LOCK_NOLOCK;
		mtr_start(&mtr);

		sel_restore_position_for_mysql(&same_user_rec,
						BTR_SEARCH_LEAF, pcur,
						moves_up, &mtr);
		if (srv_locks_unsafe_for_binlog && !same_user_rec) {
			/* Since we were not able to restore the cursor
			on the same user record, we cannot use
			row_unlock_for_mysql() to unlock any records, and
			we must thus reset the new rec lock info. Since
			in lock0lock.c we have blocked the inheriting of gap
			X-locks, we actually do not have any new record locks
			set in this case.

			Note that if we were able to restore on the 'same'
			user record, it is still possible that we were actually
			waiting on a delete-marked record, and meanwhile
			it was removed by purge and inserted again by some
			other user. But that is no problem, because in
			rec_loop we will again try to set a lock, and
			new_rec_lock_info in trx will be right at the end. */

			trx_reset_new_rec_lock_info(trx);
		}
	
		mode = pcur->search_mode;

		goto rec_loop;
	}

	thr->lock_state = QUE_THR_LOCK_NOLOCK;

#ifdef UNIV_SEARCH_DEBUG
/*	fputs("Using ", stderr);
	dict_index_name_print(stderr, index);
	fprintf(stderr, " cnt %lu ret value %lu err\n", cnt, err); */
#endif /* UNIV_SEARCH_DEBUG */
	goto func_exit;

normal_return:
	/*-------------------------------------------------------------*/
	que_thr_stop_for_mysql_no_error(thr, trx);

	mtr_commit(&mtr);

	if (prebuilt->n_fetch_cached > 0) {
		row_sel_pop_cached_row_for_mysql(buf, prebuilt);

		err = DB_SUCCESS;
	}

#ifdef UNIV_SEARCH_DEBUG
/*	fputs("Using ", stderr);
	dict_index_name_print(stderr, index);
	fprintf(stderr, " cnt %lu ret value %lu err\n", cnt, err); */
#endif /* UNIV_SEARCH_DEBUG */
	if (err == DB_SUCCESS) {
		srv_n_rows_read++;
	}

func_exit:
	trx->op_info = "";
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
}

/***********************************************************************
Checks if MySQL at the moment is allowed for this table to retrieve a
consistent read result, or store it to the query cache. */

ibool
row_search_check_if_query_cache_permitted(
/*======================================*/
					/* out: TRUE if storing or retrieving
					from the query cache is permitted */
	trx_t*		trx,		/* in: transaction object */
	const char*	norm_name)	/* in: concatenation of database name,
					'/' char, table name */
{
	dict_table_t*	table;
	ibool		ret 	= FALSE;

	table = dict_table_get(norm_name, trx);

	if (table == NULL) {

		return(FALSE);
	}

	mutex_enter(&kernel_mutex);

	/* Start the transaction if it is not started yet */

	trx_start_if_not_started_low(trx);

	/* If there are locks on the table or some trx has invalidated the
	cache up to our trx id, then ret = FALSE.
	We do not check what type locks there are on the table, though only
	IX type locks actually would require ret = FALSE. */

	if (UT_LIST_GET_LEN(table->locks) == 0
	    && ut_dulint_cmp(trx->id, table->query_cache_inv_trx_id) >= 0) {

		ret = TRUE;
		
		/* If the isolation level is high, assign a read view for the
		transaction if it does not yet have one */

		if (trx->isolation_level >= TRX_ISO_REPEATABLE_READ
		    && !trx->read_view) {

			trx->read_view = read_view_open_now(trx,
						trx->global_read_view_heap);
			trx->global_read_view = trx->read_view;
		}
	}
	
	mutex_exit(&kernel_mutex);

	return(ret);
}
