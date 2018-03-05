/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
@file dict/dict0crea.cc
Database object creation

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#include "ha_prototypes.h"

#include "dict0crea.h"

#ifdef UNIV_NONINL
#include "dict0crea.ic"
#endif

#include "btr0pcur.h"
#include "btr0btr.h"
#include "page0page.h"
#include "mach0data.h"
#include "dict0boot.h"
#include "dict0dict.h"
#include "que0que.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "pars0pars.h"
#include "trx0roll.h"
#include "usr0sess.h"
#include "ut0vec.h"
#include "dict0priv.h"
#include "fts0priv.h"
#include "fsp0space.h"
#include "fsp0sysspace.h"
#include "srv0start.h"

/*****************************************************************//**
Based on a table object, this function builds the entry to be inserted
in the SYS_TABLES system table.
@return the tuple which should be inserted */
static
dtuple_t*
dict_create_sys_tables_tuple(
/*=========================*/
	const dict_table_t*	table,	/*!< in: table */
	mem_heap_t*		heap)	/*!< in: memory heap from
					which the memory for the built
					tuple is allocated */
{
	dict_table_t*	sys_tables;
	dtuple_t*	entry;
	dfield_t*	dfield;
	byte*		ptr;
	ulint		type;

	ut_ad(table);
	ut_ad(heap);

	sys_tables = dict_sys->sys_tables;

	entry = dtuple_create(heap, 8 + DATA_N_SYS_COLS);

	dict_table_copy_types(entry, sys_tables);

	/* 0: NAME -----------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__NAME);

	dfield_set_data(dfield,
			table->name.m_name, strlen(table->name.m_name));

	/* 1: DB_TRX_ID added later */
	/* 2: DB_ROLL_PTR added later */
	/* 3: ID -------------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__ID);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);

	/* 4: N_COLS ---------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__N_COLS);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));

	/* If there is any virtual column, encode it in N_COLS */
	mach_write_to_4(ptr, dict_table_encode_n_col(
				static_cast<ulint>(table->n_def),
				static_cast<ulint>(table->n_v_def))
			| ((table->flags & DICT_TF_COMPACT) << 31));
	dfield_set_data(dfield, ptr, 4);

	/* 5: TYPE (table flags) -----------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__TYPE);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));

	/* Validate the table flags and convert them to what is saved in
	SYS_TABLES.TYPE.  Table flag values 0 and 1 are both written to
	SYS_TABLES.TYPE as 1. */
	type = dict_tf_to_sys_tables_type(table->flags);
	mach_write_to_4(ptr, type);

	dfield_set_data(dfield, ptr, 4);

	/* 6: MIX_ID (obsolete) ---------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__MIX_ID);

	ptr = static_cast<byte*>(mem_heap_zalloc(heap, 8));

	dfield_set_data(dfield, ptr, 8);

	/* 7: MIX_LEN (additional flags) --------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__MIX_LEN);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	/* Be sure all non-used bits are zero. */
	ut_a(!(table->flags2 & DICT_TF2_UNUSED_BIT_MASK));
	mach_write_to_4(ptr, table->flags2);

	dfield_set_data(dfield, ptr, 4);

	/* 8: CLUSTER_NAME ---------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__CLUSTER_ID);
	dfield_set_null(dfield); /* not supported */

	/* 9: SPACE ----------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_TABLES__SPACE);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, table->space);

	dfield_set_data(dfield, ptr, 4);
	/*----------------------------------*/

	return(entry);
}

/*****************************************************************//**
Based on a table object, this function builds the entry to be inserted
in the SYS_COLUMNS system table.
@return the tuple which should be inserted */
static
dtuple_t*
dict_create_sys_columns_tuple(
/*==========================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			i,	/*!< in: column number */
	mem_heap_t*		heap)	/*!< in: memory heap from
					which the memory for the built
					tuple is allocated */
{
	dict_table_t*		sys_columns;
	dtuple_t*		entry;
	const dict_col_t*	column;
	dfield_t*		dfield;
	byte*			ptr;
	const char*		col_name;
	ulint			num_base = 0;
	ulint			v_col_no = ULINT_UNDEFINED;

	ut_ad(table);
	ut_ad(heap);

	/* Any column beyond table->n_def would be virtual columns */
        if (i >= table->n_def) {
		dict_v_col_t*	v_col = dict_table_get_nth_v_col(
					table, i - table->n_def);
		column = &v_col->m_col;
		num_base = v_col->num_base;
		v_col_no = column->ind;
	} else {
		column = dict_table_get_nth_col(table, i);
		ut_ad(!dict_col_is_virtual(column));
	}

	sys_columns = dict_sys->sys_columns;

	entry = dtuple_create(heap, 7 + DATA_N_SYS_COLS);

	dict_table_copy_types(entry, sys_columns);

	/* 0: TABLE_ID -----------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__TABLE_ID);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);

	/* 1: POS ----------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__POS);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));

	if (v_col_no != ULINT_UNDEFINED) {
		/* encode virtual column's position in MySQL table and InnoDB
		table in "POS" */
		mach_write_to_4(ptr, dict_create_v_col_pos(
				i - table->n_def, v_col_no));
	} else {
		mach_write_to_4(ptr, i);
	}

	dfield_set_data(dfield, ptr, 4);

	/* 2: DB_TRX_ID added later */
	/* 3: DB_ROLL_PTR added later */
	/* 4: NAME ---------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__NAME);

        if (i >= table->n_def) {
		col_name = dict_table_get_v_col_name(table, i - table->n_def);
	} else {
		col_name = dict_table_get_col_name(table, i);
	}

	dfield_set_data(dfield, col_name, ut_strlen(col_name));

	/* 5: MTYPE --------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__MTYPE);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, column->mtype);

	dfield_set_data(dfield, ptr, 4);

	/* 6: PRTYPE -------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__PRTYPE);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, column->prtype);

	dfield_set_data(dfield, ptr, 4);

	/* 7: LEN ----------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__LEN);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, column->len);

	dfield_set_data(dfield, ptr, 4);

	/* 8: PREC ---------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_COLUMNS__PREC);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, num_base);

	dfield_set_data(dfield, ptr, 4);
	/*---------------------------------*/

	return(entry);
}

/** Based on a table object, this function builds the entry to be inserted
in the SYS_VIRTUAL system table. Each row maps a virtual column to one of
its base column.
@param[in]	table	table
@param[in]	v_col_n	virtual column number
@param[in]	b_col_n	base column sequence num
@param[in]	heap	memory heap
@return the tuple which should be inserted */
static
dtuple_t*
dict_create_sys_virtual_tuple(
	const dict_table_t*	table,
	ulint			v_col_n,
	ulint			b_col_n,
	mem_heap_t*		heap)
{
	dict_table_t*		sys_virtual;
	dtuple_t*		entry;
	const dict_col_t*	base_column;
	dfield_t*		dfield;
	byte*			ptr;

	ut_ad(table);
	ut_ad(heap);

	ut_ad(v_col_n < table->n_v_def);
	dict_v_col_t*	v_col = dict_table_get_nth_v_col(table, v_col_n);
	base_column = v_col->base_col[b_col_n];

	sys_virtual = dict_sys->sys_virtual;

	entry = dtuple_create(heap, DICT_NUM_COLS__SYS_VIRTUAL
			      + DATA_N_SYS_COLS);

	dict_table_copy_types(entry, sys_virtual);

	/* 0: TABLE_ID -----------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_VIRTUAL__TABLE_ID);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);

	/* 1: POS ---------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_VIRTUAL__POS);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	ulint	v_col_no = dict_create_v_col_pos(v_col_n, v_col->m_col.ind);
	mach_write_to_4(ptr, v_col_no);

	dfield_set_data(dfield, ptr, 4);

	/* 2: BASE_POS ----------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_VIRTUAL__BASE_POS);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, base_column->ind);

	dfield_set_data(dfield, ptr, 4);

	/* 3: DB_TRX_ID added later */
	/* 4: DB_ROLL_PTR added later */

	/*---------------------------------*/
	return(entry);
}

/***************************************************************//**
Builds a table definition to insert.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
dict_build_table_def_step(
/*======================*/
	que_thr_t*	thr,	/*!< in: query thread */
	tab_node_t*	node)	/*!< in: table create node */
{
	dict_table_t*	table;
	dtuple_t*	row;
	dberr_t		err = DB_SUCCESS;

	table = node->table;

	trx_t*	trx = thr_get_trx(thr);
	dict_table_assign_new_id(table, trx);

	err = dict_build_tablespace_for_table(table);
	if (err != DB_SUCCESS) {
		return(err);
	}

	row = dict_create_sys_tables_tuple(table, node->heap);

	ins_node_set_new_row(node->tab_def, row);

	return(err);
}

