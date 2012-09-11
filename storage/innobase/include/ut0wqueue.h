/*****************************************************************************

Copyright (c) 2006, 2009, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/*******************************************************************//**
@file include/ut0wqueue.h
A work queue

Created 4/26/2006 Osku Salerma
************************************************************************/

/*******************************************************************//**
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

struct ib_wqueue_t;

/****************************************************************//**
Create a new work queue.
@return	work queue */
UNIV_INTERN
ib_wqueue_t*
ib_wqueue_create(void);
/*===================*/

/****************************************************************//**
Free a work queue. */
UNIV_INTERN
void
ib_wqueue_free(
/*===========*/
	ib_wqueue_t*	wq);	/*!< in: work queue */

/****************************************************************//**
Add a work item to the queue. */
UNIV_INTERN
void
ib_wqueue_add(
/*==========*/
	ib_wqueue_t*	wq,	/*!< in: work queue */
	void*		item,	/*!< in: work item */
	mem_heap_t*	heap);	/*!< in: memory heap to use for allocating the
				list node */

/********************************************************************
Check if queue is empty. */

ibool
ib_wqueue_is_empty(
/*===============*/
					/* out: TRUE if queue empty
					else FALSE */
	const ib_wqueue_t*      wq);    /* in: work queue */

/****************************************************************//**
Wait for a work item to appear in the queue.
@return	work item */
UNIV_INTERN
void*
ib_wqueue_wait(
/*===========*/
	ib_wqueue_t*	wq);	/*!< in: work queue */

/********************************************************************
Wait for a work item to appear in the queue for specified time. */

void*
ib_wqueue_timedwait(
/*================*/
					/* out: work item or NULL on timeout*/
	ib_wqueue_t*	wq,		/* in: work queue */
	ib_time_t	wait_in_usecs); /* in: wait time in micro seconds */

/* Work queue. */
struct ib_wqueue_t {
	ib_mutex_t		mutex;	/*!< mutex protecting everything */
	ib_list_t*	items;	/*!< work item list */
	os_event_t	event;	/*!< event we use to signal additions to list */
};

#endif
