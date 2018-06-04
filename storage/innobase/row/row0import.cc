/*****************************************************************************

Copyright (c) 2012, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include <vector>

/** The size of the buffer to use for IO. Note: os_file_read() doesn't expect
reads to fail. If you set the buffer size to be greater than a multiple of the
file size then it will assert. TODO: Fix this limitation of the IO functions.
@param n - page size of the tablespace.
@retval number of pages */
#define IO_BUFFER_SIZE(n)	((1024 * 1024) / n)

/** For gathering stats on records during phase I */
struct row_stats_t {
	ulint		m_n_deleted;		/*!< Number of deleted records
						found in the index */

	ulint		m_n_purged;		/*!< Number of records purged
						optimisatically */

	ulint		m_n_rows;		/*!< Number of rows */

	ulint		m_n_purge_failed;	/*!< Number of deleted rows
						that could not be purged */
};

/** Index information required by IMPORT. */
struct row_index_t {
	index_id_t	m_id;			/*!< Index id of the table
						in the exporting server */
	byte*		m_name;			/*!< Index name */

	ulint		m_space;		/*!< Space where it is placed */

	ulint		m_page_no;		/*!< Root page number */

	ulint		m_type;			/*!< Index type */

	ulint		m_trx_id_offset;	/*!< Relevant only for clustered
						indexes, offset of transaction
						id system column */

	ulint		m_n_user_defined_cols;	/*!< User defined columns */

	ulint		m_n_uniq;		/*!< Number of columns that can
						uniquely identify the row */

	ulint		m_n_nullable;		/*!< Number of nullable
						columns */

	ulint		m_n_fields;		/*!< Total number of fields */

	dict_field_t*	m_fields;		/*!< Index fields */

	const dict_index_t*
			m_srv_index;		/*!< Index instance in the
						importing server */

	row_stats_t	m_stats;		/*!< Statistics gathered during
						the import phase */

};

/** Meta data required by IMPORT. */
struct row_import {
	row_import() UNIV_NOTHROW
		:
		m_table(),
		m_version(),
		m_hostname(),
		m_table_name(),
		m_autoinc(),
		m_page_size(),
		m_flags(),
		m_n_cols(),
		m_cols(),
		m_col_names(),
		m_n_indexes(),
		m_indexes(),
		m_missing(true) { }

	~row_import() UNIV_NOTHROW;

	/**
	Find the index entry in in the indexes array.
	@param name - index name
	@return instance if found else 0. */
	row_index_t* get_index(const char* name) const UNIV_NOTHROW;

	/**
	Get the number of rows in the index.
	@param name - index name
	@return number of rows (doesn't include delete marked rows). */
	ulint	get_n_rows(const char* name) const UNIV_NOTHROW;

	/**
	Find the ordinal value of the column name in the cfg table columns.
	@param name - of column to look for.
	@return ULINT_UNDEFINED if not found. */
	ulint find_col(const char* name) const UNIV_NOTHROW;

	/**
	Get the number of rows for which purge failed during the convert phase.
	@param name - index name
	@return number of rows for which purge failed. */
	ulint	get_n_purge_failed(const char* name) const UNIV_NOTHROW;

	/**
	Check if the index is clean. ie. no delete-marked records
	@param name - index name
	@return true if index needs to be purged. */
	bool requires_purge(const char* name) const UNIV_NOTHROW
	{
		return(get_n_purge_failed(name) > 0);
	}

	/**
	Set the index root <space, pageno> using the index name */
	void set_root_by_name() UNIV_NOTHROW;

	/**
	Set the index root <space, pageno> using a heuristic
	@return DB_SUCCESS or error code */
	dberr_t set_root_by_heuristic() UNIV_NOTHROW;

	/** Check if the index schema that was read from the .cfg file
	matches the in memory index definition.
	Note: It will update row_import_t::m_srv_index to map the meta-data
	read from the .cfg file to the server index instance.
	@return DB_SUCCESS or error code. */
	dberr_t match_index_columns(
		THD*			thd,
		const dict_index_t*	index) UNIV_NOTHROW;

	/**
	Check if the table schema that was read from the .cfg file matches the
	in memory table definition.
	@param thd - MySQL session variable
	@return DB_SUCCESS or error code. */
	dberr_t match_table_columns(
		THD*			thd) UNIV_NOTHROW;

	/**
	Check if the table (and index) schema that was read from the .cfg file
	matches the in memory table definition.
	@param thd - MySQL session variable
	@return DB_SUCCESS or error code. */
	dberr_t match_schema(
		THD*			thd) UNIV_NOTHROW;

	dict_table_t*	m_table;		/*!< Table instance */

	ulint		m_version;		/*!< Version of config file */

	byte*		m_hostname;		/*!< Hostname where the
						tablespace was exported */
	byte*		m_table_name;		/*!< Exporting instance table
						name */

	ib_uint64_t	m_autoinc;		/*!< Next autoinc value */

	ulint		m_page_size;		/*!< Tablespace page size */

	ulint		m_flags;		/*!< Table flags */

	ulint		m_n_cols;		/*!< Number of columns in the
						meta-data file */

	dict_col_t*	m_cols;			/*!< Column data */

	byte**		m_col_names;		/*!< Column names, we store the
						column naems separately becuase
						there is no field to store the
						value in dict_col_t */

	ulint		m_n_indexes;		/*!< Number of indexes,
						including clustered index */

	row_index_t*	m_indexes;		/*!< Index meta data */

	bool		m_missing;		/*!< true if a .cfg file was
						found and was readable */
};

/** Use the page cursor to iterate over records in a block. */
class RecIterator {
public:
	/**
	Default constructor */
	RecIterator() UNIV_NOTHROW
	{
		memset(&m_cur, 0x0, sizeof(m_cur));
	}

	/**
	Position the cursor on the first user record. */
	void	open(buf_block_t* block) UNIV_NOTHROW
	{
		page_cur_set_before_first(block, &m_cur);

		if (!end()) {
			next();
		}
	}

	/**
	Move to the next record. */
	void	next() UNIV_NOTHROW
	{
		page_cur_move_to_next(&m_cur);
	}

	/**
	@return the current record */
	rec_t*	current() UNIV_NOTHROW
	{
		ut_ad(!end());
		return(page_cur_get_rec(&m_cur));
	}

	/**
	@return true if cursor is at the end */
	bool	end() UNIV_NOTHROW
	{
		return(page_cur_is_after_last(&m_cur) == TRUE);
	}

	/** Remove the current record
	@return true on success */
	bool remove(
		const dict_index_t*	index,
		page_zip_des_t*		page_zip,
		ulint*			offsets) UNIV_NOTHROW
	{
		/* We can't end up with an empty page unless it is root. */
		if (page_get_n_recs(m_cur.block->frame) <= 1) {
			return(false);
		}

		return(page_delete_rec(index, &m_cur, page_zip, offsets));
	}

private:
	page_cur_t	m_cur;
};

/** Class that purges delete marked reocords from indexes, both secondary
and cluster. It does a pessimistic delete. This should only be done if we
couldn't purge the delete marked reocrds during Phase I. */
class IndexPurge {
public:
	/** Constructor
	@param trx - the user transaction covering the import tablespace
	@param index - to be imported
	@param space_id - space id of the tablespace */
	IndexPurge(
		trx_t*		trx,
		dict_index_t*	index) UNIV_NOTHROW
		:
		m_trx(trx),
		m_index(index),
		m_n_rows(0)
	{
		ib_logf(IB_LOG_LEVEL_INFO,
			"Phase II - Purge records from index %s",
			index->name);
	}

	/** Descructor */
	~IndexPurge() UNIV_NOTHROW { }

	/** Purge delete marked records.
	@return DB_SUCCESS or error code. */
	dberr_t	garbage_collect() UNIV_NOTHROW;

	/** The number of records that are not delete marked.
	@return total records in the index after purge */
	ulint	get_n_rows() const UNIV_NOTHROW
	{
		return(m_n_rows);
	}

private:
	/**
	Begin import, position the cursor on the first record. */
	void	open() UNIV_NOTHROW;

	/**
	Close the persistent curosr and commit the mini-transaction. */
	void	close() UNIV_NOTHROW;

	/**
	Position the cursor on the next record.
	@return DB_SUCCESS or error code */
	dberr_t	next() UNIV_NOTHROW;

	/**
	Store the persistent cursor position and reopen the
	B-tree cursor in BTR_MODIFY_TREE mode, because the
	tree structure may be changed during a pessimistic delete. */
	void	purge_pessimistic_delete() UNIV_NOTHROW;

	/**
	Purge delete-marked records.
	@param offsets - current row offsets. */
	void	purge() UNIV_NOTHROW;

protected:
	// Disable copying
	IndexPurge();
	IndexPurge(const IndexPurge&);
	IndexPurge &operator=(const IndexPurge&);

private:
	trx_t*			m_trx;		/*!< User transaction */
	mtr_t			m_mtr;		/*!< Mini-transaction */
	btr_pcur_t		m_pcur;		/*!< Persistent cursor */
	dict_index_t*		m_index;	/*!< Index to be processed */
	ulint			m_n_rows;	/*!< Records in index */
};

/** Functor that is called for each physical page that is read from the
tablespace file.  */
class AbstractCallback : public PageCallback {
public:
	/** Constructor
	@param trx - covering transaction */
	AbstractCallback(trx_t* trx)
		:
		m_trx(trx),
		m_space(ULINT_UNDEFINED),
		m_xdes(),
		m_xdes_page_no(ULINT_UNDEFINED),
		m_space_flags(ULINT_UNDEFINED),
		m_table_flags(ULINT_UNDEFINED) UNIV_NOTHROW { }

	/**
	Free any extent descriptor instance */
	virtual ~AbstractCallback()
	{
		delete [] m_xdes;
	}

	/** Determine the page size to use for traversing the tablespace
	@param file_size - size of the tablespace file in bytes
	@param block - contents of the first page in the tablespace file.
	@retval DB_SUCCESS or error code. */
	virtual dberr_t init(
		os_offset_t		file_size,
		const buf_block_t*	block) UNIV_NOTHROW;

	/** @return true if compressed table. */
	bool is_compressed_table() const UNIV_NOTHROW
	{
		return(get_zip_size() > 0);
	}

protected:
	/**
	Get the data page depending on the table type, compressed or not.
	@param block - block read from disk
	@retval the buffer frame */
	buf_frame_t* get_frame(buf_block_t* block) const UNIV_NOTHROW
	{
		if (is_compressed_table()) {
			return(block->page.zip.data);
		}

		return(buf_block_get_frame(block));
	}

	/** Check for session interrupt. If required we could
	even flush to disk here every N pages.
	@retval DB_SUCCESS or error code */
	dberr_t periodic_check() UNIV_NOTHROW
	{
		if (trx_is_interrupted(m_trx)) {
			return(DB_INTERRUPTED);
		}

		return(DB_SUCCESS);
	}

	/**
	Get the physical offset of the extent descriptor within the page.
	@param page_no - page number of the extent descriptor
	@param page - contents of the page containing the extent descriptor.
	@return the start of the xdes array in a page */
	const xdes_t* xdes(
		ulint		page_no,
		const page_t*	page) const UNIV_NOTHROW
	{
		ulint	offset;

		offset = xdes_calc_descriptor_index(get_zip_size(), page_no);

		return(page + XDES_ARR_OFFSET + XDES_SIZE * offset);
	}

