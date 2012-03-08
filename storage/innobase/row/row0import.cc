/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0import.cc
Import a tablespace to a running instance.

Created 2012-02-08 by Sunny Bains.
*******************************************************/

#include "row0import.h"

#ifdef UNIV_NONINL
#include "row0import.ic"
#endif

#include "btr0pcur.h"
#include "que0que.h"
#include "dict0boot.h"
#include "ibuf0ibuf.h"
#include "pars0pars.h"
#include "row0upd.h"
#include "row0sel.h"
#include "row0mysql.h"
#include "srv0start.h"
#include "row0quiesce.h"

/** Index information required by IMPORT. */
struct row_index_t {
	byte*		name;			/*!< Index name */

	ulint		space;			/*!< Space where it is placed */

	ulint		page_no;		/*!< Root page number */

	ulint		type;			/*!< Index type */

	ulint		trx_id_offset;		/*!< Relevant only for clustered
						indexes, offset of transaction
						id system column */

	ulint		n_user_defined_cols;	/*!< User defined columns */

	ulint		n_uniq;			/*!< Number of columns that can
						uniquely identify the row */

	ulint		n_nullable;		/*!< Number of nullable
						columns */

	ulint		n_fields;		/*!< Total number of fields */

	dict_field_t*	fields;			/*!< Index fields */
};

/** Meta data required by IMPORT. */
struct row_import_t {
	dict_table_t*	table;			/*!< Table instance */

	ulint		version;		/*!< Version of config file */

	byte*		hostname;		/*!< Hostname where the
						tablespace was exported */
	byte*		table_name;		/*!< Exporting instance table
						name */
	ulint		page_size;		/*!< Tablespace page size */

	ulint		flags;			/*!< Table flags */

	ulint		n_cols;			/*!< Number of columns in the
						meta-data file */

	dict_col_t*	cols;			/*!< Column data */

	byte**		col_names;		/*!< Column names, we store the
						column naems separately becuase
						there is no field to store the
						value in dict_col_t */

	ulint		n_indexes;		/*!< Number of indexes,
						including cluster index */

	row_index_t*	indexes;		/*!< Index meta data */
};

/** Class that imports indexes, both secondary and cluster. */
class IndexImporter {
public:
	/** Constructor
	@param trx - the user transaction covering the import tablespace
	@param index - to be imported
	@param space_id - space id of the tablespace */
	IndexImporter(
		trx_t*		trx,
		dict_index_t*	index,
		ulint		space_id) throw()
		:
		m_trx(trx),
		m_heap(0),
		m_index(index),
		m_n_recs(0),
		m_space_id(space_id)
	{
	}

	/** Descructor
	Free the heap if one was allocated during create offsets. */
	~IndexImporter() throw()
	{
		if (m_heap != 0) {
			mem_heap_free(m_heap);
		}
	}

	/** Scan the index and adjust sys fields, purge delete marked records
	and adjust BLOB references if it is a cluster index.
	@return DB_SUCCESS or error code. */
	db_err	import() throw()
	{
		db_err	err;
		ulint	offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*	offsets = offsets_;
		ibool	comp = dict_table_is_comp(m_index->table);

		rec_offs_init(offsets_);

		/* Open the persistent cursor and start the
		mini-transaction. */

		open();

		while ((err = next()) == DB_SUCCESS) {

			rec_t*	rec = btr_pcur_get_rec(&m_pcur);
			ibool	deleted = rec_get_deleted_flag(rec, comp);

			/* Skip secondary index rows that
			are not delete marked. */

			if (!deleted && !dict_index_is_clust(m_index)) {
				++m_n_recs;
			} else {

				offsets = update_offsets(rec, offsets);

				/* For the clustered index we have to adjust
				the BLOB reference and the system fields
				irrespective of the delete marked flag. The
				adjustment of delete marked cluster records
				is required for purge to work later. */

				err = adjust(rec, offsets, deleted);

				if (err != DB_SUCCESS) {
					break;
				}
			}
		}

		/* Close the persistent cursor and commit the
		mini-transaction. */

		close();

		return(err == DB_END_OF_INDEX ? DB_SUCCESS : err);
	}

	/** Gettor for m_n_recs.
	@return total records in the index after purge */
	ulint	get_n_recs() const throw()
	{
		return(m_n_recs);
	}

private:
	/** Begin import, position the cursor on the first record. */
	void	open() throw()
	{
		mtr_start(&m_mtr);

		btr_pcur_open_at_index_side(
			TRUE, m_index, BTR_MODIFY_LEAF, &m_pcur, TRUE, &m_mtr);
	}

	/** Close the persistent curosr and commit the mini-transaction. */
	void	close() throw()
	{
		btr_pcur_close(&m_pcur);
		mtr_commit(&m_mtr);
	}

	/** Update the column offsets w.r.t to rec.
	@param rec - new record
	@param offsets - current offsets
	@return offsets accociated with rec. */
	ulint*	update_offsets(const rec_t* rec, ulint* offsets) throw()
	{
		return(rec_get_offsets(
				rec, m_index, offsets, ULINT_UNDEFINED,
				&m_heap));
	}

	/** Position the cursor on the next record.
	@return DB_SUCCESS or error code */
	db_err	next() throw()
	{
		btr_pcur_move_to_next_on_page(&m_pcur);

		/* When switching pages, commit the mini-transaction
		in order to release the latch on the old page. */

		if (!btr_pcur_is_after_last_on_page(&m_pcur)) {
			return(DB_SUCCESS);
		} else if (trx_is_interrupted(m_trx)) {
			/* Check after every page because the check
			is expensive. */
			return(DB_INTERRUPTED);
		}

		btr_pcur_store_position(&m_pcur, &m_mtr);

		if (dict_index_is_clust(m_index)) {
			/* Write a dummy change to the page,
			so that it will be flagged dirty and
			the resetting of DB_TRX_ID and
			DB_ROLL_PTR will be written to disk. */

			mlog_write_ulint(
				FIL_PAGE_TYPE
				+ btr_pcur_get_page(&m_pcur),
				FIL_PAGE_INDEX,
				MLOG_2BYTES, &m_mtr);
		}

		mtr_commit(&m_mtr);

		mtr_start(&m_mtr);

		btr_pcur_restore_position(BTR_MODIFY_LEAF, &m_pcur, &m_mtr);

		if (!btr_pcur_move_to_next_user_rec(&m_pcur, &m_mtr)) {

			return(DB_END_OF_INDEX);
		}

		return(DB_SUCCESS);
	}

	/** Fix the BLOB column reference.
	@param page_zip - zip descriptor or 0
	@param rec - current rec
	@param offsets - offsets within current record
	@param i - column ordinal value
	@return DB_SUCCESS or error code */
	db_err	adjust_blob_column(
		page_zip_des_t*	page_zip,
		rec_t*		rec,
		const ulint*	offsets,
		ulint		i) throw()
	{
		ulint		len;
		byte*		field;

		ut_ad(dict_index_is_clust(m_index));

		field = rec_get_nth_field(rec, offsets, i, &len);

		if (len < BTR_EXTERN_FIELD_REF_SIZE) {

			char index_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				index_name, sizeof(index_name),
				m_index->name, TRUE);

			ib_pushf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				 ER_INDEX_CORRUPT,
				"Externally stored column(%lu) has a reference "
				"length of %lu in the index %s",
				(ulong) i, (ulong) len, index_name);

