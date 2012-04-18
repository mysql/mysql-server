/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file que/que0que.cc
Query graph

Created 5/27/1996 Heikki Tuuri
*******************************************************/

#include "que0que.h"

#ifdef UNIV_NONINL
#include "que0que.ic"
#endif

#include "usr0sess.h"
#include "trx0trx.h"
#include "trx0roll.h"
#include "row0undo.h"
#include "row0ins.h"
#include "row0upd.h"
#include "row0sel.h"
#include "row0purge.h"
#include "dict0crea.h"
#include "log0log.h"
#include "eval0proc.h"
#include "lock0lock.h"
#include "eval0eval.h"
#include "pars0types.h"

#define QUE_MAX_LOOPS_WITHOUT_CHECK	16

#ifdef UNIV_DEBUG
/* If the following flag is set TRUE, the module will print trace info
of SQL execution in the UNIV_SQL_DEBUG version */
UNIV_INTERN ibool	que_trace_on		= FALSE;
#endif /* UNIV_DEBUG */

/* Short introduction to query graphs
   ==================================

A query graph consists of nodes linked to each other in various ways. The
execution starts at que_run_threads() which takes a que_thr_t parameter.
que_thr_t contains two fields that control query graph execution: run_node
and prev_node. run_node is the next node to execute and prev_node is the
last node executed.

Each node has a pointer to a 'next' statement, i.e., its brother, and a
pointer to its parent node. The next pointer is NULL in the last statement
of a block.

Loop nodes contain a link to the first statement of the enclosed statement
list. While the loop runs, que_thr_step() checks if execution to the loop
node came from its parent or from one of the statement nodes in the loop. If
it came from the parent of the loop node it starts executing the first
statement node in the loop. If it came from one of the statement nodes in
the loop, then it checks if the statement node has another statement node
following it, and runs it if so.

To signify loop ending, the loop statements (see e.g. while_step()) set
que_thr_t->run_node to the loop node's parent node. This is noticed on the
next call of que_thr_step() and execution proceeds to the node pointed to by
the loop node's 'next' pointer.

For example, the code:

X := 1;
WHILE X < 5 LOOP
 X := X + 1;
 X := X + 1;
X := 5

will result in the following node hierarchy, with the X-axis indicating
'next' links and the Y-axis indicating parent/child links:

A - W - A
    |
    |
    A - A

A = assign_node_t, W = while_node_t. */

/* How a stored procedure containing COMMIT or ROLLBACK commands
is executed?

The commit or rollback can be seen as a subprocedure call.

When the transaction starts to handle a rollback or commit.
It builds a query graph which, when executed, will roll back
or commit the incomplete transaction. The transaction
is moved to the TRX_QUE_ROLLING_BACK or TRX_QUE_COMMITTING state.
If specified, the SQL cursors opened by the transaction are closed.
When the execution of the graph completes, it is like returning
from a subprocedure: the query thread which requested the operation
starts running again. */

/**********************************************************************//**
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction.
***NOTE***: This is the only function in which such a transition is allowed
to happen! */
static
void
que_thr_move_to_run_state(
/*======================*/
	que_thr_t*	thr);	/*!< in: an query thread */

/***********************************************************************//**
Creates a query graph fork node.
@return	own: fork node */
UNIV_INTERN
que_fork_t*
que_fork_create(
/*============*/
	que_t*		graph,		/*!< in: graph, if NULL then this
					fork node is assumed to be the
					graph root */
	que_node_t*	parent,		/*!< in: parent node */
	ulint		fork_type,	/*!< in: fork type */
	mem_heap_t*	heap)		/*!< in: memory heap where created */
{
	que_fork_t*	fork;

	ut_ad(heap);

	fork = static_cast<que_fork_t*>(mem_heap_zalloc(heap, sizeof(*fork)));

	fork->heap = heap;

	fork->fork_type = fork_type;

	fork->common.parent = parent;

	fork->common.type = QUE_NODE_FORK;

	fork->state = QUE_FORK_COMMAND_WAIT;

	fork->graph = (graph != NULL) ? graph : fork;

	return(fork);
}

