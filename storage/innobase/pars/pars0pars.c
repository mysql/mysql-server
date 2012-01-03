/*****************************************************************************

Copyright (c) 1996, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file pars/pars0pars.c
SQL parser

Created 11/19/1996 Heikki Tuuri
*******************************************************/

/* Historical note: Innobase executed its first SQL string (CREATE TABLE)
on 1/27/1998 */

#include "pars0pars.h"

#ifdef UNIV_NONINL
#include "pars0pars.ic"
#endif

#include "row0sel.h"
#include "row0ins.h"
#include "row0upd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0crea.h"
#include "que0que.h"
#include "pars0grm.h"
#include "pars0opt.h"
#include "data0data.h"
#include "data0type.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "lock0lock.h"
#include "eval0eval.h"

#ifdef UNIV_SQL_DEBUG
/** If the following is set TRUE, the lexer will print the SQL string
as it tokenizes it */
UNIV_INTERN ibool	pars_print_lexed	= FALSE;
#endif /* UNIV_SQL_DEBUG */

/* Global variable used while parsing a single procedure or query : the code is
NOT re-entrant */
UNIV_INTERN sym_tab_t*	pars_sym_tab_global;

/* Global variables used to denote certain reserved words, used in
constructing the parsing tree */

UNIV_INTERN pars_res_word_t	pars_to_char_token = {PARS_TO_CHAR_TOKEN};
UNIV_INTERN pars_res_word_t	pars_to_number_token = {PARS_TO_NUMBER_TOKEN};
UNIV_INTERN pars_res_word_t	pars_to_binary_token = {PARS_TO_BINARY_TOKEN};
UNIV_INTERN pars_res_word_t	pars_binary_to_number_token = {PARS_BINARY_TO_NUMBER_TOKEN};
UNIV_INTERN pars_res_word_t	pars_substr_token = {PARS_SUBSTR_TOKEN};
UNIV_INTERN pars_res_word_t	pars_replstr_token = {PARS_REPLSTR_TOKEN};
UNIV_INTERN pars_res_word_t	pars_concat_token = {PARS_CONCAT_TOKEN};
UNIV_INTERN pars_res_word_t	pars_instr_token = {PARS_INSTR_TOKEN};
UNIV_INTERN pars_res_word_t	pars_length_token = {PARS_LENGTH_TOKEN};
UNIV_INTERN pars_res_word_t	pars_sysdate_token = {PARS_SYSDATE_TOKEN};
UNIV_INTERN pars_res_word_t	pars_printf_token = {PARS_PRINTF_TOKEN};
UNIV_INTERN pars_res_word_t	pars_assert_token = {PARS_ASSERT_TOKEN};
UNIV_INTERN pars_res_word_t	pars_rnd_token = {PARS_RND_TOKEN};
UNIV_INTERN pars_res_word_t	pars_rnd_str_token = {PARS_RND_STR_TOKEN};
UNIV_INTERN pars_res_word_t	pars_count_token = {PARS_COUNT_TOKEN};
UNIV_INTERN pars_res_word_t	pars_sum_token = {PARS_SUM_TOKEN};
UNIV_INTERN pars_res_word_t	pars_distinct_token = {PARS_DISTINCT_TOKEN};
UNIV_INTERN pars_res_word_t	pars_binary_token = {PARS_BINARY_TOKEN};
UNIV_INTERN pars_res_word_t	pars_blob_token = {PARS_BLOB_TOKEN};
UNIV_INTERN pars_res_word_t	pars_int_token = {PARS_INT_TOKEN};
UNIV_INTERN pars_res_word_t	pars_char_token = {PARS_CHAR_TOKEN};
UNIV_INTERN pars_res_word_t	pars_float_token = {PARS_FLOAT_TOKEN};
UNIV_INTERN pars_res_word_t	pars_update_token = {PARS_UPDATE_TOKEN};
UNIV_INTERN pars_res_word_t	pars_asc_token = {PARS_ASC_TOKEN};
UNIV_INTERN pars_res_word_t	pars_desc_token = {PARS_DESC_TOKEN};
UNIV_INTERN pars_res_word_t	pars_open_token = {PARS_OPEN_TOKEN};
UNIV_INTERN pars_res_word_t	pars_close_token = {PARS_CLOSE_TOKEN};
UNIV_INTERN pars_res_word_t	pars_share_token = {PARS_SHARE_TOKEN};
UNIV_INTERN pars_res_word_t	pars_unique_token = {PARS_UNIQUE_TOKEN};
UNIV_INTERN pars_res_word_t	pars_clustered_token = {PARS_CLUSTERED_TOKEN};

/** Global variable used to denote the '*' in SELECT * FROM.. */
UNIV_INTERN ulint	pars_star_denoter	= 12345678;


/*********************************************************************//**
Determines the class of a function code.
@return	function class: PARS_FUNC_ARITH, ... */
static
ulint
pars_func_get_class(
/*================*/
	int	func)	/*!< in: function code: '=', PARS_GE_TOKEN, ... */
{
	switch (func) {
	case '+': case '-': case '*': case '/':
		return(PARS_FUNC_ARITH);

	case '=': case '<': case '>':
	case PARS_GE_TOKEN: case PARS_LE_TOKEN: case PARS_NE_TOKEN:
		return(PARS_FUNC_CMP);

	case PARS_AND_TOKEN: case PARS_OR_TOKEN: case PARS_NOT_TOKEN:
		return(PARS_FUNC_LOGICAL);

	case PARS_COUNT_TOKEN: case PARS_SUM_TOKEN:
		return(PARS_FUNC_AGGREGATE);

	case PARS_TO_CHAR_TOKEN:
	case PARS_TO_NUMBER_TOKEN:
	case PARS_TO_BINARY_TOKEN:
	case PARS_BINARY_TO_NUMBER_TOKEN:
	case PARS_SUBSTR_TOKEN:
	case PARS_CONCAT_TOKEN:
	case PARS_LENGTH_TOKEN:
	case PARS_INSTR_TOKEN:
	case PARS_SYSDATE_TOKEN:
	case PARS_NOTFOUND_TOKEN:
	case PARS_PRINTF_TOKEN:
	case PARS_ASSERT_TOKEN:
	case PARS_RND_TOKEN:
	case PARS_RND_STR_TOKEN:
	case PARS_REPLSTR_TOKEN:
		return(PARS_FUNC_PREDEFINED);

	default:
		return(PARS_FUNC_OTHER);
	}
}

/*********************************************************************//**
Parses an operator or predefined function expression.
@return	own: function node in a query tree */
static
func_node_t*
pars_func_low(
/*==========*/
	int		func,	/*!< in: function token code */
	que_node_t*	arg)	/*!< in: first argument in the argument list */
{
	func_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(func_node_t));

	node->common.type = QUE_NODE_FUNC;
	dfield_set_data(&(node->common.val), NULL, 0);
	node->common.val_buf_size = 0;

	node->func = func;

	node->class = pars_func_get_class(func);

	node->args = arg;

	UT_LIST_ADD_LAST(func_node_list, pars_sym_tab_global->func_node_list,
			 node);
	return(node);
}

/*********************************************************************//**
Parses a function expression.
@return	own: function node in a query tree */
UNIV_INTERN
func_node_t*
pars_func(
/*======*/
	que_node_t*	res_word,/*!< in: function name reserved word */
	que_node_t*	arg)	/*!< in: first argument in the argument list */
{
	return(pars_func_low(((pars_res_word_t*)res_word)->code, arg));
}

