/*****************************************************************************

Copyright (c) 2011, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file srv/srv0conc.cc

InnoDB concurrency manager

Created 2011/04/18 Sunny Bains
*******************************************************/

#include "srv0srv.h"
#include "sync0sync.h"
#include "trx0trx.h"

#include "mysql/plugin.h"

/** Number of times a thread is allowed to enter InnoDB within the same
SQL query after it has once got the ticket. */
UNIV_INTERN ulong	srv_n_free_tickets_to_enter = 500;

#ifdef HAVE_ATOMIC_BUILTINS
/** Maximum sleep delay (in micro-seconds), value of 0 disables it. */
UNIV_INTERN ulong	srv_adaptive_max_sleep_delay = 150000;
#endif /* HAVE_ATOMIC_BUILTINS */

UNIV_INTERN ulong	srv_thread_sleep_delay	= 10000;


/** We are prepared for a situation that we have this many threads waiting for
a semaphore inside InnoDB. innobase_start_or_create_for_mysql() sets the
value. */

UNIV_INTERN ulint	srv_max_n_threads	= 0;

/** The following controls how many threads we let inside InnoDB concurrently:
threads waiting for locks are not counted into the number because otherwise
we could get a deadlock. Value of 0 will disable the concurrency check. */

UNIV_INTERN ulong	srv_thread_concurrency	= 0;

#ifndef HAVE_ATOMIC_BUILTINS

/** This mutex protects srv_conc data structures */
static os_fast_mutex_t	srv_conc_mutex;

/** Concurrency list node */
typedef UT_LIST_NODE_T(struct srv_conc_slot_t)	srv_conc_node_t;

/** Slot for a thread waiting in the concurrency control queue. */
struct srv_conc_slot_t{
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

#if defined(UNIV_PFS_MUTEX)
/* Key to register srv_conc_mutex_key with performance schema */
UNIV_INTERN mysql_pfs_key_t	srv_conc_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#endif /* !HAVE_ATOMIC_BUILTINS */

/** Variables tracking the active and waiting threads. */
struct srv_conc_t {
	char		pad[64  - (sizeof(ulint) + sizeof(lint))];

	/** Number of transactions that have declared_to_be_inside_innodb set.
	It used to be a non-error for this value to drop below zero temporarily.
	This is no longer true. We'll, however, keep the lint datatype to add
	assertions to catch any corner cases that we may have missed. */

	volatile lint	n_active;

	/** Number of OS threads waiting in the FIFO for permission to
	enter InnoDB */
	volatile lint	n_waiting;
};

/* Control variables for tracking concurrency. */
static srv_conc_t	srv_conc;

/*********************************************************************//**
Initialise the concurrency management data structures */
void
srv_conc_init(void)
/*===============*/
{
#ifndef HAVE_ATOMIC_BUILTINS
	ulint		i;

	/* Init the server concurrency restriction data structures */

	os_fast_mutex_init(srv_conc_mutex_key, &srv_conc_mutex);

	UT_LIST_INIT(srv_conc_queue);

	srv_conc_slots = static_cast<srv_conc_slot_t*>(
		mem_zalloc(OS_THREAD_MAX_N * sizeof(*srv_conc_slots)));

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		srv_conc_slot_t*	conc_slot = &srv_conc_slots[i];

		conc_slot->event = os_event_create();
		ut_a(conc_slot->event);
	}
#endif /* !HAVE_ATOMIC_BUILTINS */
}

/*********************************************************************//**
Free the concurrency management data structures */
void
srv_conc_free(void)
/*===============*/
{
#ifndef HAVE_ATOMIC_BUILTINS
	os_fast_mutex_free(&srv_conc_mutex);
	mem_free(srv_conc_slots);
	srv_conc_slots = NULL;
#endif /* !HAVE_ATOMIC_BUILTINS */
}

#ifdef HAVE_ATOMIC_BUILTINS
/*********************************************************************//**
Note that a user thread is entering InnoDB. */
static
void
srv_enter_innodb_with_tickets(
/*==========================*/
	trx_t*	trx)			/*!< in/out: transaction that wants
					to enter InnoDB */
{
	trx->declared_to_be_inside_innodb = TRUE;
	trx->n_tickets_to_enter_innodb = srv_n_free_tickets_to_enter;
}

