/******************************************************
Server query execution

(c) 1996 Innobase Oy

Created 6/5/1996 Heikki Tuuri
*******************************************************/


#ifndef srv0que_h
#define srv0que_h

#include "univ.i"
#include "que0types.h"

/**************************************************************************
Checks if there is work to do in the server task queue. If there is, the
thread starts processing a task. Before leaving, it again checks the task
queue and picks a new task if any exists. This is called by a SRV_WORKER
thread. */

void
srv_que_task_queue_check(void);
/*==========================*/
/**************************************************************************
Performs round-robin on the server tasks. This is called by a SRV_WORKER
thread every second or so. */

que_thr_t*
srv_que_round_robin(
/*================*/
				/* out: the new (may be == thr) query thread
				to run */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Enqueues a task to server task queue and releases a worker thread, if
there exists one suspended. */

void
srv_que_task_enqueue(
/*=================*/
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Enqueues a task to server task queue and releases a worker thread, if
there exists one suspended. */

void
srv_que_task_enqueue_low(
/*=====================*/
	que_thr_t*	thr);	/* in: query thread */

#endif

