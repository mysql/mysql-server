/*****************************************************************************

Copyright (c) 1996, 2009, Innobase Oy. All Rights Reserved.

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

/******************************************************
Data dictionary system

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0dict_h
#define dict0dict_h

#include "univ.i"
#include "dict0types.h"
#include "dict0mem.h"
#include "data0type.h"
#include "data0data.h"
#include "sync0sync.h"
#include "sync0rw.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "ut0mem.h"
#include "ut0lst.h"
#include "hash0hash.h"
#include "ut0rnd.h"
#include "ut0byte.h"
#include "trx0types.h"

#ifndef UNIV_HOTBACKUP
/**********************************************************************
Makes all characters in a NUL-terminated UTF-8 string lower case. */
UNIV_INTERN
void
dict_casedn_str(
/*============*/
	char*	a);	/* in/out: string to put in lower case */
#endif /* !UNIV_HOTBACKUP */
/************************************************************************
Get the database name length in a table name. */
UNIV_INTERN
ulint
dict_get_db_name_len(
/*=================*/
				/* out: database name length */
	const char*	name);	/* in: table name in the form
				dbname '/' tablename */
/************************************************************************
Return the end of table name where we have removed dbname and '/'. */

const char*
dict_remove_db_name(
/*================*/
				/* out: table name */
	const char*	name);	/* in: table name in the form
				dbname '/' tablename */
/**************************************************************************
Returns a table object based on table id. */
UNIV_INTERN
dict_table_t*
dict_table_get_on_id(
/*=================*/
                                /* out: table, NULL if does not exist */
        dulint  table_id,       /* in: table id */
        trx_t*  trx);           /* in: transaction handle */
/************************************************************************
Decrements the count of open MySQL handles to a table. */
UNIV_INTERN
void
dict_table_decrement_handle_count(
/*==============================*/
	dict_table_t*	table,		/* in/out: table */
	ibool		dict_locked);	/* in: TRUE=data dictionary locked */
/**************************************************************************
Inits the data dictionary module. */
UNIV_INTERN
void
dict_init(void);
/*===========*/
/************************************************************************
Gets the space id of every table of the data dictionary and makes a linear
list and a hash table of them to the data dictionary cache. This function
can be called at database startup if we did not need to do a crash recovery.
In crash recovery we must scan the space id's from the .ibd files in MySQL
database directories. */
UNIV_INTERN
void
dict_load_space_id_list(void);
/*=========================*/
/*************************************************************************
Gets the column data type. */
UNIV_INLINE
void
dict_col_copy_type(
/*===============*/
	const dict_col_t*	col,	/* in: column */
	dtype_t*		type);	/* out: data type */
#ifdef UNIV_DEBUG
/*************************************************************************
Assert that a column and a data type match. */
UNIV_INLINE
ibool
dict_col_type_assert_equal(
/*=======================*/
					/* out: TRUE */
	const dict_col_t*	col,	/* in: column */
	const dtype_t*		type);	/* in: data type */
#endif /* UNIV_DEBUG */
/***************************************************************************
Returns the minimum size of the column. */
UNIV_INLINE
ulint
dict_col_get_min_size(
/*==================*/
					/* out: minimum size */
	const dict_col_t*	col);	/* in: column */
/***************************************************************************
Returns the maximum size of the column. */
UNIV_INLINE
ulint
dict_col_get_max_size(
/*==================*/
					/* out: maximum size */
	const dict_col_t*	col);	/* in: column */
/***************************************************************************
Returns the size of a fixed size column, 0 if not a fixed size column. */
UNIV_INLINE
ulint
dict_col_get_fixed_size(
/*====================*/
					/* out: fixed size, or 0 */
	const dict_col_t*	col);	/* in: column */
/***************************************************************************
Returns the ROW_FORMAT=REDUNDANT stored SQL NULL size of a column.
For fixed length types it is the fixed length of the type, otherwise 0. */
UNIV_INLINE
ulint
dict_col_get_sql_null_size(
/*=======================*/
					/* out: SQL null storage size
					in ROW_FORMAT=REDUNDANT */
	const dict_col_t*	col);	/* in: column */

/*************************************************************************
Gets the column number. */
UNIV_INLINE
ulint
dict_col_get_no(
/*============*/
	const dict_col_t*	col);
/*************************************************************************
Gets the column position in the clustered index. */
UNIV_INLINE
ulint
dict_col_get_clust_pos(
/*===================*/
	const dict_col_t*	col,		/* in: table column */
	const dict_index_t*	clust_index);	/* in: clustered index */
