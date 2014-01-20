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
Multi file, shared, system tablespace implementation.

Created 2012-11-16 by Sunny Bains as srv/srv0space.cc
Refactored 2013-7-26 by Kevin Lewis
*******************************************************/

#include "ha_prototypes.h"

#include "fsp0sysspace.h"
#include "os0file.h"
#include "srv0start.h"

/** The control info of the system tablespace. */
SysTablespace srv_sys_space;

/** The control info of a temporary table shared tablespace. */
SysTablespace srv_tmp_space;

/** If the last data file is auto-extended, we add this many pages to it
at a time. We have to make this public because it is a config variable. */
ulong sys_tablespace_auto_extend_increment;

/** Convert a numeric string that optionally ends in G or M or K,
    to a number containing megabytes.
@param[in]	str	String with a quantity in bytes
@param[out]	megs	The number in megabytes
@return next character in string */

char*
SysTablespace::parse_units(
	char*	ptr,
	ulint*	megs)
{
	char*		endp;

	*megs = strtoul(ptr, &endp, 10);

	ptr = endp;

	switch (*ptr) {
	case 'G': case 'g':
		*megs *= 1024;
		/* fall through */
	case 'M': case 'm':
		++ptr;
		break;
	case 'K': case 'k':
		*megs /= 1024;
		++ptr;
		break;
	default:
		*megs /= 1024 * 1024;
		break;
	}

	return(ptr);
}

/** Parse the input params and populate member variables.
@param[in]	filepath	path to data files
@param[in]	supports_raw	true if the tablespace supports raw devices
@return true on success parse */

bool
SysTablespace::parse(
	const char*	filepath_spec,
	bool		supports_raw)
{
	char*	filepath;
	ulint	size;
	char*	input_str;
	ulint	n_files = 0;

	ut_ad(m_last_file_size_max == 0);
	ut_ad(!m_auto_extend_last_file);

	char*	new_str = strdup(filepath_spec);
	char*	str = new_str;

	input_str = str;

	/*---------------------- PASS 1 ---------------------------*/
	/* First calculate the number of data files and check syntax:
	filepath:size[K |M | G];filepath:size[K |M | G]... .
	Note that a Windows path may contain a drive name and a ':'. */
	while (*str != '\0') {
		filepath = str;

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
			       || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == '\0') {
			::free(new_str);
			ib_logf(IB_LOG_LEVEL_ERROR,
				"syntax error in file path or size specified"
				" is less than 1 megabyte");

			return(false);
		}

		str++;

		str = parse_units(str, &size);

		if (0 == strncmp(str, ":autoextend",
				 (sizeof ":autoextend") - 1)) {

			str += (sizeof ":autoextend") - 1;

			if (0 == strncmp(str, ":max:",
					 (sizeof ":max:") - 1)) {

				str += (sizeof ":max:") - 1;

				str = parse_units(str, &size);
			}

			if (*str != '\0') {
				::free(new_str);
				ib_logf(IB_LOG_LEVEL_ERROR,
					"syntax error in file path or size specified"
					" is less than 1 megabyte");
				return(false);
			}
		}

		if (::strlen(str) >= 6
		    && *str == 'n'
		    && *(str + 1) == 'e'
		    && *(str + 2) == 'w') {

			if (!supports_raw) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"Tablespace doesn't support raw"
					" devices");

				::free(new_str);
				return(false);
			}

			str += 3;
		}

		if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {
			str += 3;

			if (!supports_raw) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"Tablespace doesn't support raw"
					" devices");

				::free(new_str);
				return(false);
			}
		}

		if (size == 0) {
			::free(new_str);
			ib_logf(IB_LOG_LEVEL_ERROR,
				"syntax error in file path or size specified"
				" is less than 1 megabyte");
			return(false);
		}

		++n_files;

		if (*str == ';') {
			str++;
		} else if (*str != '\0') {
			::free(new_str);
			return(false);
		}
	}

	if (n_files == 0) {
		/* filepath_spec must contain at least one data file definition */
		::free(new_str);
		ib_logf(IB_LOG_LEVEL_ERROR,
			"syntax error in file path or size specified"
			" is less than 1 megabyte");
		return(false);
	}

	/*---------------------- PASS 2 ---------------------------*/
	/* Then store the actual values to our arrays */
	str = input_str;
	ulint order = 0;

	while (*str != '\0') {
		filepath = str;

		/* Note that we must step over the ':' in a Windows filepath;
		a Windows path normally looks like C:\ibdata\ibdata1:1G, but
		a Windows raw partition may have a specification like
		\\.\C::1Gnewraw or \\.\PHYSICALDRIVE2:1Gnewraw */

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
			       || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == ':') {
			/* Make filepath a null-terminated string */
			*str = '\0';
			str++;
		}

		str = parse_units(str, &size);

		if (0 == strncmp(str, ":autoextend",
				 (sizeof ":autoextend") - 1)) {

			m_auto_extend_last_file = true;

			str += (sizeof ":autoextend") - 1;

			if (0 == strncmp(str, ":max:",
					 (sizeof ":max:") - 1)) {

				str += (sizeof ":max:") - 1;

				str = parse_units(str, &m_last_file_size_max);
			}

			if (*str != '\0') {
				::free(new_str);
				ib_logf(IB_LOG_LEVEL_ERROR,
					"syntax error in file path or size"
					" specified is less than 1 megabyte");
				return(false);
			}
		}

		m_files.push_back(Datafile(filepath, size, order));
		Datafile* datafile = &m_files.back();
		datafile->make_filepath_no_ext(path());

		if (::strlen(str) >= 6
		    && *str == 'n'
		    && *(str + 1) == 'e'
		    && *(str + 2) == 'w') {

			ut_a(supports_raw);

			str += 3;

			m_files.back().m_type = SRV_NEW_RAW;
		}

		if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {

			ut_a(supports_raw);

			str += 3;

			if (m_files.back().m_type == SRV_NOT_RAW) {
				m_files.back().m_type = SRV_OLD_RAW;
			}
		}

		if (*str == ';') {
			++str;
		}
		order++;
	}

	ut_ad(n_files == ulint(m_files.size()));

	::free(new_str);

	return(true);
}

