/******************************************************
SQL evaluator: evaluates simple data structures, like expressions, in
a query graph

(c) 1997 Innobase Oy

Created 12/29/1997 Heikki Tuuri
*******************************************************/

#include "eval0eval.h"

#ifdef UNIV_NONINL
#include "eval0eval.ic"
#endif

#include "data0data.h"
#include "row0sel.h"

/* The RND function seed */
ulint	eval_rnd 	= 128367121;

/* Dummy adress used when we should allocate a buffer of size 0 in
the function below */

byte	eval_dummy;

/*********************************************************************
Allocate a buffer from global dynamic memory for a value of a que_node.
NOTE that this memory must be explicitly freed when the query graph is
freed. If the node already has an allocated buffer, that buffer is freed
here. NOTE that this is the only function where dynamic memory should be
allocated for a query node val field. */

byte*
eval_node_alloc_val_buf(
/*====================*/
				/* out: pointer to allocated buffer */
	que_node_t*	node,	/* in: query graph node; sets the val field
				data field to point to the new buffer, and
				len field equal to size */
	ulint		size)	/* in: buffer size */
{
	dfield_t*	dfield;
	byte*		data;

	ut_ad(que_node_get_type(node) == QUE_NODE_SYMBOL
	      || que_node_get_type(node) == QUE_NODE_FUNC);

	dfield = que_node_get_val(node);

	data = dfield_get_data(dfield);
	
	if (data && data != &eval_dummy) {
		mem_free(data);
	}

	if (size == 0) {
		data = &eval_dummy;
	} else {
		data = mem_alloc(size);
	}

	que_node_set_val_buf_size(node, size);

	dfield_set_data(dfield, data, size);

	return(data);
}

/*********************************************************************
Free the buffer from global dynamic memory for a value of a que_node,
if it has been allocated in the above function. The freeing for pushed
column values is done in sel_col_prefetch_buf_free. */

void
eval_node_free_val_buf(
/*===================*/
	que_node_t*	node)	/* in: query graph node */
{
	dfield_t*	dfield;
	byte*		data;

	ut_ad(que_node_get_type(node) == QUE_NODE_SYMBOL
	      || que_node_get_type(node) == QUE_NODE_FUNC);

	dfield = que_node_get_val(node);

	data = dfield_get_data(dfield);
	
	if (que_node_get_val_buf_size(node) > 0) {
		ut_a(data);
	
		mem_free(data);
	}
}

/*********************************************************************
Evaluates a comparison node. */

ibool
eval_cmp(
/*=====*/
					/* out: the result of the comparison */
	func_node_t*	cmp_node)	/* in: comparison node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	int		res;
	ibool		val;
	int		func;
	
	ut_ad(que_node_get_type(cmp_node) == QUE_NODE_FUNC);

	arg1 = cmp_node->args;
	arg2 = que_node_get_next(arg1);

	res = cmp_dfield_dfield(que_node_get_val(arg1),
						que_node_get_val(arg2));
	val = TRUE;

	func = cmp_node->func;
			
	if (func == '=') {
		if (res != 0) {
			val = FALSE;
		}
	} else if (func == '<') {
		if (res != -1) {
			val = FALSE;
		}
	} else if (func == PARS_LE_TOKEN) {
		if (res == 1) {
			val = FALSE;
		}
	} else if (func == PARS_NE_TOKEN) {
		if (res == 0) {
			val = FALSE;
		}
	} else if (func == PARS_GE_TOKEN) {
		if (res == -1) {
			val = FALSE;
		}
	} else {
		ut_ad(func == '>');
		
		if (res != 1) {
			val = FALSE;
		}
	}

	eval_node_set_ibool_val(cmp_node, val);

	return(val);
}

/*********************************************************************
Evaluates a logical operation node. */
UNIV_INLINE
void
eval_logical(
/*=========*/
	func_node_t*	logical_node)	/* in: logical operation node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	ibool		val1;
	ibool		val2;
	ibool		val;
	int		func;

	ut_ad(que_node_get_type(logical_node) == QUE_NODE_FUNC);

	arg1 = logical_node->args;
	arg2 = que_node_get_next(arg1); /* arg2 is NULL if func is 'NOT' */

	val1 = eval_node_get_ibool_val(arg1);

	if (arg2) {
		val2 = eval_node_get_ibool_val(arg2);
	}

	func = logical_node->func;

	if (func == PARS_AND_TOKEN) {
		val = val1 & val2;
	} else if (func == PARS_OR_TOKEN) {
		val = val1 | val2;
	} else if (func == PARS_NOT_TOKEN) {
		val = TRUE - val1;
	} else {
		ut_error;
	}

	eval_node_set_ibool_val(logical_node, val);
}

