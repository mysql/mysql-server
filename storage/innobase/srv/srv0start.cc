/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All rights reserved.
Copyright (c) 2008, Google Inc.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

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

/********************************************************************//**
@file srv/srv0start.cc
Starts the InnoDB database server

Created 2/16/1996 Heikki Tuuri
*************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "btr0btr.h"
#include "btr0cur.h"
#include "buf0buf.h"
#include "buf0dump.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "log0log.h"
#include "log0recv.h"
#include "mem0mem.h"
#include "mtr0mtr.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_stage.h"
#include "mysqld.h"
#include "os0file.h"
#include "os0thread-create.h"
#include "os0thread.h"
#include "page0cur.h"
#include "page0page.h"
#include "rem0rec.h"
#include "row0ftsort.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "ut0mem.h"
#ifndef UNIV_HOTBACKUP
# include "btr0pcur.h"
# include "btr0sea.h"
# include "buf0flu.h"
# include "buf0rea.h"
# include "dict0boot.h"
# include "dict0crea.h"
# include "dict0load.h"
# include "dict0stats_bg.h"
# include "lock0lock.h"
# include "lock0lock.h"
# include "os0event.h"
# include "os0proc.h"
# include "pars0pars.h"
# include "que0que.h"
# include "rem0cmp.h"
# include "row0ins.h"
# include "row0mysql.h"
# include "row0row.h"
# include "row0sel.h"
# include "row0upd.h"
# include "trx0purge.h"
# include "trx0roll.h"
# include "trx0rseg.h"
# include "usr0sess.h"
# include "ut0crc32.h"
# include "ut0new.h"
# include "zlib.h"

#ifdef HAVE_LZO1X
#include <lzo/lzo1x.h>

extern bool srv_lzo_disabled;
#endif /* HAVE_LZO1X */

/** Recovered persistent metadata */
static MetadataRecover* srv_dict_metadata;

/** Log sequence number immediately after startup */
lsn_t	srv_start_lsn;
/** Log sequence number at shutdown */
lsn_t	srv_shutdown_lsn;

/** TRUE if a raw partition is in use */
ibool	srv_start_raw_disk_in_use = FALSE;

/** Number of IO threads to use */
ulint	srv_n_file_io_threads = 0;

/** true if the server is being started */
bool	srv_is_being_started = false;
/** true if SYS_TABLESPACES is available for lookups */
bool	srv_sys_tablespaces_open = false;
/** true if the server is being started, before rolling back any
incomplete transactions */
bool	srv_startup_is_before_trx_rollback_phase = false;
#ifdef UNIV_DEBUG
/** true if srv_pre_dd_shutdown() has been completed */
bool	srv_is_being_shutdown = false;
#endif /* UNIV_DEBUG */
/** true if srv_start() has been called */
static bool	srv_start_has_been_called = false;

/** Bit flags for tracking background thread creation. They are used to
determine which threads need to be stopped if we need to abort during
the initialisation step. */
enum srv_start_state_t {
	SRV_START_STATE_NONE = 0,		/*!< No thread started */
	SRV_START_STATE_LOCK_SYS = 1,		/*!< Started lock-timeout
						thread. */
	SRV_START_STATE_IO = 2,			/*!< Started IO threads */
	SRV_START_STATE_MONITOR = 4,		/*!< Started montior thread */
	SRV_START_STATE_MASTER = 8,		/*!< Started master threadd. */
	SRV_START_STATE_PURGE = 16,		/*!< Started purge thread(s) */
	SRV_START_STATE_STAT = 32		/*!< Started bufdump + dict stat
						and FTS optimize thread. */
};

/** Track server thrd starting phases */
static ulint	srv_start_state;

/** At a shutdown this value climbs from SRV_SHUTDOWN_NONE to
SRV_SHUTDOWN_CLEANUP and then to SRV_SHUTDOWN_LAST_PHASE, and so on */
enum srv_shutdown_t	srv_shutdown_state = SRV_SHUTDOWN_NONE;

/** Files comprising the system tablespace */
static pfs_os_file_t	files[1000];

/** Name of srv_monitor_file */
static char*	srv_monitor_file_name;
#endif /* !UNIV_HOTBACKUP */

/** */
#define SRV_MAX_N_PENDING_SYNC_IOS	100

/* Keys to register InnoDB threads with performance schema */
#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t	buf_dump_thread_key;
mysql_pfs_key_t	buf_resize_thread_key;
mysql_pfs_key_t	dict_stats_thread_key;
mysql_pfs_key_t	fts_optimize_thread_key;
mysql_pfs_key_t	fts_parallel_merge_thread_key;
mysql_pfs_key_t	fts_parallel_tokenization_thread_key;
mysql_pfs_key_t	io_handler_thread_key;
mysql_pfs_key_t	io_ibuf_thread_key;
mysql_pfs_key_t	io_log_thread_key;
mysql_pfs_key_t	io_read_thread_key;
mysql_pfs_key_t	io_write_thread_key;
mysql_pfs_key_t	srv_error_monitor_thread_key;
mysql_pfs_key_t	srv_lock_timeout_thread_key;
mysql_pfs_key_t	srv_master_thread_key;
mysql_pfs_key_t	srv_monitor_thread_key;
mysql_pfs_key_t	srv_purge_thread_key;
mysql_pfs_key_t	srv_worker_thread_key;
mysql_pfs_key_t	trx_recovery_rollback_thread_key;
#endif /* UNIV_PFS_THREAD */

#ifdef HAVE_PSI_STAGE_INTERFACE
/** Array of all InnoDB stage events for monitoring activities via
performance schema. */
static PSI_stage_info*	srv_stages[] =
{
	&srv_stage_alter_table_end,
	&srv_stage_alter_table_flush,
	&srv_stage_alter_table_insert,
	&srv_stage_alter_table_log_index,
	&srv_stage_alter_table_log_table,
	&srv_stage_alter_table_merge_sort,
	&srv_stage_alter_table_read_pk_internal_sort,
	&srv_stage_buffer_pool_load,
};
#endif /* HAVE_PSI_STAGE_INTERFACE */

/*********************************************************************//**
Check if a file can be opened in read-write mode.
@return true if it doesn't exist or can be opened in rw mode. */
static
bool
srv_file_check_mode(
/*================*/
	const char*	name)		/*!< in: filename to check */
{
	os_file_stat_t	stat;

	memset(&stat, 0x0, sizeof(stat));

	dberr_t		err = os_file_get_status(
		name, &stat, true, srv_read_only_mode);

	if (err == DB_FAIL) {
		ib::error() << "os_file_get_status() failed on '" << name
			<< "'. Can't determine file permissions.";
		return(false);

	} else if (err == DB_SUCCESS) {

		/* Note: stat.rw_perm is only valid on files */

		if (stat.type == OS_FILE_TYPE_FILE) {

			if (!stat.rw_perm) {
				const char*	mode = srv_read_only_mode
					? "read" : "read-write";
				ib::error() << name << " can't be opened in "
					<< mode << " mode.";
				return(false);
			}
		} else {
			/* Not a regular file, bail out. */
			ib::error() << "'" << name << "' not a regular file.";

			return(false);
		}
	} else {

		/* This is OK. If the file create fails on RO media, there
		is nothing we can do. */

		ut_a(err == DB_NOT_FOUND);
	}

	return(true);
}

#ifndef UNIV_HOTBACKUP
/** I/o-handler thread function.
@param[in]      segment         The AIO segment the thread will work on */
static
void
io_handler_thread(ulint segment)
{
	while (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS
	       || buf_page_cleaner_is_active
	       || !os_aio_all_slots_free()) {
		fil_aio_wait(segment);
	}
}
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Creates a log file.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
create_log_file(
/*============*/
	pfs_os_file_t*	file,	/*!< out: file handle */
	const char*	name)	/*!< in: log file name */
{
	bool		ret;

	*file = os_file_create(
		innodb_log_file_key, name,
		OS_FILE_CREATE|OS_FILE_ON_ERROR_NO_EXIT, OS_FILE_NORMAL,
		OS_LOG_FILE, srv_read_only_mode, &ret);

	if (!ret) {
		ib::error() << "Cannot create " << name;
		return(DB_ERROR);
	}

	ib::info() << "Setting log file " << name << " size to "
		<< (srv_log_file_size >> (20 - UNIV_PAGE_SIZE_SHIFT))
		<< " MB";

	ret = os_file_set_size(name, *file,
			       (os_offset_t) srv_log_file_size
			       << UNIV_PAGE_SIZE_SHIFT,
			       srv_read_only_mode);
	if (!ret) {
		ib::error() << "Cannot set log file " << name << " to size "
			<< (srv_log_file_size >> (20 - UNIV_PAGE_SIZE_SHIFT))
			<< " MB";
		return(DB_ERROR);
	}

	ret = os_file_close(*file);
	ut_a(ret);

	return(DB_SUCCESS);
}

/** Initial number of the first redo log file */
#define INIT_LOG_FILE0	(SRV_N_LOG_FILES_MAX + 1)