/** Frees the memory allocated by the parse method. */

void
SysTablespace::shutdown()
{
	Tablespace::shutdown();

	m_auto_extend_last_file = 0;
	m_last_file_size_max = 0;
	m_created_new_raw = 0;
	m_is_tablespace_full = false;
	m_sanity_checks_done = false;
}

/** Verify the size of the physical file.
@param[in]	file	data file object
@return DB_SUCCESS if OK else error code. */

dberr_t
SysTablespace::check_size(
	Datafile&	file)
{
	os_offset_t	size = os_file_get_size(file.m_handle);
	ut_a(size != (os_offset_t) -1);

	/* Round size downward to megabytes */
	ulint	rounded_size_pages = (ulint) (size >> UNIV_PAGE_SIZE_SHIFT);

	/* If last file */
	if (&file == &m_files.back() && m_auto_extend_last_file) {

		if (file.m_size > rounded_size_pages
		    || (m_last_file_size_max > 0
			&& m_last_file_size_max < rounded_size_pages)) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"The Auto-extending %s data file '%s' is of"
				" a different size %lu pages (rounded down"
				" to MB) than specified in the .cnf file:"
				" initial %lu pages, max %lu (relevant if"
				" non-zero) pages!",
				name(),
				file.filepath(),
				rounded_size_pages,
				file.m_size,
				m_last_file_size_max);

			return(DB_ERROR);
		}

		file.m_size = rounded_size_pages;
	}

	if (rounded_size_pages != file.m_size) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"The %s data file '%s' is of a different size %lu"
			" pages (rounded down to MB) than the %lu pages"
			" specified in the .cnf file!", name(),
			file.filepath(), rounded_size_pages, file.m_size);

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/** Set the size of the file.
@param[in]	file	data file object
@return DB_SUCCESS or error code */

