/*****************************************************************************

Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/row0trunc.h
TRUNCATE implementation

Created 2013-04-25 Krunal Bauskar
*******************************************************/

#ifndef row0trunc_h
#define row0trunc_h

#include "row0mysql.h"
#include "dict0boot.h"
#include "fil0fil.h"
#include "srv0start.h"
#include "ut0new.h"

#include <vector>

/** The information of TRUNCATE log record.
This class handles the recovery stage of TRUNCATE table. */
class truncate_t {

public:
	/**
	Constructor

	@param old_table_id	old table id assigned to table before truncate
	@param new_table_id	new table id that will be assigned to table
				after truncate
	@param dir_path		directory path */
	truncate_t(
		table_id_t	old_table_id,
		table_id_t	new_table_id,
		const char*	dir_path);

	/**
	Constructor

	@param log_file_name	parse the log file during recovery to populate
				information related to table to truncate */
	truncate_t(const char*	log_file_name);

	/**
	Consturctor

	@param space_id		space in which table reisde
	@param name		table name
	@param tablespace_flags	tablespace flags use for recreating tablespace
	@param log_flags	page format flag
	@param recv_lsn		lsn of redo log record. */
	truncate_t(
		ulint		space_id,
		const char*	name,
		ulint		tablespace_flags,
		ulint		log_flags,
		lsn_t		recv_lsn);

	/** Destructor */
	~truncate_t();

	/** The index information of MLOG_FILE_TRUNCATE redo record */
	struct index_t {

		/* Default copy constructor and destructor should be OK. */

		index_t();

		/**
		Set the truncate log values for a compressed table.
		@return DB_CORRUPTION or error code */
		dberr_t set(const dict_index_t* index);

		typedef std::vector<byte, ut_allocator<byte> >	fields_t;

		/** Index id */
		index_id_t	m_id;

		/** Index type */
		ulint		m_type;

		/** Root Page Number */
		ulint		m_root_page_no;

		/** New Root Page Number.
		Note: This field is not persisted to TRUNCATE log but used
		during truncate table fix-up for updating SYS_XXXX tables. */
		ulint		m_new_root_page_no;

		/** Number of index fields */
		ulint		m_n_fields;

		/** DATA_TRX_ID column position. */
		ulint		m_trx_id_pos;

		/** Compressed table field meta data, encode by
		page_zip_fields_encode. Empty for non-compressed tables.
		Should be NUL terminated. */
		fields_t	m_fields;
	};

	/**
	@return the directory path, can be NULL */
	const char* get_dir_path() const
	{
		return(m_dir_path);
	}

	/**
	Register index information

	@param index	index information logged as part of truncate log. */
	void add(index_t& index)
	{
		m_indexes.push_back(index);
	}

	/**
	Add table to truncate post recovery.

	@param ptr	table information need to complete truncate of table. */
	static void add(truncate_t* ptr)
	{
		s_tables.push_back(ptr);
	}

	/**
	Clear registered index vector */
	void clear()
	{
		m_indexes.clear();
	}

	/**
	@return old table id of the table to truncate */
	table_id_t old_table_id() const
	{
		return(m_old_table_id);
	}

	/**
	@return new table id of the table to truncate */
	table_id_t new_table_id() const
	{
		return(m_new_table_id);
	}

	/**
	Update root page number in SYS_XXXX tables.

	@param trx			transaction object
	@param table_id			table id for which information needs to
					be updated.
	@param reserve_dict_mutex       if TRUE, acquire/release
					dict_sys->mutex around call to pars_sql.
	@param mark_index_corrupted	if true, then mark index corrupted
	@return DB_SUCCESS or error code */
	dberr_t update_root_page_no(
		trx_t*		trx,
		table_id_t	table_id,
		ibool		reserve_dict_mutex,
		bool		mark_index_corrupted) const;

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
	ulint create_index(
		const char*		table_name,
		ulint			space_id,
		const page_size_t&	page_size,
		ulint			index_type,
		index_id_t      	index_id,
		const btr_create_t&	btr_redo_create_info,
		mtr_t*			mtr) const;

	/** Create the indexes for a table
	@param[in]	table_name	table name, for which to create the
	indexes
	@param[in]	space_id	space id where we have to create the
	indexes
	@param[in]	page_size	page size of the .ibd file
	@param[in]	flags		tablespace flags
	@param[in]	format_flags	page format flags
	@return DB_SUCCESS or error code. */
	dberr_t create_indexes(
		const char*		table_name,
		ulint			space_id,
		const page_size_t&	page_size,
		ulint			flags,
		ulint			format_flags);

	/** Check if index has been modified since TRUNCATE log snapshot
	was recorded.
	@param space_id	space_id where table/indexes resides.
	@return true if modified else false */
	bool is_index_modified_since_logged(
		ulint		space_id,
		ulint		root_page_no) const;

	/** Drop indexes for a table.
	@param space_id		space_id where table/indexes resides.
	@return DB_SUCCESS or error code. */
	void drop_indexes(ulint	space_id) const;

	/**
	Parses log record during recovery
	@param start_ptr	buffer containing log body to parse
	@param end_ptr		buffer end

	@return DB_SUCCESS or error code */
	dberr_t parse(
		byte*		start_ptr,
		const byte*	end_ptr);

