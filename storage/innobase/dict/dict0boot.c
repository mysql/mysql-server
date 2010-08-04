/*****************************************************************************

Copyright (c) 1996, 2010, Innobase Oy. All Rights Reserved.

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
@file dict/dict0boot.c
Data dictionary creation and booting

Created 4/18/1996 Heikki Tuuri
*******************************************************/

#include "dict0boot.h"

#ifdef UNIV_NONINL
#include "dict0boot.ic"
#endif

#include "dict0crea.h"
#include "btr0btr.h"
#include "dict0load.h"
#include "dict0load.h"
#include "trx0trx.h"
#include "srv0srv.h"
#include "ibuf0ibuf.h"
#include "buf0flu.h"
#include "log0recv.h"
#include "os0file.h"

/**********************************************************************//**
Gets a pointer to the dictionary header and x-latches its page.
@return	pointer to the dictionary header, page x-latched */
UNIV_INTERN
dict_hdr_t*
dict_hdr_get(
/*=========*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	buf_block_t*	block;
	dict_hdr_t*	header;

	block = buf_page_get(DICT_HDR_SPACE, 0, DICT_HDR_PAGE_NO,
			     RW_X_LATCH, mtr);
	header = DICT_HDR + buf_block_get_frame(block);

	buf_block_dbg_add_level(block, SYNC_DICT_HEADER);

	return(header);
}

/**********************************************************************//**
Returns a new table, index, or space id. */
UNIV_INTERN
void
dict_hdr_get_new_id(
/*================*/
	table_id_t*	table_id,	/*!< out: table id
					(not assigned if NULL) */
	index_id_t*	index_id,	/*!< out: index id
					(not assigned if NULL) */
	ulint*		space_id)	/*!< out: space id
					(not assigned if NULL) */
{
	dict_hdr_t*	dict_hdr;
	ib_id_t		id;
	mtr_t		mtr;

	mtr_start(&mtr);

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
UNIV_INTERN
void
dict_hdr_flush_row_id(void)
/*=======================*/
{
	dict_hdr_t*	dict_hdr;
	row_id_t	id;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	id = dict_sys->row_id;

	mtr_start(&mtr);

	dict_hdr = dict_hdr_get(&mtr);

	mlog_write_ull(dict_hdr + DICT_HDR_ROW_ID, id, &mtr);

	mtr_commit(&mtr);
}

/*****************************************************************//**
Creates the file page for the dictionary header. This function is
called only at the database creation.
@return	TRUE if succeed */
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

	ut_a(DICT_HDR_PAGE_NO == buf_block_get_page_no(block));

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
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				  DICT_HDR_SPACE, 0, DICT_TABLES_ID,
				  dict_ind_redundant, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_TABLES, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_UNIQUE, DICT_HDR_SPACE, 0,
				  DICT_TABLE_IDS_ID,
				  dict_ind_redundant, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_TABLE_IDS, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				  DICT_HDR_SPACE, 0, DICT_COLUMNS_ID,
				  dict_ind_redundant, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_COLUMNS, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				  DICT_HDR_SPACE, 0, DICT_INDEXES_ID,
				  dict_ind_redundant, mtr);
	if (root_page_no == FIL_NULL) {

		return(FALSE);
	}

	mlog_write_ulint(dict_header + DICT_HDR_INDEXES, root_page_no,
			 MLOG_4BYTES, mtr);
	/*--------------------------*/
	root_page_no = btr_create(DICT_CLUSTERED | DICT_UNIQUE,
				  DICT_HDR_SPACE, 0, DICT_FIELDS_ID,
				  dict_ind_redundant, mtr);
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
started. This function is also called when the data dictionary is created. */
UNIV_INTERN
void
dict_boot(void)
/*===========*/
{
	dict_table_t*	table;
	dict_index_t*	index;
	dict_hdr_t*	dict_hdr;
	mem_heap_t*	heap;
	mtr_t		mtr;
	ulint		error;

	mtr_start(&mtr);

	/* Create the hash tables etc. */
	dict_init();

	heap = mem_heap_create(450);

	mutex_enter(&(dict_sys->mutex));

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
	table = dict_mem_table_create("SYS_TABLES", DICT_HDR_SPACE, 8, 0);

	dict_mem_table_add_col(table, heap, "NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "ID", DATA_BINARY, 0, 0);
	/* ROW_FORMAT = (N_COLS >> 31) ? COMPACT : REDUNDANT */
	dict_mem_table_add_col(table, heap, "N_COLS", DATA_INT, 0, 4);
	/* TYPE is either DICT_TABLE_ORDINARY, or (TYPE & DICT_TF_COMPACT)
	and (TYPE & DICT_TF_FORMAT_MASK) are nonzero and TYPE = table->flags */
	dict_mem_table_add_col(table, heap, "TYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "MIX_ID", DATA_BINARY, 0, 0);
	/* MIX_LEN may contain additional table flags when
	ROW_FORMAT!=REDUNDANT.  Currently, these flags include
	DICT_TF2_TEMPORARY. */
	dict_mem_table_add_col(table, heap, "MIX_LEN", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "CLUSTER_NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "SPACE", DATA_INT, 0, 4);

	table->id = DICT_TABLES_ID;

	dict_table_add_to_cache(table, heap);
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
	table = dict_mem_table_create("SYS_COLUMNS", DICT_HDR_SPACE, 7, 0);

	dict_mem_table_add_col(table, heap, "TABLE_ID", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "POS", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "MTYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "PRTYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "LEN", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "PREC", DATA_INT, 0, 4);

	table->id = DICT_COLUMNS_ID;

	dict_table_add_to_cache(table, heap);
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
	table = dict_mem_table_create("SYS_INDEXES", DICT_HDR_SPACE, 7, 0);

	dict_mem_table_add_col(table, heap, "TABLE_ID", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "ID", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "NAME", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "N_FIELDS", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "TYPE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "SPACE", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "PAGE_NO", DATA_INT, 0, 4);

	/* The '+ 2' below comes from the fields DB_TRX_ID, DB_ROLL_PTR */
#if DICT_SYS_INDEXES_PAGE_NO_FIELD != 6 + 2
#error "DICT_SYS_INDEXES_PAGE_NO_FIELD != 6 + 2"
#endif
#if DICT_SYS_INDEXES_SPACE_NO_FIELD != 5 + 2
#error "DICT_SYS_INDEXES_SPACE_NO_FIELD != 5 + 2"
#endif
#if DICT_SYS_INDEXES_TYPE_FIELD != 4 + 2
#error "DICT_SYS_INDEXES_TYPE_FIELD != 4 + 2"
#endif
#if DICT_SYS_INDEXES_NAME_FIELD != 2 + 2
#error "DICT_SYS_INDEXES_NAME_FIELD != 2 + 2"
#endif

	table->id = DICT_INDEXES_ID;
	dict_table_add_to_cache(table, heap);
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
	table = dict_mem_table_create("SYS_FIELDS", DICT_HDR_SPACE, 3, 0);

	dict_mem_table_add_col(table, heap, "INDEX_ID", DATA_BINARY, 0, 0);
	dict_mem_table_add_col(table, heap, "POS", DATA_INT, 0, 4);
	dict_mem_table_add_col(table, heap, "COL_NAME", DATA_BINARY, 0, 0);

	table->id = DICT_FIELDS_ID;
	dict_table_add_to_cache(table, heap);
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

	/* Load definitions of other indexes on system tables */

	dict_load_sys_table(dict_sys->sys_tables);
	dict_load_sys_table(dict_sys->sys_columns);
	dict_load_sys_table(dict_sys->sys_indexes);
	dict_load_sys_table(dict_sys->sys_fields);

	mutex_exit(&(dict_sys->mutex));
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
Creates and initializes the data dictionary at the database creation. */
UNIV_INTERN
void
dict_create(void)
/*=============*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	dict_hdr_create(&mtr);

	mtr_commit(&mtr);

	dict_boot();

	dict_insert_initial_data();
}