/*********************************************************************
Evaluates an arithmetic operation node. */
UNIV_INLINE
void
eval_arith(
/*=======*/
	func_node_t*	arith_node)	/* in: arithmetic operation node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	lint		val1;
	lint		val2;
	lint		val;
	int		func;

	ut_ad(que_node_get_type(arith_node) == QUE_NODE_FUNC);

	arg1 = arith_node->args;
	arg2 = que_node_get_next(arg1); /* arg2 is NULL if func is unary '-' */

	val1 = eval_node_get_int_val(arg1);

	if (arg2) {
		val2 = eval_node_get_int_val(arg2);
	}

	func = arith_node->func;

	if (func == '+') {
		val = val1 + val2;
	} else if ((func == '-') && arg2) {
		val = val1 - val2;
	} else if (func == '-') {
		val = -val1;
	} else if (func == '*') {
		val = val1 * val2;
	} else {
		ut_ad(func == '/');
		val = val1 / val2;
	}

	eval_node_set_int_val(arith_node, val);
}

/*********************************************************************
Evaluates an aggregate operation node. */
UNIV_INLINE
void
eval_aggregate(
/*===========*/
	func_node_t*	node)	/* in: aggregate operation node */
{
	que_node_t*	arg;
	lint		val;
	lint		arg_val;
	int		func;

	ut_ad(que_node_get_type(node) == QUE_NODE_FUNC);

	val = eval_node_get_int_val(node);

	func = node->func;

	if (func == PARS_COUNT_TOKEN) {

		val = val + 1;
	} else {
		ut_ad(func == PARS_SUM_TOKEN);

		arg = node->args;
		arg_val = eval_node_get_int_val(arg);

		val = val + arg_val;
	}
	
	eval_node_set_int_val(node, val);
}

/*********************************************************************
Evaluates a predefined function node where the function is not relevant
in benchmarks. */
static
void
eval_predefined_2(
/*==============*/
	func_node_t*	func_node)	/* in: predefined function node */
{
	que_node_t*	arg;
	que_node_t*	arg1;
	que_node_t*	arg2;
	lint		int_val;
	byte*		data;
	ulint		len1;
	ulint		len2;
	int		func;
	ulint		i;

	ut_ad(que_node_get_type(func_node) == QUE_NODE_FUNC);

	arg1 = func_node->args;

	if (arg1) {
		arg2 = que_node_get_next(arg1);
	}

	func = func_node->func;

	if (func == PARS_PRINTF_TOKEN) {

		arg = arg1;
	
		while (arg) {
			dfield_print(que_node_get_val(arg));

			arg = que_node_get_next(arg);
		}

		printf("\n");	
		
	} else if (func == PARS_ASSERT_TOKEN) {

		if (!eval_node_get_ibool_val(arg1)) {
			printf("SQL assertion fails in a stored procedure!\n");
		}
 
		ut_a(eval_node_get_ibool_val(arg1));
		
		/* This function, or more precisely, a debug procedure,
		returns no value */

	} else if (func == PARS_RND_TOKEN) {

		len1 = (ulint)eval_node_get_int_val(arg1);
		len2 = (ulint)eval_node_get_int_val(arg2);

		ut_ad(len2 >= len1);

		if (len2 > len1) {		
			int_val = (lint)(len1 +
					(eval_rnd % (len2 - len1 + 1)));
		} else {
			int_val = (lint)len1;
		}

		eval_rnd = ut_rnd_gen_next_ulint(eval_rnd);

		eval_node_set_int_val(func_node, int_val);

	} else if (func == PARS_RND_STR_TOKEN) {

		len1 = (ulint)eval_node_get_int_val(arg1);

		data = eval_node_ensure_val_buf(func_node, len1);

		for (i = 0; i < len1; i++) {
			data[i] = (byte)(97 + (eval_rnd % 3));

			eval_rnd = ut_rnd_gen_next_ulint(eval_rnd);
		}
	} else {
		ut_error;
	}
}

