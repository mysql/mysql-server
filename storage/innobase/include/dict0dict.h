/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.
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
#include "data0data.h"
#include "data0type.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "ut0byte.h"
#include "ut0mem.h"
#include "ut0new.h"
#include "ut0rnd.h"
#include <deque>

#ifndef UNIV_HOTBACKUP
# include "sync0rw.h"
/********************************************************************//**
Get the database name length in a table name.
@return database name length */
ulint
dict_get_db_name_len(
/*=================*/
	const char*	name)	/*!< in: table name in the form
				dbname '/' tablename */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Open a table from its database and table name, this is currently used by
foreign constraint parser to get the referenced table.
@return complete table name with database and table name, allocated from
heap memory passed in */
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
void
dict_foreign_free(
/*==============*/
	dict_foreign_t*	foreign);	/*!< in, own: foreign key struct */
/*********************************************************************//**
Finds the highest [number] for foreign key constraints of the table. Looks
only at the >= 4.0.18-format id's, which are of the form
databasename/tablename_ibfk_[number].
@return highest number, 0 if table has no new format foreign key constraints */
ulint
dict_table_get_highest_foreign_id(
/*==============================*/
	dict_table_t*	table);		/*!< in: table in the dictionary
					memory cache */
/********************************************************************//**
Return the end of table name where we have removed dbname and '/'.
@return table name */
const char*
dict_remove_db_name(
/*================*/
	const char*	name)	/*!< in: table name in the form
				dbname '/' tablename */
	MY_ATTRIBUTE((warn_unused_result));

/** Operation to perform when opening a table */
enum dict_table_op_t {
	/** Expect the tablespace to exist. */
	DICT_TABLE_OP_NORMAL = 0,
	/** Drop any orphan indexes after an aborted online index creation */
	DICT_TABLE_OP_DROP_ORPHAN,
	/** Silently load the tablespace if it does not exist,
	and do not load the definitions of incomplete indexes. */
	DICT_TABLE_OP_LOAD_TABLESPACE
};

/**********************************************************************//**
Returns a table object based on table id.
@return table, NULL if does not exist */
dict_table_t*
dict_table_open_on_id(
/*==================*/
	table_id_t	table_id,	/*!< in: table id */
	ibool		dict_locked,	/*!< in: TRUE=data dictionary locked */
	dict_table_op_t	table_op)	/*!< in: operation to perform */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Decrements the count of open handles to a table. */
void
dict_table_close(
/*=============*/
	dict_table_t*	table,		/*!< in/out: table */
	ibool		dict_locked,	/*!< in: TRUE=data dictionary locked */
	ibool		try_drop);	/*!< in: TRUE=try to drop any orphan
					indexes after an aborted online
					index creation */
/*********************************************************************//**
Closes the only open handle to a table and drops a table while assuring
that dict_sys->mutex is held the whole time.  This assures that the table
is not evicted after the close when the count of open handles goes to zero.
Because dict_sys->mutex is held, we do not need to call
dict_table_prevent_eviction().  */
void
dict_table_close_and_drop(
/*======================*/
	trx_t*		trx,		/*!< in: data dictionary transaction */
	dict_table_t*	table);		/*!< in/out: table */
/**********************************************************************//**
Inits the data dictionary module. */
void
dict_init(void);

