/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file arch/arch0arch.cc
Common implementation for redo log and dirty page archiver system

*******************************************************/

#include "arch0arch.h"
#include "os0thread-create.h"

/** Log Archiver system global */
Arch_Log_Sys*	arch_log_sys = nullptr;

/** Page Archiver system global */
Arch_Page_Sys*	arch_page_sys = nullptr;

/** Event to signal the archiver thread. */
os_event_t	archiver_thread_event;

/** Global to indicate if the archiver task is active */
bool	archiver_is_active = false;

/** Remove page or log archived file
@param[in]	file_path	path to the file
@param[in]	file_name	name of the file */
static
void
arch_remove_file(
	const char*	file_path,
	const char*	file_name)
{
	char	path[MAX_ARCH_PAGE_FILE_NAME_LEN];

	ut_ad(MAX_ARCH_LOG_FILE_NAME_LEN <= MAX_ARCH_PAGE_FILE_NAME_LEN);
	ut_ad(strlen(file_path) + strlen(file_name)
	      < MAX_ARCH_PAGE_FILE_NAME_LEN);

	/* Remove only LOG and PAGE archival files. */
	if (0 != strncmp(file_name, ARCH_LOG_FILE, strlen(ARCH_LOG_FILE))
	    && 0 != strncmp(file_name, ARCH_PAGE_FILE,
			    strlen(ARCH_PAGE_FILE))) {

		return;
	}

	snprintf(path, sizeof(path), "%s%c%s", file_path,
		 OS_PATH_SEPARATOR, file_name);

#ifdef UNIV_DEBUG
	os_file_type_t	type;
	bool		exists = false;

	os_file_status(path, &exists, &type);
	ut_a(exists);
	ut_a(type == OS_FILE_TYPE_FILE);
#endif /* UNIV_DEBUG */

	os_file_delete(innodb_arch_file_key, path);
}

/** Remove page or log archived group directory and the files
@param[in]	dir_path	path to the directory
@param[in]	dir_name	directory name */
static
void
arch_remove_dir(
	const char*	dir_path,
	const char*	dir_name)
{
	char	path[MAX_ARCH_DIR_NAME_LEN];

	ut_ad(sizeof(ARCH_LOG_DIR) <= sizeof(ARCH_PAGE_DIR));
	ut_ad(strlen(dir_path) + strlen(dir_name)
	      < sizeof(path));

	/* Remove only LOG and PAGE archival directories. */
	if (0 != strncmp(dir_name, ARCH_LOG_DIR, strlen(ARCH_LOG_DIR))
	    && 0 != strncmp(dir_name, ARCH_PAGE_DIR, strlen(ARCH_PAGE_DIR))) {

		return;
	}

	snprintf(path, sizeof(path), "%s%c%s", dir_path,
		 OS_PATH_SEPARATOR, dir_name);

#ifdef UNIV_DEBUG
	os_file_type_t	type;
	bool		exists = false;

	os_file_status(path, &exists, &type);
	ut_a(exists);
	ut_a(type == OS_FILE_TYPE_DIR);
#endif /* UNIV_DEBUG */

	os_file_scan_directory(path, arch_remove_file, true);
}

/** Remove all page and log archived directory and files */
static
void
arch_remove_all()
{
	os_file_type_t	type;
	bool		exists = false;

	os_file_status(ARCH_DIR, &exists, &type);

	if (exists) {

		ut_ad(type == OS_FILE_TYPE_DIR);
		os_file_scan_directory(ARCH_DIR, arch_remove_dir, true);
	}
}

/** Initialize Page and Log archiver system
@return error code */
dberr_t
arch_init()
{
	if (arch_log_sys == nullptr) {

		arch_log_sys = UT_NEW(Arch_Log_Sys(), mem_key_archive);

		if (arch_log_sys == nullptr) {

			return(DB_OUT_OF_MEMORY);
		}

		arch_page_sys = UT_NEW(Arch_Page_Sys(), mem_key_archive);

		if (arch_page_sys == nullptr) {

			return(DB_OUT_OF_MEMORY);
		}

		archiver_thread_event = os_event_create(0);

		/* Remove all archived files, if there. */
		arch_remove_all();
	}

	return(DB_SUCCESS);
}

