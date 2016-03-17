/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/pars0pars.h
SQL parser

Created 11/19/1996 Heikki Tuuri
*******************************************************/

#ifndef pars0pars_h
#define pars0pars_h

#include "univ.i"
#include "que0types.h"
#include "usr0types.h"
#include "pars0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0vec.h"
#include "row0mysql.h"

/** Type of the user functions. The first argument is always InnoDB-supplied
and varies in type, while 'user_arg' is a user-supplied argument. The
meaning of the return type also varies. See the individual use cases, e.g.
the FETCH statement, for details on them. */
typedef ibool	(*pars_user_func_cb_t)(void* arg, void* user_arg);

/** If the following is set TRUE, the parser will emit debugging
information */
extern int	yydebug;

/* Global variable used while parsing a single procedure or query : the code is
NOT re-entrant */
extern sym_tab_t*	pars_sym_tab_global;

extern pars_res_word_t	pars_to_char_token;
extern pars_res_word_t	pars_to_number_token;
extern pars_res_word_t	pars_to_binary_token;
extern pars_res_word_t	pars_binary_to_number_token;
extern pars_res_word_t	pars_substr_token;
extern pars_res_word_t	pars_replstr_token;
extern pars_res_word_t	pars_concat_token;
extern pars_res_word_t	pars_length_token;
extern pars_res_word_t	pars_instr_token;
extern pars_res_word_t	pars_sysdate_token;
extern pars_res_word_t	pars_printf_token;
extern pars_res_word_t	pars_assert_token;
extern pars_res_word_t	pars_rnd_token;
extern pars_res_word_t	pars_rnd_str_token;
extern pars_res_word_t	pars_count_token;
extern pars_res_word_t	pars_sum_token;
extern pars_res_word_t	pars_distinct_token;
extern pars_res_word_t	pars_binary_token;
extern pars_res_word_t	pars_blob_token;
extern pars_res_word_t	pars_int_token;
extern pars_res_word_t	pars_bigint_token;
extern pars_res_word_t	pars_char_token;
extern pars_res_word_t	pars_float_token;
extern pars_res_word_t	pars_update_token;
extern pars_res_word_t	pars_asc_token;
extern pars_res_word_t	pars_desc_token;
extern pars_res_word_t	pars_open_token;
extern pars_res_word_t	pars_close_token;
extern pars_res_word_t	pars_share_token;
extern pars_res_word_t	pars_unique_token;
extern pars_res_word_t	pars_clustered_token;

extern ulint		pars_star_denoter;

/* Procedure parameter types */
#define PARS_INPUT	0
#define PARS_OUTPUT	1
#define PARS_NOT_PARAM	2

int
yyparse(void);

/*************************************************************//**
Parses an SQL string returning the query graph.
@return own: the query graph */
que_t*
pars_sql(
/*=====*/
	pars_info_t*	info,	/*!< in: extra information, or NULL */
	const char*	str);	/*!< in: SQL string */
/*************************************************************//**
Retrieves characters to the lexical analyzer.
@return number of characters copied or 0 on EOF */
int
pars_get_lex_chars(
/*===============*/
	char*	buf,		/*!< in/out: buffer where to copy */
	int	max_size);	/*!< in: maximum number of characters which fit
				in the buffer */
/*************************************************************//**
Called by yyparse on error. */
void
yyerror(
/*====*/
	const char*	s);	/*!< in: error message string */
/*********************************************************************//**
Parses a variable declaration.
@return own: symbol table node of type SYM_VAR */
sym_node_t*
pars_variable_declaration(
/*======================*/
	sym_node_t*	node,	/*!< in: symbol table node allocated for the
				id of the variable */
	pars_res_word_t* type);	/*!< in: pointer to a type token */
/*********************************************************************//**
Parses a function expression.
@return own: function node in a query tree */
func_node_t*
pars_func(
/*======*/
	que_node_t*	res_word,/*!< in: function name reserved word */
	que_node_t*	arg);	/*!< in: first argument in the argument list */