			return(DB_CORRUPTION);
		}

		field += BTR_EXTERN_SPACE_ID - BTR_EXTERN_FIELD_REF_SIZE + len;

		if (page_zip) {
			mach_write_to_4(field, m_space_id);

			page_zip_write_blob_ptr(
				page_zip, rec, m_index, offsets, i, &m_mtr);
		} else {
			mlog_write_ulint(
				field, m_space_id, MLOG_4BYTES, &m_mtr);
		}

		return(DB_SUCCESS);
	}

	/** Adjusts the BLOB pointers in the clustered index row.
	@param page_zip - zip descriptor or 0
	@param rec - current record
	@param offsets - column offsets within current record
	@return DB_SUCCESS or error code */
	db_err	adjust_blob_columns(
		page_zip_des_t*	page_zip,
		rec_t*		rec,
		const ulint*	offsets) throw()
	{
		ut_ad(dict_index_is_clust(m_index));

		ut_ad(rec_offs_any_extern(offsets));

		/* Adjust the space_id in the BLOB pointers. */

		for (ulint i = 0; i < rec_offs_n_fields(offsets); ++i) {

			/* Only if the column is stored "externally". */

			if (rec_offs_nth_extern(offsets, i)) {

				db_err	err = adjust_blob_column(
					page_zip, rec, offsets, i);

				if (err != DB_SUCCESS) {
					break;
				}
			}

		}

		return(DB_SUCCESS);
	}

	/** In the clustered index, adjusts DB_TRX_ID, DB_ROLL_PTR and
	BLOB pointers as needed.
	@return DB_SUCCESS or error code */
	db_err	adjust_clust_index(rec_t* rec, const ulint* offsets) throw()
	{
		ut_ad(dict_index_is_clust(m_index));

		page_zip_des_t*	page_zip;

		page_zip = buf_block_get_page_zip(btr_pcur_get_block(&m_pcur));

		if (rec_offs_any_extern(offsets)) {
			db_err		err;

			err = adjust_blob_columns(page_zip, rec, offsets);

			if (err != DB_SUCCESS) {
				return(err);
			}
		}

		/* Reset DB_TRX_ID and DB_ROLL_PTR.  This change is not covered
		by the redo log.  Normally, these fields are only written in
		conjunction with other changes to the record. However, when
		importing a tablespace, it is not necessary to log the writes
		until the dictionary transaction whose trx->table_id points to
		the table has been committed, as long as we flush the dirty
		blocks to the file before committing the transaction. */

		row_upd_rec_sys_fields(
			rec, page_zip, m_index, offsets, m_trx, 0);

		return(DB_SUCCESS);
	}

	/** Store the persistent cursor position and reopen the
	B-tree cursor in BTR_MODIFY_TREE mode, because the
	tree structure may be changed during a pessimistic delete. */
	void	purge_pessimistic_delete() throw()
	{
		ulint	err;

		btr_pcur_store_position(&m_pcur, &m_mtr);

		btr_pcur_restore_position(BTR_MODIFY_TREE, &m_pcur, &m_mtr);

		ut_ad(rec_get_deleted_flag(
				btr_pcur_get_rec(&m_pcur),
				dict_table_is_comp(m_index->table)));

		btr_cur_pessimistic_delete(
			&err, FALSE, btr_pcur_get_btr_cur(&m_pcur),
			RB_NONE, &m_mtr);

		ut_a(err == DB_SUCCESS);

		/* Reopen the B-tree cursor in BTR_MODIFY_LEAF mode */
		mtr_commit(&m_mtr);

		mtr_start(&m_mtr);

		btr_pcur_restore_position(BTR_MODIFY_LEAF, &m_pcur, &m_mtr);
	}

	/** Adjust the BLOB references and sys fields for the current record.
	@param rec - current row
	@param offsets - column offsets
	@param deleted - true if row is delete marked
	@return DB_SUCCESS or error code. */
	db_err	adjust(rec_t* rec, const ulint* offsets, bool deleted) throw()
	{
		/* Only adjust the system fields and BLOB pointers in the
		cluster index. */

		if (dict_index_is_clust(m_index)) {

			db_err	err;

			err = adjust_clust_index(rec, offsets);

			if (err != DB_SUCCESS) {
				return(err);
			}
		}

		/* We need to purge both secondary and clustered index. */

		if (deleted) {
			purge(offsets);
		} else {
			++m_n_recs;
		}

		return(DB_SUCCESS);
	}

	/** Purge delete-marked records.
	@param offsets - current row offsets. */
	void	purge(const ulint* offsets) throw()
	{
		/* Rows with externally stored columns have to be purged
		using the hard way. */

		if (rec_offs_any_extern(offsets)
		    || !btr_cur_optimistic_delete(
			    btr_pcur_get_btr_cur(&m_pcur), &m_mtr)) {

			purge_pessimistic_delete();
		}
	}

protected:
	// Disable copying
	IndexImporter(void) throw();
	IndexImporter(const IndexImporter&);
	IndexImporter &operator=(const IndexImporter&);

private:
	trx_t*			m_trx;		/*!< User transaction */
	mtr_t			m_mtr;		/*!< Mini-transaction */
	mem_heap_t*		m_heap;		/*!< Heap to use */
	btr_pcur_t		m_pcur;		/*!< Persistent cursor */
	dict_index_t*		m_index;	/*!< Index to be processed */
	ulint			m_n_recs;	/*!< Records in index */
	ulint			m_space_id;	/*!< Tablespace id */
						/*!< Column offsets */
};

/*****************************************************************//**
Free the config file memory representation. */
static	__attribute__((nonnull))
void
row_import_free(
/*============*/
	row_import_t*	cfg)		/*!< in/own: memory to free */
{
	for (ulint i = 0; i < cfg->n_indexes; ++i) {
		if (cfg->indexes[i].name != 0) {
			delete cfg->indexes[i].name;
			cfg->indexes[i].name = 0;
		}

		if (cfg->indexes[i].fields == 0) {
			continue;
		}

		ulint	n_fields = cfg->indexes[i].n_user_defined_cols;

		for (ulint j = 0; j < n_fields; ++j) {
			if (cfg->indexes[i].fields[j].name != 0) {
				delete cfg->indexes[i].fields[j].name;
				cfg->indexes[i].fields[j].name = 0;
			}
		}

		delete [] cfg->indexes[i].fields;
		cfg->indexes[i].fields = 0;
	}

	for (ulint i = 0; i < cfg->n_cols; ++i) {
		if (cfg->col_names[i] != 0) {
			delete cfg->col_names[i];
			cfg->col_names[i] = 0;
		}
	}

	if (cfg->cols != 0) {
		delete [] cfg->cols;
		cfg->cols = 0;
	}

	if (cfg->indexes != 0) {
		delete [] cfg->indexes;
		cfg->indexes = 0;
	}

	if (cfg->col_names != 0) {
		delete [] cfg->col_names;
		cfg->col_names = 0;
	}

	delete cfg;
}

