/*****************************************************************************

Copyright (c) 2011, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

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

/**************************************************//**
@file srv/srv0conc.c

InnoDB concurrency manager

Created 2011/04/18 Sunny Bains
*******************************************************/

#include "srv0srv.h"
#include "sync0sync.h"
#include "trx0trx.h"

#include "mysql/plugin.h"
#include "mysql/service_thd_wait.h"	/* MySQL callback functions */

/** We are prepared for a situation that we have this many threads waiting for
a semaphore inside InnoDB. innobase_start_or_create_for_mysql() sets the
value. */

UNIV_INTERN ulint	srv_max_n_threads	= 0;

/** The following controls how many threads we let inside InnoDB concurrently:
threads waiting for locks are not counted into the number because otherwise
we could get a deadlock. MySQL creates a thread for each user session, and
semaphore contention and convoy problems can occur withput this restriction.
Value 10 should be good if there are less than 4 processors + 4 disks in the
computer. Bigger computers need bigger values. Value 0 will disable the
concurrency check. */

UNIV_INTERN ulong	srv_thread_concurrency	= 0;

/** Number of transactions that have declared_to_be_inside_innodb set.
It used to be a non-error for this value to drop below zero temporarily.
This is no longer true. We'll, however, keep the lint datatype to add
assertions to catch any corner cases that we may have missed. */

UNIV_INTERN lint	srv_conc_n_threads	= 0;

/** This mutex protects srv_conc data structures */
static os_fast_mutex_t	srv_conc_mutex;

/** Number of OS threads waiting in the FIFO for a permission to enter
InnoDB */
static ulint		srv_conc_n_waiting_threads = 0;

/** Slot for a thread waiting in the concurrency control queue. */
typedef struct srv_conc_slot_struct	srv_conc_slot_t;

/** Concurrency list node */
typedef UT_LIST_NODE_T(srv_conc_slot_t)	srv_conc_node_t;

struct srv_conc_slot_struct{
	os_event_t	event;		/*!< event to wait */
	ibool		reserved;	/*!< TRUE if slot
					reserved */
	ibool		wait_ended;	/*!< TRUE when another thread has
					already set the event and the thread
					in this slot is free to proceed; but
					reserved may still be TRUE at that
					point */
	srv_conc_node_t	srv_conc_queue;	/*!< queue node */
};

/** Queue of threads waiting to get in */
typedef UT_LIST_BASE_NODE_T(srv_conc_slot_t)	srv_conc_queue_t;

static srv_conc_queue_t	srv_conc_queue;

/** Array of wait slots */
static srv_conc_slot_t*	srv_conc_slots;

/* Number of times a thread is allowed to enter InnoDB within the same
SQL query after it has once got the ticket at srv_conc_enter_innodb */
#define SRV_FREE_TICKETS_TO_ENTER srv_n_free_tickets_to_enter
#define SRV_THREAD_SLEEP_DELAY srv_thread_sleep_delay

#ifdef UNIV_PFS_MUTEX
/* Key to register srv_conc_mutex_key with performance schema */
UNIV_INTERN mysql_pfs_key_t	srv_conc_mutex_key;
#endif /* UNIV_PFS_MUTEX */


/*********************************************************************//**
Initialise the concurrency management data structures */
void
srv_conc_init(void)
/*===============*/
{
	ulint		i;

	/* Init the server concurrency restriction data structures */

	os_fast_mutex_init(srv_conc_mutex_key, &srv_conc_mutex);

	UT_LIST_INIT(srv_conc_queue);

	srv_conc_slots = mem_zalloc(OS_THREAD_MAX_N * sizeof(*srv_conc_slots));

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		srv_conc_slot_t*	conc_slot = &srv_conc_slots[i];

		conc_slot->event = os_event_create(NULL);
		ut_a(conc_slot->event);
	}
}

/*********************************************************************//**
Free the concurrency management data structures */
void
srv_conc_free(void)
/*===============*/
{
	os_fast_mutex_free(&srv_conc_mutex);
	mem_free(srv_conc_slots);
	srv_conc_slots = NULL;
}

