/******************************************************
SQL parser

(c) 1996 Innobase Oy

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

extern int	yydebug;

/* If the following is set TRUE, the lexer will print the SQL string
as it tokenizes it */

#ifdef UNIV_SQL_DEBUG
extern ibool	pars_print_lexed;
#endif /* UNIV_SQL_DEBUG */

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
extern pars_res_word_t	pars_int_token;
extern pars_res_word_t	pars_char_token;
extern pars_res_word_t	pars_float_token;
extern pars_res_word_t	pars_update_token;
extern pars_res_word_t	pars_asc_token;
extern pars_res_word_t	pars_desc_token;
extern pars_res_word_t	pars_open_token;
extern pars_res_word_t	pars_close_token;
extern pars_res_word_t	pars_consistent_token;
extern pars_res_word_t	pars_unique_token;
extern pars_res_word_t	pars_clustered_token;

extern ulint		pars_star_denoter;
	
/* Procedure parameter types */
#define PARS_INPUT	0
#define PARS_OUTPUT	1
#define PARS_NOT_PARAM	2

int
yyparse(void);

/*****************************************************************
Parses an SQL string returning the query graph. */

que_t*
pars_sql(
/*=====*/
				/* out, own: the query graph */
	const char*	str);	/* in: SQL string */
/*****************************************************************
Retrieves characters to the lexical analyzer. */

void
pars_get_lex_chars(
/*===============*/
	char*	buf,		/* in/out: buffer where to copy */
	int*	result,		/* out: number of characters copied or EOF */
	int	max_size);	/* in: maximum number of characters which fit
				in the buffer */
/*****************************************************************
Called by yyparse on error. */

void
yyerror(
/*====*/
	const char*	s);	/* in: error message string */
/*************************************************************************
Parses a variable declaration. */

sym_node_t*
pars_variable_declaration(
/*======================*/
				/* out, own: symbol table node of type
				SYM_VAR */
	sym_node_t*	node,	/* in: symbol table node allocated for the
				id of the variable */
	pars_res_word_t* type);	/* in: pointer to a type token */
/*************************************************************************
Parses a function expression. */

func_node_t*
pars_func(
/*======*/
				/* out, own: function node in a query tree */
	que_node_t* 	res_word,/* in: function name reserved word */
	que_node_t*	arg);	/* in: first argument in the argument list */
/*************************************************************************
Parses an operator expression. */

func_node_t*
pars_op(
/*====*/
				/* out, own: function node in a query tree */
	int		func,	/* in: operator token code */
	que_node_t*	arg1,	/* in: first argument */
	que_node_t*	arg2);	/* in: second argument or NULL for an unary
				operator */
/*************************************************************************
Parses an ORDER BY clause. Order by a single column only is supported. */

order_node_t*
pars_order_by(
/*==========*/
				/* out, own: order-by node in a query tree */
	sym_node_t*	column,	/* in: column name */
	pars_res_word_t* asc);	/* in: &pars_asc_token or pars_desc_token */
/*************************************************************************
Parses a select list; creates a query graph node for the whole SELECT
statement. */

sel_node_t*
pars_select_list(
/*=============*/
					/* out, own: select node in a query
					tree */
	que_node_t*	select_list,	/* in: select list */
	sym_node_t*	into_list);	/* in: variables list or NULL */
/*************************************************************************
Parses a cursor declaration. */

que_node_t*
pars_cursor_declaration(
/*====================*/
					/* out: sym_node */
	sym_node_t*	sym_node,	/* in: cursor id node in the symbol
					table */
	sel_node_t*	select_node);	/* in: select node */
/*************************************************************************
Parses a select statement. */

sel_node_t*
pars_select_statement(
/*==================*/
					/* out, own: select node in a query
					tree */
	sel_node_t*	select_node,	/* in: select node already containing
					the select list */
	sym_node_t*	table_list,	/* in: table list */
	que_node_t*	search_cond,	/* in: search condition or NULL */
	pars_res_word_t* for_update,	/* in: NULL or &pars_update_token */
	pars_res_word_t* consistent_read,/* in: NULL or
						&pars_consistent_token */
	order_node_t*	order_by);	/* in: NULL or an order-by node */
