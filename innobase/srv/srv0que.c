/******************************************************
Server query execution

(c) 1996 Innobase Oy

Created 6/5/1996 Heikki Tuuri
*******************************************************/

#include "srv0que.h"

#include "srv0srv.h"
#include "sync0sync.h"
#include "os0thread.h"
#include "usr0sess.h"
#include "que0que.h"

/**************************************************************************
Checks if there is work to do in the server task queue. If there is, the
thread starts processing a task. Before leaving, it again checks the task
queue and picks a new task if any exists. This is called by a SRV_WORKER
thread. */

void
srv_que_task_queue_check(void)
/*==========================*/
{
	que_thr_t*	thr;

	for (;;) {
		mutex_enter(&kernel_mutex);

		thr = UT_LIST_GET_FIRST(srv_sys->tasks);

		if (thr == NULL) {
			mutex_exit(&kernel_mutex);

			return;
		}

		UT_LIST_REMOVE(queue, srv_sys->tasks, thr);
		
		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}
}

/**************************************************************************
Performs round-robin on the server tasks. This is called by a SRV_WORKER
thread every second or so. */

que_thr_t*
srv_que_round_robin(
/*================*/
				/* out: the new (may be == thr) query thread
				to run */
	que_thr_t*	thr)	/* in: query thread */
{
	que_thr_t*	new_thr;

	ut_ad(thr);
	ut_ad(thr->state == QUE_THR_RUNNING);

	mutex_enter(&kernel_mutex);
	
	UT_LIST_ADD_LAST(queue, srv_sys->tasks, thr);

	new_thr = UT_LIST_GET_FIRST(srv_sys->tasks);
		
	mutex_exit(&kernel_mutex);

	return(new_thr);
}

/**************************************************************************
Enqueues a task to server task queue and releases a worker thread, if there
is a suspended one. */

void
srv_que_task_enqueue_low(
/*=====================*/
	que_thr_t*	thr)	/* in: query thread */
{
	ut_ad(thr);

	ut_ad(mutex_own(&kernel_mutex));

	UT_LIST_ADD_LAST(queue, srv_sys->tasks, thr);

	srv_release_threads(SRV_WORKER, 1);
}

/**************************************************************************
Enqueues a task to server task queue and releases a worker thread, if there
is a suspended one. */

void
srv_que_task_enqueue(
/*=================*/
	que_thr_t*	thr)	/* in: query thread */
{
	ut_ad(thr);

	mutex_enter(&kernel_mutex);
	
	srv_que_task_enqueue_low(thr);
	
	mutex_exit(&kernel_mutex);
}