/** Build a tablespace to store various objects.
@param[in,out]	tablespace	Tablespace object describing what to build.
@return DB_SUCCESS or error code. */
dberr_t
dict_build_tablespace(
	Tablespace*	tablespace)
{
	dberr_t		err	= DB_SUCCESS;
	mtr_t		mtr;
	ulint		space = 0;

	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(tablespace);

        DBUG_EXECUTE_IF("out_of_tablespace_disk",
                         return(DB_OUT_OF_FILE_SPACE););
	/* Get a new space id. */
	dict_hdr_get_new_id(NULL, NULL, &space, NULL, false);
	if (space == ULINT_UNDEFINED) {
		return(DB_ERROR);
	}
	tablespace->set_space_id(space);

	Datafile* datafile = tablespace->first_datafile();

	/* We create a new generic empty tablespace.
	We initially let it be 4 pages:
	- page 0 is the fsp header and an extent descriptor page,
	- page 1 is an ibuf bitmap page,
	- page 2 is the first inode page,
	- page 3 will contain the root of the clustered index of the
	first table we create here. */

	err = fil_ibd_create(
		space,
		tablespace->name(),
		datafile->filepath(),
		tablespace->flags(),
		FIL_IBD_FILE_INITIAL_SIZE);
	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Update SYS_TABLESPACES and SYS_DATAFILES */
	err = dict_replace_tablespace_and_filepath(
		tablespace->space_id(), tablespace->name(),
		datafile->filepath(), tablespace->flags());
	if (err != DB_SUCCESS) {
		os_file_delete(innodb_data_file_key, datafile->filepath());
		return(err);
	}

	mtr_start(&mtr);
	mtr.set_named_space(space);

	/* Once we allow temporary general tablespaces, we must do this;
	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO); */
	ut_a(!FSP_FLAGS_GET_TEMPORARY(tablespace->flags()));

	bool ret = fsp_header_init(space, FIL_IBD_FILE_INITIAL_SIZE, &mtr);
	mtr_commit(&mtr);

	if (!ret) {
		return(DB_ERROR);
	}

	return(err);
}

/** Builds a tablespace to contain a table, using file-per-table=1.
@param[in,out]	table	Table to build in its own tablespace.
@return DB_SUCCESS or error code */
dberr_t
dict_build_tablespace_for_table(
	dict_table_t*	table)
{
	dberr_t		err	= DB_SUCCESS;
	mtr_t		mtr;
	ulint		space = 0;
	bool		needs_file_per_table;
	char*		filepath;

	ut_ad(mutex_own(&dict_sys->mutex) || dict_table_is_intrinsic(table));

	needs_file_per_table
		= DICT_TF2_FLAG_IS_SET(table, DICT_TF2_USE_FILE_PER_TABLE);

	/* Always set this bit for all new created tables */
	DICT_TF2_FLAG_SET(table, DICT_TF2_FTS_AUX_HEX_NAME);
	DBUG_EXECUTE_IF("innodb_test_wrong_fts_aux_table_name",
			DICT_TF2_FLAG_UNSET(table,
					    DICT_TF2_FTS_AUX_HEX_NAME););

	if (needs_file_per_table) {
		/* This table will need a new tablespace. */

		ut_ad(dict_table_get_format(table) <= UNIV_FORMAT_MAX);
		ut_ad(DICT_TF_GET_ZIP_SSIZE(table->flags) == 0
		      || dict_table_get_format(table) >= UNIV_FORMAT_B);

		/* Get a new tablespace ID */
		dict_hdr_get_new_id(NULL, NULL, &space, table, false);

		DBUG_EXECUTE_IF(
			"ib_create_table_fail_out_of_space_ids",
			space = ULINT_UNDEFINED;
		);

		if (space == ULINT_UNDEFINED) {
			return(DB_ERROR);
		}
		table->space = static_cast<unsigned int>(space);

		/* Determine the tablespace flags. */
		bool	is_temp = dict_table_is_temporary(table);
		bool	is_encrypted = dict_table_is_encrypted(table);
		bool	has_data_dir = DICT_TF_HAS_DATA_DIR(table->flags);
		ulint	fsp_flags = dict_tf_to_fsp_flags(table->flags,
							 is_temp,
							 is_encrypted);

		/* Determine the full filepath */
		if (is_temp) {
			/* Temporary table filepath contains a full path
			and a filename without the extension. */
			ut_ad(table->dir_path_of_temp_table);
			filepath = fil_make_filepath(
				table->dir_path_of_temp_table,
				NULL, IBD, false);

		} else if (has_data_dir) {
			ut_ad(table->data_dir_path);
			filepath = fil_make_filepath(
				table->data_dir_path,
				table->name.m_name, IBD, true);

		} else {
			/* Make the tablespace file in the default dir
			using the table name */
			filepath = fil_make_filepath(
				NULL, table->name.m_name, IBD, false);
		}

		/* We create a new single-table tablespace for the table.
		We initially let it be 4 pages:
		- page 0 is the fsp header and an extent descriptor page,
		- page 1 is an ibuf bitmap page,
		- page 2 is the first inode page,
		- page 3 will contain the root of the clustered index of
		the table we create here. */

		err = fil_ibd_create(
			space, table->name.m_name, filepath, fsp_flags,
			FIL_IBD_FILE_INITIAL_SIZE);

		ut_free(filepath);

		if (err != DB_SUCCESS) {

			return(err);
		}

		mtr_start(&mtr);
		mtr.set_named_space(table->space);
		dict_disable_redo_if_temporary(table, &mtr);

		bool ret = fsp_header_init(table->space,
					   FIL_IBD_FILE_INITIAL_SIZE,
					   &mtr);

		mtr_commit(&mtr);
		if (!ret) {
			return(DB_ERROR);
		}
	} else {
		/* We do not need to build a tablespace for this table. It
		is already built.  Just find the correct tablespace ID. */

		if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
			ut_ad(table->tablespace != NULL);

			ut_ad(table->space == fil_space_get_id_by_name(
				table->tablespace()));
		} else if (dict_table_is_temporary(table)) {
			/* Use the shared temporary tablespace.
			Note: The temp tablespace supports all non-Compressed
			row formats whereas the system tablespace only
			supports Redundant and Compact */
			ut_ad(dict_tf_get_rec_format(table->flags)
				!= REC_FORMAT_COMPRESSED);
			table->space = static_cast<uint32_t>(
				srv_tmp_space.space_id());
		} else {
			/* Create in the system tablespace. */
			ut_ad(table->space == srv_sys_space.space_id());
		}

		DBUG_EXECUTE_IF("ib_ddl_crash_during_tablespace_alloc",
				DBUG_SUICIDE(););
	}

	return(DB_SUCCESS);
}

/***************************************************************//**
Builds a column definition to insert. */
static
void
dict_build_col_def_step(
/*====================*/
	tab_node_t*	node)	/*!< in: table create node */
{
	dtuple_t*	row;

	row = dict_create_sys_columns_tuple(node->table, node->col_no,
					    node->heap);
	ins_node_set_new_row(node->col_def, row);
}

/** Builds a SYS_VIRTUAL row definition to insert.
@param[in]	node	table create node */
static
void
dict_build_v_col_def_step(
	tab_node_t*	node)
{
	dtuple_t*	row;

	row = dict_create_sys_virtual_tuple(node->table, node->col_no,
					    node->base_col_no,
					    node->heap);
	ins_node_set_new_row(node->v_col_def, row);
}

/*****************************************************************//**
Based on an index object, this function builds the entry to be inserted
in the SYS_INDEXES system table.
@return the tuple which should be inserted */
static
dtuple_t*
dict_create_sys_indexes_tuple(
/*==========================*/
	const dict_index_t*	index,	/*!< in: index */
	mem_heap_t*		heap)	/*!< in: memory heap from
					which the memory for the built
					tuple is allocated */
{
	dict_table_t*	sys_indexes;
	dict_table_t*	table;
	dtuple_t*	entry;
	dfield_t*	dfield;
	byte*		ptr;

	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(index);
	ut_ad(heap);

	sys_indexes = dict_sys->sys_indexes;

	table = dict_table_get_low(index->table_name);

	entry = dtuple_create(
		heap, DICT_NUM_COLS__SYS_INDEXES + DATA_N_SYS_COLS);

	dict_table_copy_types(entry, sys_indexes);

	/* 0: TABLE_ID -----------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__TABLE_ID);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);

	/* 1: ID ----------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__ID);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(ptr, index->id);

	dfield_set_data(dfield, ptr, 8);

	/* 2: DB_TRX_ID added later */
	/* 3: DB_ROLL_PTR added later */
	/* 4: NAME --------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__NAME);

	if (!index->is_committed()) {
		ulint	len	= strlen(index->name) + 1;
		char*	name	= static_cast<char*>(
			mem_heap_alloc(heap, len));
		*name = *TEMP_INDEX_PREFIX_STR;
		memcpy(name + 1, index->name, len - 1);
		dfield_set_data(dfield, name, len);
	} else {
		dfield_set_data(dfield, index->name, strlen(index->name));
	}

	/* 5: N_FIELDS ----------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__N_FIELDS);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, index->n_fields);

	dfield_set_data(dfield, ptr, 4);

	/* 6: TYPE --------------------------*/
	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__TYPE);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, index->type);

	dfield_set_data(dfield, ptr, 4);

	/* 7: SPACE --------------------------*/

	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__SPACE);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, index->space);

	dfield_set_data(dfield, ptr, 4);

	/* 8: PAGE_NO --------------------------*/

	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__PAGE_NO);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, FIL_NULL);

	dfield_set_data(dfield, ptr, 4);

	/* 9: MERGE_THRESHOLD ----------------*/

	dfield = dtuple_get_nth_field(
		entry, DICT_COL__SYS_INDEXES__MERGE_THRESHOLD);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));
	mach_write_to_4(ptr, DICT_INDEX_MERGE_THRESHOLD_DEFAULT);

	dfield_set_data(dfield, ptr, 4);

	/*--------------------------------*/

	return(entry);
}

