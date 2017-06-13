/*****************************************************************************

Copyright (c) 2013, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0trunc.cc
TRUNCATE implementation

Created 2013-04-12 Sunny Bains
*******************************************************/

#include "row0mysql.h"
#include "pars0pars.h"
#include "dict0crea.h"
#include "dict0boot.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "lock0lock.h"
#include "fts0fts.h"
#include "fsp0sysspace.h"
#include "srv0start.h"
#include "row0trunc.h"
#include "os0file.h"
#include <vector>

bool	truncate_t::s_fix_up_active = false;
truncate_t::tables_t		truncate_t::s_tables;
truncate_t::truncated_tables_t	truncate_t::s_truncated_tables;

/**
Iterator over the the raw records in an index, doesn't support MVCC. */
class IndexIterator {

public:
	/**
	Iterate over an indexes records
	@param index		index to iterate over */
	explicit IndexIterator(dict_index_t* index)
		:
		m_index(index)
	{
		/* Do nothing */
	}

	/**
	Search for key. Position the cursor on a record GE key.
	@return DB_SUCCESS or error code. */
	dberr_t search(dtuple_t& key, bool noredo)
	{
		mtr_start(&m_mtr);

		if (noredo) {
			mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);
		}

		btr_pcur_open_on_user_rec(
			m_index,
			&key,
			PAGE_CUR_GE,
			BTR_MODIFY_LEAF,
			&m_pcur, &m_mtr);

		return(DB_SUCCESS);
	}

	/**
	Iterate over all the records
	@return DB_SUCCESS or error code */
	template <typename Callback>
	dberr_t for_each(Callback& callback)
	{
		dberr_t	err = DB_SUCCESS;

		for (;;) {

			if (!btr_pcur_is_on_user_rec(&m_pcur)
			    || !callback.match(&m_mtr, &m_pcur)) {

				/* The end of of the index has been reached. */
				err = DB_END_OF_INDEX;
				break;
			}

			rec_t*	rec = btr_pcur_get_rec(&m_pcur);

			if (!rec_get_deleted_flag(rec, FALSE)) {

				err = callback(&m_mtr, &m_pcur);

				if (err != DB_SUCCESS) {
					break;
				}
			}

			btr_pcur_move_to_next_user_rec(&m_pcur, &m_mtr);
		}

		btr_pcur_close(&m_pcur);
		mtr_commit(&m_mtr);

		return(err == DB_END_OF_INDEX ? DB_SUCCESS : err);
	}

private:
	// Disable copying
	IndexIterator(const IndexIterator&);
	IndexIterator& operator=(const IndexIterator&);

private:
	mtr_t		m_mtr;
	btr_pcur_t	m_pcur;
	dict_index_t*	m_index;
};

/** SysIndex table iterator, iterate over records for a table. */
class SysIndexIterator {

public:
	/**
	Iterate over all the records that match the table id.
	@return DB_SUCCESS or error code */
	template <typename Callback>
	dberr_t for_each(Callback& callback) const
	{
		dict_index_t*	sys_index;
		byte		buf[DTUPLE_EST_ALLOC(1)];
		dtuple_t*	tuple =
			dtuple_create_from_mem(buf, sizeof(buf), 1, 0);
		dfield_t*	dfield = dtuple_get_nth_field(tuple, 0);

		dfield_set_data(
			dfield,
			callback.table_id(),
			sizeof(*callback.table_id()));

		sys_index = dict_table_get_first_index(dict_sys->sys_indexes);

		dict_index_copy_types(tuple, sys_index, 1);

		IndexIterator	iterator(sys_index);

		/* Search on the table id and position the cursor
		on GE table_id. */
		iterator.search(*tuple, callback.get_logging_status());

		return(iterator.for_each(callback));
	}
};

/** Generic callback abstract class. */
class Callback
{

public:
	/**
	Constructor
	@param	table_id		id of the table being operated.
	@param	noredo			if true turn off logging. */
	Callback(table_id_t table_id, bool noredo)
		:
		m_id(),
		m_noredo(noredo)
	{
		/* Convert to storage byte order. */
		mach_write_to_8(&m_id, table_id);
	}

	/**
	Destructor */
	virtual ~Callback()
	{
		/* Do nothing */
	}

	/**
	@param mtr		mini-transaction covering the iteration
	@param pcur		persistent cursor used for iteration
	@return true if the table id column matches. */
	bool match(mtr_t* mtr, btr_pcur_t* pcur) const
	{
		ulint		len;
		const byte*	field;
		rec_t*		rec = btr_pcur_get_rec(pcur);

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_INDEXES__TABLE_ID, &len);

		ut_ad(len == 8);

		return(memcmp(&m_id, field, len) == 0);
	}

	/**
	@return pointer to table id storage format buffer */
	const table_id_t* table_id() const
	{
		return(&m_id);
	}

	/**
	@return	return if logging needs to be turned off. */
	bool get_logging_status() const
	{
		return(m_noredo);
	}

protected:
	// Disably copying
	Callback(const Callback&);
	Callback& operator=(const Callback&);

protected:
	/** Table id in storage format */
	table_id_t		m_id;

	/** Turn off logging. */
	const bool		m_noredo;
};

/**
Creates a TRUNCATE log record with space id, table name, data directory path,
tablespace flags, table format, index ids, index types, number of index fields
and index field information of the table. */
class TruncateLogger : public Callback {

public:
	/**
	Constructor

	@param table	Table to truncate
	@param flags	tablespace falgs */
	TruncateLogger(
		dict_table_t*	table,
		ulint		flags,
		table_id_t	new_table_id)
		:
		Callback(table->id, false),
		m_table(table),
		m_flags(flags),
		m_truncate(table->id, new_table_id, table->data_dir_path),
		m_log_file_name()
	{
		/* Do nothing */
	}

	/**
	Initialize Truncate Logger by constructing Truncate Log File Name.

	@return DB_SUCCESS or error code. */
	dberr_t init()
	{
		/* Construct log file name. */
		ulint	log_file_name_buf_sz =
			strlen(srv_log_group_home_dir) + 22 + 22 + 1 /* NUL */
			+ strlen(TruncateLogger::s_log_prefix)
			+ strlen(TruncateLogger::s_log_ext);

		m_log_file_name = UT_NEW_ARRAY_NOKEY(char, log_file_name_buf_sz);
		if (m_log_file_name == NULL) {
			return(DB_OUT_OF_MEMORY);
		}
		memset(m_log_file_name, 0, log_file_name_buf_sz);

		strcpy(m_log_file_name, srv_log_group_home_dir);
		ulint	log_file_name_len = strlen(m_log_file_name);
		if (m_log_file_name[log_file_name_len - 1]
			!= OS_PATH_SEPARATOR) {

			m_log_file_name[log_file_name_len]
				= OS_PATH_SEPARATOR;
			log_file_name_len = strlen(m_log_file_name);
		}

		ut_snprintf(m_log_file_name + log_file_name_len,
			    log_file_name_buf_sz - log_file_name_len,
			    "%s%lu_%lu_%s",
			    TruncateLogger::s_log_prefix,
			    (ulong) m_table->space,
			    (ulong) m_table->id,
			    TruncateLogger::s_log_ext);

		return(DB_SUCCESS);

	}

	/**
	Destructor */
	~TruncateLogger()
	{
		if (m_log_file_name != NULL) {
			bool exist;
			os_file_delete_if_exists(
				innodb_log_file_key, m_log_file_name, &exist);
			UT_DELETE_ARRAY(m_log_file_name);
			m_log_file_name = NULL;
		}
	}

	/**
	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur);

	/** Called after iteratoring over the records.
	@return true if invariant satisfied. */
	bool debug() const
	{
		/* We must find all the index entries on disk. */
		return(UT_LIST_GET_LEN(m_table->indexes)
		       == m_truncate.indexes());
	}

	/**
	Write the TRUNCATE log
	@return DB_SUCCESS or error code */
	dberr_t log() const
	{
		dberr_t	err = DB_SUCCESS;

		if (m_log_file_name == 0) {
			return(DB_ERROR);
		}

		bool		ret;
		pfs_os_file_t	handle = os_file_create(
			innodb_log_file_key, m_log_file_name,
			OS_FILE_CREATE, OS_FILE_NORMAL,
			OS_LOG_FILE, srv_read_only_mode, &ret);
		if (!ret) {
			return(DB_IO_ERROR);
		}


		ulint	sz = UNIV_PAGE_SIZE;
		void*	buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);
		if (buf == 0) {
			os_file_close(handle);
			return(DB_OUT_OF_MEMORY);
		}

		/* Align the memory for file i/o if we might have O_DIRECT set*/
		byte*	log_buf = static_cast<byte*>(
			ut_align(buf, UNIV_PAGE_SIZE));

		lsn_t	lsn = log_get_lsn();

