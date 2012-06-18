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
#include "row0mysql.h" /* row_mysql_lock_data_dictionary() */
#include "srv0srv.h" /* srv_dict_stats_thread_active */
#include "srv0start.h" /* SRV_SHUTDOWN_NONE */
#include "sync0sync.h" /* mutex_create() */

/** Minimum time interval between stats recalc for a given table */
#define MIN_RECALC_INTERVAL	10 /* seconds */

#define SHUTTING_DOWN()	(srv_shutdown_state != SRV_SHUTDOWN_NONE)

/** Event to wake up the stats thread */
UNIV_INTERN os_event_t	dict_stats_event = NULL;

/** This mutex protects the following auto_recalc_* variables. */
static ib_mutex_t	auto_recalc_mutex;
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

	ut_a(auto_recalc_used <= auto_recalc_size);

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

		p = reinterpret_cast<table_id_t*>(
			ut_realloc(auto_recalc_list,
				   auto_recalc_size * 2
				   * sizeof(auto_recalc_list[0])));

		if (p == 0) {
			/* auto_recalc_list is still valid, just quit without
			adding the table to the list, maybe allocation will
			succeed in the next enqueue operation */
			mutex_exit(&auto_recalc_mutex);
			return;
		}

		auto_recalc_list = p;
		auto_recalc_size *= 2;
	}

	auto_recalc_list[auto_recalc_used++] = table->id;

	mutex_exit(&auto_recalc_mutex);

	os_event_set(dict_stats_event);
}
/* @} */

/*****************************************************************//**
Pop a table from the auto recalc list.
dict_stats_dequeue_table_for_auto_recalc() @{
@return true if list was non-empty and "id" was set, false otherwise */
static
bool
dict_stats_dequeue_table_for_auto_recalc(
/*=====================================*/
	table_id_t*	id)	/*!< out: table id, or unmodified if list is
				empty */
{
	*id = UINT64_UNDEFINED;

	mutex_enter(&auto_recalc_mutex);

	if (auto_recalc_used > 0) {

		*id = auto_recalc_list[0];

		ut_a(auto_recalc_used > 0);

		--auto_recalc_used;

		ut_memmove(
			&auto_recalc_list[0],
			&auto_recalc_list[1],
			auto_recalc_used * sizeof(auto_recalc_list[0]));
	}

	mutex_exit(&auto_recalc_mutex);

	return(*id != UINT64_UNDEFINED);
}
/* @} */

/*****************************************************************//**
Remove a table from the auto recalc list.
dict_stats_remove_table_from_auto_recalc() */
UNIV_INTERN
void
dict_stats_remove_table_from_auto_recalc(
/*=====================================*/
	const dict_table_t*	table)	/*!< in: table to remove */
{
	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_enter(&auto_recalc_mutex);

	ut_ad(table->id > 0);

	for (ulint i = 0; i < auto_recalc_used; ++i) {

		if (auto_recalc_list[i] == table->id) {

			ut_memmove(
				&auto_recalc_list[i],
				&auto_recalc_list[i + 1],
				(auto_recalc_used - i - 1)
				* sizeof(auto_recalc_list[0]));

			auto_recalc_used--;

			break;
		}
	}

	mutex_exit(&auto_recalc_mutex);
}

/*****************************************************************//**
Wait until background stats thread has stopped using the specified table(s).
The caller must have locked the data dictionary using
row_mysql_lock_data_dictionary() and this function may unlock it temporarily
and restore the lock before it exits.
The background stats thead is guaranteed not to start using the specified
tables after this function returns and before the caller unlocks the data
dictionary because it sets the BG_STAT_IN_PROGRESS bit in table->stats_bg_flag
under dict_sys->mutex.
dict_stats_wait_bg_to_stop_using_table() @{ */
UNIV_INTERN
void
dict_stats_wait_bg_to_stop_using_tables(
/*====================================*/
	dict_table_t*	table1,	/*!< in/out: table1 */
	dict_table_t*	table2,	/*!< in/out: table2, could be NULL */
	trx_t*		trx)	/*!< in/out: transaction to use for
				unlocking/locking the data dict */
{
	while ((table1->stats_bg_flag & BG_STAT_IN_PROGRESS)
	       || (table2 != NULL
		   && (table2->stats_bg_flag & BG_STAT_IN_PROGRESS))) {

		table1->stats_bg_flag |= BG_STAT_SHOULD_QUIT;
		if (table2 != NULL) {
			table2->stats_bg_flag |= BG_STAT_SHOULD_QUIT;
		}

		row_mysql_unlock_data_dictionary(trx);
		os_thread_sleep(250000);
		row_mysql_lock_data_dictionary(trx);
	}
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
	1) the background stats gathering thread before any other latch
	   and released without latching anything else in between (thus
	   any level would do here)
	2) from row_update_statistics_if_needed()
	   and released without latching anything else in between. We know
	   that dict_sys->mutex (SYNC_DICT) is not acquired when
	   row_update_statistics_if_needed() is called and it may be acquired
	   inside that function (thus a level <=SYNC_DICT would do).
	3) from row_drop_table_for_mysql() after dict_sys->mutex (SYNC_DICT)
	   and dict_operation_lock (SYNC_DICT_OPERATION) have been locked
	   (thus a level <SYNC_DICT && <SYNC_DICT_OPERATION would do)
	So we choose SYNC_STATS_AUTO_RECALC to be about below SYNC_DICT. */
	mutex_create(auto_recalc_mutex_key, &auto_recalc_mutex,
		     SYNC_STATS_AUTO_RECALC);

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
	table_id_t	table_id;

	/* pop the first table from the auto recalc list */
	if (!dict_stats_dequeue_table_for_auto_recalc(&table_id)) {
		/* no tables for auto recalc */
		return;
	}

	dict_table_t*	table;

	mutex_enter(&dict_sys->mutex);

	table = dict_table_open_on_id(table_id, TRUE, FALSE);

	if (table == NULL) {
		/* table does not exist, must have been DROPped
		after its id was enqueued */
		mutex_exit(&dict_sys->mutex);
		return;
	}

	table->stats_bg_flag = BG_STAT_IN_PROGRESS;

	mutex_exit(&dict_sys->mutex);

	/* ut_time() could be expensive, pop_from_auto_recalc_list_and_recalc()
	is called once every time a table has been changed more than 10% and
	on a system with lots of small tables, this could become hot. If we
	find out that this is a problem, then the check below could eventually
	be replaced with something else, though a time interval is the natural
	approach. */

	if (ut_difftime(ut_time(), table->stats_last_recalc)
	    < MIN_RECALC_INTERVAL) {

		/* Stats were (re)calculated not long ago. To avoid
		too frequent stats updates we put back the table on
		the auto recalc list and do nothing. */

		dict_stats_enqueue_table_for_auto_recalc(table);

	} else {

		dict_stats_update(table, DICT_STATS_RECALC_PERSISTENT);
	}

	mutex_enter(&dict_sys->mutex);

	table->stats_bg_flag = BG_STAT_NONE;

	dict_table_close(table, TRUE, FALSE);

	mutex_exit(&dict_sys->mutex);
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

		/* Wake up periodically even if not signaled. This is
		because we may lose an event - if the below call to
		pop_from_auto_recalc_list_and_recalc() puts the entry back
		in the list, the os_event_set() will be lost by the subsequent
		os_event_reset(). */
		os_event_wait_time(
			dict_stats_event, MIN_RECALC_INTERVAL * 1000000);

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
