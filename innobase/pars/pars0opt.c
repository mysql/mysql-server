/******************************************************
Simple SQL optimizer

(c) 1997 Innobase Oy

Created 12/21/1997 Heikki Tuuri
*******************************************************/

#include "pars0opt.h"

#ifdef UNIV_NONINL
#include "pars0opt.ic"
#endif

#include "row0sel.h"
#include "row0ins.h"
#include "row0upd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "que0que.h"
#include "pars0grm.h"
#include "pars0pars.h"
#include "lock0lock.h"

#define OPT_EQUAL	1	/* comparison by = */
#define OPT_COMPARISON	2	/* comparison by <, >, <=, or >= */

#define OPT_NOT_COND	1
#define OPT_END_COND	2
#define OPT_TEST_COND	3
#define OPT_SCROLL_COND	4


/***********************************************************************
Inverts a comparison operator. */
static
int
opt_invert_cmp_op(
/*==============*/
			/* out: the equivalent operator when the order of
			the arguments is switched */
	int	op)	/* in: operator */
{
	if (op == '<') {
		return('>');
	} else if (op == '>') {
		return('<');
	} else if (op == '=') {
		return('=');
	} else if (op == PARS_LE_TOKEN) {
		return(PARS_GE_TOKEN);
	} else if (op == PARS_GE_TOKEN) {
		return(PARS_LE_TOKEN);
	} else {
		ut_error;
	}

	return(0);
}

/***********************************************************************
Checks if the value of an expression can be calculated BEFORE the nth table
in a join is accessed. If this is the case, it can possibly be used in an
index search for the nth table. */
static
ibool
opt_check_exp_determined_before(
/*============================*/
					/* out: TRUE if already determined */
	que_node_t*	exp,		/* in: expression */
	sel_node_t*	sel_node,	/* in: select node */
	ulint		nth_table)	/* in: nth table will be accessed */
{
	func_node_t*	func_node;
	sym_node_t*	sym_node;
	dict_table_t*	table;
	que_node_t*	arg;
	ulint		i;

	ut_ad(exp && sel_node);

	if (que_node_get_type(exp) == QUE_NODE_FUNC) {
		func_node = exp;

		arg = func_node->args;

		while (arg) {
			if (!opt_check_exp_determined_before(arg, sel_node,
								nth_table)) {
				return(FALSE);
			}

			arg = que_node_get_next(arg);
		}

		return(TRUE);
	}

	ut_a(que_node_get_type(exp) == QUE_NODE_SYMBOL);

	sym_node = exp;

	if (sym_node->token_type != SYM_COLUMN) {

		return(TRUE);
	}

	for (i = 0; i < nth_table; i++) {
	
		table = sel_node_get_nth_plan(sel_node, i)->table;

		if (sym_node->table == table) {

			return(TRUE);
		}
	}

	return(FALSE);	
}

/***********************************************************************
Looks in a comparison condition if a column value is already restricted by
it BEFORE the nth table is accessed. */
static
que_node_t*
opt_look_for_col_in_comparison_before(
/*==================================*/
					/* out: expression restricting the
					value of the column, or NULL if not
					known */
	ulint		cmp_type,	/* in: OPT_EQUAL, OPT_COMPARISON */
	ulint		col_no,		/* in: column number */
	func_node_t*	search_cond,	/* in: comparison condition */
	sel_node_t*	sel_node,	/* in: select node */
	ulint		nth_table,	/* in: nth table in a join (a query
					from a single table is considered a
					join of 1 table) */
	ulint*		op)		/* out: comparison operator ('=',
					PARS_GE_TOKEN, ... ); this is inverted
					if the column appears on the right
					side */
{
	sym_node_t*	sym_node;
	dict_table_t*	table;
	que_node_t*	exp;
	que_node_t*	arg;

	ut_ad(search_cond);

	ut_a((search_cond->func == '<')
	     || (search_cond->func == '>')		
	     || (search_cond->func == '=')		
	     || (search_cond->func == PARS_GE_TOKEN)
	     || (search_cond->func == PARS_LE_TOKEN));

	table = sel_node_get_nth_plan(sel_node, nth_table)->table;

	if ((cmp_type == OPT_EQUAL) && (search_cond->func != '=')) {

		return(NULL);

	} else if ((cmp_type == OPT_COMPARISON)
			&& (search_cond->func != '<')
			&& (search_cond->func != '>')			
			&& (search_cond->func != PARS_GE_TOKEN)
			&& (search_cond->func != PARS_LE_TOKEN)) {

		return(NULL);
	}

	arg = search_cond->args;

	if (que_node_get_type(arg) == QUE_NODE_SYMBOL) {
		sym_node = arg;

		if ((sym_node->token_type == SYM_COLUMN)
				&& (sym_node->table == table)
				&& (sym_node->col_no == col_no)) {
				
			/* sym_node contains the desired column id */

			/* Check if the expression on the right side of the
			operator is already determined */

			exp = que_node_get_next(arg);
				
			if (opt_check_exp_determined_before(exp, sel_node,
								nth_table)) {
				*op = search_cond->func;

				return(exp);
			}
		}
    	}

    	exp = search_cond->args;
        arg = que_node_get_next(arg);

	if (que_node_get_type(arg) == QUE_NODE_SYMBOL) {
		sym_node = arg;

		if ((sym_node->token_type == SYM_COLUMN)
				&& (sym_node->table == table)
				&& (sym_node->col_no == col_no)) {
				
			if (opt_check_exp_determined_before(exp, sel_node,
								nth_table)) {
				*op = opt_invert_cmp_op(search_cond->func);

				return(exp);
			}
		}
    	}
	
	return(NULL);
}