/*****************************************************************//**
Cleanup after import tablespace. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_cleanup(
/*===============*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt from handler */
	trx_t*		trx,		/*!< in/out: transaction for import */
	db_err		err)		/*!< in: error code */
{
	ut_a(prebuilt->trx != trx);
	ut_ad(trx_get_dict_operation(trx) == TRX_DICT_OP_TABLE);

	if (err != DB_SUCCESS) {
		dict_table_t*	table = prebuilt->table;

		prebuilt->trx->error_info = NULL;

		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name),
			prebuilt->table->name, FALSE);

		ib_logf(IB_LOG_LEVEL_INFO,
			"Discarding tablespace of table %s", table_name);

		row_mysql_lock_data_dictionary(trx);

		/* Since we update the index root page numbers on disk after
		we've done a successful import. The table will not be loadable.
		However, we need to ensure that the in memory root page numbers
		are reset to "NULL". */

		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != 0;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			index->page = FIL_NULL;
			index->space = FIL_NULL;
		}

		table->ibd_file_missing = TRUE;

		row_mysql_unlock_data_dictionary(trx);
	}

	trx_commit_for_mysql(trx);
	trx_free_for_mysql(trx);
	prebuilt->trx->op_info = "";

	return(err);
}

/*****************************************************************//**
Report error during tablespace import. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_error(
/*=============*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt from handler */
	trx_t*		trx,		/*!< in/out: transaction for import */
	db_err		err)		/*!< in: error code */
{
	if (!trx_is_interrupted(trx)) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name),
			prebuilt->table->name, FALSE);

		ib_pushf(trx->mysql_thd,
			 IB_LOG_LEVEL_WARN,
			 ER_ALTER_INFO,
			 "ALTER TABLE %s IMPORT TABLESPACE failed: %lu : %s",
			 table_name, (ulong) err, ut_strerr(err));
	}

	return(row_import_cleanup(prebuilt, trx, err));
}

/*****************************************************************//**
Adjust the root on the table's secondary indexes.
@return error code */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_adjust_root_pages(
/*=========================*/
	row_prebuilt_t*		prebuilt,	/*!< in/out: prebuilt from
						handler */
	trx_t*			trx,		/*!< in: transaction used for
						the import */
	dict_table_t*		table,		/*!< in: table the indexes
						belong to */
	ulint			n_rows_in_table)/*!< in: number of rows
						left in index */
{
	dict_index_t*		index;
	db_err			err = DB_SUCCESS;

	DBUG_EXECUTE_IF("ib_import_sec_rec_count_mismatch_failure",
			n_rows_in_table++;);

	/* Skip the cluster index. */
	index = dict_table_get_first_index(table);

	/* Adjust the root pages of the secondary indexes only. */
	while ((index = dict_table_get_next_index(index)) != NULL) {
		ulint		n_rows;
		char		index_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			index_name, sizeof(index_name), index->name, TRUE);

		ut_a(!dict_index_is_clust(index));

		err = btr_root_adjust_on_import(index);

		if (err == DB_SUCCESS
		    && !btr_validate_index(index, trx, true)) {

			err = DB_CORRUPTION;
		}

		if (err != DB_SUCCESS) {
			ib_pushf(trx->mysql_thd,
				IB_LOG_LEVEL_WARN,
				ER_INDEX_CORRUPT,
				"Index %s import failed. Corruption detected "
				"during root page update", index_name);
			break;
		}

		IndexImporter	importer(trx, index, table->space);

		trx->op_info = "importing secondary index";

		err = importer.import();

		trx->op_info = "";

		if (err != DB_SUCCESS) {
			break;
		} else if (importer.get_n_recs() != n_rows_in_table) {

			ib_pushf(trx->mysql_thd,
				IB_LOG_LEVEL_WARN,
				ER_INDEX_CORRUPT,
				"Index %s contains %lu entries, should be "
				"%lu, you should recreate this index.",
				index_name,
				(ulong) n_rows,
				(ulong) n_rows_in_table);

			/* Do not bail out, so that the data
			can be recovered. */

			err = DB_SUCCESS;
		}
	}

	return(err);
}