/***********************************************************************//**
Creates a query graph thread node.
@return	own: query thread node */
UNIV_INTERN
que_thr_t*
que_thr_create(
/*===========*/
	que_fork_t*	parent,	/*!< in: parent node, i.e., a fork node */
	mem_heap_t*	heap)	/*!< in: memory heap where created */
{
	que_thr_t*	thr;

	ut_ad(parent && heap);

	thr = static_cast<que_thr_t*>(mem_heap_zalloc(heap, sizeof(*thr)));

	thr->graph = parent->graph;

	thr->common.parent = parent;

	thr->magic_n = QUE_THR_MAGIC_N;

	thr->common.type = QUE_NODE_THR;

	thr->state = QUE_THR_COMMAND_WAIT;

	thr->lock_state = QUE_THR_LOCK_NOLOCK;

	UT_LIST_ADD_LAST(thrs, parent->thrs, thr);

	return(thr);
}

/**********************************************************************//**
Moves a suspended query thread to the QUE_THR_RUNNING state and may release
a worker thread to execute it. This function should be used to end
the wait state of a query thread waiting for a lock or a stored procedure
completion.
@return the query thread that needs to be released. */
UNIV_INTERN
que_thr_t*
que_thr_end_lock_wait(
/*==================*/
	trx_t*		trx)	/*!< in: transaction with que_state in
		       		QUE_THR_LOCK_WAIT */
{
	que_thr_t*	thr;
	ibool		was_active;

	ut_ad(lock_mutex_own());
	ut_ad(trx_mutex_own(trx));

	thr = trx->lock.wait_thr;

	ut_ad(thr != NULL);

	ut_ad(trx->lock.que_state == TRX_QUE_LOCK_WAIT);
	/* In MySQL this is the only possible state here */
	ut_a(thr->state == QUE_THR_LOCK_WAIT);

	was_active = thr->is_active;

	que_thr_move_to_run_state(thr);

	trx->lock.que_state = TRX_QUE_RUNNING;

	trx->lock.wait_thr = NULL;

	/* In MySQL we let the OS thread (not just the query thread) to wait
	for the lock to be released: */

	return((!was_active && thr != NULL) ? thr : NULL);
}

