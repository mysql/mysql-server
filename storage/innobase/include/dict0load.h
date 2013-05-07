/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/** Check type for dict_check_tablespaces_and_store_max_id() */
enum dict_check_t {
	/** No user tablespaces have been opened
	(no crash recovery, no transactions recovered). */
	DICT_CHECK_NONE_LOADED = 0,
	/** Some user tablespaces may have been opened
	(no crash recovery; recovered table locks for transactions). */
	DICT_CHECK_SOME_LOADED,
	/** All user tablespaces have been opened (crash recovery). */
	DICT_CHECK_ALL_LOADED
};

/********************************************************************//**
In a crash recovery we already have all the tablespace objects created.
This function compares the space id information in the InnoDB data dictionary
to what we already read with fil_load_single_table_tablespaces().

In a normal startup, we create the tablespace objects for every table in
InnoDB's data dictionary, if the corresponding .ibd file exists.
We also scan the biggest space id, and store it to fil_system. */
UNIV_INTERN
void
dict_check_tablespaces_and_store_max_id(
/*====================================*/
	dict_check_t	dict_check);	/*!< in: how to check */
/********************************************************************//**
Finds the first table name in the given database.
@return own: table name, NULL if does not exist; the caller must free
the memory in the string! */
UNIV_INTERN
char*
dict_get_first_table_name_in_db(
/*============================*/
	const char*	name);	/*!< in: database name which ends to '/' */

/********************************************************************//**
Loads a table definition from a SYS_TABLES record to dict_table_t.
Does not load any columns or indexes.
@return error message, or NULL on success */
UNIV_INTERN
const char*
dict_load_table_low(
/*================*/
	const char*	name,		/*!< in: table name */
	const rec_t*	rec,		/*!< in: SYS_TABLES record */
	dict_table_t**	table);		/*!< out,own: table, or NULL */
/********************************************************************//**
Loads a table column definition from a SYS_COLUMNS record to
dict_table_t.
@return error message, or NULL on success */
UNIV_INTERN
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
	const rec_t*	rec);		/*!< in: SYS_COLUMNS record */
/********************************************************************//**
Loads an index definition from a SYS_INDEXES record to dict_index_t.
If allocate=TRUE, we will create a dict_index_t structure and fill it
accordingly. If allocated=FALSE, the dict_index_t will be supplied by
the caller and filled with information read from the record.  @return
error message, or NULL on success */
UNIV_INTERN
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
UNIV_INTERN
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
UNIV_INTERN
void
dict_save_data_dir_path(
/*====================*/
	dict_table_t*	table,		/*!< in/out: table */
	char*		filepath);	/*!< in: filepath of tablespace */
/*****************************************************************//**
Make sure the data_file_name is saved in dict_table_t if needed. Try to
read it from the file dictionary first, then from SYS_DATAFILES. */
UNIV_INTERN
void
dict_get_and_save_data_dir_path(
/*============================*/
	dict_table_t*	table,		/*!< in/out: table */
	bool		dict_mutex_own);	/*!< in: true if dict_sys->mutex
					is owned already */
/********************************************************************//**
Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table.
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the
ibd_file_missing flag TRUE in the table object we return */
UNIV_INTERN
dict_table_t*
dict_load_table(
/*============*/
	const char*	name,	/*!< in: table name in the
				databasename/tablename format */
	ibool		cached,	/*!< in: TRUE=add to cache, FALSE=do not */
	dict_err_ignore_t ignore_err);
				/*!< in: error to be ignored when loading
				table and its indexes' definition */
/***********************************************************************//**
Loads a table object based on the table id.
@return	table; NULL if table does not exist */
UNIV_INTERN
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
UNIV_INTERN
void
dict_load_sys_table(
/*================*/
	dict_table_t*	table);	/*!< in: system table */
/***********************************************************************//**
Loads foreign key constraints where the table is either the foreign key
holder or where the table is referenced by a foreign key. Adds these
constraints to the data dictionary. Note that we know that the dictionary
cache already contains all constraints where the other relevant table is
already in the dictionary cache.
@return	DB_SUCCESS or error code */
UNIV_INTERN
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
	dict_err_ignore_t	ignore_err)	/*!< in: error to be ignored */
	__attribute__((nonnull(1), warn_unused_result));
/********************************************************************//**
Prints to the standard output information on all tables found in the data
dictionary system table. */
UNIV_INTERN
void
dict_print(void);
/*============*/

/********************************************************************//**
This function opens a system table, and return the first record.
@return	first record of the system table */
UNIV_INTERN
const rec_t*
dict_startscan_system(
/*==================*/
	btr_pcur_t*	pcur,		/*!< out: persistent cursor to
					the record */
	mtr_t*		mtr,		/*!< in: the mini-transaction */
	dict_system_id_t system_id);	/*!< in: which system table to open */
/********************************************************************//**
This function get the next system table record as we scan the table.
@return	the record if found, NULL if end of scan. */
UNIV_INTERN
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
UNIV_INTERN
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
UNIV_INTERN
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
UNIV_INTERN
const char*
dict_process_sys_columns_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_COLUMNS rec */
	dict_col_t*	column,		/*!< out: dict_col_t to be filled */
	table_id_t*	table_id,	/*!< out: table id */
	const char**	col_name);	/*!< out: column name */
/********************************************************************//**
This function parses a SYS_FIELDS record and populate a dict_field_t
structure with the information from the record.
@return error message, or NULL on success */
UNIV_INTERN
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
UNIV_INTERN
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
UNIV_INTERN
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
UNIV_INTERN
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
UNIV_INTERN
const char*
dict_process_sys_datafiles(
/*=======================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_DATAFILES rec */
	ulint*		space,		/*!< out: pace id */
	const char**	path);		/*!< out: datafile path */
/********************************************************************//**
Get the filepath for a spaceid from SYS_DATAFILES. This function provides
a temporary heap which is used for the table lookup, but not for the path.
The caller must free the memory for the path returned. This function can
return NULL if the space ID is not found in SYS_DATAFILES, then the caller
will assume that the ibd file is in the normal datadir.
@return	own: A copy of the first datafile found in SYS_DATAFILES.PATH for
the given space ID. NULL if space ID is zero or not found. */
UNIV_INTERN
char*
dict_get_first_path(
/*================*/
	ulint		space,	/*!< in: space id */
	const char*	name);	/*!< in: tablespace name */
/********************************************************************//**
Update the record for space_id in SYS_TABLESPACES to this filepath.
@return	DB_SUCCESS if OK, dberr_t if the insert failed */
UNIV_INTERN
dberr_t
dict_update_filepath(
/*=================*/
	ulint		space_id,	/*!< in: space id */
	const char*	filepath);	/*!< in: filepath */
/********************************************************************//**
Insert records into SYS_TABLESPACES and SYS_DATAFILES.
@return	DB_SUCCESS if OK, dberr_t if the insert failed */
UNIV_INTERN
dberr_t
dict_insert_tablespace_and_filepath(
/*================================*/
	ulint		space,		/*!< in: space id */
	const char*	name,		/*!< in: talespace name */
	const char*	filepath,	/*!< in: filepath */
	ulint		fsp_flags);	/*!< in: tablespace flags */

#ifndef UNIV_NONINL
#include "dict0load.ic"
#endif

#endif