/********************************************************************
If the given column name is reserved for InnoDB system columns, return
TRUE. */
UNIV_INTERN
ibool
dict_col_name_is_reserved(
/*======================*/
				/* out: TRUE if name is reserved */
	const char*	name);	/* in: column name */
/************************************************************************
Acquire the autoinc lock.*/
UNIV_INTERN
void
dict_table_autoinc_lock(
/*====================*/
	dict_table_t*	table);	/* in/out: table */
/************************************************************************
Unconditionally set the autoinc counter. */
UNIV_INTERN
void
dict_table_autoinc_initialize(
/*==========================*/
	dict_table_t*	table,	/* in/out: table */
	ib_uint64_t	value);	/* in: next value to assign to a row */
/************************************************************************
Reads the next autoinc value (== autoinc counter value), 0 if not yet
initialized. */
UNIV_INTERN
ib_uint64_t
dict_table_autoinc_read(
/*====================*/
					/* out: value for a new row, or 0 */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Updates the autoinc counter if the value supplied is greater than the
current value. */
UNIV_INTERN
void
dict_table_autoinc_update_if_greater(
/*=================================*/

	dict_table_t*	table,	/* in/out: table */
	ib_uint64_t	value);	/* in: value which was assigned to a row */
/************************************************************************
Release the autoinc lock.*/
UNIV_INTERN
void
dict_table_autoinc_unlock(
/*======================*/
	dict_table_t*	table);	/* in/out: table */
/**************************************************************************
Adds system columns to a table object. */
UNIV_INTERN
void
dict_table_add_system_columns(
/*==========================*/
	dict_table_t*	table,	/* in/out: table */
	mem_heap_t*	heap);	/* in: temporary heap */
/**************************************************************************
Adds a table object to the dictionary cache. */
UNIV_INTERN
void
dict_table_add_to_cache(
/*====================*/
	dict_table_t*	table,	/* in: table */
	mem_heap_t*	heap);	/* in: temporary heap */
/**************************************************************************
Removes a table object from the dictionary cache. */
UNIV_INTERN
void
dict_table_remove_from_cache(
/*=========================*/
	dict_table_t*	table);	/* in, own: table */
/**************************************************************************
Renames a table object. */
UNIV_INTERN
ibool
dict_table_rename_in_cache(
/*=======================*/
					/* out: TRUE if success */
	dict_table_t*	table,		/* in/out: table */
	const char*	new_name,	/* in: new name */
	ibool		rename_also_foreigns);/* in: in ALTER TABLE we want
					to preserve the original table name
					in constraints which reference it */
/**************************************************************************
Removes an index from the dictionary cache. */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/* in/out: table */
	dict_index_t*	index);	/* in, own: index */
/**************************************************************************
Change the id of a table object in the dictionary cache. This is used in
DISCARD TABLESPACE. */
UNIV_INTERN
void
dict_table_change_id_in_cache(
/*==========================*/
	dict_table_t*	table,	/* in/out: table object already in cache */
	dulint		new_id);/* in: new id to set */
/**************************************************************************
Adds a foreign key constraint object to the dictionary cache. May free
the object if there already is an object with the same identifier in.
At least one of foreign table or referenced table must already be in
the dictionary cache! */
UNIV_INTERN
ulint
dict_foreign_add_to_cache(
/*======================*/
					/* out: DB_SUCCESS or error code */
	dict_foreign_t*	foreign,	/* in, own: foreign key constraint */
	ibool		check_charsets);/* in: TRUE=check charset
					compatibility */
/*************************************************************************
Check if the index is referenced by a foreign key, if TRUE return the
matching instance NULL otherwise. */
UNIV_INTERN
dict_foreign_t*
dict_table_get_referenced_constraint(
/*=================================*/
				/* out: pointer to foreign key struct if index
				is defined for foreign key, otherwise NULL */
	dict_table_t*	table,	/* in: InnoDB table */
	dict_index_t*	index);	/* in: InnoDB index */
/*************************************************************************
Checks if a table is referenced by foreign keys. */
UNIV_INTERN
ibool
dict_table_is_referenced_by_foreign_key(
/*====================================*/
					/* out: TRUE if table is referenced
					by a foreign key */
	const dict_table_t*	table);	/* in: InnoDB table */