/***********************************************************************
Looks in a search condition if a column value is already restricted by the
search condition BEFORE the nth table is accessed. Takes into account that
if we will fetch in an ascending order, we cannot utilize an upper limit for
a column value; in a descending order, respectively, a lower limit. */
static
que_node_t*
opt_look_for_col_in_cond_before(
/*============================*/
					/* out: expression restricting the
					value of the column, or NULL if not
					known */
	ulint		cmp_type,	/* in: OPT_EQUAL, OPT_COMPARISON */
	ulint		col_no,		/* in: column number */
	func_node_t*	search_cond,	/* in: search condition or NULL */
	sel_node_t*	sel_node,	/* in: select node */
	ulint		nth_table,	/* in: nth table in a join (a query
					from a single table is considered a
					join of 1 table) */
	ulint*		op)		/* out: comparison operator ('=',
					PARS_GE_TOKEN, ... ) */
{
	func_node_t*	new_cond;
	que_node_t*	exp;

	if (search_cond == NULL) {

		return(NULL);
	}		

	ut_a(que_node_get_type(search_cond) == QUE_NODE_FUNC);
	ut_a(search_cond->func != PARS_OR_TOKEN);
	ut_a(search_cond->func != PARS_NOT_TOKEN);	
	
	if (search_cond->func == PARS_AND_TOKEN) {
		new_cond = search_cond->args;

		exp = opt_look_for_col_in_cond_before(cmp_type, col_no,
					new_cond, sel_node, nth_table, op);
		if (exp) {

			return(exp);
		}

		new_cond = que_node_get_next(new_cond);
		
		exp = opt_look_for_col_in_cond_before(cmp_type, col_no,
					new_cond, sel_node, nth_table, op);
		return(exp);
	}

	exp = opt_look_for_col_in_comparison_before(cmp_type, col_no,
					search_cond, sel_node, nth_table, op);
	if (exp == NULL) {

		return(NULL);
	}

	/* If we will fetch in an ascending order, we cannot utilize an upper
	limit for a column value; in a descending order, respectively, a lower
	limit */
	
	if (sel_node->asc && ((*op == '<') || (*op == PARS_LE_TOKEN))) {

		return(NULL);

	} else if (!sel_node->asc && ((*op == '>') || (*op == PARS_GE_TOKEN))) {

		return(NULL);
	}

	return(exp);
}

