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
@file dict/dict0load.cc
Loads to the memory cache database object definitions
from dictionary tables

Created 4/24/1996 Heikki Tuuri
*******************************************************/

#include "ha_prototypes.h"

#include "dict0load.h"
#ifdef UNIV_NONINL
#include "dict0load.ic"
#endif

#include "mysql_version.h"
#include "btr0pcur.h"
#include "btr0btr.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "fsp0file.h"
#include "fsp0sysspace.h"
#include "fts0priv.h"
#include "mach0data.h"
#include "page0page.h"
#include "rem0cmp.h"
#include "srv0start.h"
#include "srv0srv.h"
#include <stack>
#include <set>

/** Following are the InnoDB system tables. The positions in
this array are referenced by enum dict_system_table_id. */
static const char* SYSTEM_TABLE_NAME[] = {
	"SYS_TABLES",
	"SYS_INDEXES",
	"SYS_COLUMNS",
	"SYS_FIELDS",
	"SYS_FOREIGN",
	"SYS_FOREIGN_COLS",
	"SYS_TABLESPACES",
	"SYS_DATAFILES",
	"SYS_VIRTUAL"
};

/** Loads a table definition and also all its index definitions.

Loads those foreign key constraints whose referenced table is already in
dictionary cache.  If a foreign key constraint is not loaded, then the
referenced table is pushed into the output stack (fk_tables), if it is not
NULL.  These tables must be subsequently loaded so that all the foreign
key constraints are loaded into memory.

@param[in]	name		Table name in the db/tablename format
@param[in]	cached		true=add to cache, false=do not
@param[in]	ignore_err	Error to be ignored when loading table
				and its index definition
@param[out]	fk_tables	Related table names that must also be
				loaded to ensure that all foreign key
				constraints are loaded.
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the
ibd_file_missing flag TRUE in the table object we return */
static
dict_table_t*
dict_load_table_one(
	table_name_t&		name,
	bool			cached,
	dict_err_ignore_t	ignore_err,
	dict_names_t&		fk_tables);

/** Loads a table definition from a SYS_TABLES record to dict_table_t.
Does not load any columns or indexes.
@param[in]	name	Table name
@param[in]	rec	SYS_TABLES record
@param[out,own]	table	Table, or NULL
@return error message, or NULL on success */
static
const char*
dict_load_table_low(
	table_name_t&	name,
	const rec_t*	rec,
	dict_table_t**	table);

/* If this flag is TRUE, then we will load the cluster index's (and tables')
metadata even if it is marked as "corrupted". */
my_bool     srv_load_corrupted = FALSE;

#ifdef UNIV_DEBUG
/****************************************************************//**
Compare the name of an index column.
@return TRUE if the i'th column of index is 'name'. */
static
ibool
name_of_col_is(
/*===========*/
	const dict_table_t*	table,	/*!< in: table */
	const dict_index_t*	index,	/*!< in: index */
	ulint			i,	/*!< in: index field offset */
	const char*		name)	/*!< in: name to compare to */
{
	ulint	tmp = dict_col_get_no(dict_field_get_col(
					      dict_index_get_nth_field(
						      index, i)));

	return(strcmp(name, dict_table_get_col_name(table, tmp)) == 0);
}
#endif /* UNIV_DEBUG */

/********************************************************************//**
Finds the first table name in the given database.
@return own: table name, NULL if does not exist; the caller must free
the memory in the string! */
char*
dict_get_first_table_name_in_db(
/*============================*/
	const char*	name)	/*!< in: database name which ends in '/' */
{
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	mtr_t		mtr;

	ut_ad(mutex_own(&dict_sys->mutex));

	heap = mem_heap_create(1000);

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_ad(!dict_table_is_comp(sys_tables));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name, ut_strlen(name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
loop:
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);

	if (len < strlen(name)
	    || ut_memcmp(name, field, strlen(name)) != 0) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	if (!rec_get_deleted_flag(rec, 0)) {

		/* We found one */

		char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(table_name);
	}

	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	goto loop;
}

/********************************************************************//**
This function gets the next system table record as it scans the table.
@return the next record if found, NULL if end of scan */
static
const rec_t*
dict_getnext_system_low(
/*====================*/
	btr_pcur_t*	pcur,		/*!< in/out: persistent cursor to the
					record*/
	mtr_t*		mtr)		/*!< in: the mini-transaction */
{
	rec_t*	rec = NULL;

	while (!rec || rec_get_deleted_flag(rec, 0)) {
		btr_pcur_move_to_next_user_rec(pcur, mtr);

		rec = btr_pcur_get_rec(pcur);

		if (!btr_pcur_is_on_user_rec(pcur)) {
			/* end of index */
			btr_pcur_close(pcur);

			return(NULL);
		}
	}

	/* Get a record, let's save the position */
	btr_pcur_store_position(pcur, mtr);

	return(rec);
}

/********************************************************************//**
This function opens a system table, and returns the first record.
@return first record of the system table */
const rec_t*
dict_startscan_system(
/*==================*/
	btr_pcur_t*	pcur,		/*!< out: persistent cursor to
					the record */
	mtr_t*		mtr,		/*!< in: the mini-transaction */
	dict_system_id_t system_id)	/*!< in: which system table to open */
{
	dict_table_t*	system_table;
	dict_index_t*	clust_index;
	const rec_t*	rec;

	ut_a(system_id < SYS_NUM_SYSTEM_TABLES);

	system_table = dict_table_get_low(SYSTEM_TABLE_NAME[system_id]);

	clust_index = UT_LIST_GET_FIRST(system_table->indexes);

	btr_pcur_open_at_index_side(true, clust_index, BTR_SEARCH_LEAF, pcur,
				    true, 0, mtr);

	rec = dict_getnext_system_low(pcur, mtr);

	return(rec);
}

/********************************************************************//**
This function gets the next system table record as it scans the table.
@return the next record if found, NULL if end of scan */
const rec_t*
dict_getnext_system(
/*================*/
	btr_pcur_t*	pcur,		/*!< in/out: persistent cursor
					to the record */
	mtr_t*		mtr)		/*!< in: the mini-transaction */
{
	const rec_t*	rec;

	/* Restore the position */
	btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

	/* Get the next record */
	rec = dict_getnext_system_low(pcur, mtr);

	return(rec);
}

/********************************************************************//**
This function processes one SYS_TABLES record and populate the dict_table_t
struct for the table. Extracted out of dict_print() to be used by
both monitor table output and information schema innodb_sys_tables output.
@return error message, or NULL on success */
const char*
dict_process_sys_tables_rec_and_mtr_commit(
/*=======================================*/
	mem_heap_t*	heap,		/*!< in/out: temporary memory heap */
	const rec_t*	rec,		/*!< in: SYS_TABLES record */
	dict_table_t**	table,		/*!< out: dict_table_t to fill */
	dict_table_info_t status,	/*!< in: status bit controls
					options such as whether we shall
					look for dict_table_t from cache
					first */
	mtr_t*		mtr)		/*!< in/out: mini-transaction,
					will be committed */
{
	ulint		len;
	const char*	field;
	const char*	err_msg = NULL;
	table_name_t	table_name;

	field = (const char*) rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);

	ut_a(!rec_get_deleted_flag(rec, 0));

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));

	/* Get the table name */
	table_name.m_name = mem_heap_strdupl(heap, field, len);

	/* If DICT_TABLE_LOAD_FROM_CACHE is set, first check
	whether there is cached dict_table_t struct */
	if (status & DICT_TABLE_LOAD_FROM_CACHE) {

		/* Commit before load the table again */
		mtr_commit(mtr);

		*table = dict_table_get_low(table_name.m_name);

		if (!(*table)) {
			err_msg = "Table not found in cache";
		}
	} else {
		err_msg = dict_load_table_low(table_name, rec, table);
		mtr_commit(mtr);
	}

	if (err_msg) {
		return(err_msg);
	}

	return(NULL);
}

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
	dict_index_t*	index,		/*!< out: index to be filled */
	table_id_t*	table_id)	/*!< out: index table id */
{
	const char*	err_msg;
	byte*		buf;

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));

	/* Parse the record, and get "dict_index_t" struct filled */
	err_msg = dict_load_index_low(buf, NULL,
				      heap, rec, FALSE, &index);

	*table_id = mach_read_from_8(buf);

	return(err_msg);
}

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
	ulint*		nth_v_col)	/*!< out: if virtual col, this is
					record's sequence number */
{
	const char*	err_msg;

	/* Parse the record, and get "dict_col_t" struct filled */
	err_msg = dict_load_column_low(NULL, heap, column,
				       table_id, col_name, rec, nth_v_col);

	return(err_msg);
}

/** This function parses a SYS_VIRTUAL record and extracts virtual column
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
	ulint*		base_pos)
{
	const char*	err_msg;

	/* Parse the record, and get "dict_col_t" struct filled */
	err_msg = dict_load_virtual_low(NULL, heap, NULL, table_id,
					pos, base_pos, rec);

	return(err_msg);
}
/********************************************************************//**
This function parses a SYS_FIELDS record and populates a dict_field_t
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
	index_id_t	last_id)	/*!< in: previous index id */
{
	byte*		buf;
	byte*		last_index_id;
	const char*	err_msg;

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));

	last_index_id = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(last_index_id, last_id);

	err_msg = dict_load_field_low(buf, NULL, sys_field,
				      pos, last_index_id, heap, rec);

	*index_id = mach_read_from_8(buf);

	return(err_msg);

}

