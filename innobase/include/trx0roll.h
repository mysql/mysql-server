/******************************************************
Transaction rollback

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0roll_h
#define trx0roll_h

#include "univ.i"
#include "trx0trx.h"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"

/***********************************************************************
Returns a transaction savepoint taken at this point in time. */

trx_savept_t
trx_savept_take(
/*============*/
			/* out: savepoint */
	trx_t*	trx);	/* in: transaction */
/***********************************************************************
Creates an undo number array. */

trx_undo_arr_t*
trx_undo_arr_create(void);
/*=====================*/
/***********************************************************************
Frees an undo number array. */

void
trx_undo_arr_free(
/*==============*/
	trx_undo_arr_t*	arr);	/* in: undo number array */
/***********************************************************************
Returns pointer to nth element in an undo number array. */
UNIV_INLINE
trx_undo_inf_t*
trx_undo_arr_get_nth_info(
/*======================*/
				/* out: pointer to the nth element */
	trx_undo_arr_t*	arr,	/* in: undo number array */
	ulint		n);	/* in: position */
/***************************************************************************
Tries truncate the undo logs. */

void
trx_roll_try_truncate(
/*==================*/
	trx_t*	trx);	/* in: transaction */
/************************************************************************
Pops the topmost record when the two undo logs of a transaction are seen
as a single stack of records ordered by their undo numbers. Inserts the
undo number of the popped undo record to the array of currently processed
undo numbers in the transaction. When the query thread finishes processing
of this undo record, it must be released with trx_undo_rec_release. */

trx_undo_rec_t*
trx_roll_pop_top_rec_of_trx(
/*========================*/
				/* out: undo log record copied to heap, NULL
				if none left, or if the undo number of the
				top record would be less than the limit */
	trx_t*		trx,	/* in: transaction */
	dulint		limit,	/* in: least undo number we need */
	dulint*		roll_ptr,/* out: roll pointer to undo record */
	mem_heap_t*	heap);	/* in: memory heap where copied */
/************************************************************************
Reserves an undo log record for a query thread to undo. This should be
called if the query thread gets the undo log record not using the pop
function above. */

ibool
trx_undo_rec_reserve(
/*=================*/
			/* out: TRUE if succeeded */
	trx_t*	trx,	/* in: transaction */
	dulint	undo_no);/* in: undo number of the record */
/***********************************************************************
Releases a reserved undo record. */

void
trx_undo_rec_release(
/*=================*/
	trx_t*	trx,	/* in: transaction */
	dulint	undo_no);/* in: undo number */
/*************************************************************************
Starts a rollback operation. */	

void
trx_rollback(
/*=========*/
	trx_t*		trx,	/* in: transaction */
	trx_sig_t*	sig,	/* in: signal starting the rollback */
	que_thr_t**	next_thr);/* in/out: next query thread to run;
				if the value which is passed in is
				a pointer to a NULL pointer, then the
				calling function can start running
				a new query thread */
/***********************************************************************
Rollback uncommitted transactions which have no user session. */

void
trx_rollback_all_without_sess(void);
/*===============================*/
/********************************************************************
Finishes a transaction rollback. */

void
trx_finish_rollback_off_kernel(
/*===========================*/
	que_t*		graph,	/* in: undo graph which can now be freed */
	trx_t*		trx,	/* in: transaction */
	que_thr_t**	next_thr);/* in/out: next query thread to run;
				if the value which is passed in is
				a pointer to a NULL pointer, then the
   				calling function can start running
				a new query thread; if this parameter is
				NULL, it is ignored */
/********************************************************************
Builds an undo 'query' graph for a transaction. The actual rollback is
performed by executing this query graph like a query subprocedure call.
The reply about the completion of the rollback will be sent by this
graph. */

que_t*
trx_roll_graph_build(
/*=================*/
			/* out, own: the query graph */
	trx_t*	trx);	/* in: trx handle */
/*************************************************************************
Creates a rollback command node struct. */

roll_node_t*
roll_node_create(
/*=============*/
				/* out, own: rollback node struct */
	mem_heap_t*	heap);	/* in: mem heap where created */
/***************************************************************
Performs an execution step for a rollback command node in a query graph. */

que_thr_t*
trx_rollback_step(
/*==============*/
				/* out: query thread to run next, or NULL */
	que_thr_t*	thr);	/* in: query thread */
/***********************************************************************
Rollback a transaction used in MySQL. */

int
trx_rollback_for_mysql(
/*===================*/
			/* out: error code or DB_SUCCESS */
	trx_t*	trx);	/* in: transaction handle */
/***********************************************************************
Rollback the latest SQL statement for MySQL. */

int
trx_rollback_last_sql_stat_for_mysql(
/*=================================*/
			/* out: error code or DB_SUCCESS */
	trx_t*	trx);	/* in: transaction handle */
/***********************************************************************
Rollback a transaction used in MySQL. */

int
trx_general_rollback_for_mysql(
/*===========================*/
				/* out: error code or DB_SUCCESS */
	trx_t*		trx,	/* in: transaction handle */
	ibool		partial,/* in: TRUE if partial rollback requested */
	trx_savept_t*	savept);/* in: pointer to savepoint undo number, if
				partial rollback requested */

extern sess_t*		trx_dummy_sess;

/* A cell in the array used during a rollback and a purge */
struct	trx_undo_inf_struct{
	dulint	trx_no;		/* transaction number: not defined during
				a rollback */
	dulint	undo_no;	/* undo number of an undo record */
	ibool	in_use;		/* TRUE if the cell is in use */
};

/* During a rollback and a purge, undo numbers of undo records currently being
processed are stored in this array */

struct trx_undo_arr_struct{
	ulint		n_cells;	/* number of cells in the array */
	ulint		n_used;		/* number of cells currently in use */
	trx_undo_inf_t*	infos;		/* the array of undo infos */
	mem_heap_t*	heap;		/* memory heap from which allocated */
};

/* Rollback command node in a query graph */
struct roll_node_struct{
	que_common_t	common;	/* node type: QUE_NODE_ROLLBACK */
	ulint		state;	/* node execution state */
	ibool		partial;/* TRUE if we want a partial rollback */
	trx_savept_t	savept;	/* savepoint to which to roll back, in the
				case of a partial rollback */
};

/* Rollback node states */
#define ROLL_NODE_SEND	1
#define ROLL_NODE_WAIT	2

#ifndef UNIV_NONINL
#include "trx0roll.ic"
#endif

#endif 