dberr_t
SysTablespace::set_size(
	Datafile&	file)
{
	ut_a(!srv_read_only_mode);

	/* We created the data file and now write it full of zeros */

	ib_logf(IB_LOG_LEVEL_INFO,
		"Setting file '%s' size to %lu MB."
		" Physically writing the file full; Please wait ...",
		file.filepath(), (file.m_size >> (20 - UNIV_PAGE_SIZE_SHIFT)));

	bool	success = os_file_set_size(
		file.m_filepath, file.m_handle,
		(os_offset_t) file.m_size << UNIV_PAGE_SIZE_SHIFT);

	if (success) {
		ib_logf(IB_LOG_LEVEL_INFO,
			"File '%s' size is now %lu MB.",
		file.filepath(), (file.m_size >> (20 - UNIV_PAGE_SIZE_SHIFT)));
	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Could not set the file size of '%s'."
			" Probably out of disk space", file.filepath());

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/** Create a data file.
@param[in]	file	data file object
@return DB_SUCCESS or error code */

dberr_t
SysTablespace::create_file(
	Datafile&	file)
{
	dberr_t	err = DB_SUCCESS;

	ut_a(!file.m_exists);
	ut_a(!srv_read_only_mode);

	switch (file.m_type) {
	case SRV_NEW_RAW:

		/* The partition is opened, not created; then it is
		written over */
		m_created_new_raw = true;

		/* Fall through. */

	case SRV_OLD_RAW:

		srv_start_raw_disk_in_use = TRUE;

		/* Fall through. */

	case SRV_NOT_RAW:
		err = file.open_or_create();
		break;
	}


	if (err == DB_SUCCESS && file.m_type != SRV_OLD_RAW) {
		err = set_size(file);
	}

	return(err);
}

/** Open a data file.
@param[in]	file	data file object
@return DB_SUCCESS or error code */

dberr_t
SysTablespace::open_file(
	Datafile&	file)
{
	dberr_t	err = DB_SUCCESS;

	ut_a(file.m_exists);

	switch (file.m_type) {
	case SRV_NEW_RAW:
		/* The partition is opened, not created; then it is
		written over */
		m_created_new_raw = true;

	case SRV_OLD_RAW:
		srv_start_raw_disk_in_use = TRUE;

		if (srv_read_only_mode) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Can't open a raw device '%s' when"
				" --innodb-read-only is set",
				file.m_filepath);

			return(DB_ERROR);
		}

		/* Fall through */

	case SRV_NOT_RAW:
		err = file.open_or_create();

		if (err != DB_SUCCESS) {
			return(err);
		}
		break;
	}

	if (file.m_type != SRV_OLD_RAW) {
		err = check_size(file);
		if (err != DB_SUCCESS) {
			file.close();
		}
	}

	return(err);
}

/** Read the flushed lsn values and check the header flags in each datafile
for this tablespace.  Save the minimum and maximum flushed lsn values.
@return DB_SUCCESS or error code */

dberr_t
SysTablespace::read_lsn_and_check_flags()
{
	dberr_t	err;

	/* Only relevant for the system tablespace. */
	ut_ad(space_id() == TRX_SYS_SPACE);

	files_t::iterator begin = m_files.begin();
	files_t::iterator end = m_files.end();
	for (files_t::iterator it = begin; it != end; ++it) {

		ut_a(it->m_exists);

		if (it->m_handle == OS_FILE_CLOSED) {
			err = it->open_or_create();
			if (err != DB_SUCCESS) {
				return(err);
			}
		}

		err = it->read_first_page();
		if (err != DB_SUCCESS) {
			return(err);
		}

		/* Check tablespace attributes only for first file.
		The FSP_SPACE_ID and other fields in files greater
		than ibdata1 are unreliable. */
		if (it == begin) {
			ut_a(it->order() == 0);

			set_flags(it->m_flags);

			/* Make sure the tablespace space ID matches the
			space ID on the first page of the first datafile. */
			if (space_id() != it->m_space_id) {
				ib_logf(IB_LOG_LEVEL_ERROR,
					"The %s data file '%s' has the wrong"
					" space ID. It should be %lu,"
					" but %lu was found",
					name(), it->name(),
					space_id(), it->m_space_id);
				it->close();
				return(err);
			}

			/* Check the contents of the first page of the
			first datafile */
			err = it->validate_first_page();
			if (err != DB_SUCCESS) {
				it->close();
				return(err);
			}
		}

		it->close();

		set_min_flushed_lsn(it->flushed_lsn());
		set_max_flushed_lsn(it->flushed_lsn());
	}

	return(DB_SUCCESS);
}

/** Check if a file can be opened in the correct mode.
@param[in]	file	data file object
@param[out]	reason	exact reason if file_status check failed.
@return DB_SUCCESS or error code. */