/*********************************************************************//**
Creates all log files.
@return DB_SUCCESS or error code */
static
dberr_t
create_log_files(
/*=============*/
	char*	logfilename,	/*!< in/out: buffer for log file name */
	size_t	dirnamelen,	/*!< in: length of the directory path */
	lsn_t	lsn,		/*!< in: FIL_PAGE_FILE_FLUSH_LSN value */
	char*&	logfile0)	/*!< out: name of the first log file */
{
	dberr_t err;

	if (srv_read_only_mode) {
		ib::error() << "Cannot create log files in read-only mode";
		return(DB_READ_ONLY);
	}

	/* Remove any old log files. */
	for (unsigned i = 0; i <= INIT_LOG_FILE0; i++) {
		sprintf(logfilename + dirnamelen, "ib_logfile%u", i);

		/* Ignore errors about non-existent files or files
		that cannot be removed. The create_log_file() will
		return an error when the file exists. */
#ifdef _WIN32
		DeleteFile((LPCTSTR) logfilename);
#else
		unlink(logfilename);
#endif /* _WIN32 */
		/* Crashing after deleting the first
		file should be recoverable. The buffer
		pool was clean, and we can simply create
		all log files from the scratch. */
		RECOVERY_CRASH(6);
	}

	ut_ad(!buf_pool_check_no_pending_io());

	RECOVERY_CRASH(7);

	for (unsigned i = 0; i < srv_n_log_files; i++) {
		sprintf(logfilename + dirnamelen,
			"ib_logfile%u", i ? i : INIT_LOG_FILE0);

		err = create_log_file(&files[i], logfilename);

		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	RECOVERY_CRASH(8);

	/* We did not create the first log file initially as
	ib_logfile0, so that crash recovery cannot find it until it
	has been completed and renamed. */
	sprintf(logfilename + dirnamelen, "ib_logfile%u", INIT_LOG_FILE0);

	/* Disable the doublewrite buffer for log files, not required */

	fil_space_t*	log_space = fil_space_create(
		"innodb_redo_log", dict_sys_t::log_space_first_id,
		fsp_flags_set_page_size(0, univ_page_size),
		FIL_TYPE_LOG);
	ut_a(fil_validate());
	ut_a(log_space != NULL);

	/* Once the redo log is set to be encrypted,
	initialize encryption information. */
	if (srv_redo_log_encrypt) {
		if (!Encryption::check_keyring()) {
			ib::error()
				<< "Redo log encryption is enabled,"
				<< " but keyring plugin is not loaded.";

			return(DB_ERROR);
		}

		log_space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
		err = fil_set_encryption(log_space->id,
					 Encryption::AES,
					 NULL,
					 NULL);
		ut_ad(err == DB_SUCCESS);
	}

	logfile0 = fil_node_create(
		logfilename, static_cast<page_no_t>(srv_log_file_size),
		log_space, false, false);
	ut_a(logfile0);

	for (unsigned i = 1; i < srv_n_log_files; i++) {

		sprintf(logfilename + dirnamelen, "ib_logfile%u", i);

		if (!fil_node_create(logfilename,
				     static_cast<page_no_t>(srv_log_file_size),
				     log_space, false, false)) {

			ib::error()
				<< "Cannot create file node for log file "
				<< logfilename;

			return(DB_ERROR);
		}
	}

	if (!log_group_init(0, srv_n_log_files,
			    srv_log_file_size * UNIV_PAGE_SIZE,
			    dict_sys_t::log_space_first_id)) {
		return(DB_ERROR);
	}

	fil_open_log_and_system_tablespace_files();

	/* Create a log checkpoint. */
	log_mutex_enter();
	ut_d(log_sys->disable_redo_writes = false);
	recv_reset_logs(lsn);
	log_mutex_exit();

	/* Write encryption information into the first log file header
	if redo log is set with encryption. */
	if (FSP_FLAGS_GET_ENCRYPTION(log_space->flags)) {
		if (!log_write_encryption(log_space->encryption_key,
					  log_space->encryption_iv,
					  true)) {
			return(DB_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Renames the first log file. */
static
void
create_log_files_rename(
/*====================*/
	char*	logfilename,	/*!< in/out: buffer for log file name */
	size_t	dirnamelen,	/*!< in: length of the directory path */
	lsn_t	lsn,		/*!< in: FIL_PAGE_FILE_FLUSH_LSN value */
	char*	logfile0)	/*!< in/out: name of the first log file */
{
	/* If innodb_flush_method=O_DSYNC,
	we need to explicitly flush the log buffers. */
	fil_flush(dict_sys_t::log_space_first_id);
	/* Close the log files, so that we can rename
	the first one. */
	fil_close_log_files(false);

	/* Rename the first log file, now that a log
	checkpoint has been created. */
	sprintf(logfilename + dirnamelen, "ib_logfile%u", 0);

	RECOVERY_CRASH(9);

	ib::info() << "Renaming log file " << logfile0 << " to "
		<< logfilename;

	log_mutex_enter();
	ut_ad(strlen(logfile0) == 2 + strlen(logfilename));
	bool success = os_file_rename(
		innodb_log_file_key, logfile0, logfilename);
	ut_a(success);

	RECOVERY_CRASH(10);

	/* Replace the first file with ib_logfile0. */
	strcpy(logfile0, logfilename);
	log_mutex_exit();

	fil_open_log_and_system_tablespace_files();

	ib::warn() << "New log files created, LSN=" << lsn;
}

/*********************************************************************//**
Opens a log file.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
open_log_file(
/*==========*/
	pfs_os_file_t*	file,	/*!< out: file handle */
	const char*	name,	/*!< in: log file name */
	os_offset_t*	size)	/*!< out: file size */
{
	bool	ret;

	*file = os_file_create(innodb_log_file_key, name,
			       OS_FILE_OPEN, OS_FILE_AIO,
			       OS_LOG_FILE, srv_read_only_mode, &ret);
	if (!ret) {
		ib::error() << "Unable to open '" << name << "'";
		return(DB_ERROR);
	}

	*size = os_file_get_size(*file);

	ret = os_file_close(*file);
	ut_a(ret);
	return(DB_SUCCESS);
}

/** Create undo tablespace.
@param[in]	space_id	Undo Tablespace ID
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespace_create(
	space_id_t	space_id)
{
	pfs_os_file_t	fh;
	bool		ret;
	dberr_t		err = DB_SUCCESS;

	char*	file_name = undo::make_file_name(space_id);

	os_file_create_subdirs_if_needed(file_name);

	fh = os_file_create(
		innodb_data_file_key,
		file_name,
		srv_read_only_mode ? OS_FILE_OPEN : OS_FILE_CREATE,
		OS_FILE_NORMAL, OS_DATA_FILE, srv_read_only_mode, &ret);

	if (srv_read_only_mode && ret) {

		ib::info() << file_name << " opened in read-only mode";

	} else if (ret == FALSE) {
		if (os_file_get_last_error(false) != OS_FILE_ALREADY_EXISTS) {

			ib::error() << "Can't create UNDO tablespace "
				<< file_name;
		}
		err = DB_ERROR;
	} else {
		ut_a(!srv_read_only_mode);

		/* We created the data file and now write it full of zeros */

		ib::info() << "Creating UNDO Tablespace Data file "
			<< file_name;

		ulint size_mb = SRV_UNDO_TABLESPACE_SIZE_IN_PAGES
				<< UNIV_PAGE_SIZE_SHIFT >> 20;
		ib::info() << "Setting file " << file_name
			<< " size to " << size_mb << " MB";

		ib::info() << "Physically writing the file full";

		ret = os_file_set_size(
			file_name, fh,
			SRV_UNDO_TABLESPACE_SIZE_IN_PAGES
				<< UNIV_PAGE_SIZE_SHIFT,
			srv_read_only_mode);

		if (!ret) {
			ib::info() << "Error in creating " << file_name
				<< ": probably out of disk space";
			err = DB_OUT_OF_FILE_SPACE;
		}

		os_file_close(fh);

		/* Add this space to the list of undo tablespaces to
		fix-up by creating header pages. */
		undo::add_space_to_construction_list(space_id);
	}

	ut_free(file_name);

	return(err);
}

/** Try to enable encryption of an undo log tablespace.
@param[in]	space_id	undo tablespace id
@return DB_SUCCESS if success */
static
dberr_t
srv_undo_tablespace_enable_encryption(
	space_id_t	space_id)
{
	fil_space_t*		space;
	dberr_t			err;

	if (Encryption::check_keyring() == false) {
		my_error(ER_CANNOT_FIND_KEY_IN_KEYRING, MYF(0));
		return(DB_ERROR);
	}

	/* Set the space flag, and the encryption metadata
	will be generated in fsp_header_init later. */
	space = fil_space_get(space_id);
	if (!FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
		space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
		err = fil_set_encryption(space_id,
					 Encryption::AES,
					 NULL,
					 NULL);
		if (err != DB_SUCCESS) {
			ib::error() << "Can't set encryption"
				" metadata for space "
				<< space->name << ".";
			return(err);
		}
	}

	return(DB_SUCCESS);
}

/** Try to read encryption metat dat from an undo log file.
@param[in]	fh		file handle of undo log file
@param[in]	space		undo tablespace
@return DB_SUCCESS if success */
static
dberr_t
srv_undo_tablespace_read_encryption(
	pfs_os_file_t	fh,
	fil_space_t*	space)
{
	IORequest	request;
	ulint		n_read = 0;
	size_t		page_size = UNIV_PAGE_SIZE_MAX;
	dberr_t		err = DB_ERROR;

	byte* first_page_buf = static_cast<byte*>(
		ut_malloc_nokey(2 * UNIV_PAGE_SIZE_MAX));
	/* Align the memory for a possible read from a raw device */
	byte* first_page = static_cast<byte*>(
		ut_align(first_page_buf, UNIV_PAGE_SIZE));

	/* Don't want unnecessary complaints about partial reads. */
	request.disable_partial_io_warnings();

	err = os_file_read_no_error_handling(
		request, fh, first_page, 0, page_size, &n_read);

	if (err != DB_SUCCESS) {
		ib::info()
			<< "Cannot read first page of '"
			<< space->name << "' "
			<< ut_strerr(err);
		ut_free(first_page_buf);
		return(err);
	}

	ulint			offset;
	const page_size_t	space_page_size(space->flags);

	offset = fsp_header_get_encryption_offset(space_page_size);
	ut_ad(offset);

	/* Return if the encryption metadata is empty. */
	if (memcmp(first_page + offset,
		   ENCRYPTION_KEY_MAGIC_V2,
		   ENCRYPTION_MAGIC_SIZE) != 0) {
		ut_free(first_page_buf);
		return(DB_SUCCESS);
	}

	byte	key[ENCRYPTION_KEY_LEN];
	byte	iv[ENCRYPTION_KEY_LEN];
	if (fsp_header_get_encryption_key(space->flags, key,
					  iv, first_page)) {

		space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
		err = fil_set_encryption(space->id,
					 Encryption::AES,
					 key,
					 iv);
		ut_ad(err == DB_SUCCESS);
	} else {
		ut_free(first_page_buf);
		return(DB_FAIL);
	}

	ut_free(first_page_buf);

	return(DB_SUCCESS);
}

/** Fix up an independent undo tablespace if it was in the process of being
truncated when the server crashed. The truncation will need to be completed.
@param[in]	space_id	Tablespace ID
@return error code */
static
dberr_t
srv_undo_tablespace_fixup(
	space_id_t	space_id)
{
	undo::Tablespace	undo_space(space_id);

	if (undo::is_active_truncate_log_present(space_id)) {

		ib::info() << "Undo Tablespace number " << space_id
			<< " was being truncated when mysqld quit.";

		if (srv_read_only_mode) {
			ib::error() << "Cannot recover a truncated"
				" undo tablespace in read-only mode";
			return(DB_READ_ONLY);
		}

		ib::info() << "Reconstructing undo tablespace number"
			<< space_id << ".";

		/* Flush any changes recovered in REDO */
		fil_flush(space_id);
		fil_space_close(space_id);

		os_file_delete(innodb_data_file_key,
				undo_space.file_name());

		dberr_t	err = srv_undo_tablespace_create(space_id);
		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	return(DB_SUCCESS);
}

/** Open an undo tablespace.
@param[in]	space_id	tablespace ID
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespace_open(space_id_t space_id)
{
	pfs_os_file_t		fh;
	bool			ret;
	ulint			flags;
	dberr_t			err = DB_ERROR;
	undo::Tablespace	undo_space(space_id);
	char*			undo_name = undo_space.space_name();
	char*			file_name = undo_space.file_name();

	/* Check if it was already opened during redo discovery.. */
	err = fil_space_undo_check_if_opened(file_name, undo_name, space_id);
	if (err != DB_TABLESPACE_NOT_FOUND) {
		return(err);
	}

	if (!srv_file_check_mode(file_name)) {
		ib::error() << "UNDO tablespace " << file_name << " must be "
			<< (srv_read_only_mode ? "readable!" : "writable!");

		return(DB_ERROR);
	}

	fh = os_file_create(
		innodb_data_file_key,
		file_name,
		OS_FILE_OPEN_RETRY
		| OS_FILE_ON_ERROR_NO_EXIT
		| OS_FILE_ON_ERROR_SILENT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		srv_read_only_mode,
		&ret);
	if (!ret) {
		return(DB_CANNOT_OPEN_FILE);
	}

	/* Since the file open was successful, load the tablespace. */

	bool	atomic_write;

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
	if (!srv_use_doublewrite_buf) {
		atomic_write = fil_fusionio_enable_atomic_write(fh);
	} else {
		atomic_write = false;
	}
#else
	atomic_write = false;
#endif /* !NO_FALLOCATE && UNIV_LINUX */

	os_offset_t size = os_file_get_size(fh);
	ut_a(size != (os_offset_t)-1);

	/* We set the biggest space id to the undo tablespace
	because InnoDB hasn't opened any other tablespace apart
	from the system tablespace. */
	fil_set_max_space_id_if_bigger(space_id);

	/* Load the tablespace into InnoDB's internal data structures.
	Set the compressed page size to 0 (non-compressed) */
	flags = fsp_flags_init(
		univ_page_size, false, false, false, false);
	fil_space_t* space = fil_space_create(
		undo_name, space_id, flags, FIL_TYPE_TABLESPACE);

	ut_a(fil_validate());
	ut_a(space);

	page_no_t	n_pages = static_cast<page_no_t>(
		size / UNIV_PAGE_SIZE);

	/* On 32-bit platforms, ulint is 32 bits and os_offset_t
	is 64 bits. It is OK to cast the n_pages to ulint because
	the unit has been scaled to pages and page number is always
	32 bits. */
	if (fil_node_create(
		file_name, n_pages, space, false, atomic_write)) {

		/* For encrypted undo tablespaces, check the encryption info
		in the first page can be decrypt by master key, otherwise,
		this table can't be open. */
		err = srv_undo_tablespace_read_encryption(fh, space);

		ret = os_file_close(fh);
		ut_a(ret);
	}

	return(err);
}

/* Open existing undo tablespaces up to the number in srv_undo_tablespace.
If we are making a new database, these have been created.
If doing recovery, these should exist and may be needed for recovery.
If we fail to open any of these it is a fatal error.
The tablespace IDs should be contiguous.
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespaces_open()
{
	dberr_t		err;
	space_id_t	space_id;
	Space_Ids	spaces_to_open;

	/* Build a list of existing undo tablespaces from the references
	in the TRX_SYS page. (not including the system tablespace) 
	Use a local list until they are actually opened so that they do
	not appear to be an undo tablespace before being closed and taken
	off the LRU. If they were openned during redo discovery, they were
	not recognized as undo tablespaces and were put onto the LRU. */
	trx_rseg_get_n_undo_tablespaces(spaces_to_open);

	if (spaces_to_open.size() > 0) {

		/* Open each undo tablespace tracked in TRX_SYS. */
		for (Space_Ids::const_iterator
		     it = spaces_to_open.begin();
		     it != spaces_to_open.end(); ++it) {
			space_id = *it;

			/* Check if this undo tablespace was in the
			process of being truncated.  If so, recreate it
			and add it to the construction list. */
			err = srv_undo_tablespace_fixup(space_id);
			if (err != DB_SUCCESS) {
				return(err);
			}

			err = srv_undo_tablespace_open(space_id);
			if (err != DB_SUCCESS) {
				ib::error() << "Unable to open undo"
					" tablespace number " << space_id;
				return(err);
			}

			trx_sys_undo_spaces->push_back(space_id);

			/* Now that space and node exist, open this undo
			tablespace so that it stays open until shutdown. */
			ut_a(fil_space_open(space_id));
		}

		if (trx_sys_undo_spaces->size() > 0) {
			trx_sys_undo_spaces->sort();
		}
	}

	/* Open any extra unused undo tablespaces that exist but were not
	tracked in TRX_SYS. These must be contiguous. We stop at the first
	failure. These are undo tablespaces that were not in use previously
	and therefore not required by recovery. We check that there are no
	gaps and set max space_id. */

	space_id_t last_undo_space_id =
		(trx_sys_undo_spaces->size() == 0 ? 0
		 : trx_sys_undo_spaces->back());

	for (space_id = last_undo_space_id + 1;
	     space_id < TRX_SYS_N_RSEGS; ++space_id) {

		err = srv_undo_tablespace_open(space_id);
		if (err != DB_SUCCESS) {
			break;
		}

		/* Add this undo tablespace to the active list if the
		startup setting allows. */
		if (trx_sys_undo_spaces->size() < srv_undo_tablespaces) {
			trx_sys_undo_spaces->push_back(space_id);

			/* Now that space and node exist, open this undo
			tablespace so that it stays open until shutdown. */
			ut_a(fil_space_open(space_id));
		}
	}

	return(DB_SUCCESS);
}

/** Create undo tablespaces if we are creating a new instance
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespaces_create()
{
	dberr_t		err = DB_SUCCESS;
	space_id_t	space_id;

	if (srv_read_only_mode
	    || srv_force_recovery
	    || recv_needed_recovery) {
		return(DB_SUCCESS);
	}

	ulint	initial_undo_spaces = trx_sys_undo_spaces->size();
	if (initial_undo_spaces >= srv_undo_tablespaces) {
		return(DB_SUCCESS);
	}

	DBUG_EXECUTE_IF("innodb_undo_upgrade",
		dict_hdr_get_new_id(NULL, NULL, &space_id, NULL, true);
		dict_hdr_get_new_id(NULL, NULL, &space_id, NULL, true);
		dict_hdr_get_new_id(NULL, NULL, &space_id, NULL, true);
	);

	for (space_id_t num = 1; num < TRX_SYS_N_RSEGS; ++num) {

		/* Quit when we have enough. */
		if (trx_sys_undo_spaces->size()
		    >= srv_undo_tablespaces) {
			break;
		}

		dict_hdr_get_new_id(NULL, NULL, &space_id, NULL, true);

		/* This num may have already been opened from the
		TRX_SYS page. */
		if (trx_sys_undo_spaces->contains(space_id)) {
			continue;
		}

		err = srv_undo_tablespace_create(space_id);
		if (err != DB_SUCCESS) {
			ib::info() << "Could not create undo tablespace"
				" number " << num;
			break;
		}

		/* Open this new undo tablespace. */
		err = srv_undo_tablespace_open(space_id);
		if (err != DB_SUCCESS) {
			ib::info() << "Error " << err << " opening"
				" newly created undo tablespace number "
				<< num;
			break;
		}

		/* Enable undo log encryption if it's ON. */
		if (srv_undo_log_encrypt) {
			mtr_t	mtr;

			err = srv_undo_tablespace_enable_encryption(
				space_id);

			if (err != DB_SUCCESS) {
				ib::error() << "Unable to create"
					<< " encrypted undo tablespace,"
					<< " please check keyring"
					<< " plugin is initialized"
					<< " correctly";
				return(DB_ERROR);
			}

			ib::info() << "Undo log encryption is"
				<<" enabled.";

			mtr_start(&mtr);

			fsp_header_init(
				space_id,
				SRV_UNDO_TABLESPACE_SIZE_IN_PAGES, &mtr,
				true);
			mtr_commit(&mtr);
		}

		trx_sys_undo_spaces->push_back(space_id);
	}

	ulint	new_spaces = trx_sys_undo_spaces->size()
			     - initial_undo_spaces;

	ib::info() << "Created " << new_spaces << " undo tablespaces.";

	return(err);
}