/*********************************************************************//**
Handle the scheduling of a user thread that wants to enter InnoDB.  Setting
srv_adaptive_max_sleep_delay > 0 switches the adaptive sleep calibration to
ON. When set, we want to wait in the queue for as little time as possible.
However, very short waits will result in a lot of context switches and that
is also not desirable. When threads need to sleep multiple times we increment
os_thread_sleep_delay by one. When we see threads getting a slot without
waiting and there are no other threads waiting in the queue, we try and reduce
the wait as much as we can. Currently we reduce it by half each time. If the
thread only had to wait for one turn before it was able to enter InnoDB we
decrement it by one. This is to try and keep the sleep time stable around the
"optimum" sleep time. */
static
void
srv_conc_enter_innodb_with_atomics(
/*===============================*/
	trx_t*	trx)			/*!< in/out: transaction that wants
					to enter InnoDB */
{
	ulint	n_sleeps = 0;
	ibool	notified_mysql = FALSE;

	ut_a(!trx->declared_to_be_inside_innodb);

	for (;;) {
		ulint	sleep_in_us;

		if (srv_conc.n_active < (lint) srv_thread_concurrency) {
			ulint	n_active;

			/* Check if there are any free tickets. */
			n_active = os_atomic_increment_lint(
				&srv_conc.n_active, 1);

			if (n_active <= srv_thread_concurrency) {

				srv_enter_innodb_with_tickets(trx);

				if (notified_mysql) {

					(void) os_atomic_decrement_lint(
						&srv_conc.n_waiting, 1);

					thd_wait_end(trx->mysql_thd);
				}

				if (srv_adaptive_max_sleep_delay > 0) {
					if (srv_thread_sleep_delay > 20
					    && n_sleeps == 1) {

						--srv_thread_sleep_delay;
					}

					if (srv_conc.n_waiting == 0) {
						srv_thread_sleep_delay >>= 1;
					}
				}

				return;
			}

			/* Since there were no free seats, we relinquish
			the overbooked ticket. */

			(void) os_atomic_decrement_lint(
				&srv_conc.n_active, 1);
		}

		if (!notified_mysql) {
			(void) os_atomic_increment_lint(
				&srv_conc.n_waiting, 1);

			/* Release possible search system latch this
			thread has */

			if (trx->has_search_latch) {
				trx_search_latch_release_if_reserved(trx);
			}

			thd_wait_begin(trx->mysql_thd, THD_WAIT_USER_LOCK);

			notified_mysql = TRUE;
		}

		trx->op_info = "sleeping before entering InnoDB";

		sleep_in_us = srv_thread_sleep_delay;

		/* Guard against overflow when adaptive sleep delay is on. */

		if (srv_adaptive_max_sleep_delay > 0
		    && sleep_in_us > srv_adaptive_max_sleep_delay) {

			sleep_in_us = srv_adaptive_max_sleep_delay;
			srv_thread_sleep_delay = sleep_in_us;
		}

		os_thread_sleep(sleep_in_us);

		trx->op_info = "";

		++n_sleeps;

		if (srv_adaptive_max_sleep_delay > 0 && n_sleeps > 1) {
			++srv_thread_sleep_delay;
		}
	}
}

/*********************************************************************//**
Note that a user thread is leaving InnoDB code. */
static
void
srv_conc_exit_innodb_with_atomics(
/*==============================*/
	trx_t*	trx)		/*!< in/out: transaction */
{
	trx->n_tickets_to_enter_innodb = 0;
	trx->declared_to_be_inside_innodb = FALSE;

	(void) os_atomic_decrement_lint(&srv_conc.n_active, 1);
}
#else
/*********************************************************************//**
Note that a user thread is leaving InnoDB code. */
static
void
srv_conc_exit_innodb_without_atomics(
/*=================================*/
	trx_t*	trx)		/*!< in/out: transaction */
{
	srv_conc_slot_t*	slot;

	os_fast_mutex_lock(&srv_conc_mutex);

	ut_ad(srv_conc.n_active > 0);
	srv_conc.n_active--;
	trx->declared_to_be_inside_innodb = FALSE;
	trx->n_tickets_to_enter_innodb = 0;

	slot = NULL;

	if (srv_conc.n_active < (lint) srv_thread_concurrency) {
		/* Look for a slot where a thread is waiting and no other
		thread has yet released the thread */

		for (slot = UT_LIST_GET_FIRST(srv_conc_queue);
		     slot != NULL && slot->wait_ended == TRUE;
		     slot = UT_LIST_GET_NEXT(srv_conc_queue, slot)) {

			/* No op */
		}

		if (slot != NULL) {
			slot->wait_ended = TRUE;

			/* We increment the count on behalf of the released
			thread */

			srv_conc.n_active++;
		}
	}

	os_fast_mutex_unlock(&srv_conc_mutex);

	if (slot != NULL) {
		os_event_set(slot->event);
	}
}

