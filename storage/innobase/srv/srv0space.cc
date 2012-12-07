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
@file srv/srv0space.cc
Multi file shared tablespace implementation.

Created 2012-11-16 by Sunny Bains.
*******************************************************/

#include "srv0space.h"
#include "srv0start.h"
#include "fil0fil.h"
#include "fsp0fsp.h"

/** The control info of the system tablespace. */
UNIV_INTERN Tablespace srv_sys_space;

/** The control info of a temporary table shared tablespace. */
UNIV_INTERN Tablespace srv_tmp_space;

/**
Convert a numeric string that optionally ends in G or M, to a number
containing megabytes.
@param str - string with a quantity in bytes
@param megs - out the number in megabytes
@return next character in string */
char*
Tablespace::parse_units(char* ptr, ulint* megs)
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
	default:
		*megs /= 1024 * 1024;
		break;
	}

	return(ptr);
}

/**
Parse the input params and populate member variables.
@param filepath - path to data files
@return true on success parse */
UNIV_INTERN
bool
Tablespace::parse(const char* filepath, bool supports_raw)
{
	char*	path;
	ulint	size;
	char*	input_str;
	ulint	n_files = 0;

	ut_ad(m_last_file_size_max == 0);
	ut_ad(m_auto_extend_last_file == false);

	char*	new_str = strdup(filepath);
	char*	str = new_str;

	input_str = str;

	/*---------------------- PASS 1 ---------------------------*/
	/* First calculate the number of data files and check syntax:
	path:size[M | G];path:size[M | G]... . Note that a Windows path may
	contain a drive name and a ':'. */
	while (*str != '\0') {
		path = str;

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
			       || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == '\0') {
			::free(new_str);
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
				return(false);
			}
		}

		if (strlen(str) >= 6
		    && *str == 'n'
		    && *(str + 1) == 'e'
		    && *(str + 2) == 'w') {

			if (!supports_raw) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"Tablespace doesn't support raw "
					"devices");

				::free(new_str);
				return(false);
			}

			str += 3;
		}

		if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {
			str += 3;

			if (!supports_raw) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"Tablespace doesn't support raw "
					"devices");

				::free(new_str);
				return(false);
			}
		}

		if (size == 0) {
			::free(new_str);
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
		/* file_path must contain at least one data file definition */
		::free(new_str);
		return(false);
	}

	/*---------------------- PASS 2 ---------------------------*/
	/* Then store the actual values to our arrays */
	str = input_str;

	while (*str != '\0') {
		path = str;

		/* Note that we must step over the ':' in a Windows path;
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
			/* Make path a null-terminated string */
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
				return(false);
			}
		}

		m_files.push_back(file_t(path, size));

		if (strlen(str) >= 6
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
	}

	ut_ad(n_files == m_files.size());

	::free(new_str);

	return(true);
}

/**
Check if two shared tablespaces have common data file names.
@param space1 - space to check
@param space2 - space to check
@return true if they have the same data filenames and paths */
bool
Tablespace::intersection(const Tablespace& space1, const Tablespace& space2)
{
	files_t::const_iterator	end = space1.m_files.end();

	for (files_t::const_iterator it = space1.m_files.begin();
	     it != end;
	     ++it) {

		if (space2.find(get_file_name(it->m_name))) {

			return(true);
		}
	}

	return(false);
}

/**
Get the file name only
@param filepath - filepath as specified by user (can be relative too).
@return filename extract filepath */
char*
Tablespace::get_file_name(const char* filepath)
{
        char* last_slash = strrchr((char*) filepath, OS_FILE_PATH_SEPARATOR);
        return(last_slash ? last_slash + 1 : (char*) filepath);
}

/**
Frees the memory allocated by the parse method. */
void
Tablespace::shutdown()
{
	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {
		it->shutdown();
	}

	m_files.clear();

	m_space_id = ULINT_UNDEFINED;

	m_created_new_raw = 0;
	m_last_file_size_max = 0;
	m_auto_extend_last_file = 0;
	m_auto_extend_increment = 0;
}