/*********************************************************************//**
Parses an operator expression.
@return	own: function node in a query tree */
UNIV_INTERN
func_node_t*
pars_op(
/*====*/
	int		func,	/*!< in: operator token code */
	que_node_t*	arg1,	/*!< in: first argument */
	que_node_t*	arg2)	/*!< in: second argument or NULL for an unary
				operator */
{
	que_node_list_add_last(NULL, arg1);

	if (arg2) {
		que_node_list_add_last(arg1, arg2);
	}

	return(pars_func_low(func, arg1));
}

/*********************************************************************//**
Parses an ORDER BY clause. Order by a single column only is supported.
@return	own: order-by node in a query tree */
UNIV_INTERN
order_node_t*
pars_order_by(
/*==========*/
	sym_node_t*	column,	/*!< in: column name */
	pars_res_word_t* asc)	/*!< in: &pars_asc_token or pars_desc_token */
{
	order_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(order_node_t));

	node->common.type = QUE_NODE_ORDER;

	node->column = column;

	if (asc == &pars_asc_token) {
		node->asc = TRUE;
	} else {
		ut_a(asc == &pars_desc_token);
		node->asc = FALSE;
	}

	return(node);
}

/*********************************************************************//**
Determine if a data type is a built-in string data type of the InnoDB
SQL parser.
@return	TRUE if string data type */
static
ibool
pars_is_string_type(
/*================*/
	ulint	mtype)	/*!< in: main data type */
{
	switch (mtype) {
	case DATA_VARCHAR: case DATA_CHAR:
	case DATA_FIXBINARY: case DATA_BINARY:
		return(TRUE);
	}

	return(FALSE);
}

/*********************************************************************//**
Resolves the data type of a function in an expression. The argument data
types must already be resolved. */
static
void
pars_resolve_func_data_type(
/*========================*/
	func_node_t*	node)	/*!< in: function node */
{
	que_node_t*	arg;

	ut_a(que_node_get_type(node) == QUE_NODE_FUNC);

	arg = node->args;

	switch (node->func) {
	case PARS_SUM_TOKEN:
	case '+': case '-': case '*': case '/':
		/* Inherit the data type from the first argument (which must
		not be the SQL null literal whose type is DATA_ERROR) */

		dtype_copy(que_node_get_data_type(node),
			   que_node_get_data_type(arg));

		ut_a(dtype_get_mtype(que_node_get_data_type(node))
		     == DATA_INT);
		break;

	case PARS_COUNT_TOKEN:
		ut_a(arg);
		dtype_set(que_node_get_data_type(node), DATA_INT, 0, 4);
		break;

	case PARS_TO_CHAR_TOKEN:
	case PARS_RND_STR_TOKEN:
		ut_a(dtype_get_mtype(que_node_get_data_type(arg)) == DATA_INT);
		dtype_set(que_node_get_data_type(node), DATA_VARCHAR,
			  DATA_ENGLISH, 0);
		break;

	case PARS_TO_BINARY_TOKEN:
		if (dtype_get_mtype(que_node_get_data_type(arg)) == DATA_INT) {
			dtype_set(que_node_get_data_type(node), DATA_VARCHAR,
				  DATA_ENGLISH, 0);
		} else {
			dtype_set(que_node_get_data_type(node), DATA_BINARY,
				  0, 0);
		}
		break;

	case PARS_TO_NUMBER_TOKEN:
	case PARS_BINARY_TO_NUMBER_TOKEN:
	case PARS_LENGTH_TOKEN:
	case PARS_INSTR_TOKEN:
		ut_a(pars_is_string_type(que_node_get_data_type(arg)->mtype));
		dtype_set(que_node_get_data_type(node), DATA_INT, 0, 4);
		break;

	case PARS_SYSDATE_TOKEN:
		ut_a(arg == NULL);
		dtype_set(que_node_get_data_type(node), DATA_INT, 0, 4);
		break;

	case PARS_SUBSTR_TOKEN:
	case PARS_CONCAT_TOKEN:
		ut_a(pars_is_string_type(que_node_get_data_type(arg)->mtype));
		dtype_set(que_node_get_data_type(node), DATA_VARCHAR,
			  DATA_ENGLISH, 0);
		break;

	case '>': case '<': case '=':
	case PARS_GE_TOKEN:
	case PARS_LE_TOKEN:
	case PARS_NE_TOKEN:
	case PARS_AND_TOKEN:
	case PARS_OR_TOKEN:
	case PARS_NOT_TOKEN:
	case PARS_NOTFOUND_TOKEN:

		/* We currently have no iboolean type: use integer type */
		dtype_set(que_node_get_data_type(node), DATA_INT, 0, 4);
		break;

	case PARS_RND_TOKEN:
		ut_a(dtype_get_mtype(que_node_get_data_type(arg)) == DATA_INT);
		dtype_set(que_node_get_data_type(node), DATA_INT, 0, 4);
		break;

	default:
		ut_error;
	}
}

/*********************************************************************//**
Resolves the meaning of variables in an expression and the data types of
functions. It is an error if some identifier cannot be resolved here. */
static
void
pars_resolve_exp_variables_and_types(
/*=================================*/
	sel_node_t*	select_node,	/*!< in: select node or NULL; if
					this is not NULL then the variable
					sym nodes are added to the
					copy_variables list of select_node */
	que_node_t*	exp_node)	/*!< in: expression */
{
	func_node_t*	func_node;
	que_node_t*	arg;
	sym_node_t*	sym_node;
	sym_node_t*	node;

	ut_a(exp_node);

	if (que_node_get_type(exp_node) == QUE_NODE_FUNC) {
		func_node = exp_node;

		arg = func_node->args;

		while (arg) {
			pars_resolve_exp_variables_and_types(select_node, arg);

			arg = que_node_get_next(arg);
		}

		pars_resolve_func_data_type(func_node);

		return;
	}

	ut_a(que_node_get_type(exp_node) == QUE_NODE_SYMBOL);

	sym_node = exp_node;

	if (sym_node->resolved) {

		return;
	}

	/* Not resolved yet: look in the symbol table for a variable
	or a cursor or a function with the same name */

	node = UT_LIST_GET_FIRST(pars_sym_tab_global->sym_list);

	while (node) {
		if (node->resolved
		    && ((node->token_type == SYM_VAR)
			|| (node->token_type == SYM_CURSOR)
			|| (node->token_type == SYM_FUNCTION))
		    && node->name
		    && (sym_node->name_len == node->name_len)
		    && (ut_memcmp(sym_node->name, node->name,
				  node->name_len) == 0)) {

			/* Found a variable or a cursor declared with
			the same name */

			break;
		}

		node = UT_LIST_GET_NEXT(sym_list, node);
	}

	if (!node) {
		fprintf(stderr, "PARSER ERROR: Unresolved identifier %s\n",
			sym_node->name);
	}

	ut_a(node);

	sym_node->resolved = TRUE;
	sym_node->token_type = SYM_IMPLICIT_VAR;
	sym_node->alias = node;
	sym_node->indirection = node;

	if (select_node) {
		UT_LIST_ADD_LAST(col_var_list, select_node->copy_variables,
				 sym_node);
	}

	dfield_set_type(que_node_get_val(sym_node),
			que_node_get_data_type(node));
}

/*********************************************************************//**
Resolves the meaning of variables in an expression list. It is an error if
some identifier cannot be resolved here. Resolves also the data types of
functions. */
static
void
pars_resolve_exp_list_variables_and_types(
/*======================================*/
	sel_node_t*	select_node,	/*!< in: select node or NULL */
	que_node_t*	exp_node)	/*!< in: expression list first node, or
					NULL */
{
	while (exp_node) {
		pars_resolve_exp_variables_and_types(select_node, exp_node);

		exp_node = que_node_get_next(exp_node);
	}
}