/*********************************************************************//**
Handle the scheduling of a user thread that wants to enter InnoDB. */
static
void
srv_conc_enter_innodb_without_atomics(
/*==================================*/
	trx_t*	trx)			/*!< in/out: transaction that wants
					to enter InnoDB */
{
	ulint			i;
	srv_conc_slot_t*	slot = NULL;
	ibool			has_slept = FALSE;

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

	ut_ad(srv_conc.n_active >= 0);

	if (srv_conc.n_active < (lint) srv_thread_concurrency) {

		srv_conc.n_active++;
		trx->declared_to_be_inside_innodb = TRUE;
		trx->n_tickets_to_enter_innodb = srv_n_free_tickets_to_enter;

		os_fast_mutex_unlock(&srv_conc_mutex);

		return;
	}

	/* If the transaction is not holding resources, let it sleep
	for srv_thread_sleep_delay microseconds, and try again then */

	if (!has_slept && !trx->has_search_latch
	    && NULL == UT_LIST_GET_FIRST(trx->lock.trx_locks)) {

		has_slept = TRUE; /* We let it sleep only once to avoid
				starvation */

		srv_conc.n_waiting++;

		os_fast_mutex_unlock(&srv_conc_mutex);

		trx->op_info = "sleeping before joining InnoDB queue";

		/* Peter Zaitsev suggested that we take the sleep away
		altogether. But the sleep may be good in pathological
		situations of lots of thread switches. Simply put some
		threads aside for a while to reduce the number of thread
		switches. */
		if (srv_thread_sleep_delay > 0) {
			os_thread_sleep(srv_thread_sleep_delay);
		}

		trx->op_info = "";

		os_fast_mutex_lock(&srv_conc_mutex);

		srv_conc.n_waiting--;

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

		srv_conc.n_active++;
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

	srv_conc.n_waiting++;

	os_fast_mutex_unlock(&srv_conc_mutex);

	/* Go to wait for the event; when a thread leaves InnoDB it will
	release this thread */

	ut_ad(!trx->has_search_latch);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */
	trx->op_info = "waiting in InnoDB queue";

	thd_wait_begin(trx->mysql_thd, THD_WAIT_USER_LOCK);

	os_event_wait(slot->event);
	thd_wait_end(trx->mysql_thd);

	trx->op_info = "";

	os_fast_mutex_lock(&srv_conc_mutex);

	srv_conc.n_waiting--;

	/* NOTE that the thread which released this thread already
	incremented the thread counter on behalf of this thread */

	slot->reserved = FALSE;

	UT_LIST_REMOVE(srv_conc_queue, srv_conc_queue, slot);

	trx->declared_to_be_inside_innodb = TRUE;
	trx->n_tickets_to_enter_innodb = srv_n_free_tickets_to_enter;

	os_fast_mutex_unlock(&srv_conc_mutex);
}
#endif /* HAVE_ATOMIC_BUILTINS */

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
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */

#ifdef HAVE_ATOMIC_BUILTINS
	srv_conc_enter_innodb_with_atomics(trx);
#else
	srv_conc_enter_innodb_without_atomics(trx);
#endif /* HAVE_ATOMIC_BUILTINS */
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

	if (!srv_thread_concurrency) {

		return;
	}

	ut_ad(srv_conc.n_active >= 0);

#ifdef HAVE_ATOMIC_BUILTINS
	(void) os_atomic_increment_lint(&srv_conc.n_active, 1);
#else
	os_fast_mutex_lock(&srv_conc_mutex);
	++srv_conc.n_active;
	os_fast_mutex_unlock(&srv_conc_mutex);
#endif /* HAVE_ATOMIC_BUILTINS */

	trx->n_tickets_to_enter_innodb = 1;
	trx->declared_to_be_inside_innodb = TRUE;
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
	if ((trx->mysql_thd != NULL
	     && thd_is_replication_slave_thread(trx->mysql_thd))
	    || trx->declared_to_be_inside_innodb == FALSE) {

		return;
	}

#ifdef HAVE_ATOMIC_BUILTINS
	srv_conc_exit_innodb_with_atomics(trx);
#else
	srv_conc_exit_innodb_without_atomics(trx);
#endif /* HAVE_ATOMIC_BUILTINS */

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */
}

/*********************************************************************//**
Get the count of threads waiting inside InnoDB. */
UNIV_INTERN
ulint
srv_conc_get_waiting_threads(void)
/*==============================*/
{
	return(srv_conc.n_waiting);
}

/*********************************************************************//**
Get the count of threads active inside InnoDB. */
UNIV_INTERN
ulint
srv_conc_get_active_threads(void)
/*==============================*/
{
	return(srv_conc.n_active);
 }