/*********************************************************************//**
Gets the minimum number of bytes per character.
@return minimum multi-byte char size, in bytes */
UNIV_INLINE
ulint
dict_col_get_mbminlen(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets the maximum number of bytes per character.
@return maximum multi-byte char size, in bytes */
UNIV_INLINE
ulint
dict_col_get_mbmaxlen(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Sets the minimum and maximum number of bytes per character. */
UNIV_INLINE
void
dict_col_set_mbminmaxlen(
/*=====================*/
	dict_col_t*	col,		/*!< in/out: column */
	ulint		mbminlen,	/*!< in: minimum multi-byte
					character size, in bytes */
	ulint		mbmaxlen);	/*!< in: minimum multi-byte
					character size, in bytes */

/*********************************************************************//**
Gets the column data type. */
UNIV_INLINE
void
dict_col_copy_type(
/*===============*/
	const dict_col_t*	col,	/*!< in: column */
	dtype_t*		type);	/*!< out: data type */

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
	MY_ATTRIBUTE((warn_unused_result));

/** Determine maximum bytes of a virtual column need to be stored
in the undo log.
@param[in]	table		dict_table_t for the table
@param[in]	col_no		virtual column number
@return maximum bytes of virtual column to be stored in the undo log */
UNIV_INLINE
ulint
dict_max_v_field_len_store_undo(
	dict_table_t*		table,
	ulint			col_no);

#endif /* !UNIV_HOTBACKUP */
#ifdef UNIV_DEBUG
/*********************************************************************//**
Assert that a column and a data type match.
@return TRUE */
UNIV_INLINE
ibool
dict_col_type_assert_equal(
/*=======================*/
	const dict_col_t*	col,	/*!< in: column */
	const dtype_t*		type)	/*!< in: data type */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */
#ifndef UNIV_HOTBACKUP
/***********************************************************************//**
Returns the minimum size of the column.
@return minimum size */
UNIV_INLINE
ulint
dict_col_get_min_size(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************************//**
Returns the maximum size of the column.
@return maximum size */
UNIV_INLINE
ulint
dict_col_get_max_size(
/*==================*/
	const dict_col_t*	col)	/*!< in: column */
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************************//**
Returns the size of a fixed size column, 0 if not a fixed size column.
@return fixed size, or 0 */
UNIV_INLINE
ulint
dict_col_get_fixed_size(
/*====================*/
	const dict_col_t*	col,	/*!< in: column */
	ulint			comp)	/*!< in: nonzero=ROW_FORMAT=COMPACT  */
	MY_ATTRIBUTE((warn_unused_result));
/***********************************************************************//**
Returns the ROW_FORMAT=REDUNDANT stored SQL NULL size of a column.
For fixed length types it is the fixed length of the type, otherwise 0.
@return SQL null storage size in ROW_FORMAT=REDUNDANT */
UNIV_INLINE
ulint
dict_col_get_sql_null_size(
/*=======================*/
	const dict_col_t*	col,	/*!< in: column */
	ulint			comp)	/*!< in: nonzero=ROW_FORMAT=COMPACT  */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets the column number.
@return col->ind, table column position (starting from 0) */
UNIV_INLINE
ulint
dict_col_get_no(
/*============*/
	const dict_col_t*	col)	/*!< in: column */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets the column position in the clustered index. */
UNIV_INLINE
ulint
dict_col_get_clust_pos(
/*===================*/
	const dict_col_t*	col,		/*!< in: table column */
	const dict_index_t*	clust_index)	/*!< in: clustered index */
	MY_ATTRIBUTE((warn_unused_result));

/** Gets the column position in the given index.
@param[in]	col	table column
@param[in]	index	index to be searched for column
@return position of column in the given index. */
UNIV_INLINE
ulint
dict_col_get_index_pos(
	const dict_col_t*	col,
	const dict_index_t*	index)
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/****************************************************************//**
If the given column name is reserved for InnoDB system columns, return
TRUE.
@return TRUE if name is reserved */
ibool
dict_col_name_is_reserved(
/*======================*/
	const char*	name)	/*!< in: column name */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Acquire the autoinc lock. */
void
dict_table_autoinc_lock(
/*====================*/
	dict_table_t*	table);	/*!< in/out: table */

/********************************************************************//**
Unconditionally set the autoinc counter. */
void
dict_table_autoinc_initialize(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	ib_uint64_t	value);	/*!< in: next value to assign to a row */

/** Store autoinc value when the table is evicted.
@param[in]	table	table evicted */
void
dict_table_autoinc_store(
	const dict_table_t*	table);

/** Restore autoinc value when the table is loaded.
@param[in]	table	table loaded */
void
dict_table_autoinc_restore(
	dict_table_t*	table);

/********************************************************************//**
Reads the next autoinc value (== autoinc counter value), 0 if not yet
initialized.
@return value for a new row, or 0 */
ib_uint64_t
dict_table_autoinc_read(
/*====================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Updates the autoinc counter if the value supplied is greater than the
current value. */
void
dict_table_autoinc_update_if_greater(
/*=================================*/

	dict_table_t*	table,	/*!< in/out: table */
	ib_uint64_t	value);	/*!< in: value which was assigned to a row */
/********************************************************************//**
Release the autoinc lock. */
void
dict_table_autoinc_unlock(
/*======================*/
	dict_table_t*	table);	/*!< in/out: table */

#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Adds system columns to a table object. */
void
dict_table_add_system_columns(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap);	/*!< in: temporary heap */
#ifndef UNIV_HOTBACKUP
/** Mark if table has big rows.
@param[in,out]	table	table handler */
void
dict_table_set_big_rows(
	dict_table_t*	table)
	MY_ATTRIBUTE((nonnull));
/**********************************************************************//**
Adds a table object to the dictionary cache. */
void
dict_table_add_to_cache(
/*====================*/
	dict_table_t*	table,		/*!< in: table */
	ibool		can_be_evicted,	/*!< in: TRUE if can be evicted*/
	mem_heap_t*	heap);		/*!< in: temporary heap */

/**********************************************************************//**
Removes a table object from the dictionary cache. */
void
dict_table_remove_from_cache(
/*=========================*/
	dict_table_t*	table);	/*!< in, own: table */

/**********************************************************************//**
Removes a table object from the dictionary cache. */
void
dict_table_remove_from_cache_low(
/*=============================*/
	dict_table_t*	table,		/*!< in, own: table */
	ibool		lru_evict);	/*!< in: TRUE if table being evicted
					to make room in the table LRU list */
/**********************************************************************//**
Renames a table object.
@return TRUE if success */
dberr_t
dict_table_rename_in_cache(
/*=======================*/
	dict_table_t*	table,		/*!< in/out: table */
	const char*	new_name,	/*!< in: new name */
	ibool		rename_also_foreigns)
					/*!< in: in ALTER TABLE we want
					to preserve the original table name
					in constraints which reference it */
	MY_ATTRIBUTE((warn_unused_result));

/** Removes an index from the dictionary cache.
@param[in,out]	table	table whose index to remove
@param[in,out]	index	index to remove, this object is destroyed and must not
be accessed by the caller afterwards */
void
dict_index_remove_from_cache(
	dict_table_t*	table,
	dict_index_t*	index);

/**********************************************************************//**
Change the id of a table object in the dictionary cache. This is used in
DISCARD TABLESPACE. */
void
dict_table_change_id_in_cache(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table object already in cache */
	table_id_t	new_id);	/*!< in: new id to set */
/**********************************************************************//**
Removes a foreign constraint struct from the dictionary cache. */
void
dict_foreign_remove_from_cache(
/*===========================*/
	dict_foreign_t*	foreign);	/*!< in, own: foreign constraint */
/**********************************************************************//**
Adds a foreign key constraint object to the dictionary cache. May free
the object if there already is an object with the same identifier in.
At least one of foreign table or referenced table must already be in
the dictionary cache!
@return DB_SUCCESS or error code */
dberr_t
dict_foreign_add_to_cache(
/*======================*/
	dict_foreign_t*		foreign,
				/*!< in, own: foreign key constraint */
	const char**		col_names,
				/*!< in: column names, or NULL to use
				foreign->foreign_table->col_names */
	bool			check_charsets,
				/*!< in: whether to check charset
				compatibility */
	dict_err_ignore_t	ignore_err)
				/*!< in: error to be ignored */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Checks if a table is referenced by foreign keys.
@return TRUE if table is referenced by a foreign key */
ibool
dict_table_is_referenced_by_foreign_key(
/*====================================*/
	const dict_table_t*	table)	/*!< in: InnoDB table */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Replace the index passed in with another equivalent index in the
foreign key lists of the table.
@return whether all replacements were found */
bool
dict_foreign_replace_index(
/*=======================*/
	dict_table_t*		table,  /*!< in/out: table */
	const char**		col_names,
					/*!< in: column names, or NULL
					to use table->col_names */
	const dict_index_t*	index)	/*!< in: index to be replaced */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Determines whether a string starts with the specified keyword.
@return TRUE if str starts with keyword */
ibool
dict_str_starts_with_keyword(
/*=========================*/
	THD*		thd,		/*!< in: MySQL thread handle */
	const char*	str,		/*!< in: string to scan for keyword */
	const char*	keyword)	/*!< in: keyword to look for */
	MY_ATTRIBUTE((warn_unused_result));
/** Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
bot participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint.

@param[in]	trx		transaction
@param[in]	sql_string	table create statement where
				foreign keys are declared like:
				FOREIGN KEY (a, b) REFERENCES table2(c, d),
				table2 can be written also with the database
				name before it: test.table2; the default
				database id the database of parameter name
@param[in]	sql_length	length of sql_string
@param[in]	name		table full name in normalized form
@param[in]	reject_fks	if TRUE, fail with error code
				DB_CANNOT_ADD_CONSTRAINT if any
				foreign keys are found.
@return error code or DB_SUCCESS */
dberr_t
dict_create_foreign_constraints(
	trx_t*			trx,
	const char*		sql_string,
	size_t			sql_length,
	const char*		name,
	ibool			reject_fks)
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Parses the CONSTRAINT id's to be dropped in an ALTER TABLE statement.
@return DB_SUCCESS or DB_CANNOT_DROP_CONSTRAINT if syntax error or the
constraint id does not match */
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
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Returns a table object and increments its open handle count.
NOTE! This is a high-level function to be used mainly from outside the
'dict' directory. Inside this directory dict_table_get_low
is usually the appropriate function.
@param[in] table_name Table name
@param[in] dict_locked TRUE=data dictionary locked
@param[in] try_drop TRUE=try to drop any orphan indexes after
				an aborted online index creation
@param[in] ignore_err error to be ignored when loading the table
@return table, NULL if does not exist */
dict_table_t*
dict_table_open_on_name(
	const char*		table_name,
	ibool			dict_locked,
	ibool			try_drop,
	dict_err_ignore_t	ignore_err)
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Tries to find an index whose first fields are the columns in the array,
in the same order and is not marked for deletion and is not the same
as types_idx.
@return matching index, NULL if not found */
dict_index_t*
dict_foreign_find_index(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	const char**		col_names,
					/*!< in: column names, or NULL
					to use table->col_names */
	const char**		columns,/*!< in: array of column names */
	ulint			n_cols,	/*!< in: number of columns */
	const dict_index_t*	types_idx,
					/*!< in: NULL or an index
					whose types the column types
					must match */
	bool			check_charsets,
					/*!< in: whether to check
					charsets.  only has an effect
					if types_idx != NULL */
	ulint			check_null)
					/*!< in: nonzero if none of
					the columns must be declared
					NOT NULL */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Returns a column's name.
@return column name. NOTE: not guaranteed to stay valid if table is
modified in any way (columns added, etc.). */
const char*
dict_table_get_col_name(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			col_nr)	/*!< in: column number */
	MY_ATTRIBUTE((warn_unused_result));

/** Returns a virtual column's name.
@param[in]	table		table object
@param[in]	col_nr		virtual column number(nth virtual column)
@return column name. */
const char*
dict_table_get_v_col_name(
	const dict_table_t*	table,
	ulint			col_nr);

/** Check if the table has a given column.
@param[in]	table		table object
@param[in]	col_name	column name
@param[in]	col_nr		column number guessed, 0 as default
@return column number if the table has the specified column,
otherwise table->n_def */
ulint
dict_table_has_column(
	const dict_table_t*	table,
	const char*		col_name,
	ulint			col_nr = 0);

/**********************************************************************//**
Outputs info on foreign keys of a table. */
void
dict_print_info_on_foreign_keys(
/*============================*/
	ibool		create_table_format, /*!< in: if TRUE then print in
				a format suitable to be inserted into
				a CREATE TABLE, otherwise in the format
				of SHOW TABLE STATUS */
	FILE*		file,	/*!< in: file where to print */
	trx_t*		trx,	/*!< in: transaction */
	dict_table_t*	table);	/*!< in: table */
/**********************************************************************//**
Outputs info on a foreign key of a table in a format suitable for
CREATE TABLE. */
void
dict_print_info_on_foreign_key_in_create_format(
/*============================================*/
	FILE*		file,		/*!< in: file where to print */
	trx_t*		trx,		/*!< in: transaction */
	dict_foreign_t*	foreign,	/*!< in: foreign key constraint */
	ibool		add_newline);	/*!< in: whether to add a newline */
/*********************************************************************//**
Tries to find an index whose first fields are the columns in the array,
in the same order and is not marked for deletion and is not the same
as types_idx.
@return matching index, NULL if not found */
bool
dict_foreign_qualify_index(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	const char**		col_names,
					/*!< in: column names, or NULL
					to use table->col_names */
	const char**		columns,/*!< in: array of column names */
	ulint			n_cols,	/*!< in: number of columns */
	const dict_index_t*	index,	/*!< in: index to check */
	const dict_index_t*	types_idx,
					/*!< in: NULL or an index
					whose types the column types
					must match */
	bool			check_charsets,
					/*!< in: whether to check
					charsets.  only has an effect
					if types_idx != NULL */
	ulint			check_null)
					/*!< in: nonzero if none of
					the columns must be declared
					NOT NULL */
	MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/********************************************************************//**
Gets the first index on the table (the clustered index).
@return index, NULL if none exists */
UNIV_INLINE
dict_index_t*
dict_table_get_first_index(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the last index on the table.
@return index, NULL if none exists */
UNIV_INLINE
dict_index_t*
dict_table_get_last_index(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the next index on the table.
@return index, NULL if none left */
UNIV_INLINE
dict_index_t*
dict_table_get_next_index(
/*======================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
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
@return nonzero for clustered index, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_clust(
/*================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));

/** Check if index is auto-generated clustered index.
@param[in]	index	index

@return true if index is auto-generated clustered index. */
UNIV_INLINE
bool
dict_index_is_auto_gen_clust(
	const dict_index_t*	index);

/********************************************************************//**
Check whether the index is unique.
@return nonzero for unique index, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_unique(
/*=================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Check whether the index is a Spatial Index.
@return	nonzero for Spatial Index, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_spatial(
/*==================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/** Check whether the index contains a virtual column.
@param[in]	index	index
@return	nonzero for index on virtual column, zero for other indexes */
UNIV_INLINE
ulint
dict_index_has_virtual(
	const dict_index_t*	index);
/********************************************************************//**
Check whether the index is the insert buffer tree.
@return nonzero for insert buffer, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_ibuf(
/*===============*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Check whether the index is a secondary index or the insert buffer tree.
@return nonzero for insert buffer, zero for other indexes */
UNIV_INLINE
ulint
dict_index_is_sec_or_ibuf(
/*======================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));

/** Get all the FTS indexes on a table.
@param[in]	table	table
@param[out]	indexes	all FTS indexes on this table
@return number of FTS indexes */
ulint
dict_table_get_all_fts_indexes(
	const dict_table_t*	table,
	ib_vector_t*		indexes);

/********************************************************************//**
Gets the number of user-defined non-virtual columns in a table in the
dictionary cache.
@return number of user-defined (e.g., not ROW_ID) non-virtual
columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_user_cols(
/*=======================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/** Gets the number of user-defined virtual and non-virtual columns in a table
in the dictionary cache.
@param[in]	table	table
@return number of user-defined (e.g., not ROW_ID) columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_tot_u_cols(
	const dict_table_t*	table);
/********************************************************************//**
Gets the number of system columns in a table.
For intrinsic table on ROW_ID column is added for all other
tables TRX_ID and ROLL_PTR are all also appeneded.
@return number of system (e.g., ROW_ID) columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_sys_cols(
/*======================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the number of all non-virtual columns (also system) in a table
in the dictionary cache.
@return number of columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_cols(
/*==================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));

/** Gets the number of virtual columns in a table in the dictionary cache.
@param[in]	table	the table to check
@return number of virtual columns of a table */
UNIV_INLINE
ulint
dict_table_get_n_v_cols(
	const dict_table_t*	table);

/** Check if a table has indexed virtual columns
@param[in]	table	the table to check
@return true is the table has indexed virtual columns */
UNIV_INLINE
bool
dict_table_has_indexed_v_cols(
	const dict_table_t*	table);

/********************************************************************//**
Gets the approximately estimated number of rows in the table.
@return estimated number of rows */
UNIV_INLINE
ib_uint64_t
dict_table_get_n_rows(
/*==================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Increment the number of rows in the table by one.
Notice that this operation is not protected by any latch, the number is
approximate. */
UNIV_INLINE
void
dict_table_n_rows_inc(
/*==================*/
	dict_table_t*	table);	/*!< in/out: table */
/********************************************************************//**
Decrement the number of rows in the table by one.
Notice that this operation is not protected by any latch, the number is
approximate. */
UNIV_INLINE
void
dict_table_n_rows_dec(
/*==================*/
	dict_table_t*	table);	/*!< in/out: table */

/** Get nth virtual column
@param[in]	table	target table
@param[in]	col_nr	column number in MySQL Table definition
@return dict_v_col_t ptr */
dict_v_col_t*
dict_table_get_nth_v_col_mysql(
	const dict_table_t*	table,
	ulint			col_nr);

#ifdef UNIV_DEBUG
/********************************************************************//**
Gets the nth column of a table.
@return pointer to column object */
UNIV_INLINE
dict_col_t*
dict_table_get_nth_col(
/*===================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			pos)	/*!< in: position of column */
	MY_ATTRIBUTE((warn_unused_result));
/** Gets the nth virtual column of a table.
@param[in]	table	table
@param[in]	pos	position of virtual column
@return pointer to virtual column object */
UNIV_INLINE
dict_v_col_t*
dict_table_get_nth_v_col(
        const dict_table_t*	table,
        ulint			pos);
/********************************************************************//**
Gets the given system column of a table.
@return pointer to column object */
UNIV_INLINE
dict_col_t*
dict_table_get_sys_col(
/*===================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			sys)	/*!< in: DATA_ROW_ID, ... */
	MY_ATTRIBUTE((warn_unused_result));
#else /* UNIV_DEBUG */
#define dict_table_get_nth_col(table, pos)	\
((table)->cols + (pos))
#define dict_table_get_sys_col(table, sys)	\
((table)->cols + (table)->n_cols + (sys)	\
 - (dict_table_get_n_sys_cols(table)))
/* Get nth virtual columns */
#define dict_table_get_nth_v_col(table, pos)	((table)->v_cols + (pos))
#endif /* UNIV_DEBUG */
/********************************************************************//**
Gets the given system column number of a table.
@return column number */
UNIV_INLINE
ulint
dict_table_get_sys_col_no(
/*======================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			sys)	/*!< in: DATA_ROW_ID, ... */
	MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Returns the minimum data size of an index record.
@return minimum data size in bytes */
UNIV_INLINE
ulint
dict_index_get_min_size(
/*====================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Check whether the table uses the compact page format.
@return TRUE if table uses the compact page format */
UNIV_INLINE
ibool
dict_table_is_comp(
/*===============*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Determine the file format of a table.
@return file format version */
UNIV_INLINE
ulint
dict_table_get_format(
/*==================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Determine the file format from a dict_table_t::flags.
@return file format version */
UNIV_INLINE
ulint
dict_tf_get_format(
/*===============*/
	ulint		flags)		/*!< in: dict_table_t::flags */
	MY_ATTRIBUTE((warn_unused_result));

/** Set the various values in a dict_table_t::flags pointer.
@param[in,out]	flags,		Pointer to a 4 byte Table Flags
@param[in]	format,		File Format
@param[in]	zip_ssize	Zip Shift Size
@param[in]	use_data_dir	Table uses DATA DIRECTORY
@param[in]	shared_space	Table uses a General Shared Tablespace */
UNIV_INLINE
void
dict_tf_set(
	ulint*		flags,
	rec_format_t	format,
	ulint		zip_ssize,
	bool		use_data_dir,
	bool		shared_space);

/** Initialize a dict_table_t::flags pointer.
@param[in]	compact,	Table uses Compact or greater
@param[in]	zip_ssize	Zip Shift Size (log 2 minus 9)
@param[in]	atomic_blobs	Table uses Compressed or Dynamic
@param[in]	data_dir	Table uses DATA DIRECTORY
@param[in]	shared_space	Table uses a General Shared Tablespace */
UNIV_INLINE
ulint
dict_tf_init(
	bool		compact,
	ulint		zip_ssize,
	bool		atomic_blobs,
	bool		data_dir,
	bool		shared_space);

/** Convert a 32 bit integer table flags to the 32 bit FSP Flags.
Fsp Flags are written into the tablespace header at the offset
FSP_SPACE_FLAGS and are also stored in the fil_space_t::flags field.
The following chart shows the translation of the low order bit.
Other bits are the same.
========================= Low order bit ==========================
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
dict_table_t::flags |     0     |    1    |     1      |    1
fil_space_t::flags  |     0     |    0    |     1      |    1
==================================================================
@param[in]	table_flags	dict_table_t::flags
@param[in]	is_temp		whether the tablespace is temporary
@param[in]	is_encrypted	whether the tablespace is encrypted
@return tablespace flags (fil_space_t::flags) */
ulint
dict_tf_to_fsp_flags(
	ulint	table_flags,
	bool	is_temp,
	bool	is_encrypted = false)
	MY_ATTRIBUTE((const));

/** Extract the page size from table flags.
@param[in]	flags	flags
@return compressed page size, or 0 if not compressed */
UNIV_INLINE
const page_size_t
dict_tf_get_page_size(
	ulint	flags)
MY_ATTRIBUTE((const));

/** Determine the extent size (in pages) for the given table
@param[in]	table	the table whose extent size is being
			calculated.
@return extent size in pages (256, 128 or 64) */
ulint
dict_table_extent_size(
	const dict_table_t*	table);

/** Get the table page size.
@param[in]	table	table
@return compressed page size, or 0 if not compressed */
UNIV_INLINE
const page_size_t
dict_table_page_size(
	const dict_table_t*	table)
	MY_ATTRIBUTE((warn_unused_result));

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Obtain exclusive locks on all index trees of the table. This is to prevent
accessing index trees while InnoDB is updating internal metadata for
operations such as truncate tables. */
UNIV_INLINE
void
dict_table_x_lock_indexes(
/*======================*/
	dict_table_t*	table);	/*!< in: table */
/*********************************************************************//**
Release the exclusive locks on all index tree. */
UNIV_INLINE
void
dict_table_x_unlock_indexes(
/*========================*/
	dict_table_t*	table);	/*!< in: table */
/********************************************************************//**
Checks if a column is in the ordering columns of the clustered index of a
table. Column prefixes are treated like whole columns.
@return TRUE if the column, or its prefix, is in the clustered key */
ibool
dict_table_col_in_clustered_key(
/*============================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			n)	/*!< in: column number */
	MY_ATTRIBUTE((warn_unused_result));
/*******************************************************************//**
Check if the table has an FTS index.
@return TRUE if table has an FTS index */
UNIV_INLINE
ibool
dict_table_has_fts_index(
/*=====================*/
	dict_table_t*   table)		/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));
/** Copies types of virtual columns contained in table to tuple and sets all
fields of the tuple to the SQL NULL value.  This function should
be called right after dtuple_create().
@param[in,out]	tuple	data tuple
@param[in]	table	table
*/
void
dict_table_copy_v_types(
	dtuple_t*		tuple,
	const dict_table_t*	table);

/*******************************************************************//**
Copies types of columns contained in table to tuple and sets all
fields of the tuple to the SQL NULL value.  This function should
be called right after dtuple_create(). */
void
dict_table_copy_types(
/*==================*/
	dtuple_t*		tuple,	/*!< in/out: data tuple */
	const dict_table_t*	table);	/*!< in: table */
/********************************************************************
Wait until all the background threads of the given table have exited, i.e.,
bg_threads == 0. Note: bg_threads_mutex must be reserved when
calling this. */
void
dict_table_wait_for_bg_threads_to_exit(
/*===================================*/
	dict_table_t*	table,	/* in: table */
	ulint		delay);	/* in: time in microseconds to wait between
				checks of bg_threads. */
/**********************************************************************//**
Looks for an index with the given id. NOTE that we do not reserve
the dictionary mutex: this function is for emergency purposes like
printing info of a corrupt database page!
@return index or NULL if not found from cache */
dict_index_t*
dict_index_find_on_id_low(
/*======================*/
	index_id_t	id)	/*!< in: index id */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Make room in the table cache by evicting an unused table. The unused table
should not be part of FK relationship and currently not used in any user
transaction. There is no guarantee that it will remove a table.
@return number of tables evicted. */
ulint
dict_make_room_in_cache(
/*====================*/
	ulint		max_tables,	/*!< in: max tables allowed in cache */
	ulint		pct_check);	/*!< in: max percent to check */

#define BIG_ROW_SIZE	1024

/** Adds an index to the dictionary cache.
@param[in]	table	table on which the index is
@param[in]	index	index; NOTE! The index memory
			object is freed in this function!
@param[in]	page_no	root page number of the index
@param[in]	strict	TRUE=refuse to create the index
			if records could be too big to fit in
			an B-tree page
@return DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
dberr_t
dict_index_add_to_cache(
	dict_table_t*	table,
	dict_index_t*	index,
	ulint		page_no,
	ibool		strict)
	MY_ATTRIBUTE((warn_unused_result));

/** Clears the virtual column's index list before index is being freed.
@param[in]  index   Index being freed */
void
dict_index_remove_from_v_col_list(
	dict_index_t* index);

/** Adds an index to the dictionary cache, with possible indexing newly
added column.
@param[in]	table	table on which the index is
@param[in]	index	index; NOTE! The index memory
			object is freed in this function!
@param[in]	add_v	new virtual column that being added along with
			an add index call
@param[in]	page_no	root page number of the index
@param[in]	strict	TRUE=refuse to create the index
			if records could be too big to fit in
			an B-tree page
@return DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
dberr_t
dict_index_add_to_cache_w_vcol(
	dict_table_t*		table,
	dict_index_t*		index,
	const dict_add_v_col_t* add_v,
	ulint			page_no,
	ibool			strict)
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
Gets the number of fields in the internal representation of an index,
including fields added by the dictionary system.
@return number of fields */
UNIV_INLINE
ulint
dict_index_get_n_fields(
/*====================*/
	const dict_index_t*	index)	/*!< in: an internal
					representation of index (in
					the dictionary cache) */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the number of fields in the internal representation of an index
that uniquely determine the position of an index entry in the index, if
we do not take multiversioning into account: in the B-tree use the value
returned by dict_index_get_n_unique_in_tree.
@return number of fields */
UNIV_INLINE
ulint
dict_index_get_n_unique(
/*====================*/
	const dict_index_t*	index)	/*!< in: an internal representation
					of index (in the dictionary cache) */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the number of fields in the internal representation of an index
which uniquely determine the position of an index entry in the index, if
we also take multiversioning into account.
@return number of fields */
UNIV_INLINE
ulint
dict_index_get_n_unique_in_tree(
/*============================*/
	const dict_index_t*	index)	/*!< in: an internal representation
					of index (in the dictionary cache) */
	MY_ATTRIBUTE((warn_unused_result));

/** The number of fields in the nonleaf page of spatial index, except
the page no field. */
#define DICT_INDEX_SPATIAL_NODEPTR_SIZE	1
/**
Gets the number of fields on nonleaf page level in the internal representation
of an index which uniquely determine the position of an index entry in the
index, if we also take multiversioning into account. Note, it doesn't
include page no field.
@param[in]	index	index
@return number of fields */
UNIV_INLINE
ulint
dict_index_get_n_unique_in_tree_nonleaf(
	const dict_index_t*	index)
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the number of user-defined ordering fields in the index. In the internal
representation we add the row id to the ordering fields to make all indexes
unique, but this function returns the number of fields the user defined
in the index as ordering fields.
@return number of fields */
UNIV_INLINE
ulint
dict_index_get_n_ordering_defined_by_user(
/*======================================*/
	const dict_index_t*	index)	/*!< in: an internal representation
					of index (in the dictionary cache) */
	MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/********************************************************************//**
Gets the nth field of an index.
@return pointer to field object */
UNIV_INLINE
dict_field_t*
dict_index_get_nth_field(
/*=====================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			pos)	/*!< in: position of field */
	MY_ATTRIBUTE((warn_unused_result));
#else /* UNIV_DEBUG */
# define dict_index_get_nth_field(index, pos) ((index)->fields + (pos))
#endif /* UNIV_DEBUG */
/********************************************************************//**
Gets pointer to the nth column in an index.
@return column */
UNIV_INLINE
const dict_col_t*
dict_index_get_nth_col(
/*===================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			pos)	/*!< in: position of the field */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Gets the column number of the nth field in an index.
@return column number */
UNIV_INLINE
ulint
dict_index_get_nth_col_no(
/*======================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			pos)	/*!< in: position of the field */
	MY_ATTRIBUTE((warn_unused_result));
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
	MY_ATTRIBUTE((nonnull, warn_unused_result));
/** Looks for column n in an index.
@param[in]	index		index
@param[in]	n		column number
@param[in]	inc_prefix	true=consider column prefixes too
@param[in]	is_virtual	true==virtual column
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
ulint
dict_index_get_nth_col_or_prefix_pos(
	const dict_index_t*	index,		/*!< in: index */
	ulint			n,		/*!< in: column number */
	bool			inc_prefix,	/*!< in: TRUE=consider
						column prefixes too */
	bool			is_virtual)	/*!< in: is a virtual column */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Returns TRUE if the index contains a column or a prefix of that column.
@param[in]	index		index
@param[in]	n		column number
@param[in]	is_virtual	whether it is a virtual col
@return TRUE if contains the column or its prefix */
ibool
dict_index_contains_col_or_prefix(
/*==============================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			n,	/*!< in: column number */
	bool			is_virtual)
					/*!< in: whether it is a virtual col */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Looks for a matching field in an index. The column has to be the same. The
column in index must be complete, or must contain a prefix longer than the
column in index2. That is, we must be able to construct the prefix in index2
from the prefix in index.
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
ulint
dict_index_get_nth_field_pos(
/*=========================*/
	const dict_index_t*	index,	/*!< in: index from which to search */
	const dict_index_t*	index2,	/*!< in: index */
	ulint			n)	/*!< in: field number in index2 */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Looks for column n position in the clustered index.
@return position in internal representation of the clustered index */
ulint
dict_table_get_nth_col_pos(
/*=======================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			n)	/*!< in: column number */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Returns the position of a system column in an index.
@return position, ULINT_UNDEFINED if not contained */
UNIV_INLINE
ulint
dict_index_get_sys_col_pos(
/*=======================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			type)	/*!< in: DATA_ROW_ID, ... */
	MY_ATTRIBUTE((warn_unused_result));
/*******************************************************************//**
Adds a column to index. */
void
dict_index_add_col(
/*===============*/
	dict_index_t*		index,		/*!< in/out: index */
	const dict_table_t*	table,		/*!< in: table */
	dict_col_t*		col,		/*!< in: column */
	ulint			prefix_len);	/*!< in: column prefix length */
#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Copies types of fields contained in index to tuple. */
void
dict_index_copy_types(
/*==================*/
	dtuple_t*		tuple,		/*!< in/out: data tuple */
	const dict_index_t*	index,		/*!< in: index */
	ulint			n_fields);	/*!< in: number of
						field types to copy */
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Gets the field column.
@return field->col, pointer to the table column */
UNIV_INLINE
const dict_col_t*
dict_field_get_col(
/*===============*/
	const dict_field_t*	field)	/*!< in: index field */
	MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Returns an index object if it is found in the dictionary cache.
Assumes that dict_sys->mutex is already being held.
@return index, NULL if not found */
dict_index_t*
dict_index_get_if_in_cache_low(
/*===========================*/
	index_id_t	index_id)	/*!< in: index id */
	MY_ATTRIBUTE((warn_unused_result));
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**********************************************************************//**
Returns an index object if it is found in the dictionary cache.
@return index, NULL if not found */
dict_index_t*
dict_index_get_if_in_cache(
/*=======================*/
	index_id_t	index_id)	/*!< in: index id */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#ifdef UNIV_DEBUG
/**********************************************************************//**
Checks that a tuple has n_fields_cmp value in a sensible range, so that
no comparison can occur with the page number field in a node pointer.
@return TRUE if ok */
ibool
dict_index_check_search_tuple(
/*==========================*/
	const dict_index_t*	index,	/*!< in: index tree */
	const dtuple_t*		tuple)	/*!< in: tuple used in a search */
	MY_ATTRIBUTE((warn_unused_result));
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
void
dict_table_check_for_dup_indexes(
/*=============================*/
	const dict_table_t*	table,	/*!< in: Check for dup indexes
					in this table */
	enum check_name		check);	/*!< in: whether and when to allow
					temporary index names */
#endif /* UNIV_DEBUG */
/**********************************************************************//**
Builds a node pointer out of a physical record and a page number.
@return own: node pointer */
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
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Copies an initial segment of a physical record, long enough to specify an
index entry uniquely.
@return pointer to the prefix record */
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
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Builds a typed data tuple out of a physical record.
@return own: data tuple */
dtuple_t*
dict_index_build_data_tuple(
/*========================*/
	dict_index_t*	index,	/*!< in: index */
	rec_t*		rec,	/*!< in: record for which to build data tuple */
	ulint		n_fields,/*!< in: number of data fields */
	mem_heap_t*	heap)	/*!< in: memory heap where tuple created */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets the space id of the root of the index tree.
@return space id */
UNIV_INLINE
ulint
dict_index_get_space(
/*=================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Sets the space id of the root of the index tree. */
UNIV_INLINE
void
dict_index_set_space(
/*=================*/
	dict_index_t*	index,	/*!< in/out: index */
	ulint		space);	/*!< in: space id */

/*********************************************************************//**
Gets the page number of the root of the index tree.
@return page number */
UNIV_INLINE
ulint
dict_index_get_page(
/*================*/
	const dict_index_t*	tree)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets the read-write lock of the index tree.
@return read-write lock */
UNIV_INLINE
rw_lock_t*
dict_index_get_lock(
/*================*/
	dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Returns free space reserved for future updates of records. This is
relevant only in the case of many consecutive inserts, as updates
which make the records bigger might fragment the index.
@return number of free bytes on page, reserved for updates */
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
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Sets the status of online index creation. */
UNIV_INLINE
void
dict_index_set_online_status(
/*=========================*/
	dict_index_t*			index,		/*!< in/out: index */
	enum online_index_status	status);	/*!< in: status */
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
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Calculates the minimum record length in an index. */
ulint
dict_index_calc_min_rec_len(
/*========================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Reserves the dictionary system mutex for MySQL. */
void
dict_mutex_enter_for_mysql(void);
/*============================*/
/********************************************************************//**
Releases the dictionary system mutex for MySQL. */
void
dict_mutex_exit_for_mysql(void);
/*===========================*/

/** Create a dict_table_t's stats latch or delay for lazy creation.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]	table	table whose stats latch to create
@param[in]	enabled	if false then the latch is disabled
and dict_table_stats_lock()/unlock() become noop on this table. */
void
dict_table_stats_latch_create(
	dict_table_t*	table,
	bool		enabled);

/** Destroy a dict_table_t's stats latch.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]	table	table whose stats latch to destroy */
void
dict_table_stats_latch_destroy(
	dict_table_t*	table);

/** Lock the appropriate latch to protect a given table's statistics.
@param[in]	table		table whose stats to lock
@param[in]	latch_mode	RW_S_LATCH or RW_X_LATCH */
void
dict_table_stats_lock(
	dict_table_t*	table,
	ulint		latch_mode);

/** Unlock the latch that has been locked by dict_table_stats_lock().
@param[in]	table		table whose stats to unlock
@param[in]	latch_mode	RW_S_LATCH or RW_X_LATCH */
void
dict_table_stats_unlock(
	dict_table_t*	table,
	ulint		latch_mode);

/********************************************************************//**
Checks if the database name in two table names is the same.
@return TRUE if same db name */
ibool
dict_tables_have_same_db(
/*=====================*/
	const char*	name1,	/*!< in: table name in the form
				dbname '/' tablename */
	const char*	name2)	/*!< in: table name in the form
				dbname '/' tablename */
	MY_ATTRIBUTE((warn_unused_result));
/** Get an index by name.
@param[in]	table		the table where to look for the index
@param[in]	name		the index name to look for
@param[in]	committed	true=search for committed,
false=search for uncommitted
@return index, NULL if does not exist */
dict_index_t*
dict_table_get_index_on_name(
	dict_table_t*	table,
	const char*	name,
	bool		committed=true)
	MY_ATTRIBUTE((warn_unused_result));
/** Get an index by name.
@param[in]	table		the table where to look for the index
@param[in]	name		the index name to look for
@param[in]	committed	true=search for committed,
false=search for uncommitted
@return index, NULL if does not exist */
inline
const dict_index_t*
dict_table_get_index_on_name(
	const dict_table_t*	table,
	const char*		name,
	bool			committed=true)
{
	return(dict_table_get_index_on_name(
		       const_cast<dict_table_t*>(table), name, committed));
}

/***************************************************************
Check whether a column exists in an FTS index. */
UNIV_INLINE
ulint
dict_table_is_fts_column(
/*=====================*/
				/* out: ULINT_UNDEFINED if no match else
				the offset within the vector */
	ib_vector_t*	indexes,/* in: vector containing only FTS indexes */
	ulint		col_no,	/* in: col number to search for */
	bool		is_virtual)/*!< in: whether it is a virtual column */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Prevent table eviction by moving a table to the non-LRU list from the
LRU list if it is not already there. */
UNIV_INLINE
void
dict_table_prevent_eviction(
/*========================*/
	dict_table_t*	table);	/*!< in: table to prevent eviction */
/**********************************************************************//**
Move a table to the non LRU end of the LRU list. */
void
dict_table_move_from_lru_to_non_lru(
/*================================*/
	dict_table_t*	table);	/*!< in: table to move from LRU to non-LRU */
/** Looks for an index with the given id given a table instance.
@param[in]	table	table instance
@param[in]	id	index id
@return index or NULL */
dict_index_t*
dict_table_find_index_on_id(
	const dict_table_t*	table,
	index_id_t		id);
/**********************************************************************//**
Move to the most recently used segment of the LRU list. */
void
dict_move_to_mru(
/*=============*/
	dict_table_t*	table);	/*!< in: table to move to MRU */

/** Maximum number of columns in a foreign key constraint. Please Note MySQL
has a much lower limit on the number of columns allowed in a foreign key
constraint */
#define MAX_NUM_FK_COLUMNS		500

/* Buffers for storing detailed information about the latest foreign key
and unique key errors */
extern FILE*		dict_foreign_err_file;
extern ib_mutex_t	dict_foreign_err_mutex; /* mutex protecting the
						foreign key error messages */

/** the dictionary system */
extern dict_sys_t*	dict_sys;
/** the data dictionary rw-latch protecting dict_sys */
extern rw_lock_t*	dict_operation_lock;

typedef std::map<table_id_t, ib_uint64_t> autoinc_map_t;

/* Dictionary system struct */
struct dict_sys_t{
	DictSysMutex	mutex;		/*!< mutex protecting the data
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
	lint		size;		/*!< varying space in bytes occupied
					by the data dictionary table and
					index objects */
	dict_table_t*	sys_tables;	/*!< SYS_TABLES table */
	dict_table_t*	sys_columns;	/*!< SYS_COLUMNS table */
	dict_table_t*	sys_indexes;	/*!< SYS_INDEXES table */
	dict_table_t*	sys_fields;	/*!< SYS_FIELDS table */
	dict_table_t*	sys_virtual;	/*!< SYS_VIRTUAL table */

	/*=============================*/
	UT_LIST_BASE_NODE_T(dict_table_t)
			table_LRU;	/*!< List of tables that can be evicted
					from the cache */
	UT_LIST_BASE_NODE_T(dict_table_t)
			table_non_LRU;	/*!< List of tables that can't be
					evicted from the cache */
	autoinc_map_t*	autoinc_map;	/*!< Map to store table id and autoinc
					when table is evicted */
};
#endif /* !UNIV_HOTBACKUP */

/** dummy index for ROW_FORMAT=REDUNDANT supremum and infimum records */
extern dict_index_t*	dict_ind_redundant;

/**********************************************************************//**
Inits dict_ind_redundant. */
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
whether it has a predefined schema (number of columns and column names
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
	MY_ATTRIBUTE((warn_unused_result));
/* @} */

/*********************************************************************//**
Converts a database and table name from filesystem encoding
(e.g. d@i1b/a@q1b@1Kc, same format as used in dict_table_t::name) in two
strings in UTF8 encoding (e.g. dцb and aюbØc). The output buffers must be
at least MAX_DB_UTF8_LEN and MAX_TABLE_UTF8_LEN bytes. */
void
dict_fs2utf8(
/*=========*/
	const char*	db_and_table,	/*!< in: database and table names,
					e.g. d@i1b/a@q1b@1Kc */
	char*		db_utf8,	/*!< out: database name, e.g. dцb */
	size_t		db_utf8_size,	/*!< in: dbname_utf8 size */
	char*		table_utf8,	/*!< out: table name, e.g. aюbØc */
	size_t		table_utf8_size); /*!< in: table_utf8 size */

/** Resize the hash tables besed on the current buffer pool size. */
void
dict_resize();

/**********************************************************************//**
Closes the data dictionary module. */
void
dict_close(void);
/*============*/
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Check whether the table is corrupted.
@return nonzero for corrupted table, zero for valid tables */
UNIV_INLINE
ulint
dict_table_is_corrupted(
/*====================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));

/**********************************************************************//**
Check whether the index is corrupted.
@return nonzero for corrupted index, zero for valid indexes */
UNIV_INLINE
ulint
dict_index_is_corrupted(
/*====================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));

#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Flags an index and table corrupted both in the data dictionary cache
and in the system table SYS_INDEXES. */
void
dict_set_corrupted(
/*===============*/
	dict_index_t*	index,	/*!< in/out: index */
	trx_t*		trx,	/*!< in/out: transaction */
	const char*	ctx);	/*!< in: context */

/** Flags an index corrupted in the data dictionary cache only. This
is used mostly to mark a corrupted index when index's own dictionary
is corrupted, and we force to load such index for repair purpose
@param[in,out]	index	index that is corrupted */
void
dict_set_corrupted_index_cache_only(
	dict_index_t*	index);

/**********************************************************************//**
Flags a table with specified space_id corrupted in the table dictionary
cache.
@return TRUE if successful */
ibool
dict_set_corrupted_by_space(
/*========================*/
	ulint		space_id);	/*!< in: space ID */

/** Sets merge_threshold in the SYS_INDEXES
@param[in,out]	index		index
@param[in]	merge_threshold	value to set */
void
dict_index_set_merge_threshold(
	dict_index_t*	index,
	ulint		merge_threshold);

#ifdef UNIV_DEBUG
/** Sets merge_threshold for all indexes in dictionary cache for debug.
@param[in]	merge_threshold_all	value to set for all indexes */
void
dict_set_merge_threshold_all_debug(
	uint	merge_threshold_all);
#endif /* UNIV_DEBUG */

/** Validate the table flags.
@param[in]	flags	Table flags
@return true if valid. */
UNIV_INLINE
bool
dict_tf_is_valid(
	ulint	flags);

/** Validate both table flags and table flags2 and make sure they
are compatible.
@param[in]	flags	Table flags
@param[in]	flags2	Table flags2
@return true if valid. */
UNIV_INLINE
bool
dict_tf2_is_valid(
	ulint	flags,
	ulint	flags2);

/********************************************************************//**
Check if the tablespace for the table has been discarded.
@return true if the tablespace has been discarded. */
UNIV_INLINE
bool
dict_table_is_discarded(
/*====================*/
	const dict_table_t*	table)	/*!< in: table to check */
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Check if it is a temporary table.
@return true if temporary table flag is set. */
UNIV_INLINE
bool
dict_table_is_temporary(
/*====================*/
	const dict_table_t*	table)	/*!< in: table to check */
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Check if it is a encrypted table.
@return true if table encryption flag is set. */
UNIV_INLINE
bool
dict_table_is_encrypted(
/*====================*/
	const dict_table_t*	table)	/*!< in: table to check */
	MY_ATTRIBUTE((warn_unused_result));

/** Check whether the table is intrinsic.
An intrinsic table is a special kind of temporary table that
is invisible to the end user.  It is created internally by the MySQL server
layer or other module connected to InnoDB in order to gather and use data
as part of a larger task.  Since access to it must be as fast as possible,
it does not need UNDO semantics, system fields DB_TRX_ID & DB_ROLL_PTR,
doublewrite, checksum, insert buffer, use of the shared data dictionary,
locking, or even a transaction.  In short, these are not ACID tables at all,
just temporary

@param[in]	table	table to check
@return true if intrinsic table flag is set. */
UNIV_INLINE
bool
dict_table_is_intrinsic(
	const dict_table_t*	table)
	MY_ATTRIBUTE((warn_unused_result));

/** Check if the table is in a shared tablespace (System or General).
@param[in]	id	Space ID to check
@return true if id is a shared tablespace, false if not. */
UNIV_INLINE
bool
dict_table_in_shared_tablespace(
	const dict_table_t*	table)
	MY_ATTRIBUTE((warn_unused_result));

/** Check whether locking is disabled for this table.
Currently this is done for intrinsic table as their visibility is limited
to the connection only.

@param[in]	table	table to check
@return true if locking is disabled. */
UNIV_INLINE
bool
dict_table_is_locking_disabled(
	const dict_table_t*	table)
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Turn-off redo-logging if temporary table. */
UNIV_INLINE
void
dict_disable_redo_if_temporary(
/*===========================*/
	const dict_table_t*	table,	/*!< in: table to check */
	mtr_t*			mtr);	/*!< out: mini-transaction */

/** Get table session row-id and increment the row-id counter for next use.
@param[in,out]	table	table handler
@return next table local row-id. */
UNIV_INLINE
row_id_t
dict_table_get_next_table_sess_row_id(
	dict_table_t*		table);

/** Get table session trx-id and increment the trx-id counter for next use.
@param[in,out]	table	table handler
@return next table local trx-id. */
UNIV_INLINE
trx_id_t
dict_table_get_next_table_sess_trx_id(
	dict_table_t*		table);

/** Get current session trx-id.
@param[in]	table	table handler
@return table local trx-id. */
UNIV_INLINE
trx_id_t
dict_table_get_curr_table_sess_trx_id(
	const dict_table_t*	table);

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
This function should be called whenever a page is successfully
compressed. Updates the compression padding information. */
void
dict_index_zip_success(
/*===================*/
	dict_index_t*	index);	/*!< in/out: index to be updated. */
/*********************************************************************//**
This function should be called whenever a page compression attempt
fails. Updates the compression padding information. */
void
dict_index_zip_failure(
/*===================*/
	dict_index_t*	index);	/*!< in/out: index to be updated. */
/*********************************************************************//**
Return the optimal page size, for which page will likely compress.
@return page size beyond which page may not compress*/
ulint
dict_index_zip_pad_optimal_page_size(
/*=================================*/
	dict_index_t*	index)	/*!< in: index for which page size
				is requested */
	MY_ATTRIBUTE((warn_unused_result));
/*************************************************************//**
Convert table flag to row format string.
@return row format name */
const char*
dict_tf_to_row_format_string(
/*=========================*/
	ulint	table_flag);		/*!< in: row format setting */
/****************************************************************//**
Return maximum size of the node pointer record.
@return maximum size of the record in bytes */
ulint
dict_index_node_ptr_max_size(
/*=========================*/
	const dict_index_t*	index)	/*!< in: index */
	MY_ATTRIBUTE((warn_unused_result));
/*****************************************************************//**
Get index by first field of the index
@return index which is having first field matches
with the field present in field_index position of table */
UNIV_INLINE
dict_index_t*
dict_table_get_index_on_first_col(
/*==============================*/
	const dict_table_t*	table,		/*!< in: table */
	ulint			col_index);	/*!< in: position of column
						in table */
/** Check if a column is a virtual column
@param[in]	col	column
@return true if it is a virtual column, false otherwise */
UNIV_INLINE
bool
dict_col_is_virtual(
	const dict_col_t*	col);

/** encode number of columns and number of virtual columns in one
4 bytes value. We could do this because the number of columns in
InnoDB is limited to 1017
@param[in]	n_col	number of non-virtual column
@param[in]	n_v_col	number of virtual column
@return encoded value */
UNIV_INLINE
ulint
dict_table_encode_n_col(
	ulint	n_col,
	ulint	n_v_col);

/** Decode number of virtual and non-virtual columns in one 4 bytes value.
@param[in]	encoded	encoded value
@param[in,out]	n_col	number of non-virtual column
@param[in,out]	n_v_col	number of virtual column */
UNIV_INLINE
void
dict_table_decode_n_col(
	ulint	encoded,
	ulint*	n_col,
	ulint*	n_v_col);

/** Look for any dictionary objects that are found in the given tablespace.
@param[in]	space_id	Tablespace ID to search for.
@return true if tablespace is empty. */
bool
dict_space_is_empty(
	ulint	space_id);

/** Find the space_id for the given name in sys_tablespaces.
@param[in]	name	Tablespace name to search for.
@return the tablespace ID. */
ulint
dict_space_get_id(
	const char*	name);

/** Free the virtual column template
@param[in,out]	vc_templ	virtual column template */
UNIV_INLINE
void
dict_free_vc_templ(
	dict_vcol_templ_t*	vc_templ);

/** Check whether the table have virtual index.
@param[in]	table	InnoDB table
@return true if the table have virtual index, false otherwise. */
UNIV_INLINE
bool
dict_table_have_virtual_index(
	dict_table_t*	table);

/** Allocate memory for intrinsic cache elements in the index
 * @param[in]      index   index object */
UNIV_INLINE
void
dict_allocate_mem_intrinsic_cache(
                dict_index_t*           index);

#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "dict0dict.ic"
#endif

#endif