/** Finish building an undo tablespace. So far these tablespace files in
the construction list should be created and filled with zeros. */
static
void
srv_undo_tablespaces_construct(bool create_new_db)
{
	space_id_t		space_id;
	page_no_t		page_no;
	ulint			slot_space_id;
	ulint			rseg_id;
	mtr_t			mtr;

	Space_Ids::const_iterator	it;
	for (it = undo::s_under_construction.begin();
	     it != undo::s_under_construction.end(); ++it) {
		space_id = *it;

		mtr_start(&mtr);
		/* trx_rseg_header_create() will write to the TRX_SYS page. */
		mtr_x_lock(fil_space_get_latch(space_id, NULL), &mtr);

		fsp_header_init(
			space_id, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES,
			&mtr, create_new_db);

		if (create_new_db) {
			/* The rollback segments will be created later in
			trx_sys_create_additional_rsegs() */
			mtr_commit(&mtr);
			continue;
		}

		/* These tablespaces are being recreated from a truncate
		fixup. Replace the rollback segments that are recorded in
		the TRX_SYS page.*/
		trx_sysf_t*	sys_header = trx_sysf_get(&mtr);

		/* Look at each slot in the TRX_SYS page except 0 since
		that is always reserved for the system space. The recreated
		rollback segment should have the same header page number
		that it did before. */
		for (rseg_id = 1; rseg_id < TRX_SYS_N_RSEGS; rseg_id++) {

			slot_space_id = trx_sysf_rseg_get_space(
				sys_header, rseg_id, &mtr);

			if (space_id == slot_space_id) {
				page_no = trx_rseg_header_create(
					space_id, univ_page_size,
					PAGE_NO_MAX, rseg_id, &mtr);
				ut_a(page_no != FIL_NULL);
			}
		}

		mtr_commit(&mtr);
	}
}

/** Flush any pages written during the construction process.
Clean up any left over truncation log files.
Clear the construction list. */
static
void
srv_undo_tablespaces_construction_list_clear()
{
	space_id_t			space_id;
	Space_Ids::const_iterator	it;

	buf_LRU_flush_or_remove_pages(
		TRX_SYS_SPACE, BUF_REMOVE_FLUSH_WRITE, NULL);

	for (it = undo::s_under_construction.begin();
	     it != undo::s_under_construction.end(); ++it) {
		space_id = *it;

		buf_LRU_flush_or_remove_pages(
			space_id, BUF_REMOVE_FLUSH_WRITE, NULL);

		/* Remove the truncate redo log file if it exists. */
		if (undo::is_active_truncate_log_present(space_id)) {
			undo::Truncate	undo_trunc;
			undo_trunc.done_logging(space_id);
		}
	}

	undo::clear_construction_list();
}

