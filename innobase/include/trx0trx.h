/******************************************************
The transaction

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0trx_h
#define trx0trx_h

#include "univ.i"
#include "trx0types.h"
#include "lock0types.h"
#include "usr0types.h"
#include "que0types.h"
#include "mem0mem.h"
#include "read0types.h"

/* If this flag is defined, then unneeded update undo logs are discarded,
saving CPU time. The kernel mutex contention is increased, however. */

#define TRX_UPDATE_UNDO_OPT

extern ulint	trx_n_mysql_transactions;

/************************************************************************
Releases the search latch if trx has reserved it. */

void
trx_search_latch_release_if_reserved(
/*=================================*/
        trx_t*     trx); /* in: transaction */
/********************************************************************
Retrieves the error_info field from a trx. */

void*
trx_get_error_info(
/*===============*/
		     /* out: the error info */
	trx_t*  trx); /* in: trx object */
/********************************************************************
Creates and initializes a transaction object. */

trx_t*
trx_create(
/*=======*/
			/* out, own: the transaction */
	sess_t*	sess);	/* in: session or NULL */
/************************************************************************
Creates a transaction object for MySQL. */

trx_t*
trx_allocate_for_mysql(void);
/*========================*/
				/* out, own: transaction object */
/************************************************************************
Frees a transaction object. */

void
trx_free(
/*=====*/
	trx_t*	trx);	/* in, own: trx object */
/************************************************************************
Frees a transaction object for MySQL. */

void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx);	/* in, own: trx object */
/********************************************************************
Creates trx objects for transactions and initializes the trx list of
trx_sys at database start. Rollback segment and undo log lists must
already exist when this function is called, because the lists of
transactions to be rolled back or cleaned up are built based on the
undo log lists. */

void
trx_lists_init_at_db_start(void);
/*============================*/
/********************************************************************
Starts a new transaction. */

ibool
trx_start(
/*======*/
			/* out: TRUE if success, FALSE if the rollback
			segment could not support this many transactions */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id);/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
/********************************************************************
Starts a new transaction. */

ibool
trx_start_low(
/*==========*/
			/* out: TRUE */
	trx_t* 	trx,	/* in: transaction */
	ulint	rseg_id);/* in: rollback segment id; if ULINT_UNDEFINED
			is passed, the system chooses the rollback segment
			automatically in a round-robin fashion */
/*****************************************************************
Starts the transaction if it is not yet started. */
UNIV_INLINE
void
trx_start_if_not_started(
/*=====================*/
	trx_t*	trx);	/* in: transaction */
/********************************************************************
Commits a transaction. */

void
trx_commit_off_kernel(
/*==================*/
	trx_t*	trx);	/* in: transaction */
/**************************************************************************
Does the transaction commit for MySQL. */

ulint
trx_commit_for_mysql(
/*=================*/
			/* out: 0 or error number */
	trx_t*	trx);	/* in: trx handle */
/**************************************************************************
Marks the latest SQL statement ended. */

void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx);	/* in: trx handle */
/**************************************************************************
Marks the latest SQL statement ended but does not start a new transaction
if the trx is not started. */

void
trx_mark_sql_stat_end_do_not_start_new(
/*===================================*/
	trx_t*	trx);	/* in: trx handle */
/************************************************************************
Assigns a read view for a consistent read query. All the consistent reads
within the same transaction will get the same read view, which is created
when this function is first called for a new started transaction. */

read_view_t*
trx_assign_read_view(
/*=================*/
			/* out: consistent read view */
	trx_t*	trx);	/* in: active transaction */
/***************************************************************
The transaction must be in the TRX_QUE_LOCK_WAIT state. Puts it to
the TRX_QUE_RUNNING state and releases query threads which were
waiting for a lock in the wait_thrs list. */

void
trx_end_lock_wait(
/*==============*/
	trx_t*	trx);	/* in: transaction */
/********************************************************************
Sends a signal to a trx object. */

