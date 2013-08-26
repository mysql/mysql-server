/*****************************************************************************

Copyright (c) 2007, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file include/fts0ast.h
The FTS query parser (AST) abstract syntax tree routines

Created 2007/03/16/03 Sunny Bains
*******************************************************/

#ifndef INNOBASE_FST0AST_H
#define INNOBASE_FST0AST_H

#include "mem0mem.h"
#include "ha_prototypes.h"

/* The type of AST Node */
enum fts_ast_type_t {
	FTS_AST_OPER,				/*!< Operator */
	FTS_AST_NUMB,				/*!< Number */
	FTS_AST_TERM,				/*!< Term (or word) */
	FTS_AST_TEXT,				/*!< Text string */
	FTS_AST_LIST,				/*!< Expression list */
	FTS_AST_SUBEXP_LIST			/*!< Sub-Expression list */
};

/* The FTS query operators that we support */
enum fts_ast_oper_t {
	FTS_NONE,				/*!< No operator */

	FTS_IGNORE,				/*!< Ignore rows that contain
						this word */

	FTS_EXIST,				/*!< Include rows that contain
						this word */

	FTS_NEGATE,				/*!< Include rows that contain
						this word but rank them
						lower*/

	FTS_INCR_RATING,			/*!< Increase the rank for this
						word*/

	FTS_DECR_RATING,			/*!< Decrease the rank for this
						word*/

	FTS_DISTANCE,				/*!< Proximity distance */
	FTS_IGNORE_SKIP,			/*!< Transient node operator
						signifies that this is a
						FTS_IGNORE node, and ignored in
						the first pass of
						fts_ast_visit() */
	FTS_EXIST_SKIP				/*!< Transient node operator
						signifies that this ia a
						FTS_EXIST node, and ignored in
						the first pass of
						fts_ast_visit() */
};

/* Data types used by the FTS parser */
struct fts_lexer_t;
struct fts_ast_node_t;
struct fts_ast_state_t;

typedef dberr_t (*fts_ast_callback)(fts_ast_oper_t, fts_ast_node_t*, void*);

/********************************************************************
Parse the string using the lexer setup within state.*/
int
fts_parse(
/*======*/
						/* out: 0 on OK, 1 on error */
	fts_ast_state_t* state);		/*!< in: ast state instance.*/

/********************************************************************
Create an AST operator node */
extern
fts_ast_node_t*
fts_ast_create_node_oper(
/*=====================*/
	void*		arg,			/*!< in: ast state */
	fts_ast_oper_t	oper);			/*!< in: ast operator */
/********************************************************************
Create an AST term node, makes a copy of ptr */
extern
fts_ast_node_t*
fts_ast_create_node_term(
/*=====================*/
	void*		arg,			/*!< in: ast state */
	const char*	ptr);			/*!< in: term string */
/********************************************************************
Create an AST text node */
extern
fts_ast_node_t*
fts_ast_create_node_text(
/*=====================*/
	void*		arg,			/*!< in: ast state */
	const char*	ptr);			/*!< in: text string */
/********************************************************************
Create an AST expr list node */
extern
fts_ast_node_t*
fts_ast_create_node_list(
/*=====================*/
	void*		arg,			/*!< in: ast state */
	fts_ast_node_t*	expr);			/*!< in: ast expr */
/********************************************************************
Create a sub-expression list node. This function takes ownership of
expr and is responsible for deleting it. */
extern
fts_ast_node_t*
fts_ast_create_node_subexp_list(
/*============================*/
						/* out: new node */
	void*		arg,			/*!< in: ast state instance */
	fts_ast_node_t*	expr);			/*!< in: ast expr instance */
/********************************************************************
Set the wildcard attribute of a term.*/
extern
void
fts_ast_term_set_wildcard(
/*======================*/
	fts_ast_node_t*	node);			/*!< in: term to change */
/********************************************************************
Set the proximity attribute of a text node. */

void
fts_ast_term_set_distance(
/*======================*/
	fts_ast_node_t*	node,			/*!< in/out: text node */
	ulint		distance);		/*!< in: the text proximity
						distance */
/********************************************************************//**
Free a fts_ast_node_t instance.
@return next node to free */
UNIV_INTERN
fts_ast_node_t*
fts_ast_free_node(
/*==============*/
	fts_ast_node_t*	node);			/*!< in: node to free */