		/* Generally loop should exit in single go but
		just for those 1% of rare cases we need to assume
		corner case. */
		do {
			/* First 4 bytes are reserved for magic number
			which is currently 0. */
			err = m_truncate.write(
				log_buf + 4, log_buf + sz - 4,
				m_table->space, m_table->name.m_name,
				m_flags, m_table->flags, lsn);

			DBUG_EXECUTE_IF("ib_err_trunc_oom_logging",
					err = DB_FAIL;);

			if (err != DB_SUCCESS) {
				ut_ad(err == DB_FAIL);
				ut_free(buf);
				sz *= 2;
				buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);
				DBUG_EXECUTE_IF("ib_err_trunc_oom_logging",
						ut_free(buf);
						buf = 0;);
				if (buf == 0) {
					os_file_close(handle);
					return(DB_OUT_OF_MEMORY);
				}
				log_buf = static_cast<byte*>(
					ut_align(buf, UNIV_PAGE_SIZE));
			}

		} while (err != DB_SUCCESS);

		dberr_t	io_err;

		IORequest	request(IORequest::WRITE);

		request.disable_compression();

		io_err = os_file_write(
			request, m_log_file_name, handle, log_buf, 0, sz);

		if (io_err != DB_SUCCESS) {

			ib::error()
				<< "IO: Failed to write the file size to '"
				<< m_log_file_name << "'";

			/* Preserve the original error code */
			if (err == DB_SUCCESS) {
				err = io_err;
			}
		}

		os_file_flush(handle);
		os_file_close(handle);

		ut_free(buf);

		/* Why we need MLOG_TRUNCATE when we have truncate_log for
		recovery?
		- truncate log can protect us if crash happens while truncate
		  is active. Once truncate is done truncate log is removed.
		- If crash happens post truncate and system is yet to
		  checkpoint, on recovery we would see REDO records from action
		  before truncate (unless we explicitly checkpoint before
		  returning from truncate API. Costly alternative so rejected).
		- These REDO records may reference a page that doesn't exist
		  post truncate so we need a mechanism to skip all such REDO
		  records. MLOG_TRUNCATE records space_id and lsn that exactly
		  serve the purpose.
		- If checkpoint happens post truncate and crash happens post
		  this point then neither MLOG_TRUNCATE nor REDO record
		  from action before truncate are accessible. */
		if (!is_system_tablespace(m_table->space)) {
			mtr_t	mtr;
			byte*	log_ptr;

			mtr_start(&mtr);

			log_ptr = mlog_open(&mtr, 11 + 8);
			log_ptr = mlog_write_initial_log_record_low(
				MLOG_TRUNCATE, m_table->space, 0,
				log_ptr, &mtr);

			mach_write_to_8(log_ptr, lsn);
			log_ptr += 8;

			mlog_close(&mtr, log_ptr);
			mtr_commit(&mtr);
		}

		return(err);
	}

	/**
	Indicate completion of truncate log by writing magic-number.
	File will be removed from the system but to protect against
	unlink (File-System) anomalies we ensure we write magic-number. */
	void done()
	{
		if (m_log_file_name == 0) {
			return;
		}

		bool	ret;
		pfs_os_file_t handle = os_file_create_simple_no_error_handling(
			innodb_log_file_key, m_log_file_name,
			OS_FILE_OPEN, OS_FILE_READ_WRITE,
			srv_read_only_mode, &ret);
		DBUG_EXECUTE_IF("ib_err_trunc_writing_magic_number",
				os_file_close(handle);
				ret = false;);
		if (!ret) {
			ib::error() << "Failed to open truncate log file "
				<< m_log_file_name << "."
				" If server crashes before truncate log is"
				" removed make sure it is manually removed"
				" before restarting server";
			os_file_delete(innodb_log_file_key, m_log_file_name);
			return;
		}

		byte	buffer[sizeof(TruncateLogger::s_magic)];
		mach_write_to_4(buffer, TruncateLogger::s_magic);

		dberr_t	err;

		IORequest	request(IORequest::WRITE);

		request.disable_compression();

		err = os_file_write(
			request,
			m_log_file_name, handle, buffer, 0, sizeof(buffer));

		if (err != DB_SUCCESS) {

			ib::error()
				<< "IO: Failed to write the magic number to '"
				<< m_log_file_name << "'";
		}

		DBUG_EXECUTE_IF("ib_trunc_crash_after_updating_magic_no",
				DBUG_SUICIDE(););
		os_file_flush(handle);
		os_file_close(handle);
		DBUG_EXECUTE_IF("ib_trunc_crash_after_logging_complete",
				log_buffer_flush_to_disk();
				os_thread_sleep(1000000);
				DBUG_SUICIDE(););
		os_file_delete(innodb_log_file_key, m_log_file_name);
	}

private:
	// Disably copying
	TruncateLogger(const TruncateLogger&);
	TruncateLogger& operator=(const TruncateLogger&);

private:
	/** Lookup the index using the index id.
	@return index instance if found else NULL */
	const dict_index_t* find(index_id_t id) const
	{
		for (const dict_index_t* index = UT_LIST_GET_FIRST(
				m_table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			if (index->id == id) {
				return(index);
			}
		}

		return(NULL);
	}

private:
	/** Table to be truncated */
	dict_table_t*		m_table;

	/** Tablespace flags */
	ulint			m_flags;

	/** Collect table to truncate information */
	truncate_t		m_truncate;

	/** Truncate log file name. */
	char*			m_log_file_name;


public:
	/** Magic Number to indicate truncate action is complete. */
	const static ib_uint32_t	s_magic;

	/** Truncate Log file Prefix. */
	const static char*		s_log_prefix;

	/** Truncate Log file Extension. */
	const static char*		s_log_ext;
};

const ib_uint32_t	TruncateLogger::s_magic = 32743712;
const char*		TruncateLogger::s_log_prefix = "ib_";
const char*		TruncateLogger::s_log_ext = "trunc.log";

/**
Scan to find out truncate log file from the given directory path.

@param dir_path		look for log directory in following path.
@param log_files	cache to hold truncate log file name found.
@return DB_SUCCESS or error code. */
dberr_t
TruncateLogParser::scan(
	const char*		dir_path,
	trunc_log_files_t&	log_files)
{
	os_file_dir_t	dir;
	os_file_stat_t	fileinfo;
	dberr_t		err = DB_SUCCESS;
	ulint		ext_len = strlen(TruncateLogger::s_log_ext);
	ulint		prefix_len = strlen(TruncateLogger::s_log_prefix);
	ulint		dir_len = strlen(dir_path);

	/* Scan and look out for the truncate log files. */
	dir = os_file_opendir(dir_path, true);
	if (dir == NULL) {
		return(DB_IO_ERROR);
	}

	while (fil_file_readdir_next_file(
			&err, dir_path, dir, &fileinfo) == 0) {

		ulint nm_len = strlen(fileinfo.name);

		if (fileinfo.type == OS_FILE_TYPE_FILE
		    && nm_len > ext_len + prefix_len
		    && (0 == strncmp(fileinfo.name + nm_len - ext_len,
				     TruncateLogger::s_log_ext, ext_len))
		    && (0 == strncmp(fileinfo.name,
				     TruncateLogger::s_log_prefix,
				     prefix_len))) {

			if (fileinfo.size == 0) {
				/* Truncate log not written. Remove the file. */
				os_file_delete(
					innodb_log_file_key, fileinfo.name);
				continue;
			}

			/* Construct file name by appending directory path */
			ulint	sz = dir_len + 22 + 22 + 1 + ext_len + prefix_len;
			char*	log_file_name = UT_NEW_ARRAY_NOKEY(char, sz);
			if (log_file_name == NULL) {
				err = DB_OUT_OF_MEMORY;
				break;
			}
			memset(log_file_name, 0, sz);

			strncpy(log_file_name, dir_path, dir_len);
			ulint	log_file_name_len = strlen(log_file_name);
			if (log_file_name[log_file_name_len - 1]
				!= OS_PATH_SEPARATOR) {

				log_file_name[log_file_name_len]
					= OS_PATH_SEPARATOR;
				log_file_name_len = strlen(log_file_name);
			}
			strcat(log_file_name, fileinfo.name);
			log_files.push_back(log_file_name);
		}
	}

	os_file_closedir(dir);

	return(err);
}

/**
Parse the log file and populate table to truncate information.
(Add this table to truncate information to central vector that is then
 used by truncate fix-up routine to fix-up truncate action of the table.)

@param	log_file_name	log file to parse
@return DB_SUCCESS or error code. */
dberr_t
TruncateLogParser::parse(
	const char*	log_file_name)
{
	dberr_t		err = DB_SUCCESS;
	truncate_t*	truncate = NULL;

	/* Open the file and read magic-number to findout if truncate action
	was completed. */
	bool		ret;
	pfs_os_file_t	handle = os_file_create_simple(
		innodb_log_file_key, log_file_name,
		OS_FILE_OPEN, OS_FILE_READ_ONLY, srv_read_only_mode, &ret);
	if (!ret) {
		ib::error() << "Error opening truncate log file: "
			<< log_file_name;
		return(DB_IO_ERROR);
	}

	ulint	sz = UNIV_PAGE_SIZE;
	void*	buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);
	if (buf == 0) {
		os_file_close(handle);
		return(DB_OUT_OF_MEMORY);
	}

	IORequest	request(IORequest::READ);

	request.disable_compression();

	/* Align the memory for file i/o if we might have O_DIRECT set*/
	byte*	log_buf = static_cast<byte*>(ut_align(buf, UNIV_PAGE_SIZE));

	do {
		err = os_file_read(request, handle, log_buf, 0, sz);

		if (err != DB_SUCCESS) {
			os_file_close(handle);
			break;
		}

		ulint	magic_n = mach_read_from_4(log_buf);
		if (magic_n == TruncateLogger::s_magic) {

			/* Truncate action completed. Avoid parsing the file. */
			os_file_close(handle);

			os_file_delete(innodb_log_file_key, log_file_name);
			break;
		}

		if (truncate == NULL) {
			truncate = UT_NEW_NOKEY(truncate_t(log_file_name));
			if (truncate == NULL) {
				os_file_close(handle);
				err = DB_OUT_OF_MEMORY;
				break;
			}
		}

		err = truncate->parse(log_buf + 4, log_buf + sz - 4);

		if (err != DB_SUCCESS) {

			ut_ad(err == DB_FAIL);

			ut_free(buf);
			buf = 0;

			sz *= 2;

			buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);

			if (buf == 0) {
				os_file_close(handle);
				err = DB_OUT_OF_MEMORY;
				UT_DELETE(truncate);
				truncate = NULL;
				break;
			}

			log_buf = static_cast<byte*>(
				ut_align(buf, UNIV_PAGE_SIZE));
		}
	} while (err != DB_SUCCESS);

	ut_free(buf);

	if (err == DB_SUCCESS && truncate != NULL) {
		truncate_t::add(truncate);
		os_file_close(handle);
	}

	return(err);
}

/**
Scan and Parse truncate log files.

@param dir_path		look for log directory in following path
@return DB_SUCCESS or error code. */
dberr_t
TruncateLogParser::scan_and_parse(
	const char*	dir_path)
{
	dberr_t			err;
	trunc_log_files_t	log_files;

	/* Scan and trace all the truncate log files. */
	err = TruncateLogParser::scan(dir_path, log_files);

	/* Parse truncate lof files if scan was successful. */
	if (err == DB_SUCCESS) {

		for (ulint i = 0;
		     i < log_files.size() && err == DB_SUCCESS;
		     i++) {
			err = TruncateLogParser::parse(log_files[i]);
		}
	}

	trunc_log_files_t::const_iterator end = log_files.end();
	for (trunc_log_files_t::const_iterator it = log_files.begin();
	     it != end;
	     ++it) {
		if (*it != NULL) {
			UT_DELETE_ARRAY(*it);
		}
	}
	log_files.clear();

	return(err);
}

/** Callback to drop indexes during TRUNCATE */
class DropIndex : public Callback {

public:
	/**
	Constructor

	@param[in,out]	table	Table to truncate
	@param[in]	noredo	whether to disable redo logging */
	DropIndex(dict_table_t* table, bool noredo)
		:
		Callback(table->id, noredo),
		m_table(table)
	{
		/* No op */
	}

	/**
	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur) const;

private:
	/** Table to be truncated */
	dict_table_t*		m_table;
};

/** Callback to create the indexes during TRUNCATE */
class CreateIndex : public Callback {

public:
	/**
	Constructor

	@param[in,out]	table	Table to truncate
	@param[in]	noredo	whether to disable redo logging */
	CreateIndex(dict_table_t* table, bool noredo)
		:
		Callback(table->id, noredo),
		m_table(table)
	{
		/* No op */
	}