/**********************************************************************//**
Inits a query thread for a command. */
UNIV_INLINE
void
que_thr_init_command(
/*=================*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	thr->run_node = thr;
	thr->prev_node = thr->common.parent;

	que_thr_move_to_run_state(thr);
}

/**********************************************************************//**
Round robin scheduler.
@return a query thread of the graph moved to QUE_THR_RUNNING state, or
NULL; the query thread should be executed by que_run_threads by the
caller */
UNIV_INTERN
que_thr_t*
que_fork_scheduler_round_robin(
/*===========================*/
	que_fork_t*	fork,		/*!< in: a query fork */
	que_thr_t*	thr)		/*!< in: current pos */
{
	trx_mutex_enter(fork->trx);

	/* If no current, start first available. */
	if (thr == NULL) {
		thr = UT_LIST_GET_FIRST(fork->thrs);
	} else {
		thr = UT_LIST_GET_NEXT(thrs, thr);
	}

	if (thr) {

		fork->state = QUE_FORK_ACTIVE;

		fork->last_sel_node = NULL;

		switch (thr->state) {
		case QUE_THR_COMMAND_WAIT:
		case QUE_THR_COMPLETED:
			ut_a(!thr->is_active);
			que_thr_init_command(thr);
			break;

		case QUE_THR_SUSPENDED:
		case QUE_THR_LOCK_WAIT:
		default:
			ut_error;

		}
	}

	trx_mutex_exit(fork->trx);

	return(thr);
}

/**********************************************************************//**
Starts execution of a command in a query fork. Picks a query thread which
is not in the QUE_THR_RUNNING state and moves it to that state. If none
can be chosen, a situation which may arise in parallelized fetches, NULL
is returned.
@return a query thread of the graph moved to QUE_THR_RUNNING state, or
NULL; the query thread should be executed by que_run_threads by the
caller */
UNIV_INTERN
que_thr_t*
que_fork_start_command(
/*===================*/
	que_fork_t*	fork)	/*!< in: a query fork */
{
	que_thr_t*	thr;
	que_thr_t*	suspended_thr = NULL;
	que_thr_t*	completed_thr = NULL;

	fork->state = QUE_FORK_ACTIVE;

	fork->last_sel_node = NULL;

	suspended_thr = NULL;
	completed_thr = NULL;

	/* Choose the query thread to run: usually there is just one thread,
	but in a parallelized select, which necessarily is non-scrollable,
	there may be several to choose from */

	/* First we try to find a query thread in the QUE_THR_COMMAND_WAIT
	state. Then we try to find a query thread in the QUE_THR_SUSPENDED
	state, finally we try to find a query thread in the QUE_THR_COMPLETED
	state */

	/* We make a single pass over the thr list within which we note which
	threads are ready to run. */
	for (thr = UT_LIST_GET_FIRST(fork->thrs);
	     thr != NULL;
	     thr = UT_LIST_GET_NEXT(thrs, thr)) {

		switch (thr->state) {
		case QUE_THR_COMMAND_WAIT:

			/* We have to send the initial message to query thread
			to start it */

			que_thr_init_command(thr);

			return(thr);

		case QUE_THR_SUSPENDED:
			/* In this case the execution of the thread was
			suspended: no initial message is needed because
			execution can continue from where it was left */
			if (!suspended_thr) {
				suspended_thr = thr;
			}

			break;

		case QUE_THR_COMPLETED:
			if (!completed_thr) {
				completed_thr = thr;
			}

			break;

		case QUE_THR_LOCK_WAIT:
			ut_error;

		}
	}

	if (suspended_thr) {

		thr = suspended_thr;
		que_thr_move_to_run_state(thr);

	} else if (completed_thr) {

		thr = completed_thr;
		que_thr_init_command(thr);
	} else {
		ut_error;
	}

	return(thr);
}

/****************************************************************//**
Tests if all the query threads in the same fork have a given state.
@return TRUE if all the query threads in the same fork were in the
given state */
UNIV_INLINE
ibool
que_fork_all_thrs_in_state(
/*=======================*/
	que_fork_t*	fork,	/*!< in: query fork */
	ulint		state)	/*!< in: state */
{
	que_thr_t*	thr_node;

	for (thr_node = UT_LIST_GET_FIRST(fork->thrs);
	     thr_node != NULL;
	     thr_node = UT_LIST_GET_NEXT(thrs, thr_node)) {

		if (thr_node->state != state) {

			return(FALSE);
		}
	}

	return(TRUE);
}

/**********************************************************************//**
Calls que_graph_free_recursive for statements in a statement list. */
static
void
que_graph_free_stat_list(
/*=====================*/
	que_node_t*	node)	/*!< in: first query graph node in the list */
{
	while (node) {
		que_graph_free_recursive(node);

		node = que_node_get_next(node);
	}
}

/**********************************************************************//**
Frees a query graph, but not the heap where it was created. Does not free
explicit cursor declarations, they are freed in que_graph_free. */
UNIV_INTERN
void
que_graph_free_recursive(
/*=====================*/
	que_node_t*	node)	/*!< in: query graph node */
{
	que_fork_t*	fork;
	que_thr_t*	thr;
	undo_node_t*	undo;
	sel_node_t*	sel;
	ins_node_t*	ins;
	upd_node_t*	upd;
	tab_node_t*	cre_tab;
	ind_node_t*	cre_ind;
	purge_node_t*	purge;

	if (node == NULL) {

		return;
	}

	switch (que_node_get_type(node)) {

	case QUE_NODE_FORK:
		fork = static_cast<que_fork_t*>(node);

		thr = UT_LIST_GET_FIRST(fork->thrs);

		while (thr) {
			que_graph_free_recursive(thr);

			thr = UT_LIST_GET_NEXT(thrs, thr);
		}

		break;
	case QUE_NODE_THR:

		thr = static_cast<que_thr_t*>(node);

		if (thr->magic_n != QUE_THR_MAGIC_N) {
			fprintf(stderr,
				"que_thr struct appears corrupt;"
				" magic n %lu\n",
				(unsigned long) thr->magic_n);
			mem_analyze_corruption(thr);
			ut_error;
		}

		thr->magic_n = QUE_THR_MAGIC_FREED;

		que_graph_free_recursive(thr->child);

		break;
	case QUE_NODE_UNDO:

		undo = static_cast<undo_node_t*>(node);

		mem_heap_free(undo->heap);

		break;
	case QUE_NODE_SELECT:

		sel = static_cast<sel_node_t*>(node);

		sel_node_free_private(sel);

		break;
	case QUE_NODE_INSERT:

		ins = static_cast<ins_node_t*>(node);

		que_graph_free_recursive(ins->select);

		mem_heap_free(ins->entry_sys_heap);

		break;
	case QUE_NODE_PURGE:
		purge = static_cast<purge_node_t*>(node);

		mem_heap_free(purge->heap);

		break;

	case QUE_NODE_UPDATE:

		upd = static_cast<upd_node_t*>(node);

		if (upd->in_mysql_interface) {

			btr_pcur_free_for_mysql(upd->pcur);
		}

		que_graph_free_recursive(upd->cascade_node);

		if (upd->cascade_heap) {
			mem_heap_free(upd->cascade_heap);
		}

		que_graph_free_recursive(upd->select);

		mem_heap_free(upd->heap);

		break;
	case QUE_NODE_CREATE_TABLE:
		cre_tab = static_cast<tab_node_t*>(node);

		que_graph_free_recursive(cre_tab->tab_def);
		que_graph_free_recursive(cre_tab->col_def);
		que_graph_free_recursive(cre_tab->commit_node);

		mem_heap_free(cre_tab->heap);

		break;
	case QUE_NODE_CREATE_INDEX:
		cre_ind = static_cast<ind_node_t*>(node);

		que_graph_free_recursive(cre_ind->ind_def);
		que_graph_free_recursive(cre_ind->field_def);
		que_graph_free_recursive(cre_ind->commit_node);

		mem_heap_free(cre_ind->heap);

		break;
	case QUE_NODE_PROC:
		que_graph_free_stat_list(((proc_node_t*) node)->stat_list);

		break;
	case QUE_NODE_IF:
		que_graph_free_stat_list(((if_node_t*) node)->stat_list);
		que_graph_free_stat_list(((if_node_t*) node)->else_part);
		que_graph_free_stat_list(((if_node_t*) node)->elsif_list);

		break;
	case QUE_NODE_ELSIF:
		que_graph_free_stat_list(((elsif_node_t*) node)->stat_list);

		break;
	case QUE_NODE_WHILE:
		que_graph_free_stat_list(((while_node_t*) node)->stat_list);

		break;
	case QUE_NODE_FOR:
		que_graph_free_stat_list(((for_node_t*) node)->stat_list);

		break;

	case QUE_NODE_ASSIGNMENT:
	case QUE_NODE_EXIT:
	case QUE_NODE_RETURN:
	case QUE_NODE_COMMIT:
	case QUE_NODE_ROLLBACK:
	case QUE_NODE_LOCK:
	case QUE_NODE_FUNC:
	case QUE_NODE_ORDER:
	case QUE_NODE_ROW_PRINTF:
	case QUE_NODE_OPEN:
	case QUE_NODE_FETCH:
		/* No need to do anything */

		break;
	default:
		fprintf(stderr,
			"que_node struct appears corrupt; type %lu\n",
			(unsigned long) que_node_get_type(node));
		mem_analyze_corruption(node);
		ut_error;
	}
}

/**********************************************************************//**
Frees a query graph. */
UNIV_INTERN
void
que_graph_free(
/*===========*/
	que_t*	graph)	/*!< in: query graph; we assume that the memory
			heap where this graph was created is private
			to this graph: if not, then use
			que_graph_free_recursive and free the heap
			afterwards! */
{
	ut_ad(graph);

	if (graph->sym_tab) {
		/* The following call frees dynamic memory allocated
		for variables etc. during execution. Frees also explicit
		cursor definitions. */

		sym_tab_free_private(graph->sym_tab);
	}

	if (graph->info && graph->info->graph_owns_us) {
		pars_info_free(graph->info);
	}

	que_graph_free_recursive(graph);

	mem_heap_free(graph->heap);
}

/****************************************************************//**
Performs an execution step on a thr node.
@return	query thread to run next, or NULL if none */
static
que_thr_t*
que_thr_node_step(
/*==============*/
	que_thr_t*	thr)	/*!< in: query thread where run_node must
				be the thread node itself */
{
	ut_ad(thr->run_node == thr);

	if (thr->prev_node == thr->common.parent) {
		/* If control to the node came from above, it is just passed
		on */

		thr->run_node = thr->child;

		return(thr);
	}

	trx_mutex_enter(thr_get_trx(thr));

	if (que_thr_peek_stop(thr)) {

		trx_mutex_exit(thr_get_trx(thr));

		return(thr);
	}

	/* Thread execution completed */

	thr->state = QUE_THR_COMPLETED;

	trx_mutex_exit(thr_get_trx(thr));

	return(NULL);
}

/**********************************************************************//**
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction if thr was
not active.
***NOTE***: This and ..._mysql are  the only functions in which such a
transition is allowed to happen! */
static
void
que_thr_move_to_run_state(
/*======================*/
	que_thr_t*	thr)	/*!< in: an query thread */
{
	ut_ad(thr->state != QUE_THR_RUNNING);

	if (!thr->is_active) {
		trx_t*	trx;

		trx = thr_get_trx(thr);

		thr->graph->n_active_thrs++;

		trx->lock.n_active_thrs++;

		thr->is_active = TRUE;
	}

	thr->state = QUE_THR_RUNNING;
}

/**********************************************************************//**
Stops a query thread if graph or trx is in a state requiring it. The
conditions are tested in the order (1) graph, (2) trx.
@return	TRUE if stopped */
UNIV_INTERN
ibool
que_thr_stop(
/*=========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	que_t*		graph;
	trx_t*		trx = thr_get_trx(thr);

	graph = thr->graph;

	ut_ad(trx_mutex_own(trx));

	if (graph->state == QUE_FORK_COMMAND_WAIT) {

		thr->state = QUE_THR_SUSPENDED;

	} else if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {

		trx->lock.wait_thr = thr;
		thr->state = QUE_THR_LOCK_WAIT;

	} else if (trx->error_state != DB_SUCCESS
		   && trx->error_state != DB_LOCK_WAIT) {

		/* Error handling built for the MySQL interface */
		thr->state = QUE_THR_COMPLETED;

	} else if (graph->fork_type == QUE_FORK_ROLLBACK) {

		thr->state = QUE_THR_SUSPENDED;
	} else {
		ut_ad(graph->state == QUE_FORK_ACTIVE);

		return(FALSE);
	}

	return(TRUE);
}