	/**
	Set the current page directory (xdes). If the extent descriptor is
	marked as free then free the current extent descriptor and set it to
	0. This implies that all pages that are covered by this extent
	descriptor are also freed.

	@param page_no - offset of page within the file
	@param page - page contents
	@return DB_SUCCESS or error code. */
	dberr_t	set_current_xdes(
		ulint		page_no,
		const page_t*	page) UNIV_NOTHROW
	{
		m_xdes_page_no = page_no;

		delete[] m_xdes;

		m_xdes = 0;

		ulint		state;
		const xdes_t*	xdesc = page + XDES_ARR_OFFSET;

		state = mach_read_ulint(xdesc + XDES_STATE, MLOG_4BYTES);

		if (state != XDES_FREE) {

			m_xdes = new(std::nothrow) xdes_t[m_page_size];

			/* Trigger OOM */
			DBUG_EXECUTE_IF("ib_import_OOM_13",
					delete [] m_xdes; m_xdes = 0;);

			if (m_xdes == 0) {
				return(DB_OUT_OF_MEMORY);
			}

			memcpy(m_xdes, page, m_page_size);
		}

		return(DB_SUCCESS);
	}

	/**
	@return true if it is a root page */
	bool is_root_page(const page_t* page) const UNIV_NOTHROW
	{
		ut_ad(fil_page_get_type(page) == FIL_PAGE_INDEX);

		return(mach_read_from_4(page + FIL_PAGE_NEXT) == FIL_NULL
		       && mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL);
	}

	/**
	Check if the page is marked as free in the extent descriptor.
	@param page_no - page number to check in the extent descriptor.
	@return true if the page is marked as free */
	bool is_free(ulint page_no) const UNIV_NOTHROW
	{
		ut_a(xdes_calc_descriptor_page(get_zip_size(), page_no)
		     == m_xdes_page_no);

		if (m_xdes != 0) {
			const xdes_t*	xdesc = xdes(page_no, m_xdes);
			ulint		pos = page_no % FSP_EXTENT_SIZE;

			return(xdes_get_bit(xdesc, XDES_FREE_BIT, pos));
		}

		/* If the current xdes was free, the page must be free. */
		return(true);
	}

protected:
	/** Covering transaction. */
	trx_t*			m_trx;

	/** Space id of the file being iterated over. */
	ulint			m_space;

	/** Minimum page number for which the free list has not been
	initialized: the pages >= this limit are, by definition, free;
	note that in a single-table tablespace where size < 64 pages,
	this number is 64, i.e., we have initialized the space about
	the first extent, but have not physically allocted those pages
	to the file. @see FSP_LIMIT. */
	ulint			m_free_limit;

	/** Current size of the space in pages */
	ulint			m_size;

	/** Current extent descriptor page */
	xdes_t*			m_xdes;

	/** Physical page offset in the file of the extent descriptor */
	ulint			m_xdes_page_no;

	/** Flags value read from the header page */
	ulint			m_space_flags;

	/** Derived from m_space_flags and row format type, the row format
	type is determined from the page header. */
	ulint			m_table_flags;
};

/** Determine the page size to use for traversing the tablespace
@param file_size - size of the tablespace file in bytes
@param block - contents of the first page in the tablespace file.
@retval DB_SUCCESS or error code. */
dberr_t
AbstractCallback::init(
	os_offset_t		file_size,
	const buf_block_t*	block) UNIV_NOTHROW
{
	const page_t*		page = block->frame;

	m_space_flags = fsp_header_get_flags(page);

	/* Since we don't know whether it is a compressed table
	or not, the data is always read into the block->frame. */

	dberr_t	err = set_zip_size(block->frame);

	if (err != DB_SUCCESS) {
		return(DB_CORRUPTION);
	}

	/* Set the page size used to traverse the tablespace. */

	m_page_size = (is_compressed_table())
		? get_zip_size() : fsp_flags_get_page_size(m_space_flags);

	if (m_page_size == 0) {
		ib_logf(IB_LOG_LEVEL_ERROR, "Page size is 0");
		return(DB_CORRUPTION);
	} else if (!is_compressed_table() && m_page_size != UNIV_PAGE_SIZE) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Page size %lu of ibd file is not the same "
			"as the server page size %lu",
			m_page_size, UNIV_PAGE_SIZE);

		return(DB_CORRUPTION);

	} else if ((file_size % m_page_size)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"File size " UINT64PF " is not a multiple "
			"of the page size %lu",
			(ib_uint64_t) file_size, (ulong) m_page_size);

		return(DB_CORRUPTION);
	}

	ut_a(m_space == ULINT_UNDEFINED);

	m_size  = mach_read_from_4(page + FSP_SIZE);
	m_free_limit = mach_read_from_4(page + FSP_FREE_LIMIT);
	m_space = mach_read_from_4(page + FSP_HEADER_OFFSET + FSP_SPACE_ID);

	if ((err = set_current_xdes(0, page)) != DB_SUCCESS) {
		return(err);
	}

	return(DB_SUCCESS);
}

/**
Try and determine the index root pages by checking if the next/prev
pointers are both FIL_NULL. We need to ensure that skip deleted pages. */
struct FetchIndexRootPages : public AbstractCallback {

	/** Index information gathered from the .ibd file. */
	struct Index {

		Index(index_id_t id, ulint page_no)
			:
			m_id(id),
			m_page_no(page_no) { }

		index_id_t	m_id;		/*!< Index id */
		ulint		m_page_no;	/*!< Root page number */
	};

	typedef std::vector<Index> Indexes;

	/** Constructor
	@param trx - covering (user) transaction
	@param table - table definition in server .*/
	FetchIndexRootPages(const dict_table_t* table, trx_t* trx)
		:
		AbstractCallback(trx),
		m_table(table) UNIV_NOTHROW { }

	/** Destructor */
	virtual ~FetchIndexRootPages() UNIV_NOTHROW { }

	/**
	@retval the space id of the tablespace being iterated over */
	virtual ulint get_space_id() const UNIV_NOTHROW
	{
		return(m_space);
	}

	/**
	Check if the .ibd file row format is the same as the table's.
	@param ibd_table_flags - determined from space and page.
	@return DB_SUCCESS or error code. */
	dberr_t check_row_format(ulint ibd_table_flags) UNIV_NOTHROW
	{
		dberr_t		err;
		rec_format_t	ibd_rec_format;
		rec_format_t	table_rec_format;

		if (!dict_tf_is_valid(ibd_table_flags)) {

			ib_errf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				ER_TABLE_SCHEMA_MISMATCH,
				".ibd file has invlad table flags: %lx",
				ibd_table_flags);

			return(DB_CORRUPTION);
		}

		ibd_rec_format = dict_tf_get_rec_format(ibd_table_flags);
		table_rec_format = dict_tf_get_rec_format(m_table->flags);

		if (table_rec_format != ibd_rec_format) {

			ib_errf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				ER_TABLE_SCHEMA_MISMATCH,
				"Table has %s row format, .ibd "
				"file has %s row format.",
				dict_tf_to_row_format_string(m_table->flags),
				dict_tf_to_row_format_string(ibd_table_flags));

			err = DB_CORRUPTION;
		} else {
			err = DB_SUCCESS;
		}

		return(err);
	}

	/**
	Called for each block as it is read from the file.
	@param offset - physical offset in the file
	@param block - block to convert, it is not from the buffer pool.
	@retval DB_SUCCESS or error code. */
	virtual dberr_t operator() (
		os_offset_t	offset,
		buf_block_t*	block) UNIV_NOTHROW;

	/** Update the import configuration that will be used to import
	the tablespace. */
	dberr_t build_row_import(row_import* cfg) const UNIV_NOTHROW;

	/** Table definition in server. */
	const dict_table_t*	m_table;

	/** Index information */
	Indexes			m_indexes;
};

/**
Called for each block as it is read from the file. Check index pages to
determine the exact row format. We can't get that from the tablespace
header flags alone.

@param offset - physical offset in the file
@param block - block to convert, it is not from the buffer pool.
@retval DB_SUCCESS or error code. */
dberr_t
FetchIndexRootPages::operator() (
	os_offset_t	offset,
	buf_block_t*	block) UNIV_NOTHROW
{
	dberr_t		err;

	if ((err = periodic_check()) != DB_SUCCESS) {
		return(err);
	}

	const page_t*	page = get_frame(block);

	ulint	page_type = fil_page_get_type(page);

	if (block->page.offset * m_page_size != offset) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Page offset doesn't match file offset: "
			"page offset: %lu, file offset: %lu",
			(ulint) block->page.offset,
			(ulint) (offset / m_page_size));

		err = DB_CORRUPTION;
	} else if (page_type == FIL_PAGE_TYPE_XDES) {
		err = set_current_xdes(block->page.offset, page);
	} else if (page_type == FIL_PAGE_INDEX
		   && !is_free(block->page.offset)
		   && is_root_page(page)) {

		index_id_t	id = btr_page_get_index_id(page);
		ulint		page_no = buf_block_get_page_no(block);

		m_indexes.push_back(Index(id, page_no));

		if (m_indexes.size() == 1) {

			m_table_flags = dict_sys_tables_type_to_tf(
				m_space_flags,
				page_is_comp(page) ? DICT_N_COLS_COMPACT : 0);

			err = check_row_format(m_table_flags);
		}
	}

	return(err);
}

