/******************************************************
Executes SQL stored procedures and their control structures

(c) 1998 Innobase Oy

Created 1/20/1998 Heikki Tuuri
*******************************************************/

#include "eval0proc.h"

#ifdef UNIV_NONINL
#include "eval0proc.ic"
#endif

/**************************************************************************
Performs an execution step of an if-statement node. */

que_thr_t*
if_step(
/*====*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	if_node_t*	node;
	elsif_node_t*	elsif_node;

	ut_ad(thr);
	
	node = thr->run_node;
	ut_ad(que_node_get_type(node) == QUE_NODE_IF);

	if (thr->prev_node == que_node_get_parent(node)) {

		/* Evaluate the condition */

		eval_exp(node->cond);

		if (eval_node_get_ibool_val(node->cond)) {

			/* The condition evaluated to TRUE: start execution
			from the first statement in the statement list */

			thr->run_node = node->stat_list;

		} else if (node->else_part) {
			thr->run_node = node->else_part;

		} else if (node->elsif_list) {
			elsif_node = node->elsif_list;

			for (;;) {
				eval_exp(elsif_node->cond);

				if (eval_node_get_ibool_val(elsif_node->cond)) {

					/* The condition evaluated to TRUE:
					start execution from the first
					statement in the statement list */

					thr->run_node = elsif_node->stat_list;

					break;
				}

				elsif_node = que_node_get_next(elsif_node);

				if (elsif_node == NULL) {
					thr->run_node = NULL;

					break;
				}
			}
		} else {
			thr->run_node = NULL;
		}
	} else {
		/* Move to the next statement */
		ut_ad(que_node_get_next(thr->prev_node) == NULL);

		thr->run_node = NULL;
	}

	if (thr->run_node == NULL) {
		thr->run_node = que_node_get_parent(node);
	}
	
	return(thr);
} 

/**************************************************************************
Performs an execution step of a while-statement node. */

que_thr_t*
while_step(
/*=======*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	while_node_t*	node;

	ut_ad(thr);
	
	node = thr->run_node;
	ut_ad(que_node_get_type(node) == QUE_NODE_WHILE);

	ut_ad((thr->prev_node == que_node_get_parent(node))
			|| (que_node_get_next(thr->prev_node) == NULL));

	/* Evaluate the condition */

	eval_exp(node->cond);

	if (eval_node_get_ibool_val(node->cond)) {

		/* The condition evaluated to TRUE: start execution
		from the first statement in the statement list */

		thr->run_node = node->stat_list;
	} else {
		thr->run_node = que_node_get_parent(node);
	}

	return(thr);
} 

/**************************************************************************
Performs an execution step of an assignment statement node. */

que_thr_t*
assign_step(
/*========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	assign_node_t*	node;

	ut_ad(thr);
	
	node = thr->run_node;
	ut_ad(que_node_get_type(node) == QUE_NODE_ASSIGNMENT);

	/* Evaluate the value to assign */

	eval_exp(node->val);

	eval_node_copy_val(node->var->alias, node->val);
	
	thr->run_node = que_node_get_parent(node);

	return(thr);
} 

/**************************************************************************
Performs an execution step of a for-loop node. */

que_thr_t*
for_step(
/*=====*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	for_node_t*	node;
	que_node_t*	parent;
	int		loop_var_value;

	ut_ad(thr);
	
	node = thr->run_node;
	
	ut_ad(que_node_get_type(node) == QUE_NODE_FOR);

	parent = que_node_get_parent(node);

	if (thr->prev_node != parent) {

		/* Move to the next statement */
		thr->run_node = que_node_get_next(thr->prev_node);

		if (thr->run_node != NULL) {

			return(thr);
		}

		/* Increment the value of loop_var */
		
		loop_var_value = 1 + eval_node_get_int_val(node->loop_var);
	} else {
		/* Initialize the loop */

		eval_exp(node->loop_start_limit);
		eval_exp(node->loop_end_limit);

		loop_var_value = eval_node_get_int_val(node->loop_start_limit);

		node->loop_end_value = eval_node_get_int_val(
							node->loop_end_limit);
	}

	/* Check if we should do another loop */

	if (loop_var_value > node->loop_end_value) {

		/* Enough loops done */

		thr->run_node = parent;
	} else {
		eval_node_set_int_val(node->loop_var, loop_var_value);

		thr->run_node = node->stat_list;
	}

	return(thr);
} 

/**************************************************************************
Performs an execution step of a return-statement node. */

que_thr_t*
return_step(
/*========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	return_node_t*	node;
	que_node_t*	parent;

	ut_ad(thr);
	
	node = thr->run_node;
	
	ut_ad(que_node_get_type(node) == QUE_NODE_RETURN);

	parent = node;

	while (que_node_get_type(parent) != QUE_NODE_PROC) {

		parent = que_node_get_parent(parent);
	}

	ut_a(parent);

	thr->run_node = que_node_get_parent(parent);

	return(thr);
}