	/** Parse MLOG_TRUNCATE log record from REDO log file during recovery.
	@param[in,out]	start_ptr	buffer containing log body to parse
	@param[in]	end_ptr		buffer end
	@param[in]	space_id	tablespace identifier
	@return parsed upto or NULL. */
	static byte* parse_redo_entry(
		byte*		start_ptr,
		const byte*	end_ptr,
		ulint		space_id);

	/**
	Write a log record for truncating a single-table tablespace.

	@param start_ptr	buffer to write log record
	@param end_ptr		buffer end
	@param space_id		space id
	@param tablename	the table name in the usual
				databasename/tablename format of InnoDB
	@param flags		tablespace flags
	@param format_flags	page format
	@param lsn		lsn while logging */
	dberr_t write(
		byte*		start_ptr,
		byte*		end_ptr,
		ulint		space_id,
		const char*	tablename,
		ulint		flags,
		ulint		format_flags,
		lsn_t		lsn) const;

	/**
	@return number of indexes parsed from the truncate log record */
	size_t indexes() const;

	/**
	Truncate a single-table tablespace. The tablespace must be cached
	in the memory cache.

	Note: This is defined in fil0fil.cc because it needs to access some
	types that are local to that file.

	@param space_id		space id
	@param dir_path		directory path
	@param tablename	the table name in the usual
				databasename/tablename format of InnoDB
	@param flags		tablespace flags
	@param default_size	if true, truncate to default size if tablespace
				is being newly re-initialized.
	@return DB_SUCCESS or error */
	static dberr_t truncate(
		ulint		space_id,
		const char*	dir_path,
		const char*	tablename,
		ulint		flags,
		bool		default_size);

	/**
	Fix the table truncate by applying information parsed from TRUNCATE log.
	Fix-up includes re-creating table (drop and re-create indexes) 
	@return	error code or DB_SUCCESS */
	static dberr_t fixup_tables_in_system_tablespace();

	/**
	Fix the table truncate by applying information parsed from TRUNCATE log.
	Fix-up includes re-creating tablespace.
	@return	error code or DB_SUCCESS */
	static dberr_t fixup_tables_in_non_system_tablespace();

	/**
	Check whether a tablespace was truncated during recovery
	@param space_id		tablespace id to check
	@return true if the tablespace was truncated */
	static bool is_tablespace_truncated(ulint space_id);

	/** Was tablespace truncated (on crash before checkpoint).
	If the MLOG_TRUNCATE redo-record is still available then tablespace
	was truncated and checkpoint is yet to happen.
	@param[in]	space_id	tablespace id to check.
	@return true if tablespace was truncated. */
	static bool was_tablespace_truncated(ulint space_id);

	/** Get the lsn associated with space.
	@param[in]	space_id	tablespace id to check.
	@return associated lsn. */
	static lsn_t get_truncated_tablespace_init_lsn(ulint space_id);

private:
	typedef std::vector<index_t, ut_allocator<index_t> >	indexes_t;

	/** Space ID of tablespace */
	ulint			m_space_id;

	/** ID of table that is being truncated. */
	table_id_t		m_old_table_id;

	/** New ID that will be assigned to table on truncation. */
	table_id_t		m_new_table_id;

	/** Data dir path of tablespace */
	char*			m_dir_path;

	/** Table name */
	char*			m_tablename;

	/** Tablespace Flags */
	ulint			m_tablespace_flags;

	/** Format flags (log flags; stored in page-no field of header) */
	ulint			m_format_flags;

	/** Index meta-data */
	indexes_t		m_indexes;

	/** LSN of TRUNCATE log record. */
	lsn_t			m_log_lsn;

	/** Log file name. */
	char*			m_log_file_name;

	/** Vector of tables to truncate. */
	typedef	std::vector<truncate_t*, ut_allocator<truncate_t*> >
		tables_t;

	/** Information about tables to truncate post recovery */
	static	tables_t	s_tables;

	/** Information about truncated table
	This is case when truncate is complete but checkpoint hasn't. */
	typedef std::map<ulint, lsn_t>	truncated_tables_t;
	static truncated_tables_t	s_truncated_tables;

public:
	/** If true then fix-up of table is active and so while creating
	index instead of grabbing information from dict_index_t, grab it
	from parsed truncate log record. */
	static	bool		s_fix_up_active;
};

/**
Parse truncate log file. */
class TruncateLogParser {

public:

	/**
	Scan and Parse truncate log files.

	@param dir_path         look for log directory in following path
	@return DB_SUCCESS or error code. */
	static dberr_t scan_and_parse(
		const char*	dir_path);

private:
	typedef std::vector<char*, ut_allocator<char*> >
		trunc_log_files_t;

private:
	/**
	Scan to find out truncate log file from the given directory path.

	@param dir_path		look for log directory in following path.
	@param log_files	cache to hold truncate log file name found.
	@return DB_SUCCESS or error code. */
	static dberr_t scan(
		const char*		dir_path,
		trunc_log_files_t&	log_files);

	/**
	Parse the log file and populate table to truncate information.
	(Add this table to truncate information to central vector that is then
	used by truncate fix-up routine to fix-up truncate action of the table.)

	@param	log_file_name	log file to parse
	@return DB_SUCCESS or error code. */
	static dberr_t parse(
		const char*		log_file_name);
};


/**
Truncates a table for MySQL.
@param table		table being truncated
@param trx		transaction covering the truncate
@return	error code or DB_SUCCESS */
dberr_t
row_truncate_table_for_mysql(dict_table_t* table, trx_t* trx);

#endif /* row0trunc_h */

