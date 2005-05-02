/******************************************************
SQL evaluator: evaluates simple data structures, like expressions, in
a query graph

(c) 1997 Innobase Oy

Created 12/29/1997 Heikki Tuuri
*******************************************************/

#ifndef eval0eval_h
#define eval0eval_h

#include "univ.i"
#include "que0types.h"
#include "pars0sym.h"
#include "pars0pars.h"

/*********************************************************************
Free the buffer from global dynamic memory for a value of a que_node,
if it has been allocated in the above function. The freeing for pushed
column values is done in sel_col_prefetch_buf_free. */

void
eval_node_free_val_buf(
/*===================*/
	que_node_t*	node);	/* in: query graph node */
/*********************************************************************
Evaluates a symbol table symbol. */
UNIV_INLINE
void
eval_sym(
/*=====*/
	sym_node_t*	sym_node);	/* in: symbol table node */
/*********************************************************************
Evaluates an expression. */
UNIV_INLINE
void
eval_exp(
/*=====*/
	que_node_t*	exp_node);	/* in: expression */
/*********************************************************************
Sets an integer value as the value of an expression node. */
UNIV_INLINE
void
eval_node_set_int_val(
/*==================*/
	que_node_t*	node,	/* in: expression node */
	lint		val);	/* in: value to set */
/*********************************************************************
Gets an integer value from an expression node. */
UNIV_INLINE
lint
eval_node_get_int_val(
/*==================*/
				/* out: integer value */
	que_node_t*	node);	/* in: expression node */
/*********************************************************************
Copies a binary string value as the value of a query graph node. Allocates a
new buffer if necessary. */
UNIV_INLINE
void
eval_node_copy_and_alloc_val(
/*=========================*/
	que_node_t*	node,	/* in: query graph node */
	byte*		str,	/* in: binary string */
	ulint		len);	/* in: string length or UNIV_SQL_NULL */
/*********************************************************************
Copies a query node value to another node. */
UNIV_INLINE
void
eval_node_copy_val(
/*===============*/
	que_node_t*	node1,	/* in: node to copy to */
	que_node_t*	node2);	/* in: node to copy from */
/*********************************************************************
Gets a iboolean value from a query node. */
UNIV_INLINE
ibool
eval_node_get_ibool_val(
/*===================*/
				/* out: iboolean value */
	que_node_t*	node);	/* in: query graph node */
/*********************************************************************
Evaluates a comparison node. */

ibool
eval_cmp(
/*=====*/
					/* out: the result of the comparison */
	func_node_t*	cmp_node);	/* in: comparison node */


#ifndef UNIV_NONINL
#include "eval0eval.ic"
#endif

#endif 
