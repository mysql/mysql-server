/*****************************************************************************

Copyright (c) 2013, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/fsp0space.h
General shared tablespace implementation.

Created 2013-7-26 by Kevin Lewis
*******************************************************/

#ifndef fsp0space_h
#define fsp0space_h

#include "univ.i"
#include "fsp0file.h"
#include "fsp0fsp.h"
#include "fsp0types.h"
#include "ut0new.h"

#include <vector>

/** Data structure that contains the information about shared tablespaces.
Currently this can be the system tablespace or a temporary table tablespace */
class Tablespace {

public:
	typedef std::vector<Datafile, ut_allocator<Datafile> >	files_t;

	/** Data file information - each Datafile can be accessed globally */
	files_t		m_files;

	Tablespace()
		:
		m_files(),
		m_name(),
		m_space_id(ULINT_UNDEFINED),
		m_path(),
		m_flags(),
		m_ignore_read_only(false)
	{
		/* No op */
	}

	virtual ~Tablespace()
	{
		shutdown();
		ut_ad(m_files.empty());
		ut_ad(m_space_id == ULINT_UNDEFINED);
		if (m_name != NULL) {
			ut_free(m_name);
			m_name = NULL;
		}
		if (m_path != NULL) {
			ut_free(m_path);
			m_path = NULL;
		}
	}

	// Disable copying
	Tablespace(const Tablespace&);
	Tablespace& operator=(const Tablespace&);

	/** Set tablespace name
	@param[in]	name	tablespace name */
	void set_name(const char* name)
	{
		ut_ad(m_name == NULL);
		m_name = mem_strdup(name);
		ut_ad(m_name != NULL);
	}

	/** Get tablespace name
	@return tablespace name */
	const char* name()	const
	{
		return(m_name);
	}

	/** Set tablespace path and filename members.
	@param[in]	path	where tablespace file(s) resides
	@param[in]	len	length of the file path */
	void set_path(const char* path, size_t len)
	{
		ut_ad(m_path == NULL);
		m_path = mem_strdupl(path, len);
		ut_ad(m_path != NULL);

		os_normalize_path(m_path);
	}

	/** Set tablespace path and filename members.
	@param[in]	path	where tablespace file(s) resides */
	void set_path(const char* path)
	{
		set_path(path, strlen(path));
	}

	/** Get tablespace path
	@return tablespace path */
	const char* path()	const
	{
		return(m_path);
	}

	/** Set the space id of the tablespace
	@param[in]	space_id	 tablespace ID to set */
	void set_space_id(ulint space_id)
	{
		ut_ad(m_space_id == ULINT_UNDEFINED);
		m_space_id = space_id;
	}

	/** Get the space id of the tablespace
	@return m_space_id space id of the tablespace */
	ulint space_id()	const
	{
		return(m_space_id);
	}

	/** Set the tablespace flags
	@param[in]	fsp_flags	tablespace flags */
	void set_flags(ulint fsp_flags)
	{
		ut_ad(fsp_flags_is_valid(fsp_flags));
		m_flags = fsp_flags;
	}

	/** Get the tablespace flags
	@return m_flags tablespace flags */
	ulint flags()	const
	{
		return(m_flags);
	}

	/** Set Ignore Read Only Status for tablespace.
	@param[in]	read_only_status	read only status indicator */
	void set_ignore_read_only(bool read_only_status)
	{
		m_ignore_read_only = read_only_status;
	}

	/** Free the memory allocated by the Tablespace object */
	void shutdown();

	/** @return the sum of the file sizes of each Datafile */
	ulint get_sum_of_sizes() const
	{
		ulint	sum = 0;

		for (files_t::const_iterator it = m_files.begin();
		     it != m_files.end(); ++it) {
			sum += it->m_size;
		}

		return(sum);
	}

	/** Open or Create the data files if they do not exist.
	@param[in]	is_temp	whether this is a temporary tablespace
	@return DB_SUCCESS or error code */
	dberr_t open_or_create(bool is_temp)
		MY_ATTRIBUTE((warn_unused_result));

	/** Delete all the data files. */
	void delete_files();

	/** Check if two tablespaces have common data file names.
	@param[in]	other_space	Tablespace to check against this.
	@return true if they have the same data filenames and paths */
	bool intersection(const Tablespace* other_space);

	/** Use the ADD DATAFILE path to create a Datafile object and add
	it to the front of m_files. Parse the datafile path into a path
	and a basename with extension 'ibd'. This datafile_path provided
	may be an absolute or relative path, but it must end with the
	extension .ibd and have a basename of at least 1 byte.

	Set tablespace m_path member and add a Datafile with the filename.
	@param[in]	datafile_path	full path of the tablespace file. */
	dberr_t add_datafile(
		const char*	datafile_path);

	/* Return a pointer to the first Datafile for this Tablespace
	@return pointer to the first Datafile for this Tablespace*/
	Datafile* first_datafile()
	{
		ut_a(!m_files.empty());
		return(&m_files.front());
	}

	/** Check if undo tablespace.
	@return true if undo tablespace */
	static bool is_undo_tablespace(ulint id);
private:
	/**
	@param[in]	filename	Name to lookup in the data files.
	@return true if the filename exists in the data files */
	bool find(const char* filename);

	/** Note that the data file was found.
	@param[in]	file	data file object */
	void file_found(Datafile& file);

	/* DATA MEMBERS */

	/** Name of the tablespace. */
	char*		m_name;

	/** Tablespace ID */
	ulint		m_space_id;

	/** Path where tablespace files will reside, not including a filename.*/
	char*		m_path;

	/** Tablespace flags */
	ulint		m_flags;

protected:
	/** Ignore server read only configuration for this tablespace. */
	bool		m_ignore_read_only;
};

#endif /* fsp0space_h */