/*************************************************************************
Rebind a LIKE search string. NOTE: We ignore any '%' characters embedded
within the search string.
@return own: function node in a query tree */
int
pars_like_rebind(
/*=============*/
        sym_node_t*     node,   /* in: The search string node.*/
        const byte*     ptr,    /* in: literal to (re) bind */
        ulint           len);   /* in: length of literal to (re) bind*/
/*********************************************************************//**
Parses an operator expression.
@return own: function node in a query tree */
func_node_t*
pars_op(
/*====*/
	int		func,	/*!< in: operator token code */
	que_node_t*	arg1,	/*!< in: first argument */
	que_node_t*	arg2);	/*!< in: second argument or NULL for an unary
				operator */
/*********************************************************************//**
Parses an ORDER BY clause. Order by a single column only is supported.
@return own: order-by node in a query tree */
order_node_t*
pars_order_by(
/*==========*/
	sym_node_t*	column,	/*!< in: column name */
	pars_res_word_t* asc);	/*!< in: &pars_asc_token or pars_desc_token */
/*********************************************************************//**
Parses a select list; creates a query graph node for the whole SELECT
statement.
@return own: select node in a query tree */
sel_node_t*
pars_select_list(
/*=============*/
	que_node_t*	select_list,	/*!< in: select list */
	sym_node_t*	into_list);	/*!< in: variables list or NULL */
/*********************************************************************//**
Parses a cursor declaration.
@return sym_node */
que_node_t*
pars_cursor_declaration(
/*====================*/
	sym_node_t*	sym_node,	/*!< in: cursor id node in the symbol
					table */
	sel_node_t*	select_node);	/*!< in: select node */
/*********************************************************************//**
Parses a function declaration.
@return sym_node */
que_node_t*
pars_function_declaration(
/*======================*/
	sym_node_t*	sym_node);	/*!< in: function id node in the symbol
					table */
/*********************************************************************//**
Parses a select statement.
@return own: select node in a query tree */
sel_node_t*
pars_select_statement(
/*==================*/
	sel_node_t*	select_node,	/*!< in: select node already containing
					the select list */
	sym_node_t*	table_list,	/*!< in: table list */
	que_node_t*	search_cond,	/*!< in: search condition or NULL */
	pars_res_word_t* for_update,	/*!< in: NULL or &pars_update_token */
	pars_res_word_t* consistent_read,/*!< in: NULL or
						&pars_consistent_token */
	order_node_t*	order_by);	/*!< in: NULL or an order-by node */
/*********************************************************************//**
Parses a column assignment in an update.
@return column assignment node */
col_assign_node_t*
pars_column_assignment(
/*===================*/
	sym_node_t*	column,	/*!< in: column to assign */
	que_node_t*	exp);	/*!< in: value to assign */
/*********************************************************************//**
Parses a delete or update statement start.
@return own: update node in a query tree */
upd_node_t*
pars_update_statement_start(
/*========================*/
	ibool		is_delete,	/*!< in: TRUE if delete */
	sym_node_t*	table_sym,	/*!< in: table name node */
	col_assign_node_t* col_assign_list);/*!< in: column assignment list, NULL
					if delete */
/*********************************************************************//**
Parses an update or delete statement.
@return own: update node in a query tree */
upd_node_t*
pars_update_statement(
/*==================*/
	upd_node_t*	node,		/*!< in: update node */
	sym_node_t*	cursor_sym,	/*!< in: pointer to a cursor entry in
					the symbol table or NULL */
	que_node_t*	search_cond);	/*!< in: search condition or NULL */
/*********************************************************************//**
Parses an insert statement.
@return own: update node in a query tree */
ins_node_t*
pars_insert_statement(
/*==================*/
	sym_node_t*	table_sym,	/*!< in: table name node */
	que_node_t*	values_list,	/*!< in: value expression list or NULL */
	sel_node_t*	select);	/*!< in: select condition or NULL */
/*********************************************************************//**
Parses a procedure parameter declaration.
@return own: symbol table node of type SYM_VAR */
sym_node_t*
pars_parameter_declaration(
/*=======================*/
	sym_node_t*	node,	/*!< in: symbol table node allocated for the
				id of the parameter */
	ulint		param_type,
				/*!< in: PARS_INPUT or PARS_OUTPUT */
	pars_res_word_t* type);	/*!< in: pointer to a type token */
