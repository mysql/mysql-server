/*****************************************************************************

Copyright (c) 1996, 2009, Innobase Oy. All Rights Reserved.

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

/******************************************************
Server query execution

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
UNIV_INTERN
void
srv_que_task_queue_check(void);
/*==========================*/
/**************************************************************************
Performs round-robin on the server tasks. This is called by a SRV_WORKER
thread every second or so. */
UNIV_INTERN
que_thr_t*
srv_que_round_robin(
/*================*/
				/* out: the new (may be == thr) query thread
				to run */
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Enqueues a task to server task queue and releases a worker thread, if
there exists one suspended. */
UNIV_INTERN
void
srv_que_task_enqueue(
/*=================*/
	que_thr_t*	thr);	/* in: query thread */
/**************************************************************************
Enqueues a task to server task queue and releases a worker thread, if
there exists one suspended. */
UNIV_INTERN
void
srv_que_task_enqueue_low(
/*=====================*/
	que_thr_t*	thr);	/* in: query thread */

#endif