/*****************************************************************//**
Ensure that dict_sys->row_id exceeds SELECT MAX(DB_ROW_ID).
@return error code */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_set_sys_max_row_id(
/*==========================*/
	row_prebuilt_t*		prebuilt,	/*!< in/out: prebuilt from
						handler */
	const dict_table_t*	table)		/*!< in: table to import */
{
	db_err			err;
	const rec_t*		rec;
	mtr_t			mtr;
	btr_pcur_t		pcur;
	row_id_t		row_id	= 0;
	dict_index_t*		index;

	index = dict_table_get_first_index(table);
	ut_a(dict_index_is_clust(index));

	mtr_start(&mtr);

	btr_pcur_open_at_index_side(
		FALSE,		// High end
		index,
		BTR_SEARCH_LEAF,
		&pcur,
		TRUE,		// Init cursor
		&mtr);

	btr_pcur_move_to_prev_on_page(&pcur);
	rec = btr_pcur_get_rec(&pcur);

	/* Check for empty table. */
	if (!page_rec_is_infimum(rec)) {
		ulint		len;
		const byte*	field;
		mem_heap_t*	heap = NULL;
		ulint		offsets_[1 + REC_OFFS_HEADER_SIZE];
		ulint*		offsets;

		rec_offs_init(offsets_);

		offsets = rec_get_offsets(
			rec, index, offsets_, ULINT_UNDEFINED, &heap);

		field = rec_get_nth_field(
			rec, offsets,
			dict_index_get_sys_col_pos(index, DATA_ROW_ID),
			&len);

		if (len == DATA_ROW_ID_LEN) {
			row_id = mach_read_from_6(field);
			err = DB_SUCCESS;
		} else {
			err = DB_CORRUPTION;
		}

		if (heap != NULL) {
			mem_heap_free(heap);
		}
	} else {
		/* The table is empty. */
		err = DB_SUCCESS;
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	DBUG_EXECUTE_IF("ib_import_set_max_rowid_failure",
			err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		char		index_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			index_name, sizeof(index_name), index->name, TRUE);

		ib_pushf(prebuilt->trx->mysql_thd,
			 IB_LOG_LEVEL_WARN,
			 ER_INDEX_CORRUPT,
			 "Index %s corruption detected, invalid row id "
			 "in index.", index_name);

		return(err);

	} else if (row_id > 0) {

		/* Update the system row id if the imported index row id is
		greater than the max system row id. */

		mutex_enter(&dict_sys->mutex);

		if (row_id >= dict_sys->row_id) {
			dict_sys->row_id = row_id + 1;
			dict_hdr_flush_row_id();
		}

		mutex_exit(&dict_sys->mutex);
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Read the a string from the meta data file.
@return DB_SUCCESS or error code. */
static
db_err
row_import_cfg_read_string(
/*=======================*/
	FILE*		file,		/*!< in/out: File to read from */
	byte*		ptr,		/*!< out: string to read */
	ulint		max_len)	/*!< in: max size of buffer, this
					is also the expected length */
{
	ulint		len = 0;

	while (!feof(file)) {
		int	ch = fgetc(file);

		if (ch == EOF) {
			break;
		} else if (ch != 0) {
			if (len < max_len) {
				ptr[len++] = ch;
			} else {
				break;
			}
		/* max_len includes the NUL byte */
		} else if (len != max_len - 1) {
			break;
		} else {
			ptr[len] = 0;
			return(DB_SUCCESS);
		}
	}

	return(DB_IO_ERROR);
}

/*********************************************************************//**
Write the meta data (index user fields) config file.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_cfg_read_index_fields(
/*=============================*/
	FILE*			file,	/*!< in: file to write to */
	void*			thd,	/*!< in/out: session */
	row_index_t*		index,	/*!< Index being read in */
	row_import_t*		cfg)	/*!< in/out: meta-data read */
{
	byte			row[sizeof(ib_uint32_t) * 3];
	ulint			n_fields = index->n_fields;

	index->fields = new(std::nothrow) dict_field_t[n_fields];

	if (index->fields == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	dict_field_t*	field = index->fields;

	memset(field, 0x0, sizeof(dict_field_t) * n_fields);

	for (ulint i = 0; i < n_fields; ++i, ++field) {
		byte*		ptr = row;

		if (fread(row, 1, sizeof(row), file) != sizeof(row)) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_READ_ERROR,
				 "I/O error (%lu), while reading index fields.",
				 (ulint) errno);

			return(DB_IO_ERROR);
		}

		field->prefix_len = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		field->fixed_len = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		/* Include the NUL byte in the length. */
		ulint	len = mach_read_from_4(ptr);

		byte*	name = new(std::nothrow) byte[len];

		if (name == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		field->name = reinterpret_cast<const char*>(name);

		db_err	err = row_import_cfg_read_string(file, name, len);

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Read the index names and root page numbers of the indexes and set the values.
Row format [root_page_no, len of str, str ... ]
@return DB_SUCCESS or error code. */
static __attribute__((nonnull, warn_unused_result))
db_err
row_import_read_index_data(
/*=======================*/
	FILE*		file,		/*!< in: File to read from */
	void*		thd,		/*!< in: session */
	row_import_t*	cfg)		/*!< in/out: meta-data read */
{
	byte*		ptr;
	row_index_t*	cfg_index;
	byte		row[sizeof(ib_uint32_t) * 9];

	/* FIXME: What is the max value? */
	ut_a(cfg->n_indexes > 0);
	ut_a(cfg->n_indexes < 1024);

	cfg->indexes = new(std::nothrow) row_index_t[cfg->n_indexes];

	if (cfg->indexes == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	memset(cfg->indexes, 0x0, sizeof(cfg->indexes) * cfg->n_indexes);

	cfg_index = cfg->indexes;

	for (ulint i = 0; i < cfg->n_indexes; ++i, ++cfg_index) {

		/* Read the index data. */
		if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				"I/O error (%lu) while reading index "
				"meta-data", (ulint) errno);

			return(DB_IO_ERROR);
		}

		ptr = row;

		cfg_index->space = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->page_no = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->type = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->trx_id_offset = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->n_user_defined_cols = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->n_uniq = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->n_nullable = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->n_fields = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		/* The NUL byte is included in the name length. */
		ulint	len = mach_read_from_4(ptr);

		if (len > OS_FILE_MAX_PATH) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
				 "Index name length (%lu) is too long, "
				 "the meta-data is corrupt", len);

			return(DB_CORRUPTION);
		}

		cfg_index->name = new(std::nothrow) byte[len];

		if (cfg_index->name == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		db_err	err;

		err = row_import_cfg_read_string(file, cfg_index->name, len);

		if (err != DB_SUCCESS) {
			return(err);
		}

		err = row_import_cfg_read_index_fields(
			file, thd, cfg_index, cfg);

		if (err != DB_SUCCESS) {
			return(err);
		}

	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Set the index root page number for v1 format.
@return DB_SUCCESS or error code. */
static
db_err
row_import_read_indexes(
/*====================*/
	FILE*		file,		/*!< in: File to read from */
	void*		thd,		/*!< in: session */
	row_import_t*	cfg)		/*!< in/out: meta-data read */
{
	byte		row[sizeof(ib_uint32_t)];

	/* Read the number of indexes. */
	if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			 "I/O error (%lu) while reading number of indexes.",
			 (ulint) errno);

		return(DB_IO_ERROR);
	}

	cfg->n_indexes = mach_read_from_4(row);

	if (cfg->n_indexes == 0) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			 "Number of indexes in meta-data file is 0");

		return(DB_CORRUPTION);

	} else if (cfg->n_indexes > 1024) {
		// FIXME: What is the upper limit? */
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			 "Number of indexes in meta-data file is too high: %lu",
			 (ulong) cfg->n_indexes);

		return(DB_CORRUPTION);
	}

	return(row_import_read_index_data(file, thd, cfg));
}