/*********************************************************************//**
Parses an elsif element.
@return elsif node */
elsif_node_t*
pars_elsif_element(
/*===============*/
	que_node_t*	cond,		/*!< in: if-condition */
	que_node_t*	stat_list);	/*!< in: statement list */
/*********************************************************************//**
Parses an if-statement.
@return if-statement node */
if_node_t*
pars_if_statement(
/*==============*/
	que_node_t*	cond,		/*!< in: if-condition */
	que_node_t*	stat_list,	/*!< in: statement list */
	que_node_t*	else_part);	/*!< in: else-part statement list */
/*********************************************************************//**
Parses a for-loop-statement.
@return for-statement node */
for_node_t*
pars_for_statement(
/*===============*/
	sym_node_t*	loop_var,	/*!< in: loop variable */
	que_node_t*	loop_start_limit,/*!< in: loop start expression */
	que_node_t*	loop_end_limit,	/*!< in: loop end expression */
	que_node_t*	stat_list);	/*!< in: statement list */
/*********************************************************************//**
Parses a while-statement.
@return while-statement node */
while_node_t*
pars_while_statement(
/*=================*/
	que_node_t*	cond,		/*!< in: while-condition */
	que_node_t*	stat_list);	/*!< in: statement list */
/*********************************************************************//**
Parses an exit statement.
@return exit statement node */
exit_node_t*
pars_exit_statement(void);
/*=====================*/
/*********************************************************************//**
Parses a return-statement.
@return return-statement node */
return_node_t*
pars_return_statement(void);
/*=======================*/
/*********************************************************************//**
Parses a procedure call.
@return function node */
func_node_t*
pars_procedure_call(
/*================*/
	que_node_t*	res_word,/*!< in: procedure name reserved word */
	que_node_t*	args);	/*!< in: argument list */
/*********************************************************************//**
Parses an assignment statement.
@return assignment statement node */
assign_node_t*
pars_assignment_statement(
/*======================*/
	sym_node_t*	var,	/*!< in: variable to assign */
	que_node_t*	val);	/*!< in: value to assign */
/*********************************************************************//**
Parses a fetch statement. into_list or user_func (but not both) must be
non-NULL.
@return fetch statement node */
fetch_node_t*
pars_fetch_statement(
/*=================*/
	sym_node_t*	cursor,		/*!< in: cursor node */
	sym_node_t*	into_list,	/*!< in: variables to set, or NULL */
	sym_node_t*	user_func);	/*!< in: user function name, or NULL */
/*********************************************************************//**
Parses an open or close cursor statement.
@return fetch statement node */
open_node_t*
pars_open_statement(
/*================*/
	ulint		type,	/*!< in: ROW_SEL_OPEN_CURSOR
				or ROW_SEL_CLOSE_CURSOR */
	sym_node_t*	cursor);	/*!< in: cursor node */
/*********************************************************************//**
Parses a row_printf-statement.
@return row_printf-statement node */
row_printf_node_t*
pars_row_printf_statement(
/*======================*/
	sel_node_t*	sel_node);	/*!< in: select node */
/*********************************************************************//**
Parses a commit statement.
@return own: commit node struct */
commit_node_t*
pars_commit_statement(void);
/*=======================*/
/*********************************************************************//**
Parses a rollback statement.
@return own: rollback node struct */
roll_node_t*
pars_rollback_statement(void);
/*=========================*/
/*********************************************************************//**
Parses a column definition at a table creation.
@return column sym table node */
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
	void*			is_not_null);	/*!< in: if not NULL, column
						is of type NOT NULL. */