	/**
	Create the new index and update the root page number in the
	SysIndex table.

	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur) const;

private:
	// Disably copying
	CreateIndex(const CreateIndex&);
	CreateIndex& operator=(const CreateIndex&);

private:
	/** Table to be truncated */
	dict_table_t*		m_table;
};

/** Check for presence of table-id in SYS_XXXX tables. */
class TableLocator : public Callback {

public:
	/**
	Constructor
	@param table_id	table_id to look for */
	explicit TableLocator(table_id_t table_id)
		:
		Callback(table_id, false),
		m_table_found()
	{
		/* No op */
	}

	/**
	@return true if table is found */
	bool is_table_found() const
	{
		return(m_table_found);
	}

	/**
	Look for table-id in SYS_XXXX tables without loading the table.

	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur);

private:
	// Disably copying
	TableLocator(const TableLocator&);
	TableLocator& operator=(const TableLocator&);

private:
	/** Set to true if table is present */
	bool			m_table_found;
};

/**
@param mtr	mini-transaction covering the read
@param pcur	persistent cursor used for reading
@return DB_SUCCESS or error code */
dberr_t
TruncateLogger::operator()(mtr_t* mtr, btr_pcur_t* pcur)
{
	ulint			len;
	const byte*		field;
	rec_t*			rec = btr_pcur_get_rec(pcur);
	truncate_t::index_t	index;

	field = rec_get_nth_field_old(
		rec, DICT_FLD__SYS_INDEXES__TYPE, &len);
	ut_ad(len == 4);
	index.m_type = mach_read_from_4(field);

	field = rec_get_nth_field_old(rec, DICT_FLD__SYS_INDEXES__ID, &len);
	ut_ad(len == 8);
	index.m_id = mach_read_from_8(field);

	field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_INDEXES__PAGE_NO, &len);
	ut_ad(len == 4);
	index.m_root_page_no = mach_read_from_4(field);

	/* For compressed tables we need to store extra meta-data
	required during btr_create(). */
	if (fsp_flags_is_compressed(m_flags)) {

		const dict_index_t* dict_index = find(index.m_id);

		if (dict_index != NULL) {

			dberr_t err = index.set(dict_index);

			if (err != DB_SUCCESS) {
				m_truncate.clear();
				return(err);
			}

		} else {
			ib::warn() << "Index id " << index.m_id
				<< " not found";
		}
	}

	m_truncate.add(index);

	return(DB_SUCCESS);
}

/**
Drop an index in the table.

@param mtr	mini-transaction covering the read
@param pcur	persistent cursor used for reading
@return DB_SUCCESS or error code */
dberr_t
DropIndex::operator()(mtr_t* mtr, btr_pcur_t* pcur) const
{
	rec_t*	rec = btr_pcur_get_rec(pcur);

	bool	freed = dict_drop_index_tree(rec, pcur, mtr);

#ifdef UNIV_DEBUG
	{
		ulint		len;
		const byte*	field;
		ulint		index_type;

		field = rec_get_nth_field_old(
			btr_pcur_get_rec(pcur), DICT_FLD__SYS_INDEXES__TYPE,
			&len);
		ut_ad(len == 4);

		index_type = mach_read_from_4(field);

		if (index_type & DICT_CLUSTERED) {
			/* Clustered index */
			DBUG_EXECUTE_IF("ib_trunc_crash_on_drop_of_clust_index",
					log_buffer_flush_to_disk();
					os_thread_sleep(2000000);
					DBUG_SUICIDE(););
		} else if (index_type & DICT_UNIQUE) {
			/* Unique index */
			DBUG_EXECUTE_IF("ib_trunc_crash_on_drop_of_uniq_index",
					log_buffer_flush_to_disk();
					os_thread_sleep(2000000);
					DBUG_SUICIDE(););
		} else if (index_type == 0) {
			/* Secondary index */
			DBUG_EXECUTE_IF("ib_trunc_crash_on_drop_of_sec_index",
					log_buffer_flush_to_disk();
					os_thread_sleep(2000000);
					DBUG_SUICIDE(););
		}
	}
#endif /* UNIV_DEBUG */

	DBUG_EXECUTE_IF("ib_err_trunc_drop_index",
			freed = false;);

	if (freed) {

		/* We will need to commit and restart the
		mini-transaction in order to avoid deadlocks.
		The dict_drop_index_tree() call has freed
		a page in this mini-transaction, and the rest
		of this loop could latch another index page.*/
		const mtr_log_t log_mode = mtr->get_log_mode();
		mtr_commit(mtr);

		mtr_start(mtr);
		mtr->set_log_mode(log_mode);

		btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur, mtr);
	} else {
		/* Check if the .ibd file is missing. */
		bool	found;

		fil_space_get_page_size(m_table->space, &found);

		DBUG_EXECUTE_IF("ib_err_trunc_drop_index",
				found = false;);

		if (!found) {
			return(DB_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/**
Create the new index and update the root page number in the
SysIndex table.

@param mtr	mini-transaction covering the read
@param pcur	persistent cursor used for reading
@return DB_SUCCESS or error code */
dberr_t
CreateIndex::operator()(mtr_t* mtr, btr_pcur_t* pcur) const
{
	ulint	root_page_no;

	root_page_no = dict_recreate_index_tree(m_table, pcur, mtr);

#ifdef UNIV_DEBUG
	{
		ulint		len;
		const byte*	field;
		ulint		index_type;

		field = rec_get_nth_field_old(
			btr_pcur_get_rec(pcur), DICT_FLD__SYS_INDEXES__TYPE,
			&len);
		ut_ad(len == 4);

		index_type = mach_read_from_4(field);

		if (index_type & DICT_CLUSTERED) {
			/* Clustered index */
			DBUG_EXECUTE_IF(
				"ib_trunc_crash_on_create_of_clust_index",
				log_buffer_flush_to_disk();
				os_thread_sleep(2000000);
				DBUG_SUICIDE(););
		} else if (index_type & DICT_UNIQUE) {
			/* Unique index */
			DBUG_EXECUTE_IF(
				"ib_trunc_crash_on_create_of_uniq_index",
				log_buffer_flush_to_disk();
				os_thread_sleep(2000000);
				DBUG_SUICIDE(););
		} else if (index_type == 0) {
			/* Secondary index */
			DBUG_EXECUTE_IF(
				"ib_trunc_crash_on_create_of_sec_index",
				log_buffer_flush_to_disk();
				os_thread_sleep(2000000);
				DBUG_SUICIDE(););
		}
	}
#endif /* UNIV_DEBUG */

	DBUG_EXECUTE_IF("ib_err_trunc_create_index",
			root_page_no = FIL_NULL;);

	if (root_page_no != FIL_NULL) {

		rec_t*	rec = btr_pcur_get_rec(pcur);

		page_rec_write_field(
			rec, DICT_FLD__SYS_INDEXES__PAGE_NO,
			root_page_no, mtr);

		/* We will need to commit and restart the
		mini-transaction in order to avoid deadlocks.
		The dict_create_index_tree() call has allocated
		a page in this mini-transaction, and the rest of
		this loop could latch another index page. */
		mtr_commit(mtr);

		mtr_start(mtr);

		btr_pcur_restore_position(BTR_MODIFY_LEAF, pcur, mtr);

	} else {
		bool	found;
		fil_space_get_page_size(m_table->space, &found);

		DBUG_EXECUTE_IF("ib_err_trunc_create_index",
				found = false;);

		if (!found) {
			return(DB_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/**
Look for table-id in SYS_XXXX tables without loading the table.

@param mtr	mini-transaction covering the read
@param pcur	persistent cursor used for reading
@return DB_SUCCESS */
dberr_t
TableLocator::operator()(mtr_t* mtr, btr_pcur_t* pcur)
{
	m_table_found = true;

	return(DB_SUCCESS);
}

/**
Rollback the transaction and release the index locks.
Drop indexes if table is corrupted so that drop/create
sequence works as expected.

@param table			table to truncate
@param trx			transaction covering the TRUNCATE
@param new_id			new table id that was suppose to get assigned
				to the table if truncate executed successfully.
@param has_internal_doc_id	indicate existence of fts index
@param no_redo			if true, turn-off redo logging
@param corrupted		table corrupted status
@param unlock_index		if true then unlock indexes before action */
static
void
row_truncate_rollback(
	dict_table_t*	table,
	trx_t*		trx,
	table_id_t	new_id,
	bool		has_internal_doc_id,
	bool		no_redo,
	bool		corrupted,
	bool		unlock_index)
{
	if (unlock_index) {
		dict_table_x_unlock_indexes(table);
	}

	trx->error_state = DB_SUCCESS;

	trx_rollback_to_savepoint(trx, NULL);

	trx->error_state = DB_SUCCESS;

	if (corrupted && !dict_table_is_temporary(table)) {

		/* Cleanup action to ensure we don't left over stale entries
		if we are marking table as corrupted. This will ensure
		it can be recovered using drop/create sequence. */
		dict_table_x_lock_indexes(table);

		DropIndex       dropIndex(table, no_redo);

		SysIndexIterator().for_each(dropIndex);

		dict_table_x_unlock_indexes(table);

		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			dict_set_corrupted(index, trx, "TRUNCATE TABLE");
		}

		if (has_internal_doc_id) {

			ut_ad(!trx_is_started(trx));

			table_id_t      id = table->id;

			table->id = new_id;

			fts_drop_tables(trx, table);

			table->id = id;

			ut_ad(trx_is_started(trx));

			trx_commit_for_mysql(trx);
		}

	} else if (corrupted && dict_table_is_temporary(table)) {

		dict_table_x_lock_indexes(table);

		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			dict_drop_index_tree_in_mem(index, index->page);

			index->page = FIL_NULL;
		}

		dict_table_x_unlock_indexes(table);
	}

	table->corrupted = corrupted;
}

/**
Finish the TRUNCATE operations for both commit and rollback.

@param table		table being truncated
@param trx		transaction covering the truncate
@param fsp_flags	tablespace flags
@param logger		table to truncate information logger
@param err		status of truncate operation

@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_complete(
	dict_table_t*		table,
	trx_t*			trx,
	ulint			fsp_flags,
	TruncateLogger*		&logger,
	dberr_t			err)
{
	bool	is_file_per_table = dict_table_is_file_per_table(table);

	if (table->memcached_sync_count == DICT_TABLE_IN_DDL) {
		/* We need to set the memcached sync back to 0, unblock
		memcached operations. */
		table->memcached_sync_count = 0;
	}

	row_mysql_unlock_data_dictionary(trx);

	DEBUG_SYNC_C("ib_trunc_table_trunc_completing");

	if (!dict_table_is_temporary(table)) {

		DBUG_EXECUTE_IF("ib_trunc_crash_before_log_removal",
				log_buffer_flush_to_disk();
				os_thread_sleep(500000);
				DBUG_SUICIDE(););

		/* Note: We don't log-checkpoint instead we have written
		a special REDO log record MLOG_TRUNCATE that is used to
		avoid applying REDO records before truncate for crash
		that happens post successful truncate completion. */

		if (logger != NULL) {
			logger->done();
			UT_DELETE(logger);
			logger = NULL;
		}
	}

	/* If non-temp file-per-table tablespace... */
	if (is_file_per_table
	    && !dict_table_is_temporary(table)
	    && fsp_flags != ULINT_UNDEFINED) {

		/* This function will reset back the stop_new_ops
		and is_being_truncated so that fil-ops can re-start. */
		dberr_t err2 = truncate_t::truncate(
			table->space,
			table->data_dir_path,
			table->name.m_name, fsp_flags, false);

		if (err2 != DB_SUCCESS) {
			return(err2);
		}
	}

	if (err == DB_SUCCESS) {
		dict_stats_update(table, DICT_STATS_EMPTY_TABLE);
	}

	trx->op_info = "";

	/* For temporary tables or if there was an error, we need to reset
	the dict operation flags. */
	trx->ddl = false;
	trx->dict_operation = TRX_DICT_OP_NONE;

	ut_ad(!trx_is_started(trx));

	srv_wake_master_thread();

	DBUG_EXECUTE_IF("ib_trunc_crash_after_truncate_done",
			DBUG_SUICIDE(););

	return(err);
}

/**
Handle FTS truncate issues.
@param table		table being truncated
@param new_id		new id for the table
@param trx		transaction covering the truncate
@return DB_SUCCESS or error code. */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_fts(
	dict_table_t*	table,
	table_id_t	new_id,
	trx_t*		trx)
{
	dict_table_t	fts_table;

	fts_table.id = new_id;
	fts_table.name = table->name;
	fts_table.flags2 = table->flags2;
	fts_table.flags = table->flags;
	fts_table.tablespace = table->tablespace;
	fts_table.space = table->space;

	/* table->data_dir_path is used for FTS AUX table
	creation. */
	if (DICT_TF_HAS_DATA_DIR(table->flags)
	    && table->data_dir_path == NULL) {
		dict_get_and_save_data_dir_path(table, true);
		ut_ad(table->data_dir_path != NULL);
	}

	/* table->tablespace() may not be always populated or
	if table->tablespace() uses "innodb_general" name,
	fetch the real name. */
	if (DICT_TF_HAS_SHARED_SPACE(table->flags)
	    && (table->tablespace() == NULL
		|| dict_table_has_temp_general_tablespace_name(
			table->tablespace()))) {
		dict_get_and_save_space_name(table, true);
		ut_ad(table->tablespace() != NULL);
		ut_ad(!dict_table_has_temp_general_tablespace_name(
			table->tablespace()));
	}

	fts_table.tablespace = table->tablespace();
	fts_table.data_dir_path = table->data_dir_path;

	dberr_t		err;

	err = fts_create_common_tables(
		trx, &fts_table, table->name.m_name, TRUE);

	for (ulint i = 0;
	     i < ib_vector_size(table->fts->indexes) && err == DB_SUCCESS;
	     i++) {

		dict_index_t*	fts_index;

		fts_index = static_cast<dict_index_t*>(
			ib_vector_getp(table->fts->indexes, i));

		err = fts_create_index_tables_low(
			trx, fts_index, table->name.m_name, new_id);
	}

	DBUG_EXECUTE_IF("ib_err_trunc_during_fts_trunc",
			err = DB_ERROR;);

	if (err != DB_SUCCESS) {

		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);
		trx->error_state = DB_SUCCESS;

		ib::error() << "Unable to truncate FTS index for table "
			<< table->name;
	} else {

		ut_ad(trx_is_started(trx));
	}

	return(err);
}

/**
Update system table to reflect new table id.
@param old_table_id		old table id
@param new_table_id		new table id
@param reserve_dict_mutex	if TRUE, acquire/release
				dict_sys->mutex around call to pars_sql.
@param trx			transaction
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_update_table_id(
	table_id_t	old_table_id,
	table_id_t	new_table_id,
	ibool		reserve_dict_mutex,
	trx_t*		trx)
{
	pars_info_t*	info	= NULL;
	dberr_t		err	= DB_SUCCESS;

	/* Scan the SYS_XXXX table and update to reflect new table-id. */
	info = pars_info_create();
	pars_info_add_ull_literal(info, "old_id", old_table_id);
	pars_info_add_ull_literal(info, "new_id", new_table_id);

	err = que_eval_sql(
		info,
		"PROCEDURE RENUMBER_TABLE_ID_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_TABLES"
		" SET ID = :new_id\n"
		" WHERE ID = :old_id;\n"
		"UPDATE SYS_COLUMNS SET TABLE_ID = :new_id\n"
		" WHERE TABLE_ID = :old_id;\n"
		"UPDATE SYS_INDEXES"
		" SET TABLE_ID = :new_id\n"
		" WHERE TABLE_ID = :old_id;\n"
		"UPDATE SYS_VIRTUAL"
		" SET TABLE_ID = :new_id\n"
		" WHERE TABLE_ID = :old_id;\n"
		"END;\n", reserve_dict_mutex, trx);

	return(err);
}

/**
Get the table id to truncate.
@param truncate_t		old/new table id of table to truncate
@return table_id_t		table_id to use in SYS_XXXX table update. */
static MY_ATTRIBUTE((warn_unused_result))
table_id_t
row_truncate_get_trunc_table_id(
	const truncate_t&	truncate)
{
	TableLocator tableLocator(truncate.old_table_id());

	SysIndexIterator().for_each(tableLocator);

	return(tableLocator.is_table_found() ?
		truncate.old_table_id(): truncate.new_table_id());
}

/**
Update system table to reflect new table id and root page number.
@param truncate_t		old/new table id of table to truncate
				and updated root_page_no of indexes.
@param new_table_id		new table id
@param reserve_dict_mutex	if TRUE, acquire/release
				dict_sys->mutex around call to pars_sql.
@param mark_index_corrupted	if true, then mark index corrupted.
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_update_sys_tables_during_fix_up(
	const truncate_t&	truncate,
	table_id_t		new_table_id,
	ibool			reserve_dict_mutex,
	bool			mark_index_corrupted)
{
	trx_t*		trx = trx_allocate_for_background();

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	table_id_t	table_id = row_truncate_get_trunc_table_id(truncate);

	/* Step-1: Update the root-page-no */

	dberr_t	err;

	err = truncate.update_root_page_no(
		trx, table_id, reserve_dict_mutex, mark_index_corrupted);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Step-2: Update table-id. */

	err = row_truncate_update_table_id(
		table_id, new_table_id, reserve_dict_mutex, trx);

	if (err == DB_SUCCESS) {
		dict_mutex_enter_for_mysql();

		/* Remove the table with old table_id from cache. */
		dict_table_t*	old_table = dict_table_open_on_id(
			table_id, true, DICT_TABLE_OP_NORMAL);

		if (old_table != NULL) {
			dict_table_close(old_table, true, false);
			dict_table_remove_from_cache(old_table);
		}

		/* Open table with new table_id and set table as
		corrupted if it has FTS index. */

		dict_table_t*	table = dict_table_open_on_id(
			new_table_id, true, DICT_TABLE_OP_NORMAL);
		ut_ad(table->id == new_table_id);

		bool	has_internal_doc_id =
			dict_table_has_fts_index(table)
			|| DICT_TF2_FLAG_IS_SET(
				table, DICT_TF2_FTS_HAS_DOC_ID);

		if (has_internal_doc_id) {
			trx->dict_operation_lock_mode = RW_X_LATCH;
			fts_check_corrupt(table, trx);
			trx->dict_operation_lock_mode = 0;
		}

		dict_table_close(table, true, false);
		dict_mutex_exit_for_mysql();
	}

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	return(err);
}

