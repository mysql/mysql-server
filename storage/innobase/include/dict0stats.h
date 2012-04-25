/*****************************************************************************

Copyright (c) 2009, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

#include "db0err.h"
#include "dict0types.h"
#include "trx0types.h"

enum dict_stats_upd_option {
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

typedef enum dict_stats_upd_option	dict_stats_upd_option_t;

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
	__attribute__((nonnull));

/*********************************************************************//**
Check whether persistent statistics is enabled for a given table.
@return TRUE if enabled, FALSE otherwise */
UNIV_INLINE
ibool
dict_stats_is_persistent_enabled(
/*=============================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));

/*********************************************************************//**
Set the auto recalc flag for a given table (only honored for a persistent
stats enabled table). This is set only in the in-memory table object and is
not saved on disk. It will be read from the .frm file upon first open from
MySQL after a server restart. */
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
	dict_table_t*	table,			/*!< in/out: table */
	ibool		ps_on,			/*!< in: persistent stats
						explicitly enabled */
	ibool		ps_off,			/*!< in: persistent stats
						explicitly disabled */
	ibool		auto_recalc_on,		/*!< in: auto recalc enabled */
	ibool		auto_recalc_off);	/*!< in: auto recalc disabled */

/*********************************************************************//**
Deinitialize table's stats after the last close of the table. This is
used to detect "FLUSH TABLE" and refresh the stats upon next open. */
UNIV_INLINE
void
dict_stats_deinit(
/*==============*/
	dict_table_t*	table)	/*!< in/out: table */
	__attribute__((nonnull));

/*********************************************************************//**
Calculates new estimates for table and index statistics. The statistics
are used in query optimization.
@return DB_* error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
dict_stats_update(
/*==============*/
	dict_table_t*		table,	/*!< in/out: table */
	dict_stats_upd_option_t	stats_upd_option,
					/*!< in: whether to (re) calc
					the stats or to fetch them from
					the persistent storage */
	ibool			caller_has_dict_sys_mutex);
					/*!< in: TRUE if the caller
					owns dict_sys->mutex */

/*********************************************************************//**
Removes the information for a particular index's stats from the persistent
storage if it exists and if there is data stored for this index.
This function creates its own trx and commits it.
@return DB_SUCCESS or error code */
UNIV_INTERN
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
UNIV_INTERN
dberr_t
dict_stats_drop_table(
/*==================*/
	const char*	table_name,	/*!< in: table name */
	char*		errstr,		/*!< out: error message
					if != DB_SUCCESS is returned */
	ulint		errstr_sz);	/*!< in: size of errstr buffer */

/*********************************************************************//**
Fetches or calculates new estimates for index statistics. */
UNIV_INTERN
void
dict_stats_update_for_index(
/*========================*/
	dict_index_t*	index)	/*!< in/out: index */
	__attribute__((nonnull));

/*********************************************************************//**
Renames a table in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_stats_rename_table(
/*====================*/
	const char*	old_name,	/*!< in: old table name */
	const char*	new_name,	/*!< in: new table name */
	char*		errstr,		/*!< out: error string if != DB_SUCCESS
					is returned */
	size_t		errstr_sz);	/*!< in: errstr size */

/*********************************************************************//**
Duplicate the stats of a table and its indexes.
This function creates a dummy dict_table_t object and copies the input
table's stats into it. The returned table object is not in the dictionary
cache, cannot be accessed by any other threads and has only the following
members initialized:
dict_table_t::id
dict_table_t::heap
dict_table_t::name
dict_table_t::indexes<>
dict_table_t::stat_initialized
dict_table_t::stat_persistent
dict_table_t::stat_n_rows
dict_table_t::stat_clustered_index_size
dict_table_t::stat_sum_of_other_index_sizes
dict_table_t::stat_modified_counter
dict_table_t::magic_n
for each entry in dict_table_t::indexes, the following are initialized:
dict_index_t::id
dict_index_t::name
dict_index_t::table_name
dict_index_t::table (points to the above semi-initialized object)
dict_index_t::n_uniq
dict_index_t::fields[] (only first n_uniq and only fields[i].name)
dict_index_t::indexes<>
dict_index_t::stat_n_diff_key_vals[]
dict_index_t::stat_n_sample_sizes[]
dict_index_t::stat_n_non_null_key_vals[]
dict_index_t::stat_index_size
dict_index_t::stat_n_leaf_pages
dict_index_t::magic_n
The returned object should be freed with dict_stats_dup_free() when no
longer needed.
@return incomplete table object */
UNIV_INTERN
dict_table_t*
dict_stats_snapshot_create(
/*=======================*/
	const dict_table_t*	table);	/*!< in: table whose stats to copy */

/*********************************************************************//**
Free the resources occupied by an object returned by
dict_stats_snapshot_create(). */
UNIV_INTERN
void
dict_stats_snapshot_free(
/*=====================*/
	dict_table_t*	t);	/*!< in: dummy table object to free */

#ifndef UNIV_NONINL
#include "dict0stats.ic"
#endif

#endif /* dict0stats_h */