/**************************************************************************
Replace the index in the foreign key list that matches this index's
definition with an equivalent index. */
UNIV_INTERN
void
dict_table_replace_index_in_foreign_list(
/*=====================================*/
	dict_table_t*	table,  /* in/out: table */
	dict_index_t*	index);	/* in: index to be replaced */
/*************************************************************************
Checks if a index is defined for a foreign key constraint. Index is a part
of a foreign key constraint if the index is referenced by foreign key
or index is a foreign key index */
UNIV_INTERN
dict_foreign_t*
dict_table_get_foreign_constraint(
/*==============================*/
				/* out: pointer to foreign key struct if index
				is defined for foreign key, otherwise NULL */
	dict_table_t*	table,	/* in: InnoDB table */
	dict_index_t*	index);	/* in: InnoDB index */
/*************************************************************************
Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
bot participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint. */
UNIV_INTERN
ulint
dict_create_foreign_constraints(
/*============================*/
					/* out: error code or DB_SUCCESS */
	trx_t*		trx,		/* in: transaction */
	const char*	sql_string,	/* in: table create statement where
					foreign keys are declared like:
					FOREIGN KEY (a, b) REFERENCES
					table2(c, d), table2 can be written
					also with the database
					name before it: test.table2; the
					default database id the database of
					parameter name */
	const char*	name,		/* in: table full name in the
					normalized form
					database_name/table_name */
	ibool		reject_fks);	/* in: if TRUE, fail with error
					code DB_CANNOT_ADD_CONSTRAINT if
					any foreign keys are found. */
/**************************************************************************
Parses the CONSTRAINT id's to be dropped in an ALTER TABLE statement. */
UNIV_INTERN
ulint
dict_foreign_parse_drop_constraints(
/*================================*/
						/* out: DB_SUCCESS or
						DB_CANNOT_DROP_CONSTRAINT if
						syntax error or the constraint
						id does not match */
	mem_heap_t*	heap,			/* in: heap from which we can
						allocate memory */
	trx_t*		trx,			/* in: transaction */
	dict_table_t*	table,			/* in: table */
	ulint*		n,			/* out: number of constraints
						to drop */
	const char***	constraints_to_drop);	/* out: id's of the
						constraints to drop */
/**************************************************************************
Returns a table object and optionally increment its MySQL open handle count.
NOTE! This is a high-level function to be used mainly from outside the
'dict' directory. Inside this directory dict_table_get_low is usually the
appropriate function. */
UNIV_INTERN
dict_table_t*
dict_table_get(
/*===========*/
					/* out: table, NULL if
					does not exist */
	const char*	table_name,	/* in: table name */
	ibool		inc_mysql_count);
					/* in: whether to increment the open
					handle count on the table */
/**************************************************************************
Returns a index object, based on table and index id, and memoryfixes it. */
UNIV_INTERN
dict_index_t*
dict_index_get_on_id_low(
/*=====================*/
					/* out: index, NULL if does not
					exist */
	dict_table_t*	table,		/* in: table */
	dulint		index_id);	/* in: index id */
/**************************************************************************
Checks if a table is in the dictionary cache. */

UNIV_INLINE
dict_table_t*
dict_table_check_if_in_cache_low(
/*=============================*/
					/* out: table, NULL if not found */
	const char*	table_name);	/* in: table name */
/**************************************************************************
Gets a table; loads it to the dictionary cache if necessary. A low-level
function. */
UNIV_INLINE
dict_table_t*
dict_table_get_low(
/*===============*/
					/* out: table, NULL if not found */
	const char*	table_name);	/* in: table name */
/**************************************************************************
Returns a table object based on table id. */
UNIV_INLINE
dict_table_t*
dict_table_get_on_id_low(
/*=====================*/
				/* out: table, NULL if does not exist */
	dulint	table_id);	/* in: table id */
/**************************************************************************
Find an index that is equivalent to the one passed in and is not marked
for deletion. */
UNIV_INTERN
dict_index_t*
dict_foreign_find_equiv_index(
/*==========================*/
				/* out: index equivalent to
				foreign->foreign_index, or NULL */
	dict_foreign_t*	foreign);/* in: foreign key */
/**************************************************************************
Returns an index object by matching on the name and column names and if
more than index is found return the index with the higher id.*/
UNIV_INTERN
dict_index_t*
dict_table_get_index_by_max_id(
/*===========================*/
				/* out: matching index, NULL if not found */
	dict_table_t*	table,	/* in: table */
	const char*	name,	/* in: the index name to find */
	const char**	columns,/* in: array of column names */
	ulint		n_cols);/* in: number of columns */