/********************************************************************//**
This function parses a SYS_FOREIGN record and populate a dict_foreign_t
structure with the information from the record. For detail information
about SYS_FOREIGN fields, please refer to dict_load_foreign() function.
@return error message, or NULL on success */
const char*
dict_process_sys_foreign_rec(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_FOREIGN rec */
	dict_foreign_t*	foreign)	/*!< out: dict_foreign_t struct
					to be filled */
{
	ulint		len;
	const byte*	field;
	ulint		n_fields_and_type;

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_FOREIGN");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_FOREIGN) {
		return("wrong number of columns in SYS_FOREIGN record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__ID, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
err_len:
		return("incorrect column length in SYS_FOREIGN");
	}

	/* This recieves a dict_foreign_t* that points to a stack variable.
	So mem_heap_free(foreign->heap) is not used as elsewhere.
	Since the heap used here is freed elsewhere, foreign->heap
	is not assigned. */
	foreign->id = mem_heap_strdupl(heap, (const char*) field, len);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	/* The _lookup versions of the referenced and foreign table names
	 are not assigned since they are not used in this dict_foreign_t */

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__FOR_NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	foreign->foreign_table_name = mem_heap_strdupl(
		heap, (const char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__REF_NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	foreign->referenced_table_name = mem_heap_strdupl(
		heap, (const char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__N_COLS, &len);
	if (len != 4) {
		goto err_len;
	}
	n_fields_and_type = mach_read_from_4(field);

	foreign->type = (unsigned int) (n_fields_and_type >> 24);
	foreign->n_fields = (unsigned int) (n_fields_and_type & 0x3FFUL);

	return(NULL);
}

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
	ulint*		pos)		/*!< out: column position */
{
	ulint		len;
	const byte*	field;

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_FOREIGN_COLS");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_FOREIGN_COLS) {
		return("wrong number of columns in SYS_FOREIGN_COLS record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__ID, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
err_len:
		return("incorrect column length in SYS_FOREIGN_COLS");
	}
	*name = mem_heap_strdupl(heap, (char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__POS, &len);
	if (len != 4) {
		goto err_len;
	}
	*pos = mach_read_from_4(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	*for_col_name = mem_heap_strdupl(heap, (char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	*ref_col_name = mem_heap_strdupl(heap, (char*) field, len);

	return(NULL);
}

/********************************************************************//**
This function parses a SYS_TABLESPACES record, extracts necessary
information from the record and returns to caller.
@return error message, or NULL on success */
const char*
dict_process_sys_tablespaces(
/*=========================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_TABLESPACES rec */
	ulint*		space,		/*!< out: space id */
	const char**	name,		/*!< out: tablespace name */
	ulint*		flags)		/*!< out: tablespace flags */
{
	ulint		len;
	const byte*	field;

	/* Initialize the output values */
	*space = ULINT_UNDEFINED;
	*name = NULL;
	*flags = ULINT_UNDEFINED;

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_TABLESPACES");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_TABLESPACES) {
		return("wrong number of columns in SYS_TABLESPACES record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLESPACES__SPACE, &len);
	if (len != DICT_FLD_LEN_SPACE) {
err_len:
		return("incorrect column length in SYS_TABLESPACES");
	}
	*space = mach_read_from_4(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLESPACES__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLESPACES__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLESPACES__NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	*name = mem_heap_strdupl(heap, (char*) field, len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLESPACES__FLAGS, &len);
	if (len != DICT_FLD_LEN_FLAGS) {
		goto err_len;
	}
	*flags = mach_read_from_4(field);

	return(NULL);
}

/********************************************************************//**
This function parses a SYS_DATAFILES record, extracts necessary
information from the record and returns it to the caller.
@return error message, or NULL on success */
const char*
dict_process_sys_datafiles(
/*=======================*/
	mem_heap_t*	heap,		/*!< in/out: heap memory */
	const rec_t*	rec,		/*!< in: current SYS_DATAFILES rec */
	ulint*		space,		/*!< out: space id */
	const char**	path)		/*!< out: datafile paths */
{
	ulint		len;
	const byte*	field;

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_DATAFILES");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_DATAFILES) {
		return("wrong number of columns in SYS_DATAFILES record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_DATAFILES__SPACE, &len);
	if (len != DICT_FLD_LEN_SPACE) {
err_len:
		return("incorrect column length in SYS_DATAFILES");
	}
	*space = mach_read_from_4(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_DATAFILES__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_DATAFILES__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_DATAFILES__PATH, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}
	*path = mem_heap_strdupl(heap, (char*) field, len);

	return(NULL);
}

/** Get the first filepath from SYS_DATAFILES for a given space_id.
@param[in]	space_id	Tablespace ID
@return First filepath (caller must invoke ut_free() on it)
@retval NULL if no SYS_DATAFILES entry was found. */
char*
dict_get_first_path(
	ulint	space_id)
{
	mtr_t		mtr;
	dict_table_t*	sys_datafiles;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	byte*		buf;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	char*		filepath = NULL;
	mem_heap_t*	heap = mem_heap_create(1024);

	ut_ad(mutex_own(&dict_sys->mutex));

	mtr_start(&mtr);

	sys_datafiles = dict_table_get_low("SYS_DATAFILES");
	sys_index = UT_LIST_GET_FIRST(sys_datafiles->indexes);

	ut_ad(!dict_table_is_comp(sys_datafiles));
	ut_ad(name_of_col_is(sys_datafiles, sys_index,
			     DICT_FLD__SYS_DATAFILES__SPACE, "SPACE"));
	ut_ad(name_of_col_is(sys_datafiles, sys_index,
			     DICT_FLD__SYS_DATAFILES__PATH, "PATH"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, DICT_FLD__SYS_DATAFILES__SPACE);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(buf, space_id);

	dfield_set_data(dfield, buf, 4);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	/* Get the filepath from this SYS_DATAFILES record. */
	if (btr_pcur_is_on_user_rec(&pcur)) {
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_DATAFILES__SPACE, &len);
		ut_a(len == 4);

		if (space_id == mach_read_from_4(field)) {
			/* A record for this space ID was found. */
			field = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_DATAFILES__PATH, &len);

			ut_ad(len > 0);
			ut_ad(len < OS_FILE_MAX_PATH);

			if (len > 0 && len != UNIV_SQL_NULL) {
				filepath = mem_strdupl(
					reinterpret_cast<const char*>(field),
					len);
				ut_ad(filepath != NULL);

				/* The dictionary may have been written on
				another OS. */
				os_normalize_path(filepath);
			}
		}
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(filepath);
}

/** Gets the space name from SYS_TABLESPACES for a given space ID.
@param[in]	space_id	Tablespace ID
@param[in]	callers_heap	A heap to allocate from, may be NULL
@return Tablespace name (caller is responsible to free it)
@retval NULL if no dictionary entry was found. */
static
char*
dict_space_get_name(
	ulint		space_id,
	mem_heap_t*	callers_heap)
{
	mtr_t		mtr;
	dict_table_t*	sys_tablespaces;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	byte*		buf;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	char*		space_name = NULL;
	mem_heap_t*	heap = mem_heap_create(1024);

	ut_ad(mutex_own(&dict_sys->mutex));

	sys_tablespaces = dict_table_get_low("SYS_TABLESPACES");
	if (sys_tablespaces == NULL) {
		ut_a(!srv_sys_tablespaces_open);
		return(NULL);
	}

	sys_index = UT_LIST_GET_FIRST(sys_tablespaces->indexes);

	ut_ad(!dict_table_is_comp(sys_tablespaces));
	ut_ad(name_of_col_is(sys_tablespaces, sys_index,
			     DICT_FLD__SYS_TABLESPACES__SPACE, "SPACE"));
	ut_ad(name_of_col_is(sys_tablespaces, sys_index,
			     DICT_FLD__SYS_TABLESPACES__NAME, "NAME"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, DICT_FLD__SYS_TABLESPACES__SPACE);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(buf, space_id);

	dfield_set_data(dfield, buf, 4);
	dict_index_copy_types(tuple, sys_index, 1);

	mtr_start(&mtr);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	/* Get the tablespace name from this SYS_TABLESPACES record. */
	if (btr_pcur_is_on_user_rec(&pcur)) {
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLESPACES__SPACE, &len);
		ut_a(len == 4);

		if (space_id == mach_read_from_4(field)) {
			/* A record for this space ID was found. */
			field = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_TABLESPACES__NAME, &len);

			ut_ad(len > 0);
			ut_ad(len < OS_FILE_MAX_PATH);

			if (len > 0 && len != UNIV_SQL_NULL) {
				/* Found a tablespace name. */
				if (callers_heap == NULL) {
					space_name = mem_strdupl(
						reinterpret_cast<
							const char*>(field),
						len);
				} else {
					space_name = mem_heap_strdupl(
						callers_heap,
						reinterpret_cast<
							const char*>(field),
						len);
				}
				ut_ad(space_name);
			}
		}
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(space_name);
}

/** Update the record for space_id in SYS_TABLESPACES to this filepath.
@param[in]	space_id	Tablespace ID
@param[in]	filepath	Tablespace filepath
@return DB_SUCCESS if OK, dberr_t if the insert failed */
dberr_t
dict_update_filepath(
	ulint		space_id,
	const char*	filepath)
{
	if (!srv_sys_tablespaces_open) {
		/* Startup procedure is not yet ready for updates. */
		return(DB_SUCCESS);
	}

	dberr_t		err = DB_SUCCESS;
	trx_t*		trx;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(mutex_own(&dict_sys->mutex));

	trx = trx_allocate_for_background();
	trx->op_info = "update filepath";
	trx->dict_operation_lock_mode = RW_X_LATCH;
	trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

	pars_info_t*	info = pars_info_create();

	pars_info_add_int4_literal(info, "space", space_id);
	pars_info_add_str_literal(info, "path", filepath);

	err = que_eval_sql(info,
			   "PROCEDURE UPDATE_FILEPATH () IS\n"
			   "BEGIN\n"
			   "UPDATE SYS_DATAFILES"
			   " SET PATH = :path\n"
			   " WHERE SPACE = :space;\n"
			   "END;\n", FALSE, trx);

	trx_commit_for_mysql(trx);
	trx->dict_operation_lock_mode = 0;
	trx_free_for_background(trx);

	if (err == DB_SUCCESS) {
		/* We just updated SYS_DATAFILES due to the contents in
		a link file.  Make a note that we did this. */
		ib::info() << "The InnoDB data dictionary table SYS_DATAFILES"
			" for tablespace ID " << space_id
			<< " was updated to use file " << filepath << ".";
	} else {
		ib::warn() << "Error occurred while updating InnoDB data"
			" dictionary table SYS_DATAFILES for tablespace ID "
			<< space_id << " to file " << filepath << ": "
			<< ut_strerr(err) << ".";
	}

	return(err);
}

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
	ulint		fsp_flags)
{
	if (!srv_sys_tablespaces_open) {
		/* Startup procedure is not yet ready for updates.
		Return success since this will likely get updated
		later. */
		return(DB_SUCCESS);
	}

	dberr_t		err = DB_SUCCESS;
	trx_t*		trx;

	DBUG_EXECUTE_IF("innodb_fail_to_update_tablespace_dict",
			return(DB_INTERRUPTED););

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(filepath);

	trx = trx_allocate_for_background();
	trx->op_info = "insert tablespace and filepath";
	trx->dict_operation_lock_mode = RW_X_LATCH;
	trx_start_for_ddl(trx, TRX_DICT_OP_INDEX);

	/* A record for this space ID was not found in
	SYS_DATAFILES. Assume the record is also missing in
	SYS_TABLESPACES.  Insert records into them both. */
	err = dict_replace_tablespace_in_dictionary(
		space_id, name, fsp_flags, filepath, trx, false);

	trx_commit_for_mysql(trx);
	trx->dict_operation_lock_mode = 0;
	trx_free_for_background(trx);

	return(err);
}

/** Check the validity of a SYS_TABLES record
Make sure the fields are the right length and that they
do not contain invalid contents.
@param[in]	rec	SYS_TABLES record
@return error message, or NULL on success */
static
const char*
dict_sys_tables_rec_check(
	const rec_t*	rec)
{
	const byte*	field;
	ulint		len;

	ut_ad(mutex_own(&dict_sys->mutex));

	if (rec_get_deleted_flag(rec, 0)) {
		return("delete-marked record in SYS_TABLES");
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_TABLES) {
		return("wrong number of columns in SYS_TABLES record");
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
err_len:
		return("incorrect column length in SYS_TABLES");
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(rec, DICT_FLD__SYS_TABLES__ID, &len);
	if (len != 8) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
	if (field == NULL || len != 4) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(rec, DICT_FLD__SYS_TABLES__TYPE, &len);
	if (len != 4) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__MIX_ID, &len);
	if (len != 8) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__MIX_LEN, &len);
	if (field == NULL || len != 4) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_TABLES__CLUSTER_ID, &len);
	if (len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__SPACE, &len);
	if (field == NULL || len != 4) {
		goto err_len;
	}

	return(NULL);
}

/** Read and return the contents of a SYS_TABLESPACES record.
@param[in]	rec	A record of SYS_TABLESPACES
@param[out]	id	Pointer to the space_id for this table
@param[in,out]	name	Buffer for Tablespace Name of length NAME_LEN
@param[out]	flags	Pointer to tablespace flags
@return true if the record was read correctly, false if not. */
bool
dict_sys_tablespaces_rec_read(
	const rec_t*	rec,
	ulint*		id,
	char*		name,
	ulint*		flags)
{
	const byte*	field;
	ulint		len;

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLESPACES__SPACE, &len);
	if (len != DICT_FLD_LEN_SPACE) {
		ib::error() << "Wrong field length in SYS_TABLESPACES.SPACE: "
		<< len;
		return(false);
	}
	*id = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLESPACES__NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		ib::error() << "Wrong field length in SYS_TABLESPACES.NAME: "
			<< len;
		return(false);
	}
	strncpy(name, reinterpret_cast<const char*>(field), NAME_LEN);

	/* read the 4 byte flags from the TYPE field */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLESPACES__FLAGS, &len);
	if (len != 4) {
		ib::error() << "Wrong field length in SYS_TABLESPACES.FLAGS: "
			<< len;
		return(false);
	}
	*flags = mach_read_from_4(field);

	return(true);
}

/** Load and check each general tablespace mentioned in the SYS_TABLESPACES.
Ignore system and file-per-table tablespaces.
If it is valid, add it to the file_system list.
@param[in]	validate	true when the previous shutdown was not clean
@return the highest space ID found. */
UNIV_INLINE
ulint
dict_check_sys_tablespaces(
	bool		validate)
{
	ulint		max_space_id = 0;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mtr_t		mtr;

	DBUG_ENTER("dict_check_sys_tablespaces");

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(mutex_own(&dict_sys->mutex));

	/* Before traversing it, let's make sure we have
	SYS_TABLESPACES and SYS_DATAFILES loaded. */
	dict_table_get_low("SYS_TABLESPACES");
	dict_table_get_low("SYS_DATAFILES");

	mtr_start(&mtr);

	for (rec = dict_startscan_system(&pcur, &mtr, SYS_TABLESPACES);
	     rec != NULL;
	     rec = dict_getnext_system(&pcur, &mtr))
	{
		char	space_name[NAME_LEN];
		ulint	space_id = 0;
		ulint	fsp_flags;

		if (!dict_sys_tablespaces_rec_read(rec, &space_id,
						   space_name, &fsp_flags)) {
			continue;
		}

		/* Ignore system and file-per-table tablespaces. */
		if (is_system_tablespace(space_id)
		    || !fsp_is_shared_tablespace(fsp_flags)) {
			continue;
		}

		/* Ignore tablespaces that already are in the tablespace
		cache. */
		if (fil_space_for_table_exists_in_mem(
				space_id, space_name, false, true, NULL, 0)) {
			/* Recovery can open a datafile that does not
			match SYS_DATAFILES.  If they don't match, update
			SYS_DATAFILES. */
			char *dict_path = dict_get_first_path(space_id);
			char *fil_path = fil_space_get_first_path(space_id);
			if (dict_path && fil_path
			    && strcmp(dict_path, fil_path)) {
				dict_update_filepath(space_id, fil_path);
			}
			ut_free(dict_path);
			ut_free(fil_path);
			continue;
		}

		/* Set the expected filepath from the data dictionary.
		If the file is found elsewhere (from an ISL or the default
		location) or this path is the same file but looks different,
		fil_ibd_open() will update the dictionary with what is
		opened. */
		char*	filepath = dict_get_first_path(space_id);

		/* Check that the .ibd file exists. */
		dberr_t	err = fil_ibd_open(
			validate,
			!srv_read_only_mode && srv_log_file_size != 0,
			FIL_TYPE_TABLESPACE,
			space_id,
			fsp_flags,
			space_name,
			filepath);

		if (err != DB_SUCCESS) {
			ib::warn() << "Ignoring tablespace "
				<< id_name_t(space_name)
				<< " because it could not be opened.";
		}

		max_space_id = ut_max(max_space_id, space_id);

		ut_free(filepath);
	}

	mtr_commit(&mtr);

	DBUG_RETURN(max_space_id);
}

/** Read and return 5 integer fields from a SYS_TABLES record.
@param[in]	rec		A record of SYS_TABLES
@param[in]	name		Table Name, the same as SYS_TABLES.NAME
@param[out]	table_id	Pointer to the table_id for this table
@param[out]	space_id	Pointer to the space_id for this table
@param[out]	n_cols		Pointer to number of columns for this table.
@param[out]	flags		Pointer to table flags
@param[out]	flags2		Pointer to table flags2
@return true if the record was read correctly, false if not. */
static
bool
dict_sys_tables_rec_read(
	const rec_t*		rec,
	const table_name_t&	table_name,
	table_id_t*		table_id,
	ulint*			space_id,
	ulint*			n_cols,
	ulint*			flags,
	ulint*			flags2)
{
	const byte*	field;
	ulint		len;
	ulint		type;

	*flags2 = 0;

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__ID, &len);
	ut_ad(len == 8);
	*table_id = static_cast<table_id_t>(mach_read_from_8(field));

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__SPACE, &len);
	ut_ad(len == 4);
	*space_id = mach_read_from_4(field);

	/* Read the 4 byte flags from the TYPE field */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__TYPE, &len);
	ut_a(len == 4);
	type = mach_read_from_4(field);

	/* The low order bit of SYS_TABLES.TYPE is always set to 1. But in
	dict_table_t::flags the low order bit is used to determine if the
	row format is Redundant (0) or Compact (1) when the format is Antelope.
	Read the 4 byte N_COLS field and look at the high order bit.  It
	should be set for COMPACT and later.  It should not be set for
	REDUNDANT. */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
	ut_a(len == 4);
	*n_cols = mach_read_from_4(field);

	/* This validation function also combines the DICT_N_COLS_COMPACT
	flag in n_cols into the type field to effectively make it a
	dict_table_t::flags. */

	if (ULINT_UNDEFINED == dict_sys_tables_type_validate(type, *n_cols)) {
		ib::error() << "Table " << table_name << " in InnoDB"
			" data dictionary contains invalid flags."
			" SYS_TABLES.TYPE=" << type <<
			" SYS_TABLES.N_COLS=" << *n_cols;
		*flags = ULINT_UNDEFINED;
		return(false);
	}

	*flags = dict_sys_tables_type_to_tf(type, *n_cols);

	/* Get flags2 from SYS_TABLES.MIX_LEN */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__MIX_LEN, &len);
	*flags2 = mach_read_from_4(field);

	/* DICT_TF2_FTS will be set when indexes are being loaded */
	*flags2 &= ~DICT_TF2_FTS;

	/* Now that we have used this bit, unset it. */
	*n_cols &= ~DICT_N_COLS_COMPACT;

	return(true);
}