/**
Truncate also results in assignment of new table id, update the system
SYSTEM TABLES with the new id.
@param table,			table being truncated
@param new_id,			new table id
@param has_internal_doc_id,	has doc col (fts)
@param no_redo			if true, turn-off redo logging
@param trx			transaction handle
@return	error code or DB_SUCCESS */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_update_system_tables(
	dict_table_t*	table,
	table_id_t	new_id,
	bool		has_internal_doc_id,
	bool		no_redo,
	trx_t*		trx)
{
	dberr_t		err	= DB_SUCCESS;

	ut_a(!dict_table_is_temporary(table));

	err = row_truncate_update_table_id(table->id, new_id, FALSE, trx);

	DBUG_EXECUTE_IF("ib_err_trunc_during_sys_table_update",
			err = DB_ERROR;);

	if (err != DB_SUCCESS) {

		row_truncate_rollback(
			table, trx, new_id, has_internal_doc_id,
			no_redo, true, false);

		ib::error() << "Unable to assign a new identifier to table "
			<< table->name << " after truncating it. Marked the"
			" table as corrupted. In-memory representation is now"
			" different from the on-disk representation.";
		err = DB_ERROR;
	} else {
		/* Drop the old FTS index */
		if (has_internal_doc_id) {

			ut_ad(trx_is_started(trx));

			fts_drop_tables(trx, table);

			DBUG_EXECUTE_IF("ib_truncate_crash_while_fts_cleanup",
					DBUG_SUICIDE(););

			ut_ad(trx_is_started(trx));
		}

		DBUG_EXECUTE_IF("ib_trunc_crash_after_fts_drop",
				log_buffer_flush_to_disk();
				os_thread_sleep(2000000);
				DBUG_SUICIDE(););

		dict_table_change_id_in_cache(table, new_id);

		/* Reset the Doc ID in cache to 0 */
		if (has_internal_doc_id && table->fts->cache != NULL) {
			table->fts->fts_status |= TABLE_DICT_LOCKED;
			fts_update_next_doc_id(trx, table, NULL, 0);
			fts_cache_clear(table->fts->cache);
			fts_cache_init(table->fts->cache);
			table->fts->fts_status &= ~TABLE_DICT_LOCKED;
		}
	}

	return(err);
}

