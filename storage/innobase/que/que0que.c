/******************************************************
Query graph

(c) 1996 Innobase Oy

Created 5/27/1996 Heikki Tuuri
*******************************************************/

#include "que0que.h"

#ifdef UNIV_NONINL
#include "que0que.ic"
#endif

#include "srv0que.h"
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
#include "eval0eval.h"
#include "pars0types.h"

#define QUE_PARALLELIZE_LIMIT	(64 * 256 * 256 * 256)
#define QUE_ROUND_ROBIN_LIMIT	(64 * 256 * 256 * 256)
#define QUE_MAX_LOOPS_WITHOUT_CHECK	16

/* If the following flag is set TRUE, the module will print trace info
of SQL execution in the UNIV_SQL_DEBUG version */
ibool	que_trace_on		= FALSE;

ibool	que_always_false	= FALSE;

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
The problem is that if there are several query threads
currently running within the transaction, their action could
mess the commit or rollback operation. Or, at the least, the
operation would be difficult to visualize and keep in control.

Therefore the query thread requesting a commit or a rollback
sends to the transaction a signal, which moves the transaction
to TRX_QUE_SIGNALED state. All running query threads of the
transaction will eventually notice that the transaction is now in
this state and voluntarily suspend themselves. Only the last
query thread which suspends itself will trigger handling of
the signal.

When the transaction starts to handle a rollback or commit
signal, it builds a query graph which, when executed, will
roll back or commit the incomplete transaction. The transaction
is moved to the TRX_QUE_ROLLING_BACK or TRX_QUE_COMMITTING state.
If specified, the SQL cursors opened by the transaction are closed.
When the execution of the graph completes, it is like returning
from a subprocedure: the query thread which requested the operation
starts running again. */

/**************************************************************************
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction.
***NOTE***: This is the only function in which such a transition is allowed
to happen! */
static
void
que_thr_move_to_run_state(
/*======================*/
	que_thr_t*	thr);	/* in: an query thread */

/***************************************************************************
Adds a query graph to the session's list of graphs. */

void
que_graph_publish(
/*==============*/
	que_t*	graph,	/* in: graph */
	sess_t*	sess)	/* in: session */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	UT_LIST_ADD_LAST(graphs, sess->graphs, graph);
}

/***************************************************************************
Creates a query graph fork node. */

que_fork_t*
que_fork_create(
/*============*/
					/* out, own: fork node */
	que_t*		graph,		/* in: graph, if NULL then this
					fork node is assumed to be the
					graph root */
	que_node_t*	parent,		/* in: parent node */
	ulint		fork_type,	/* in: fork type */
	mem_heap_t*	heap)		/* in: memory heap where created */
{
	que_fork_t*	fork;

	ut_ad(heap);

	fork = mem_heap_alloc(heap, sizeof(que_fork_t));

	fork->common.type = QUE_NODE_FORK;
	fork->n_active_thrs = 0;

	fork->state = QUE_FORK_COMMAND_WAIT;

	if (graph != NULL) {
		fork->graph = graph;
	} else {
		fork->graph = fork;
	}

	fork->common.parent = parent;
	fork->fork_type = fork_type;

	fork->caller = NULL;

	UT_LIST_INIT(fork->thrs);

	fork->sym_tab = NULL;
	fork->info = NULL;

	fork->heap = heap;

	return(fork);
}

/***************************************************************************
Creates a query graph thread node. */

que_thr_t*
que_thr_create(
/*===========*/
				/* out, own: query thread node */
	que_fork_t*	parent,	/* in: parent node, i.e., a fork node */
	mem_heap_t*	heap)	/* in: memory heap where created */
{
	que_thr_t*	thr;

	ut_ad(parent && heap);

	thr = mem_heap_alloc(heap, sizeof(que_thr_t));

	thr->common.type = QUE_NODE_THR;
	thr->common.parent = parent;

	thr->magic_n = QUE_THR_MAGIC_N;

	thr->graph = parent->graph;

	thr->state = QUE_THR_COMMAND_WAIT;

	thr->is_active = FALSE;

	thr->run_node = NULL;
	thr->resource = 0;
	thr->lock_state = QUE_THR_LOCK_NOLOCK;

	UT_LIST_ADD_LAST(thrs, parent->thrs, thr);

	return(thr);
}