/*************************************************************************
Parses a column assignment in an update. */

col_assign_node_t*
pars_column_assignment(
/*===================*/
				/* out: column assignment node */
	sym_node_t*	column,	/* in: column to assign */
	que_node_t*	exp);	/* in: value to assign */
/*************************************************************************
Parses a delete or update statement start. */

upd_node_t*
pars_update_statement_start(
/*========================*/
					/* out, own: update node in a query
					tree */
	ibool		is_delete,	/* in: TRUE if delete */
	sym_node_t*	table_sym,	/* in: table name node */
	col_assign_node_t* col_assign_list);/* in: column assignment list, NULL
					if delete */	
/*************************************************************************
Parses an update or delete statement. */

upd_node_t*
pars_update_statement(
/*==================*/
					/* out, own: update node in a query
					tree */
	upd_node_t*	node,		/* in: update node */
	sym_node_t*	cursor_sym,	/* in: pointer to a cursor entry in
					the symbol table or NULL */
	que_node_t*	search_cond);	/* in: search condition or NULL */
/*************************************************************************
Parses an insert statement. */

ins_node_t*
pars_insert_statement(
/*==================*/
					/* out, own: update node in a query
					tree */
	sym_node_t*	table_sym,	/* in: table name node */
	que_node_t* 	values_list,	/* in: value expression list or NULL */
	sel_node_t*	select);	/* in: select condition or NULL */
/*************************************************************************
Parses a procedure parameter declaration. */

sym_node_t*
pars_parameter_declaration(
/*=======================*/
				/* out, own: symbol table node of type
				SYM_VAR */
	sym_node_t*	node,	/* in: symbol table node allocated for the
				id of the parameter */
	ulint		param_type,
				/* in: PARS_INPUT or PARS_OUTPUT */
	pars_res_word_t* type);	/* in: pointer to a type token */
/*************************************************************************
Parses an elsif element. */

elsif_node_t*
pars_elsif_element(
/*===============*/
					/* out: elsif node */
	que_node_t*	cond,		/* in: if-condition */
	que_node_t*	stat_list);	/* in: statement list */
/*************************************************************************
Parses an if-statement. */

if_node_t*
pars_if_statement(
/*==============*/
					/* out: if-statement node */
	que_node_t*	cond,		/* in: if-condition */
	que_node_t*	stat_list,	/* in: statement list */
	que_node_t*	else_part);	/* in: else-part statement list */
/*************************************************************************
Parses a for-loop-statement. */

for_node_t*
pars_for_statement(
/*===============*/
					/* out: for-statement node */
	sym_node_t*	loop_var,	/* in: loop variable */
	que_node_t*	loop_start_limit,/* in: loop start expression */
	que_node_t*	loop_end_limit,	/* in: loop end expression */
	que_node_t*	stat_list);	/* in: statement list */
/*************************************************************************
Parses a while-statement. */

while_node_t*
pars_while_statement(
/*=================*/
					/* out: while-statement node */
	que_node_t*	cond,		/* in: while-condition */
	que_node_t*	stat_list);	/* in: statement list */
/*************************************************************************
Parses a return-statement. */

return_node_t*
pars_return_statement(void);
/*=======================*/
					/* out: return-statement node */
/*************************************************************************
Parses a procedure call. */

func_node_t*
pars_procedure_call(
/*================*/
				/* out: function node */
	que_node_t*	res_word,/* in: procedure name reserved word */
	que_node_t*	args);	/* in: argument list */
/*************************************************************************
Parses an assignment statement. */

assign_node_t*
pars_assignment_statement(
/*======================*/
				/* out: assignment statement node */
	sym_node_t*	var,	/* in: variable to assign */
	que_node_t*	val);	/* in: value to assign */