/** Open the configured number of undo tablespaces.
@param[in]	create_new_db	true if new db being created
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespaces_init(bool create_new_db)
{
	dberr_t		err = DB_SUCCESS;

	ut_a(srv_undo_tablespaces <= TRX_SYS_N_RSEGS);

	trx_sys_undo_spaces_init();

	if (!create_new_db) {
		err = srv_undo_tablespaces_open();
		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	/* If this is opening an existing database, create and open any
	undo tablespaces that are still needed. For a new DB, create
	them all. */
	err = srv_undo_tablespaces_create();
	if (err != DB_SUCCESS) {
		return(err);
	}

	/* If the user says that there are fewer than what we find we
	tolerate that discrepancy but not the inverse. Because there could
	be unused undo tablespaces for future use. */

	if (srv_undo_tablespaces > trx_sys_undo_spaces->size()) {
		ib::error() << "Expected to open " << srv_undo_tablespaces
			<< " undo tablespaces but was able to find only "
			<< trx_sys_undo_spaces->size()
			<< " undo tablespaces. Set the"
			" innodb_undo_tablespaces parameter to the correct"
			" value and retry. Suggested value is "
			<< trx_sys_undo_spaces->size();

		return(err != DB_SUCCESS ? err : DB_ERROR);

	} else  if (trx_sys_undo_spaces->size() > 0) {

		ib::info() << "Opened " << trx_sys_undo_spaces->size()
			<< " undo tablespaces";

		ib::info() << trx_sys_undo_spaces->size() << " undo tablespaces"
			<< " made active";

		if (srv_undo_tablespaces == 0) {
			ib::info() << "Will use system tablespace for all newly"
				<< " created rollback-segments since"
				<< " innodb_undo_tablespaces=0";
		}
	}

	return(DB_SUCCESS);
}

/********************************************************************
Wait for the purge thread(s) to start up. */
static
void
srv_start_wait_for_purge_to_start()
/*===============================*/
{
	/* Wait for the purge coordinator and master thread to startup. */

	purge_state_t	state = trx_purge_state();

	ut_a(state != PURGE_STATE_DISABLED);

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE
	       && srv_force_recovery < SRV_FORCE_NO_BACKGROUND
	       && state == PURGE_STATE_INIT) {

		switch (state = trx_purge_state()) {
		case PURGE_STATE_RUN:
		case PURGE_STATE_STOP:
			break;

		case PURGE_STATE_INIT:
			ib::info() << "Waiting for purge to start";

			os_thread_sleep(50000);
			break;

		case PURGE_STATE_EXIT:
		case PURGE_STATE_DISABLED:
			ut_error;
		}
	}
}

/** Create the temporary file tablespace.
@param[in]	create_new_db	whether we are creating a new database
@param[in,out]	tmp_space	Shared Temporary SysTablespace
@return DB_SUCCESS or error code. */
static
dberr_t
srv_open_tmp_tablespace(
	bool		create_new_db,
	SysTablespace*	tmp_space)
{
	page_no_t	sum_of_new_sizes;

	/* Will try to remove if there is existing file left-over by last
	unclean shutdown */
	tmp_space->set_sanity_check_status(true);
	tmp_space->delete_files();
	tmp_space->set_ignore_read_only(true);

	ib::info() << "Creating shared tablespace for temporary tables";

	bool		create_new_temp_space = true;

	tmp_space->set_space_id(dict_sys_t::temp_space_id);

	RECOVERY_CRASH(100);

	dberr_t	err = tmp_space->check_file_spec(
		create_new_temp_space, 12 * 1024 * 1024);

	if (err == DB_FAIL) {

		ib::error() << "The " << tmp_space->name()
			<< " data file must be writable!";

		err = DB_ERROR;

	} else if (err != DB_SUCCESS) {
		ib::error() << "Could not create the shared "
			<< tmp_space->name() << ".";

	} else if ((err = tmp_space->open_or_create(
			    true, create_new_db, &sum_of_new_sizes, NULL))
		   != DB_SUCCESS) {

		ib::error() << "Unable to create the shared "
			<< tmp_space->name();

	} else {

		mtr_t		mtr;
		page_no_t	size = tmp_space->get_sum_of_sizes();

		/* Open this shared temp tablespace in the fil_system so that
		it stays open until shutdown. */
		if (fil_space_open(tmp_space->space_id())) {

			/* Initialize the header page */
			mtr_start(&mtr);
			mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

			fsp_header_init(tmp_space->space_id(),
					size,
					&mtr,
					false);

			mtr_commit(&mtr);
		} else {
			/* This file was just opened in the code above! */
			ib::error() << "The " << tmp_space->name()
				<< " data file cannot be re-opened"
				" after check_file_spec() succeeded!";

			err = DB_ERROR;
		}
	}

	return(err);
}

/** Create SDI Indexes in system tablespace. */
static
void
srv_create_sdi_indexes()
{
	btr_sdi_create_indexes(SYSTEM_TABLE_SPACE, false);
}

/****************************************************************//**
Set state to indicate start of particular group of threads in InnoDB. */
UNIV_INLINE
void
srv_start_state_set(
/*================*/
	srv_start_state_t state)	/*!< in: indicate current state of
					thread startup */
{
	srv_start_state |= state;
}

/****************************************************************//**
Check if following group of threads is started.
@return true if started */
UNIV_INLINE
bool
srv_start_state_is_set(
/*===================*/
	srv_start_state_t state)	/*!< in: state to check for */
{
	return(srv_start_state & state);
}

/**
Shutdown all background threads created by InnoDB. */
static
void
srv_shutdown_all_bg_threads()
{
	ulint	i;

	srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

	if (srv_start_state == SRV_START_STATE_NONE) {
		return;
	}

	UT_DELETE(srv_dict_metadata);
	srv_dict_metadata = NULL;

	/* All threads end up waiting for certain events. Put those events
	to the signaled state. Then the threads will exit themselves after
	os_event_wait(). */
	for (i = 0; i < 1000; i++) {
		/* NOTE: IF YOU CREATE THREADS IN INNODB, YOU MUST EXIT THEM
		HERE OR EARLIER */

		if (!srv_read_only_mode) {

			if (srv_start_state_is_set(SRV_START_STATE_LOCK_SYS)) {
				/* a. Let the lock timeout thread exit */
				os_event_set(lock_sys->timeout_event);
			}

			/* b. srv error monitor thread exits automatically,
			no need to do anything here */

			if (srv_start_state_is_set(SRV_START_STATE_MASTER)) {
				/* c. We wake the master thread so that
				it exits */
				srv_wake_master_thread();
			}

			if (srv_start_state_is_set(SRV_START_STATE_PURGE)) {
				/* d. Wakeup purge threads. */
				srv_purge_wakeup();
			}
		}

		if (srv_start_state_is_set(SRV_START_STATE_IO)) {
			/* e. Exit the i/o threads */
			if (!srv_read_only_mode) {
				if (recv_sys->flush_start != NULL) {
					os_event_set(recv_sys->flush_start);
				}
				if (recv_sys->flush_end != NULL) {
					os_event_set(recv_sys->flush_end);
				}
			}

			os_event_set(buf_flush_event);

			if (!buf_page_cleaner_is_active
			    && os_aio_all_slots_free()) {
				os_aio_wake_all_threads_at_shutdown();
			}
		}

		/* f. dict_stats_thread is signaled from
		logs_empty_and_mark_files_at_shutdown() and should have
		already quit or is quitting right now. */

		bool	active = os_thread_any_active();

		os_thread_sleep(100000);

		if (!active) {
			break;
		}
	}

	if (i == 1000) {
		ib::warn() << os_thread_count << " threads created by InnoDB"
			" had not exited at shutdown!";
#ifdef UNIV_DEBUG
		os_aio_print_pending_io(stderr);
		ut_ad(0);
#endif /* UNIV_DEBUG */
	} else {
		/* Reset the start state. */
		srv_start_state = SRV_START_STATE_NONE;
	}
}

#ifdef UNIV_DEBUG
# define srv_init_abort(_db_err)	\
	srv_init_abort_low(create_new_db, __FILE__, __LINE__, _db_err)
#else
# define srv_init_abort(_db_err)	\
	srv_init_abort_low(create_new_db, _db_err)
#endif /* UNIV_DEBUG */

/** Innobase start-up aborted. Perform cleanup actions.
@param[in]	create_new_db	TRUE if new db is  being created
@param[in]	file		File name
@param[in]	line		Line number
@param[in]	err		Reason for aborting InnoDB startup
@return DB_SUCCESS or error code. */
static
dberr_t
srv_init_abort_low(
	bool		create_new_db,
#ifdef UNIV_DEBUG
	const char*	file,
	ulint		line,
#endif /* UNIV_DEBUG */
	dberr_t		err)
{
	if (create_new_db) {
		ib::error() << "InnoDB Database creation was aborted"
#ifdef UNIV_DEBUG
			" at " << innobase_basename(file) << "[" << line << "]"
#endif /* UNIV_DEBUG */
			" with error " << ut_strerr(err) << ". You may need"
			" to delete the ibdata1 file before trying to start"
			" up again.";
	} else {
		ib::error() << "Plugin initialization aborted"
#ifdef UNIV_DEBUG
			" at " << innobase_basename(file) << "[" << line << "]"
#endif /* UNIV_DEBUG */
			" with error " << ut_strerr(err);
	}

	srv_shutdown_all_bg_threads();
	return(err);
}

/** Prepare to delete the redo log files. Flush the dirty pages from all the
buffer pools.  Flush the redo log buffer to the redo log file.
@param[in]	n_files		number of old redo log files
@return lsn upto which data pages have been flushed. */
static
lsn_t
srv_prepare_to_delete_redo_log_files(
	ulint	n_files)
{
	lsn_t	flushed_lsn;
	ulint	pending_io = 0;
	ulint	count = 0;

	do {
		/* Write back all dirty metadata first. To resize the logs
		files to smaller ones, we will do the checkpoint at last,
		if we write back there, it could be found that the new log
		group was not big enough for the new redo logs, thus a
		cascade checkpoint would be invoked, which is unexpected.
		There should be no concurrent DML, so no need to require
		dict_persist::lock. */
		dict_persist_to_dd_table_buffer();

		/* Clean the buffer pool. */
		buf_flush_sync_all_buf_pools();

		RECOVERY_CRASH(1);

		log_mutex_enter();

		flushed_lsn = log_sys->lsn;

		{
			ib::warn	warning;
			if (srv_log_file_size == 0) {
				warning << "Upgrading redo log: ";
			} else {
				warning << "Resizing redo log from "
					<< n_files << "*"
					<< srv_log_file_size << " to ";
			}
			warning << srv_n_log_files << "*"
				<< srv_log_file_size_requested
				<< " pages, LSN=" << flushed_lsn;
		}

		/* Flush the old log files. */
		log_mutex_exit();

		log_write_up_to(flushed_lsn, true);

		/* If innodb_flush_method=O_DSYNC,
		we need to explicitly flush the log buffers. */
		fil_flush(dict_sys_t::log_space_first_id);

		ut_ad(flushed_lsn == log_get_lsn());

		/* Check if the buffer pools are clean.  If not
		retry till it is clean. */
		pending_io = buf_pool_check_no_pending_io();

		if (pending_io > 0) {
			count++;
			/* Print a message every 60 seconds if we
			are waiting to clean the buffer pools */
			if (srv_print_verbose_log && count > 600) {
				ib::info() << "Waiting for "
					<< pending_io << " buffer "
					<< "page I/Os to complete";
				count = 0;
			}
		}
		os_thread_sleep(100000);

	} while (buf_pool_check_no_pending_io());

	return(flushed_lsn);
}

