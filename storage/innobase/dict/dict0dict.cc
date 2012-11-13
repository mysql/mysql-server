/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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

/******************************************************************//**
@file dict/dict0dict.cc
Data dictionary system

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

#include "dict0dict.h"
#include "fts0fts.h"
#include "fil0fil.h"

#ifdef UNIV_NONINL
#include "dict0dict.ic"
#include "dict0priv.ic"
#endif

/** dummy index for ROW_FORMAT=REDUNDANT supremum and infimum records */
UNIV_INTERN dict_index_t*	dict_ind_redundant;
/** dummy index for ROW_FORMAT=COMPACT supremum and infimum records */
UNIV_INTERN dict_index_t*	dict_ind_compact;

#ifndef UNIV_HOTBACKUP
#include "buf0buf.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0boot.h"
#include "dict0mem.h"
#include "dict0crea.h"
#include "dict0stats.h"
#include "trx0undo.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "page0zip.h"
#include "page0page.h"
#include "pars0pars.h"
#include "pars0sym.h"
#include "que0que.h"
#include "rem0cmp.h"
#include "fts0fts.h"
#include "fts0types.h"
#include "m_ctype.h" /* my_isspace() */
#include "ha_prototypes.h" /* innobase_strcasecmp(), innobase_casedn_str() */
#include "srv0mon.h"
#include "srv0start.h"
#include "lock0lock.h"
#include "dict0priv.h"
#include "row0upd.h"
#include "row0mysql.h"
#include "row0merge.h"
#include "row0log.h"
#include "ut0ut.h" /* ut_format_name() */
#include "m_string.h"
#include "my_sys.h"
#include "mysqld.h" /* system_charset_info */
#include "strfunc.h" /* strconvert() */

#include <ctype.h>

/** the dictionary system */
UNIV_INTERN dict_sys_t*	dict_sys	= NULL;

/** @brief the data dictionary rw-latch protecting dict_sys

table create, drop, etc. reserve this in X-mode; implicit or
backround operations purge, rollback, foreign key checks reserve this
in S-mode; we cannot trust that MySQL protects implicit or background
operations a table drop since MySQL does not know of them; therefore
we need this; NOTE: a transaction which reserves this must keep book
on the mode in trx_t::dict_operation_lock_mode */
UNIV_INTERN rw_lock_t	dict_operation_lock;

/** Percentage of compression failures that are allowed in a single
round */
UNIV_INTERN ulong	zip_failure_threshold_pct = 5;

/** Maximum percentage of a page that can be allowed as a pad to avoid
compression failures */
UNIV_INTERN ulong	zip_pad_max = 50;

/* Keys to register rwlocks and mutexes with performance schema */
#ifdef UNIV_PFS_RWLOCK
UNIV_INTERN mysql_pfs_key_t	dict_operation_lock_key;
UNIV_INTERN mysql_pfs_key_t	index_tree_rw_lock_key;
UNIV_INTERN mysql_pfs_key_t	index_online_log_key;
UNIV_INTERN mysql_pfs_key_t	dict_table_stats_latch_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	zip_pad_mutex_key;
UNIV_INTERN mysql_pfs_key_t	dict_sys_mutex_key;
UNIV_INTERN mysql_pfs_key_t	dict_foreign_err_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#define	DICT_HEAP_SIZE		100	/*!< initial memory heap size when
					creating a table or index object */
#define DICT_POOL_PER_TABLE_HASH 512	/*!< buffer pool max size per table
					hash table fixed size in bytes */
#define DICT_POOL_PER_VARYING	4	/*!< buffer pool max size per data
					dictionary varying size in bytes */

/** Identifies generated InnoDB foreign key names */
static char	dict_ibfk[] = "_ibfk_";

/** array of rw locks protecting
dict_table_t::stat_initialized
dict_table_t::stat_n_rows (*)
dict_table_t::stat_clustered_index_size
dict_table_t::stat_sum_of_other_index_sizes
dict_table_t::stat_modified_counter (*)
dict_table_t::indexes*::stat_n_diff_key_vals[]
dict_table_t::indexes*::stat_index_size
dict_table_t::indexes*::stat_n_leaf_pages
(*) those are not always protected for performance reasons */
#define DICT_TABLE_STATS_LATCHES_SIZE	64
static rw_lock_t	dict_table_stats_latches[DICT_TABLE_STATS_LATCHES_SIZE];