/***********************************************************************
Calculates the goodness for an index according to a select node. The
goodness is 4 times the number of first fields in index whose values we
already know exactly in the query. If we have a comparison condition for
an additional field, 2 point are added. If the index is unique, and we know
all the unique fields for the index we add 1024 points. For a clustered index
we add 1 point. */
static
ulint
opt_calc_index_goodness(
/*====================*/
					/* out: goodness */
	dict_index_t*	index,		/* in: index */
	sel_node_t*	sel_node,	/* in: parsed select node */
	ulint		nth_table,	/* in: nth table in a join */
	que_node_t**	index_plan,	/* in/out: comparison expressions for
					this index */
	ulint*		last_op)	/* out: last comparison operator, if
					goodness > 1 */
{
	que_node_t*	exp;
	ulint		goodness;
	ulint		n_fields;
	ulint		col_no;
	ulint		mix_id_col_no;
	ulint		op;
	ulint		j;

	goodness = 0;

	/* Note that as higher level node pointers in the B-tree contain
	page addresses as the last field, we must not put more fields in
	the search tuple than dict_index_get_n_unique_in_tree(index); see
	the note in btr_cur_search_to_nth_level. */
	
	n_fields = dict_index_get_n_unique_in_tree(index);

	mix_id_col_no = dict_table_get_sys_col_no(index->table, DATA_MIX_ID);
	
	for (j = 0; j < n_fields; j++) {

		col_no = dict_index_get_nth_col_no(index, j);

		exp = opt_look_for_col_in_cond_before(OPT_EQUAL, col_no,
						sel_node->search_cond,
						sel_node, nth_table, &op);
		if (col_no == mix_id_col_no) {
			ut_ad(exp == NULL);
			
			index_plan[j] = NULL;
			*last_op = '=';
			goodness += 4;
		} else if (exp) {			
			/* The value for this column is exactly known already
			at this stage of the join */

			index_plan[j] = exp;
			*last_op = op;
			goodness += 4;
		} else {
			/* Look for non-equality comparisons */

			exp = opt_look_for_col_in_cond_before(OPT_COMPARISON,
						col_no, sel_node->search_cond,
						sel_node, nth_table, &op);
			if (exp) {
				index_plan[j] = exp;
				*last_op = op;
				goodness += 2;
			}				
		
			break;
		}	
	}

	if (goodness >= 4 * dict_index_get_n_unique(index)) {
		goodness += 1024;

		if (index->type & DICT_CLUSTERED) {

			goodness += 1024;
		}
	}

	/* We have to test for goodness here, as last_op may note be set */
	if (goodness && index->type & DICT_CLUSTERED) {

		goodness++;
	}

	return(goodness);
}

/***********************************************************************
Calculates the number of matched fields based on an index goodness. */
UNIV_INLINE
ulint
opt_calc_n_fields_from_goodness(
/*============================*/
				/* out: number of excatly or partially matched
				fields */
	ulint	goodness)	/* in: goodness */
{
	return(((goodness % 1024) + 2) / 4);
}

/***********************************************************************
Converts a comparison operator to the corresponding search mode PAGE_CUR_GE,
... */
UNIV_INLINE
ulint
opt_op_to_search_mode(
/*==================*/
			/* out: search mode */
	ibool	asc,	/* in: TRUE if the rows should be fetched in an
			ascending order */
	ulint	op)	/* in: operator '=', PARS_GE_TOKEN, ... */
{
	if (op == '=') {
		if (asc) {
			return(PAGE_CUR_GE);
		} else {
			return(PAGE_CUR_LE);
		}	
	} else if (op == '<') {
		ut_a(!asc);
		return(PAGE_CUR_L);
	} else if (op == '>') {
		ut_a(asc);
		return(PAGE_CUR_G);
	} else if (op == PARS_GE_TOKEN) {
		ut_a(asc);
		return(PAGE_CUR_GE);
	} else if (op == PARS_LE_TOKEN) {
		ut_a(!asc);
		return(PAGE_CUR_LE);
	} else {
		ut_error;
	}

	return(0);
}

/***********************************************************************
Determines if a node is an argument node of a function node. */
static
ibool
opt_is_arg(
/*=======*/
					/* out: TRUE if is an argument */
	que_node_t*	arg_node,	/* in: possible argument node */
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg;

	arg = func_node->args;

	while (arg) {
		if (arg == arg_node) {

			return(TRUE);
		}

		arg = que_node_get_next(arg);
	}

	return(FALSE);
}

