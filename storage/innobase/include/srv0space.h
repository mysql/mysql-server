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
@file include/srv0space.h
Multi file shared tablespace implementation.

Created 2012-11-16 by Sunny Bains.
*******************************************************/

#ifndef srv0space_h
#define srv0space_h

#include "srv0srv.h"
#include "os0file.h"

/** Data structure that contains the information about shared tablespaces.
Currently this can be the system tablespace or a temporary table tablespace */
class Tablespace {

	/** Types of raw partitions in innodb_data_file_path */
	enum device_t {
		SRV_NOT_RAW = 0,	/*!< Not a raw partition */
		SRV_NEW_RAW,		/*!< A 'newraw' partition, only to be
					initialized */
		SRV_OLD_RAW		/*!< An initialized raw partition */
	};

	/** Data file control information. */
	struct file_t {

		file_t(const char* name, ulint size)
			:
			m_name(::strdup(name)),
			m_size(size),
			m_type(SRV_NOT_RAW),
			m_handle(os_file_t(~0)),
			m_exists(),
			m_open_flags(OS_FILE_OPEN),
			m_filename()
		{
			/* No op */
		}

		~file_t()
		{
			shutdown();
		}

		file_t(const file_t& file)
			:
			m_size(file.m_size),
			m_type(file.m_type),
			m_handle(file.m_handle),
			m_exists(file.m_exists),
			m_open_flags(file.m_open_flags)
		{
			m_name = ::strdup(file.m_name);
			ut_a(m_name != 0);

			if (file.m_filename != 0) {
				m_filename = ::strdup(file.m_filename);
				ut_a(m_filename != 0);
			} else {
				m_filename = 0;
			}
		}

		file_t& operator=(const file_t& file)
		{
			ut_a(this != &file);

			if (m_name != 0) {
				::free(m_name);
			}

			m_name = ::strdup(file.m_name);
			ut_a(m_name != 0);

			m_size = file.m_size;
			m_type = file.m_type;

			ut_a(m_handle == os_file_t(~0));
			m_handle = file.m_handle;

			m_exists = file.m_exists;
			m_open_flags = file.m_open_flags;

			if (m_filename != 0) {
				::free(m_filename);
				m_filename = 0;
			}

			if (file.m_filename != 0) {
				m_filename = ::strdup(file.m_filename);
			}

			return(*this);
		}

		/** Release the resources. */
		void shutdown()
		{
			ut_a(m_handle == os_file_t(~0));

			if (m_name != 0) {
				::free(m_name);
				m_name = 0;
			}

			if (m_filename != 0) {
				::free(m_filename);
				m_filename = 0;
			}
		}

		/** Temp data files names */
		char*			m_name;

		/** size in database pages */
		ulint			m_size;

		/** The type of the data file */
		device_t		m_type;

		/** Open file handle */
		os_file_t		m_handle;

		/** true if file already existed on startup */
		bool			m_exists;

		/** Flags to use for opening the data file */
		os_file_create_t	m_open_flags;

		/** Physical filename */
		char*			m_filename;
	};

	typedef std::vector<file_t> files_t;

public:
	Tablespace()
		:
		m_space_id(ULINT_UNDEFINED),
		m_files(),
		m_auto_extend_last_file(),
		m_last_file_size_max(),
		m_created_new_raw(),
		m_auto_extend_increment()
	{
		/* No op */
	}

	~Tablespace()
	{
		shutdown();
		ut_ad(m_files.empty());
		ut_ad(m_space_id == ULINT_UNDEFINED);
	}

	/**
	Set the space id of the tablespace
	@param space_id - space id to set */
	void set_space_id(ulint space_id)
	{
		ut_a(m_space_id == ULINT_UNDEFINED);
		m_space_id = space_id;
	}

	/**
	Parse the input params and populate member variables.
	@param filepath - path to data files
	@param supports_raw - true if it supports raw devices
	@return true on success parse */
	bool parse(const char* filepath, bool supports_raw);

	/**
	Check the data file specification.
	@param create_new_db - true if a new database is to be created
	@param min_expected_tablespace_size - [in] expected tablespace
		size in bytes
	@return DB_SUCCESS if all OK else error code */
	dberr_t check_file_spec(
		ibool*	create_new_db,
		ulint	min_expected_tablespace_size);

	/**
	Free the memory allocated by parse() */
	void shutdown();

	/**
	Normalize the file size, convert to extents. */
	void normalize();

	/**
	@return the space id of this tablespace. */
	ulint space_id() const
	{
		return(m_space_id);
	}

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

	/**
	Set the last file size.
	@param size - the size to set */
	void set_last_file_size(ulint size)
	{
		ut_a(!m_files.empty());
		m_files.back().m_size = size;
	}

	/**
	@return ULINT_UNDEFINED if the size is invalid else the sum of sizes */
	ulint get_sum_of_sizes() const;

