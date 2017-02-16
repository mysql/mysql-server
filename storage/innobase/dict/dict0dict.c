/*****************************************************************************

Copyright (c) 1996, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file dict/dict0dict.c
Data dictionary system

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

#include <my_sys.h>

#include "dict0dict.h"
#include "ut0rbt.h"

#ifdef UNIV_NONINL
#include "dict0dict.ic"
#endif

/** dummy index for ROW_FORMAT=REDUNDANT supremum and infimum records */
UNIV_INTERN dict_index_t*	dict_ind_redundant;
/** dummy index for ROW_FORMAT=COMPACT supremum and infimum records */
UNIV_INTERN dict_index_t*	dict_ind_compact;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
UNIV_INTERN uint	ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/**********************************************************************
Issue a warning that the row is too big. */
void
ib_warn_row_too_big(const dict_table_t*	table);


#ifndef UNIV_HOTBACKUP
#include "buf0buf.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0boot.h"
#include "dict0mem.h"
#include "dict0crea.h"
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
#include "row0merge.h"
#include "m_ctype.h" /* my_isspace() */
#include "ha_prototypes.h" /* innobase_strcasecmp(), innobase_casedn_str()*/
#include "row0upd.h"
#include "m_string.h"
#include "my_sys.h"

#include <ctype.h>

/** the dictionary system */
UNIV_INTERN dict_sys_t*	dict_sys	= NULL;

/** @brief the data dictionary rw-latch protecting dict_sys

table create, drop, etc. reserve this in X-mode; implicit or
backround operations purge, rollback, foreign key checks reserve this
in S-mode; we cannot trust that MySQL protects implicit or background
operations a table drop since MySQL does not know of them; therefore
we need this; NOTE: a transaction which reserves this must keep book
on the mode in trx_struct::dict_operation_lock_mode */
UNIV_INTERN rw_lock_t	dict_operation_lock;

/* Keys to register rwlocks and mutexes with performance schema */
#ifdef UNIV_PFS_RWLOCK
UNIV_INTERN mysql_pfs_key_t	dict_operation_lock_key;
UNIV_INTERN mysql_pfs_key_t	index_tree_rw_lock_key;
UNIV_INTERN mysql_pfs_key_t	dict_table_stats_latch_key;
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
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
Removes a foreign constraint struct from the dictionary cache. */
static
void
dict_foreign_remove_from_cache(
/*===========================*/
	dict_foreign_t*	foreign);	/*!< in, own: foreign constraint */
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
#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Frees a foreign key struct. */
static
void
dict_foreign_free(
/*==============*/
	dict_foreign_t*	foreign);	/*!< in, own: foreign key struct */