/**
Update the import configuration that will be used to import the tablespace.
@return error code or DB_SUCCESS */
dberr_t
FetchIndexRootPages::build_row_import(row_import* cfg) const UNIV_NOTHROW
{
	Indexes::const_iterator end = m_indexes.end();

	ut_a(cfg->m_table == m_table);
	cfg->m_page_size = m_page_size;
	cfg->m_n_indexes = m_indexes.size();

	if (cfg->m_n_indexes == 0) {

		ib_logf(IB_LOG_LEVEL_ERROR, "No B+Tree found in tablespace");

		return(DB_CORRUPTION);
	}

	cfg->m_indexes = new(std::nothrow) row_index_t[cfg->m_n_indexes];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_11",
			delete [] cfg->m_indexes; cfg->m_indexes = 0;);

	if (cfg->m_indexes == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	memset(cfg->m_indexes, 0x0, sizeof(*cfg->m_indexes) * cfg->m_n_indexes);

	row_index_t*	cfg_index = cfg->m_indexes;

	for (Indexes::const_iterator it = m_indexes.begin();
	     it != end;
	     ++it, ++cfg_index) {

		char	name[BUFSIZ];

		ut_snprintf(name, sizeof(name), "index" IB_ID_FMT, it->m_id);

		ulint	len = strlen(name) + 1;

		cfg_index->m_name = new(std::nothrow) byte[len];

		/* Trigger OOM */
		DBUG_EXECUTE_IF("ib_import_OOM_12",
				delete [] cfg_index->m_name;
				cfg_index->m_name = 0;);

		if (cfg_index->m_name == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		memcpy(cfg_index->m_name, name, len);

		cfg_index->m_id = it->m_id;

		cfg_index->m_space = m_space;

		cfg_index->m_page_no = it->m_page_no;
	}

	return(DB_SUCCESS);
}

/* Functor that is called for each physical page that is read from the
tablespace file.

  1. Check each page for corruption.

  2. Update the space id and LSN on every page
     * For the header page
       - Validate the flags
       - Update the LSN

  3. On Btree pages
     * Set the index id
     * Update the max trx id
     * In a cluster index, update the system columns
     * In a cluster index, update the BLOB ptr, set the space id
     * Purge delete marked records, but only if they can be easily
       removed from the page
     * Keep a counter of number of rows, ie. non-delete-marked rows
     * Keep a counter of number of delete marked rows
     * Keep a counter of number of purge failure
     * If a page is stamped with an index id that isn't in the .cfg file
       we assume it is deleted and the page can be ignored.

   4. Set the page state to dirty so that it will be written to disk.
*/
class PageConverter : public AbstractCallback {
public:
	/** Constructor
	* @param cfg - config of table being imported.
	* @param trx - transaction covering the import */
	PageConverter(row_import* cfg, trx_t* trx) UNIV_NOTHROW;

	virtual ~PageConverter() UNIV_NOTHROW
	{
		if (m_heap != 0) {
			mem_heap_free(m_heap);
		}
	}

	/**
	@retval the server space id of the tablespace being iterated over */
	virtual ulint get_space_id() const UNIV_NOTHROW
	{
		return(m_cfg->m_table->space);
	}

	/**
	Called for each block as it is read from the file.
	@param offset - physical offset in the file
	@param block - block to convert, it is not from the buffer pool.
	@retval DB_SUCCESS or error code. */
	virtual dberr_t operator() (
		os_offset_t	offset,
		buf_block_t*	block) UNIV_NOTHROW;
private:

	/** Status returned by PageConverter::validate() */
	enum import_page_status_t {
		IMPORT_PAGE_STATUS_OK,		/*!< Page is OK */
		IMPORT_PAGE_STATUS_ALL_ZERO,	/*!< Page is all zeros */
		IMPORT_PAGE_STATUS_CORRUPTED	/*!< Page is corrupted */
	};

	/**
	Update the page, set the space id, max trx id and index id.
	@param block - block read from file
	@param page_type - type of the page
	@retval DB_SUCCESS or error code */
	dberr_t update_page(
		buf_block_t*	block,
		ulint&		page_type) UNIV_NOTHROW;

#if defined UNIV_DEBUG
	/**
	@return true error condition is enabled. */
	bool trigger_corruption() UNIV_NOTHROW
	{
		return(false);
	}
	#else
#define trigger_corruption()	(false)
#endif /* UNIV_DEBUG */

	/**
	Update the space, index id, trx id.
	@param block - block to convert
	@return DB_SUCCESS or error code */
	dberr_t	update_index_page(buf_block_t*	block) UNIV_NOTHROW;

	/** Update the BLOB refrences and write UNDO log entries for
	rows that can't be purged optimistically.
	@param block - block to update
	@retval DB_SUCCESS or error code */
	dberr_t	update_records(buf_block_t* block) UNIV_NOTHROW;

	/**
	Validate the page, check for corruption.
	@param offset - physical offset within file.
	@param page - page read from file.
	@return 0 on success, 1 if all zero, 2 if corrupted */
	import_page_status_t validate(
		os_offset_t	offset,
		buf_block_t*	page) UNIV_NOTHROW;

	/**
	Validate the space flags and update tablespace header page.
	@param block - block read from file, not from the buffer pool.
	@retval DB_SUCCESS or error code */
	dberr_t	update_header(buf_block_t* block) UNIV_NOTHROW;

	/**
	Adjust the BLOB reference for a single column that is externally stored
	@param rec - record to update
	@param offsets - column offsets for the record
	@param i - column ordinal value
	@return DB_SUCCESS or error code */
	dberr_t	adjust_cluster_index_blob_column(
		rec_t*		rec,
		const ulint*	offsets,
		ulint		i) UNIV_NOTHROW;

	/**
	Adjusts the BLOB reference in the clustered index row for all
	externally stored columns.
	@param rec - record to update
	@param offsets - column offsets for the record
	@return DB_SUCCESS or error code */
	dberr_t	adjust_cluster_index_blob_columns(
		rec_t*		rec,
		const ulint*	offsets) UNIV_NOTHROW;

	/**
	In the clustered index, adjist the BLOB pointers as needed.
	Also update the BLOB reference, write the new space id.
	@param rec - record to update
	@param offsets - column offsets for the record
	@return DB_SUCCESS or error code */
	dberr_t	adjust_cluster_index_blob_ref(
		rec_t*		rec,
		const ulint*	offsets) UNIV_NOTHROW;

	/**
	Purge delete-marked records, only if it is possible to do
	so without re-organising the B+tree.
	@param offsets - current row offsets.
	@retval true if purged */
	bool	purge(const ulint* offsets) UNIV_NOTHROW;

	/**
	Adjust the BLOB references and sys fields for the current record.
	@param index - the index being converted
	@param rec - record to update
	@param offsets - column offsets for the record
	@param deleted - true if row is delete marked
	@return DB_SUCCESS or error code. */
	dberr_t	adjust_cluster_record(
		const dict_index_t*	index,
		rec_t*			rec,
		const ulint*		offsets,
		bool			deleted) UNIV_NOTHROW;

	/**
	Find an index with the matching id.
	@return row_index_t* instance or 0 */
	row_index_t* find_index(index_id_t id) UNIV_NOTHROW
	{
		row_index_t*	index = &m_cfg->m_indexes[0];

		for (ulint i = 0; i < m_cfg->m_n_indexes; ++i, ++index) {
			if (id == index->m_id) {
				return(index);
			}
		}

		return(0);

	}
private:
	/** Config for table that is being imported. */
	row_import*		m_cfg;

	/** Current index whose pages are being imported */
	row_index_t*		m_index;

	/** Current system LSN */
	lsn_t			m_current_lsn;

	/** Alias for m_page_zip, only set for compressed pages. */
	page_zip_des_t*		m_page_zip_ptr;

	/** Iterator over records in a block */
	RecIterator		m_rec_iter;

	/** Record offset */
	ulint			m_offsets_[REC_OFFS_NORMAL_SIZE];

	/** Pointer to m_offsets_ */
	ulint*			m_offsets;

	/** Memory heap for the record offsets */
	mem_heap_t*		m_heap;

	/** Cluster index instance */
	dict_index_t*		m_cluster_index;
};

/**
row_import destructor. */
row_import::~row_import() UNIV_NOTHROW
{
	for (ulint i = 0; m_indexes != 0 && i < m_n_indexes; ++i) {
		delete [] m_indexes[i].m_name;

		if (m_indexes[i].m_fields == 0) {
			continue;
		}

		dict_field_t*	fields = m_indexes[i].m_fields;
		ulint		n_fields = m_indexes[i].m_n_fields;

		for (ulint j = 0; j < n_fields; ++j) {
			delete [] fields[j].name;
		}

		delete [] fields;
	}

	for (ulint i = 0; m_col_names != 0 && i < m_n_cols; ++i) {
		delete [] m_col_names[i];
	}

	delete [] m_cols;
	delete [] m_indexes;
	delete [] m_col_names;
	delete [] m_table_name;
	delete [] m_hostname;
}

/**
Find the index entry in in the indexes array.
@param name - index name
@return instance if found else 0. */
row_index_t*
row_import::get_index(
	const char*	name) const UNIV_NOTHROW
{
	for (ulint i = 0; i < m_n_indexes; ++i) {
		const char*	index_name;
		row_index_t*	index = &m_indexes[i];

		index_name = reinterpret_cast<const char*>(index->m_name);

		if (strcmp(index_name, name) == 0) {

			return(index);
		}
	}

	return(0);
}

/**
Get the number of rows in the index.
@param name - index name
@return number of rows (doesn't include delete marked rows). */
ulint
row_import::get_n_rows(
	const char*	name) const UNIV_NOTHROW
{
	const row_index_t*	index = get_index(name);

	ut_a(name != 0);

	return(index->m_stats.m_n_rows);
}

/**
Get the number of rows for which purge failed uding the convert phase.
@param name - index name
@return number of rows for which purge failed. */
ulint
row_import::get_n_purge_failed(
	const char*	name) const UNIV_NOTHROW
{
	const row_index_t*	index = get_index(name);

	ut_a(name != 0);

	return(index->m_stats.m_n_purge_failed);
}

/**
Find the ordinal value of the column name in the cfg table columns.
@param name - of column to look for.
@return ULINT_UNDEFINED if not found. */
ulint
row_import::find_col(
	const char*	name) const UNIV_NOTHROW
{
	for (ulint i = 0; i < m_n_cols; ++i) {
		const char*	col_name;

		col_name = reinterpret_cast<const char*>(m_col_names[i]);

		if (strcmp(col_name, name) == 0) {
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**
Check if the index schema that was read from the .cfg file matches the
in memory index definition.
@return DB_SUCCESS or error code. */
dberr_t
row_import::match_index_columns(
	THD*			thd,
	const dict_index_t*	index) UNIV_NOTHROW
{
	row_index_t*		cfg_index;
	dberr_t			err = DB_SUCCESS;

	cfg_index = get_index(index->name);

	if (cfg_index == 0) {
		ib_errf(thd, IB_LOG_LEVEL_ERROR,
			 ER_TABLE_SCHEMA_MISMATCH,
			 "Index %s not found in tablespace meta-data file.",
			 index->name);

		return(DB_ERROR);
	}

	if (cfg_index->m_n_fields != index->n_fields) {

		ib_errf(thd, IB_LOG_LEVEL_ERROR,
			 ER_TABLE_SCHEMA_MISMATCH,
			 "Index field count %lu doesn't match"
			 " tablespace metadata file value %lu",
			 (ulong) index->n_fields,
			 (ulong) cfg_index->m_n_fields);

		return(DB_ERROR);
	}

	cfg_index->m_srv_index = index;

	const dict_field_t*	field = index->fields;
	const dict_field_t*	cfg_field = cfg_index->m_fields;

	for (ulint i = 0; i < index->n_fields; ++i, ++field, ++cfg_field) {

		if (strcmp(field->name, cfg_field->name) != 0) {
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Index field name %s doesn't match"
				 " tablespace metadata field name %s"
				 " for field position %lu",
				 field->name, cfg_field->name, (ulong) i);

			err = DB_ERROR;
		}

		if (cfg_field->prefix_len != field->prefix_len) {
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Index %s field %s prefix len %lu"
				 " doesn't match metadata file value"
				 " %lu",
				 index->name, field->name,
				 (ulong) field->prefix_len,
				 (ulong) cfg_field->prefix_len);

			err = DB_ERROR;
		}

		if (cfg_field->fixed_len != field->fixed_len) {
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Index %s field %s fixed len %lu"
				 " doesn't match metadata file value"
				 " %lu",
				 index->name, field->name,
				 (ulong) field->fixed_len,
				 (ulong) cfg_field->fixed_len);

			err = DB_ERROR;
		}
	}

	return(err);
}

/**
Check if the table schema that was read from the .cfg file matches the
in memory table definition.
@param thd - MySQL session variable
@return DB_SUCCESS or error code. */
dberr_t
row_import::match_table_columns(
	THD*			thd) UNIV_NOTHROW
{
	dberr_t			err = DB_SUCCESS;
	const dict_col_t*	col = m_table->cols;

	for (ulint i = 0; i < m_table->n_cols; ++i, ++col) {

		const char*	col_name;
		ulint		cfg_col_index;

		col_name = dict_table_get_col_name(
			m_table, dict_col_get_no(col));

		cfg_col_index = find_col(col_name);

		if (cfg_col_index == ULINT_UNDEFINED) {

			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Column %s not found in tablespace.",
				 col_name);

			err = DB_ERROR;
		} else if (cfg_col_index != col->ind) {

			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				 ER_TABLE_SCHEMA_MISMATCH,
				 "Column %s ordinal value mismatch, it's at "
				 "%lu in the table and %lu in the tablespace "
				 "meta-data file",
				 col_name,
				 (ulong) col->ind, (ulong) cfg_col_index);

			err = DB_ERROR;
		} else {
			const dict_col_t*	cfg_col;

			cfg_col = &m_cols[cfg_col_index];
			ut_a(cfg_col->ind == cfg_col_index);

			if (cfg_col->prtype != col->prtype) {
				ib_errf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s precise type mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->mtype != col->mtype) {
				ib_errf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s main type mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->len != col->len) {
				ib_errf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s length mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->mbminmaxlen != col->mbminmaxlen) {
				ib_errf(thd,
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
				ib_errf(thd,
					 IB_LOG_LEVEL_ERROR,
					 ER_TABLE_SCHEMA_MISMATCH,
					 "Column %s ordering mismatch.",
					 col_name);
				err = DB_ERROR;
			}

			if (cfg_col->max_prefix != col->max_prefix) {
				ib_errf(thd,
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

/**
Check if the table (and index) schema that was read from the .cfg file
matches the in memory table definition.
@param thd - MySQL session variable
@return DB_SUCCESS or error code. */
dberr_t
row_import::match_schema(
	THD*		thd) UNIV_NOTHROW
{
	/* Do some simple checks. */

	if (m_flags != m_table->flags) {
		if (dict_tf_to_row_format_string(m_flags) !=
				dict_tf_to_row_format_string(m_table->flags)) {
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				ER_TABLE_SCHEMA_MISMATCH,
				"Table flags don't match,"
				"server table has %s "
				"and the meta-data file has %s",
				dict_tf_to_row_format_string(m_table->flags),
				dict_tf_to_row_format_string(m_flags));
		} else {
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				ER_TABLE_SCHEMA_MISMATCH,
				"Table flags don't match");
		}
		return(DB_ERROR);
	} else if (m_table->n_cols != m_n_cols) {
		ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
			 "Number of columns don't match, table has %lu "
			 "columns but the tablespace meta-data file has "
			 "%lu columns",
			 (ulong) m_table->n_cols, (ulong) m_n_cols);

		return(DB_ERROR);
	} else if (UT_LIST_GET_LEN(m_table->indexes) != m_n_indexes) {

		/* If the number of indexes don't match then it is better
		to abort the IMPORT. It is easy for the user to create a
		table matching the IMPORT definition. */

		ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
			 "Number of indexes don't match, table has %lu "
			 "indexes but the tablespace meta-data file has "
			 "%lu indexes",
			 (ulong) UT_LIST_GET_LEN(m_table->indexes),
			 (ulong) m_n_indexes);

		return(DB_ERROR);
	}

	dberr_t	err = match_table_columns(thd);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Check if the index definitions match. */

	const dict_index_t* index;

	for (index = UT_LIST_GET_FIRST(m_table->indexes);
	     index != 0;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		dberr_t	index_err;

		index_err = match_index_columns(thd, index);

		if (index_err != DB_SUCCESS) {
			err = index_err;
		}
	}

	return(err);
}

/**
Set the index root <space, pageno>, using index name. */
void
row_import::set_root_by_name() UNIV_NOTHROW
{
	row_index_t*	cfg_index = m_indexes;

	for (ulint i = 0; i < m_n_indexes; ++i, ++cfg_index) {
		dict_index_t*	index;

		const char*	index_name;

		index_name = reinterpret_cast<const char*>(cfg_index->m_name);

		index = dict_table_get_index_on_name(m_table, index_name);

		/* We've already checked that it exists. */
		ut_a(index != 0);

		/* Set the root page number and space id. */
		index->space = m_table->space;
		index->page = cfg_index->m_page_no;
	}
}

/**
Set the index root <space, pageno>, using a heuristic.
@return DB_SUCCESS or error code */
dberr_t
row_import::set_root_by_heuristic() UNIV_NOTHROW
{
	row_index_t*	cfg_index = m_indexes;

	ut_a(m_n_indexes > 0);

	// TODO: For now use brute force, based on ordinality

	if (UT_LIST_GET_LEN(m_table->indexes) != m_n_indexes) {

		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name), m_table->name, FALSE);

		ib_logf(IB_LOG_LEVEL_WARN,
			"Table %s should have %lu indexes but the tablespace "
			"has %lu indexes",
			table_name,
			UT_LIST_GET_LEN(m_table->indexes),
			m_n_indexes);
	}

	dict_mutex_enter_for_mysql();

	ulint	i = 0;
	dberr_t	err = DB_SUCCESS;

	for (dict_index_t* index = UT_LIST_GET_FIRST(m_table->indexes);
	     index != 0;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		if (index->type & DICT_FTS) {
			index->type |= DICT_CORRUPT;
			ib_logf(IB_LOG_LEVEL_WARN,
				"Skipping FTS index: %s", index->name);
		} else if (i < m_n_indexes) {

			delete [] cfg_index[i].m_name;

			ulint	len = strlen(index->name) + 1;

			cfg_index[i].m_name = new(std::nothrow) byte[len];

			/* Trigger OOM */
			DBUG_EXECUTE_IF("ib_import_OOM_14",
					delete[] cfg_index[i].m_name;
					cfg_index[i].m_name = 0;);

			if (cfg_index[i].m_name == 0) {
				err = DB_OUT_OF_MEMORY;
				break;
			}

			memcpy(cfg_index[i].m_name, index->name, len);

			cfg_index[i].m_srv_index = index;

			index->space = m_table->space;
			index->page = cfg_index[i].m_page_no;

			++i;
		}
	}

	dict_mutex_exit_for_mysql();

	return(err);
}