/** Load and check each non-predefined tablespace mentioned in SYS_TABLES.
Search SYS_TABLES and check each tablespace mentioned that has not
already been added to the fil_system.  If it is valid, add it to the
file_system list.  Perform extra validation on the table if recovery from
the REDO log occurred.
@param[in]	validate	Whether to do validation on the table.
@return the highest space ID found. */
UNIV_INLINE
ulint
dict_check_sys_tables(
	bool		validate)
{
	ulint		max_space_id = 0;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mtr_t		mtr;

	DBUG_ENTER("dict_check_sys_tables");

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(mutex_own(&dict_sys->mutex));

	mtr_start(&mtr);

	/* Before traversing SYS_TABLES, let's make sure we have
	SYS_TABLESPACES and SYS_DATAFILES loaded. */
	dict_table_t*	sys_tablespaces;
	dict_table_t*	sys_datafiles;
	sys_tablespaces = dict_table_get_low("SYS_TABLESPACES");
	ut_a(sys_tablespaces != NULL);
	sys_datafiles = dict_table_get_low("SYS_DATAFILES");
	ut_a(sys_datafiles != NULL);

	for (rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES);
	     rec != NULL;
	     rec = dict_getnext_system(&pcur, &mtr)) {
		const byte*	field;
		ulint		len;
		char*		space_name;
		table_name_t	table_name;
		table_id_t	table_id;
		ulint		space_id;
		ulint		n_cols;
		ulint		flags;
		ulint		flags2;

		/* If a table record is not useable, ignore it and continue
		on to the next record. Error messages were logged. */
		if (dict_sys_tables_rec_check(rec) != NULL) {
			continue;
		}

		/* Copy the table name from rec */
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__NAME, &len);
		table_name.m_name = mem_strdupl((char*) field, len);
		DBUG_PRINT("dict_check_sys_tables",
			   ("name: %p, '%s'", table_name.m_name,
			    table_name.m_name));

		dict_sys_tables_rec_read(rec, table_name,
					 &table_id, &space_id,
					 &n_cols, &flags, &flags2);
		if (flags == ULINT_UNDEFINED
		    || is_system_tablespace(space_id)) {
			ut_free(table_name.m_name);
			continue;
		}

		if (flags2 & DICT_TF2_DISCARDED) {
			ib::info() << "Ignoring tablespace " << table_name
				<< " because the DISCARD flag is set .";
			ut_free(table_name.m_name);
			continue;
		}

		/* If the table is not a predefined tablespace then it must
		be in a file-per-table or shared tablespace.
		Note that flags2 is not available for REDUNDANT tables,
		so don't check those. */
		ut_ad(DICT_TF_HAS_SHARED_SPACE(flags)
		      || !DICT_TF_GET_COMPACT(flags)
		      || flags2 & DICT_TF2_USE_FILE_PER_TABLE);

		/* Look up the tablespace name in the data dictionary if this
		is a shared tablespace.  For file-per-table, the table_name
		and the tablespace_name are the same.
		Some hidden tables like FTS AUX tables may not be found in
		the dictionary since they can always be found in the default
		location. If so, then dict_space_get_name() will return NULL,
		the space name must be the table_name, and the filepath can be
		discovered in the default location.*/
		char*	shared_space_name = dict_space_get_name(space_id, NULL);
		space_name = shared_space_name == NULL
			? table_name.m_name
			: shared_space_name;

		/* Now that we have the proper name for this tablespace,
		whether it is a shared tablespace or a single table
		tablespace, look to see if it is already in the tablespace
		cache. */
		if (fil_space_for_table_exists_in_mem(
			    space_id, space_name, false, true, NULL, 0)) {
			/* Recovery can open a datafile that does not
			match SYS_DATAFILES.  If they don't match, update
			SYS_DATAFILES. */
			char *dict_path = dict_get_first_path(space_id);
			char *fil_path = fil_space_get_first_path(space_id);
			if (dict_path && fil_path
			    && strcmp(dict_path, fil_path)) {
				dict_update_filepath(space_id, fil_path);
			}
			ut_free(dict_path);
			ut_free(fil_path);
			ut_free(table_name.m_name);
			ut_free(shared_space_name);
			continue;
		}

		/* Set the expected filepath from the data dictionary.
		If the file is found elsewhere (from an ISL or the default
		location) or this path is the same file but looks different,
		fil_ibd_open() will update the dictionary with what is
		opened. */
		char*	filepath = dict_get_first_path(space_id);

		/* Check that the .ibd file exists. */
		bool	is_temp = flags2 & DICT_TF2_TEMPORARY;
		bool	is_encrypted = flags2 & DICT_TF2_ENCRYPTION;
		ulint	fsp_flags = dict_tf_to_fsp_flags(flags,
							 is_temp,
							 is_encrypted);

		dberr_t	err = fil_ibd_open(
			validate,
			!srv_read_only_mode && srv_log_file_size != 0,
			FIL_TYPE_TABLESPACE,
			space_id,
			fsp_flags,
			space_name,
			filepath);

		if (err != DB_SUCCESS) {
			ib::warn() << "Ignoring tablespace "
				<< id_name_t(space_name)
				<< " because it could not be opened.";
		}

		max_space_id = ut_max(max_space_id, space_id);

		ut_free(table_name.m_name);
		ut_free(shared_space_name);
		ut_free(filepath);
	}

	mtr_commit(&mtr);

	DBUG_RETURN(max_space_id);
}