/**
@return ULINT_UNDEFINED if the size is invalid else the sum of sizes */
ulint
Tablespace::get_sum_of_sizes() const
{
	ulint	sum = 0;

	files_t::const_iterator	end = m_files.end();

	for (files_t::const_iterator it = m_files.begin(); it != end; ++it) {

#ifndef __WIN__
		if (sizeof(off_t) < 5
		    && it->m_size >= (1UL << (32UL - UNIV_PAGE_SIZE_SHIFT))) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"File size must be < 4 GB "
				"with this MySQL binary.");

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Operating system combination, in some "
				"OS's < 2 GB");

			return(ULINT_UNDEFINED);
		}
#endif /* __WIN__ */
		sum += it->m_size;
	}

	return(sum);
}

/**
Create/open a data file.
@param file - data file spec
@return DB_SUCCESS or error code */
dberr_t
Tablespace::open_data_file(file_t& file)
{
	ibool	success;

	file.m_handle = os_file_create(
		innodb_file_data_key, file.m_filename, file.m_open_flags,
		OS_FILE_NORMAL, OS_DATA_FILE, &success);

	if (!success) {

		os_file_get_last_error(true);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Can't open \"%s\"", file.m_filename);

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/**
Verify the size of the physical file.
@param file - data file spec
@return DB_SUCCESS if OK else error code. */
dberr_t
Tablespace::check_size(file_t& file)
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
				"auto-extending data file %s is of a "
				"different size %lu pages (rounded "
				"down to MB) than specified in the .cnf "
				"file: initial %lu pages, max %lu "
				"(relevant if non-zero) pages!",
				file.m_filename,
				rounded_size_pages,
				file.m_size,
				m_last_file_size_max);

			return(DB_ERROR);
		}

		file.m_size = rounded_size_pages;
	}

	if (rounded_size_pages != file.m_size) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Data file %s is of a different "
			"size %lu pages (rounded down to MB) "
			"than specified in the .cnf file "
			"%lu pages!",
			file.m_filename, rounded_size_pages, file.m_size);

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/**
Make physical filename from control info.
@param file - data file spec */
void
Tablespace::make_name(file_t& file)
{
	char	name[OS_FILE_MAX_PATH];

	if (file.m_filename != 0) {
		::free(file.m_filename);
		file.m_filename = 0;
	}

	ut_a(file.m_filename == 0);

	ulint	dirnamelen = strlen(srv_data_home);

	ut_a(dirnamelen + strlen(file.m_name) < sizeof(name) - 1);

	memcpy(name, srv_data_home, dirnamelen);

	/* Add a path separator if needed. */
	if (dirnamelen && name[dirnamelen - 1] != SRV_PATH_SEPARATOR) {
		name[dirnamelen++] = SRV_PATH_SEPARATOR;
	}

	strcpy(name + dirnamelen, file.m_name);

	srv_normalize_path_for_win(name);

	file.m_filename = strdup(name);
}

/**
Set the size of the file.
@param file - data file spec
@return DB_SUCCESS or error code */
dberr_t
Tablespace::set_size(file_t& file)
{
	ut_a(!srv_read_only_mode);

	/* We created the data file and now write it full of zeros */

	ib_logf(IB_LOG_LEVEL_INFO,
		"Setting file \"%s\" size to %lu MB",
		file.m_filename, (file.m_size >> (20 - UNIV_PAGE_SIZE_SHIFT)));

	ib_logf(IB_LOG_LEVEL_INFO,
		"Database physically writes the file full: wait ...");

	ibool	success = os_file_set_size(
		file.m_filename, file.m_handle,
		(os_offset_t) file.m_size << UNIV_PAGE_SIZE_SHIFT);

	if (!success) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"During create of \"%s\": probably out of "
			"disk space", file.m_filename);

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/**
Create a data file.
@param file - data file spec
@return DB_SUCCESS or error code */
dberr_t
Tablespace::create_file(file_t& file)
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
		err = open_data_file(file);
		break;
	}


	if (err == DB_SUCCESS && file.m_type != SRV_OLD_RAW) {
		err = set_size(file);
	}

	return(err);
}