dberr_t
SysTablespace::check_file_status(
	const Datafile&		file,
	file_status_t&		reason)
{
	os_file_stat_t	stat;

	memset(&stat, 0x0, sizeof(stat));

	dberr_t	err = os_file_get_status(file.m_filepath, &stat, true);

	reason = FILE_STATUS_VOID;
	/* File exists but we can't read the rw-permission settings. */
	switch (err) {
	case DB_FAIL:
		ib_logf(IB_LOG_LEVEL_ERROR,
			"os_file_get_status() failed on '%s'."
			" Can't determine file permissions",
			file.filepath());

		err = DB_ERROR;
		reason = FILE_STATUS_RW_PERMISSION_ERROR;
		break;

	case DB_SUCCESS:

		/* Note: stat.rw_perm is only valid for "regular" files */

		if (stat.type == OS_FILE_TYPE_FILE) {

			if (!stat.rw_perm) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"The %s data file '%s' must be %s",
					name(), file.name(),
					!srv_read_only_mode
					? "writable" : "readable");

				err = DB_ERROR;
				reason = FILE_STATUS_READ_WRITE_ERROR;
			}

		} else {
			/* Not a regular file, bail out. */

			ib_logf(IB_LOG_LEVEL_ERROR,
				"The %s data file '%s' is not a regular"
				" InnoDB data file.",
				name(), file.name());

			err = DB_ERROR;
			reason = FILE_STATUS_NOT_REGULAR_FILE_ERROR;
		}
		break;

	case DB_NOT_FOUND:
		break;

	default:
		ut_ad(0);
	}

	return(err);
}

/** Note that the data file was not found.
@param[in]	file		data file object
@param[out]	create_new_db	true if a new instance to be created
@return DB_SUCESS or error code */

dberr_t
SysTablespace::file_not_found(
	Datafile&	file,
	bool*	create_new_db)
{
	file.m_exists = false;

	if (srv_read_only_mode) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Can't create file '%s' when"
			" --innodb-read-only is set",
			file.filepath());

		return(DB_ERROR);

	} else if (&file == &m_files.front()) {

		/* First data file. */
		ut_a(!*create_new_db);
		*create_new_db = TRUE;

		if (space_id() == TRX_SYS_SPACE) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"The first %s data file '%s' did not exist."
				" A new tablespace will be created!",
				name(), file.name());
		}

	} else {
		ib_logf(IB_LOG_LEVEL_INFO,
			"Need to create a new %s data file '%s'.",
			name(), file.name());
	}

	/* Set the file create mode. */
	switch (file.m_type) {
	case SRV_NOT_RAW:
		file.set_open_flags(OS_FILE_CREATE);
		break;

	case SRV_NEW_RAW:
	case SRV_OLD_RAW:
		file.set_open_flags(OS_FILE_OPEN_RAW);
		break;
	}

	return(DB_SUCCESS);
}

/** Note that the data file was found.
@param[in/out]	file	data file object */

void
SysTablespace::file_found(
	Datafile&	file)
{
	/* Note that the file exists and can be opened
	in the appropriate mode. */
	file.m_exists = true;

	/* Set the file open mode */
	switch (file.m_type) {
	case SRV_NOT_RAW:
	case SRV_NEW_RAW:
		file.set_open_flags(
			&file == &m_files.front()
			? OS_FILE_OPEN_RETRY : OS_FILE_OPEN);
		break;

	case SRV_OLD_RAW:
		file.set_open_flags(OS_FILE_OPEN_RAW);
		break;
	}
}

/** Check the data file specification.
@param[out] create_new_db	true if a new database is to be created
@param[in] min_expected_size	Minimum expected tablespace size in bytes
@return DB_SUCCESS if all OK else error code */