/*********************************************************************//**
Puts an OS thread to wait if there are too many concurrent threads
(>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue. */
UNIV_INTERN
void
srv_conc_enter_innodb(
/*==================*/
	trx_t*	trx)	/*!< in: transaction object associated with the
			thread */
{
	ibool			has_slept = FALSE;
	srv_conc_slot_t*	slot = NULL;
	ulint			i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */

	if (trx->mysql_thd != NULL
	    && thd_is_replication_slave_thread(trx->mysql_thd)) {

		UT_WAIT_FOR(srv_conc_n_threads
			    < (lint)srv_thread_concurrency,
			    srv_replication_delay * 1000);

		return;
	}

	/* If trx has 'free tickets' to enter the engine left, then use one
	such ticket */

	if (trx->n_tickets_to_enter_innodb > 0) {
		trx->n_tickets_to_enter_innodb--;

		return;
	}

	os_fast_mutex_lock(&srv_conc_mutex);
retry:
	if (UNIV_UNLIKELY(trx->declared_to_be_inside_innodb)) {
		os_fast_mutex_unlock(&srv_conc_mutex);
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: trying to declare trx"
		      " to enter InnoDB, but\n"
		      "InnoDB: it already is declared.\n", stderr);
		trx_print(stderr, trx, 0);
		putc('\n', stderr);

		return;
	}

	ut_ad(srv_conc_n_threads >= 0);

	if (srv_conc_n_threads < (lint)srv_thread_concurrency) {

		srv_conc_n_threads++;
		trx->declared_to_be_inside_innodb = TRUE;
		trx->n_tickets_to_enter_innodb = SRV_FREE_TICKETS_TO_ENTER;

		os_fast_mutex_unlock(&srv_conc_mutex);

		return;
	}

	/* If the transaction is not holding resources, let it sleep
	for SRV_THREAD_SLEEP_DELAY microseconds, and try again then */

	if (!has_slept && !trx->has_search_latch
	    && NULL == UT_LIST_GET_FIRST(trx->lock.trx_locks)) {

		has_slept = TRUE; /* We let it sleep only once to avoid
				starvation */

		srv_conc_n_waiting_threads++;

		os_fast_mutex_unlock(&srv_conc_mutex);

		trx->op_info = "sleeping before joining InnoDB queue";

		/* Peter Zaitsev suggested that we take the sleep away
		altogether. But the sleep may be good in pathological
		situations of lots of thread switches. Simply put some
		threads aside for a while to reduce the number of thread
		switches. */
		if (SRV_THREAD_SLEEP_DELAY > 0) {
			os_thread_sleep(SRV_THREAD_SLEEP_DELAY);
		}

		trx->op_info = "";

		os_fast_mutex_lock(&srv_conc_mutex);

		srv_conc_n_waiting_threads--;

		goto retry;
	}

	/* Too many threads inside: put the current thread to a queue */

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		slot = srv_conc_slots + i;

		if (!slot->reserved) {

			break;
		}
	}

	if (i == OS_THREAD_MAX_N) {
		/* Could not find a free wait slot, we must let the
		thread enter */

		srv_conc_n_threads++;
		trx->declared_to_be_inside_innodb = TRUE;
		trx->n_tickets_to_enter_innodb = 0;

		os_fast_mutex_unlock(&srv_conc_mutex);

		return;
	}

	/* Release possible search system latch this thread has */
	if (trx->has_search_latch) {
		trx_search_latch_release_if_reserved(trx);
	}

	/* Add to the queue */
	slot->reserved = TRUE;
	slot->wait_ended = FALSE;

	UT_LIST_ADD_LAST(srv_conc_queue, srv_conc_queue, slot);

	os_event_reset(slot->event);

	srv_conc_n_waiting_threads++;

	os_fast_mutex_unlock(&srv_conc_mutex);

	/* Go to wait for the event; when a thread leaves InnoDB it will
	release this thread */

	ut_ad(!trx->has_search_latch);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */
	trx->op_info = "waiting in InnoDB queue";

	thd_wait_begin(trx->mysql_thd, THD_WAIT_ROW_LOCK);
	os_event_wait(slot->event);
	thd_wait_end(trx->mysql_thd);

	trx->op_info = "";

	os_fast_mutex_lock(&srv_conc_mutex);

	srv_conc_n_waiting_threads--;

	/* NOTE that the thread which released this thread already
	incremented the thread counter on behalf of this thread */

	slot->reserved = FALSE;

	UT_LIST_REMOVE(srv_conc_queue, srv_conc_queue, slot);

	trx->declared_to_be_inside_innodb = TRUE;
	trx->n_tickets_to_enter_innodb = SRV_FREE_TICKETS_TO_ENTER;

	os_fast_mutex_unlock(&srv_conc_mutex);
}