/**
Purge delete marked records.
@return DB_SUCCESS or error code. */
dberr_t
IndexPurge::garbage_collect() UNIV_NOTHROW
{
	dberr_t	err;
	ibool	comp = dict_table_is_comp(m_index->table);

	/* Open the persistent cursor and start the mini-transaction. */

	open();

	while ((err = next()) == DB_SUCCESS) {

		rec_t*	rec = btr_pcur_get_rec(&m_pcur);
		ibool	deleted = rec_get_deleted_flag(rec, comp);

		if (!deleted) {
			++m_n_rows;
		} else {
			purge();
		}
	}

	/* Close the persistent cursor and commit the mini-transaction. */

	close();

	return(err == DB_END_OF_INDEX ? DB_SUCCESS : err);
}

/**
Begin import, position the cursor on the first record. */
void
IndexPurge::open() UNIV_NOTHROW
{
	mtr_start(&m_mtr);

	mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);

	btr_pcur_open_at_index_side(
		true, m_index, BTR_MODIFY_LEAF, &m_pcur, true, 0, &m_mtr);
}

/**
Close the persistent curosr and commit the mini-transaction. */
void
IndexPurge::close() UNIV_NOTHROW
{
	btr_pcur_close(&m_pcur);
	mtr_commit(&m_mtr);
}

/**
Position the cursor on the next record.
@return DB_SUCCESS or error code */
dberr_t
IndexPurge::next() UNIV_NOTHROW
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

	mtr_commit(&m_mtr);

	mtr_start(&m_mtr);

	mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);

	btr_pcur_restore_position(BTR_MODIFY_LEAF, &m_pcur, &m_mtr);

	if (!btr_pcur_move_to_next_user_rec(&m_pcur, &m_mtr)) {

		return(DB_END_OF_INDEX);
	}

	return(DB_SUCCESS);
}

/**
Store the persistent cursor position and reopen the
B-tree cursor in BTR_MODIFY_TREE mode, because the
tree structure may be changed during a pessimistic delete. */
void
IndexPurge::purge_pessimistic_delete() UNIV_NOTHROW
{
	dberr_t	err;

	btr_pcur_restore_position(BTR_MODIFY_TREE, &m_pcur, &m_mtr);

	ut_ad(rec_get_deleted_flag(
			btr_pcur_get_rec(&m_pcur),
			dict_table_is_comp(m_index->table)));

	btr_cur_pessimistic_delete(
		&err, FALSE, btr_pcur_get_btr_cur(&m_pcur), 0, RB_NONE, &m_mtr);

	ut_a(err == DB_SUCCESS);

	/* Reopen the B-tree cursor in BTR_MODIFY_LEAF mode */
	mtr_commit(&m_mtr);
}

/**
Purge delete-marked records. */
void
IndexPurge::purge() UNIV_NOTHROW
{
	btr_pcur_store_position(&m_pcur, &m_mtr);

	purge_pessimistic_delete();

	mtr_start(&m_mtr);

	mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);

	btr_pcur_restore_position(BTR_MODIFY_LEAF, &m_pcur, &m_mtr);
}

/**
Constructor
* @param cfg - config of table being imported.
* @param trx - transaction covering the import */
PageConverter::PageConverter(
	row_import*	cfg,
	trx_t*		trx)
	:
	AbstractCallback(trx),
	m_cfg(cfg),
	m_page_zip_ptr(0),
	m_heap(0) UNIV_NOTHROW
{
	m_index = m_cfg->m_indexes;

	m_current_lsn = log_get_lsn();
	ut_a(m_current_lsn > 0);

	m_offsets = m_offsets_;
	rec_offs_init(m_offsets_);

	m_cluster_index = dict_table_get_first_index(m_cfg->m_table);
}

/**
Adjust the BLOB reference for a single column that is externally stored
@param rec - record to update
@param offsets - column offsets for the record
@param i - column ordinal value
@return DB_SUCCESS or error code */
dberr_t
PageConverter::adjust_cluster_index_blob_column(
	rec_t*		rec,
	const ulint*	offsets,
	ulint		i) UNIV_NOTHROW
{
	ulint		len;
	byte*		field;

	field = rec_get_nth_field(rec, offsets, i, &len);

	DBUG_EXECUTE_IF("ib_import_trigger_corruption_2",
			len = BTR_EXTERN_FIELD_REF_SIZE - 1;);

	if (len < BTR_EXTERN_FIELD_REF_SIZE) {

		char index_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			index_name, sizeof(index_name),
			m_cluster_index->name, TRUE);

		ib_errf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			ER_INNODB_INDEX_CORRUPT,
			"Externally stored column(%lu) has a reference "
			"length of %lu in the cluster index %s",
			(ulong) i, (ulong) len, index_name);

		return(DB_CORRUPTION);
	}

	field += BTR_EXTERN_SPACE_ID - BTR_EXTERN_FIELD_REF_SIZE + len;

	if (is_compressed_table()) {
		mach_write_to_4(field, get_space_id());

		page_zip_write_blob_ptr(
			m_page_zip_ptr, rec, m_cluster_index, offsets, i, 0);
	} else {
		mlog_write_ulint(field, get_space_id(), MLOG_4BYTES, 0);
	}

	return(DB_SUCCESS);
}

/**
Adjusts the BLOB reference in the clustered index row for all externally
stored columns.
@param rec - record to update
@param offsets - column offsets for the record
@return DB_SUCCESS or error code */
dberr_t
PageConverter::adjust_cluster_index_blob_columns(
	rec_t*		rec,
	const ulint*	offsets) UNIV_NOTHROW
{
	ut_ad(rec_offs_any_extern(offsets));

	/* Adjust the space_id in the BLOB pointers. */

	for (ulint i = 0; i < rec_offs_n_fields(offsets); ++i) {

		/* Only if the column is stored "externally". */

		if (rec_offs_nth_extern(offsets, i)) {
			dberr_t	err;

			err = adjust_cluster_index_blob_column(rec, offsets, i);

			if (err != DB_SUCCESS) {
				return(err);
			}
		}
	}

	return(DB_SUCCESS);
}