dberr_t
SysTablespace::check_file_spec(
	bool*	create_new_db,
	ulint	min_expected_size)
{
	*create_new_db = FALSE;

	if (m_files.size() >= 1000) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"There must be < 1000 data files in %s"
			" but %lu have been defined.",
			name(), ulong(m_files.size()));

		return(DB_ERROR);
	}

	ulint tablespace_size = get_sum_of_sizes();
	if (tablespace_size == ULINT_UNDEFINED) {
		return(DB_ERROR);
	} else if (tablespace_size
		   < min_expected_size / UNIV_PAGE_SIZE) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Tablespace size must be at least %ld MB",
			min_expected_size / (1024 * 1024));

		return(DB_ERROR);
	}

	dberr_t	err = DB_SUCCESS;

	ut_a(!m_files.empty());

	/* If there is more than one data file and the last data file
	doesn't exist, that is OK. We allow adding of new data files. */

	files_t::iterator	begin = m_files.begin();
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = begin; it != end; ++it) {

		file_status_t reason_if_failed;
		err = check_file_status(*it, reason_if_failed);

		if (err == DB_NOT_FOUND) {

			err = file_not_found(*it, create_new_db);

			if (err != DB_SUCCESS) {
				break;
			}

		} else if (err != DB_SUCCESS) {
			if (reason_if_failed == FILE_STATUS_READ_WRITE_ERROR) {
				ib_logf(IB_LOG_LEVEL_ERROR,
					"The %s data file '%s' must be %s",
					name(), it->name(), !srv_read_only_mode
					? "writable" : "readable");
			}

			ut_a(err != DB_FAIL);
			break;

		} else if (*create_new_db) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"The %s data file '%s' was not found but"
				" one of the other data files '%s' exists.",
				name(), begin->m_name, it->m_name);

			err = DB_ERROR;
			break;

		} else {
			file_found(*it);
		}
	}

	return(err);
}

/** Opens or Creates the data files if they do not exist.
@param[out] sum_new_sizes	Sum of sizes of the new files added.
@param[out] min_lsn		Minimum flushed LSN among all datafiles.
@param[out] max_lsn		Maximum flushed LSN among all datafiles.
@return DB_SUCCESS or error code */
dberr_t
SysTablespace::open_or_create(
	ulint*	sum_new_sizes,
	lsn_t*	min_lsn,
	lsn_t*	max_lsn)
{
	dberr_t		err = DB_SUCCESS;

	ut_ad(!m_files.empty());

	if (sum_new_sizes) {
		*sum_new_sizes = 0;
	}

	files_t::iterator	begin = m_files.begin();
	files_t::iterator	end = m_files.end();

	ut_ad(begin->order() == 0);
	bool create_new_db = begin->m_exists;

	for (files_t::iterator it = begin; it != end; ++it) {

		if (it->m_exists) {
			err = open_file(*it);
		} else {
			err = create_file(*it);

			if (sum_new_sizes) {
				*sum_new_sizes += it->m_size;
			}

			/* Set the correct open flags now that we have
			successfully created the file. */
			if (err == DB_SUCCESS) {
				file_found(*it);
			}
		}

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	if (create_new_db) {
		/* Validate the header page in the first datafile
		and read LSNs fom the others. */
		err = read_lsn_and_check_flags();
		if (err != DB_SUCCESS) {
			return(err);
		}
		*min_lsn = min_flushed_lsn();
		*max_lsn = max_flushed_lsn();
	}

	/* We can close the handles now and open the tablespace
	the proper way. */
	for (files_t::iterator it = begin; it != end; ++it) {
		it->close();
		it->m_exists = true;

		if (it == begin) {
			/* First data file. */

			ulint	flags;

			flags = fsp_flags_set_page_size(0, UNIV_PAGE_SIZE);

			/* Create the tablespace entry for the multi-file
			tablespace in the tablespace manager. */
			fil_space_create(
				it->m_filepath, space_id(), flags,
				FIL_TABLESPACE);
		}

		ut_a(fil_validate());

		/* Open the data file. */
		const char*	filename = fil_node_create(
			it->m_filepath, it->m_size,
			space_id(), it->m_type != SRV_NOT_RAW);

		if (filename == 0) {
		       err = DB_ERROR;
		       break;
		}
	}

	return(err);
}

/** Normalize the file size, convert from megabytes to number of pages. */

void
SysTablespace::normalize()
{
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		it->m_size *= (1024 * 1024) / UNIV_PAGE_SIZE;
	}

	m_last_file_size_max *= (1024 * 1024) / UNIV_PAGE_SIZE;
}


/**
@return next increment size */

ulint
SysTablespace::get_increment() const
{
	ulint	increment;

	if (m_last_file_size_max == 0) {
		increment = get_autoextend_increment();
	} else {

		if (!is_valid_size()) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"The last data file in %s has a size of"
				" %lu but the max size allowed is %lu",
				name(), last_file_size(),
				m_last_file_size_max);
		}

		increment = m_last_file_size_max - last_file_size();
	}

	if (increment > get_autoextend_increment()) {
		increment = get_autoextend_increment();
	}

	return(increment);
}


/**
@return true if configured to use raw devices */

bool
SysTablespace::has_raw_device()
{
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		if (it->is_raw_device()) {
			return(true);
		}
	}

	return(false);
}
