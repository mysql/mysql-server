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
#include "odbc0odbc.h"

#define QUE_PARALLELIZE_LIMIT	(64 * 256 * 256 * 256)
#define QUE_ROUND_ROBIN_LIMIT	(64 * 256 * 256 * 256)
#define QUE_MAX_LOOPS_WITHOUT_CHECK	16

/* If the following flag is set TRUE, the module will print trace info
of SQL execution in the UNIV_SQL_DEBUG version */
ibool	que_trace_on		= FALSE;

ibool	que_always_false	= FALSE;

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
/**************************************************************************
Tries to parallelize query if it is not parallel enough yet. */
static
que_thr_t*
que_try_parallelize(
/*================*/
				/* out: next thread to execute */
	que_thr_t*	thr);	/* in: query thread */

#ifdef notdefined
/********************************************************************
Adds info about the number of inserted rows etc. to the message to the
client. */
static
void
que_thr_add_update_info(
/*====================*/
	que_thr_t*	thr)	/* in: query thread */
{
	que_fork_t*	graph;

	graph = thr->graph;

	mach_write_to_8(thr->msg_buf + SESS_SRV_MSG_N_INSERTS,
							graph->n_inserts);
	mach_write_to_8(thr->msg_buf + SESS_SRV_MSG_N_UPDATES,
							graph->n_updates);
	mach_write_to_8(thr->msg_buf + SESS_SRV_MSG_N_DELETES,
							graph->n_deletes);
}
#endif	

/***************************************************************************
Adds a query graph to the session's list of graphs. */