/**
Open a data file.
@param file - data file spec
@return DB_SUCCESS or error code */
dberr_t
Tablespace::open_file(file_t& file)
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
				"Can't open a raw device \"%s\" when "
				"--innodb-read-only is set",
				file.m_filename);

			return(DB_ERROR);
		}

		/* Fall through */

	case SRV_NOT_RAW:
		err = open_data_file(file);

		if (err != DB_SUCCESS) {
			return(err);
		}
		break;
	}

	if (file.m_type != SRV_OLD_RAW) {
		err = check_size(file);
	}

	return(err);
}

/**
Read the flush lsn values and check the header flags.

@param min_flushed_lsn - min of flushed lsn values in data files
@param max_flushed_lsn - max of flushed lsn values in data files
@return DB_SUCCESS or error code */
dberr_t
Tablespace::read_lsn_and_check_flags(
	lsn_t*		min_flushed_lsn,
	lsn_t*		max_flushed_lsn)
{
	/* Only relevant for the system tablespace. */
	ut_ad(m_space_id == TRX_SYS_SPACE);

	*max_flushed_lsn = 0;
	*min_flushed_lsn = LSN_MAX;

	files_t::iterator	end				= m_files.end();
	bool			check_tablespace_attributes	= true;
	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		ulint	flags;
		ulint	space;

		ut_a(it->m_exists);
		ut_a(it->m_handle == os_file_t(~0));

		dberr_t	err = open_data_file(*it);

		if (err != DB_SUCCESS) {
			return(DB_ERROR);
		}

		fil_read_first_page(
			it->m_handle, &flags, &space,
			min_flushed_lsn, max_flushed_lsn);

		ibool	success = os_file_close(it->m_handle);
		ut_a(success);

		it->m_handle = os_file_t(~0);

		/* The first file of the system tablespace must have space
		ID = TRX_SYS_SPACE.  The FSP_SPACE_ID field in files greater
		than ibdata1 are unreliable. */

		ut_a(!check_tablespace_attributes || space == TRX_SYS_SPACE);

		/* Check the flags for the first system tablespace file only. */
		if (check_tablespace_attributes
		    && UNIV_PAGE_SIZE != fsp_flags_get_page_size(flags)) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Data file \"%s\" uses page size %lu, "
				"but the start-up parameter is "
				"--innodb-page-size=%lu",
				it->m_filename,
				fsp_flags_get_page_size(flags),
				UNIV_PAGE_SIZE);

			return(DB_ERROR);
		}

		/* Check tablespace attributes only for first file. */
		check_tablespace_attributes = false;
	}

	return(DB_SUCCESS);
}

/**
Check if a file can be opened in the correct mode.
@param file - data file spec
@return DB_SUCCESS or error code. */
dberr_t
Tablespace::check_file_status(const file_t& file)
{
	os_file_stat_t	stat;

	memset(&stat, 0x0, sizeof(stat));

	dberr_t	err = os_file_get_status(file.m_filename, &stat, true);

	/* File exists but we can't read the rw-permission settings. */
	switch (err) {
	case DB_FAIL:
		ib_logf(IB_LOG_LEVEL_ERROR,
			"os_file_get_status() failed on \"%s\". "
			"Can't determine file permissions",
			file.m_filename);

		err = DB_ERROR;
		break;

	case DB_SUCCESS:

		/* Note: stat.rw_perm is only valid for "regular" files */

		if (stat.type == OS_FILE_TYPE_FILE) {

			if (!stat.rw_perm) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"The system tablespace must be %s",
					!srv_read_only_mode
					? "writable" : "readable");

				err = DB_ERROR;
			}

		} else {
			/* Not a regular file, bail out. */

			ib_logf(IB_LOG_LEVEL_ERROR,
				"\"%s\" not a regular file.",
				file.m_filename);

			err = DB_ERROR;
		}
		break;

	case DB_NOT_FOUND:
		break;

	default:
		ut_ad(0);
	}

	return(err);
}

