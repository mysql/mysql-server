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

/**************************************************//**
@file include/dict0dict.h
Data dictionary system

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0dict_h
#define dict0dict_h

#include "univ.i"
#include "db0err.h"
#include "dict0types.h"
#include "dict0mem.h"
#include "data0type.h"
#include "data0data.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "ut0mem.h"
#include "ut0lst.h"
#include "hash0hash.h"
#include "ut0rnd.h"
#include "ut0byte.h"
#include "trx0types.h"
#include "row0types.h"

#ifndef UNIV_HOTBACKUP
# include "sync0sync.h"
# include "sync0rw.h"
/******************************************************************//**
Makes all characters in a NUL-terminated UTF-8 string lower case. */
UNIV_INTERN
void
dict_casedn_str(
/*============*/
	char*	a)	/*!< in/out: string to put in lower case */
	__attribute__((nonnull));
/********************************************************************//**
Get the database name length in a table name.
@return	database name length */
UNIV_INTERN
ulint
dict_get_db_name_len(
/*=================*/
	const char*	name)	/*!< in: table name in the form
				dbname '/' tablename */
	__attribute__((nonnull, warn_unused_result));
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
	ulint		database_name_len,/*!< in: db name length */
	const char*	table_name,	/*!< in: table name */
	ulint		table_name_len,	/*!< in: table name length */
	dict_table_t**	table,		/*!< out: table object or NULL */
	mem_heap_t*	heap);		/*!< in: heap memory */
/*********************************************************************//**
Frees a foreign key struct. */
UNIV_INTERN
void
dict_foreign_free(
/*==============*/
	dict_foreign_t*	foreign);	/*!< in, own: foreign key struct */
/*********************************************************************//**
Finds the highest [number] for foreign key constraints of the table. Looks
only at the >= 4.0.18-format id's, which are of the form
databasename/tablename_ibfk_[number].
@return highest number, 0 if table has no new format foreign key constraints */
UNIV_INTERN
ulint
dict_table_get_highest_foreign_id(
/*==============================*/
	dict_table_t*	table);		/*!< in: table in the dictionary
					memory cache */
/********************************************************************//**
Return the end of table name where we have removed dbname and '/'.
@return	table name */
UNIV_INTERN
const char*
dict_remove_db_name(
/*================*/
	const char*	name)	/*!< in: table name in the form
				dbname '/' tablename */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((warn_unused_result));
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
	__attribute__((nonnull));