/** Start InnoDB.
@param[in]	create_new_db		Whether to create a new database
@param[in]	scan_directories	Scan directories for .ibd files for
					recovery "dir1;dir2; ... dirN"
@return DB_SUCCESS or error code */
dberr_t
srv_start(bool create_new_db, const char* scan_directories)
{
	lsn_t		flushed_lsn;
	page_no_t	sum_of_data_file_sizes;
	page_no_t	tablespace_size_in_header;
	dberr_t		err;
	ulint		srv_n_log_files_found = srv_n_log_files;
	mtr_t		mtr;
	purge_pq_t*	purge_queue;
	char		logfilename[10000];
	char*		logfile0	= NULL;
	size_t		dirnamelen;
	unsigned	i = 0;

	DBUG_ASSERT(srv_dict_metadata == NULL);
	/* Reset the start state. */
	srv_start_state = SRV_START_STATE_NONE;

#ifdef UNIV_LINUX
# ifdef HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
	ib::info() << "PUNCH HOLE support available";
# else
	ib::info() << "PUNCH HOLE support not available";
# endif /* HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE */
#endif /* UNIV_LINUX */

	if (sizeof(ulint) != sizeof(void*)) {
		ib::error() << "Size of InnoDB's ulint is " << sizeof(ulint)
			<< ", but size of void* is " << sizeof(void*)
			<< ". The sizes should be the same so that on"
			" a 64-bit platforms you can allocate more than 4 GB"
			" of memory.";
	}

#ifdef UNIV_DEBUG
	ib::info() << "!!!!!!!! UNIV_DEBUG switched on !!!!!!!!!";
#endif

#ifdef UNIV_IBUF_DEBUG
	ib::info() << "!!!!!!!! UNIV_IBUF_DEBUG switched on !!!!!!!!!";
# ifdef UNIV_IBUF_COUNT_DEBUG
	ib::info() << "!!!!!!!! UNIV_IBUF_COUNT_DEBUG switched on !!!!!!!!!";
	ib::error() << "Crash recovery will fail with UNIV_IBUF_COUNT_DEBUG";
# endif
#endif

#ifdef UNIV_LOG_LSN_DEBUG
	ib::info() << "!!!!!!!! UNIV_LOG_LSN_DEBUG switched on !!!!!!!!!";
#endif /* UNIV_LOG_LSN_DEBUG */

#if defined(COMPILER_HINTS_ENABLED)
	ib::info() << "Compiler hints enabled.";
#endif /* defined(COMPILER_HINTS_ENABLED) */

	ib::info() << IB_ATOMICS_STARTUP_MSG;
	ib::info() << MUTEX_TYPE;
	ib::info() << IB_MEMORY_BARRIER_STARTUP_MSG;

	if (srv_force_recovery > 0) {

		ib::info()
			<< "!!! innodb_force_recovery is set to "
			<< srv_force_recovery << " !!!";
	}

#ifndef HAVE_MEMORY_BARRIER
#if defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64 || defined _WIN32
#else
	ib::warn() << "MySQL was built without a memory barrier capability on"
		" this architecture, which might allow a mutex/rw_lock"
		" violation under high thread concurrency. This may cause a"
		" hang.";
#endif /* IA32 or AMD64 */
#endif /* HAVE_MEMORY_BARRIER */

	ib::info() << "Compressed tables use zlib " ZLIB_VERSION
#ifdef UNIV_ZIP_DEBUG
	      " with validation"
#endif /* UNIV_ZIP_DEBUG */
	      ;
#ifdef UNIV_ZIP_COPY
	ib::info() << "and extra copying";
#endif /* UNIV_ZIP_COPY */

	/* Since InnoDB does not currently clean up all its internal data
	structures in MySQL Embedded Server Library server_end(), we
	print an error message if someone tries to start up InnoDB a
	second time during the process lifetime. */

	if (srv_start_has_been_called) {
		ib::error() << "Startup called second time"
			" during the process lifetime."
			" In the MySQL Embedded Server Library"
			" you cannot call server_init() more than"
			" once during the process lifetime.";
	}

	srv_start_has_been_called = true;

	srv_is_being_started = true;

	/* Register performance schema stages before any real work has been
	started which may need to be instrumented. */
	mysql_stage_register("innodb", srv_stages, UT_ARR_SIZE(srv_stages));

	/* Switch latching order checks on in sync0debug.cc, if
	--innodb-sync-debug=false (default) */
	ut_d(sync_check_enable());

	srv_boot();

	ib::info() << (ut_crc32_cpu_enabled ? "Using" : "Not using")
		<< " CPU crc32 instructions";

	if (!create_new_db
	    && scan_directories != nullptr
	    && strlen(scan_directories) > 0) {

		dberr_t	err;

		err = fil_scan_for_tablespaces(scan_directories);

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}
	}

	if (!srv_read_only_mode) {

		mutex_create(LATCH_ID_SRV_MONITOR_FILE,
			     &srv_monitor_file_mutex);

		if (srv_innodb_status) {

			srv_monitor_file_name = static_cast<char*>(
				ut_malloc_nokey(
					strlen(fil_path_to_mysql_datadir)
					+ 20 + sizeof "/innodb_status."));

			sprintf(srv_monitor_file_name,
				"%s/innodb_status." ULINTPF,
				fil_path_to_mysql_datadir,
				os_proc_get_number());

			srv_monitor_file = fopen(srv_monitor_file_name, "w+");

			if (!srv_monitor_file) {
				ib::error() << "Unable to create "
					<< srv_monitor_file_name << ": "
					<< strerror(errno);
				return(srv_init_abort(DB_ERROR));
			}
		} else {

			srv_monitor_file_name = NULL;
			srv_monitor_file = os_file_create_tmpfile(NULL);

			if (!srv_monitor_file) {
				return(srv_init_abort(DB_ERROR));
			}
		}

		mutex_create(LATCH_ID_SRV_DICT_TMPFILE,
			     &srv_dict_tmpfile_mutex);

		srv_dict_tmpfile = os_file_create_tmpfile(NULL);

		if (!srv_dict_tmpfile) {
			return(srv_init_abort(DB_ERROR));
		}

		mutex_create(LATCH_ID_SRV_MISC_TMPFILE,
			     &srv_misc_tmpfile_mutex);

		srv_misc_tmpfile = os_file_create_tmpfile(NULL);

		if (!srv_misc_tmpfile) {
			return(srv_init_abort(DB_ERROR));
		}
	}

	srv_n_file_io_threads = srv_n_read_io_threads;

	srv_n_file_io_threads += srv_n_write_io_threads;

	if (!srv_read_only_mode) {
		/* Add the log and ibuf IO threads. */
		srv_n_file_io_threads += 2;
	} else {
		ib::info() << "Disabling background log and ibuf IO write"
			<< " threads.";
	}

	ut_a(srv_n_file_io_threads <= SRV_MAX_N_IO_THREADS);

	if (!os_aio_init(srv_n_read_io_threads,
			 srv_n_write_io_threads,
			 SRV_MAX_N_PENDING_SYNC_IOS)) {

		ib::error() << "Cannot initialize AIO sub-system";

		return(srv_init_abort(DB_ERROR));
	}

	fil_init(srv_file_per_table ? 50000 : 5000, srv_max_n_open_files);

	double	size;
	char	unit;

	if (srv_buf_pool_size >= 1024 * 1024 * 1024) {
		size = ((double) srv_buf_pool_size) / (1024 * 1024 * 1024);
		unit = 'G';
	} else {
		size = ((double) srv_buf_pool_size) / (1024 * 1024);
		unit = 'M';
	}

	double	chunk_size;
	char	chunk_unit;

	if (srv_buf_pool_chunk_unit >= 1024 * 1024 * 1024) {
		chunk_size = srv_buf_pool_chunk_unit / 1024.0 / 1024 / 1024;
		chunk_unit = 'G';
	} else {
		chunk_size = srv_buf_pool_chunk_unit / 1024.0 / 1024;
		chunk_unit = 'M';
	}

	ib::info() << "Initializing buffer pool, total size = "
		<< size << unit << ", instances = " << srv_buf_pool_instances
		<< ", chunk size = " << chunk_size << chunk_unit;

	err = buf_pool_init(srv_buf_pool_size, srv_buf_pool_instances);

	if (err != DB_SUCCESS) {
		ib::error() << "Cannot allocate memory for the buffer pool";

		return(srv_init_abort(DB_ERROR));
	}

	ib::info() << "Completed initialization of buffer pool";

#ifdef UNIV_DEBUG
	/* We have observed deadlocks with a 5MB buffer pool but
	the actual lower limit could very well be a little higher. */

	if (srv_buf_pool_size <= 5 * 1024 * 1024) {

		ib::info() << "Small buffer pool size ("
			<< srv_buf_pool_size / 1024 / 1024
			<< "M), the flst_validate() debug function can cause a"
			<< " deadlock if the buffer pool fills up.";
	}