void
que_graph_publish(
/*==============*/
	que_t*	graph,	/* in: graph */
	sess_t*	sess)	/* in: session */
{
	ut_ad(mutex_own(&kernel_mutex));

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

	thr->graph = parent->graph;

	thr->state = QUE_THR_COMMAND_WAIT;

	thr->is_active = FALSE;	

	thr->run_node = NULL;
	thr->resource = 0;

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

	ut_ad(mutex_own(&kernel_mutex));
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
	ut_ad(mutex_own(&kernel_mutex));
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
	que_fork_t* 	fork,	/* in: a query fork */
	ulint		command,/* in: command SESS_COMM_FETCH_NEXT, ... */
	ulint		param)	/* in: possible parameter to the command */
{
	que_thr_t*	thr;
	
	/* Set the command parameters in the fork root */
	fork->command = command;
	fork->param = param;	

	fork->state = QUE_FORK_ACTIVE;
	
	fork->last_sel_node = NULL;

	/* Choose the query thread to run: usually there is just one thread,
	but in a parallelized select, which necessarily is non-scrollable,
	there may be several to choose from */

	/*---------------------------------------------------------------
	First we try to find a query thread in the QUE_THR_COMMAND_WAIT state */
	
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
	trx_t*	trx,	/* in: trx */
	que_t*	fork)	/* in: query graph which was run before signal
			handling started, NULL not allowed */
{
	que_thr_t*	thr;

	ut_ad(mutex_own(&kernel_mutex));
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
		ut_a(0);
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

	ut_ad(mutex_own(&kernel_mutex));

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

/**************************************************************************
Handles an SQL error noticed during query thread execution. Currently,
does nothing! */

void
que_thr_handle_error(
/*=================*/
	que_thr_t*	thr,	/* in: query thread */
	ulint		err_no,	/* in: error number */
	byte*		err_str,/* in, own: error string or NULL; NOTE: the
				function will take care of freeing of the
				string! */
	ulint		err_len)/* in: error string length */	
{
	UT_NOT_USED(thr);
	UT_NOT_USED(err_no);
	UT_NOT_USED(err_str);
	UT_NOT_USED(err_len);
	
	/* Does nothing */
}

/**************************************************************************
Tries to parallelize query if it is not parallel enough yet. */
static
que_thr_t*
que_try_parallelize(
/*================*/
				/* out: next thread to execute */
	que_thr_t*	thr)	/* in: query thread */
{
	ut_ad(thr);

	/* Does nothing yet */

	return(thr);
}

/********************************************************************
Builds a command completed-message to the client. */
static
ulint
que_build_srv_msg(
/*==============*/
				/* out: message data length */
	byte*		buf,	/* in: message buffer */
	que_fork_t*	fork,	/* in: query graph where execution completed */
	sess_t*		sess)	/* in: session */
{
	ulint	len;
	
	/* Currently, we only support stored procedures: */
	ut_ad(fork->fork_type == QUE_FORK_PROCEDURE);

	if (sess->state == SESS_ERROR) {

		return(0);
	}

  	sess_srv_msg_init(sess, buf, SESS_SRV_SUCCESS);

	len = pars_proc_write_output_params_to_buf(buf + SESS_SRV_MSG_DATA,
									fork);
	return(len);
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
This and que_thr_stop_for_mysql are
the only functions where the reference count can be decremented and
this function may only be called from inside que_run_threads or
que_thr_check_if_switch! These restrictions exist to make the rollback code
easier to maintain. */
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
	ibool		send_srv_msg		= FALSE;
	ibool		release_stored_proc	= FALSE;
	ulint		msg_len;
	byte		msg_buf[ODBC_DATAGRAM_SIZE];
	ulint		fork_type;
	ibool		stopped;
	
	fork = thr->common.parent;
	trx = thr->graph->trx;
	sess = trx->sess;

	mutex_enter(&kernel_mutex);

	ut_a(thr->is_active);

	if (thr->state == QUE_THR_RUNNING) {

		stopped = que_thr_stop(thr);

		if (!stopped) {
			/* The reason for the thr suspension or wait was
			already canceled before we came here: continue
			running the thread */

			/* printf(
			"!!!!!!!!!! Wait already ended: continue thr\n"); */

			if (next_thr && *next_thr == NULL) {
				*next_thr = thr;
			} else {
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
		} else if (fork->common.parent == NULL
				&& fork->caller == NULL
			 	&& UT_LIST_GET_LEN(trx->signals) == 0) {

			ut_a(0);	/* not used in MySQL */

			/* Reply to the client */ 
	
			/* que_thr_add_update_info(thr); */
		
			fork->state = QUE_FORK_COMMAND_WAIT;

			msg_len = que_build_srv_msg(msg_buf, fork, sess);

			send_srv_msg = TRUE;

			if (fork->fork_type == QUE_FORK_PROCEDURE) {

				release_stored_proc = TRUE;
			}

			ut_ad(trx->graph == fork);

			trx->graph = NULL;
		} else {
			/* Subprocedure calls not implemented yet */
			ut_a(0);
		}
	}

	if (UT_LIST_GET_LEN(trx->signals) > 0 && trx->n_active_thrs == 0) {

		ut_ad(!send_srv_msg);

	    	/* If the trx is signaled and its query thread count drops to
		zero, then we start processing a signal; from it we may get
		a new query thread to run */

		trx_sig_start_handle(trx, next_thr);
	}

	if (trx->handling_signals && UT_LIST_GET_LEN(trx->signals) == 0) {

		trx_end_signal_handling(trx);
	}

	mutex_exit(&kernel_mutex);

	if (send_srv_msg) {
		/* Note that, as we do not own the kernel mutex at this point,
		and neither do we own it all the time when doing the actual
		communication operation within the next function, it is
		possible that the messages will not get delivered in the right
		sequential order. This is possible if the client communicates
		an extra message to the server while the message below is still
		undelivered. But then the client should notice that there
		is an error in the order numbers of the messages. */
		
		sess_command_completed_message(sess, msg_buf, msg_len);
	}

	if (release_stored_proc) {

		/* Return the stored procedure graph to the dictionary cache */

		dict_procedure_release_parsed_copy(fork);
	}
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

	ut_ad(mutex_own(&kernel_mutex));
	
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
A patch for MySQL used to 'stop' a dummy query thread used in MySQL. */

void
que_thr_stop_for_mysql(
/*===================*/
	que_thr_t*	thr)	/* in: query thread */
{
	ibool	stopped 	= FALSE;
	trx_t*	trx;

	trx = thr_get_trx(thr);
	
	mutex_enter(&kernel_mutex);

	if (thr->state == QUE_THR_RUNNING) {

		if (trx->error_state != DB_SUCCESS
			   	&& trx->error_state != DB_LOCK_WAIT) {

			/* Error handling built for the MySQL interface */
			thr->state = QUE_THR_COMPLETED;

			stopped = TRUE;
		}
		
		if (!stopped) {
			/* It must have been a lock wait but the
			lock was already released */

			mutex_exit(&kernel_mutex);

			return;
		}
	}
		
	thr->is_active = FALSE;
	(thr->graph)->n_active_thrs--;

	trx->n_active_thrs--;

	mutex_exit(&kernel_mutex);
}

/**************************************************************************
Prints info of an SQL query graph node. */

void
que_node_print_info(
/*================*/
	que_node_t*	node)	/* in: query graph node */
{
	ulint	type;
	char*	str;
	ulint	addr;

	type = que_node_get_type(node);

	addr = (ulint)node;

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
	} else {
		str = "UNKNOWN NODE TYPE";
	}

	printf("Node type %lu: %s, address %lx\n", type, str, addr);
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
	
	ut_ad(thr->state == QUE_THR_RUNNING);

	thr->resource++;
	
	type = que_node_get_type(thr->run_node);
	node = thr->run_node;

	old_thr = thr;
	
#ifdef UNIV_DEBUG
	if (que_trace_on) {
		printf("To execute: ");
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
				trx = thr_get_trx(thr);
				trx->last_sql_stat_start.least_undo_no
							= trx->undo_no;
			}
			
			proc_step(thr);
		} else if (type == QUE_NODE_WHILE) {
			while_step(thr);
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

	old_thr->prev_node = node;

	return(thr);
}

/***********************************************************************
Checks if there is a need for a query thread switch or stopping the current
thread. */
static
que_thr_t*
que_thr_check_if_switch(
/*====================*/
	que_thr_t*	thr,		/* in: current query thread */
	ulint*		cumul_resource)	/* in: amount of resources used
					by the current call of que_run_threads
					(resources used by the OS thread!) */
{
	que_thr_t*	next_thr;
	ibool		stopped;

	if (que_thr_peek_stop(thr)) {

		mutex_enter(&kernel_mutex);

		stopped = que_thr_stop(thr);

		mutex_exit(&kernel_mutex);

		if (stopped) {
			/* If a signal is processed, we may get a new query
			thread next_thr to run */

			next_thr = NULL;

			que_thr_dec_refer_count(thr, &next_thr);

			if (next_thr == NULL) {

				return(NULL);
			}

			thr = next_thr;
		}
	}

	if (thr->resource > QUE_PARALLELIZE_LIMIT) { 

		/* Try parallelization of the query thread */
		thr = que_try_parallelize(thr);

		thr->resource = 0;
	}

	(*cumul_resource)++;

	if (*cumul_resource > QUE_ROUND_ROBIN_LIMIT) {

		/* It is time to round-robin query threads in the
		server task queue */

		if (srv_get_thread_type() == SRV_COM) {
			/* This OS thread is a SRV_COM thread: we put
			the query thread to the task queue and return
			to allow the OS thread to receive more
			messages from clients */

			ut_ad(thr->is_active);
	    	
			srv_que_task_enqueue(thr);

			return(NULL);
		} else {
			/* Change the query thread if there is another
			in the server task queue */

			thr = srv_que_round_robin(thr);
		}

		*cumul_resource = 0;
	}

	return(thr);
}

/**************************************************************************
Runs query threads. Note that the individual query thread which is run
within this function may change if, e.g., the OS thread executing this
function uses a threshold amount of resources. */

void
que_run_threads(
/*============*/
	que_thr_t*	thr)	/* in: query thread which is run initially */
{
	que_thr_t*	next_thr;
	ulint		cumul_resource;	
	ulint		loop_count;
	
	ut_ad(thr->state == QUE_THR_RUNNING);
	ut_ad(!mutex_own(&kernel_mutex));

	/* cumul_resource counts how much resources the OS thread (NOT the
	query thread) has spent in this function */

	loop_count = QUE_MAX_LOOPS_WITHOUT_CHECK;
	cumul_resource = 0;	
loop:
	if (loop_count >= QUE_MAX_LOOPS_WITHOUT_CHECK) {

/* In MySQL this thread switch is never needed! 

		loop_count = 0;

		next_thr = que_thr_check_if_switch(thr, &cumul_resource);

		if (next_thr != thr) {
			if (next_thr == NULL) {
	
				return;
			}

			loop_count = QUE_MAX_LOOPS_WITHOUT_CHECK;
		}
				
		thr = next_thr;
*/
	}

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

	/* Test the effect on performance of adding extra mutex
	reservations */

/*	if (srv_test_extra_mutexes) {
		mutex_enter(&kernel_mutex);
		mutex_exit(&kernel_mutex);
	}	
*/
	/* TRUE below denotes that the thread is allowed to own the dictionary
	mutex, though */
	ut_ad(sync_thread_levels_empty_gen(TRUE));

	loop_count++;

	if (next_thr != thr) {
		que_thr_dec_refer_count(thr, &next_thr);

		if (next_thr == NULL) {

			return;
		}

		loop_count = QUE_MAX_LOOPS_WITHOUT_CHECK;

		thr = next_thr;
	}

	goto loop;
}
