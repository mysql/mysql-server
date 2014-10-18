/*****************************************************************************

Copyright (c) 2013, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
@file fsp/fsp0space.cc
General shared tablespace implementation.

Created 2012-11-16 by Sunny Bains as srv/srv0space.cc
*******************************************************/

#include "ha_prototypes.h"

#include "fsp0space.h"
#include "fsp0sysspace.h"
#include "fsp0fsp.h"
#include "os0file.h"


/** Check if two tablespaces have common data file names.
@param other_space	Tablespace to check against this.
@return true if they have the same data filenames and paths */
bool
Tablespace::intersection(
	const Tablespace*	other_space)
{
	files_t::const_iterator	end = other_space->m_files.end();

	for (files_t::const_iterator it = other_space->m_files.begin();
	     it != end;
	     ++it) {

		if (find(it->m_filename)) {

			return(true);
		}
	}

	return(false);
}

/** Frees the memory allocated by the SysTablespace object. */
void
Tablespace::shutdown()
{
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {
		it->shutdown();
	}

	m_files.clear();

	m_space_id = ULINT_UNDEFINED;
}

/** Get the sum of the file sizes of each Datafile in a tablespace
@return ULINT_UNDEFINED if the size is invalid else the sum of sizes */
ulint
Tablespace::get_sum_of_sizes() const
{
	ulint	sum = 0;

	files_t::const_iterator	end = m_files.end();

	for (files_t::const_iterator it = m_files.begin(); it != end; ++it) {

#ifndef _WIN32
		if (sizeof(off_t) < 5
		    && it->m_size >= (1UL << (32UL - UNIV_PAGE_SIZE_SHIFT))) {

			ib::error() << "File size must be < 4 GB with this"
				" MySQL binary-operating system combination."
				" In some OS's < 2 GB";

			return(ULINT_UNDEFINED);
		}
#endif /* _WIN32 */
		sum += it->m_size;
	}

	return(sum);
}

/** Note that the data file was found.
@param[in,out] file	Data file object to set */
void
Tablespace::file_found(Datafile& file)
{
	/* Note that the file exists and can be opened
	in the appropriate mode. */
	file.m_exists = true;

	file.set_open_flags(
		&file == &m_files.front()
		? OS_FILE_OPEN_RETRY : OS_FILE_OPEN);
}

/** Open or Create the data files if they do not exist.
@param[in]	is_temp	whether this is a temporary tablespace
@return DB_SUCCESS or error code */
dberr_t
Tablespace::open_or_create(bool is_temp)
{
	fil_space_t*		space = NULL;
	dberr_t			err = DB_SUCCESS;

	ut_ad(!m_files.empty());

	files_t::iterator	begin = m_files.begin();
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = begin; it != end; ++it) {

		if (it->m_exists) {
			err = it->open_or_create(
				m_ignore_read_only
				? false : srv_read_only_mode);
		} else {
			err = it->open_or_create(
				m_ignore_read_only
				? false : srv_read_only_mode);

			/* Set the correct open flags now that we have
			successfully created the file. */
			if (err == DB_SUCCESS) {
				file_found(*it);
			}
		}

		if (err != DB_SUCCESS) {
			break;
		}

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
		/* Note: This should really be per node and not per
		tablespace because a tablespace can contain multiple
		files (nodes). The implication is that all files of
		the tablespace should be on the same medium. */

		if (!srv_use_doublewrite_buf) {
			fil_fusionio_enable_atomic_write(it->m_handle);
		}

#endif /* !NO_FALLOCATE && UNIV_LINUX */

		/* We can close the handle now and open the tablespace
		the proper way. */
		it->close();

		if (it == begin) {
			/* First data file. */

			ulint	flags;

			flags = fsp_flags_set_page_size(0, univ_page_size);

			/* Create the tablespace entry for the multi-file
			tablespace in the tablespace manager. */
			space = fil_space_create(
				m_name, m_space_id, flags, is_temp
				? FIL_TYPE_TEMPORARY : FIL_TYPE_TABLESPACE);
		}

		ut_a(fil_validate());

		/* Create the tablespace node entry for this data file. */
		if (!fil_node_create(
			    it->m_filepath, it->m_size, space, false)) {
		       err = DB_ERROR;
		       break;
		}
	}

	return(err);
}

/** Find a filename in the list of Datafiles for a tablespace
@return true if the filename exists in the data files */
bool
Tablespace::find(const char* filename)
{
	files_t::const_iterator	end = m_files.end();

	for (files_t::const_iterator it = m_files.begin(); it != end; ++it) {

		if (innobase_strcasecmp(filename, it->m_filename) == 0) {
			return(true);
		}
	}

	return(false);
}


/** Delete all the data files. */
void
Tablespace::delete_files()
{
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		it->close();

		bool file_pre_exists;
		bool success = os_file_delete_if_exists(
			innodb_data_file_key, it->m_filepath, &file_pre_exists);

		if (success && file_pre_exists) {
			ib::info() << "Removed temporary tablespace data"
				" file: \"" << it->m_name << "\"";
		}
	}
}

/** Check if undo tablespace.
@return true if undo tablespace */
bool
Tablespace::is_undo_tablespace(
	ulint	id)
{
	return(id <= srv_undo_tablespaces_open
	       && id != srv_sys_space.space_id()
	       && id != srv_tmp_space.space_id());
}