/*****************************************************************//**
Based on an index object, this function builds the entry to be inserted
in the SYS_FIELDS system table.
@return the tuple which should be inserted */
static
dtuple_t*
dict_create_sys_fields_tuple(
/*=========================*/
	const dict_index_t*	index,	/*!< in: index */
	ulint			fld_no,	/*!< in: field number */
	mem_heap_t*		heap)	/*!< in: memory heap from
					which the memory for the built
					tuple is allocated */
{
	dict_table_t*	sys_fields;
	dtuple_t*	entry;
	dict_field_t*	field;
	dfield_t*	dfield;
	byte*		ptr;
	ibool		index_contains_column_prefix_field	= FALSE;
	ulint		j;

	ut_ad(index);
	ut_ad(heap);

	for (j = 0; j < index->n_fields; j++) {
		if (dict_index_get_nth_field(index, j)->prefix_len > 0) {
			index_contains_column_prefix_field = TRUE;
			break;
		}
	}

	field = dict_index_get_nth_field(index, fld_no);

	sys_fields = dict_sys->sys_fields;

	entry = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(entry, sys_fields);

	/* 0: INDEX_ID -----------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_FIELDS__INDEX_ID);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(ptr, index->id);

	dfield_set_data(dfield, ptr, 8);

	/* 1: POS; FIELD NUMBER & PREFIX LENGTH -----------------------*/

	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_FIELDS__POS);

	ptr = static_cast<byte*>(mem_heap_alloc(heap, 4));

	if (index_contains_column_prefix_field) {
		/* If there are column prefix fields in the index, then
		we store the number of the field to the 2 HIGH bytes
		and the prefix length to the 2 low bytes, */

		mach_write_to_4(ptr, (fld_no << 16) + field->prefix_len);
	} else {
		/* Else we store the number of the field to the 2 LOW bytes.
		This is to keep the storage format compatible with
		InnoDB versions < 4.0.14. */

		mach_write_to_4(ptr, fld_no);
	}

	dfield_set_data(dfield, ptr, 4);

	/* 2: DB_TRX_ID added later */
	/* 3: DB_ROLL_PTR added later */
	/* 4: COL_NAME -------------------------*/
	dfield = dtuple_get_nth_field(entry, DICT_COL__SYS_FIELDS__COL_NAME);

	dfield_set_data(dfield, field->name,
			ut_strlen(field->name));
	/*---------------------------------*/

	return(entry);
}

/*****************************************************************//**
Creates the tuple with which the index entry is searched for writing the index
tree root page number, if such a tree is created.
@return the tuple for search */
static
dtuple_t*
dict_create_search_tuple(
/*=====================*/
	const dtuple_t*	tuple,	/*!< in: the tuple inserted in the SYS_INDEXES
				table */
	mem_heap_t*	heap)	/*!< in: memory heap from which the memory for
				the built tuple is allocated */
{
	dtuple_t*	search_tuple;
	const dfield_t*	field1;
	dfield_t*	field2;

	ut_ad(tuple && heap);

	search_tuple = dtuple_create(heap, 2);

	field1 = dtuple_get_nth_field(tuple, 0);
	field2 = dtuple_get_nth_field(search_tuple, 0);

	dfield_copy(field2, field1);

	field1 = dtuple_get_nth_field(tuple, 1);
	field2 = dtuple_get_nth_field(search_tuple, 1);

	dfield_copy(field2, field1);

	ut_ad(dtuple_validate(search_tuple));

	return(search_tuple);
}

/***************************************************************//**
Builds an index definition row to insert.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
dict_build_index_def_step(
/*======================*/
	que_thr_t*	thr,	/*!< in: query thread */
	ind_node_t*	node)	/*!< in: index create node */
{
	dict_table_t*	table;
	dict_index_t*	index;
	dtuple_t*	row;
	trx_t*		trx;

	ut_ad(mutex_own(&dict_sys->mutex));

	trx = thr_get_trx(thr);

	index = node->index;

	table = dict_table_get_low(index->table_name);

	if (table == NULL) {
		return(DB_TABLE_NOT_FOUND);
	}

	if (!trx->table_id) {
		/* Record only the first table id. */
		trx->table_id = table->id;
	}

	node->table = table;

	ut_ad((UT_LIST_GET_LEN(table->indexes) > 0)
	      || dict_index_is_clust(index));

	dict_hdr_get_new_id(NULL, &index->id, NULL, table, false);

	/* Inherit the space id from the table; we store all indexes of a
	table in the same tablespace */

	index->space = table->space;
	node->page_no = FIL_NULL;
	row = dict_create_sys_indexes_tuple(index, node->heap);
	node->ind_row = row;

	ins_node_set_new_row(node->ind_def, row);

	/* Note that the index was created by this transaction. */
	index->trx_id = trx->id;
	ut_ad(table->def_trx_id <= trx->id);
	table->def_trx_id = trx->id;

	return(DB_SUCCESS);
}

/***************************************************************//**
Builds an index definition without updating SYSTEM TABLES.
@return DB_SUCCESS or error code */
void
dict_build_index_def(
/*=================*/
	const dict_table_t*	table,	/*!< in: table */
	dict_index_t*		index,	/*!< in/out: index */
	trx_t*			trx)	/*!< in/out: InnoDB transaction handle */
{
	ut_ad(mutex_own(&dict_sys->mutex) || dict_table_is_intrinsic(table));

	if (trx->table_id == 0) {
		/* Record only the first table id. */
		trx->table_id = table->id;
	}

	ut_ad((UT_LIST_GET_LEN(table->indexes) > 0)
	      || dict_index_is_clust(index));

	if (!dict_table_is_intrinsic(table)) {
		dict_hdr_get_new_id(NULL, &index->id, NULL, table, false);
	} else {
		/* Index are re-loaded in process of creation using id.
		If same-id is used for all indexes only first index will always
		be retrieved when expected is iterative return of all indexes*/
		if (UT_LIST_GET_LEN(table->indexes) > 0) {
			index->id = UT_LIST_GET_LAST(table->indexes)->id + 1;
		} else {
			index->id = 1;
		}
	}

	/* Inherit the space id from the table; we store all indexes of a
	table in the same tablespace */

	index->space = table->space;

	/* Note that the index was created by this transaction. */
	index->trx_id = trx->id;
}

/***************************************************************//**
Builds a field definition row to insert. */
static
void
dict_build_field_def_step(
/*======================*/
	ind_node_t*	node)	/*!< in: index create node */
{
	dict_index_t*	index;
	dtuple_t*	row;

	index = node->index;

	row = dict_create_sys_fields_tuple(index, node->field_no, node->heap);

	ins_node_set_new_row(node->field_def, row);
}

/***************************************************************//**
Creates an index tree for the index if it is not a member of a cluster.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
dict_create_index_tree_step(
/*========================*/
	ind_node_t*	node)	/*!< in: index create node */
{
	mtr_t		mtr;
	btr_pcur_t	pcur;
	dict_index_t*	index;
	dict_table_t*	sys_indexes;
	dtuple_t*	search_tuple;

	ut_ad(mutex_own(&dict_sys->mutex));

	index = node->index;

	sys_indexes = dict_sys->sys_indexes;

	if (index->type == DICT_FTS) {
		/* FTS index does not need an index tree */
		return(DB_SUCCESS);
	}

	/* Run a mini-transaction in which the index tree is allocated for
	the index and its root address is written to the index entry in
	sys_indexes */

	mtr_start(&mtr);

	const bool	missing = index->table->ibd_file_missing
		|| dict_table_is_discarded(index->table);

	if (!missing) {
		mtr.set_named_space(index->space);
	}

	search_tuple = dict_create_search_tuple(node->ind_row, node->heap);

	btr_pcur_open(UT_LIST_GET_FIRST(sys_indexes->indexes),
		      search_tuple, PAGE_CUR_L, BTR_MODIFY_LEAF,
		      &pcur, &mtr);

	btr_pcur_move_to_next_user_rec(&pcur, &mtr);


	dberr_t		err = DB_SUCCESS;

	if (missing) {
		node->page_no = FIL_NULL;
	} else {
		node->page_no = btr_create(
			index->type, index->space,
			dict_table_page_size(index->table),
			index->id, index, NULL, &mtr);

		if (node->page_no == FIL_NULL) {
			err = DB_OUT_OF_FILE_SPACE;
		}

		DBUG_EXECUTE_IF("ib_import_create_index_failure_1",
				node->page_no = FIL_NULL;
				err = DB_OUT_OF_FILE_SPACE; );
	}

	page_rec_write_field(
		btr_pcur_get_rec(&pcur), DICT_FLD__SYS_INDEXES__PAGE_NO,
		node->page_no, &mtr);

	btr_pcur_close(&pcur);

	mtr_commit(&mtr);

	return(err);
}

/***************************************************************//**
Creates an index tree for the index if it is not a member of a cluster.
Don't update SYSTEM TABLES.
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
dberr_t
dict_create_index_tree_in_mem(
/*==========================*/
	dict_index_t*	index,	/*!< in/out: index */
	const trx_t*	trx)	/*!< in: InnoDB transaction handle */
{
	mtr_t		mtr;
	ulint		page_no = FIL_NULL;

	ut_ad(mutex_own(&dict_sys->mutex)
	      || dict_table_is_intrinsic(index->table));

	if (index->type == DICT_FTS) {
		/* FTS index does not need an index tree */
		return(DB_SUCCESS);
	}

	mtr_start(&mtr);
	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

	dberr_t		err = DB_SUCCESS;

	/* Currently this function is being used by temp-tables only.
	Import/Discard of temp-table is blocked and so this assert. */
	ut_ad(index->table->ibd_file_missing == 0
	      && !dict_table_is_discarded(index->table));

	page_no = btr_create(
		index->type, index->space,
		dict_table_page_size(index->table),
		index->id, index, NULL, &mtr);

	index->page = page_no;
	index->trx_id = trx->id;

	if (page_no == FIL_NULL) {
		err = DB_OUT_OF_FILE_SPACE;
	}

	mtr_commit(&mtr);

	return(err);
}