/*********************************************************************//**
Parses a table creation operation.
@return table create subgraph */
tab_node_t*
pars_create_table(
/*==============*/
	sym_node_t*	table_sym,	/*!< in: table name node in the symbol
					table */
	sym_node_t*	column_defs,	/*!< in: list of column names */
	sym_node_t*	compact,	/* in: non-NULL if COMPACT table. */
	sym_node_t*	block_size,	/* in: block size (can be NULL) */
	void*		not_fit_in_memory);
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
/*********************************************************************//**
Parses an index creation operation.
@return index create subgraph */
ind_node_t*
pars_create_index(
/*==============*/
	pars_res_word_t* unique_def,	/*!< in: not NULL if a unique index */
	pars_res_word_t* clustered_def,	/*!< in: not NULL if a clustered index */
	sym_node_t*	index_sym,	/*!< in: index name node in the symbol
					table */
	sym_node_t*	table_sym,	/*!< in: table name node in the symbol
					table */
	sym_node_t*	column_list);	/*!< in: list of column names */
/*********************************************************************//**
Parses a procedure definition.
@return query fork node */
que_fork_t*
pars_procedure_definition(
/*======================*/
	sym_node_t*	sym_node,	/*!< in: procedure id node in the symbol
					table */
	sym_node_t*	param_list,	/*!< in: parameter declaration list */
	que_node_t*	stat_list);	/*!< in: statement list */

/*************************************************************//**
Parses a stored procedure call, when this is not within another stored
procedure, that is, the client issues a procedure call directly.
In MySQL/InnoDB, stored InnoDB procedures are invoked via the
parsed procedure tree, not via InnoDB SQL, so this function is not used.
@return query graph */
que_fork_t*
pars_stored_procedure_call(
/*=======================*/
	sym_node_t*	sym_node);	/*!< in: stored procedure name */
/** Completes a query graph by adding query thread and fork nodes
above it and prepares the graph for running. The fork created is of
type QUE_FORK_MYSQL_INTERFACE.
@param[in]	node		root node for an incomplete query
				graph, or NULL for dummy graph
@param[in]	trx		transaction handle
@param[in]	heap		memory heap from which allocated
@param[in]	prebuilt	row prebuilt structure
@return query thread node to run */
que_thr_t*
pars_complete_graph_for_exec(
	que_node_t*	node,
	trx_t*		trx,
	mem_heap_t*	heap,
	row_prebuilt_t*	prebuilt)
	MY_ATTRIBUTE((nonnull(2,3), warn_unused_result));

/****************************************************************//**
Create parser info struct.
@return own: info struct */
pars_info_t*
pars_info_create(void);
/*==================*/

/****************************************************************//**
Free info struct and everything it contains. */
void
pars_info_free(
/*===========*/
	pars_info_t*	info);	/*!< in, own: info struct */

/****************************************************************//**
Add bound literal. */
void
pars_info_add_literal(
/*==================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const void*	address,	/*!< in: address */
	ulint		length,		/*!< in: length of data */
	ulint		type,		/*!< in: type, e.g. DATA_FIXBINARY */
	ulint		prtype);	/*!< in: precise type, e.g.
					DATA_UNSIGNED */