/**
In the clustered index, adjust BLOB pointers as needed. Also update the
BLOB reference, write the new space id.
@param rec - record to update
@param offsets - column offsets for the record
@return DB_SUCCESS or error code */
dberr_t
PageConverter::adjust_cluster_index_blob_ref(
	rec_t*		rec,
	const ulint*	offsets) UNIV_NOTHROW
{
	if (rec_offs_any_extern(offsets)) {
		dberr_t	err;

		err = adjust_cluster_index_blob_columns(rec, offsets);

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	return(DB_SUCCESS);
}

/**
Purge delete-marked records, only if it is possible to do so without
re-organising the B+tree.
@param offsets - current row offsets.
@return true if purge succeeded */
bool
PageConverter::purge(const ulint* offsets) UNIV_NOTHROW
{
	const dict_index_t*	index = m_index->m_srv_index;

	/* We can't have a page that is empty and not root. */
	if (m_rec_iter.remove(index, m_page_zip_ptr, m_offsets)) {

		++m_index->m_stats.m_n_purged;

		return(true);
	} else {
		++m_index->m_stats.m_n_purge_failed;
	}

	return(false);
}

/**
Adjust the BLOB references and sys fields for the current record.
@param rec - record to update
@param offsets - column offsets for the record
@param deleted - true if row is delete marked
@return DB_SUCCESS or error code. */
dberr_t
PageConverter::adjust_cluster_record(
	const dict_index_t*	index,
	rec_t*			rec,
	const ulint*		offsets,
	bool			deleted) UNIV_NOTHROW
{
	dberr_t	err;

	if ((err = adjust_cluster_index_blob_ref(rec, offsets)) == DB_SUCCESS) {

		/* Reset DB_TRX_ID and DB_ROLL_PTR.  Normally, these fields
		are only written in conjunction with other changes to the
		record. */

		row_upd_rec_sys_fields(
			rec, m_page_zip_ptr, m_cluster_index, m_offsets,
			m_trx, 0);
	}

	return(err);
}

/**
Update the BLOB refrences and write UNDO log entries for
rows that can't be purged optimistically.
@param block - block to update
@retval DB_SUCCESS or error code */
dberr_t
PageConverter::update_records(
	buf_block_t*	block) UNIV_NOTHROW
{
	ibool	comp = dict_table_is_comp(m_cfg->m_table);
	bool	clust_index = m_index->m_srv_index == m_cluster_index;

	/* This will also position the cursor on the first user record. */

	m_rec_iter.open(block);

	while (!m_rec_iter.end()) {

		rec_t*	rec = m_rec_iter.current();

		ibool	deleted = rec_get_deleted_flag(rec, comp);

		/* For the clustered index we have to adjust the BLOB
		reference and the system fields irrespective of the
		delete marked flag. The adjustment of delete marked
		cluster records is required for purge to work later. */

		if (deleted || clust_index) {
			m_offsets = rec_get_offsets(
				rec, m_index->m_srv_index, m_offsets,
				ULINT_UNDEFINED, &m_heap);
		}

		if (clust_index) {

			dberr_t err = adjust_cluster_record(
				m_index->m_srv_index, rec, m_offsets,
				deleted);

			if (err != DB_SUCCESS) {
				return(err);
			}
		}

		/* If it is a delete marked record then try an
		optimistic delete. */

		if (deleted) {
			/* A successful purge will move the cursor to the
			next record. */

			if (!purge(m_offsets)) {
				m_rec_iter.next();
			}

			++m_index->m_stats.m_n_deleted;
		} else {
			++m_index->m_stats.m_n_rows;
			m_rec_iter.next();
		}
	}

	return(DB_SUCCESS);
}

/**
Update the space, index id, trx id.
@return DB_SUCCESS or error code */
dberr_t
PageConverter::update_index_page(
	buf_block_t*	block) UNIV_NOTHROW
{
	index_id_t	id;
	buf_frame_t*	page = block->frame;

	if (is_free(buf_block_get_page_no(block))) {
		return(DB_SUCCESS);
	} else if ((id = btr_page_get_index_id(page)) != m_index->m_id) {

		row_index_t*	index = find_index(id);

		if (index == 0) {
			m_index = 0;
			return(DB_CORRUPTION);
		}

		/* Update current index */
		m_index = index;
	}

	/* If the .cfg file is missing and there is an index mismatch
	then ignore the error. */
	if (m_cfg->m_missing && (m_index == 0 || m_index->m_srv_index == 0)) {
		return(DB_SUCCESS);
	}

#ifdef UNIV_ZIP_DEBUG
	ut_a(!is_compressed_table()
	     || page_zip_validate(m_page_zip_ptr, page, m_index->m_srv_index));
#endif /* UNIV_ZIP_DEBUG */

	/* This has to be written to uncompressed index header. Set it to
	the current index id. */
	btr_page_set_index_id(
		page, m_page_zip_ptr, m_index->m_srv_index->id, 0);

	page_set_max_trx_id(block, m_page_zip_ptr, m_trx->id, 0);

	if (page_is_empty(block->frame)) {

		/* Only a root page can be empty. */
		if (!is_root_page(block->frame)) {
			// TODO: We should relax this and skip secondary
			// indexes. Mark them as corrupt because they can
			// always be rebuilt.
			return(DB_CORRUPTION);
		}

		return(DB_SUCCESS);
	}

	if (!page_is_leaf(block->frame)) {
		return (DB_SUCCESS);
	}

	return(update_records(block));
}

/**
Validate the space flags and update tablespace header page.
@param block - block read from file, not from the buffer pool.
@retval DB_SUCCESS or error code */
dberr_t
PageConverter::update_header(
	buf_block_t*	block) UNIV_NOTHROW
{
	/* Check for valid header */
	switch(fsp_header_get_space_id(get_frame(block))) {
	case 0:
		return(DB_CORRUPTION);
	case ULINT_UNDEFINED:
		ib_logf(IB_LOG_LEVEL_WARN,
			"Space id check in the header failed "
			"- ignored");
	}

	ulint		space_flags = fsp_header_get_flags(get_frame(block));

	if (!fsp_flags_is_valid(space_flags)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unsupported tablespace format %lu",
			(ulong) space_flags);

		return(DB_UNSUPPORTED);
	}

	mach_write_to_8(
		get_frame(block) + FIL_PAGE_FILE_FLUSH_LSN, m_current_lsn);

	/* Write space_id to the tablespace header, page 0. */
	mach_write_to_4(
		get_frame(block) + FSP_HEADER_OFFSET + FSP_SPACE_ID,
		get_space_id());

	/* This is on every page in the tablespace. */
	mach_write_to_4(
		get_frame(block) + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
		get_space_id());

	return(DB_SUCCESS);
}

/**
Update the page, set the space id, max trx id and index id.
@param block - block read from file
@retval DB_SUCCESS or error code */
dberr_t
PageConverter::update_page(
	buf_block_t*	block,
	ulint&		page_type) UNIV_NOTHROW
{
	dberr_t		err = DB_SUCCESS;

	switch (page_type = fil_page_get_type(get_frame(block))) {
	case FIL_PAGE_TYPE_FSP_HDR:
		/* Work directly on the uncompressed page headers. */
		ut_a(buf_block_get_page_no(block) == 0);
		return(update_header(block));

	case FIL_PAGE_INDEX:
		/* We need to decompress the contents into block->frame
		before we can do any thing with Btree pages. */

		if (is_compressed_table() && !buf_zip_decompress(block, TRUE)) {
			return(DB_CORRUPTION);
		}

		/* This is on every page in the tablespace. */
		mach_write_to_4(
			get_frame(block)
			+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, get_space_id());

		/* Only update the Btree nodes. */
		return(update_index_page(block));

	case FIL_PAGE_TYPE_SYS:
		/* This is page 0 in the system tablespace. */
		return(DB_CORRUPTION);

	case FIL_PAGE_TYPE_XDES:
		err = set_current_xdes(
			buf_block_get_page_no(block), get_frame(block));
	case FIL_PAGE_INODE:
	case FIL_PAGE_TYPE_TRX_SYS:
	case FIL_PAGE_IBUF_FREE_LIST:
	case FIL_PAGE_TYPE_ALLOCATED:
	case FIL_PAGE_IBUF_BITMAP:
	case FIL_PAGE_TYPE_BLOB:
	case FIL_PAGE_TYPE_ZBLOB:
	case FIL_PAGE_TYPE_ZBLOB2:

		/* Work directly on the uncompressed page headers. */
		/* This is on every page in the tablespace. */
		mach_write_to_4(
			get_frame(block)
			+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, get_space_id());

		return(err);
	}

	ib_logf(IB_LOG_LEVEL_WARN, "Unknown page type (%lu)", page_type);

	return(DB_CORRUPTION);
}

/**
Validate the page
@param offset - physical offset within file.
@param page - page read from file.
@return status */
PageConverter::import_page_status_t
PageConverter::validate(
	os_offset_t	offset,
	buf_block_t*	block) UNIV_NOTHROW
{
	buf_frame_t*	page = get_frame(block);

	/* Check that the page number corresponds to the offset in
	the file. Flag as corrupt if it doesn't. Disable the check
	for LSN in buf_page_is_corrupted() */

	if (buf_page_is_corrupted(false, page, get_zip_size())
	    || (page_get_page_no(page) != offset / m_page_size
		&& page_get_page_no(page) != 0)) {

		return(IMPORT_PAGE_STATUS_CORRUPTED);

	} else if (offset > 0 && page_get_page_no(page) == 0) {
		ulint		checksum;

		checksum = mach_read_from_4(page + FIL_PAGE_SPACE_OR_CHKSUM);
		if (checksum != 0) {
			/* Checksum check passed in buf_page_is_corrupted(). */
			ib_logf(IB_LOG_LEVEL_WARN,
				"%s: Page %lu checksum %lu should be zero.",
				m_filepath, (ulong) (offset / m_page_size),
				checksum);
		}

		const byte*	b = page + FIL_PAGE_OFFSET;
		const byte*	e = page + m_page_size
				    - FIL_PAGE_END_LSN_OLD_CHKSUM;

		/* If the page number is zero and offset > 0 then
		the entire page MUST consist of zeroes. If not then
		we flag it as corrupt. */

		while (b != e) {

			if (*b++ && !trigger_corruption()) {
				return(IMPORT_PAGE_STATUS_CORRUPTED);
			}
		}

		/* The page is all zero: do nothing. */
		return(IMPORT_PAGE_STATUS_ALL_ZERO);
	}

	return(IMPORT_PAGE_STATUS_OK);
}

/**
Called for every page in the tablespace. If the page was not
updated then its state must be set to BUF_PAGE_NOT_USED.
@param offset - physical offset within the file
@param block - block read from file, note it is not from the buffer pool
@retval DB_SUCCESS or error code. */
dberr_t
PageConverter::operator() (
	os_offset_t	offset,
	buf_block_t*	block) UNIV_NOTHROW
{
	ulint		page_type;
	dberr_t		err = DB_SUCCESS;

	if ((err = periodic_check()) != DB_SUCCESS) {
		return(err);
	}

	if (is_compressed_table()) {
		m_page_zip_ptr = &block->page.zip;
	} else {
		ut_ad(m_page_zip_ptr == 0);
	}

	switch(validate(offset, block)) {
	case IMPORT_PAGE_STATUS_OK:

		/* We have to decompress the compressed pages before
		we can work on them */

		if ((err = update_page(block, page_type)) != DB_SUCCESS) {
			return(err);
		}

		/* Note: For compressed pages this function will write to the
		zip descriptor and for uncompressed pages it will write to
		page (ie. the block->frame). Therefore the caller should write
		out the descriptor contents and not block->frame for compressed
		pages. */

		if (!is_compressed_table() || page_type == FIL_PAGE_INDEX) {

			buf_flush_init_for_writing(
				!is_compressed_table()
				? block->frame : block->page.zip.data,
				!is_compressed_table() ? 0 : m_page_zip_ptr,
				m_current_lsn);
		} else {
			/* Calculate and update the checksum of non-btree
			pages for compressed tables explicitly here. */

			buf_flush_update_zip_checksum(
				get_frame(block), get_zip_size(),
				m_current_lsn);
		}

		break;

	case IMPORT_PAGE_STATUS_ALL_ZERO:
		/* The page is all zero: leave it as is. */
		break;

	case IMPORT_PAGE_STATUS_CORRUPTED:

		ib_logf(IB_LOG_LEVEL_WARN,
			"%s: Page %lu at offset " UINT64PF " looks corrupted.",
			m_filepath, (ulong) (offset / m_page_size), offset);

		return(DB_CORRUPTION);
	}

	return(err);
}

