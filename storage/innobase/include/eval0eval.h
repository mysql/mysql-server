/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/eval0eval.h
 SQL evaluator: evaluates simple data structures, like expressions, in
 a query graph

 Created 12/29/1997 Heikki Tuuri
 *******************************************************/

#ifndef eval0eval_h
#define eval0eval_h

#include "pars0pars.h"
#include "pars0sym.h"
#include "que0types.h"
#include "univ.i"

/** Free the buffer from global dynamic memory for a value of a que_node,
 if it has been allocated in the above function. The freeing for pushed
 column values is done in sel_col_prefetch_buf_free. */
void eval_node_free_val_buf(que_node_t *node); /*!< in: query graph node */
/** Evaluates a symbol table symbol. */
static inline void eval_sym(sym_node_t *sym_node); /*!< in: symbol table node */
/** Evaluates an expression. */
static inline void eval_exp(que_node_t *exp_node); /*!< in: expression */

/** Sets an integer value as the value of an expression node.
@param[in]      node    expression node
@param[in]      val     value to set */
static inline void eval_node_set_int_val(que_node_t *node, lint val);

/** Gets an integer value from an expression node.
 @return integer value */
static inline lint eval_node_get_int_val(
    que_node_t *node); /*!< in: expression node */

/** Copies a binary string value as the value of a query graph node. Allocates
a new buffer if necessary.
@param[in]      node    query graph node
@param[in]      str     binary string
@param[in]      len     string length or UNIV_SQL_NULL */
static inline void eval_node_copy_and_alloc_val(que_node_t *node,
                                                const byte *str, ulint len);

/** Copies a query node value to another node.
@param[in]      node1   node to copy to
@param[in]      node2   node to copy from */
static inline void eval_node_copy_val(que_node_t *node1, que_node_t *node2);

/** Gets a boolean value from a query node.
 @return boolean value */
static inline bool eval_node_get_bool_val(
    que_node_t *node); /*!< in: query graph node */
/** Evaluates a comparison node.
 @return the result of the comparison */
bool eval_cmp(func_node_t *cmp_node); /*!< in: comparison node */

#include "eval0eval.ic"

#endif
