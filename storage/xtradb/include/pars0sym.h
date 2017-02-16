/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file include/pars0sym.h
SQL parser symbol table

Created 12/15/1997 Heikki Tuuri
*******************************************************/

#ifndef pars0sym_h
#define pars0sym_h

#include "univ.i"
#include "que0types.h"
#include "usr0types.h"
#include "dict0types.h"
#include "pars0types.h"
#include "row0types.h"

/******************************************************************//**
Creates a symbol table for a single stored procedure or query.
@return	own: symbol table */
UNIV_INTERN
sym_tab_t*
sym_tab_create(
/*===========*/
	mem_heap_t*	heap);	/*!< in: memory heap where to create */
/******************************************************************//**
Frees the memory allocated dynamically AFTER parsing phase for variables
etc. in the symbol table. Does not free the mem heap where the table was
originally created. Frees also SQL explicit cursor definitions. */
UNIV_INTERN
void
sym_tab_free_private(
/*=================*/
	sym_tab_t*	sym_tab);	/*!< in, own: symbol table */
/******************************************************************//**
Adds an integer literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_int_lit(
/*================*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	ulint		val);		/*!< in: integer value */
/******************************************************************//**
Adds an string literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_str_lit(
/*================*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	byte*		str,		/*!< in: string with no quotes around
					it */
	ulint		len);		/*!< in: string length */
/******************************************************************//**
Add a bound literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_bound_lit(
/*==================*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	const char*	name,		/*!< in: name of bound literal */
	ulint*		lit_type);	/*!< out: type of literal (PARS_*_LIT) */
/******************************************************************//**
Adds an SQL null literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_null_lit(
/*=================*/
	sym_tab_t*	sym_tab);	/*!< in: symbol table */
/******************************************************************//**
Adds an identifier to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_id(
/*===========*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	byte*		name,		/*!< in: identifier name */
	ulint		len);		/*!< in: identifier length */

/******************************************************************//**
Add a bound identifier to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_bound_id(
/*===========*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	const char*	name);		/*!< in: name of bound id */

/** Index of sym_node_struct::field_nos corresponding to the clustered index */
#define	SYM_CLUST_FIELD_NO	0
/** Index of sym_node_struct::field_nos corresponding to a secondary index */
#define	SYM_SEC_FIELD_NO	1

/** Types of a symbol table node */
enum sym_tab_entry {
	SYM_VAR = 91,		/*!< declared parameter or local
				variable of a procedure */
	SYM_IMPLICIT_VAR,	/*!< storage for a intermediate result
				of a calculation */
	SYM_LIT,		/*!< literal */
	SYM_TABLE,		/*!< database table name */
	SYM_COLUMN,		/*!< database table name */
	SYM_CURSOR,		/*!< named cursor */
	SYM_PROCEDURE_NAME,	/*!< stored procedure name */
	SYM_INDEX,		/*!< database index name */
	SYM_FUNCTION		/*!< user function name */
};

/** Symbol table node */
struct sym_node_struct{
	que_common_t			common;		/*!< node type:
							QUE_NODE_SYMBOL */
	/* NOTE: if the data field in 'common.val' is not NULL and the symbol
	table node is not for a temporary column, the memory for the value has
	been allocated from dynamic memory and it should be freed when the
	symbol table is discarded */

	/* 'alias' and 'indirection' are almost the same, but not quite.
	'alias' always points to the primary instance of the variable, while
	'indirection' does the same only if we should use the primary
	instance's values for the node's data. This is usually the case, but
	when initializing a cursor (e.g., "DECLARE CURSOR c IS SELECT * FROM
	t WHERE id = x;"), we copy the values from the primary instance to
	the cursor's instance so that they are fixed for the duration of the
	cursor, and set 'indirection' to NULL. If we did not, the value of
	'x' could change between fetches and things would break horribly.

	TODO: It would be cleaner to make 'indirection' a boolean field and
	always use 'alias' to refer to the primary node. */

	sym_node_t*			indirection;	/*!< pointer to
							another symbol table
							node which contains
							the value for this
							node, NULL otherwise */
	sym_node_t*			alias;		/*!< pointer to
							another symbol table
							node for which this
							node is an alias,
							NULL otherwise */
	UT_LIST_NODE_T(sym_node_t)	col_var_list;	/*!< list of table
							columns or a list of
							input variables for an
							explicit cursor */
	ibool				copy_val;	/*!< TRUE if a column
							and its value should
							be copied to dynamic
							memory when fetched */
	ulint				field_nos[2];	/*!< if a column, in
							the position
							SYM_CLUST_FIELD_NO is
							the field number in the
							clustered index; in
							the position
							SYM_SEC_FIELD_NO
							the field number in the
							non-clustered index to
							use first; if not found
							from the index, then
							ULINT_UNDEFINED */
	ibool				resolved;	/*!< TRUE if the
							meaning of a variable
							or a column has been
							resolved; for literals
							this is always TRUE */
	enum sym_tab_entry		token_type;	/*!< type of the
							parsed token */
	const char*			name;		/*!< name of an id */
	ulint				name_len;	/*!< id name length */
	dict_table_t*			table;		/*!< table definition
							if a table id or a
							column id */
	ulint				col_no;		/*!< column number if a
							column */
	sel_buf_t*			prefetch_buf;	/*!< NULL, or a buffer
							for cached column
							values for prefetched
							rows */
	sel_node_t*			cursor_def;	/*!< cursor definition
							select node if a
							named cursor */
	ulint				param_type;	/*!< PARS_INPUT,
							PARS_OUTPUT, or
							PARS_NOT_PARAM if not a
							procedure parameter */
	sym_tab_t*			sym_table;	/*!< back pointer to
							the symbol table */
	UT_LIST_NODE_T(sym_node_t)	sym_list;	/*!< list of symbol
							nodes */
};

/** Symbol table */
struct sym_tab_struct{
	que_t*			query_graph;
					/*!< query graph generated by the
					parser */
	const char*		sql_string;
					/*!< SQL string to parse */
	size_t			string_len;
					/*!< SQL string length */
	int			next_char_pos;
					/*!< position of the next character in
					sql_string to give to the lexical
					analyzer */
	pars_info_t*		info;	/*!< extra information, or NULL */
	sym_node_list_t		sym_list;
					/*!< list of symbol nodes in the symbol
					table */
	UT_LIST_BASE_NODE_T(func_node_t)
				func_node_list;
					/*!< list of function nodes in the
					parsed query graph */
	mem_heap_t*		heap;	/*!< memory heap from which we can
					allocate space */
};

#ifndef UNIV_NONINL
#include "pars0sym.ic"
#endif

#endif