/*********************************************************************//**
Read the meta data (table columns) config file. Deserialise the contents of
dict_col_t structure, along with the column name. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_read_columns(
/*====================*/
	FILE*			file,	/*!< in: file to write to */
	void*			thd,	/*!< in/out: session */
	row_import_t*		cfg)	/*!< in/out: meta-data read */
{
	dict_col_t*		col;
	byte			row[sizeof(ib_uint32_t) * 8];

	/* FIXME: What should the upper limit be? */
	ut_a(cfg->n_cols > 0);
	ut_a(cfg->n_cols < 1024);

	cfg->cols = new(std::nothrow) dict_col_t[cfg->n_cols];

	if (cfg->cols == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	cfg->col_names = new(std::nothrow) byte* [cfg->n_cols];

	if (cfg->col_names == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	memset(cfg->cols, 0x0, sizeof(cfg->cols) * cfg->n_cols);
	memset(cfg->col_names, 0x0, sizeof(cfg->col_names) * cfg->n_cols);

	col = cfg->cols;

	for (ulint i = 0; i < cfg->n_cols; ++i, ++col) {
		byte*		ptr = row;

		if (fread(row, 1,  sizeof(row), file) != sizeof(row)) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_READ_ERROR,
				 "I/O error (%lu), while reading table column "
				 "meta-data.", (ulint) errno);

			return(DB_IO_ERROR);
		}

		col->prtype = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		col->mtype = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		col->len = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		col->mbminmaxlen = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		col->ind = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		col->ord_part = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		col->max_prefix = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		/* Read in the column name as [len, byte array]. The len
		includes the NUL byte. */

		ulint		len = mach_read_from_4(ptr);

		/* FIXME: What is the maximum column name length? */
		if (len == 0 || len > 128) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_EXCEPTIONS_WRITE_ERROR,
				 "Column name length %lu, is invalid",
				 (ulong) len);

			return(DB_CORRUPTION);
		}

		cfg->col_names[i] = new(std::nothrow) byte[len];

		if (cfg->col_names[i] == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		db_err	err;

		err = row_import_cfg_read_string(file, cfg->col_names[i], len);

		if (err != DB_SUCCESS) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_INDEX_CORRUPT,
				 "While reading table column name: %s.",
				 ut_strerr(err));
			return(err);
		}
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Read the contents of the <tablespace>.cfg file.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_read_v1(
/*===============*/
	FILE*		file,		/*!< in: File to read from */
	void*		thd,		/*!< in: session */
	row_import_t*	cfg)		/*!< out: meta data */
{
	byte		value[sizeof(ib_uint32_t)];

	/* Read the hostname where the tablespace was exported. */
	if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
		ib_pushf(thd, IB_LOG_LEVEL_WARN, ER_IO_READ_ERROR,
			"I/O error (%lu) while reading meta-data export "
			"hostname length.", (ulint) errno);

		return(DB_IO_ERROR);
	}

	ulint	len = mach_read_from_4(value);

	/* NUL byte is part of name length. */
	cfg->hostname = new(std::nothrow) byte[len];

	if (cfg->hostname == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	db_err	err = row_import_cfg_read_string(file, cfg->hostname, len);

	if (err != DB_SUCCESS) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			 "While reading export hostname: %s.", ut_strerr(err));

		return(err);
	}

	/* Read the table name of tablespace that was exported. */
	if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
		ib_pushf(thd, IB_LOG_LEVEL_WARN, ER_IO_READ_ERROR,
			"I/O error (%lu) while reading meta-data "
			"table name length.", (ulint) errno);

		return(DB_IO_ERROR);
	}

	len = mach_read_from_4(value);

	/* NUL byte is part of name length. */
	cfg->table_name = new(std::nothrow) byte[len];

	if (cfg->table_name == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	err = row_import_cfg_read_string(file, cfg->table_name, len);

	if (err != DB_SUCCESS) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			 "While reading table name: %s.", ut_strerr(err));

		return(err);
	}

	ib_logf(IB_LOG_LEVEL_INFO,
		"Importing tablespace for table %s that was exported "
		"from host %s", cfg->table_name, cfg->hostname);

	byte		row[sizeof(ib_uint32_t) * 3];

	/* Read the tablespace page size. */
	if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			 "I/O error (%lu) while reading meta-data header.",
			 (ulint) errno);

		return(DB_IO_ERROR);
	}

	byte*		ptr = row;

	cfg->page_size = mach_read_from_4(ptr);
	ptr += sizeof(ib_uint32_t);

	if (cfg->page_size != UNIV_PAGE_SIZE) {

		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
			"Tablespace to be imported has a different "
			"page size than this server. Server page size "
			"is %lu, whereas tablespace page size is %lu",
			UNIV_PAGE_SIZE, (ulong) cfg->page_size);

		return(DB_ERROR);
	}

	cfg->flags = mach_read_from_4(ptr);
	ptr += sizeof(ib_uint32_t);

	cfg->n_cols = mach_read_from_4(ptr);

	if (!dict_tf_valid(cfg->flags)) {
		return(DB_CORRUPTION);
	} if ((err = row_import_read_columns(file, thd, cfg)) != DB_SUCCESS) {
		return(err);
	} if ((err = row_import_read_indexes(file, thd, cfg)) != DB_SUCCESS) {
		return(err);
	}

	ut_a(err == DB_SUCCESS);
	return(err);
}

/*****************************************************************//**
Read the contents of the <tablespace>.cfg file.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_read_meta_data(
/*======================*/
	dict_table_t*	table,		/*!< in: table */
	FILE*		file,		/*!< in: File to read from */
	void*		thd,		/*!< in: session */
	row_import_t**	cfg)		/*!< out: contents of the .cfg file */
{
	byte		row[sizeof(ib_uint32_t)];

	*cfg = 0;

	if (fread(&row, 1, sizeof(row), file) != sizeof(row)) {
		ib_pushf(thd, IB_LOG_LEVEL_WARN, ER_IO_READ_ERROR,
			"I/O error (%lu) while reading meta-data version",
			(ulint) errno);

		return(DB_IO_ERROR);
	}

	*cfg = new(std::nothrow) row_import_t;

	if (*cfg == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	memset(*cfg, 0x0, sizeof(**cfg));

	(*cfg)->version = mach_read_from_4(row);

	/* Check the version number. */
	switch ((*cfg)->version) {
	case IB_EXPORT_CFG_VERSION_V1:

		(*cfg)->table = table;

		return(row_import_read_v1(file, thd, *cfg));
	default:
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			"Unsupported meta-data version number (%lu), "
			"file ignored", (ulong) (*cfg)->version);
	}

	return(DB_ERROR);
}

/*****************************************************************//**
Read the contents of the <tablename>.cfg file.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_read_cfg(
/*================*/
	dict_table_t*	table,	/*!< in: table */
	void*		thd,	/*!< in: session */
	row_import_t**	cfg)	/*!< out: contents of the .cfg file */
{
	db_err		err;
	char		name[OS_FILE_MAX_PATH];

	srv_get_meta_data_filename(table, name, sizeof(name));

	FILE*	file = fopen(name, "r");

	if (file == NULL) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			 "Error opening: %s", name);
		err = DB_IO_ERROR;
	} else {
		err = row_import_read_meta_data(table, file, thd, cfg);
		fclose(file);
	}

	return(err);
}