/*********************************************************************//**
Resolves the columns in an expression. */
static
void
pars_resolve_exp_columns(
/*=====================*/
	sym_node_t*	table_node,	/*!< in: first node in a table list */
	que_node_t*	exp_node)	/*!< in: expression */
{
	func_node_t*	func_node;
	que_node_t*	arg;
	sym_node_t*	sym_node;
	dict_table_t*	table;
	sym_node_t*	t_node;
	ulint		n_cols;
	ulint		i;

	ut_a(exp_node);

	if (que_node_get_type(exp_node) == QUE_NODE_FUNC) {
		func_node = exp_node;

		arg = func_node->args;

		while (arg) {
			pars_resolve_exp_columns(table_node, arg);

			arg = que_node_get_next(arg);
		}

		return;
	}

	ut_a(que_node_get_type(exp_node) == QUE_NODE_SYMBOL);

	sym_node = exp_node;

	if (sym_node->resolved) {

		return;
	}

	/* Not resolved yet: look in the table list for a column with the
	same name */

	t_node = table_node;

	while (t_node) {
		table = t_node->table;

		n_cols = dict_table_get_n_cols(table);

		for (i = 0; i < n_cols; i++) {
			const dict_col_t*	col
				= dict_table_get_nth_col(table, i);
			const char*		col_name
				= dict_table_get_col_name(table, i);

			if ((sym_node->name_len == ut_strlen(col_name))
			    && (0 == ut_memcmp(sym_node->name, col_name,
					       sym_node->name_len))) {
				/* Found */
				sym_node->resolved = TRUE;
				sym_node->token_type = SYM_COLUMN;
				sym_node->table = table;
				sym_node->col_no = i;
				sym_node->prefetch_buf = NULL;

				dict_col_copy_type(
					col,
					dfield_get_type(&sym_node
							->common.val));

				return;
			}
		}

		t_node = que_node_get_next(t_node);
	}
}

/*********************************************************************//**
Resolves the meaning of columns in an expression list. */
static
void
pars_resolve_exp_list_columns(
/*==========================*/
	sym_node_t*	table_node,	/*!< in: first node in a table list */
	que_node_t*	exp_node)	/*!< in: expression list first node, or
					NULL */
{
	while (exp_node) {
		pars_resolve_exp_columns(table_node, exp_node);

		exp_node = que_node_get_next(exp_node);
	}
}

/*********************************************************************//**
Retrieves the table definition for a table name id. */
static
void
pars_retrieve_table_def(
/*====================*/
	sym_node_t*	sym_node)	/*!< in: table node */
{
	const char*	table_name;

	ut_a(sym_node);
	ut_a(que_node_get_type(sym_node) == QUE_NODE_SYMBOL);

	sym_node->resolved = TRUE;
	sym_node->token_type = SYM_TABLE;

	table_name = (const char*) sym_node->name;

	sym_node->table = dict_table_get_low(table_name);

	ut_a(sym_node->table);
}

/*********************************************************************//**
Retrieves the table definitions for a list of table name ids.
@return	number of tables */
static
ulint
pars_retrieve_table_list_defs(
/*==========================*/
	sym_node_t*	sym_node)	/*!< in: first table node in list */
{
	ulint		count		= 0;

	if (sym_node == NULL) {

		return(count);
	}

	while (sym_node) {
		pars_retrieve_table_def(sym_node);

		count++;

		sym_node = que_node_get_next(sym_node);
	}

	return(count);
}

/*********************************************************************//**
Adds all columns to the select list if the query is SELECT * FROM ... */
static
void
pars_select_all_columns(
/*====================*/
	sel_node_t*	select_node)	/*!< in: select node already containing
					the table list */
{
	sym_node_t*	col_node;
	sym_node_t*	table_node;
	dict_table_t*	table;
	ulint		i;

	select_node->select_list = NULL;

	table_node = select_node->table_list;

	while (table_node) {
		table = table_node->table;

		for (i = 0; i < dict_table_get_n_user_cols(table); i++) {
			const char*	col_name = dict_table_get_col_name(
				table, i);

			col_node = sym_tab_add_id(pars_sym_tab_global,
						  (byte*)col_name,
						  ut_strlen(col_name));

			select_node->select_list = que_node_list_add_last(
				select_node->select_list, col_node);
		}

		table_node = que_node_get_next(table_node);
	}
}

/*********************************************************************//**
Parses a select list; creates a query graph node for the whole SELECT
statement.
@return	own: select node in a query tree */
UNIV_INTERN
sel_node_t*
pars_select_list(
/*=============*/
	que_node_t*	select_list,	/*!< in: select list */
	sym_node_t*	into_list)	/*!< in: variables list or NULL */
{
	sel_node_t*	node;

	node = sel_node_create(pars_sym_tab_global->heap);

	node->select_list = select_list;
	node->into_list = into_list;

	pars_resolve_exp_list_variables_and_types(NULL, into_list);

	return(node);
}

/*********************************************************************//**
Checks if the query is an aggregate query, in which case the selct list must
contain only aggregate function items. */
static
void
pars_check_aggregate(
/*=================*/
	sel_node_t*	select_node)	/*!< in: select node already containing
					the select list */
{
	que_node_t*	exp_node;
	func_node_t*	func_node;
	ulint		n_nodes			= 0;
	ulint		n_aggregate_nodes	= 0;

	exp_node = select_node->select_list;

	while (exp_node) {

		n_nodes++;

		if (que_node_get_type(exp_node) == QUE_NODE_FUNC) {

			func_node = exp_node;

			if (func_node->class == PARS_FUNC_AGGREGATE) {

				n_aggregate_nodes++;
			}
		}

		exp_node = que_node_get_next(exp_node);
	}

	if (n_aggregate_nodes > 0) {
		ut_a(n_nodes == n_aggregate_nodes);

		select_node->is_aggregate = TRUE;
	} else {
		select_node->is_aggregate = FALSE;
	}
}

/*********************************************************************//**
Parses a select statement.
@return	own: select node in a query tree */
UNIV_INTERN
sel_node_t*
pars_select_statement(
/*==================*/
	sel_node_t*	select_node,	/*!< in: select node already containing
					the select list */
	sym_node_t*	table_list,	/*!< in: table list */
	que_node_t*	search_cond,	/*!< in: search condition or NULL */
	pars_res_word_t* for_update,	/*!< in: NULL or &pars_update_token */
	pars_res_word_t* lock_shared,	/*!< in: NULL or &pars_share_token */
	order_node_t*	order_by)	/*!< in: NULL or an order-by node */
{
	select_node->state = SEL_NODE_OPEN;

	select_node->table_list = table_list;
	select_node->n_tables = pars_retrieve_table_list_defs(table_list);

	if (select_node->select_list == &pars_star_denoter) {

		/* SELECT * FROM ... */
		pars_select_all_columns(select_node);
	}

	if (select_node->into_list) {
		ut_a(que_node_list_get_len(select_node->into_list)
		     == que_node_list_get_len(select_node->select_list));
	}

	UT_LIST_INIT(select_node->copy_variables);

	pars_resolve_exp_list_columns(table_list, select_node->select_list);
	pars_resolve_exp_list_variables_and_types(select_node,
						  select_node->select_list);
	pars_check_aggregate(select_node);

	select_node->search_cond = search_cond;

	if (search_cond) {
		pars_resolve_exp_columns(table_list, search_cond);
		pars_resolve_exp_variables_and_types(select_node, search_cond);
	}

	if (for_update) {
		ut_a(!lock_shared);

		select_node->set_x_locks = TRUE;
		select_node->row_lock_mode = LOCK_X;

		select_node->consistent_read = FALSE;
		select_node->read_view = NULL;
	} else if (lock_shared){
		select_node->set_x_locks = FALSE;
		select_node->row_lock_mode = LOCK_S;

		select_node->consistent_read = FALSE;
		select_node->read_view = NULL;
	} else {
		select_node->set_x_locks = FALSE;
		select_node->row_lock_mode = LOCK_S;

		select_node->consistent_read = TRUE;
	}

	select_node->order_by = order_by;

	if (order_by) {
		pars_resolve_exp_columns(table_list, order_by->column);
	}

	/* The final value of the following fields depend on the environment
	where the select statement appears: */

	select_node->can_get_updated = FALSE;
	select_node->explicit_cursor = NULL;

	opt_search_plan(select_node);

	return(select_node);
}