/** Check each tablespace found in the data dictionary.
Look at each general tablespace found in SYS_TABLESPACES.
Then look at each table defined in SYS_TABLES that has a space_id > 0
to find all the file-per-table tablespaces.

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
	bool	validate)
{
	mtr_t	mtr;

	DBUG_ENTER("dict_check_tablespaces_and_store_max_id");

	rw_lock_x_lock(dict_operation_lock);
	mutex_enter(&dict_sys->mutex);

	/* Initialize the max space_id from sys header */
	mtr_start(&mtr);
	ulint	max_space_id = mtr_read_ulint(
		dict_hdr_get(&mtr) + DICT_HDR_MAX_SPACE_ID,
		MLOG_4BYTES, &mtr);
	mtr_commit(&mtr);

	fil_set_max_space_id_if_bigger(max_space_id);

	/* Open all general tablespaces found in SYS_TABLESPACES. */
	ulint	max1 = dict_check_sys_tablespaces(validate);

	/* Open all tablespaces referenced in SYS_TABLES.
	This will update SYS_TABLESPACES and SYS_DATAFILES if it
	finds any file-per-table tablespaces not already there. */
	ulint	max2 = dict_check_sys_tables(validate);

	/* Store the max space_id found */
	max_space_id = ut_max(max1, max2);
	fil_set_max_space_id_if_bigger(max_space_id);

	mutex_exit(&dict_sys->mutex);
	rw_lock_x_unlock(dict_operation_lock);

	DBUG_VOID_RETURN;
}

/** Error message for a delete-marked record in dict_load_column_low() */
static const char* dict_load_column_del = "delete-marked record in SYS_COLUMN";

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
	ulint*		nth_v_col)	/*!< out: if not NULL, this
					records the "n" of "nth" virtual
					column */
{
	char*		name;
	const byte*	field;
	ulint		len;
	ulint		mtype;
	ulint		prtype;
	ulint		col_len;
	ulint		pos;
	ulint		num_base;

	ut_ad(table || column);

	if (rec_get_deleted_flag(rec, 0)) {
		return(dict_load_column_del);
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_COLUMNS) {
		return("wrong number of columns in SYS_COLUMNS record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__TABLE_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_COLUMNS");
	}

	if (table_id) {
		*table_id = mach_read_from_8(field);
	} else if (table->id != mach_read_from_8(field)) {
		return("SYS_COLUMNS.TABLE_ID mismatch");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__POS, &len);
	if (len != 4) {
		goto err_len;
	}

	pos = mach_read_from_4(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_COLUMNS__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_COLUMNS__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}

	name = mem_heap_strdupl(heap, (const char*) field, len);

	if (col_name) {
		*col_name = name;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__MTYPE, &len);
	if (len != 4) {
		goto err_len;
	}

	mtype = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__PRTYPE, &len);
	if (len != 4) {
		goto err_len;
	}
	prtype = mach_read_from_4(field);

	if (dtype_get_charset_coll(prtype) == 0
	    && dtype_is_string_type(mtype)) {
		/* The table was created with < 4.1.2. */

		if (dtype_is_binary_string_type(mtype, prtype)) {
			/* Use the binary collation for
			string columns of binary type. */

			prtype = dtype_form_prtype(
				prtype,
				DATA_MYSQL_BINARY_CHARSET_COLL);
		} else {
			/* Use the default charset for
			other than binary columns. */

			prtype = dtype_form_prtype(
				prtype,
				data_mysql_default_charset_coll);
		}
	}

	if (table && table->n_def != pos && !(prtype & DATA_VIRTUAL)) {
		return("SYS_COLUMNS.POS mismatch");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__LEN, &len);
	if (len != 4) {
		goto err_len;
	}
	col_len = mach_read_from_4(field);
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_COLUMNS__PREC, &len);
	if (len != 4) {
		goto err_len;
	}
	num_base = mach_read_from_4(field);

	if (column == NULL) {
		if (prtype & DATA_VIRTUAL) {
#ifdef UNIV_DEBUG
			dict_v_col_t*	vcol =
#endif
			dict_mem_table_add_v_col(
				table, heap, name, mtype,
				prtype, col_len,
				dict_get_v_col_mysql_pos(pos), num_base);
			ut_ad(vcol->v_pos == dict_get_v_col_pos(pos));
		} else {
			ut_ad(num_base == 0);
			dict_mem_table_add_col(table, heap, name, mtype,
					       prtype, col_len);
		}
	} else {
		dict_mem_fill_column_struct(column, pos, mtype,
					    prtype, col_len);
	}

	/* Report the virtual column number */
	if ((prtype & DATA_VIRTUAL) && nth_v_col != NULL) {
		*nth_v_col = dict_get_v_col_pos(pos);
	}

	return(NULL);
}

/** Error message for a delete-marked record in dict_load_virtual_low() */
static const char* dict_load_virtual_del = "delete-marked record in SYS_VIRTUAL";

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
	dict_table_t*	table,
	mem_heap_t*	heap,
	dict_col_t**	column,
	table_id_t*	table_id,
	ulint*		pos,
	ulint*		base_pos,
	const rec_t*	rec)
{
	const byte*	field;
	ulint		len;
	ulint		base;

	if (rec_get_deleted_flag(rec, 0)) {
		return(dict_load_virtual_del);
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_VIRTUAL) {
		return("wrong number of columns in SYS_VIRTUAL record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_VIRTUAL__TABLE_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_VIRTUAL");
	}

	if (table_id != NULL) {
		*table_id = mach_read_from_8(field);
	} else if (table->id != mach_read_from_8(field)) {
		return("SYS_VIRTUAL.TABLE_ID mismatch");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_VIRTUAL__POS, &len);
	if (len != 4) {
		goto err_len;
	}

	if (pos != NULL) {
		*pos = mach_read_from_4(field);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_VIRTUAL__BASE_POS, &len);
	if (len != 4) {
		goto err_len;
	}

	base = mach_read_from_4(field);

	if (base_pos != NULL) {
		*base_pos = base;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_VIRTUAL__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_VIRTUAL__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	if (column != NULL) {
		*column = dict_table_get_nth_col(table, base);
	}

	return(NULL);
}
/********************************************************************//**
Loads definitions for table columns. */
static
void
dict_load_columns(
/*==============*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap)	/*!< in/out: memory heap
				for temporary storage */
{
	dict_table_t*	sys_columns;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	ulint		i;
	mtr_t		mtr;
	ulint		n_skipped = 0;

	ut_ad(mutex_own(&dict_sys->mutex));

	mtr_start(&mtr);

	sys_columns = dict_table_get_low("SYS_COLUMNS");
	sys_index = UT_LIST_GET_FIRST(sys_columns->indexes);
	ut_ad(!dict_table_is_comp(sys_columns));

	ut_ad(name_of_col_is(sys_columns, sys_index,
			     DICT_FLD__SYS_COLUMNS__NAME, "NAME"));
	ut_ad(name_of_col_is(sys_columns, sys_index,
			     DICT_FLD__SYS_COLUMNS__PREC, "PREC"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);

	ut_ad(table->n_t_cols == static_cast<ulint>(
	      table->n_cols) + static_cast<ulint>(table->n_v_cols));

	for (i = 0;
	     i + DATA_N_SYS_COLS < table->n_t_cols + n_skipped;
	     i++) {
		const char*	err_msg;
		const char*	name = NULL;
		ulint		nth_v_col = ULINT_UNDEFINED;

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		err_msg = dict_load_column_low(table, heap, NULL, NULL,
					       &name, rec, &nth_v_col);

		if (err_msg == dict_load_column_del) {
			n_skipped++;
			goto next_rec;
		} else if (err_msg) {
			ib::fatal() << err_msg;
		}

		/* Note: Currently we have one DOC_ID column that is
		shared by all FTS indexes on a table. And only non-virtual
		column can be used for FULLTEXT index */
		if (innobase_strcasecmp(name,
					FTS_DOC_ID_COL_NAME) == 0
		    && nth_v_col == ULINT_UNDEFINED) {
			dict_col_t*	col;
			/* As part of normal loading of tables the
			table->flag is not set for tables with FTS
			till after the FTS indexes are loaded. So we
			create the fts_t instance here if there isn't
			one already created.

			This case does not arise for table create as
			the flag is set before the table is created. */
			if (table->fts == NULL) {
				table->fts = fts_create(table);
				fts_optimize_add_table(table);
			}

			ut_a(table->fts->doc_col == ULINT_UNDEFINED);

			col = dict_table_get_nth_col(table, i - n_skipped);

			ut_ad(col->len == sizeof(doc_id_t));

			if (col->prtype & DATA_FTS_DOC_ID) {
				DICT_TF2_FLAG_SET(
					table, DICT_TF2_FTS_HAS_DOC_ID);
				DICT_TF2_FLAG_UNSET(
					table, DICT_TF2_FTS_ADD_DOC_ID);
			}

			table->fts->doc_col = i - n_skipped;
		}
next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/** Loads SYS_VIRTUAL info for one virtual column
@param[in,out]	table		table
@param[in]	nth_v_col	virtual column sequence num
@param[in,out]	v_col		virtual column
@param[in,out]	heap		memory heap
*/
static
void
dict_load_virtual_one_col(
	dict_table_t*	table,
	ulint		nth_v_col,
	dict_v_col_t*	v_col,
	mem_heap_t*	heap)
{
	dict_table_t*	sys_virtual;
	dict_index_t*	sys_virtual_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	ulint		i = 0;
	mtr_t		mtr;
	ulint		skipped = 0;

	ut_ad(mutex_own(&dict_sys->mutex));

	if (v_col->num_base == 0) {
		return;
	}

	mtr_start(&mtr);

	sys_virtual = dict_table_get_low("SYS_VIRTUAL");
	sys_virtual_index = UT_LIST_GET_FIRST(sys_virtual->indexes);
	ut_ad(!dict_table_is_comp(sys_virtual));

	ut_ad(name_of_col_is(sys_virtual, sys_virtual_index,
			     DICT_FLD__SYS_VIRTUAL__POS, "POS"));

	tuple = dtuple_create(heap, 2);

	/* table ID field */
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);

	/* virtual column pos field */
	dfield = dtuple_get_nth_field(tuple, 1);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 4));
	ulint	vcol_pos = dict_create_v_col_pos(nth_v_col, v_col->m_col.ind);
	mach_write_to_4(buf, vcol_pos);

	dfield_set_data(dfield, buf, 4);

	dict_index_copy_types(tuple, sys_virtual_index, 2);

	btr_pcur_open_on_user_rec(sys_virtual_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);

	for (i = 0; i < v_col->num_base + skipped; i++) {
		const char*	err_msg;
		ulint		pos;

		ut_ad(btr_pcur_is_on_user_rec(&pcur));

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		err_msg = dict_load_virtual_low(table, heap,
						&v_col->base_col[i - skipped],
						NULL,
					        &pos, NULL, rec);

		if (err_msg) {
			if (err_msg != dict_load_virtual_del) {
				ib::fatal() << err_msg;
			} else {
				skipped++;
			}
		} else {
			ut_ad(pos == vcol_pos);
		}

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/** Loads info from SYS_VIRTUAL for virtual columns.
@param[in,out]	table	table
@param[in]	heap	memory heap
*/
static
void
dict_load_virtual(
	dict_table_t*	table,
	mem_heap_t*	heap)
{
	for (ulint i = 0; i < table->n_v_cols; i++) {
		dict_v_col_t*	v_col = dict_table_get_nth_v_col(table, i);

		dict_load_virtual_one_col(table, i, v_col, heap);
	}
}

/** Error message for a delete-marked record in dict_load_field_low() */
static const char* dict_load_field_del = "delete-marked record in SYS_FIELDS";

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
	const rec_t*	rec)		/*!< in: SYS_FIELDS record */
{
	const byte*	field;
	ulint		len;
	ulint		pos_and_prefix_len;
	ulint		prefix_len;
	ibool		first_field;
	ulint		position;

	/* Either index or sys_field is supplied, not both */
	ut_a((!index) || (!sys_field));

	if (rec_get_deleted_flag(rec, 0)) {
		return(dict_load_field_del);
	}

	if (rec_get_n_fields_old(rec) != DICT_NUM_FIELDS__SYS_FIELDS) {
		return("wrong number of columns in SYS_FIELDS record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FIELDS__INDEX_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_FIELDS");
	}

	if (!index) {
		ut_a(last_index_id);
		memcpy(index_id, (const char*) field, 8);
		first_field = memcmp(index_id, last_index_id, 8);
	} else {
		first_field = (index->n_def == 0);
		if (memcmp(field, index_id, 8)) {
			return("SYS_FIELDS.INDEX_ID mismatch");
		}
	}

	/* The next field stores the field position in the index and a
	possible column prefix length if the index field does not
	contain the whole column. The storage format is like this: if
	there is at least one prefix field in the index, then the HIGH
	2 bytes contain the field number (index->n_def) and the low 2
	bytes the prefix length for the field. Otherwise the field
	number (index->n_def) is contained in the 2 LOW bytes. */

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FIELDS__POS, &len);
	if (len != 4) {
		goto err_len;
	}

	pos_and_prefix_len = mach_read_from_4(field);

	if (index && UNIV_UNLIKELY
	    ((pos_and_prefix_len & 0xFFFFUL) != index->n_def
	     && (pos_and_prefix_len >> 16 & 0xFFFF) != index->n_def)) {
		return("SYS_FIELDS.POS mismatch");
	}

	if (first_field || pos_and_prefix_len > 0xFFFFUL) {
		prefix_len = pos_and_prefix_len & 0xFFFFUL;
		position = (pos_and_prefix_len & 0xFFFF0000UL)  >> 16;
	} else {
		prefix_len = 0;
		position = pos_and_prefix_len & 0xFFFFUL;
	}

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FIELDS__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_FIELDS__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FIELDS__COL_NAME, &len);
	if (len == 0 || len == UNIV_SQL_NULL) {
		goto err_len;
	}

	if (index) {
		dict_mem_index_add_field(
			index, mem_heap_strdupl(heap, (const char*) field, len),
			prefix_len);
	} else {
		ut_a(sys_field);
		ut_a(pos);

		sys_field->name = mem_heap_strdupl(
			heap, (const char*) field, len);
		sys_field->prefix_len = prefix_len;
		*pos = position;
	}

	return(NULL);
}

/********************************************************************//**
Loads definitions for index fields.
@return DB_SUCCESS if ok, DB_CORRUPTION if corruption */
static
ulint
dict_load_fields(
/*=============*/
	dict_index_t*	index,	/*!< in/out: index whose fields to load */
	mem_heap_t*	heap)	/*!< in: memory heap for temporary storage */
{
	dict_table_t*	sys_fields;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	ulint		i;
	mtr_t		mtr;
	dberr_t		error;

	ut_ad(mutex_own(&dict_sys->mutex));

	mtr_start(&mtr);

	sys_fields = dict_table_get_low("SYS_FIELDS");
	sys_index = UT_LIST_GET_FIRST(sys_fields->indexes);
	ut_ad(!dict_table_is_comp(sys_fields));
	ut_ad(name_of_col_is(sys_fields, sys_index,
			     DICT_FLD__SYS_FIELDS__COL_NAME, "COL_NAME"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, index->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i < index->n_fields; i++) {
		const char* err_msg;

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		err_msg = dict_load_field_low(buf, index, NULL, NULL, NULL,
					      heap, rec);

		if (err_msg == dict_load_field_del) {
			/* There could be delete marked records in
			SYS_FIELDS because SYS_FIELDS.INDEX_ID can be
			updated by ALTER TABLE ADD INDEX. */

			goto next_rec;
		} else if (err_msg) {
			ib::error() << err_msg;
			error = DB_CORRUPTION;
			goto func_exit;
		}
next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	error = DB_SUCCESS;
func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	return(error);
}

/** Error message for a delete-marked record in dict_load_index_low() */
static const char* dict_load_index_del = "delete-marked record in SYS_INDEXES";
/** Error message for table->id mismatch in dict_load_index_low() */
static const char* dict_load_index_id_err = "SYS_INDEXES.TABLE_ID mismatch";

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
	dict_index_t**	index)		/*!< out,own: index, or NULL */
{
	const byte*	field;
	ulint		len;
	ulint		name_len;
	char*		name_buf;
	index_id_t	id;
	ulint		n_fields;
	ulint		type;
	ulint		space;
	ulint		merge_threshold;

	if (allocate) {
		/* If allocate=TRUE, no dict_index_t will
		be supplied. Initialize "*index" to NULL */
		*index = NULL;
	}

	if (rec_get_deleted_flag(rec, 0)) {
		return(dict_load_index_del);
	}

	if (rec_get_n_fields_old(rec) == DICT_NUM_FIELDS__SYS_INDEXES) {
		/* MERGE_THRESHOLD exists */
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_INDEXES__MERGE_THRESHOLD, &len);
		switch (len) {
		case 4:
			merge_threshold = mach_read_from_4(field);
			break;
		case UNIV_SQL_NULL:
			merge_threshold = DICT_INDEX_MERGE_THRESHOLD_DEFAULT;
			break;
		default:
			return("incorrect MERGE_THRESHOLD length"
			       " in SYS_INDEXES");
		}
	} else if (rec_get_n_fields_old(rec)
		   == DICT_NUM_FIELDS__SYS_INDEXES - 1) {
		/* MERGE_THRESHOLD doesn't exist */

		merge_threshold = DICT_INDEX_MERGE_THRESHOLD_DEFAULT;
	} else {
		return("wrong number of columns in SYS_INDEXES record");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__TABLE_ID, &len);
	if (len != 8) {
err_len:
		return("incorrect column length in SYS_INDEXES");
	}

	if (!allocate) {
		/* We are reading a SYS_INDEXES record. Copy the table_id */
		memcpy(table_id, (const char*) field, 8);
	} else if (memcmp(field, table_id, 8)) {
		/* Caller supplied table_id, verify it is the same
		id as on the index record */
		return(dict_load_index_id_err);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__ID, &len);
	if (len != 8) {
		goto err_len;
	}

	id = mach_read_from_8(field);

	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_INDEXES__DB_TRX_ID, &len);
	if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}
	rec_get_nth_field_offs_old(
		rec, DICT_FLD__SYS_INDEXES__DB_ROLL_PTR, &len);
	if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
		goto err_len;
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__NAME, &name_len);
	if (name_len == UNIV_SQL_NULL) {
		goto err_len;
	}

	name_buf = mem_heap_strdupl(heap, (const char*) field,
				    name_len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__N_FIELDS, &len);
	if (len != 4) {
		goto err_len;
	}
	n_fields = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__TYPE, &len);
	if (len != 4) {
		goto err_len;
	}
	type = mach_read_from_4(field);
	if (type & (~0U << DICT_IT_BITS)) {
		return("unknown SYS_INDEXES.TYPE bits");
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__SPACE, &len);
	if (len != 4) {
		goto err_len;
	}
	space = mach_read_from_4(field);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__PAGE_NO, &len);
	if (len != 4) {
		goto err_len;
	}

	if (allocate) {
		*index = dict_mem_index_create(table_name, name_buf,
					       space, type, n_fields);
	} else {
		ut_a(*index);

		dict_mem_fill_index_struct(*index, NULL, NULL, name_buf,
					   space, type, n_fields);
	}

	(*index)->id = id;
	(*index)->page = mach_read_from_4(field);
	ut_ad((*index)->page);
	(*index)->merge_threshold = merge_threshold;

	return(NULL);
}