/*****************************************************************//**
Find the ordinal value of the column name in the cfg table columns.
@return ULINT_UNDEFINED if not found. */
static	__attribute__((nonnull, warn_unused_result))
ulint
row_import_find_col(
/*================*/
	const row_import_t*	cfg,		/*!< in: contents of the
						.cfg file */
	const char*		name)		/*!< in: name of column to
						look for */
{
	for (ulint i = 0; i < cfg->n_cols; ++i) {

		if (strcmp(reinterpret_cast<const char*>(cfg->col_names[i]),
			   name) == 0) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/*****************************************************************//**
Find the index entry in in the cfg indexes.
@return instance if found else 0. */
static	__attribute__((nonnull, warn_unused_result))
const row_index_t*
row_import_find_index(
/*==================*/
	const row_import_t*	cfg,		/*!< in: contents of the
						.cfg file */
	const char*		name)		/*!< in: name of index to
						look for */
{
	for (ulint i = 0; i < cfg->n_indexes; ++i) {
		const row_index_t*	cfg_index = &cfg->indexes[i];

		if (strcmp(reinterpret_cast<const char*>(cfg_index->name),
			   name) == 0) {

			return(cfg_index);
		}
	}

	return(0);
}

/*****************************************************************//**
Find the index field entry in in the cfg indexes fields.
@return instance if found else 0. */
static	__attribute__((nonnull, warn_unused_result))
const dict_field_t*
row_import_find_field(
/*==================*/
	const row_index_t*	cfg_index,	/*!< in: index definition read
						from the .cfg file */
	const char*		name)		/*!< in: name of index field to
						look for */
{
	const dict_field_t*	field = cfg_index->fields;

	for (ulint i = 0; i < cfg_index->n_fields; ++i, ++field) {
		if (strcmp(reinterpret_cast<const char*>(field->name),
			   name) == 0) {

			return(field);
		}
	}

	return(0);
}

/*****************************************************************//**
Check if the index schema that was read from the .cfg file matches the
in memory index definition.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_match_index_columns(
/*===========================*/
	void*			thd,		/*!< in: session */
	const row_import_t*	cfg,		/*!< in: contents of the
						.cfg file */
	const dict_index_t*	index)		/*!< in: index to match */
{
	const row_index_t*	cfg_index;
	db_err			err = DB_SUCCESS;

	cfg_index = row_import_find_index(cfg, index->name);

	if (cfg_index == 0) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR,
			 ER_TABLE_SCHEMA_MISMATCH,
			 "Index %s not found in tablespace meta-data file.",
			 index->name);

		return(DB_ERROR);
	}

	const dict_field_t*	field = index->fields;

	for (ulint i = 0; i < index->n_fields; ++i, ++field) {

		const dict_field_t*	cfg_field;

		cfg_field = row_import_find_field(cfg_index, field->name);

		if (cfg_field == 0) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Index %s field %s not found in tablespace "
				 "meta-data file.",
				 index->name, field->name);

			err = DB_ERROR;
		} else {

			if (cfg_field->prefix_len != field->prefix_len) {
				ib_pushf(thd, IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Index %s field %s prefix len %lu "
					 "doesn't match meta-data file value "
					 "%lu",
					 index->name, field->name,
					 (ulong) field->prefix_len,
					 (ulong) cfg_field->prefix_len);

				err = DB_ERROR;
			}

			if (cfg_field->fixed_len != field->fixed_len) {
				ib_pushf(thd, IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Index %s field %s fixed len %lu "
					 "doesn't match meta-data file value "
					 "%lu",
					 index->name, field->name,
					 (ulong) field->fixed_len,
					 (ulong) cfg_field->fixed_len);

				err = DB_ERROR;
			}
		}
	}

	return(err);
}

/*****************************************************************//**
Check if the table schema that was read from the .cfg file matches the
in memory table definition.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_import_match_table_columns(
/*===========================*/
	void*			thd,		/*!< in: session */
	const row_import_t*	cfg)		/*!< in: contents of the
						.cfg file */
{
	db_err			err = DB_SUCCESS;
	const dict_col_t*	col = cfg->table->cols;

	for (ulint i = 0; i < cfg->table->n_cols; ++i, ++col) {

		const char*	col_name;
		ulint		cfg_col_index;

		col_name = dict_table_get_col_name(
			cfg->table, dict_col_get_no(col));

		cfg_col_index = row_import_find_col(cfg, col_name);

		if (cfg_col_index == ULINT_UNDEFINED) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Column %s not found in tablespace.",
				 col_name);

			err = DB_ERROR;
		} else if (cfg_col_index != col->ind) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Column %s ordinal value mismatch, it's at "
				 "%lu in the table and %lu in the tablespace "
				 "meta-data file",
				 col_name,
				 (ulong) col->ind, (ulong) cfg_col_index);

			err = DB_ERROR;
		} else {
			const dict_col_t*	cfg_col;

			cfg_col = &cfg->cols[cfg_col_index];
			ut_a(cfg_col->ind == cfg_col_index);

			if (cfg_col->prtype != col->prtype) {
				ib_pushf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s precise type mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->mtype != col->mtype) {
				ib_pushf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s main type mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->len != col->len) {
				ib_pushf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s length mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->mbminmaxlen != col->mbminmaxlen) {
				ib_pushf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s multi-byte len mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->ind != col->ind) {
				err = DB_ERROR;
			}

			if (cfg_col->ord_part != col->ord_part) {
				ib_pushf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s ordering mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->max_prefix != col->max_prefix) {
				ib_pushf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s max prefix mismatch.",
					 col_name);
				err = DB_ERROR;
			}
		}
	}

	return(err);
}

/*****************************************************************//**
Check if the table (and index) schema that was read from the .cfg file
matches the in memory table definition.
@return DB_SUCCESS or error code. */
static
db_err
row_import_match_schema(
/*====================*/
	void*			thd,		/*!< in: session */
	const row_import_t*	cfg)		/*!< in: contents of the
						.cfg file */
{

	/* Do some simple checks. */

	if (cfg->table->n_cols != cfg->n_cols) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
			 "Number of columns don't match, table has %lu "
			 "columns but the tablespace meta-data file has "
			 "%lu columns",
			 (ulong) cfg->table->n_cols, (ulong) cfg->n_cols);

		return(DB_ERROR);
	} else if (UT_LIST_GET_LEN(cfg->table->indexes) != cfg->n_indexes) {

		/* If the number of indexes don't match then it is better
		to abort the IMPORT. It is easy for the user to create a
		table matching the IMPORT definition. */

		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
			 "Number of indexes don't match, table has %lu "
			 "indexes but the tablespace meta-data file has "
			 "%lu indexes",
			 (ulong) UT_LIST_GET_LEN(cfg->table->indexes),
			 (ulong) cfg->n_indexes);

		return(DB_ERROR);
	}

	db_err	err = row_import_match_table_columns(thd, cfg);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Check if the index definitions match. */

	for (const dict_index_t* index = UT_LIST_GET_FIRST(cfg->table->indexes);
	     index != 0;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		db_err	index_err;

		index_err = row_import_match_index_columns(thd, cfg, index);

		if (index_err != DB_SUCCESS) {
			err = index_err;
		}
	}

	return(err);
}

/*****************************************************************//**
Set the index root <space, pageno> from the meta-data.
@return DB_SUCCESS or error code. */
static
db_err
row_import_index_set_root(
/*======================*/
	void*		thd,		/*!< in: session */
	row_import_t*	cfg)		/*!< out: contents of the .cfg file */
{
	db_err		err = DB_SUCCESS;
	row_index_t*	cfg_index = &cfg->indexes[0];

	for (ulint i = 0; i < cfg->n_indexes; ++i, ++cfg_index) {
		dict_index_t*	index;

		ut_ad(i < cfg->n_indexes);

		index = dict_table_get_index_on_name(
			cfg->table,
			reinterpret_cast<const char*>(cfg_index->name));

		/* We've already checked that it exists. */
		ut_a(index != 0);
		index->space = cfg->table->space;
		index->page = cfg_index->page_no;
	}

	return(err);
}

/*****************************************************************//**
Update the <space, root page> of a table's indexes from the values
in the data dictionary.
@return DB_SUCCESS or error code */
UNIV_INTERN
db_err
row_import_update_index_root(
/*=========================*/
	trx_t*			trx,		/*!< in/out: transaction that
						covers the update */
	const dict_table_t*	table,		/*!< in: Table for which we want
						to set the root page_no */
	bool			reset,		/*!< if true then set to
						FIL_NUL */
	bool			dict_locked)	/*!< Set to TRUE if the 
						caller already owns the 
						dict_sys_t:: mutex. */