/*********************************************************************//**
Parses a cursor declaration.
@return	sym_node */
UNIV_INTERN
que_node_t*
pars_cursor_declaration(
/*====================*/
	sym_node_t*	sym_node,	/*!< in: cursor id node in the symbol
					table */
	sel_node_t*	select_node)	/*!< in: select node */
{
	sym_node->resolved = TRUE;
	sym_node->token_type = SYM_CURSOR;
	sym_node->cursor_def = select_node;

	select_node->state = SEL_NODE_CLOSED;
	select_node->explicit_cursor = sym_node;

	return(sym_node);
}

/*********************************************************************//**
Parses a function declaration.
@return	sym_node */
UNIV_INTERN
que_node_t*
pars_function_declaration(
/*======================*/
	sym_node_t*	sym_node)	/*!< in: function id node in the symbol
					table */
{
	sym_node->resolved = TRUE;
	sym_node->token_type = SYM_FUNCTION;

	/* Check that the function exists. */
	ut_a(pars_info_get_user_func(pars_sym_tab_global->info,
				     sym_node->name));

	return(sym_node);
}

/*********************************************************************//**
Parses a delete or update statement start.
@return	own: update node in a query tree */
UNIV_INTERN
upd_node_t*
pars_update_statement_start(
/*========================*/
	ibool		is_delete,	/*!< in: TRUE if delete */
	sym_node_t*	table_sym,	/*!< in: table name node */
	col_assign_node_t* col_assign_list)/*!< in: column assignment list, NULL
					if delete */
{
	upd_node_t*	node;

	node = upd_node_create(pars_sym_tab_global->heap);

	node->is_delete = is_delete;

	node->table_sym = table_sym;
	node->col_assign_list = col_assign_list;

	return(node);
}

/*********************************************************************//**
Parses a column assignment in an update.
@return	column assignment node */
UNIV_INTERN
col_assign_node_t*
pars_column_assignment(
/*===================*/
	sym_node_t*	column,	/*!< in: column to assign */
	que_node_t*	exp)	/*!< in: value to assign */
{
	col_assign_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap,
			      sizeof(col_assign_node_t));
	node->common.type = QUE_NODE_COL_ASSIGNMENT;

	node->col = column;
	node->val = exp;

	return(node);
}

/*********************************************************************//**
Processes an update node assignment list. */
static
void
pars_process_assign_list(
/*=====================*/
	upd_node_t*	node)	/*!< in: update node */
{
	col_assign_node_t*	col_assign_list;
	sym_node_t*		table_sym;
	col_assign_node_t*	assign_node;
	upd_field_t*		upd_field;
	dict_index_t*		clust_index;
	sym_node_t*		col_sym;
	ulint			changes_ord_field;
	ulint			changes_field_size;
	ulint			n_assigns;
	ulint			i;

	table_sym = node->table_sym;
	col_assign_list = node->col_assign_list;
	clust_index = dict_table_get_first_index(node->table);

	assign_node = col_assign_list;
	n_assigns = 0;

	while (assign_node) {
		pars_resolve_exp_columns(table_sym, assign_node->col);
		pars_resolve_exp_columns(table_sym, assign_node->val);
		pars_resolve_exp_variables_and_types(NULL, assign_node->val);
#if 0
		ut_a(dtype_get_mtype(
			     dfield_get_type(que_node_get_val(
						     assign_node->col)))
		     == dtype_get_mtype(
			     dfield_get_type(que_node_get_val(
						     assign_node->val))));
#endif

		/* Add to the update node all the columns found in assignment
		values as columns to copy: therefore, TRUE */

		opt_find_all_cols(TRUE, clust_index, &(node->columns), NULL,
				  assign_node->val);
		n_assigns++;

		assign_node = que_node_get_next(assign_node);
	}

	node->update = upd_create(n_assigns, pars_sym_tab_global->heap);

	assign_node = col_assign_list;

	changes_field_size = UPD_NODE_NO_SIZE_CHANGE;

	for (i = 0; i < n_assigns; i++) {
		upd_field = upd_get_nth_field(node->update, i);

		col_sym = assign_node->col;

		upd_field_set_field_no(upd_field, dict_index_get_nth_col_pos(
					       clust_index, col_sym->col_no),
				       clust_index, NULL);
		upd_field->exp = assign_node->val;

		if (!dict_col_get_fixed_size(
			    dict_index_get_nth_col(clust_index,
						   upd_field->field_no),
			    dict_table_is_comp(node->table))) {
			changes_field_size = 0;
		}

		assign_node = que_node_get_next(assign_node);
	}

	/* Find out if the update can modify an ordering field in any index */

	changes_ord_field = UPD_NODE_NO_ORD_CHANGE;

	if (row_upd_changes_some_index_ord_field_binary(node->table,
							node->update)) {
		changes_ord_field = 0;
	}

	node->cmpl_info = changes_ord_field | changes_field_size;
}

/*********************************************************************//**
Parses an update or delete statement.
@return	own: update node in a query tree */
UNIV_INTERN
upd_node_t*
pars_update_statement(
/*==================*/
	upd_node_t*	node,		/*!< in: update node */
	sym_node_t*	cursor_sym,	/*!< in: pointer to a cursor entry in
					the symbol table or NULL */
	que_node_t*	search_cond)	/*!< in: search condition or NULL */
{
	sym_node_t*	table_sym;
	sel_node_t*	sel_node;
	plan_t*		plan;

	table_sym = node->table_sym;

	pars_retrieve_table_def(table_sym);
	node->table = table_sym->table;

	UT_LIST_INIT(node->columns);

	/* Make the single table node into a list of table nodes of length 1 */

	que_node_list_add_last(NULL, table_sym);

	if (cursor_sym) {
		pars_resolve_exp_variables_and_types(NULL, cursor_sym);

		sel_node = cursor_sym->alias->cursor_def;

		node->searched_update = FALSE;
	} else {
		sel_node = pars_select_list(NULL, NULL);

		pars_select_statement(sel_node, table_sym, search_cond, NULL,
				      &pars_share_token, NULL);
		node->searched_update = TRUE;
		sel_node->common.parent = node;
	}

	node->select = sel_node;

	ut_a(!node->is_delete || (node->col_assign_list == NULL));
	ut_a(node->is_delete || (node->col_assign_list != NULL));

	if (node->is_delete) {
		node->cmpl_info = 0;
	} else {
		pars_process_assign_list(node);
	}

	if (node->searched_update) {
		node->has_clust_rec_x_lock = TRUE;
		sel_node->set_x_locks = TRUE;
		sel_node->row_lock_mode = LOCK_X;
	} else {
		node->has_clust_rec_x_lock = sel_node->set_x_locks;
	}

	ut_a(sel_node->n_tables == 1);
	ut_a(sel_node->consistent_read == FALSE);
	ut_a(sel_node->order_by == NULL);
	ut_a(sel_node->is_aggregate == FALSE);

	sel_node->can_get_updated = TRUE;

	node->state = UPD_NODE_UPDATE_CLUSTERED;

	plan = sel_node_get_nth_plan(sel_node, 0);

	plan->no_prefetch = TRUE;

	if (!dict_index_is_clust(plan->index)) {

		plan->must_get_clust = TRUE;

		node->pcur = &(plan->clust_pcur);
	} else {
		node->pcur = &(plan->pcur);
	}

	return(node);
}

