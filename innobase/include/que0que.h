/******************************************************
Query graph

(c) 1996 Innobase Oy

Created 5/27/1996 Heikki Tuuri
*******************************************************/

#ifndef que0que_h
#define que0que_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0trx.h"
#include "srv0srv.h"
#include "usr0types.h"
#include "que0types.h"
#include "row0types.h"
#include "pars0types.h"

/* If the following flag is set TRUE, the module will print trace info
of SQL execution in the UNIV_SQL_DEBUG version */
extern ibool	que_trace_on;

/***************************************************************************
Adds a query graph to the session's list of graphs. */

void
que_graph_publish(
/*==============*/
	que_t*	graph,	/* in: graph */
	sess_t*	sess);	/* in: session */
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
	mem_heap_t*	heap);		/* in: memory heap where created */
/***************************************************************************
Gets the first thr in a fork. */
UNIV_INLINE
que_thr_t*
que_fork_get_first_thr(
/*===================*/
	que_fork_t*	fork); 	/* in: query fork */
/***************************************************************************
Gets the child node of the first thr in a fork. */
UNIV_INLINE
que_node_t*
que_fork_get_child(
/*===============*/
	que_fork_t*	fork);	/* in: query fork */
/***************************************************************************
Sets the parent of a graph node. */
UNIV_INLINE
void
que_node_set_parent(
/*================*/
	que_node_t*	node,	/* in: graph node */
	que_node_t*	parent);/* in: parent */
/***************************************************************************
Creates a query graph thread node. */

que_thr_t*
que_thr_create(
/*===========*/
				/* out, own: query thread node */
	que_fork_t*	parent,	/* in: parent node, i.e., a fork node */
	mem_heap_t*	heap);	/* in: memory heap where created */
/**************************************************************************
Checks if the query graph is in a state where it should be freed, and
frees it in that case. If the session is in a state where it should be
closed, also this is done. */

ibool
que_graph_try_free(
/*===============*/
			/* out: TRUE if freed */
	que_t*	graph);	/* in: query graph */
/**************************************************************************
Frees a query graph, but not the heap where it was created. Does not free
explicit cursor declarations, they are freed in que_graph_free. */

void
que_graph_free_recursive(
/*=====================*/
	que_node_t*	node);	/* in: query graph node */
/**************************************************************************
Frees a query graph. */

void
que_graph_free(
/*===========*/
	que_t*	graph);	/* in: query graph; we assume that the memory
			heap where this graph was created is private
			to this graph: if not, then use
			que_graph_free_recursive and free the heap
			afterwards! */
/**************************************************************************
Stops a query thread if graph or trx is in a state requiring it. The
conditions are tested in the order (1) graph, (2) trx. The kernel mutex has
to be reserved. */