/****************************************************************//**
Equivalent to pars_info_add_literal(info, name, str, strlen(str),
DATA_VARCHAR, DATA_ENGLISH). */
void
pars_info_add_str_literal(
/*======================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const char*	str);		/*!< in: string */
/********************************************************************
If the literal value already exists then it rebinds otherwise it
creates a new entry.*/
void
pars_info_bind_literal(
/*===================*/
	pars_info_t*	info,		/* in: info struct */
	const char*	name,		/* in: name */
	const void*	address,	/* in: address */
	ulint		length,		/* in: length of data */
	ulint		type,		/* in: type, e.g. DATA_FIXBINARY */
	ulint		prtype);	/* in: precise type, e.g. */
/********************************************************************
If the literal value already exists then it rebinds otherwise it
creates a new entry.*/
void
pars_info_bind_varchar_literal(
/*===========================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const byte*	str,		/*!< in: string */
	ulint		str_len);	/*!< in: string length */
/****************************************************************//**
Equivalent to:

char buf[4];
mach_write_to_4(buf, val);
pars_info_add_literal(info, name, buf, 4, DATA_INT, 0);

except that the buffer is dynamically allocated from the info struct's
heap. */
void
pars_info_bind_int4_literal(
/*=======================*/
	pars_info_t*		info,		/*!< in: info struct */
	const char*		name,		/*!< in: name */
	const ib_uint32_t*	val);		/*!< in: value */
/********************************************************************
If the literal value already exists then it rebinds otherwise it
creates a new entry. */
void
pars_info_bind_int8_literal(
/*=======================*/
	pars_info_t*		info,		/*!< in: info struct */
	const char*		name,		/*!< in: name */
	const ib_uint64_t*	val);		/*!< in: value */
/****************************************************************//**
Add user function. */
void
pars_info_bind_function(
/*===================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name,	/*!< in: function name */
	pars_user_func_cb_t	func,	/*!< in: function address */
	void*			arg);	/*!< in: user-supplied argument */
/****************************************************************//**
Add bound id. */
void
pars_info_bind_id(
/*=============*/
	pars_info_t*		info,	/*!< in: info struct */
	ibool			copy_name,/* in: make a copy of name if TRUE */
	const char*		name,	/*!< in: name */
	const char*		id);	/*!< in: id */
/****************************************************************//**
Equivalent to:

char buf[4];
mach_write_to_4(buf, val);
pars_info_add_literal(info, name, buf, 4, DATA_INT, 0);

except that the buffer is dynamically allocated from the info struct's
heap. */
void
pars_info_add_int4_literal(
/*=======================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	lint		val);		/*!< in: value */

/****************************************************************//**
Equivalent to:

char buf[8];
mach_write_to_8(buf, val);
pars_info_add_literal(info, name, buf, 8, DATA_FIXBINARY, 0);

except that the buffer is dynamically allocated from the info struct's
heap. */
void
pars_info_add_ull_literal(
/*======================*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	ib_uint64_t	val);		/*!< in: value */

/****************************************************************//**
If the literal value already exists then it rebinds otherwise it
creates a new entry. */
void
pars_info_bind_ull_literal(
/*=======================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name,	/*!< in: name */
	const ib_uint64_t*	val)	/*!< in: value */
	MY_ATTRIBUTE((nonnull));

/****************************************************************//**
Add bound id. */
void
pars_info_add_id(
/*=============*/
	pars_info_t*	info,		/*!< in: info struct */
	const char*	name,		/*!< in: name */
	const char*	id);		/*!< in: id */

/****************************************************************//**
Get bound literal with the given name.
@return bound literal, or NULL if not found */
pars_bound_lit_t*
pars_info_get_bound_lit(
/*====================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name);	/*!< in: bound literal name to find */

/****************************************************************//**
Get bound id with the given name.
@return bound id, or NULL if not found */
pars_bound_id_t*
pars_info_get_bound_id(
/*===================*/
	pars_info_t*		info,	/*!< in: info struct */
	const char*		name);	/*!< in: bound id name to find */

/******************************************************************//**
Release any resources used by the lexer. */
void
pars_lexer_close(void);
/*==================*/

/** Extra information supplied for pars_sql(). */
struct pars_info_t {
	mem_heap_t*	heap;		/*!< our own memory heap */

	ib_vector_t*	funcs;		/*!< user functions, or NUll
					(pars_user_func_t*) */
	ib_vector_t*	bound_lits;	/*!< bound literals, or NULL
					(pars_bound_lit_t*) */
	ib_vector_t*	bound_ids;	/*!< bound ids, or NULL
					(pars_bound_id_t*) */

	ibool		graph_owns_us;	/*!< if TRUE (which is the default),
					que_graph_free() will free us */
};

/** User-supplied function and argument. */
struct pars_user_func_t {
	const char*		name;	/*!< function name */
	pars_user_func_cb_t	func;	/*!< function address */
	void*			arg;	/*!< user-supplied argument */
};

/** Bound literal. */
struct pars_bound_lit_t {
	const char*	name;		/*!< name */
	const void*	address;	/*!< address */
	ulint		length;		/*!< length of data */
	ulint		type;		/*!< type, e.g. DATA_FIXBINARY */
	ulint		prtype;		/*!< precise type, e.g. DATA_UNSIGNED */
	sym_node_t*	node;		/*!< symbol node */
};

/** Bound identifier. */
struct pars_bound_id_t {
	const char*	name;		/*!< name */
	const char*	id;		/*!< identifier */
};

/** Struct used to denote a reserved word in a parsing tree */
struct pars_res_word_t{
	int	code;	/*!< the token code for the reserved word from
			pars0grm.h */
};

/** A predefined function or operator node in a parsing tree; this construct
is also used for some non-functions like the assignment ':=' */
struct func_node_t{
	que_common_t	common;	/*!< type: QUE_NODE_FUNC */
	int		func;	/*!< token code of the function name */
	ulint		fclass;	/*!< class of the function */
	que_node_t*	args;	/*!< argument(s) of the function */
	UT_LIST_NODE_T(func_node_t) cond_list;
				/*!< list of comparison conditions; defined
				only for comparison operator nodes except,
				presently, for OPT_SCROLL_TYPE ones */
	UT_LIST_NODE_T(func_node_t) func_node_list;
				/*!< list of function nodes in a parsed
				query graph */
};

/** An order-by node in a select */
struct order_node_t{
	que_common_t	common;	/*!< type: QUE_NODE_ORDER */
	sym_node_t*	column;	/*!< order-by column */
	ibool		asc;	/*!< TRUE if ascending, FALSE if descending */
};

/** Procedure definition node */
struct proc_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_PROC */
	sym_node_t*	proc_id;	/*!< procedure name symbol in the symbol
					table of this same procedure */
	sym_node_t*	param_list;	/*!< input and output parameters */
	que_node_t*	stat_list;	/*!< statement list */
	sym_tab_t*	sym_tab;	/*!< symbol table of this procedure */
};

/** elsif-element node */
struct elsif_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_ELSIF */
	que_node_t*	cond;		/*!< if condition */
	que_node_t*	stat_list;	/*!< statement list */
};

/** if-statement node */
struct if_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_IF */
	que_node_t*	cond;		/*!< if condition */
	que_node_t*	stat_list;	/*!< statement list */
	que_node_t*	else_part;	/*!< else-part statement list */
	elsif_node_t*	elsif_list;	/*!< elsif element list */
};

/** while-statement node */
struct while_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_WHILE */
	que_node_t*	cond;		/*!< while condition */
	que_node_t*	stat_list;	/*!< statement list */
};

