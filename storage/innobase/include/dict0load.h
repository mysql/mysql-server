/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dict0load.h
Loads to the memory cache database object definitions
from dictionary tables

Created 4/24/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0load_h
#define dict0load_h

#include "univ.i"
#include "dict0types.h"
#include "trx0types.h"
#include "ut0byte.h"
#include "mem0mem.h"
#include "btr0types.h"
#include "ut0new.h"

#include <deque>

/** A stack of table names related through foreign key constraints */
typedef std::deque<const char*, ut_allocator<const char*> >	dict_names_t;

/** enum that defines all system table IDs. @see SYSTEM_TABLE_NAME[] */
enum dict_system_id_t {
	SYS_TABLES = 0,
	SYS_INDEXES,
	SYS_COLUMNS,
	SYS_FIELDS,
	SYS_FOREIGN,
	SYS_FOREIGN_COLS,
	SYS_TABLESPACES,
	SYS_DATAFILES,
	SYS_VIRTUAL,

	/* This must be last item. Defines the number of system tables. */
	SYS_NUM_SYSTEM_TABLES
};

/** Status bit for dict_process_sys_tables_rec_and_mtr_commit() */
enum dict_table_info_t {
	DICT_TABLE_LOAD_FROM_RECORD = 0,/*!< Directly populate a dict_table_t
					structure with information from
					a SYS_TABLES record */
	DICT_TABLE_LOAD_FROM_CACHE = 1	/*!< Check first whether dict_table_t
					is in the cache, if so, return it */
};

/** Check each tablespace found in the data dictionary.
Look at each table defined in SYS_TABLES that has a space_id > 0.
If the tablespace is not yet in the fil_system cache, look up the
tablespace in SYS_DATAFILES to ensure the correct path.

In a crash recovery we already have some tablespace objects created from
processing the REDO log.  Any other tablespace in SYS_TABLESPACES not
previously used in recovery will be opened here.  We will compare the
space_id information in the data dictionary to what we find in the
tablespace file. In addition, more validation will be done if recovery
was needed and force_recovery is not set.

We also scan the biggest space id, and store it to fil_system.
@param[in]	validate	true if recovery was needed */
void
dict_check_tablespaces_and_store_max_id(
	bool		validate);

/********************************************************************//**
Finds the first table name in the given database.
@return own: table name, NULL if does not exist; the caller must free
the memory in the string! */
char*
dict_get_first_table_name_in_db(
/*============================*/
	const char*	name);	/*!< in: database name which ends to '/' */

/********************************************************************//**
Loads a table column definition from a SYS_COLUMNS record to
dict_table_t.
@return error message, or NULL on success */
const char*
dict_load_column_low(
/*=================*/
	dict_table_t*	table,		/*!< in/out: table, could be NULL
					if we just populate a dict_column_t
					struct with information from
					a SYS_COLUMNS record */
	mem_heap_t*	heap,		/*!< in/out: memory heap
					for temporary storage */
	dict_col_t*	column,		/*!< out: dict_column_t to fill,
					or NULL if table != NULL */
	table_id_t*	table_id,	/*!< out: table id */
	const char**	col_name,	/*!< out: column name */
	const rec_t*	rec,		/*!< in: SYS_COLUMNS record */
	ulint*		nth_v_col);	/*!< out: if not NULL, this
					records the "n" of "nth" virtual
					column */

/** Loads a virtual column "mapping" (to base columns) information
from a SYS_VIRTUAL record
@param[in,out]	table		table
@param[in,out]	heap		memory heap
@param[in,out]	column		mapped base column's dict_column_t
@param[in,out]	table_id	table id
@param[in,out]	pos		virtual column position
@param[in,out]	base_pos	base column position
@param[in]	rec		SYS_VIRTUAL record
@return error message, or NULL on success */
const char*
dict_load_virtual_low(
	dict_table_t*   table,
	mem_heap_t*     heap,
	dict_col_t**    column,
	table_id_t*     table_id,
	ulint*		pos,
	ulint*		base_pos,
	const rec_t*    rec);
/********************************************************************//**
Loads an index definition from a SYS_INDEXES record to dict_index_t.
If allocate=TRUE, we will create a dict_index_t structure and fill it
accordingly. If allocated=FALSE, the dict_index_t will be supplied by
the caller and filled with information read from the record.  @return
error message, or NULL on success */
const char*
dict_load_index_low(
/*================*/
	byte*		table_id,	/*!< in/out: table id (8 bytes),
					an "in" value if allocate=TRUE
					and "out" when allocate=FALSE */
	const char*	table_name,	/*!< in: table name */
	mem_heap_t*	heap,		/*!< in/out: temporary memory heap */
	const rec_t*	rec,		/*!< in: SYS_INDEXES record */
	ibool		allocate,	/*!< in: TRUE=allocate *index,
					FALSE=fill in a pre-allocated
					*index */
	dict_index_t**	index);		/*!< out,own: index, or NULL */
