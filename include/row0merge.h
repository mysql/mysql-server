/*****************************************************************************

Copyright (c) 2005, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/row0merge.h
Index build routines using a merge sort

Created 13/06/2005 Jan Lindstrom
*******************************************************/

#ifndef row0merge_h
#define row0merge_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "mtr0mtr.h"
#include "rem0types.h"
#include "rem0rec.h"
#include "read0types.h"
#include "btr0types.h"
#include "row0mysql.h"
#include "lock0types.h"

/** Index field definition */
struct merge_index_field_struct {
	ulint		prefix_len;	/*!< column prefix length, or 0
					if indexing the whole column */
	const char*	field_name;	/*!< field name */
};

/** Index field definition */
typedef struct merge_index_field_struct merge_index_field_t;

/** Definition of an index being created */
struct merge_index_def_struct {
	const char*		name;		/*!< index name */
	ulint			ind_type;	/*!< 0, DICT_UNIQUE,
						or DICT_CLUSTERED */
	ulint			n_fields;	/*!< number of fields
						in index */
	merge_index_field_t*	fields;		/*!< field definitions */
};

/** Definition of an index being created */
typedef struct merge_index_def_struct merge_index_def_t;

/*********************************************************************//**
Sets an exclusive lock on a table, for the duration of creating indexes.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_merge_lock_table(
/*=================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode);		/*!< in: LOCK_X or LOCK_S */
/*********************************************************************//**
Drop an index from the InnoDB system tables.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed. */
UNIV_INTERN
void
row_merge_drop_index(
/*=================*/
	dict_index_t*	index,	/*!< in: index to be removed */
	dict_table_t*	table,	/*!< in: table */
	trx_t*		trx);	/*!< in: transaction handle */
/*********************************************************************//**
Drop those indexes which were created before an error occurred when
building an index.  The data dictionary must have been locked
exclusively by the caller, because the transaction will not be
committed. */
UNIV_INTERN
void
row_merge_drop_indexes(
/*===================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table,		/*!< in: table containing the indexes */
	dict_index_t**	index,		/*!< in: indexes to drop */
	ulint		num_created);	/*!< in: number of elements in index[] */
/*********************************************************************//**
Drop all partially created indexes during crash recovery. */
UNIV_INTERN
void
row_merge_drop_temp_indexes(void);
/*=============================*/
/*********************************************************************//**
Rename the tables in the data dictionary.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_merge_rename_tables(
/*====================*/
	dict_table_t*	old_table,	/*!< in/out: old table, renamed to
					tmp_name */
	dict_table_t*	new_table,	/*!< in/out: new table, renamed to
					old_table->name */
	const char*	tmp_name,	/*!< in: new name for old_table */
	trx_t*		trx);		/*!< in: transaction handle */

/*********************************************************************//**
Create a temporary table for creating a primary key, using the definition
of an existing table.
@return	table, or NULL on error */
UNIV_INTERN
dict_table_t*
row_merge_create_temporary_table(
/*=============================*/
	const char*		table_name,	/*!< in: new table name */
	const merge_index_def_t*index_def,	/*!< in: the index definition
						of the primary key */
	const dict_table_t*	table,		/*!< in: old table definition */
	trx_t*			trx);		/*!< in/out: transaction
						(sets error_state) */
/*********************************************************************//**
Rename the temporary indexes in the dictionary to permanent ones.  The
data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed.
@return	DB_SUCCESS if all OK */
UNIV_INTERN
ulint
row_merge_rename_indexes(
/*=====================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table);		/*!< in/out: table with new indexes */
/*********************************************************************//**
Create the index and load in to the dictionary.
@return	index, or NULL on error */
UNIV_INTERN
dict_index_t*
row_merge_create_index(
/*===================*/
	trx_t*			trx,	/*!< in/out: trx (sets error_state) */
	dict_table_t*		table,	/*!< in: the index is on this table */
	const merge_index_def_t*index_def);
					/*!< in: the index definition */
/*********************************************************************//**
Check if a transaction can use an index.
@return	TRUE if index can be used by the transaction else FALSE */
UNIV_INTERN
ibool
row_merge_is_index_usable(
/*======================*/
	const trx_t*		trx,	/*!< in: transaction */
	const dict_index_t*	index);	/*!< in: index to check */
/*********************************************************************//**
If there are views that refer to the old table name then we "attach" to
the new instance of the table else we drop it immediately.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_drop_table(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table);		/*!< in: table instance to drop */

/*********************************************************************//**
Build indexes on a table by reading a clustered index,
creating a temporary file containing index entries, merge sorting
these index entries and inserting sorted index entries to indexes.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_build_indexes(
/*====================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	old_table,	/*!< in: table where rows are
					read from */
	dict_table_t*	new_table,	/*!< in: table where indexes are
					created; identical to old_table
					unless creating a PRIMARY KEY */
	dict_index_t**	indexes,	/*!< in: indexes to be created */
	ulint		n_indexes,	/*!< in: size of indexes[] */
	struct TABLE*	table);		/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
#endif /* row0merge.h */