/**************************************************************************
Moves a suspended query thread to the QUE_THR_RUNNING state and may release
a single worker thread to execute it. This function should be used to end
the wait state of a query thread waiting for a lock or a stored procedure
completion. */

void
que_thr_end_wait(
/*=============*/
	que_thr_t*	thr,		/* in: query thread in the
					QUE_THR_LOCK_WAIT,
					or QUE_THR_PROCEDURE_WAIT, or
					QUE_THR_SIG_REPLY_WAIT state */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if NULL is passed
					as the parameter, it is ignored */
{
	ibool	was_active;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(thr);
	ut_ad((thr->state == QUE_THR_LOCK_WAIT)
	      || (thr->state == QUE_THR_PROCEDURE_WAIT)
	      || (thr->state == QUE_THR_SIG_REPLY_WAIT));
	ut_ad(thr->run_node);

	thr->prev_node = thr->run_node;

	was_active = thr->is_active;

	que_thr_move_to_run_state(thr);

	if (was_active) {

		return;
	}

	if (next_thr && *next_thr == NULL) {
		*next_thr = thr;
	} else {
		ut_a(0);
		srv_que_task_enqueue_low(thr);
	}
}

/**************************************************************************
Same as que_thr_end_wait, but no parameter next_thr available. */

void
que_thr_end_wait_no_next_thr(
/*=========================*/
	que_thr_t*	thr)	/* in: query thread in the QUE_THR_LOCK_WAIT,
				or QUE_THR_PROCEDURE_WAIT, or
				QUE_THR_SIG_REPLY_WAIT state */
{
	ibool	was_active;

	ut_a(thr->state == QUE_THR_LOCK_WAIT);	/* In MySQL this is the
						only possible state here */
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(thr);
	ut_ad((thr->state == QUE_THR_LOCK_WAIT)
	      || (thr->state == QUE_THR_PROCEDURE_WAIT)
	      || (thr->state == QUE_THR_SIG_REPLY_WAIT));

	was_active = thr->is_active;

	que_thr_move_to_run_state(thr);

	if (was_active) {

		return;
	}

	/* In MySQL we let the OS thread (not just the query thread) to wait
	for the lock to be released: */

	srv_release_mysql_thread_if_suspended(thr);

	/* srv_que_task_enqueue_low(thr); */
}

/**************************************************************************
Inits a query thread for a command. */
UNIV_INLINE
void
que_thr_init_command(
/*=================*/
	que_thr_t*	thr)	/* in: query thread */
{
	thr->run_node = thr;
	thr->prev_node = thr->common.parent;

	que_thr_move_to_run_state(thr);
}

/**************************************************************************
Starts execution of a command in a query fork. Picks a query thread which
is not in the QUE_THR_RUNNING state and moves it to that state. If none
can be chosen, a situation which may arise in parallelized fetches, NULL
is returned. */

que_thr_t*
que_fork_start_command(
/*===================*/
				/* out: a query thread of the graph moved to
				QUE_THR_RUNNING state, or NULL; the query
				thread should be executed by que_run_threads
				by the caller */
	que_fork_t*	fork)	/* in: a query fork */
{
	que_thr_t*	thr;

	fork->state = QUE_FORK_ACTIVE;

	fork->last_sel_node = NULL;

	/* Choose the query thread to run: usually there is just one thread,
	but in a parallelized select, which necessarily is non-scrollable,
	there may be several to choose from */

	/*---------------------------------------------------------------
	First we try to find a query thread in the QUE_THR_COMMAND_WAIT state
	*/

	thr = UT_LIST_GET_FIRST(fork->thrs);

	while (thr != NULL) {
		if (thr->state == QUE_THR_COMMAND_WAIT) {

			/* We have to send the initial message to query thread
			to start it */

			que_thr_init_command(thr);

			return(thr);
		}

		ut_ad(thr->state != QUE_THR_LOCK_WAIT);

		thr = UT_LIST_GET_NEXT(thrs, thr);
	}

	/*----------------------------------------------------------------
	Then we try to find a query thread in the QUE_THR_SUSPENDED state */

	thr = UT_LIST_GET_FIRST(fork->thrs);

	while (thr != NULL) {
		if (thr->state == QUE_THR_SUSPENDED) {
			/* In this case the execution of the thread was
			suspended: no initial message is needed because
			execution can continue from where it was left */

			que_thr_move_to_run_state(thr);

			return(thr);
		}

		thr = UT_LIST_GET_NEXT(thrs, thr);
	}

	/*-----------------------------------------------------------------
	Then we try to find a query thread in the QUE_THR_COMPLETED state */

	thr = UT_LIST_GET_FIRST(fork->thrs);

	while (thr != NULL) {
		if (thr->state == QUE_THR_COMPLETED) {
			que_thr_init_command(thr);

			return(thr);
		}

		thr = UT_LIST_GET_NEXT(thrs, thr);
	}

	/* Else we return NULL */
	return(NULL);
}

/**************************************************************************
After signal handling is finished, returns control to a query graph error
handling routine. (Currently, just returns the control to the root of the
graph so that the graph can communicate an error message to the client.) */

void
que_fork_error_handle(
/*==================*/
	trx_t*	trx __attribute__((unused)),	/* in: trx */
	que_t*	fork)	/* in: query graph which was run before signal
			handling started, NULL not allowed */
{
	que_thr_t*	thr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(trx->sess->state == SESS_ERROR);
	ut_ad(UT_LIST_GET_LEN(trx->reply_signals) == 0);
	ut_ad(UT_LIST_GET_LEN(trx->wait_thrs) == 0);

	thr = UT_LIST_GET_FIRST(fork->thrs);

	while (thr != NULL) {
		ut_ad(!thr->is_active);
		ut_ad(thr->state != QUE_THR_SIG_REPLY_WAIT);
		ut_ad(thr->state != QUE_THR_LOCK_WAIT);

		thr->run_node = thr;
		thr->prev_node = thr->child;
		thr->state = QUE_THR_COMPLETED;

		thr = UT_LIST_GET_NEXT(thrs, thr);
	}

	thr = UT_LIST_GET_FIRST(fork->thrs);

	que_thr_move_to_run_state(thr);

	ut_a(0);
	srv_que_task_enqueue_low(thr);
}

/********************************************************************
Tests if all the query threads in the same fork have a given state. */
UNIV_INLINE
ibool
que_fork_all_thrs_in_state(
/*=======================*/
				/* out: TRUE if all the query threads in the
				same fork were in the given state */
	que_fork_t*	fork,	/* in: query fork */
	ulint		state)	/* in: state */
{
	que_thr_t*	thr_node;

	thr_node = UT_LIST_GET_FIRST(fork->thrs);

	while (thr_node != NULL) {
		if (thr_node->state != state) {

			return(FALSE);
		}

		thr_node = UT_LIST_GET_NEXT(thrs, thr_node);
	}

	return(TRUE);
}

/**************************************************************************
Calls que_graph_free_recursive for statements in a statement list. */
static
void
que_graph_free_stat_list(
/*=====================*/
	que_node_t*	node)	/* in: first query graph node in the list */
{
	while (node) {
		que_graph_free_recursive(node);

		node = que_node_get_next(node);
	}
}

/**************************************************************************
Frees a query graph, but not the heap where it was created. Does not free
explicit cursor declarations, they are freed in que_graph_free. */

void
que_graph_free_recursive(
/*=====================*/
	que_node_t*	node)	/* in: query graph node */
{
	que_fork_t*	fork;
	que_thr_t*	thr;
	undo_node_t*	undo;
	sel_node_t*	sel;
	ins_node_t*	ins;
	upd_node_t*	upd;
	tab_node_t*	cre_tab;
	ind_node_t*	cre_ind;

	if (node == NULL) {

		return;
	}

	switch (que_node_get_type(node)) {

	case QUE_NODE_FORK:
		fork = node;

		thr = UT_LIST_GET_FIRST(fork->thrs);

		while (thr) {
			que_graph_free_recursive(thr);

			thr = UT_LIST_GET_NEXT(thrs, thr);
		}

		break;
	case QUE_NODE_THR:

		thr = node;

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

		undo = node;

		mem_heap_free(undo->heap);

		break;
	case QUE_NODE_SELECT:

		sel = node;

		sel_node_free_private(sel);

		break;
	case QUE_NODE_INSERT:

		ins = node;

		que_graph_free_recursive(ins->select);

		mem_heap_free(ins->entry_sys_heap);

		break;
	case QUE_NODE_UPDATE:

		upd = node;

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
		cre_tab = node;

		que_graph_free_recursive(cre_tab->tab_def);
		que_graph_free_recursive(cre_tab->col_def);
		que_graph_free_recursive(cre_tab->commit_node);

		mem_heap_free(cre_tab->heap);

		break;
	case QUE_NODE_CREATE_INDEX:
		cre_ind = node;

		que_graph_free_recursive(cre_ind->ind_def);
		que_graph_free_recursive(cre_ind->field_def);
		que_graph_free_recursive(cre_ind->commit_node);

		mem_heap_free(cre_ind->heap);

		break;
	case QUE_NODE_PROC:
		que_graph_free_stat_list(((proc_node_t*)node)->stat_list);

		break;
	case QUE_NODE_IF:
		que_graph_free_stat_list(((if_node_t*)node)->stat_list);
		que_graph_free_stat_list(((if_node_t*)node)->else_part);
		que_graph_free_stat_list(((if_node_t*)node)->elsif_list);

		break;
	case QUE_NODE_ELSIF:
		que_graph_free_stat_list(((elsif_node_t*)node)->stat_list);

		break;
	case QUE_NODE_WHILE:
		que_graph_free_stat_list(((while_node_t*)node)->stat_list);

		break;
	case QUE_NODE_FOR:
		que_graph_free_stat_list(((for_node_t*)node)->stat_list);

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

/**************************************************************************
Frees a query graph. */

void
que_graph_free(
/*===========*/
	que_t*	graph)	/* in: query graph; we assume that the memory
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

/**************************************************************************
Checks if the query graph is in a state where it should be freed, and
frees it in that case. If the session is in a state where it should be
closed, also this is done. */

ibool
que_graph_try_free(
/*===============*/
			/* out: TRUE if freed */
	que_t*	graph)	/* in: query graph */
{
	sess_t*	sess;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	sess = (graph->trx)->sess;

	if ((graph->state == QUE_FORK_BEING_FREED)
	    && (graph->n_active_thrs == 0)) {

		UT_LIST_REMOVE(graphs, sess->graphs, graph);
		que_graph_free(graph);

		sess_try_close(sess);

		return(TRUE);
	}

	return(FALSE);
}

/********************************************************************
Performs an execution step on a thr node. */
static
que_thr_t*
que_thr_node_step(
/*==============*/
				/* out: query thread to run next, or NULL
				if none */
	que_thr_t*	thr)	/* in: query thread where run_node must
				be the thread node itself */
{
	ut_ad(thr->run_node == thr);

	if (thr->prev_node == thr->common.parent) {
		/* If control to the node came from above, it is just passed
		on */

		thr->run_node = thr->child;

		return(thr);
	}

	mutex_enter(&kernel_mutex);

	if (que_thr_peek_stop(thr)) {

		mutex_exit(&kernel_mutex);

		return(thr);
	}

	/* Thread execution completed */

	thr->state = QUE_THR_COMPLETED;

	mutex_exit(&kernel_mutex);

	return(NULL);
}

/**************************************************************************
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction if thr was
not active.
***NOTE***: This and ..._mysql are  the only functions in which such a
transition is allowed to happen! */
static
void
que_thr_move_to_run_state(
/*======================*/
	que_thr_t*	thr)	/* in: an query thread */
{
	trx_t*	trx;

	ut_ad(thr->state != QUE_THR_RUNNING);

	trx = thr_get_trx(thr);

	if (!thr->is_active) {

		(thr->graph)->n_active_thrs++;

		trx->n_active_thrs++;

		thr->is_active = TRUE;

		ut_ad((thr->graph)->n_active_thrs == 1);
		ut_ad(trx->n_active_thrs == 1);
	}

	thr->state = QUE_THR_RUNNING;
}

/**************************************************************************
Decrements the query thread reference counts in the query graph and the
transaction. May start signal handling, e.g., a rollback.
*** NOTE ***:
This and que_thr_stop_for_mysql are the only functions where the reference
count can be decremented and this function may only be called from inside
que_run_threads or que_thr_check_if_switch! These restrictions exist to make
the rollback code easier to maintain. */
static
void
que_thr_dec_refer_count(
/*====================*/
	que_thr_t*	thr,		/* in: query thread */
	que_thr_t**	next_thr)	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
{
	que_fork_t*	fork;
	trx_t*		trx;
	sess_t*		sess;
	ulint		fork_type;
	ibool		stopped;

	fork = thr->common.parent;
	trx = thr_get_trx(thr);
	sess = trx->sess;

	mutex_enter(&kernel_mutex);

	ut_a(thr->is_active);

	if (thr->state == QUE_THR_RUNNING) {

		stopped = que_thr_stop(thr);

		if (!stopped) {
			/* The reason for the thr suspension or wait was
			already canceled before we came here: continue
			running the thread */

			/* fputs("!!!!!!!! Wait already ended: continue thr\n",
			stderr); */

			if (next_thr && *next_thr == NULL) {
				/* Normally srv_suspend_mysql_thread resets
				the state to DB_SUCCESS before waiting, but
				in this case we have to do it here,
				otherwise nobody does it. */
				trx->error_state = DB_SUCCESS;

				*next_thr = thr;
			} else {
				ut_a(0);
				srv_que_task_enqueue_low(thr);
			}

			mutex_exit(&kernel_mutex);

			return;
		}
	}

	ut_ad(fork->n_active_thrs == 1);
	ut_ad(trx->n_active_thrs == 1);

	fork->n_active_thrs--;
	trx->n_active_thrs--;

	thr->is_active = FALSE;

	if (trx->n_active_thrs > 0) {

		mutex_exit(&kernel_mutex);

		return;
	}

	fork_type = fork->fork_type;

	/* Check if all query threads in the same fork are completed */

	if (que_fork_all_thrs_in_state(fork, QUE_THR_COMPLETED)) {

		if (fork_type == QUE_FORK_ROLLBACK) {
			/* This is really the undo graph used in rollback,
			no roll_node in this graph */

			ut_ad(UT_LIST_GET_LEN(trx->signals) > 0);
			ut_ad(trx->handling_signals == TRUE);

			trx_finish_rollback_off_kernel(fork, trx, next_thr);

		} else if (fork_type == QUE_FORK_PURGE) {

			/* Do nothing */
		} else if (fork_type == QUE_FORK_RECOVERY) {

			/* Do nothing */
		} else if (fork_type == QUE_FORK_MYSQL_INTERFACE) {

			/* Do nothing */
		} else {
			ut_error;	/* not used in MySQL */
		}
	}

	if (UT_LIST_GET_LEN(trx->signals) > 0 && trx->n_active_thrs == 0) {

		/* If the trx is signaled and its query thread count drops to
		zero, then we start processing a signal; from it we may get
		a new query thread to run */

		trx_sig_start_handle(trx, next_thr);
	}

	if (trx->handling_signals && UT_LIST_GET_LEN(trx->signals) == 0) {

		trx_end_signal_handling(trx);
	}

	mutex_exit(&kernel_mutex);
}

/**************************************************************************
Stops a query thread if graph or trx is in a state requiring it. The
conditions are tested in the order (1) graph, (2) trx. The kernel mutex has
to be reserved. */

ibool
que_thr_stop(
/*=========*/
				/* out: TRUE if stopped */
	que_thr_t*	thr)	/* in: query thread */
{
	trx_t*	trx;
	que_t*	graph;
	ibool	ret	= TRUE;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	graph = thr->graph;
	trx = graph->trx;

	if (graph->state == QUE_FORK_COMMAND_WAIT) {
		thr->state = QUE_THR_SUSPENDED;

	} else if (trx->que_state == TRX_QUE_LOCK_WAIT) {

		UT_LIST_ADD_FIRST(trx_thrs, trx->wait_thrs, thr);
		thr->state = QUE_THR_LOCK_WAIT;

	} else if (trx->error_state != DB_SUCCESS
		   && trx->error_state != DB_LOCK_WAIT) {

		/* Error handling built for the MySQL interface */
		thr->state = QUE_THR_COMPLETED;

	} else if (UT_LIST_GET_LEN(trx->signals) > 0
		   && graph->fork_type != QUE_FORK_ROLLBACK) {

		thr->state = QUE_THR_SUSPENDED;
	} else {
		ut_ad(graph->state == QUE_FORK_ACTIVE);

		ret = FALSE;
	}

	return(ret);
}

/**************************************************************************
A patch for MySQL used to 'stop' a dummy query thread used in MySQL. The
query thread is stopped and made inactive, except in the case where
it was put to the lock wait state in lock0lock.c, but the lock has already
been granted or the transaction chosen as a victim in deadlock resolution. */

void
que_thr_stop_for_mysql(
/*===================*/
	que_thr_t*	thr)	/* in: query thread */
{
	trx_t*	trx;

	trx = thr_get_trx(thr);

	mutex_enter(&kernel_mutex);

	if (thr->state == QUE_THR_RUNNING) {

		if (trx->error_state != DB_SUCCESS
		    && trx->error_state != DB_LOCK_WAIT) {

			/* Error handling built for the MySQL interface */
			thr->state = QUE_THR_COMPLETED;
		} else {
			/* It must have been a lock wait but the lock was
			already released, or this transaction was chosen
			as a victim in selective deadlock resolution */

			mutex_exit(&kernel_mutex);

			return;
		}
	}

	ut_ad(thr->is_active == TRUE);
	ut_ad(trx->n_active_thrs == 1);
	ut_ad(thr->graph->n_active_thrs == 1);

	thr->is_active = FALSE;
	(thr->graph)->n_active_thrs--;

	trx->n_active_thrs--;

	mutex_exit(&kernel_mutex);
}

/**************************************************************************
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction if thr was
not active. */

void
que_thr_move_to_run_state_for_mysql(
/*================================*/
	que_thr_t*	thr,	/* in: an query thread */
	trx_t*		trx)	/* in: transaction */
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

		trx->n_active_thrs++;

		thr->is_active = TRUE;
	}

	thr->state = QUE_THR_RUNNING;
}

/**************************************************************************
A patch for MySQL used to 'stop' a dummy query thread used in MySQL
select, when there is no error or lock wait. */

void
que_thr_stop_for_mysql_no_error(
/*============================*/
	que_thr_t*	thr,	/* in: query thread */
	trx_t*		trx)	/* in: transaction */
{
	ut_ad(thr->state == QUE_THR_RUNNING);
	ut_ad(thr->is_active == TRUE);
	ut_ad(trx->n_active_thrs == 1);
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
	(thr->graph)->n_active_thrs--;

	trx->n_active_thrs--;
}

/********************************************************************
Get the first containing loop node (e.g. while_node_t or for_node_t) for the
given node, or NULL if the node is not within a loop. */

que_node_t*
que_node_get_containing_loop_node(
/*==============================*/
				/* out: containing loop node, or NULL. */
	que_node_t*	node)	/* in: node */
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

/**************************************************************************
Prints info of an SQL query graph node. */

void
que_node_print_info(
/*================*/
	que_node_t*	node)	/* in: query graph node */
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

/**************************************************************************
Performs an execution step on a query thread. */
UNIV_INLINE
que_thr_t*
que_thr_step(
/*=========*/
				/* out: query thread to run next: it may
				differ from the input parameter if, e.g., a
				subprocedure call is made */
	que_thr_t*	thr)	/* in: query thread */
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
		/*
		thr = que_lock_step(thr);
		*/
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

/**************************************************************************
Run a query thread until it finishes or encounters e.g. a lock wait. */
static
void
que_run_threads_low(
/*================*/
	que_thr_t*	thr)	/* in: query thread */
{
	que_thr_t*	next_thr;
	ulint		cumul_resource;
	ulint		loop_count;

	ut_ad(thr->state == QUE_THR_RUNNING);
	ut_a(thr_get_trx(thr)->error_state == DB_SUCCESS);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!mutex_own(&kernel_mutex));
#endif /* UNIV_SYNC_DEBUG */

	/* cumul_resource counts how much resources the OS thread (NOT the
	query thread) has spent in this function */

	loop_count = QUE_MAX_LOOPS_WITHOUT_CHECK;
	cumul_resource = 0;
loop:
	/* Check that there is enough space in the log to accommodate
	possible log entries by this query step; if the operation can touch
	more than about 4 pages, checks must be made also within the query
	step! */

	log_free_check();

	/* Perform the actual query step: note that the query thread
	may change if, e.g., a subprocedure call is made */

	/*-------------------------*/
	next_thr = que_thr_step(thr);
	/*-------------------------*/

	ut_a(!next_thr || (thr_get_trx(next_thr)->error_state == DB_SUCCESS));

	loop_count++;

	if (next_thr != thr) {
		ut_a(next_thr == NULL);

		/* This can change next_thr to a non-NULL value if there was
		a lock wait that already completed. */
		que_thr_dec_refer_count(thr, &next_thr);

		if (next_thr == NULL) {

			return;
		}

		loop_count = QUE_MAX_LOOPS_WITHOUT_CHECK;

		thr = next_thr;
	}

	goto loop;
}