/*****************************************************************//**
Clean up after import tablespace failure, this function will acquire
the dictionary latches on behalf of the transaction if the transaction
hasn't already acquired them. */
static	MY_ATTRIBUTE((nonnull))
void
row_import_discard_changes(
/*=======================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt from handler */
	trx_t*		trx,		/*!< in/out: transaction for import */
	dberr_t		err)		/*!< in: error code */
{
	dict_table_t*	table = prebuilt->table;

	ut_a(err != DB_SUCCESS);

	prebuilt->trx->error_info = NULL;

	char	table_name[MAX_FULL_NAME_LEN + 1];

	innobase_format_name(
		table_name, sizeof(table_name),
		prebuilt->table->name, FALSE);

	ib_logf(IB_LOG_LEVEL_INFO,
		"Discarding tablespace of table %s: %s",
		table_name, ut_strerr(err));

	if (trx->dict_operation_lock_mode != RW_X_LATCH) {
		ut_a(trx->dict_operation_lock_mode == 0);
		row_mysql_lock_data_dictionary(trx);
	}

	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

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

	fil_close_tablespace(trx, table->space);
}

/*****************************************************************//**
Clean up after import tablespace. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_cleanup(
/*===============*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt from handler */
	trx_t*		trx,		/*!< in/out: transaction for import */
	dberr_t		err)		/*!< in: error code */
{
	ut_a(prebuilt->trx != trx);

	if (err != DB_SUCCESS) {
		row_import_discard_changes(prebuilt, trx, err);
	}

	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

	DBUG_EXECUTE_IF("ib_import_before_commit_crash", DBUG_SUICIDE(););

	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);

	prebuilt->trx->op_info = "";

	DBUG_EXECUTE_IF("ib_import_before_checkpoint_crash", DBUG_SUICIDE(););

	log_make_checkpoint_at(LSN_MAX, TRUE);

	return(err);
}

/*****************************************************************//**
Report error during tablespace import. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_error(
/*=============*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt from handler */
	trx_t*		trx,		/*!< in/out: transaction for import */
	dberr_t		err)		/*!< in: error code */
{
	if (!trx_is_interrupted(trx)) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name),
			prebuilt->table->name, FALSE);

		ib_senderrf(
			trx->mysql_thd, IB_LOG_LEVEL_WARN,
			ER_INNODB_IMPORT_ERROR,
			table_name, (ulong) err, ut_strerr(err));
	}

	return(row_import_cleanup(prebuilt, trx, err));
}

/*****************************************************************//**
Adjust the root page index node and leaf node segment headers, update
with the new space id. For all the table's secondary indexes.
@return error code */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_adjust_root_pages_of_secondary_indexes(
/*==============================================*/
	row_prebuilt_t*		prebuilt,	/*!< in/out: prebuilt from
						handler */
	trx_t*			trx,		/*!< in: transaction used for
						the import */
	dict_table_t*		table,		/*!< in: table the indexes
						belong to */
	const row_import&	cfg)		/*!< Import context */
{
	dict_index_t*		index;
	ulint			n_rows_in_table;
	dberr_t			err = DB_SUCCESS;

	/* Skip the clustered index. */
	index = dict_table_get_first_index(table);

	n_rows_in_table = cfg.get_n_rows(index->name);

	DBUG_EXECUTE_IF("ib_import_sec_rec_count_mismatch_failure",
			n_rows_in_table++;);

	/* Adjust the root pages of the secondary indexes only. */
	while ((index = dict_table_get_next_index(index)) != NULL) {
		char		index_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			index_name, sizeof(index_name), index->name, TRUE);

		ut_a(!dict_index_is_clust(index));

		if (!(index->type & DICT_CORRUPT)
		    && index->space != FIL_NULL
		    && index->page != FIL_NULL) {

			/* Update the Btree segment headers for index node and
			leaf nodes in the root page. Set the new space id. */

			err = btr_root_adjust_on_import(index);
		} else {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Skip adjustment of root pages for "
				"index %s.", index->name);

			err = DB_CORRUPTION;
		}

		if (err != DB_SUCCESS) {

			if (index->type & DICT_CLUSTERED) {
				break;
			}

			ib_errf(trx->mysql_thd,
				IB_LOG_LEVEL_WARN,
				ER_INNODB_INDEX_CORRUPT,
				"Index '%s' not found or corrupt, "
				"you should recreate this index.",
				index_name);

			/* Do not bail out, so that the data
			can be recovered. */

			err = DB_SUCCESS;
			index->type |= DICT_CORRUPT;
			continue;
		}

		/* If we failed to purge any records in the index then
		do it the hard way.

		TODO: We can do this in the first pass by generating UNDO log
		records for the failed rows. */

		if (!cfg.requires_purge(index->name)) {
			continue;
		}

		IndexPurge   purge(trx, index);

		trx->op_info = "secondary: purge delete marked records";

		err = purge.garbage_collect();

		trx->op_info = "";

		if (err != DB_SUCCESS) {
			break;
		} else if (purge.get_n_rows() != n_rows_in_table) {

			ib_errf(trx->mysql_thd,
				IB_LOG_LEVEL_WARN,
				ER_INNODB_INDEX_CORRUPT,
				"Index '%s' contains %lu entries, "
				"should be %lu, you should recreate "
				"this index.", index_name,
				(ulong) purge.get_n_rows(),
				(ulong) n_rows_in_table);

			index->type |= DICT_CORRUPT;

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
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_set_sys_max_row_id(
/*==========================*/
	row_prebuilt_t*		prebuilt,	/*!< in/out: prebuilt from
						handler */
	const dict_table_t*	table)		/*!< in: table to import */
{
	dberr_t			err;
	const rec_t*		rec;
	mtr_t			mtr;
	btr_pcur_t		pcur;
	row_id_t		row_id	= 0;
	dict_index_t*		index;

	index = dict_table_get_first_index(table);
	ut_a(dict_index_is_clust(index));

	mtr_start(&mtr);

	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

	btr_pcur_open_at_index_side(
		false,		// High end
		index,
		BTR_SEARCH_LEAF,
		&pcur,
		true,		// Init cursor
		0,		// Leaf level
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

		ib_errf(prebuilt->trx->mysql_thd,
			IB_LOG_LEVEL_WARN,
			ER_INNODB_INDEX_CORRUPT,
			"Index '%s' corruption detected, invalid DB_ROW_ID "
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
dberr_t
row_import_cfg_read_string(
/*=======================*/
	FILE*		file,		/*!< in/out: File to read from */
	byte*		ptr,		/*!< out: string to read */
	ulint		max_len)	/*!< in: maximum length of the output
					buffer in bytes */
{
	DBUG_EXECUTE_IF("ib_import_string_read_error",
			errno = EINVAL; return(DB_IO_ERROR););

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

	errno = EINVAL;

	return(DB_IO_ERROR);
}

/*********************************************************************//**
Write the meta data (index user fields) config file.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_cfg_read_index_fields(
/*=============================*/
	FILE*			file,	/*!< in: file to write to */
	THD*			thd,	/*!< in/out: session */
	row_index_t*		index,	/*!< Index being read in */
	row_import*		cfg)	/*!< in/out: meta-data read */
{
	byte			row[sizeof(ib_uint32_t) * 3];
	ulint			n_fields = index->m_n_fields;

	index->m_fields = new(std::nothrow) dict_field_t[n_fields];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_4",
			delete [] index->m_fields; index->m_fields = 0;);

	if (index->m_fields == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	dict_field_t*	field = index->m_fields;

	memset(field, 0x0, sizeof(*field) * n_fields);

	for (ulint i = 0; i < n_fields; ++i, ++field) {
		byte*		ptr = row;

		/* Trigger EOF */
		DBUG_EXECUTE_IF("ib_import_io_read_error_1",
				(void) fseek(file, 0L, SEEK_END););

		if (fread(row, 1, sizeof(row), file) != sizeof(row)) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				errno, strerror(errno),
				"while reading index fields.");

			return(DB_IO_ERROR);
		}

		field->prefix_len = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		field->fixed_len = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		/* Include the NUL byte in the length. */
		ulint	len = mach_read_from_4(ptr);

		byte*	name = new(std::nothrow) byte[len];

		/* Trigger OOM */
		DBUG_EXECUTE_IF("ib_import_OOM_5", delete [] name; name = 0;);

		if (name == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		field->name = reinterpret_cast<const char*>(name);

		dberr_t	err = row_import_cfg_read_string(file, name, len);

		if (err != DB_SUCCESS) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				errno, strerror(errno),
				"while parsing table name.");

			return(err);
		}
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Read the index names and root page numbers of the indexes and set the values.
Row format [root_page_no, len of str, str ... ]
@return DB_SUCCESS or error code. */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_read_index_data(
/*=======================*/
	FILE*		file,		/*!< in: File to read from */
	THD*		thd,		/*!< in: session */
	row_import*	cfg)		/*!< in/out: meta-data read */
{
	byte*		ptr;
	row_index_t*	cfg_index;
	byte		row[sizeof(index_id_t) + sizeof(ib_uint32_t) * 9];

	/* FIXME: What is the max value? */
	ut_a(cfg->m_n_indexes > 0);
	ut_a(cfg->m_n_indexes < 1024);

	cfg->m_indexes = new(std::nothrow) row_index_t[cfg->m_n_indexes];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_6",
			delete [] cfg->m_indexes; cfg->m_indexes = 0;);

	if (cfg->m_indexes == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	memset(cfg->m_indexes, 0x0, sizeof(*cfg->m_indexes) * cfg->m_n_indexes);

	cfg_index = cfg->m_indexes;

	for (ulint i = 0; i < cfg->m_n_indexes; ++i, ++cfg_index) {
		/* Trigger EOF */
		DBUG_EXECUTE_IF("ib_import_io_read_error_2",
				(void) fseek(file, 0L, SEEK_END););

		/* Read the index data. */
		size_t	n_bytes = fread(row, 1, sizeof(row), file);

		/* Trigger EOF */
		DBUG_EXECUTE_IF("ib_import_io_read_error",
				(void) fseek(file, 0L, SEEK_END););

		if (n_bytes != sizeof(row)) {
			char	msg[BUFSIZ];

			ut_snprintf(msg, sizeof(msg),
				    "while reading index meta-data, expected "
				    "to read %lu bytes but read only %lu "
				    "bytes",
				    (ulong) sizeof(row), (ulong) n_bytes);

			ib_senderrf(
				thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				errno, strerror(errno), msg);

			ib_logf(IB_LOG_LEVEL_ERROR, "IO Error: %s", msg);

			return(DB_IO_ERROR);
		}

		ptr = row;

		cfg_index->m_id = mach_read_from_8(ptr);
		ptr += sizeof(index_id_t);

		cfg_index->m_space = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_page_no = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_type = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_trx_id_offset = mach_read_from_4(ptr);
		if (cfg_index->m_trx_id_offset != mach_read_from_4(ptr)) {
			ut_ad(0);
			/* Overflow. Pretend that the clustered index
			has a variable-length PRIMARY KEY. */
			cfg_index->m_trx_id_offset = 0;
		}
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_n_user_defined_cols = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_n_uniq = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_n_nullable = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		cfg_index->m_n_fields = mach_read_from_4(ptr);
		ptr += sizeof(ib_uint32_t);

		/* The NUL byte is included in the name length. */
		ulint	len = mach_read_from_4(ptr);

		if (len > OS_FILE_MAX_PATH) {
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				ER_INNODB_INDEX_CORRUPT,
				"Index name length (%lu) is too long, "
				"the meta-data is corrupt", len);

			return(DB_CORRUPTION);
		}

		cfg_index->m_name = new(std::nothrow) byte[len];

		/* Trigger OOM */
		DBUG_EXECUTE_IF("ib_import_OOM_7",
				delete [] cfg_index->m_name;
				cfg_index->m_name = 0;);

		if (cfg_index->m_name == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		dberr_t	err;

		err = row_import_cfg_read_string(file, cfg_index->m_name, len);

		if (err != DB_SUCCESS) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				errno, strerror(errno),
				"while parsing index name.");

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
dberr_t
row_import_read_indexes(
/*====================*/
	FILE*		file,		/*!< in: File to read from */
	THD*		thd,		/*!< in: session */
	row_import*	cfg)		/*!< in/out: meta-data read */
{
	byte		row[sizeof(ib_uint32_t)];

	/* Trigger EOF */
	DBUG_EXECUTE_IF("ib_import_io_read_error_3",
			(void) fseek(file, 0L, SEEK_END););

	/* Read the number of indexes. */
	if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while reading number of indexes.");

		return(DB_IO_ERROR);
	}

	cfg->m_n_indexes = mach_read_from_4(row);

	if (cfg->m_n_indexes == 0) {
		ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			"Number of indexes in meta-data file is 0");

		return(DB_CORRUPTION);

	} else if (cfg->m_n_indexes > 1024) {
		// FIXME: What is the upper limit? */
		ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			"Number of indexes in meta-data file is too high: %lu",
			(ulong) cfg->m_n_indexes);
		cfg->m_n_indexes = 0;

		return(DB_CORRUPTION);
	}

	return(row_import_read_index_data(file, thd, cfg));
}