/*********************************************************************//**
This lets a thread enter InnoDB regardless of the number of threads inside
InnoDB. This must be called when a thread ends a lock wait. */
UNIV_INTERN
void
srv_conc_force_enter_innodb(
/*========================*/
	trx_t*	trx)	/*!< in: transaction object associated with the
			thread */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */

	if (UNIV_LIKELY(!srv_thread_concurrency)) {

		return;
	}

	ut_ad(srv_conc_n_threads >= 0);

	os_fast_mutex_lock(&srv_conc_mutex);

	srv_conc_n_threads++;
	trx->declared_to_be_inside_innodb = TRUE;
	trx->n_tickets_to_enter_innodb = 1;

	os_fast_mutex_unlock(&srv_conc_mutex);
}

/*********************************************************************//**
This must be called when a thread exits InnoDB in a lock wait or at the
end of an SQL statement. */
UNIV_INTERN
void
srv_conc_force_exit_innodb(
/*=======================*/
	trx_t*	trx)	/*!< in: transaction object associated with the
			thread */
{
	srv_conc_slot_t*	slot	= NULL;

	if (trx->mysql_thd != NULL
	    && thd_is_replication_slave_thread(trx->mysql_thd)) {

		return;
	}

	if (trx->declared_to_be_inside_innodb == FALSE) {

		return;
	}

	os_fast_mutex_lock(&srv_conc_mutex);

	ut_ad(srv_conc_n_threads > 0);
	srv_conc_n_threads--;
	trx->declared_to_be_inside_innodb = FALSE;
	trx->n_tickets_to_enter_innodb = 0;

	if (srv_conc_n_threads < (lint)srv_thread_concurrency) {
		/* Look for a slot where a thread is waiting and no other
		thread has yet released the thread */

		slot = UT_LIST_GET_FIRST(srv_conc_queue);

		while (slot && slot->wait_ended == TRUE) {
			slot = UT_LIST_GET_NEXT(srv_conc_queue, slot);
		}

		if (slot != NULL) {
			slot->wait_ended = TRUE;

			/* We increment the count on behalf of the released
			thread */

			srv_conc_n_threads++;
		}
	}

	os_fast_mutex_unlock(&srv_conc_mutex);

	if (slot != NULL) {
		os_event_set(slot->event);
	}

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */
}

/*********************************************************************//**
This must be called when a thread exits InnoDB. */
UNIV_INTERN
void
srv_conc_exit_innodb(
/*=================*/
	trx_t*	trx)	/*!< in: transaction object associated with the
			thread */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */

	if (trx->n_tickets_to_enter_innodb > 0) {
		/* We will pretend the thread is still inside InnoDB though it
		now leaves the InnoDB engine. In this way we save
		a lot of semaphore operations. srv_conc_force_exit_innodb is
		used to declare the thread definitely outside InnoDB. It
		should be called when there is a lock wait or an SQL statement
		ends. */

		return;
	}

	srv_conc_force_exit_innodb(trx);
}

/*********************************************************************//**
Get the count of threads waiting inside InnoDB. */
UNIV_INTERN
ulint
srv_conc_get_waiting_threads(void)
/*==============================*/
{
	return(srv_conc_n_waiting_threads);
}