/**********************************************************************//**
Decrements the query thread reference counts in the query graph and the
transaction.
*** NOTE ***:
This and que_thr_stop_for_mysql are the only functions where the reference
count can be decremented and this function may only be called from inside
que_run_threads! These restrictions exist to make the rollback code easier
to maintain. */
static
void
que_thr_dec_refer_count(
/*====================*/
	que_thr_t*	thr,		/*!< in: query thread */
	que_thr_t**	next_thr)	/*!< in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	trx_t*		trx;
	que_fork_t*	fork;

	trx = thr_get_trx(thr);

	ut_a(thr->is_active);
	ut_ad(trx_mutex_own(trx));

	if (thr->state == QUE_THR_RUNNING) {

		if (!que_thr_stop(thr)) {

			ut_a(next_thr != NULL && *next_thr == NULL);

			/* The reason for the thr suspension or wait was
			already canceled before we came here: continue
			running the thread.

			This is also possible because in trx_commit_step() we
			assume a single query thread. We set the query thread
			state to QUE_THR_RUNNING. */

			/* fprintf(stderr,
		       		"Wait already ended: trx: %p\n", trx); */

			/* Normally srv_suspend_mysql_thread resets
			the state to DB_SUCCESS before waiting, but
			in this case we have to do it here,
			otherwise nobody does it. */

			trx->error_state = DB_SUCCESS;

			*next_thr = thr;

			return;
		}
	}

	fork = static_cast<que_fork_t*>(thr->common.parent);

	--trx->lock.n_active_thrs;

	--fork->n_active_thrs;

	thr->is_active = FALSE;
}