/**
Prepare for the truncate process. On success all of the table's indexes will
be locked in X mode.
@param table		table to truncate
@param flags		tablespace flags
@return	error code or DB_SUCCESS */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_prepare(dict_table_t* table, ulint* flags)
{
	ut_ad(!dict_table_is_temporary(table));
	ut_ad(dict_table_is_file_per_table(table));

	*flags = fil_space_get_flags(table->space);

	ut_ad(!dict_table_is_temporary(table));

	dict_get_and_save_data_dir_path(table, true);

	dict_get_and_save_space_name(table, true);

	if (*flags != ULINT_UNDEFINED) {

		dberr_t	err = fil_prepare_for_truncate(table->space);

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	return(DB_SUCCESS);
}

/**
Do foreign key checks before starting TRUNCATE.
@param table		table being truncated
@param trx		transaction covering the truncate
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_foreign_key_checks(
	const dict_table_t*	table,
	const trx_t*		trx)
{
	/* Check if the table is referenced by foreign key constraints from
	some other table (not the table itself) */

	dict_foreign_set::iterator	it
		= std::find_if(table->referenced_set.begin(),
			       table->referenced_set.end(),
			       dict_foreign_different_tables());

	if (!srv_read_only_mode
	    && it != table->referenced_set.end()
	    && trx->check_foreigns) {

		dict_foreign_t*	foreign = *it;

		FILE*	ef = dict_foreign_err_file;

		/* We only allow truncating a referenced table if
		FOREIGN_KEY_CHECKS is set to 0 */

		mutex_enter(&dict_foreign_err_mutex);

		rewind(ef);

		ut_print_timestamp(ef);

		fputs("  Cannot truncate table ", ef);
		ut_print_name(ef, trx, table->name.m_name);
		fputs(" by DROP+CREATE\n"
		      "InnoDB: because it is referenced by ", ef);
		ut_print_name(ef, trx, foreign->foreign_table_name);
		putc('\n', ef);

		mutex_exit(&dict_foreign_err_mutex);

		return(DB_ERROR);
	}

	/* TODO: could we replace the counter n_foreign_key_checks_running
	with lock checks on the table? Acquire here an exclusive lock on the
	table, and rewrite lock0lock.cc and the lock wait in srv0srv.cc so that
	they can cope with the table having been truncated here? Foreign key
	checks take an IS or IX lock on the table. */

	if (table->n_foreign_key_checks_running > 0) {
		ib::warn() << "Cannot truncate table " << table->name
			<< " because there is a foreign key check running on"
			" it.";

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/**
Do some sanity checks before starting the actual TRUNCATE.
@param table		table being truncated
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
row_truncate_sanity_checks(
	const dict_table_t* table)
{
	if (dict_table_is_discarded(table)) {

		return(DB_TABLESPACE_DELETED);

	} else if (table->ibd_file_missing) {

		return(DB_TABLESPACE_NOT_FOUND);

	} else if (dict_table_is_corrupted(table)) {

		return(DB_TABLE_CORRUPT);
	}

	return(DB_SUCCESS);
}

/**
Truncates a table for MySQL.
@param table		table being truncated
@param trx		transaction covering the truncate
@return	error code or DB_SUCCESS */
dberr_t
row_truncate_table_for_mysql(
	dict_table_t* table,
	trx_t* trx)
{
	bool	is_file_per_table = dict_table_is_file_per_table(table);
	dberr_t		err;
#ifdef UNIV_DEBUG
	ulint		old_space = table->space;
#endif /* UNIV_DEBUG */
	TruncateLogger*	logger = NULL;

	/* Understanding the truncate flow.

	Step-1: Perform intiial sanity check to ensure table can be truncated.
	This would include check for tablespace discard status, ibd file
	missing, etc ....

	Step-2: Start transaction (only for non-temp table as temp-table don't
	modify any data on disk doesn't need transaction object).

	Step-3: Validate ownership of needed locks (Exclusive lock).
	Ownership will also ensure there is no active SQL queries, INSERT,
	SELECT, .....

	Step-4: Stop all the background process associated with table.

	Step-5: There are few foreign key related constraint under which
	we can't truncate table (due to referential integrity unless it is
	turned off). Ensure this condition is satisfied.

	Step-6: Truncate operation can be rolled back in case of error
	till some point. Associate rollback segment to record undo log.

	Step-7: Generate new table-id.
	Why we need new table-id ?
	Purge and rollback case: we assign a new table id for the table.
	Since purge and rollback look for the table based on the table id,
	they see the table as 'dropped' and discard their operations.

	Step-8: Log information about tablespace which includes
	table and index information. If there is a crash in the next step
	then during recovery we will attempt to fixup the operation.

	Step-9: Drop all indexes (this include freeing of the pages
	associated with them).

	Step-10: Re-create new indexes.

	Step-11: Update new table-id to in-memory cache (dictionary),
	on-disk (INNODB_SYS_TABLES). INNODB_SYS_INDEXES also needs to
	be updated to reflect updated root-page-no of new index created
	and updated table-id.

	Step-12: Cleanup Stage. Reset auto-inc value to 1.
	Release all the locks.
	Commit the transaction. Update trx operation state.

	Notes:
	- On error, log checkpoint is done followed writing of magic number to
	truncate log file. If servers crashes after truncate, fix-up action
	will not be applied.

	- log checkpoint is done before starting truncate table to ensure
	that previous REDO log entries are not applied if current truncate
	crashes. Consider following use-case:
	 - create table .... insert/load table .... truncate table (crash)
	 - on restart table is restored .... truncate table (crash)
	 - on restart (assuming default log checkpoint is not done) will have
	   2 REDO log entries for same table. (Note 2 REDO log entries
	   for different table is not an issue).
	For system-tablespace we can't truncate the tablespace so we need
	to initiate a local cleanup that involves dropping of indexes and
	re-creating them. If we apply stale entry we might end-up issuing
	drop on wrong indexes.

	- Insert buffer: TRUNCATE TABLE is analogous to DROP TABLE,
	so we do not have to remove insert buffer records, as the
	insert buffer works at a low level. If a freed page is later
	reallocated, the allocator will remove the ibuf entries for
	it. When we prepare to truncate *.ibd files, we remove all entries
	for the table in the insert buffer tree. This is not strictly
	necessary, but we can free up some space in the system tablespace.

	- Linear readahead and random readahead: we use the same
	method as in 3) to discard ongoing operations. (This is only
	relevant for TRUNCATE TABLE by TRUNCATE TABLESPACE.)
	Ensure that the table will be dropped by trx_rollback_active() in
	case of a crash.
	*/

	/*-----------------------------------------------------------------*/
	/* Step-1: Perform intiial sanity check to ensure table can be
	truncated. This would include check for tablespace discard status,
	ibd file missing, etc .... */
	err = row_truncate_sanity_checks(table);
	if (err != DB_SUCCESS) {
		return(err);

	}

	/* Step-2: Start transaction (only for non-temp table as temp-table
	don't modify any data on disk doesn't need transaction object). */
	if (!dict_table_is_temporary(table)) {
		/* Avoid transaction overhead for temporary table DDL. */
		trx_start_for_ddl(trx, TRX_DICT_OP_TABLE);
	}

	/* Step-3: Validate ownership of needed locks (Exclusive lock).
	Ownership will also ensure there is no active SQL queries, INSERT,
	SELECT, .....*/
	trx->op_info = "truncating table";
	ut_a(trx->dict_operation_lock_mode == 0);
	row_mysql_lock_data_dictionary(trx);
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

	/* Step-4: Stop all the background process associated with table. */
	dict_stats_wait_bg_to_stop_using_table(table, trx);

	/* Step-5: There are few foreign key related constraint under which
	we can't truncate table (due to referential integrity unless it is
	turned off). Ensure this condition is satisfied. */
	ulint	fsp_flags = ULINT_UNDEFINED;
	err = row_truncate_foreign_key_checks(table, trx);
	if (err != DB_SUCCESS) {
		trx_rollback_to_savepoint(trx, NULL);
		return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
	}

	/* Check if memcached DML is running on this table. if is, we don't
	allow truncate this table. */
	if (table->memcached_sync_count != 0) {
		ib::error() << "Cannot truncate table "
			<< table->name
			<< " by DROP+CREATE because there are memcached"
			" operations running on it.";
		err = DB_ERROR;
		trx_rollback_to_savepoint(trx, NULL);
		return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
	} else {
                /* We need to set this counter to -1 for blocking
                memcached operations. */
		table->memcached_sync_count = DICT_TABLE_IN_DDL;
        }

	/* Remove all locks except the table-level X lock. */
	lock_remove_all_on_table(table, FALSE);
	trx->table_id = table->id;
	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	/* Step-6: Truncate operation can be rolled back in case of error
	till some point. Associate rollback segment to record undo log. */
	if (!dict_table_is_temporary(table)) {

		/* Temporary tables don't need undo logging for autocommit stmt.
		On crash (i.e. mysql restart) temporary tables are anyway not
		accessible. */
		mutex_enter(&trx->undo_mutex);

		err = trx_undo_assign_undo(
			trx, &trx->rsegs.m_redo, TRX_UNDO_UPDATE);

		mutex_exit(&trx->undo_mutex);

		DBUG_EXECUTE_IF("ib_err_trunc_assigning_undo_log",
				err = DB_ERROR;);
		if (err != DB_SUCCESS) {
			trx_rollback_to_savepoint(trx, NULL);
			return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
		}
	}

	/* Step-7: Generate new table-id.
	Why we need new table-id ?
	Purge and rollback: we assign a new table id for the
	table. Since purge and rollback look for the table based on
	the table id, they see the table as 'dropped' and discard
	their operations. */
	table_id_t	new_id;
	dict_hdr_get_new_id(&new_id, NULL, NULL, table, false);

	/* Check if table involves FTS index. */
	bool	has_internal_doc_id =
		dict_table_has_fts_index(table)
		|| DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID);

	bool	no_redo = is_file_per_table && !has_internal_doc_id;

	/* Step-8: Log information about tablespace which includes
	table and index information. If there is a crash in the next step
	then during recovery we will attempt to fixup the operation. */

	/* Lock all index trees for this table, as we will truncate
	the table/index and possibly change their metadata. All
	DML/DDL are blocked by table level X lock, with a few exceptions
	such as queries into information schema about the table,
	MySQL could try to access index stats for this kind of query,
	we need to use index locks to sync up */
	dict_table_x_lock_indexes(table);

	if (!dict_table_is_temporary(table)) {

		if (is_file_per_table) {

			err = row_truncate_prepare(table, &fsp_flags);

			DBUG_EXECUTE_IF("ib_err_trunc_preparing_for_truncate",
					err = DB_ERROR;);

			if (err != DB_SUCCESS) {
				row_truncate_rollback(
					table, trx, new_id,
					has_internal_doc_id,
					no_redo, false, true);
				return(row_truncate_complete(
					table, trx, fsp_flags, logger, err));
			}
		} else {
			fsp_flags = fil_space_get_flags(table->space);

			DBUG_EXECUTE_IF("ib_err_trunc_preparing_for_truncate",
					fsp_flags = ULINT_UNDEFINED;);

			if (fsp_flags == ULINT_UNDEFINED) {
				row_truncate_rollback(
					table, trx, new_id,
					has_internal_doc_id,
					no_redo, false, true);
				return(row_truncate_complete(
						table, trx, fsp_flags,
						logger, DB_ERROR));
			}
		}

		logger = UT_NEW_NOKEY(TruncateLogger(
				table, fsp_flags, new_id));

		err = logger->init();
		if (err != DB_SUCCESS) {
			row_truncate_rollback(
				table, trx, new_id, has_internal_doc_id,
				no_redo, false, true);
			return(row_truncate_complete(
				table, trx, fsp_flags, logger, DB_ERROR));

		}

		err = SysIndexIterator().for_each(*logger);
		if (err != DB_SUCCESS) {
			row_truncate_rollback(
				table, trx, new_id, has_internal_doc_id,
				no_redo, false, true);
			return(row_truncate_complete(
				table, trx, fsp_flags, logger, DB_ERROR));

		}

		ut_ad(logger->debug());

		err = logger->log();

		if (err != DB_SUCCESS) {
			row_truncate_rollback(
				table, trx, new_id, has_internal_doc_id,
				no_redo, false, true);
			return(row_truncate_complete(
				table, trx, fsp_flags, logger, DB_ERROR));
		}
	}

	DBUG_EXECUTE_IF("ib_trunc_crash_after_redo_log_write_complete",
			log_buffer_flush_to_disk();
			os_thread_sleep(3000000);
			DBUG_SUICIDE(););

	/* Step-9: Drop all indexes (free index pages associated with these
	indexes) */
	if (!dict_table_is_temporary(table)) {

		DropIndex	dropIndex(table, no_redo);

		err = SysIndexIterator().for_each(dropIndex);

		if (err != DB_SUCCESS) {

			row_truncate_rollback(
				table, trx, new_id, has_internal_doc_id,
				no_redo, true, true);

			return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
		}

	} else {
		/* For temporary tables we don't have entries in SYSTEM TABLES*/
		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			err = dict_truncate_index_tree_in_mem(index);

			if (err != DB_SUCCESS) {
				row_truncate_rollback(
					table, trx, new_id, has_internal_doc_id,
					no_redo, true, true);
				return(row_truncate_complete(
					table, trx, fsp_flags, logger, err));
			}

			DBUG_EXECUTE_IF(
				"ib_trunc_crash_during_drop_index_temp_table",
				log_buffer_flush_to_disk();
				os_thread_sleep(2000000);
				DBUG_SUICIDE(););
		}
	}

	if (is_file_per_table
	    && !dict_table_is_temporary(table)
	    && fsp_flags != ULINT_UNDEFINED) {

		/* A single-table tablespace has initially
		FIL_IBD_FILE_INITIAL_SIZE number of pages allocated and an
		extra page is allocated for each of the indexes present. But in
		the case of clust index 2 pages are allocated and as one is
		covered in the calculation as part of table->indexes.count we
		take care of the other page by adding 1. */
		ulint	space_size = table->indexes.count +
				FIL_IBD_FILE_INITIAL_SIZE + 1;

		if (has_internal_doc_id) {
			/* Since aux tables are created for fts indexes and
			they use seperate tablespaces. */
			space_size -= ib_vector_size(table->fts->indexes);
		}

		fil_reinit_space_header_for_table(table, space_size, trx);
	}

	DBUG_EXECUTE_IF("ib_trunc_crash_with_intermediate_log_checkpoint",
			log_buffer_flush_to_disk();
			os_thread_sleep(2000000);
			log_checkpoint(TRUE, TRUE);
			os_thread_sleep(1000000);
			DBUG_SUICIDE(););

	DBUG_EXECUTE_IF("ib_trunc_crash_drop_reinit_done_create_to_start",
			log_buffer_flush_to_disk();
			os_thread_sleep(2000000);
			DBUG_SUICIDE(););

	/* Step-10: Re-create new indexes. */
	if (!dict_table_is_temporary(table)) {

		CreateIndex	createIndex(table, no_redo);

		err = SysIndexIterator().for_each(createIndex);

		if (err != DB_SUCCESS) {

			row_truncate_rollback(
				table, trx, new_id, has_internal_doc_id,
				no_redo, true, true);

			return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
		}
	}

	/* Done with index truncation, release index tree locks,
	subsequent work relates to table level metadata change */
	dict_table_x_unlock_indexes(table);

	if (has_internal_doc_id) {

		err = row_truncate_fts(table, new_id, trx);

		if (err != DB_SUCCESS) {

			row_truncate_rollback(
				table, trx, new_id, has_internal_doc_id,
				no_redo, true, false);

			return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
		}
	}

	/* Step-11: Update new table-id to in-memory cache (dictionary),
	on-disk (INNODB_SYS_TABLES). INNODB_SYS_INDEXES also needs to
	be updated to reflect updated root-page-no of new index created
	and updated table-id. */
	if (dict_table_is_temporary(table)) {

		dict_table_change_id_in_cache(table, new_id);
		err = DB_SUCCESS;

	} else {

		/* If this fails then we are in an inconsistent state and
		the results are undefined. */
		ut_ad(old_space == table->space);

		err = row_truncate_update_system_tables(
			table, new_id, has_internal_doc_id, no_redo, trx);

		if (err != DB_SUCCESS) {
			return(row_truncate_complete(
				table, trx, fsp_flags, logger, err));
		}
	}

	DBUG_EXECUTE_IF("ib_trunc_crash_on_updating_dict_sys_info",
			log_buffer_flush_to_disk();
			os_thread_sleep(2000000);
			DBUG_SUICIDE(););

	/* Step-12: Cleanup Stage. Reset auto-inc value to 1.
	Release all the locks.
	Commit the transaction. Update trx operation state. */
	dict_table_autoinc_lock(table);
	dict_table_autoinc_initialize(table, 1);
	dict_table_autoinc_unlock(table);

	if (trx_is_started(trx)) {

		trx_commit_for_mysql(trx);
	}

	return(row_truncate_complete(table, trx, fsp_flags, logger, err));
}