/* Stream for storing detailed information about the latest foreign key
and unique key errors */
UNIV_INTERN FILE*	dict_foreign_err_file		= NULL;
/* mutex protecting the foreign and unique error buffers */
UNIV_INTERN mutex_t	dict_foreign_err_mutex;
#endif /* !UNIV_HOTBACKUP */

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
	(&dict_table_stats_latches[ut_fold_ull(table->id) \
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

/********************************************************************//**
Decrements the count of open MySQL handles to a table. */
UNIV_INTERN
void
dict_table_decrement_handle_count(
/*==============================*/
	dict_table_t*	table,		/*!< in/out: table */
	ibool		dict_locked)	/*!< in: TRUE=data dictionary locked */
{
	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(table->n_mysql_handles_opened > 0);

	table->n_mysql_handles_opened--;

	if (!dict_locked) {
		mutex_exit(&dict_sys->mutex);
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

/**********************************************************************//**
Looks for an index with the given table and index id.
NOTE that we do not reserve the dictionary mutex.
@return	index or NULL if not found from cache */
UNIV_INTERN
dict_index_t*
dict_index_get_on_id_low(
/*=====================*/
	dict_table_t*	table,	/*!< in: table */
	index_id_t	id)	/*!< in: index id */
{
	dict_index_t*	index;

	index = dict_table_get_first_index(table);

	while (index) {
		if (id == index->id) {
			/* Found */

			return(index);
		}

		index = dict_table_get_next_index(index);
	}

	return(NULL);
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

/********************************************************************//**
Looks for column n in an index.
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
UNIV_INTERN
ulint
dict_index_get_nth_col_pos(
/*=======================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			n)	/*!< in: column number */
{
	return(dict_index_get_nth_col_or_prefix_pos(index, n, FALSE));
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
dict_table_get_on_id(
/*=================*/
	table_id_t	table_id,	/*!< in: table id */
	trx_t*		trx)		/*!< in: transaction handle */
{
	dict_table_t*	table;

	if (trx->dict_operation_lock_mode == RW_X_LATCH) {

		/* Note: An X latch implies that the transaction
		already owns the dictionary mutex. */

		ut_ad(mutex_own(&dict_sys->mutex));

		return(dict_table_get_on_id_low(table_id));
	}

	mutex_enter(&(dict_sys->mutex));

	table = dict_table_get_on_id_low(table_id);

	mutex_exit(&(dict_sys->mutex));

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

	dict_sys = mem_alloc(sizeof(dict_sys_t));

	mutex_create(dict_sys_mutex_key, &dict_sys->mutex, SYNC_DICT);

	dict_sys->table_hash = hash_create(buf_pool_get_curr_size()
					   / (DICT_POOL_PER_TABLE_HASH
					      * UNIV_WORD_SIZE));
	dict_sys->table_id_hash = hash_create(buf_pool_get_curr_size()
					      / (DICT_POOL_PER_TABLE_HASH
						 * UNIV_WORD_SIZE));
	dict_sys->size = 0;

	UT_LIST_INIT(dict_sys->table_LRU);

	rw_lock_create(dict_operation_lock_key,
		       &dict_operation_lock, SYNC_DICT_OPERATION);

	dict_foreign_err_file = os_file_create_tmpfile();
	ut_a(dict_foreign_err_file);

	mutex_create(dict_foreign_err_mutex_key,
		     &dict_foreign_err_mutex, SYNC_ANY_LATCH);

	for (i = 0; i < DICT_TABLE_STATS_LATCHES_SIZE; i++) {
		rw_lock_create(dict_table_stats_latch_key,
			       &dict_table_stats_latches[i], SYNC_INDEX_TREE);
	}
}

/**********************************************************************//**
Returns a table object and optionally increment its MySQL open handle count.
NOTE! This is a high-level function to be used mainly from outside the
'dict' directory. Inside this directory dict_table_get_low is usually the
appropriate function.
@return	table, NULL if does not exist */
UNIV_INTERN
dict_table_t*
dict_table_get(
/*===========*/
	const char*		table_name,	/*!< in: table name */
	ibool			inc_mysql_count,/*!< in: whether to increment
						the open handle count on the
						table */
	dict_err_ignore_t	ignore_err)	/*!< in: errors to ignore when
						loading the table */
{
	dict_table_t*	table;

	mutex_enter(&(dict_sys->mutex));

	table = dict_table_get_low(table_name, ignore_err);

	if (inc_mysql_count && table) {
		table->n_mysql_handles_opened++;
	}

	mutex_exit(&(dict_sys->mutex));

	if (table != NULL) {
		/* If table->ibd_file_missing == TRUE, this will
		print an error message and return without doing
		anything. */
		dict_update_statistics(
			table,
			TRUE, /* only update stats if not initialized */
			FALSE /* update even if not changed too much */);
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
	dict_table_t*	table,	/*!< in: table */
	mem_heap_t*	heap)	/*!< in: temporary heap */
{
	ulint	fold;
	ulint	id_fold;
	ulint	i;
	ulint	row_len;

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
	/* Add table to LRU list of tables */
	UT_LIST_ADD_FIRST(table_LRU, dict_sys->table_LRU, table);

	dict_sys->size += mem_heap_get_size(table->heap)
		+ strlen(table->name) + 1;
}

/**********************************************************************//**
Looks for an index with the given id. NOTE that we do not reserve
the dictionary mutex: this function is for emergency purposes like
printing info of a corrupt database page!
@return	index or NULL if not found from cache */
UNIV_INTERN
dict_index_t*
dict_index_find_on_id_low(
/*======================*/
	index_id_t	id)	/*!< in: index id */
{
	dict_table_t*	table;
	dict_index_t*	index;

	/* This can happen if the system tablespace is the wrong page size */
	if (dict_sys == NULL) {
		return(NULL);
	}

	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);

	while (table) {
		index = dict_table_get_first_index(table);

		while (index) {
			if (id == index->id) {
				/* Found */

				return(index);
			}

			index = dict_table_get_next_index(index);
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
	}

	return(NULL);
}

/**********************************************************************//**
Renames a table object.
@return	TRUE if success */
UNIV_INTERN
ibool
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
	{
		dict_table_t*	table2;
		HASH_SEARCH(name_hash, dict_sys->table_hash, fold,
			    dict_table_t*, table2, ut_ad(table2->cached),
			    (ut_strcmp(table2->name, new_name) == 0));
		if (UNIV_LIKELY_NULL(table2)) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: dictionary cache"
			      " already contains a table ", stderr);
			ut_print_name(stderr, NULL, TRUE, new_name);
			fputs("\n"
			      "InnoDB: cannot rename table ", stderr);
			ut_print_name(stderr, NULL, TRUE, old_name);
			putc('\n', stderr);
			return(FALSE);
		}
	}

	/* If the table is stored in a single-table tablespace, rename the
	.ibd file */

	if (table->space != 0) {
		if (table->dir_path_of_temp_table != NULL) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: trying to rename a"
			      " TEMPORARY TABLE ", stderr);
			ut_print_name(stderr, NULL, TRUE, old_name);
			fputs(" (", stderr);
			ut_print_filename(stderr,
					  table->dir_path_of_temp_table);
			fputs(" )\n", stderr);
			return(FALSE);
		} else if (!fil_rename_tablespace(old_name, table->space,
						  new_name)) {
			return(FALSE);
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
		table->name = ut_realloc(table->name, MAX_FULL_NAME_LEN + 1);
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

		if (table->referenced_rbt != NULL) {
			rbt_clear(table->referenced_rbt);
		}

		return(TRUE);
	}

	/* Update the table name fields in foreign constraints, and update also
	the constraint id of new format >= 4.0.18 constraints. Note that at
	this point we have already changed table->name to the new name. */

	foreign = UT_LIST_GET_FIRST(table->foreign_list);

	while (foreign != NULL) {

		/* The id will be changed.  So remove old one */
		rbt_delete(foreign->foreign_table->foreign_rbt, foreign->id);

		if (foreign->referenced_table) {
			rbt_delete(foreign->referenced_table->referenced_rbt,
				   foreign->id);
		}

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
			/* This is a >= 4.0.18 format id */

			ulint	db_len;
			char*	old_id;
			char    old_name_cs_filename[MAX_TABLE_NAME_LEN+20];
			uint    errors = 0;

			/* All table names are internally stored in charset
			my_charset_filename (except the temp tables and the
			partition identifier suffix in partition tables). The
			foreign key constraint names are internally stored
			in UTF-8 charset.  The variable fkid here is used
			to store foreign key constraint name in charset
			my_charset_filename for comparison further below. */
			char    fkid[MAX_TABLE_NAME_LEN+20];
			ibool	on_tmp = FALSE;

			/* The old table name in my_charset_filename is stored
			in old_name_cs_filename */

			strncpy(old_name_cs_filename, old_name,
				MAX_TABLE_NAME_LEN);
			if (strstr(old_name, TEMP_TABLE_PATH_PREFIX) == NULL) {

				innobase_convert_to_system_charset(
					strchr(old_name_cs_filename, '/') + 1,
					strchr(old_name, '/') + 1,
					MAX_TABLE_NAME_LEN, &errors);

				if (errors) {
					/* There has been an error to convert
					old table into UTF-8.  This probably
					means that the old table name is
					actually in UTF-8. */
					innobase_convert_to_filename_charset(
						strchr(old_name_cs_filename,
						       '/') + 1,
						strchr(old_name, '/') + 1,
						MAX_TABLE_NAME_LEN);
				} else {
					/* Old name already in
					my_charset_filename */
					strncpy(old_name_cs_filename, old_name,
						MAX_TABLE_NAME_LEN);
				}
			}

			strncpy(fkid, foreign->id, MAX_TABLE_NAME_LEN);

			if (strstr(fkid, TEMP_TABLE_PATH_PREFIX) == NULL) {
				innobase_convert_to_filename_charset(
					strchr(fkid, '/') + 1,
					strchr(foreign->id, '/') + 1,
					MAX_TABLE_NAME_LEN+20);
			} else {
				on_tmp = TRUE;
			}

			old_id = mem_strdup(foreign->id);

			if (ut_strlen(fkid) > ut_strlen(old_name_cs_filename)
			    + ((sizeof dict_ibfk) - 1)
			    && !memcmp(fkid, old_name_cs_filename,
				       ut_strlen(old_name_cs_filename))
			    && !memcmp(fkid + ut_strlen(old_name_cs_filename),
				       dict_ibfk, (sizeof dict_ibfk) - 1)) {

				/* This is a generated >= 4.0.18 format id */

				char	table_name[MAX_TABLE_NAME_LEN] = "";
				uint	errors = 0;

				if (strlen(table->name) > strlen(old_name)) {
					foreign->id = mem_heap_alloc(
						foreign->heap,
						strlen(table->name)
						+ strlen(old_id) + 1);
				}

				/* Convert the table name to UTF-8 */
				strncpy(table_name, table->name,
					MAX_TABLE_NAME_LEN);
				innobase_convert_to_system_charset(
					strchr(table_name, '/') + 1,
					strchr(table->name, '/') + 1,
					MAX_TABLE_NAME_LEN, &errors);

				if (errors) {
					/* Table name could not be converted
					from charset my_charset_filename to
					UTF-8. This means that the table name
					is already in UTF-8 (#mysql#50). */
					strncpy(table_name, table->name,
						MAX_TABLE_NAME_LEN);
				}

				/* Replace the prefix 'databasename/tablename'
				with the new names */
				strcpy(foreign->id, table_name);
				if (on_tmp) {
					strcat(foreign->id,
					       old_id + ut_strlen(old_name));
				} else {
					sprintf(strchr(foreign->id, '/') + 1,
						"%s%s",
						strchr(table_name, '/') +1,
						strstr(old_id, "_ibfk_") );
				}

			} else {
				/* This is a >= 4.0.18 format id where the user
				gave the id name */
				db_len = dict_get_db_name_len(table->name) + 1;

				if (dict_get_db_name_len(table->name)
				    > dict_get_db_name_len(foreign->id)) {

					foreign->id = mem_heap_alloc(
						foreign->heap,
						db_len + strlen(old_id) + 1);
				}

				/* Replace the database prefix in id with the
				one from table->name */

				ut_memcpy(foreign->id, table->name, db_len);

				strcpy(foreign->id + db_len,
				       dict_remove_db_name(old_id));
			}

			mem_free(old_id);
		}

		rbt_insert(foreign->foreign_table->foreign_rbt,
			   foreign->id, &foreign);

		if (foreign->referenced_table) {
			rbt_insert(foreign->referenced_table->referenced_rbt,
				   foreign->id, &foreign);
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

	return(TRUE);
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
UNIV_INTERN
void
dict_table_remove_from_cache(
/*=========================*/
	dict_table_t*	table)	/*!< in, own: table */
{
	dict_foreign_t*	foreign;
	dict_index_t*	index;
	ulint		size;

	ut_ad(table);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

#if 0
	fputs("Removing table ", stderr);
	ut_print_name(stderr, table->name, ULINT_UNDEFINED);
	fputs(" from dictionary cache\n", stderr);
#endif

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

	/* Remove the indexes from the cache */
	index = UT_LIST_GET_LAST(table->indexes);

	while (index != NULL) {
		dict_index_remove_from_cache(table, index);
		index = UT_LIST_GET_LAST(table->indexes);
	}

	/* Remove table from the hash tables of tables */
	HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
		    ut_fold_string(table->name), table);
	HASH_DELETE(dict_table_t, id_hash, dict_sys->table_id_hash,
		    ut_fold_ull(table->id), table);

	/* Remove table from LRU list of tables */
	UT_LIST_REMOVE(table_LRU, dict_sys->table_LRU, table);

	size = mem_heap_get_size(table->heap) + strlen(table->name) + 1;

	ut_ad(dict_sys->size >= size);

	dict_sys->size -= size;

	dict_mem_table_free(table);
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

	if (UNIV_UNLIKELY(!clust_index)) {
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
				   >= DICT_TF_FORMAT_ZIP);

				max_size = ut_min(max_prefix, max_size);
			}

			max_size += BTR_EXTERN_FIELD_REF_SIZE;
		}

		undo_page_len += 5 + max_size;
	}

	return(undo_page_len >= UNIV_PAGE_SIZE);
}

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

	DBUG_EXECUTE_IF(
		"ib_force_create_table",
		return(FALSE););

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
						   zip_size);
		if (page_rec_max) {
			page_rec_max--;
		}
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
ulint
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

	ut_ad(mem_heap_validate(index->heap));
	ut_a(!dict_index_is_clust(index)
	     || UT_LIST_GET_LEN(table->indexes) == 0);

	if (!dict_index_find_cols(table, index)) {

		dict_mem_index_free(index);
		return(DB_CORRUPTION);
	}

	/* Build the cache internal representation of the index,
	containing also the added system fields */

	if (dict_index_is_clust(index)) {
		new_index = dict_index_build_internal_clust(table, index);
	} else {
		new_index = dict_index_build_internal_non_clust(table, index);
	}

	/* Set the n_fields value in new_index to the actual defined
	number of fields in the cache internal representation */

	new_index->n_fields = new_index->n_def;

	if (dict_index_too_big_for_tree(table, new_index)) {

		if (strict) {
too_big:
			dict_mem_index_free(new_index);
			dict_mem_index_free(index);
			return(DB_TOO_BIG_RECORD);
		} else {

			ib_warn_row_too_big(table);

		}
	}

	if (UNIV_UNLIKELY(index->type & DICT_UNIVERSAL)) {
		n_ord = new_index->n_fields;
	} else {
		n_ord = new_index->n_uniq;
	}

	switch (dict_table_get_format(table)) {
	case DICT_TF_FORMAT_51:
		/* ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT store
		prefixes of externally stored columns locally within
		the record.  There are no special considerations for
		the undo log record size. */
		goto undo_size_ok;

	case DICT_TF_FORMAT_ZIP:
		/* In ROW_FORMAT=DYNAMIC and ROW_FORMAT=COMPRESSED,
		column prefix indexes require that prefixes of
		externally stored columns are written to the undo log.
		This may make the undo log record bigger than the
		record on the B-tree page.  The maximum size of an
		undo log record is the page size.  That must be
		checked for below. */
		break;

#if DICT_TF_FORMAT_ZIP != DICT_TF_FORMAT_MAX
# error "DICT_TF_FORMAT_ZIP != DICT_TF_FORMAT_MAX"
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
	/* Flag the ordering columns and also set column max_prefix */

	for (i = 0; i < n_ord; i++) {
		const dict_field_t*	field
			= dict_index_get_nth_field(new_index, i);

		field->col->ord_part = 1;

		if (field->prefix_len > field->col->max_prefix) {
			field->col->max_prefix = field->prefix_len;
		}
	}

	/* Add the new index as the last index for the table */

	UT_LIST_ADD_LAST(indexes, table->indexes, new_index);
	new_index->table = table;
	new_index->table_name = table->name;

	new_index->search_info = btr_search_info_create(new_index->heap);

	new_index->stat_index_size = 1;
	new_index->stat_n_leaf_pages = 1;

	new_index->page = page_no;
	rw_lock_create(index_tree_rw_lock_key, &new_index->lock,
		       dict_index_is_ibuf(index)
		       ? SYNC_IBUF_INDEX_TREE : SYNC_INDEX_TREE);

	DBUG_EXECUTE_IF(
		"index_partially_created_should_kick",
		DEBUG_SYNC_C("index_partially_created");
	);

	if (!UNIV_UNLIKELY(new_index->type & DICT_UNIVERSAL)) {

		new_index->stat_n_diff_key_vals = mem_heap_alloc(
			new_index->heap,
			(1 + dict_index_get_n_unique(new_index))
			* sizeof(ib_int64_t));

		new_index->stat_n_non_null_key_vals = mem_heap_zalloc(
			new_index->heap,
			(1 + dict_index_get_n_unique(new_index))
			* sizeof(*new_index->stat_n_non_null_key_vals));

		/* Give some sensible values to stat_n_... in case we do
		not calculate statistics quickly enough */

		for (i = 0; i <= dict_index_get_n_unique(new_index); i++) {

			new_index->stat_n_diff_key_vals[i] = 100;
		}
	}

	dict_sys->size += mem_heap_get_size(new_index->heap);

	dict_mem_index_free(index);

	return(DB_SUCCESS);
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
	ulint		size;
	ulint		retries = 0;
	btr_search_t*	info;

	ut_ad(table && index);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	/* We always create search info whether or not adaptive
	hash index is enabled or not. */
	info = index->search_info;
	ut_ad(info);

	/* We are not allowed to free the in-memory index struct
 	dict_index_t until all entries in the adaptive hash index
	that point to any of the page belonging to his b-tree index
	are dropped. This is so because dropping of these entries
	require access to dict_index_t struct. To avoid such scenario
	We keep a count of number of such pages in the search_info and
	only free the dict_index_t struct when this count drops to
	zero. */

	for (;;) {
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
	}

	rw_lock_free(&index->lock);

	/* Remove the index from the list of indexes of the table */
	UT_LIST_REMOVE(indexes, table->indexes, index);

	size = mem_heap_get_size(index->heap);

	ut_ad(dict_sys->size >= size);

	dict_sys->size -= size;

	dict_mem_index_free(index);
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
			if (!innobase_strcasecmp(dict_table_get_col_name(table, j),
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

	if (UNIV_UNLIKELY(index->type & DICT_UNIVERSAL)) {
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

	if (UNIV_UNLIKELY(index->type & DICT_UNIVERSAL)) {
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
	indexed = mem_zalloc(table->n_cols * sizeof *indexed);

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
	ut_ad(!(clust_index->type & DICT_UNIVERSAL));

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
	indexed = mem_zalloc(table->n_cols * sizeof *indexed);

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

#ifndef UNIV_HOTBACKUP
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

		if (foreign->foreign_index == index
		    || foreign->referenced_index == index) {

			return(foreign);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Frees a foreign key struct. */
static
void
dict_foreign_free(
/*==============*/
	dict_foreign_t*	foreign)	/*!< in, own: foreign key struct */
{
	ut_a(foreign->foreign_table->n_foreign_key_checks_running == 0);

	mem_heap_free(foreign->heap);
}

/**********************************************************************//**
Removes a foreign constraint struct from the dictionary cache. */
static
void
dict_foreign_remove_from_cache(
/*===========================*/
	dict_foreign_t*	foreign)	/*!< in, own: foreign constraint */
{
	DBUG_ENTER("dict_foreign_remove_from_cache");

	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_a(foreign);

	if (foreign->referenced_table) {
		ib_rbt_t*	rbt;

		UT_LIST_REMOVE(referenced_list,
			       foreign->referenced_table->referenced_list,
			       foreign);

		rbt = foreign->referenced_table->referenced_rbt;

		if (rbt != NULL && foreign->id != NULL) {
			const ib_rbt_node_t*	node
				= rbt_lookup(rbt, foreign->id);

			if (node != NULL) {
				dict_foreign_t*	val
					= *(dict_foreign_t**) node->value;

				if (val == foreign) {
					rbt_delete(rbt, foreign->id);
				}
			}
		}
	}

	if (foreign->foreign_table) {
		ib_rbt_t*	rbt;

		UT_LIST_REMOVE(foreign_list,
			       foreign->foreign_table->foreign_list,
			       foreign);
		rbt = foreign->foreign_table->foreign_rbt;

		if (rbt != NULL && foreign->id != NULL) {
			const ib_rbt_node_t*	node
				= rbt_lookup(rbt, foreign->id);

			if (node != NULL) {
				dict_foreign_t*	val
					= *(dict_foreign_t**) node->value;

				if (val == foreign) {
					rbt_delete(rbt, foreign->id);
				}
			}
		}
	}

	dict_foreign_free(foreign);

	DBUG_VOID_RETURN;
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
	const ib_rbt_node_t*	node;

	DBUG_ENTER("dict_foreign_find");

	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(dict_table_check_foreign_keys(table));

	if (table->foreign_rbt != NULL) {
		ut_a(UT_LIST_GET_LEN(table->foreign_list)
		     == rbt_size(table->foreign_rbt));
		node = rbt_lookup(table->foreign_rbt, id);
		if (node != NULL) {
			DBUG_RETURN(*(dict_foreign_t**) node->value);
		}
	} else {
		ut_a(UT_LIST_GET_LEN(table->foreign_list) == 0);
	}

	if (table->referenced_rbt != NULL) {
		ut_a(UT_LIST_GET_LEN(table->referenced_list)
		     == rbt_size(table->referenced_rbt));
		node = rbt_lookup(table->referenced_rbt, id);
		if (node != NULL) {
			DBUG_RETURN(*(dict_foreign_t**) node->value);
		}
	} else {
		ut_a(UT_LIST_GET_LEN(table->referenced_list) == 0);
	}

	DBUG_RETURN(NULL);
}

#define  DB_FOREIGN_KEY_IS_PREFIX_INDEX 200
#define  DB_FOREIGN_KEY_COL_NOT_NULL    201
#define  DB_FOREIGN_KEY_COLS_NOT_EQUAL  202
#define  DB_FOREIGN_KEY_INDEX_NOT_FOUND 203

/*********************************************************************//**
Tries to find an index whose first fields are the columns in the array,
in the same order and is not marked for deletion and is not the same
as types_idx.
@return	matching index, NULL if not found */
static
dict_index_t*
dict_foreign_find_index(
/*====================*/
	dict_table_t*	table,	/*!< in: table */
	const char**	columns,/*!< in: array of column names */
	ulint		n_cols,	/*!< in: number of columns */
	dict_index_t*	types_idx, /*!< in: NULL or an index to whose types the
				   column types must match */
	ibool		check_charsets,
				/*!< in: whether to check charsets.
				only has an effect if types_idx != NULL */
	ulint		check_null,
				/*!< in: nonzero if none of the columns must
				be declared NOT NULL */
	ulint*		error,	/*!< out: error code */
	ulint*		err_col_no,
				/*!< out: column number where error happened */
	dict_index_t**	err_index)
			        /*!< out: index where error happened */
{
	dict_index_t*	index;

	if (error) {
		*error = DB_FOREIGN_KEY_INDEX_NOT_FOUND;
	}

	index = dict_table_get_first_index(table);

	while (index != NULL) {
		/* Ignore matches that refer to the same instance
		or the index is to be dropped */
		if (index->to_be_dropped || types_idx == index) {

			goto next_rec;

		} else if (dict_index_get_n_fields(index) >= n_cols) {
			ulint		i;

			for (i = 0; i < n_cols; i++) {
				dict_field_t*	field;
				const char*	col_name;

				field = dict_index_get_nth_field(index, i);

				col_name = dict_table_get_col_name(
					table, dict_col_get_no(field->col));

				if (field->prefix_len != 0) {
					/* We do not accept column prefix
					indexes here */

					if (error && err_col_no && err_index) {
						*error = DB_FOREIGN_KEY_IS_PREFIX_INDEX;
						*err_col_no = i;
						*err_index = index;
					}

					break;
				}

				if (0 != innobase_strcasecmp(columns[i],
							     col_name)) {
					break;
				}

				if (check_null
				    && (field->col->prtype & DATA_NOT_NULL)) {

					if (error && err_col_no && err_index) {
						*error = DB_FOREIGN_KEY_COL_NOT_NULL;
						*err_col_no = i;
						*err_index = index;
					}
					return(NULL);
				}

				if (types_idx && !cmp_cols_are_equal(
					    dict_index_get_nth_col(index, i),
					    dict_index_get_nth_col(types_idx,
								   i),
					    check_charsets)) {

					if (error && err_col_no && err_index) {
						*error = DB_FOREIGN_KEY_COLS_NOT_EQUAL;
						*err_col_no = i;
						*err_index = index;
					}

					break;
				}
			}

			if (i == n_cols) {
				/* We found a matching index */

				if (error) {
					*error = DB_SUCCESS;
				}

				return(index);
			}
		}

next_rec:
		index = dict_table_get_next_index(index);
	}

	return(NULL);
}

/**********************************************************************//**
Find an index that is equivalent to the one passed in and is not marked
for deletion.
@return	index equivalent to foreign->foreign_index, or NULL */
UNIV_INTERN
dict_index_t*
dict_foreign_find_equiv_index(
/*==========================*/
	dict_foreign_t*	foreign)/*!< in: foreign key */
{
	ut_a(foreign != NULL);

	/* Try to find an index which contains the columns as the
	first fields and in the right order, and the types are the
	same as in foreign->foreign_index */

	return(dict_foreign_find_index(
		       foreign->foreign_table,
		       foreign->foreign_col_names, foreign->n_fields,
		       foreign->foreign_index, TRUE, /* check types */
		       FALSE/* allow columns to be NULL */, NULL, NULL, NULL));
}

#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Returns an index object by matching on the name and column names and
if more than one index matches return the index with the max id
@return	matching index, NULL if not found */
UNIV_INTERN
dict_index_t*
dict_table_get_index_by_max_id(
/*===========================*/
	dict_table_t*	table,	/*!< in: table */
	const char*	name,	/*!< in: the index name to find */
	const char**	columns,/*!< in: array of column names */
	ulint		n_cols)	/*!< in: number of columns */
{
	dict_index_t*	index;
	dict_index_t*	found;

	found = NULL;
	index = dict_table_get_first_index(table);

	while (index != NULL) {
		if (ut_strcmp(index->name, name) == 0
		    && dict_index_get_n_ordering_defined_by_user(index)
		    == n_cols) {

			ulint		i;

			for (i = 0; i < n_cols; i++) {
				dict_field_t*	field;
				const char*	col_name;

				field = dict_index_get_nth_field(index, i);

				col_name = dict_table_get_col_name(
					table, dict_col_get_no(field->col));

				if (0 != innobase_strcasecmp(
					    columns[i], col_name)) {

					break;
				}
			}

			if (i == n_cols) {
				/* We found a matching index, select
				the index with the higher id*/

				if (!found || index->id > found->id) {

					found = index;
				}
			}
		}

		index = dict_table_get_next_index(index);
	}

	return(found);
}

#ifndef UNIV_HOTBACKUP
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
ulint
dict_foreign_add_to_cache(
/*======================*/
	dict_foreign_t*		foreign,	/*!< in, own: foreign key
						constraint */
	ibool			check_charsets,	/*!< in: TRUE=check charset
						compatibility */
	dict_err_ignore_t	ignore_err)	/*!< in: error to be ignored */
{
	dict_table_t*	for_table;
	dict_table_t*	ref_table;
	dict_foreign_t*	for_in_cache		= NULL;
	dict_index_t*	index;
	ibool		added_to_referenced_list= FALSE;
	FILE*		ef			= dict_foreign_err_file;

	DBUG_ENTER("dict_foreign_add_to_cache");

	ut_ad(mutex_own(&(dict_sys->mutex)));

	for_table = dict_table_check_if_in_cache_low(
		foreign->foreign_table_name_lookup);

	ref_table = dict_table_check_if_in_cache_low(
		foreign->referenced_table_name_lookup);
	ut_a(for_table || ref_table);

	if (ref_table != NULL && ref_table->referenced_rbt == NULL) {
		dict_table_init_referenced_rbt(ref_table);
	}

	if (for_table) {
		if (for_table->foreign_rbt == NULL) {
			dict_table_init_foreign_rbt(for_table);
		}
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
		ulint index_error;
		ulint err_col;
		dict_index_t *err_index=NULL;

		index = dict_foreign_find_index(
			ref_table,
			for_in_cache->referenced_col_names,
			for_in_cache->n_fields, for_in_cache->foreign_index,
			check_charsets, FALSE, &index_error, &err_col, &err_index);

		if (index == NULL
		    && !(ignore_err & DICT_ERR_IGNORE_FK_NOKEY)) {
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

			DBUG_RETURN(DB_CANNOT_ADD_CONSTRAINT);
		}

		for_in_cache->referenced_table = ref_table;
		for_in_cache->referenced_index = index;

		UT_LIST_ADD_LAST(referenced_list,
				 ref_table->referenced_list, for_in_cache);
		added_to_referenced_list = TRUE;

		rbt_insert(ref_table->referenced_rbt,
			   for_in_cache->id, &for_in_cache);
	}

	if (for_in_cache->foreign_table == NULL && for_table) {
		ulint index_error;
		ulint err_col;
		dict_index_t* err_index=NULL;

		index = dict_foreign_find_index(
			for_table,
			for_in_cache->foreign_col_names,
			for_in_cache->n_fields,
			for_in_cache->referenced_index, check_charsets,
			for_in_cache->type
			& (DICT_FOREIGN_ON_DELETE_SET_NULL
				| DICT_FOREIGN_ON_UPDATE_SET_NULL),
			&index_error, &err_col, &err_index);

		if (index == NULL
		    && !(ignore_err & DICT_ERR_IGNORE_FK_NOKEY)) {
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
					rbt_delete(ref_table->referenced_rbt,
						   for_in_cache->id);
				}

				mem_heap_free(foreign->heap);
			}

			DBUG_RETURN(DB_CANNOT_ADD_CONSTRAINT);
		}

		for_in_cache->foreign_table = for_table;
		for_in_cache->foreign_index = index;

		UT_LIST_ADD_LAST(foreign_list,
				 for_table->foreign_list,
				 for_in_cache);

		rbt_insert(for_table->foreign_rbt, for_in_cache->id,
			   &for_in_cache);
	}

	DBUG_RETURN(DB_SUCCESS);
}

#endif /* !UNIV_HOTBACKUP */
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
	ibool	escape	= FALSE;

	for (; *ptr; ptr++) {
		if (*ptr == quote) {
			/* Closing quote character: do not look for
			starting quote or the keyword. */

			/* If the quote character is escaped by a
			backslash, ignore it. */
			if (escape) {
				escape = FALSE;
			} else {
				quote = '\0';
			}
		} else if (quote) {
			/* Within quotes: do nothing. */
			if (escape) {
				escape = FALSE;
			} else if (*ptr == '\\') {
				escape = TRUE;
			}
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
		str = d = mem_heap_alloc(heap, len + 1);
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
		*id = dst = mem_heap_alloc(heap, len);

		innobase_convert_from_id(cs, dst, str, len);
	} else if (!strncmp(str, srv_mysql50_table_name_prefix,
			    sizeof srv_mysql50_table_name_prefix)) {
		/* This is a pre-5.1 table name
		containing chars other than [A-Za-z0-9].
		Discard the prefix and use raw UTF-8 encoding. */
		str += sizeof srv_mysql50_table_name_prefix;
		len -= sizeof srv_mysql50_table_name_prefix;
		goto convert_id;
	} else {
		/* Encode using filename-safe characters. */
		len = 5 * len + 1;
		*id = dst = mem_heap_alloc(heap, len);

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
	ulint		table_name_len;
	const char*	scan_name;
	char*		ref;

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

	if (database_name == NULL) {
		/* Use the database name of the foreign key table */

		database_name = name;
		database_name_len = dict_get_db_name_len(name);
	}

	table_name_len = strlen(table_name);

	/* Copy database_name, '/', table_name, '\0' */
	ref = mem_heap_alloc(heap, database_name_len + table_name_len + 2);
	memcpy(ref, database_name, database_name_len);
	ref[database_name_len] = '/';
	memcpy(ref + database_name_len + 1, table_name, table_name_len + 1);

	/* Values;  0 = Store and compare as given; case sensitive
	            1 = Store and compare in lower; case insensitive
	            2 = Store as given, compare in lower; case semi-sensitive */
	if (innobase_get_lower_case_table_names() == 2) {
		innobase_casedn_str(ref);
		*table = dict_table_get_low(ref, DICT_ERR_IGNORE_NONE);
		memcpy(ref, database_name, database_name_len);
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
		*table = dict_table_get_low(ref, DICT_ERR_IGNORE_NONE);
	}

	*success = TRUE;
	*ref_name = ref;
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
	ibool		escape = FALSE;

	DBUG_ENTER("dict_strip_comments");

	DBUG_PRINT("dict_strip_comments", ("%s", sql_string));

	str = mem_alloc(sql_length + 1);

	sptr = sql_string;
	ptr = str;

	for (;;) {
scan_more:
		if (sptr >= eptr || *sptr == '\0') {
end_of_string:
			*ptr = '\0';

			ut_a(ptr <= str + sql_length);

			DBUG_PRINT("dict_strip_comments", ("%s", str));
			DBUG_RETURN(str);
		}

		if (*sptr == quote) {
			/* Closing quote character: do not look for
			starting quote or comments. */

			/* If the quote character is escaped by a
			backslash, ignore it. */
			if (escape) {
				escape = FALSE;
			} else {
				quote = 0;
			}
		} else if (quote) {
			/* Within quotes: do not look for
			starting quotes or comments. */
			if (escape) {
				escape = FALSE;
			} else if (*sptr == '\\') {
				escape = TRUE;
			}
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

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Finds the highest [number] for foreign key constraints of the table. Looks
only at the >= 4.0.18-format id's, which are of the form
databasename/tablename_ibfk_[number].
@return	highest number, 0 if table has no new format foreign key constraints */
static
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
	const char*     fmt,		/*!< in: syntax err msg */
	const char*	oper,		/*!< in: operation */
	const char*	name,		/*!< in: table name */
	const char*	start_of_latest_foreign,
					/*!< in: start of the foreign key clause
					in the SQL string */
	const char*	ptr)		/*!< in: place of the syntax error */
{
	FILE*	ef = dict_foreign_err_file;

	mutex_enter(&dict_foreign_err_mutex);
	dict_foreign_error_report_low(ef, name);
	fprintf(ef, fmt, oper, name, start_of_latest_foreign, ptr);
	mutex_exit(&dict_foreign_err_mutex);
}

/*********************************************************************//**
Push warning message to SQL-layer based on foreign key constraint
index match error. */
static
void
dict_foreign_push_index_error(
/*==========================*/
	trx_t*		trx,		/*!< in: trx */
	const char*	operation,	/*!< in: operation create or alter
					*/
	const char*	create_name,	/*!< in: table name in create or
					alter table */
	const char*	latest_foreign,	/*!< in: start of latest foreign key
					constraint name */
	const char**	columns,	/*!< in: foreign key columns */
	ulint		index_error,	/*!< in: error code */
	ulint		err_col,	/*!< in: column where error happened
					*/
	dict_index_t*	err_index,	/*!< in: index where error happened
					*/
	dict_table_t*	table,		/*!< in: table */
	FILE*		ef)		/*!< in: output stream */
{
	switch (index_error) {
	case DB_FOREIGN_KEY_INDEX_NOT_FOUND: {
		fprintf(ef,
			"%s table '%s' with foreign key constraint"
			" failed. There is no index in the referenced"
			" table where the referenced columns appear"
			" as the first columns near '%s'.\n",
			operation, create_name, latest_foreign);
		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table '%s' with foreign key constraint"
			" failed. There is no index in the referenced"
			" table where the referenced columns appear"
			" as the first columns near '%s'.",
			operation, create_name, latest_foreign);
		break;
	}
	case DB_FOREIGN_KEY_IS_PREFIX_INDEX: {
		fprintf(ef,
			"%s table '%s' with foreign key constraint"
			" failed. There is only prefix index in the referenced"
			" table where the referenced columns appear"
			" as the first columns near '%s'.\n",
			operation, create_name, latest_foreign);
		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table '%s' with foreign key constraint"
			" failed. There is only prefix index in the referenced"
			" table where the referenced columns appear"
			" as the first columns near '%s'.",
			operation, create_name, latest_foreign);
		break;
	}
	case DB_FOREIGN_KEY_COL_NOT_NULL: {
		fprintf(ef,
			"%s table %s with foreign key constraint"
			" failed. You have defined a SET NULL condition but "
			"column '%s' on index is defined as NOT NULL near '%s'.\n",
			operation, create_name, columns[err_col], latest_foreign);
		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. You have defined a SET NULL condition but "
			"column '%s' on index is defined as NOT NULL near '%s'.",
			operation, create_name, columns[err_col], latest_foreign);
		break;
	}
	case DB_FOREIGN_KEY_COLS_NOT_EQUAL: {
		dict_field_t*	field;
		const char*	col_name;
		field = dict_index_get_nth_field(err_index, err_col);

		col_name = dict_table_get_col_name(
			table, dict_col_get_no(field->col));
		fprintf(ef,
			"%s table %s with foreign key constraint"
			" failed. Field type or character set for column '%s' "
			"does not mach referenced column '%s' near '%s'.\n",
			operation, create_name, columns[err_col], col_name, latest_foreign);
		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Field type or character set for column '%s' "
			"does not mach referenced column '%s' near '%s'.",
			operation, create_name, columns[err_col], col_name, latest_foreign);
		break;
	}
	default:
		ut_error;
	}
}

/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary the foreign
key constraints declared in the string. This function should be called after
the indexes for a table have been created. Each foreign key constraint must
be accompanied with indexes in both participating tables. The indexes are
allowed to contain more fields than mentioned in the constraint.
@return	error code or DB_SUCCESS */
static
ulint
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
	dict_table_t*	table			= NULL;
	dict_table_t*	referenced_table	= NULL;
	dict_table_t*	table_to_alter		= NULL;
	dict_table_t*	table_to_create		= NULL;
	ulint		highest_id_so_far	= 0;
	dict_index_t*	index			= NULL;
	dict_foreign_t*	foreign			= NULL;
	const char*	ptr			= sql_string;
	const char*	start_of_latest_foreign	= sql_string;
	const char*	start_of_latest_set     = NULL;
	FILE*		ef			= dict_foreign_err_file;
	ulint		index_error		= DB_SUCCESS;
	dict_index_t*	err_index		= NULL;
	ulint		err_col;
	const char*	constraint_name;
	ibool		success;
	ulint		error;
	const char*	ptr1;
	const char*	ptr2;
	ulint		i;
	ulint		j;
	ibool		is_on_delete;
	ulint		n_on_deletes;
	ulint		n_on_updates;
	const dict_col_t*columns[500];
	const char*	column_names[500];
	const char*	ref_column_names[500];
	const char*	referenced_table_name;
	const char*	create_table_name;
	const char*	orig;
	char		create_name[MAX_TABLE_NAME_LEN + 1];
	const char	operation[8];

	ut_ad(mutex_own(&(dict_sys->mutex)));

	table = dict_table_get_low(name, DICT_ERR_IGNORE_NONE);
	/* First check if we are actually doing an ALTER TABLE, and in that
	case look for the table being altered */
	ptr = dict_accept(cs, ptr, "ALTER", &success);

	strcpy((char *)operation, success ? "Alter " : "Create ");

	if (!success) {
		orig = ptr;
		ptr = dict_scan_to(ptr, "CREATE");
		ptr = dict_scan_to(ptr, "TABLE");
		ptr = dict_accept(cs, ptr, "TABLE", &success);

		if (success) {
			ptr = dict_scan_table_name(cs, ptr, &table_to_create, name,
					&success, heap, &create_table_name);
		}

		if (success) {
			char *bufend;
			bufend = innobase_convert_name((char *)create_name, MAX_TABLE_NAME_LEN,
					create_table_name, strlen(create_table_name),
					trx->mysql_thd, TRUE);
			create_name[bufend-create_name]='\0';
			ptr = orig;
		} else {
			char *bufend;
			ptr = orig;
			bufend = innobase_convert_name((char *)create_name, MAX_TABLE_NAME_LEN,
					name, strlen(name), trx->mysql_thd, TRUE);
			create_name[bufend-create_name]='\0';
		}

		goto loop;
	}

	if (table == NULL) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fprintf(ef, "%s table %s with foreign key constraint"
			" failed. Table %s not found from data dictionary."
			" Error close to %s.\n",
			operation, create_name, create_name, start_of_latest_foreign);
		mutex_exit(&dict_foreign_err_mutex);
		ib_push_warning(trx, DB_ERROR,
			"%s table %s with foreign key constraint"
			" failed. Table %s not found from data dictionary."
			" Error close to %s.",
			operation, create_name, create_name, start_of_latest_foreign);
		return(DB_ERROR);
	}

	/* If not alter table jump to loop */
	if (!success) {

		goto loop;
	}

	ptr = dict_accept(cs, ptr, "TABLE", &success);

	if (!success) {

		goto loop;
	}

	/* We are doing an ALTER TABLE: scan the table name we are altering */

	orig = ptr;
	ptr = dict_scan_table_name(cs, ptr, &table_to_alter, name,
				   &success, heap, &referenced_table_name);

	if (table_to_alter) {
		char *bufend;
		bufend = innobase_convert_name((char *)create_name, MAX_TABLE_NAME_LEN,
				table_to_alter->name, strlen(table_to_alter->name),
				trx->mysql_thd, TRUE);
		create_name[bufend-create_name]='\0';
	} else {
		char *bufend;
		bufend = innobase_convert_name((char *)create_name, MAX_TABLE_NAME_LEN,
				referenced_table_name, strlen(referenced_table_name),
				trx->mysql_thd, TRUE);
		create_name[bufend-create_name]='\0';

	}

	if (!success) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fprintf(ef,
			"%s table %s with foreign key constraint"
			" failed. Table %s not found from data dictionary."
			" Error close to %s.\n",
			operation, create_name, create_name, orig);
		mutex_exit(&dict_foreign_err_mutex);

		ib_push_warning(trx, DB_ERROR,
			"%s table %s with foreign key constraint"
			" failed. Table %s not found from data dictionary."
			" Error close to %s.",
			operation, create_name, create_name, orig);

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
			mutex_enter(&dict_foreign_err_mutex);
			dict_foreign_error_report_low(ef, create_name);
			fprintf(ef, "%s table %s with foreign key constraint"
				" failed. Temporary tables can't have foreign key constraints."
				" Error close to %s.\n",
				operation, create_name, start_of_latest_foreign);
			mutex_exit(&dict_foreign_err_mutex);

			ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
				"%s table %s with foreign key constraint"
				" failed. Temporary tables can't have foreign key constraints."
				" Error close to %s.",
				operation, create_name, start_of_latest_foreign);
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
		orig = ptr;
		ptr = dict_skip_word(cs, ptr, &success);

		if (!success) {
			dict_foreign_report_syntax_err(
				"%s table %s with foreign key constraint"
				" failed. Parse error in '%s'"
				" near '%s'.\n",
				operation, create_name, start_of_latest_foreign, orig);

			ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
				"%s table %s with foreign key constraint"
				" failed. Parse error in '%s'"
				" near '%s'.",
				operation, create_name, start_of_latest_foreign, orig);

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
	orig = ptr;
	ptr = dict_scan_col(cs, ptr, &success, table, columns + i,
			    heap, column_names + i);
	if (!success) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fprintf(ef,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, orig);

		mutex_exit(&dict_foreign_err_mutex);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, orig);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	i++;

	ptr = dict_accept(cs, ptr, ",", &success);

	if (success) {
		goto col_loop1;
	}

	orig = ptr;
	ptr = dict_accept(cs, ptr, ")", &success);

	if (!success) {
		dict_foreign_report_syntax_err(
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, orig);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, orig);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Try to find an index which contains the columns
	as the first fields and in the right order */

	index = dict_foreign_find_index(table, column_names, i,
		NULL, TRUE, FALSE, &index_error, &err_col, &err_index);

	if (!index) {
		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fputs("There is no index in table ", ef);
		ut_print_name(ef, NULL, TRUE, create_name);
		fprintf(ef, " where the columns appear\n"
			"as the first columns. Constraint:\n%s\n"
			"See " REFMAN "innodb-foreign-key-constraints.html\n"
			"for correct foreign key definition.\n",
			start_of_latest_foreign);

		dict_foreign_push_index_error(trx, operation, create_name, start_of_latest_foreign,
			column_names, index_error, err_col, err_index, table, ef);

		mutex_exit(&dict_foreign_err_mutex);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	orig = ptr;
	ptr = dict_accept(cs, ptr, "REFERENCES", &success);

	if (!success || !my_isspace(cs, *ptr)) {
		dict_foreign_report_syntax_err(
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, orig);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, orig);
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

		foreign->id = mem_heap_alloc(
			foreign->heap, db_len + strlen(constraint_name) + 2);

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
	foreign->foreign_col_names = mem_heap_alloc(foreign->heap,
						    i * sizeof(void*));
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
		char	buf[MAX_TABLE_NAME_LEN + 1] = "";
		char*	bufend;

		bufend = innobase_convert_name(buf, MAX_TABLE_NAME_LEN,
				referenced_table_name, strlen(referenced_table_name),
				trx->mysql_thd, TRUE);
		buf[bufend - buf] = '\0';

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint failed. Referenced table %s not found in the data dictionary "
			"near '%s'.",
			operation, create_name, buf, start_of_latest_foreign);

		dict_foreign_free(foreign);

		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fprintf(ef,
			"%s table %s with foreign key constraint failed. Referenced table %s not found in the data dictionary "
			"near '%s'.\n",
			operation, create_name, buf, start_of_latest_foreign);

		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	orig = ptr;
	ptr = dict_accept(cs, ptr, "(", &success);

	if (!success) {
		dict_foreign_free(foreign);
		dict_foreign_report_syntax_err(
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, orig);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, orig);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Scan the columns in the second list */
	i = 0;

