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
#include "row0upd.h"
#include "row0mysql.h"
#include "srv0start.h"
#include "row0quiesce.h"

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
		rec_offs_init(m_offsets_);
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
		ulint*	offsets = m_offsets_;
		ibool	comp = dict_table_is_comp(m_index->table);

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
	ulint	get_n_recs() throw()
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

	/** Postition the cursor on the next record.
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

		if (len <= BTR_EXTERN_FIELD_REF_SIZE) {

			char index_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				index_name, sizeof(index_name),
				m_index->name, TRUE);

			ib_pushf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				 ER_INDEX_CORRUPT,
				"externally stored column has a reference "
				"length of %lu\n in index",
				len, index_name);

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
	tree structure may be changed in pessimistic delete. */
	void	purge_pessimistic_delete() throw()
	{
		ulint	err;

		btr_pcur_store_position(&m_pcur, &m_mtr);

		btr_pcur_restore_position(BTR_MODIFY_TREE, &m_pcur, &m_mtr);

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
	@param deleted - true of row is delete marked
	@return DB_SUCCESS or error code. */
	db_err	adjust(rec_t* rec, const ulint* offsets, ibool deleted) throw()
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
	ulint			m_offsets_[REC_OFFS_NORMAL_SIZE];
						/*!< Column offsets */
};

/*****************************************************************//**
Cleanup after import tablespace. */
static
db_err
row_import_cleanup(
/*===============*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt from handler */
	trx_t*		trx,		/*!< in/out: transaction for import */
	db_err		err)		/*!< in: error code */
{
	ut_a(prebuilt->trx != trx);

	if (err != DB_SUCCESS) {
		dict_table_t*	table = prebuilt->table;

		prebuilt->trx->error_info = NULL;

		char table_name[MAX_FULL_NAME_LEN + 1];

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
static
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

		ib_logf(IB_LOG_LEVEL_INFO,
			"ALTER TABLE %s IMPORT TABLESPACE failed: %lu",
			table_name, (ulint) err);
	}

	return(row_import_cleanup(prebuilt, trx, err));
}

/*****************************************************************//**
Adjust the root on the table's indexes.
@return error code */
static
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
				(ulint) n_rows,
				(ulint) n_rows_in_table);
					
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
static
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
		err = DB_SUCCESS;
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

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
			dict_sys->row_id = row_id;
			dict_hdr_flush_row_id();
		}

		mutex_exit(&dict_sys->mutex);
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Read the tablename from the meta data file.
@return DB_SUCCESS or error code. */
static
db_err
row_import_cfg_read_index_name(
/*===========================*/
	FILE*		file,		/*!< in/out: File to read from */
	char*		name,		/*!< out: index name */
	ulint		max_len)	/*!< in: max size of name buffer, this
					is also the expected length */
{
	ulint		len = 0;

	while (!feof(file)) {
		int	ch = fgetc(file);

		if (ch == EOF) {
			break;
		} else if (ch != 0) {
			if (len < max_len) {
				name[len++] = ch;
			} else {
				break;
			}
		/* max_len includes the NUL byte */
		} else if (len != max_len - 1) {
			return(DB_IO_ERROR);
		} else {
			name[len] = 0;
			return(DB_SUCCESS);
		}
	}

	return(DB_IO_ERROR);
}

