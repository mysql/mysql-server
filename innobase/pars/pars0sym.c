/******************************************************
SQL parser symbol table

(c) 1997 Innobase Oy

Created 12/15/1997 Heikki Tuuri
*******************************************************/

#include "pars0sym.h"

#ifdef UNIV_NONINL
#include "pars0sym.ic"
#endif

#include "mem0mem.h"
#include "data0type.h"
#include "data0data.h"
#include "pars0pars.h"
#include "que0que.h"
#include "eval0eval.h"
#include "row0sel.h"

/**********************************************************************
Creates a symbol table for a single stored procedure or query. */

sym_tab_t*
sym_tab_create(
/*===========*/
				/* out, own: symbol table */
	mem_heap_t*	heap)	/* in: memory heap where to create */
{
	sym_tab_t*	sym_tab;

	sym_tab = mem_heap_alloc(heap, sizeof(sym_tab_t));

	UT_LIST_INIT(sym_tab->sym_list);
	UT_LIST_INIT(sym_tab->func_node_list);

	sym_tab->heap = heap;

	return(sym_tab);
}

/**********************************************************************
Frees the memory allocated dynamically AFTER parsing phase for variables
etc. in the symbol table. Does not free the mem heap where the table was
originally created. Frees also SQL explicit cursor definitions. */

void
sym_tab_free_private(
/*=================*/
	sym_tab_t*	sym_tab)	/* in, own: symbol table */
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

/**********************************************************************
Adds an integer literal to a symbol table. */

sym_node_t*
sym_tab_add_int_lit(
/*================*/
					/* out: symbol table node */
	sym_tab_t*	sym_tab,	/* in: symbol table */
	ulint		val)		/* in: integer value */
{
	sym_node_t*	node;
	byte*		data;
	
	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;
	
	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;
	
	dtype_set(&(node->common.val.type), DATA_INT, 0, 4, 0);

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

/**********************************************************************
Adds a string literal to a symbol table. */

sym_node_t*
sym_tab_add_str_lit(
/*================*/
					/* out: symbol table node */
	sym_tab_t*	sym_tab,	/* in: symbol table */
	byte*		str,		/* in: string starting with a single
					quote; the string literal will
					extend to the next single quote, but
					the quotes are not included in it */
	ulint		len)		/* in: string length */
{
	sym_node_t*	node;
	byte*		data;
	ulint		i;
	
	ut_a(len > 1);
	ut_a(str[0] == '\'');

	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;

	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;
	
	dtype_set(&(node->common.val.type), DATA_VARCHAR, DATA_ENGLISH, 0, 0);

	for (i = 1;; i++) {
		ut_a(i < len);
		
		if (str[i] == '\'') {

			break;
		}
	}

	if (i > 1) {
		data = mem_heap_alloc(sym_tab->heap, i - 1);
		ut_memcpy(data, str + 1, i - 1);
	} else {
		data = NULL;
	}

	dfield_set_data(&(node->common.val), data, i - 1);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;
	
	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	node->sym_table = sym_tab;

	return(node);
}

/**********************************************************************
Adds an SQL null literal to a symbol table. */

sym_node_t*
sym_tab_add_null_lit(
/*=================*/
					/* out: symbol table node */
	sym_tab_t*	sym_tab)	/* in: symbol table */
{
	sym_node_t*	node;
	
	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;
	
	node->resolved = TRUE;
	node->token_type = SYM_LIT;

	node->indirection = NULL;
	
	node->common.val.type.mtype = DATA_ERROR;

	dfield_set_data(&(node->common.val), NULL, UNIV_SQL_NULL);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;
	
	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	node->sym_table = sym_tab;

	return(node);
}

/**********************************************************************
Adds an identifier to a symbol table. */

sym_node_t*
sym_tab_add_id(
/*===========*/
					/* out: symbol table node */
	sym_tab_t*	sym_tab,	/* in: symbol table */
	byte*		name,		/* in: identifier name */
	ulint		len)		/* in: identifier length */
{
	sym_node_t*	node;
	
	node = mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t));

	node->common.type = QUE_NODE_SYMBOL;
	
	node->name = mem_heap_alloc(sym_tab->heap, len + 1);
	node->resolved = FALSE;
	node->indirection = NULL;

	ut_memcpy(node->name, name, len);
	node->name[len] = '\0';

	node->name_len = len;

	UT_LIST_ADD_LAST(sym_list, sym_tab->sym_list, node);

	dfield_set_data(&(node->common.val), NULL, UNIV_SQL_NULL);

	node->common.val_buf_size = 0;
	node->prefetch_buf = NULL;
	node->cursor_def = NULL;
	
	node->sym_table = sym_tab;

	return(node);
}