#endif /* UNIV_DEBUG */

	fsp_init();
	log_init();

	recv_sys_create();
	recv_sys_init(buf_pool_get_curr_size());
	trx_sys_create();
	lock_sys_create(srv_lock_table_size);
	srv_start_state_set(SRV_START_STATE_LOCK_SYS);

	/* Create i/o-handler threads: */

	/* For read only mode, we don't need ibuf and log I/O thread.
	Please see innobase_start_or_create_for_mysql() */
	ulint	start = (srv_read_only_mode) ? 0 : 2;

	for (ulint t = 0; t < srv_n_file_io_threads; ++t) {

		if (t < start) {
			if (t == 0) {
		                os_thread_create(
					io_ibuf_thread_key,
					io_handler_thread,
					t);
			} else {
				ut_ad(t == 1);
		                os_thread_create(
					io_log_thread_key,
					io_handler_thread, t);
			}
		} else if (t >= start && t < (start + srv_n_read_io_threads)) {

		        os_thread_create(
				io_read_thread_key,
				io_handler_thread, t);

		} else if (t >= (start + srv_n_read_io_threads)
			   && t < (start + srv_n_read_io_threads
				   + srv_n_write_io_threads)) {

		        os_thread_create(
				io_write_thread_key,
				io_handler_thread, t);
		} else {
		        os_thread_create(
				io_handler_thread_key,
				io_handler_thread, t);
		}
	}

	/* Even in read-only mode there could be flush job generated by
	intrinsic table operations. */
	buf_flush_page_cleaner_init(srv_n_page_cleaners);

	srv_start_state_set(SRV_START_STATE_IO);

	srv_startup_is_before_trx_rollback_phase = !create_new_db;

	if (create_new_db) {
		recv_sys_free();
	}

	/* Open or create the data files. */
	page_no_t	sum_of_new_sizes;

	err = srv_sys_space.open_or_create(
		false, create_new_db, &sum_of_new_sizes, &flushed_lsn);

	/* FIXME: This can be done earlier, but we now have to wait for
	checking of system tablespace. */
	dict_persist_init();

	switch (err) {
	case DB_SUCCESS:
		break;
	case DB_CANNOT_OPEN_FILE:
		ib::error()
			<< "Could not open or create the system tablespace. If"
			" you tried to add new data files to the system"
			" tablespace, and it failed here, you should now"
			" edit innodb_data_file_path in my.cnf back to what"
			" it was, and remove the new ibdata files InnoDB"
			" created in this failed attempt. InnoDB only wrote"
			" those files full of zeros, but did not yet use"
			" them in any way. But be careful: do not remove"
			" old data files which contain your precious data!";
		/* fall through */
	default:

		/* Other errors might come from
		Datafile::validate_first_page() */

		return(srv_init_abort(err));
	}

	dirnamelen = strlen(srv_log_group_home_dir);
	ut_a(dirnamelen < (sizeof logfilename) - 10 - sizeof "ib_logfile");
	memcpy(logfilename, srv_log_group_home_dir, dirnamelen);

	/* Add a path separator if needed. */
	if (dirnamelen && logfilename[dirnamelen - 1] != OS_PATH_SEPARATOR) {
		logfilename[dirnamelen++] = OS_PATH_SEPARATOR;
	}

	srv_log_file_size_requested = srv_log_file_size;

	if (create_new_db) {

		buf_flush_sync_all_buf_pools();

		flushed_lsn = log_get_lsn();

		err = create_log_files(
			logfilename, dirnamelen, flushed_lsn, logfile0);

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}
	} else {
		for (i = 0; i < SRV_N_LOG_FILES_MAX; i++) {
			os_offset_t	size;
			os_file_stat_t	stat_info;

			sprintf(logfilename + dirnamelen,
				"ib_logfile%u", i);

			err = os_file_get_status(
				logfilename, &stat_info, false,
				srv_read_only_mode);

			if (err == DB_NOT_FOUND) {
				if (i == 0) {
					if (flushed_lsn
					    < static_cast<lsn_t>(1000)) {
						ib::error()
							<< "Cannot create"
							" log files because"
							" data files are"
							" corrupt or the"
							" database was not"
							" shut down cleanly"
							" after creating"
							" the data files.";
						return(srv_init_abort(
							DB_ERROR));
					}

					err = create_log_files(
						logfilename, dirnamelen,
						flushed_lsn, logfile0);

					if (err != DB_SUCCESS) {
						return(srv_init_abort(err));
					}

					create_log_files_rename(
						logfilename, dirnamelen,
						flushed_lsn, logfile0);

					/* Suppress the message about
					crash recovery. */
					flushed_lsn = log_get_lsn();
					goto files_checked;
				} else if (i < 2) {
					/* must have at least 2 log files */
					ib::error() << "Only one log file"
						" found.";
					return(srv_init_abort(err));
				}

				/* opened all files */
				break;
			}

			if (!srv_file_check_mode(logfilename)) {
				return(srv_init_abort(DB_ERROR));
			}

			err = open_log_file(&files[i], logfilename, &size);

			if (err != DB_SUCCESS) {
				return(srv_init_abort(err));
			}

			ut_a(size != (os_offset_t) -1);

			if (size & ((1 << UNIV_PAGE_SIZE_SHIFT) - 1)) {

				ib::error() << "Log file " << logfilename
					<< " size " << size << " is not a"
					" multiple of innodb_page_size";
				return(srv_init_abort(DB_ERROR));
			}

			size >>= UNIV_PAGE_SIZE_SHIFT;

			if (i == 0) {
				srv_log_file_size = size;
			} else if (size != srv_log_file_size) {

				ib::error() << "Log file " << logfilename
					<< " is of different size "
					<< (size << UNIV_PAGE_SIZE_SHIFT)
					<< " bytes than other log files "
					<< (srv_log_file_size
					    << UNIV_PAGE_SIZE_SHIFT)
					<< " bytes!";
				return(srv_init_abort(DB_ERROR));
			}
		}

		srv_n_log_files_found = i;

		/* Create the in-memory file space objects. */

		sprintf(logfilename + dirnamelen, "ib_logfile%u", 0);

		/* Disable the doublewrite buffer for log files. */
		fil_space_t*	log_space = fil_space_create(
			"innodb_redo_log",
			dict_sys_t::log_space_first_id,
			fsp_flags_set_page_size(0, univ_page_size),
			FIL_TYPE_LOG);

		ut_a(fil_validate());
		ut_a(log_space);

		/* srv_log_file_size is measured in pages; if page size is 16KB,
		then we have a limit of 64TB on 32 bit systems */
		ut_a(srv_log_file_size <= PAGE_NO_MAX);

		for (unsigned j = 0; j < i; j++) {
			sprintf(logfilename + dirnamelen, "ib_logfile%u", j);

			if (!fil_node_create(
				logfilename,
				static_cast<page_no_t>(srv_log_file_size),
				log_space, false, false)) {
				return(srv_init_abort(DB_ERROR));
			}
		}

		if (!log_group_init(0, i, srv_log_file_size * UNIV_PAGE_SIZE,
				    dict_sys_t::log_space_first_id)) {
			return(srv_init_abort(DB_ERROR));
		}

		/* Read the first log file header to get the encryption
		information if it exist. */
		if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
		    if (!log_read_encryption()) {
			return(srv_init_abort(DB_ERROR));
		    }
		}
	}