/*****************************************************************//**
Read the index names and root page numbers of the indexes and set the values.
Row format [root_page_no, len of str, str ... ]
@return DB_SUCCESS or error code. */
static
db_err
row_import_set_index_root_v1(
/*=========================*/
	dict_table_t*	table,		/*!< in: table */
	ulint		n_indexes,	/*!< in: number of indexes */
	FILE*		file,		/*!< in: File to read from */
	void*		thd)		/*!< in: session */
{
	byte*		ptr;
	byte		row[sizeof(ib_uint32_t) * 2];

	for (ulint i = 0; i < n_indexes; ++i) {
		db_err		err;
		dict_index_t*	index;
		ib_uint32_t	pageno;

		ptr = row;

		/* Read the root page number of the index. */
		if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
				"IO error (%lu), reading index meta-data",
				(ulint) errno);

			return(DB_IO_ERROR);
		}

		pageno = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		/* The NUL byte is included in the name length. */
		ulint	len = mach_read_from_4(ptr);

		if (len > OS_FILE_MAX_PATH) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
				"Index name length (%lu) too long "
				"the meta-data is corrupt", len);

			return(DB_CORRUPTION);
		}

		char*	name = static_cast<char*>(ut_malloc(len));

		err = row_import_cfg_read_index_name(file, name, len);

		if (err != DB_SUCCESS) {
			ut_free(name);
			return(err);
		}

		index = dict_table_get_index_on_name(table, name);

		if (index != 0) {
			index->page = pageno;
			index->space = table->space;
		} else {
			char index_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				index_name, sizeof(index_name), name, TRUE);

			char table_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				table_name, sizeof(table_name),
				table->name, FALSE);

			ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
				 "Index %s not in table %s",
				 index_name, table_name);

			/* TODO: If the table schema matches, it would
			be better to just ignore this error. */

			ut_free(name);
			return(DB_ERROR);
		}

		ut_free(name);
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Set the index root page number for v1 format.
@return DB_SUCCESS or error code. */
static
db_err
row_import_set_index_root_v1(
/*=========================*/
	dict_table_t*	table,		/*!< in: table */
	FILE*		file,		/*!< in: File to read from */
	void*		thd)		/*!< in: session */
{
	byte*		ptr;
	ulint		n_indexes;
	ulint		page_size;
	byte		row[sizeof(ib_uint32_t) * 3];

	/* Read the tablespace page size. */
	if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			 "IO error (%lu), reading page size",
			 (ulint) errno);

		return(DB_IO_ERROR);
	}

	ptr = row;

	page_size = mach_read_from_4(ptr);
	ptr += sizeof(ib_uint32_t);

	if (page_size != UNIV_PAGE_SIZE) {

		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			"Tablespace to be imported has different "
			"page size than this server. Server page size "
			"is: %lu, whereas tablespace page size is %lu",
			UNIV_PAGE_SIZE, page_size);

		return(DB_ERROR);
	}

	ulint	flags = mach_read_from_4(ptr);
	ptr += sizeof(ib_uint32_t);

	if (dict_tf_validate(flags) != DB_SUCCESS) {
		return(DB_CORRUPTION);
	}

	table->flags = flags;

	n_indexes = mach_read_from_4(ptr);

	return(row_import_set_index_root_v1(table, n_indexes, file, thd));
}

/*****************************************************************//**
Set the index root <space, pageno> from the meta-data.
@return DB_SUCCESS or error code. */
static
db_err
row_import_set_index_root(
/*======================*/
	dict_table_t*	table,		/*!< in: table */
	FILE*		file,		/*!< in: File to read from */
	void*		thd)		/*!< in: session */
{
	ib_uint32_t	value;

	if (fread(&value, sizeof(value), 1, file) != 1) {
		ib_pushf(thd, IB_LOG_LEVEL_WARN, ER_INDEX_CORRUPT,
			"IO error (%lu), reading version",
			(ulint) errno);

		return(DB_IO_ERROR);
	}

	value = mach_read_from_4(reinterpret_cast<const byte*>(&value));

	/* Check the version number. */
	switch (value) {
	case IB_EXPORT_CFG_VERSION_V1:
		return(row_import_set_index_root_v1(table, file, thd));
	default:
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_INDEX_CORRUPT,
			"Unsupported meta-data version number (%lu), "
			"file ignored", (ulint) value);

		return(DB_ERROR);
	}

	return(DB_ERROR);
}