/**
Fix the table truncate by applying information parsed from TRUNCATE log.
Fix-up includes re-creating table (drop and re-create indexes)
@return	error code or DB_SUCCESS */
dberr_t
truncate_t::fixup_tables_in_system_tablespace()
{
	dberr_t	err = DB_SUCCESS;

	/* Using the info cached during REDO log scan phase fix the
	table truncate. */

	for (tables_t::iterator it = s_tables.begin();
	     it != s_tables.end();) {

		if ((*it)->m_space_id == TRX_SYS_SPACE) {
			/* Step-1: Drop and re-create indexes. */
			ib::info() << "Completing truncate for table with "
				"id (" << (*it)->m_old_table_id << ") "
				"residing in the system tablespace.";

			err = fil_recreate_table(
				(*it)->m_space_id,
				(*it)->m_format_flags,
				(*it)->m_tablespace_flags,
				(*it)->m_tablename,
				**it);

			/* Step-2: Update the SYS_XXXX tables to reflect
			this new table_id and root_page_no. */
			table_id_t	new_id;

			dict_hdr_get_new_id(&new_id, NULL, NULL, NULL, true);

			err = row_truncate_update_sys_tables_during_fix_up(
				**it, new_id, TRUE,
				(err == DB_SUCCESS) ? false : true);

			if (err != DB_SUCCESS) {
				break;
			}

			os_file_delete(
				innodb_log_file_key, (*it)->m_log_file_name);
			UT_DELETE(*it);
			it = s_tables.erase(it);
		} else {
			++it;
		}
	}

	/* Also clear the map used to track tablespace truncated. */
	s_truncated_tables.clear();

	return(err);
}

/**
Fix the table truncate by applying information parsed from TRUNCATE log.
Fix-up includes re-creating tablespace.
@return	error code or DB_SUCCESS */
dberr_t
truncate_t::fixup_tables_in_non_system_tablespace()
{
	dberr_t	err = DB_SUCCESS;

	/* Using the info cached during REDO log scan phase fix the
	table truncate. */
	tables_t::iterator end = s_tables.end();

	for (tables_t::iterator it = s_tables.begin(); it != end; ++it) {

		/* All tables in the system tablespace have already been
		done and erased from this list. */
		ut_a((*it)->m_space_id != TRX_SYS_SPACE);

		/* Step-1: Drop tablespace (only for single-tablespace),
		drop indexes and re-create indexes. */

		if (fsp_is_file_per_table((*it)->m_space_id,
					  (*it)->m_tablespace_flags)) {
			/* The table is file_per_table */

			ib::info() << "Completing truncate for table with "
				"id (" << (*it)->m_old_table_id << ") "
				"residing in file-per-table tablespace with "
				"id (" << (*it)->m_space_id << ")";

			if (!fil_space_get((*it)->m_space_id)) {

				/* Create the database directory for name,
				if it does not exist yet */
				fil_create_directory_for_tablename(
					(*it)->m_tablename);

				err = fil_ibd_create(
					(*it)->m_space_id,
					(*it)->m_tablename,
					(*it)->m_dir_path,
					(*it)->m_tablespace_flags,
					FIL_IBD_FILE_INITIAL_SIZE);
				if (err != DB_SUCCESS) {
					/* If checkpoint is not yet done
					and table is dropped and then we might
					still have REDO entries for this table
					which are INVALID. Ignore them. */
					ib::warn() << "Failed to create"
						" tablespace for "
						<< (*it)->m_space_id
						<< " space-id";
					err = DB_ERROR;
					break;
				}
			}

			ut_ad(fil_space_get((*it)->m_space_id));

			err = fil_recreate_tablespace(
				(*it)->m_space_id,
				(*it)->m_format_flags,
				(*it)->m_tablespace_flags,
				(*it)->m_tablename,
				**it, log_get_lsn());

		} else {
			/* Table is in a shared tablespace */

			ib::info() << "Completing truncate for table with "
				"id (" << (*it)->m_old_table_id << ") "
				"residing in shared tablespace with "
				"id (" << (*it)->m_space_id << ")";

			/* Temp-tables in temp-tablespace are never restored.*/
			ut_ad((*it)->m_space_id != srv_tmp_space.space_id());

			err = fil_recreate_table(
				(*it)->m_space_id,
				(*it)->m_format_flags,
				(*it)->m_tablespace_flags,
				(*it)->m_tablename,
				**it);
		}

		/* Step-2: Update the SYS_XXXX tables to reflect new
		table-id and root_page_no. */
		table_id_t	new_id;

		dict_hdr_get_new_id(&new_id, NULL, NULL, NULL, true);

		err = row_truncate_update_sys_tables_during_fix_up(
			**it, new_id, TRUE, (err == DB_SUCCESS) ? false : true);

		if (err != DB_SUCCESS) {
			break;
		}
	}

	if (err == DB_SUCCESS && s_tables.size() > 0) {

		log_make_checkpoint_at(LSN_MAX, TRUE);
	}

	for (ulint i = 0; i < s_tables.size(); ++i) {
		os_file_delete(
			innodb_log_file_key, s_tables[i]->m_log_file_name);
		UT_DELETE(s_tables[i]);
	}

	s_tables.clear();

	return(err);
}