/********************************************************************//**
Loads definitions for table indexes. Adds them to the data dictionary
cache.
@return DB_SUCCESS if ok, DB_CORRUPTION if corruption of dictionary
table or DB_UNSUPPORTED if table has unknown index type */
static MY_ATTRIBUTE((nonnull))
dberr_t
dict_load_indexes(
/*==============*/
	dict_table_t*	table,	/*!< in/out: table */
	mem_heap_t*	heap,	/*!< in: memory heap for temporary storage */
	dict_err_ignore_t ignore_err)
				/*!< in: error to be ignored when
				loading the index definition */
{
	dict_table_t*	sys_indexes;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	byte*		buf;
	mtr_t		mtr;
	dberr_t		error = DB_SUCCESS;

	ut_ad(mutex_own(&dict_sys->mutex));

	mtr_start(&mtr);

	sys_indexes = dict_table_get_low("SYS_INDEXES");
	sys_index = UT_LIST_GET_FIRST(sys_indexes->indexes);
	ut_ad(!dict_table_is_comp(sys_indexes));
	ut_ad(name_of_col_is(sys_indexes, sys_index,
			     DICT_FLD__SYS_INDEXES__NAME, "NAME"));
	ut_ad(name_of_col_is(sys_indexes, sys_index,
			     DICT_FLD__SYS_INDEXES__PAGE_NO, "PAGE_NO"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (;;) {
		dict_index_t*	index = NULL;
		const char*	err_msg;

		if (!btr_pcur_is_on_user_rec(&pcur)) {

			/* We should allow the table to open even
			without index when DICT_ERR_IGNORE_CORRUPT is set.
			DICT_ERR_IGNORE_CORRUPT is currently only set
			for drop table */
			if (dict_table_get_first_index(table) == NULL
			    && !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {
				ib::warn() << "Cannot load table "
					<< table->name
					<< " because it has no indexes in"
					" InnoDB internal data dictionary.";
				error = DB_CORRUPTION;
				goto func_exit;
			}

			break;
		}

		rec = btr_pcur_get_rec(&pcur);

		if ((ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK)
		    && (rec_get_n_fields_old(rec)
			== DICT_NUM_FIELDS__SYS_INDEXES
			/* a record for older SYS_INDEXES table
			(missing merge_threshold column) is acceptable. */
			|| rec_get_n_fields_old(rec)
			   == DICT_NUM_FIELDS__SYS_INDEXES - 1)) {
			const byte*	field;
			ulint		len;
			field = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_INDEXES__NAME, &len);

			if (len != UNIV_SQL_NULL
			    && static_cast<char>(*field)
			    == static_cast<char>(*TEMP_INDEX_PREFIX_STR)) {
				/* Skip indexes whose name starts with
				TEMP_INDEX_PREFIX, because they will
				be dropped during crash recovery. */
				goto next_rec;
			}
		}

		err_msg = dict_load_index_low(
			buf, table->name.m_name, heap, rec, TRUE, &index);
		ut_ad((index == NULL && err_msg != NULL)
		      || (index != NULL && err_msg == NULL));

		if (err_msg == dict_load_index_id_err) {
			/* TABLE_ID mismatch means that we have
			run out of index definitions for the table. */

			if (dict_table_get_first_index(table) == NULL
			    && !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {

				ib::warn() << "Failed to load the"
					" clustered index for table "
					<< table->name
					<< " because of the following error: "
					<< err_msg << "."
					" Refusing to load the rest of the"
					" indexes (if any) and the whole table"
					" altogether.";
				error = DB_CORRUPTION;
				goto func_exit;
			}

			break;
		} else if (err_msg == dict_load_index_del) {
			/* Skip delete-marked records. */
			goto next_rec;
		} else if (err_msg) {
			ib::error() << err_msg;
			if (ignore_err & DICT_ERR_IGNORE_CORRUPT) {
				goto next_rec;
			}
			error = DB_CORRUPTION;
			goto func_exit;
		}

		ut_ad(index);

		/* Check whether the index is corrupted */
		if (dict_index_is_corrupted(index)) {

			ib::error() << "Index " << index->name
				<< " of table " << table->name
				<< " is corrupted";

			if (!srv_load_corrupted
			    && !(ignore_err & DICT_ERR_IGNORE_CORRUPT)
			    && dict_index_is_clust(index)) {
				dict_mem_index_free(index);

				error = DB_INDEX_CORRUPT;
				goto func_exit;
			} else {
				/* We will load the index if
				1) srv_load_corrupted is TRUE
				2) ignore_err is set with
				DICT_ERR_IGNORE_CORRUPT
				3) if the index corrupted is a secondary
				index */
				ib::info() << "Load corrupted index "
					<< index->name
					<< " of table " << table->name;
			}
		}

		if (index->type & DICT_FTS
		    && !dict_table_has_fts_index(table)) {
			/* This should have been created by now. */
			ut_a(table->fts != NULL);
			DICT_TF2_FLAG_SET(table, DICT_TF2_FTS);
		}

		/* We check for unsupported types first, so that the
		subsequent checks are relevant for the supported types. */
		if (index->type & ~(DICT_CLUSTERED | DICT_UNIQUE
				    | DICT_CORRUPT | DICT_FTS
				    | DICT_SPATIAL | DICT_VIRTUAL)) {

			ib::error() << "Unknown type " << index->type
				<< " of index " << index->name
				<< " of table " << table->name;

			error = DB_UNSUPPORTED;
			dict_mem_index_free(index);
			goto func_exit;
		} else if (index->page == FIL_NULL
			   && !table->ibd_file_missing
			   && (!(index->type & DICT_FTS))) {

			ib::error() << "Trying to load index " << index->name
				<< " for table " << table->name
				<< ", but the index tree has been freed!";

			if (ignore_err & DICT_ERR_IGNORE_INDEX_ROOT) {
				/* If caller can tolerate this error,
				we will continue to load the index and
				let caller deal with this error. However
				mark the index and table corrupted. We
				only need to mark such in the index
				dictionary cache for such metadata corruption,
				since we would always be able to set it
				when loading the dictionary cache */
				index->table = table;
				dict_set_corrupted_index_cache_only(index);

				ib::info() << "Index is corrupt but forcing"
					" load into data dictionary";
			} else {
corrupted:
				dict_mem_index_free(index);
				error = DB_CORRUPTION;
				goto func_exit;
			}
		} else if (!dict_index_is_clust(index)
			   && NULL == dict_table_get_first_index(table)) {

			ib::error() << "Trying to load index " << index->name
				<< " for table " << table->name
				<< ", but the first index is not clustered!";

			goto corrupted;
		} else if (dict_is_sys_table(table->id)
			   && (dict_index_is_clust(index)
			       || ((table == dict_sys->sys_tables)
				   && !strcmp("ID_IND", index->name)))) {

			/* The index was created in memory already at booting
			of the database server */
			dict_mem_index_free(index);
		} else {
			dict_load_fields(index, heap);

			error = dict_index_add_to_cache(
				table, index, index->page, FALSE);

			/* The data dictionary tables should never contain
			invalid index definitions.  If we ignored this error
			and simply did not load this index definition, the
			.frm file would disagree with the index definitions
			inside InnoDB. */
			if (UNIV_UNLIKELY(error != DB_SUCCESS)) {

				goto func_exit;
			}
		}
next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	ut_ad(table->fts_doc_id_index == NULL);

	if (table->fts != NULL) {
		table->fts_doc_id_index = dict_table_get_index_on_name(
			table, FTS_DOC_ID_INDEX_NAME);
	}

	/* If the table contains FTS indexes, populate table->fts->indexes */
	if (dict_table_has_fts_index(table)) {
		ut_ad(table->fts_doc_id_index != NULL);
		/* table->fts->indexes should have been created. */
		ut_a(table->fts->indexes != NULL);
		dict_table_get_all_fts_indexes(table, table->fts->indexes);
	}

func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(error);
}

/** Loads a table definition from a SYS_TABLES record to dict_table_t.
Does not load any columns or indexes.
@param[in]	name	Table name
@param[in]	rec	SYS_TABLES record
@param[out,own]	table	table, or NULL
@return error message, or NULL on success */
static
const char*
dict_load_table_low(
	table_name_t&	name,
	const rec_t*	rec,
	dict_table_t**	table)
{
	table_id_t	table_id;
	ulint		space_id;
	ulint		n_cols;
	ulint		t_num;
	ulint		flags;
	ulint		flags2;
	ulint		n_v_col;

	const char* error_text = dict_sys_tables_rec_check(rec);
	if (error_text != NULL) {
		return(error_text);
	}

	dict_sys_tables_rec_read(rec, name, &table_id, &space_id,
				 &t_num, &flags, &flags2);

	if (flags == ULINT_UNDEFINED) {
		return("incorrect flags in SYS_TABLES");
	}

	dict_table_decode_n_col(t_num, &n_cols, &n_v_col);

	*table = dict_mem_table_create(
		name.m_name, space_id, n_cols + n_v_col, n_v_col, flags, flags2);
	(*table)->id = table_id;
	(*table)->ibd_file_missing = FALSE;

	return(NULL);
}

/********************************************************************//**
Using the table->heap, copy the null-terminated filepath into
table->data_dir_path and replace the 'databasename/tablename.ibd'
portion with 'tablename'.
This allows SHOW CREATE TABLE to return the correct DATA DIRECTORY path.
Make this data directory path only if it has not yet been saved. */
void
dict_save_data_dir_path(
/*====================*/
	dict_table_t*	table,		/*!< in/out: table */
	char*		filepath)	/*!< in: filepath of tablespace */
{
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(DICT_TF_HAS_DATA_DIR(table->flags));

	ut_a(!table->data_dir_path);
	ut_a(filepath);

	/* Be sure this filepath is not the default filepath. */
	char*	default_filepath = fil_make_filepath(
			NULL, table->name.m_name, IBD, false);
	if (default_filepath) {
		if (0 != strcmp(filepath, default_filepath)) {
			ulint pathlen = strlen(filepath);
			ut_a(pathlen < OS_FILE_MAX_PATH);
			ut_a(0 == strcmp(filepath + pathlen - 4, DOT_IBD));

			table->data_dir_path = mem_heap_strdup(
				table->heap, filepath);
			os_file_make_data_dir_path(table->data_dir_path);
		}

		ut_free(default_filepath);
	}
}

/** Make sure the data_dir_path is saved in dict_table_t if DATA DIRECTORY
was used. Try to read it from the fil_system first, then from SYS_DATAFILES.
@param[in]	table		Table object
@param[in]	dict_mutex_own	true if dict_sys->mutex is owned already */
void
dict_get_and_save_data_dir_path(
	dict_table_t*	table,
	bool		dict_mutex_own)
{
	if (DICT_TF_HAS_DATA_DIR(table->flags)
	    && (!table->data_dir_path)) {
		char*	path = fil_space_get_first_path(table->space);

		if (!dict_mutex_own) {
			dict_mutex_enter_for_mysql();
		}

		if (path == NULL) {
			path = dict_get_first_path(table->space);
		}

		if (path != NULL) {
			dict_save_data_dir_path(table, path);
			ut_free(path);
		}

		if (table->data_dir_path == NULL) {
			/* Since we did not set the table data_dir_path,
			unset the flag.  This does not change SYS_DATAFILES
			or SYS_TABLES or FSP_FLAGS on the header page of the
			tablespace, but it makes dict_table_t consistent. */
			table->flags &= ~DICT_TF_MASK_DATA_DIR;
		}

		if (!dict_mutex_own) {
			dict_mutex_exit_for_mysql();
		}
	}
}

/** Make sure the tablespace name is saved in dict_table_t if the table
uses a general tablespace.
Try to read it from the fil_system_t first, then from SYS_TABLESPACES.
@param[in]	table		Table object
@param[in]	dict_mutex_own)	true if dict_sys->mutex is owned already */
void
dict_get_and_save_space_name(
	dict_table_t*	table,
	bool		dict_mutex_own)
{
	/* Do this only for general tablespaces. */
	if (!DICT_TF_HAS_SHARED_SPACE(table->flags)) {
		return;
	}

	bool	use_cache = true;
	if (table->tablespace != NULL) {

		if (srv_sys_tablespaces_open
		    && dict_table_has_temp_general_tablespace_name(
			    table->tablespace)) {
			/* We previous saved the temporary name,
			get the real one now. */
			use_cache = false;
		} else {
			/* Keep and use this name */
			return;
		}
	}

	if (use_cache) {
		fil_space_t* space = fil_space_acquire_silent(table->space);

		if (space != NULL) {
			/* Use this name unless it is a temporary general
			tablespace name and we can now replace it. */
			if (!srv_sys_tablespaces_open
			    || !dict_table_has_temp_general_tablespace_name(
				    space->name)) {

				/* Use this tablespace name */
				table->tablespace = mem_heap_strdup(
					table->heap, space->name);

				fil_space_release(space);
				return;
			}
			fil_space_release(space);
		}
	}

	/* Read it from the dictionary. */
	if (srv_sys_tablespaces_open) {
		if (!dict_mutex_own) {
			dict_mutex_enter_for_mysql();
		}

		table->tablespace = dict_space_get_name(
			table->space, table->heap);

		if (!dict_mutex_own) {
			dict_mutex_exit_for_mysql();
		}
	}
}

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
	dict_err_ignore_t ignore_err)
{
	dict_names_t			fk_list;
	dict_table_t*			result;
	dict_names_t::iterator		i;
	table_name_t			table_name;

	DBUG_ENTER("dict_load_table");
	DBUG_PRINT("dict_load_table", ("loading table: '%s'", name));

	ut_ad(mutex_own(&dict_sys->mutex));

	table_name.m_name = const_cast<char*>(name);

	result = dict_table_check_if_in_cache_low(name);

	if (!result) {
		result = dict_load_table_one(table_name, cached, ignore_err,
					     fk_list);
		while (!fk_list.empty()) {
			table_name_t	fk_table_name;
			dict_table_t*	fk_table;

			fk_table_name.m_name =
				const_cast<char*>(fk_list.front());
			fk_table = dict_table_check_if_in_cache_low(
				fk_table_name.m_name);
			if (!fk_table) {
				dict_load_table_one(fk_table_name, cached,
						    ignore_err, fk_list);
			}
			fk_list.pop_front();
		}
	}

	DBUG_RETURN(result);
}

/** Opens a tablespace for dict_load_table_one()
@param[in,out]	table		A table that refers to the tablespace to open
@param[in]	heap		A memory heap
@param[in]	ignore_err	Whether to ignore an error. */
UNIV_INLINE
void
dict_load_tablespace(
	dict_table_t*		table,
	mem_heap_t*		heap,
	dict_err_ignore_t	ignore_err)
{
	/* The system tablespace is always available. */
	if (is_system_tablespace(table->space)) {
		return;
	}

	if (table->flags2 & DICT_TF2_DISCARDED) {
		ib::warn() << "Tablespace for table " << table->name
			<< " is set as discarded.";
		table->ibd_file_missing = TRUE;
		return;
	}

	if (dict_table_is_temporary(table)) {
		/* Do not bother to retry opening temporary tables. */
		table->ibd_file_missing = TRUE;
		return;
	}

	/* A file-per-table table name is also the tablespace name.
	A general tablespace name is not the same as the table name.
	Use the general tablespace name if it can be read from the
	dictionary, if not use 'innodb_general_##. */
	char*	shared_space_name = NULL;
	char*	space_name;
	if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
		if (srv_sys_tablespaces_open) {
			shared_space_name =
				dict_space_get_name(table->space, NULL);

		} else {
			/* Make the temporary tablespace name. */
			shared_space_name = static_cast<char*>(
				ut_malloc_nokey(
					strlen(general_space_name) + 20));

			sprintf(shared_space_name, "%s_" ULINTPF,
				general_space_name,
				static_cast<ulint>(table->space));
		}
		space_name = shared_space_name;
	} else {
		space_name = table->name.m_name;
	}

	/* The tablespace may already be open. */
	if (fil_space_for_table_exists_in_mem(
		    table->space, space_name, false,
		    true, heap, table->id)) {
		ut_free(shared_space_name);
		return;
	}

	if (!(ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK)) {
		ib::error() << "Failed to find tablespace for table "
			<< table->name << " in the cache. Attempting"
			" to load the tablespace with space id "
			<< table->space;
	}

	/* Use the remote filepath if needed. This parameter is optional
	in the call to fil_ibd_open(). If not supplied, it will be built
	from the space_name. */
	char* filepath = NULL;
	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		/* This will set table->data_dir_path from either
		fil_system or SYS_DATAFILES */
		dict_get_and_save_data_dir_path(table, true);

		if (table->data_dir_path) {
			filepath = fil_make_filepath(
				table->data_dir_path,
				table->name.m_name, IBD, true);
		}

	} else if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
		/* Set table->tablespace from either
		fil_system or SYS_TABLESPACES */
		dict_get_and_save_space_name(table, true);

		/* Set the filepath from either
		fil_system or SYS_DATAFILES. */
		filepath = dict_get_first_path(table->space);
		if (filepath == NULL) {
			ib::warn() << "Could not find the filepath"
				" for table " << table->name <<
				", space ID " << table->space;
		}
	}

	/* Try to open the tablespace.  We set the 2nd param (fix_dict) to
	false because we do not have an x-lock on dict_operation_lock */
	ulint fsp_flags = dict_tf_to_fsp_flags(table->flags,
					       false,
					       dict_table_is_encrypted(table));
	dberr_t err = fil_ibd_open(
		true, false, FIL_TYPE_TABLESPACE, table->space,
		fsp_flags, space_name, filepath);

	if (err != DB_SUCCESS) {
		/* We failed to find a sensible tablespace file */
		table->ibd_file_missing = TRUE;
	}

	ut_free(shared_space_name);
	ut_free(filepath);
}

