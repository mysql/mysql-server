/*****************************************************************************

Copyright (c) 2009, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dict0stats.h
Code used for calculating and manipulating table statistics.

Created Jan 06, 2010 Vasil Dimov
*******************************************************/

#ifndef dict0stats_h
#define dict0stats_h

#include "univ.i"

#include "dict0types.h"
#include "trx0types.h"

#define TABLE_STATS_NAME        "mysql/innodb_table_stats"
#define INDEX_STATS_NAME        "mysql/innodb_index_stats"

enum dict_stats_upd_option_t {
	DICT_STATS_RECALC_PERSISTENT,/* (re) calculate the
				statistics using a precise and slow
				algo and save them to the persistent
				storage, if the persistent storage is
				not present then emit a warning and
				fall back to transient stats */
	DICT_STATS_RECALC_TRANSIENT,/* (re) calculate the statistics
				using an imprecise quick algo
				without saving the results
				persistently */
	DICT_STATS_EMPTY_TABLE,	/* Write all zeros (or 1 where it makes sense)
				into a table and its indexes' statistics
				members. The resulting stats correspond to an
				empty table. If the table is using persistent
				statistics, then they are saved on disk. */
	DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY /* fetch the stats
				from the persistent storage if the in-memory
				structures have not been initialized yet,
				otherwise do nothing */
};

/*********************************************************************//**
Calculates new estimates for table and index statistics. This function
is relatively quick and is used to calculate transient statistics that
are not saved on disk.
This was the only way to calculate statistics before the
Persistent Statistics feature was introduced. */
void
dict_stats_update_transient(
/*========================*/
	dict_table_t*	table);	/*!< in/out: table */

/*********************************************************************//**
Set the persistent statistics flag for a given table. This is set only
in the in-memory table object and is not saved on disk. It will be read
from the .frm file upon first open from MySQL after a server restart. */
UNIV_INLINE
void
dict_stats_set_persistent(
/*======================*/
	dict_table_t*	table,	/*!< in/out: table */
	ibool		ps_on,	/*!< in: persistent stats explicitly enabled */
	ibool		ps_off)	/*!< in: persistent stats explicitly disabled */
	MY_ATTRIBUTE((nonnull));

/*********************************************************************//**
Check whether persistent statistics is enabled for a given table.
@return TRUE if enabled, FALSE otherwise */
UNIV_INLINE
ibool
dict_stats_is_persistent_enabled(
/*=============================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/*********************************************************************//**
Set the auto recalc flag for a given table (only honored for a persistent
stats enabled table). The flag is set only in the in-memory table object
and is not saved in InnoDB files. It will be read from the .frm file upon
first open from MySQL after a server restart. */
UNIV_INLINE
void
dict_stats_auto_recalc_set(
/*=======================*/
	dict_table_t*	table,			/*!< in/out: table */
	ibool		auto_recalc_on,		/*!< in: explicitly enabled */
	ibool		auto_recalc_off);	/*!< in: explicitly disabled */

/*********************************************************************//**
Check whether auto recalc is enabled for a given table.
@return TRUE if enabled, FALSE otherwise */
UNIV_INLINE
ibool
dict_stats_auto_recalc_is_enabled(
/*==============================*/
	const dict_table_t*	table);	/*!< in: table */

/*********************************************************************//**
Initialize table's stats for the first time when opening a table. */
UNIV_INLINE
void
dict_stats_init(
/*============*/
	dict_table_t*	table);	/*!< in/out: table */

/*********************************************************************//**
Deinitialize table's stats after the last close of the table. This is
used to detect "FLUSH TABLE" and refresh the stats upon next open. */
UNIV_INLINE
void
dict_stats_deinit(
/*==============*/
	dict_table_t*	table)	/*!< in/out: table */
	MY_ATTRIBUTE((nonnull));

/*********************************************************************//**
Calculates new estimates for table and index statistics. The statistics
are used in query optimization.
@return DB_* error code or DB_SUCCESS */
dberr_t
dict_stats_update(
/*==============*/
	dict_table_t*		table,	/*!< in/out: table */
	dict_stats_upd_option_t	stats_upd_option);
					/*!< in: whether to (re) calc
					the stats or to fetch them from
					the persistent storage */

/*********************************************************************//**
Removes the information for a particular index's stats from the persistent
storage if it exists and if there is data stored for this index.
This function creates its own trx and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_drop_index(
/*==================*/
	const char*	tname,	/*!< in: table name */
	const char*	iname,	/*!< in: index name */
	char*		errstr, /*!< out: error message if != DB_SUCCESS
				is returned */
	ulint		errstr_sz);/*!< in: size of the errstr buffer */

/*********************************************************************//**
Removes the statistics for a table and all of its indexes from the
persistent storage if it exists and if there is data stored for the table.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_drop_table(
/*==================*/
	const char*	table_name,	/*!< in: table name */
	char*		errstr,		/*!< out: error message
					if != DB_SUCCESS is returned */
	ulint		errstr_sz);	/*!< in: size of errstr buffer */

/*********************************************************************//**
Fetches or calculates new estimates for index statistics. */
void
dict_stats_update_for_index(
/*========================*/
	dict_index_t*	index)	/*!< in/out: index */
	MY_ATTRIBUTE((nonnull));

/*********************************************************************//**
Renames a table in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_rename_table(
/*====================*/
	bool		dict_locked,	/*!< in: true if dict_sys mutex
                                        and dict_operation_lock are held,
                                        otherwise false*/
	const char*	old_name,	/*!< in: old table name */
	const char*	new_name,	/*!< in: new table name */
	char*		errstr,		/*!< out: error string if != DB_SUCCESS
					is returned */
	size_t		errstr_sz);	/*!< in: errstr size */

/*********************************************************************//**
Renames an index in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code. DB_STATS_DO_NOT_EXIST will be returned
if the persistent stats do not exist. */
dberr_t
dict_stats_rename_index(
/*====================*/
	const dict_table_t*	table,		/*!< in: table whose index
						is renamed */
	const char*		old_index_name,	/*!< in: old index name */
	const char*		new_index_name)	/*!< in: new index name */
	MY_ATTRIBUTE((warn_unused_result));

#ifndef UNIV_NONINL
#include "dict0stats.ic"
#endif

#ifdef UNIV_ENABLE_UNIT_TEST_DICT_STATS
void test_dict_stats_all();
#endif /* UNIV_ENABLE_UNIT_TEST_DICT_STATS */

#endif /* dict0stats_h */
