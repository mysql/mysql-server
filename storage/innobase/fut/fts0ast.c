/******************************************************************//**
@file fut/fts0ast.c
Full Text Search parser helper file.

Created 3/16/2007 Sunny Bains.
***********************************************************************/

#include "mem0mem.h"
#include "fts0ast.h"
#include "fts0pars.h"

/********************************************************************
Create an empty fts_ast_node_t.*/
static
fts_ast_node_t*
fts_ast_node_create(
/*================*/
						/* out: Create a new node */
		void)
{
	fts_ast_node_t*	node;

	node = (fts_ast_node_t*) ut_malloc(sizeof(*node));
	memset(node, 0x0, sizeof(*node));

	return(node);
}

/********************************************************************
Create a operator fts_ast_node_t.*/

fts_ast_node_t*
fts_ast_create_node_oper(
/*=====================*/
						/* out: new node */
	void*		arg,			/* in: ast state instance */
	fts_ast_oper_t	oper)			/* in: ast operator */
{
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_OPER;
	node->oper = oper;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/********************************************************************
This function takes ownership of the ptr and is responsible
for free'ing it*/

fts_ast_node_t*
fts_ast_create_node_term(
/*=====================*/
						/* out: new node */
	void*		arg,			/* in: ast state instance */
	const char*	ptr)			/* in: ast term string */
{
	ulint		len = strlen(ptr);
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_TERM;

	node->term.ptr = ut_malloc(len + 1);
	memcpy(node->term.ptr, ptr, len + 1);

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/********************************************************************
This function takes ownership of the ptr and is responsible
for free'ing it. */

fts_ast_node_t*
fts_ast_create_node_text(
/*=====================*/
						/* out: new node */
	void*		arg,			/* in: ast state instance */
	const char*	ptr)			/* in: ast text string */
{
	/* We ignore the actual quotes "" */
	ulint		len = strlen(ptr) - 2;
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_TEXT;
	node->text.ptr = ut_malloc(len + 1);

	/* Skip copying the first quote */
	memcpy(node->text.ptr, ptr + 1, len);
	node->text.ptr[len] = 0;
	node->text.distance = ULINT_UNDEFINED;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/********************************************************************
This function takes ownership of the expr and is responsible
for free'ing it. */

fts_ast_node_t*
fts_ast_create_node_list(
/*=====================*/
						/* out: new node */
	void*		arg,			/* in: ast state instance */
	fts_ast_node_t*	expr)			/* in: ast expr instance */
{
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_LIST;
	node->list.head = node->list.tail = expr;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/********************************************************************
Create a sub-expression list node. This function takes ownership of
expr and is responsible for deleting it. */

fts_ast_node_t*
fts_ast_create_node_subexp_list(
/*============================*/
						/* out: new node */
	void*		arg,			/* in: ast state instance */
	fts_ast_node_t*	expr)			/* in: ast expr instance */
{
	fts_ast_node_t*	node = fts_ast_node_create();

	node->type = FTS_AST_SUBEXP_LIST;
	node->list.head = node->list.tail = expr;

	fts_ast_state_add_node((fts_ast_state_t*)arg, node);

	return(node);
}

/********************************************************************
Free an expr list node elements. */
static
void
fts_ast_free_list(
/*==============*/
	fts_ast_node_t*	node)
{
	ut_a(node->type == FTS_AST_LIST
	     || node->type == FTS_AST_SUBEXP_LIST);

	node = node->list.head;

	while (node) {
		fts_ast_free_node(node);

		node = node->next;
	}
}

/********************************************************************
Free a fts_ast_node_t instance. */

void
fts_ast_free_node(
/*==============*/
	fts_ast_node_t*	node)			/* in: the node to free */
{
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

	ut_free(node);
}

/********************************************************************
This AST takes ownership of the expr and is responsible
for free'ing it. */

fts_ast_node_t*
fts_ast_add_node(
/*=============*/
						/* out: in param "list" */
	fts_ast_node_t*	node,			/* in: list instance */
	fts_ast_node_t*	elem)			/* in: node to add to list */
{
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

/********************************************************************
For tracking node allocations, in case there is an error during
parsing. */

void
fts_ast_state_add_node(
/*===================*/
	fts_ast_state_t*state,			/* in: ast instance */
	fts_ast_node_t*	node)			/* in: node to add to ast */
{
	if (!state->list.head) {
		ut_a(!state->list.tail);

		state->list.head = state->list.tail = node;
	} else {
		state->list.tail->next_alloc = node;
		state->list.tail = node;
	}
}

/********************************************************************
Set the wildcard attribute of a term. */

void
fts_ast_term_set_wildcard(
/*======================*/
	fts_ast_node_t*	node)			/* in/out: set attribute of
						a term node */
{
	ut_a(node->type == FTS_AST_TERM);
	ut_a(!node->term.wildcard);

	node->term.wildcard = TRUE;
}

/********************************************************************
Set the proximity attribute of a text node. */

void
fts_ast_term_set_distance(
/*======================*/
	fts_ast_node_t*	node,			/* in/out: text node */
	ulint		distance)		/* in: the text proximity
						distance */
{
	ut_a(node->type == FTS_AST_TEXT);
	ut_a(node->text.distance == ULINT_UNDEFINED);

	node->text.distance = distance;
}

/********************************************************************
Free node and expr allocations. */

void
fts_ast_state_free(
/*===============*/
	fts_ast_state_t*state)			/* in: ast state to free */
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

/********************************************************************
Print an ast node. */

void
fts_ast_node_print(
/*===============*/
	fts_ast_node_t*	node)			/* in: ast node to print */
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

/********************************************************************
Traverse the AST - in-order traversal. */

ulint
fts_ast_visit(
/*==========*/
						/* out: DB_SUCCESS if all
						went well */
	fts_ast_oper_t		oper,		/* in: current operator */
	fts_ast_node_t*		node,		/* in: current root node */
	fts_ast_callback	visitor,	/* in: callback function */
	void*			arg)		/* in: arg for callback */
{
	ulint			error = DB_SUCCESS;

	ut_a(node->type == FTS_AST_LIST
	     || node->type == FTS_AST_SUBEXP_LIST);

	node = node->list.head;

	while (node && error == DB_SUCCESS) {
		if (node->type == FTS_AST_LIST) {
			error = fts_ast_visit(oper, node, visitor, arg);
		} else if (node->type == FTS_AST_SUBEXP_LIST) {
			error = fts_ast_visit_sub_exp(node, visitor, arg);
		} else if (node->type == FTS_AST_OPER) {
			oper = node->oper;
		} else {
			visitor(oper, node, arg);
		}

		node = node->next;
	}

	return(error);
}