/***********************************************************************
Decides if the fetching of rows should be made in a descending order, and
also checks that the chosen query plan produces a result which satisfies
the order-by. */
static
void
opt_check_order_by(
/*===============*/
	sel_node_t*	sel_node)	/* in: select node; asserts an error
					if the plan does not agree with the
					order-by */
{
	order_node_t*	order_node;
	dict_table_t*	order_table;
	ulint		order_col_no;
	plan_t*		plan;
	ulint		i;

	if (!sel_node->order_by) {

		return;
	}

	order_node = sel_node->order_by;
	order_col_no = order_node->column->col_no;
	order_table = order_node->column->table;

	/* If there is an order-by clause, the first non-exactly matched field
	in the index used for the last table in the table list should be the
	column defined in the order-by clause, and for all the other tables
	we should get only at most a single row, otherwise we cannot presently
	calculate the order-by, as we have no sort utility */
		
	for (i = 0; i < sel_node->n_tables; i++) {	

		plan = sel_node_get_nth_plan(sel_node, i);

		if (i < sel_node->n_tables - 1) {
			ut_a(dict_index_get_n_unique(plan->index)
						<= plan->n_exact_match);
		} else {
			ut_a(plan->table == order_table);

			ut_a((dict_index_get_n_unique(plan->index)
						<= plan->n_exact_match)
			     || (dict_index_get_nth_col_no(plan->index,
			     				plan->n_exact_match)
			   			== order_col_no));
		}
	}
}

/***********************************************************************
Optimizes a select. Decides which indexes to tables to use. The tables
are accessed in the order that they were written to the FROM part in the
select statement. */
static
void
opt_search_plan_for_table(
/*======================*/
	sel_node_t*	sel_node,	/* in: parsed select node */
	ulint		i,		/* in: this is the ith table */
	dict_table_t*	table)		/* in: table */
{
	plan_t*		plan;
	dict_index_t*	index;
	dict_index_t*	best_index;
	ulint		n_fields;
	ulint		goodness;
	ulint		last_op		= 75946965;	/* Eliminate a Purify
							warning */
	ulint		best_goodness;
	ulint		best_last_op = 0; /* remove warning */
	ulint		mix_id_pos;
	que_node_t*	index_plan[256];
	que_node_t*	best_index_plan[256];

	plan = sel_node_get_nth_plan(sel_node, i);

	plan->table = table;
	plan->asc = sel_node->asc;
	plan->pcur_is_open = FALSE;
	plan->cursor_at_end = FALSE;

	/* Calculate goodness for each index of the table */

	index = dict_table_get_first_index(table);
	best_index = index; /* Eliminate compiler warning */
	best_goodness = 0;
	
	/* should be do ... until ? comment by Jani */
	while (index) {
		goodness = opt_calc_index_goodness(index, sel_node, i,
						index_plan, &last_op);
		if (goodness > best_goodness) {

			best_index = index;
			best_goodness = goodness;
			n_fields = opt_calc_n_fields_from_goodness(goodness);

			ut_memcpy(best_index_plan, index_plan,
						n_fields * sizeof(void*));
			best_last_op = last_op;
		}

		index = dict_table_get_next_index(index);
	}	

	plan->index = best_index;

	n_fields = opt_calc_n_fields_from_goodness(best_goodness);

	if (n_fields == 0) {
		plan->tuple = NULL;
		plan->n_exact_match = 0;
	} else {
		plan->tuple = dtuple_create(pars_sym_tab_global->heap,
								n_fields);
		dict_index_copy_types(plan->tuple, plan->index, n_fields);
		
		plan->tuple_exps = mem_heap_alloc(pars_sym_tab_global->heap,
						n_fields * sizeof(void*));

		ut_memcpy(plan->tuple_exps, best_index_plan,
				  		n_fields * sizeof(void*));
		if (best_last_op == '=') {
			plan->n_exact_match = n_fields;
		} else {
			plan->n_exact_match = n_fields - 1;
		}
			
		plan->mode = opt_op_to_search_mode(sel_node->asc,
								best_last_op);
	}

	if ((best_index->type & DICT_CLUSTERED)
	    && (plan->n_exact_match >= dict_index_get_n_unique(best_index))) {

		plan->unique_search = TRUE;
	} else {
		plan->unique_search = FALSE;
	}

	if ((table->type != DICT_TABLE_ORDINARY)
	    			&& (best_index->type & DICT_CLUSTERED)) {

	    	plan->mixed_index = TRUE;

	    	mix_id_pos = table->mix_len;

	    	if (mix_id_pos < n_fields) {
	    		/* We have to add the mix id as a (string) literal
			expression to the tuple_exps */

			plan->tuple_exps[mix_id_pos] =
				sym_tab_add_str_lit(pars_sym_tab_global,
							table->mix_id_buf,
							table->mix_id_len);
	    	}
	} else {
		plan->mixed_index = FALSE;
	}
	
	plan->old_vers_heap = NULL;

	btr_pcur_init(&(plan->pcur));
	btr_pcur_init(&(plan->clust_pcur));
}