/**********************************************************************//**
A patch for MySQL used to 'stop' a dummy query thread used in MySQL. The
query thread is stopped and made inactive, except in the case where
it was put to the lock wait state in lock0lock.cc, but the lock has already
been granted or the transaction chosen as a victim in deadlock resolution. */
UNIV_INTERN
void
que_thr_stop_for_mysql(
/*===================*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	trx_t*	trx;

	trx = thr_get_trx(thr);

	/* Can't be the purge transaction. */
	ut_a(trx->id != 0);

	trx_mutex_enter(trx);

	if (thr->state == QUE_THR_RUNNING) {

		if (trx->error_state != DB_SUCCESS
		    && trx->error_state != DB_LOCK_WAIT) {

			/* Error handling built for the MySQL interface */
			thr->state = QUE_THR_COMPLETED;
		} else {
			/* It must have been a lock wait but the lock was
			already released, or this transaction was chosen
			as a victim in selective deadlock resolution */

			trx_mutex_exit(trx);

			return;
		}
	}

	ut_ad(thr->is_active == TRUE);
	ut_ad(trx->lock.n_active_thrs == 1);
	ut_ad(thr->graph->n_active_thrs == 1);

	thr->is_active = FALSE;
	thr->graph->n_active_thrs--;

	trx->lock.n_active_thrs--;

	trx_mutex_exit(trx);
}

