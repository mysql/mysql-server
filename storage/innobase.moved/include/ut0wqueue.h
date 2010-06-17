/***********************************************************************
A Work queue. Threads can add work items to the queue and other threads can
wait for work items to be available and take them off the queue for
processing.

************************************************************************/

#ifndef IB_WORK_QUEUE_H
#define IB_WORK_QUEUE_H

#include "ut0list.h"
#include "mem0mem.h"
#include "os0sync.h"
#include "sync0types.h"

typedef struct ib_wqueue_struct ib_wqueue_t;

/********************************************************************
Create a new work queue. */

ib_wqueue_t*
ib_wqueue_create(void);
/*===================*/
			/* out: work queue */

/********************************************************************
Free a work queue. */

void
ib_wqueue_free(
/*===========*/
	ib_wqueue_t*	wq);	/* in: work queue */

/********************************************************************
Add a work item to the queue. */

void
ib_wqueue_add(
/*==========*/
	ib_wqueue_t*	wq,	/* in: work queue */
	void*		item,	/* in: work item */
	mem_heap_t*	heap);	/* in: memory heap to use for allocating the
				list node */

/********************************************************************
Wait for a work item to appear in the queue. */

void*
ib_wqueue_wait(
				/* out: work item */
	ib_wqueue_t*	wq);	/* in: work queue */

/* Work queue. */
struct ib_wqueue_struct {
	mutex_t		mutex;	/* mutex protecting everything */
	ib_list_t*	items;	/* work item list */
	os_event_t	event;	/* event we use to signal additions to list */
};

#endif