/***********************************************************************
Looks at a comparison condition and decides if it can, and need, be tested for
a table AFTER the table has been accessed. */
static
ulint
opt_classify_comparison(
/*====================*/
					/* out: OPT_NOT_COND if not for this
					table, else OPT_END_COND,
					OPT_TEST_COND, or OPT_SCROLL_COND,
					where the last means that the
					condition need not be tested, except
					when scroll cursors are used */
	sel_node_t*	sel_node,	/* in: select node */
	ulint		i,		/* in: ith table in the join */
	func_node_t*	cond)		/* in: comparison condition */
{
	plan_t*	plan;
	ulint	n_fields;
	ulint	op;
	ulint	j;

	ut_ad(cond && sel_node);

	plan = sel_node_get_nth_plan(sel_node, i);

	/* Check if the condition is determined after the ith table has been
	accessed, but not after the i - 1:th */

	if (!opt_check_exp_determined_before(cond, sel_node, i + 1)) {

		return(OPT_NOT_COND);
	}

	if ((i > 0) && opt_check_exp_determined_before(cond, sel_node, i)) {

		return(OPT_NOT_COND);
	}

	/* If the condition is an exact match condition used in constructing
	the search tuple, it is classified as OPT_END_COND */

	if (plan->tuple) {
		n_fields = dtuple_get_n_fields(plan->tuple);
	} else {
		n_fields = 0;
	}

	for (j = 0; j < plan->n_exact_match; j++) {

		if (opt_is_arg(plan->tuple_exps[j], cond)) {

			return(OPT_END_COND);
		}
	}

	/* If the condition is an non-exact match condition used in
	constructing the search tuple, it is classified as OPT_SCROLL_COND.
	When the cursor is positioned, and if a non-scroll cursor is used,
	there is no need to test this condition; if a scroll cursor is used
	the testing is necessary when the cursor is reversed. */

	if ((n_fields > plan->n_exact_match)
	    	&& opt_is_arg(plan->tuple_exps[n_fields - 1], cond)) {

	    	return(OPT_SCROLL_COND);
	}

	/* If the condition is a non-exact match condition on the first field
	in index for which there is no exact match, and it limits the search
	range from the opposite side of the search tuple already BEFORE we
	access the table, it is classified as OPT_END_COND */

	if ((dict_index_get_n_fields(plan->index) > plan->n_exact_match)
	    && opt_look_for_col_in_comparison_before(
				OPT_COMPARISON,
	    			dict_index_get_nth_col_no(plan->index,
	    						plan->n_exact_match),
	    			cond, sel_node, i, &op)) {
	    				
		if (sel_node->asc && ((op == '<') || (op == PARS_LE_TOKEN))) {

			return(OPT_END_COND);
		}

		if (!sel_node->asc && ((op == '>') || (op == PARS_GE_TOKEN))) {

			return(OPT_END_COND);
		}
	}

	/* Otherwise, cond is classified as OPT_TEST_COND */

	return(OPT_TEST_COND);
}

/***********************************************************************
Recursively looks for test conditions for a table in a join. */
static
void
opt_find_test_conds(
/*================*/
	sel_node_t*	sel_node,	/* in: select node */
	ulint		i,		/* in: ith table in the join */
	func_node_t*	cond)		/* in: conjunction of search
					conditions or NULL */
{
	func_node_t*	new_cond;
	ulint		class;
	plan_t*		plan;

	if (cond == NULL) {

		return;
	}

	if (cond->func == PARS_AND_TOKEN) {
		new_cond = cond->args;

		opt_find_test_conds(sel_node, i, new_cond);

		new_cond = que_node_get_next(new_cond);
		
		opt_find_test_conds(sel_node, i, new_cond);

		return;
	}

	plan = sel_node_get_nth_plan(sel_node, i);

	class = opt_classify_comparison(sel_node, i, cond);

	if (class == OPT_END_COND) {
		UT_LIST_ADD_LAST(cond_list, plan->end_conds, cond);

	} else if (class == OPT_TEST_COND) {
		UT_LIST_ADD_LAST(cond_list, plan->other_conds, cond);

	}
}