/**
Constructor

@param old_table_id	old table id assigned to table before truncate
@param new_table_id	new table id that will be assigned to table
			after truncate
@param dir_path		directory path */

truncate_t::truncate_t(
	table_id_t	old_table_id,
	table_id_t	new_table_id,
	const char*	dir_path)
	:
	m_space_id(),
	m_old_table_id(old_table_id),
	m_new_table_id(new_table_id),
	m_dir_path(),
	m_tablename(),
	m_tablespace_flags(),
	m_format_flags(),
	m_indexes(),
	m_log_lsn(),
	m_log_file_name()
{
	if (dir_path != NULL) {
		m_dir_path = mem_strdup(dir_path);
	}
}

/**
Consturctor

@param log_file_name	parse the log file during recovery to populate
			information related to table to truncate */
truncate_t::truncate_t(
	const char*	log_file_name)
	:
	m_space_id(),
	m_old_table_id(),
	m_new_table_id(),
	m_dir_path(),
	m_tablename(),
	m_tablespace_flags(),
	m_format_flags(),
	m_indexes(),
	m_log_lsn(),
	m_log_file_name()
{
	m_log_file_name = mem_strdup(log_file_name);
	if (m_log_file_name == NULL) {
		ib::fatal() << "Failed creating truncate_t; out of memory";
	}
}

/** Constructor */

truncate_t::index_t::index_t()
	:
	m_id(),
	m_type(),
	m_root_page_no(FIL_NULL),
	m_new_root_page_no(FIL_NULL),
	m_n_fields(),
	m_trx_id_pos(ULINT_UNDEFINED),
	m_fields()
{
	/* Do nothing */
}

/** Destructor */

truncate_t::~truncate_t()
{
	if (m_dir_path != NULL) {
		ut_free(m_dir_path);
		m_dir_path = NULL;
	}

	if (m_tablename != NULL) {
		ut_free(m_tablename);
		m_tablename = NULL;
	}

	if (m_log_file_name != NULL) {
		ut_free(m_log_file_name);
		m_log_file_name = NULL;
	}

	m_indexes.clear();
}

/**
@return number of indexes parsed from the log record */

size_t
truncate_t::indexes() const
{
	return(m_indexes.size());
}

/**
Update root page number in SYS_XXXX tables.

@param trx			transaction object
@param table_id			table id for which information needs to
				be updated.
@param reserve_dict_mutex	if TRUE, acquire/release
				dict_sys->mutex around call to pars_sql.
@param mark_index_corrupted	if true, then mark index corrupted.
@return DB_SUCCESS or error code */

dberr_t
truncate_t::update_root_page_no(
	trx_t*		trx,
	table_id_t	table_id,
	ibool		reserve_dict_mutex,
	bool		mark_index_corrupted) const
{
	indexes_t::const_iterator end = m_indexes.end();

	dberr_t	err = DB_SUCCESS;

	for (indexes_t::const_iterator it = m_indexes.begin();
	     it != end;
	     ++it) {

		pars_info_t*	info = pars_info_create();

		pars_info_add_int4_literal(
			info, "page_no", it->m_new_root_page_no);

		pars_info_add_ull_literal(info, "table_id", table_id);

		pars_info_add_ull_literal(
			info, "index_id",
			(mark_index_corrupted ? -1 : it->m_id));

		err = que_eval_sql(
			info,
			"PROCEDURE RENUMBER_IDX_PAGE_NO_PROC () IS\n"
			"BEGIN\n"
			"UPDATE SYS_INDEXES"
			" SET PAGE_NO = :page_no\n"
			" WHERE TABLE_ID = :table_id"
			" AND ID = :index_id;\n"
			"END;\n", reserve_dict_mutex, trx);

		if (err != DB_SUCCESS) {
			break;
		}
	}

	return(err);
}

/**
Check whether a tablespace was truncated during recovery
@param space_id	tablespace id to check
@return true if the tablespace was truncated */

bool
truncate_t::is_tablespace_truncated(ulint space_id)
{
	tables_t::iterator end = s_tables.end();

	for (tables_t::iterator it = s_tables.begin(); it != end; ++it) {

		if ((*it)->m_space_id == space_id) {

			return(true);
		}
	}

	return(false);
}

/** Was tablespace truncated (on crash before checkpoint).
If the MLOG_TRUNCATE redo-record is still available then tablespace
was truncated and checkpoint is yet to happen.
@param[in]	space_id	tablespace id to check.
@return true if tablespace is was truncated. */
bool
truncate_t::was_tablespace_truncated(ulint space_id)
{
	return(s_truncated_tables.find(space_id) != s_truncated_tables.end());
}

/** Get the lsn associated with space.
@param[in]	space_id	tablespace id to check.
@return associated lsn. */
lsn_t
truncate_t::get_truncated_tablespace_init_lsn(ulint space_id)
{
	ut_ad(was_tablespace_truncated(space_id));

	return(s_truncated_tables.find(space_id)->second);
}

/**
Parses log record during recovery
@param start_ptr	buffer containing log body to parse
@param end_ptr		buffer end

@return DB_SUCCESS or error code */

dberr_t
truncate_t::parse(
	byte*		start_ptr,
	const byte*	end_ptr)
{
	/* Parse lsn, space-id, format-flags and tablespace-flags. */
	if (end_ptr < start_ptr + (8 + 4 + 4 + 4)) {
		return(DB_FAIL);
	}

	m_log_lsn = mach_read_from_8(start_ptr);
	start_ptr += 8;

	m_space_id = mach_read_from_4(start_ptr);
	start_ptr += 4;

	m_format_flags = mach_read_from_4(start_ptr);
	start_ptr += 4;

	m_tablespace_flags = mach_read_from_4(start_ptr);
	start_ptr += 4;

	/* Parse table-name. */
	if (end_ptr < start_ptr + (2)) {
		return(DB_FAIL);
	}

	ulint n_tablename_len = mach_read_from_2(start_ptr);
	start_ptr += 2;

	if (n_tablename_len > 0) {
		if (end_ptr < start_ptr + n_tablename_len) {
			return(DB_FAIL);
		}
		m_tablename = mem_strdup(reinterpret_cast<char*>(start_ptr));
		ut_ad(m_tablename[n_tablename_len - 1] == 0);
		start_ptr += n_tablename_len;
	}


	/* Parse and read old/new table-id, number of indexes */
	if (end_ptr < start_ptr + (8 + 8 + 2 + 2)) {
		return(DB_FAIL);
	}

	ut_ad(m_indexes.empty());

	m_old_table_id = mach_read_from_8(start_ptr);
	start_ptr += 8;

	m_new_table_id = mach_read_from_8(start_ptr);
	start_ptr += 8;

	ulint n_indexes = mach_read_from_2(start_ptr);
	start_ptr += 2;

	/* Parse the remote directory from TRUNCATE log record */
	{
		ulint	n_tabledirpath_len = mach_read_from_2(start_ptr);
		start_ptr += 2;

		if (end_ptr < start_ptr + n_tabledirpath_len) {
			return(DB_FAIL);
		}

		if (n_tabledirpath_len > 0) {

			m_dir_path = mem_strdup(reinterpret_cast<char*>(start_ptr));
			ut_ad(m_dir_path[n_tabledirpath_len - 1] == 0);
			start_ptr += n_tabledirpath_len;
		}
	}

	/* Parse index ids and types from TRUNCATE log record */
	for (ulint i = 0; i < n_indexes; ++i) {
		index_t	index;

		if (end_ptr < start_ptr + (8 + 4 + 4 + 4)) {
			return(DB_FAIL);
		}

		index.m_id = mach_read_from_8(start_ptr);
		start_ptr += 8;

		index.m_type = mach_read_from_4(start_ptr);
		start_ptr += 4;

		index.m_root_page_no = mach_read_from_4(start_ptr);
		start_ptr += 4;

		index.m_trx_id_pos = mach_read_from_4(start_ptr);
		start_ptr += 4;

		if (!(index.m_type & DICT_FTS)) {
			m_indexes.push_back(index);
		}
	}

	ut_ad(!m_indexes.empty());

	if (fsp_flags_is_compressed(m_tablespace_flags)) {

		/* Parse the number of index fields from TRUNCATE log record */
		for (ulint i = 0; i < m_indexes.size(); ++i) {

			if (end_ptr < start_ptr + (2 + 2)) {
				return(DB_FAIL);
			}

			m_indexes[i].m_n_fields = mach_read_from_2(start_ptr);
			start_ptr += 2;

			ulint	len = mach_read_from_2(start_ptr);
			start_ptr += 2;

			if (end_ptr < start_ptr + len) {
				return(DB_FAIL);
			}

			index_t&	index = m_indexes[i];

			/* Should be NUL terminated. */
			ut_ad((start_ptr)[len - 1] == 0);

			index_t::fields_t::iterator	end;

			end = index.m_fields.end();

			index.m_fields.insert(
				end, start_ptr, &(start_ptr)[len]);

			start_ptr += len;
		}
	}

	return(DB_SUCCESS);
}

/** Parse log record from REDO log file during recovery.
@param[in,out]	start_ptr	buffer containing log body to parse
@param[in]	end_ptr		buffer end
@param[in]	space_id	tablespace identifier
@return parsed upto or NULL. */
byte*
truncate_t::parse_redo_entry(
	byte*		start_ptr,
	const byte*	end_ptr,
	ulint		space_id)
{
	lsn_t	lsn;

	/* Parse space-id, lsn */
	if (end_ptr < (start_ptr + 8)) {
		return(NULL);
	}

	lsn = mach_read_from_8(start_ptr);
	start_ptr += 8;

	/* Tablespace can't exist in both state.
	(scheduled-for-truncate, was-truncated). */
	if (!is_tablespace_truncated(space_id)) {

		truncated_tables_t::iterator	it =
				s_truncated_tables.find(space_id);

		if (it == s_truncated_tables.end()) {
			s_truncated_tables.insert(
				std::pair<ulint, lsn_t>(space_id, lsn));
		} else {
			it->second = lsn;
		}
	}

	return(start_ptr);
}

/**
Set the truncate log values for a compressed table.
@param index	index from which recreate infoormation needs to be extracted
@return DB_SUCCESS or error code */