/** Drop the index tree associated with a row in SYS_INDEXES table.
@param[in,out]	rec	SYS_INDEXES record
@param[in,out]	pcur	persistent cursor on rec
@param[in,out]	mtr	mini-transaction
@return	whether freeing the B-tree was attempted */
bool
dict_drop_index_tree(
	rec_t*		rec,
	btr_pcur_t*	pcur,
	mtr_t*		mtr)
{
	const byte*	ptr;
	ulint		len;
	ulint		space;
	ulint		root_page_no;

	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(!dict_table_is_comp(dict_sys->sys_indexes));

	ptr = rec_get_nth_field_old(rec, DICT_FLD__SYS_INDEXES__PAGE_NO, &len);

	ut_ad(len == 4);

	btr_pcur_store_position(pcur, mtr);

	root_page_no = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	if (root_page_no == FIL_NULL) {
		/* The tree has already been freed */

		return(false);
	}

	mlog_write_ulint(const_cast<byte*>(ptr), FIL_NULL, MLOG_4BYTES, mtr);

	ptr = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__SPACE, &len);

	ut_ad(len == 4);

	space = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	ptr = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__ID, &len);

	ut_ad(len == 8);

	bool			found;
	const page_size_t	page_size(fil_space_get_page_size(space,
								  &found));

	if (!found) {
		/* It is a single table tablespace and the .ibd file is
		missing: do nothing */

		return(false);
	}

	/* If tablespace is scheduled for truncate, do not try to drop
	the indexes in that tablespace. There is a truncate fixup action
	which will take care of it. */
	if (srv_is_tablespace_truncated(space)) {
		return(false);
	}

	btr_free_if_exists(page_id_t(space, root_page_no), page_size,
			   mach_read_from_8(ptr), mtr);

	return(true);
}

/*******************************************************************//**
Drops the index tree but don't update SYS_INDEXES table. */
void
dict_drop_index_tree_in_mem(
/*========================*/
	const dict_index_t*	index,		/*!< in: index */
	ulint			page_no)	/*!< in: index page-no */
{
	ut_ad(mutex_own(&dict_sys->mutex)
	      || dict_table_is_intrinsic(index->table));
	ut_ad(dict_table_is_temporary(index->table));

	ulint			root_page_no = page_no;
	ulint			space = index->space;
	bool			found;
	const page_size_t	page_size(fil_space_get_page_size(space,
								  &found));

	/* If tree has already been freed or it is a single table
	tablespace and the .ibd file is missing do nothing,
	else free the all the pages */
	if (root_page_no != FIL_NULL && found) {
		btr_free(page_id_t(space, root_page_no), page_size);
	}
}

/*******************************************************************//**
Recreate the index tree associated with a row in SYS_INDEXES table.
@return	new root page number, or FIL_NULL on failure */
ulint
dict_recreate_index_tree(
/*=====================*/
	const dict_table_t*
			table,	/*!< in/out: the table the index belongs to */
	btr_pcur_t*	pcur,	/*!< in/out: persistent cursor pointing to
				record in the clustered index of
				SYS_INDEXES table. The cursor may be
				repositioned in this call. */
	mtr_t*		mtr)	/*!< in/out: mtr having the latch
				on the record page. */
{
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_a(!dict_table_is_comp(dict_sys->sys_indexes));

	ulint		len;
	rec_t*		rec = btr_pcur_get_rec(pcur);

	const byte*	ptr = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__PAGE_NO, &len);

	ut_ad(len == 4);

	ulint	root_page_no = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	ptr = rec_get_nth_field_old(rec, DICT_FLD__SYS_INDEXES__SPACE, &len);
	ut_ad(len == 4);

	ut_a(table->space == mtr_read_ulint(ptr, MLOG_4BYTES, mtr));

	ulint			space = table->space;
	bool			found;
	const page_size_t	page_size(fil_space_get_page_size(space,
								  &found));

	if (!found) {
		/* It is a single table tablespae and the .ibd file is
		missing: do nothing. */

		ib::warn()
			<< "Trying to TRUNCATE a missing .ibd file of table "
			<< table->name << "!";

		return(FIL_NULL);
	}

	ptr = rec_get_nth_field_old(rec, DICT_FLD__SYS_INDEXES__TYPE, &len);
	ut_ad(len == 4);
	ulint	type = mach_read_from_4(ptr);

	ptr = rec_get_nth_field_old(rec, DICT_FLD__SYS_INDEXES__ID, &len);
	ut_ad(len == 8);
	index_id_t	index_id = mach_read_from_8(ptr);

	/* We will need to commit the mini-transaction in order to avoid
	deadlocks in the btr_create() call, because otherwise we would
	be freeing and allocating pages in the same mini-transaction. */
	btr_pcur_store_position(pcur, mtr);
	mtr_commit(mtr);

	mtr_start(mtr);
	mtr->set_named_space(space);
	btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur, mtr);

	/* Find the index corresponding to this SYS_INDEXES record. */
	for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != NULL;
	     index = UT_LIST_GET_NEXT(indexes, index)) {
		if (index->id == index_id) {
			if (index->type & DICT_FTS) {
				return(FIL_NULL);
			} else {
				root_page_no = btr_create(
					type, space, page_size, index_id,
					index, NULL, mtr);
				index->page = (unsigned int) root_page_no;
				return(root_page_no);
			}
		}
	}

	ib::error() << "Failed to create index with index id " << index_id
		<< " of table " << table->name;

	return(FIL_NULL);
}

/*******************************************************************//**
Truncates the index tree but don't update SYSTEM TABLES.
@return DB_SUCCESS or error */
dberr_t
dict_truncate_index_tree_in_mem(
/*============================*/
	dict_index_t*	index)		/*!< in/out: index */
{
	mtr_t		mtr;
	bool		truncate;
	ulint		space = index->space;

	ut_ad(mutex_own(&dict_sys->mutex)
	      || dict_table_is_intrinsic(index->table));
	ut_ad(dict_table_is_temporary(index->table));

	ulint		type = index->type;
	ulint		root_page_no = index->page;

	if (root_page_no == FIL_NULL) {

		/* The tree has been freed. */
		ib::warn() << "Trying to TRUNCATE a missing index of table "
			<< index->table->name << "!";

		truncate = false;
	} else {
		truncate = true;
	}

	bool			found;
	const page_size_t	page_size(fil_space_get_page_size(space,
								  &found));

	if (!found) {

		/* It is a single table tablespace and the .ibd file is
		missing: do nothing */

		ib::warn()
			<< "Trying to TRUNCATE a missing .ibd file of table "
			<< index->table->name << "!";
	}

	/* If table to truncate resides in its on own tablespace that will
	be re-created on truncate then we can ignore freeing of existing
	tablespace objects. */

	if (truncate) {
		btr_free(page_id_t(space, root_page_no), page_size);
	}

	mtr_start(&mtr);
	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

	root_page_no = btr_create(
		type, space, page_size, index->id, index, NULL, &mtr);

	DBUG_EXECUTE_IF("ib_err_trunc_temp_recreate_index",
			root_page_no = FIL_NULL;);

	index->page = root_page_no;

	mtr_commit(&mtr);

	return(index->page == FIL_NULL ? DB_ERROR : DB_SUCCESS);
}

/*********************************************************************//**
Creates a table create graph.
@return own: table create node */
tab_node_t*
tab_create_graph_create(
/*====================*/
	dict_table_t*	table,	/*!< in: table to create, built as a memory data
				structure */
	mem_heap_t*	heap)	/*!< in: heap where created */
{
	tab_node_t*	node;

	node = static_cast<tab_node_t*>(
		mem_heap_alloc(heap, sizeof(tab_node_t)));

	node->common.type = QUE_NODE_CREATE_TABLE;

	node->table = table;

	node->state = TABLE_BUILD_TABLE_DEF;
	node->heap = mem_heap_create(256);

	node->tab_def = ins_node_create(INS_DIRECT, dict_sys->sys_tables,
					heap);
	node->tab_def->common.parent = node;

	node->col_def = ins_node_create(INS_DIRECT, dict_sys->sys_columns,
					heap);
	node->col_def->common.parent = node;

	node->v_col_def = ins_node_create(INS_DIRECT, dict_sys->sys_virtual,
                                          heap);
	node->v_col_def->common.parent = node;

	return(node);
}

/** Creates an index create graph.
@param[in]	index	index to create, built as a memory data structure
@param[in,out]	heap	heap where created
@param[in]	add_v	new virtual columns added in the same clause with
			add index
@return own: index create node */
ind_node_t*
ind_create_graph_create(
	dict_index_t*		index,
	mem_heap_t*		heap,
	const dict_add_v_col_t*	add_v)
{
	ind_node_t*	node;

	node = static_cast<ind_node_t*>(
		mem_heap_alloc(heap, sizeof(ind_node_t)));

	node->common.type = QUE_NODE_CREATE_INDEX;

	node->index = index;

	node->add_v = add_v;

	node->state = INDEX_BUILD_INDEX_DEF;
	node->page_no = FIL_NULL;
	node->heap = mem_heap_create(256);

	node->ind_def = ins_node_create(INS_DIRECT,
					dict_sys->sys_indexes, heap);
	node->ind_def->common.parent = node;

	node->field_def = ins_node_create(INS_DIRECT,
					  dict_sys->sys_fields, heap);
	node->field_def->common.parent = node;

	return(node);
}