/** Free Page and Log archiver system */
void
arch_free()
{
	if (arch_log_sys != nullptr) {

		UT_DELETE(arch_log_sys);
		arch_log_sys = nullptr;

		UT_DELETE(arch_page_sys);
		arch_page_sys = nullptr;

		os_event_destroy(archiver_thread_event);

		/* Remove ARCH_DIR files, if there. */
		arch_remove_all();
	}
}

/** Archive data to one or more files.
The source is either a file context or buffer. Caller must ensure
that data is in single file in source file context.
@param[in]	from_file	file context to copy data from
@param[in]	from_buffer	buffer to copy data or NULL
@param[in]	length		size of data to copy in bytes
@return error code */
dberr_t
Arch_Group::write_to_file(
	Arch_File_Ctx*	from_file,
	byte*		from_buffer,
	uint		length)
{
	uint	write_size;
	dberr_t	err;

	if (m_file_ctx.is_closed()) {

		/* First file in the archive group. */
		ut_ad(m_file_ctx.get_file_count() == 0);

		err = m_file_ctx.open_new(m_begin_lsn, m_header_len);

		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	while (length > 0) {

		ib_uint64_t	len_copy;
		ib_uint64_t	len_left;

		len_copy = static_cast<ib_uint64_t>(length);

		len_left = m_file_ctx.bytes_left();

		/* Current file is over, switch to next file. */
		if (len_left == 0) {

			err = m_file_ctx.open_new(m_begin_lsn, m_header_len);
			if (err != DB_SUCCESS) {

				return(err);
			}

			len_left = m_file_ctx.bytes_left();
		}

		/* Write as much as possible in current file. */
		if (len_left < len_copy) {

			write_size = static_cast<uint>(len_left);
		} else {

			write_size = length;
		}

		err = m_file_ctx.write(from_file, from_buffer, write_size);

		if (err != DB_SUCCESS) {

			return(err);
		}

		ut_ad(length >= write_size);
		length -= write_size;
	}

	return(DB_SUCCESS);
}

/** Delete all files for this archive group */
void
Arch_Group::delete_files()
{
	bool	exists = false;
	char	dir_name[MAX_ARCH_DIR_NAME_LEN];

	os_file_type_t	type;

	get_dir_name(dir_name, MAX_ARCH_DIR_NAME_LEN);
	os_file_status(dir_name, &exists, &type);

	if (exists) {

		ut_ad(type == OS_FILE_TYPE_DIR);
		os_file_scan_directory(dir_name, arch_remove_file, true);
	}
}

/** Initializes archiver file context.
@param[in]	path		path to the file
@param[in]	base_dir	directory name prefix
@param[in]	base_file	file name prefix
@param[in]	num_files	initial number of files
@param[in]	file_size	file size in bytes
@return error code. */
dberr_t
Arch_File_Ctx::init(
	const char*	path,
	const char*	base_dir,
	const char*	base_file,
	uint		num_files,
	ib_uint64_t	file_size)
{
	m_base_len = static_cast<uint>(strlen(path));

	m_name_len = m_base_len
		+ static_cast<uint>(strlen(base_file))
		+ MAX_LSN_DECIMAL_DIGIT;

	if (base_dir != nullptr) {

		m_name_len += static_cast<uint>(strlen(base_dir));
		m_name_len += MAX_LSN_DECIMAL_DIGIT;
	}

	/* Add some extra buffer. */
	m_name_len += MAX_LSN_DECIMAL_DIGIT;

	m_name_buf = static_cast<char*>(ut_malloc(m_name_len,
						  mem_key_archive));

	if (m_name_buf == nullptr) {

		return(DB_OUT_OF_MEMORY);
	}

	m_path_name = path;
	m_dir_name = base_dir;
	m_file_name = base_file;

	strcpy(m_name_buf, path);

	if (m_name_buf[m_base_len - 1]  != OS_PATH_SEPARATOR) {

		m_name_buf[m_base_len] = OS_PATH_SEPARATOR;
		++m_base_len;
		m_name_buf[m_base_len] = '\0';
	}

	m_file.m_file = OS_FILE_CLOSED;

	m_index = 0;
	m_count = num_files;

	m_offset = 0;
	m_size = file_size;

	return(DB_SUCCESS);
}

/** Open a file at specific index
@param[in]	read_only	open in read only mode
@param[in]	start_lsn	start lsn for the group
@param[in]	file_index	index of the file within the group
@param[in]	file_offset	start offset
@return error code. */
dberr_t
Arch_File_Ctx::open(
	bool		read_only,
	lsn_t		start_lsn,
	uint		file_index,
	ib_uint64_t	file_offset)
{
	os_file_create_t	option;
	os_file_type_t		type;

	bool	success;
	bool	exists;

	/* Close current file, if open. */
	if (m_file.m_file != OS_FILE_CLOSED) {

		os_file_close(m_file);
	}

	m_index = file_index;
	m_offset = file_offset;

	build_name(m_index, start_lsn, nullptr, 0);

	success = os_file_status(m_name_buf, &exists, &type);

        if (!success) {

                return(DB_CANNOT_OPEN_FILE);
        }

	if (read_only) {

		option = OS_FILE_OPEN;
	} else {

		option = exists ? OS_FILE_OPEN : OS_FILE_CREATE_PATH;
	}

	m_file = os_file_create(innodb_arch_file_key, m_name_buf,
				option, OS_FILE_NORMAL, OS_CLONE_LOG_FILE,
				read_only, &success);

	if (success) {

		success = os_file_seek(m_name_buf, m_file.m_file, m_offset);
	}

	return(success ? DB_SUCCESS : DB_CANNOT_OPEN_FILE);
}

/** Add a new file and open
@param[in]	start_lsn	start lsn for the group
@param[in]	file_offset	start offset
@return error code. */
dberr_t
Arch_File_Ctx::open_new(
	lsn_t		start_lsn,
	ib_uint64_t	file_offset)
{
	dberr_t	error;

	/* Create and open next file. */
	error = open(false, start_lsn, m_count, file_offset);

	if (error != DB_SUCCESS) {

		return(error);
	}

	/* Increase file count. */
	++m_count;
	return(DB_SUCCESS);
}

/** Open next file for read
@param[in]	start_lsn	start lsn for the group
@param[in]	file_offset	start offset
@return error code. */
dberr_t
Arch_File_Ctx::open_next(
	lsn_t		start_lsn,
	ib_uint64_t	file_offset)
{
	dberr_t	error;

	/* Get next file index. */
	++m_index;
	if (m_index == m_count) {

		m_index = 0;
	}

	/* Open next file. */
	error = open(true, start_lsn, m_index, file_offset);

	if (error != DB_SUCCESS) {

		return(error);
	}

	return(DB_SUCCESS);
}

/** Write data to this file context.
Data source is another file context or buffer. If buffer is NULL,
data is copied from input file context. Caller must ensure that
the size is within the limits of current file for both source and
destination file context.
@param[in]	from_file	file context to copy data from
@param[in]	from_buffer	buffer to copy data or NULL
@param[in]	size		size of data to copy in bytes
@return error code */
dberr_t
Arch_File_Ctx::write(
	Arch_File_Ctx*	from_file,
	byte*		from_buffer,
	uint		size)
{
	dberr_t	err;

	if (from_buffer == nullptr) {
		/* write from File */
		err = os_file_copy(from_file->m_file, from_file->m_offset,
				   m_file, m_offset, size);

		if (err == DB_SUCCESS) {

			from_file->m_offset += size;
			ut_ad(from_file->m_offset <= from_file->m_size);
		}

	} else {
		/* write from buffer */
		IORequest	request(IORequest::WRITE);
		request.disable_compression();
		request.clear_encrypted();

		err = os_file_write(request, "Page Track File",
			m_file, from_buffer, m_offset, size);
	}

	if (err != DB_SUCCESS) {

		return(err);
	}

	m_offset += size;
	ut_ad(m_offset <= m_size);

	return(DB_SUCCESS);
}

/** Construct file name at specific index
@param[in]	idx	file index
@param[in]	dir_lsn	lsn of the group
@param[out]	buffer	file name including path.
			The buffer is allocated by caller.
@param[in]	length	buffer length */
void
Arch_File_Ctx::build_name(
	uint	idx,
	lsn_t	dir_lsn,
	char*	buffer,
	uint	length)
{
	char*	buf_ptr;
	uint	buf_len;

	/* If user has passed NULL, use pre-allocated buffer. */
	if (buffer == nullptr) {

		buf_ptr = m_name_buf;
		buf_len = m_name_len;
	} else {

		buf_ptr = buffer;
		buf_len = length;

		strncpy(buf_ptr, m_name_buf, buf_len);
	}

	ut_ad(buf_len > m_base_len);

	buf_ptr += m_base_len;
	buf_len -= m_base_len;

	if (m_dir_name == nullptr) {

		snprintf(buf_ptr, buf_len, "%s%u", m_file_name, idx);
	} else {

		snprintf(buf_ptr, buf_len, "%s" UINT64PF "%c%s%u",
			 m_dir_name, dir_lsn, OS_PATH_SEPARATOR, m_file_name, idx);
	}
}

/** Construct group directory name
@param[in]	dir_lsn	lsn of the group
@param[out]	buffer	directory name.
			The buffer is allocated by caller.
@param[in]	length	buffer length */
void
Arch_File_Ctx::build_dir_name(
	lsn_t	dir_lsn,
	char*	buffer,
	uint	length)
{
	snprintf(buffer, length, "%s%c%s" UINT64PF, m_path_name,
		 OS_PATH_SEPARATOR, m_dir_name, dir_lsn);
}

/** Start archiver background thread.
@return true if successful */
bool
start_archiver_background()
{
	bool	ret;
	char	errbuf[MYSYS_STRERROR_SIZE];

	ret = os_file_create_directory(ARCH_DIR, false);

	if (ret) {

		os_thread_create(archiver_thread_key, archiver_thread);
	} else {

		my_error(ER_CANT_CREATE_FILE, MYF(0), ARCH_DIR, errno,
			my_strerror(errbuf, sizeof(errbuf), errno));
	}

	return(ret);
}

/** Archiver background thread */
void
archiver_thread()
{
	Arch_File_Ctx	log_file_ctx;
	lsn_t		log_arch_lsn = LSN_MAX;

	bool		log_abort = false;
	bool		log_wait = false;
	bool		log_init = true;

	bool		page_abort = false;
	bool		page_wait = false;

	archiver_is_active = true;

	while (true) {

		if (!log_abort) {

			/* Archive available redo log data. */
			log_abort = arch_log_sys->archive(log_init,
				&log_file_ctx, &log_arch_lsn, &log_wait);

			if (log_abort) {

				ib::info() << "Exiting Log Archiver";
			}

			log_init = false;
		}

		if (!page_abort) {

			/* Archive in memory data blocks to disk. */
			page_abort = arch_page_sys->archive(&page_wait);

			if (page_abort) {

				ib::info() << "Exiting Page Archiver";
			}
		}

		if (log_abort && page_abort) {
			break;
		}

		if ((log_abort || log_wait)
		    && (page_abort || page_wait)) {

			/* Nothing to archive. Wait until next trigger. */
			os_event_wait(archiver_thread_event);
			os_event_reset(archiver_thread_event);
		}
	}

	archiver_is_active = false;
}