files_checked:
	/* Open all log files and data files in the system
	tablespace: we keep them open until database
	shutdown */

	fil_open_log_and_system_tablespace_files();

	if (create_new_db) {
		ut_a(!srv_read_only_mode);

		err = srv_undo_tablespaces_init(true);

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		mtr_start(&mtr);

		bool ret = fsp_header_init(0, sum_of_new_sizes, &mtr, false);

		mtr_commit(&mtr);

		if (!ret) {
			return(srv_init_abort(DB_ERROR));
		}

		/* To maintain backward compatibility we create only
		the first rollback segment before the double write buffer.
		All the remaining rollback segments will be created later,
		after the double write buffers haves been created. */
		trx_sys_create_sys_pages();

		/* Finish building new undo tablespaces by adding header
		pages and rollback segments. Write the space_ids and
		header page numbers to the TRX_SYS page created above.
		Then delete any undo truncation log files and clear the
		construction list. This list includes any tablespace
		newly created or fixed-up. */
		srv_undo_tablespaces_construct(create_new_db);
		srv_undo_tablespaces_construction_list_clear();

		purge_queue = trx_sys_init_at_db_start();

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, purge_queue);

		err = dict_create();

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		srv_create_sdi_indexes();

		buf_flush_sync_all_buf_pools();

		flushed_lsn = log_get_lsn();

		fil_write_flushed_lsn(flushed_lsn);

		create_log_files_rename(
			logfilename, dirnamelen, flushed_lsn, logfile0);

		buf_flush_sync_all_buf_pools();
	} else {
		/* Invalidate the buffer pool to ensure that we reread
		the page that we read above, during recovery.
		Note that this is not as heavy weight as it seems. At
		this point there will be only ONE page in the buf_LRU
		and there must be no page in the buf_flush list. */
		buf_pool_invalidate();

		/* We always try to do a recovery, even if the database had
		been shut down normally: this is the normal startup path */

		err = recv_recovery_from_checkpoint_start(flushed_lsn);

		recv_sys->dblwr.pages.clear();

		if (err == DB_SUCCESS) {
			/* Initialize the change buffer. */
			err = dict_boot();
		}

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
			/* Apply the hashed log records to the
			respective file pages, for the last batch of
			recv_group_scan_log_recs(). */

			recv_apply_hashed_log_recs(true);

			if (recv_sys->found_corrupt_log  == true) {
				err = DB_ERROR;
				return(srv_init_abort(err));
			}

			DBUG_PRINT("ib_log", ("apply completed"));

			/* Check and print if there were any tablespaces
			which had redo log records but we couldn't apply
			them because the filenames were missing. */
		}

		if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
			/* Recovery complete, start verifying the
			page LSN on read. */
			recv_lsn_checks_on = true;
		}

		/* We have gone through the redo log, now check if all the
		tablespaces were found and recovered. */

		if (srv_force_recovery == 0
		    && fil_check_missing_tablespaces()) {

			ib::error()
				<< "Use --innodb-scan-directories to find the"
				<< " the tablespace files. If that fails then use"
				<< " --innodb-force-recvovery=1 to ignore"
				<< " this and to permanently lose all changes"
				<< " to the missing tablespace(s)";

			/* Set the abort flag to true. */
			void*	ptr = recv_recovery_from_checkpoint_finish(true);
			ut_a(ptr == nullptr);

			return(srv_init_abort(DB_ERROR));
		}

		/* We have successfully recovered from the redo log. The
		data dictionary should now be readable. */

		if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO
		    && recv_needed_recovery) {

			trx_sys_print_mysql_binlog_offset();
		}

		if (recv_sys->found_corrupt_log) {
			ib::warn()
				<< "The log file may have been corrupt and it"
				" is possible that the log scan or parsing"
				" did not proceed far enough in recovery."
				" Please run CHECK TABLE on your InnoDB tables"
				" to check that they are ok!"
				" It may be safest to recover your"
				" InnoDB database from a backup!";
		}

		if (!srv_force_recovery && !srv_read_only_mode) {
			buf_flush_sync_all_buf_pools();
		}

		srv_dict_metadata = recv_recovery_from_checkpoint_finish(false);

		err = srv_undo_tablespaces_init(false);

		if (err != DB_SUCCESS
		    && srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {

			if (err == DB_TABLESPACE_NOT_FOUND) {
				/* A tablespace was not found.
				The user must force recovery. */

				srv_fatal_error();
			}

			return(srv_init_abort(err));
		}

		/* Finish building recreated undo tablespaces by adding
		header pages and rollback segments. Write the space_ids
		and header page numbers to the TRX_SYS page.
		Then delete any undo truncation log files and clear the
		construction list. This list includes any tablespace
		fixed-up because of an unfinished truncate. */
		srv_undo_tablespaces_construct(create_new_db);
		srv_undo_tablespaces_construction_list_clear();

		if (!srv_force_recovery
		    && !recv_sys->found_corrupt_log
		    && (srv_log_file_size_requested != srv_log_file_size
			|| srv_n_log_files_found != srv_n_log_files)) {

			/* Prepare to replace the redo log files. */

			if (srv_read_only_mode) {
				ib::error() << "Cannot resize log files"
					" in read-only mode.";
				return(srv_init_abort(DB_READ_ONLY));
			}

			/* Prepare to delete the old redo log files */
			flushed_lsn = srv_prepare_to_delete_redo_log_files(i);

			/* Prohibit redo log writes from any other
			threads until creating a log checkpoint at the
			end of create_log_files(). */
			ut_d(log_sys->disable_redo_writes = true);

			ut_ad(!buf_pool_check_no_pending_io());

			RECOVERY_CRASH(3);

			/* Stamp the LSN to the data files. */
			fil_write_flushed_lsn(flushed_lsn);

			RECOVERY_CRASH(4);

			/* Close and free the redo log files, so that
			we can replace them. */
			fil_close_log_files(true);

			RECOVERY_CRASH(5);

			/* Free the old log file space. */
			log_group_close_all();

			ib::warn() << "Starting to delete and rewrite log"
				" files.";

			srv_log_file_size = srv_log_file_size_requested;

			err = create_log_files(
				logfilename, dirnamelen, flushed_lsn,
				logfile0);

			if (err != DB_SUCCESS) {
				return(srv_init_abort(err));
			}

			create_log_files_rename(
				logfilename, dirnamelen, flushed_lsn,
				logfile0);
		}

		if (sum_of_new_sizes > 0) {

			/* New data file(s) were added */
			mtr_start(&mtr);

			fsp_header_inc_size(0, sum_of_new_sizes, &mtr);

			mtr_commit(&mtr);

			/* Immediately write the log record about
			increased tablespace size to disk, so that it
			is durable even if mysqld would crash
			quickly */

			log_buffer_flush_to_disk();
		}

		purge_queue = trx_sys_init_at_db_start();

		if (srv_is_upgrade_mode && !purge_queue->empty()) {
			ib::info() << "Undo from 5.7 found. It will be purged";
			srv_upgrade_old_undo_found = true;
		}

		DBUG_EXECUTE_IF("check_no_undo",
				ut_ad(purge_queue->empty());
				);

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys and trx lists were
		initialized in trx_sys_init_at_db_start(). */
		trx_purge_sys_create(srv_n_purge_threads, purge_queue);
	}

	/* Open temp-tablespace and keep it open until shutdown. */
	err = srv_open_tmp_tablespace(create_new_db, &srv_tmp_space);
	if (err != DB_SUCCESS) {
		return(srv_init_abort(err));
	}

	/* Create the doublewrite buffer to a new tablespace */
	if (buf_dblwr == NULL && !buf_dblwr_create()) {
		return(srv_init_abort(DB_ERROR));
	}

	/* Here the double write buffer has already been created and so
	any new rollback segments will be allocated after the double
	write buffer. The default segment should already exist.
	We create the new segments only if it's a new database or
	the database was shutdown cleanly. */

	/* Note: When creating the extra rollback segments during an upgrade
	we violate the latching order, even if the change buffer is empty.
	We make an exception in sync0sync.cc and check srv_is_being_started
	for that violation. It cannot create a deadlock because we are still
	running in single threaded mode essentially. Only the IO threads
	should be running at this stage. */

	ut_a(srv_rollback_segments > 0);
	ut_a(srv_rollback_segments <= TRX_SYS_N_RSEGS);
	ut_a(srv_tmp_rollback_segments > 0);
	ut_a(srv_tmp_rollback_segments <= TRX_SYS_N_RSEGS);

	/* Create temporary rollback segments. */
	if (!srv_read_only_mode) {
		ulint	n_rsegs = trx_rsegs_create_in_temp_space();
		if (n_rsegs < srv_tmp_rollback_segments) {
			ib::error() << "Could not create all rollback"
				" segments in the temporary tablespace."
				" The disk may be running of out of space";
			return(srv_init_abort(DB_ERROR));
		}
	}

	/* Create more rollback segments in the system tablespace if
	srv_undo_tablespaces = 0 since we allow srv_rollback_segments
	to be different from the previous value.
	When srv_undo_tablespaces > 0, we have already created the number
	of rollback segments specified by srv_rollback_segments. */
	if (!trx_sys_create_additional_rsegs(recv_needed_recovery)) {
		return(srv_init_abort(DB_ERROR));
	}

	srv_startup_is_before_trx_rollback_phase = false;

	if (!srv_read_only_mode) {
		if (create_new_db) {
			srv_buffer_pool_load_at_startup = FALSE;
		}

		/* Create the thread which watches the timeouts
		for lock waits */
		os_thread_create(
			srv_lock_timeout_thread_key,
			lock_wait_timeout_thread);

		/* Create the thread which warns of long semaphore waits */
		os_thread_create(
			srv_error_monitor_thread_key,
			srv_error_monitor_thread);

		/* Create the thread which prints InnoDB monitor info */
		os_thread_create(
			srv_monitor_thread_key,
			srv_monitor_thread);


		srv_start_state_set(SRV_START_STATE_MONITOR);
	}

	srv_sys_tablespaces_open = true;

	/* Rotate the encryption key for recovery. It's because
	server could crash in middle of key rotation. Some tablespace
	didn't complete key rotation. Here, we will resume the
	rotation. */
	if (!srv_read_only_mode && !create_new_db
	    && srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
		fil_encryption_rotate();
	}

	srv_is_being_started = false;

	ut_a(trx_purge_state() == PURGE_STATE_INIT);

	/* wake main loop of page cleaner up */
	os_event_set(buf_flush_event);

	sum_of_data_file_sizes = srv_sys_space.get_sum_of_sizes();
	ut_a(sum_of_new_sizes != FIL_NULL);

	tablespace_size_in_header = fsp_header_get_tablespace_size();

	if (!srv_read_only_mode
	    && !srv_sys_space.can_auto_extend_last_file()
	    && sum_of_data_file_sizes != tablespace_size_in_header) {

		ib::error() << "Tablespace size stored in header is "
			<< tablespace_size_in_header << " pages, but the sum"
			" of data file sizes is " << sum_of_data_file_sizes
			<< " pages";

		if (srv_force_recovery == 0
		    && sum_of_data_file_sizes < tablespace_size_in_header) {
			/* This is a fatal error, the tail of a tablespace is
			missing */

			ib::error()
				<< "Cannot start InnoDB."
				" The tail of the system tablespace is"
				" missing. Have you edited"
				" innodb_data_file_path in my.cnf in an"
				" inappropriate way, removing"
				" ibdata files from there?"
				" You can set innodb_force_recovery=1"
				" in my.cnf to force"
				" a startup if you are trying"
				" to recover a badly corrupt database.";

			return(srv_init_abort(DB_ERROR));
		}
	}

	if (!srv_read_only_mode
	    && srv_sys_space.can_auto_extend_last_file()
	    && sum_of_data_file_sizes < tablespace_size_in_header) {

		ib::error() << "Tablespace size stored in header is "
			<< tablespace_size_in_header << " pages, but the sum"
			" of data file sizes is only "
			<< sum_of_data_file_sizes << " pages";

		if (srv_force_recovery == 0) {

			ib::error()
				<< "Cannot start InnoDB. The tail of"
				" the system tablespace is"
				" missing. Have you edited"
				" innodb_data_file_path in my.cnf in an"
				" InnoDB: inappropriate way, removing"
				" ibdata files from there?"
				" You can set innodb_force_recovery=1"
				" in my.cnf to force"
				" InnoDB: a startup if you are trying to"
				" recover a badly corrupt database.";

			return(srv_init_abort(DB_ERROR));
		}
	}

	if (srv_print_verbose_log) {
		ib::info() << INNODB_VERSION_STR
			<< " started; log sequence number "
			<< srv_start_lsn;
	}

	return(DB_SUCCESS);
}

/** Applier of dynamic metadata */
struct metadata_applier
{
        /** Default constructor */
        metadata_applier() {}
        /** Visitor.
        @param[in]      table   table to visit */
        void operator()(dict_table_t* table) const
        {
                ut_ad(dict_sys->dynamic_metadata != NULL);
		ib_uint64_t	autoinc = table->autoinc;
                dict_table_load_dynamic_metadata(table);
		/* For those tables which were not opened by
		ha_innobase::open() and not initialized by
		innobase_initialize_autoinc(), the next counter should be
		advanced properly */
		if (autoinc != table->autoinc && table->autoinc != ~0ULL) {
			++table->autoinc;
		}
        }
};

/** Apply the dynamic metadata to all tables */
static
void
apply_dynamic_metadata()
{
        const metadata_applier  applier;

        dict_sys->for_each_table(applier);

        if (srv_dict_metadata != NULL) {
                srv_dict_metadata->apply();
                UT_DELETE(srv_dict_metadata);
                srv_dict_metadata = NULL;
        }
}

/** On a restart, initialize the remaining InnoDB subsystems so that
any tables (including data dictionary tables) can be accessed. */
void
srv_dict_recover_on_restart()
{
	apply_dynamic_metadata();

	trx_resurrect_locks();

	/* Roll back any recovered data dictionary transactions, so
	that the data dictionary tables will be free of any locks.
	The data dictionary latch should guarantee that there is at
	most one data dictionary transaction active at a time. */
	if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO) {
		trx_rollback_or_clean_recovered(FALSE);
	}

	if (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
		/* Open or Create SYS_TABLESPACES and SYS_DATAFILES
		so that tablespace names and other metadata can be
		found. */
		srv_sys_tablespaces_open = true;

#ifdef INNODB_NO_NEW_DD
		dberr_t	err = dict_create_or_check_sys_tablespace();

		ut_a(err == DB_SUCCESS); // FIXME: remove in WL#9535
	}

	/* We can't start any (DDL) transactions if UNDO logging has
	been disabled. */
	if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO
	    && !srv_read_only_mode) {

		/* Drop partially created indexes. */
		row_merge_drop_temp_indexes();

		/* Drop any auxiliary tables that were not
		dropped when the parent table was
		dropped. This can happen if the parent table
		was dropped but the server crashed before the
		auxiliary tables were dropped. */
		fts_drop_orphaned_tables();
#endif /* INNODB_NO_NEW_DD */
	}
}

/** Start purge threads. During upgrade we start
purge threads early to apply purge. */
void
srv_start_purge_threads()
{
	/* Start purge threads only if they are not started
	earlier. */
	if (srv_start_state_is_set(SRV_START_STATE_PURGE)) {
		ut_ad(srv_is_upgrade_mode);
		return;
	}

	os_thread_create(
		srv_purge_thread_key,
		srv_purge_coordinator_thread);

	/* We've already created the purge coordinator thread above. */
	for (ulong i = 1; i < srv_n_purge_threads; ++i) {

		os_thread_create(
			srv_worker_thread_key,
			srv_worker_thread);
	}

	srv_start_wait_for_purge_to_start();

	srv_start_state_set(SRV_START_STATE_PURGE);
}