{
	const dict_index_t*	index;
	que_t*			graph = 0;
	db_err			err = DB_SUCCESS;

	static const char	sql[] = {
		"PROCEDURE UPDATE_INDEX_ROOT() IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES\n"
		"SET SPACE = :space,\n"
		"    PAGE_NO = :page\n"
		"WHERE TABLE_ID = :table_id AND ID = :index_id;\n"
		"END;\n"};

	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	for (index = dict_table_get_first_index(table);
	     index != 0;
	     index = dict_table_get_next_index(index)) {

		pars_info_t*	info;
		ib_uint32_t	page;
		ib_uint32_t	space;
		index_id_t	index_id;
		table_id_t	table_id;

		info = (graph != 0) ? graph->info : pars_info_create();

		mach_write_to_4(
			reinterpret_cast<byte*>(&page),
		       	reset ? FIL_NULL : index->page);

		mach_write_to_4(
			reinterpret_cast<byte*>(&space),
		       	reset ? FIL_NULL : index->space);

		mach_write_to_8(
			reinterpret_cast<byte*>(&index_id),
			index->id);

		mach_write_to_8(
			reinterpret_cast<byte*>(&table_id),
			table->id);

		pars_info_bind_int4_literal(info, "space", &space);
		pars_info_bind_int4_literal(info, "page", &page);
		pars_info_bind_ull_literal(info, "index_id", &index_id);
		pars_info_bind_ull_literal(info, "table_id", &table_id);

		if (graph == 0) {
			graph = pars_sql(info, sql);
			ut_a(graph);
			graph->trx = trx;
		}

		que_thr_t*	thr;

		graph->fork_type = QUE_FORK_MYSQL_INTERFACE;

		ut_a(thr = que_fork_start_command(graph));

		que_run_threads(thr);

		err = trx->error_state;

		if (err != DB_SUCCESS) {
			char index_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				index_name, sizeof(index_name),
				index->name, TRUE);

			ib_pushf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				 ER_INTERNAL_ERROR,
				 "While updating the <space, root page number> "
				 "of index %s - %s",
				 index_name, ut_strerr(err));

			break;
		}
	}

	que_graph_free(graph);

	if (!dict_locked) {
		mutex_exit(&dict_sys->mutex);
	}

	return(err);
}

/** Callback arg for row_import_set_discarded. */
struct discard_t {
	ib_uint32_t	flags2;			/*!< Value read from column */
	bool		state;			/*!< New state of the flag */
	ulint		n_recs;			/*!< Number of recs processed */
};

/******************************************************************//**
Fetch callback that sets or unsets the DISCARDED tablespace flag in
SYS_TABLES. The flags is stored in MIX_LEN column.
@return FALSE if all OK */
static
ibool
row_import_set_discarded(
/*=====================*/
	void*		row,			/*!< in: sel_node_t* */
	void*		user_arg)		/*!< in: bool set/unset flag */
{
	sel_node_t*	node = static_cast<sel_node_t*>(row);
	discard_t*	discard = static_cast<discard_t*>(user_arg);
	dfield_t*	dfield = que_node_get_val(node->select_list);
	dtype_t*	type = dfield_get_type(dfield);
	ulint		len = dfield_get_len(dfield);

	ut_a(dtype_get_mtype(type) == DATA_INT);
	ut_a(len == sizeof(ib_uint32_t));

	ulint	flags2 = mach_read_from_4(
		static_cast<byte*>(dfield_get_data(dfield)));

	if (discard->state) {
		flags2 |= DICT_TF2_DISCARDED;
	} else {
		flags2 &= ~DICT_TF2_DISCARDED;
	}

	mach_write_to_4(reinterpret_cast<byte*>(&discard->flags2), flags2);

	++discard->n_recs;

	/* There should be at most one matching record. */
	ut_a(discard->n_recs == 1);

	return(FALSE);
}

/*****************************************************************//**
Update the DICT_TF2_DISCARDED flag in SYS_TABLES.
@return DB_SUCCESS or error code. */
UNIV_INTERN
db_err
row_import_update_discarded_flag(
/*=============================*/
	trx_t*		trx,		/*!< in/out: transaction that
					covers the update */
	table_id_t	table_id,	/*!< in: Table for which we want
					to set the root table->flags2 */
	bool		discarded,	/*!< in: set MIX_LEN column bit
					to discarded, if true */
	bool		dict_locked)	/*!< Set to TRUE if the 
					caller already owns the 
					dict_sys_t:: mutex. */

{
	pars_info_t*		info;
	discard_t		discard;

	static const char	sql[] =
		"PROCEDURE UPDATE_DISCARDED_FLAG() IS\n"
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS\n"
		" SELECT MIX_LEN "
		" FROM SYS_TABLES "
		" WHERE ID = :table_id FOR UPDATE;"
		"\n"
		"BEGIN\n"
		"OPEN c;\n"
		"WHILE 1 = 1 LOOP\n"
		"  FETCH c INTO my_func();\n"
		"  IF c % NOTFOUND THEN\n"
		"    EXIT;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"UPDATE SYS_TABLES"
		" SET MIX_LEN = :flags2"
		" WHERE ID = :table_id;\n"
		"CLOSE c;\n"
		"END;\n";

	discard.n_recs = 0;
	discard.state = discarded;
	discard.flags2 = ULINT32_UNDEFINED;

	info = pars_info_create();

	pars_info_add_ull_literal(info, "table_id", table_id);
	pars_info_bind_int4_literal(info, "flags2", &discard.flags2);

	pars_info_bind_function(
		info, "my_func", row_import_set_discarded, &discard);

	db_err	err = que_eval_sql(info, sql, !dict_locked, trx);

	ut_a(discard.n_recs == 1);
	ut_a(discard.flags2 != ULINT32_UNDEFINED);
	
	return(err);
}