/**************************************************************************
Returns a column's name. */

const char*
dict_table_get_col_name(
/*====================*/
					/* out: column name. NOTE: not
					guaranteed to stay valid if table is
					modified in any way (columns added,
					etc.). */
	const dict_table_t*	table,	/* in: table */
	ulint			col_nr);/* in: column number */

/**************************************************************************
Prints a table definition. */
UNIV_INTERN
void
dict_table_print(
/*=============*/
	dict_table_t*	table);	/* in: table */
/**************************************************************************
Prints a table data. */
UNIV_INTERN
void
dict_table_print_low(
/*=================*/
	dict_table_t*	table);	/* in: table */
/**************************************************************************
Prints a table data when we know the table name. */
UNIV_INTERN
void
dict_table_print_by_name(
/*=====================*/
	const char*	name);
/**************************************************************************
Outputs info on foreign keys of a table. */
UNIV_INTERN
void
dict_print_info_on_foreign_keys(
/*============================*/
	ibool		create_table_format, /* in: if TRUE then print in
				a format suitable to be inserted into
				a CREATE TABLE, otherwise in the format
				of SHOW TABLE STATUS */
	FILE*		file,	/* in: file where to print */
	trx_t*		trx,	/* in: transaction */
	dict_table_t*	table);	/* in: table */
/**************************************************************************
Outputs info on a foreign key of a table in a format suitable for
CREATE TABLE. */
UNIV_INTERN
void
dict_print_info_on_foreign_key_in_create_format(
/*============================================*/
	FILE*		file,		/* in: file where to print */
	trx_t*		trx,		/* in: transaction */
	dict_foreign_t*	foreign,	/* in: foreign key constraint */
	ibool		add_newline);	/* in: whether to add a newline */
/************************************************************************
Displays the names of the index and the table. */
UNIV_INTERN
void
dict_index_name_print(
/*==================*/
	FILE*			file,	/* in: output stream */
	trx_t*			trx,	/* in: transaction */
	const dict_index_t*	index);	/* in: index to print */