/** Start up the remaining InnoDB service threads.
@param[in]	bootstrap	True if this is in bootstrap */
void
srv_start_threads(
	bool	bootstrap)
{
	os_thread_create(buf_resize_thread_key, buf_resize_thread);

	if (srv_read_only_mode) {
		purge_sys->state = PURGE_STATE_DISABLED;
		return;
	}

	if (!bootstrap && srv_force_recovery < SRV_FORCE_NO_TRX_UNDO
	    && trx_sys_need_rollback()) {
		/* Rollback all recovered transactions that are
		not in committed nor in XA PREPARE state. */
		trx_rollback_or_clean_is_active = true;

		os_thread_create(
			trx_recovery_rollback_thread_key,
			trx_recovery_rollback_thread);
	}

	/* Create the master thread which does purge and other utility
	operations */

	os_thread_create(srv_master_thread_key, srv_master_thread);


	srv_start_state_set(SRV_START_STATE_MASTER);

	if (srv_force_recovery < SRV_FORCE_NO_BACKGROUND) {
		srv_start_purge_threads();
	} else {
		purge_sys->state = PURGE_STATE_DISABLED;
	}

	if (srv_force_recovery == 0) {
		/* In the insert buffer we may have even bigger tablespace
		id's, because we may have dropped those tablespaces, but
		insert buffer merge has not had time to clean the records from
		the ibuf tree. */

		ibuf_update_max_tablespace_id();
	}

	/* Create the buffer pool dump/load thread */
	os_thread_create(buf_dump_thread_key, buf_dump_thread);

	dict_stats_thread_init();

	/* Create the dict stats gathering thread */
	os_thread_create(dict_stats_thread_key, dict_stats_thread);

	/* Create the thread that will optimize the FTS sub-system. */
	fts_optimize_init();

	srv_start_state_set(SRV_START_STATE_STAT);
}

#if 0
/********************************************************************
Sync all FTS cache before shutdown */
static
void
srv_fts_close(void)
/*===============*/
{
	dict_table_t*	table;

	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	     table; table = UT_LIST_GET_NEXT(table_LRU, table)) {
		fts_t*          fts = table->fts;

		if (fts != NULL) {
			fts_sync_table(table);
		}
	}

	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU);
	     table; table = UT_LIST_GET_NEXT(table_LRU, table)) {
		fts_t*          fts = table->fts;

		if (fts != NULL) {
			fts_sync_table(table);
		}
	}
}
#endif

/** Shut down all InnoDB background tasks that may look up objects in
the data dictionary. */
void
srv_pre_dd_shutdown()
{
	ut_ad(!srv_is_being_shutdown);

	if (srv_read_only_mode) {
		/* In read-only mode, no background tasks should
		access the data dictionary. */
		ut_d(srv_is_being_shutdown = true);
		return;
	}

	if (srv_start_state_is_set(SRV_START_STATE_STAT)) {
		fts_optimize_shutdown();
		dict_stats_shutdown();
	}

	/* On slow shutdown, we have to wait for background thread
	doing the rollback to finish first because it can add undo to
	purge. So exit this thread before initiating purge shutdown. */
	while (srv_fast_shutdown == 0 && trx_rollback_or_clean_is_active) {
		/* we should wait until rollback after recovery end
		for slow shutdown */
		os_thread_sleep(100000);
	}

	/* Here, we will only shut down the tasks that may be looking up
	tables or other objects in the Global Data Dictionary.
	The following background tasks will not be affected:
	* background rollback of recovered transactions (those table
	definitions were already looked up IX-locked at server startup)
	* change buffer merge (until we replace the IBUF_DUMMY objects
	with access to the data dictionary)
	* I/O subsystem (page cleaners, I/O threads, redo log) */

	srv_shutdown_state = SRV_SHUTDOWN_CLEANUP;
	srv_purge_wakeup();

	if (srv_start_state_is_set(SRV_START_STATE_STAT)) {
		os_event_set(dict_stats_event);
	}

	for (ulint count = 1;;) {
		bool	wait = srv_purge_threads_active();

		if (wait) {
			srv_purge_wakeup();
			if (srv_print_verbose_log
			    && (count % 600) == 0) {
				ib::info() << "Waiting for purge to complete";
			}
		} else {
			switch (trx_purge_state()) {
			case PURGE_STATE_INIT:
			case PURGE_STATE_EXIT:
			case PURGE_STATE_DISABLED:
				srv_start_state &= ~SRV_START_STATE_PURGE;
				break;
			case PURGE_STATE_RUN:
			case PURGE_STATE_STOP:
				ut_ad(0);
			}
		}

		if (srv_dict_stats_thread_active) {
			wait = true;

			os_event_set(dict_stats_event);

			if (srv_print_verbose_log && ((count % 600) == 0)) {
				ib::info() << "Waiting for dict_stats_thread"
					" to exit";
			}
		}

		if (!wait) {
			break;
		}

		count++;
		os_thread_sleep(100000);
	}

	if (srv_start_state_is_set(SRV_START_STATE_STAT)) {
		dict_stats_thread_deinit();
	}

	ut_d(srv_is_being_shutdown = true);
}

/** Shut down the InnoDB database. */
void
srv_shutdown()
{
	ut_ad(srv_is_being_shutdown);
	/* 1. Flush the buffer pool to disk, write the current lsn to
	the tablespace header(s), and copy all log data to archive.
	The step 1 is the real InnoDB shutdown. The remaining steps 2 - ...
	just free data structures after the shutdown. */

	logs_empty_and_mark_files_at_shutdown();

	if (srv_conc_get_active_threads() != 0) {
		ib::warn() << "Query counter shows "
			<< srv_conc_get_active_threads() << " queries still"
			" inside InnoDB at shutdown";
	}

	/* 2. Make all threads created by InnoDB to exit */
	srv_shutdown_all_bg_threads();

	if (srv_monitor_file) {
		fclose(srv_monitor_file);
		srv_monitor_file = 0;
		if (srv_monitor_file_name) {
			unlink(srv_monitor_file_name);
			ut_free(srv_monitor_file_name);
		}
		mutex_free(&srv_monitor_file_mutex);
	}

	if (srv_dict_tmpfile) {
		fclose(srv_dict_tmpfile);
		srv_dict_tmpfile = 0;
		mutex_free(&srv_dict_tmpfile_mutex);
	}

	if (srv_misc_tmpfile) {
		fclose(srv_misc_tmpfile);
		srv_misc_tmpfile = 0;
		mutex_free(&srv_misc_tmpfile_mutex);
	}

	/* This must be disabled before closing the buffer pool
	and closing the data dictionary.  */
	btr_search_disable(true);

	ibuf_close();
	log_shutdown();
	trx_sys_close();
	lock_sys_close();
	trx_pool_close();

	dict_close();
	dict_persist_close();
	btr_search_sys_free();
	trx_sys_undo_spaces_deinit();

	UT_DELETE(srv_dict_metadata);

	/* 3. Free all InnoDB's own mutexes and the os_fast_mutexes inside
	them */
	os_aio_free();
	que_close();
	row_mysql_close();
	srv_free();
	fil_close();

	/* 4. Free all allocated memory */

	pars_lexer_close();
	buf_pool_free(srv_buf_pool_instances);

	/* 6. Free the thread management resoruces. */
	os_thread_close();

	/* 7. Free the synchronisation infrastructure. */
	sync_check_close();

	if (srv_print_verbose_log) {
		ib::info() << "Shutdown completed; log sequence number "
			<< srv_shutdown_lsn;
	}

	srv_start_has_been_called = false;
	ut_d(srv_is_being_shutdown = false);
	srv_shutdown_state = SRV_SHUTDOWN_NONE;
	srv_start_state = SRV_START_STATE_NONE;
}
#endif /* !UNIV_HOTBACKUP */

#if 0 // TODO: Enable this in WL#6608
/********************************************************************
Signal all per-table background threads to shutdown, and wait for them to do
so. */
static
void
srv_shutdown_table_bg_threads(void)
/*===============================*/
{
	dict_table_t*	table;
	dict_table_t*	first;
	dict_table_t*	last = NULL;

	mutex_enter(&dict_sys->mutex);

	/* Signal all threads that they should stop. */
	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	first = table;
	while (table) {
		dict_table_t*	next;
		fts_t*		fts = table->fts;

		if (fts != NULL) {
			fts_start_shutdown(table, fts);
		}

		next = UT_LIST_GET_NEXT(table_LRU, table);

		if (!next) {
			last = table;
		}

		table = next;
	}

	/* We must release dict_sys->mutex here; if we hold on to it in the
	loop below, we will deadlock if any of the background threads try to
	acquire it (for example, the FTS thread by calling que_eval_sql).

	Releasing it here and going through dict_sys->table_LRU without
	holding it is safe because:

	 a) MySQL only starts the shutdown procedure after all client
	 threads have been disconnected and no new ones are accepted, so no
	 new tables are added or old ones dropped.

	 b) Despite its name, the list is not LRU, and the order stays
	 fixed.

	To safeguard against the above assumptions ever changing, we store
	the first and last items in the list above, and then check that
	they've stayed the same below. */

	mutex_exit(&dict_sys->mutex);

	/* Wait for the threads of each table to stop. This is not inside
	the above loop, because by signaling all the threads first we can
	overlap their shutting down delays. */
	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);
	ut_a(first == table);
	while (table) {
		dict_table_t*	next;
		fts_t*		fts = table->fts;

		if (fts != NULL) {
			fts_shutdown(table, fts);
		}

		next = UT_LIST_GET_NEXT(table_LRU, table);

		if (table == last) {
			ut_a(!next);
		}

		table = next;
	}
}
#endif

/** Get the encryption-data filename from the table name for a
single-table tablespace.
@param[in]	table		table object
@param[out]	filename	filename
@param[in]	max_len		filename max length */
void
srv_get_encryption_data_filename(
	dict_table_t*	table,
	char*		filename,
	ulint		max_len)
{
	ulint		len;
	char*		path;

	/* Make sure the data_dir_path is set. */
	dd_get_and_save_data_dir_path<dd::Table>(table, NULL, false);

	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		ut_a(table->data_dir_path);

		path = fil_make_filepath(
			table->data_dir_path, table->name.m_name, CFP, true);
	} else {
		path = fil_make_filepath(NULL, table->name.m_name, CFP, false);
	}

	ut_a(path);
	len = ut_strlen(path);
	ut_a(max_len >= len);

	strcpy(filename, path);

	ut_free(path);
}

/** Call exit(3) */
void
srv_fatal_error()
{

	ib::error() << "Cannot continue operation.";

	fflush(stderr);

	ut_d(innodb_calling_exit = true);

	srv_shutdown_all_bg_threads();

	exit(3);
}