ibool
trx_sig_send(
/*=========*/
					/* out: TRUE if the signal was
					successfully delivered */
	trx_t*		trx,		/* in: trx handle */
	ulint		type,		/* in: signal type */
	ulint		sender,		/* in: TRX_SIG_SELF or
					TRX_SIG_OTHER_SESS */
	ibool		reply,		/* in: TRUE if the sender of the signal
					wants reply after the operation induced
					by the signal is completed; if type
					is TRX_SIG_END_WAIT, this must be
					FALSE */
	que_thr_t*	receiver_thr,	/* in: query thread which wants the
					reply, or NULL */
	trx_savept_t* 	savept,		/* in: possible rollback savepoint, or
					NULL */
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread; if the parameter
					is NULL, it is ignored */
/********************************************************************
Send the reply message when a signal in the queue of the trx has
been handled. */

void
trx_sig_reply(
/*==========*/
	trx_t*		trx,		/* in: trx handle */
	trx_sig_t*	sig,		/* in: signal */
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
/********************************************************************
Removes the signal object from a trx signal queue. */

void
trx_sig_remove(
/*===========*/
	trx_t*		trx,	/* in: trx handle */
	trx_sig_t*	sig);	/* in, own: signal */
/********************************************************************
Starts handling of a trx signal. */

void
trx_sig_start_handle(
/*=================*/
	trx_t*		trx,		/* in: trx handle */
	que_thr_t**	next_thr);	/* in/out: next query thread to run;
					if the value which is passed in is
					a pointer to a NULL pointer, then the
					calling function can start running
					a new query thread */
/********************************************************************
Ends signal handling. If the session is in the error state, and
trx->graph_before_signal_handling != NULL, returns control to the error
handling routine of the graph (currently only returns the control to the
graph root which then sends an error message to the client). */

void
trx_end_signal_handling(
/*====================*/
	trx_t*	trx);	/* in: trx */
/*************************************************************************
Creates a commit command node struct. */

commit_node_t*
commit_node_create(
/*===============*/
				/* out, own: commit node struct */
	mem_heap_t*	heap);	/* in: mem heap where created */
/***************************************************************
Performs an execution step for a commit type node in a query graph. */

que_thr_t*
trx_commit_step(
/*============*/
				/* out: query thread to run next, or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Prints info about a transaction to the standard output. The caller must
own the kernel mutex. */

void
trx_print(
/*======*/
	  trx_t* trx); /* in: transaction */


/* Signal to a transaction */
struct trx_sig_struct{
	ulint		type;		/* signal type */
	ulint		state;		/* TRX_SIG_WAITING or
					TRX_SIG_BEING_HANDLED */
	ulint		sender;		/* TRX_SIG_SELF or
					TRX_SIG_OTHER_SESS */
	ibool		reply;		/* TRUE if the sender of the signal
					wants reply after the operation induced
					by the signal is completed; if this
					field is TRUE and the receiver field
					below is NULL, then a SUCCESS message
					is sent to the client of the session
					to which this trx belongs */
	que_thr_t*	receiver;	/* query thread which wants the reply,
					or NULL */
	trx_savept_t	savept;		/* possible rollback savepoint */
	UT_LIST_NODE_T(trx_sig_t)
			signals;	/* queue of pending signals to the
					transaction */
	UT_LIST_NODE_T(trx_sig_t)
			reply_signals;	/* list of signals for which the sender
					transaction is waiting a reply */
};

/* The transaction handle; every session has a trx object which is freed only
when the session is freed; in addition there may be session-less transactions
rolling back after a database recovery */

struct trx_struct{
	/* All the next fields are protected by the kernel mutex, except the
	undo logs which are protected by undo_mutex */
	char*		op_info;	/* English text describing the
					current operation, or an empty
					string */
	ulint		type;		/* TRX_USER, TRX_PURGE */
	ulint		conc_state;	/* state of the trx from the point
					of view of concurrency control:
					TRX_ACTIVE, TRX_COMMITTED_IN_MEMORY,
					... */
	dulint		id;		/* transaction id */
	dulint		no;		/* transaction serialization number ==
					max trx id when the transaction is 
					moved to COMMITTED_IN_MEMORY state */
	ibool		dict_operation;	/* TRUE if the trx is used to create
					a table, create an index, or drop a
					table */
	dulint		table_id;	/* table id if the preceding field is
					TRUE */
        void*           mysql_thd;      /* MySQL thread handle corresponding
                                        to this trx, or NULL */
	os_thread_id_t	mysql_thread_id;/* id of the MySQL thread associated
					with this transaction object */
	ulint		n_mysql_tables_in_use; /* number of Innobase tables
					used in the processing of the current
					SQL statement in MySQL */
        ulint           mysql_n_tables_locked;
                                        /* how many tables the current SQL
					statement uses, except those
					in consistent read */
        ibool           has_search_latch;
			                /* TRUE if this trx has latched the
			                search system latch in S-mode */
        ibool           ignore_duplicates_in_insert;
                                        /* in an insert roll back only insert
                                        of the latest row in case
                                        of a duplicate key error */
	UT_LIST_NODE_T(trx_t)
			trx_list;	/* list of transactions */
	UT_LIST_NODE_T(trx_t)
			mysql_trx_list;	/* list of transactions created for
					MySQL */
	/*------------------------------*/
	mutex_t		undo_mutex;	/* mutex protecting the fields in this
					section (down to undo_no_arr), EXCEPT
					last_sql_stat_start, which can be
					accessed only when we know that there
					cannot be any activity in the undo
					logs! */
	dulint		undo_no;	/* next undo log record number to
					assign */
	trx_savept_t	last_sql_stat_start;
					/* undo_no when the last sql statement
					was started: in case of an error, trx
					is rolled back down to this undo
					number; see note at undo_mutex! */
	trx_rseg_t*	rseg;		/* rollback segment assigned to the
					transaction, or NULL if not assigned
					yet */
	trx_undo_t*	insert_undo;	/* pointer to the insert undo log, or 
					NULL if no inserts performed yet */
	trx_undo_t* 	update_undo;	/* pointer to the update undo log, or
					NULL if no update performed yet */
	dulint		roll_limit;	/* least undo number to undo during
					a rollback */
	ulint		pages_undone;	/* number of undo log pages undone
					since the last undo log truncation */
	trx_undo_arr_t*	undo_no_arr;	/* array of undo numbers of undo log
					records which are currently processed
					by a rollback operation */
	/*------------------------------*/
	ulint		error_state;	/* 0 if no error, otherwise error
					number */
	void*		error_info;	/* if the error number indicates a
					duplicate key error, a pointer to
					the problematic index is stored here */
	sess_t*		sess;		/* session of the trx, NULL if none */
 	ulint		que_state;	/* TRX_QUE_RUNNING, TRX_QUE_LOCK_WAIT,
					... */
	que_t*		graph;		/* query currently run in the session,
					or NULL if none; NOTE that the query
					belongs to the session, and it can
					survive over a transaction commit, if
					it is a stored procedure with a COMMIT
					WORK statement, for instance */
	ulint		n_active_thrs;	/* number of active query threads */
	ibool		handling_signals;/* this is TRUE as long as the trx
					is handling signals */
	que_t*		graph_before_signal_handling;
					/* value of graph when signal handling
					for this trx started: this is used to
					return control to the original query
					graph for error processing */
	trx_sig_t	sig;		/* one signal object can be allocated
					in this space, avoiding mem_alloc */
	UT_LIST_BASE_NODE_T(trx_sig_t)
			signals;	/* queue of processed or pending
					signals to the trx */
	UT_LIST_BASE_NODE_T(trx_sig_t)
			reply_signals;	/* list of signals sent by the query
					threads of this trx for which a thread
					is waiting for a reply; if this trx is
					killed, the reply requests in the list
					must be canceled */
	/*------------------------------*/
	lock_t*		wait_lock;	/* if trx execution state is
					TRX_QUE_LOCK_WAIT, this points to
					the lock request, otherwise this is
					NULL */
	UT_LIST_BASE_NODE_T(que_thr_t)
			wait_thrs;	/* query threads belonging to this
					trx that are in the QUE_THR_LOCK_WAIT
					state */
	ulint		deadlock_mark;	/* a mark field used in deadlock
					checking algorithm */
	/*------------------------------*/
	mem_heap_t*	lock_heap;	/* memory heap for the locks of the
					transaction; protected by
					lock_heap_mutex */
	UT_LIST_BASE_NODE_T(lock_t) 
			trx_locks;	/* locks reserved by the transaction;
					protected by lock_heap_mutex */
	/*------------------------------*/
	mem_heap_t*	read_view_heap;	/* memory heap for the read view */
	read_view_t*	read_view;	/* consistent read view or NULL */
};

#define TRX_MAX_N_THREADS	32	/* maximum number of concurrent
					threads running a single operation of
					a transaction, e.g., a parallel query */
/* Transaction types */
#define	TRX_USER		1	/* normal user transaction */
#define	TRX_PURGE		2	/* purge transaction: this is not
					inserted to the trx list of trx_sys
					and no rollback segment is assigned to
					this */
/* Transaction concurrency states */
#define	TRX_NOT_STARTED		1
#define	TRX_ACTIVE		2
#define	TRX_COMMITTED_IN_MEMORY	3

/* Transaction execution states when trx state is TRX_ACTIVE */
#define TRX_QUE_RUNNING		1	/* transaction is running */
#define TRX_QUE_LOCK_WAIT	2	/* transaction is waiting for a lock */
#define TRX_QUE_ROLLING_BACK	3	/* transaction is rolling back */
#define TRX_QUE_COMMITTING	4	/* transaction is committing */

/* Types of a trx signal */
#define TRX_SIG_NO_SIGNAL		100
#define TRX_SIG_TOTAL_ROLLBACK		1
#define TRX_SIG_ROLLBACK_TO_SAVEPT	2
#define TRX_SIG_COMMIT			3
#define	TRX_SIG_ERROR_OCCURRED		4
#define TRX_SIG_BREAK_EXECUTION		5

/* Sender types of a signal */
#define TRX_SIG_SELF		1	/* sent by the session itself, or
					by an error occurring within this
					session */
#define TRX_SIG_OTHER_SESS	2	/* sent by another session (which
					must hold rights to this) */
/* Signal states */
#define	TRX_SIG_WAITING		1
#define TRX_SIG_BEING_HANDLED	2
					
/* Commit command node in a query graph */
struct commit_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_COMMIT */
	ulint		state;	/* node execution state */
};

/* Commit node states */
#define COMMIT_NODE_SEND	1
#define COMMIT_NODE_WAIT	2


#ifndef UNIV_NONINL
#include "trx0trx.ic"
#endif

#endif 