/*****************************************************************//**
Imports a tablespace. The space id in the .ibd file must match the space id
of the table in the data dictionary.
@return	error code or DB_SUCCESS */
UNIV_INTERN
db_err
row_import_for_mysql(
/*=================*/
	dict_table_t*	table,		/*!< in/out: table */
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL */
{
	db_err		err;
	trx_t*		trx;
	lsn_t		current_lsn;
	ulint		n_rows_in_table;
	char		table_name[MAX_FULL_NAME_LEN + 1];

	innobase_format_name(
		table_name, sizeof(table_name), table->name, FALSE);

	ut_a(table->space);
	ut_ad(prebuilt->trx);
	ut_a(table->ibd_file_missing);

	trx_start_if_not_started(prebuilt->trx);

	trx = trx_allocate_for_mysql();

	++trx->will_lock;

	/* So that we can send error messages to the user. */
	trx->mysql_thd = prebuilt->trx->mysql_thd;

	trx_start_if_not_started(trx);

	/* Ensure that the table will be dropped by trx_rollback_active()
	in case of a crash. */

	trx->table_id = table->id;
	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	/* Assign an undo segment for the transaction, so that the
	transaction will be recovered after a crash. */

	mutex_enter(&trx->undo_mutex);

	err = (db_err) trx_undo_assign_undo(trx, TRX_UNDO_UPDATE);

	mutex_exit(&trx->undo_mutex);

	DBUG_EXECUTE_IF("ib_import_undo_assign_failure",
			err = DB_TOO_MANY_CONCURRENT_TRXS;);

	if (err != DB_SUCCESS) {

		return(row_import_cleanup(prebuilt, trx, err));

	} else if (trx->update_undo == NULL) {

		err = DB_TOO_MANY_CONCURRENT_TRXS;
		return(row_import_cleanup(prebuilt, trx, err));
	}

	prebuilt->trx->op_info = "read meta-data file";

	{
		row_import_t*	cfg = 0;

		/* Prevent DDL operations while we are checking. */

		rw_lock_s_lock_func(
			&dict_operation_lock, 0, __FILE__, __LINE__);

		err = row_import_read_cfg(table, trx->mysql_thd, &cfg);

		/* Check if the table column definitions match the contents
		of the config file. */

		if (err == DB_SUCCESS) {
			err = row_import_match_schema(trx->mysql_thd, cfg);
		}

		/* Update index->page and SYS_INDEXES.PAGE_NO to match the
		B-tree root page numbers in the tablespace. */

		if (err == DB_SUCCESS) {
			err = row_import_index_set_root(trx->mysql_thd, cfg);
		}

		if (cfg != 0) {
			row_import_free(cfg);
		}

		rw_lock_s_unlock_gen(&dict_operation_lock, 0);
	}

	DBUG_EXECUTE_IF("ib_import_set_index_root_failure",
			err = DB_TOO_MANY_CONCURRENT_TRXS;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	prebuilt->trx->op_info = "importing tablespace";

	current_lsn = log_get_lsn();

	/* Ensure that trx_lists_init_at_db_start() will find the
	transaction, so that trx_rollback_active() will be able to
	drop the half-imported table. */

	log_make_checkpoint_at(current_lsn, TRUE);

	/* Reassign table->id, so that purge will not remove entries
	of the imported table. The undo logs may contain entries that
	are referring to the tablespace that was discarded before the
	import was initiated. */

	table_id_t	new_id;

	mutex_enter(&dict_sys->mutex);

	err = row_mysql_table_id_reassign(table, trx, &new_id);

	mutex_exit(&dict_sys->mutex);

	DBUG_EXECUTE_IF("ib_import_table_id_reassign_failure",
			err = DB_TOO_MANY_CONCURRENT_TRXS;);

	if (err != DB_SUCCESS) {
		return(row_import_cleanup(prebuilt, trx, err));
	}

	/* It is possible that the lsn's in the tablespace to be
	imported are above the current system lsn or the space id in
	the tablespace files differs from the table->space.  If that
	is the case, reset space_id and page lsn in the file.  We
	assume that mysqld stamped the latest lsn to the
	FIL_PAGE_FILE_FLUSH_LSN in the first page of the .ibd file. */

	err = fil_reset_space_and_lsn(table, current_lsn);

	DBUG_EXECUTE_IF("ib_import_reset_space_and_lsn_failure",
			err = DB_TOO_MANY_CONCURRENT_TRXS;);

	if (err != DB_SUCCESS) {
		ib_pushf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			 ER_GET_ERRNO,
			 "Cannot reset LSN's in table %s : %s",
			 table_name, ut_strerr(err));

		return(row_import_cleanup(prebuilt, trx, err));
	}

	/* Play it safe and remove all insert buffer entries for this
	tablespace. */

	ibuf_delete_for_discarded_space(table->space);

	err = fil_open_single_table_tablespace(
		    table, table->space,
		    dict_tf_to_fsp_flags(table->flags),
		    table->name);

	DBUG_EXECUTE_IF("ib_import_open_tablespace_failure",
			err = DB_TABLESPACE_NOT_FOUND;);

	if (err != DB_SUCCESS) {
		char*	ibd_filename;

		ibd_filename = fil_make_ibd_name(table->name, FALSE);

		ib_pushf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			 ER_FILE_NOT_FOUND,
			 "Cannot open '%s' file of %s : %s",
			 ibd_filename, table_name, ut_strerr(err));

		mem_free(ibd_filename);

		return(row_import_cleanup(prebuilt, trx, err));
	}

	err = ibuf_check_bitmap_on_import(trx, table->space);

	DBUG_EXECUTE_IF("ib_import_check_bitmap_failure",
			err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_cleanup(prebuilt, trx, err));
	}

	/* The first index must always be the cluster index. */

	dict_index_t*	index = dict_table_get_first_index(table);

	if (!dict_index_is_clust(index)) {
		return(row_import_error(prebuilt, trx, DB_CORRUPTION));
	}

	/* Scan the indexes. In the clustered index, initialize DB_TRX_ID
	and DB_ROLL_PTR.  Ensure that the next available DB_ROW_ID is not
	smaller than any DB_ROW_ID stored in the table. Purge any
	delete-marked records from every index. */

	err = btr_root_adjust_on_import(index);

	DBUG_EXECUTE_IF("ib_import_cluster_root_adjust_failure",
			err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	} else if (!btr_validate_index(index, trx, true)) {
		return(row_import_error(prebuilt, trx, DB_CORRUPTION));
	} else {
		IndexImporter	importer(trx, index, table->space);

		trx->op_info = "importing cluster index";

		err = importer.import();

		if (err == DB_SUCCESS) {
			n_rows_in_table = importer.get_n_recs();
		}

		trx->op_info = "";
	}

	DBUG_EXECUTE_IF("ib_import_cluster_failure", err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	err = row_import_adjust_root_pages(
		prebuilt, trx, table, n_rows_in_table);

	DBUG_EXECUTE_IF("ib_import_sec_root_adjust_failure",
			err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	if (prebuilt->clust_index_was_generated) {

		err = row_import_set_sys_max_row_id(prebuilt, table);

		if (err != DB_SUCCESS) {
			return(row_import_error(prebuilt, trx, err));
		}
	}

	/* Update the root pages of the table's indexes. */
	err = row_import_update_index_root(trx, table, false, false);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	/* Update the table's discarded flag, unset it. */
	err = row_import_update_discarded_flag(trx, new_id, false, false);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	mutex_enter(&dict_sys->mutex);

	table->flags2 &= ~DICT_TF2_DISCARDED;
	dict_table_change_id_in_cache(table, new_id);

	mutex_exit(&dict_sys->mutex);

	DBUG_EXECUTE_IF("ib_import_before_checkpoint_crash", DBUG_SUICIDE(););

	/* Flush dirty blocks to the file. */
	log_make_checkpoint_at(IB_ULONGLONG_MAX, TRUE);

	DBUG_EXECUTE_IF("ib_import_after_checkpoint_crash", DBUG_SUICIDE(););

	ut_a(err == DB_SUCCESS);

	table->ibd_file_missing = false;

	return(row_import_cleanup(prebuilt, trx, err));
}
