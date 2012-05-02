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
@file include/dict0stats_background.h
Code used for background table and index stats gathering.

Created Apr 26, 2012 Vasil Dimov
*******************************************************/

#ifndef dict0stats_background_h
#define dict0stats_background_h

#include "univ.i"

#include "dict0types.h" /* dict_table_t, table_id_t */
#include "os0sync.h" /* os_event_t */
#include "os0thread.h" /* DECLARE_THREAD */

/** Event to wake up the stats thread */
extern os_event_t	dict_stats_event;

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
	const dict_table_t*	table);	/*!< in: table */
/* @} */

/*****************************************************************//**
Initialize global variables needed for the operation of dict_stats_thread().
Must be called before dict_stats_thread() is started.
dict_stats_thread_init() @{ */
UNIV_INTERN
void
dict_stats_thread_init();
/*====================*/
/* @} */

/*****************************************************************//**
Free resources allocated by dict_stats_thread_init(), must be called
after dict_stats_thread() has exited.
dict_stats_thread_deinit() @{ */
UNIV_INTERN
void
dict_stats_thread_deinit();
/*======================*/
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
	void*	arg);	/*!< in: a dummy parameter
			required by os_thread_create */
/* @} */

#endif /* dict0stats_background_h */