col_loop2:
	orig = ptr;
	ptr = dict_scan_col(cs, ptr, &success, referenced_table, columns + i,
			    heap, ref_column_names + i);
	i++;

	if (!success) {
		dict_foreign_free(foreign);

		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fprintf(ef,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, orig);

		mutex_exit(&dict_foreign_err_mutex);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, orig);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	ptr = dict_accept(cs, ptr, ",", &success);

	if (success) {
		goto col_loop2;
	}

	orig = ptr;
	ptr = dict_accept(cs, ptr, ")", &success);

	if (!success || foreign->n_fields != i) {

		dict_foreign_report_syntax_err(
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s' near '%s'.  Referencing column count does not match referenced column count.\n",
			operation, create_name, start_of_latest_foreign, orig);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s' near '%s'.  Referencing column count %d does not match referenced column count %d.\n",
			operation, create_name, start_of_latest_foreign, orig, i, foreign->n_fields);

		dict_foreign_free(foreign);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	n_on_deletes = 0;
	n_on_updates = 0;

scan_on_conditions:
	/* Loop here as long as we can find ON ... conditions */

	start_of_latest_set = ptr;
	ptr = dict_accept(cs, ptr, "ON", &success);

	if (!success) {

		goto try_find_index;
	}

	ptr = dict_accept(cs, ptr, "DELETE", &success);

	if (!success) {
		orig = ptr;
		ptr = dict_accept(cs, ptr, "UPDATE", &success);

		if (!success) {
			dict_foreign_free(foreign);

			dict_foreign_report_syntax_err(
				"%s table %s with foreign key constraint"
				" failed. Parse error in '%s'"
				" near '%s'.\n",
				operation, create_name, start_of_latest_foreign, start_of_latest_set);

			ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
				"%s table %s with foreign key constraint"
				" failed. Parse error in '%s'"
				" near '%s'.",
				operation, create_name, start_of_latest_foreign, start_of_latest_set);

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
		orig = ptr;
		ptr = dict_accept(cs, ptr, "ACTION", &success);

		if (!success) {
			dict_foreign_free(foreign);
			dict_foreign_report_syntax_err(
				"%s table %s with foreign key constraint"
				" failed. Parse error in '%s'"
				" near '%s'.\n",
				operation, create_name, start_of_latest_foreign, start_of_latest_set);

			ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
				"%s table %s with foreign key constraint"
				" failed. Parse error in '%s'"
				" near '%s'.",
				operation, create_name, start_of_latest_foreign, start_of_latest_set);

			return(DB_CANNOT_ADD_CONSTRAINT);
		}

		if (is_on_delete) {
			foreign->type |= DICT_FOREIGN_ON_DELETE_NO_ACTION;
		} else {
			foreign->type |= DICT_FOREIGN_ON_UPDATE_NO_ACTION;
		}

		goto scan_on_conditions;
	}

	orig = ptr;
	ptr = dict_accept(cs, ptr, "SET", &success);

	if (!success) {
		dict_foreign_free(foreign);
		dict_foreign_report_syntax_err(
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, start_of_latest_set);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, start_of_latest_set);

		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	orig = ptr;
	ptr = dict_accept(cs, ptr, "NULL", &success);

	if (!success) {
		dict_foreign_free(foreign);
		dict_foreign_report_syntax_err(
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.\n",
			operation, create_name, start_of_latest_foreign, start_of_latest_set);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. Parse error in '%s'"
			" near '%s'.",
			operation, create_name, start_of_latest_foreign, start_of_latest_set);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	for (j = 0; j < foreign->n_fields; j++) {
		if ((dict_index_get_nth_col(foreign->foreign_index, j)->prtype)
		    & DATA_NOT_NULL) {
			const dict_col_t*	col
				= dict_index_get_nth_col(foreign->foreign_index, j);
			const char* col_name = dict_table_get_col_name(foreign->foreign_index->table,
				dict_col_get_no(col));

			/* It is not sensible to define SET NULL
			if the column is not allowed to be NULL! */

			mutex_enter(&dict_foreign_err_mutex);
			dict_foreign_error_report_low(ef, create_name);
			fprintf(ef,
				"%s table %s with foreign key constraint"
				" failed. You have defined a SET NULL condition but column '%s' is defined as NOT NULL"
				" in '%s' near '%s'.\n",
				operation, create_name, col_name, start_of_latest_foreign, start_of_latest_set);
			mutex_exit(&dict_foreign_err_mutex);


			ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
				"%s table %s with foreign key constraint"
				" failed. You have defined a SET NULL condition but column '%s' is defined as NOT NULL"
				" in '%s' near '%s'.",
				operation, create_name, col_name, start_of_latest_foreign, start_of_latest_set);

			dict_foreign_free(foreign);
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


		mutex_enter(&dict_foreign_err_mutex);
		dict_foreign_error_report_low(ef, create_name);
		fprintf(ef,
			"%s table %s with foreign key constraint"
			" failed. You have more than one on delete or on update clause"
			" in '%s' near '%s'.\n",
			operation, create_name, start_of_latest_foreign, start_of_latest_set);
		mutex_exit(&dict_foreign_err_mutex);

		ib_push_warning(trx, DB_CANNOT_ADD_CONSTRAINT,
			"%s table %s with foreign key constraint"
			" failed. You have more than one on delete or on update clause"
			" in '%s' near '%s'.",
			operation, create_name, start_of_latest_foreign, start_of_latest_set);
		dict_foreign_free(foreign);
		return(DB_CANNOT_ADD_CONSTRAINT);
	}

	/* Try to find an index which contains the columns as the first fields
	and in the right order, and the types are the same as in
	foreign->foreign_index */

	if (referenced_table) {
		index = dict_foreign_find_index(referenced_table,
					ref_column_names, i,
					foreign->foreign_index,
					TRUE, FALSE, &index_error, &err_col, &err_index);
		if (!index) {
			dict_foreign_free(foreign);
			mutex_enter(&dict_foreign_err_mutex);
			dict_foreign_error_report_low(ef, create_name);
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

			dict_foreign_push_index_error(trx, operation, create_name, start_of_latest_foreign,
				column_names, index_error, err_col, err_index, referenced_table, ef);

			mutex_exit(&dict_foreign_err_mutex);

			return(DB_CANNOT_ADD_CONSTRAINT);
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

	foreign->referenced_col_names = mem_heap_alloc(foreign->heap,
						       i * sizeof(void*));
	for (i = 0; i < foreign->n_fields; i++) {
		foreign->referenced_col_names[i]
			= mem_heap_strdup(foreign->heap, ref_column_names[i]);
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

/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary the foreign
key constraints declared in the string. This function should be called after
the indexes for a table have been created. Each foreign key constraint must
be accompanied with indexes in both participating tables. The indexes are
allowed to contain more fields than mentioned in the constraint.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
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
	char*			str;
	ulint			err;
	mem_heap_t*		heap;

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
ulint
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
	FILE*			ef	= dict_foreign_err_file;
	struct charset_info_st*	cs;

	ut_a(trx);
	ut_a(trx->mysql_thd);

	cs = innobase_get_charset(trx->mysql_thd);

	*n = 0;

	*constraints_to_drop = mem_heap_alloc(heap, 1000 * sizeof(char*));

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
		if (0 == strcmp(foreign->id, id)
		    || (strchr(foreign->id, '/')
			&& 0 == strcmp(id,
				       dict_remove_db_name(foreign->id)))) {
			/* Found */
			break;
		}

		foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
	}

	if (foreign == NULL) {
		mutex_enter(&dict_foreign_err_mutex);
		rewind(ef);
		ut_print_timestamp(ef);
		fputs(" Error in dropping of a foreign key constraint"
		      " of table ", ef);
		ut_print_name(ef, NULL, TRUE, table->name);
		fputs(",\n"
		      "in SQL command\n", ef);
		fputs(str, ef);
		fputs("\nCannot find a constraint with the given id ", ef);
		ut_print_name(ef, NULL, FALSE, id);
		fputs(".\n", ef);
		mutex_exit(&dict_foreign_err_mutex);

		mem_free(str);

		return(DB_CANNOT_DROP_CONSTRAINT);
	}

	goto loop;

syntax_error:
	mutex_enter(&dict_foreign_err_mutex);
	rewind(ef);
	ut_print_timestamp(ef);
	fputs(" Syntax error in dropping of a"
	      " foreign key constraint of table ", ef);
	ut_print_name(ef, NULL, TRUE, table->name);
	fprintf(ef, ",\n"
		"close to:\n%s\n in SQL command\n%s\n", ptr, str);
	mutex_exit(&dict_foreign_err_mutex);

	mem_free(str);

	return(DB_CANNOT_DROP_CONSTRAINT);
}

/*==================== END OF FOREIGN KEY PROCESSING ====================*/
#endif /* !UNIV_HOTBACKUP */
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

	if (UNIV_UNLIKELY(index->type & DICT_UNIVERSAL)) {
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

	buf = mem_heap_alloc(heap, 4);

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

	if (UNIV_UNLIKELY(index->type & DICT_UNIVERSAL)) {
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

/*********************************************************************//**
Calculates new estimates for table and index statistics. The statistics
are used in query optimization. */
UNIV_INTERN
void
dict_update_statistics(
/*===================*/
	dict_table_t*	table,		/*!< in/out: table */
	ibool		only_calc_if_missing_stats,/*!< in: only
					update/recalc the stats if they have
					not been initialized yet, otherwise
					do nothing */
	ibool		only_calc_if_changed_too_much)/*!< in: only
					update/recalc the stats if the table
					has been changed too much since the
					last stats update/recalc */
{
	dict_index_t*	index;
	ulint		sum_of_index_sizes	= 0;

	DBUG_EXECUTE_IF("skip_innodb_statistics", return;);

	if (table->ibd_file_missing) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: cannot calculate statistics for table %s\n"
			"InnoDB: because the .ibd file is missing.  For help,"
			" please refer to\n"
			"InnoDB: " REFMAN "innodb-troubleshooting.html\n",
			table->name);

		return;
	}

	/* Find out the sizes of the indexes and how many different values
	for the key they approximately have */

	index = dict_table_get_first_index(table);

	if (index == NULL) {
		/* Table definition is corrupt */

		return;
	}

	dict_table_stats_lock(table, RW_X_LATCH);

	if ((only_calc_if_missing_stats && table->stat_initialized)
	    || (only_calc_if_changed_too_much
		&& !DICT_TABLE_CHANGED_TOO_MUCH(table))) {

		dict_table_stats_unlock(table, RW_X_LATCH);
		return;
	}

	for (; index != NULL; index = dict_table_get_next_index(index)) {

		/* Skip incomplete indexes. */
		if (index->name[0] == TEMP_INDEX_PREFIX) {
			continue;
		}

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
		if (ibuf_debug && !dict_index_is_clust(index)) {
			goto fake_statistics;
		}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

		if (UNIV_LIKELY
		    (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE
		     || (srv_force_recovery < SRV_FORCE_NO_LOG_REDO
			 && dict_index_is_clust(index)))) {
			mtr_t	mtr;
			ulint	size;

			mtr_start(&mtr);
			mtr_s_lock(dict_index_get_lock(index), &mtr);

			size = btr_get_size(index, BTR_TOTAL_SIZE, &mtr);

			if (size != ULINT_UNDEFINED) {
				sum_of_index_sizes += size;
				index->stat_index_size = size;
				size = btr_get_size(
					index, BTR_N_LEAF_PAGES, &mtr);
			}

			mtr_commit(&mtr);

			switch (size) {
			case ULINT_UNDEFINED:
				goto fake_statistics;
			case 0:
				/* The root node of the tree is a leaf */
				size = 1;
			}

			index->stat_n_leaf_pages = size;

			btr_estimate_number_of_different_key_vals(index);
		} else {
			/* If we have set a high innodb_force_recovery
			level, do not calculate statistics, as a badly
			corrupted index can cause a crash in it.
			Initialize some bogus index cardinality
			statistics, so that the data can be queried in
			various means, also via secondary indexes. */
			ulint	i;

fake_statistics:
			sum_of_index_sizes++;
			index->stat_index_size = index->stat_n_leaf_pages = 1;

			for (i = dict_index_get_n_unique(index); i; ) {
				index->stat_n_diff_key_vals[i--] = 1;
			}

			memset(index->stat_n_non_null_key_vals, 0,
			       (1 + dict_index_get_n_unique(index))
                               * sizeof(*index->stat_n_non_null_key_vals));
		}
	}

	index = dict_table_get_first_index(table);

	table->stat_n_rows = index->stat_n_diff_key_vals[
		dict_index_get_n_unique(index)];

	table->stat_clustered_index_size = index->stat_index_size;

	table->stat_sum_of_other_index_sizes = sum_of_index_sizes
		- index->stat_index_size;

	table->stat_initialized = TRUE;

	table->stat_modified_counter = 0;

	dict_table_stats_unlock(table, RW_X_LATCH);
}

#ifndef UNIV_HOTBACKUP
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

#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Prints a table data. */
UNIV_INTERN
void
dict_table_print(
/*=============*/
	dict_table_t*	table)	/*!< in: table */
{
	mutex_enter(&(dict_sys->mutex));
	dict_table_print_low(table);
	mutex_exit(&(dict_sys->mutex));
}

/**********************************************************************//**
Prints a table data when we know the table name. */
UNIV_INTERN
void
dict_table_print_by_name(
/*=====================*/
	const char*	name)	/*!< in: table name */
{
	dict_table_t*	table;

	mutex_enter(&(dict_sys->mutex));

	table = dict_table_get_low(name, DICT_ERR_IGNORE_NONE);

	ut_a(table);

	dict_table_print_low(table);
	mutex_exit(&(dict_sys->mutex));
}

/**********************************************************************//**
Prints a table data. */
UNIV_INTERN
void
dict_table_print_low(
/*=================*/
	dict_table_t*	table)	/*!< in: table */
{
	dict_index_t*	index;
	dict_foreign_t*	foreign;
	ulint		i;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	dict_update_statistics(
		table,
		FALSE, /* update even if initialized */
		FALSE /* update even if not changed too much */);

	dict_table_stats_lock(table, RW_S_LATCH);

	fprintf(stderr,
		"--------------------------------------\n"
		"TABLE: name %s, id %llu, flags %lx, columns %lu,"
		" indexes %lu, appr.rows %lu\n"
		"  COLUMNS: ",
		table->name,
		(ullint) table->id,
		(ulong) table->flags,
		(ulong) table->n_cols,
		(ulong) UT_LIST_GET_LEN(table->indexes),
		(ulong) table->stat_n_rows);

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

	dict_table_stats_unlock(table, RW_S_LATCH);

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

	ut_ad(mutex_own(&(dict_sys->mutex)));

	if (index->n_user_defined_cols > 0) {
		n_vals = index->stat_n_diff_key_vals[
			index->n_user_defined_cols];
	} else {
		n_vals = index->stat_n_diff_key_vals[1];
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

#ifndef UNIV_HOTBACKUP
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

#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Displays the names of the index and the table. */
UNIV_INTERN
void
dict_index_name_print(
/*==================*/
	FILE*			file,	/*!< in: output stream */
	trx_t*			trx,	/*!< in: transaction */
	const dict_index_t*	index)	/*!< in: index to print */
{
	fputs("index ", file);
	ut_print_name(file, trx, FALSE, index->name);
	fputs(" of table ", file);
	ut_print_name(file, trx, TRUE, index->table_name);
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
	table = dict_mem_table_create("SYS_DUMMY1", DICT_HDR_SPACE, 1, 0);
	dict_mem_table_add_col(table, NULL, NULL, DATA_CHAR,
			       DATA_ENGLISH | DATA_NOT_NULL, 8);

	dict_ind_redundant = dict_mem_index_create("SYS_DUMMY1", "SYS_DUMMY1",
						   DICT_HDR_SPACE, 0, 1);
	dict_index_add_col(dict_ind_redundant, table,
			   dict_table_get_nth_col(table, 0), 0);
	dict_ind_redundant->table = table;
	/* create dummy table and index for COMPACT infimum and supremum */
	table = dict_mem_table_create("SYS_DUMMY2",
				      DICT_HDR_SPACE, 1, DICT_TF_COMPACT);
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

#ifndef UNIV_HOTBACKUP
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

	index = dict_table_get_first_index(table);

	while (index != NULL) {
		if (ut_strcmp(index->name, name) == 0) {

			return(index);
		}

		index = dict_table_get_next_index(index);
	}

	return(NULL);

}

/**********************************************************************//**
Replace the index passed in with another equivalent index in the tables
foreign key list. */
UNIV_INTERN
void
dict_table_replace_index_in_foreign_list(
/*=====================================*/
	dict_table_t*	table,  /*!< in/out: table */
	dict_index_t*	index,	/*!< in: index to be replaced */
	const trx_t*	trx)	/*!< in: transaction handle */
{
	dict_foreign_t*	foreign;

	for (foreign = UT_LIST_GET_FIRST(table->foreign_list);
	     foreign;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {

		if (foreign->foreign_index == index) {
			dict_index_t*	new_index
				= dict_foreign_find_equiv_index(foreign);

			/* There must exist an alternative index if
			check_foreigns (FOREIGN_KEY_CHECKS) is on, 
			since ha_innobase::prepare_drop_index had done
			the check before we reach here. */

			ut_a(new_index || !trx->check_foreigns);

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
				/*check_charsets=*/TRUE, /*check_null=*/FALSE,
				NULL, NULL, NULL);
			ut_ad(new_index || !trx->check_foreigns);
			ut_ad(!new_index || new_index->table == index->table);

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
	ibool			tmp_ok)	/*!< in: TRUE=allow temporary
					index names */
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
		ut_ad(tmp_ok || *index1->name != TEMP_INDEX_PREFIX);

		index2 = UT_LIST_GET_NEXT(indexes, index1);

		while (index2) {

			if (!index2->to_be_dropped) {
				ut_ad(ut_strcmp(index1->name, index2->name));
			}

			index2 = UT_LIST_GET_NEXT(indexes, index2);
		}

		index1 = UT_LIST_GET_NEXT(indexes, index1);
	} while (index1);
}
#endif /* UNIV_DEBUG */

/**************************************************************************
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

		table = HASH_GET_FIRST(dict_sys->table_hash, i);

		while (table) {
			dict_table_t*	prev_table = table;

			table = HASH_GET_NEXT(name_hash, prev_table);
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

	mutex_free(&dict_foreign_err_mutex);

	mem_free(dict_sys);
	dict_sys = NULL;

	for (i = 0; i < DICT_TABLE_STATS_LATCHES_SIZE; i++) {
		rw_lock_free(&dict_table_stats_latches[i]);
	}
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

	if (dict_sys == NULL) {
		/* This could happen when it's in redo processing. */
		return(NULL);
	}

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

	if (UNIV_UNLIKELY(dict_index_is_corrupted(index))) {
		/* The index was already flagged corrupted. */
		ut_ad(index->table->corrupted);
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
	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, index->table->id);
	dfield_set_data(dfield, buf, 8);

	dfield = dtuple_get_nth_field(tuple, 1);
	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, index->id);
	dfield_set_data(dfield, buf, 8);

	dict_index_copy_types(tuple, sys_index, 2);

	btr_cur_search_to_nth_level(sys_index, 0, tuple, PAGE_CUR_LE,
				    BTR_MODIFY_LEAF,
				    &cursor, 0, __FILE__, __LINE__, &mtr);

	if (cursor.low_match == dtuple_get_n_fields(tuple)) {
		/* UPDATE SYS_INDEXES SET TYPE=index->type
		WHERE TABLE_ID=index->table->id AND INDEX_ID=index->id */
		ulint	len;
		byte*	field	= rec_get_nth_field_old(
			btr_cur_get_rec(&cursor),
			DICT_SYS_INDEXES_TYPE_FIELD, &len);
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