/*********************************************************************//**
Parses an insert statement.
@return	own: update node in a query tree */
UNIV_INTERN
ins_node_t*
pars_insert_statement(
/*==================*/
	sym_node_t*	table_sym,	/*!< in: table name node */
	que_node_t*	values_list,	/*!< in: value expression list or NULL */
	sel_node_t*	select)		/*!< in: select condition or NULL */
{
	ins_node_t*	node;
	dtuple_t*	row;
	ulint		ins_type;

	ut_a(values_list || select);
	ut_a(!values_list || !select);

	if (values_list) {
		ins_type = INS_VALUES;
	} else {
		ins_type = INS_SEARCHED;
	}

	pars_retrieve_table_def(table_sym);

	node = ins_node_create(ins_type, table_sym->table,
			       pars_sym_tab_global->heap);

	row = dtuple_create(pars_sym_tab_global->heap,
			    dict_table_get_n_cols(node->table));

	dict_table_copy_types(row, table_sym->table);

	ins_node_set_new_row(node, row);

	node->select = select;

	if (select) {
		select->common.parent = node;

		ut_a(que_node_list_get_len(select->select_list)
		     == dict_table_get_n_user_cols(table_sym->table));
	}

	node->values_list = values_list;

	if (node->values_list) {
		pars_resolve_exp_list_variables_and_types(NULL, values_list);

		ut_a(que_node_list_get_len(values_list)
		     == dict_table_get_n_user_cols(table_sym->table));
	}

	return(node);
}

/*********************************************************************//**
Set the type of a dfield. */
static
void
pars_set_dfield_type(
/*=================*/
	dfield_t*		dfield,		/*!< in: dfield */
	pars_res_word_t*	type,		/*!< in: pointer to a type
						token */
	ulint			len,		/*!< in: length, or 0 */
	ibool			is_unsigned,	/*!< in: if TRUE, column is
						UNSIGNED. */
	ibool			is_not_null)	/*!< in: if TRUE, column is
						NOT NULL. */
{
	ulint flags = 0;

	if (is_not_null) {
		flags |= DATA_NOT_NULL;
	}

	if (is_unsigned) {
		flags |= DATA_UNSIGNED;
	}

	if (type == &pars_int_token) {
		ut_a(len == 0);

		dtype_set(dfield_get_type(dfield), DATA_INT, flags, 4);

	} else if (type == &pars_char_token) {
		ut_a(len == 0);

		dtype_set(dfield_get_type(dfield), DATA_VARCHAR,
			  DATA_ENGLISH | flags, 0);
	} else if (type == &pars_binary_token) {
		ut_a(len != 0);

		dtype_set(dfield_get_type(dfield), DATA_FIXBINARY,
			  DATA_BINARY_TYPE | flags, len);
	} else if (type == &pars_blob_token) {
		ut_a(len == 0);

		dtype_set(dfield_get_type(dfield), DATA_BLOB,
			  DATA_BINARY_TYPE | flags, 0);
	} else {
		ut_error;
	}
}

/*********************************************************************//**
Parses a variable declaration.
@return	own: symbol table node of type SYM_VAR */
UNIV_INTERN
sym_node_t*
pars_variable_declaration(
/*======================*/
	sym_node_t*	node,	/*!< in: symbol table node allocated for the
				id of the variable */
	pars_res_word_t* type)	/*!< in: pointer to a type token */
{
	node->resolved = TRUE;
	node->token_type = SYM_VAR;

	node->param_type = PARS_NOT_PARAM;

	pars_set_dfield_type(que_node_get_val(node), type, 0, FALSE, FALSE);

	return(node);
}

/*********************************************************************//**
Parses a procedure parameter declaration.
@return	own: symbol table node of type SYM_VAR */
UNIV_INTERN
sym_node_t*
pars_parameter_declaration(
/*=======================*/
	sym_node_t*	node,	/*!< in: symbol table node allocated for the
				id of the parameter */
	ulint		param_type,
				/*!< in: PARS_INPUT or PARS_OUTPUT */
	pars_res_word_t* type)	/*!< in: pointer to a type token */
{
	ut_a((param_type == PARS_INPUT) || (param_type == PARS_OUTPUT));

	pars_variable_declaration(node, type);

	node->param_type = param_type;

	return(node);
}

/*********************************************************************//**
Sets the parent field in a query node list. */
static
void
pars_set_parent_in_list(
/*====================*/
	que_node_t*	node_list,	/*!< in: first node in a list */
	que_node_t*	parent)		/*!< in: parent value to set in all
					nodes of the list */
{
	que_common_t*	common;

	common = node_list;

	while (common) {
		common->parent = parent;

		common = que_node_get_next(common);
	}
}

/*********************************************************************//**
Parses an elsif element.
@return	elsif node */
UNIV_INTERN
elsif_node_t*
pars_elsif_element(
/*===============*/
	que_node_t*	cond,		/*!< in: if-condition */
	que_node_t*	stat_list)	/*!< in: statement list */
{
	elsif_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(elsif_node_t));

	node->common.type = QUE_NODE_ELSIF;

	node->cond = cond;

	pars_resolve_exp_variables_and_types(NULL, cond);

	node->stat_list = stat_list;

	return(node);
}

/*********************************************************************//**
Parses an if-statement.
@return	if-statement node */
UNIV_INTERN
if_node_t*
pars_if_statement(
/*==============*/
	que_node_t*	cond,		/*!< in: if-condition */
	que_node_t*	stat_list,	/*!< in: statement list */
	que_node_t*	else_part)	/*!< in: else-part statement list
					or elsif element list */
{
	if_node_t*	node;
	elsif_node_t*	elsif_node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(if_node_t));

	node->common.type = QUE_NODE_IF;

	node->cond = cond;

	pars_resolve_exp_variables_and_types(NULL, cond);

	node->stat_list = stat_list;

	if (else_part && (que_node_get_type(else_part) == QUE_NODE_ELSIF)) {

		/* There is a list of elsif conditions */

		node->else_part = NULL;
		node->elsif_list = else_part;

		elsif_node = else_part;

		while (elsif_node) {
			pars_set_parent_in_list(elsif_node->stat_list, node);

			elsif_node = que_node_get_next(elsif_node);
		}
	} else {
		node->else_part = else_part;
		node->elsif_list = NULL;

		pars_set_parent_in_list(else_part, node);
	}

	pars_set_parent_in_list(stat_list, node);

	return(node);
}

/*********************************************************************//**
Parses a while-statement.
@return	while-statement node */
UNIV_INTERN
while_node_t*
pars_while_statement(
/*=================*/
	que_node_t*	cond,		/*!< in: while-condition */
	que_node_t*	stat_list)	/*!< in: statement list */
{
	while_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(while_node_t));

	node->common.type = QUE_NODE_WHILE;

	node->cond = cond;

	pars_resolve_exp_variables_and_types(NULL, cond);

	node->stat_list = stat_list;

	pars_set_parent_in_list(stat_list, node);

	return(node);
}

