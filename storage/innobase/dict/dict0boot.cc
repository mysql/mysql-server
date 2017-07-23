/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file dict/dict0boot.cc
Data dictionary creation and booting

Created 4/18/1996 Heikki Tuuri
*******************************************************/

#include "ha_prototypes.h"

#include "dict0boot.h"

#ifdef UNIV_NONINL
#include "dict0boot.ic"
#endif

#include "dict0crea.h"
#include "btr0btr.h"
#include "dict0load.h"
#include "trx0trx.h"
#include "srv0srv.h"
#include "ibuf0ibuf.h"
#include "buf0flu.h"
#include "log0recv.h"
#include "os0file.h"

/**********************************************************************//**
Gets a pointer to the dictionary header and x-latches its page.
@return pointer to the dictionary header, page x-latched */
dict_hdr_t*
dict_hdr_get(
/*=========*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	buf_block_t*	block;
	dict_hdr_t*	header;

	block = buf_page_get(page_id_t(DICT_HDR_SPACE, DICT_HDR_PAGE_NO),
			     univ_page_size, RW_X_LATCH, mtr);
	header = DICT_HDR + buf_block_get_frame(block);

	buf_block_dbg_add_level(block, SYNC_DICT_HEADER);

	return(header);
}

/**********************************************************************//**
Returns a new table, index, or space id. */
void
dict_hdr_get_new_id(
/*================*/
	table_id_t*		table_id,	/*!< out: table id
						(not assigned if NULL) */
	index_id_t*		index_id,	/*!< out: index id
						(not assigned if NULL) */
	ulint*			space_id,	/*!< out: space id
						(not assigned if NULL) */
	const dict_table_t*	table,		/*!< in: table */
	bool			disable_redo)	/*!< in: if true and table
						object is NULL
						then disable-redo */
{
	dict_hdr_t*	dict_hdr;
	ib_id_t		id;
	mtr_t		mtr;

	mtr_start(&mtr);
	if (table) {
		dict_disable_redo_if_temporary(table, &mtr);
	} else if (disable_redo) {
		/* In non-read-only mode we need to ensure that space-id header
		page is written to disk else if page is removed from buffer
		cache and re-loaded it would assign temporary tablespace id
		to another tablespace.
		This is not a case with read-only mode as there is no new object
		that is created except temporary tablespace. */
		mtr_set_log_mode(&mtr,
			(srv_read_only_mode ? MTR_LOG_NONE : MTR_LOG_NO_REDO));
	}

	/* Server started and let's say space-id = x
	- table created with file-per-table
	- space-id = x + 1
	- crash
	Case 1: If it was redo logged then we know that it will be
		restored to x + 1
	Case 2: if not redo-logged
		Header will have the old space-id = x
		This is OK because on restart there is no object with
		space id = x + 1
	Case 3:
		space-id = x (on start)
		space-id = x+1 (temp-table allocation) - no redo logging
		space-id = x+2 (non-temp-table allocation), this get's
			   redo logged.
		If there is a crash there will be only 2 entries
		x (original) and x+2 (new) and disk hdr will be updated
		to reflect x + 2 entry.
		We cannot allocate the same space id to different objects. */
	dict_hdr = dict_hdr_get(&mtr);

	if (table_id) {
		id = mach_read_from_8(dict_hdr + DICT_HDR_TABLE_ID);
		id++;
		mlog_write_ull(dict_hdr + DICT_HDR_TABLE_ID, id, &mtr);
		*table_id = id;
	}

	if (index_id) {
		id = mach_read_from_8(dict_hdr + DICT_HDR_INDEX_ID);
		id++;
		mlog_write_ull(dict_hdr + DICT_HDR_INDEX_ID, id, &mtr);
		*index_id = id;
	}

	if (space_id) {
		*space_id = mtr_read_ulint(dict_hdr + DICT_HDR_MAX_SPACE_ID,
					   MLOG_4BYTES, &mtr);
		if (fil_assign_new_space_id(space_id)) {
			mlog_write_ulint(dict_hdr + DICT_HDR_MAX_SPACE_ID,
					 *space_id, MLOG_4BYTES, &mtr);
		}
	}

	mtr_commit(&mtr);
}

/**********************************************************************//**
Writes the current value of the row id counter to the dictionary header file
page. */
void
dict_hdr_flush_row_id(void)
/*=======================*/
{
	dict_hdr_t*	dict_hdr;
	row_id_t	id;
	mtr_t		mtr;

	ut_ad(mutex_own(&dict_sys->mutex));

	id = dict_sys->row_id;

	mtr_start(&mtr);

	dict_hdr = dict_hdr_get(&mtr);

	mlog_write_ull(dict_hdr + DICT_HDR_ROW_ID, id, &mtr);

	mtr_commit(&mtr);
}

/*****************************************************************//**
Creates the file page for the dictionary header. This function is
called only at the database creation.
@return TRUE if succeed */
static
ibool
dict_hdr_create(
/*============*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	buf_block_t*	block;
	dict_hdr_t*	dict_header;
	ulint		root_page_no;

	ut_ad(mtr);

	/* Create the dictionary header file block in a new, allocated file
	segment in the system tablespace */
	block = fseg_create(DICT_HDR_SPACE, 0,
			    DICT_HDR + DICT_HDR_FSEG_HEADER, mtr);

	ut_a(DICT_HDR_PAGE_NO == block->page.id.page_no());

	dict_header = dict_hdr_get(mtr);

	/* Start counting row, table, index, and tree ids from
	DICT_HDR_FIRST_ID */
	mlog_write_ull(dict_header + DICT_HDR_ROW_ID,
		       DICT_HDR_FIRST_ID, mtr);

	mlog_write_ull(dict_header + DICT_HDR_TABLE_ID,
		       DICT_HDR_FIRST_ID, mtr);

	mlog_write_ull(dict_header + DICT_HDR_INDEX_ID,
		       DICT_HDR_FIRST_ID, mtr);

	mlog_write_ulint(dict_header + DICT_HDR_MAX_SPACE_ID,
			 0, MLOG_4BYTES, mtr);

	/* Obsolete, but we must initialize it anyway. */
	mlog_write_ulint(dict_header + DICT_HDR_MIX_ID_LOW,
			 DICT_HDR_FIRST_ID, MLOG_4BYTES, mtr);

	/* Create the B-tree roots for the clustered indexes of the basic
	system tables */

	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE, DICT_HDR_SPACE,
				  univ_page_size, DICT_TABLES_ID,
				  dict_ind_redundant, NULL, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_TABLES, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_UNIQUE, DICT_HDR_SPACE,
				  univ_page_size, DICT_TABLE_IDS_ID,
				  dict_ind_redundant, NULL, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_TABLE_IDS, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE, DICT_HDR_SPACE,
				  univ_page_size, DICT_COLUMNS_ID,
				  dict_ind_redundant, NULL, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_COLUMNS, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE, DICT_HDR_SPACE,
				  univ_page_size, DICT_INDEXES_ID,
				  dict_ind_redundant, NULL, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_INDEXES, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE, DICT_HDR_SPACE,
				  univ_page_size, DICT_FIELDS_ID,
				  dict_ind_redundant, NULL, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_FIELDS, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/

	return(TRUE);
}

/*****************************************************************//**
Initializes the data dictionary memory structures when the database is
started. This function is also called when the data dictionary is created.
@return DB_SUCCESS or error code. */
dberr_t
dict_boot(void)
/*===========*/
{
	dict_table_t*	table;
	dict_index_t*	index;
	dict_hdr_t*	dict_hdr;
	mem_heap_t*	heap;
	mtr_t		mtr;
	dberr_t		error;

	/* Be sure these constants do not ever change.  To avoid bloat,
	only check the *NUM_FIELDS* in each table */

	ut_ad(DICT_NUM_COLS__SYS_TABLES == 8);
	ut_ad(DICT_NUM_FIELDS__SYS_TABLES == 10);
	ut_ad(DICT_NUM_FIELDS__SYS_TABLE_IDS == 2);
	ut_ad(DICT_NUM_COLS__SYS_COLUMNS == 7);
	ut_ad(DICT_NUM_FIELDS__SYS_COLUMNS == 9);
	ut_ad(DICT_NUM_COLS__SYS_INDEXES == 8);
	ut_ad(DICT_NUM_FIELDS__SYS_INDEXES == 10);
	ut_ad(DICT_NUM_COLS__SYS_FIELDS == 3);
	ut_ad(DICT_NUM_FIELDS__SYS_FIELDS == 5);
	ut_ad(DICT_NUM_COLS__SYS_FOREIGN == 4);
	ut_ad(DICT_NUM_FIELDS__SYS_FOREIGN == 6);
	ut_ad(DICT_NUM_FIELDS__SYS_FOREIGN_FOR_NAME == 2);
	ut_ad(DICT_NUM_COLS__SYS_FOREIGN_COLS == 4);
	ut_ad(DICT_NUM_FIELDS__SYS_FOREIGN_COLS == 6);

	mtr_start(&mtr);

	/* Create the hash tables etc. */
	dict_init();

	heap = mem_heap_create(450);

	mutex_enter(&dict_sys->mutex);

	/* Get the dictionary header */
	dict_hdr = dict_hdr_get(&mtr);

	/* Because we only write new row ids to disk-based data structure
	(dictionary header) when it is divisible by
	DICT_HDR_ROW_ID_WRITE_MARGIN, in recovery we will not recover
	the latest value of the row id counter. Therefore we advance
	the counter at the database startup to avoid overlapping values.
	Note that when a user after database startup first time asks for
	a new row id, then because the counter is now divisible by
	..._MARGIN, it will immediately be updated to the disk-based
	header. */

	dict_sys->row_id = DICT_HDR_ROW_ID_WRITE_MARGIN
		+ ut_uint64_align_up(mach_read_from_8(dict_hdr + DICT_HDR_ROW_ID),
				     DICT_HDR_ROW_ID_WRITE_MARGIN);

	/* Insert into the dictionary cache the descriptions of the basic
	system tables */
	/*-------------------------*/
	table = dict_mem_table_create("SYS_TABLES", DICT_HDR_SPACE, 8, 0, 0, 0);

	dict_mem_table_add_col(table, heap, "NAME", DATA_BINARY, 0,
			       MAX_FULL_NAME_LEN);
	dict_mem_table_add_col(table, heap, "ID", DATA_BINARY, 0, 8);
	/* ROW_FORMAT = (N_COLS >> 31) ? COMPACT : REDUNDANT */
	dict_mem_table_add_col(table, heap, "N_COLS", DATA_INT, 0, 4);
	/* The low order bit of TYPE is always set to 1.  If the format
	is UNIV_FORMAT_B or higher, this field matches table->flags. */
	dict_mem_table_add_col(table, heap, "TYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "MIX_ID", DATA_BINARY, 0, 0);
	/* MIX_LEN may contain additional table flags when
	ROW_FORMAT!=REDUNDANT.  Currently, these flags include
	DICT_TF2_TEMPORARY. */
	dict_mem_table_add_col(table, heap, "MIX_LEN", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "CLUSTER_NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "SPACE", DATA_INT, 0, 4);

	table->id = DICT_TABLES_ID;

	dict_table_add_to_cache(table, FALSE, heap);
	dict_sys->sys_tables = table;
	mem_heap_empty(heap);

	index = dict_mem_index_create("SYS_TABLES", "CLUST_IND",
				      DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 1);

	dict_mem_index_add_field(index, "NAME", 0);

	index->id = DICT_TABLES_ID;

	error = dict_index_add_to_cache(table, index,
					mtr_read_ulint(dict_hdr
						       + DICT_HDR_TABLES,
						       MLOG_4BYTES, &mtr),
					FALSE);
	ut_a(error == DB_SUCCESS);

	/*-------------------------*/
	index = dict_mem_index_create("SYS_TABLES", "ID_IND",
				      DICT_HDR_SPACE, DICT_UNIQUE, 1);
	dict_mem_index_add_field(index, "ID", 0);

	index->id = DICT_TABLE_IDS_ID;
	error = dict_index_add_to_cache(table, index,
					mtr_read_ulint(dict_hdr
						       + DICT_HDR_TABLE_IDS,
						       MLOG_4BYTES, &mtr),
					FALSE);
	ut_a(error == DB_SUCCESS);

	/*-------------------------*/
	table = dict_mem_table_create("SYS_COLUMNS", DICT_HDR_SPACE,
				      7, 0, 0, 0);

	dict_mem_table_add_col(table, heap, "TABLE_ID", DATA_BINARY, 0, 8);
	dict_mem_table_add_col(table, heap, "POS", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "MTYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "PRTYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "LEN", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "PREC", DATA_INT, 0, 4);

	table->id = DICT_COLUMNS_ID;

	dict_table_add_to_cache(table, FALSE, heap);
	dict_sys->sys_columns = table;
	mem_heap_empty(heap);

	index = dict_mem_index_create("SYS_COLUMNS", "CLUST_IND",
				      DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, "TABLE_ID", 0);
	dict_mem_index_add_field(index, "POS", 0);

	index->id = DICT_COLUMNS_ID;
	error = dict_index_add_to_cache(table, index,
					mtr_read_ulint(dict_hdr
						       + DICT_HDR_COLUMNS,
						       MLOG_4BYTES, &mtr),
					FALSE);
	ut_a(error == DB_SUCCESS);

	/*-------------------------*/
	table = dict_mem_table_create("SYS_INDEXES", DICT_HDR_SPACE,
				      DICT_NUM_COLS__SYS_INDEXES, 0, 0, 0);

	dict_mem_table_add_col(table, heap, "TABLE_ID", DATA_BINARY, 0, 8);
	dict_mem_table_add_col(table, heap, "ID", DATA_BINARY, 0, 8);
	dict_mem_table_add_col(table, heap, "NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "N_FIELDS", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "TYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "SPACE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "PAGE_NO", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "MERGE_THRESHOLD", DATA_INT, 0, 4);

	table->id = DICT_INDEXES_ID;

	dict_table_add_to_cache(table, FALSE, heap);
	dict_sys->sys_indexes = table;
	mem_heap_empty(heap);

	index = dict_mem_index_create("SYS_INDEXES", "CLUST_IND",
				      DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, "TABLE_ID", 0);
	dict_mem_index_add_field(index, "ID", 0);

	index->id = DICT_INDEXES_ID;
	error = dict_index_add_to_cache(table, index,
					mtr_read_ulint(dict_hdr
						       + DICT_HDR_INDEXES,
						       MLOG_4BYTES, &mtr),
					FALSE);
	ut_a(error == DB_SUCCESS);

	/*-------------------------*/
	table = dict_mem_table_create("SYS_FIELDS", DICT_HDR_SPACE, 3, 0, 0, 0);

	dict_mem_table_add_col(table, heap, "INDEX_ID", DATA_BINARY, 0, 8);
	dict_mem_table_add_col(table, heap, "POS", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "COL_NAME", DATA_BINARY, 0, 0);

	table->id = DICT_FIELDS_ID;

	dict_table_add_to_cache(table, FALSE, heap);
	dict_sys->sys_fields = table;
	mem_heap_free(heap);

	index = dict_mem_index_create("SYS_FIELDS", "CLUST_IND",
				      DICT_HDR_SPACE,
				      DICT_UNIQUE | DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, "INDEX_ID", 0);
	dict_mem_index_add_field(index, "POS", 0);

	index->id = DICT_FIELDS_ID;
	error = dict_index_add_to_cache(table, index,
					mtr_read_ulint(dict_hdr
						       + DICT_HDR_FIELDS,
						       MLOG_4BYTES, &mtr),
					FALSE);
	ut_a(error == DB_SUCCESS);

	mtr_commit(&mtr);

	/*-------------------------*/

	/* Initialize the insert buffer table and index for each tablespace */

	ibuf_init_at_db_start();

	dberr_t	err = DB_SUCCESS;

	/** If innodb_force_recovery is set to 6 then allow
	the innodb to start the server even though ibuf is not
	empty. */
	if (srv_force_recovery != SRV_FORCE_NO_LOG_REDO
	    && srv_read_only_mode && !ibuf_is_empty()) {

		ib::error() << "Change buffer must be empty when"
			" --innodb-read-only is set!";

		err = DB_ERROR;
	} else {
		/* Load definitions of other indexes on system tables */

		dict_load_sys_table(dict_sys->sys_tables);
		dict_load_sys_table(dict_sys->sys_columns);
		dict_load_sys_table(dict_sys->sys_indexes);
		dict_load_sys_table(dict_sys->sys_fields);
	}

	mutex_exit(&dict_sys->mutex);

	return(err);
}

/*****************************************************************//**
Inserts the basic system table data into themselves in the database
creation. */
static
void
dict_insert_initial_data(void)
/*==========================*/
{
	/* Does nothing yet */
}

/*****************************************************************//**
Creates and initializes the data dictionary at the server bootstrap.
@return DB_SUCCESS or error code. */
dberr_t
dict_create(void)
/*=============*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	dict_hdr_create(&mtr);

	mtr_commit(&mtr);

	dberr_t	err = dict_boot();

	if (err == DB_SUCCESS) {
		dict_insert_initial_data();
	}

	return(err);
}