/**********************************************************************//**
Inits the data dictionary module. */
UNIV_INTERN
void
dict_init(void);
/*===========*/
/********************************************************************//**
Gets the space id of every table of the data dictionary and makes a linear
list and a hash table of them to the data dictionary cache. This function
can be called at database startup if we did not need to do a crash recovery.
In crash recovery we must scan the space id's from the .ibd files in MySQL
database directories. */
UNIV_INTERN
void
dict_load_space_id_list(void);
/*=========================*/
/*********************************************************************//**
Gets the minimum number of bytes per character.
@return minimum multi-byte char size, in bytes */
UNIV_INLINE
ulint
dict_col_get_mbminlen(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Gets the maximum number of bytes per character.
@return maximum multi-byte char size, in bytes */
UNIV_INLINE
ulint
dict_col_get_mbmaxlen(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Sets the minimum and maximum number of bytes per character. */
UNIV_INLINE
void
dict_col_set_mbminmaxlen(
/*=====================*/
	dict_col_t*	col,		/*!< in/out: column */
	ulint		mbminlen,	/*!< in: minimum multi-byte
					character size, in bytes */
	ulint		mbmaxlen)	/*!< in: minimum multi-byte
					character size, in bytes */
	__attribute__((nonnull));
/*********************************************************************//**
Gets the column data type. */
UNIV_INLINE
void
dict_col_copy_type(
/*===============*/
	const dict_col_t*	col,	/*!< in: column */
	dtype_t*		type)	/*!< out: data type */
	__attribute__((nonnull));
/**********************************************************************//**
Determine bytes of column prefix to be stored in the undo log. Please
note if the table format is UNIV_FORMAT_A (< UNIV_FORMAT_B), no prefix
needs to be stored in the undo log.
@return bytes of column prefix to be stored in the undo log */
UNIV_INLINE
ulint
dict_max_field_len_store_undo(
/*==========================*/
	dict_table_t*		table,	/*!< in: table */
	const dict_col_t*	col)	/*!< in: column which index prefix
					is based on */
	__attribute__((nonnull, warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
#ifdef UNIV_DEBUG
/*********************************************************************//**
Assert that a column and a data type match.
@return	TRUE */
UNIV_INLINE
ibool
dict_col_type_assert_equal(
/*=======================*/
	const dict_col_t*	col,	/*!< in: column */
	const dtype_t*		type)	/*!< in: data type */
	__attribute__((nonnull, warn_unused_result));
#endif /* UNIV_DEBUG */
#ifndef UNIV_HOTBACKUP
/***********************************************************************//**
Returns the minimum size of the column.
@return	minimum size */
UNIV_INLINE
ulint
dict_col_get_min_size(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	__attribute__((nonnull, warn_unused_result));
/***********************************************************************//**
Returns the maximum size of the column.
@return	maximum size */
UNIV_INLINE
ulint
dict_col_get_max_size(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	__attribute__((nonnull, warn_unused_result));
/***********************************************************************//**
Returns the size of a fixed size column, 0 if not a fixed size column.
@return	fixed size, or 0 */
UNIV_INLINE
ulint
dict_col_get_fixed_size(
/*====================*/
	const dict_col_t*	col,	/*!< in: column */
	ulint			comp)	/*!< in: nonzero=ROW_FORMAT=COMPACT  */
	__attribute__((nonnull, warn_unused_result));
/***********************************************************************//**
Returns the ROW_FORMAT=REDUNDANT stored SQL NULL size of a column.
For fixed length types it is the fixed length of the type, otherwise 0.
@return	SQL null storage size in ROW_FORMAT=REDUNDANT */
UNIV_INLINE
ulint
dict_col_get_sql_null_size(
/*=======================*/
	const dict_col_t*	col,	/*!< in: column */
	ulint			comp)	/*!< in: nonzero=ROW_FORMAT=COMPACT  */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Gets the column number.
@return	col->ind, table column position (starting from 0) */
UNIV_INLINE
ulint
dict_col_get_no(
/*============*/
	const dict_col_t*	col)	/*!< in: column */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Gets the column position in the clustered index. */
UNIV_INLINE
ulint
dict_col_get_clust_pos(
/*===================*/
	const dict_col_t*	col,		/*!< in: table column */
	const dict_index_t*	clust_index)	/*!< in: clustered index */
	__attribute__((nonnull, warn_unused_result));
/****************************************************************//**
If the given column name is reserved for InnoDB system columns, return
TRUE.
@return	TRUE if name is reserved */
UNIV_INTERN
ibool
dict_col_name_is_reserved(
/*======================*/
	const char*	name)	/*!< in: column name */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Acquire the autoinc lock. */
UNIV_INTERN
void
dict_table_autoinc_lock(
/*====================*/
	dict_table_t*	table)	/*!< in/out: table */
	__attribute__((nonnull));
/********************************************************************//**
Unconditionally set the autoinc counter. */
UNIV_INTERN
void
dict_table_autoinc_initialize(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	ib_uint64_t	value)	/*!< in: next value to assign to a row */
	__attribute__((nonnull));
/********************************************************************//**
Reads the next autoinc value (== autoinc counter value), 0 if not yet
initialized.
@return	value for a new row, or 0 */
UNIV_INTERN
ib_uint64_t
dict_table_autoinc_read(
/*====================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Updates the autoinc counter if the value supplied is greater than the
current value. */
UNIV_INTERN
void
dict_table_autoinc_update_if_greater(
/*=================================*/

	dict_table_t*	table,	/*!< in/out: table */
	ib_uint64_t	value)	/*!< in: value which was assigned to a row */
	__attribute__((nonnull));
/********************************************************************//**
Release the autoinc lock. */
UNIV_INTERN
void
dict_table_autoinc_unlock(
/*======================*/
	dict_table_t*	table)	/*!< in/out: table */
	__attribute__((nonnull));
#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Adds system columns to a table object. */
UNIV_INTERN
void
dict_table_add_system_columns(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap)	/*!< in: temporary heap */
	__attribute__((nonnull));
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Adds a table object to the dictionary cache. */
UNIV_INTERN
void
dict_table_add_to_cache(
/*====================*/
	dict_table_t*	table,		/*!< in: table */
	ibool		can_be_evicted,	/*!< in: TRUE if can be evicted*/
	mem_heap_t*	heap)		/*!< in: temporary heap */
	__attribute__((nonnull));
/**********************************************************************//**
Removes a table object from the dictionary cache. */
UNIV_INTERN
void
dict_table_remove_from_cache(
/*=========================*/
	dict_table_t*	table)	/*!< in, own: table */
	__attribute__((nonnull));
/**********************************************************************//**
Renames a table object.
@return	TRUE if success */
UNIV_INTERN
dberr_t
dict_table_rename_in_cache(
/*=======================*/
	dict_table_t*	table,		/*!< in/out: table */
	const char*	new_name,	/*!< in: new name */
	ibool		rename_also_foreigns)
					/*!< in: in ALTER TABLE we want
					to preserve the original table name
					in constraints which reference it */
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Removes an index from the dictionary cache. */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/*!< in/out: table */
	dict_index_t*	index)	/*!< in, own: index */
	__attribute__((nonnull));
/**********************************************************************//**
Change the id of a table object in the dictionary cache. This is used in
DISCARD TABLESPACE. */
UNIV_INTERN
void
dict_table_change_id_in_cache(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table object already in cache */
	table_id_t	new_id)	/*!< in: new id to set */
	__attribute__((nonnull));
/**********************************************************************//**
Removes a foreign constraint struct from the dictionary cache. */
UNIV_INTERN
void
dict_foreign_remove_from_cache(
/*===========================*/
	dict_foreign_t*	foreign)	/*!< in, own: foreign constraint */
	__attribute__((nonnull));
/**********************************************************************//**
Adds a foreign key constraint object to the dictionary cache. May free
the object if there already is an object with the same identifier in.
At least one of foreign table or referenced table must already be in
the dictionary cache!
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
dict_foreign_add_to_cache(
/*======================*/
	dict_foreign_t*	foreign,	/*!< in, own: foreign key constraint */
	ibool		check_charsets)	/*!< in: TRUE=check charset
					compatibility */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Check if the index is referenced by a foreign key, if TRUE return the
matching instance NULL otherwise.
@return pointer to foreign key struct if index is defined for foreign
key, otherwise NULL */
UNIV_INTERN
dict_foreign_t*
dict_table_get_referenced_constraint(
/*=================================*/
	dict_table_t*	table,	/*!< in: InnoDB table */
	dict_index_t*	index)	/*!< in: InnoDB index */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Checks if a table is referenced by foreign keys.
@return	TRUE if table is referenced by a foreign key */
UNIV_INTERN
ibool
dict_table_is_referenced_by_foreign_key(
/*====================================*/
	const dict_table_t*	table)	/*!< in: InnoDB table */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull));
/**********************************************************************//**
Determines whether a string starts with the specified keyword.
@return TRUE if str starts with keyword */
UNIV_INTERN
ibool
dict_str_starts_with_keyword(
/*=========================*/
	THD*		thd,		/*!< in: MySQL thread handle */
	const char*	str,		/*!< in: string to scan for keyword */
	const char*	keyword)	/*!< in: keyword to look for */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Checks if a index is defined for a foreign key constraint. Index is a part
of a foreign key constraint if the index is referenced by foreign key
or index is a foreign key index
@return pointer to foreign key struct if index is defined for foreign
key, otherwise NULL */
UNIV_INTERN
dict_foreign_t*
dict_table_get_foreign_constraint(
/*==============================*/
	dict_table_t*	table,	/*!< in: InnoDB table */
	dict_index_t*	index)	/*!< in: InnoDB index */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
bot participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint.
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
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Returns a table object and increments its open handle count.
NOTE! This is a high-level function to be used mainly from outside the
'dict' directory. Inside this directory dict_table_get_low
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
					loading the table */
	__attribute__((nonnull, warn_unused_result));

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
	__attribute__((nonnull(1,2), warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Prints a table data. */
UNIV_INTERN
void
dict_table_print(
/*=============*/
	dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull));
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
	__attribute__((nonnull));
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
	__attribute__((nonnull(1,3)));
/********************************************************************//**
Displays the names of the index and the table. */
UNIV_INTERN
void
dict_index_name_print(
/*==================*/
	FILE*			file,	/*!< in: output stream */
	const trx_t*		trx,	/*!< in: transaction */
	const dict_index_t*	index)	/*!< in: index to print */
	__attribute__((nonnull(1,3)));
/*********************************************************************//**
Tries to find an index whose first fields are the columns in the array,
in the same order and is not marked for deletion and is not the same
as types_idx.
@return	matching index, NULL if not found */
UNIV_INTERN
bool
dict_foreign_qualify_index(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	const char**		columns,/*!< in: array of column names */
	ulint			n_cols,	/*!< in: number of columns */
	const dict_index_t*	index,	/*!< in: index to check */
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
	__attribute__((nonnull(1,2), warn_unused_result));
#ifdef UNIV_DEBUG
/********************************************************************//**
Gets the first index on the table (the clustered index).
@return	index, NULL if none exists */
UNIV_INLINE
dict_index_t*
dict_table_get_first_index(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the last index on the table.
@return	index, NULL if none exists */
UNIV_INLINE
dict_index_t*
dict_table_get_last_index(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the next index on the table.
@return	index, NULL if none left */
UNIV_INLINE
dict_index_t*
dict_table_get_next_index(
/*======================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
#else /* UNIV_DEBUG */
# define dict_table_get_first_index(table) UT_LIST_GET_FIRST((table)->indexes)
# define dict_table_get_last_index(table) UT_LIST_GET_LAST((table)->indexes)
# define dict_table_get_next_index(index) UT_LIST_GET_NEXT(indexes, index)
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

/* Skip corrupted index */
#define dict_table_skip_corrupt_index(index)			\
	while (index && dict_index_is_corrupted(index)) {	\
		index = dict_table_get_next_index(index);	\
	}

/* Get the next non-corrupt index */
#define dict_table_next_uncorrupted_index(index)		\
do {								\
	index = dict_table_get_next_index(index);		\
	dict_table_skip_corrupt_index(index);			\
} while (0)

/********************************************************************//**
Check whether the index is the clustered index.
@return	nonzero for clustered index, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_clust(
/*================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, pure, warn_unused_result));
/********************************************************************//**
Check whether the index is unique.
@return	nonzero for unique index, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_unique(
/*=================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, pure, warn_unused_result));
/********************************************************************//**
Check whether the index is the insert buffer tree.
@return	nonzero for insert buffer, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_ibuf(
/*===============*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, pure, warn_unused_result));
/********************************************************************//**
Check whether the index is a secondary index or the insert buffer tree.
@return	nonzero for insert buffer, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_sec_or_ibuf(
/*======================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, pure, warn_unused_result));

/************************************************************************
Gets the all the FTS indexes for the table. NOTE: must not be called for
tables which do not have an FTS-index. */
UNIV_INTERN
ulint
dict_table_get_all_fts_indexes(
/*===========================*/
				/* out: number of indexes collected */
	dict_table_t*	table,	/* in: table */
	ib_vector_t*	indexes)/* out: vector for collecting FTS indexes */
	__attribute__((nonnull));
/********************************************************************//**
Gets the number of user-defined columns in a table in the dictionary
cache.
@return	number of user-defined (e.g., not ROW_ID) columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_user_cols(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, pure, warn_unused_result));
/********************************************************************//**
Gets the number of system columns in a table in the dictionary cache.
@return	number of system (e.g., ROW_ID) columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_sys_cols(
/*======================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, pure, warn_unused_result));
/********************************************************************//**
Gets the number of all columns (also system) in a table in the dictionary
cache.
@return	number of columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_cols(
/*==================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, pure, warn_unused_result));
/********************************************************************//**
Gets the approximately estimated number of rows in the table.
@return	estimated number of rows */
UNIV_INLINE
ib_uint64_t
dict_table_get_n_rows(
/*==================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Increment the number of rows in the table by one.
Notice that this operation is not protected by any latch, the number is
approximate. */
UNIV_INLINE
void
dict_table_n_rows_inc(
/*==================*/
	dict_table_t*	table)	/*!< in/out: table */
	__attribute__((nonnull));
/********************************************************************//**
Decrement the number of rows in the table by one.
Notice that this operation is not protected by any latch, the number is
approximate. */
UNIV_INLINE
void
dict_table_n_rows_dec(
/*==================*/
	dict_table_t*	table)	/*!< in/out: table */
	__attribute__((nonnull));
#ifdef UNIV_DEBUG
/********************************************************************//**
Gets the nth column of a table.
@return	pointer to column object */
UNIV_INLINE
dict_col_t*
dict_table_get_nth_col(
/*===================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			pos)	/*!< in: position of column */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the given system column of a table.
@return	pointer to column object */
UNIV_INLINE
dict_col_t*
dict_table_get_sys_col(
/*===================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			sys)	/*!< in: DATA_ROW_ID, ... */
	__attribute__((nonnull, warn_unused_result));
#else /* UNIV_DEBUG */
#define dict_table_get_nth_col(table, pos) \
((table)->cols + (pos))
#define dict_table_get_sys_col(table, sys) \
((table)->cols + (table)->n_cols + (sys) - DATA_N_SYS_COLS)
#endif /* UNIV_DEBUG */
/********************************************************************//**
Gets the given system column number of a table.
@return	column number */
UNIV_INLINE
ulint
dict_table_get_sys_col_no(
/*======================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			sys)	/*!< in: DATA_ROW_ID, ... */
	__attribute__((nonnull, warn_unused_result));
#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Returns the minimum data size of an index record.
@return	minimum data size in bytes */
UNIV_INLINE
ulint
dict_index_get_min_size(
/*====================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Check whether the table uses the compact page format.
@return	TRUE if table uses the compact page format */
UNIV_INLINE
ibool
dict_table_is_comp(
/*===============*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Determine the file format of a table.
@return	file format version */
UNIV_INLINE
ulint
dict_table_get_format(
/*==================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Determine the file format from a dict_table_t::flags.
@return	file format version */
UNIV_INLINE
ulint
dict_tf_get_format(
/*===============*/
	ulint		flags)		/*!< in: dict_table_t::flags */
	__attribute__((warn_unused_result));
/********************************************************************//**
Set the various values in a dict_table_t::flags pointer. */
UNIV_INLINE
void
dict_tf_set(
/*========*/
	ulint*		flags,		/*!< in/out: table */
	rec_format_t	format,		/*!< in: file format */
	ulint		zip_ssize,	/*!< in: zip shift size */
	bool		remote_path)	/*!< in: table uses DATA DIRECTORY */
	__attribute__((nonnull));
/********************************************************************//**
Convert a 32 bit integer table flags to the 32 bit integer that is
written into the tablespace header at the offset FSP_SPACE_FLAGS and is
also stored in the fil_space_t::flags field.  The following chart shows
the translation of the low order bit.  Other bits are the same.
========================= Low order bit ==========================
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
dict_table_t::flags |     0     |    1    |     1      |    1
fil_space_t::flags  |     0     |    0    |     1      |    1
==================================================================
@return	tablespace flags (fil_space_t::flags) */
UNIV_INLINE
ulint
dict_tf_to_fsp_flags(
/*=================*/
	ulint	flags)	/*!< in: dict_table_t::flags */
	__attribute__((const));
/********************************************************************//**
Extract the compressed page size from table flags.
@return	compressed page size, or 0 if not compressed */
UNIV_INLINE
ulint
dict_tf_get_zip_size(
/*=================*/
	ulint	flags)			/*!< in: flags */
	__attribute__((const));
/********************************************************************//**
Check whether the table uses the compressed compact page format.
@return	compressed page size, or 0 if not compressed */
UNIV_INLINE
ulint
dict_table_zip_size(
/*================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Obtain exclusive locks on all index trees of the table. This is to prevent
accessing index trees while InnoDB is updating internal metadata for
operations such as truncate tables. */
UNIV_INLINE
void
dict_table_x_lock_indexes(
/*======================*/
	dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull));
/*********************************************************************//**
Release the exclusive locks on all index tree. */
UNIV_INLINE
void
dict_table_x_unlock_indexes(
/*========================*/
	dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull));
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
	__attribute__((nonnull, warn_unused_result));
/*******************************************************************//**
Check if the table has an FTS index.
@return TRUE if table has an FTS index */
UNIV_INLINE
ibool
dict_table_has_fts_index(
/*=====================*/
	dict_table_t*   table)		/*!< in: table */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull));
/********************************************************************
Wait until all the background threads of the given table have exited, i.e.,
bg_threads == 0. Note: bg_threads_mutex must be reserved when
calling this. */
UNIV_INTERN
void
dict_table_wait_for_bg_threads_to_exit(
/*===================================*/
	dict_table_t*	table,	/* in: table */
	ulint		delay)	/* in: time in microseconds to wait between
				checks of bg_threads. */
	__attribute__((nonnull));
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
	__attribute__((warn_unused_result));
/**********************************************************************//**
Make room in the table cache by evicting an unused table. The unused table
should not be part of FK relationship and currently not used in any user
transaction. There is no guarantee that it will remove a table.
@return number of tables evicted. */
UNIV_INTERN
ulint
dict_make_room_in_cache(
/*====================*/
	ulint		max_tables,	/*!< in: max tables allowed in cache */
	ulint		pct_check);	/*!< in: max percent to check */
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
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Removes an index from the dictionary cache. */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/*!< in/out: table */
	dict_index_t*	index)	/*!< in, own: index */
	__attribute__((nonnull));
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Gets the number of fields in the internal representation of an index,
including fields added by the dictionary system.
@return	number of fields */
UNIV_INLINE
ulint
dict_index_get_n_fields(
/*====================*/
	const dict_index_t*	index)	/*!< in: an internal
					representation of index (in
					the dictionary cache) */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the number of fields in the internal representation of an index
that uniquely determine the position of an index entry in the index, if
we do not take multiversioning into account: in the B-tree use the value
returned by dict_index_get_n_unique_in_tree.
@return	number of fields */
UNIV_INLINE
ulint
dict_index_get_n_unique(
/*====================*/
	const dict_index_t*	index)	/*!< in: an internal representation
					of index (in the dictionary cache) */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the number of fields in the internal representation of an index
which uniquely determine the position of an index entry in the index, if
we also take multiversioning into account.
@return	number of fields */
UNIV_INLINE
ulint
dict_index_get_n_unique_in_tree(
/*============================*/
	const dict_index_t*	index)	/*!< in: an internal representation
					of index (in the dictionary cache) */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the number of user-defined ordering fields in the index. In the internal
representation we add the row id to the ordering fields to make all indexes
unique, but this function returns the number of fields the user defined
in the index as ordering fields.
@return	number of fields */
UNIV_INLINE
ulint
dict_index_get_n_ordering_defined_by_user(
/*======================================*/
	const dict_index_t*	index)	/*!< in: an internal representation
					of index (in the dictionary cache) */
	__attribute__((nonnull, warn_unused_result));
#ifdef UNIV_DEBUG
/********************************************************************//**
Gets the nth field of an index.
@return	pointer to field object */
UNIV_INLINE
dict_field_t*
dict_index_get_nth_field(
/*=====================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			pos)	/*!< in: position of field */
	__attribute__((nonnull, warn_unused_result));
#else /* UNIV_DEBUG */
# define dict_index_get_nth_field(index, pos) ((index)->fields + (pos))
#endif /* UNIV_DEBUG */
/********************************************************************//**
Gets pointer to the nth column in an index.
@return	column */
UNIV_INLINE
const dict_col_t*
dict_index_get_nth_col(
/*===================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			pos)	/*!< in: position of the field */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Gets the column number of the nth field in an index.
@return	column number */
UNIV_INLINE
ulint
dict_index_get_nth_col_no(
/*======================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			pos)	/*!< in: position of the field */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Looks for column n in an index.
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
UNIV_INLINE
ulint
dict_index_get_nth_col_pos(
/*=======================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			n)	/*!< in: column number */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Returns TRUE if the index contains a column or a prefix of that column.
@return	TRUE if contains the column or its prefix */
UNIV_INTERN
ibool
dict_index_contains_col_or_prefix(
/*==============================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			n)	/*!< in: column number */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Looks for column n position in the clustered index.
@return	position in internal representation of the clustered index */
UNIV_INTERN
ulint
dict_table_get_nth_col_pos(
/*=======================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			n)	/*!< in: column number */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Returns the position of a system column in an index.
@return	position, ULINT_UNDEFINED if not contained */
UNIV_INLINE
ulint
dict_index_get_sys_col_pos(
/*=======================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			type)	/*!< in: DATA_ROW_ID, ... */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull));
#ifndef UNIV_HOTBACKUP
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
	__attribute__((nonnull));
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Gets the field column.
@return	field->col, pointer to the table column */
UNIV_INLINE
const dict_col_t*
dict_field_get_col(
/*===============*/
	const dict_field_t*	field)	/*!< in: index field */
	__attribute__((nonnull, warn_unused_result));
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Returns an index object if it is found in the dictionary cache.
Assumes that dict_sys->mutex is already being held.
@return	index, NULL if not found */
UNIV_INTERN
dict_index_t*
dict_index_get_if_in_cache_low(
/*===========================*/
	index_id_t	index_id)	/*!< in: index id */
	__attribute__((warn_unused_result));
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**********************************************************************//**
Returns an index object if it is found in the dictionary cache.
@return	index, NULL if not found */
UNIV_INTERN
dict_index_t*
dict_index_get_if_in_cache(
/*=======================*/
	index_id_t	index_id)	/*!< in: index id */
	__attribute__((warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/** Whether and when to allow temporary index names */
enum check_name {
	/** Require all indexes to be complete. */
	CHECK_ALL_COMPLETE,
	/** Allow aborted online index creation. */
	CHECK_ABORTED_OK,
	/** Allow partial indexes to exist. */
	CHECK_PARTIAL_OK
};
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
	__attribute__((nonnull));
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
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Builds a typed data tuple out of a physical record.
@return	own: data tuple */
UNIV_INTERN
dtuple_t*
dict_index_build_data_tuple(
/*========================*/
	dict_index_t*	index,	/*!< in: index */
	rec_t*		rec,	/*!< in: record for which to build data tuple */
	ulint		n_fields,/*!< in: number of data fields */
	mem_heap_t*	heap)	/*!< in: memory heap where tuple created */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Gets the space id of the root of the index tree.
@return	space id */
UNIV_INLINE
ulint
dict_index_get_space(
/*=================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Sets the space id of the root of the index tree. */
UNIV_INLINE
void
dict_index_set_space(
/*=================*/
	dict_index_t*	index,	/*!< in/out: index */
	ulint		space)	/*!< in: space id */
	__attribute__((nonnull));
/*********************************************************************//**
Gets the page number of the root of the index tree.
@return	page number */
UNIV_INLINE
ulint
dict_index_get_page(
/*================*/
	const dict_index_t*	tree)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Gets the read-write lock of the index tree.
@return	read-write lock */
UNIV_INLINE
rw_lock_t*
dict_index_get_lock(
/*================*/
	dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Returns free space reserved for future updates of records. This is
relevant only in the case of many consecutive inserts, as updates
which make the records bigger might fragment the index.
@return	number of free bytes on page, reserved for updates */
UNIV_INLINE
ulint
dict_index_get_space_reserve(void);
/*==============================*/

/* Online index creation @{ */
/********************************************************************//**
Gets the status of online index creation.
@return the status */
UNIV_INLINE
enum online_index_status
dict_index_get_online_status(
/*=========================*/
	const dict_index_t*	index)	/*!< in: secondary index */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Sets the status of online index creation. */
UNIV_INLINE
void
dict_index_set_online_status(
/*=========================*/
	dict_index_t*			index,	/*!< in/out: index */
	enum online_index_status	status)	/*!< in: status */
	__attribute__((nonnull));
/********************************************************************//**
Determines if a secondary index is being or has been created online,
or if the table is being rebuilt online, allowing concurrent modifications
to the table.
@retval true if the index is being or has been built online, or
if this is a clustered index and the table is being or has been rebuilt online
@retval false if the index has been created or the table has been
rebuilt completely */
UNIV_INLINE
bool
dict_index_is_online_ddl(
/*=====================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
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
	UNIV_COLD __attribute__((nonnull));
/*********************************************************************//**
Attempts to log an operation on a secondary index that is being created.
@return TRUE if the operation was logged or the index creation failed;
FALSE if the index creation was completed */
UNIV_INLINE
ibool
dict_index_online_trylog(
/*=====================*/
	dict_index_t*	index,	/*!< in/out: index */
	const dtuple_t*	entry,	/*!< in: index entry */
	trx_id_t	trx_id,	/*!< in: transaction ID or 0 if not known */
	enum row_op	op)	/*!< in: operation on the index entry */
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Calculates the minimum record length in an index. */
UNIV_INTERN
ulint
dict_index_calc_min_rec_len(
/*========================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));
/********************************************************************//**
Reserves the dictionary system mutex for MySQL. */
UNIV_INTERN
void
dict_mutex_enter_for_mysql(void);
/*============================*/
/********************************************************************//**
Releases the dictionary system mutex for MySQL. */
UNIV_INTERN
void
dict_mutex_exit_for_mysql(void);
/*===========================*/
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
	__attribute__((nonnull));
/**********************************************************************//**
Unlock the latch that has been locked by dict_table_stats_lock() */
UNIV_INTERN
void
dict_table_stats_unlock(
/*====================*/
	const dict_table_t*	table,		/*!< in: table */
	ulint			latch_mode)	/*!< in: RW_S_LATCH or
						RW_X_LATCH */
	__attribute__((nonnull));
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
	__attribute__((nonnull, warn_unused_result));
/*********************************************************************//**
Removes an index from the cache */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/*!< in/out: table */
	dict_index_t*	index)	/*!< in, own: index */
	__attribute__((nonnull));
/**********************************************************************//**
Get index by name
@return	index, NULL if does not exist */
UNIV_INTERN
dict_index_t*
dict_table_get_index_on_name(
/*=========================*/
	dict_table_t*	table,	/*!< in: table */
	const char*	name)	/*!< in: name of the index to find */
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
In case there is more than one index with the same name return the index
with the min(id).
@return	index, NULL if does not exist */
UNIV_INTERN
dict_index_t*
dict_table_get_index_on_name_and_min_id(
/*====================================*/
	dict_table_t*	table,	/*!< in: table */
	const char*	name)	/*!< in: name of the index to find */
	__attribute__((nonnull, warn_unused_result));
/***************************************************************
Check whether a column exists in an FTS index. */
UNIV_INLINE
ulint
dict_table_is_fts_column(
/*=====================*/
				/* out: ULINT_UNDEFINED if no match else
				the offset within the vector */
	ib_vector_t*	indexes,/* in: vector containing only FTS indexes */
	ulint		col_no)	/* in: col number to search for */
	__attribute__((nonnull, warn_unused_result));
/**********************************************************************//**
Move a table to the non LRU end of the LRU list. */
UNIV_INTERN
void
dict_table_move_from_lru_to_non_lru(
/*================================*/
	dict_table_t*	table)	/*!< in: table to move from LRU to non-LRU */
	__attribute__((nonnull));
/**********************************************************************//**
Move a table to the LRU list from the non-LRU list. */
UNIV_INTERN
void
dict_table_move_from_non_lru_to_lru(
/*================================*/
	dict_table_t*	table)	/*!< in: table to move from non-LRU to LRU */
	__attribute__((nonnull));
/**********************************************************************//**
Move to the most recently used segment of the LRU list. */
UNIV_INTERN
void
dict_move_to_mru(
/*=============*/
	dict_table_t*	table)	/*!< in: table to move to MRU */
	__attribute__((nonnull));

/** Maximum number of columns in a foreign key constraint. Please Note MySQL
has a much lower limit on the number of columns allowed in a foreign key
constraint */
#define MAX_NUM_FK_COLUMNS		500

/* Buffers for storing detailed information about the latest foreign key
and unique key errors */
extern FILE*	dict_foreign_err_file;
extern ib_mutex_t	dict_foreign_err_mutex; /* mutex protecting the buffers */

/** the dictionary system */
extern dict_sys_t*	dict_sys;
/** the data dictionary rw-latch protecting dict_sys */
extern rw_lock_t	dict_operation_lock;

/* Dictionary system struct */
struct dict_sys_t{
	ib_mutex_t		mutex;		/*!< mutex protecting the data
					dictionary; protects also the
					disk-based dictionary system tables;
					this mutex serializes CREATE TABLE
					and DROP TABLE, as well as reading
					the dictionary data for a table from
					system tables */
	row_id_t	row_id;		/*!< the next row id to assign;
					NOTE that at a checkpoint this
					must be written to the dict system
					header and flushed to a file; in
					recovery this must be derived from
					the log records */
	hash_table_t*	table_hash;	/*!< hash table of the tables, based
					on name */
	hash_table_t*	table_id_hash;	/*!< hash table of the tables, based
					on id */
	ulint		size;		/*!< varying space in bytes occupied
					by the data dictionary table and
					index objects */
	dict_table_t*	sys_tables;	/*!< SYS_TABLES table */
	dict_table_t*	sys_columns;	/*!< SYS_COLUMNS table */
	dict_table_t*	sys_indexes;	/*!< SYS_INDEXES table */
	dict_table_t*	sys_fields;	/*!< SYS_FIELDS table */

	/*=============================*/
	UT_LIST_BASE_NODE_T(dict_table_t)
			table_LRU;	/*!< List of tables that can be evicted
					from the cache */
	UT_LIST_BASE_NODE_T(dict_table_t)
			table_non_LRU;	/*!< List of tables that can't be
					evicted from the cache */
};
#endif /* !UNIV_HOTBACKUP */

/** dummy index for ROW_FORMAT=REDUNDANT supremum and infimum records */
extern dict_index_t*	dict_ind_redundant;
/** dummy index for ROW_FORMAT=COMPACT supremum and infimum records */
extern dict_index_t*	dict_ind_compact;

/**********************************************************************//**
Inits dict_ind_redundant and dict_ind_compact. */
UNIV_INTERN
void
dict_ind_init(void);
/*===============*/

/* Auxiliary structs for checking a table definition @{ */

/* This struct is used to specify the name and type that a column must
have when checking a table's schema. */
struct dict_col_meta_t {
	const char*	name;		/* column name */
	ulint		mtype;		/* required column main type */
	ulint		prtype_mask;	/* required column precise type mask;
					if this is non-zero then all the
					bits it has set must also be set
					in the column's prtype */
	ulint		len;		/* required column length */
};

/* This struct is used for checking whether a given table exists and
whether it has a predefined schema (number of columns and columns names
and types) */
struct dict_table_schema_t {
	const char*		table_name;	/* the name of the table whose
						structure we are checking */
	ulint			n_cols;		/* the number of columns the
						table must have */
	dict_col_meta_t*	columns;	/* metadata for the columns;
						this array has n_cols
						elements */
	ulint			n_foreign;	/* number of foreign keys this
						table has, pointing to other
						tables (where this table is
						FK child) */
	ulint			n_referenced;	/* number of foreign keys other
						tables have, pointing to this
						table (where this table is
						parent) */
};
/* @} */

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
						message if != DB_SUCCESS and
						!= DB_TABLE_NOT_FOUND is
						returned */
	size_t			errstr_sz)	/*!< in: errstr size */
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull));

/**********************************************************************//**
Closes the data dictionary module. */
UNIV_INTERN
void
dict_close(void);
/*============*/
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Check whether the table is corrupted.
@return	nonzero for corrupted table, zero for valid tables */
UNIV_INLINE
ulint
dict_table_is_corrupted(
/*====================*/
	const dict_table_t*	table)	/*!< in: table */
	__attribute__((nonnull, warn_unused_result));

/**********************************************************************//**
Check whether the index is corrupted.
@return	nonzero for corrupted index, zero for valid indexes */
UNIV_INLINE
ulint
dict_index_is_corrupted(
/*====================*/
	const dict_index_t*	index)	/*!< in: index */
	__attribute__((nonnull, warn_unused_result));

#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Flags an index and table corrupted both in the data dictionary cache
and in the system table SYS_INDEXES. */
UNIV_INTERN
void
dict_set_corrupted(
/*===============*/
	dict_index_t*	index)		/*!< in/out: index */
	UNIV_COLD __attribute__((nonnull));

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
	__attribute__((nonnull));

/**********************************************************************//**
Flags a table with specified space_id corrupted in the table dictionary
cache.
@return TRUE if successful */
UNIV_INTERN
ibool
dict_set_corrupted_by_space(
/*========================*/
	ulint		space_id);	/*!< in: space ID */

/********************************************************************//**
Validate the table flags.
@return	true if valid. */
UNIV_INLINE
bool
dict_tf_is_valid(
/*=============*/
	ulint		flags)		/*!< in: table flags */
	__attribute__((warn_unused_result));

/********************************************************************//**
Check if the tablespace for the table has been discarded.
@return	true if the tablespace has been discarded. */
UNIV_INLINE
bool
dict_table_is_discarded(
/*====================*/
	const dict_table_t*	table)	/*!< in: table to check */
	__attribute__((nonnull, pure, warn_unused_result));

/********************************************************************//**
Check if it is a temporary table.
@return	true if temporary table flag is set. */
UNIV_INLINE
bool
dict_table_is_temporary(
/*====================*/
	const dict_table_t*	table)	/*!< in: table to check */
	__attribute__((nonnull, pure, warn_unused_result));

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
This function should be called whenever a page is successfully
compressed. Updates the compression padding information. */
UNIV_INTERN
void
dict_index_zip_success(
/*===================*/
	dict_index_t*	index)	/*!< in/out: index to be updated. */
	__attribute__((nonnull));
/*********************************************************************//**
This function should be called whenever a page compression attempt
fails. Updates the compression padding information. */
UNIV_INTERN
void
dict_index_zip_failure(
/*===================*/
	dict_index_t*	index)	/*!< in/out: index to be updated. */
	__attribute__((nonnull));
/*********************************************************************//**
Return the optimal page size, for which page will likely compress.
@return page size beyond which page may not compress*/
UNIV_INTERN
ulint
dict_index_zip_pad_optimal_page_size(
/*=================================*/
	dict_index_t*	index)	/*!< in: index for which page size
				is requested */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Convert table flag to row format string.
@return row format name */
UNIV_INTERN
const char*
dict_tf_to_row_format_string(
/*=========================*/
	ulint	table_flag);		/*!< in: row format setting */

#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "dict0dict.ic"
#endif

#endif
