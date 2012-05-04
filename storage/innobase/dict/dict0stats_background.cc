/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file dict/dict0stats_background.cc
Code used for background table and index stats gathering.

Created Apr 25, 2012 Vasil Dimov
*******************************************************/

#include "univ.i"

#include "dict0dict.h" /* dict_table_open_on_id() */
#include "dict0stats.h" /* DICT_STATS_RECALC_PERSISTENT */
#include "dict0stats_background.h"
#include "dict0mem.h" /* dict_table_struct */
#include "dict0types.h" /* table_id_t */
#include "ha0storage.h" /* ha_storage_* */
#include "os0thread.h" /* DECLARE_THREAD, os_thread_*, mysql_pfs_key_t */
#include "os0sync.h" /* os_event_t, os_event_create() */
#include "srv0srv.h" /* srv_dict_stats_thread_active */
#include "srv0start.h" /* SRV_SHUTDOWN_NONE */
#include "sync0sync.h" /* mutex_create() */

/** Minimum time interval between stats recalc for a given table */
#define MIN_RECALC_INTERVAL	10 /* seconds */

#define SHUTTING_DOWN()	(srv_shutdown_state != SRV_SHUTDOWN_NONE)

/** Event to wake up the stats thread */
UNIV_INTERN os_event_t	dict_stats_event = NULL;

/** This mutex protects the following auto_recalc_* variables. */
static mutex_t		auto_recalc_mutex;
#ifdef HAVE_PSI_INTERFACE
static mysql_pfs_key_t	auto_recalc_mutex_key;
#endif /* HAVE_PSI_INTERFACE */
/** The number of tables that can be queued before the queue is enlarged */
#define AUTO_RECALC_LIST_INITIAL_SLOTS	128
/** The autorecalc list - a simple array */
static table_id_t*	auto_recalc_list = NULL;
/** The size of the auto_recalc_list, in number of slots (not bytes) */
static ulint		auto_recalc_size;
/** The number of slots used in auto_recalc_list */
static ulint		auto_recalc_used;

/*****************************************************************//**
Add a table to the auto recalc list, which is processed by the
background stats gathering thread. Only the table id is added to the
list, so the table can be closed after being enqueued and it will be
opened when needed. If the table does not exist later (has been DROPped),
then it will be removed from the list and skipped.
dict_stats_enqueue_table_for_auto_recalc() @{ */
UNIV_INTERN
void
dict_stats_enqueue_table_for_auto_recalc(
/*=====================================*/
	const dict_table_t*	table)	/*!< in: table */
{
	mutex_enter(&auto_recalc_mutex);

	/* search if already in the list */
	for (ulint i = 0; i < auto_recalc_used; i++) {
		if (auto_recalc_list[i] == table->id) {
			/* already in the list, do not add */
			mutex_exit(&auto_recalc_mutex);
			return;
		}
	}

	/* not in the list, need to be added */

	/* enlarge if no more space */
	if (auto_recalc_used == auto_recalc_size) {
		table_id_t*	p;
		p = (table_id_t*)
			ut_realloc(auto_recalc_list, auto_recalc_size * 2);
		if (p == NULL) {
			/* auto_recalc_list is still valid, just quit without
			adding the table to the list, maybe allocation will
			succeed in the next enqueue operation */
			mutex_exit(&auto_recalc_mutex);
			return;
		}
		auto_recalc_list = p;
		auto_recalc_size *= 2;
	}

	auto_recalc_list[auto_recalc_used] = table->id;

	auto_recalc_used++;

	mutex_exit(&auto_recalc_mutex);

	os_event_set(dict_stats_event);
}
/* @} */

/*****************************************************************//**
Pop a table from the auto recalc list.
dict_stats_dequeue_table_for_auto_recalc() @{
@return TRUE if list was non-empty and "id" was set, FALSE otherwise */
static
ibool
dict_stats_dequeue_table_for_auto_recalc(
/*=====================================*/
	table_id_t*	id)	/*!< out: table id, or unmodified if list is
				empty */
{
	mutex_enter(&auto_recalc_mutex);

	if (auto_recalc_used == 0) {
		/* empty list */
		mutex_exit(&auto_recalc_mutex);
		return(FALSE);
	}
	/* else */

	*id = auto_recalc_list[0];

	auto_recalc_used--;

	ut_memmove(
		&auto_recalc_list[0],
		&auto_recalc_list[1],
		auto_recalc_used * sizeof(auto_recalc_list[0]));

	mutex_exit(&auto_recalc_mutex);

	return(TRUE);
}
/* @} */