/**********************************************************************//**
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction if thr was
not active. */
UNIV_INTERN
void
que_thr_move_to_run_state_for_mysql(
/*================================*/
	que_thr_t*	thr,	/*!< in: an query thread */
	trx_t*		trx)	/*!< in: transaction */
{
	if (thr->magic_n != QUE_THR_MAGIC_N) {
		fprintf(stderr,
			"que_thr struct appears corrupt; magic n %lu\n",
			(unsigned long) thr->magic_n);

		mem_analyze_corruption(thr);

		ut_error;
	}

	if (!thr->is_active) {

		thr->graph->n_active_thrs++;

		trx->lock.n_active_thrs++;

		thr->is_active = TRUE;
	}

	thr->state = QUE_THR_RUNNING;
}

/**********************************************************************//**
A patch for MySQL used to 'stop' a dummy query thread used in MySQL
select, when there is no error or lock wait. */
UNIV_INTERN
void
que_thr_stop_for_mysql_no_error(
/*============================*/
	que_thr_t*	thr,	/*!< in: query thread */
	trx_t*		trx)	/*!< in: transaction */
{
	ut_ad(thr->state == QUE_THR_RUNNING);
	ut_ad(thr_get_trx(thr)->id != 0);
	ut_ad(thr->is_active == TRUE);
	ut_ad(trx->lock.n_active_thrs == 1);
	ut_ad(thr->graph->n_active_thrs == 1);

	if (thr->magic_n != QUE_THR_MAGIC_N) {
		fprintf(stderr,
			"que_thr struct appears corrupt; magic n %lu\n",
			(unsigned long) thr->magic_n);

		mem_analyze_corruption(thr);

		ut_error;
	}

	thr->state = QUE_THR_COMPLETED;

	thr->is_active = FALSE;
	thr->graph->n_active_thrs--;

	trx->lock.n_active_thrs--;
}

/****************************************************************//**
Get the first containing loop node (e.g. while_node_t or for_node_t) for the
given node, or NULL if the node is not within a loop.
@return	containing loop node, or NULL. */
UNIV_INTERN
que_node_t*
que_node_get_containing_loop_node(
/*==============================*/
	que_node_t*	node)	/*!< in: node */
{
	ut_ad(node);

	for (;;) {
		ulint	type;

		node = que_node_get_parent(node);

		if (!node) {
			break;
		}

		type = que_node_get_type(node);

		if ((type == QUE_NODE_FOR) || (type == QUE_NODE_WHILE)) {
			break;
		}
	}

	return(node);
}

/**********************************************************************//**
Prints info of an SQL query graph node. */
UNIV_INTERN
void
que_node_print_info(
/*================*/
	que_node_t*	node)	/*!< in: query graph node */
{
	ulint		type;
	const char*	str;

	type = que_node_get_type(node);

	if (type == QUE_NODE_SELECT) {
		str = "SELECT";
	} else if (type == QUE_NODE_INSERT) {
		str = "INSERT";
	} else if (type == QUE_NODE_UPDATE) {
		str = "UPDATE";
	} else if (type == QUE_NODE_WHILE) {
		str = "WHILE";
	} else if (type == QUE_NODE_ASSIGNMENT) {
		str = "ASSIGNMENT";
	} else if (type == QUE_NODE_IF) {
		str = "IF";
	} else if (type == QUE_NODE_FETCH) {
		str = "FETCH";
	} else if (type == QUE_NODE_OPEN) {
		str = "OPEN";
	} else if (type == QUE_NODE_PROC) {
		str = "STORED PROCEDURE";
	} else if (type == QUE_NODE_FUNC) {
		str = "FUNCTION";
	} else if (type == QUE_NODE_LOCK) {
		str = "LOCK";
	} else if (type == QUE_NODE_THR) {
		str = "QUERY THREAD";
	} else if (type == QUE_NODE_COMMIT) {
		str = "COMMIT";
	} else if (type == QUE_NODE_UNDO) {
		str = "UNDO ROW";
	} else if (type == QUE_NODE_PURGE) {
		str = "PURGE ROW";
	} else if (type == QUE_NODE_ROLLBACK) {
		str = "ROLLBACK";
	} else if (type == QUE_NODE_CREATE_TABLE) {
		str = "CREATE TABLE";
	} else if (type == QUE_NODE_CREATE_INDEX) {
		str = "CREATE INDEX";
	} else if (type == QUE_NODE_FOR) {
		str = "FOR LOOP";
	} else if (type == QUE_NODE_RETURN) {
		str = "RETURN";
	} else if (type == QUE_NODE_EXIT) {
		str = "EXIT";
	} else {
		str = "UNKNOWN NODE TYPE";
	}

	fprintf(stderr, "Node type %lu: %s, address %p\n",
		(ulong) type, str, (void*) node);
}