/*************************************************************************
Parses a fetch statement. */

fetch_node_t*
pars_fetch_statement(
/*=================*/
					/* out: fetch statement node */
	sym_node_t*	cursor,		/* in: cursor node */
	sym_node_t*	into_list);	/* in: variables to set */
/*************************************************************************
Parses an open or close cursor statement. */

open_node_t*
pars_open_statement(
/*================*/
				/* out: fetch statement node */
	ulint		type,	/* in: ROW_SEL_OPEN_CURSOR
				or ROW_SEL_CLOSE_CURSOR */
	sym_node_t*	cursor);	/* in: cursor node */
/*************************************************************************
Parses a row_printf-statement. */

row_printf_node_t*
pars_row_printf_statement(
/*======================*/
					/* out: row_printf-statement node */
	sel_node_t*	sel_node);	/* in: select node */
/*************************************************************************
Parses a commit statement. */

commit_node_t*
pars_commit_statement(void);
/*=======================*/
/*************************************************************************
Parses a rollback statement. */

roll_node_t*
pars_rollback_statement(void);
/*=========================*/
/*************************************************************************
Parses a column definition at a table creation. */

sym_node_t*
pars_column_def(
/*============*/
					/* out: column sym table node */
	sym_node_t*	sym_node,	/* in: column node in the symbol
					table */
	pars_res_word_t* type);		/* in: data type */
/*************************************************************************
Parses a table creation operation. */

tab_node_t*
pars_create_table(
/*==============*/
					/* out: table create subgraph */
	sym_node_t*	table_sym,	/* in: table name node in the symbol
					table */
	sym_node_t*	column_defs,	/* in: list of column names */
	void*		not_fit_in_memory);/* in: a non-NULL pointer means that
					this is a table which in simulations
					should be simulated as not fitting
					in memory; thread is put to sleep
					to simulate disk accesses; NOTE that
					this flag is not stored to the data
					dictionary on disk, and the database
					will forget about non-NULL value if
					it has to reload the table definition
					from disk */
/*************************************************************************
Parses an index creation operation. */

ind_node_t*
pars_create_index(
/*==============*/
					/* out: index create subgraph */
	pars_res_word_t* unique_def,	/* in: not NULL if a unique index */
	pars_res_word_t* clustered_def,	/* in: not NULL if a clustered index */
	sym_node_t*	index_sym,	/* in: index name node in the symbol
					table */
	sym_node_t*	table_sym,	/* in: table name node in the symbol
					table */
	sym_node_t*	column_list);	/* in: list of column names */
/*************************************************************************
Parses a procedure definition. */

que_fork_t*
pars_procedure_definition(
/*======================*/
					/* out: query fork node */
	sym_node_t*	sym_node,	/* in: procedure id node in the symbol
					table */
	sym_node_t*	param_list,	/* in: parameter declaration list */
	que_node_t*	stat_list);	/* in: statement list */

/*****************************************************************
Parses a stored procedure call, when this is not within another stored
procedure, that is, the client issues a procedure call directly.
In MySQL/InnoDB, stored InnoDB procedures are invoked via the
parsed procedure tree, not via InnoDB SQL, so this function is not used. */

que_fork_t*
pars_stored_procedure_call(
/*=======================*/
					/* out: query graph */
	sym_node_t*	sym_node);	/* in: stored procedure name */
/**********************************************************************
Completes a query graph by adding query thread and fork nodes
above it and prepares the graph for running. The fork created is of
type QUE_FORK_MYSQL_INTERFACE. */

que_thr_t*
pars_complete_graph_for_exec(
/*=========================*/
				/* out: query thread node to run */
	que_node_t*	node,	/* in: root node for an incomplete
				query graph */
	trx_t*		trx,	/* in: transaction handle */
	mem_heap_t*	heap);	/* in: memory heap from which allocated */


/* Struct used to denote a reserved word in a parsing tree */
struct pars_res_word_struct{
	int	code;	/* the token code for the reserved word from
			pars0grm.h */
};

