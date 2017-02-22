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
@file include/fsp0sysspace.h
Multi file, shared, system tablespace implementation.

Created 2013-7-26 by Kevin Lewis
*******************************************************/

#ifndef fsp0sysspace_h
#define fsp0sysspace_h

#include "univ.i"
#include "fsp0space.h"

/** If the last data file is auto-extended, we add this many pages to it
at a time. We have to make this public because it is a config variable. */
extern ulong sys_tablespace_auto_extend_increment;

#ifdef UNIV_DEBUG
/** Control if extra debug checks need to be done for temporary tablespace.
Default = true that is disable such checks.
This variable is not exposed to end-user but still kept as variable for
developer to enable it during debug. */
extern bool srv_skip_temp_table_checks_debug;
#endif /* UNIV_DEBUG */

/** Data structure that contains the information about shared tablespaces.
Currently this can be the system tablespace or a temporary table tablespace */
class SysTablespace : public Tablespace
{
public:

	SysTablespace()
		:
		m_auto_extend_last_file(),
		m_last_file_size_max(),
		m_created_new_raw(),
		m_is_tablespace_full(false),
		m_sanity_checks_done(false)
	{
		/* No op */
	}

	~SysTablespace()
	{
		shutdown();
	}

	/** Set tablespace full status
	@param[in]	is_full		true if full */
	void set_tablespace_full_status(bool is_full)
	{
		m_is_tablespace_full = is_full;
	}

	/** Get tablespace full status
	@return true if table is full */
	bool get_tablespace_full_status()
	{
		return(m_is_tablespace_full);
	}

	/** Set sanity check status
	@param[in]	status	true if sanity checks are done */
	void set_sanity_check_status(bool status)
	{
		m_sanity_checks_done = status;
	}

	/** Get sanity check status
	@return true if sanity checks are done */
	bool get_sanity_check_status()
	{
		return(m_sanity_checks_done);
	}

	/** Parse the input params and populate member variables.
	@param	filepath	path to data files
	@param	supports_raw	true if it supports raw devices
	@return true on success parse */
	bool parse_params(const char* filepath, bool supports_raw);

	/** Check the data file specification.
	@param[out]	create_new_db		true if a new database
	is to be created
	@param[in]	min_expected_size	expected tablespace
	size in bytes
	@return DB_SUCCESS if all OK else error code */
	dberr_t check_file_spec(
		bool*	create_new_db,
		ulint	min_expected_tablespace_size);

	/** Free the memory allocated by parse() */
	void shutdown();

	/** Normalize the file size, convert to extents. */
	void normalize();

	/**
	@return true if a new raw device was created. */
	bool created_new_raw() const
	{
		return(m_created_new_raw);
	}

	/**
	@return auto_extend value setting */
	ulint can_auto_extend_last_file() const
	{
		return(m_auto_extend_last_file);
	}

	/** Set the last file size.
	@param[in]	size	the size to set */
	void set_last_file_size(ulint size)
	{
		ut_ad(!m_files.empty());
		m_files.back().m_size = size;
	}

	/** Get the size of the last data file in the tablespace
	@return the size of the last data file in the array */
	ulint last_file_size() const
	{
		ut_ad(!m_files.empty());
		return(m_files.back().m_size);
	}

	/**
	@return the autoextend increment in pages. */
	ulint get_autoextend_increment() const
	{
		return(sys_tablespace_auto_extend_increment
		       * ((1024 * 1024) / UNIV_PAGE_SIZE));
	}

	/** Roundoff to MegaBytes is similar as done in
	SysTablespace::parse_units() function.
	@return the pages when given size of file (bytes). */
	ulint get_pages_from_size(os_offset_t size)
	{
		return (ulint)((size / (1024 * 1024))
			       * ((1024 * 1024) / UNIV_PAGE_SIZE));
	}

	/**
	@return next increment size */
	ulint get_increment() const;

	/** Open or create the data files
	@param[in]  is_temp		whether this is a temporary tablespace
	@param[in]  create_new_db	whether we are creating a new database
	@param[out] sum_new_sizes	sum of sizes of the new files added
	@param[out] flush_lsn		FIL_PAGE_FILE_FLUSH_LSN of first file
	@return DB_SUCCESS or error code */
	dberr_t open_or_create(
		bool	is_temp,
		bool	create_new_db,
		ulint*	sum_new_sizes,
		lsn_t*	flush_lsn)
		MY_ATTRIBUTE((warn_unused_result));

private:
	/** Check the tablespace header for this tablespace.
	@param[out]	flushed_lsn	the value of FIL_PAGE_FILE_FLUSH_LSN
	@return DB_SUCCESS or error code */
	dberr_t read_lsn_and_check_flags(lsn_t* flushed_lsn);

