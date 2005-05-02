/******************************************************
Executes SQL stored procedures and their control structures

(c) 1998 Innobase Oy

Created 1/20/1998 Heikki Tuuri
*******************************************************/

#ifndef eval0proc_h
#define eval0proc_h

#include "univ.i"
#include "que0types.h"
#include "pars0sym.h"
#include "pars0pars.h"

/**************************************************************************
Performs an execution step of a procedure node. */
UNIV_INLINE
que_thr_t*
proc_step(
/*======*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of an if-statement node. */

que_thr_t*
if_step(
/*====*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of a while-statement node. */

que_thr_t*
while_step(
/*=======*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of a for-loop node. */

que_thr_t*
for_step(
/*=====*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of an assignment statement node. */

que_thr_t*
assign_step(
/*========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of a procedure call node. */
UNIV_INLINE
que_thr_t*
proc_eval_step(
/*===========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Performs an execution step of a return-statement node. */

que_thr_t*
return_step(
/*========*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr);	/* in: query thread */


#ifndef UNIV_NONINL
#include "eval0proc.ic"
#endif

#endif 