/** for-loop-statement node */
struct for_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_FOR */
	sym_node_t*	loop_var;	/*!< loop variable: this is the
					dereferenced symbol from the
					variable declarations, not the
					symbol occurrence in the for loop
					definition */
	que_node_t*	loop_start_limit;/*!< initial value of loop variable */
	que_node_t*	loop_end_limit;	/*!< end value of loop variable */
	lint		loop_end_value;	/*!< evaluated value for the end value:
					it is calculated only when the loop
					is entered, and will not change within
					the loop */
	que_node_t*	stat_list;	/*!< statement list */
};

/** exit statement node */
struct exit_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_EXIT */
};

/** return-statement node */
struct return_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_RETURN */
};

/** Assignment statement node */
struct assign_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_ASSIGNMENT */
	sym_node_t*	var;		/*!< variable to set */
	que_node_t*	val;		/*!< value to assign */
};

/** Column assignment node */
struct col_assign_node_t{
	que_common_t	common;		/*!< type: QUE_NODE_COL_ASSIGN */
	sym_node_t*	col;		/*!< column to set */
	que_node_t*	val;		/*!< value to assign */
};

/** Classes of functions */
/* @{ */
#define PARS_FUNC_ARITH		1	/*!< +, -, *, / */
#define	PARS_FUNC_LOGICAL	2	/*!< AND, OR, NOT */
#define PARS_FUNC_CMP		3	/*!< comparison operators */
#define	PARS_FUNC_PREDEFINED	4	/*!< TO_NUMBER, SUBSTR, ... */
#define	PARS_FUNC_AGGREGATE	5	/*!< COUNT, DISTINCT, SUM */
#define	PARS_FUNC_OTHER		6	/*!< these are not real functions,
					e.g., := */
/* @} */

#ifndef UNIV_NONINL
#include "pars0pars.ic"
#endif

#endif
