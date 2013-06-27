/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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

Created 2013-04-25 Krunal Bauskar
*******************************************************/

#ifndef row0trunc_h
#define row0trunc_h

#include "row0mysql.h"
#include "dict0boot.h"

/** The information of MLOG_FILE_TRUNCATE redo record.
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
		ulint		old_table_id,
		ulint		new_table_id,
		const char*	dir_path);

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
		Set the truncate redo log values for a compressed table.
		@return DB_CORRUPTION or error code */
		dberr_t set(const dict_index_t* index);

		typedef std::vector<byte> fields_t;

		/** Index id */
		index_id_t	m_id;

		/** Index type */
		ulint		m_type;

		/** Root Page Number */
		ulint		m_root_page_no;

		/** New Root Page Number.
		Note: This field is not persisted to REDO log but used during
		truncate table fix-up for updating SYS_XXXX tables. */
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

	@param index	index information logged as part of truncate redo. */
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
	@param reserve_dict_mutex       if true, acquire/release
					dict_sys->mutex around call to pars_sql.
	@return DB_SUCCESS or error code */
	dberr_t update_root_page_no(
		trx_t*		trx,
		table_id_t	table_id,
		bool		reserve_dict_mutex) const;

	/**
	Create an index for a table.

	@param table_name	table name, for which to create the index
	@param space_id		space id where we have to create the index
	@param zip_size		page size of the .ibd file
	@param index_type	type of index to truncate
	@param index_id		id of index to truncate
	@param btr_create_info	control info for ::btr_create()
	@param mtr		mini-transaction covering the create index
	@return root page no or FIL_NULL on failure */
	ulint create_index(
		const char*	table_name,
		ulint		space_id,
		ulint		zip_size,
		ulint		index_type,
		index_id_t      index_id,
		btr_create_t&	btr_create_info,
		mtr_t*		mtr) const;

	/** Create the indexes for a table

	@param table_name	table name, for which to create the indexes
	@param space_id		space id where we have to create the indexes
	@param zip_size		page size of the .ibd file
	@param flags		tablespace flags
	@param format_flags	page format flags
	@return DB_SUCCESS or error code. */
	dberr_t create_indexes(
		const char*	table_name,
		ulint		space_id,
		ulint		zip_size,
		ulint		flags,
		ulint		format_flags);

	/** Check if index has been modified since REDO log snapshot
	was recorded.
	@param space_id	space_id where table/indexes resides.
	@return true if modified else false */
	bool is_index_modified_since_redologged(
		ulint		space_id,
		ulint		root_page_no) const;

	/** Drop indexes for a table.
	@param space_id		space_id where table/indexes resides.
	@return DB_SUCCESS or error code. */
	void drop_indexes(ulint space_id) const;

	/**
	Parses MLOG_FILE_TRUNCATE redo record during recovery
	@param ptr		buffer containing the main body of
				MLOG_FILE_TRUNCATE record
	@param end_ptr		buffer end
	@param flags		tablespace flags

	@return true if successfully parsed the MLOG_FILE_TRUNCATE record */
	bool parse(byte** ptr, const byte** end_ptr, ulint flags);

	/**
	Write a redo log record for truncating a single-table tablespace.

	@param space_id		space id
	@param tablename	the table name in the usual
				databasename/tablename format of InnoDB
	@param flags		tablespace flags
	@param format_flags	page format */
	void write(
		ulint		space_id,
		const char*	tablename,
		ulint		flags,
		ulint		format_flags) const;

	/**
	@return number of indexes parsed from the redo log record */
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
	Fix the table truncate by applying information cached while REDO log
	scan phase. Fix-up includes re-creating table (drop and re-create
	indexes) and for single-tablespace re-creating tablespace.
	@return	error code or DB_SUCCESS */
	static dberr_t fixup_tables();

	/**
	Check whether a tablespace was truncated during recovery
	@param space_id		tablespace id to check
	@return true if the tablespace was truncated */
	static bool is_tablespace_truncated(ulint space_id);
private:
	typedef std::vector<index_t> indexes_t;

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

	/** Format flags (log flags; stored in page-no field of redo header) */
	ulint			m_format_flags;

	/** Index meta-data */
	indexes_t		m_indexes;

	/** LSN of REDO log record. */
	lsn_t			m_redo_log_lsn;

	/** Vector of tables to truncate. */
	typedef	std::vector<truncate_t*> tables_t;

	/** Information about tables to truncate post recovery */
	static	tables_t	s_tables;
public:
	static	bool		s_fix_up_active;
};

/**
Truncates a table for MySQL.
@param table		table being truncated
@param trx		transaction covering the truncate
@return	error code or DB_SUCCESS */

dberr_t
row_truncate_table_for_mysql(dict_table_t* table, trx_t* trx);

#endif /* row0trunc_h */