/***********************************************************************
Normalizes a list of comparison conditions so that a column of the table
appears on the left side of the comparison if possible. This is accomplished
by switching the arguments of the operator. */
static
void
opt_normalize_cmp_conds(
/*====================*/
	func_node_t*	cond,	/* in: first in a list of comparison
				conditions, or NULL */
	dict_table_t*	table)	/* in: table */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	sym_node_t*	sym_node;

	while (cond) {
		arg1 = cond->args;
		arg2 = que_node_get_next(arg1);

		if (que_node_get_type(arg2) == QUE_NODE_SYMBOL) {

			sym_node = arg2;

			if ((sym_node->token_type == SYM_COLUMN)
					&& (sym_node->table == table)) {

				/* Switch the order of the arguments */

				cond->args = arg2;
				que_node_list_add_last(NULL, arg2);
				que_node_list_add_last(arg2, arg1);

				/* Invert the operator */
				cond->func = opt_invert_cmp_op(cond->func);
			}
		}

		cond = UT_LIST_GET_NEXT(cond_list, cond);
	}
}	

/***********************************************************************
Finds out the search condition conjuncts we can, and need, to test as the ith
table in a join is accessed. The search tuple can eliminate the need to test
some conjuncts. */
static
void
opt_determine_and_normalize_test_conds(
/*===================================*/
	sel_node_t*	sel_node,	/* in: select node */
	ulint		i)		/* in: ith table in the join */
{
	plan_t*	plan;

	plan = sel_node_get_nth_plan(sel_node, i);

	UT_LIST_INIT(plan->end_conds);
	UT_LIST_INIT(plan->other_conds);
	
	/* Recursively go through the conjuncts and classify them */

	opt_find_test_conds(sel_node, i, sel_node->search_cond);

	opt_normalize_cmp_conds(UT_LIST_GET_FIRST(plan->end_conds),
								plan->table);

	ut_a(UT_LIST_GET_LEN(plan->end_conds) >= plan->n_exact_match);
}

/***********************************************************************
Looks for occurrences of the columns of the table in the query subgraph and
adds them to the list of columns if an occurrence of the same column does not
already exist in the list. If the column is already in the list, puts a value
indirection to point to the occurrence in the column list, except if the
column occurrence we are looking at is in the column list, in which case
nothing is done. */

void
opt_find_all_cols(
/*==============*/
	ibool		copy_val,	/* in: if TRUE, new found columns are
					added as columns to copy */
	dict_index_t*	index,		/* in: index of the table to use */
	sym_node_list_t* col_list,	/* in: base node of a list where
					to add new found columns */
	plan_t*		plan,		/* in: plan or NULL */
	que_node_t*	exp)		/* in: expression or condition or
					NULL */
{
	func_node_t*	func_node;
	que_node_t*	arg;
	sym_node_t*	sym_node;
	sym_node_t*	col_node;
	ulint		col_pos;

	if (exp == NULL) {

		return;
	}
	
	if (que_node_get_type(exp) == QUE_NODE_FUNC) {
		func_node = exp;

		arg = func_node->args;

		while (arg) {
			opt_find_all_cols(copy_val, index, col_list, plan,
									arg);
			arg = que_node_get_next(arg);
		}

		return;
	}

	ut_a(que_node_get_type(exp) == QUE_NODE_SYMBOL);

	sym_node = exp;

	if (sym_node->token_type != SYM_COLUMN) {

		return;
	}

	if (sym_node->table != index->table) {

		return;
	}

	/* Look for an occurrence of the same column in the plan column
	list */

	col_node = UT_LIST_GET_FIRST(*col_list);

	while (col_node) {
		if (col_node->col_no == sym_node->col_no) {

			if (col_node == sym_node) {
				/* sym_node was already in a list: do
				nothing */

				return;
			}

			/* Put an indirection */
			sym_node->indirection = col_node;
			sym_node->alias = col_node;

			return;
		}

		col_node = UT_LIST_GET_NEXT(col_var_list, col_node);
	}

	/* The same column did not occur in the list: add it */

	UT_LIST_ADD_LAST(col_var_list, *col_list, sym_node);

	sym_node->copy_val = copy_val;

	/* Fill in the field_no fields in sym_node */
	
	sym_node->field_nos[SYM_CLUST_FIELD_NO]
				= dict_index_get_nth_col_pos(
				dict_table_get_first_index(index->table),
							sym_node->col_no);
	if (!(index->type & DICT_CLUSTERED)) {	

		ut_a(plan);

		col_pos = dict_index_get_nth_col_pos(index, sym_node->col_no);

		if (col_pos == ULINT_UNDEFINED) {

			plan->must_get_clust = TRUE;
		}
		
		sym_node->field_nos[SYM_SEC_FIELD_NO] = col_pos;
	}
}