/********************************************************************
Add a sub-expression to an AST*/
extern
fts_ast_node_t*
fts_ast_add_node(
/*=============*/
	fts_ast_node_t*	list,			/*!< in: list node instance */
	fts_ast_node_t*	node);			/*!< in: (sub) expr to add */
/********************************************************************
Print the AST node recursively.*/
extern
void
fts_ast_node_print(
/*===============*/
	fts_ast_node_t*	node);			/*!< in: ast node to print */
/********************************************************************
For tracking node allocations, in case there is an during parsing.*/
extern
void
fts_ast_state_add_node(
/*===================*/
	fts_ast_state_t*state,			/*!< in: ast state instance */
	fts_ast_node_t*	node);			/*!< in: node to add to state */
/********************************************************************
Free node and expr allocations.*/
extern
void
fts_ast_state_free(
/*===============*/
	fts_ast_state_t*state);			/*!< in: state instance
						to free */
/******************************************************************//**
Traverse the AST - in-order traversal.
@return DB_SUCCESS if all went well */
UNIV_INTERN
dberr_t
fts_ast_visit(
/*==========*/
	fts_ast_oper_t		oper,		/*!< in: FTS operator */
	fts_ast_node_t*		node,		/*!< in: instance to traverse*/
	fts_ast_callback	visitor,	/*!< in: callback */
	void*			arg,		/*!< in: callback arg */
	bool*			has_ignore)	/*!< out: whether we encounter
						and ignored processing an
						operator, currently we only
						ignore FTS_IGNORE operator */
	__attribute__((nonnull, warn_unused_result));
/*****************************************************************//**
Process (nested) sub-expression, create a new result set to store the
sub-expression result by processing nodes under current sub-expression
list. Merge the sub-expression result with that of parent expression list.
@return DB_SUCCESS if all went well */
UNIV_INTERN
dberr_t
fts_ast_visit_sub_exp(
/*==================*/
	fts_ast_node_t*		node,		/*!< in: instance to traverse*/
	fts_ast_callback	visitor,	/*!< in: callback */
	void*			arg)		/*!< in: callback arg */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************
Create a lex instance.*/
UNIV_INTERN
fts_lexer_t*
fts_lexer_create(
/*=============*/
	ibool		boolean_mode,		/*!< in: query type */
	const byte*	query,			/*!< in: query string */
	ulint		query_len)		/*!< in: query string len */
	__attribute__((nonnull, malloc, warn_unused_result));
/********************************************************************
Free an fts_lexer_t instance.*/
UNIV_INTERN
void
fts_lexer_free(
/*===========*/
	fts_lexer_t*	fts_lexer)		/*!< in: lexer instance to
						free */
	__attribute__((nonnull));

/* Query term type */
struct fts_ast_term_t {
	byte*		ptr;			/*!< Pointer to term string.*/
	ibool		wildcard;		/*!< TRUE if wild card set.*/
};

/* Query text type */
struct fts_ast_text_t {
	byte*		ptr;			/*!< Pointer to term string.*/
	ulint		distance;		/*!< > 0 if proximity distance
						set */
};

/* The list of nodes in an expr list */
struct fts_ast_list_t {
	fts_ast_node_t*	head;			/*!< Children list head */
	fts_ast_node_t*	tail;			/*!< Children list tail */
};

/* FTS AST node to store the term, text, operator and sub-expressions.*/
struct fts_ast_node_t {
	fts_ast_type_t	type;			/*!< The type of node */
	fts_ast_text_t	text;			/*!< Text node */
	fts_ast_term_t	term;			/*!< Term node */
	fts_ast_oper_t	oper;			/*!< Operator value */
	fts_ast_list_t	list;			/*!< Expression list */
	fts_ast_node_t*	next;			/*!< Link for expr list */
	fts_ast_node_t*	next_alloc;		/*!< For tracking allocations */
	bool		visited;		/*!< whether this node is
						already processed */
};

/* To track state during parsing */
struct fts_ast_state_t {
	mem_heap_t*	heap;			/*!< Heap to use for alloc */
	fts_ast_node_t*	root;			/*!< If all goes OK, then this
						will point to the root.*/

	fts_ast_list_t	list;			/*!< List of nodes allocated */

	fts_lexer_t*	lexer;			/*!< Lexer callback + arg */
	CHARSET_INFO*	charset;		/*!< charset used for
						tokenization */
};

#endif /* INNOBASE_FSTS0AST_H */