/** Loads a table definition and also all its index definitions.

Loads those foreign key constraints whose referenced table is already in
dictionary cache.  If a foreign key constraint is not loaded, then the
referenced table is pushed into the output stack (fk_tables), if it is not
NULL.  These tables must be subsequently loaded so that all the foreign
key constraints are loaded into memory.

@param[in]	name		Table name in the db/tablename format
@param[in]	cached		true=add to cache, false=do not
@param[in]	ignore_err	Error to be ignored when loading table
				and its index definition
@param[out]	fk_tables	Related table names that must also be
				loaded to ensure that all foreign key
				constraints are loaded.
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the
ibd_file_missing flag TRUE in the table object we return */
static
dict_table_t*
dict_load_table_one(
	table_name_t&		name,
	bool			cached,
	dict_err_ignore_t	ignore_err,
	dict_names_t&		fk_tables)
{
	dberr_t		err;
	dict_table_t*	table;
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	const char*	err_msg;
	mtr_t		mtr;

	DBUG_ENTER("dict_load_table_one");
	DBUG_PRINT("dict_load_table_one", ("table: %s", name.m_name));

	ut_ad(mutex_own(&dict_sys->mutex));

	heap = mem_heap_create(32000);

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_ad(!dict_table_is_comp(sys_tables));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__ID, "ID"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__N_COLS, "N_COLS"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__TYPE, "TYPE"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__MIX_LEN, "MIX_LEN"));
	ut_ad(name_of_col_is(sys_tables, sys_index,
			     DICT_FLD__SYS_TABLES__SPACE, "SPACE"));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name.m_name, ut_strlen(name.m_name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)
	    || rec_get_deleted_flag(rec, 0)) {
		/* Not found */
err_exit:
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		DBUG_RETURN(NULL);
	}

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_TABLES__NAME, &len);

	/* Check if the table name in record is the searched one */
	if (len != ut_strlen(name.m_name)
	    || 0 != ut_memcmp(name.m_name, field, len)) {

		goto err_exit;
	}

	err_msg = dict_load_table_low(name, rec, &table);

	if (err_msg) {

		ib::error() << err_msg;
		goto err_exit;
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_tablespace(table, heap, ignore_err);

	dict_load_columns(table, heap);

	dict_load_virtual(table, heap);

	if (cached) {
		dict_table_add_to_cache(table, TRUE, heap);
	} else {
		dict_table_add_system_columns(table, heap);
	}

	mem_heap_empty(heap);

	/* If there is no tablespace for the table then we only need to
	load the index definitions. So that we can IMPORT the tablespace
	later. When recovering table locks for resurrected incomplete
	transactions, the tablespace should exist, because DDL operations
	were not allowed while the table is being locked by a transaction. */
	dict_err_ignore_t index_load_err =
		!(ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK)
		&& table->ibd_file_missing
		? DICT_ERR_IGNORE_ALL
		: ignore_err;
	err = dict_load_indexes(table, heap, index_load_err);

	if (err == DB_INDEX_CORRUPT) {
		/* Refuse to load the table if the table has a corrupted
		cluster index */
		if (!srv_load_corrupted) {

			ib::error() << "Load table " << table->name
				<< " failed, the table has"
				" corrupted clustered indexes. Turn on"
				" 'innodb_force_load_corrupted' to drop it";
			dict_table_remove_from_cache(table);
			table = NULL;
			goto func_exit;
		} else {
			dict_index_t*	clust_index;
			clust_index = dict_table_get_first_index(table);

			if (dict_index_is_corrupted(clust_index)) {
				table->corrupted = TRUE;
			}
		}
	}

	/* We don't trust the table->flags2(retrieved from SYS_TABLES.MIX_LEN
	field) if the datafiles are from 3.23.52 version. To identify this
	version, we do the below check and reset the flags. */
	if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)
	    && table->space == srv_sys_space.space_id()
	    && table->flags == 0) {
		table->flags2 = 0;
	}

	DBUG_EXECUTE_IF("ib_table_invalid_flags",
			if(strcmp(table->name.m_name, "test/t1") == 0) {
				table->flags2 = 255;
				table->flags = 255;
			});

	if (!dict_tf2_is_valid(table->flags, table->flags2)) {
		ib::error() << "Table " << table->name << " in InnoDB"
			" data dictionary contains invalid flags."
			" SYS_TABLES.MIX_LEN=" << table->flags2;
		table->flags2 &= ~(DICT_TF2_TEMPORARY|DICT_TF2_INTRINSIC);
		dict_table_remove_from_cache(table);
		table = NULL;
		err = DB_FAIL;
		goto func_exit;
	}

	/* Initialize table foreign_child value. Its value could be
	changed when dict_load_foreigns() is called below */
	table->fk_max_recusive_level = 0;

	/* If the force recovery flag is set, we open the table irrespective
	of the error condition, since the user may want to dump data from the
	clustered index. However we load the foreign key information only if
	all indexes were loaded. */
	if (!cached || table->ibd_file_missing) {
		/* Don't attempt to load the indexes from disk. */
	} else if (err == DB_SUCCESS) {
		err = dict_load_foreigns(table->name.m_name, NULL,
					 true, true,
					 ignore_err, fk_tables);

		if (err != DB_SUCCESS) {
			ib::warn() << "Load table " << table->name
				<< " failed, the table has missing"
				" foreign key indexes. Turn off"
				" 'foreign_key_checks' and try again.";

			dict_table_remove_from_cache(table);
			table = NULL;
		} else {
			dict_mem_table_fill_foreign_vcol_set(table);
			table->fk_max_recusive_level = 0;
		}
	} else {
		dict_index_t*   index;

		/* Make sure that at least the clustered index was loaded.
		Otherwise refuse to load the table */
		index = dict_table_get_first_index(table);

		if (!srv_force_recovery
		    || !index
		    || !dict_index_is_clust(index)) {

			dict_table_remove_from_cache(table);
			table = NULL;

		} else if (dict_index_is_corrupted(index)
			   && !table->ibd_file_missing) {

			/* It is possible we force to load a corrupted
			clustered index if srv_load_corrupted is set.
			Mark the table as corrupted in this case */
			table->corrupted = TRUE;
		}
	}