/********************************************************************//**
Loads an index field definition from a SYS_FIELDS record to
dict_index_t.
@return error message, or NULL on success */
const char*
dict_load_field_low(
/*================*/
	byte*		index_id,	/*!< in/out: index id (8 bytes)
					an "in" value if index != NULL
					and "out" if index == NULL */
	dict_index_t*	index,		/*!< in/out: index, could be NULL
					if we just populate a dict_field_t
					struct with information from
					a SYS_FIELDS record */
	dict_field_t*	sys_field,	/*!< out: dict_field_t to be
					filled */
	ulint*		pos,		/*!< out: Field position */
	byte*		last_index_id,	/*!< in: last index id */
	mem_heap_t*	heap,		/*!< in/out: memory heap
					for temporary storage */
	const rec_t*	rec);		/*!< in: SYS_FIELDS record */
/********************************************************************//**
Using the table->heap, copy the null-terminated filepath into
table->data_dir_path and put a null byte before the extension.
This allows SHOW CREATE TABLE to return the correct DATA DIRECTORY path.
Make this data directory path only if it has not yet been saved. */
void
dict_save_data_dir_path(
/*====================*/
	dict_table_t*	table,		/*!< in/out: table */
	char*		filepath);	/*!< in: filepath of tablespace */

/** Get the first filepath from SYS_DATAFILES for a given space_id.
@param[in]	space_id	Tablespace ID
@return First filepath (caller must invoke ut_free() on it)
@retval NULL if no SYS_DATAFILES entry was found. */
char*
dict_get_first_path(
	ulint	space_id);

/** Make sure the data_file_name is saved in dict_table_t if needed.
Try to read it from the fil_system first, then from SYS_DATAFILES.
@param[in]	table		Table object
@param[in]	dict_mutex_own	true if dict_sys->mutex is owned already */
void
dict_get_and_save_data_dir_path(
	dict_table_t*	table,
	bool		dict_mutex_own);

/** Make sure the tablespace name is saved in dict_table_t if needed.
Try to read it from the file dictionary first, then from SYS_TABLESPACES.
@param[in]	table		Table object
@param[in]	dict_mutex_own)	true if dict_sys->mutex is owned already */
void
dict_get_and_save_space_name(
	dict_table_t*	table,
	bool		dict_mutex_own);

/** Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table.
@param[in]	name		Table name in the dbname/tablename format
@param[in]	cached		true=add to cache, false=do not
@param[in]	ignore_err	Error to be ignored when loading
				table and its index definition
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the ibd_file_missing
flag in the table object we return. */
dict_table_t*
dict_load_table(
	const char*	name,
	bool		cached,
	dict_err_ignore_t ignore_err);

/***********************************************************************//**
Loads a table object based on the table id.
@return table; NULL if table does not exist */
dict_table_t*
dict_load_table_on_id(
/*==================*/
	table_id_t		table_id,	/*!< in: table id */
	dict_err_ignore_t	ignore_err);	/*!< in: errors to ignore
						when loading the table */
/********************************************************************//**
This function is called when the database is booted.
Loads system table index definitions except for the clustered index which
is added to the dictionary cache at booting before calling this function. */
void
dict_load_sys_table(
/*================*/
	dict_table_t*	table);	/*!< in: system table */
/***********************************************************************//**
Loads foreign key constraints where the table is either the foreign key
holder or where the table is referenced by a foreign key. Adds these
constraints to the data dictionary.

The foreign key constraint is loaded only if the referenced table is also
in the dictionary cache.  If the referenced table is not in dictionary
cache, then it is added to the output parameter (fk_tables).

@return DB_SUCCESS or error code */
dberr_t
dict_load_foreigns(
/*===============*/
	const char*		table_name,	/*!< in: table name */
	const char**		col_names,	/*!< in: column names, or NULL
						to use table->col_names */
	bool			check_recursive,/*!< in: Whether to check
						recursive load of tables
						chained by FK */
	bool			check_charsets,	/*!< in: whether to check
						charset compatibility */
	dict_err_ignore_t	ignore_err,	/*!< in: error to be ignored */
	dict_names_t&		fk_tables)	/*!< out: stack of table names
						which must be loaded
						subsequently to load all the
						foreign key constraints. */
	MY_ATTRIBUTE((nonnull(1), warn_unused_result));

/********************************************************************//**
This function opens a system table, and return the first record.
@return first record of the system table */
const rec_t*
dict_startscan_system(
/*==================*/
	btr_pcur_t*	pcur,		/*!< out: persistent cursor to
					the record */
	mtr_t*		mtr,		/*!< in: the mini-transaction */
	dict_system_id_t system_id);	/*!< in: which system table to open */
/********************************************************************//**
This function get the next system table record as we scan the table.
@return the record if found, NULL if end of scan. */
const rec_t*
dict_getnext_system(
/*================*/
	btr_pcur_t*	pcur,		/*!< in/out: persistent cursor
					to the record */
	mtr_t*		mtr);		/*!< in: the mini-transaction */