/*********************************************************************//**
Read the meta data (table columns) config file. Deserialise the contents of
dict_col_t structure, along with the column name. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_read_columns(
/*====================*/
	FILE*			file,	/*!< in: file to write to */
	THD*			thd,	/*!< in/out: session */
	row_import*		cfg)	/*!< in/out: meta-data read */
{
	dict_col_t*		col;
	byte			row[sizeof(ib_uint32_t) * 8];

	/* FIXME: What should the upper limit be? */
	ut_a(cfg->m_n_cols > 0);
	ut_a(cfg->m_n_cols < 1024);

	cfg->m_cols = new(std::nothrow) dict_col_t[cfg->m_n_cols];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_8",
			delete [] cfg->m_cols; cfg->m_cols = 0;);

	if (cfg->m_cols == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	cfg->m_col_names = new(std::nothrow) byte* [cfg->m_n_cols];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_9",
			delete [] cfg->m_col_names; cfg->m_col_names = 0;);

	if (cfg->m_col_names == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	memset(cfg->m_cols, 0x0, sizeof(cfg->m_cols) * cfg->m_n_cols);
	memset(cfg->m_col_names, 0x0, sizeof(cfg->m_col_names) * cfg->m_n_cols);

	col = cfg->m_cols;

	for (ulint i = 0; i < cfg->m_n_cols; ++i, ++col) {
		byte*		ptr = row;

		/* Trigger EOF */
		DBUG_EXECUTE_IF("ib_import_io_read_error_4",
				(void) fseek(file, 0L, SEEK_END););

		if (fread(row, 1,  sizeof(row), file) != sizeof(row)) {
			ib_senderrf(
				thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				errno, strerror(errno),
				"while reading table column meta-data.");

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
			ib_errf(thd, IB_LOG_LEVEL_ERROR,
				ER_IO_READ_ERROR,
				"Column name length %lu, is invalid",
				(ulong) len);

			return(DB_CORRUPTION);
		}

		cfg->m_col_names[i] = new(std::nothrow) byte[len];

		/* Trigger OOM */
		DBUG_EXECUTE_IF("ib_import_OOM_10",
				delete [] cfg->m_col_names[i];
				cfg->m_col_names[i] = 0;);

		if (cfg->m_col_names[i] == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		dberr_t	err;

		err = row_import_cfg_read_string(
			file, cfg->m_col_names[i], len);

		if (err != DB_SUCCESS) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
				errno, strerror(errno),
				"while parsing table column name.");

			return(err);
		}
	}

	return(DB_SUCCESS);
}

/*****************************************************************//**
Read the contents of the <tablespace>.cfg file.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_read_v1(
/*===============*/
	FILE*		file,		/*!< in: File to read from */
	THD*		thd,		/*!< in: session */
	row_import*	cfg)		/*!< out: meta data */
{
	byte		value[sizeof(ib_uint32_t)];

	/* Trigger EOF */
	DBUG_EXECUTE_IF("ib_import_io_read_error_5",
			(void) fseek(file, 0L, SEEK_END););

	/* Read the hostname where the tablespace was exported. */
	if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while reading meta-data export hostname length.");

		return(DB_IO_ERROR);
	}

	ulint	len = mach_read_from_4(value);

	/* NUL byte is part of name length. */
	cfg->m_hostname = new(std::nothrow) byte[len];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_1",
			delete [] cfg->m_hostname; cfg->m_hostname = 0;);

	if (cfg->m_hostname == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	dberr_t	err = row_import_cfg_read_string(file, cfg->m_hostname, len);

	if (err != DB_SUCCESS) {

		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while parsing export hostname.");

		return(err);
	}

	/* Trigger EOF */
	DBUG_EXECUTE_IF("ib_import_io_read_error_6",
			(void) fseek(file, 0L, SEEK_END););

	/* Read the table name of tablespace that was exported. */
	if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while reading meta-data table name length.");

		return(DB_IO_ERROR);
	}

	len = mach_read_from_4(value);

	/* NUL byte is part of name length. */
	cfg->m_table_name = new(std::nothrow) byte[len];

	/* Trigger OOM */
	DBUG_EXECUTE_IF("ib_import_OOM_2",
			delete [] cfg->m_table_name; cfg->m_table_name = 0;);

	if (cfg->m_table_name == 0) {
		return(DB_OUT_OF_MEMORY);
	}

	err = row_import_cfg_read_string(file, cfg->m_table_name, len);

	if (err != DB_SUCCESS) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while parsing table name.");

		return(err);
	}

	ib_logf(IB_LOG_LEVEL_INFO,
		"Importing tablespace for table '%s' that was exported "
		"from host '%s'", cfg->m_table_name, cfg->m_hostname);

	byte		row[sizeof(ib_uint32_t) * 3];

	/* Trigger EOF */
	DBUG_EXECUTE_IF("ib_import_io_read_error_7",
			(void) fseek(file, 0L, SEEK_END););

	/* Read the autoinc value. */
	if (fread(row, 1, sizeof(ib_uint64_t), file) != sizeof(ib_uint64_t)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while reading autoinc value.");

		return(DB_IO_ERROR);
	}

	cfg->m_autoinc = mach_read_from_8(row);

	/* Trigger EOF */
	DBUG_EXECUTE_IF("ib_import_io_read_error_8",
			(void) fseek(file, 0L, SEEK_END););

	/* Read the tablespace page size. */
	if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while reading meta-data header.");

		return(DB_IO_ERROR);
	}

	byte*		ptr = row;

	cfg->m_page_size = mach_read_from_4(ptr);
	ptr += sizeof(ib_uint32_t);

	if (cfg->m_page_size != UNIV_PAGE_SIZE) {

		ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
			"Tablespace to be imported has a different "
			"page size than this server. Server page size "
			"is %lu, whereas tablespace page size is %lu",
			UNIV_PAGE_SIZE, (ulong) cfg->m_page_size);

		return(DB_ERROR);
	}

	cfg->m_flags = mach_read_from_4(ptr);
	ptr += sizeof(ib_uint32_t);

	cfg->m_n_cols = mach_read_from_4(ptr);

	if (!dict_tf_is_valid(cfg->m_flags)) {

		return(DB_CORRUPTION);

	} else if ((err = row_import_read_columns(file, thd, cfg))
		   != DB_SUCCESS) {

		return(err);

	} else  if ((err = row_import_read_indexes(file, thd, cfg))
		   != DB_SUCCESS) {

		return(err);
	}

	ut_a(err == DB_SUCCESS);
	return(err);
}

/**
Read the contents of the <tablespace>.cfg file.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_read_meta_data(
/*======================*/
	dict_table_t*	table,		/*!< in: table */
	FILE*		file,		/*!< in: File to read from */
	THD*		thd,		/*!< in: session */
	row_import&	cfg)		/*!< out: contents of the .cfg file */
{
	byte		row[sizeof(ib_uint32_t)];

	/* Trigger EOF */
	DBUG_EXECUTE_IF("ib_import_io_read_error_9",
			(void) fseek(file, 0L, SEEK_END););

	if (fread(&row, 1, sizeof(row), file) != sizeof(row)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			errno, strerror(errno),
			"while reading meta-data version.");

		return(DB_IO_ERROR);
	}

	cfg.m_version = mach_read_from_4(row);

	/* Check the version number. */
	switch (cfg.m_version) {
	case IB_EXPORT_CFG_VERSION_V1:

		return(row_import_read_v1(file, thd, &cfg));
	default:
		ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR,
			"Unsupported meta-data version number (%lu), "
			"file ignored", (ulong) cfg.m_version);
	}

	return(DB_ERROR);
}

/**
Read the contents of the <tablename>.cfg file.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_import_read_cfg(
/*================*/
	dict_table_t*	table,	/*!< in: table */
	THD*		thd,	/*!< in: session */
	row_import&	cfg)	/*!< out: contents of the .cfg file */
{
	dberr_t		err;
	char		name[OS_FILE_MAX_PATH];

	cfg.m_table = table;

	srv_get_meta_data_filename(table, name, sizeof(name));

	FILE*	file = fopen(name, "rb");

	if (file == NULL) {
		char	msg[BUFSIZ];

		ut_snprintf(msg, sizeof(msg),
			    "Error opening '%s', will attempt to import "
			    "without schema verification", name);

		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_READ_ERROR,
			errno, strerror(errno), msg);

		cfg.m_missing = true;

		err = DB_FAIL;
	} else {

		cfg.m_missing = false;

		err = row_import_read_meta_data(table, file, thd, cfg);
		fclose(file);
	}

	return(err);
}

/*****************************************************************//**
Update the <space, root page> of a table's indexes from the values
in the data dictionary.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
row_import_update_index_root(
/*=========================*/
	trx_t*			trx,		/*!< in/out: transaction that
						covers the update */
	const dict_table_t*	table,		/*!< in: Table for which we want
						to set the root page_no */
	bool			reset,		/*!< in: if true then set to
						FIL_NUL */
	bool			dict_locked)	/*!< in: Set to true if the
						caller already owns the
						dict_sys_t:: mutex. */