/*********************************************************************
Evaluates a notfound-function node. */
UNIV_INLINE
void
eval_notfound(
/*==========*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	sym_node_t*	cursor;
	sel_node_t*	sel_node;
	ibool		ibool_val;

	arg1 = func_node->args;
	arg2 = que_node_get_next(arg1);

	ut_ad(func_node->func == PARS_NOTFOUND_TOKEN);

	cursor = arg1;

	ut_ad(que_node_get_type(cursor) == QUE_NODE_SYMBOL);

	if (cursor->token_type == SYM_LIT) {
		
		ut_ad(ut_memcmp(dfield_get_data(que_node_get_val(cursor)),
							"SQL", 3) == 0);

		sel_node = cursor->sym_table->query_graph->last_sel_node;
	} else {
		sel_node = cursor->alias->cursor_def;
	}

	if (sel_node->state == SEL_NODE_NO_MORE_ROWS) {
		ibool_val = TRUE;
	} else {
		ibool_val = FALSE;
	}

	eval_node_set_ibool_val(func_node, ibool_val);
}

/*********************************************************************
Evaluates a substr-function node. */
UNIV_INLINE
void
eval_substr(
/*========*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	que_node_t*	arg3;
	dfield_t*	dfield;
	byte*		str1;
	ulint		len1;
	ulint		len2;

	arg1 = func_node->args;
	arg2 = que_node_get_next(arg1);

	ut_ad(func_node->func == PARS_SUBSTR_TOKEN);

	arg3 = que_node_get_next(arg2);

	str1 = dfield_get_data(que_node_get_val(arg1));
		
	len1 = (ulint)eval_node_get_int_val(arg2);
	len2 = (ulint)eval_node_get_int_val(arg3);

	dfield = que_node_get_val(func_node);

	dfield_set_data(dfield, str1 + len1, len2);
}

/*********************************************************************
Evaluates a replstr-procedure node. */
static
void
eval_replstr(
/*=========*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	que_node_t*	arg3;
	que_node_t*	arg4;
	byte*		str1;
	byte*		str2;
	ulint		len1;
	ulint		len2;

	arg1 = func_node->args;
	arg2 = que_node_get_next(arg1);

	ut_ad(que_node_get_type(arg1) == QUE_NODE_SYMBOL);

	arg3 = que_node_get_next(arg2);
	arg4 = que_node_get_next(arg3);

	str1 = dfield_get_data(que_node_get_val(arg1));
	str2 = dfield_get_data(que_node_get_val(arg2));

	len1 = (ulint)eval_node_get_int_val(arg3);
	len2 = (ulint)eval_node_get_int_val(arg4);

	if ((dfield_get_len(que_node_get_val(arg1)) < len1 + len2)
			|| (dfield_get_len(que_node_get_val(arg2)) < len2)) {

		ut_error;
	}

	ut_memcpy(str1 + len1, str2, len2);
}
		
/*********************************************************************
Evaluates an instr-function node. */
static
void
eval_instr(
/*=======*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	dfield_t*	dfield1;
	dfield_t*	dfield2;
	lint		int_val;
	byte*		str1;
	byte*		str2;
	byte		match_char;
	ulint		len1;
	ulint		len2;
	ulint		i;
	ulint		j;

	arg1 = func_node->args;
	arg2 = que_node_get_next(arg1);

	dfield1 = que_node_get_val(arg1);
	dfield2 = que_node_get_val(arg2);
	
	str1 = dfield_get_data(dfield1);
	str2 = dfield_get_data(dfield2);

	len1 = dfield_get_len(dfield1);
	len2 = dfield_get_len(dfield2);

	if (len2 == 0) {
		ut_error;
	}

	match_char = str2[0];

	for (i = 0; i < len1; i++) {
		/* In this outer loop, the number of matched characters is 0 */

		if (str1[i] == match_char) {

			if (i + len2 > len1) {

				break;
			}

			for (j = 1;; j++) {
				/* We have already matched j characters */

				if (j == len2) {
					int_val = i + 1;

					goto match_found;
				}

				if (str1[i + j] != str2[j]) {

					break;
				}
			}
		}
	}
	
	int_val = 0;

match_found:
	eval_node_set_int_val(func_node, int_val);
}

/*********************************************************************
Evaluates a predefined function node. */
UNIV_INLINE
void
eval_binary_to_number(
/*==================*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	dfield_t*	dfield;
	byte*		str1;
	byte*		str2;
	ulint		len1;
	ulint		int_val;

	arg1 = func_node->args;

	dfield = que_node_get_val(arg1);

	str1 = dfield_get_data(dfield);
	len1 = dfield_get_len(dfield);

 	if (len1 > 4) {
		ut_error;
	}

	if (len1 == 4) {
		str2 = str1;
	} else {
		int_val = 0;
		str2 = (byte*)&int_val;
			
		ut_memcpy(str2 + (4 - len1), str1, len1);
	}

	eval_node_copy_and_alloc_val(func_node, str2, 4);
}
		
/*********************************************************************
Evaluates a predefined function node. */
static
void
eval_concat(
/*========*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg;
	dfield_t*	dfield;
	byte*		data;
	ulint		len;
	ulint		len1;

	arg = func_node->args;
	len = 0;

	while (arg) {
		len1 = dfield_get_len(que_node_get_val(arg));

		len += len1;

		arg = que_node_get_next(arg);
	}

	data = eval_node_ensure_val_buf(func_node, len);

	arg = func_node->args;
	len = 0;

	while (arg) {
		dfield = que_node_get_val(arg);
		len1 = dfield_get_len(dfield);

		ut_memcpy(data + len, dfield_get_data(dfield), len1);

		len += len1;

		arg = que_node_get_next(arg);
	}
}

/*********************************************************************
Evaluates a predefined function node. */
UNIV_INLINE
void
eval_to_binary(
/*===========*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	que_node_t*	arg2;
	dfield_t*	dfield;
	byte*		str1;
	ulint		len1;

	arg1 = func_node->args;

	str1 = dfield_get_data(que_node_get_val(arg1));

	arg2 = que_node_get_next(arg1);
	
	len1 = (ulint)eval_node_get_int_val(arg2);

	if (len1 > 4) {

		ut_error;
	}
		
	dfield = que_node_get_val(func_node);

	dfield_set_data(dfield, str1 + (4 - len1), len1);
}

/*********************************************************************
Evaluates a predefined function node. */
UNIV_INLINE
void
eval_predefined(
/*============*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg1;
	lint		int_val;
	byte*		str1;
	byte*		data;
	int		func;

	func = func_node->func;
	
	arg1 = func_node->args;

	if (func == PARS_LENGTH_TOKEN) {

		int_val = (lint)dfield_get_len(que_node_get_val(arg1));

	} else if (func == PARS_TO_CHAR_TOKEN) {

		int_val = eval_node_get_int_val(arg1);
				
		data = eval_node_ensure_val_buf(func_node, 11);

		sprintf((char*)data, "%10li", int_val);

		dfield_set_len(que_node_get_val(func_node), 10);

		return;

	} else if (func == PARS_TO_NUMBER_TOKEN) {

		str1 = dfield_get_data(que_node_get_val(arg1));

		int_val = atoi((char*)str1);

	} else if (func == PARS_SYSDATE_TOKEN) {
		int_val = (lint)ut_time();
	} else {
		eval_predefined_2(func_node);

		return;
	}

	eval_node_set_int_val(func_node, int_val); 
}

/*********************************************************************
Evaluates a function node. */

void
eval_func(
/*======*/
	func_node_t*	func_node)	/* in: function node */
{
	que_node_t*	arg;
	ulint		class;
	ulint		func;

	ut_ad(que_node_get_type(func_node) == QUE_NODE_FUNC);

	class = func_node->class;
	func = func_node->func;

	arg = func_node->args;

	/* Evaluate first the argument list */
	while (arg) {
		eval_exp(arg);

		/* The functions are not defined for SQL null argument
		values, except for eval_cmp and notfound */
		
		if ((dfield_get_len(que_node_get_val(arg)) == UNIV_SQL_NULL)
					&& (class != PARS_FUNC_CMP)
					&& (func != PARS_NOTFOUND_TOKEN)
					&& (func != PARS_PRINTF_TOKEN)) {
			ut_error;
		}

		arg = que_node_get_next(arg);
	}

	if (class == PARS_FUNC_CMP) {
		eval_cmp(func_node);
	} else if (class == PARS_FUNC_ARITH) {
		eval_arith(func_node);
	} else if (class == PARS_FUNC_AGGREGATE) {
		eval_aggregate(func_node);
	} else if (class == PARS_FUNC_PREDEFINED) {

		if (func == PARS_NOTFOUND_TOKEN) {
			eval_notfound(func_node);
		} else if (func == PARS_SUBSTR_TOKEN) {
			eval_substr(func_node);
		} else if (func == PARS_REPLSTR_TOKEN) {
			eval_replstr(func_node);
		} else if (func == PARS_INSTR_TOKEN) {
			eval_instr(func_node);
		} else if (func == PARS_BINARY_TO_NUMBER_TOKEN) {
			eval_binary_to_number(func_node);
		} else if (func == PARS_CONCAT_TOKEN) {
			eval_concat(func_node);
		} else if (func == PARS_TO_BINARY_TOKEN) {
			eval_to_binary(func_node);
		} else {
			eval_predefined(func_node);
		}
	} else {
		ut_ad(class == PARS_FUNC_LOGICAL);

		eval_logical(func_node);
	}
}