/***********************************************************//**
Creates a table. This is a high-level function used in SQL execution graphs.
@return query thread to run next or NULL */
que_thr_t*
dict_create_table_step(
/*===================*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	tab_node_t*	node;
	dberr_t		err	= DB_ERROR;
	trx_t*		trx;

	ut_ad(thr);
	ut_ad(mutex_own(&dict_sys->mutex));

	trx = thr_get_trx(thr);

	node = static_cast<tab_node_t*>(thr->run_node);

	ut_ad(que_node_get_type(node) == QUE_NODE_CREATE_TABLE);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = TABLE_BUILD_TABLE_DEF;
	}

	if (node->state == TABLE_BUILD_TABLE_DEF) {

		/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */

		err = dict_build_table_def_step(thr, node);

		if (err != DB_SUCCESS) {

			goto function_exit;
		}

		node->state = TABLE_BUILD_COL_DEF;
		node->col_no = 0;

		thr->run_node = node->tab_def;

		return(thr);
	}

	if (node->state == TABLE_BUILD_COL_DEF) {

		if (node->col_no < (static_cast<ulint>(node->table->n_def)
				    + static_cast<ulint>(node->table->n_v_def))) {

			dict_build_col_def_step(node);

			node->col_no++;

			thr->run_node = node->col_def;

			return(thr);
		} else {
			/* Move on to SYS_VIRTUAL table */
			node->col_no = 0;
                        node->base_col_no = 0;
                        node->state = TABLE_BUILD_V_COL_DEF;
		}
	}

	if (node->state == TABLE_BUILD_V_COL_DEF) {

		if (node->col_no < static_cast<ulint>(node->table->n_v_def)) {
			dict_v_col_t*   v_col = dict_table_get_nth_v_col(
						node->table, node->col_no);

			/* If no base column */
			while (v_col->num_base == 0) {
				node->col_no++;
				if (node->col_no == static_cast<ulint>(
					(node->table)->n_v_def)) {
					node->state = TABLE_ADD_TO_CACHE;
					break;
				}

				v_col = dict_table_get_nth_v_col(
					node->table, node->col_no);
				node->base_col_no = 0;
			}

			if (node->state != TABLE_ADD_TO_CACHE) {
				ut_ad(node->col_no == v_col->v_pos);
				dict_build_v_col_def_step(node);

				if (node->base_col_no < v_col->num_base - 1) {
					/* move on to next base column */
					node->base_col_no++;
				} else {
					/* move on to next virtual column */
					node->col_no++;
					node->base_col_no = 0;
				}

				thr->run_node = node->v_col_def;

				return(thr);
			}
		} else {
			node->state = TABLE_ADD_TO_CACHE;
		}
	}

	if (node->state == TABLE_ADD_TO_CACHE) {
		DBUG_EXECUTE_IF("ib_ddl_crash_during_create", DBUG_SUICIDE(););

		dict_table_add_to_cache(node->table, TRUE, node->heap);

		err = DB_SUCCESS;
	}

function_exit:
	trx->error_state = err;

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */

	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
		/* SQL error detected */

		return(NULL);
	}

	thr->run_node = que_node_get_parent(node);

	return(thr);
}

/***********************************************************//**
Creates an index. This is a high-level function used in SQL execution
graphs.
@return query thread to run next or NULL */
que_thr_t*
dict_create_index_step(
/*===================*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	ind_node_t*	node;
	dberr_t		err	= DB_ERROR;
	trx_t*		trx;

	ut_ad(thr);
	ut_ad(mutex_own(&dict_sys->mutex));

	trx = thr_get_trx(thr);

	node = static_cast<ind_node_t*>(thr->run_node);

	ut_ad(que_node_get_type(node) == QUE_NODE_CREATE_INDEX);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = INDEX_BUILD_INDEX_DEF;
	}

	if (node->state == INDEX_BUILD_INDEX_DEF) {
		/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */
		err = dict_build_index_def_step(thr, node);

		if (err != DB_SUCCESS) {

			goto function_exit;
		}

		node->state = INDEX_BUILD_FIELD_DEF;
		node->field_no = 0;

		thr->run_node = node->ind_def;

		return(thr);
	}

	if (node->state == INDEX_BUILD_FIELD_DEF) {

		if (node->field_no < (node->index)->n_fields) {

			dict_build_field_def_step(node);

			node->field_no++;

			thr->run_node = node->field_def;

			return(thr);
		} else {
			node->state = INDEX_ADD_TO_CACHE;
		}
	}

	if (node->state == INDEX_ADD_TO_CACHE) {

		index_id_t	index_id = node->index->id;

		err = dict_index_add_to_cache_w_vcol(
			node->table, node->index, node->add_v, FIL_NULL,
			trx_is_strict(trx));

		node->index = dict_index_get_if_in_cache_low(index_id);
		ut_a(!node->index == (err != DB_SUCCESS));

		if (err != DB_SUCCESS) {

			goto function_exit;
		}

		node->state = INDEX_CREATE_INDEX_TREE;
	}

	if (node->state == INDEX_CREATE_INDEX_TREE) {

		err = dict_create_index_tree_step(node);

		DBUG_EXECUTE_IF("ib_dict_create_index_tree_fail",
				err = DB_OUT_OF_MEMORY;);

		if (err != DB_SUCCESS) {
			/* If this is a FTS index, we will need to remove
			it from fts->cache->indexes list as well */
			if ((node->index->type & DICT_FTS)
			    && node->table->fts) {
				fts_index_cache_t*	index_cache;

				rw_lock_x_lock(
					&node->table->fts->cache->init_lock);

				index_cache = (fts_index_cache_t*)
					 fts_find_index_cache(
						node->table->fts->cache,
						node->index);

				if (index_cache->words) {
					rbt_free(index_cache->words);
					index_cache->words = 0;
				}

				ib_vector_remove(
					node->table->fts->cache->indexes,
					*reinterpret_cast<void**>(index_cache));

				rw_lock_x_unlock(
					&node->table->fts->cache->init_lock);
			}

			dict_index_remove_from_cache(node->table, node->index);
			node->index = NULL;

			goto function_exit;
		}

		node->index->page = node->page_no;
		/* These should have been set in
		dict_build_index_def_step() and
		dict_index_add_to_cache(). */
		ut_ad(node->index->trx_id == trx->id);
		ut_ad(node->index->table->def_trx_id == trx->id);
	}

function_exit:
	trx->error_state = err;

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */

	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
		/* SQL error detected */

		return(NULL);
	}

	thr->run_node = que_node_get_parent(node);

	return(thr);
}

/****************************************************************//**
Check whether a system table exists.  Additionally, if it exists,
move it to the non-LRU end of the table LRU list.  This is oly used
for system tables that can be upgraded or added to an older database,
which include SYS_FOREIGN, SYS_FOREIGN_COLS, SYS_TABLESPACES and
SYS_DATAFILES.
@return DB_SUCCESS if the sys table exists, DB_CORRUPTION if it exists
but is not current, DB_TABLE_NOT_FOUND if it does not exist*/
static
dberr_t
dict_check_if_system_table_exists(
/*==============================*/
	const char*	tablename,	/*!< in: name of table */
	ulint		num_fields,	/*!< in: number of fields */
	ulint		num_indexes)	/*!< in: number of indexes */
{
	dict_table_t*	sys_table;
	dberr_t		error = DB_SUCCESS;

	ut_a(srv_get_active_thread_type() == SRV_NONE);

	mutex_enter(&dict_sys->mutex);

	sys_table = dict_table_get_low(tablename);

	if (sys_table == NULL) {
		error = DB_TABLE_NOT_FOUND;

	} else if (UT_LIST_GET_LEN(sys_table->indexes) != num_indexes
		   || sys_table->n_cols != num_fields) {
		error = DB_CORRUPTION;

	} else {
		/* This table has already been created, and it is OK.
		Ensure that it can't be evicted from the table LRU cache. */

		dict_table_prevent_eviction(sys_table);
	}

	mutex_exit(&dict_sys->mutex);

	return(error);
}