/********************************************************************//**
This function processes one SYS_TABLES record and populate the dict_table_t
struct for the table. Extracted out of dict_print() to be used by
both monitor table output and information schema innodb_sys_tables output.
@return error message, or NULL on success */
const char*
dict_process_sys_tables_rec_and_mtr_commit(
/*=======================================*/
	mem_heap_t*	heap,		/*!< in: temporary memory heap */
	const rec_t*	rec,		/*!< in: SYS_TABLES record */
	dict_table_t**	table,		/*!< out: dict_table_t to fill */
	dict_table_info_t status,	/*!< in: status bit controls
					options such as whether we shall
					look for dict_table_t from cache
					first */
	mtr_t*		mtr);		/*!< in/out: mini-transaction,
					will be committed */
/********************************************************************//**
This function parses a SYS_INDEXES record and populate a dict_index_t
structure with the information from the record. For detail information
about SYS_INDEXES fields, please refer to dict_boot() function.
@return error message, or NULL on success */
const char*
dict_process_sys_indexes_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_INDEXES rec */
	dict_index_t*	index,		/*!< out: dict_index_t to be
					filled */
	table_id_t*	table_id);	/*!< out: table id */
/********************************************************************//**
This function parses a SYS_COLUMNS record and populate a dict_column_t
structure with the information from the record.
@return error message, or NULL on success */
const char*
dict_process_sys_columns_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_COLUMNS rec */
	dict_col_t*	column,		/*!< out: dict_col_t to be filled */
	table_id_t*	table_id,	/*!< out: table id */
	const char**	col_name,	/*!< out: column name */
	ulint*		nth_v_col);	/*!< out: if virtual col, this is
					records its sequence number */

/** This function parses a SYS_VIRTUAL record and extract virtual column
information
@param[in,out]	heap		heap memory
@param[in]	rec		current SYS_COLUMNS rec
@param[in,out]	table_id	table id
@param[in,out]	pos		virtual column position
@param[in,out]	base_pos	base column position
@return error message, or NULL on success */
const char*
dict_process_sys_virtual_rec(
	mem_heap_t*	heap,
	const rec_t*	rec,
	table_id_t*	table_id,
	ulint*		pos,
	ulint*		base_pos);
/********************************************************************//**
This function parses a SYS_FIELDS record and populate a dict_field_t
structure with the information from the record.
@return error message, or NULL on success */
const char*
dict_process_sys_fields_rec(
/*========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FIELDS rec */
	dict_field_t*	sys_field,	/*!< out: dict_field_t to be
					filled */
	ulint*		pos,		/*!< out: Field position */
	index_id_t*	index_id,	/*!< out: current index id */
	index_id_t	last_id);	/*!< in: previous index id */
/********************************************************************//**
This function parses a SYS_FOREIGN record and populate a dict_foreign_t
structure with the information from the record. For detail information
about SYS_FOREIGN fields, please refer to dict_load_foreign() function
@return error message, or NULL on success */
const char*
dict_process_sys_foreign_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FOREIGN rec */
	dict_foreign_t*	foreign);	/*!< out: dict_foreign_t to be
					filled */
/********************************************************************//**
This function parses a SYS_FOREIGN_COLS record and extract necessary
information from the record and return to caller.
@return error message, or NULL on success */
const char*
dict_process_sys_foreign_col_rec(
/*=============================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FOREIGN_COLS rec */
	const char**	name,		/*!< out: foreign key constraint name */
	const char**	for_col_name,	/*!< out: referencing column name */
	const char**	ref_col_name,	/*!< out: referenced column name
					in referenced table */
	ulint*		pos);		/*!< out: column position */
/********************************************************************//**
This function parses a SYS_TABLESPACES record, extracts necessary
information from the record and returns to caller.
@return error message, or NULL on success */
const char*
dict_process_sys_tablespaces(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_TABLESPACES rec */
	ulint*		space,		/*!< out: pace id */
	const char**	name,		/*!< out: tablespace name */
	ulint*		flags);		/*!< out: tablespace flags */
/********************************************************************//**
This function parses a SYS_DATAFILES record, extracts necessary
information from the record and returns to caller.
@return error message, or NULL on success */
const char*
dict_process_sys_datafiles(
/*=======================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_DATAFILES rec */
	ulint*		space,		/*!< out: pace id */
	const char**	path);		/*!< out: datafile path */

/** Update the record for space_id in SYS_TABLESPACES to this filepath.
@param[in]	space_id	Tablespace ID
@param[in]	filepath	Tablespace filepath
@return DB_SUCCESS if OK, dberr_t if the insert failed */
dberr_t
dict_update_filepath(
	ulint		space_id,
	const char*	filepath);

/** Replace records in SYS_TABLESPACES and SYS_DATAFILES associated with
the given space_id using an independent transaction.
@param[in]	space_id	Tablespace ID
@param[in]	name		Tablespace name
@param[in]	filepath	First filepath
@param[in]	fsp_flags	Tablespace flags
@return DB_SUCCESS if OK, dberr_t if the insert failed */
dberr_t
dict_replace_tablespace_and_filepath(
	ulint		space_id,
	const char*	name,
	const char*	filepath,
	ulint		fsp_flags);

#ifndef UNIV_NONINL
#include "dict0load.ic"
#endif

#endif
