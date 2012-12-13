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
@file dict/dict0stats_bg.cc
Code used for background table and index stats gathering.

Created Apr 25, 2012 Vasil Dimov
*******************************************************/

#include "univ.i"

#include "dict0dict.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "row0mysql.h"
#include "srv0start.h"

#include <vector>

/** Minimum time interval between stats recalc for a given table */
#define MIN_RECALC_INTERVAL	10 /* seconds */

#define SHUTTING_DOWN()		(srv_shutdown_state != SRV_SHUTDOWN_NONE)

/** Event to wake up the stats thread */
UNIV_INTERN os_event_t		dict_stats_event = NULL;

/** This mutex protects the "recalc_pool" variable. */
static ib_mutex_t		recalc_pool_mutex;

#ifdef HAVE_PSI_INTERFACE
UNIV_INTERN mysql_pfs_key_t	recalc_pool_mutex_key;
#endif /* HAVE_PSI_INTERFACE */

/** The number of tables that can be added to "recalc_pool" before
it is enlarged */
static const ulint RECALC_POOL_INITIAL_SLOTS = 128;

/** The multitude of tables whose stats are to be automatically
recalculated - an STL vector */
typedef std::vector<table_id_t>	recalc_pool_t;

static recalc_pool_t		recalc_pool;

typedef recalc_pool_t::iterator	recalc_pool_iterator_t;

/*****************************************************************//**
Initialize the recalc pool, called once during thread initialization. */
static
void
dict_stats_recalc_pool_init()
/*=========================*/
{
	ut_ad(!srv_read_only_mode);

	recalc_pool.reserve(RECALC_POOL_INITIAL_SLOTS);
}

/*****************************************************************//**
Free the resources occupied by the recalc pool, called once during
thread de-initialization. */
static
void
dict_stats_recalc_pool_deinit()
/*===========================*/
{
	ut_ad(!srv_read_only_mode);

	recalc_pool.clear();
}

/*****************************************************************//**
Add a table to the recalc pool, which is processed by the
background stats gathering thread. Only the table id is added to the
list, so the table can be closed after being enqueued and it will be
opened when needed. If the table does not exist later (has been DROPped),
then it will be removed from the pool and skipped.
dict_stats_recalc_pool_add() @{ */
UNIV_INTERN
void
dict_stats_recalc_pool_add(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table to add */
{
	ut_ad(!srv_read_only_mode);

	mutex_enter(&recalc_pool_mutex);

	/* quit if already in the list */
	for (recalc_pool_iterator_t iter = recalc_pool.begin();
	     iter != recalc_pool.end();
	     ++iter) {

		if (*iter == table->id) {
			mutex_exit(&recalc_pool_mutex);
			return;
		}
	}

	recalc_pool.push_back(table->id);

	mutex_exit(&recalc_pool_mutex);

	os_event_set(dict_stats_event);
}
/* @} */

/*****************************************************************//**
Get a table from the auto recalc pool. The returned table id is removed
from the pool.
dict_stats_recalc_pool_get() @{
@return true if the pool was non-empty and "id" was set, false otherwise */
static
bool
dict_stats_recalc_pool_get(
/*=======================*/
	table_id_t*	id)	/*!< out: table id, or unmodified if list is
				empty */
{
	ut_ad(!srv_read_only_mode);

	mutex_enter(&recalc_pool_mutex);

	if (recalc_pool.empty()) {
		mutex_exit(&recalc_pool_mutex);
		return(false);
	}

	*id = recalc_pool[0];

	recalc_pool.erase(recalc_pool.begin());

	mutex_exit(&recalc_pool_mutex);

	return(true);
}
/* @} */

/*****************************************************************//**
Delete a given table from the auto recalc pool.
dict_stats_recalc_pool_del() */
UNIV_INTERN
void
dict_stats_recalc_pool_del(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table to remove */
{
	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_enter(&recalc_pool_mutex);

	ut_ad(table->id > 0);

	for (recalc_pool_iterator_t iter = recalc_pool.begin();
	     iter != recalc_pool.end();
	     ++iter) {

		if (*iter == table->id) {
			/* erase() invalidates the iterator */
			recalc_pool.erase(iter);
			break;
		}
	}

	mutex_exit(&recalc_pool_mutex);
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
	ut_ad(!srv_read_only_mode);

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
	ut_a(!srv_read_only_mode);

	dict_stats_event = os_event_create(0);

	/* The recalc_pool_mutex is acquired from:
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

	mutex_create("recalc_pool", &recalc_pool_mutex);

	dict_stats_recalc_pool_init();
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
	ut_a(!srv_read_only_mode);
	ut_ad(!srv_dict_stats_thread_active);

	dict_stats_recalc_pool_deinit();

	mutex_free(&recalc_pool_mutex);
	memset(&recalc_pool_mutex, 0x0, sizeof(recalc_pool_mutex));

	os_event_destroy(dict_stats_event);
	dict_stats_event = NULL;
}
/* @} */

/*****************************************************************//**
Get the first table that has been added for auto recalc and eventually
update its stats.
dict_stats_process_entry_from_recalc_pool() @{ */
static
void
dict_stats_process_entry_from_recalc_pool()
/*=======================================*/
{
	table_id_t	table_id;

	ut_ad(!srv_read_only_mode);

	/* pop the first table from the auto recalc pool */
	if (!dict_stats_recalc_pool_get(&table_id)) {
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

	/* Check whether table is corrupted */
	if (table->corrupted) {
		dict_table_close(table, TRUE, FALSE);
		mutex_exit(&dict_sys->mutex);
		return;
	}

	table->stats_bg_flag = BG_STAT_IN_PROGRESS;

	mutex_exit(&dict_sys->mutex);

	/* ut_time() could be expensive, the current function
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

		dict_stats_recalc_pool_add(table);

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
	ut_a(!srv_read_only_mode);

	srv_dict_stats_thread_active = TRUE;

	while (!SHUTTING_DOWN()) {

		/* Wake up periodically even if not signaled. This is
		because we may lose an event - if the below call to
		dict_stats_process_entry_from_recalc_pool() puts the entry back
		in the list, the os_event_set() will be lost by the subsequent
		os_event_reset(). */
		os_event_wait_time(
			dict_stats_event, MIN_RECALC_INTERVAL * 1000000);

		if (SHUTTING_DOWN()) {
			break;
		}

		dict_stats_process_entry_from_recalc_pool();

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