/* A predefined function or operator node in a parsing tree; this construct
is also used for some non-functions like the assignment ':=' */
struct func_node_struct{
	que_common_t	common;	/* type: QUE_NODE_FUNC */
	int		func;	/* token code of the function name */
	ulint		class;	/* class of the function */
	que_node_t*	args;	/* argument(s) of the function */
	UT_LIST_NODE_T(func_node_t) cond_list;
				/* list of comparison conditions; defined
				only for comparison operator nodes except,
				presently, for OPT_SCROLL_TYPE ones */
	UT_LIST_NODE_T(func_node_t) func_node_list;
				/* list of function nodes in a parsed
				query graph */
};

/* An order-by node in a select */
struct order_node_struct{
	que_common_t	common;	/* type: QUE_NODE_ORDER */
	sym_node_t*	column;	/* order-by column */
	ibool		asc;	/* TRUE if ascending, FALSE if descending */
};

/* Procedure definition node */
struct proc_node_struct{
	que_common_t	common;		/* type: QUE_NODE_PROC */
	sym_node_t*	proc_id;	/* procedure name symbol in the symbol
					table of this same procedure */
	sym_node_t*	param_list;	/* input and output parameters */
	que_node_t*	stat_list;	/* statement list */
	sym_tab_t*	sym_tab;	/* symbol table of this procedure */
};

/* elsif-element node */
struct elsif_node_struct{
	que_common_t	common;		/* type: QUE_NODE_ELSIF */
	que_node_t*	cond;		/* if condition */
	que_node_t*	stat_list;	/* statement list */
};

/* if-statement node */
struct if_node_struct{
	que_common_t	common;		/* type: QUE_NODE_IF */
	que_node_t*	cond;		/* if condition */
	que_node_t*	stat_list;	/* statement list */
	que_node_t*	else_part;	/* else-part statement list */
 	elsif_node_t*	elsif_list;	/* elsif element list */
};

/* while-statement node */
struct while_node_struct{
	que_common_t	common;		/* type: QUE_NODE_WHILE */
	que_node_t*	cond;		/* while condition */
	que_node_t*	stat_list;	/* statement list */
};

/* for-loop-statement node */
struct for_node_struct{
	que_common_t	common;		/* type: QUE_NODE_FOR */
	sym_node_t*	loop_var;	/* loop variable: this is the
					dereferenced symbol from the
					variable declarations, not the
					symbol occurrence in the for loop
					definition */
	que_node_t*	loop_start_limit;/* initial value of loop variable */
	que_node_t*	loop_end_limit;	/* end value of loop variable */
	int		loop_end_value;	/* evaluated value for the end value:
					it is calculated only when the loop
					is entered, and will not change within
					the loop */
	que_node_t*	stat_list;	/* statement list */
};

/* return-statement node */
struct return_node_struct{
	que_common_t	common;		/* type: QUE_NODE_RETURN */
};

/* Assignment statement node */
struct assign_node_struct{
	que_common_t	common;		/* type: QUE_NODE_ASSIGNMENT */
	sym_node_t*	var;		/* variable to set */
	que_node_t*	val;		/* value to assign */
};

/* Column assignment node */
struct col_assign_node_struct{
	que_common_t	common;		/* type: QUE_NODE_COL_ASSIGN */
	sym_node_t*	col;		/* column to set */
	que_node_t*	val;		/* value to assign */
};

/* Classes of functions */
#define PARS_FUNC_ARITH		1	/* +, -, *, / */
#define	PARS_FUNC_LOGICAL	2
#define PARS_FUNC_CMP		3
#define	PARS_FUNC_PREDEFINED	4	/* TO_NUMBER, SUBSTR, ... */
#define	PARS_FUNC_AGGREGATE	5	/* COUNT, DISTINCT, SUM */
#define	PARS_FUNC_OTHER		6	/* these are not real functions,
					e.g., := */

#ifndef UNIV_NONINL
#include "pars0pars.ic"
#endif

#endif 