/**************************************************************************
Run a query thread. Handles lock waits. */
void
que_run_threads(
/*============*/
	que_thr_t*	thr)	/* in: query thread */
{
loop:
	ut_a(thr_get_trx(thr)->error_state == DB_SUCCESS);
	que_run_threads_low(thr);

	mutex_enter(&kernel_mutex);

	switch (thr->state) {

	case QUE_THR_RUNNING:
		/* There probably was a lock wait, but it already ended
		before we came here: continue running thr */

		mutex_exit(&kernel_mutex);

		goto loop;

	case QUE_THR_LOCK_WAIT:
		mutex_exit(&kernel_mutex);

		/* The ..._mysql_... function works also for InnoDB's
		internal threads. Let us wait that the lock wait ends. */

		srv_suspend_mysql_thread(thr);

		if (thr_get_trx(thr)->error_state != DB_SUCCESS) {
			/* thr was chosen as a deadlock victim or there was
			a lock wait timeout */

			que_thr_dec_refer_count(thr, NULL);

			return;
		}

		goto loop;

	case QUE_THR_COMPLETED:
	case QUE_THR_COMMAND_WAIT:
		/* Do nothing */
		break;

	default:
		ut_error;
	}

	mutex_exit(&kernel_mutex);
}

/*************************************************************************
Evaluate the given SQL. */

ulint
que_eval_sql(
/*=========*/
				/* out: error code or DB_SUCCESS */
	pars_info_t*	info,	/* in: info struct, or NULL */
	const char*	sql,	/* in: SQL string */
	ibool		reserve_dict_mutex,
				/* in: if TRUE, acquire/release
				dict_sys->mutex around call to pars_sql. */
	trx_t*		trx)	/* in: trx */
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

	que_graph_free(graph);

	return(trx->error_state);
}
