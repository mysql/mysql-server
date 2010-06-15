/*****************************************************************************

Copyright (c) 2006, 2009, Innobase Oy. All Rights Reserved.

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

#include "ut0wqueue.h"

/*******************************************************************//**
@file ut/ut0wqueue.c
A work queue

Created 4/26/2006 Osku Salerma
************************************************************************/

/****************************************************************//**
Create a new work queue.
@return	work queue */
UNIV_INTERN
ib_wqueue_t*
ib_wqueue_create(void)
/*===================*/
{
	ib_wqueue_t*	wq = mem_alloc(sizeof(ib_wqueue_t));

	/* Function ib_wqueue_create() has not been used anywhere,
	not necessary to instrument this mutex */
	mutex_create(PFS_NOT_INSTRUMENTED, &wq->mutex, SYNC_WORK_QUEUE);

	wq->items = ib_list_create();
	wq->event = os_event_create(NULL);

	return(wq);
}

/****************************************************************//**
Free a work queue. */
UNIV_INTERN
void
ib_wqueue_free(
/*===========*/
	ib_wqueue_t*	wq)	/*!< in: work queue */
{
	ut_a(!ib_list_get_first(wq->items));

	mutex_free(&wq->mutex);
	ib_list_free(wq->items);
	os_event_free(wq->event);

	mem_free(wq);
}

/****************************************************************//**
Add a work item to the queue. */
UNIV_INTERN
void
ib_wqueue_add(
/*==========*/
	ib_wqueue_t*	wq,	/*!< in: work queue */
	void*		item,	/*!< in: work item */
	mem_heap_t*	heap)	/*!< in: memory heap to use for allocating the
				list node */
{
	mutex_enter(&wq->mutex);

	ib_list_add_last(wq->items, item, heap);
	os_event_set(wq->event);

	mutex_exit(&wq->mutex);
}

/****************************************************************//**
Wait for a work item to appear in the queue.
@return	work item */
UNIV_INTERN
void*
ib_wqueue_wait(
/*===========*/
	ib_wqueue_t*	wq)	/*!< in: work queue */
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
