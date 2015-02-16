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
@file include/fsp0file.h
Tablespace data file implementation.

Created 2013-7-26 by Kevin Lewis
*******************************************************/

#ifndef fsp0file_h
#define fsp0file_h

#include "ha_prototypes.h"
#include "log0log.h"
#include "mem0mem.h"
#include "os0file.h"
#include <vector>

/** Types of raw partitions in innodb_data_file_path */
enum device_t {
	SRV_NOT_RAW = 0,	/*!< Not a raw partition */
	SRV_NEW_RAW,		/*!< A 'newraw' partition, only to be
				initialized */
	SRV_OLD_RAW		/*!< An initialized raw partition */
};

/** Data file control information. */
class Datafile
{
	friend class Tablespace;
	friend class SysTablespace;

public:

	Datafile()
		:
		m_name(),
		m_filepath(),
		m_filename(),
		m_handle(OS_FILE_CLOSED),
		m_open_flags(OS_FILE_OPEN),
		m_size(),
		m_order(),
		m_type(SRV_NOT_RAW),
		m_space_id(ULINT_UNDEFINED),
		m_flags(),
		m_exists(),
		m_is_valid(),
		m_first_page_buf(),
		m_first_page(),
		m_last_os_error()
	{
		/* No op */
	}

	Datafile(const char* name, ulint size, ulint order)
		:
		m_name(mem_strdup(name)),
		m_filepath(),
		m_filename(),
		m_handle(OS_FILE_CLOSED),
		m_open_flags(OS_FILE_OPEN),
		m_size(size),
		m_order(order),
		m_type(SRV_NOT_RAW),
		m_space_id(ULINT_UNDEFINED),
		m_flags(),
		m_exists(),
		m_is_valid(),
		m_first_page_buf(),
		m_first_page(),
		m_last_os_error()

	{
		ut_ad(m_name != NULL);
		/* No op */
	}

	Datafile(const Datafile& file)
		:
		m_handle(file.m_handle),
		m_open_flags(file.m_open_flags),
		m_size(file.m_size),
		m_order(file.m_order),
		m_type(file.m_type),
		m_space_id(file.m_space_id),
		m_flags(file.m_flags),
		m_exists(file.m_exists),
		m_is_valid(file.m_is_valid),
		m_first_page_buf(),
		m_first_page(),
		m_last_os_error()
	{
		m_name = mem_strdup(file.m_name);
		ut_ad(m_name != NULL);

		if (file.m_filepath != NULL) {
			m_filepath = mem_strdup(file.m_filepath);
			ut_a(m_filepath != NULL);
			set_filename();
		} else {
			m_filepath = NULL;
			m_filename = NULL;
		}
	}

	virtual ~Datafile()
	{
		shutdown();
	}

	Datafile& operator=(const Datafile& file)
	{
		ut_a(this != &file);

		ut_ad(m_name == NULL);
		m_name = mem_strdup(file.m_name);
		ut_a(m_name != NULL);

		m_size = file.m_size;
		m_order = file.m_order;
		m_type = file.m_type;

		ut_a(m_handle == OS_FILE_CLOSED);
		m_handle = file.m_handle;

		m_exists = file.m_exists;
		m_is_valid = file.m_is_valid;
		m_open_flags = file.m_open_flags;
		m_space_id = file.m_space_id;
		m_flags = file.m_flags;
		m_last_os_error = 0;

		if (m_filepath != NULL) {
			ut_free(m_filepath);
			m_filepath = NULL;
			m_filename = NULL;
		}

		if (file.m_filepath != NULL) {
			m_filepath = mem_strdup(file.m_filepath);
			ut_a(m_filepath != NULL);
			set_filename();
		}

		/* Do not make a copy of the first page,
		it should be reread if needed */
		m_first_page_buf = NULL;
		m_first_page = NULL;

		return(*this);
	}

	/** Initialize the name, size and order of this datafile
	@param[in]	name	tablespace name, will be copied
	@param[in]	size	size in database pages
	@param[in]	order	ordinal position or the datafile
	in the tablespace */
	void init(const char* name, ulint size, ulint order);

	/** Release the resources. */
	virtual void shutdown();

	/** Open a data file in read-only mode to check if it exists
	so that it can be validated.
	@param[in]	strict	whether to issue error messages
	@return DB_SUCCESS or error code */
	virtual dberr_t open_read_only(bool strict)
		__attribute__((warn_unused_result));

	/** Open a data file in read-write mode during start-up so that
	doublewrite pages can be restored and then it can be validated.
	@param[in]	read_only_mode	if true, then readonly mode checks
					are enforced.
	@return DB_SUCCESS or error code */
	virtual dberr_t open_read_write(bool read_only_mode)
		__attribute__((warn_unused_result));

	/** Close a data file.
	@return DB_SUCCESS or error code */
	dberr_t close();

	/** Make physical filename from the Tablespace::m_path
	plus the Datafile::m_name and store it in Datafile::m_filepath.
	@param path	NULL or full path for this datafile.
	@param suffix	File extension for this tablespace datafile. */
	void make_filepath(const char* path);