#ifdef UNIV_DEBUG
/************************************************************************
Gets the first index on the table (the clustered index). */
UNIV_INLINE
dict_index_t*
dict_table_get_first_index(
/*=======================*/
					/* out: index, NULL if none exists */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Gets the next index on the table. */
UNIV_INLINE
dict_index_t*
dict_table_get_next_index(
/*======================*/
					/* out: index, NULL if none left */
	const dict_index_t*	index);	/* in: index */
#else /* UNIV_DEBUG */
# define dict_table_get_first_index(table) UT_LIST_GET_FIRST((table)->indexes)
# define dict_table_get_next_index(index) UT_LIST_GET_NEXT(indexes, index)
#endif /* UNIV_DEBUG */
/************************************************************************
Check whether the index is the clustered index. */
UNIV_INLINE
ulint
dict_index_is_clust(
/*================*/
					/* out: nonzero for clustered index,
					zero for other indexes */
	const dict_index_t*	index)	/* in: index */
	__attribute__((pure));
/************************************************************************
Check whether the index is unique. */
UNIV_INLINE
ulint
dict_index_is_unique(
/*=================*/
					/* out: nonzero for unique index,
					zero for other indexes */
	const dict_index_t*	index)	/* in: index */
	__attribute__((pure));
/************************************************************************
Check whether the index is the insert buffer tree. */
UNIV_INLINE
ulint
dict_index_is_ibuf(
/*===============*/
					/* out: nonzero for insert buffer,
					zero for other indexes */
	const dict_index_t*	index)	/* in: index */
	__attribute__((pure));

/************************************************************************
Gets the number of user-defined columns in a table in the dictionary
cache. */
UNIV_INLINE
ulint
dict_table_get_n_user_cols(
/*=======================*/
					/* out: number of user-defined
					(e.g., not ROW_ID)
					columns of a table */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Gets the number of system columns in a table in the dictionary cache. */
UNIV_INLINE
ulint
dict_table_get_n_sys_cols(
/*======================*/
					/* out: number of system (e.g.,
					ROW_ID) columns of a table */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Gets the number of all columns (also system) in a table in the dictionary
cache. */
UNIV_INLINE
ulint
dict_table_get_n_cols(
/*==================*/
					/* out: number of columns of a table */
	const dict_table_t*	table);	/* in: table */
#ifdef UNIV_DEBUG
/************************************************************************
Gets the nth column of a table. */
UNIV_INLINE
dict_col_t*
dict_table_get_nth_col(
/*===================*/
					/* out: pointer to column object */
	const dict_table_t*	table,	/* in: table */
	ulint			pos);	/* in: position of column */
/************************************************************************
Gets the given system column of a table. */
UNIV_INLINE
dict_col_t*
dict_table_get_sys_col(
/*===================*/
					/* out: pointer to column object */
	const dict_table_t*	table,	/* in: table */
	ulint			sys);	/* in: DATA_ROW_ID, ... */
#else /* UNIV_DEBUG */
#define dict_table_get_nth_col(table, pos) \
((table)->cols + (pos))
#define dict_table_get_sys_col(table, sys) \
((table)->cols + (table)->n_cols + (sys) - DATA_N_SYS_COLS)
#endif /* UNIV_DEBUG */
/************************************************************************
Gets the given system column number of a table. */
UNIV_INLINE
ulint
dict_table_get_sys_col_no(
/*======================*/
					/* out: column number */
	const dict_table_t*	table,	/* in: table */
	ulint			sys);	/* in: DATA_ROW_ID, ... */
/************************************************************************
Returns the minimum data size of an index record. */
UNIV_INLINE
ulint
dict_index_get_min_size(
/*====================*/
					/* out: minimum data size in bytes */
	const dict_index_t*	index);	/* in: index */
/************************************************************************
Check whether the table uses the compact page format. */
UNIV_INLINE
ibool
dict_table_is_comp(
/*===============*/
					/* out: TRUE if table uses the
					compact page format */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Determine the file format of a table. */
UNIV_INLINE
ulint
dict_table_get_format(
/*==================*/
					/* out: file format version */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Set the file format of a table. */
UNIV_INLINE
void
dict_table_set_format(
/*==================*/
	dict_table_t*	table,	/* in/out: table */
	ulint		format);/* in: file format version */
/************************************************************************
Extract the compressed page size from table flags. */
UNIV_INLINE
ulint
dict_table_flags_to_zip_size(
/*=========================*/
			/* out: compressed page size,
			or 0 if not compressed */
	ulint	flags)	/* in: flags */
	__attribute__((const));
/************************************************************************
Check whether the table uses the compressed compact page format. */
UNIV_INLINE
ulint
dict_table_zip_size(
/*================*/
					/* out: compressed page size,
					or 0 if not compressed */
	const dict_table_t*	table);	/* in: table */
/************************************************************************
Checks if a column is in the ordering columns of the clustered index of a
table. Column prefixes are treated like whole columns. */
UNIV_INTERN
ibool
dict_table_col_in_clustered_key(
/*============================*/
					/* out: TRUE if the column, or its
					prefix, is in the clustered key */
	const dict_table_t*	table,	/* in: table */
	ulint			n);	/* in: column number */
/***********************************************************************
Copies types of columns contained in table to tuple and sets all
fields of the tuple to the SQL NULL value.  This function should
be called right after dtuple_create(). */
UNIV_INTERN
void
dict_table_copy_types(
/*==================*/
	dtuple_t*		tuple,	/* in/out: data tuple */
	const dict_table_t*	table);	/* in: table */
/**************************************************************************
Looks for an index with the given id. NOTE that we do not reserve
the dictionary mutex: this function is for emergency purposes like
printing info of a corrupt database page! */
UNIV_INTERN
dict_index_t*
dict_index_find_on_id_low(
/*======================*/
			/* out: index or NULL if not found from cache */
	dulint	id);	/* in: index id */
/**************************************************************************
Adds an index to the dictionary cache. */
UNIV_INTERN
ulint
dict_index_add_to_cache(
/*====================*/
				/* out: DB_SUCCESS or error code */
	dict_table_t*	table,	/* in: table on which the index is */
	dict_index_t*	index,	/* in, own: index; NOTE! The index memory
				object is freed in this function! */
	ulint		page_no,/* in: root page number of the index */
	ibool		strict);/* in: TRUE=refuse to create the index
				if records could be too big to fit in
				an B-tree page */
/**************************************************************************
Removes an index from the dictionary cache. */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/* in/out: table */
	dict_index_t*	index);	/* in, own: index */
/************************************************************************
Gets the number of fields in the internal representation of an index,
including fields added by the dictionary system. */
UNIV_INLINE
ulint
dict_index_get_n_fields(
/*====================*/
					/* out: number of fields */
	const dict_index_t*	index);	/* in: an internal
					representation of index (in
					the dictionary cache) */
/************************************************************************
Gets the number of fields in the internal representation of an index
that uniquely determine the position of an index entry in the index, if
we do not take multiversioning into account: in the B-tree use the value
returned by dict_index_get_n_unique_in_tree. */
UNIV_INLINE
ulint
dict_index_get_n_unique(
/*====================*/
					/* out: number of fields */
	const dict_index_t*	index);	/* in: an internal representation
					of index (in the dictionary cache) */
/************************************************************************
Gets the number of fields in the internal representation of an index
which uniquely determine the position of an index entry in the index, if
we also take multiversioning into account. */
UNIV_INLINE
ulint
dict_index_get_n_unique_in_tree(
/*============================*/
					/* out: number of fields */
	const dict_index_t*	index);	/* in: an internal representation
					of index (in the dictionary cache) */
/************************************************************************
Gets the number of user-defined ordering fields in the index. In the internal
representation we add the row id to the ordering fields to make all indexes
unique, but this function returns the number of fields the user defined
in the index as ordering fields. */
UNIV_INLINE
ulint
dict_index_get_n_ordering_defined_by_user(
/*======================================*/
					/* out: number of fields */
	const dict_index_t*	index);	/* in: an internal representation
					of index (in the dictionary cache) */
#ifdef UNIV_DEBUG
/************************************************************************
Gets the nth field of an index. */
UNIV_INLINE
dict_field_t*
dict_index_get_nth_field(
/*=====================*/
					/* out: pointer to field object */
	const dict_index_t*	index,	/* in: index */
	ulint			pos);	/* in: position of field */
#else /* UNIV_DEBUG */
# define dict_index_get_nth_field(index, pos) ((index)->fields + (pos))
#endif /* UNIV_DEBUG */
/************************************************************************
Gets pointer to the nth column in an index. */
UNIV_INLINE
const dict_col_t*
dict_index_get_nth_col(
/*===================*/
					/* out: column */
	const dict_index_t*	index,	/* in: index */
	ulint			pos);	/* in: position of the field */
/************************************************************************
Gets the column number of the nth field in an index. */
UNIV_INLINE
ulint
dict_index_get_nth_col_no(
/*======================*/
					/* out: column number */
	const dict_index_t*	index,	/* in: index */
	ulint			pos);	/* in: position of the field */
/************************************************************************
Looks for column n in an index. */
UNIV_INTERN
ulint
dict_index_get_nth_col_pos(
/*=======================*/
					/* out: position in internal
					representation of the index;
					if not contained, returns
					ULINT_UNDEFINED */
	const dict_index_t*	index,	/* in: index */
	ulint			n);	/* in: column number */
/************************************************************************
Returns TRUE if the index contains a column or a prefix of that column. */
UNIV_INTERN
ibool
dict_index_contains_col_or_prefix(
/*==============================*/
					/* out: TRUE if contains the column
					or its prefix */
	const dict_index_t*	index,	/* in: index */
	ulint			n);	/* in: column number */
/************************************************************************
Looks for a matching field in an index. The column has to be the same. The
column in index must be complete, or must contain a prefix longer than the
column in index2. That is, we must be able to construct the prefix in index2
from the prefix in index. */
UNIV_INTERN
ulint
dict_index_get_nth_field_pos(
/*=========================*/
					/* out: position in internal
					representation of the index;
					if not contained, returns
					ULINT_UNDEFINED */
	const dict_index_t*	index,	/* in: index from which to search */
	const dict_index_t*	index2,	/* in: index */
	ulint			n);	/* in: field number in index2 */
/************************************************************************
Looks for column n position in the clustered index. */
UNIV_INTERN
ulint
dict_table_get_nth_col_pos(
/*=======================*/
					/* out: position in internal
					representation of
					the clustered index */
	const dict_table_t*	table,	/* in: table */
	ulint			n);	/* in: column number */
/************************************************************************
Returns the position of a system column in an index. */
UNIV_INLINE
ulint
dict_index_get_sys_col_pos(
/*=======================*/
					/* out: position,
					ULINT_UNDEFINED if not contained */
	const dict_index_t*	index,	/* in: index */
	ulint			type);	/* in: DATA_ROW_ID, ... */
/***********************************************************************
Adds a column to index. */
UNIV_INTERN
void
dict_index_add_col(
/*===============*/
	dict_index_t*		index,		/* in/out: index */
	const dict_table_t*	table,		/* in: table */
	dict_col_t*		col,		/* in: column */
	ulint			prefix_len);	/* in: column prefix length */
/***********************************************************************
Copies types of fields contained in index to tuple. */
UNIV_INTERN
void
dict_index_copy_types(
/*==================*/
	dtuple_t*		tuple,		/* in/out: data tuple */
	const dict_index_t*	index,		/* in: index */
	ulint			n_fields);	/* in: number of
						field types to copy */
/*************************************************************************
Gets the field column. */
UNIV_INLINE
const dict_col_t*
dict_field_get_col(
/*===============*/
	const dict_field_t*	field);

/**************************************************************************
Returns an index object if it is found in the dictionary cache.
Assumes that dict_sys->mutex is already being held. */
UNIV_INTERN
dict_index_t*
dict_index_get_if_in_cache_low(
/*===========================*/
				/* out: index, NULL if not found */
	dulint	index_id);	/* in: index id */
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/**************************************************************************
Returns an index object if it is found in the dictionary cache. */
UNIV_INTERN
dict_index_t*
dict_index_get_if_in_cache(
/*=======================*/
				/* out: index, NULL if not found */
	dulint	index_id);	/* in: index id */
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#ifdef UNIV_DEBUG
/**************************************************************************
Checks that a tuple has n_fields_cmp value in a sensible range, so that
no comparison can occur with the page number field in a node pointer. */
UNIV_INTERN
ibool
dict_index_check_search_tuple(
/*==========================*/
					/* out: TRUE if ok */
	const dict_index_t*	index,	/* in: index tree */
	const dtuple_t*		tuple);	/* in: tuple used in a search */
/**************************************************************************
Check for duplicate index entries in a table [using the index name] */
UNIV_INTERN
void
dict_table_check_for_dup_indexes(
/*=============================*/
	const dict_table_t*	table);	/* in: Check for dup indexes
					in this table */

#endif /* UNIV_DEBUG */
/**************************************************************************
Builds a node pointer out of a physical record and a page number. */
UNIV_INTERN
dtuple_t*
dict_index_build_node_ptr(
/*======================*/
					/* out, own: node pointer */
	const dict_index_t*	index,	/* in: index */
	const rec_t*		rec,	/* in: record for which to build node
					pointer */
	ulint			page_no,/* in: page number to put in node
					pointer */
	mem_heap_t*		heap,	/* in: memory heap where pointer
					created */
	ulint			level);	/* in: level of rec in tree:
					0 means leaf level */
/**************************************************************************
Copies an initial segment of a physical record, long enough to specify an
index entry uniquely. */
UNIV_INTERN
rec_t*
dict_index_copy_rec_order_prefix(
/*=============================*/
					/* out: pointer to the prefix record */
	const dict_index_t*	index,	/* in: index */
	const rec_t*		rec,	/* in: record for which to
					copy prefix */
	ulint*			n_fields,/* out: number of fields copied */
	byte**			buf,	/* in/out: memory buffer for the
					copied prefix, or NULL */
	ulint*			buf_size);/* in/out: buffer size */
/**************************************************************************
Builds a typed data tuple out of a physical record. */
UNIV_INTERN
dtuple_t*
dict_index_build_data_tuple(
/*========================*/
				/* out, own: data tuple */
	dict_index_t*	index,	/* in: index */
	rec_t*		rec,	/* in: record for which to build data tuple */
	ulint		n_fields,/* in: number of data fields */
	mem_heap_t*	heap);	/* in: memory heap where tuple created */
/*************************************************************************
Gets the space id of the root of the index tree. */
UNIV_INLINE
ulint
dict_index_get_space(
/*=================*/
					/* out: space id */
	const dict_index_t*	index);	/* in: index */
/*************************************************************************
Sets the space id of the root of the index tree. */
UNIV_INLINE
void
dict_index_set_space(
/*=================*/
	dict_index_t*	index,	/* in/out: index */
	ulint		space);	/* in: space id */
/*************************************************************************
Gets the page number of the root of the index tree. */
UNIV_INLINE
ulint
dict_index_get_page(
/*================*/
					/* out: page number */
	const dict_index_t*	tree);	/* in: index */
/*************************************************************************
Sets the page number of the root of index tree. */
UNIV_INLINE
void
dict_index_set_page(
/*================*/
	dict_index_t*	index,	/* in/out: index */
	ulint		page);	/* in: page number */
/*************************************************************************
Gets the read-write lock of the index tree. */
UNIV_INLINE
rw_lock_t*
dict_index_get_lock(
/*================*/
				/* out: read-write lock */
	dict_index_t*	index);	/* in: index */
/************************************************************************
Returns free space reserved for future updates of records. This is
relevant only in the case of many consecutive inserts, as updates
which make the records bigger might fragment the index. */
UNIV_INLINE
ulint
dict_index_get_space_reserve(void);
/*==============================*/
				/* out: number of free bytes on page,
				reserved for updates */
/*************************************************************************
Calculates the minimum record length in an index. */
UNIV_INTERN
ulint
dict_index_calc_min_rec_len(
/*========================*/
	const dict_index_t*	index);	/* in: index */
/*************************************************************************
Calculates new estimates for table and index statistics. The statistics
are used in query optimization. */
UNIV_INTERN
void
dict_update_statistics_low(
/*=======================*/
	dict_table_t*	table,		/* in/out: table */
	ibool		has_dict_mutex);/* in: TRUE if the caller has the
					dictionary mutex */
/*************************************************************************
Calculates new estimates for table and index statistics. The statistics
are used in query optimization. */
UNIV_INTERN
void
dict_update_statistics(
/*===================*/
	dict_table_t*	table);	/* in/out: table */
/************************************************************************
Reserves the dictionary system mutex for MySQL. */
UNIV_INTERN
void
dict_mutex_enter_for_mysql(void);
/*============================*/
/************************************************************************
Releases the dictionary system mutex for MySQL. */
UNIV_INTERN
void
dict_mutex_exit_for_mysql(void);
/*===========================*/
/************************************************************************
Checks if the database name in two table names is the same. */
UNIV_INTERN
ibool
dict_tables_have_same_db(
/*=====================*/
				/* out: TRUE if same db name */
	const char*	name1,	/* in: table name in the form
				dbname '/' tablename */
	const char*	name2);	/* in: table name in the form
				dbname '/' tablename */
/*************************************************************************
Removes an index from the cache */
UNIV_INTERN
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/* in/out: table */
	dict_index_t*	index);	/* in, own: index */
/**************************************************************************
Get index by name */
UNIV_INTERN
dict_index_t*
dict_table_get_index_on_name(
/*=========================*/
				/* out: index, NULL if does not exist */
	dict_table_t*	table,	/* in: table */
	const char*	name);	/* in: name of the index to find */
/**************************************************************************
In case there is more than one index with the same name return the index
with the min(id). */
UNIV_INTERN
dict_index_t*
dict_table_get_index_on_name_and_min_id(
/*====================================*/
				/* out: index, NULL if does not exist */
	dict_table_t*	table,	/* in: table */
	const char*	name);	/* in: name of the index to find */

UNIV_INTERN
void
dict_table_LRU_trim(
/*================*/
	dict_table_t*	self);
/* Buffers for storing detailed information about the latest foreign key
and unique key errors */
extern FILE*	dict_foreign_err_file;
extern mutex_t	dict_foreign_err_mutex; /* mutex protecting the buffers */

extern dict_sys_t*	dict_sys;	/* the dictionary system */
extern rw_lock_t	dict_operation_lock;

/* Dictionary system struct */
struct dict_sys_struct{
	mutex_t		mutex;		/* mutex protecting the data
					dictionary; protects also the
					disk-based dictionary system tables;
					this mutex serializes CREATE TABLE
					and DROP TABLE, as well as reading
					the dictionary data for a table from
					system tables */
	dulint		row_id;		/* the next row id to assign;
					NOTE that at a checkpoint this
					must be written to the dict system
					header and flushed to a file; in
					recovery this must be derived from
					the log records */
	hash_table_t*	table_hash;	/* hash table of the tables, based
					on name */
	hash_table_t*	table_id_hash;	/* hash table of the tables, based
					on id */
	UT_LIST_BASE_NODE_T(dict_table_t)
			table_LRU;	/* LRU list of tables */
	ulint		size;		/* varying space in bytes occupied
					by the data dictionary table and
					index objects */
	dict_table_t*	sys_tables;	/* SYS_TABLES table */
	dict_table_t*	sys_columns;	/* SYS_COLUMNS table */
	dict_table_t*	sys_indexes;	/* SYS_INDEXES table */
	dict_table_t*	sys_fields;	/* SYS_FIELDS table */
};

#ifndef UNIV_NONINL
#include "dict0dict.ic"
#endif

#endif
