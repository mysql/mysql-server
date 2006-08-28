#include "ut0wqueue.h"

/********************************************************************
Create a new work queue. */

ib_wqueue_t*
ib_wqueue_create(void)
/*===================*/
			/* out: work queue */
{
	ib_wqueue_t*	wq = mem_alloc(sizeof(ib_wqueue_t));

	mutex_create(&wq->mutex, SYNC_WORK_QUEUE);

	wq->items = ib_list_create();
	wq->event = os_event_create(NULL);

	return(wq);
}

/********************************************************************
Free a work queue. */

void
ib_wqueue_free(
/*===========*/
	ib_wqueue_t*	wq)	/* in: work queue */
{
	ut_a(!ib_list_get_first(wq->items));

	mutex_free(&wq->mutex);
	ib_list_free(wq->items);
	os_event_free(wq->event);

	mem_free(wq);
}

/********************************************************************
Add a work item to the queue. */

void
ib_wqueue_add(
/*==========*/
	ib_wqueue_t*	wq,	/* in: work queue */
	void*		item,	/* in: work item */
	mem_heap_t*	heap)	/* in: memory heap to use for allocating the
				list node */
{
	mutex_enter(&wq->mutex);

	ib_list_add_last(wq->items, item, heap);
	os_event_set(wq->event);

	mutex_exit(&wq->mutex);
}

/********************************************************************
Wait for a work item to appear in the queue. */

void*
ib_wqueue_wait(
				/* out: work item */
	ib_wqueue_t*	wq)	/* in: work queue */
{
	ib_list_node_t*	node;

	for (;;) {
		os_event_wait(wq->event);

		mutex_enter(&wq->mutex);

		node = ib_list_get_first(wq->items);

		if (node) {
			ib_list_remove(wq->items, node);

			if (!ib_list_get_first(wq->items)) {
				/* We must reset the event when the list
				gets emptied. */
				os_event_reset(wq->event);
			}

			break;
		}

		mutex_exit(&wq->mutex);
	}

	mutex_exit(&wq->mutex);

	return(node->data);
}