	/**
	@return true if the last file size is valid. */
	bool is_valid_size() const
	{
		return(m_last_file_size_max >= last_file_size());
	}

	/**
	@return true if configured to use raw devices */
	bool has_raw_device();

	/** Note that the data file was not found.
	@param[in]	file		data file object
	@param[out]	create_new_db	true if a new instance to be created
	@return DB_SUCESS or error code */
	dberr_t file_not_found(Datafile& file, bool* create_new_db);

	/** Note that the data file was found.
	@param[in,out]	file	data file object
	@return true if a new instance to be created */
	bool file_found(Datafile& file);

	/** Create a data file.
	@param[in,out]	file	data file object
	@return DB_SUCCESS or error code */
	dberr_t create(Datafile& file);

	/** Create a data file.
	@param[in,out]	file	data file object
	@return DB_SUCCESS or error code */
	dberr_t create_file(Datafile& file);

	/** Open a data file.
	@param[in,out]	file	data file object
	@return DB_SUCCESS or error code */
	dberr_t open_file(Datafile& file);

	/** Set the size of the file.
	@param[in,out]	file	data file object
	@return DB_SUCCESS or error code */
	dberr_t set_size(Datafile& file);

	/** Convert a numeric string that optionally ends in G or M, to a
	number containing megabytes.
	@param[in]	ptr	string with a quantity in bytes
	@param[out]	megs	the number in megabytes
	@return next character in string */
	static char* parse_units(char* ptr, ulint* megs);

private:
	enum file_status_t {
		FILE_STATUS_VOID = 0,		/** status not set */
		FILE_STATUS_RW_PERMISSION_ERROR,/** permission error */
		FILE_STATUS_READ_WRITE_ERROR,	/** not readable/writable */
		FILE_STATUS_NOT_REGULAR_FILE_ERROR /** not a regular file */
	};

	/** Verify the size of the physical file
	@param[in]	file	data file object
	@return DB_SUCCESS if OK else error code. */
	dberr_t check_size(Datafile& file);

	/** Check if a file can be opened in the correct mode.
	@param[in,out]	file	data file object
	@param[out]	reason	exact reason if file_status check failed.
	@return DB_SUCCESS or error code. */
	dberr_t check_file_status(
		const Datafile& 	file,
		file_status_t& 		reason);

	/* DATA MEMBERS */

	/** if true, then we auto-extend the last data file */
	bool		m_auto_extend_last_file;

	/** if != 0, this tells the max size auto-extending may increase the
	last data file size */
	ulint		m_last_file_size_max;

	/** If the following is true we do not allow
	inserts etc. This protects the user from forgetting
	the 'newraw' keyword to my.cnf */
	bool		m_created_new_raw;

	/** Tablespace full status */
	bool		m_is_tablespace_full;

	/** if false, then sanity checks are still pending */
	bool		m_sanity_checks_done;
};

/* GLOBAL OBJECTS */

/** The control info of the system tablespace. */
extern SysTablespace srv_sys_space;

/** The control info of a temporary table shared tablespace. */
extern SysTablespace srv_tmp_space;

/** Check if the space_id is for a system-tablespace (shared + temp).
@param[in]	id	Space ID to check
@return true if id is a system tablespace, false if not. */
UNIV_INLINE
bool
is_system_tablespace(
	ulint	id)
{
	return(id == srv_sys_space.space_id()
	       || id == srv_tmp_space.space_id());
}

/** Check if shared-system or undo tablespace.
@return true if shared-system or undo tablespace */
UNIV_INLINE
bool
is_system_or_undo_tablespace(
	ulint   id)
{
	return(id == srv_sys_space.space_id()
	       || srv_is_undo_tablespace(id));
}

/** Check if predefined shared tablespace.
@return true if predefined shared tablespace */
UNIV_INLINE
bool
is_predefined_tablespace(
	ulint   id)
{
	ut_ad(srv_sys_space.space_id() == TRX_SYS_SPACE);
	ut_ad(TRX_SYS_SPACE == 0);
	return(is_system_or_undo_tablespace(id)
	       || id == srv_tmp_space.space_id());
}
#endif /* fsp0sysspace_h */