dberr_t
truncate_t::index_t::set(
	const dict_index_t* index)
{
	/* Get trx-id column position (set only for clustered index) */
	if (dict_index_is_clust(index)) {
		m_trx_id_pos = dict_index_get_sys_col_pos(index, DATA_TRX_ID);
		ut_ad(m_trx_id_pos > 0);
		ut_ad(m_trx_id_pos != ULINT_UNDEFINED);
	} else {
		m_trx_id_pos = 0;
	}

	/* Original logic set this field differently if page is not leaf.
	For truncate case this being first page to get created it is
	always a leaf page and so we don't need that condition here. */
	m_n_fields = dict_index_get_n_fields(index);

	/* See requirements of page_zip_fields_encode for size. */
	ulint	encoded_buf_size = (m_n_fields + 1) * 2;
	byte*	encoded_buf = UT_NEW_ARRAY_NOKEY(byte, encoded_buf_size);

	if (encoded_buf == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	ulint len = page_zip_fields_encode(
		m_n_fields, index, m_trx_id_pos, encoded_buf);
	ut_a(len <= encoded_buf_size);

	/* Append the encoded fields data. */
	m_fields.insert(m_fields.end(), &encoded_buf[0], &encoded_buf[len]);

	/* NUL terminate the encoded data */
	m_fields.push_back(0);

	UT_DELETE_ARRAY(encoded_buf);

	return(DB_SUCCESS);
}

/** Create an index for a table.
@param[in]	table_name		table name, for which to create
the index
@param[in]	space_id		space id where we have to
create the index
@param[in]	page_size		page size of the .ibd file
@param[in]	index_type		type of index to truncate
@param[in]	index_id		id of index to truncate
@param[in]	btr_redo_create_info	control info for ::btr_create()
@param[in,out]	mtr			mini-transaction covering the
create index
@return root page no or FIL_NULL on failure */
ulint
truncate_t::create_index(
	const char*		table_name,
	ulint			space_id,
	const page_size_t&	page_size,
	ulint			index_type,
	index_id_t		index_id,
	const btr_create_t&	btr_redo_create_info,
	mtr_t*			mtr) const
{
	ulint	root_page_no = btr_create(
		index_type, space_id, page_size, index_id,
		NULL, &btr_redo_create_info, mtr);

	if (root_page_no == FIL_NULL) {

		ib::info() << "innodb_force_recovery was set to "
			<< srv_force_recovery << ". Continuing crash recovery"
			" even though we failed to create index " << index_id
			<< " for compressed table '" << table_name << "' with"
			" tablespace " << space_id << " during recovery";
	}

	return(root_page_no);
}

/** Check if index has been modified since TRUNCATE log snapshot
was recorded.
@param space_id		space_id where table/indexes resides.
@param root_page_no	root page of index that needs to be verified.
@return true if modified else false */

bool
truncate_t::is_index_modified_since_logged(
	ulint		space_id,
	ulint		root_page_no) const
{
	mtr_t			mtr;
	bool			found;
	const page_size_t&	page_size = fil_space_get_page_size(space_id,
								    &found);

	ut_ad(found);

	mtr_start(&mtr);

	/* Root page could be in free state if truncate crashed after drop_index
	and page was not allocated for any other object. */
	buf_block_t* block= buf_page_get_gen(
		page_id_t(space_id, root_page_no), page_size, RW_X_LATCH, NULL,
		BUF_GET_POSSIBLY_FREED, __FILE__, __LINE__, &mtr);

	page_t* root = buf_block_get_frame(block);

#ifdef UNIV_DEBUG
	/* If the root page has been freed as part of truncate drop_index action
	and not yet allocated for any object still the pagelsn > snapshot lsn */
	if (block->page.file_page_was_freed) {
		ut_ad(mach_read_from_8(root + FIL_PAGE_LSN) > m_log_lsn);
	}
#endif /* UNIV_DEBUG */

	lsn_t page_lsn = mach_read_from_8(root + FIL_PAGE_LSN);

	mtr_commit(&mtr);

	if (page_lsn > m_log_lsn) {
		return(true);
	}

	return(false);
}

/** Drop indexes for a table.
@param space_id		space_id where table/indexes resides. */

void
truncate_t::drop_indexes(
	ulint		space_id) const
{
	mtr_t           mtr;
	ulint		root_page_no = FIL_NULL;

	indexes_t::const_iterator       end = m_indexes.end();

	for (indexes_t::const_iterator it = m_indexes.begin();
	     it != end;
	     ++it) {

		root_page_no = it->m_root_page_no;

		bool			found;
		const page_size_t&	page_size
			= fil_space_get_page_size(space_id, &found);

		ut_ad(found);

		if (is_index_modified_since_logged(
			space_id, root_page_no)) {
			/* Page has been modified since TRUNCATE log snapshot
			was recorded so not safe to drop the index. */
			continue;
		}

		mtr_start(&mtr);

		if (space_id != TRX_SYS_SPACE) {
			/* Do not log changes for single-table
			tablespaces, we are in recovery mode. */
			mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
		}

		if (root_page_no != FIL_NULL) {
			const page_id_t	root_page_id(space_id, root_page_no);

			btr_free_if_exists(
				root_page_id, page_size, it->m_id, &mtr);
		}

		/* If tree is already freed then we might return immediately
		in which case we need to release the lock we have acquired
		on root_page. */
		mtr_commit(&mtr);
	}
}


/** Create the indexes for a table
@param[in]	table_name	table name, for which to create the indexes
@param[in]	space_id	space id where we have to create the indexes
@param[in]	page_size	page size of the .ibd file
@param[in]	flags		tablespace flags
@param[in]	format_flags	page format flags
@return DB_SUCCESS or error code. */
dberr_t
truncate_t::create_indexes(
	const char*		table_name,
	ulint			space_id,
	const page_size_t&	page_size,
	ulint			flags,
	ulint			format_flags)
{
	mtr_t           mtr;

	mtr_start(&mtr);

	if (space_id != TRX_SYS_SPACE) {
		/* Do not log changes for single-table tablespaces, we
		are in recovery mode. */
		mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
	}

	/* Create all new index trees with table format, index ids, index
	types, number of index fields and index field information taken
	out from the TRUNCATE log record. */

	ulint   root_page_no = FIL_NULL;
	indexes_t::iterator       end = m_indexes.end();
	for (indexes_t::iterator it = m_indexes.begin();
	     it != end;
	     ++it) {

		btr_create_t    btr_redo_create_info(
			fsp_flags_is_compressed(flags)
			? &it->m_fields[0] : NULL);

		btr_redo_create_info.format_flags = format_flags;

		if (fsp_flags_is_compressed(flags)) {

			btr_redo_create_info.n_fields = it->m_n_fields;
			/* Skip the NUL appended field */
			btr_redo_create_info.field_len =
				it->m_fields.size() - 1;
			btr_redo_create_info.trx_id_pos = it->m_trx_id_pos;
		}

		root_page_no = create_index(
			table_name, space_id, page_size, it->m_type, it->m_id,
			btr_redo_create_info, &mtr);

		if (root_page_no == FIL_NULL) {
			break;
		}

		it->m_new_root_page_no = root_page_no;
	}

	mtr_commit(&mtr);

	return(root_page_no == FIL_NULL ? DB_ERROR : DB_SUCCESS);
}

/**
Write a TRUNCATE log record for fixing up table if truncate crashes.
@param start_ptr	buffer to write log record
@param end_ptr		buffer end
@param space_id		space id
@param tablename	the table name in the usual databasename/tablename
			format of InnoDB
@param flags		tablespace flags
@param format_flags	page format
@param lsn		lsn while logging
@return DB_SUCCESS or error code */

dberr_t
truncate_t::write(
	byte*		start_ptr,
	byte*		end_ptr,
	ulint		space_id,
	const char*	tablename,
	ulint		flags,
	ulint		format_flags,
	lsn_t		lsn) const
{
	if (end_ptr < start_ptr) {
		return(DB_FAIL);
	}

	/* LSN, Type, Space-ID, format-flag (also know as log_flag.
	Stored in page_no field), tablespace flags */
	if (end_ptr < (start_ptr + (8 + 4 + 4 + 4)))  {
		return(DB_FAIL);
	}

	mach_write_to_8(start_ptr, lsn);
	start_ptr += 8;

	mach_write_to_4(start_ptr, space_id);
	start_ptr += 4;

	mach_write_to_4(start_ptr, format_flags);
	start_ptr += 4;

	mach_write_to_4(start_ptr, flags);
	start_ptr += 4;

	/* Name of the table. */
	/* Include the NUL in the log record. */
	ulint len = strlen(tablename) + 1;
	if (end_ptr < (start_ptr + (len + 2))) {
		return(DB_FAIL);
	}

	mach_write_to_2(start_ptr, len);
	start_ptr += 2;

	memcpy(start_ptr, tablename, len - 1);
	start_ptr += len;

	DBUG_EXECUTE_IF("ib_trunc_crash_while_writing_redo_log",
			DBUG_SUICIDE(););

	/* Old/New Table-ID, Number of Indexes and Tablespace dir-path-name. */
	/* Write the remote directory of the table into mtr log */
	len = m_dir_path != NULL ? strlen(m_dir_path) + 1 : 0;
	if (end_ptr < (start_ptr + (len + 8 + 8 + 2 + 2))) {
		return(DB_FAIL);
	}

	/* Write out old-table-id. */
	mach_write_to_8(start_ptr, m_old_table_id);
	start_ptr += 8;

	/* Write out new-table-id. */
	mach_write_to_8(start_ptr, m_new_table_id);
	start_ptr += 8;

	/* Write out the number of indexes. */
	mach_write_to_2(start_ptr, m_indexes.size());
	start_ptr += 2;

	/* Write the length (NUL included) of the .ibd path. */
	mach_write_to_2(start_ptr, len);
	start_ptr += 2;

	if (m_dir_path != NULL) {
		memcpy(start_ptr, m_dir_path, len - 1);
		start_ptr += len;
	}

	/* Indexes information (id, type) */
	/* Write index ids, type, root-page-no into mtr log */
	for (ulint i = 0; i < m_indexes.size(); ++i) {

		if (end_ptr < (start_ptr + (8 + 4 + 4 + 4))) {
			return(DB_FAIL);
		}

		mach_write_to_8(start_ptr, m_indexes[i].m_id);
		start_ptr += 8;

		mach_write_to_4(start_ptr, m_indexes[i].m_type);
		start_ptr += 4;

		mach_write_to_4(start_ptr, m_indexes[i].m_root_page_no);
		start_ptr += 4;

		mach_write_to_4(start_ptr, m_indexes[i].m_trx_id_pos);
		start_ptr += 4;
	}

	/* If tablespace compressed then field info of each index. */
	if (fsp_flags_is_compressed(flags)) {

		for (ulint i = 0; i < m_indexes.size(); ++i) {

			ulint len = m_indexes[i].m_fields.size();
			if (end_ptr < (start_ptr + (len + 2 + 2))) {
				return(DB_FAIL);
			}

			mach_write_to_2(
				start_ptr, m_indexes[i].m_n_fields);
			start_ptr += 2;

			mach_write_to_2(start_ptr, len);
			start_ptr += 2;

			const byte*	ptr = &m_indexes[i].m_fields[0];
			memcpy(start_ptr, ptr, len - 1);
			start_ptr += len;
		}
	}

	return(DB_SUCCESS);
}