/****************************************************************//**
Creates the foreign key constraints system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return DB_SUCCESS or error code */
dberr_t
dict_create_or_check_foreign_constraint_tables(void)
/*================================================*/
{
	trx_t*		trx;
	my_bool		srv_file_per_table_backup;
	dberr_t		err;
	dberr_t		sys_foreign_err;
	dberr_t		sys_foreign_cols_err;

	ut_a(srv_get_active_thread_type() == SRV_NONE);

	/* Note: The master thread has not been started at this point. */


	sys_foreign_err = dict_check_if_system_table_exists(
		"SYS_FOREIGN", DICT_NUM_FIELDS__SYS_FOREIGN + 1, 3);
	sys_foreign_cols_err = dict_check_if_system_table_exists(
		"SYS_FOREIGN_COLS", DICT_NUM_FIELDS__SYS_FOREIGN_COLS + 1, 1);

	if (sys_foreign_err == DB_SUCCESS
	    && sys_foreign_cols_err == DB_SUCCESS) {
		return(DB_SUCCESS);
	}

	trx = trx_allocate_for_mysql();

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	trx->op_info = "creating foreign key sys tables";

	row_mysql_lock_data_dictionary(trx);

	/* Check which incomplete table definition to drop. */

	if (sys_foreign_err == DB_CORRUPTION) {
		ib::warn() << "Dropping incompletely created"
			" SYS_FOREIGN table.";
		row_drop_table_for_mysql("SYS_FOREIGN", trx, TRUE);
	}

	if (sys_foreign_cols_err == DB_CORRUPTION) {
		ib::warn() << "Dropping incompletely created"
			" SYS_FOREIGN_COLS table.";

		row_drop_table_for_mysql("SYS_FOREIGN_COLS", trx, TRUE);
	}

	ib::warn() << "Creating foreign key constraint system tables.";

	/* NOTE: in dict_load_foreigns we use the fact that
	there are 2 secondary indexes on SYS_FOREIGN, and they
	are defined just like below */

	/* NOTE: when designing InnoDB's foreign key support in 2001, we made
	an error and made the table names and the foreign key id of type
	'CHAR' (internally, really a VARCHAR). We should have made the type
	VARBINARY, like in other InnoDB system tables, to get a clean
	design. */

	srv_file_per_table_backup = srv_file_per_table;

	/* We always want SYSTEM tables to be created inside the system
	tablespace. */

	srv_file_per_table = 0;

	err = que_eval_sql(
		NULL,
		"PROCEDURE CREATE_FOREIGN_SYS_TABLES_PROC () IS\n"
		"BEGIN\n"
		"CREATE TABLE\n"
		"SYS_FOREIGN(ID CHAR, FOR_NAME CHAR,"
		" REF_NAME CHAR, N_COLS INT);\n"
		"CREATE UNIQUE CLUSTERED INDEX ID_IND"
		" ON SYS_FOREIGN (ID);\n"
		"CREATE INDEX FOR_IND"
		" ON SYS_FOREIGN (FOR_NAME);\n"
		"CREATE INDEX REF_IND"
		" ON SYS_FOREIGN (REF_NAME);\n"
		"CREATE TABLE\n"
		"SYS_FOREIGN_COLS(ID CHAR, POS INT,"
		" FOR_COL_NAME CHAR, REF_COL_NAME CHAR);\n"
		"CREATE UNIQUE CLUSTERED INDEX ID_IND"
		" ON SYS_FOREIGN_COLS (ID, POS);\n"
		"END;\n",
		FALSE, trx);

	if (err != DB_SUCCESS) {

		ib::error() << "Creation of SYS_FOREIGN and SYS_FOREIGN_COLS"
			" failed: " << ut_strerr(err) << ". Tablespace is"
			" full. Dropping incompletely created tables.";

		ut_ad(err == DB_OUT_OF_FILE_SPACE
		      || err == DB_TOO_MANY_CONCURRENT_TRXS);

		row_drop_table_for_mysql("SYS_FOREIGN", trx, TRUE);
		row_drop_table_for_mysql("SYS_FOREIGN_COLS", trx, TRUE);

		if (err == DB_OUT_OF_FILE_SPACE) {
			err = DB_MUST_GET_MORE_FILE_SPACE;
		}
	}

	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);

	srv_file_per_table = srv_file_per_table_backup;

	if (err == DB_SUCCESS) {
		ib::info() << "Foreign key constraint system tables created";
	}

	/* Note: The master thread has not been started at this point. */
	/* Confirm and move to the non-LRU part of the table LRU list. */
	sys_foreign_err = dict_check_if_system_table_exists(
		"SYS_FOREIGN", DICT_NUM_FIELDS__SYS_FOREIGN + 1, 3);
	ut_a(sys_foreign_err == DB_SUCCESS);

	sys_foreign_cols_err = dict_check_if_system_table_exists(
		"SYS_FOREIGN_COLS", DICT_NUM_FIELDS__SYS_FOREIGN_COLS + 1, 1);
	ut_a(sys_foreign_cols_err == DB_SUCCESS);

	return(err);
}

/** Creates the virtual column system table (SYS_VIRTUAL) inside InnoDB
at server bootstrap or server start if the table is not found or is
not of the right form.
@return DB_SUCCESS or error code */
dberr_t
dict_create_or_check_sys_virtual()
{
	trx_t*		trx;
	my_bool		srv_file_per_table_backup;
	dberr_t		err;

	ut_a(srv_get_active_thread_type() == SRV_NONE);

	/* Note: The master thread has not been started at this point. */
	err = dict_check_if_system_table_exists(
		"SYS_VIRTUAL", DICT_NUM_FIELDS__SYS_VIRTUAL + 1, 1);

	if (err == DB_SUCCESS) {
		mutex_enter(&dict_sys->mutex);
		dict_sys->sys_virtual = dict_table_get_low("SYS_VIRTUAL");
		mutex_exit(&dict_sys->mutex);
		return(DB_SUCCESS);
	}

	if (srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO
	    || srv_read_only_mode) {
		ib::error() << "Cannot create sys_virtual system tables;"
			" running in read-only mode.";
		return(DB_ERROR);
	}

	trx = trx_allocate_for_mysql();

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	trx->op_info = "creating sys_virtual tables";

	row_mysql_lock_data_dictionary(trx);

	/* Check which incomplete table definition to drop. */

	if (err == DB_CORRUPTION) {
		ib::warn() << "Dropping incompletely created"
			" SYS_VIRTUAL table.";
		row_drop_table_for_mysql("SYS_VIRTUAL", trx, TRUE);
	}

	ib::info() << "Creating sys_virtual system tables.";

	srv_file_per_table_backup = srv_file_per_table;

	/* We always want SYSTEM tables to be created inside the system
	tablespace. */

	srv_file_per_table = 0;

	err = que_eval_sql(
		NULL,
		"PROCEDURE CREATE_SYS_VIRTUAL_TABLES_PROC () IS\n"
		"BEGIN\n"
		"CREATE TABLE\n"
		"SYS_VIRTUAL(TABLE_ID BIGINT, POS INT,"
		" BASE_POS INT);\n"
		"CREATE UNIQUE CLUSTERED INDEX BASE_IDX"
		" ON SYS_VIRTUAL(TABLE_ID, POS, BASE_POS);\n"
		"END;\n",
		FALSE, trx);

	if (err != DB_SUCCESS) {

		ib::error() << "Creation of SYS_VIRTUAL"
			" failed: " << ut_strerr(err) << ". Tablespace is"
			" full or too many transactions."
			" Dropping incompletely created tables.";

		ut_ad(err == DB_OUT_OF_FILE_SPACE
		      || err == DB_TOO_MANY_CONCURRENT_TRXS);

		row_drop_table_for_mysql("SYS_VIRTUAL", trx, TRUE);

		if (err == DB_OUT_OF_FILE_SPACE) {
			err = DB_MUST_GET_MORE_FILE_SPACE;
		}
	}

	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);

	srv_file_per_table = srv_file_per_table_backup;

	if (err == DB_SUCCESS) {
		ib::info() << "sys_virtual table created";
	}

	/* Note: The master thread has not been started at this point. */
	/* Confirm and move to the non-LRU part of the table LRU list. */
	dberr_t sys_virtual_err = dict_check_if_system_table_exists(
		"SYS_VIRTUAL", DICT_NUM_FIELDS__SYS_VIRTUAL + 1, 1);
	ut_a(sys_virtual_err == DB_SUCCESS);
	mutex_enter(&dict_sys->mutex);
	dict_sys->sys_virtual = dict_table_get_low("SYS_VIRTUAL");
	mutex_exit(&dict_sys->mutex);

	return(err);
}

/****************************************************************//**
Evaluate the given foreign key SQL statement.
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
dict_foreign_eval_sql(
/*==================*/
	pars_info_t*	info,	/*!< in: info struct */
	const char*	sql,	/*!< in: SQL string to evaluate */
	const char*	name,	/*!< in: table name (for diagnostics) */
	const char*	id,	/*!< in: foreign key id */
	trx_t*		trx)	/*!< in/out: transaction */
{
	dberr_t	error;
	FILE*	ef	= dict_foreign_err_file;

	error = que_eval_sql(info, sql, FALSE, trx);

	if (error == DB_DUPLICATE_KEY) {
		mutex_enter(&dict_foreign_err_mutex);
		rewind(ef);
		ut_print_timestamp(ef);
		fputs(" Error in foreign key constraint creation for table ",
		      ef);
		ut_print_name(ef, trx, name);
		fputs(".\nA foreign key constraint of name ", ef);
		ut_print_name(ef, trx, id);
		fputs("\nalready exists."
		      " (Note that internally InnoDB adds 'databasename'\n"
		      "in front of the user-defined constraint name.)\n"
		      "Note that InnoDB's FOREIGN KEY system tables store\n"
		      "constraint names as case-insensitive, with the\n"
		      "MySQL standard latin1_swedish_ci collation. If you\n"
		      "create tables or databases whose names differ only in\n"
		      "the character case, then collisions in constraint\n"
		      "names can occur. Workaround: name your constraints\n"
		      "explicitly with unique names.\n",
		      ef);

		mutex_exit(&dict_foreign_err_mutex);

		return(error);
	}

	if (error != DB_SUCCESS) {
		ib::error() << "Foreign key constraint creation failed: "
			<< ut_strerr(error);

		mutex_enter(&dict_foreign_err_mutex);
		ut_print_timestamp(ef);
		fputs(" Internal error in foreign key constraint creation"
		      " for table ", ef);
		ut_print_name(ef, trx, name);
		fputs(".\n"
		      "See the MySQL .err log in the datadir"
		      " for more information.\n", ef);
		mutex_exit(&dict_foreign_err_mutex);

		return(error);
	}

	return(DB_SUCCESS);
}