func_exit:
	mem_heap_free(heap);

	ut_ad(!table
	      || ignore_err != DICT_ERR_IGNORE_NONE
	      || table->ibd_file_missing
	      || !table->corrupted);

	if (table && table->fts) {
		if (!(dict_table_has_fts_index(table)
		      || DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)
		      || DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID))) {
			/* the table->fts could be created in dict_load_column
			when a user defined FTS_DOC_ID is present, but no
			FTS */
			fts_optimize_remove_table(table);
			fts_free(table);
		} else {
			fts_optimize_add_table(table);
		}
	}

	ut_ad(err != DB_SUCCESS || dict_foreign_set_validate(*table));

	DBUG_RETURN(table);
}

/***********************************************************************//**
Loads a table object based on the table id.
@return table; NULL if table does not exist */
dict_table_t*
dict_load_table_on_id(
/*==================*/
	table_id_t		table_id,	/*!< in: table id */
	dict_err_ignore_t	ignore_err)	/*!< in: errors to ignore
						when loading the table */
{
	byte		id_buf[8];
	btr_pcur_t	pcur;
	mem_heap_t*	heap;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sys_table_ids;
	dict_table_t*	sys_tables;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	dict_table_t*	table;
	mtr_t		mtr;

	ut_ad(mutex_own(&dict_sys->mutex));

	table = NULL;

	/* NOTE that the operation of this function is protected by
	the dictionary mutex, and therefore no deadlocks can occur
	with other dictionary operations. */

	mtr_start(&mtr);
	/*---------------------------------------------------*/
	/* Get the secondary index based on ID for table SYS_TABLES */
	sys_tables = dict_sys->sys_tables;
	sys_table_ids = dict_table_get_next_index(
		dict_table_get_first_index(sys_tables));
	ut_ad(!dict_table_is_comp(sys_tables));
	ut_ad(!dict_index_is_clust(sys_table_ids));
	heap = mem_heap_create(256);

	tuple  = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	/* Write the table id in byte format to id_buf */
	mach_write_to_8(id_buf, table_id);

	dfield_set_data(dfield, id_buf, 8);
	dict_index_copy_types(tuple, sys_table_ids, 1);

	btr_pcur_open_on_user_rec(sys_table_ids, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (page_rec_is_user_rec(rec)) {
		/*---------------------------------------------------*/
		/* Now we have the record in the secondary index
		containing the table ID and NAME */
check_rec:
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLE_IDS__ID, &len);
		ut_ad(len == 8);

		/* Check if the table id in record is the one searched for */
		if (table_id == mach_read_from_8(field)) {
			if (rec_get_deleted_flag(rec, 0)) {
				/* Until purge has completed, there
				may be delete-marked duplicate records
				for the same SYS_TABLES.ID, but different
				SYS_TABLES.NAME. */
				while (btr_pcur_move_to_next(&pcur, &mtr)) {
					rec = btr_pcur_get_rec(&pcur);

					if (page_rec_is_user_rec(rec)) {
						goto check_rec;
					}
				}
			} else {
				/* Now we get the table name from the record */
				field = rec_get_nth_field_old(rec,
					DICT_FLD__SYS_TABLE_IDS__NAME, &len);
				/* Load the table definition to memory */
				char*	table_name = mem_heap_strdupl(
					heap, (char*) field, len);
				table = dict_load_table(table_name, true, ignore_err);
			}
		}
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(table);
}

/********************************************************************//**
This function is called when the database is booted. Loads system table
index definitions except for the clustered index which is added to the
dictionary cache at booting before calling this function. */
void
dict_load_sys_table(
/*================*/
	dict_table_t*	table)	/*!< in: system table */
{
	mem_heap_t*	heap;

	ut_ad(mutex_own(&dict_sys->mutex));

	heap = mem_heap_create(1000);

	dict_load_indexes(table, heap, DICT_ERR_IGNORE_NONE);

	mem_heap_free(heap);
}

/********************************************************************//**
Loads foreign key constraint col names (also for the referenced table).
Members that must be set (and valid) in foreign:
foreign->heap
foreign->n_fields
foreign->id ('\0'-terminated)
Members that will be created and set by this function:
foreign->foreign_col_names[i]
foreign->referenced_col_names[i]
(for i=0..foreign->n_fields-1) */
static
void
dict_load_foreign_cols(
/*===================*/
	dict_foreign_t*	foreign)/*!< in/out: foreign constraint object */
{
	dict_table_t*	sys_foreign_cols;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		i;
	mtr_t		mtr;
	size_t		id_len;

	ut_ad(mutex_own(&dict_sys->mutex));

	id_len = strlen(foreign->id);

	foreign->foreign_col_names = static_cast<const char**>(
		mem_heap_alloc(foreign->heap,
			       foreign->n_fields * sizeof(void*)));

	foreign->referenced_col_names = static_cast<const char**>(
		mem_heap_alloc(foreign->heap,
			       foreign->n_fields * sizeof(void*)));

	mtr_start(&mtr);

	sys_foreign_cols = dict_table_get_low("SYS_FOREIGN_COLS");

	sys_index = UT_LIST_GET_FIRST(sys_foreign_cols->indexes);
	ut_ad(!dict_table_is_comp(sys_foreign_cols));

	tuple = dtuple_create(foreign->heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, foreign->id, id_len);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i < foreign->n_fields; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));
		ut_a(!rec_get_deleted_flag(rec, 0));

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__ID, &len);

		if (len != id_len || ut_memcmp(foreign->id, field, len) != 0) {
			const rec_t*	pos;
			ulint		pos_len;
			const rec_t*	for_col_name;
			ulint		for_col_name_len;
			const rec_t*	ref_col_name;
			ulint		ref_col_name_len;

			pos = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_FOREIGN_COLS__POS,
				&pos_len);

			for_col_name = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME,
				&for_col_name_len);

			ref_col_name = rec_get_nth_field_old(
				rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME,
				&ref_col_name_len);

			ib::fatal	sout;

			sout << "Unable to load column names for foreign"
				" key '" << foreign->id
				<< "' because it was not found in"
				" InnoDB internal table SYS_FOREIGN_COLS. The"
				" closest entry we found is:"
				" (ID='";
			sout.write(field, len);
			sout << "', POS=" << mach_read_from_4(pos)
				<< ", FOR_COL_NAME='";
			sout.write(for_col_name, for_col_name_len);
			sout << "', REF_COL_NAME='";
			sout.write(ref_col_name, ref_col_name_len);
			sout << "')";
		}

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__POS, &len);
		ut_a(len == 4);
		ut_a(i == mach_read_from_4(field));

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME, &len);
		foreign->foreign_col_names[i] = mem_heap_strdupl(
			foreign->heap, (char*) field, len);

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME, &len);
		foreign->referenced_col_names[i] = mem_heap_strdupl(
			foreign->heap, (char*) field, len);

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/***********************************************************************//**
Loads a foreign key constraint to the dictionary cache. If the referenced
table is not yet loaded, it is added in the output parameter (fk_tables).
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull(1), warn_unused_result))
dberr_t
dict_load_foreign(
/*==============*/
	const char*		id,
				/*!< in: foreign constraint id, must be
				'\0'-terminated */
	const char**		col_names,
				/*!< in: column names, or NULL
				to use foreign->foreign_table->col_names */
	bool			check_recursive,
				/*!< in: whether to record the foreign table
				parent count to avoid unlimited recursive
				load of chained foreign tables */
	bool			check_charsets,
				/*!< in: whether to check charset
				compatibility */
	dict_err_ignore_t	ignore_err,
				/*!< in: error to be ignored */
	dict_names_t&	fk_tables)
				/*!< out: the foreign key constraint is added
				to the dictionary cache only if the referenced
				table is already in cache.  Otherwise, the
				foreign key constraint is not added to cache,
				and the referenced table is added to this
				stack. */
{
	dict_foreign_t*	foreign;
	dict_table_t*	sys_foreign;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap2;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		n_fields_and_type;
	mtr_t		mtr;
	dict_table_t*	for_table;
	dict_table_t*	ref_table;
	size_t		id_len;

	DBUG_ENTER("dict_load_foreign");
	DBUG_PRINT("dict_load_foreign",
		   ("id: '%s', check_recursive: %d", id, check_recursive));

	ut_ad(mutex_own(&dict_sys->mutex));

	id_len = strlen(id);

	heap2 = mem_heap_create(1000);

	mtr_start(&mtr);

	sys_foreign = dict_table_get_low("SYS_FOREIGN");

	sys_index = UT_LIST_GET_FIRST(sys_foreign->indexes);
	ut_ad(!dict_table_is_comp(sys_foreign));

	tuple = dtuple_create(heap2, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, id_len);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)
	    || rec_get_deleted_flag(rec, 0)) {
		/* Not found */

		ib::error() << "Cannot load foreign constraint " << id
			<< ": could not find the relevant record in "
			<< "SYS_FOREIGN";

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);

		DBUG_RETURN(DB_ERROR);
	}

	field = rec_get_nth_field_old(rec, DICT_FLD__SYS_FOREIGN__ID, &len);

	/* Check if the id in record is the searched one */
	if (len != id_len || ut_memcmp(id, field, len) != 0) {

		{
			ib::error	err;
			err << "Cannot load foreign constraint " << id
				<< ": found ";
			err.write(field, len);
			err << " instead in SYS_FOREIGN";
		}

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);

		DBUG_RETURN(DB_ERROR);
	}

	/* Read the table names and the number of columns associated
	with the constraint */

	mem_heap_free(heap2);

	foreign = dict_mem_foreign_create();

	n_fields_and_type = mach_read_from_4(
		rec_get_nth_field_old(
			rec, DICT_FLD__SYS_FOREIGN__N_COLS, &len));

	ut_a(len == 4);

	/* We store the type in the bits 24..29 of n_fields_and_type. */

	foreign->type = (unsigned int) (n_fields_and_type >> 24);
	foreign->n_fields = (unsigned int) (n_fields_and_type & 0x3FFUL);

	foreign->id = mem_heap_strdupl(foreign->heap, id, id_len);

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__FOR_NAME, &len);

	foreign->foreign_table_name = mem_heap_strdupl(
		foreign->heap, (char*) field, len);
	dict_mem_foreign_table_name_lookup_set(foreign, TRUE);

	const ulint foreign_table_name_len = len;

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN__REF_NAME, &len);
	foreign->referenced_table_name = mem_heap_strdupl(
		foreign->heap, (char*) field, len);
	dict_mem_referenced_table_name_lookup_set(foreign, TRUE);

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_foreign_cols(foreign);

	ref_table = dict_table_check_if_in_cache_low(
		foreign->referenced_table_name_lookup);
	for_table = dict_table_check_if_in_cache_low(
		foreign->foreign_table_name_lookup);

	if (!for_table) {
		/* To avoid recursively loading the tables related through
		the foreign key constraints, the child table name is saved
		here.  The child table will be loaded later, along with its
		foreign key constraint. */

		lint	old_size = mem_heap_get_size(ref_table->heap);

		ut_a(ref_table != NULL);
		fk_tables.push_back(
			mem_heap_strdupl(ref_table->heap,
					 foreign->foreign_table_name_lookup,
					 foreign_table_name_len));

		lint	new_size = mem_heap_get_size(ref_table->heap);
		dict_sys->size += new_size - old_size;

		dict_foreign_remove_from_cache(foreign);
		DBUG_RETURN(DB_SUCCESS);
	}

	ut_a(for_table || ref_table);

	/* Note that there may already be a foreign constraint object in
	the dictionary cache for this constraint: then the following
	call only sets the pointers in it to point to the appropriate table
	and index objects and frees the newly created object foreign.
	Adding to the cache should always succeed since we are not creating
	a new foreign key constraint but loading one from the data
	dictionary. */

	DBUG_RETURN(dict_foreign_add_to_cache(foreign, col_names,
					      check_charsets,
					      ignore_err));
}

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
	dict_names_t&		fk_tables)
						/*!< out: stack of table
						names which must be loaded
						subsequently to load all the
						foreign key constraints. */
{
	ulint		tuple_buf[(DTUPLE_EST_ALLOC(1) + sizeof(ulint) - 1)
				/ sizeof(ulint)];
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sec_index;
	dict_table_t*	sys_foreign;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	dberr_t		err;
	mtr_t		mtr;

	DBUG_ENTER("dict_load_foreigns");

	ut_ad(mutex_own(&dict_sys->mutex));

	sys_foreign = dict_table_get_low("SYS_FOREIGN");

	if (sys_foreign == NULL) {
		/* No foreign keys defined yet in this database */

		ib::info() << "No foreign key system tables in the database";
		DBUG_RETURN(DB_ERROR);
	}

	ut_ad(!dict_table_is_comp(sys_foreign));
	mtr_start(&mtr);

	/* Get the secondary index based on FOR_NAME from table
	SYS_FOREIGN */

	sec_index = dict_table_get_next_index(
		dict_table_get_first_index(sys_foreign));
	ut_ad(!dict_index_is_clust(sec_index));
start_load:

	tuple = dtuple_create_from_mem(tuple_buf, sizeof(tuple_buf), 1, 0);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, table_name, ut_strlen(table_name));
	dict_index_copy_types(tuple, sec_index, 1);

	btr_pcur_open_on_user_rec(sec_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
loop:
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* End of index */

		goto load_next_index;
	}

	/* Now we have the record in the secondary index containing a table
	name and a foreign constraint ID */

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_FOR_NAME__NAME, &len);

	/* Check if the table name in the record is the one searched for; the
	following call does the comparison in the latin1_swedish_ci
	charset-collation, in a case-insensitive way. */

	if (0 != cmp_data_data(dfield_get_type(dfield)->mtype,
			       dfield_get_type(dfield)->prtype,
			       static_cast<const byte*>(
				       dfield_get_data(dfield)),
			       dfield_get_len(dfield),
			       field, len)) {

		goto load_next_index;
	}

	/* Since table names in SYS_FOREIGN are stored in a case-insensitive
	order, we have to check that the table name matches also in a binary
	string comparison. On Unix, MySQL allows table names that only differ
	in character case.  If lower_case_table_names=2 then what is stored
	may not be the same case, but the previous comparison showed that they
	match with no-case.  */

	if (rec_get_deleted_flag(rec, 0)) {
		goto next_rec;
	}

	if ((innobase_get_lower_case_table_names() != 2)
	    && (0 != ut_memcmp(field, table_name, len))) {
		goto next_rec;
	}

	/* Now we get a foreign key constraint id */
	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_FOREIGN_FOR_NAME__ID, &len);

	/* Copy the string because the page may be modified or evicted
	after mtr_commit() below. */
	char	fk_id[MAX_TABLE_NAME_LEN + 1];

	ut_a(len <= MAX_TABLE_NAME_LEN);
	memcpy(fk_id, field, len);
	fk_id[len] = '\0';

	btr_pcur_store_position(&pcur, &mtr);

	mtr_commit(&mtr);

	/* Load the foreign constraint definition to the dictionary cache */

	err = dict_load_foreign(fk_id, col_names,
				check_recursive, check_charsets, ignore_err,
				fk_tables);

	if (err != DB_SUCCESS) {
		btr_pcur_close(&pcur);

		DBUG_RETURN(err);
	}

	mtr_start(&mtr);

	btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
next_rec:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	goto loop;

load_next_index:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	sec_index = dict_table_get_next_index(sec_index);

	if (sec_index != NULL) {

		mtr_start(&mtr);

		/* Switch to scan index on REF_NAME, fk_max_recusive_level
		already been updated when scanning FOR_NAME index, no need to
		update again */
		check_recursive = FALSE;

		goto start_load;
	}

	DBUG_RETURN(DB_SUCCESS);
}