/**
Note that the data file was not found.
@param file - data file spec
@param create_new_db - [out] true if a new instance to be created
@return DB_SUCESS or error code */
dberr_t
Tablespace::file_not_found(file_t& file, ibool* create_new_db)
{
	file.m_exists = false;

	if (srv_read_only_mode) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Can't create file \"%s\" when "
			"--innodb-read-only is set",
			file.m_name);

		return(DB_ERROR);

	} else if (&file == &m_files.front()) {

		/* First data file. */
		ut_a(!*create_new_db);
		*create_new_db = TRUE;

		ib_logf(IB_LOG_LEVEL_INFO,
			"The first specified data file \"%s\" "
			"did not exist%s",
			file.m_name,
			(m_space_id == TRX_SYS_SPACE)
			? " : a new database to be created!"
			: "");

	} else if (&file == &m_files.back()) {

		/* Last data file. */
		ib_logf(IB_LOG_LEVEL_INFO,
			"Data file \"%s\" did not exist: new "
			"to be created", file.m_name);

	} else if (*create_new_db) {

		/* Other data files. */
		ib_logf(IB_LOG_LEVEL_ERROR,
			"You can add a new data file at the "
			"end but not in the middle. Data file "
			"\"%s\" not found.",
			file.m_name);

			return(DB_ERROR);
	} else {
		ib_logf(IB_LOG_LEVEL_INFO,
			"Need to create new data file \"%s\"",
			file.m_name);
	}

	/* Set the file create mode. */
	switch(file.m_type) {
	case SRV_NOT_RAW:
		file.m_open_flags = OS_FILE_CREATE;
		break;

	case SRV_NEW_RAW:
	case SRV_OLD_RAW:
		file.m_open_flags = OS_FILE_OPEN_RAW;
		break;
	}

	return(DB_SUCCESS);
}

/**
Note that the data file was found.
@param file - data file spec */
void
Tablespace::file_found(file_t& file)
{
	/* Note that the file exists and can be opened
	in the appropriate mode. */
	file.m_exists = true;

	/* Set the file open mode */
	switch(file.m_type) {
	case SRV_NOT_RAW:
	case SRV_NEW_RAW:
		file.m_open_flags =
			(&file == &m_files.front())
			? OS_FILE_OPEN_RETRY : OS_FILE_OPEN;
		break;

	case SRV_OLD_RAW:
		file.m_open_flags = OS_FILE_OPEN_RAW;
		break;
	}
}

/**
Check the data file specification.
@param create_new_db - [out] true if a new database is to be created
@param min_expected_tablespace_size - [in] expected tablespace size in bytes
@return DB_SUCCESS if all OK else error code */
dberr_t
Tablespace::check_file_spec(
	ibool*	create_new_db,
	ulint	min_expected_tablespace_size)
{
	srv_normalize_path_for_win(srv_data_home);

	*create_new_db = FALSE;

	if (m_files.size() >= 1000) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Can only have < 1000 data files, you have "
			"defined %lu",
                        (ulint) m_files.size());

		return(DB_ERROR);
	}

	ulint tablespace_size = get_sum_of_sizes();
	if (tablespace_size == ULINT_UNDEFINED) {
		return(DB_ERROR);
	} else if (tablespace_size
		   < min_expected_tablespace_size / UNIV_PAGE_SIZE) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Tablespace size must be at least %ld MB",
			min_expected_tablespace_size / (1024 * 1024));

		return(DB_ERROR);
        }

	dberr_t	err = DB_SUCCESS;

	ut_a(!m_files.empty());

	/* If there is more than one data file and the last data file
	doesn't exist, that is OK. We allow adding of new data files. */

	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		make_name(*it);

		err = check_file_status(*it);

		if (err == DB_NOT_FOUND) {

			err = file_not_found(*it, create_new_db);

			if (err != DB_SUCCESS) {
				break;
			}

		} else if (err != DB_SUCCESS) {

			ut_a(err != DB_FAIL);
			break;

		} else if (*create_new_db) {

			ut_ad(it->m_exists);

			ib_logf(IB_LOG_LEVEL_ERROR,
				"First data file \"%s\" of tablespace not "
				"found but one of the other data files \"%s\" "
				"exists.",
				it->m_name, it->m_name);

			err = DB_ERROR;
			break;

		} else {
			file_found(*it);
		}
	}

	return(err);
}

