/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

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
@file pars/pars0sym.c
SQL parser symbol table

Created 12/15/1997 Heikki Tuuri
*******************************************************/

#include "pars0sym.h"

#ifdef UNIV_NONINL
#include "pars0sym.ic"
#endif

#include "mem0mem.h"
#include "data0type.h"
#include "data0data.h"
#include "pars0grm.h"
#include "pars0pars.h"
#include "que0que.h"
#include "eval0eval.h"
#include "row0sel.h"

/******************************************************************//**
Creates a symbol table for a single stored procedure or query.
@return	own: symbol table */
UNIV_INTERN
sym_tab_t*
sym_tab_create(
/*===========*/
	mem_heap_t*	heap)	/*!< in: memory heap where to create */
{
	sym_tab_t*	sym_tab;

	sym_tab = mem_heap_alloc(heap, sizeof(sym_tab_t));

	UT_LIST_INIT(sym_tab->sym_list);
	UT_LIST_INIT(sym_tab->func_node_list);

	sym_tab->heap = heap;

	return(sym_tab);
}

/******************************************************************//**
Frees the memory allocated dynamically AFTER parsing phase for variables
etc. in the symbol table. Does not free the mem heap where the table was
originally created. Frees also SQL explicit cursor definitions. */
UNIV_INTERN
void
sym_tab_free_private(
/*=================*/
	sym_tab_t*	sym_tab)	/*!< in, own: symbol table */
{
	sym_node_t*	sym;
	func_node_t*	func;

	sym = UT_LIST_GET_FIRST(sym_tab->sym_list);

	while (sym) {
		eval_node_free_val_buf(sym);

		if (sym->prefetch_buf) {
			sel_col_prefetch_buf_free(sym->prefetch_buf);
		}

		if (sym->cursor_def) {
			que_graph_free_recursive(sym->cursor_def);
		}

		sym = UT_LIST_GET_NEXT(sym_list, sym);
	}

	func = UT_LIST_GET_FIRST(sym_tab->func_node_list);

	while (func) {
		eval_node_free_val_buf(func);

		func = UT_LIST_GET_NEXT(func_node_list, func);
	}
}

/******************************************************************//**
Adds an integer literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_int_lit(
/*================*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	ulint		val)		/*!< in: integer value */
{
	sym_node_t*	node;
	byte*		data;

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;

	dtype_set(dfield_get_type(&node->common.val), DATA_INT, 0, 4);

	data = mem_heap_alloc(sym_tab->heap, 4);
	mach_write_to_4(data, val);

	dfield_set_data(&(node->common.val), data, 4);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	node->sym_table = sym_tab;

	return(node);
}

/******************************************************************//**
Adds a string literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_str_lit(
/*================*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	byte*		str,		/*!< in: string with no quotes around
					it */
	ulint		len)		/*!< in: string length */
{
	sym_node_t*	node;
	byte*		data;

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;

	dtype_set(dfield_get_type(&node->common.val),
		  DATA_VARCHAR, DATA_ENGLISH, 0);

	if (len) {
		data = mem_heap_alloc(sym_tab->heap, len);
		ut_memcpy(data, str, len);
	} else {
		data = NULL;
	}

	dfield_set_data(&(node->common.val), data, len);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	node->sym_table = sym_tab;

	return(node);
}

/******************************************************************//**
Add a bound literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_bound_lit(
/*==================*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	const char*	name,		/*!< in: name of bound literal */
	ulint*		lit_type)	/*!< out: type of literal (PARS_*_LIT) */
{
	sym_node_t*		node;
	pars_bound_lit_t*	blit;
	ulint			len = 0;

	blit = pars_info_get_bound_lit(sym_tab->info, name);
	ut_a(blit);

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;

	switch (blit->type) {
	case DATA_FIXBINARY:
		len = blit->length;
		*lit_type = PARS_FIXBINARY_LIT;
		break;

	case DATA_BLOB:
		*lit_type = PARS_BLOB_LIT;
		break;

	case DATA_VARCHAR:
		*lit_type = PARS_STR_LIT;
		break;

	case DATA_CHAR:
		ut_a(blit->length > 0);

		len = blit->length;
		*lit_type = PARS_STR_LIT;
		break;

	case DATA_INT:
		ut_a(blit->length > 0);
		ut_a(blit->length <= 8);

		len = blit->length;
		*lit_type = PARS_INT_LIT;
		break;

	default:
		ut_error;
	}

	dtype_set(dfield_get_type(&node->common.val),
		  blit->type, blit->prtype, len);

	dfield_set_data(&(node->common.val), blit->address, blit->length);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	node->sym_table = sym_tab;

	return(node);
}

/******************************************************************//**
Adds an SQL null literal to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_null_lit(
/*=================*/
	sym_tab_t*	sym_tab)	/*!< in: symbol table */
{
	sym_node_t*	node;

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;

	dfield_get_type(&node->common.val)->mtype = DATA_ERROR;

	dfield_set_null(&node->common.val);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	node->sym_table = sym_tab;

	return(node);
}

/******************************************************************//**
Adds an identifier to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_id(
/*===========*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	byte*		name,		/*!< in: identifier name */
	ulint		len)		/*!< in: identifier length */
{
	sym_node_t*	node;

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = FALSE;
	node->indirection = NULL;

	node->name = mem_heap_strdupl(sym_tab->heap, (char*) name, len);
	node->name_len = len;

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	dfield_set_null(&node->common.val);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;

	node->sym_table = sym_tab;

	return(node);
}

/******************************************************************//**
Add a bound identifier to a symbol table.
@return	symbol table node */
UNIV_INTERN
sym_node_t*
sym_tab_add_bound_id(
/*===========*/
	sym_tab_t*	sym_tab,	/*!< in: symbol table */
	const char*	name)		/*!< in: name of bound id */
{
	sym_node_t*		node;
	pars_bound_id_t*	bid;

	bid = pars_info_get_bound_id(sym_tab->info, name);
	ut_a(bid);

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = FALSE;
	node->indirection = NULL;

	node->name = mem_heap_strdup(sym_tab->heap, bid->id);
	node->name_len = strlen(node->name);

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	dfield_set_null(&node->common.val);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;

	node->sym_table = sym_tab;

	return(node);
}