	/** Make physical filename from the Tablespace::m_path
	plus the Datafile::m_name and store it in Datafile::m_filepath.
	@param path	NULL or full path for this datafile.
	@param suffix	File extension for this tablespace datafile. */
	void make_filepath_no_ext(const char* path);

	/** Set the filepath by duplicating the filepath sent in */
	void set_filepath(const char* filepath);

	/** Allocate and set the datafile or tablespace name in m_name.
	If a name is provided, use it; else if the datafile is file-per-table,
	extract a file-per-table tablespace name from m_filepath; else it is a
	general tablespace, so just call it that for now. The value of m_name
	will be freed in the destructor.
	@param[in]	name	Tablespace Name if known, NULL if not */
	void set_name(const char*	name);

	/** Validates the datafile and checks that it conforms with
	the expected space ID and flags.  The file should exist and be
	successfully opened in order for this function to validate it.
	@param[in]	space_id	The expected tablespace ID.
	@param[in]	flags		The expected tablespace flags.
	@retval DB_SUCCESS if tablespace is valid, DB_ERROR if not.
	m_is_valid is also set true on success, else false. */
	dberr_t validate_to_dd(
		ulint	space_id,
		ulint	flags)
		__attribute__((warn_unused_result));

	/** Validates this datafile for the purpose of recovery.
	The file should exist and be successfully opened. We initially
	open it in read-only mode because we just want to read the SpaceID.
	However, if the first page is corrupt and needs to be restored
	from the doublewrite buffer, we will reopen it in write mode and
	ry to restore that page.
	@retval DB_SUCCESS if tablespace is valid, DB_ERROR if not.
	m_is_valid is also set true on success, else false. */
	dberr_t validate_for_recovery()
		__attribute__((warn_unused_result));

	/** Checks the consistency of the first page of a datafile when the
	tablespace is opened.  This occurs before the fil_space_t is created
	so the Space ID found here must not already be open.
	m_is_valid is set true on success, else false.
	@param[out]	flush_lsn	contents of FIL_PAGE_FILE_FLUSH_LSN
	(only valid for the first file of the system tablespace)
	@retval DB_SUCCESS on if the datafile is valid
	@retval DB_CORRUPTION if the datafile is not readable
	@retval DB_TABLESPACE_EXISTS if there is a duplicate space_id */
	dberr_t validate_first_page(lsn_t* flush_lsn = 0)
		__attribute__((warn_unused_result));

	/** Get Datafile::m_name.
	@return m_name */
	const char*	name()	const
	{
		return(m_name);
	}

	/** Get Datafile::m_filepath.
	@return m_filepath */
	const char*	filepath()	const
	{
		return(m_filepath);
	}

	/** Test if the filepath provided is the same as this object.
	When lower_case_table_names != 0 we store the filename as it is given,
	but compare it case insensitive.
	@return true if it is the same file, else false */
	bool	same_filepath_as(const char* other_filepath)
	{
		if (innobase_get_lower_case_table_names() == 0) {
			return(0 == strcmp(m_filepath, other_filepath));
		}

		return(0 == innobase_strcasecmp(m_filepath, other_filepath));
	}

	/** Get Datafile::m_handle.
	@return m_handle */
	os_file_t	handle()	const
	{
		return(m_handle);
	}

	/** Get Datafile::m_order.
	@return m_order */
	ulint	order()	const
	{
		return(m_order);
	}

	/** Get Datafile::m_space_id.
	@return m_space_id */
	ulint	space_id()	const
	{
		return(m_space_id);
	}

	/** Get Datafile::m_flags.
	@return m_flags */
	ulint	flags()	const
	{
		return(m_flags);
	}

	/**
	@return true if m_handle is open, false if not */
	bool	is_open()	const
	{
		return(m_handle != OS_FILE_CLOSED);
	}

	/** Get Datafile::m_is_valid.
	@return m_is_valid */
	bool	is_valid()	const
	{
		return(m_is_valid);
	}

	/** Get the last OS error reported
	@return m_last_os_error */
	ulint	last_os_error()		const
	{
		return(m_last_os_error);
	}

private:
	/** Free the filepath buffer. */
	void free_filepath();

	/** Set the filename pointer to the start of the file name
	in the filepath. */
	void set_filename()
	{
		if (m_filepath == NULL) {
			return;
		}

		char* last_slash = strrchr(m_filepath, OS_PATH_SEPARATOR);

		m_filename = last_slash ? last_slash + 1 : m_filepath;
	}

	/** Create/open a data file.
	@param[in]	read_only_mode	if true, then readonly mode checks
					are enforced.
	@return DB_SUCCESS or error code */
	dberr_t open_or_create(bool read_only_mode)
		__attribute__((warn_unused_result));

	/** Reads a few significant fields from the first page of the
	datafile, which must already be open.
	@param[in]	read_only_mode	if true, then readonly mode checks
					are enforced.
	@return DB_SUCCESS or DB_IO_ERROR if page cannot be read */
	dberr_t read_first_page(bool read_first_page)
		__attribute__((warn_unused_result));

