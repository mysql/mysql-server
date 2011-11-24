/*****************************************************************************

Copyright (c) 2007, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file fts/fts0ast.cc
Full Text Search parser helper file.

Created 2007/3/16 Sunny Bains.
***********************************************************************/

#include "mem0mem.h"
#include "fts0ast.h"
#include "fts0pars.h"

/******************************************************************//**
Create an empty fts_ast_node_t.
@return Create a new node */
static
fts_ast_node_t*
fts_ast_node_create(void)
/*=====================*/
{
	fts_ast_node_t*	node;

	node = (fts_ast_node_t*) ut_malloc(sizeof(*node));
	memset(node, 0x0, sizeof(*node));

	return(node);
}

/******************************************************************//**
Create a operator fts_ast_node_t.
@return new node */
UNIV_INTERN
fts_ast_node_t*
fts_ast_create_node_oper(
/*=====================*/
	void*		arg,			/*!< in: ast state instance */
	fts_ast_oper_t	oper)			/*!< in: ast operator */
{
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_OPER;
	node->oper = oper;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/******************************************************************//**
This function takes ownership of the ptr and is responsible
for free'ing it
@return new node */
UNIV_INTERN
fts_ast_node_t*
fts_ast_create_node_term(
/*=====================*/
	void*		arg,			/*!< in: ast state instance */
	const char*	ptr)			/*!< in: ast term string */
{
	ulint		len = strlen(ptr);
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_TERM;

	node->term.ptr = static_cast<byte*>(ut_malloc(len + 1));
	memcpy(node->term.ptr, ptr, len + 1);

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/******************************************************************//**
This function takes ownership of the ptr and is responsible
for free'ing it. 
@return new node */
UNIV_INTERN
fts_ast_node_t*
fts_ast_create_node_text(
/*=====================*/
	void*		arg,			/*!< in: ast state instance */
	const char*	ptr)			/*!< in: ast text string */
{
	/*!< We ignore the actual quotes "" */
	ulint		len = strlen(ptr) - 2;
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_TEXT;
	node->text.ptr = static_cast<byte*>(ut_malloc(len + 1));

	/*!< Skip copying the first quote */
	memcpy(node->text.ptr, ptr + 1, len);
	node->text.ptr[len] = 0;
	node->text.distance = ULINT_UNDEFINED;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/******************************************************************//**
This function takes ownership of the expr and is responsible
for free'ing it. 
@return new node */
UNIV_INTERN
fts_ast_node_t*
fts_ast_create_node_list(
/*=====================*/
	void*		arg,			/*!< in: ast state instance */
	fts_ast_node_t*	expr)			/*!< in: ast expr instance */
{
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_LIST;
	node->list.head = node->list.tail = expr;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/******************************************************************//**
Create a sub-expression list node. This function takes ownership of
expr and is responsible for deleting it. 
@return new node */
UNIV_INTERN
fts_ast_node_t*
fts_ast_create_node_subexp_list(
/*============================*/
	void*		arg,			/*!< in: ast state instance */
	fts_ast_node_t*	expr)			/*!< in: ast expr instance */
{
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_SUBEXP_LIST;
	node->list.head = node->list.tail = expr;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/******************************************************************//**
Free an expr list node elements. */
static
void
fts_ast_free_list(
/*==============*/
	fts_ast_node_t*	node)			/*!< in: ast node to free */
{
	ut_a(node->type == FTS_AST_LIST
	     || node->type == FTS_AST_SUBEXP_LIST);

	for (node = node->list.head;
	     node != NULL;
	     node = fts_ast_free_node(node)) {

		/*!< No op */
	}
}

/********************************************************************//**
Free a fts_ast_node_t instance.
@return next node to free */
UNIV_INTERN
fts_ast_node_t*
fts_ast_free_node(
/*==============*/
	fts_ast_node_t*	node)			/*!< in: the node to free */
{
	fts_ast_node_t*	next_node;

	switch(node->type) {
	case FTS_AST_TEXT:
		if (node->text.ptr) {
			ut_free(node->text.ptr);
			node->text.ptr = NULL;
		}
		break;

	case FTS_AST_TERM:
		if (node->term.ptr) {
			ut_free(node->term.ptr);
			node->term.ptr = NULL;
		}
		break;

	case FTS_AST_LIST:
	case FTS_AST_SUBEXP_LIST:
		fts_ast_free_list(node);
		node->list.head = node->list.tail = NULL;
		break;

	case FTS_AST_OPER:
		break;

	default:
		ut_error;
	}

	/*!< Get next node before freeing the node itself */
	next_node = node->next;

	ut_free(node);

	return(next_node);
}

/******************************************************************//**
This AST takes ownership of the expr and is responsible
for free'ing it. 
@return in param "list" */
UNIV_INTERN
fts_ast_node_t*
fts_ast_add_node(
/*=============*/
	fts_ast_node_t*	node,			/*!< in: list instance */
	fts_ast_node_t*	elem)			/*!< in: node to add to list */
{
	if (!elem) {
		return(NULL);
	}

	ut_a(!elem->next);
	ut_a(node->type == FTS_AST_LIST
	     || node->type == FTS_AST_SUBEXP_LIST);

	if (!node->list.head) {
		ut_a(!node->list.tail);

		node->list.head = node->list.tail = elem;
	} else {
		ut_a(node->list.tail);

		node->list.tail->next = elem;
		node->list.tail = elem;
	}

	return(node);
}

/******************************************************************//**
For tracking node allocations, in case there is an error during
parsing. */
UNIV_INTERN
void
fts_ast_state_add_node(
/*===================*/
	fts_ast_state_t*state,			/*!< in: ast instance */
	fts_ast_node_t*	node)			/*!< in: node to add to ast */
{
	if (!state->list.head) {
		ut_a(!state->list.tail);

		state->list.head = state->list.tail = node;
	} else {
		state->list.tail->next_alloc = node;
		state->list.tail = node;
	}
}

/******************************************************************//**
Set the wildcard attribute of a term. */
UNIV_INTERN
void
fts_ast_term_set_wildcard(
/*======================*/
	fts_ast_node_t*	node)			/*!< in/out: set attribute of
						a term node */
{
	ut_a(node->type == FTS_AST_TERM);
	ut_a(!node->term.wildcard);

	node->term.wildcard = TRUE;
}

/******************************************************************//**
Set the proximity attribute of a text node. */
UNIV_INTERN
void
fts_ast_term_set_distance(
/*======================*/
	fts_ast_node_t*	node,			/*!< in/out: text node */
	ulint		distance)		/*!< in: the text proximity
						distance */
{
	ut_a(node->type == FTS_AST_TEXT);
	ut_a(node->text.distance == ULINT_UNDEFINED);

	node->text.distance = distance;
}

/******************************************************************//**
Free node and expr allocations. */
UNIV_INTERN
void
fts_ast_state_free(
/*===============*/
	fts_ast_state_t*state)			/*!< in: ast state to free */
{
	fts_ast_node_t*	node = state->list.head;

	/* Free the nodes that were allocated during parsing. */
	while (node) {
		fts_ast_node_t*	next = node->next_alloc;

		if (node->type == FTS_AST_TEXT && node->text.ptr) {
			ut_free(node->text.ptr);
			node->text.ptr = NULL;
		} else if (node->type == FTS_AST_TERM && node->term.ptr) {
			ut_free(node->term.ptr);
			node->term.ptr = NULL;
		}

		ut_free(node);
		node = next;
	}

	state->root = state->list.head = state->list.tail = NULL;
}

/******************************************************************//**
Print an ast node. */
UNIV_INTERN
void
fts_ast_node_print(
/*===============*/
	fts_ast_node_t*	node)			/*!< in: ast node to print */
{
	switch(node->type) {
	case FTS_AST_TEXT:
		printf("TEXT: %s\n", node->text.ptr);
		break;

	case FTS_AST_TERM:
		printf("TERM: %s\n", node->term.ptr);
		break;

	case FTS_AST_LIST:
		printf("LIST: ");
		node = node->list.head;

		while (node) {
			fts_ast_node_print(node);
			node = node->next;
		}
		break;

	case FTS_AST_SUBEXP_LIST:
		printf("SUBEXP_LIST: ");
		node = node->list.head;

		while (node) {
			fts_ast_node_print(node);
			node = node->next;
		}
	case FTS_AST_OPER:
		printf("OPER: %d\n", node->oper);
		break;

	default:
		ut_error;
	}
}

/******************************************************************//**
Traverse the AST - in-order traversal. 
@return DB_SUCCESS if all went well */
UNIV_INTERN
ulint
fts_ast_visit(
/*==========*/
	fts_ast_oper_t		oper,		/*!< in: current operator */
	fts_ast_node_t*		node,		/*!< in: current root node */
	fts_ast_callback	visitor,	/*!< in: callback function */
	void*			arg)		/*!< in: arg for callback */
{
	ulint			error = DB_SUCCESS;

	ut_a(node->type == FTS_AST_LIST
	     || node->type == FTS_AST_SUBEXP_LIST);

	for (node = node->list.head;
	     node && error == DB_SUCCESS;
	     node = node->next) {

		if (node->type == FTS_AST_LIST) {
			error = fts_ast_visit(oper, node, visitor, arg);
		} else if (node->type == FTS_AST_SUBEXP_LIST) {
			error = fts_ast_visit_sub_exp(node, visitor, arg);
		} else if (node->type == FTS_AST_OPER) {
			oper = node->oper;
		} else {
			visitor(oper, node, arg);
		}
	}

	return(error);
}