/***********************************************************************
Looks for occurrences of the columns of the table in conditions which are
not yet determined AFTER the join operation has fetched a row in the ith
table. The values for these column must be copied to dynamic memory for
later use. */
static
void
opt_find_copy_cols(
/*===============*/
	sel_node_t*	sel_node,	/* in: select node */
	ulint		i,		/* in: ith table in the join */
	func_node_t*	search_cond)	/* in: search condition or NULL */
{
	func_node_t*	new_cond;
	plan_t*		plan;

	if (search_cond == NULL) {

		return;
	}
	
	ut_ad(que_node_get_type(search_cond) == QUE_NODE_FUNC);

	if (search_cond->func == PARS_AND_TOKEN) {
		new_cond = search_cond->args;

		opt_find_copy_cols(sel_node, i, new_cond);
		
		new_cond = que_node_get_next(new_cond);
		
		opt_find_copy_cols(sel_node, i, new_cond);

		return;
	}

	if (!opt_check_exp_determined_before(search_cond, sel_node, i + 1)) {

		/* Any ith table columns occurring in search_cond should be
		copied, as this condition cannot be tested already on the
		fetch from the ith table */

		plan = sel_node_get_nth_plan(sel_node, i);
		
		opt_find_all_cols(TRUE, plan->index, &(plan->columns), plan,
								search_cond);
	}
}

/***********************************************************************
Classifies the table columns according to whether we use the column only while
holding the latch on the page, or whether we have to copy the column value to
dynamic memory. Puts the first occurrence of a column to either list in the
plan node, and puts indirections to later occurrences of the column. */
static
void
opt_classify_cols(
/*==============*/
	sel_node_t*	sel_node,	/* in: select node */
	ulint		i)		/* in: ith table in the join */
{
	plan_t*		plan;
	que_node_t*	exp;

	plan = sel_node_get_nth_plan(sel_node, i);

	/* The final value of the following field will depend on the
	environment of the select statement: */

	plan->must_get_clust = FALSE;

	UT_LIST_INIT(plan->columns);

	/* All select list columns should be copied: therefore TRUE as the
	first argument */

	exp = sel_node->select_list;

	while (exp) {
		opt_find_all_cols(TRUE, plan->index, &(plan->columns), plan,
									exp);
		exp = que_node_get_next(exp);
	}

	opt_find_copy_cols(sel_node, i, sel_node->search_cond);

	/* All remaining columns in the search condition are temporary
	columns: therefore FALSE */
	
	opt_find_all_cols(FALSE, plan->index, &(plan->columns), plan,
						sel_node->search_cond);
}

/***********************************************************************
Fills in the info in plan which is used in accessing a clustered index
record. The columns must already be classified for the plan node. */
static
void
opt_clust_access(
/*=============*/
	sel_node_t*	sel_node,	/* in: select node */
	ulint		n)		/* in: nth table in select */
{
	plan_t*		plan;
	dict_table_t*	table;
	dict_index_t*	clust_index;
	dict_index_t*	index;
	dfield_t*	dfield;
	mem_heap_t*	heap;
	ulint		n_fields;
	ulint		pos;
	ulint		i;

	plan = sel_node_get_nth_plan(sel_node, n);

	index = plan->index;
	
	/* The final value of the following field depends on the environment
	of the select statement: */
		
	plan->no_prefetch = FALSE;

	if (index->type & DICT_CLUSTERED) {
		plan->clust_map = NULL;
		plan->clust_ref = NULL;

		return;
	}

	table = index->table;

	clust_index = dict_table_get_first_index(table);

	n_fields = dict_index_get_n_unique(clust_index);

	heap = pars_sym_tab_global->heap;

	plan->clust_ref = dtuple_create(heap, n_fields);

	dict_index_copy_types(plan->clust_ref, clust_index, n_fields);
	
	plan->clust_map = mem_heap_alloc(heap, n_fields * sizeof(ulint));
	
	for (i = 0; i < n_fields; i++) {
		pos = dict_index_get_nth_field_pos(index, clust_index, i);

		*(plan->clust_map + i) = pos;

		ut_ad((pos != ULINT_UNDEFINED)
			|| ((table->type == DICT_TABLE_CLUSTER_MEMBER)
				 		&& (i == table->mix_len)));
	}

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {
		
		/* Preset the mix id field to the mix id constant */
		
		dfield = dtuple_get_nth_field(plan->clust_ref, table->mix_len);
		
		dfield_set_data(dfield, mem_heap_alloc(heap,
							table->mix_id_len),
							table->mix_id_len);
		ut_memcpy(dfield_get_data(dfield), table->mix_id_buf,
							table->mix_id_len);
	}
}