/*****************************************************************//**
Initialize global variables needed for the operation of dict_stats_thread()
Must be called before dict_stats_thread() is started.
dict_stats_thread_init() @{ */
UNIV_INTERN
void
dict_stats_thread_init()
/*====================*/
{
	dict_stats_event = os_event_create("dict_stats_event");

	/* The auto_recalc_mutex is acquired from:
	+ the background stats gathering thread before any other latch
 	  and released without latching anything else in between (thus any
	  level would do here)
	+ from row_update_statistics_if_needed()
	  and released without latching anything else in between. We know
	  that dict_sys->mutex (level SYNC_DICT) is not acquired when
	  row_update_statistics_if_needed() is called and it may be acquired
	  inside that function. So level SYNC_DICT is appropriate for
	  auto_recalc_mutex. */
	mutex_create(auto_recalc_mutex_key, &auto_recalc_mutex, SYNC_DICT);

	ut_ad(auto_recalc_list == NULL);

	auto_recalc_list = (table_id_t*)
		ut_malloc(AUTO_RECALC_LIST_INITIAL_SLOTS
			  * sizeof(auto_recalc_list[0]));
	ut_a(auto_recalc_list != NULL);
	auto_recalc_size = AUTO_RECALC_LIST_INITIAL_SLOTS;
	auto_recalc_used = 0;
}
/* @} */

/*****************************************************************//**
Free resources allocated by dict_stats_thread_init(), must be called
after dict_stats_thread() has exited.
dict_stats_thread_deinit() @{ */
UNIV_INTERN
void
dict_stats_thread_deinit()
/*======================*/
{
	ut_ad(!srv_dict_stats_thread_active);

	ut_free(auto_recalc_list);
	auto_recalc_list = NULL;
	auto_recalc_size = 0;
	auto_recalc_used = 0;

	mutex_free(&auto_recalc_mutex);
	memset(&auto_recalc_mutex, 0x0, sizeof(auto_recalc_mutex));

	os_event_free(dict_stats_event);
	dict_stats_event = NULL;
}
/* @} */

/*****************************************************************//**
Pop the first table that has been enqueued for auto recalc and eventually
update its stats.
pop_from_auto_recalc_list_and_recalc() @{ */
static
void
pop_from_auto_recalc_list_and_recalc()
/*==================================*/
{
printf("%s() begin\n", __func__);
	table_id_t	table_id;

	/* pop the first table from the auto recalc list */
	if (!dict_stats_dequeue_table_for_auto_recalc(&table_id)) {
		/* no tables for auto recalc */
printf("%s() empty list\n", __func__);
		return;
	}
	/* else */

	dict_table_t*	table;

	table = dict_table_open_on_id(table_id, FALSE, FALSE);

	if (table == NULL) {
printf("%s() table %lu has gone\n", __func__, table_id);
		/* table does not exist, must have been DROPped
		after its id was enqueued */
		return;
	}
	/* else */

	if (ut_difftime(ut_time(), table->stats_last_recalc)
	    < MIN_RECALC_INTERVAL) {
printf("%s() table %lu recalc was too soon, noop\n", __func__, table_id);
		/* Stats were (re)calculated not long ago. To avoid
		too frequent stats updates we put back the table on
		the auto recalc list and do nothing. */
		dict_stats_enqueue_table_for_auto_recalc(table);
		dict_table_close(table, FALSE, FALSE);
		return;
	}
	/* else */

printf("%s() table %lu RECALC\n", __func__, table_id);
	dict_stats_update(table, DICT_STATS_RECALC_PERSISTENT, FALSE);

	dict_table_close(table, FALSE, FALSE);
}
/* @} */

/*****************************************************************//**
This is the thread for background stats gathering. It pops tables, from
the auto recalc list and proceeds them, eventually recalculating their
statistics.
dict_stats_thread() @{
@return this function does not return, it calls os_thread_exit() */
extern "C" UNIV_INTERN
os_thread_ret_t
DECLARE_THREAD(dict_stats_thread)(
/*==============================*/
	void*	arg __attribute__((unused)))	/*!< in: a dummy parameter
						required by os_thread_create */
{
	srv_dict_stats_thread_active = TRUE;

	while (!SHUTTING_DOWN()) {

printf("%s() wait\n", __func__);
		/* Wake up periodically even if not signaled. This is
		because we may lose an event - if the below call to
		pop_from_auto_recalc_list_and_recalc() puts the entry back
		in the list, the os_event_set() will be lost by the subsequent
		os_event_reset(). */
		os_event_wait_time(dict_stats_event,
				   MIN_RECALC_INTERVAL * 1000000);
ut_print_timestamp(stdout);
printf(" %s() waked up\n", __func__);

		if (SHUTTING_DOWN()) {
			break;
		}

		pop_from_auto_recalc_list_and_recalc();

		os_event_reset(dict_stats_event);
	}

	srv_dict_stats_thread_active = FALSE;

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit instead of return(). */
	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}
/* @} */

/* vim: set foldmethod=marker foldmarker=@{,@}: */