/*********************************************************************//**
Parses a for-loop-statement.
@return	for-statement node */
UNIV_INTERN
for_node_t*
pars_for_statement(
/*===============*/
	sym_node_t*	loop_var,	/*!< in: loop variable */
	que_node_t*	loop_start_limit,/*!< in: loop start expression */
	que_node_t*	loop_end_limit,	/*!< in: loop end expression */
	que_node_t*	stat_list)	/*!< in: statement list */
{
	for_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(for_node_t));

	node->common.type = QUE_NODE_FOR;

	pars_resolve_exp_variables_and_types(NULL, loop_var);
	pars_resolve_exp_variables_and_types(NULL, loop_start_limit);
	pars_resolve_exp_variables_and_types(NULL, loop_end_limit);

	node->loop_var = loop_var->indirection;

	ut_a(loop_var->indirection);

	node->loop_start_limit = loop_start_limit;
	node->loop_end_limit = loop_end_limit;

	node->stat_list = stat_list;

	pars_set_parent_in_list(stat_list, node);

	return(node);
}

/*********************************************************************//**
Parses an exit statement.
@return	exit statement node */
UNIV_INTERN
exit_node_t*
pars_exit_statement(void)
/*=====================*/
{
	exit_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(exit_node_t));
	node->common.type = QUE_NODE_EXIT;

	return(node);
}

/*********************************************************************//**
Parses a return-statement.
@return	return-statement node */
UNIV_INTERN
return_node_t*
pars_return_statement(void)
/*=======================*/
{
	return_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap,
			      sizeof(return_node_t));
	node->common.type = QUE_NODE_RETURN;

	return(node);
}

/*********************************************************************//**
Parses an assignment statement.
@return	assignment statement node */
UNIV_INTERN
assign_node_t*
pars_assignment_statement(
/*======================*/
	sym_node_t*	var,	/*!< in: variable to assign */
	que_node_t*	val)	/*!< in: value to assign */
{
	assign_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap,
			      sizeof(assign_node_t));
	node->common.type = QUE_NODE_ASSIGNMENT;

	node->var = var;
	node->val = val;

	pars_resolve_exp_variables_and_types(NULL, var);
	pars_resolve_exp_variables_and_types(NULL, val);

	ut_a(dtype_get_mtype(dfield_get_type(que_node_get_val(var)))
	     == dtype_get_mtype(dfield_get_type(que_node_get_val(val))));

	return(node);
}

/*********************************************************************//**
Parses a procedure call.
@return	function node */
UNIV_INTERN
func_node_t*
pars_procedure_call(
/*================*/
	que_node_t*	res_word,/*!< in: procedure name reserved word */
	que_node_t*	args)	/*!< in: argument list */
{
	func_node_t*	node;

	node = pars_func(res_word, args);

	pars_resolve_exp_list_variables_and_types(NULL, args);

	return(node);
}

/*********************************************************************//**
Parses a fetch statement. into_list or user_func (but not both) must be
non-NULL.
@return	fetch statement node */
UNIV_INTERN
fetch_node_t*
pars_fetch_statement(
/*=================*/
	sym_node_t*	cursor,		/*!< in: cursor node */
	sym_node_t*	into_list,	/*!< in: variables to set, or NULL */
	sym_node_t*	user_func)	/*!< in: user function name, or NULL */
{
	sym_node_t*	cursor_decl;
	fetch_node_t*	node;

	/* Logical XOR. */
	ut_a(!into_list != !user_func);

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(fetch_node_t));

	node->common.type = QUE_NODE_FETCH;

	pars_resolve_exp_variables_and_types(NULL, cursor);

	if (into_list) {
		pars_resolve_exp_list_variables_and_types(NULL, into_list);
		node->into_list = into_list;
		node->func = NULL;
	} else {
		pars_resolve_exp_variables_and_types(NULL, user_func);

		node->func = pars_info_get_user_func(pars_sym_tab_global->info,
						     user_func->name);
		ut_a(node->func);

		node->into_list = NULL;
	}

	cursor_decl = cursor->alias;

	ut_a(cursor_decl->token_type == SYM_CURSOR);

	node->cursor_def = cursor_decl->cursor_def;

	if (into_list) {
		ut_a(que_node_list_get_len(into_list)
		     == que_node_list_get_len(node->cursor_def->select_list));
	}

	return(node);
}

/*********************************************************************//**
Parses an open or close cursor statement.
@return	fetch statement node */
UNIV_INTERN
open_node_t*
pars_open_statement(
/*================*/
	ulint		type,	/*!< in: ROW_SEL_OPEN_CURSOR
				or ROW_SEL_CLOSE_CURSOR */
	sym_node_t*	cursor)	/*!< in: cursor node */
{
	sym_node_t*	cursor_decl;
	open_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap, sizeof(open_node_t));

	node->common.type = QUE_NODE_OPEN;

	pars_resolve_exp_variables_and_types(NULL, cursor);

	cursor_decl = cursor->alias;

	ut_a(cursor_decl->token_type == SYM_CURSOR);

	node->op_type = type;
	node->cursor_def = cursor_decl->cursor_def;

	return(node);
}

/*********************************************************************//**
Parses a row_printf-statement.
@return	row_printf-statement node */
UNIV_INTERN
row_printf_node_t*
pars_row_printf_statement(
/*======================*/
	sel_node_t*	sel_node)	/*!< in: select node */
{
	row_printf_node_t*	node;

	node = mem_heap_alloc(pars_sym_tab_global->heap,
			      sizeof(row_printf_node_t));
	node->common.type = QUE_NODE_ROW_PRINTF;

	node->sel_node = sel_node;

	sel_node->common.parent = node;

	return(node);
}

/*********************************************************************//**
Parses a commit statement.
@return	own: commit node struct */
UNIV_INTERN
commit_node_t*
pars_commit_statement(void)
/*=======================*/
{
	return(commit_node_create(pars_sym_tab_global->heap));
}

/*********************************************************************//**
Parses a rollback statement.
@return	own: rollback node struct */
UNIV_INTERN
roll_node_t*
pars_rollback_statement(void)
/*=========================*/
{
	return(roll_node_create(pars_sym_tab_global->heap));
}

/*********************************************************************//**
Parses a column definition at a table creation.
@return	column sym table node */
UNIV_INTERN
sym_node_t*
pars_column_def(
/*============*/
	sym_node_t*		sym_node,	/*!< in: column node in the
						symbol table */
	pars_res_word_t*	type,		/*!< in: data type */
	sym_node_t*		len,		/*!< in: length of column, or
						NULL */
	void*			is_unsigned,	/*!< in: if not NULL, column
						is of type UNSIGNED. */
	void*			is_not_null)	/*!< in: if not NULL, column
						is of type NOT NULL. */
{
	ulint len2;

	if (len) {
		len2 = eval_node_get_int_val(len);
	} else {
		len2 = 0;
	}

	pars_set_dfield_type(que_node_get_val(sym_node), type, len2,
			     is_unsigned != NULL, is_not_null != NULL);

	return(sym_node);
}