/********************************************************************//**
Add a single foreign key field definition to the data dictionary tables in
the database.
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
dict_create_add_foreign_field_to_dictionary(
/*========================================*/
	ulint			field_nr,	/*!< in: field number */
	const char*		table_name,	/*!< in: table name */
	const dict_foreign_t*	foreign,	/*!< in: foreign */
	trx_t*			trx)		/*!< in/out: transaction */
{
	DBUG_ENTER("dict_create_add_foreign_field_to_dictionary");

	pars_info_t*	info = pars_info_create();

	pars_info_add_str_literal(info, "id", foreign->id);

	pars_info_add_int4_literal(info, "pos", field_nr);

	pars_info_add_str_literal(info, "for_col_name",
				  foreign->foreign_col_names[field_nr]);

	pars_info_add_str_literal(info, "ref_col_name",
				  foreign->referenced_col_names[field_nr]);

	DBUG_RETURN(dict_foreign_eval_sql(
		       info,
		       "PROCEDURE P () IS\n"
		       "BEGIN\n"
		       "INSERT INTO SYS_FOREIGN_COLS VALUES"
		       "(:id, :pos, :for_col_name, :ref_col_name);\n"
		       "END;\n",
		       table_name, foreign->id, trx));
}

/********************************************************************//**
Add a foreign key definition to the data dictionary tables.
@return error code or DB_SUCCESS */
dberr_t
dict_create_add_foreign_to_dictionary(
/*==================================*/
	const char*		name,	/*!< in: table name */
	const dict_foreign_t*	foreign,/*!< in: foreign key */
	trx_t*			trx)	/*!< in/out: dictionary transaction */
{
	dberr_t		error;

	DBUG_ENTER("dict_create_add_foreign_to_dictionary");

	pars_info_t*	info = pars_info_create();

	pars_info_add_str_literal(info, "id", foreign->id);

	pars_info_add_str_literal(info, "for_name", name);

	pars_info_add_str_literal(info, "ref_name",
				  foreign->referenced_table_name);

	pars_info_add_int4_literal(info, "n_cols",
				   foreign->n_fields + (foreign->type << 24));

	DBUG_PRINT("dict_create_add_foreign_to_dictionary",
		   ("'%s', '%s', '%s', %d", foreign->id, name,
		    foreign->referenced_table_name,
		    foreign->n_fields + (foreign->type << 24)));

	error = dict_foreign_eval_sql(info,
				      "PROCEDURE P () IS\n"
				      "BEGIN\n"
				      "INSERT INTO SYS_FOREIGN VALUES"
				      "(:id, :for_name, :ref_name, :n_cols);\n"
				      "END;\n"
				      , name, foreign->id, trx);

	if (error != DB_SUCCESS) {

		DBUG_RETURN(error);
	}

	for (ulint i = 0; i < foreign->n_fields; i++) {
		error = dict_create_add_foreign_field_to_dictionary(
			i, name, foreign, trx);

		if (error != DB_SUCCESS) {

			DBUG_RETURN(error);
		}
	}

	DBUG_RETURN(error);
}

/** Check whether a column is in an index by the column name
@param[in]	col_name	column name for the column to be checked
@param[in]	index		the index to be searched
@return	true if this column is in the index, otherwise, false */
static
bool
dict_index_has_col_by_name(
	const char*		col_name,
	const dict_index_t*	index)
{
        for (ulint i = 0; i < index->n_fields; i++) {
                dict_field_t*   field = dict_index_get_nth_field(index, i);

		if (strcmp(field->name, col_name) == 0) {
			return(true);
		}
	}
	return(false);
}

/** Check whether the foreign constraint could be on a column that is
part of a virtual index (index contains virtual column) in the table
@param[in]	fk_col_name	FK column name to be checked
@param[in]	table		the table
@return	true if this column is indexed with other virtual columns */
bool
dict_foreign_has_col_in_v_index(
	const char*		fk_col_name,
	const dict_table_t*	table)
{
	/* virtual column can't be Primary Key, so start with secondary index */
	for (dict_index_t* index = dict_table_get_next_index(
		     dict_table_get_first_index(table));
	     index;
	     index = dict_table_get_next_index(index)) {

		if (dict_index_has_virtual(index)) {
			if (dict_index_has_col_by_name(fk_col_name, index)) {
				return(true);
			}
		}
	}

	return(false);
}


/** Check whether the foreign constraint could be on a column that is
a base column of some indexed virtual columns.
@param[in]	col_name	column name for the column to be checked
@param[in]	table		the table
@return	true if this column is a base column, otherwise, false */
bool
dict_foreign_has_col_as_base_col(
	const char*		col_name,
	const dict_table_t*	table)
{
	/* Loop through each virtual column and check if its base column has
	the same name as the column name being checked */
	for (ulint i = 0; i < table->n_v_cols; i++) {
		dict_v_col_t*	v_col = dict_table_get_nth_v_col(table, i);

		/* Only check if the virtual column is indexed */
		if (!v_col->m_col.ord_part) {
			continue;
		}

		for (ulint j = 0; j < v_col->num_base; j++) {
			if (strcmp(col_name, dict_table_get_col_name(
					   table,
					   v_col->base_col[j]->ind)) == 0) {
				return(true);
			}
		}
	}

	return(false);
}

/** Check if a foreign constraint is on the given column name.
@param[in]	col_name	column name to be searched for fk constraint
@param[in]	table		table to which foreign key constraint belongs
@return true if fk constraint is present on the table, false otherwise. */
static
bool
dict_foreign_base_for_stored(
	const char*		col_name,
	const dict_table_t*	table)
{
	/* Loop through each stored column and check if its base column has
	the same name as the column name being checked */
	dict_s_col_list::const_iterator	it;
	for (it = table->s_cols->begin();
	     it != table->s_cols->end(); ++it) {
		dict_s_col_t	s_col = *it;

		for (ulint j = 0; j < s_col.num_base; j++) {
			/** If the stored column can refer to virtual column
			or another stored column then it can points to NULL. */

			if (s_col.base_col[j] == NULL) {
				continue;
			}

			if (strcmp(col_name, dict_table_get_col_name(
						table,
						s_col.base_col[j]->ind)) == 0) {
				return(true);
			}
		}
	}

	return(false);
}

/** Check if a foreign constraint is on columns served as base columns
of any stored column. This is to prevent creating SET NULL or CASCADE
constraint on such columns
@param[in]	local_fk_set	set of foreign key objects, to be added to
the dictionary tables
@param[in]	table		table to which the foreign key objects in
local_fk_set belong to
@return true if yes, otherwise, false */
bool
dict_foreigns_has_s_base_col(
	const dict_foreign_set&	local_fk_set,
	const dict_table_t*	table)
{
	dict_foreign_t*	foreign;

	if (table->s_cols == NULL) {
		return (false);
	}

	for (dict_foreign_set::const_iterator it = local_fk_set.begin();
	     it != local_fk_set.end(); ++it) {

		foreign = *it;
		ulint	type = foreign->type;

		type &= ~(DICT_FOREIGN_ON_DELETE_NO_ACTION
			  | DICT_FOREIGN_ON_UPDATE_NO_ACTION);

		if (type == 0) {
			continue;
		}

		for (ulint i = 0; i < foreign->n_fields; i++) {
			/* Check if the constraint is on a column that
			is a base column of any stored column */
			if (dict_foreign_base_for_stored(
				foreign->foreign_col_names[i], table)) {
				return(true);
			}
		}
	}

	return(false);
}

/** Check if a column is in foreign constraint with CASCADE properties or
SET NULL
@param[in]	table		table
@param[in]	fk_col_name	name for the column to be checked
@return true if the column is in foreign constraint, otherwise, false */
bool
dict_foreigns_has_this_col(
	const dict_table_t*	table,
	const char*		col_name)
{
	dict_foreign_t*		foreign;
	const dict_foreign_set*	local_fk_set = &table->foreign_set;

	for (dict_foreign_set::const_iterator it = local_fk_set->begin();
	     it != local_fk_set->end();
	     ++it) {
		foreign = *it;
		ut_ad(foreign->id != NULL);
		ulint	type = foreign->type;

		type &= ~(DICT_FOREIGN_ON_DELETE_NO_ACTION
			  | DICT_FOREIGN_ON_UPDATE_NO_ACTION);

		if (type == 0) {
			continue;
		}

		for (ulint i = 0; i < foreign->n_fields; i++) {
			if (strcmp(foreign->foreign_col_names[i],
				   col_name) == 0) {
				return(true);
			}
		}
	}
	return(false);
}

