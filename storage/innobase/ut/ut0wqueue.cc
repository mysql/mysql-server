/*****************************************************************************

Copyright (c) 2006, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "ut0wqueue.h"

#include <stddef.h>
#include <sys/types.h>

#include "mem0mem.h"
#include "ut0list.h"

/** @file ut/ut0wqueue.cc
 A work queue

 Created 4/26/2006 Osku Salerma
 ************************************************************************/

/* Work queue. */
struct ib_wqueue_t {
  ib_mutex_t mutex; /*!< mutex protecting everything */
  ib_list_t *items; /*!< work item list */
  os_event_t event; /*!< event we use to signal additions to list */
};

/** Create a new work queue.
 @return work queue */
ib_wqueue_t *ib_wqueue_create(void) {
  ib_wqueue_t *wq = static_cast<ib_wqueue_t *>(ut_malloc_nokey(sizeof(*wq)));

  /* Function ib_wqueue_create() has not been used anywhere,
  not necessary to instrument this mutex */

  mutex_create(LATCH_ID_WORK_QUEUE, &wq->mutex);

  wq->items = ib_list_create();
  wq->event = os_event_create(0);

  return (wq);
}

/** Free a work queue. */
void ib_wqueue_free(ib_wqueue_t *wq) /*!< in: work queue */
{
  mutex_free(&wq->mutex);
  ib_list_free(wq->items);
  os_event_destroy(wq->event);

  ut_free(wq);
}

/** Add a work item to the queue. */
void ib_wqueue_add(ib_wqueue_t *wq,  /*!< in: work queue */
                   void *item,       /*!< in: work item */
                   mem_heap_t *heap) /*!< in: memory heap to use for allocating
                                     the list node */
{
  mutex_enter(&wq->mutex);

  ib_list_add_last(wq->items, item, heap);
  os_event_set(wq->event);

  mutex_exit(&wq->mutex);
}

/********************************************************************
Wait for a work item to appear in the queue for specified time. */
void *ib_wqueue_timedwait(
    /* out: work item or NULL on timeout*/
    ib_wqueue_t *wq,         /* in: work queue */
    ib_time_t wait_in_usecs) /* in: wait time in micro seconds */
{
  ib_list_node_t *node = NULL;

  for (;;) {
    ulint error;
    int64_t sig_count;

    mutex_enter(&wq->mutex);

    node = ib_list_get_first(wq->items);

    if (node) {
      ib_list_remove(wq->items, node);

      mutex_exit(&wq->mutex);
      break;
    }

    sig_count = os_event_reset(wq->event);

    mutex_exit(&wq->mutex);

    error = os_event_wait_time_low(wq->event, (ulint)wait_in_usecs, sig_count);

    if (error == OS_SYNC_TIME_EXCEEDED) {
      break;
    }
  }

  return (node ? node->data : NULL);
}

/********************************************************************
Check if queue is empty. */
ibool ib_wqueue_is_empty(
    /* out: TRUE if queue empty
    else FALSE */
    const ib_wqueue_t *wq) /* in: work queue */
{
  return (ib_list_is_empty(wq->items));
}