/*****************************************************************//**
Get the index root <space, pageno> from the meta-data.
@return DB_SUCCESS or error code. */
static
db_err
row_import_set_index_root(
/*======================*/
	dict_table_t*	table,	/*!< in: table */
	void*		thd)	/*!< in: session */
{
	db_err		err;
	char		name[OS_FILE_MAX_PATH];

	srv_get_meta_data_filename(table, name, sizeof(name));

	FILE*	file = fopen(name, "r");

	if (file == NULL) {
		ib_pushf(thd, IB_LOG_LEVEL_WARN, ER_INDEX_CORRUPT,
			 "Error opening: %s", name);
		err = DB_IO_ERROR;
	} else {
		err = row_import_set_index_root(table, file, thd);
		fclose(file);
	}

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
	char 		table_name[MAX_FULL_NAME_LEN + 1];

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

	if (err != DB_SUCCESS) {

		return(row_import_cleanup(prebuilt, trx, err));

	} else if (trx->update_undo == NULL) {

		err = DB_TOO_MANY_CONCURRENT_TRXS;
		return(row_import_cleanup(prebuilt, trx, err));
	}

	prebuilt->trx->op_info = "read meta-data file";

	/* Update index->page and SYS_INDEXES.PAGE_NO to match the
	B-tree root page numbers in the tablespace. Also, check if
	the tablespace page size is the same as UNIV_PAGE_SIZE. */

	err = row_import_set_index_root(table, trx->mysql_thd);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	prebuilt->trx->op_info = "importing tablespace";

	current_lsn = log_get_lsn();

	/* Ensure that trx_lists_init_at_db_start() will find the
	transaction, so that trx_rollback_active() will be able to
	drop the half-imported table. */

	log_make_checkpoint_at(current_lsn, TRUE);

	mutex_enter(&dict_sys->mutex);

	/* Reassign table->id, so that purge will not remove entries
	of the imported table.  The undo logs may contain entries that
	are referring to the tablespace that was discarded before the
	import was initiated. */

	err = (db_err) row_mysql_table_id_reassign(table, trx);

	mutex_exit(&dict_sys->mutex);

	if (err != DB_SUCCESS) {
		return(row_import_cleanup(prebuilt, trx, err));
	}

	/* It is possible that the lsn's in the tablespace to be
	imported are above the current system lsn or the space id in
	the tablespace files differs from the table->space.  If that
	is the case, reset space_id and page lsn in the file.  We
	assume that mysqld stamped the latest lsn to the
	FIL_PAGE_FILE_FLUSH_LSN in the first page of the .ibd file. */

	if (!fil_reset_space_and_lsn(table, current_lsn)) {

		ib_pushf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			 ER_INDEX_CORRUPT,
			 "Error: cannot reset LSN's in table %s",
			 table_name);

		return(row_import_cleanup(prebuilt, trx, DB_ERROR));
	}

	/* Play it safe and remove all insert buffer entries for this
	tablespace. */

	ibuf_delete_for_discarded_space(table->space);

	err = fil_open_single_table_tablespace(
		    table, table->space,
		    dict_tf_to_fsp_flags(table->flags),
		    table->name);

	if (err != DB_SUCCESS) {

		if (table->ibd_file_missing) {
			ib_pushf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				 ER_FILE_NOT_FOUND,
				"Cannot find or open in the "
				"database directory the .ibd file of %s",
				table_name);
		}

		return(row_import_cleanup(prebuilt, trx, err));
	}

	err = (db_err) ibuf_check_bitmap_on_import(trx, table->space);

	if (err != DB_SUCCESS) {
		return(row_import_cleanup(prebuilt, trx, err));
	}

	dict_index_t*	index = dict_table_get_first_index(table);

	/* Scan the indexes. In the clustered index, initialize DB_TRX_ID
	and DB_ROLL_PTR.  Ensure that the next available DB_ROW_ID is not
	smaller than any DB_ROW_ID stored in the table. Purge any
	delete-marked records from every index. */

	if (!dict_index_is_clust(index)) {
		return(row_import_error(prebuilt, trx, DB_CORRUPTION));
	}

	err = btr_root_adjust_on_import(index);

	if (err != DB_SUCCESS) {

		return(row_import_error(prebuilt, trx, err));

	} else if (!btr_validate_index(index, trx, TRUE)) {

		return(row_import_error(prebuilt, trx, DB_CORRUPTION));
	}

	{
		IndexImporter	importer(trx, index, table->space);

		trx->op_info = "importing cluster index";

		err = importer.import();

		if (err == DB_SUCCESS) {
			n_rows_in_table = importer.get_n_recs();
		}

		trx->op_info = "";
	}

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	err = row_import_adjust_root_pages(
		prebuilt, trx, table, n_rows_in_table);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	/* TODO: Copy table->flags to SYS_TABLES.TYPE and N_COLS */

	table->ibd_file_missing = FALSE;

	if (!dict_index_is_unique(index)) {

		err = row_import_set_sys_max_row_id(prebuilt, table);

		if (err != DB_SUCCESS) {
			return(row_import_error(prebuilt, trx, err));
		}
	}

	/* Scan SYS_INDEXES for all indexes of the table and set root page. */
	{
		SetIndexRoot	callback(table);

		row_mysql_sys_index_iterate(table->id, &callback);
	}

	/* Flush dirty blocks to the file. */
	log_make_checkpoint_at(IB_ULONGLONG_MAX, TRUE);

	ut_a(err == DB_SUCCESS);
	return(row_import_cleanup(prebuilt, trx, err));
}