ibool
que_thr_stop(
/*=========*/
				/* out: TRUE if stopped */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Moves a thread from another state to the QUE_THR_RUNNING state. Increments
the n_active_thrs counters of the query graph and transaction. */
UNIV_INLINE
void
que_thr_move_to_run_state_for_mysql(
/*================================*/
	que_thr_t*	thr,	/* in: an query thread */
	trx_t*		trx);	/* in: transaction */
/**************************************************************************
A patch for MySQL used to 'stop' a dummy query thread used in MySQL
select, when there is no error or lock wait. */
UNIV_INLINE
void
que_thr_stop_for_mysql_no_error(
/*============================*/
	que_thr_t*	thr,	/* in: query thread */
	trx_t*		trx);	/* in: transaction */
/**************************************************************************
A patch for MySQL used to 'stop' a dummy query thread used in MySQL
select. */

void
que_thr_stop_for_mysql(
/*===================*/
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Runs query threads. Note that the individual query thread which is run
within this function may change if, e.g., the OS thread executing this
function uses a threshold amount of resources. */

void
que_run_threads(
/*============*/
	que_thr_t*	thr);	/* in: query thread which is run initially */
/**************************************************************************
After signal handling is finished, returns control to a query graph error
handling routine. (Currently, just returns the control to the root of the
graph so that the graph can communicate an error message to the client.) */

void
que_fork_error_handle(
/*==================*/
	trx_t*	trx,	/* in: trx */
	que_t*	fork);	/* in: query graph which was run before signal
			handling started, NULL not allowed */
/**************************************************************************
Handles an SQL error noticed during query thread execution. At the moment,
does nothing! */

void
que_thr_handle_error(
/*=================*/
	que_thr_t*	thr,	/* in: query thread */
	ulint		err_no,	/* in: error number */
	byte*		err_str,/* in, own: error string or NULL; NOTE: the
				function will take care of freeing of the
				string! */
	ulint		err_len);/* in: error string length */	
/**************************************************************************
Moves a suspended query thread to the QUE_THR_RUNNING state and releases
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
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
/**************************************************************************
Same as que_thr_end_wait, but no parameter next_thr available. */

void
que_thr_end_wait_no_next_thr(
/*=========================*/
	que_thr_t*	thr);		/* in: query thread in the
					QUE_THR_LOCK_WAIT,
					or QUE_THR_PROCEDURE_WAIT, or
					QUE_THR_SIG_REPLY_WAIT state */
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
	ulint		param);	/* in: possible parameter to the command */
/***************************************************************************
Gets the trx of a query thread. */
UNIV_INLINE
trx_t*
thr_get_trx(
/*========*/
	que_thr_t*	thr);	/* in: query thread */
/***************************************************************************
Gets the type of a graph node. */
UNIV_INLINE
ulint
que_node_get_type(
/*==============*/
	que_node_t*	node);	/* in: graph node */
/***************************************************************************
Gets pointer to the value data type field of a graph node. */
UNIV_INLINE
dtype_t*
que_node_get_data_type(
/*===================*/
	que_node_t*	node);	/* in: graph node */
/***************************************************************************
Gets pointer to the value dfield of a graph node. */
UNIV_INLINE
dfield_t*
que_node_get_val(
/*=============*/
	que_node_t*	node);	/* in: graph node */
/***************************************************************************
Gets the value buffer size of a graph node. */
UNIV_INLINE
ulint
que_node_get_val_buf_size(
/*======================*/
				/* out: val buffer size, not defined if
				val.data == NULL in node */
	que_node_t*	node);	/* in: graph node */
/***************************************************************************
Sets the value buffer size of a graph node. */
UNIV_INLINE
void
que_node_set_val_buf_size(
/*======================*/
	que_node_t*	node,	/* in: graph node */
	ulint		size);	/* in: size */
/*************************************************************************
Gets the next list node in a list of query graph nodes. */
UNIV_INLINE
que_node_t*
que_node_get_next(
/*==============*/
	que_node_t*	node);	/* in: node in a list */
/*************************************************************************
Gets the parent node of a query graph node. */
UNIV_INLINE
que_node_t*
que_node_get_parent(
/*================*/
				/* out: parent node or NULL */
	que_node_t*	node);	/* in: node */
/*************************************************************************
Catenates a query graph node to a list of them, possible empty list. */
UNIV_INLINE
que_node_t*
que_node_list_add_last(
/*===================*/
					/* out: one-way list of nodes */
	que_node_t*	node_list,	/* in: node list, or NULL */
	que_node_t*	node);		/* in: node */
/*************************************************************************
Gets a query graph node list length. */
UNIV_INLINE
ulint
que_node_list_get_len(
/*==================*/
					/* out: length, for NULL list 0 */
	que_node_t*	node_list);	/* in: node list, or NULL */
/**************************************************************************
Checks if graph, trx, or session is in a state where the query thread should
be stopped. */
UNIV_INLINE
ibool
que_thr_peek_stop(
/*==============*/
				/* out: TRUE if should be stopped; NOTE that
				if the peek is made without reserving the
				kernel mutex, then another peek with the
				mutex reserved is necessary before deciding
				the actual stopping */
	que_thr_t*	thr);	/* in: query thread */
/***************************************************************************
Returns TRUE if the query graph is for a SELECT statement. */
UNIV_INLINE
ibool
que_graph_is_select(
/*================*/
					/* out: TRUE if a select */
	que_t*		graph);		/* in: graph */
/**************************************************************************
Prints info of an SQL query graph node. */

void
que_node_print_info(
/*================*/
	que_node_t*	node);	/* in: query graph node */


/* Query graph query thread node: the fields are protected by the kernel
mutex with the exceptions named below */

struct que_thr_struct{
	que_common_t	common;		/* type: QUE_NODE_THR */
	que_node_t*	child;		/* graph child node */
	que_t*		graph;		/* graph where this node belongs */
	ibool		is_active;	/* TRUE if the thread has been set
					to the run state in
					que_thr_move_to_run_state, but not
					deactivated in
					que_thr_dec_reference_count */
	ulint		state;		/* state of the query thread */
	UT_LIST_NODE_T(que_thr_t)
			thrs;		/* list of thread nodes of the fork
					node */
	UT_LIST_NODE_T(que_thr_t)
			trx_thrs;	/* lists of threads in wait list of
					the trx */
	UT_LIST_NODE_T(que_thr_t)
			queue;		/* list of runnable thread nodes in
					the server task queue */
	/*------------------------------*/
	/* The following fields are private to the OS thread executing the
	query thread, and are not protected by the kernel mutex: */

	que_node_t*	run_node;	/* pointer to the node where the
					subgraph down from this node is
					currently executed */
	que_node_t*	prev_node;	/* pointer to the node from which
					the control came */
	ulint		resource;	/* resource usage of the query thread
					thus far */
};

/* Query graph fork node: its fields are protected by the kernel mutex */
struct que_fork_struct{
	que_common_t	common;		/* type: QUE_NODE_FORK */
	que_t*		graph;		/* query graph of this node */
	ulint		fork_type;	/* fork type */
	ulint		n_active_thrs;	/* if this is the root of a graph, the
					number query threads that have been
					started in que_thr_move_to_run_state
					but for which que_thr_dec_refer_count
					has not yet been called */
	trx_t*		trx;		/* transaction: this is set only in
					the root node */
	ulint		state;		/* state of the fork node */
	que_thr_t*	caller;		/* pointer to a possible calling query
					thread */
	UT_LIST_BASE_NODE_T(que_thr_t)
			thrs;		/* list of query threads */
	/*------------------------------*/
	/* The fields in this section are defined only in the root node */
	sym_tab_t*	sym_tab;	/* symbol table of the query,
					generated by the parser, or NULL
					if the graph was created 'by hand' */
	ulint		id;		/* id of this query graph */
	ulint		command;	/* command currently executed in the
					graph */
	ulint		param;		/* possible command parameter */

	/* The following cur_... fields are relevant only in a select graph */

	ulint		cur_end;	/* QUE_CUR_NOT_DEFINED, QUE_CUR_START,
					QUE_CUR_END */
	ulint		cur_pos;	/* if there are n rows in the result
					set, values 0 and n + 1 mean before
					first row, or after last row, depending
					on cur_end; values 1...n mean a row
					index */
	ibool		cur_on_row;	/* TRUE if cursor is on a row, i.e.,
					it is not before the first row or
					after the last row */
	dulint		n_inserts;	/* number of rows inserted */
	dulint		n_updates;	/* number of rows updated */
	dulint		n_deletes;	/* number of rows deleted */
	sel_node_t*	last_sel_node;	/* last executed select node, or NULL
					if none */
	UT_LIST_NODE_T(que_fork_t)
			graphs;		/* list of query graphs of a session
					or a stored procedure */
	/*------------------------------*/
	mem_heap_t*	heap;		/* memory heap where the fork was
					created */
	
};

/* Query fork (or graph) types */
#define QUE_FORK_SELECT_NON_SCROLL	1	/* forward-only cursor */
#define QUE_FORK_SELECT_SCROLL		2	/* scrollable cursor */
#define QUE_FORK_INSERT			3
#define QUE_FORK_UPDATE			4
#define QUE_FORK_ROLLBACK		5
			/* This is really the undo graph used in rollback,
			no signal-sending roll_node in this graph */
#define QUE_FORK_PURGE			6
#define	QUE_FORK_EXECUTE		7
#define QUE_FORK_PROCEDURE		8
#define QUE_FORK_PROCEDURE_CALL		9
#define QUE_FORK_MYSQL_INTERFACE	10
#define	QUE_FORK_RECOVERY		11

/* Query fork (or graph) states */
#define QUE_FORK_ACTIVE		1
#define QUE_FORK_COMMAND_WAIT	2
#define QUE_FORK_INVALID	3
#define QUE_FORK_BEING_FREED	4

/* Flag which is ORed to control structure statement node types */
#define QUE_NODE_CONTROL_STAT	1024

/* Query graph node types */
#define	QUE_NODE_LOCK		1
#define	QUE_NODE_INSERT		2
#define QUE_NODE_UPDATE		4
#define	QUE_NODE_CURSOR		5
#define	QUE_NODE_SELECT		6
#define	QUE_NODE_AGGREGATE	7
#define QUE_NODE_FORK		8
#define QUE_NODE_THR		9
#define QUE_NODE_UNDO		10
#define QUE_NODE_COMMIT		11
#define QUE_NODE_ROLLBACK	12
#define QUE_NODE_PURGE		13
#define QUE_NODE_CREATE_TABLE	14
#define QUE_NODE_CREATE_INDEX	15
#define QUE_NODE_SYMBOL		16
#define QUE_NODE_RES_WORD	17
#define QUE_NODE_FUNC		18
#define QUE_NODE_ORDER		19
#define QUE_NODE_PROC		(20 + QUE_NODE_CONTROL_STAT)
#define QUE_NODE_IF		(21 + QUE_NODE_CONTROL_STAT)
#define QUE_NODE_WHILE		(22 + QUE_NODE_CONTROL_STAT)
#define QUE_NODE_ASSIGNMENT	23
#define QUE_NODE_FETCH		24
#define QUE_NODE_OPEN		25
#define QUE_NODE_COL_ASSIGNMENT	26
#define QUE_NODE_FOR		(27 + QUE_NODE_CONTROL_STAT)
#define QUE_NODE_RETURN		28
#define QUE_NODE_ROW_PRINTF	29
#define QUE_NODE_ELSIF		30
#define QUE_NODE_CALL		31

/* Query thread states */
#define QUE_THR_RUNNING		1
#define QUE_THR_PROCEDURE_WAIT	2
#define	QUE_THR_COMPLETED	3	/* in selects this means that the
					thread is at the end of its result set
					(or start, in case of a scroll cursor);
					in other statements, this means the
					thread has done its task */
#define QUE_THR_COMMAND_WAIT	4
#define QUE_THR_LOCK_WAIT	5
#define QUE_THR_SIG_REPLY_WAIT	6
#define QUE_THR_SUSPENDED	7
#define QUE_THR_ERROR		8

/* From where the cursor position is counted */
#define QUE_CUR_NOT_DEFINED	1
#define QUE_CUR_START		2
#define	QUE_CUR_END		3


#ifndef UNIV_NONINL
#include "que0que.ic"
#endif

#endif