	/** Free the first page from memory when it is no longer needed. */
	void free_first_page();

	/** Set the Datafile::m_open_flags.
	@param open_flags	The Open flags to set. */
	void set_open_flags(os_file_create_t	open_flags)
	{
		m_open_flags = open_flags;
	};

	/**
	@return true if it is a RAW device. */
	bool is_raw_device()
	{
		return(m_type != SRV_NOT_RAW);
	}

	/* DATA MEMBERS */

	/** Datafile name at the tablespace location.
	This is either the basename of the file if an absolute path
	was entered, or it is the relative path to the datadir or
	Tablespace::m_path. */
	char*			m_name;

protected:
	/** Physical file path with base name and extension */
	char*			m_filepath;

private:
	/** Determine the space id of the given file descriptor by reading
	a few pages from the beginning of the .ibd file.
	@return DB_SUCCESS if space id was successfully identified,
	else DB_ERROR. */
	dberr_t find_space_id();

	/** Finds a given page of the given space id from the double write
	buffer and copies it to the corresponding .ibd file.
	@param[in]	page_no		Page number to restore
	@return DB_SUCCESS if page was restored, else DB_ERROR */
	dberr_t restore_from_doublewrite(
		ulint	restore_page_no);

	/** Points into m_filepath to the file name with extension */
	char*			m_filename;

	/** Open file handle */
	os_file_t		m_handle;

	/** Flags to use for opening the data file */
	os_file_create_t	m_open_flags;

	/** size in database pages */
	ulint			m_size;

	/** ordinal position or this datafile in the tablespace */
	ulint			m_order;

	/** The type of the data file */
	device_t		m_type;

	/** Tablespace ID. Contained in the datafile header.
	If this is a system tablespace, FSP_SPACE_ID is only valid
	in the first datafile. */
	ulint			m_space_id;

	/** Tablespace flags. Contained in the datafile header.
	If this is a system tablespace, FSP_SPACE_FLAGS are only valid
	in the first datafile. */
	ulint			m_flags;

	/** true if file already existed on startup */
	bool			m_exists;

	/* true if the tablespace is valid */
	bool			m_is_valid;

	/** Buffer to hold first page */
	byte*			m_first_page_buf;

	/** Pointer to the first page held in the buffer above */
	byte*			m_first_page;

protected:
	/** Last OS error received so it can be reported if needed. */
	ulint			m_last_os_error;
};


/** Data file control information. */
class RemoteDatafile : public Datafile
{
private:
	/** Link filename (full path) */
	char*			m_link_filepath;

public:

	RemoteDatafile()
		:
		m_link_filepath()
	{
		/* No op - base constructor is called. */
	}

	RemoteDatafile(const char* name, ulint size, ulint order)
		:
		m_link_filepath()
	{
		/* No op - base constructor is called. */
	}

	~RemoteDatafile()
	{
		shutdown();
	}

	/** Release the resources. */
	void shutdown();

	/** Get the link filepath.
	@return m_link_filepath */
	const char*	link_filepath()	const
	{
		return(m_link_filepath);
	}

	/** Open a handle to the file linked to in an InnoDB Symbolic Link file
	in read-only mode so that it can be validated.
	@param[in]	strict	whether to issue error messages
	@return DB_SUCCESS or error code */
	dberr_t open_read_only(bool strict)
		__attribute__((warn_unused_result));

	/** Opens a handle to the file linked to in an InnoDB Symbolic Link
	file in read-write mode so that it can be restored from doublewrite
	and validated.
	@param[in]	read_only_mode	if true, then readonly mode checks
					are enforced.
	@return DB_SUCCESS or error code */
	dberr_t open_read_write(bool read_only_mode)
		__attribute__((warn_unused_result));

	/******************************************************************
	Global Static Functions;  Cannot refer to data members.
	******************************************************************/

	/** Creates a new InnoDB Symbolic Link (ISL) file.  It is always
	created under the 'datadir' of MySQL. The datadir is the directory
	of a running mysqld program. We can refer to it by simply using
	the path ".".
	@param[in] name Tablespace Name
	@param[in] filepath Remote filepath of tablespace datafile
	@return DB_SUCCESS or error code */
	static dberr_t create_link_file(
		const char*	name,
		const char*	filepath);

	/** Deletes an InnoDB Symbolic Link (ISL) file.
	@param[in] name Tablespace name */
	static void delete_link_file(const char* name);

	/** Reads an InnoDB Symbolic Link (ISL) file.
	It is always created under the 'datadir' of MySQL.  The name is of
	the form {databasename}/{tablename}. and the isl file is expected
	to be in a '{databasename}' directory called '{tablename}.isl'.
	The caller must free the memory of the null-terminated path returned
	if it is not null.
	@param[in] name  The tablespace name
	@param[out] link_filepath Filepath of the ISL file
	@param[out] ibd_filepath Filepath of the IBD file read from the ISL file */
	static void read_link_file(
		const char*	name,
		char**		link_filepath,
		char**		ibd_filepath);

};
#endif /* fsp0file_h */