	/**
	@return next increment size */
	ulint get_increment() const;

	/**
	Open the data files.

	@param sum_of_new_sizes - sum of sizes of new files added
	@return DB_SUCCESS or error code */
	dberr_t open(ulint* sum_of_new_sizes);

	/**
	Read the flush lsn values and check the header flags.

	@param min_flushed_lsn - min of flushed lsn values in data files
	@param max_flushed_lsn - max of flushed lsn values in data files
	@return DB_SUCCESS or error code */
	dberr_t read_lsn_and_check_flags(
		lsn_t*		min_flushed_lsn,
		lsn_t*		max_flushed_lsn);

	/**
	Delete all the data files. */
	void delete_files();

	/** Check if two shared tablespaces have common data file names.
	@param space1 - space to check
	@param space2 - space to check
	@return true if they have the same data filenames and paths */
	static bool intersection(
		const Tablespace&	space1,
		const Tablespace&	space2);

private:
	/**
	@return the size of the last data file in the array */
	ulint last_file_size() const
	{
		ut_ad(!m_files.empty());
		return(m_files.back().m_size);
	}

	/**
	@return true if the last file size is valid. */
	bool is_valid_size() const
	{
		return(m_last_file_size_max >= last_file_size());
	}

	/**
	@return the autoextend increment in pages. */
	ulint get_autoextend_increment() const
	{
		return(m_auto_extend_increment
		       * ((1024 * 1024) / UNIV_PAGE_SIZE));
	}

	/**
	@param file - data file spec
	@return true if it is a RAW device. */
	bool is_raw_device(const file_t& file) const
	{
		return(file.m_type != SRV_NOT_RAW);
	}

	/**
	@return true if configured to use raw devices */
	bool has_raw_device() const;

	/**
	@param filename - name to lookup in the data files
	@return true if the filename exists in the data files */
	bool find(const char* filename) const;

	/**
	Note that the data file was not found.
	@param file - data file spec
	@param create_new_db - [out] true if a new instances to be created
	@return DB_SUCESS or error code */
	dberr_t file_not_found(file_t& file, ibool* create_new_db);

	/**
	Note that the data file was found.
	@param file - data file spec */
	void file_found(file_t& file);

	/**
	Create a data file.
	@param file - data file spec
	@return DB_SUCCESS or error code */
	dberr_t create(file_t& file);

	/**
	Verify the size of the physical file
	@param file - data file spec
	@return DB_SUCCESS if OK else error code. */
	dberr_t check_size(file_t& file);

	/**
	Create a data file.
	@param file - data file spec
	@return DB_SUCCESS or error code */
	dberr_t create_file(file_t& file);

	/**
	Open a data file.
	@param file - data file spec
	@return DB_SUCCESS or error code */
	dberr_t open_file(file_t& file);

	/**
	Open/create a data file.
	@param file - data file spec
	@return DB_SUCCESS or error code */
	static dberr_t open_data_file(file_t& file);

	/**
	Check if a file can be opened in the correct mode.
	@param file - file control information
	@return DB_SUCCESS or error code. */
	static dberr_t check_file_status(const file_t& file);

	/**
	Set the size of the file.
	@param file - data file spec
	@return DB_SUCCESS or error code */
	static dberr_t set_size(file_t& file);

	/**
	Make physical filename from control info.
	@param file - control information */
	static void make_name(file_t& file);

	/**
	Convert a numeric string that optionally ends in G or M, to a number
	containing megabytes.
	@param str - string with a quantity in bytes
	@param megs - out the number in megabytes
	@return next character in string */
	static char* parse_units(char* ptr, ulint* megs);

	/**
	Get the file name only
	@param filepath - filepath as specified by user (can be relative too).
	@return filename extract filepath */
	static char* get_file_name(const char* filepath);

	// Disable copying
	Tablespace(const Tablespace&);
	Tablespace& operator=(const Tablespace&);

private:
	/** This is dynamically allocated on each start of server. */
	ulint		m_space_id;

	/** Data file information */
	files_t		m_files;

	/** if true, then we auto-extend the last data file */
	bool		m_auto_extend_last_file;

	/** if != 0, this tells the max size auto-extending may increase the
	last data file size */
	ulint		m_last_file_size_max;

	/** If the following is true we do not allow
	inserts etc. This protects the user from forgetting
	the 'newraw' keyword to my.cnf */
	bool		m_created_new_raw;

public:
	/* We have to make this public because it is a config variable. */

	/** If the last data file is auto-extended, we add this
	many pages to it at a time */
	ulong		m_auto_extend_increment;
};

/** The control info of the system tablespace. */
extern Tablespace srv_sys_space;

/** The control info of a temporary table shared tablespace. */
extern Tablespace srv_tmp_space;
#endif /* srv0space_h */