/*********************************************************************//**
Parses a table creation operation.
@return	table create subgraph */
UNIV_INTERN
tab_node_t*
pars_create_table(
/*==============*/
	sym_node_t*	table_sym,	/*!< in: table name node in the symbol
					table */
	sym_node_t*	column_defs,	/*!< in: list of column names */
	void*		not_fit_in_memory __attribute__((unused)))
					/*!< in: a non-NULL pointer means that
					this is a table which in simulations
					should be simulated as not fitting
					in memory; thread is put to sleep
					to simulate disk accesses; NOTE that
					this flag is not stored to the data
					dictionary on disk, and the database
					will forget about non-NULL value if
					it has to reload the table definition
					from disk */
{
	dict_table_t*	table;
	sym_node_t*	column;
	tab_node_t*	node;
	const dtype_t*	dtype;
	ulint		n_cols;

	n_cols = que_node_list_get_len(column_defs);

	/* As the InnoDB SQL parser is for internal use only,
	for creating some system tables, this function will only
	create tables in the old (not compact) record format. */
	table = dict_mem_table_create(table_sym->name, 0, n_cols, 0);

#ifdef UNIV_DEBUG
	if (not_fit_in_memory != NULL) {
		table->does_not_fit_in_memory = TRUE;
	}
#endif /* UNIV_DEBUG */
	column = column_defs;

	while (column) {
		dtype = dfield_get_type(que_node_get_val(column));

		dict_mem_table_add_col(table, table->heap,
				       column->name, dtype->mtype,
				       dtype->prtype, dtype->len);
		column->resolved = TRUE;
		column->token_type = SYM_COLUMN;

		column = que_node_get_next(column);
	}

	node = tab_create_graph_create(table, pars_sym_tab_global->heap);

	table_sym->resolved = TRUE;
	table_sym->token_type = SYM_TABLE;

	return(node);
}

/*********************************************************************//**
Parses an index creation operation.
@return	index create subgraph */
UNIV_INTERN
ind_node_t*
pars_create_index(
/*==============*/
	pars_res_word_t* unique_def,	/*!< in: not NULL if a unique index */
	pars_res_word_t* clustered_def,	/*!< in: not NULL if a clustered index */
	sym_node_t*	index_sym,	/*!< in: index name node in the symbol
					table */
	sym_node_t*	table_sym,	/*!< in: table name node in the symbol
					table */
	sym_node_t*	column_list)	/*!< in: list of column names */
{
	dict_index_t*	index;
	sym_node_t*	column;
	ind_node_t*	node;
	ulint		n_fields;
	ulint		ind_type;

	n_fields = que_node_list_get_len(column_list);

	ind_type = 0;

	if (unique_def) {
		ind_type = ind_type | DICT_UNIQUE;
	}

	if (clustered_def) {
		ind_type = ind_type | DICT_CLUSTERED;
	}

	index = dict_mem_index_create(table_sym->name, index_sym->name, 0,
				      ind_type, n_fields);
	column = column_list;

	while (column) {
		dict_mem_index_add_field(index, column->name, 0);

		column->resolved = TRUE;
		column->token_type = SYM_COLUMN;

		column = que_node_get_next(column);
	}

	node = ind_create_graph_create(index, pars_sym_tab_global->heap);

	table_sym->resolved = TRUE;
	table_sym->token_type = SYM_TABLE;

	index_sym->resolved = TRUE;
	index_sym->token_type = SYM_TABLE;

	return(node);
}

/*********************************************************************//**
Parses a procedure definition.
@return	query fork node */
UNIV_INTERN
que_fork_t*
pars_procedure_definition(
/*======================*/
	sym_node_t*	sym_node,	/*!< in: procedure id node in the symbol
					table */
	sym_node_t*	param_list,	/*!< in: parameter declaration list */
	que_node_t*	stat_list)	/*!< in: statement list */
{
	proc_node_t*	node;
	que_fork_t*	fork;
	que_thr_t*	thr;
	mem_heap_t*	heap;

	heap = pars_sym_tab_global->heap;

	fork = que_fork_create(NULL, NULL, QUE_FORK_PROCEDURE, heap);
	fork->trx = NULL;

	thr = que_thr_create(fork, heap);

	node = mem_heap_alloc(heap, sizeof(proc_node_t));

	node->common.type = QUE_NODE_PROC;
	node->common.parent = thr;

	sym_node->token_type = SYM_PROCEDURE_NAME;
	sym_node->resolved = TRUE;

	node->proc_id = sym_node;
	node->param_list = param_list;
	node->stat_list = stat_list;

	pars_set_parent_in_list(stat_list, node);

	node->sym_tab = pars_sym_tab_global;

	thr->child = node;

	pars_sym_tab_global->query_graph = fork;

	return(fork);
}

/*************************************************************//**
Parses a stored procedure call, when this is not within another stored
procedure, that is, the client issues a procedure call directly.
In MySQL/InnoDB, stored InnoDB procedures are invoked via the
parsed procedure tree, not via InnoDB SQL, so this function is not used.
@return	query graph */
UNIV_INTERN
que_fork_t*
pars_stored_procedure_call(
/*=======================*/
	sym_node_t*	sym_node __attribute__((unused)))
					/*!< in: stored procedure name */
{
	ut_error;
	return(NULL);
}

/*************************************************************//**
Retrieves characters to the lexical analyzer. */
UNIV_INTERN
void
pars_get_lex_chars(
/*===============*/
	char*	buf,		/*!< in/out: buffer where to copy */
	int*	result,		/*!< out: number of characters copied or EOF */
	int	max_size)	/*!< in: maximum number of characters which fit
				in the buffer */
{
	int	len;

	len = pars_sym_tab_global->string_len
		- pars_sym_tab_global->next_char_pos;
	if (len == 0) {
#ifdef YYDEBUG
		/* fputs("SQL string ends\n", stderr); */
#endif
		*result = 0;

		return;
	}

	if (len > max_size) {
		len = max_size;
	}

#ifdef UNIV_SQL_DEBUG
	if (pars_print_lexed) {

		if (len >= 5) {
			len = 5;
		}

		fwrite(pars_sym_tab_global->sql_string
		       + pars_sym_tab_global->next_char_pos,
		       1, len, stderr);
	}
#endif /* UNIV_SQL_DEBUG */

	ut_memcpy(buf, pars_sym_tab_global->sql_string
		  + pars_sym_tab_global->next_char_pos, len);
	*result = len;

	pars_sym_tab_global->next_char_pos += len;
}

/*************************************************************//**
Called by yyparse on error. */
UNIV_INTERN
void
yyerror(
/*====*/
	const char*	s __attribute__((unused)))
				/*!< in: error message string */
{
	ut_ad(s);

	fputs("PARSER ERROR: Syntax error in SQL string\n", stderr);

	ut_error;
}

/*************************************************************//**
Parses an SQL string returning the query graph.
@return	own: the query graph */
UNIV_INTERN
que_t*
pars_sql(
/*=====*/
	pars_info_t*	info,	/*!< in: extra information, or NULL */
	const char*	str)	/*!< in: SQL string */
{
	sym_node_t*	sym_node;
	mem_heap_t*	heap;
	que_t*		graph;

	ut_ad(str);

	heap = mem_heap_create(16000);

	/* Currently, the parser is not reentrant: */
	ut_ad(mutex_own(&(dict_sys->mutex)));

	pars_sym_tab_global = sym_tab_create(heap);

	pars_sym_tab_global->string_len = strlen(str);
	pars_sym_tab_global->sql_string = mem_heap_dup(
		heap, str, pars_sym_tab_global->string_len + 1);
	pars_sym_tab_global->next_char_pos = 0;
	pars_sym_tab_global->info = info;

	yyparse();

	sym_node = UT_LIST_GET_FIRST(pars_sym_tab_global->sym_list);

	while (sym_node) {
		ut_a(sym_node->resolved);

		sym_node = UT_LIST_GET_NEXT(sym_list, sym_node);
	}

	graph = pars_sym_tab_global->query_graph;

	graph->sym_tab = pars_sym_tab_global;
	graph->info = info;

	/* fprintf(stderr, "SQL graph size %lu\n", mem_heap_get_size(heap)); */

	return(graph);
}