/**********************************************************************//**
Performs an execution step on a query thread.
@return query thread to run next: it may differ from the input
parameter if, e.g., a subprocedure call is made */
UNIV_INLINE
que_thr_t*
que_thr_step(
/*=========*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	que_node_t*	node;
	que_thr_t*	old_thr;
	trx_t*		trx;
	ulint		type;

	trx = thr_get_trx(thr);

	ut_ad(thr->state == QUE_THR_RUNNING);
	ut_a(trx->error_state == DB_SUCCESS);

	thr->resource++;

	node = thr->run_node;
	type = que_node_get_type(node);

	old_thr = thr;

#ifdef UNIV_DEBUG
	if (que_trace_on) {
		fputs("To execute: ", stderr);
		que_node_print_info(node);
	}
#endif
	if (type & QUE_NODE_CONTROL_STAT) {
		if ((thr->prev_node != que_node_get_parent(node))
		    && que_node_get_next(thr->prev_node)) {

			/* The control statements, like WHILE, always pass the
			control to the next child statement if there is any
			child left */

			thr->run_node = que_node_get_next(thr->prev_node);

		} else if (type == QUE_NODE_IF) {
			if_step(thr);
		} else if (type == QUE_NODE_FOR) {
			for_step(thr);
		} else if (type == QUE_NODE_PROC) {

			/* We can access trx->undo_no without reserving
			trx->undo_mutex, because there cannot be active query
			threads doing updating or inserting at the moment! */

			if (thr->prev_node == que_node_get_parent(node)) {
				trx->last_sql_stat_start.least_undo_no
					= trx->undo_no;
			}

			proc_step(thr);
		} else if (type == QUE_NODE_WHILE) {
			while_step(thr);
		} else {
			ut_error;
		}
	} else if (type == QUE_NODE_ASSIGNMENT) {
		assign_step(thr);
	} else if (type == QUE_NODE_SELECT) {
		thr = row_sel_step(thr);
	} else if (type == QUE_NODE_INSERT) {
		thr = row_ins_step(thr);
	} else if (type == QUE_NODE_UPDATE) {
		thr = row_upd_step(thr);
	} else if (type == QUE_NODE_FETCH) {
		thr = fetch_step(thr);
	} else if (type == QUE_NODE_OPEN) {
		thr = open_step(thr);
	} else if (type == QUE_NODE_FUNC) {
		proc_eval_step(thr);

	} else if (type == QUE_NODE_LOCK) {

		ut_error;
	} else if (type == QUE_NODE_THR) {
		thr = que_thr_node_step(thr);
	} else if (type == QUE_NODE_COMMIT) {
		thr = trx_commit_step(thr);
	} else if (type == QUE_NODE_UNDO) {
		thr = row_undo_step(thr);
	} else if (type == QUE_NODE_PURGE) {
		thr = row_purge_step(thr);
	} else if (type == QUE_NODE_RETURN) {
		thr = return_step(thr);
	} else if (type == QUE_NODE_EXIT) {
		thr = exit_step(thr);
	} else if (type == QUE_NODE_ROLLBACK) {
		thr = trx_rollback_step(thr);
	} else if (type == QUE_NODE_CREATE_TABLE) {
		thr = dict_create_table_step(thr);
	} else if (type == QUE_NODE_CREATE_INDEX) {
		thr = dict_create_index_step(thr);
	} else if (type == QUE_NODE_ROW_PRINTF) {
		thr = row_printf_step(thr);
	} else {
		ut_error;
	}

	if (type == QUE_NODE_EXIT) {
		old_thr->prev_node = que_node_get_containing_loop_node(node);
	} else {
		old_thr->prev_node = node;
	}

	if (thr) {
		ut_a(thr_get_trx(thr)->error_state == DB_SUCCESS);
	}

	return(thr);
}