/**
Opens/Creates the data files if they don't exist.

@param sum_of_new_sizes - sum of sizes of the new files added
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
Tablespace::open(ulint* sum_of_new_sizes)
{
	dberr_t		err = DB_SUCCESS;

	ut_ad(!m_files.empty());

	if (sum_of_new_sizes) {
		*sum_of_new_sizes = 0;
	}

	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		if (it->m_exists) {
			err = open_file(*it);
		} else {
			err = create_file(*it);

			if (sum_of_new_sizes) {
				*sum_of_new_sizes += it->m_size;
			}

			/* Set the correct open flags now that we have
			successfully created the file. */
			if (err == DB_SUCCESS) {
				file_found(*it);
			}
		}

		if (err != DB_SUCCESS) {
			break;
		}

		/* We can close the handle now and open the tablespace
		the proper way. */
		ibool	success = os_file_close(it->m_handle);
		ut_a(success);

		it->m_handle = os_file_t(~0);
		it->m_exists = true;

		if (it == m_files.begin()) {
			/* First data file. */

			ulint	flags;

			flags = fsp_flags_set_page_size(0, UNIV_PAGE_SIZE);

			/* Create the tablespace entry for the multi-file
			tablespace in the tablespace manager. */
			fil_space_create(
				it->m_filename, m_space_id, flags,
				FIL_TABLESPACE);
		}

		ut_a(fil_validate());

		/* Open the data file. */
		const char*	filename = fil_node_create(
			it->m_filename, it->m_size,
			m_space_id, it->m_type != SRV_NOT_RAW);

		if (filename == 0) {
		       err = DB_ERROR;
		       break;
		}
	}

	return(err);
}

/**
Normalize the file size, convert to extents. */
void
Tablespace::normalize()
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
Tablespace::get_increment() const
{
	ulint	increment;

	if (m_last_file_size_max == 0) {
		increment = get_autoextend_increment();
	} else {

		if (!is_valid_size()) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Last data file size is %lu, max "
				"size allowed %lu",
				last_file_size(),
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
Tablespace::has_raw_device() const
{
	files_t::const_iterator	end = m_files.end();

	for (files_t::const_iterator it = m_files.begin(); it != end; ++it) {

		if (is_raw_device(*it)) {
			return(true);
		}
	}

	return(false);
}

/**
@return true if the filename exists in the data files */
bool
Tablespace::find(const char* filename) const
{
	files_t::const_iterator	end = m_files.end();

	for (files_t::const_iterator it = m_files.begin(); it != end; ++it) {

		if (innobase_strcasecmp(
			filename, get_file_name(it->m_name)) == 0) {
			return(true);
		}
	}

	return(false);
}


/**
Delete all the data files. */
void
Tablespace::delete_files()
{
	srv_normalize_path_for_win(srv_data_home);

	files_t::iterator	end = m_files.end();

	for (files_t::iterator it = m_files.begin(); it != end; ++it) {

		make_name(*it);

		if (os_file_delete_if_exists(it->m_filename)) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Removed temporary tablespace data file: "
				"\"%s\"", it->m_name);
		}
	}
}

/** Check if system-tablespace (shared + temp).
@return true if system tablespace */
bool
Tablespace::is_system_tablespace(ulint id)
{
	return(id == srv_sys_space.space_id()
	       || id == srv_tmp_space.space_id());
}