/******************************************************************//**
Completes a query graph by adding query thread and fork nodes
above it and prepares the graph for running. The fork created is of
type QUE_FORK_MYSQL_INTERFACE.
@return	query thread node to run */
UNIV_INTERN
que_thr_t*
pars_complete_graph_for_exec(
/*=========================*/
	que_node_t*	node,	/*!< in: root node for an incomplete
				query graph */
	trx_t*		trx,	/*!< in: transaction handle */
	mem_heap_t*	heap)	/*!< in: memory heap from which allocated */
{
	que_fork_t*	fork;
	que_thr_t*	thr;

	fork = que_fork_create(NULL, NULL, QUE_FORK_MYSQL_INTERFACE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, heap);

	thr->child = node;

	que_node_set_parent(node, thr);

	trx->graph = NULL;

	return(thr);
}

/****************************************************************//**
Create parser info struct.
@return	own: info struct */
UNIV_INTERN
pars_info_t*
pars_info_create(void)
/*==================*/
{
	pars_info_t*	info;
	mem_heap_t*	heap;

	heap = mem_heap_create(512);

	info = mem_heap_alloc(heap, sizeof(*info));

	info->heap = heap;
	info->funcs = NULL;
	info->bound_lits = NULL;
	info->bound_ids = NULL;
	info->graph_owns_us = TRUE;

	return(info);
}

/****************************************************************//**
Free info struct and everything it contains. */
UNIV_INTERN
void
pars_info_free(
/*===========*/
	pars_info_t*	info)	/*!< in, own: info struct */
{
	mem_heap_free(info->heap);
}

/****************************************************************//**
Add bound literal. */
UNIV_INTERN
void
pars_info_add_literal(
/*==================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const void*	address,	/*!< in: address */
	ulint		length,		/*!< in: length of data */
	ulint		type,		/*!< in: type, e.g. DATA_FIXBINARY */
	ulint		prtype)		/*!< in: precise type, e.g.
					DATA_UNSIGNED */
{
	pars_bound_lit_t*	pbl;

	ut_ad(!pars_info_get_bound_lit(info, name));

	pbl = mem_heap_alloc(info->heap, sizeof(*pbl));

	pbl->name = name;
	pbl->address = address;
	pbl->length = length;
	pbl->type = type;
	pbl->prtype = prtype;

	if (!info->bound_lits) {
		info->bound_lits = ib_vector_create(info->heap, 8);
	}

	ib_vector_push(info->bound_lits, pbl);
}

/****************************************************************//**
Equivalent to pars_info_add_literal(info, name, str, strlen(str),
DATA_VARCHAR, DATA_ENGLISH). */
UNIV_INTERN
void
pars_info_add_str_literal(
/*======================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const char*	str)		/*!< in: string */
{
	pars_info_add_literal(info, name, str, strlen(str),
			      DATA_VARCHAR, DATA_ENGLISH);
}

/****************************************************************//**
Equivalent to:

char buf[4];
mach_write_to_4(buf, val);
pars_info_add_literal(info, name, buf, 4, DATA_INT, 0);

except that the buffer is dynamically allocated from the info struct's
heap. */
UNIV_INTERN
void
pars_info_add_int4_literal(
/*=======================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	lint		val)		/*!< in: value */
{
	byte*	buf = mem_heap_alloc(info->heap, 4);

	mach_write_to_4(buf, val);
	pars_info_add_literal(info, name, buf, 4, DATA_INT, 0);
}

/****************************************************************//**
Equivalent to:

char buf[8];
mach_write_to_8(buf, val);
pars_info_add_literal(info, name, buf, 8, DATA_FIXBINARY, 0);

except that the buffer is dynamically allocated from the info struct's
heap. */
UNIV_INTERN
void
pars_info_add_ull_literal(
/*======================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	ib_uint64_t	val)		/*!< in: value */
{
	byte*	buf = mem_heap_alloc(info->heap, 8);

	mach_write_to_8(buf, val);

	pars_info_add_literal(info, name, buf, 8, DATA_FIXBINARY, 0);
}

/****************************************************************//**
Add user function. */
UNIV_INTERN
void
pars_info_add_function(
/*===================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name,	/*!< in: function name */
	pars_user_func_cb_t	func,	/*!< in: function address */
	void*			arg)	/*!< in: user-supplied argument */
{
	pars_user_func_t*	puf;

	ut_ad(!pars_info_get_user_func(info, name));

	puf = mem_heap_alloc(info->heap, sizeof(*puf));

	puf->name = name;
	puf->func = func;
	puf->arg = arg;

	if (!info->funcs) {
		info->funcs = ib_vector_create(info->heap, 8);
	}

	ib_vector_push(info->funcs, puf);
}

/****************************************************************//**
Add bound id. */
UNIV_INTERN
void
pars_info_add_id(
/*=============*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const char*	id)		/*!< in: id */
{
	pars_bound_id_t*	bid;

	ut_ad(!pars_info_get_bound_id(info, name));

	bid = mem_heap_alloc(info->heap, sizeof(*bid));

	bid->name = name;
	bid->id = id;

	if (!info->bound_ids) {
		info->bound_ids = ib_vector_create(info->heap, 8);
	}

	ib_vector_push(info->bound_ids, bid);
}

/****************************************************************//**
Get user function with the given name.
@return	user func, or NULL if not found */
UNIV_INTERN
pars_user_func_t*
pars_info_get_user_func(
/*====================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name)	/*!< in: function name to find*/
{
	ulint		i;
	ib_vector_t*	vec;

	if (!info || !info->funcs) {
		return(NULL);
	}

	vec = info->funcs;

	for (i = 0; i < ib_vector_size(vec); i++) {
		pars_user_func_t*	puf = ib_vector_get(vec, i);

		if (strcmp(puf->name, name) == 0) {
			return(puf);
		}
	}

	return(NULL);
}

/****************************************************************//**
Get bound literal with the given name.
@return	bound literal, or NULL if not found */
UNIV_INTERN
pars_bound_lit_t*
pars_info_get_bound_lit(
/*====================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name)	/*!< in: bound literal name to find */
{
	ulint		i;
	ib_vector_t*	vec;

	if (!info || !info->bound_lits) {
		return(NULL);
	}

	vec = info->bound_lits;

	for (i = 0; i < ib_vector_size(vec); i++) {
		pars_bound_lit_t*	pbl = ib_vector_get(vec, i);

		if (strcmp(pbl->name, name) == 0) {
			return(pbl);
		}
	}

	return(NULL);
}

/****************************************************************//**
Get bound id with the given name.
@return	bound id, or NULL if not found */
UNIV_INTERN
pars_bound_id_t*
pars_info_get_bound_id(
/*===================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name)	/*!< in: bound id name to find */
{
	ulint		i;
	ib_vector_t*	vec;

	if (!info || !info->bound_ids) {
		return(NULL);
	}

	vec = info->bound_ids;

	for (i = 0; i < ib_vector_size(vec); i++) {
		pars_bound_id_t*	bid = ib_vector_get(vec, i);

		if (strcmp(bid->name, name) == 0) {
			return(bid);
		}
	}

	return(NULL);
}