/**********************************************************************//**
Run a query thread until it finishes or encounters e.g. a lock wait. */
static
void
que_run_threads_low(
/*================*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	trx_t*		trx;
	que_thr_t*	next_thr;

	ut_ad(thr->state == QUE_THR_RUNNING);
	ut_a(thr_get_trx(thr)->error_state == DB_SUCCESS);
	ut_ad(!trx_mutex_own(thr_get_trx(thr)));

	/* cumul_resource counts how much resources the OS thread (NOT the
	query thread) has spent in this function */

	trx = thr_get_trx(thr);

	do {
		/* Check that there is enough space in the log to accommodate
		possible log entries by this query step; if the operation can
		touch more than about 4 pages, checks must be made also within
		the query step! */

		log_free_check();

		/* Perform the actual query step: note that the query thread
		may change if, e.g., a subprocedure call is made */

		/*-------------------------*/
		next_thr = que_thr_step(thr);
		/*-------------------------*/

		trx_mutex_enter(trx);

		ut_a(next_thr == NULL || trx->error_state == DB_SUCCESS);

		if (next_thr != thr) {
			ut_a(next_thr == NULL);

			/* This can change next_thr to a non-NULL value
			if there was a lock wait that already completed. */

			que_thr_dec_refer_count(thr, &next_thr);

			if (next_thr != NULL) {

				thr = next_thr;
			}
		}

		ut_ad(trx == thr_get_trx(thr));

		trx_mutex_exit(trx);

	} while (next_thr != NULL);
}

/**********************************************************************//**
Run a query thread. Handles lock waits. */
UNIV_INTERN
void
que_run_threads(
/*============*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	ut_ad(!trx_mutex_own(thr_get_trx(thr)));

loop:
	ut_a(thr_get_trx(thr)->error_state == DB_SUCCESS);

	que_run_threads_low(thr);

	switch (thr->state) {

	case QUE_THR_RUNNING:
		/* There probably was a lock wait, but it already ended
		before we came here: continue running thr */

		goto loop;

	case QUE_THR_LOCK_WAIT:
		lock_wait_suspend_thread(thr);

		trx_mutex_enter(thr_get_trx(thr));

		ut_a(thr_get_trx(thr)->id != 0);

		if (thr_get_trx(thr)->error_state != DB_SUCCESS) {
			/* thr was chosen as a deadlock victim or there was
			a lock wait timeout */

			que_thr_dec_refer_count(thr, NULL);
			trx_mutex_exit(thr_get_trx(thr));
			break;
		}

		trx_mutex_exit(thr_get_trx(thr));
		goto loop;

	case QUE_THR_COMPLETED:
	case QUE_THR_COMMAND_WAIT:
		/* Do nothing */
		break;

	default:
		ut_error;
	}
}

/*********************************************************************//**
Evaluate the given SQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
que_eval_sql(
/*=========*/
	pars_info_t*	info,	/*!< in: info struct, or NULL */
	const char*	sql,	/*!< in: SQL string */
	ibool		reserve_dict_mutex,
				/*!< in: if TRUE, acquire/release
				dict_sys->mutex around call to pars_sql. */
	trx_t*		trx)	/*!< in: trx */
{
	que_thr_t*	thr;
	que_t*		graph;

	ut_a(trx->error_state == DB_SUCCESS);

	if (reserve_dict_mutex) {
		mutex_enter(&dict_sys->mutex);
	}

	graph = pars_sql(info, sql);

	if (reserve_dict_mutex) {
		mutex_exit(&dict_sys->mutex);
	}

	ut_a(graph);

	graph->trx = trx;
	trx->graph = NULL;

	graph->fork_type = QUE_FORK_MYSQL_INTERFACE;

	ut_a(thr = que_fork_start_command(graph));

	que_run_threads(thr);

	if (reserve_dict_mutex) {
		mutex_enter(&dict_sys->mutex);
	}

	que_graph_free(graph);

	if (reserve_dict_mutex) {
		mutex_exit(&dict_sys->mutex);
	}

	return(trx->error_state);
}

/*********************************************************************//**
Initialise the query sub-system. */
UNIV_INTERN
void
que_init(void)
/*==========*/
{
	/* No op */
}

/*********************************************************************//**
Close the query sub-system. */
UNIV_INTERN
void
que_close(void)
/*===========*/
{
	/* No op */
}
