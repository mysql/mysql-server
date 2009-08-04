/*****************************************************************************

Copyright (c) 1998, 2009, Innobase Oy. All Rights Reserved.

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
@file eval/eval0proc.c
Executes SQL stored procedures and their control structures

Created 1/20/1998 Heikki Tuuri
*******************************************************/

#include "eval0proc.h"

#ifdef UNIV_NONINL
#include "eval0proc.ic"
#endif

/**********************************************************************//**
Performs an execution step of an if-statement node.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
if_step(
/*====*/
	que_thr_t*	thr)	/*!< in: query thread */
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

				if (eval_node_get_ibool_val(
					    elsif_node->cond)) {

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

/**********************************************************************//**
Performs an execution step of a while-statement node.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
while_step(
/*=======*/
	que_thr_t*	thr)	/*!< in: query thread */
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

/**********************************************************************//**
Performs an execution step of an assignment statement node.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
assign_step(
/*========*/
	que_thr_t*	thr)	/*!< in: query thread */
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

/**********************************************************************//**
Performs an execution step of a for-loop node.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
for_step(
/*=====*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	for_node_t*	node;
	que_node_t*	parent;
	lint		loop_var_value;

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

		node->loop_end_value
                  = (int) eval_node_get_int_val(node->loop_end_limit);
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

/**********************************************************************//**
Performs an execution step of an exit statement node.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
exit_step(
/*======*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	exit_node_t*	node;
	que_node_t*	loop_node;

	ut_ad(thr);

	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_EXIT);

	/* Loops exit by setting thr->run_node as the loop node's parent, so
	find our containing loop node and get its parent. */

	loop_node = que_node_get_containing_loop_node(node);

	/* If someone uses an EXIT statement outside of a loop, this will
	trigger. */
	ut_a(loop_node);

	thr->run_node = que_node_get_parent(loop_node);

	return(thr);
}

/**********************************************************************//**
Performs an execution step of a return-statement node.
@return	query thread to run next or NULL */
UNIV_INTERN
que_thr_t*
return_step(
/*========*/
	que_thr_t*	thr)	/*!< in: query thread */
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