/** Adds the given set of foreign key objects to the dictionary tables
in the database. This function does not modify the dictionary cache. The
caller must ensure that all foreign key objects contain a valid constraint
name in foreign->id.
@param[in]	local_fk_set	set of foreign key objects, to be added to
the dictionary tables
@param[in]	table		table to which the foreign key objects in
local_fk_set belong to
@param[in,out]	trx		transaction
@return error code or DB_SUCCESS */
dberr_t
dict_create_add_foreigns_to_dictionary(
/*===================================*/
	const dict_foreign_set&	local_fk_set,
	const dict_table_t*	table,
	trx_t*			trx)
{
	dict_foreign_t*	foreign;
	dberr_t		error;

	ut_ad(mutex_own(&dict_sys->mutex)
	      || dict_table_is_intrinsic(table));

	if (dict_table_is_intrinsic(table)) {
		goto exit_loop;
	}

	if (NULL == dict_table_get_low("SYS_FOREIGN")) {

		ib::error() << "Table SYS_FOREIGN not found"
			" in internal data dictionary";

		return(DB_ERROR);
	}

	for (dict_foreign_set::const_iterator it = local_fk_set.begin();
	     it != local_fk_set.end();
	     ++it) {

		foreign = *it;
		ut_ad(foreign->id != NULL);

		error = dict_create_add_foreign_to_dictionary(
			table->name.m_name, foreign, trx);

		if (error != DB_SUCCESS) {

			return(error);
		}
	}

exit_loop:
	trx->op_info = "committing foreign key definitions";

	if (trx_is_started(trx)) {

		trx_commit(trx);
	}

	trx->op_info = "";

	return(DB_SUCCESS);
}

/****************************************************************//**
Creates the tablespaces and datafiles system tables inside InnoDB
at server bootstrap or server start if they are not found or are
not of the right form.
@return DB_SUCCESS or error code */
dberr_t
dict_create_or_check_sys_tablespace(void)
/*=====================================*/
{
	trx_t*		trx;
	my_bool		srv_file_per_table_backup;
	dberr_t		err;
	dberr_t		sys_tablespaces_err;
	dberr_t		sys_datafiles_err;

	ut_a(srv_get_active_thread_type() == SRV_NONE);

	/* Note: The master thread has not been started at this point. */

	sys_tablespaces_err = dict_check_if_system_table_exists(
		"SYS_TABLESPACES", DICT_NUM_FIELDS__SYS_TABLESPACES + 1, 1);
	sys_datafiles_err = dict_check_if_system_table_exists(
		"SYS_DATAFILES", DICT_NUM_FIELDS__SYS_DATAFILES + 1, 1);

	if (sys_tablespaces_err == DB_SUCCESS
	    && sys_datafiles_err == DB_SUCCESS) {
		return(DB_SUCCESS);
	}

	trx = trx_allocate_for_mysql();

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	trx->op_info = "creating tablepace and datafile sys tables";

	row_mysql_lock_data_dictionary(trx);

	/* Check which incomplete table definition to drop. */

	if (sys_tablespaces_err == DB_CORRUPTION) {
		ib::warn() << "Dropping incompletely created"
			" SYS_TABLESPACES table.";
		row_drop_table_for_mysql("SYS_TABLESPACES", trx, TRUE);
	}

	if (sys_datafiles_err == DB_CORRUPTION) {
		ib::warn() << "Dropping incompletely created"
			" SYS_DATAFILES table.";

		row_drop_table_for_mysql("SYS_DATAFILES", trx, TRUE);
	}

	ib::info() << "Creating tablespace and datafile system tables.";

	/* We always want SYSTEM tables to be created inside the system
	tablespace. */
	srv_file_per_table_backup = srv_file_per_table;
	srv_file_per_table = 0;

	err = que_eval_sql(
		NULL,
		"PROCEDURE CREATE_SYS_TABLESPACE_PROC () IS\n"
		"BEGIN\n"
		"CREATE TABLE SYS_TABLESPACES(\n"
		" SPACE INT, NAME CHAR, FLAGS INT);\n"
		"CREATE UNIQUE CLUSTERED INDEX SYS_TABLESPACES_SPACE"
		" ON SYS_TABLESPACES (SPACE);\n"
		"CREATE TABLE SYS_DATAFILES(\n"
		" SPACE INT, PATH CHAR);\n"
		"CREATE UNIQUE CLUSTERED INDEX SYS_DATAFILES_SPACE"
		" ON SYS_DATAFILES (SPACE);\n"
		"END;\n",
		FALSE, trx);

	if (err != DB_SUCCESS) {

		ib::error() << "Creation of SYS_TABLESPACES and SYS_DATAFILES"
			" has failed with error " << ut_strerr(err)
			<< ". Dropping incompletely created tables.";

		ut_a(err == DB_OUT_OF_FILE_SPACE
		     || err == DB_TOO_MANY_CONCURRENT_TRXS);

		row_drop_table_for_mysql("SYS_TABLESPACES", trx, TRUE);
		row_drop_table_for_mysql("SYS_DATAFILES", trx, TRUE);

		if (err == DB_OUT_OF_FILE_SPACE) {
			err = DB_MUST_GET_MORE_FILE_SPACE;
		}
	}

	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);

	srv_file_per_table = srv_file_per_table_backup;

	if (err == DB_SUCCESS) {
		ib::info() << "Tablespace and datafile system tables created.";
	}

	/* Note: The master thread has not been started at this point. */
	/* Confirm and move to the non-LRU part of the table LRU list. */

	sys_tablespaces_err = dict_check_if_system_table_exists(
		"SYS_TABLESPACES", DICT_NUM_FIELDS__SYS_TABLESPACES + 1, 1);
	ut_a(sys_tablespaces_err == DB_SUCCESS);

	sys_datafiles_err = dict_check_if_system_table_exists(
		"SYS_DATAFILES", DICT_NUM_FIELDS__SYS_DATAFILES + 1, 1);
	ut_a(sys_datafiles_err == DB_SUCCESS);

	return(err);
}

/** Put a tablespace definition into the data dictionary,
replacing what was there previously.
@param[in]	space	Tablespace id
@param[in]	name	Tablespace name
@param[in]	flags	Tablespace flags
@param[in]	path	Tablespace path
@param[in]	trx	Transaction
@param[in]	commit	If true, commit the transaction
@return error code or DB_SUCCESS */
dberr_t
dict_replace_tablespace_in_dictionary(
	ulint		space_id,
	const char*	name,
	ulint		flags,
	const char*	path,
	trx_t*		trx,
	bool		commit)
{
	if (!srv_sys_tablespaces_open) {
		/* Startup procedure is not yet ready for updates. */
		return(DB_SUCCESS);
	}

	dberr_t		error;

	pars_info_t*	info = pars_info_create();

	pars_info_add_int4_literal(info, "space", space_id);

	pars_info_add_str_literal(info, "name", name);

	pars_info_add_int4_literal(info, "flags", flags);

	pars_info_add_str_literal(info, "path", path);

	error = que_eval_sql(info,
			     "PROCEDURE P () IS\n"
			     "p CHAR;\n"

			     "DECLARE CURSOR c IS\n"
			     " SELECT PATH FROM SYS_DATAFILES\n"
			     " WHERE SPACE=:space FOR UPDATE;\n"

			     "BEGIN\n"
			     "OPEN c;\n"
			     "FETCH c INTO p;\n"

			     "IF (SQL % NOTFOUND) THEN"
			     "  DELETE FROM SYS_TABLESPACES "
			     "WHERE SPACE=:space;\n"
			     "  INSERT INTO SYS_TABLESPACES VALUES"
			     "(:space, :name, :flags);\n"
			     "  INSERT INTO SYS_DATAFILES VALUES"
			     "(:space, :path);\n"
			     "ELSIF p <> :path THEN\n"
			     "  UPDATE SYS_DATAFILES SET PATH=:path"
			     " WHERE CURRENT OF c;\n"
			     "END IF;\n"
			     "END;\n",
			     FALSE, trx);

	if (error != DB_SUCCESS) {
		return(error);
	}

	if (commit) {
		trx->op_info = "committing tablespace and datafile definition";
		trx_commit(trx);
	}

	trx->op_info = "";

	return(error);
}

/** Delete records from SYS_TABLESPACES and SYS_DATAFILES associated
with a particular tablespace ID.
@param[in]	space	Tablespace ID
@param[in,out]	trx	Current transaction
@return DB_SUCCESS if OK, dberr_t if the operation failed */

dberr_t
dict_delete_tablespace_and_datafiles(
	ulint		space,
	trx_t*		trx)
{
	dberr_t		err = DB_SUCCESS;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(srv_sys_tablespaces_open);

	trx->op_info = "delete tablespace and datafiles from dictionary";

	pars_info_t*	info = pars_info_create();
	ut_a(!is_system_tablespace(space));
	pars_info_add_int4_literal(info, "space", space);

	err = que_eval_sql(info,
			   "PROCEDURE P () IS\n"
			   "BEGIN\n"
			   "DELETE FROM SYS_TABLESPACES\n"
			   "WHERE SPACE = :space;\n"
			   "DELETE FROM SYS_DATAFILES\n"
			   "WHERE SPACE = :space;\n"
			   "END;\n",
			   FALSE, trx);

	if (err != DB_SUCCESS) {
		ib::warn() << "Could not delete space_id "
			<< space << " from data dictionary";
	}

	trx->op_info = "";

	return(err);
}

/** Assign a new table ID and put it into the table cache and the transaction.
@param[in,out]	table	Table that needs an ID
@param[in,out]	trx	Transaction */
void
dict_table_assign_new_id(
	dict_table_t*	table,
	trx_t*		trx)
{
	if (dict_table_is_intrinsic(table)) {
		/* There is no significance of this table->id (if table is
		intrinsic) so assign it default instead of something meaningful
		to avoid confusion.*/
		table->id = ULINT_UNDEFINED;
	} else {
		dict_hdr_get_new_id(&table->id, NULL, NULL, table, false);
	}

	trx->table_id = table->id;
}