{
	const dict_index_t*	index;
	que_t*			graph = 0;
	dberr_t			err = DB_SUCCESS;

	static const char	sql[] = {
		"PROCEDURE UPDATE_INDEX_ROOT() IS\n"
		"BEGIN\n"
		"UPDATE SYS_INDEXES\n"
		"SET SPACE = :space,\n"
		"    PAGE_NO = :page,\n"
		"    TYPE = :type\n"
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
		ib_uint32_t	type;
		index_id_t	index_id;
		table_id_t	table_id;

		info = (graph != 0) ? graph->info : pars_info_create();

		mach_write_to_4(
			reinterpret_cast<byte*>(&type),
			index->type);

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

		/* If we set the corrupt bit during the IMPORT phase then
		we need to update the system tables. */
		pars_info_bind_int4_literal(info, "type", &type);
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

		DBUG_EXECUTE_IF("ib_import_internal_error",
				trx->error_state = DB_ERROR;);

		err = trx->error_state;

		if (err != DB_SUCCESS) {
			char		index_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				index_name, sizeof(index_name),
				index->name, TRUE);

			ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
				ER_INTERNAL_ERROR,
				"While updating the <space, root page "
				"number> of index %s - %s",
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
dberr_t
row_import_update_discarded_flag(
/*=============================*/
	trx_t*		trx,		/*!< in/out: transaction that
					covers the update */
	table_id_t	table_id,	/*!< in: Table for which we want
					to set the root table->flags2 */
	bool		discarded,	/*!< in: set MIX_LEN column bit
					to discarded, if true */
	bool		dict_locked)	/*!< in: set to true if the
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

	dberr_t	err = que_eval_sql(info, sql, !dict_locked, trx);

	ut_a(discard.n_recs == 1);
	ut_a(discard.flags2 != ULINT32_UNDEFINED);

	return(err);
}

/*****************************************************************//**
Imports a tablespace. The space id in the .ibd file must match the space id
of the table in the data dictionary.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_import_for_mysql(
/*=================*/
	dict_table_t*	table,		/*!< in/out: table */
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL */
{
	dberr_t		err;
	trx_t*		trx;
	ib_uint64_t	autoinc = 0;
	char		table_name[MAX_FULL_NAME_LEN + 1];
	char*		filepath = NULL;

	ut_ad(!srv_read_only_mode);

	innobase_format_name(
		table_name, sizeof(table_name), table->name, FALSE);

	ut_a(table->space);
	ut_ad(prebuilt->trx);
	ut_a(table->ibd_file_missing);

	trx_start_if_not_started(prebuilt->trx);

	trx = trx_allocate_for_mysql();

	/* So that the table is not DROPped during recovery. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	trx_start_if_not_started(trx);

	/* So that we can send error messages to the user. */
	trx->mysql_thd = prebuilt->trx->mysql_thd;

	/* Ensure that the table will be dropped by trx_rollback_active()
	in case of a crash. */

	trx->table_id = table->id;

	/* Assign an undo segment for the transaction, so that the
	transaction will be recovered after a crash. */

	mutex_enter(&trx->undo_mutex);

	err = trx_undo_assign_undo(trx, TRX_UNDO_UPDATE);

	mutex_exit(&trx->undo_mutex);

	DBUG_EXECUTE_IF("ib_import_undo_assign_failure",
			err = DB_TOO_MANY_CONCURRENT_TRXS;);

	if (err != DB_SUCCESS) {

		return(row_import_cleanup(prebuilt, trx, err));

	} else if (trx->update_undo == 0) {

		err = DB_TOO_MANY_CONCURRENT_TRXS;
		return(row_import_cleanup(prebuilt, trx, err));
	}

	prebuilt->trx->op_info = "read meta-data file";

	/* Prevent DDL operations while we are checking. */

	rw_lock_s_lock_func(&dict_operation_lock, 0, __FILE__, __LINE__);

	row_import	cfg;

	memset(&cfg, 0x0, sizeof(cfg));

	err = row_import_read_cfg(table, trx->mysql_thd, cfg);

	/* Check if the table column definitions match the contents
	of the config file. */

	if (err == DB_SUCCESS) {

		/* We have a schema file, try and match it with the our
		data dictionary. */

		err = cfg.match_schema(trx->mysql_thd);

		/* Update index->page and SYS_INDEXES.PAGE_NO to match the
		B-tree root page numbers in the tablespace. Use the index
		name from the .cfg file to find match. */

		if (err == DB_SUCCESS) {
			cfg.set_root_by_name();
			autoinc = cfg.m_autoinc;
		}

		rw_lock_s_unlock_gen(&dict_operation_lock, 0);

		DBUG_EXECUTE_IF("ib_import_set_index_root_failure",
				err = DB_TOO_MANY_CONCURRENT_TRXS;);

	} else if (cfg.m_missing) {

		rw_lock_s_unlock_gen(&dict_operation_lock, 0);

		/* We don't have a schema file, we will have to discover
		the index root pages from the .ibd file and skip the schema
		matching step. */

		ut_a(err == DB_FAIL);

		cfg.m_page_size = UNIV_PAGE_SIZE;

		FetchIndexRootPages	fetchIndexRootPages(table, trx);

		err = fil_tablespace_iterate(
			table, IO_BUFFER_SIZE(cfg.m_page_size),
			fetchIndexRootPages);

		if (err == DB_SUCCESS) {

			err = fetchIndexRootPages.build_row_import(&cfg);

			/* Update index->page and SYS_INDEXES.PAGE_NO
			to match the B-tree root page numbers in the
			tablespace. */

			if (err == DB_SUCCESS) {
				err = cfg.set_root_by_heuristic();
			}
		}

	} else {
		rw_lock_s_unlock_gen(&dict_operation_lock, 0);
	}

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	prebuilt->trx->op_info = "importing tablespace";

	ib_logf(IB_LOG_LEVEL_INFO, "Phase I - Update all pages");

	/* Iterate over all the pages and do the sanity checking and
	the conversion required to import the tablespace. */

	PageConverter	converter(&cfg, trx);

	/* Set the IO buffer size in pages. */

	err = fil_tablespace_iterate(
		table, IO_BUFFER_SIZE(cfg.m_page_size), converter);

	DBUG_EXECUTE_IF("ib_import_reset_space_and_lsn_failure",
			err = DB_TOO_MANY_CONCURRENT_TRXS;);

	if (err != DB_SUCCESS) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name), table->name, FALSE);

		ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			ER_INTERNAL_ERROR,
			"Cannot reset LSNs in table '%s' : %s",
			table_name, ut_strerr(err));

		return(row_import_cleanup(prebuilt, trx, err));
	}

	row_mysql_lock_data_dictionary(trx);

	/* If the table is stored in a remote tablespace, we need to
	determine that filepath from the link file and system tables.
	Find the space ID in SYS_TABLES since this is an ALTER TABLE. */
	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		dict_get_and_save_data_dir_path(table, true);
		ut_a(table->data_dir_path);

		filepath = os_file_make_remote_pathname(
			table->data_dir_path, table->name, "ibd");
	} else {
		filepath = fil_make_ibd_name(table->name, false);
	}
	ut_a(filepath);

	/* Open the tablespace so that we can access via the buffer pool.
	We set the 2nd param (fix_dict = true) here because we already
	have an x-lock on dict_operation_lock and dict_sys->mutex. */

	err = fil_open_single_table_tablespace(
		true, true, table->space,
		dict_tf_to_fsp_flags(table->flags),
		table->name, filepath);

	DBUG_EXECUTE_IF("ib_import_open_tablespace_failure",
			err = DB_TABLESPACE_NOT_FOUND;);

	if (err != DB_SUCCESS) {
		row_mysql_unlock_data_dictionary(trx);

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			ER_FILE_NOT_FOUND,
			filepath, err, ut_strerr(err));

		mem_free(filepath);

		return(row_import_cleanup(prebuilt, trx, err));
	}

	row_mysql_unlock_data_dictionary(trx);

	mem_free(filepath);

	err = ibuf_check_bitmap_on_import(trx, table->space);

	DBUG_EXECUTE_IF("ib_import_check_bitmap_failure", err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_cleanup(prebuilt, trx, err));
	}

	/* The first index must always be the clustered index. */

	dict_index_t*	index = dict_table_get_first_index(table);

	if (!dict_index_is_clust(index)) {
		return(row_import_error(prebuilt, trx, DB_CORRUPTION));
	}

	/* Update the Btree segment headers for index node and
	leaf nodes in the root page. Set the new space id. */

	err = btr_root_adjust_on_import(index);

	DBUG_EXECUTE_IF("ib_import_cluster_root_adjust_failure",
			err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	} else if (cfg.requires_purge(index->name)) {

		/* Purge any delete-marked records that couldn't be
		purged during the page conversion phase from the
		cluster index. */

		IndexPurge	purge(trx, index);

		trx->op_info = "cluster: purging delete marked records";

		err = purge.garbage_collect();

		trx->op_info = "";
	}

	DBUG_EXECUTE_IF("ib_import_cluster_failure", err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	/* For secondary indexes, purge any records that couldn't be purged
	during the page conversion phase. */

	err = row_import_adjust_root_pages_of_secondary_indexes(
		prebuilt, trx, table, cfg);

	DBUG_EXECUTE_IF("ib_import_sec_root_adjust_failure",
			err = DB_CORRUPTION;);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	/* Ensure that the next available DB_ROW_ID is not smaller than
	any DB_ROW_ID stored in the table. */

	if (prebuilt->clust_index_was_generated) {

		err = row_import_set_sys_max_row_id(prebuilt, table);

		if (err != DB_SUCCESS) {
			return(row_import_error(prebuilt, trx, err));
		}
	}

	ib_logf(IB_LOG_LEVEL_INFO, "Phase III - Flush changes to disk");

	/* Ensure that all pages dirtied during the IMPORT make it to disk.
	The only dirty pages generated should be from the pessimistic purge
	of delete marked records that couldn't be purged in Phase I. */

	buf_LRU_flush_or_remove_pages(
		prebuilt->table->space, BUF_REMOVE_FLUSH_WRITE, trx);

	if (trx_is_interrupted(trx)) {
		ib_logf(IB_LOG_LEVEL_INFO, "Phase III - Flush interrupted");
		return(row_import_error(prebuilt, trx, DB_INTERRUPTED));
	} else {
		ib_logf(IB_LOG_LEVEL_INFO, "Phase IV - Flush complete");
	}

	/* The dictionary latches will be released in in row_import_cleanup()
	after the transaction commit, for both success and error. */

	row_mysql_lock_data_dictionary(trx);

	/* Update the root pages of the table's indexes. */
	err = row_import_update_index_root(trx, table, false, true);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	/* Update the table's discarded flag, unset it. */
	err = row_import_update_discarded_flag(trx, table->id, false, true);

	if (err != DB_SUCCESS) {
		return(row_import_error(prebuilt, trx, err));
	}

	table->ibd_file_missing = false;
	table->flags2 &= ~DICT_TF2_DISCARDED;

	if (autoinc != 0) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name), table->name, FALSE);

		ib_logf(IB_LOG_LEVEL_INFO, "%s autoinc value set to " IB_ID_FMT,
			table_name, autoinc);

		dict_table_autoinc_lock(table);
		dict_table_autoinc_initialize(table, autoinc);
		dict_table_autoinc_unlock(table);
	}

	ut_a(err == DB_SUCCESS);

	return(row_import_cleanup(prebuilt, trx, err));
}