/***********************************************************************
Optimizes a select. Decides which indexes to tables to use. The tables
are accessed in the order that they were written to the FROM part in the
select statement. */

void
opt_search_plan(
/*============*/
	sel_node_t*	sel_node)	/* in: parsed select node */
{
	sym_node_t*	table_node;
	dict_table_t*	table;
	order_node_t*	order_by;
	ulint		i;
	
	sel_node->plans = mem_heap_alloc(pars_sym_tab_global->heap,
					sel_node->n_tables * sizeof(plan_t));

	/* Analyze the search condition to find out what we know at each
	join stage about the conditions that the columns of a table should
	satisfy */

	table_node = sel_node->table_list;

	if (sel_node->order_by == NULL) {
		sel_node->asc = TRUE;
	} else {
		order_by = sel_node->order_by;

		sel_node->asc = order_by->asc;
	}
	
	for (i = 0; i < sel_node->n_tables; i++) {

		table = table_node->table;

		/* Choose index through which to access the table */
	
		opt_search_plan_for_table(sel_node, i, table);

		/* Determine the search condition conjuncts we can test at
		this table; normalize the end conditions */
	
		opt_determine_and_normalize_test_conds(sel_node, i);

		table_node = que_node_get_next(table_node);
	}

	table_node = sel_node->table_list;

	for (i = 0; i < sel_node->n_tables; i++) {

		/* Classify the table columns into those we only need to access
		but not copy, and to those we must copy to dynamic memory */

		opt_classify_cols(sel_node, i);

		/* Calculate possible info for accessing the clustered index
		record */

		opt_clust_access(sel_node, i);

		table_node = que_node_get_next(table_node);
	}
	
	/* Check that the plan obeys a possible order-by clause: if not,
	an assertion error occurs */
	
	opt_check_order_by(sel_node);

#ifdef UNIV_SQL_DEBUG
	opt_print_query_plan(sel_node);
#endif
}

/************************************************************************
Prints info of a query plan. */

void
opt_print_query_plan(
/*=================*/
	sel_node_t*	sel_node)	/* in: select node */
{
	plan_t*	plan;
	ulint	n_fields;
	ulint	i;

	fputs("QUERY PLAN FOR A SELECT NODE\n", stderr);

	fputs(sel_node->asc ? "Asc. search; " : "Desc. search; ", stderr);

	if (sel_node->set_x_locks) {
		fputs("sets row x-locks; ", stderr);
		ut_a(sel_node->row_lock_mode == LOCK_X);
		ut_a(!sel_node->consistent_read);
	} else if (sel_node->consistent_read) {
		fputs("consistent read; ", stderr);
	} else {
		ut_a(sel_node->row_lock_mode == LOCK_S);
		fputs("sets row s-locks; ", stderr);
	}

	putc('\n', stderr);

	for (i = 0; i < sel_node->n_tables; i++) {
		plan = sel_node_get_nth_plan(sel_node, i);

		if (plan->tuple) {
			n_fields = dtuple_get_n_fields(plan->tuple);
		} else {
			n_fields = 0;
		}

		fputs("Table ", stderr);
		dict_index_name_print(stderr, NULL, plan->index);
		fprintf(stderr,"; exact m. %lu, match %lu, end conds %lu\n",
		        (unsigned long) plan->n_exact_match,
		        (unsigned long) n_fields,
			(unsigned long) UT_LIST_GET_LEN(plan->end_conds));
	}
}