/*******************************************************************//**
Tries to find column names for the index and sets the col field of the
index.
@return TRUE if the column names were found */
static
ibool
dict_index_find_cols(
/*=================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index);	/*!< in: index */
/*******************************************************************//**
Builds the internal dictionary cache representation for a clustered
index, containing also system fields not defined by the user.
@return	own: the internal representation of the clustered index */
static
dict_index_t*
dict_index_build_internal_clust(
/*============================*/
	const dict_table_t*	table,	/*!< in: table */
	dict_index_t*		index);	/*!< in: user representation of
					a clustered index */
/*******************************************************************//**
Builds the internal dictionary cache representation for a non-clustered
index, containing also system fields not defined by the user.
@return	own: the internal representation of the non-clustered index */
static
dict_index_t*
dict_index_build_internal_non_clust(
/*================================*/
	const dict_table_t*	table,	/*!< in: table */
	dict_index_t*		index);	/*!< in: user representation of
					a non-clustered index */
/**********************************************************************//**
Builds the internal dictionary cache representation for an FTS index.
@return	own: the internal representation of the FTS index */
static
dict_index_t*
dict_index_build_internal_fts(
/*==========================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index);	/*!< in: user representation of an FTS index */
/**********************************************************************//**
Prints a column data. */
static
void
dict_col_print_low(
/*===============*/
	const dict_table_t*	table,	/*!< in: table */
	const dict_col_t*	col);	/*!< in: column */
/**********************************************************************//**
Prints an index data. */
static
void
dict_index_print_low(
/*=================*/
	dict_index_t*	index);	/*!< in: index */
/**********************************************************************//**
Prints a field data. */
static
void
dict_field_print_low(
/*=================*/
	const dict_field_t*	field);	/*!< in: field */

/**********************************************************************//**
Removes an index from the dictionary cache. */
static
void
dict_index_remove_from_cache_low(
/*=============================*/
	dict_table_t*	table,		/*!< in/out: table */
	dict_index_t*	index,		/*!< in, own: index */
	ibool		lru_evict);	/*!< in: TRUE if page being evicted
					to make room in the table LRU list */
/**********************************************************************//**
Removes a table object from the dictionary cache. */
static
void
dict_table_remove_from_cache_low(
/*=============================*/
	dict_table_t*	table,		/*!< in, own: table */
	ibool		lru_evict);	/*!< in: TRUE if evicting from LRU */
#ifdef UNIV_DEBUG
/**********************************************************************//**
Validate the dictionary table LRU list.
@return TRUE if validate OK */
static
ibool
dict_lru_validate(void);
/*===================*/
/**********************************************************************//**
Check if table is in the dictionary table LRU list.
@return TRUE if table found */
static
ibool
dict_lru_find_table(
/*================*/
	const dict_table_t*	find_table);	/*!< in: table to find */
/**********************************************************************//**
Check if a table exists in the dict table non-LRU list.
@return TRUE if table found */
static
ibool
dict_non_lru_find_table(
/*====================*/
	const dict_table_t*	find_table);	/*!< in: table to find */
#endif /* UNIV_DEBUG */

/* Stream for storing detailed information about the latest foreign key
and unique key errors. Only created if !srv_read_only_mode */
UNIV_INTERN FILE*	dict_foreign_err_file		= NULL;
/* mutex protecting the foreign and unique error buffers */
UNIV_INTERN ib_mutex_t	dict_foreign_err_mutex;

/******************************************************************//**
Makes all characters in a NUL-terminated UTF-8 string lower case. */
UNIV_INTERN
void
dict_casedn_str(
/*============*/
	char*	a)	/*!< in/out: string to put in lower case */
{
	innobase_casedn_str(a);
}

/********************************************************************//**
Checks if the database name in two table names is the same.
@return	TRUE if same db name */
UNIV_INTERN
ibool
dict_tables_have_same_db(
/*=====================*/
	const char*	name1,	/*!< in: table name in the form
				dbname '/' tablename */
	const char*	name2)	/*!< in: table name in the form
				dbname '/' tablename */
{
	for (; *name1 == *name2; name1++, name2++) {
		if (*name1 == '/') {
			return(TRUE);
		}
		ut_a(*name1); /* the names must contain '/' */
	}
	return(FALSE);
}

/********************************************************************//**
Return the end of table name where we have removed dbname and '/'.
@return	table name */
UNIV_INTERN
const char*
dict_remove_db_name(
/*================*/
	const char*	name)	/*!< in: table name in the form
				dbname '/' tablename */
{
	const char*	s = strchr(name, '/');
	ut_a(s);

	return(s + 1);
}

/********************************************************************//**
Get the database name length in a table name.
@return	database name length */
UNIV_INTERN
ulint
dict_get_db_name_len(
/*=================*/
	const char*	name)	/*!< in: table name in the form
				dbname '/' tablename */
{
	const char*	s;
	s = strchr(name, '/');
	ut_a(s);
	return(s - name);
}

/********************************************************************//**
Reserves the dictionary system mutex for MySQL. */
UNIV_INTERN
void
dict_mutex_enter_for_mysql(void)
/*============================*/
{
	mutex_enter(&(dict_sys->mutex));
}

/********************************************************************//**
Releases the dictionary system mutex for MySQL. */
UNIV_INTERN
void
dict_mutex_exit_for_mysql(void)
/*===========================*/
{
	mutex_exit(&(dict_sys->mutex));
}

/** Get the latch that protects the stats of a given table */
#define GET_TABLE_STATS_LATCH(table) \
	(&dict_table_stats_latches[ut_fold_ull((ib_uint64_t) table) \
				   % DICT_TABLE_STATS_LATCHES_SIZE])

/**********************************************************************//**
Lock the appropriate latch to protect a given table's statistics.
table->id is used to pick the corresponding latch from a global array of
latches. */
UNIV_INTERN
void
dict_table_stats_lock(
/*==================*/
	const dict_table_t*	table,		/*!< in: table */
	ulint			latch_mode)	/*!< in: RW_S_LATCH or
						RW_X_LATCH */
{
	ut_ad(table != NULL);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	switch (latch_mode) {
	case RW_S_LATCH:
		rw_lock_s_lock(GET_TABLE_STATS_LATCH(table));
		break;
	case RW_X_LATCH:
		rw_lock_x_lock(GET_TABLE_STATS_LATCH(table));
		break;
	case RW_NO_LATCH:
		/* fall through */
	default:
		ut_error;
	}
}

/**********************************************************************//**
Unlock the latch that has been locked by dict_table_stats_lock() */
UNIV_INTERN
void
dict_table_stats_unlock(
/*====================*/
	const dict_table_t*	table,		/*!< in: table */
	ulint			latch_mode)	/*!< in: RW_S_LATCH or
						RW_X_LATCH */
{
	ut_ad(table != NULL);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	switch (latch_mode) {
	case RW_S_LATCH:
		rw_lock_s_unlock(GET_TABLE_STATS_LATCH(table));
		break;
	case RW_X_LATCH:
		rw_lock_x_unlock(GET_TABLE_STATS_LATCH(table));
		break;
	case RW_NO_LATCH:
		/* fall through */
	default:
		ut_error;
	}
}

/**********************************************************************//**
Try to drop any indexes after an aborted index creation.
This can also be after a server kill during DROP INDEX. */
static
void
dict_table_try_drop_aborted(
/*========================*/
	dict_table_t*	table,		/*!< in: table, or NULL if it
					needs to be looked up again */
	table_id_t	table_id,	/*!< in: table identifier */
	ulint		ref_count)	/*!< in: expected table->n_ref_count */
{
	trx_t*		trx;

	trx = trx_allocate_for_background();
	trx->op_info = "try to drop any indexes after an aborted index creation";
	row_mysql_lock_data_dictionary(trx);
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	if (table == NULL) {
		table = dict_table_open_on_id_low(table_id);
	} else {
		ut_ad(table->id == table_id);
	}

	if (table && table->n_ref_count == ref_count && table->drop_aborted) {
		/* Silence a debug assertion in row_merge_drop_indexes(). */
		ut_d(table->n_ref_count++);
		row_merge_drop_indexes(trx, table, TRUE);
		ut_d(table->n_ref_count--);
		ut_ad(table->n_ref_count == ref_count);
		trx_commit_for_mysql(trx);
	}

	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_background(trx);
}

/**********************************************************************//**
When opening a table,
try to drop any indexes after an aborted index creation.
Release the dict_sys->mutex. */
static
void
dict_table_try_drop_aborted_and_mutex_exit(
/*=======================================*/
	dict_table_t*	table,		/*!< in: table (may be NULL) */
	ibool		try_drop)	/*!< in: FALSE if should try to
					drop indexes whose online creation
					was aborted */
{
	if (try_drop
	    && table != NULL
	    && table->drop_aborted
	    && table->n_ref_count == 1
	    && dict_table_get_first_index(table)) {

		/* Attempt to drop the indexes whose online creation
		was aborted. */
		table_id_t	table_id = table->id;

		mutex_exit(&dict_sys->mutex);

		dict_table_try_drop_aborted(table, table_id, 1);
	} else {
		mutex_exit(&dict_sys->mutex);
	}
}

/********************************************************************//**
Decrements the count of open handles to a table. */
UNIV_INTERN
void
dict_table_close(
/*=============*/
	dict_table_t*	table,		/*!< in/out: table */
	ibool		dict_locked,	/*!< in: TRUE=data dictionary locked */
	ibool		try_drop)	/*!< in: TRUE=try to drop any orphan
					indexes after an aborted online
					index creation */
{
	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(table->n_ref_count > 0);

	--table->n_ref_count;

	/* Force persistent stats re-read upon next open of the table
	so that FLUSH TABLE can be used to forcibly fetch stats from disk
	if they have been manually modified. We reset table->stat_initialized
	only if table reference count is 0 because we do not want too frequent
	stats re-reads (e.g. in other cases than FLUSH TABLE). */
	if (strchr(table->name, '/') != NULL
	    && table->n_ref_count == 0
	    && dict_stats_is_persistent_enabled(table)) {

		dict_stats_deinit(table);
	}

	MONITOR_DEC(MONITOR_TABLE_REFERENCE);

	ut_ad(dict_lru_validate());

#ifdef UNIV_DEBUG
	if (table->can_be_evicted) {
		ut_ad(dict_lru_find_table(table));
	} else {
		ut_ad(dict_non_lru_find_table(table));
	}
#endif /* UNIV_DEBUG */

	if (!dict_locked) {
		table_id_t	table_id	= table->id;
		ibool		drop_aborted;

		drop_aborted = try_drop
			&& table->drop_aborted
			&& table->n_ref_count == 1
			&& dict_table_get_first_index(table);

		mutex_exit(&dict_sys->mutex);

		if (drop_aborted) {
			dict_table_try_drop_aborted(NULL, table_id, 0);
		}
	}
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Returns a column's name.
@return column name. NOTE: not guaranteed to stay valid if table is
modified in any way (columns added, etc.). */
UNIV_INTERN
const char*
dict_table_get_col_name(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			col_nr)	/*!< in: column number */
{
	ulint		i;
	const char*	s;

	ut_ad(table);
	ut_ad(col_nr < table->n_def);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	s = table->col_names;
	if (s) {
		for (i = 0; i < col_nr; i++) {
			s += strlen(s) + 1;
		}
	}

	return(s);
}

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Acquire the autoinc lock. */
UNIV_INTERN
void
dict_table_autoinc_lock(
/*====================*/
	dict_table_t*	table)	/*!< in/out: table */
{
	mutex_enter(&table->autoinc_mutex);
}

/********************************************************************//**
Unconditionally set the autoinc counter. */
UNIV_INTERN
void
dict_table_autoinc_initialize(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	ib_uint64_t	value)	/*!< in: next value to assign to a row */
{
	ut_ad(mutex_own(&table->autoinc_mutex));

	table->autoinc = value;
}

/************************************************************************
Get all the FTS indexes on a table.
@return	number of FTS indexes */
UNIV_INTERN
ulint
dict_table_get_all_fts_indexes(
/*===========================*/
	dict_table_t*   table,          /*!< in: table */
	ib_vector_t*    indexes)        /*!< out: all FTS indexes on this
					table */
{
	dict_index_t* index;

	ut_a(ib_vector_size(indexes) == 0);

	for (index = dict_table_get_first_index(table);
	     index;
	     index = dict_table_get_next_index(index)) {

		if (index->type == DICT_FTS) {
			ib_vector_push(indexes, &index);
		}
	}

	return(ib_vector_size(indexes));
}

/********************************************************************//**
Reads the next autoinc value (== autoinc counter value), 0 if not yet
initialized.
@return	value for a new row, or 0 */
UNIV_INTERN
ib_uint64_t
dict_table_autoinc_read(
/*====================*/
	const dict_table_t*	table)	/*!< in: table */
{
	ut_ad(mutex_own(&table->autoinc_mutex));

	return(table->autoinc);
}

/********************************************************************//**
Updates the autoinc counter if the value supplied is greater than the
current value. */
UNIV_INTERN
void
dict_table_autoinc_update_if_greater(
/*=================================*/

	dict_table_t*	table,	/*!< in/out: table */
	ib_uint64_t	value)	/*!< in: value which was assigned to a row */
{
	ut_ad(mutex_own(&table->autoinc_mutex));

	if (value > table->autoinc) {

		table->autoinc = value;
	}
}

/********************************************************************//**
Release the autoinc lock. */
UNIV_INTERN
void
dict_table_autoinc_unlock(
/*======================*/
	dict_table_t*	table)	/*!< in/out: table */
{
	mutex_exit(&table->autoinc_mutex);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************************//**
Looks for column n in an index.
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
UNIV_INTERN
ulint
dict_index_get_nth_col_or_prefix_pos(
/*=================================*/
	const dict_index_t*	index,		/*!< in: index */
	ulint			n,		/*!< in: column number */
	ibool			inc_prefix)	/*!< in: TRUE=consider
						column prefixes too */
{
	const dict_field_t*	field;
	const dict_col_t*	col;
	ulint			pos;
	ulint			n_fields;

	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

	col = dict_table_get_nth_col(index->table, n);

	if (dict_index_is_clust(index)) {

		return(dict_col_get_clust_pos(col, index));
	}

	n_fields = dict_index_get_n_fields(index);

	for (pos = 0; pos < n_fields; pos++) {
		field = dict_index_get_nth_field(index, pos);

		if (col == field->col
		    && (inc_prefix || field->prefix_len == 0)) {

			return(pos);
		}
	}

	return(ULINT_UNDEFINED);
}

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Returns TRUE if the index contains a column or a prefix of that column.
@return	TRUE if contains the column or its prefix */
UNIV_INTERN
ibool
dict_index_contains_col_or_prefix(
/*==============================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			n)	/*!< in: column number */
{
	const dict_field_t*	field;
	const dict_col_t*	col;
	ulint			pos;
	ulint			n_fields;

	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

	if (dict_index_is_clust(index)) {

		return(TRUE);
	}

	col = dict_table_get_nth_col(index->table, n);

	n_fields = dict_index_get_n_fields(index);

	for (pos = 0; pos < n_fields; pos++) {
		field = dict_index_get_nth_field(index, pos);

		if (col == field->col) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************//**
Looks for a matching field in an index. The column has to be the same. The
column in index must be complete, or must contain a prefix longer than the
column in index2. That is, we must be able to construct the prefix in index2
from the prefix in index.
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
UNIV_INTERN
ulint
dict_index_get_nth_field_pos(
/*=========================*/
	const dict_index_t*	index,	/*!< in: index from which to search */
	const dict_index_t*	index2,	/*!< in: index */
	ulint			n)	/*!< in: field number in index2 */
{
	const dict_field_t*	field;
	const dict_field_t*	field2;
	ulint			n_fields;
	ulint			pos;

	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

	field2 = dict_index_get_nth_field(index2, n);

	n_fields = dict_index_get_n_fields(index);

	for (pos = 0; pos < n_fields; pos++) {
		field = dict_index_get_nth_field(index, pos);

		if (field->col == field2->col
		    && (field->prefix_len == 0
			|| (field->prefix_len >= field2->prefix_len
			    && field2->prefix_len != 0))) {

			return(pos);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Returns a table object based on table id.
@return	table, NULL if does not exist */
UNIV_INTERN
dict_table_t*
dict_table_open_on_id(
/*==================*/
	table_id_t	table_id,	/*!< in: table id */
	ibool		dict_locked,	/*!< in: TRUE=data dictionary locked */
	ibool		try_drop)	/*!< in: TRUE=try to drop any orphan
					indexes after an aborted online
					index creation */
{
	dict_table_t*	table;

	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(mutex_own(&dict_sys->mutex));

	table = dict_table_open_on_id_low(table_id);

	if (table != NULL) {

		if (table->can_be_evicted) {
			dict_move_to_mru(table);
		}

		++table->n_ref_count;

		MONITOR_INC(MONITOR_TABLE_REFERENCE);
	}

	if (!dict_locked) {
		dict_table_try_drop_aborted_and_mutex_exit(table, try_drop);
	}

	return(table);
}

/********************************************************************//**
Looks for column n position in the clustered index.
@return	position in internal representation of the clustered index */
UNIV_INTERN
ulint
dict_table_get_nth_col_pos(
/*=======================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			n)	/*!< in: column number */
{
	return(dict_index_get_nth_col_pos(dict_table_get_first_index(table),
					  n));
}

/********************************************************************//**
Checks if a column is in the ordering columns of the clustered index of a
table. Column prefixes are treated like whole columns.
@return	TRUE if the column, or its prefix, is in the clustered key */
UNIV_INTERN
ibool
dict_table_col_in_clustered_key(
/*============================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			n)	/*!< in: column number */
{
	const dict_index_t*	index;
	const dict_field_t*	field;
	const dict_col_t*	col;
	ulint			pos;
	ulint			n_fields;

	ut_ad(table);

	col = dict_table_get_nth_col(table, n);

	index = dict_table_get_first_index(table);

	n_fields = dict_index_get_n_unique(index);

	for (pos = 0; pos < n_fields; pos++) {
		field = dict_index_get_nth_field(index, pos);

		if (col == field->col) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/**********************************************************************//**
Inits the data dictionary module. */
UNIV_INTERN
void
dict_init(void)
/*===========*/
{
	int	i;

	dict_sys = static_cast<dict_sys_t*>(mem_zalloc(sizeof(*dict_sys)));

	mutex_create(dict_sys_mutex_key, &dict_sys->mutex, SYNC_DICT);

	dict_sys->table_hash = hash_create(buf_pool_get_curr_size()
					   / (DICT_POOL_PER_TABLE_HASH
					      * UNIV_WORD_SIZE));
	dict_sys->table_id_hash = hash_create(buf_pool_get_curr_size()
					      / (DICT_POOL_PER_TABLE_HASH
						 * UNIV_WORD_SIZE));
	rw_lock_create(dict_operation_lock_key,
		       &dict_operation_lock, SYNC_DICT_OPERATION);

	if (!srv_read_only_mode) {
		dict_foreign_err_file = os_file_create_tmpfile();
		ut_a(dict_foreign_err_file);

		mutex_create(dict_foreign_err_mutex_key,
			     &dict_foreign_err_mutex, SYNC_NO_ORDER_CHECK);
	}

	for (i = 0; i < DICT_TABLE_STATS_LATCHES_SIZE; i++) {
		rw_lock_create(dict_table_stats_latch_key,
			       &dict_table_stats_latches[i], SYNC_INDEX_TREE);
	}
}

/**********************************************************************//**
Move to the most recently used segment of the LRU list. */
UNIV_INTERN
void
dict_move_to_mru(
/*=============*/
	dict_table_t*	table)		/*!< in: table to move to MRU */
{
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(dict_lru_validate());
	ut_ad(dict_lru_find_table(table));

	ut_a(table->can_be_evicted);

	UT_LIST_REMOVE(table_LRU, dict_sys->table_LRU, table);

	UT_LIST_ADD_FIRST(table_LRU, dict_sys->table_LRU, table);

	ut_ad(dict_lru_validate());
}

/**********************************************************************//**
Returns a table object and increment its open handle count.
NOTE! This is a high-level function to be used mainly from outside the
'dict' module. Inside this directory dict_table_get_low
is usually the appropriate function.
@return	table, NULL if does not exist */
UNIV_INTERN
dict_table_t*
dict_table_open_on_name(
/*====================*/
	const char*	table_name,	/*!< in: table name */
	ibool		dict_locked,	/*!< in: TRUE=data dictionary locked */
	ibool		try_drop,	/*!< in: TRUE=try to drop any orphan
					indexes after an aborted online
					index creation */
	dict_err_ignore_t
			ignore_err)	/*!< in: error to be ignored when
					loading a table definition */
{
	dict_table_t*	table;

	if (!dict_locked) {
		mutex_enter(&(dict_sys->mutex));
	}

	ut_ad(table_name);
	ut_ad(mutex_own(&dict_sys->mutex));

	table = dict_table_check_if_in_cache_low(table_name);

	if (table == NULL) {
		table = dict_load_table(table_name, TRUE, ignore_err);
	}

	ut_ad(!table || table->cached);

	if (table != NULL) {

		/* If table is corrupted, return NULL */
		if (ignore_err == DICT_ERR_IGNORE_NONE
		    && table->corrupted) {

			/* Make life easy for drop table. */
			if (table->can_be_evicted) {
				dict_table_move_from_lru_to_non_lru(table);
			}

			if (!dict_locked) {
				mutex_exit(&dict_sys->mutex);
			}

			ut_print_timestamp(stderr);

			fprintf(stderr, "  InnoDB: table ");
			ut_print_name(stderr, NULL, TRUE, table->name);
			fprintf(stderr, "is corrupted. Please drop the table "
				"and recreate\n");

			return(NULL);
		}

		if (table->can_be_evicted) {
			dict_move_to_mru(table);
		}

		++table->n_ref_count;

		MONITOR_INC(MONITOR_TABLE_REFERENCE);
	}

	ut_ad(dict_lru_validate());

	if (!dict_locked) {
		dict_table_try_drop_aborted_and_mutex_exit(table, try_drop);
	}

	return(table);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Adds system columns to a table object. */
UNIV_INTERN
void
dict_table_add_system_columns(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap)	/*!< in: temporary heap */
{
	ut_ad(table);
	ut_ad(table->n_def == table->n_cols - DATA_N_SYS_COLS);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(!table->cached);

	/* NOTE: the system columns MUST be added in the following order
	(so that they can be indexed by the numerical value of DATA_ROW_ID,
	etc.) and as the last columns of the table memory object.
	The clustered index will not always physically contain all
	system columns. */

	dict_mem_table_add_col(table, heap, "DB_ROW_ID", DATA_SYS,
			       DATA_ROW_ID | DATA_NOT_NULL,
			       DATA_ROW_ID_LEN);
#if DATA_ROW_ID != 0
#error "DATA_ROW_ID != 0"
#endif
	dict_mem_table_add_col(table, heap, "DB_TRX_ID", DATA_SYS,
			       DATA_TRX_ID | DATA_NOT_NULL,
			       DATA_TRX_ID_LEN);
#if DATA_TRX_ID != 1
#error "DATA_TRX_ID != 1"
#endif
	dict_mem_table_add_col(table, heap, "DB_ROLL_PTR", DATA_SYS,
			       DATA_ROLL_PTR | DATA_NOT_NULL,
			       DATA_ROLL_PTR_LEN);
#if DATA_ROLL_PTR != 2
#error "DATA_ROLL_PTR != 2"
#endif

	/* This check reminds that if a new system column is added to
	the program, it should be dealt with here */
#if DATA_N_SYS_COLS != 3
#error "DATA_N_SYS_COLS != 3"
#endif
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Adds a table object to the dictionary cache. */
UNIV_INTERN
void
dict_table_add_to_cache(
/*====================*/
	dict_table_t*	table,		/*!< in: table */
	ibool		can_be_evicted,	/*!< in: TRUE if can be evicted */
	mem_heap_t*	heap)		/*!< in: temporary heap */
{
	ulint	fold;
	ulint	id_fold;
	ulint	i;
	ulint	row_len;

	ut_ad(dict_lru_validate());

	/* The lower limit for what we consider a "big" row */
#define BIG_ROW_SIZE 1024

	ut_ad(mutex_own(&(dict_sys->mutex)));

	dict_table_add_system_columns(table, heap);

	table->cached = TRUE;

	fold = ut_fold_string(table->name);
	id_fold = ut_fold_ull(table->id);

	row_len = 0;
	for (i = 0; i < table->n_def; i++) {
		ulint	col_len = dict_col_get_max_size(
			dict_table_get_nth_col(table, i));

		row_len += col_len;

		/* If we have a single unbounded field, or several gigantic
		fields, mark the maximum row size as BIG_ROW_SIZE. */
		if (row_len >= BIG_ROW_SIZE || col_len >= BIG_ROW_SIZE) {
			row_len = BIG_ROW_SIZE;

			break;
		}
	}

	table->big_rows = row_len >= BIG_ROW_SIZE;

	/* Look for a table with the same name: error if such exists */
	{
		dict_table_t*	table2;
		HASH_SEARCH(name_hash, dict_sys->table_hash, fold,
			    dict_table_t*, table2, ut_ad(table2->cached),
			    ut_strcmp(table2->name, table->name) == 0);
		ut_a(table2 == NULL);

#ifdef UNIV_DEBUG
		/* Look for the same table pointer with a different name */
		HASH_SEARCH_ALL(name_hash, dict_sys->table_hash,
				dict_table_t*, table2, ut_ad(table2->cached),
				table2 == table);
		ut_ad(table2 == NULL);
#endif /* UNIV_DEBUG */
	}

	/* Look for a table with the same id: error if such exists */
	{
		dict_table_t*	table2;
		HASH_SEARCH(id_hash, dict_sys->table_id_hash, id_fold,
			    dict_table_t*, table2, ut_ad(table2->cached),
			    table2->id == table->id);
		ut_a(table2 == NULL);

#ifdef UNIV_DEBUG
		/* Look for the same table pointer with a different id */
		HASH_SEARCH_ALL(id_hash, dict_sys->table_id_hash,
				dict_table_t*, table2, ut_ad(table2->cached),
				table2 == table);
		ut_ad(table2 == NULL);
#endif /* UNIV_DEBUG */
	}

	/* Add table to hash table of tables */
	HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, fold,
		    table);

	/* Add table to hash table of tables based on table id */
	HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash, id_fold,
		    table);

	table->can_be_evicted = can_be_evicted;

	if (table->can_be_evicted) {
		UT_LIST_ADD_FIRST(table_LRU, dict_sys->table_LRU, table);
	} else {
		UT_LIST_ADD_FIRST(table_LRU, dict_sys->table_non_LRU, table);
	}

	ut_ad(dict_lru_validate());

	dict_sys->size += mem_heap_get_size(table->heap)
		+ strlen(table->name) + 1;
}

/**********************************************************************//**
Test whether a table can be evicted from the LRU cache.
@return TRUE if table can be evicted. */
static
ibool
dict_table_can_be_evicted(
/*======================*/
	const dict_table_t*	table)		/*!< in: table to test */
{
	ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(table->can_be_evicted);
	ut_a(UT_LIST_GET_LEN(table->foreign_list) == 0);
	ut_a(UT_LIST_GET_LEN(table->referenced_list) == 0);

	if (table->n_ref_count == 0) {
		dict_index_t*	index;

		/* The transaction commit and rollback are called from
		outside the handler interface. This means that there is
		a window where the table->n_ref_count can be zero but
		the table instance is in "use". */

		if (lock_table_has_locks(table)) {
			return(FALSE);
		}

		for (index = dict_table_get_first_index(table);
		     index != NULL;
		     index = dict_table_get_next_index(index)) {

			btr_search_t*	info = btr_search_get_info(index);

			/* We are not allowed to free the in-memory index
			struct dict_index_t until all entries in the adaptive
			hash index that point to any of the page belonging to
			his b-tree index are dropped. This is so because
			dropping of these entries require access to
			dict_index_t struct. To avoid such scenario we keep
			a count of number of such pages in the search_info and
			only free the dict_index_t struct when this count
			drops to zero.

			See also: dict_index_remove_from_cache_low() */

			if (btr_search_info_get_ref_count(info) > 0) {
				return(FALSE);
			}
		}

		return(TRUE);
	}

	return(FALSE);
}

/**********************************************************************//**
Make room in the table cache by evicting an unused table. The unused table
should not be part of FK relationship and currently not used in any user
transaction. There is no guarantee that it will remove a table.
@return number of tables evicted. If the number of tables in the dict_LRU
is less than max_tables it will not do anything. */
UNIV_INTERN
ulint
dict_make_room_in_cache(
/*====================*/
	ulint		max_tables,	/*!< in: max tables allowed in cache */
	ulint		pct_check)	/*!< in: max percent to check */
{
	ulint		i;
	ulint		len;
	dict_table_t*	table;
	ulint		check_up_to;
	ulint		n_evicted = 0;

	ut_a(pct_check > 0);
	ut_a(pct_check <= 100);
	ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(dict_lru_validate());

	i = len = UT_LIST_GET_LEN(dict_sys->table_LRU);

	if (len < max_tables) {
		return(0);
	}

	check_up_to = len - ((len * pct_check) / 100);

	/* Check for overflow */
	ut_a(i == 0 || check_up_to <= i);

	/* Find a suitable candidate to evict from the cache. Don't scan the
	entire LRU list. Only scan pct_check list entries. */

	for (table = UT_LIST_GET_LAST(dict_sys->table_LRU);
	     table != NULL
	     && i > check_up_to
	     && (len - n_evicted) > max_tables;
	     --i) {

		dict_table_t*	prev_table;

	        prev_table = UT_LIST_GET_PREV(table_LRU, table);

		if (dict_table_can_be_evicted(table)) {

			dict_table_remove_from_cache_low(table, TRUE);

			++n_evicted;
		}

		table = prev_table;
	}

	return(n_evicted);
}

/**********************************************************************//**
Move a table to the non-LRU list from the LRU list. */
UNIV_INTERN
void
dict_table_move_from_lru_to_non_lru(
/*================================*/
	dict_table_t*	table)	/*!< in: table to move from LRU to non-LRU */
{
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(dict_lru_find_table(table));

	ut_a(table->can_be_evicted);

	UT_LIST_REMOVE(table_LRU, dict_sys->table_LRU, table);

	UT_LIST_ADD_LAST(table_LRU, dict_sys->table_non_LRU, table);

	table->can_be_evicted = FALSE;
}

/**********************************************************************//**
Move a table to the LRU list from the non-LRU list. */
UNIV_INTERN
void
dict_table_move_from_non_lru_to_lru(
/*================================*/
	dict_table_t*	table)	/*!< in: table to move from non-LRU to LRU */
{
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(dict_non_lru_find_table(table));

	ut_a(!table->can_be_evicted);

	UT_LIST_REMOVE(table_LRU, dict_sys->table_non_LRU, table);

	UT_LIST_ADD_LAST(table_LRU, dict_sys->table_LRU, table);

	table->can_be_evicted = TRUE;
}

/**********************************************************************//**
Looks for an index with the given id given a table instance.
@return	index or NULL */
static
dict_index_t*
dict_table_find_index_on_id(
/*========================*/
	const dict_table_t*	table,	/*!< in: table instance */
	index_id_t		id)	/*!< in: index id */
{
	dict_index_t*	index;

	for (index = dict_table_get_first_index(table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {

		if (id == index->id) {
			/* Found */

			return(index);
		}
	}

	return(NULL);
}

/**********************************************************************//**
Looks for an index with the given id. NOTE that we do not reserve
the dictionary mutex: this function is for emergency purposes like
printing info of a corrupt database page!
@return	index or NULL if not found in cache */
UNIV_INTERN
dict_index_t*
dict_index_find_on_id_low(
/*======================*/
	index_id_t	id)	/*!< in: index id */
{
	dict_table_t*	table;

	/* This can happen if the system tablespace is the wrong page size */
	if (dict_sys == NULL) {
		return(NULL);
	}

	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		dict_index_t*	index = dict_table_find_index_on_id(table, id);

		if (index != NULL) {
			return(index);
		}
	}

	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		dict_index_t*	index = dict_table_find_index_on_id(table, id);

		if (index != NULL) {
			return(index);
		}
	}

	return(NULL);
}

/**********************************************************************//**
Renames a table object.
@return	TRUE if success */
UNIV_INTERN
dberr_t
dict_table_rename_in_cache(
/*=======================*/
	dict_table_t*	table,		/*!< in/out: table */
	const char*	new_name,	/*!< in: new name */
	ibool		rename_also_foreigns)/*!< in: in ALTER TABLE we want
					to preserve the original table name
					in constraints which reference it */
{
	dict_foreign_t*	foreign;
	dict_index_t*	index;
	ulint		fold;
	char		old_name[MAX_FULL_NAME_LEN + 1];

	ut_ad(table);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	/* store the old/current name to an automatic variable */
	if (strlen(table->name) + 1 <= sizeof(old_name)) {
		memcpy(old_name, table->name, strlen(table->name) + 1);
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr, "InnoDB: too long table name: '%s', "
			"max length is %d\n", table->name,
			MAX_FULL_NAME_LEN);
		ut_error;
	}

	fold = ut_fold_string(new_name);

	/* Look for a table with the same name: error if such exists */
	dict_table_t*	table2;
	HASH_SEARCH(name_hash, dict_sys->table_hash, fold,
			dict_table_t*, table2, ut_ad(table2->cached),
			(ut_strcmp(table2->name, new_name) == 0));
	DBUG_EXECUTE_IF("dict_table_rename_in_cache_failure",
		if (table2 == NULL) {
			table2 = (dict_table_t*) -1;
		} );
	if (table2) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot rename table '%s' to '%s' since the "
			"dictionary cache already contains '%s'.",
			old_name, new_name, new_name);
		return(DB_ERROR);
	}

	/* If the table is stored in a single-table tablespace, rename the
	.ibd file and rebuild the .isl file if needed. */

	if (table->space != TRX_SYS_SPACE
	    && !dict_table_is_discarded(table)
	    && !table->ibd_file_missing) {
		ibool	success;
		char*	new_path = NULL;
		if (table->dir_path_of_temp_table != NULL) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: trying to rename a"
			      " TEMPORARY TABLE ", stderr);
			ut_print_name(stderr, NULL, TRUE, old_name);
			fputs(" (", stderr);
			ut_print_filename(stderr,
					  table->dir_path_of_temp_table);
			fputs(" )\n", stderr);
			return(DB_ERROR);
		}

		if (DICT_TF_HAS_DATA_DIR(table->flags)) {
			char*		old_path;

			old_path = fil_space_get_first_path(table->space);

			new_path = os_file_make_new_pathname(
				old_path, new_name);
			mem_free(old_path);

			dberr_t	err = fil_create_link_file(
				new_name, new_path);

			if (err != DB_SUCCESS) {
				mem_free(new_path);
				return(DB_TABLESPACE_EXISTS);
			}
		}

		success = fil_rename_tablespace(
			old_name, table->space, new_name, new_path);

		/* If the tablespace is remote, a new .isl file was created
		If success, delete the old one. If not, delete the new one.  */
		if (new_path) {
			mem_free(new_path);
			if (success) {
				fil_delete_link_file(old_name);
			} else {
				fil_delete_link_file(new_name);
			}
		}

		if (!success) {
			return(DB_ERROR);
		}
	}

	/* Remove table from the hash tables of tables */
	HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
		    ut_fold_string(old_name), table);

	if (strlen(new_name) > strlen(table->name)) {
		/* We allocate MAX_FULL_NAME_LEN + 1 bytes here to avoid
		memory fragmentation, we assume a repeated calls of
		ut_realloc() with the same size do not cause fragmentation */
		ut_a(strlen(new_name) <= MAX_FULL_NAME_LEN);

		table->name = static_cast<char*>(
			ut_realloc(table->name, MAX_FULL_NAME_LEN + 1));
	}
	memcpy(table->name, new_name, strlen(new_name) + 1);

	/* Add table to hash table of tables */
	HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, fold,
		    table);

	dict_sys->size += strlen(new_name) - strlen(old_name);
	ut_a(dict_sys->size > 0);

	/* Update the table_name field in indexes */
	index = dict_table_get_first_index(table);

	while (index != NULL) {
		index->table_name = table->name;

		index = dict_table_get_next_index(index);
	}

	if (!rename_also_foreigns) {
		/* In ALTER TABLE we think of the rename table operation
		in the direction table -> temporary table (#sql...)
		as dropping the table with the old name and creating
		a new with the new name. Thus we kind of drop the
		constraints from the dictionary cache here. The foreign key
		constraints will be inherited to the new table from the
		system tables through a call of dict_load_foreigns. */

		/* Remove the foreign constraints from the cache */
		foreign = UT_LIST_GET_LAST(table->foreign_list);

		while (foreign != NULL) {
			dict_foreign_remove_from_cache(foreign);
			foreign = UT_LIST_GET_LAST(table->foreign_list);
		}

		/* Reset table field in referencing constraints */

		foreign = UT_LIST_GET_FIRST(table->referenced_list);

		while (foreign != NULL) {
			foreign->referenced_table = NULL;
			foreign->referenced_index = NULL;

			foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
		}

		/* Make the list of referencing constraints empty */

		UT_LIST_INIT(table->referenced_list);

		return(DB_SUCCESS);
	}

	/* Update the table name fields in foreign constraints, and update also
	the constraint id of new format >= 4.0.18 constraints. Note that at
	this point we have already changed table->name to the new name. */

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign != NULL) {
		if (ut_strlen(foreign->foreign_table_name)
		    < ut_strlen(table->name)) {
			/* Allocate a longer name buffer;
			TODO: store buf len to save memory */

			foreign->foreign_table_name = mem_heap_strdup(
				foreign->heap, table->name);
			dict_mem_foreign_table_name_lookup_set(foreign, TRUE);
		} else {
			strcpy(foreign->foreign_table_name, table->name);
			dict_mem_foreign_table_name_lookup_set(foreign, FALSE);
		}
		if (strchr(foreign->id, '/')) {
			ulint	db_len;
			char*	old_id;

			/* This is a >= 4.0.18 format id */

			old_id = mem_strdup(foreign->id);

			if (ut_strlen(foreign->id) > ut_strlen(old_name)
			    + ((sizeof dict_ibfk) - 1)
			    && !memcmp(foreign->id, old_name,
				       ut_strlen(old_name))
			    && !memcmp(foreign->id + ut_strlen(old_name),
				       dict_ibfk, (sizeof dict_ibfk) - 1)) {

				/* This is a generated >= 4.0.18 format id */

				if (strlen(table->name) > strlen(old_name)) {
					foreign->id = static_cast<char*>(
						mem_heap_alloc(
						foreign->heap,
						strlen(table->name)
						+ strlen(old_id) + 1));
				}

				/* Replace the prefix 'databasename/tablename'
				with the new names */
				strcpy(foreign->id, table->name);
				strcat(foreign->id,
				       old_id + ut_strlen(old_name));
			} else {
				/* This is a >= 4.0.18 format id where the user
				gave the id name */
				db_len = dict_get_db_name_len(table->name) + 1;

				if (dict_get_db_name_len(table->name)
				    > dict_get_db_name_len(foreign->id)) {

					foreign->id = static_cast<char*>(
						mem_heap_alloc(
						foreign->heap,
						db_len + strlen(old_id) + 1));
				}

				/* Replace the database prefix in id with the
				one from table->name */

				ut_memcpy(foreign->id, table->name, db_len);

				strcpy(foreign->id + db_len,
				       dict_remove_db_name(old_id));
			}

			mem_free(old_id);
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign != NULL) {
		if (ut_strlen(foreign->referenced_table_name)
		    < ut_strlen(table->name)) {
			/* Allocate a longer name buffer;
			TODO: store buf len to save memory */

			foreign->referenced_table_name = mem_heap_strdup(
				foreign->heap, table->name);
			dict_mem_referenced_table_name_lookup_set(foreign, TRUE);
		} else {
			/* Use the same buffer */
			strcpy(foreign->referenced_table_name, table->name);
			dict_mem_referenced_table_name_lookup_set(foreign, FALSE);
		}
		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}

	return(DB_SUCCESS);
}

/**********************************************************************//**
Change the id of a table object in the dictionary cache. This is used in
DISCARD TABLESPACE. */
UNIV_INTERN
void
dict_table_change_id_in_cache(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table object already in cache */
	table_id_t	new_id)	/*!< in: new id to set */
{
	ut_ad(table);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* Remove the table from the hash table of id's */

	HASH_DELETE(dict_table_t, id_hash, dict_sys->table_id_hash,
		    ut_fold_ull(table->id), table);
	table->id = new_id;

	/* Add the table back to the hash table */
	HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash,
		    ut_fold_ull(table->id), table);
}

/**********************************************************************//**
Removes a table object from the dictionary cache. */
static
void
dict_table_remove_from_cache_low(
/*=============================*/
	dict_table_t*	table,		/*!< in, own: table */
	ibool		lru_evict)	/*!< in: TRUE if table being evicted
					to make room in the table LRU list */
{
	dict_foreign_t*	foreign;
	dict_index_t*	index;
	ulint		size;

	ut_ad(table);
	ut_ad(dict_lru_validate());
	ut_a(table->n_ref_count == 0);
	ut_a(table->n_rec_locks == 0);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

#if 0
	fputs("Removing table ", stderr);
	ut_print_name(stderr, table->name, ULINT_UNDEFINED);
	fputs(" from dictionary cache\n", stderr);
#endif

	/* Remove the foreign constraints from the cache */

	for (foreign = UT_LIST_GET_LAST(table->foreign_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_LAST(table->foreign_list)) {

		dict_foreign_remove_from_cache(foreign);
	}

	/* Reset table field in referencing constraints */

	for (foreign = UT_LIST_GET_FIRST(table->referenced_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

		foreign->referenced_table = NULL;
		foreign->referenced_index = NULL;
	}

	/* Remove the indexes from the cache */

	for (index = UT_LIST_GET_LAST(table->indexes);
	     index != NULL;
	     index = UT_LIST_GET_LAST(table->indexes)) {

		dict_index_remove_from_cache_low(table, index, lru_evict);
	}

	/* Remove table from the hash tables of tables */

	HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
		    ut_fold_string(table->name), table);

	HASH_DELETE(dict_table_t, id_hash, dict_sys->table_id_hash,
		    ut_fold_ull(table->id), table);

	/* Remove table from LRU or non-LRU list. */
	if (table->can_be_evicted) {
		ut_ad(dict_lru_find_table(table));
		UT_LIST_REMOVE(table_LRU, dict_sys->table_LRU, table);
	} else {
		ut_ad(dict_non_lru_find_table(table));
		UT_LIST_REMOVE(table_LRU, dict_sys->table_non_LRU, table);
	}

	ut_ad(dict_lru_validate());

	if (lru_evict && table->drop_aborted) {
		/* Do as dict_table_try_drop_aborted() does. */

		trx_t* trx = trx_allocate_for_background();

		ut_ad(mutex_own(&dict_sys->mutex));
#ifdef UNIV_SYNC_DEBUG
		ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
		/* Mimic row_mysql_lock_data_dictionary(). */
		trx->dict_operation_lock_mode = RW_X_LATCH;

		trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

		/* Silence a debug assertion in row_merge_drop_indexes(). */
		ut_d(table->n_ref_count++);
		row_merge_drop_indexes(trx, table, TRUE);
		ut_d(table->n_ref_count--);
		ut_ad(table->n_ref_count == 0);
		trx_commit_for_mysql(trx);
		trx->dict_operation_lock_mode = 0;
		trx_free_for_background(trx);
	}

	size = mem_heap_get_size(table->heap) + strlen(table->name) + 1;

	ut_ad(dict_sys->size >= size);

	dict_sys->size -= size;

	dict_mem_table_free(table);
}

/**********************************************************************//**
Removes a table object from the dictionary cache. */
UNIV_INTERN
void
dict_table_remove_from_cache(
/*=========================*/
	dict_table_t*	table)	/*!< in, own: table */
{
	dict_table_remove_from_cache_low(table, FALSE);
}

/****************************************************************//**
If the given column name is reserved for InnoDB system columns, return
TRUE.
@return	TRUE if name is reserved */
UNIV_INTERN
ibool
dict_col_name_is_reserved(
/*======================*/
	const char*	name)	/*!< in: column name */
{
	/* This check reminds that if a new system column is added to
	the program, it should be dealt with here. */
#if DATA_N_SYS_COLS != 3
#error "DATA_N_SYS_COLS != 3"
#endif

	static const char*	reserved_names[] = {
		"DB_ROW_ID", "DB_TRX_ID", "DB_ROLL_PTR"
	};

	ulint			i;

	for (i = 0; i < UT_ARR_SIZE(reserved_names); i++) {
		if (innobase_strcasecmp(name, reserved_names[i]) == 0) {

			return(TRUE);
		}
	}

	return(FALSE);
}

#if 1	/* This function is not very accurate at determining
	whether an UNDO record will be too big. See innodb_4k.test,
	Bug 13336585, for a testcase that shows an index that can
	be created but cannot be updated. */

/****************************************************************//**
If an undo log record for this table might not fit on a single page,
return TRUE.
@return	TRUE if the undo log record could become too big */
static
ibool
dict_index_too_big_for_undo(
/*========================*/
	const dict_table_t*	table,		/*!< in: table */
	const dict_index_t*	new_index)	/*!< in: index */
{
	/* Make sure that all column prefixes will fit in the undo log record
	in trx_undo_page_report_modify() right after trx_undo_page_init(). */

	ulint			i;
	const dict_index_t*	clust_index
		= dict_table_get_first_index(table);
	ulint			undo_page_len
		= TRX_UNDO_PAGE_HDR - TRX_UNDO_PAGE_HDR_SIZE
		+ 2 /* next record pointer */
		+ 1 /* type_cmpl */
		+ 11 /* trx->undo_no */ + 11 /* table->id */
		+ 1 /* rec_get_info_bits() */
		+ 11 /* DB_TRX_ID */
		+ 11 /* DB_ROLL_PTR */
		+ 10 + FIL_PAGE_DATA_END /* trx_undo_left() */
		+ 2/* pointer to previous undo log record */;

	/* FTS index consists of auxiliary tables, they shall be excluded from
	index row size check */
	if (new_index->type & DICT_FTS) {
		return(false);
	}

	if (!clust_index) {
		ut_a(dict_index_is_clust(new_index));
		clust_index = new_index;
	}

	/* Add the size of the ordering columns in the
	clustered index. */
	for (i = 0; i < clust_index->n_uniq; i++) {
		const dict_col_t*	col
			= dict_index_get_nth_col(clust_index, i);

		/* Use the maximum output size of
		mach_write_compressed(), although the encoded
		length should always fit in 2 bytes. */
		undo_page_len += 5 + dict_col_get_max_size(col);
	}

	/* Add the old values of the columns to be updated.
	First, the amount and the numbers of the columns.
	These are written by mach_write_compressed() whose
	maximum output length is 5 bytes.  However, given that
	the quantities are below REC_MAX_N_FIELDS (10 bits),
	the maximum length is 2 bytes per item. */
	undo_page_len += 2 * (dict_table_get_n_cols(table) + 1);

	for (i = 0; i < clust_index->n_def; i++) {
		const dict_col_t*	col
			= dict_index_get_nth_col(clust_index, i);
		ulint			max_size
			= dict_col_get_max_size(col);
		ulint			fixed_size
			= dict_col_get_fixed_size(col,
						  dict_table_is_comp(table));
		ulint			max_prefix
			= col->max_prefix;

		if (fixed_size) {
			/* Fixed-size columns are stored locally. */
			max_size = fixed_size;
		} else if (max_size <= BTR_EXTERN_FIELD_REF_SIZE * 2) {
			/* Short columns are stored locally. */
		} else if (!col->ord_part
			   || (col->max_prefix
			       < (ulint) DICT_MAX_FIELD_LEN_BY_FORMAT(table))) {
			/* See if col->ord_part would be set
			because of new_index. Also check if the new
			index could have longer prefix on columns
			that already had ord_part set  */
			ulint	j;

			for (j = 0; j < new_index->n_uniq; j++) {
				if (dict_index_get_nth_col(
					    new_index, j) == col) {
					const dict_field_t*     field
						= dict_index_get_nth_field(
							new_index, j);

					if (field->prefix_len
					    > col->max_prefix) {
						max_prefix =
							 field->prefix_len;
					}

					goto is_ord_part;
				}
			}

			if (col->ord_part) {
				goto is_ord_part;
			}

			/* This is not an ordering column in any index.
			Thus, it can be stored completely externally. */
			max_size = BTR_EXTERN_FIELD_REF_SIZE;
		} else {
			ulint	max_field_len;
is_ord_part:
			max_field_len = DICT_MAX_FIELD_LEN_BY_FORMAT(table);

			/* This is an ordering column in some index.
			A long enough prefix must be written to the
			undo log.  See trx_undo_page_fetch_ext(). */
			max_size = ut_min(max_size, max_field_len);

			/* We only store the needed prefix length in undo log */
			if (max_prefix) {
			     ut_ad(dict_table_get_format(table)
				   >= UNIV_FORMAT_B);

				max_size = ut_min(max_prefix, max_size);
			}

			max_size += BTR_EXTERN_FIELD_REF_SIZE;
		}

		undo_page_len += 5 + max_size;
	}

	return(undo_page_len >= UNIV_PAGE_SIZE);
}
#endif

/****************************************************************//**
If a record of this index might not fit on a single B-tree page,
return TRUE.
@return	TRUE if the index record could become too big */
static
ibool
dict_index_too_big_for_tree(
/*========================*/
	const dict_table_t*	table,		/*!< in: table */
	const dict_index_t*	new_index)	/*!< in: index */
{
	ulint	zip_size;
	ulint	comp;
	ulint	i;
	/* maximum possible storage size of a record */
	ulint	rec_max_size;
	/* maximum allowed size of a record on a leaf page */
	ulint	page_rec_max;
	/* maximum allowed size of a node pointer record */
	ulint	page_ptr_max;

	/* FTS index consists of auxiliary tables, they shall be excluded from
	index row size check */
	if (new_index->type & DICT_FTS) {
		return(false);
	}

	comp = dict_table_is_comp(table);
	zip_size = dict_table_zip_size(table);

	if (zip_size && zip_size < UNIV_PAGE_SIZE) {
		/* On a compressed page, two records must fit in the
		uncompressed page modification log.  On compressed
		pages with zip_size == UNIV_PAGE_SIZE, this limit will
		never be reached. */
		ut_ad(comp);
		/* The maximum allowed record size is the size of
		an empty page, minus a byte for recoding the heap
		number in the page modification log.  The maximum
		allowed node pointer size is half that. */
		page_rec_max = page_zip_empty_size(new_index->n_fields,
						   zip_size) - 1;
		page_ptr_max = page_rec_max / 2;
		/* On a compressed page, there is a two-byte entry in
		the dense page directory for every record.  But there
		is no record header. */
		rec_max_size = 2;
	} else {
		/* The maximum allowed record size is half a B-tree
		page.  No additional sparse page directory entry will
		be generated for the first few user records. */
		page_rec_max = page_get_free_space_of_empty(comp) / 2;
		page_ptr_max = page_rec_max;
		/* Each record has a header. */
		rec_max_size = comp
			? REC_N_NEW_EXTRA_BYTES
			: REC_N_OLD_EXTRA_BYTES;
	}

	if (comp) {
		/* Include the "null" flags in the
		maximum possible record size. */
		rec_max_size += UT_BITS_IN_BYTES(new_index->n_nullable);
	} else {
		/* For each column, include a 2-byte offset and a
		"null" flag.  The 1-byte format is only used in short
		records that do not contain externally stored columns.
		Such records could never exceed the page limit, even
		when using the 2-byte format. */
		rec_max_size += 2 * new_index->n_fields;
	}

	/* Compute the maximum possible record size. */
	for (i = 0; i < new_index->n_fields; i++) {
		const dict_field_t*	field
			= dict_index_get_nth_field(new_index, i);
		const dict_col_t*	col
			= dict_field_get_col(field);
		ulint			field_max_size;
		ulint			field_ext_max_size;

		/* In dtuple_convert_big_rec(), variable-length columns
		that are longer than BTR_EXTERN_FIELD_REF_SIZE * 2
		may be chosen for external storage.

		Fixed-length columns, and all columns of secondary
		index records are always stored inline. */

		/* Determine the maximum length of the index field.
		The field_ext_max_size should be computed as the worst
		case in rec_get_converted_size_comp() for
		REC_STATUS_ORDINARY records. */

		field_max_size = dict_col_get_fixed_size(col, comp);
		if (field_max_size) {
			/* dict_index_add_col() should guarantee this */
			ut_ad(!field->prefix_len
			      || field->fixed_len == field->prefix_len);
			/* Fixed lengths are not encoded
			in ROW_FORMAT=COMPACT. */
			field_ext_max_size = 0;
			goto add_field_size;
		}

		field_max_size = dict_col_get_max_size(col);
		field_ext_max_size = field_max_size < 256 ? 1 : 2;

		if (field->prefix_len) {
			if (field->prefix_len < field_max_size) {
				field_max_size = field->prefix_len;
			}
		} else if (field_max_size > BTR_EXTERN_FIELD_REF_SIZE * 2
			   && dict_index_is_clust(new_index)) {

			/* In the worst case, we have a locally stored
			column of BTR_EXTERN_FIELD_REF_SIZE * 2 bytes.
			The length can be stored in one byte.  If the
			column were stored externally, the lengths in
			the clustered index page would be
			BTR_EXTERN_FIELD_REF_SIZE and 2. */
			field_max_size = BTR_EXTERN_FIELD_REF_SIZE * 2;
			field_ext_max_size = 1;
		}

		if (comp) {
			/* Add the extra size for ROW_FORMAT=COMPACT.
			For ROW_FORMAT=REDUNDANT, these bytes were
			added to rec_max_size before this loop. */
			rec_max_size += field_ext_max_size;
		}
add_field_size:
		rec_max_size += field_max_size;

		/* Check the size limit on leaf pages. */
		if (UNIV_UNLIKELY(rec_max_size >= page_rec_max)) {

			return(TRUE);
		}

		/* Check the size limit on non-leaf pages.  Records
		stored in non-leaf B-tree pages consist of the unique
		columns of the record (the key columns of the B-tree)
		and a node pointer field.  When we have processed the
		unique columns, rec_max_size equals the size of the
		node pointer record minus the node pointer column. */
		if (i + 1 == dict_index_get_n_unique_in_tree(new_index)
		    && rec_max_size + REC_NODE_PTR_SIZE >= page_ptr_max) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/**********************************************************************//**
Adds an index to the dictionary cache.
@return	DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
UNIV_INTERN
dberr_t
dict_index_add_to_cache(
/*====================*/
	dict_table_t*	table,	/*!< in: table on which the index is */
	dict_index_t*	index,	/*!< in, own: index; NOTE! The index memory
				object is freed in this function! */
	ulint		page_no,/*!< in: root page number of the index */
	ibool		strict)	/*!< in: TRUE=refuse to create the index
				if records could be too big to fit in
				an B-tree page */
{
	dict_index_t*	new_index;
	ulint		n_ord;
	ulint		i;

	ut_ad(index);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(index->n_def == index->n_fields);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
	ut_ad(!dict_index_is_online_ddl(index));

	ut_ad(mem_heap_validate(index->heap));
	ut_a(!dict_index_is_clust(index)
	     || UT_LIST_GET_LEN(table->indexes) == 0);

	if (!dict_index_find_cols(table, index)) {

		dict_mem_index_free(index);
		return(DB_CORRUPTION);
	}

	/* Build the cache internal representation of the index,
	containing also the added system fields */

	if (index->type == DICT_FTS) {
		new_index = dict_index_build_internal_fts(table, index);
	} else if (dict_index_is_clust(index)) {
		new_index = dict_index_build_internal_clust(table, index);
	} else {
		new_index = dict_index_build_internal_non_clust(table, index);
	}

	/* Set the n_fields value in new_index to the actual defined
	number of fields in the cache internal representation */

	new_index->n_fields = new_index->n_def;

	if (strict && dict_index_too_big_for_tree(table, new_index)) {
too_big:
		dict_mem_index_free(new_index);
		dict_mem_index_free(index);
		return(DB_TOO_BIG_RECORD);
	}

	if (dict_index_is_univ(index)) {
		n_ord = new_index->n_fields;
	} else {
		n_ord = new_index->n_uniq;
	}

#if 1	/* The following code predetermines whether to call
	dict_index_too_big_for_undo().  This function is not
	accurate. See innodb_4k.test, Bug 13336585, for a
	testcase that shows an index that can be created but
	cannot be updated. */

	switch (dict_table_get_format(table)) {
	case UNIV_FORMAT_A:
		/* ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT store
		prefixes of externally stored columns locally within
		the record.  There are no special considerations for
		the undo log record size. */
		goto undo_size_ok;

	case UNIV_FORMAT_B:
		/* In ROW_FORMAT=DYNAMIC and ROW_FORMAT=COMPRESSED,
		column prefix indexes require that prefixes of
		externally stored columns are written to the undo log.
		This may make the undo log record bigger than the
		record on the B-tree page.  The maximum size of an
		undo log record is the page size.  That must be
		checked for below. */
		break;

#if UNIV_FORMAT_B != UNIV_FORMAT_MAX
# error "UNIV_FORMAT_B != UNIV_FORMAT_MAX"
#endif
	}

	for (i = 0; i < n_ord; i++) {
		const dict_field_t*	field
			= dict_index_get_nth_field(new_index, i);
		const dict_col_t*	col
			= dict_field_get_col(field);

		/* In dtuple_convert_big_rec(), variable-length columns
		that are longer than BTR_EXTERN_FIELD_REF_SIZE * 2
		may be chosen for external storage.  If the column appears
		in an ordering column of an index, a longer prefix determined
		by dict_max_field_len_store_undo() will be copied to the undo
		log by trx_undo_page_report_modify() and
		trx_undo_page_fetch_ext().  It suffices to check the
		capacity of the undo log whenever new_index includes
		a column prefix on a column that may be stored externally. */

		if (field->prefix_len /* prefix index */
		    && (!col->ord_part /* not yet ordering column */
			|| field->prefix_len > col->max_prefix)
		    && !dict_col_get_fixed_size(col, TRUE) /* variable-length */
		    && dict_col_get_max_size(col)
		    > BTR_EXTERN_FIELD_REF_SIZE * 2 /* long enough */) {

			if (dict_index_too_big_for_undo(table, new_index)) {
				/* An undo log record might not fit in
				a single page.  Refuse to create this index. */

				goto too_big;
			}

			break;
		}
	}

undo_size_ok:
#endif
	/* Flag the ordering columns and also set column max_prefix */

	for (i = 0; i < n_ord; i++) {
		const dict_field_t*	field
			= dict_index_get_nth_field(new_index, i);

		field->col->ord_part = 1;

		if (field->prefix_len > field->col->max_prefix) {
			field->col->max_prefix = field->prefix_len;
		}
	}

	if (!dict_index_is_univ(new_index)) {

		new_index->stat_n_diff_key_vals =
			static_cast<ib_uint64_t*>(mem_heap_zalloc(
			new_index->heap,
			dict_index_get_n_unique(new_index)
			* sizeof(*new_index->stat_n_diff_key_vals)));

		new_index->stat_n_sample_sizes =
			static_cast<ib_uint64_t*>(mem_heap_zalloc(
			new_index->heap,
			dict_index_get_n_unique(new_index)
			* sizeof(*new_index->stat_n_sample_sizes)));

		new_index->stat_n_non_null_key_vals =
			static_cast<ib_uint64_t*>(mem_heap_zalloc(
			new_index->heap,
			dict_index_get_n_unique(new_index)
			* sizeof(*new_index->stat_n_non_null_key_vals)));
	}

	new_index->stat_index_size = 1;
	new_index->stat_n_leaf_pages = 1;

	/* Add the new index as the last index for the table */

	UT_LIST_ADD_LAST(indexes, table->indexes, new_index);
	new_index->table = table;
	new_index->table_name = table->name;
	new_index->search_info = btr_search_info_create(new_index->heap);

	new_index->page = page_no;
	rw_lock_create(index_tree_rw_lock_key, &new_index->lock,
		       dict_index_is_ibuf(index)
		       ? SYNC_IBUF_INDEX_TREE : SYNC_INDEX_TREE);

	dict_sys->size += mem_heap_get_size(new_index->heap);

	dict_mem_index_free(index);

	return(DB_SUCCESS);
}

/**********************************************************************//**
Removes an index from the dictionary cache. */
static
void
dict_index_remove_from_cache_low(
/*=============================*/
	dict_table_t*	table,		/*!< in/out: table */
	dict_index_t*	index,		/*!< in, own: index */
	ibool		lru_evict)	/*!< in: TRUE if index being evicted
					to make room in the table LRU list */
{
	ulint		size;
	ulint		retries = 0;
	btr_search_t*	info;

	ut_ad(table && index);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	/* No need to acquire the dict_index_t::lock here because
	there can't be any active operations on this index (or table). */

	if (index->online_log) {
		ut_ad(index->online_status == ONLINE_INDEX_CREATION);
		row_log_free(index->online_log);
	}

	/* We always create search info whether or not adaptive
	hash index is enabled or not. */
	info = btr_search_get_info(index);
	ut_ad(info);

	/* We are not allowed to free the in-memory index struct
	dict_index_t until all entries in the adaptive hash index
	that point to any of the page belonging to his b-tree index
	are dropped. This is so because dropping of these entries
	require access to dict_index_t struct. To avoid such scenario
	We keep a count of number of such pages in the search_info and
	only free the dict_index_t struct when this count drops to
	zero. See also: dict_table_can_be_evicted() */

	do {
		ulint ref_count = btr_search_info_get_ref_count(info);

		if (ref_count == 0) {
			break;
		}

		/* Sleep for 10ms before trying again. */
		os_thread_sleep(10000);
		++retries;

		if (retries % 500 == 0) {
			/* No luck after 5 seconds of wait. */
			fprintf(stderr, "InnoDB: Error: Waited for"
				" %lu secs for hash index"
				" ref_count (%lu) to drop"
				" to 0.\n"
				"index: \"%s\""
				" table: \"%s\"\n",
				retries/100,
				ref_count,
				index->name,
				table->name);
		}

		/* To avoid a hang here we commit suicide if the
		ref_count doesn't drop to zero in 600 seconds. */
		if (retries >= 60000) {
			ut_error;
		}
	} while (srv_shutdown_state == SRV_SHUTDOWN_NONE || !lru_evict);

	rw_lock_free(&index->lock);

	/* Remove the index from the list of indexes of the table */
	UT_LIST_REMOVE(indexes, table->indexes, index);

	size = mem_heap_get_size(index->heap);

	ut_ad(dict_sys->size >= size);

	dict_sys->size -= size;

	dict_mem_index_free(index);
}

/**********************************************************************//**
Removes an index from the dictionary cache. */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/*!< in/out: table */
	dict_index_t*	index)	/*!< in, own: index */
{
	dict_index_remove_from_cache_low(table, index, FALSE);
}

/*******************************************************************//**
Tries to find column names for the index and sets the col field of the
index.
@return TRUE if the column names were found */
static
ibool
dict_index_find_cols(
/*=================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index)	/*!< in: index */
{
	ulint		i;

	ut_ad(table && index);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	for (i = 0; i < index->n_fields; i++) {
		ulint		j;
		dict_field_t*	field = dict_index_get_nth_field(index, i);

		for (j = 0; j < table->n_cols; j++) {
			if (!strcmp(dict_table_get_col_name(table, j),
				    field->name)) {
				field->col = dict_table_get_nth_col(table, j);

				goto found;
			}
		}

#ifdef UNIV_DEBUG
		/* It is an error not to find a matching column. */
		fputs("InnoDB: Error: no matching column for ", stderr);
		ut_print_name(stderr, NULL, FALSE, field->name);
		fputs(" in ", stderr);
		dict_index_name_print(stderr, NULL, index);
		fputs("!\n", stderr);
#endif /* UNIV_DEBUG */
		return(FALSE);

found:
		;
	}

	return(TRUE);
}
#endif /* !UNIV_HOTBACKUP */

/*******************************************************************//**
Adds a column to index. */
UNIV_INTERN
void
dict_index_add_col(
/*===============*/
	dict_index_t*		index,		/*!< in/out: index */
	const dict_table_t*	table,		/*!< in: table */
	dict_col_t*		col,		/*!< in: column */
	ulint			prefix_len)	/*!< in: column prefix length */
{
	dict_field_t*	field;
	const char*	col_name;

	col_name = dict_table_get_col_name(table, dict_col_get_no(col));

	dict_mem_index_add_field(index, col_name, prefix_len);

	field = dict_index_get_nth_field(index, index->n_def - 1);

	field->col = col;
	field->fixed_len = (unsigned int) dict_col_get_fixed_size(
		col, dict_table_is_comp(table));

	if (prefix_len && field->fixed_len > prefix_len) {
		field->fixed_len = (unsigned int) prefix_len;
	}

	/* Long fixed-length fields that need external storage are treated as
	variable-length fields, so that the extern flag can be embedded in
	the length word. */

	if (field->fixed_len > DICT_MAX_FIXED_COL_LEN) {
		field->fixed_len = 0;
	}
#if DICT_MAX_FIXED_COL_LEN != 768
	/* The comparison limit above must be constant.  If it were
	changed, the disk format of some fixed-length columns would
	change, which would be a disaster. */
# error "DICT_MAX_FIXED_COL_LEN != 768"
#endif

	if (!(col->prtype & DATA_NOT_NULL)) {
		index->n_nullable++;
	}
}

#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Copies fields contained in index2 to index1. */
static
void
dict_index_copy(
/*============*/
	dict_index_t*		index1,	/*!< in: index to copy to */
	dict_index_t*		index2,	/*!< in: index to copy from */
	const dict_table_t*	table,	/*!< in: table */
	ulint			start,	/*!< in: first position to copy */
	ulint			end)	/*!< in: last position to copy */
{
	dict_field_t*	field;
	ulint		i;

	/* Copy fields contained in index2 */

	for (i = start; i < end; i++) {

		field = dict_index_get_nth_field(index2, i);
		dict_index_add_col(index1, table, field->col,
				   field->prefix_len);
	}
}

/*******************************************************************//**
Copies types of fields contained in index to tuple. */
UNIV_INTERN
void
dict_index_copy_types(
/*==================*/
	dtuple_t*		tuple,		/*!< in/out: data tuple */
	const dict_index_t*	index,		/*!< in: index */
	ulint			n_fields)	/*!< in: number of
						field types to copy */
{
	ulint		i;

	if (dict_index_is_univ(index)) {
		dtuple_set_types_binary(tuple, n_fields);

		return;
	}

	for (i = 0; i < n_fields; i++) {
		const dict_field_t*	ifield;
		dtype_t*		dfield_type;

		ifield = dict_index_get_nth_field(index, i);
		dfield_type = dfield_get_type(dtuple_get_nth_field(tuple, i));
		dict_col_copy_type(dict_field_get_col(ifield), dfield_type);
	}
}

/*******************************************************************//**
Copies types of columns contained in table to tuple and sets all
fields of the tuple to the SQL NULL value.  This function should
be called right after dtuple_create(). */
UNIV_INTERN
void
dict_table_copy_types(
/*==================*/
	dtuple_t*		tuple,	/*!< in/out: data tuple */
	const dict_table_t*	table)	/*!< in: table */
{
	ulint		i;

	for (i = 0; i < dtuple_get_n_fields(tuple); i++) {

		dfield_t*	dfield	= dtuple_get_nth_field(tuple, i);
		dtype_t*	dtype	= dfield_get_type(dfield);

		dfield_set_null(dfield);
		dict_col_copy_type(dict_table_get_nth_col(table, i), dtype);
	}
}

/********************************************************************
Wait until all the background threads of the given table have exited, i.e.,
bg_threads == 0. Note: bg_threads_mutex must be reserved when
calling this. */
UNIV_INTERN
void
dict_table_wait_for_bg_threads_to_exit(
/*===================================*/
	dict_table_t*	table,	/*< in: table */
	ulint		delay)	/*< in: time in microseconds to wait between
				checks of bg_threads. */
{
	fts_t*		fts = table->fts;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&fts->bg_threads_mutex));
#endif /* UNIV_SYNC_DEBUG */

	while (fts->bg_threads > 0) {
		mutex_exit(&fts->bg_threads_mutex);

		os_thread_sleep(delay);

		mutex_enter(&fts->bg_threads_mutex);
	}
}

/*******************************************************************//**
Builds the internal dictionary cache representation for a clustered
index, containing also system fields not defined by the user.
@return	own: the internal representation of the clustered index */
static
dict_index_t*
dict_index_build_internal_clust(
/*============================*/
	const dict_table_t*	table,	/*!< in: table */
	dict_index_t*		index)	/*!< in: user representation of
					a clustered index */
{
	dict_index_t*	new_index;
	dict_field_t*	field;
	ulint		trx_id_pos;
	ulint		i;
	ibool*		indexed;

	ut_ad(table && index);
	ut_ad(dict_index_is_clust(index));
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* Create a new index object with certainly enough fields */
	new_index = dict_mem_index_create(table->name,
					  index->name, table->space,
					  index->type,
					  index->n_fields + table->n_cols);

	/* Copy other relevant data from the old index struct to the new
	struct: it inherits the values */

	new_index->n_user_defined_cols = index->n_fields;

	new_index->id = index->id;

	/* Copy the fields of index */
	dict_index_copy(new_index, index, table, 0, index->n_fields);

	if (dict_index_is_univ(index)) {
		/* No fixed number of fields determines an entry uniquely */

		new_index->n_uniq = REC_MAX_N_FIELDS;

	} else if (dict_index_is_unique(index)) {
		/* Only the fields defined so far are needed to identify
		the index entry uniquely */

		new_index->n_uniq = new_index->n_def;
	} else {
		/* Also the row id is needed to identify the entry */
		new_index->n_uniq = 1 + new_index->n_def;
	}

	new_index->trx_id_offset = 0;

	if (!dict_index_is_ibuf(index)) {
		/* Add system columns, trx id first */

		trx_id_pos = new_index->n_def;

#if DATA_ROW_ID != 0
# error "DATA_ROW_ID != 0"
#endif
#if DATA_TRX_ID != 1
# error "DATA_TRX_ID != 1"
#endif
#if DATA_ROLL_PTR != 2
# error "DATA_ROLL_PTR != 2"
#endif

		if (!dict_index_is_unique(index)) {
			dict_index_add_col(new_index, table,
					   dict_table_get_sys_col(
						   table, DATA_ROW_ID),
					   0);
			trx_id_pos++;
		}

		dict_index_add_col(new_index, table,
				   dict_table_get_sys_col(table, DATA_TRX_ID),
				   0);

		dict_index_add_col(new_index, table,
				   dict_table_get_sys_col(table,
							  DATA_ROLL_PTR),
				   0);

		for (i = 0; i < trx_id_pos; i++) {

			ulint	fixed_size = dict_col_get_fixed_size(
				dict_index_get_nth_col(new_index, i),
				dict_table_is_comp(table));

			if (fixed_size == 0) {
				new_index->trx_id_offset = 0;

				break;
			}

			if (dict_index_get_nth_field(new_index, i)->prefix_len
			    > 0) {
				new_index->trx_id_offset = 0;

				break;
			}

			/* Add fixed_size to new_index->trx_id_offset.
			Because the latter is a bit-field, an overflow
			can theoretically occur. Check for it. */
			fixed_size += new_index->trx_id_offset;

			new_index->trx_id_offset = fixed_size;

			if (new_index->trx_id_offset != fixed_size) {
				/* Overflow. Pretend that this is a
				variable-length PRIMARY KEY. */
				ut_ad(0);
				new_index->trx_id_offset = 0;
				break;
			}
		}

	}

	/* Remember the table columns already contained in new_index */
	indexed = static_cast<ibool*>(
		mem_zalloc(table->n_cols * sizeof *indexed));

	/* Mark the table columns already contained in new_index */
	for (i = 0; i < new_index->n_def; i++) {

		field = dict_index_get_nth_field(new_index, i);

		/* If there is only a prefix of the column in the index
		field, do not mark the column as contained in the index */

		if (field->prefix_len == 0) {

			indexed[field->col->ind] = TRUE;
		}
	}

	/* Add to new_index non-system columns of table not yet included
	there */
	for (i = 0; i + DATA_N_SYS_COLS < (ulint) table->n_cols; i++) {

		dict_col_t*	col = dict_table_get_nth_col(table, i);
		ut_ad(col->mtype != DATA_SYS);

		if (!indexed[col->ind]) {
			dict_index_add_col(new_index, table, col, 0);
		}
	}

	mem_free(indexed);

	ut_ad(dict_index_is_ibuf(index)
	      || (UT_LIST_GET_LEN(table->indexes) == 0));

	new_index->cached = TRUE;

	return(new_index);
}

/*******************************************************************//**
Builds the internal dictionary cache representation for a non-clustered
index, containing also system fields not defined by the user.
@return	own: the internal representation of the non-clustered index */
static
dict_index_t*
dict_index_build_internal_non_clust(
/*================================*/
	const dict_table_t*	table,	/*!< in: table */
	dict_index_t*		index)	/*!< in: user representation of
					a non-clustered index */
{
	dict_field_t*	field;
	dict_index_t*	new_index;
	dict_index_t*	clust_index;
	ulint		i;
	ibool*		indexed;

	ut_ad(table && index);
	ut_ad(!dict_index_is_clust(index));
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* The clustered index should be the first in the list of indexes */
	clust_index = UT_LIST_GET_FIRST(table->indexes);

	ut_ad(clust_index);
	ut_ad(dict_index_is_clust(clust_index));
	ut_ad(!dict_index_is_univ(clust_index));

	/* Create a new index */
	new_index = dict_mem_index_create(
		table->name, index->name, index->space, index->type,
		index->n_fields + 1 + clust_index->n_uniq);

	/* Copy other relevant data from the old index
	struct to the new struct: it inherits the values */

	new_index->n_user_defined_cols = index->n_fields;

	new_index->id = index->id;

	/* Copy fields from index to new_index */
	dict_index_copy(new_index, index, table, 0, index->n_fields);

	/* Remember the table columns already contained in new_index */
	indexed = static_cast<ibool*>(
		mem_zalloc(table->n_cols * sizeof *indexed));

	/* Mark the table columns already contained in new_index */
	for (i = 0; i < new_index->n_def; i++) {

		field = dict_index_get_nth_field(new_index, i);

		/* If there is only a prefix of the column in the index
		field, do not mark the column as contained in the index */

		if (field->prefix_len == 0) {

			indexed[field->col->ind] = TRUE;
		}
	}

	/* Add to new_index the columns necessary to determine the clustered
	index entry uniquely */

	for (i = 0; i < clust_index->n_uniq; i++) {

		field = dict_index_get_nth_field(clust_index, i);

		if (!indexed[field->col->ind]) {
			dict_index_add_col(new_index, table, field->col,
					   field->prefix_len);
		}
	}

	mem_free(indexed);

	if (dict_index_is_unique(index)) {
		new_index->n_uniq = index->n_fields;
	} else {
		new_index->n_uniq = new_index->n_def;
	}

	/* Set the n_fields value in new_index to the actual defined
	number of fields */

	new_index->n_fields = new_index->n_def;

	new_index->cached = TRUE;

	return(new_index);
}

/***********************************************************************
Builds the internal dictionary cache representation for an FTS index.
@return	own: the internal representation of the FTS index */
static
dict_index_t*
dict_index_build_internal_fts(
/*==========================*/
	dict_table_t*	table,	/*!< in: table */
	dict_index_t*	index)	/*!< in: user representation of an FTS index */
{
	dict_index_t*	new_index;

	ut_ad(table && index);
	ut_ad(index->type == DICT_FTS);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* Create a new index */
	new_index = dict_mem_index_create(
		table->name, index->name, index->space, index->type,
		index->n_fields);

	/* Copy other relevant data from the old index struct to the new
	struct: it inherits the values */

	new_index->n_user_defined_cols = index->n_fields;

	new_index->id = index->id;

	/* Copy fields from index to new_index */
	dict_index_copy(new_index, index, table, 0, index->n_fields);

	new_index->n_uniq = 0;
	new_index->cached = TRUE;

	if (table->fts->cache == NULL) {
		table->fts->cache = fts_cache_create(table);
	}

	rw_lock_x_lock(&table->fts->cache->init_lock);
	/* Notify the FTS cache about this index. */
	fts_cache_index_cache_create(table, new_index);
	rw_lock_x_unlock(&table->fts->cache->init_lock);

	return(new_index);
}
/*====================== FOREIGN KEY PROCESSING ========================*/

/*********************************************************************//**
Checks if a table is referenced by foreign keys.
@return	TRUE if table is referenced by a foreign key */
UNIV_INTERN
ibool
dict_table_is_referenced_by_foreign_key(
/*====================================*/
	const dict_table_t*	table)	/*!< in: InnoDB table */
{
	return(UT_LIST_GET_LEN(table->referenced_list) > 0);
}

/*********************************************************************//**
Check if the index is referenced by a foreign key, if TRUE return foreign
else return NULL
@return pointer to foreign key struct if index is defined for foreign
key, otherwise NULL */
UNIV_INTERN
dict_foreign_t*
dict_table_get_referenced_constraint(
/*=================================*/
	dict_table_t*	table,	/*!< in: InnoDB table */
	dict_index_t*	index)	/*!< in: InnoDB index */
{
	dict_foreign_t*	foreign;

	ut_ad(index != NULL);
	ut_ad(table != NULL);

	for (foreign = UT_LIST_GET_FIRST(table->referenced_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

		if (foreign->referenced_index == index) {

			return(foreign);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Checks if a index is defined for a foreign key constraint. Index is a part
of a foreign key constraint if the index is referenced by foreign key
or index is a foreign key index.
@return pointer to foreign key struct if index is defined for foreign
key, otherwise NULL */
UNIV_INTERN
dict_foreign_t*
dict_table_get_foreign_constraint(
/*==============================*/
	dict_table_t*	table,	/*!< in: InnoDB table */
	dict_index_t*	index)	/*!< in: InnoDB index */
{
	dict_foreign_t*	foreign;

	ut_ad(index != NULL);
	ut_ad(table != NULL);

	for (foreign = UT_LIST_GET_FIRST(table->foreign_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {

		if (foreign->foreign_index == index) {

			return(foreign);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Frees a foreign key struct. */
UNIV_INTERN
void
dict_foreign_free(
/*==============*/
	dict_foreign_t*	foreign)	/*!< in, own: foreign key struct */
{
	mem_heap_free(foreign->heap);
}

/**********************************************************************//**
Removes a foreign constraint struct from the dictionary cache. */
UNIV_INTERN
void
dict_foreign_remove_from_cache(
/*===========================*/
	dict_foreign_t*	foreign)	/*!< in, own: foreign constraint */
{
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_a(foreign);

	if (foreign->referenced_table) {
		UT_LIST_REMOVE(referenced_list,
			       foreign->referenced_table->referenced_list,
			       foreign);
	}

	if (foreign->foreign_table) {
		UT_LIST_REMOVE(foreign_list,
			       foreign->foreign_table->foreign_list,
			       foreign);
	}

	dict_foreign_free(foreign);
}

/**********************************************************************//**
Looks for the foreign constraint from the foreign and referenced lists
of a table.
@return	foreign constraint */
static
dict_foreign_t*
dict_foreign_find(
/*==============*/
	dict_table_t*	table,	/*!< in: table object */
	const char*	id)	/*!< in: foreign constraint id */
{
	dict_foreign_t*	foreign;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign) {
		if (ut_strcmp(id, foreign->id) == 0) {

			return(foreign);
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign) {
		if (ut_strcmp(id, foreign->id) == 0) {

			return(foreign);
		}

		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}

	return(NULL);
}


/*********************************************************************//**
Tries to find an index whose first fields are the columns in the array,
in the same order and is not marked for deletion and is not the same
as types_idx.
@return	matching index, NULL if not found */
UNIV_INTERN
dict_index_t*
dict_foreign_find_index(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	const char**		columns,/*!< in: array of column names */
	ulint			n_cols,	/*!< in: number of columns */
	const dict_index_t*	types_idx,
					/*!< in: NULL or an index
					whose types the column types
					must match */
	ibool			check_charsets,
					/*!< in: whether to check
					charsets.  only has an effect
					if types_idx != NULL */
	ulint			check_null)
					/*!< in: nonzero if none of
					the columns must be declared
					NOT NULL */
{
	dict_index_t*	index;

	ut_ad(mutex_own(&dict_sys->mutex));

	index = dict_table_get_first_index(table);

	while (index != NULL) {
		/* Ignore matches that refer to the same instance
		(or the index is to be dropped) */
		if (types_idx == index || index->type & DICT_FTS
		    || index->to_be_dropped) {

			goto next_rec;

		} else if (dict_foreign_qualify_index(
			table, columns, n_cols, index, types_idx,
			check_charsets, check_null)) {
			return(index);
		}

next_rec:
		index = dict_table_get_next_index(index);
	}

	return(NULL);
}

/**********************************************************************//**
Report an error in a foreign key definition. */
static
void
dict_foreign_error_report_low(
/*==========================*/
	FILE*		file,	/*!< in: output stream */
	const char*	name)	/*!< in: table name */
{
	rewind(file);
	ut_print_timestamp(file);
	fprintf(file, " Error in foreign key constraint of table %s:\n",
		name);
}

/**********************************************************************//**
Report an error in a foreign key definition. */
static
void
dict_foreign_error_report(
/*======================*/
	FILE*		file,	/*!< in: output stream */
	dict_foreign_t*	fk,	/*!< in: foreign key constraint */
	const char*	msg)	/*!< in: the error message */
{
	mutex_enter(&dict_foreign_err_mutex);
	dict_foreign_error_report_low(file, fk->foreign_table_name);
	fputs(msg, file);
	fputs(" Constraint:\n", file);
	dict_print_info_on_foreign_key_in_create_format(file, NULL, fk, TRUE);
	putc('\n', file);
	if (fk->foreign_index) {
		fputs("The index in the foreign key in table is ", file);
		ut_print_name(file, NULL, FALSE, fk->foreign_index->name);
		fputs("\n"
		      "See " REFMAN "innodb-foreign-key-constraints.html\n"
		      "for correct foreign key definition.\n",
		      file);
	}
	mutex_exit(&dict_foreign_err_mutex);
}

/**********************************************************************//**
Adds a foreign key constraint object to the dictionary cache. May free
the object if there already is an object with the same identifier in.
At least one of the foreign table and the referenced table must already
be in the dictionary cache!
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_foreign_add_to_cache(
/*======================*/
	dict_foreign_t*	foreign,	/*!< in, own: foreign key constraint */
	ibool		check_charsets)	/*!< in: TRUE=check charset
					compatibility */
{
	dict_table_t*	for_table;
	dict_table_t*	ref_table;
	dict_foreign_t*	for_in_cache		= NULL;
	dict_index_t*	index;
	ibool		added_to_referenced_list= FALSE;
	FILE*		ef			= dict_foreign_err_file;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	for_table = dict_table_check_if_in_cache_low(
		foreign->foreign_table_name_lookup);

	ref_table = dict_table_check_if_in_cache_low(
		foreign->referenced_table_name_lookup);
	ut_a(for_table || ref_table);

	if (for_table) {
		for_in_cache = dict_foreign_find(for_table, foreign->id);
	}

	if (!for_in_cache && ref_table) {
		for_in_cache = dict_foreign_find(ref_table, foreign->id);
	}

	if (for_in_cache) {
		/* Free the foreign object */
		mem_heap_free(foreign->heap);
	} else {
		for_in_cache = foreign;
	}

	if (for_in_cache->referenced_table == NULL && ref_table) {
		index = dict_foreign_find_index(
			ref_table,
			for_in_cache->referenced_col_names,
			for_in_cache->n_fields, for_in_cache->foreign_index,
			check_charsets, FALSE);

		if (index == NULL) {
			dict_foreign_error_report(
				ef, for_in_cache,
				"there is no index in referenced table"
				" which would contain\n"
				"the columns as the first columns,"
				" or the data types in the\n"
				"referenced table do not match"
				" the ones in table.");

			if (for_in_cache == foreign) {
				mem_heap_free(foreign->heap);
			}

			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		for_in_cache->referenced_table = ref_table;
		for_in_cache->referenced_index = index;
		UT_LIST_ADD_LAST(referenced_list,
				 ref_table->referenced_list,
				 for_in_cache);
		added_to_referenced_list = TRUE;
	}

	if (for_in_cache->foreign_table == NULL && for_table) {
		index = dict_foreign_find_index(
			for_table,
			for_in_cache->foreign_col_names,
			for_in_cache->n_fields,
			for_in_cache->referenced_index, check_charsets,
			for_in_cache->type
			& (DICT_FOREIGN_ON_DELETE_SET_NULL
			   | DICT_FOREIGN_ON_UPDATE_SET_NULL));

		if (index == NULL) {
			dict_foreign_error_report(
				ef, for_in_cache,
				"there is no index in the table"
				" which would contain\n"
				"the columns as the first columns,"
				" or the data types in the\n"
				"table do not match"
				" the ones in the referenced table\n"
				"or one of the ON ... SET NULL columns"
				" is declared NOT NULL.");

			if (for_in_cache == foreign) {
				if (added_to_referenced_list) {
					UT_LIST_REMOVE(
						referenced_list,
						ref_table->referenced_list,
						for_in_cache);
				}

				mem_heap_free(foreign->heap);
			}

			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		for_in_cache->foreign_table = for_table;
		for_in_cache->foreign_index = index;
		UT_LIST_ADD_LAST(foreign_list,
				 for_table->foreign_list,
				 for_in_cache);
	}

	/* We need to move the table to the non-LRU end of the table LRU
	list. Otherwise it will be evicted from the cache. */

	if (ref_table != NULL && ref_table->can_be_evicted) {
		dict_table_move_from_lru_to_non_lru(ref_table);
	}

	if (for_table != NULL && for_table->can_be_evicted) {
		dict_table_move_from_lru_to_non_lru(for_table);
	}

	ut_ad(dict_lru_validate());

	return(DB_SUCCESS);
}

/*********************************************************************//**
Scans from pointer onwards. Stops if is at the start of a copy of
'string' where characters are compared without case sensitivity, and
only outside `` or "" quotes. Stops also at NUL.
@return	scanned up to this */
static
const char*
dict_scan_to(
/*=========*/
	const char*	ptr,	/*!< in: scan from */
	const char*	string)	/*!< in: look for this */
{
	char	quote	= '\0';

	for (; *ptr; ptr++) {
		if (*ptr == quote) {
			/* Closing quote character: do not look for
			starting quote or the keyword. */
			quote = '\0';
		} else if (quote) {
			/* Within quotes: do nothing. */
		} else if (*ptr == '`' || *ptr == '"' || *ptr == '\'') {
			/* Starting quote: remember the quote character. */
			quote = *ptr;
		} else {
			/* Outside quotes: look for the keyword. */
			ulint	i;
			for (i = 0; string[i]; i++) {
				if (toupper((int)(unsigned char)(ptr[i]))
				    != toupper((int)(unsigned char)
					       (string[i]))) {
					goto nomatch;
				}
			}
			break;
nomatch:
			;
		}
	}

	return(ptr);
}

/*********************************************************************//**
Accepts a specified string. Comparisons are case-insensitive.
@return if string was accepted, the pointer is moved after that, else
ptr is returned */
static
const char*
dict_accept(
/*========*/
	struct charset_info_st*	cs,/*!< in: the character set of ptr */
	const char*	ptr,	/*!< in: scan from this */
	const char*	string,	/*!< in: accept only this string as the next
				non-whitespace string */
	ibool*		success)/*!< out: TRUE if accepted */
{
	const char*	old_ptr = ptr;
	const char*	old_ptr2;

	*success = FALSE;

	while (my_isspace(cs, *ptr)) {
		ptr++;
	}

	old_ptr2 = ptr;

	ptr = dict_scan_to(ptr, string);

	if (*ptr == '\0' || old_ptr2 != ptr) {
		return(old_ptr);
	}

	*success = TRUE;

	return(ptr + ut_strlen(string));
}

/*********************************************************************//**
Scans an id. For the lexical definition of an 'id', see the code below.
Strips backquotes or double quotes from around the id.
@return	scanned to */
static
const char*
dict_scan_id(
/*=========*/
	struct charset_info_st*	cs,/*!< in: the character set of ptr */
	const char*	ptr,	/*!< in: scanned to */
	mem_heap_t*	heap,	/*!< in: heap where to allocate the id
				(NULL=id will not be allocated, but it
				will point to string near ptr) */
	const char**	id,	/*!< out,own: the id; NULL if no id was
				scannable */
	ibool		table_id,/*!< in: TRUE=convert the allocated id
				as a table name; FALSE=convert to UTF-8 */
	ibool		accept_also_dot)
				/*!< in: TRUE if also a dot can appear in a
				non-quoted id; in a quoted id it can appear
				always */
{
	char		quote	= '\0';
	ulint		len	= 0;
	const char*	s;
	char*		str;
	char*		dst;

	*id = NULL;

	while (my_isspace(cs, *ptr)) {
		ptr++;
	}

	if (*ptr == '\0') {

		return(ptr);
	}

	if (*ptr == '`' || *ptr == '"') {
		quote = *ptr++;
	}

	s = ptr;

	if (quote) {
		for (;;) {
			if (!*ptr) {
				/* Syntax error */
				return(ptr);
			}
			if (*ptr == quote) {
				ptr++;
				if (*ptr != quote) {
					break;
				}
			}
			ptr++;
			len++;
		}
	} else {
		while (!my_isspace(cs, *ptr) && *ptr != '(' && *ptr != ')'
		       && (accept_also_dot || *ptr != '.')
		       && *ptr != ',' && *ptr != '\0') {

			ptr++;
		}

		len = ptr - s;
	}

	if (UNIV_UNLIKELY(!heap)) {
		/* no heap given: id will point to source string */
		*id = s;
		return(ptr);
	}

	if (quote) {
		char*	d;

		str = d = static_cast<char*>(
			mem_heap_alloc(heap, len + 1));

		while (len--) {
			if ((*d++ = *s++) == quote) {
				s++;
			}
		}
		*d++ = 0;
		len = d - str;
		ut_ad(*s == quote);
		ut_ad(s + 1 == ptr);
	} else {
		str = mem_heap_strdupl(heap, s, len);
	}

	if (!table_id) {
convert_id:
		/* Convert the identifier from connection character set
		to UTF-8. */
		len = 3 * len + 1;
		*id = dst = static_cast<char*>(mem_heap_alloc(heap, len));

		innobase_convert_from_id(cs, dst, str, len);
	} else if (!strncmp(str, srv_mysql50_table_name_prefix,
			    sizeof(srv_mysql50_table_name_prefix) - 1)) {
		/* This is a pre-5.1 table name
		containing chars other than [A-Za-z0-9].
		Discard the prefix and use raw UTF-8 encoding. */
		str += sizeof(srv_mysql50_table_name_prefix) - 1;
		len -= sizeof(srv_mysql50_table_name_prefix) - 1;
		goto convert_id;
	} else {
		/* Encode using filename-safe characters. */
		len = 5 * len + 1;
		*id = dst = static_cast<char*>(mem_heap_alloc(heap, len));

		innobase_convert_from_table_id(cs, dst, str, len);
	}

	return(ptr);
}

/*********************************************************************//**
Tries to scan a column name.
@return	scanned to */
static
const char*
dict_scan_col(
/*==========*/
	struct charset_info_st*	cs,	/*!< in: the character set of ptr */
	const char*		ptr,	/*!< in: scanned to */
	ibool*			success,/*!< out: TRUE if success */
	dict_table_t*		table,	/*!< in: table in which the column is */
	const dict_col_t**	column,	/*!< out: pointer to column if success */
	mem_heap_t*		heap,	/*!< in: heap where to allocate */
	const char**		name)	/*!< out,own: the column name;
					NULL if no name was scannable */
{
	ulint		i;

	*success = FALSE;

	ptr = dict_scan_id(cs, ptr, heap, name, FALSE, TRUE);

	if (*name == NULL) {

		return(ptr);	/* Syntax error */
	}

	if (table == NULL) {
		*success = TRUE;
		*column = NULL;
	} else {
		for (i = 0; i < dict_table_get_n_cols(table); i++) {

			const char*	col_name = dict_table_get_col_name(
				table, i);

			if (0 == innobase_strcasecmp(col_name, *name)) {
				/* Found */

				*success = TRUE;
				*column = dict_table_get_nth_col(table, i);
				strcpy((char*) *name, col_name);

				break;
			}
		}
	}

	return(ptr);
}


/*********************************************************************//**
Open a table from its database and table name, this is currently used by
foreign constraint parser to get the referenced table.
@return complete table name with database and table name, allocated from
heap memory passed in */
UNIV_INTERN
char*
dict_get_referenced_table(
/*======================*/
	const char*	name,		/*!< in: foreign key table name */
	const char*	database_name,	/*!< in: table db name */
	ulint		database_name_len, /*!< in: db name length */
	const char*	table_name,	/*!< in: table name */
	ulint		table_name_len, /*!< in: table name length */
	dict_table_t**	table,		/*!< out: table object or NULL */
	mem_heap_t*	heap)		/*!< in/out: heap memory */
{
	char*		ref;
	const char*	db_name;

	if (!database_name) {
		/* Use the database name of the foreign key table */

		db_name = name;
		database_name_len = dict_get_db_name_len(name);
	} else {
		db_name = database_name;
	}

	/* Copy database_name, '/', table_name, '\0' */
	ref = static_cast<char*>(
		mem_heap_alloc(heap, database_name_len + table_name_len + 2));

	memcpy(ref, db_name, database_name_len);
	ref[database_name_len] = '/';
	memcpy(ref + database_name_len + 1, table_name, table_name_len + 1);

	/* Values;  0 = Store and compare as given; case sensitive
	            1 = Store and compare in lower; case insensitive
	            2 = Store as given, compare in lower; case semi-sensitive */
	if (innobase_get_lower_case_table_names() == 2) {
		innobase_casedn_str(ref);
		*table = dict_table_get_low(ref);
		memcpy(ref, db_name, database_name_len);
		ref[database_name_len] = '/';
		memcpy(ref + database_name_len + 1, table_name, table_name_len + 1);

	} else {
#ifndef __WIN__
		if (innobase_get_lower_case_table_names() == 1) {
			innobase_casedn_str(ref);
		}
#else
		innobase_casedn_str(ref);
#endif /* !__WIN__ */
		*table = dict_table_get_low(ref);
	}

	return(ref);
}
/*********************************************************************//**
Scans a table name from an SQL string.
@return	scanned to */
static
const char*
dict_scan_table_name(
/*=================*/
	struct charset_info_st*	cs,/*!< in: the character set of ptr */
	const char*	ptr,	/*!< in: scanned to */
	dict_table_t**	table,	/*!< out: table object or NULL */
	const char*	name,	/*!< in: foreign key table name */
	ibool*		success,/*!< out: TRUE if ok name found */
	mem_heap_t*	heap,	/*!< in: heap where to allocate the id */
	const char**	ref_name)/*!< out,own: the table name;
				NULL if no name was scannable */
{
	const char*	database_name	= NULL;
	ulint		database_name_len = 0;
	const char*	table_name	= NULL;
	const char*	scan_name;

	*success = FALSE;
	*table = NULL;

	ptr = dict_scan_id(cs, ptr, heap, &scan_name, TRUE, FALSE);

	if (scan_name == NULL) {

		return(ptr);	/* Syntax error */
	}

	if (*ptr == '.') {
		/* We scanned the database name; scan also the table name */

		ptr++;

		database_name = scan_name;
		database_name_len = strlen(database_name);

		ptr = dict_scan_id(cs, ptr, heap, &table_name, TRUE, FALSE);

		if (table_name == NULL) {

			return(ptr);	/* Syntax error */
		}
	} else {
		/* To be able to read table dumps made with InnoDB-4.0.17 or
		earlier, we must allow the dot separator between the database
		name and the table name also to appear within a quoted
		identifier! InnoDB used to print a constraint as:
		... REFERENCES `databasename.tablename` ...
		starting from 4.0.18 it is
		... REFERENCES `databasename`.`tablename` ... */
		const char* s;

		for (s = scan_name; *s; s++) {
			if (*s == '.') {
				database_name = scan_name;
				database_name_len = s - scan_name;
				scan_name = ++s;
				break;/* to do: multiple dots? */
			}
		}

		table_name = scan_name;
	}

	*ref_name = dict_get_referenced_table(
		name, database_name, database_name_len,
		table_name, strlen(table_name), table, heap);

	*success = TRUE;
	return(ptr);
}

/*********************************************************************//**
Skips one id. The id is allowed to contain also '.'.
@return	scanned to */
static
const char*
dict_skip_word(
/*===========*/
	struct charset_info_st*	cs,/*!< in: the character set of ptr */
	const char*	ptr,	/*!< in: scanned to */
	ibool*		success)/*!< out: TRUE if success, FALSE if just spaces
				left in string or a syntax error */
{
	const char*	start;

	*success = FALSE;

	ptr = dict_scan_id(cs, ptr, NULL, &start, FALSE, TRUE);

	if (start) {
		*success = TRUE;
	}

	return(ptr);
}

/*********************************************************************//**
Removes MySQL comments from an SQL string. A comment is either
(a) '#' to the end of the line,
(b) '--[space]' to the end of the line, or
(c) '[slash][asterisk]' till the next '[asterisk][slash]' (like the familiar
C comment syntax).
@return own: SQL string stripped from comments; the caller must free
this with mem_free()! */
static
char*
dict_strip_comments(
/*================*/
	const char*	sql_string,	/*!< in: SQL string */
	size_t		sql_length)	/*!< in: length of sql_string */
{
	char*		str;
	const char*	sptr;
	const char*	eptr	= sql_string + sql_length;
	char*		ptr;
	/* unclosed quote character (0 if none) */
	char		quote	= 0;

	str = static_cast<char*>(mem_alloc(sql_length + 1));

	sptr = sql_string;
	ptr = str;

	for (;;) {
scan_more:
		if (sptr >= eptr || *sptr == '\0') {
end_of_string:
			*ptr = '\0';

			ut_a(ptr <= str + sql_length);

			return(str);
		}

		if (*sptr == quote) {
			/* Closing quote character: do not look for
			starting quote or comments. */
			quote = 0;
		} else if (quote) {
			/* Within quotes: do not look for
			starting quotes or comments. */
		} else if (*sptr == '"' || *sptr == '`' || *sptr == '\'') {
			/* Starting quote: remember the quote character. */
			quote = *sptr;
		} else if (*sptr == '#'
			   || (sptr[0] == '-' && sptr[1] == '-'
			       && sptr[2] == ' ')) {
			for (;;) {
				if (++sptr >= eptr) {
					goto end_of_string;
				}

				/* In Unix a newline is 0x0A while in Windows
				it is 0x0D followed by 0x0A */

				switch (*sptr) {
				case (char) 0X0A:
				case (char) 0x0D:
				case '\0':
					goto scan_more;
				}
			}
		} else if (!quote && *sptr == '/' && *(sptr + 1) == '*') {
			sptr += 2;
			for (;;) {
				if (sptr >= eptr) {
					goto end_of_string;
				}

				switch (*sptr) {
				case '\0':
					goto scan_more;
				case '*':
					if (sptr[1] == '/') {
						sptr += 2;
						goto scan_more;
					}
				}

				sptr++;
			}
		}

		*ptr = *sptr;

		ptr++;
		sptr++;
	}
}

/*********************************************************************//**
Finds the highest [number] for foreign key constraints of the table. Looks
only at the >= 4.0.18-format id's, which are of the form
databasename/tablename_ibfk_[number].
@return	highest number, 0 if table has no new format foreign key constraints */
UNIV_INTERN
ulint
dict_table_get_highest_foreign_id(
/*==============================*/
	dict_table_t*	table)	/*!< in: table in the dictionary memory cache */
{
	dict_foreign_t*	foreign;
	char*		endp;
	ulint		biggest_id	= 0;
	ulint		id;
	ulint		len;

	ut_a(table);

	len = ut_strlen(table->name);
	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign) {
		if (ut_strlen(foreign->id) > ((sizeof dict_ibfk) - 1) + len
		    && 0 == ut_memcmp(foreign->id, table->name, len)
		    && 0 == ut_memcmp(foreign->id + len,
				      dict_ibfk, (sizeof dict_ibfk) - 1)
		    && foreign->id[len + ((sizeof dict_ibfk) - 1)] != '0') {
			/* It is of the >= 4.0.18 format */

			id = strtoul(foreign->id + len
				     + ((sizeof dict_ibfk) - 1),
				     &endp, 10);
			if (*endp == '\0') {
				ut_a(id != biggest_id);

				if (id > biggest_id) {
					biggest_id = id;
				}
			}
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	return(biggest_id);
}

/*********************************************************************//**
Reports a simple foreign key create clause syntax error. */
static
void
dict_foreign_report_syntax_err(
/*===========================*/
	const char*	name,		/*!< in: table name */
	const char*	start_of_latest_foreign,
					/*!< in: start of the foreign key clause
					in the SQL string */
	const char*	ptr)		/*!< in: place of the syntax error */
{
	ut_ad(!srv_read_only_mode);

	FILE*	ef = dict_foreign_err_file;

	mutex_enter(&dict_foreign_err_mutex);
	dict_foreign_error_report_low(ef, name);
	fprintf(ef, "%s:\nSyntax error close to:\n%s\n",
		start_of_latest_foreign, ptr);
	mutex_exit(&dict_foreign_err_mutex);
}

/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary the foreign
key constraints declared in the string. This function should be called after
the indexes for a table have been created. Each foreign key constraint must
be accompanied with indexes in both participating tables. The indexes are
allowed to contain more fields than mentioned in the constraint.
@return	error code or DB_SUCCESS */
static
dberr_t
dict_create_foreign_constraints_low(
/*================================*/
	trx_t*		trx,	/*!< in: transaction */
	mem_heap_t*	heap,	/*!< in: memory heap */
	struct charset_info_st*	cs,/*!< in: the character set of sql_string */
	const char*	sql_string,
				/*!< in: CREATE TABLE or ALTER TABLE statement
				where foreign keys are declared like:
				FOREIGN KEY (a, b) REFERENCES table2(c, d),
				table2 can be written also with the database
				name before it: test.table2; the default
				database is the database of parameter name */
	const char*	name,	/*!< in: table full name in the normalized form
				database_name/table_name */
	ibool		reject_fks)
				/*!< in: if TRUE, fail with error code
				DB_CANNOT_ADD_CONSTRAINT if any foreign
				keys are found. */
{
	dict_table_t*	table;
	dict_table_t*	referenced_table;
	dict_table_t*	table_to_alter;
	ulint		highest_id_so_far	= 0;
	dict_index_t*	index;
	dict_foreign_t*	foreign;
	const char*	ptr			= sql_string;
	const char*	start_of_latest_foreign	= sql_string;
	FILE*		ef			= dict_foreign_err_file;
	const char*	constraint_name;
	ibool		success;
	dberr_t		error;
	const char*	ptr1;
	const char*	ptr2;
	ulint		i;
	ulint		j;
	ibool		is_on_delete;
	ulint		n_on_deletes;
	ulint		n_on_updates;
	const dict_col_t*columns[500];
	const char*	column_names[500];
	const char*	referenced_table_name;

	ut_ad(!srv_read_only_mode);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	table = dict_table_get_low(name);

	if (table == NULL) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, name);
		fprintf(ef,
			"Cannot find the table in the internal"
			" data dictionary of InnoDB.\n"
			"Create table statement:\n%s\n", sql_string);
		mutex_exit(&dict_foreign_err_mutex);

		return(DB_ERROR);
	}

	/* First check if we are actually doing an ALTER TABLE, and in that
	case look for the table being altered */

	ptr = dict_accept(cs, ptr, "ALTER", &success);

	if (!success) {

		goto loop;
	}

	ptr = dict_accept(cs, ptr, "TABLE", &success);

	if (!success) {

		goto loop;
	}

	/* We are doing an ALTER TABLE: scan the table name we are altering */

	ptr = dict_scan_table_name(cs, ptr, &table_to_alter, name,
				   &success, heap, &referenced_table_name);
	if (!success) {
		fprintf(stderr,
			"InnoDB: Error: could not find"
			" the table being ALTERED in:\n%s\n",
			sql_string);

		return(DB_ERROR);
	}

	/* Starting from 4.0.18 and 4.1.2, we generate foreign key id's in the
	format databasename/tablename_ibfk_[number], where [number] is local
	to the table; look for the highest [number] for table_to_alter, so
	that we can assign to new constraints higher numbers. */

	/* If we are altering a temporary table, the table name after ALTER
	TABLE does not correspond to the internal table name, and
	table_to_alter is NULL. TODO: should we fix this somehow? */

	if (table_to_alter == NULL) {
		highest_id_so_far = 0;
	} else {
		highest_id_so_far = dict_table_get_highest_foreign_id(
			table_to_alter);
	}

	/* Scan for foreign key declarations in a loop */
loop:
	/* Scan either to "CONSTRAINT" or "FOREIGN", whichever is closer */

	ptr1 = dict_scan_to(ptr, "CONSTRAINT");
	ptr2 = dict_scan_to(ptr, "FOREIGN");

	constraint_name = NULL;

	if (ptr1 < ptr2) {
		/* The user may have specified a constraint name. Pick it so
		that we can store 'databasename/constraintname' as the id of
		of the constraint to system tables. */
		ptr = ptr1;

		ptr = dict_accept(cs, ptr, "CONSTRAINT", &success);

		ut_a(success);

		if (!my_isspace(cs, *ptr) && *ptr != '"' && *ptr != '`') {
			goto loop;
		}

		while (my_isspace(cs, *ptr)) {
			ptr++;
		}

		/* read constraint name unless got "CONSTRAINT FOREIGN" */
		if (ptr != ptr2) {
			ptr = dict_scan_id(cs, ptr, heap,
					   &constraint_name, FALSE, FALSE);
		}
	} else {
		ptr = ptr2;
	}

	if (*ptr == '\0') {
		/* The proper way to reject foreign keys for temporary
		tables would be to split the lexing and syntactical
		analysis of foreign key clauses from the actual adding
		of them, so that ha_innodb.cc could first parse the SQL
		command, determine if there are any foreign keys, and
		if so, immediately reject the command if the table is a
		temporary one. For now, this kludge will work. */
		if (reject_fks && (UT_LIST_GET_LEN(table->foreign_list) > 0)) {

			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		/**********************************************************/
		/* The following call adds the foreign key constraints
		to the data dictionary system tables on disk */

		error = dict_create_add_foreigns_to_dictionary(
			highest_id_so_far, table, trx);
		return(error);
	}

	start_of_latest_foreign = ptr;

	ptr = dict_accept(cs, ptr, "FOREIGN", &success);

	if (!success) {
		goto loop;
	}

	if (!my_isspace(cs, *ptr)) {
		goto loop;
	}

	ptr = dict_accept(cs, ptr, "KEY", &success);

	if (!success) {
		goto loop;
	}

	ptr = dict_accept(cs, ptr, "(", &success);

	if (!success) {
		/* MySQL allows also an index id before the '('; we
		skip it */
		ptr = dict_skip_word(cs, ptr, &success);

		if (!success) {
			dict_foreign_report_syntax_err(
				name, start_of_latest_foreign, ptr);

			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		ptr = dict_accept(cs, ptr, "(", &success);

		if (!success) {
			/* We do not flag a syntax error here because in an
			ALTER TABLE we may also have DROP FOREIGN KEY abc */

			goto loop;
		}
	}

	i = 0;

	/* Scan the columns in the first list */
col_loop1:
	ut_a(i < (sizeof column_names) / sizeof *column_names);
	ptr = dict_scan_col(cs, ptr, &success, table, columns + i,
			    heap, column_names + i);
	if (!success) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, name);
		fprintf(ef, "%s:\nCannot resolve column name close to:\n%s\n",
			start_of_latest_foreign, ptr);
		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	i++;

	ptr = dict_accept(cs, ptr, ",", &success);

	if (success) {
		goto col_loop1;
	}

	ptr = dict_accept(cs, ptr, ")", &success);

	if (!success) {
		dict_foreign_report_syntax_err(
			name, start_of_latest_foreign, ptr);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Try to find an index which contains the columns
	as the first fields and in the right order */

	index = dict_foreign_find_index(table, column_names, i,
					NULL, TRUE, FALSE);

	if (!index) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, name);
		fputs("There is no index in table ", ef);
		ut_print_name(ef, NULL, TRUE, name);
		fprintf(ef, " where the columns appear\n"
			"as the first columns. Constraint:\n%s\n"
			"See " REFMAN "innodb-foreign-key-constraints.html\n"
			"for correct foreign key definition.\n",
			start_of_latest_foreign);
		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CHILD_NO_INDEX);
	}
	ptr = dict_accept(cs, ptr, "REFERENCES", &success);

	if (!success || !my_isspace(cs, *ptr)) {
		dict_foreign_report_syntax_err(
			name, start_of_latest_foreign, ptr);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Let us create a constraint struct */

	foreign = dict_mem_foreign_create();

	if (constraint_name) {
		ulint	db_len;

		/* Catenate 'databasename/' to the constraint name specified
		by the user: we conceive the constraint as belonging to the
		same MySQL 'database' as the table itself. We store the name
		to foreign->id. */

		db_len = dict_get_db_name_len(table->name);

		foreign->id = static_cast<char*>(mem_heap_alloc(
			foreign->heap, db_len + strlen(constraint_name) + 2));

		ut_memcpy(foreign->id, table->name, db_len);
		foreign->id[db_len] = '/';
		strcpy(foreign->id + db_len + 1, constraint_name);
	}

	foreign->foreign_table = table;
	foreign->foreign_table_name = mem_heap_strdup(
		foreign->heap, table->name);
	dict_mem_foreign_table_name_lookup_set(foreign, TRUE);

	foreign->foreign_index = index;
	foreign->n_fields = (unsigned int) i;

	foreign->foreign_col_names = static_cast<const char**>(
		mem_heap_alloc(foreign->heap, i * sizeof(void*)));

	for (i = 0; i < foreign->n_fields; i++) {
		foreign->foreign_col_names[i] = mem_heap_strdup(
			foreign->heap,
			dict_table_get_col_name(table,
						dict_col_get_no(columns[i])));
	}

	ptr = dict_scan_table_name(cs, ptr, &referenced_table, name,
				   &success, heap, &referenced_table_name);

	/* Note that referenced_table can be NULL if the user has suppressed
	checking of foreign key constraints! */

	if (!success || (!referenced_table && trx->check_foreigns)) {
		dict_foreign_free(foreign);

		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, name);
		fprintf(ef, "%s:\nCannot resolve table name close to:\n"
			"%s\n",
			start_of_latest_foreign, ptr);
		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	ptr = dict_accept(cs, ptr, "(", &success);

	if (!success) {
		dict_foreign_free(foreign);
		dict_foreign_report_syntax_err(name, start_of_latest_foreign,
					       ptr);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Scan the columns in the second list */
	i = 0;

col_loop2:
	ptr = dict_scan_col(cs, ptr, &success, referenced_table, columns + i,
			    heap, column_names + i);
	i++;

	if (!success) {
		dict_foreign_free(foreign);

		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, name);
		fprintf(ef, "%s:\nCannot resolve column name close to:\n"
			"%s\n",
			start_of_latest_foreign, ptr);
		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	ptr = dict_accept(cs, ptr, ",", &success);

	if (success) {
		goto col_loop2;
	}

	ptr = dict_accept(cs, ptr, ")", &success);

	if (!success || foreign->n_fields != i) {
		dict_foreign_free(foreign);

		dict_foreign_report_syntax_err(name, start_of_latest_foreign,
					       ptr);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	n_on_deletes = 0;
	n_on_updates = 0;

scan_on_conditions:
	/* Loop here as long as we can find ON ... conditions */

	ptr = dict_accept(cs, ptr, "ON", &success);

	if (!success) {

		goto try_find_index;
	}

	ptr = dict_accept(cs, ptr, "DELETE", &success);

	if (!success) {
		ptr = dict_accept(cs, ptr, "UPDATE", &success);

		if (!success) {
			dict_foreign_free(foreign);

			dict_foreign_report_syntax_err(
				name, start_of_latest_foreign, ptr);
			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		is_on_delete = FALSE;
		n_on_updates++;
	} else {
		is_on_delete = TRUE;
		n_on_deletes++;
	}

	ptr = dict_accept(cs, ptr, "RESTRICT", &success);

	if (success) {
		goto scan_on_conditions;
	}

	ptr = dict_accept(cs, ptr, "CASCADE", &success);

	if (success) {
		if (is_on_delete) {
			foreign->type |= DICT_FOREIGN_ON_DELETE_CASCADE;
		} else {
			foreign->type |= DICT_FOREIGN_ON_UPDATE_CASCADE;
		}

		goto scan_on_conditions;
	}

	ptr = dict_accept(cs, ptr, "NO", &success);

	if (success) {
		ptr = dict_accept(cs, ptr, "ACTION", &success);

		if (!success) {
			dict_foreign_free(foreign);
			dict_foreign_report_syntax_err(
				name, start_of_latest_foreign, ptr);

			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		if (is_on_delete) {
			foreign->type |= DICT_FOREIGN_ON_DELETE_NO_ACTION;
		} else {
			foreign->type |= DICT_FOREIGN_ON_UPDATE_NO_ACTION;
		}

		goto scan_on_conditions;
	}

	ptr = dict_accept(cs, ptr, "SET", &success);

	if (!success) {
		dict_foreign_free(foreign);
		dict_foreign_report_syntax_err(name, start_of_latest_foreign,
					       ptr);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	ptr = dict_accept(cs, ptr, "NULL", &success);

	if (!success) {
		dict_foreign_free(foreign);
		dict_foreign_report_syntax_err(name, start_of_latest_foreign,
					       ptr);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	for (j = 0; j < foreign->n_fields; j++) {
		if ((dict_index_get_nth_col(foreign->foreign_index, j)->prtype)
		    & DATA_NOT_NULL) {

			/* It is not sensible to define SET NULL
			if the column is not allowed to be NULL! */

			dict_foreign_free(foreign);

			mutex_enter(&dict_foreign_err_mutex);
			dict_foreign_error_report_low(ef, name);
			fprintf(ef, "%s:\n"
				"You have defined a SET NULL condition"
				" though some of the\n"
				"columns are defined as NOT NULL.\n",
				start_of_latest_foreign);
			mutex_exit(&dict_foreign_err_mutex);

			return(DB_CANNOT_ADD_CONSTRAINT);
		}
	}

	if (is_on_delete) {
		foreign->type |= DICT_FOREIGN_ON_DELETE_SET_NULL;
	} else {
		foreign->type |= DICT_FOREIGN_ON_UPDATE_SET_NULL;
	}

	goto scan_on_conditions;

try_find_index:
	if (n_on_deletes > 1 || n_on_updates > 1) {
		/* It is an error to define more than 1 action */

		dict_foreign_free(foreign);

		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, name);
		fprintf(ef, "%s:\n"
			"You have twice an ON DELETE clause"
			" or twice an ON UPDATE clause.\n",
			start_of_latest_foreign);
		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Try to find an index which contains the columns as the first fields
	and in the right order, and the types are the same as in
	foreign->foreign_index */

	if (referenced_table) {
		index = dict_foreign_find_index(referenced_table,
						column_names, i,
						foreign->foreign_index,
						TRUE, FALSE);
		if (!index) {
			dict_foreign_free(foreign);
			mutex_enter(&dict_foreign_err_mutex);
			dict_foreign_error_report_low(ef, name);
			fprintf(ef, "%s:\n"
				"Cannot find an index in the"
				" referenced table where the\n"
				"referenced columns appear as the"
				" first columns, or column types\n"
				"in the table and the referenced table"
				" do not match for constraint.\n"
				"Note that the internal storage type of"
				" ENUM and SET changed in\n"
				"tables created with >= InnoDB-4.1.12,"
				" and such columns in old tables\n"
				"cannot be referenced by such columns"
				" in new tables.\n"
				"See " REFMAN
				"innodb-foreign-key-constraints.html\n"
				"for correct foreign key definition.\n",
				start_of_latest_foreign);
			mutex_exit(&dict_foreign_err_mutex);

			return(DB_PARENT_NO_INDEX);
		}
	} else {
		ut_a(trx->check_foreigns == FALSE);
		index = NULL;
	}

	foreign->referenced_index = index;
	foreign->referenced_table = referenced_table;

	foreign->referenced_table_name = mem_heap_strdup(
		foreign->heap, referenced_table_name);
	dict_mem_referenced_table_name_lookup_set(foreign, TRUE);

	foreign->referenced_col_names = static_cast<const char**>(
		mem_heap_alloc(foreign->heap, i * sizeof(void*)));

	for (i = 0; i < foreign->n_fields; i++) {
		foreign->referenced_col_names[i]
			= mem_heap_strdup(foreign->heap, column_names[i]);
	}

	/* We found an ok constraint definition: add to the lists */

	UT_LIST_ADD_LAST(foreign_list, table->foreign_list, foreign);

	if (referenced_table) {
		UT_LIST_ADD_LAST(referenced_list,
				 referenced_table->referenced_list,
				 foreign);
	}

	goto loop;
}
/**************************************************************************
Determines whether a string starts with the specified keyword.
@return	TRUE if str starts with keyword */
UNIV_INTERN
ibool
dict_str_starts_with_keyword(
/*=========================*/
	THD*		thd,		/*!< in: MySQL thread handle */
	const char*	str,		/*!< in: string to scan for keyword */
	const char*	keyword)	/*!< in: keyword to look for */
{
	struct charset_info_st*	cs = innobase_get_charset(thd);
	ibool			success;

	dict_accept(cs, str, keyword, &success);
	return(success);
}

/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary the foreign
key constraints declared in the string. This function should be called after
the indexes for a table have been created. Each foreign key constraint must
be accompanied with indexes in both participating tables. The indexes are
allowed to contain more fields than mentioned in the constraint.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
dict_create_foreign_constraints(
/*============================*/
	trx_t*		trx,		/*!< in: transaction */
	const char*	sql_string,	/*!< in: table create statement where
					foreign keys are declared like:
					FOREIGN KEY (a, b) REFERENCES
					table2(c, d), table2 can be written
					also with the database
					name before it: test.table2; the
					default database id the database of
					parameter name */
	size_t		sql_length,	/*!< in: length of sql_string */
	const char*	name,		/*!< in: table full name in the
					normalized form
					database_name/table_name */
	ibool		reject_fks)	/*!< in: if TRUE, fail with error
					code DB_CANNOT_ADD_CONSTRAINT if
					any foreign keys are found. */
{
	char*		str;
	dberr_t		err;
	mem_heap_t*	heap;

	ut_a(trx);
	ut_a(trx->mysql_thd);

	str = dict_strip_comments(sql_string, sql_length);
	heap = mem_heap_create(10000);

	err = dict_create_foreign_constraints_low(
		trx, heap, innobase_get_charset(trx->mysql_thd), str, name,
		reject_fks);

	mem_heap_free(heap);
	mem_free(str);

	return(err);
}

/**********************************************************************//**
Parses the CONSTRAINT id's to be dropped in an ALTER TABLE statement.
@return DB_SUCCESS or DB_CANNOT_DROP_CONSTRAINT if syntax error or the
constraint id does not match */
UNIV_INTERN
dberr_t
dict_foreign_parse_drop_constraints(
/*================================*/
	mem_heap_t*	heap,			/*!< in: heap from which we can
						allocate memory */
	trx_t*		trx,			/*!< in: transaction */
	dict_table_t*	table,			/*!< in: table */
	ulint*		n,			/*!< out: number of constraints
						to drop */
	const char***	constraints_to_drop)	/*!< out: id's of the
						constraints to drop */
{
	dict_foreign_t*		foreign;
	ibool			success;
	char*			str;
	size_t			len;
	const char*		ptr;
	const char*		id;
	struct charset_info_st*	cs;

	ut_a(trx);
	ut_a(trx->mysql_thd);

	cs = innobase_get_charset(trx->mysql_thd);

	*n = 0;

	*constraints_to_drop = static_cast<const char**>(
		mem_heap_alloc(heap, 1000 * sizeof(char*)));

	ptr = innobase_get_stmt(trx->mysql_thd, &len);

	str = dict_strip_comments(ptr, len);

	ptr = str;

	ut_ad(mutex_own(&(dict_sys->mutex)));
loop:
	ptr = dict_scan_to(ptr, "DROP");

	if (*ptr == '\0') {
		mem_free(str);

		return(DB_SUCCESS);
	}

	ptr = dict_accept(cs, ptr, "DROP", &success);

	if (!my_isspace(cs, *ptr)) {

		goto loop;
	}

	ptr = dict_accept(cs, ptr, "FOREIGN", &success);

	if (!success || !my_isspace(cs, *ptr)) {

		goto loop;
	}

	ptr = dict_accept(cs, ptr, "KEY", &success);

	if (!success) {

		goto syntax_error;
	}

	ptr = dict_scan_id(cs, ptr, heap, &id, FALSE, TRUE);

	if (id == NULL) {

		goto syntax_error;
	}

	ut_a(*n < 1000);
	(*constraints_to_drop)[*n] = id;
	(*n)++;

	/* Look for the given constraint id */

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign != NULL) {
		if (0 == innobase_strcasecmp(foreign->id, id)
		    || (strchr(foreign->id, '/')
			&& 0 == innobase_strcasecmp(
				id,
				dict_remove_db_name(foreign->id)))) {
			/* Found */
			break;
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}


	if (foreign == NULL) {

		if (!srv_read_only_mode) {
			FILE*	ef = dict_foreign_err_file;

			mutex_enter(&dict_foreign_err_mutex);
			rewind(ef);
			ut_print_timestamp(ef);
			fputs(" Error in dropping of a foreign key "
			      "constraint of table ", ef);
			ut_print_name(ef, NULL, TRUE, table->name);
			fputs(",\nin SQL command\n", ef);
			fputs(str, ef);
			fputs("\nCannot find a constraint with the "
			      "given id ", ef);
			ut_print_name(ef, NULL, FALSE, id);
			fputs(".\n", ef);
			mutex_exit(&dict_foreign_err_mutex);
		}

		mem_free(str);

		return(DB_CANNOT_DROP_CONSTRAINT);
	}

	goto loop;

syntax_error:
	if (!srv_read_only_mode) {
		FILE*	ef = dict_foreign_err_file;

		mutex_enter(&dict_foreign_err_mutex);
		rewind(ef);
		ut_print_timestamp(ef);
		fputs(" Syntax error in dropping of a"
		      " foreign key constraint of table ", ef);
		ut_print_name(ef, NULL, TRUE, table->name);
		fprintf(ef, ",\n"
			"close to:\n%s\n in SQL command\n%s\n", ptr, str);
		mutex_exit(&dict_foreign_err_mutex);
	}

	mem_free(str);

	return(DB_CANNOT_DROP_CONSTRAINT);
}

/*==================== END OF FOREIGN KEY PROCESSING ====================*/

/**********************************************************************//**
Returns an index object if it is found in the dictionary cache.
Assumes that dict_sys->mutex is already being held.
@return	index, NULL if not found */
UNIV_INTERN
dict_index_t*
dict_index_get_if_in_cache_low(
/*===========================*/
	index_id_t	index_id)	/*!< in: index id */
{
	ut_ad(mutex_own(&(dict_sys->mutex)));

	return(dict_index_find_on_id_low(index_id));
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**********************************************************************//**
Returns an index object if it is found in the dictionary cache.
@return	index, NULL if not found */
UNIV_INTERN
dict_index_t*
dict_index_get_if_in_cache(
/*=======================*/
	index_id_t	index_id)	/*!< in: index id */
{
	dict_index_t*	index;

	if (dict_sys == NULL) {
		return(NULL);
	}

	mutex_enter(&(dict_sys->mutex));

	index = dict_index_get_if_in_cache_low(index_id);

	mutex_exit(&(dict_sys->mutex));

	return(index);
}
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

#ifdef UNIV_DEBUG
/**********************************************************************//**
Checks that a tuple has n_fields_cmp value in a sensible range, so that
no comparison can occur with the page number field in a node pointer.
@return	TRUE if ok */
UNIV_INTERN
ibool
dict_index_check_search_tuple(
/*==========================*/
	const dict_index_t*	index,	/*!< in: index tree */
	const dtuple_t*		tuple)	/*!< in: tuple used in a search */
{
	ut_a(index);
	ut_a(dtuple_get_n_fields_cmp(tuple)
	     <= dict_index_get_n_unique_in_tree(index));
	return(TRUE);
}
#endif /* UNIV_DEBUG */

/**********************************************************************//**
Builds a node pointer out of a physical record and a page number.
@return	own: node pointer */
UNIV_INTERN
dtuple_t*
dict_index_build_node_ptr(
/*======================*/
	const dict_index_t*	index,	/*!< in: index */
	const rec_t*		rec,	/*!< in: record for which to build node
					pointer */
	ulint			page_no,/*!< in: page number to put in node
					pointer */
	mem_heap_t*		heap,	/*!< in: memory heap where pointer
					created */
	ulint			level)	/*!< in: level of rec in tree:
					0 means leaf level */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	byte*		buf;
	ulint		n_unique;

	if (dict_index_is_univ(index)) {
		/* In a universal index tree, we take the whole record as
		the node pointer if the record is on the leaf level,
		on non-leaf levels we remove the last field, which
		contains the page number of the child page */

		ut_a(!dict_table_is_comp(index->table));
		n_unique = rec_get_n_fields_old(rec);

		if (level > 0) {
			ut_a(n_unique > 1);
			n_unique--;
		}
	} else {
		n_unique = dict_index_get_n_unique_in_tree(index);
	}

	tuple = dtuple_create(heap, n_unique + 1);

	/* When searching in the tree for the node pointer, we must not do
	comparison on the last field, the page number field, as on upper
	levels in the tree there may be identical node pointers with a
	different page number; therefore, we set the n_fields_cmp to one
	less: */

	dtuple_set_n_fields_cmp(tuple, n_unique);

	dict_index_copy_types(tuple, index, n_unique);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 4));

	mach_write_to_4(buf, page_no);

	field = dtuple_get_nth_field(tuple, n_unique);
	dfield_set_data(field, buf, 4);

	dtype_set(dfield_get_type(field), DATA_SYS_CHILD, DATA_NOT_NULL, 4);

	rec_copy_prefix_to_dtuple(tuple, rec, index, n_unique, heap);
	dtuple_set_info_bits(tuple, dtuple_get_info_bits(tuple)
			     | REC_STATUS_NODE_PTR);

	ut_ad(dtuple_check_typed(tuple));

	return(tuple);
}

/**********************************************************************//**
Copies an initial segment of a physical record, long enough to specify an
index entry uniquely.
@return	pointer to the prefix record */
UNIV_INTERN
rec_t*
dict_index_copy_rec_order_prefix(
/*=============================*/
	const dict_index_t*	index,	/*!< in: index */
	const rec_t*		rec,	/*!< in: record for which to
					copy prefix */
	ulint*			n_fields,/*!< out: number of fields copied */
	byte**			buf,	/*!< in/out: memory buffer for the
					copied prefix, or NULL */
	ulint*			buf_size)/*!< in/out: buffer size */
{
	ulint		n;

	UNIV_PREFETCH_R(rec);

	if (dict_index_is_univ(index)) {
		ut_a(!dict_table_is_comp(index->table));
		n = rec_get_n_fields_old(rec);
	} else {
		n = dict_index_get_n_unique_in_tree(index);
	}

	*n_fields = n;
	return(rec_copy_prefix_to_buf(rec, index, n, buf, buf_size));
}

/**********************************************************************//**
Builds a typed data tuple out of a physical record.
@return	own: data tuple */
UNIV_INTERN
dtuple_t*
dict_index_build_data_tuple(
/*========================*/
	dict_index_t*	index,	/*!< in: index tree */
	rec_t*		rec,	/*!< in: record for which to build data tuple */
	ulint		n_fields,/*!< in: number of data fields */
	mem_heap_t*	heap)	/*!< in: memory heap where tuple created */
{
	dtuple_t*	tuple;

	ut_ad(dict_table_is_comp(index->table)
	      || n_fields <= rec_get_n_fields_old(rec));

	tuple = dtuple_create(heap, n_fields);

	dict_index_copy_types(tuple, index, n_fields);

	rec_copy_prefix_to_dtuple(tuple, rec, index, n_fields, heap);

	ut_ad(dtuple_check_typed(tuple));

	return(tuple);
}

/*********************************************************************//**
Logs an operation to a secondary index that is being created. */
UNIV_INTERN
void
dict_index_online_log(
/*==================*/
	dict_index_t*	index,	/*!< in/out: index, S-locked */
	const dtuple_t*	entry,	/*!< in: index entry */
	trx_id_t	trx_id,	/*!< in: transaction ID or 0 if not known */
	enum row_op	op)	/*!< in: operation */
{
	row_log_online_op(index, entry, trx_id, op);
}

/*********************************************************************//**
Calculates the minimum record length in an index. */
UNIV_INTERN
ulint
dict_index_calc_min_rec_len(
/*========================*/
	const dict_index_t*	index)	/*!< in: index */
{
	ulint	sum	= 0;
	ulint	i;
	ulint	comp	= dict_table_is_comp(index->table);

	if (comp) {
		ulint nullable = 0;
		sum = REC_N_NEW_EXTRA_BYTES;
		for (i = 0; i < dict_index_get_n_fields(index); i++) {
			const dict_col_t*	col
				= dict_index_get_nth_col(index, i);
			ulint	size = dict_col_get_fixed_size(col, comp);
			sum += size;
			if (!size) {
				size = col->len;
				sum += size < 128 ? 1 : 2;
			}
			if (!(col->prtype & DATA_NOT_NULL)) {
				nullable++;
			}
		}

		/* round the NULL flags up to full bytes */
		sum += UT_BITS_IN_BYTES(nullable);

		return(sum);
	}

	for (i = 0; i < dict_index_get_n_fields(index); i++) {
		sum += dict_col_get_fixed_size(
			dict_index_get_nth_col(index, i), comp);
	}

	if (sum > 127) {
		sum += 2 * dict_index_get_n_fields(index);
	} else {
		sum += dict_index_get_n_fields(index);
	}

	sum += REC_N_OLD_EXTRA_BYTES;

	return(sum);
}

/**********************************************************************//**
Prints info of a foreign key constraint. */
static
void
dict_foreign_print_low(
/*===================*/
	dict_foreign_t*	foreign)	/*!< in: foreign key constraint */
{
	ulint	i;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	fprintf(stderr, "  FOREIGN KEY CONSTRAINT %s: %s (",
		foreign->id, foreign->foreign_table_name);

	for (i = 0; i < foreign->n_fields; i++) {
		fprintf(stderr, " %s", foreign->foreign_col_names[i]);
	}

	fprintf(stderr, " )\n"
		"             REFERENCES %s (",
		foreign->referenced_table_name);

	for (i = 0; i < foreign->n_fields; i++) {
		fprintf(stderr, " %s", foreign->referenced_col_names[i]);
	}

	fputs(" )\n", stderr);
}

/**********************************************************************//**
Prints a table data. */
UNIV_INTERN
void
dict_table_print(
/*=============*/
	dict_table_t*	table)	/*!< in: table */
{
	dict_index_t*	index;
	dict_foreign_t*	foreign;
	ulint		i;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	dict_table_stats_lock(table, RW_X_LATCH);

	if (!table->stat_initialized) {
		dict_stats_update_transient(table);
	}

	fprintf(stderr,
		"--------------------------------------\n"
		"TABLE: name %s, id %llu, flags %lx, columns %lu,"
		" indexes %lu, appr.rows " UINT64PF "\n"
		"  COLUMNS: ",
		table->name,
		(ullint) table->id,
		(ulong) table->flags,
		(ulong) table->n_cols,
		(ulong) UT_LIST_GET_LEN(table->indexes),
		table->stat_n_rows);

	for (i = 0; i < (ulint) table->n_cols; i++) {
		dict_col_print_low(table, dict_table_get_nth_col(table, i));
		fputs("; ", stderr);
	}

	putc('\n', stderr);

	index = UT_LIST_GET_FIRST(table->indexes);

	while (index != NULL) {
		dict_index_print_low(index);
		index = UT_LIST_GET_NEXT(indexes, index);
	}

	table->stat_initialized = FALSE;

	dict_table_stats_unlock(table, RW_X_LATCH);

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign != NULL) {
		dict_foreign_print_low(foreign);
		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign != NULL) {
		dict_foreign_print_low(foreign);
		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}
}

/**********************************************************************//**
Prints a column data. */
static
void
dict_col_print_low(
/*===============*/
	const dict_table_t*	table,	/*!< in: table */
	const dict_col_t*	col)	/*!< in: column */
{
	dtype_t	type;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	dict_col_copy_type(col, &type);
	fprintf(stderr, "%s: ", dict_table_get_col_name(table,
							dict_col_get_no(col)));

	dtype_print(&type);
}

/**********************************************************************//**
Prints an index data. */
static
void
dict_index_print_low(
/*=================*/
	dict_index_t*	index)	/*!< in: index */
{
	ib_int64_t	n_vals;
	ulint		i;

	ut_a(index->table->stat_initialized);

	ut_ad(mutex_own(&(dict_sys->mutex)));

	if (index->n_user_defined_cols > 0) {
		n_vals = index->stat_n_diff_key_vals[
			index->n_user_defined_cols - 1];
	} else {
		n_vals = index->stat_n_diff_key_vals[0];
	}

	fprintf(stderr,
		"  INDEX: name %s, id %llu, fields %lu/%lu,"
		" uniq %lu, type %lu\n"
		"   root page %lu, appr.key vals %lu,"
		" leaf pages %lu, size pages %lu\n"
		"   FIELDS: ",
		index->name,
		(ullint) index->id,
		(ulong) index->n_user_defined_cols,
		(ulong) index->n_fields,
		(ulong) index->n_uniq,
		(ulong) index->type,
		(ulong) index->page,
		(ulong) n_vals,
		(ulong) index->stat_n_leaf_pages,
		(ulong) index->stat_index_size);

	for (i = 0; i < index->n_fields; i++) {
		dict_field_print_low(dict_index_get_nth_field(index, i));
	}

	putc('\n', stderr);

#ifdef UNIV_BTR_PRINT
	btr_print_size(index);

	btr_print_index(index, 7);
#endif /* UNIV_BTR_PRINT */
}

/**********************************************************************//**
Prints a field data. */
static
void
dict_field_print_low(
/*=================*/
	const dict_field_t*	field)	/*!< in: field */
{
	ut_ad(mutex_own(&(dict_sys->mutex)));

	fprintf(stderr, " %s", field->name);

	if (field->prefix_len != 0) {
		fprintf(stderr, "(%lu)", (ulong) field->prefix_len);
	}
}

/**********************************************************************//**
Outputs info on a foreign key of a table in a format suitable for
CREATE TABLE. */
UNIV_INTERN
void
dict_print_info_on_foreign_key_in_create_format(
/*============================================*/
	FILE*		file,		/*!< in: file where to print */
	trx_t*		trx,		/*!< in: transaction */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint */
	ibool		add_newline)	/*!< in: whether to add a newline */
{
	const char*	stripped_id;
	ulint	i;

	if (strchr(foreign->id, '/')) {
		/* Strip the preceding database name from the constraint id */
		stripped_id = foreign->id + 1
			+ dict_get_db_name_len(foreign->id);
	} else {
		stripped_id = foreign->id;
	}

	putc(',', file);

	if (add_newline) {
		/* SHOW CREATE TABLE wants constraints each printed nicely
		on its own line, while error messages want no newlines
		inserted. */
		fputs("\n ", file);
	}

	fputs(" CONSTRAINT ", file);
	ut_print_name(file, trx, FALSE, stripped_id);
	fputs(" FOREIGN KEY (", file);

	for (i = 0;;) {
		ut_print_name(file, trx, FALSE, foreign->foreign_col_names[i]);
		if (++i < foreign->n_fields) {
			fputs(", ", file);
		} else {
			break;
		}
	}

	fputs(") REFERENCES ", file);

	if (dict_tables_have_same_db(foreign->foreign_table_name_lookup,
				     foreign->referenced_table_name_lookup)) {
		/* Do not print the database name of the referenced table */
		ut_print_name(file, trx, TRUE,
			      dict_remove_db_name(
				      foreign->referenced_table_name));
	} else {
		ut_print_name(file, trx, TRUE,
			      foreign->referenced_table_name);
	}

	putc(' ', file);
	putc('(', file);

	for (i = 0;;) {
		ut_print_name(file, trx, FALSE,
			      foreign->referenced_col_names[i]);
		if (++i < foreign->n_fields) {
			fputs(", ", file);
		} else {
			break;
		}
	}

	putc(')', file);

	if (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE) {
		fputs(" ON DELETE CASCADE", file);
	}

	if (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL) {
		fputs(" ON DELETE SET NULL", file);
	}

	if (foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION) {
		fputs(" ON DELETE NO ACTION", file);
	}

	if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE) {
		fputs(" ON UPDATE CASCADE", file);
	}

	if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL) {
		fputs(" ON UPDATE SET NULL", file);
	}

	if (foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION) {
		fputs(" ON UPDATE NO ACTION", file);
	}
}

/**********************************************************************//**
Outputs info on foreign keys of a table. */
UNIV_INTERN
void
dict_print_info_on_foreign_keys(
/*============================*/
	ibool		create_table_format, /*!< in: if TRUE then print in
				a format suitable to be inserted into
				a CREATE TABLE, otherwise in the format
				of SHOW TABLE STATUS */
	FILE*		file,	/*!< in: file where to print */
	trx_t*		trx,	/*!< in: transaction */
	dict_table_t*	table)	/*!< in: table */
{
	dict_foreign_t*	foreign;

	mutex_enter(&(dict_sys->mutex));

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	if (foreign == NULL) {
		mutex_exit(&(dict_sys->mutex));

		return;
	}

	while (foreign != NULL) {
		if (create_table_format) {
			dict_print_info_on_foreign_key_in_create_format(
				file, trx, foreign, TRUE);
		} else {
			ulint	i;
			fputs("; (", file);

			for (i = 0; i < foreign->n_fields; i++) {
				if (i) {
					putc(' ', file);
				}

				ut_print_name(file, trx, FALSE,
					      foreign->foreign_col_names[i]);
			}

			fputs(") REFER ", file);
			ut_print_name(file, trx, TRUE,
				      foreign->referenced_table_name);
			putc('(', file);

			for (i = 0; i < foreign->n_fields; i++) {
				if (i) {
					putc(' ', file);
				}
				ut_print_name(
					file, trx, FALSE,
					foreign->referenced_col_names[i]);
			}

			putc(')', file);

			if (foreign->type == DICT_FOREIGN_ON_DELETE_CASCADE) {
				fputs(" ON DELETE CASCADE", file);
			}

			if (foreign->type == DICT_FOREIGN_ON_DELETE_SET_NULL) {
				fputs(" ON DELETE SET NULL", file);
			}

			if (foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION) {
				fputs(" ON DELETE NO ACTION", file);
			}

			if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE) {
				fputs(" ON UPDATE CASCADE", file);
			}

			if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL) {
				fputs(" ON UPDATE SET NULL", file);
			}

			if (foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION) {
				fputs(" ON UPDATE NO ACTION", file);
			}
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	mutex_exit(&(dict_sys->mutex));
}

/********************************************************************//**
Displays the names of the index and the table. */
UNIV_INTERN
void
dict_index_name_print(
/*==================*/
	FILE*			file,	/*!< in: output stream */
	const trx_t*		trx,	/*!< in: transaction */
	const dict_index_t*	index)	/*!< in: index to print */
{
	fputs("index ", file);
	ut_print_name(file, trx, FALSE, index->name);
	fputs(" of table ", file);
	ut_print_name(file, trx, TRUE, index->table_name);
}

/**********************************************************************//**
Find a table in dict_sys->table_LRU list with specified space id
@return table if found, NULL if not */
static
dict_table_t*
dict_find_table_by_space(
/*=====================*/
	ulint	space_id)		/*!< in: space ID */
{
	dict_table_t*   table;
	ulint		num_item;
	ulint		count = 0;

	ut_ad(space_id > 0);

	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	num_item =  UT_LIST_GET_LEN(dict_sys->table_LRU);

	/* This function intentionally does not acquire mutex as it is used
	by error handling code in deep call stack as last means to avoid
	killing the server, so it worth to risk some consequencies for
	the action. */
	while (table && count < num_item) {
		if (table->space == space_id) {
			return(table);
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
		count++;
	}

	return(NULL);
}

/**********************************************************************//**
Flags a table with specified space_id corrupted in the data dictionary
cache
@return TRUE if successful */
UNIV_INTERN
ibool
dict_set_corrupted_by_space(
/*========================*/
	ulint	space_id)		/*!< in: space ID */
{
	dict_table_t*   table;

	table = dict_find_table_by_space(space_id);

	if (!table) {
		return(FALSE);
	}

	/* mark the table->corrupted bit only, since the caller
	could be too deep in the stack for SYS_INDEXES update */
	table->corrupted = TRUE;

	return(TRUE);
}

/**********************************************************************//**
Flags an index corrupted both in the data dictionary cache
and in the SYS_INDEXES */
UNIV_INTERN
void
dict_set_corrupted(
/*===============*/
	dict_index_t*	index)		/*!< in/out: index */
{
	mem_heap_t*	heap;
	mtr_t		mtr;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	byte*		buf;
	const char*	status;
	btr_cur_t	cursor;

	ut_ad(index);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(!dict_table_is_comp(dict_sys->sys_tables));
	ut_ad(!dict_table_is_comp(dict_sys->sys_indexes));

#ifdef UNIV_SYNC_DEBUG
        ut_ad(sync_thread_levels_empty_except_dict());
#endif

	/* Mark the table as corrupted only if the clustered index
	is corrupted */
	if (dict_index_is_clust(index)) {
		index->table->corrupted = TRUE;
	}

	if (index->type & DICT_CORRUPT) {
		/* The index was already flagged corrupted. */
		ut_ad(!dict_index_is_clust(index) || index->table->corrupted);
		return;
	}

	heap = mem_heap_create(sizeof(dtuple_t) + 2 * (sizeof(dfield_t)
			       + sizeof(que_fork_t) + sizeof(upd_node_t)
			       + sizeof(upd_t) + 12));
	mtr_start(&mtr);
	index->type |= DICT_CORRUPT;

	sys_index = UT_LIST_GET_FIRST(dict_sys->sys_indexes->indexes);

	/* Find the index row in SYS_INDEXES */
	tuple = dtuple_create(heap, 2);

	dfield = dtuple_get_nth_field(tuple, 0);
	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, index->table->id);
	dfield_set_data(dfield, buf, 8);

	dfield = dtuple_get_nth_field(tuple, 1);
	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, index->id);
	dfield_set_data(dfield, buf, 8);

	dict_index_copy_types(tuple, sys_index, 2);

	btr_cur_search_to_nth_level(sys_index, 0, tuple, PAGE_CUR_GE,
				    BTR_MODIFY_LEAF,
				    &cursor, 0, __FILE__, __LINE__, &mtr);

	if (cursor.up_match == dtuple_get_n_fields(tuple)) {
		/* UPDATE SYS_INDEXES SET TYPE=index->type
		WHERE TABLE_ID=index->table->id AND INDEX_ID=index->id */
		ulint	len;
		byte*	field	= rec_get_nth_field_old(
			btr_cur_get_rec(&cursor),
			DICT_FLD__SYS_INDEXES__TYPE, &len);
		if (len != 4) {
			goto fail;
		}
		mlog_write_ulint(field, index->type, MLOG_4BYTES, &mtr);
		status = "  InnoDB: Flagged corruption of ";
	} else {
fail:
		status = "  InnoDB: Unable to flag corruption of ";
	}

	mtr_commit(&mtr);
	mem_heap_free(heap);

	ut_print_timestamp(stderr);
	fputs(status, stderr);
	dict_index_name_print(stderr, NULL, index);
	putc('\n', stderr);
}

/**********************************************************************//**
Flags an index corrupted in the data dictionary cache only. This
is used mostly to mark a corrupted index when index's own dictionary
is corrupted, and we force to load such index for repair purpose */
UNIV_INTERN
void
dict_set_corrupted_index_cache_only(
/*================================*/
	dict_index_t*	index,		/*!< in/out: index */
	dict_table_t*	table)		/*!< in/out: table */
{
	ut_ad(index);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(!dict_table_is_comp(dict_sys->sys_tables));
	ut_ad(!dict_table_is_comp(dict_sys->sys_indexes));

	/* Mark the table as corrupted only if the clustered index
	is corrupted */
	if (dict_index_is_clust(index)) {
		dict_table_t*	corrupt_table;

		corrupt_table = table ? table : index->table;
		ut_ad(!index->table || !table || index->table  == table);

		if (corrupt_table) {
			corrupt_table->corrupted = TRUE;
		}
	}

	index->type |= DICT_CORRUPT;
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Inits dict_ind_redundant and dict_ind_compact. */
UNIV_INTERN
void
dict_ind_init(void)
/*===============*/
{
	dict_table_t*		table;

	/* create dummy table and index for REDUNDANT infimum and supremum */
	table = dict_mem_table_create("SYS_DUMMY1", DICT_HDR_SPACE, 1, 0, 0);
	dict_mem_table_add_col(table, NULL, NULL, DATA_CHAR,
			       DATA_ENGLISH | DATA_NOT_NULL, 8);

	dict_ind_redundant = dict_mem_index_create("SYS_DUMMY1", "SYS_DUMMY1",
						   DICT_HDR_SPACE, 0, 1);
	dict_index_add_col(dict_ind_redundant, table,
			   dict_table_get_nth_col(table, 0), 0);
	dict_ind_redundant->table = table;

	/* create dummy table and index for COMPACT infimum and supremum */
	table = dict_mem_table_create("SYS_DUMMY2",
				      DICT_HDR_SPACE, 1,
				      DICT_TF_COMPACT, 0);
	dict_mem_table_add_col(table, NULL, NULL, DATA_CHAR,
			       DATA_ENGLISH | DATA_NOT_NULL, 8);
	dict_ind_compact = dict_mem_index_create("SYS_DUMMY2", "SYS_DUMMY2",
						 DICT_HDR_SPACE, 0, 1);
	dict_index_add_col(dict_ind_compact, table,
			   dict_table_get_nth_col(table, 0), 0);
	dict_ind_compact->table = table;

	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	dict_ind_redundant->cached = dict_ind_compact->cached = TRUE;
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Frees dict_ind_redundant and dict_ind_compact. */
static
void
dict_ind_free(void)
/*===============*/
{
	dict_table_t*	table;

	table = dict_ind_compact->table;
	dict_mem_index_free(dict_ind_compact);
	dict_ind_compact = NULL;
	dict_mem_table_free(table);

	table = dict_ind_redundant->table;
	dict_mem_index_free(dict_ind_redundant);
	dict_ind_redundant = NULL;
	dict_mem_table_free(table);
}

/**********************************************************************//**
Get index by name
@return	index, NULL if does not exist */
UNIV_INTERN
dict_index_t*
dict_table_get_index_on_name(
/*=========================*/
	dict_table_t*	table,	/*!< in: table */
	const char*	name)	/*!< in: name of the index to find */
{
	dict_index_t*	index;

	/* If name is NULL, just return */
	if (!name) {
		return(NULL);
	}

	index = dict_table_get_first_index(table);

	while (index != NULL) {
		if (innobase_strcasecmp(index->name, name) == 0) {

			return(index);
		}

		index = dict_table_get_next_index(index);
	}

	return(NULL);
}

/**********************************************************************//**
Replace the index passed in with another equivalent index in the
foreign key lists of the table. */
UNIV_INTERN
void
dict_foreign_replace_index(
/*=======================*/
	dict_table_t*		table,  /*!< in/out: table */
	const dict_index_t*	index,	/*!< in: index to be replaced */
	const trx_t*		trx)	/*!< in: transaction handle */
{
	dict_foreign_t*	foreign;

	ut_ad(index->to_be_dropped);

	for (foreign = UT_LIST_GET_FIRST(table->foreign_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {

		dict_index_t*	new_index;

		if (foreign->foreign_index == index) {
			ut_ad(foreign->foreign_table == index->table);

			new_index = dict_foreign_find_index(
				foreign->foreign_table,
				foreign->foreign_col_names,
				foreign->n_fields, index,
				/*check_charsets=*/TRUE, /*check_null=*/FALSE);
			/* There must exist an alternative index,
			since this must have been checked earlier. */
			ut_a(new_index || !trx->check_foreigns);
			ut_ad(!new_index || new_index->table == index->table);
			ut_ad(!new_index || !new_index->to_be_dropped);

			foreign->foreign_index = new_index;
		}
	}

	for (foreign = UT_LIST_GET_FIRST(table->referenced_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

		dict_index_t*	new_index;

		if (foreign->referenced_index == index) {
			ut_ad(foreign->referenced_table == index->table);

			new_index = dict_foreign_find_index(
				foreign->referenced_table,
				foreign->referenced_col_names,
				foreign->n_fields, index,
				/*check_charsets=*/TRUE, /*check_null=*/FALSE);
			/* There must exist an alternative index,
			since this must have been checked earlier. */
			ut_a(new_index || !trx->check_foreigns);
			ut_ad(!new_index || new_index->table == index->table);
			ut_ad(!new_index || !new_index->to_be_dropped);

			foreign->referenced_index = new_index;
		}
	}
}

/**********************************************************************//**
In case there is more than one index with the same name return the index
with the min(id).
@return	index, NULL if does not exist */
UNIV_INTERN
dict_index_t*
dict_table_get_index_on_name_and_min_id(
/*=====================================*/
	dict_table_t*	table,	/*!< in: table */
	const char*	name)	/*!< in: name of the index to find */
{
	dict_index_t*	index;
	dict_index_t*	min_index; /* Index with matching name and min(id) */

	min_index = NULL;
	index = dict_table_get_first_index(table);

	while (index != NULL) {
		if (ut_strcmp(index->name, name) == 0) {
			if (!min_index || index->id < min_index->id) {

				min_index = index;
			}
		}

		index = dict_table_get_next_index(index);
	}

	return(min_index);

}

#ifdef UNIV_DEBUG
/**********************************************************************//**
Check for duplicate index entries in a table [using the index name] */
UNIV_INTERN
void
dict_table_check_for_dup_indexes(
/*=============================*/
	const dict_table_t*	table,	/*!< in: Check for dup indexes
					in this table */
	enum check_name		check)	/*!< in: whether and when to allow
					temporary index names */
{
	/* Check for duplicates, ignoring indexes that are marked
	as to be dropped */

	const dict_index_t*	index1;
	const dict_index_t*	index2;

	ut_ad(mutex_own(&dict_sys->mutex));

	/* The primary index _must_ exist */
	ut_a(UT_LIST_GET_LEN(table->indexes) > 0);

	index1 = UT_LIST_GET_FIRST(table->indexes);

	do {
		if (*index1->name == TEMP_INDEX_PREFIX) {
			ut_a(!dict_index_is_clust(index1));

			switch (check) {
			case CHECK_ALL_COMPLETE:
				ut_error;
			case CHECK_ABORTED_OK:
				switch (dict_index_get_online_status(index1)) {
				case ONLINE_INDEX_COMPLETE:
				case ONLINE_INDEX_CREATION:
					ut_error;
					break;
				case ONLINE_INDEX_ABORTED:
				case ONLINE_INDEX_ABORTED_DROPPED:
					break;
				}
				/* fall through */
			case CHECK_PARTIAL_OK:
				break;
			}
		}

		for (index2 = UT_LIST_GET_NEXT(indexes, index1);
		     index2 != NULL;
		     index2 = UT_LIST_GET_NEXT(indexes, index2)) {
			ut_ad(ut_strcmp(index1->name, index2->name));
		}

		index1 = UT_LIST_GET_NEXT(indexes, index1);
	} while (index1);
}
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Checks whether a table exists and whether it has the given structure.
The table must have the same number of columns with the same names and
types. The order of the columns does not matter.
The caller must own the dictionary mutex.
dict_table_schema_check() @{
@return DB_SUCCESS if the table exists and contains the necessary columns */
UNIV_INTERN
dberr_t
dict_table_schema_check(
/*====================*/
	dict_table_schema_t*	req_schema,	/*!< in/out: required table
						schema */
	char*			errstr,		/*!< out: human readable error
						message if != DB_SUCCESS is
						returned */
	size_t			errstr_sz)	/*!< in: errstr size */
{
	char		buf[MAX_FULL_NAME_LEN];
	dict_table_t*	table;
	ulint		i;

	ut_ad(mutex_own(&dict_sys->mutex));

	table = dict_table_get_low(req_schema->table_name);

	if (table == NULL) {
		/* no such table */

		ut_snprintf(errstr, errstr_sz,
			    "Table %s not found.",
			    ut_format_name(req_schema->table_name,
					   TRUE, buf, sizeof(buf)));

		return(DB_TABLE_NOT_FOUND);
	}

	if (table->ibd_file_missing) {
		/* missing tablespace */

		ut_snprintf(errstr, errstr_sz,
			    "Tablespace for table %s is missing.",
			    ut_format_name(req_schema->table_name,
					   TRUE, buf, sizeof(buf)));

		return(DB_TABLE_NOT_FOUND);
	}

	if ((ulint) table->n_def - DATA_N_SYS_COLS != req_schema->n_cols) {
		/* the table has a different number of columns than
		required */

		ut_snprintf(errstr, errstr_sz,
			    "%s has %d columns but should have %lu.",
			    ut_format_name(req_schema->table_name,
					   TRUE, buf, sizeof(buf)),
			    table->n_def - DATA_N_SYS_COLS,
			    req_schema->n_cols);

		return(DB_ERROR);
	}

	/* For each column from req_schema->columns[] search
	whether it is present in table->cols[].
	The following algorithm is O(n_cols^2), but is optimized to
	be O(n_cols) if the columns are in the same order in both arrays. */

	for (i = 0; i < req_schema->n_cols; i++) {
		ulint	j;

		char	req_type[64];
		char	actual_type[64];

		/* check if i'th column is the same in both arrays */
		if (innobase_strcasecmp(req_schema->columns[i].name,
			       dict_table_get_col_name(table, i)) == 0) {

			/* we found the column in table->cols[] quickly */
			j = i;
		} else {

			/* columns in both arrays are not in the same order,
			do a full scan of the second array */
			for (j = 0; j < table->n_def; j++) {
				const char*	name;

				name = dict_table_get_col_name(table, j);

				if (innobase_strcasecmp(name,
					req_schema->columns[i].name) == 0) {

					/* found the column on j'th
					position */
					break;
				}
			}

			if (j == table->n_def) {

				ut_snprintf(errstr, errstr_sz,
					    "required column %s "
					    "not found in table %s.",
					    req_schema->columns[i].name,
					    ut_format_name(
						    req_schema->table_name,
						    TRUE, buf, sizeof(buf)));

				return(DB_ERROR);
			}
		}

		/* we found a column with the same name on j'th position,
		compare column types and flags */

		dtype_sql_name(req_schema->columns[i].mtype,
			       req_schema->columns[i].prtype_mask,
			       req_schema->columns[i].len,
			       req_type, sizeof(req_type));

		dtype_sql_name(table->cols[j].mtype,
			       table->cols[j].prtype,
			       table->cols[j].len,
			       actual_type, sizeof(actual_type));

		/* check length for exact match */
		if (req_schema->columns[i].len != table->cols[j].len) {

			ut_snprintf(errstr, errstr_sz,
				    "Column %s in table %s is %s "
				    "but should be %s (length mismatch).",
				    req_schema->columns[i].name,
				    ut_format_name(req_schema->table_name,
						   TRUE, buf, sizeof(buf)),
				    actual_type, req_type);

			return(DB_ERROR);
		}

		/* check mtype for exact match */
		if (req_schema->columns[i].mtype != table->cols[j].mtype) {

			ut_snprintf(errstr, errstr_sz,
				    "Column %s in table %s is %s "
				    "but should be %s (type mismatch).",
				    req_schema->columns[i].name,
				    ut_format_name(req_schema->table_name,
						   TRUE, buf, sizeof(buf)),
				    actual_type, req_type);

			return(DB_ERROR);
		}

		/* check whether required prtype mask is set */
		if (req_schema->columns[i].prtype_mask != 0
		    && (table->cols[j].prtype
			& req_schema->columns[i].prtype_mask)
		       != req_schema->columns[i].prtype_mask) {

			ut_snprintf(errstr, errstr_sz,
				    "Column %s in table %s is %s "
				    "but should be %s (flags mismatch).",
				    req_schema->columns[i].name,
				    ut_format_name(req_schema->table_name,
						   TRUE, buf, sizeof(buf)),
				    actual_type, req_type);

			return(DB_ERROR);
		}
	}

	if (req_schema->n_foreign != UT_LIST_GET_LEN(table->foreign_list)) {
		ut_snprintf(
			errstr, errstr_sz,
			"Table %s has %lu foreign key(s) pointing to other "
			"tables, but it must have %lu.",
			ut_format_name(req_schema->table_name,
				       TRUE, buf, sizeof(buf)),
			UT_LIST_GET_LEN(table->foreign_list),
			req_schema->n_foreign);
		return(DB_ERROR);
	}

	if (req_schema->n_referenced != UT_LIST_GET_LEN(table->referenced_list)) {
		ut_snprintf(
			errstr, errstr_sz,
			"There are %lu foreign key(s) pointing to %s, "
			"but there must be %lu.",
			UT_LIST_GET_LEN(table->referenced_list),
			ut_format_name(req_schema->table_name,
				       TRUE, buf, sizeof(buf)),
			req_schema->n_referenced);
		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}
/* @} */

/*********************************************************************//**
Converts a database and table name from filesystem encoding
(e.g. d@i1b/a@q1b@1Kc, same format as used in dict_table_t::name) in two
strings in UTF8 encoding (e.g. dцb and aюbØc). The output buffers must be
at least MAX_DB_UTF8_LEN and MAX_TABLE_UTF8_LEN bytes. */
UNIV_INTERN
void
dict_fs2utf8(
/*=========*/
	const char*	db_and_table,	/*!< in: database and table names,
					e.g. d@i1b/a@q1b@1Kc */
	char*		db_utf8,	/*!< out: database name, e.g. dцb */
	size_t		db_utf8_size,	/*!< in: dbname_utf8 size */
	char*		table_utf8,	/*!< out: table name, e.g. aюbØc */
	size_t		table_utf8_size)/*!< in: table_utf8 size */
{
	char	db[MAX_DATABASE_NAME_LEN + 1];
	ulint	db_len;
	uint	errors;

	db_len = dict_get_db_name_len(db_and_table);

	ut_a(db_len <= sizeof(db));

	memcpy(db, db_and_table, db_len);
	db[db_len] = '\0';

	strconvert(
		&my_charset_filename, db,
		system_charset_info, db_utf8, db_utf8_size,
		&errors);

	/* convert each # to @0023 in table name and store the result in buf */
	const char*	table = dict_remove_db_name(db_and_table);
	const char*	table_p;
	char		buf[MAX_TABLE_NAME_LEN * 5 + 1];
	char*		buf_p;
	for (table_p = table, buf_p = buf; table_p[0] != '\0'; table_p++) {
		if (table_p[0] != '#') {
			buf_p[0] = table_p[0];
			buf_p++;
		} else {
			buf_p[0] = '@';
			buf_p[1] = '0';
			buf_p[2] = '0';
			buf_p[3] = '2';
			buf_p[4] = '3';
			buf_p += 5;
		}
		ut_a((size_t) (buf_p - buf) < sizeof(buf));
	}
	buf_p[0] = '\0';

	errors = 0;
	strconvert(
		&my_charset_filename, buf,
		system_charset_info, table_utf8, table_utf8_size,
		&errors);

	if (errors != 0) {
		ut_snprintf(table_utf8, table_utf8_size, "%s%s",
			    srv_mysql50_table_name_prefix, table);
	}
}

/**********************************************************************//**
Closes the data dictionary module. */
UNIV_INTERN
void
dict_close(void)
/*============*/
{
	ulint	i;

	/* Free the hash elements. We don't remove them from the table
	because we are going to destroy the table anyway. */
	for (i = 0; i < hash_get_n_cells(dict_sys->table_hash); i++) {
		dict_table_t*	table;

		table = static_cast<dict_table_t*>(
			HASH_GET_FIRST(dict_sys->table_hash, i));

		while (table) {
			dict_table_t*	prev_table = table;

			table = static_cast<dict_table_t*>(
				HASH_GET_NEXT(name_hash, prev_table));
#ifdef UNIV_DEBUG
			ut_a(prev_table->magic_n == DICT_TABLE_MAGIC_N);
#endif
			/* Acquire only because it's a pre-condition. */
			mutex_enter(&dict_sys->mutex);

			dict_table_remove_from_cache(prev_table);

			mutex_exit(&dict_sys->mutex);
		}
	}

	hash_table_free(dict_sys->table_hash);

	/* The elements are the same instance as in dict_sys->table_hash,
	therefore we don't delete the individual elements. */
	hash_table_free(dict_sys->table_id_hash);

	dict_ind_free();

	mutex_free(&dict_sys->mutex);

	rw_lock_free(&dict_operation_lock);
	memset(&dict_operation_lock, 0x0, sizeof(dict_operation_lock));

	if (!srv_read_only_mode) {
		mutex_free(&dict_foreign_err_mutex);
	}

	mem_free(dict_sys);
	dict_sys = NULL;

	for (i = 0; i < DICT_TABLE_STATS_LATCHES_SIZE; i++) {
		rw_lock_free(&dict_table_stats_latches[i]);
	}
}

# ifdef UNIV_DEBUG
/**********************************************************************//**
Validate the dictionary table LRU list.
@return TRUE if valid  */
static
ibool
dict_lru_validate(void)
/*===================*/
{
	dict_table_t*	table;

	ut_ad(mutex_own(&dict_sys->mutex));

	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		ut_a(table->can_be_evicted);
	}

	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		ut_a(!table->can_be_evicted);
	}

	return(TRUE);
}

/**********************************************************************//**
Check if a table exists in the dict table LRU list.
@return TRUE if table found in LRU list */
static
ibool
dict_lru_find_table(
/*================*/
	const dict_table_t*	find_table)	/*!< in: table to find */
{
	dict_table_t*		table;

	ut_ad(find_table != NULL);
	ut_ad(mutex_own(&dict_sys->mutex));

	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		ut_a(table->can_be_evicted);

		if (table == find_table) {
			return(TRUE);
		}
	}

	return(FALSE);
}

/**********************************************************************//**
Check if a table exists in the dict table non-LRU list.
@return TRUE if table found in non-LRU list */
static
ibool
dict_non_lru_find_table(
/*====================*/
	const dict_table_t*	find_table)	/*!< in: table to find */
{
	dict_table_t*		table;

	ut_ad(find_table != NULL);
	ut_ad(mutex_own(&dict_sys->mutex));

	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		ut_a(!table->can_be_evicted);

		if (table == find_table) {
			return(TRUE);
		}
	}

	return(FALSE);
}
# endif /* UNIV_DEBUG */
/*********************************************************************//**
Check an index to see whether its first fields are the columns in the array,
in the same order and is not marked for deletion and is not the same
as types_idx.
@return true if the index qualifies, otherwise false */
UNIV_INTERN
bool
dict_foreign_qualify_index(
/*=======================*/
        const dict_table_t*     table,  /*!< in: table */
        const char**            columns,/*!< in: array of column names */
        ulint                   n_cols, /*!< in: number of columns */
        const dict_index_t*     index,  /*!< in: index to check */
        const dict_index_t*     types_idx,
                                        /*!< in: NULL or an index
                                        whose types the column types
                                        must match */
        ibool                   check_charsets,
                                        /*!< in: whether to check
                                        charsets.  only has an effect
                                        if types_idx != NULL */
        ulint                   check_null)
                                        /*!< in: nonzero if none of
                                        the columns must be declared
                                        NOT NULL */
{
        ulint   i;

        if (dict_index_get_n_fields(index) < n_cols) {
                return(false);
        }

        for (i= 0; i < n_cols; i++) {
                dict_field_t*   field;
                const char*     col_name;

                field = dict_index_get_nth_field(index, i);

                col_name = dict_table_get_col_name(
                        table, dict_col_get_no(field->col));

                if (field->prefix_len != 0) {
                        /* We do not accept column prefix
                        indexes here */

                        break;
                }

                if (0 != innobase_strcasecmp(columns[i],
                                             col_name)) {
                        break;
                }

                if (check_null
                    && (field->col->prtype & DATA_NOT_NULL)) {

                        break;
                }

                if (types_idx && !cmp_cols_are_equal(
                            dict_index_get_nth_col(index, i),
                            dict_index_get_nth_col(types_idx,
                                                   i),
                            check_charsets)) {

                        break;
                }
        }

        return((i == n_cols) ? true : false);
}

/*********************************************************************//**
Update the state of compression failure padding heuristics. This is
called whenever a compression operation succeeds or fails.
The caller must be holding info->mutex */
static
void
dict_index_zip_pad_update(
/*======================*/
	zip_pad_info_t*	info,	/*<! in/out: info to be updated */
	ulint	zip_threshold)	/*<! in: zip threshold value */
{
	ulint	total;
	ulint	fail_pct;

	ut_ad(info);

	total = info->success + info->failure;

	ut_ad(total > 0);

	if(zip_threshold == 0) {
		/* User has just disabled the padding. */
		return;
	}

	if (total < ZIP_PAD_ROUND_LEN) {
		/* We are in middle of a round. Do nothing. */
		return;
	}

	/* We are at a 'round' boundary. Reset the values but first
	calculate fail rate for our heuristic. */
	fail_pct = (info->failure * 100) / total;
	info->failure = 0;
	info->success = 0;

	if (fail_pct > zip_threshold) {
		/* Compression failures are more then user defined
		threshold. Increase the pad size to reduce chances of
		compression failures. */
		ut_ad(info->pad % ZIP_PAD_INCR == 0);

		/* Only do increment if it won't increase padding
		beyond max pad size. */
		if (info->pad + ZIP_PAD_INCR
		    < (UNIV_PAGE_SIZE * zip_pad_max) / 100) {
#ifdef HAVE_ATOMIC_BUILTINS
			/* Use atomics even though we have the mutex.
			This is to ensure that we are able to read
			info->pad atomically where atomics are
			supported. */
			os_atomic_increment_ulint(&info->pad, ZIP_PAD_INCR);
#else /* HAVE_ATOMIC_BUILTINS */
			info->pad += ZIP_PAD_INCR;
#endif /* HAVE_ATOMIC_BUILTINS */

			MONITOR_INC(MONITOR_PAD_INCREMENTS);
		}

		info->n_rounds = 0;

	} else {
		/* Failure rate was OK. Another successful round
		completed. */
		++info->n_rounds;

		/* If enough successful rounds are completed with
		compression failure rate in control, decrease the
		padding. */
		if (info->n_rounds >= ZIP_PAD_SUCCESSFUL_ROUND_LIMIT
		    && info->pad > 0) {

			ut_ad(info->pad % ZIP_PAD_INCR == 0);
#ifdef HAVE_ATOMIC_BUILTINS
			/* Use atomics even though we have the mutex.
			This is to ensure that we are able to read
			info->pad atomically where atomics are
			supported. */
			os_atomic_decrement_ulint(&info->pad, ZIP_PAD_INCR);
#else /* HAVE_ATOMIC_BUILTINS */
			info->pad -= ZIP_PAD_INCR;
#endif /* HAVE_ATOMIC_BUILTINS */

			info->n_rounds = 0;

			MONITOR_INC(MONITOR_PAD_DECREMENTS);
		}
	}
}

/*********************************************************************//**
This function should be called whenever a page is successfully
compressed. Updates the compression padding information. */
UNIV_INTERN
void
dict_index_zip_success(
/*===================*/
	dict_index_t*	index)	/*!< in/out: index to be updated. */
{
	ut_ad(index);

	ulint zip_threshold = zip_failure_threshold_pct;
	if (!zip_threshold) {
		/* Disabled by user. */
		return;
	}

	os_fast_mutex_lock(&index->zip_pad.mutex);
	++index->zip_pad.success;
	dict_index_zip_pad_update(&index->zip_pad, zip_threshold);
	os_fast_mutex_unlock(&index->zip_pad.mutex);
}

/*********************************************************************//**
This function should be called whenever a page compression attempt
fails. Updates the compression padding information. */
UNIV_INTERN
void
dict_index_zip_failure(
/*===================*/
	dict_index_t*	index)	/*!< in/out: index to be updated. */
{
	ut_ad(index);

	ulint zip_threshold = zip_failure_threshold_pct;
	if (!zip_threshold) {
		/* Disabled by user. */
		return;
	}

	os_fast_mutex_lock(&index->zip_pad.mutex);
	++index->zip_pad.failure;
	dict_index_zip_pad_update(&index->zip_pad, zip_threshold);
	os_fast_mutex_unlock(&index->zip_pad.mutex);
}


/*********************************************************************//**
Return the optimal page size, for which page will likely compress.
@return page size beyond which page might not compress */
UNIV_INTERN
ulint
dict_index_zip_pad_optimal_page_size(
/*=================================*/
	dict_index_t*	index)	/*!< in: index for which page size
				is requested */
{
	ulint	pad;
	ulint	min_sz;
	ulint	sz;

	ut_ad(index);

	if (!zip_failure_threshold_pct) {
		/* Disabled by user. */
		return(UNIV_PAGE_SIZE);
	}

	/* We use atomics to read index->zip_pad.pad. Here we use zero
	as increment as are not changing the value of the 'pad'. On
	platforms where atomics are not available we grab the mutex. */

#ifdef HAVE_ATOMIC_BUILTINS
	pad = os_atomic_increment_ulint(&index->zip_pad.pad, 0);
#else /* HAVE_ATOMIC_BUILTINS */
	os_fast_mutex_lock(&index->zip_pad.mutex);
	pad = index->zip_pad.pad;
	os_fast_mutex_unlock(&index->zip_pad.mutex);
#endif /* HAVE_ATOMIC_BUILTINS */

	ut_ad(pad < UNIV_PAGE_SIZE);
	sz = UNIV_PAGE_SIZE - pad;

	/* Min size allowed by user. */
	ut_ad(zip_pad_max < 100);
	min_sz = (UNIV_PAGE_SIZE * (100 - zip_pad_max)) / 100;

	return(ut_max(sz, min_sz));
}

/*************************************************************//**
Convert table flag to row format string.
@return row format name. */
UNIV_INTERN
const char*
dict_tf_to_row_format_string(
/*=========================*/
	ulint	table_flag)		/*!< in: row format setting */
{
	switch (dict_tf_get_rec_format(table_flag)) {
	case REC_FORMAT_REDUNDANT:
		return("ROW_TYPE_REDUNDANT");
	case REC_FORMAT_COMPACT:
		return("ROW_TYPE_COMPACT");
	case REC_FORMAT_COMPRESSED:
		return("ROW_TYPE_COMPRESSED");
	case REC_FORMAT_DYNAMIC:
		return("ROW_TYPE_DYNAMIC");
	}

	ut_error;
	return(0);
}
#endif /* !UNIV_HOTBACKUP */
