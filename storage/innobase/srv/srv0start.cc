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

#include "my_global.h"

#include "ha_prototypes.h"

#include "mysqld.h"
#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/psi.h"

#include "row0ftsort.h"
#include "ut0mem.h"
#include "mem0mem.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "buf0buf.h"
#include "buf0dump.h"
#include "os0file.h"
#include "os0thread.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "rem0rec.h"
#include "mtr0mtr.h"
#include "log0log.h"
#include "log0recv.h"
#include "page0page.h"
#include "page0cur.h"
#include "trx0trx.h"
#include "trx0sys.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "rem0rec.h"
#include "ibuf0ibuf.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "fsp0sysspace.h"
#include "row0trunc.h"
#ifndef UNIV_HOTBACKUP
# include "trx0rseg.h"
# include "os0proc.h"
# include "buf0flu.h"
# include "buf0rea.h"
# include "dict0boot.h"
# include "dict0load.h"
# include "dict0stats_bg.h"
# include "que0que.h"
# include "usr0sess.h"
# include "lock0lock.h"
# include "trx0roll.h"
# include "trx0purge.h"
# include "lock0lock.h"
# include "pars0pars.h"
# include "btr0sea.h"
# include "rem0cmp.h"
# include "dict0crea.h"
# include "row0ins.h"
# include "row0sel.h"
# include "row0upd.h"
# include "row0row.h"
# include "row0mysql.h"
# include "row0trunc.h"
# include "btr0pcur.h"
# include "os0event.h"
# include "zlib.h"
# include "ut0crc32.h"
# include "ut0new.h"

#ifdef HAVE_LZO1X
#include <lzo/lzo1x.h>
extern bool srv_lzo_disabled;
#endif /* HAVE_LZO1X */

/** Log sequence number immediately after startup */
lsn_t	srv_start_lsn;
/** Log sequence number at shutdown */
lsn_t	srv_shutdown_lsn;

/** TRUE if a raw partition is in use */
ibool	srv_start_raw_disk_in_use = FALSE;

/** UNDO tablespaces starts with space id. */
ulint	srv_undo_space_id_start;

/** Number of IO threads to use */
ulint	srv_n_file_io_threads = 0;

/** TRUE if the server is being started, before rolling back any
incomplete transactions */
bool	srv_startup_is_before_trx_rollback_phase = false;
/** TRUE if the server is being started */
bool	srv_is_being_started = false;
/** TRUE if SYS_TABLESPACES is available for lookups */
bool	srv_sys_tablespaces_open = false;
/** TRUE if the server was successfully started */
ibool	srv_was_started = FALSE;
/** TRUE if innobase_start_or_create_for_mysql() has been called */
static ibool	srv_start_has_been_called = FALSE;

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

/** io_handler_thread parameters for thread identification */
static ulint		n[SRV_MAX_N_IO_THREADS + 6];
/** io_handler_thread identifiers, 32 is the maximum number of purge threads  */
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6 + 32];

/** Name of srv_monitor_file */
static char*	srv_monitor_file_name;
#endif /* !UNIV_HOTBACKUP */

/** Minimum expected tablespace size. (10M) */
static const ulint MIN_EXPECTED_TABLESPACE_SIZE = 5 * 1024 * 1024;

/** */
#define SRV_MAX_N_PENDING_SYNC_IOS	100

#ifdef UNIV_PFS_THREAD
/* Keys to register InnoDB threads with performance schema */
mysql_pfs_key_t	buf_dump_thread_key;
mysql_pfs_key_t	dict_stats_thread_key;
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

		/* Note: stat.rw_perm is only valid of files */

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
/********************************************************************//**
I/o-handler thread function.
@return OS_THREAD_DUMMY_RETURN */
extern "C"
os_thread_ret_t
DECLARE_THREAD(io_handler_thread)(
/*==============================*/
	void*	arg)	/*!< in: pointer to the number of the segment in
			the aio array */
{
	ulint	segment;

	segment = *((ulint*) arg);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Io handler thread " << segment << " starts, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif

#ifdef UNIV_PFS_THREAD
	/* For read only mode, we don't need ibuf and log I/O thread.
	Please see innobase_start_or_create_for_mysql() */
	ulint   start = (srv_read_only_mode) ? 0 : 2;

	if (segment < start) {
		if (segment == 0) {
			pfs_register_thread(io_ibuf_thread_key);
		} else {
			ut_ad(segment == 1);
			pfs_register_thread(io_log_thread_key);
		}
	} else if (segment >= start
		   && segment < (start + srv_n_read_io_threads)) {
			pfs_register_thread(io_read_thread_key);

	} else if (segment >= (start + srv_n_read_io_threads)
		   && segment < (start + srv_n_read_io_threads
				 + srv_n_write_io_threads)) {
		pfs_register_thread(io_write_thread_key);

	} else {
		pfs_register_thread(io_handler_thread_key);
	}
#endif /* UNIV_PFS_THREAD */

	while (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS
	       || buf_page_cleaner_is_active
	       || !os_aio_all_slots_free()) {
		fil_aio_wait(segment);
	}

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit.
	The thread actually never comes here because it is exited in an
	os_event_wait(). */

	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;
}
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Creates a log file.
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
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
#endif
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
		"innodb_redo_log", SRV_LOG_SPACE_FIRST_ID,
		fsp_flags_set_page_size(0, univ_page_size),
		FIL_TYPE_LOG);
	ut_a(fil_validate());
	ut_a(log_space != NULL);

	logfile0 = fil_node_create(
		logfilename, (ulint) srv_log_file_size,
		log_space, false, false);
	ut_a(logfile0);

	for (unsigned i = 1; i < srv_n_log_files; i++) {

		sprintf(logfilename + dirnamelen, "ib_logfile%u", i);

		if (!fil_node_create(logfilename,
				     (ulint) srv_log_file_size,
				     log_space, false, false)) {

			ib::error()
				<< "Cannot create file node for log file "
				<< logfilename;

			return(DB_ERROR);
		}
	}

	if (!log_group_init(0, srv_n_log_files,
			    srv_log_file_size * UNIV_PAGE_SIZE,
			    SRV_LOG_SPACE_FIRST_ID)) {
		return(DB_ERROR);
	}

	fil_open_log_and_system_tablespace_files();

	/* Create a log checkpoint. */
	log_mutex_enter();
	ut_d(recv_no_log_write = false);
	recv_reset_logs(lsn);
	log_mutex_exit();

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
	fil_flush(SRV_LOG_SPACE_FIRST_ID);
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
static MY_ATTRIBUTE((nonnull, warn_unused_result))
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

/*********************************************************************//**
Create undo tablespace.
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespace_create(
/*=======================*/
	const char*	name,		/*!< in: tablespace name */
	ulint		size)		/*!< in: tablespace size in pages */
{
	pfs_os_file_t	fh;
	bool		ret;
	dberr_t		err = DB_SUCCESS;

	os_file_create_subdirs_if_needed(name);

	fh = os_file_create(
		innodb_data_file_key,
		name,
		srv_read_only_mode ? OS_FILE_OPEN : OS_FILE_CREATE,
		OS_FILE_NORMAL, OS_DATA_FILE, srv_read_only_mode, &ret);

	if (srv_read_only_mode && ret) {

		ib::info() << name << " opened in read-only mode";

	} else if (ret == FALSE) {
		if (os_file_get_last_error(false) != OS_FILE_ALREADY_EXISTS) {

			ib::error() << "Can't create UNDO tablespace "
				<< name;
		}
		err = DB_ERROR;
	} else {
		ut_a(!srv_read_only_mode);

		/* We created the data file and now write it full of zeros */

		ib::info() << "Data file " << name << " did not exist: new to"
			" be created";

		ib::info() << "Setting file " << name << " size to "
			<< (size >> (20 - UNIV_PAGE_SIZE_SHIFT)) << " MB";

		ib::info() << "Database physically writes the file full: "
			<< "wait...";

		ret = os_file_set_size(
			name, fh, size << UNIV_PAGE_SIZE_SHIFT,
			srv_read_only_mode);

		if (!ret) {
			ib::info() << "Error in creating " << name
				<< ": probably out of disk space";

			err = DB_ERROR;
		}

		os_file_close(fh);
	}

	return(err);
}
/*********************************************************************//**
Open an undo tablespace.
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespace_open(
/*=====================*/
	const char*	name,		/*!< in: tablespace file name */
	ulint		space_id)	/*!< in: tablespace id */
{
	pfs_os_file_t	fh;
	bool		ret;
	ulint		flags;
	dberr_t		err	= DB_ERROR;
	char		undo_name[sizeof "innodb_undo000"];

	ut_snprintf(undo_name, sizeof(undo_name),
		   "innodb_undo%03u", static_cast<unsigned>(space_id));

	if (!srv_file_check_mode(name)) {
		ib::error() << "UNDO tablespaces must be " <<
			(srv_read_only_mode ? "writable" : "readable") << "!";

		return(DB_ERROR);
	}

	fh = os_file_create(
		innodb_data_file_key, name,
		OS_FILE_OPEN_RETRY
		| OS_FILE_ON_ERROR_NO_EXIT
		| OS_FILE_ON_ERROR_SILENT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		srv_read_only_mode,
		&ret);

	/* If the file open was successful then load the tablespace. */

	if (ret) {
		os_offset_t	size;
		fil_space_t*	space;

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

		size = os_file_get_size(fh);
		ut_a(size != (os_offset_t) -1);

		ret = os_file_close(fh);
		ut_a(ret);

		/* Load the tablespace into InnoDB's internal
		data structures. */

		/* We set the biggest space id to the undo tablespace
		because InnoDB hasn't opened any other tablespace apart
		from the system tablespace. */

		fil_set_max_space_id_if_bigger(space_id);

		/* Set the compressed page size to 0 (non-compressed) */
		flags = fsp_flags_init(
			univ_page_size, false, false, false, false);
		space = fil_space_create(
			undo_name, space_id, flags, FIL_TYPE_TABLESPACE);

		ut_a(fil_validate());
		ut_a(space);

		os_offset_t	n_pages = size / UNIV_PAGE_SIZE;

		/* On 32-bit platforms, ulint is 32 bits and os_offset_t
		is 64 bits. It is OK to cast the n_pages to ulint because
		the unit has been scaled to pages and page number is always
		32 bits. */
		if (fil_node_create(
			name, (ulint) n_pages, space, false, atomic_write)) {

			err = DB_SUCCESS;
		}
	}

	return(err);
}

/** Check if undo tablespaces and redo log files exist before creating a
new system tablespace
@retval DB_SUCCESS  if all undo and redo logs are not found
@retval DB_ERROR    if any undo and redo logs are found */
static
dberr_t
srv_check_undo_redo_logs_exists()
{
	bool		ret;
	pfs_os_file_t	fh;
	char	name[OS_FILE_MAX_PATH];

	/* Check if any undo tablespaces exist */
	for (ulint i = 1; i <= srv_undo_tablespaces; ++i) {

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu",
			srv_undo_dir, OS_PATH_SEPARATOR,
			i);

		fh = os_file_create(
			innodb_data_file_key, name,
			OS_FILE_OPEN_RETRY
			| OS_FILE_ON_ERROR_NO_EXIT
			| OS_FILE_ON_ERROR_SILENT,
			OS_FILE_NORMAL,
			OS_DATA_FILE,
			srv_read_only_mode,
			&ret);

		if (ret) {
			os_file_close(fh);
			ib::error()
				<< "undo tablespace '" << name << "' exists."
				" Creating system tablespace with existing undo"
				" tablespaces is not supported. Please delete"
				" all undo tablespaces before creating new"
				" system tablespace.";
			return(DB_ERROR);
		}
	}

	/* Check if any redo log files exist */
	char	logfilename[OS_FILE_MAX_PATH];
	size_t dirnamelen = strlen(srv_log_group_home_dir);
	memcpy(logfilename, srv_log_group_home_dir, dirnamelen);

	for (unsigned i = 0; i < srv_n_log_files; i++) {
		sprintf(logfilename + dirnamelen,
			"ib_logfile%u", i);

		fh = os_file_create(
			innodb_log_file_key, logfilename,
			OS_FILE_OPEN_RETRY
			| OS_FILE_ON_ERROR_NO_EXIT
			| OS_FILE_ON_ERROR_SILENT,
			OS_FILE_NORMAL,
			OS_LOG_FILE,
			srv_read_only_mode,
			&ret);

		if (ret) {
			os_file_close(fh);
			ib::error() << "redo log file '" << logfilename
				<< "' exists. Creating system tablespace with"
				" existing redo log files is not recommended."
				" Please delete all redo log files before"
				" creating new system tablespace.";
			return(DB_ERROR);
		}
	}

	return(DB_SUCCESS);
}

undo::undo_spaces_t	undo::Truncate::s_fix_up_spaces;

/********************************************************************
Opens the configured number of undo tablespaces.
@return DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespaces_init(
/*======================*/
	bool		create_new_db,		/*!< in: TRUE if new db being
						created */
	const ulint	n_conf_tablespaces,	/*!< in: configured undo
						tablespaces */
	ulint*		n_opened)		/*!< out: number of UNDO
						tablespaces successfully
						discovered and opened */
{
	ulint			i;
	dberr_t			err = DB_SUCCESS;
	ulint			prev_space_id = 0;
	ulint			n_undo_tablespaces;
	ulint			undo_tablespace_ids[TRX_SYS_N_RSEGS + 1];

	*n_opened = 0;

	ut_a(n_conf_tablespaces <= TRX_SYS_N_RSEGS);

	memset(undo_tablespace_ids, 0x0, sizeof(undo_tablespace_ids));

	/* Create the undo spaces only if we are creating a new
	instance. We don't allow creating of new undo tablespaces
	in an existing instance (yet).  This restriction exists because
	we check in several places for SYSTEM tablespaces to be less than
	the min of user defined tablespace ids. Once we implement saving
	the location of the undo tablespaces and their space ids this
	restriction will/should be lifted. */

	for (i = 0; create_new_db && i < n_conf_tablespaces; ++i) {
		char		name[OS_FILE_MAX_PATH];
		ulint		space_id;

		DBUG_EXECUTE_IF("innodb_undo_upgrade",
			if (i == 0) {
				dict_hdr_get_new_id(
					NULL, NULL, &space_id, NULL, true);
				dict_hdr_get_new_id(
					NULL, NULL, &space_id, NULL, true);
				dict_hdr_get_new_id(
					NULL, NULL, &space_id, NULL, true);
			});

		dict_hdr_get_new_id(NULL, NULL, &space_id, NULL, true);

		fil_set_max_space_id_if_bigger(space_id);

		if (i == 0) {
			srv_undo_space_id_start = space_id;
			prev_space_id = srv_undo_space_id_start - 1;
		}

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu",
			srv_undo_dir, OS_PATH_SEPARATOR, space_id);

		undo_tablespace_ids[i] = space_id;

		err = srv_undo_tablespace_create(
			name, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES);

		if (err != DB_SUCCESS) {
			ib::error() << "Could not create undo tablespace '"
				<< name << "'.";
			return(err);
		}
	}

	/* Get the tablespace ids of all the undo segments excluding
	the system tablespace (0). If we are creating a new instance then
	we build the undo_tablespace_ids ourselves since they don't
	already exist. */

	if (!create_new_db) {
		n_undo_tablespaces = trx_rseg_get_n_undo_tablespaces(
			undo_tablespace_ids);

		srv_undo_tablespaces_active = n_undo_tablespaces;

		if (srv_undo_tablespaces_active != 0) {
			srv_undo_space_id_start = undo_tablespace_ids[0];
			prev_space_id = srv_undo_space_id_start - 1;
		}

		/* Check if any of the UNDO tablespace needs fix-up because
		server crashed while truncate was active on UNDO tablespace.*/
		for (i = 0; i < n_undo_tablespaces; ++i) {

			undo::Truncate	undo_trunc;

			if (undo_trunc.needs_fix_up(undo_tablespace_ids[i])) {

				char	name[OS_FILE_MAX_PATH];

				ut_snprintf(name, sizeof(name),
					    "%s%cundo%03lu",
					    srv_undo_dir, OS_PATH_SEPARATOR,
					    undo_tablespace_ids[i]);

				os_file_delete(innodb_data_file_key, name);

				err = srv_undo_tablespace_create(
					name,
					SRV_UNDO_TABLESPACE_SIZE_IN_PAGES);

				if (err != DB_SUCCESS) {
					ib::error() << "Could not fix-up undo "
						" tablespace truncate '"
						<< name << "'.";
					return(err);
				}

				undo::Truncate::s_fix_up_spaces.push_back(
					undo_tablespace_ids[i]);
			}
		}
	} else {
		n_undo_tablespaces = n_conf_tablespaces;

		undo_tablespace_ids[n_conf_tablespaces] = ULINT_UNDEFINED;
	}

	/* Open all the undo tablespaces that are currently in use. If we
	fail to open any of these it is a fatal error. The tablespace ids
	should be contiguous. It is a fatal error because they are required
	for recovery and are referenced by the UNDO logs (a.k.a RBS). */

	for (i = 0; i < n_undo_tablespaces; ++i) {
		char	name[OS_FILE_MAX_PATH];

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu",
			srv_undo_dir, OS_PATH_SEPARATOR,
			undo_tablespace_ids[i]);

		/* Should be no gaps in undo tablespace ids. */
		ut_a(prev_space_id + 1 == undo_tablespace_ids[i]);

		/* The system space id should not be in this array. */
		ut_a(undo_tablespace_ids[i] != 0);
		ut_a(undo_tablespace_ids[i] != ULINT_UNDEFINED);

		fil_set_max_space_id_if_bigger(undo_tablespace_ids[i]);

		err = srv_undo_tablespace_open(name, undo_tablespace_ids[i]);

		if (err != DB_SUCCESS) {
			ib::error() << "Unable to open undo tablespace '"
				<< name << "'.";
			return(err);
		}

		prev_space_id = undo_tablespace_ids[i];

		++*n_opened;
	}

	/* Open any extra unused undo tablespaces. These must be contiguous.
	We stop at the first failure. These are undo tablespaces that are
	not in use and therefore not required by recovery. We only check
	that there are no gaps. */

	for (i = prev_space_id + 1; i < TRX_SYS_N_RSEGS; ++i) {
		char	name[OS_FILE_MAX_PATH];

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu", srv_undo_dir, OS_PATH_SEPARATOR, i);

		err = srv_undo_tablespace_open(name, i);

		if (err != DB_SUCCESS) {
			break;
		}

		/** Note the first undo tablespace id in case of
		no active undo tablespace. */
		if (n_undo_tablespaces == 0) {
			srv_undo_space_id_start = i;
		}

		++n_undo_tablespaces;

		++*n_opened;
	}

	/** Explictly specify the srv_undo_space_id_start
	as zero when there are no undo tablespaces. */
	if (n_undo_tablespaces == 0) {
		srv_undo_space_id_start = 0;
	}

	/* If the user says that there are fewer than what we find we
	tolerate that discrepancy but not the inverse. Because there could
	be unused undo tablespaces for future use. */

	if (n_conf_tablespaces > n_undo_tablespaces) {
		ib::error() << "Expected to open " << n_conf_tablespaces
			<< " undo tablespaces but was able to find only "
			<< n_undo_tablespaces << " undo tablespaces. Set the"
			" innodb_undo_tablespaces parameter to the correct"
			" value and retry. Suggested value is "
			<< n_undo_tablespaces;

		return(err != DB_SUCCESS ? err : DB_ERROR);

	} else  if (n_undo_tablespaces > 0) {

		ib::info() << "Opened " << n_undo_tablespaces
			<< " undo tablespaces";

		ib::info() << srv_undo_tablespaces_active << " undo tablespaces"
			<< " made active";

		if (n_conf_tablespaces == 0) {
			ib::warn() << "Will use system tablespace for all newly"
				<< " created rollback-segment as"
				<< " innodb_undo_tablespaces=0";
		}
	}

	if (create_new_db) {
		mtr_t	mtr;

		mtr_start(&mtr);

		/* The undo log tablespace */
		for (i = 0; i < n_undo_tablespaces; ++i) {

			fsp_header_init(
				undo_tablespace_ids[i],
				SRV_UNDO_TABLESPACE_SIZE_IN_PAGES, &mtr);
		}

		mtr_commit(&mtr);
	}

	if (!undo::Truncate::s_fix_up_spaces.empty()) {

		/* Step-1: Initialize the tablespace header and rsegs header. */
		mtr_t		mtr;
		trx_sysf_t*	sys_header;

		mtr_start(&mtr);
		/* Turn off REDO logging. We are in server start mode and fixing
		UNDO tablespace even before REDO log is read. Let's say we
		do REDO logging here then this REDO log record will be applied
		as part of the current recovery process. We surely don't need
		that as this is fix-up action parallel to REDO logging. */
		mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
		sys_header = trx_sysf_get(&mtr);

		for (undo::undo_spaces_t::const_iterator it
			     = undo::Truncate::s_fix_up_spaces.begin();
		     it != undo::Truncate::s_fix_up_spaces.end();
		     ++it) {

			undo::Truncate::add_space_to_trunc_list(*it);

			fsp_header_init(
				*it, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES, &mtr);

			mtr_x_lock(fil_space_get_latch(*it, NULL), &mtr);

			for (ulint i = 0; i < TRX_SYS_N_RSEGS; i++) {

				ulint	space_id = trx_sysf_rseg_get_space(
						sys_header, i, &mtr);

				if (space_id == *it) {
					trx_rseg_header_create(
						*it, univ_page_size, ULINT_MAX,
						i, &mtr);
				}
			}

			undo::Truncate::clear_trunc_list();
		}
		mtr_commit(&mtr);

		/* Step-2: Flush the dirty pages from the buffer pool. */
		for (undo::undo_spaces_t::const_iterator it
			     = undo::Truncate::s_fix_up_spaces.begin();
		     it != undo::Truncate::s_fix_up_spaces.end();
		     ++it) {

			buf_LRU_flush_or_remove_pages(
				TRX_SYS_SPACE, BUF_REMOVE_FLUSH_WRITE, NULL);

			buf_LRU_flush_or_remove_pages(
				*it, BUF_REMOVE_FLUSH_WRITE, NULL);

			/* Remove the truncate redo log file. */
			undo::Truncate	undo_trunc;
			undo_trunc.done_logging(*it);
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
	ulint	sum_of_new_sizes;

	/* Will try to remove if there is existing file left-over by last
	unclean shutdown */
	tmp_space->set_sanity_check_status(true);
	tmp_space->delete_files();
	tmp_space->set_ignore_read_only(true);

	ib::info() << "Creating shared tablespace for temporary tables";

	bool	create_new_temp_space;
	ulint	temp_space_id = ULINT_UNDEFINED;

	dict_hdr_get_new_id(NULL, NULL, &temp_space_id, NULL, true);

	tmp_space->set_space_id(temp_space_id);

	RECOVERY_CRASH(100);

	dberr_t	err = tmp_space->check_file_spec(
			&create_new_temp_space, 12 * 1024 * 1024);

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

		mtr_t	mtr;
		ulint	size = tmp_space->get_sum_of_sizes();

		ut_a(temp_space_id != ULINT_UNDEFINED);
		ut_a(tmp_space->space_id() == temp_space_id);

		/* Open this shared temp tablespace in the fil_system so that
		it stays open until shutdown. */
		if (fil_space_open(tmp_space->name())) {

			/* Initialize the header page */
			mtr_start(&mtr);
			mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

			fsp_header_init(tmp_space->space_id(), size, &mtr);

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
void
srv_shutdown_all_bg_threads()
{
	ulint	i;

	srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

	if (!srv_start_state) {
		return;
	}

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

		bool	active = os_thread_active();

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
		/* Clean the buffer pool. */
		buf_flush_sync_all_buf_pools();

		RECOVERY_CRASH(1);

		log_mutex_enter();

		fil_names_clear(log_sys->lsn, false);

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
		fil_flush(SRV_LOG_SPACE_FIRST_ID);

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

/********************************************************************
Starts InnoDB and creates a new database if database files
are not found and the user wants.
@return DB_SUCCESS or error code */
dberr_t
innobase_start_or_create_for_mysql(void)
/*====================================*/
{
	bool		create_new_db = false;
	lsn_t		flushed_lsn;
	ulint		sum_of_data_file_sizes;
	ulint		tablespace_size_in_header;
	dberr_t		err;
	ulint		srv_n_log_files_found = srv_n_log_files;
	mtr_t		mtr;
	purge_pq_t*	purge_queue;
	char		logfilename[10000];
	char*		logfile0	= NULL;
	size_t		dirnamelen;
	unsigned	i = 0;

	/* Reset the start state. */
	srv_start_state = SRV_START_STATE_NONE;

	if (srv_force_recovery == SRV_FORCE_NO_LOG_REDO) {
		srv_read_only_mode = true;
	}

	high_level_read_only = srv_read_only_mode
		|| srv_force_recovery > SRV_FORCE_NO_TRX_UNDO;

	if (srv_read_only_mode) {
		ib::info() << "Started in read only mode";

		/* There is no write except to intrinsic table and so turn-off
		doublewrite mechanism completely. */
		srv_use_doublewrite_buf = FALSE;
	}

#ifdef HAVE_LZO1X
	if (lzo_init() != LZO_E_OK) {
		ib::warn() << "lzo_init() failed, support disabled";
		srv_lzo_disabled = true;
	} else {
		ib::info() << "LZO1X support available";
		srv_lzo_disabled = false;
	}
#endif /* HAVE_LZO1X */

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

	srv_start_has_been_called = TRUE;

	srv_is_being_started = true;

#ifdef _WIN32
	srv_use_native_aio = TRUE;

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		ib::info() << "Using Linux native AIO";
	}
#else
	/* Currently native AIO is supported only on windows and linux
	and that also when the support is compiled in. In all other
	cases, we ignore the setting of innodb_use_native_aio. */
	srv_use_native_aio = FALSE;
#endif /* _WIN32 */

	/* Register performance schema stages before any real work has been
	started which may need to be instrumented. */
	mysql_stage_register("innodb", srv_stages, UT_ARR_SIZE(srv_stages));

	if (srv_file_flush_method_str == NULL) {
		/* These are the default options */
#ifndef _WIN32
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fsync")) {
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DSYNC")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DIRECT;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT_NO_FSYNC")) {
		srv_unix_file_flush_method = SRV_UNIX_O_DIRECT_NO_FSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
		srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
		srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
#else
		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "normal")) {
		srv_win_file_flush_method = SRV_WIN_IO_NORMAL;
		srv_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "unbuffered")) {
		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
		srv_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str,
				  "async_unbuffered")) {
		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#endif /* _WIN32 */
	} else {
		ib::error() << "Unrecognized value "
			<< srv_file_flush_method_str
			<< " for innodb_flush_method";
		return(srv_init_abort(DB_ERROR));
	}

	/* Note that the call srv_boot() also changes the values of
	some variables to the units used by InnoDB internally */

	/* Set the maximum number of threads which can wait for a semaphore
	inside InnoDB: this is the 'sync wait array' size, as well as the
	maximum number of threads that can wait in the 'srv_conc array' for
	their time to enter InnoDB. */

	srv_max_n_threads = 1   /* io_ibuf_thread */
			    + 1 /* io_log_thread */
			    + 1 /* lock_wait_timeout_thread */
			    + 1 /* srv_error_monitor_thread */
			    + 1 /* srv_monitor_thread */
			    + 1 /* srv_master_thread */
			    + 1 /* srv_purge_coordinator_thread */
			    + 1 /* buf_dump_thread */
			    + 1 /* dict_stats_thread */
			    + 1 /* fts_optimize_thread */
			    + 1 /* recv_writer_thread */
			    + 1 /* trx_rollback_or_clean_all_recovered */
			    + 128 /* added as margin, for use of
				  InnoDB Memcached etc. */
			    + max_connections
			    + srv_n_read_io_threads
			    + srv_n_write_io_threads
			    + srv_n_purge_threads
			    + srv_n_page_cleaners
			    /* FTS Parallel Sort */
			    + fts_sort_pll_degree * FTS_NUM_AUX_INDEX
			      * max_connections;

	if (srv_buf_pool_size >= BUF_POOL_SIZE_THRESHOLD) {

		if (srv_buf_pool_instances == srv_buf_pool_instances_default) {
#if defined(_WIN32) && !defined(_WIN64)
			/* Do not allocate too large of a buffer pool on
			Windows 32-bit systems, which can have trouble
			allocating larger single contiguous memory blocks. */
			srv_buf_pool_instances = ut_min(
				static_cast<ulong>(MAX_BUFFER_POOLS),
				static_cast<ulong>(srv_buf_pool_size
						   / (128 * 1024 * 1024)));
#else /* defined(_WIN32) && !defined(_WIN64) */
			/* Default to 8 instances when size > 1GB. */
			srv_buf_pool_instances = 8;
#endif /* defined(_WIN32) && !defined(_WIN64) */
		}
	} else {
		/* If buffer pool is less than 1 GiB, assume fewer
		threads. Also use only one buffer pool instance. */
		if (srv_buf_pool_instances != srv_buf_pool_instances_default
		    && srv_buf_pool_instances != 1) {
			/* We can't distinguish whether the user has explicitly
			started mysqld with --innodb-buffer-pool-instances=0,
			(srv_buf_pool_instances_default is 0) or has not
			specified that option at all. Thus we have the
			limitation that if the user started with =0, we
			will not emit a warning here, but we should actually
			do so. */
			ib::info()
				<< "Adjusting innodb_buffer_pool_instances"
				" from " << srv_buf_pool_instances << " to 1"
				" since innodb_buffer_pool_size is less than "
				<< BUF_POOL_SIZE_THRESHOLD / (1024 * 1024)
				<< " MiB";
		}

		srv_buf_pool_instances = 1;
	}

	if (srv_buf_pool_chunk_unit * srv_buf_pool_instances
	    > srv_buf_pool_size) {
		/* Size unit of buffer pool is larger than srv_buf_pool_size.
		adjust srv_buf_pool_chunk_unit for srv_buf_pool_size. */
		srv_buf_pool_chunk_unit
			= static_cast<ulong>(srv_buf_pool_size)
			  / srv_buf_pool_instances;
		if (srv_buf_pool_size % srv_buf_pool_instances != 0) {
			++srv_buf_pool_chunk_unit;
		}
	}

	srv_buf_pool_size = buf_pool_size_align(srv_buf_pool_size);

	if (srv_n_page_cleaners > srv_buf_pool_instances) {
		/* limit of page_cleaner parallelizability
		is number of buffer pool instances. */
		srv_n_page_cleaners = srv_buf_pool_instances;
	}

	srv_boot();

	ib::info() << (ut_crc32_sse2_enabled ? "Using" : "Not using")
		<< " CPU crc32 instructions";

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
	lock_sys_create(srv_lock_table_size);
	srv_start_state_set(SRV_START_STATE_LOCK_SYS);

	/* Create i/o-handler threads: */

	for (ulint t = 0; t < srv_n_file_io_threads; ++t) {

		n[t] = t;

		os_thread_create(io_handler_thread, n + t, thread_ids + t);
	}

	/* Even in read-only mode there could be flush job generated by
	intrinsic table operations. */
	buf_flush_page_cleaner_init();

	os_thread_create(buf_flush_page_cleaner_coordinator,
			 NULL, NULL);

	for (i = 1; i < srv_n_page_cleaners; ++i) {
		os_thread_create(buf_flush_page_cleaner_worker,
				 NULL, NULL);
	}

	/* Make sure page cleaner is active. */
	while (!buf_page_cleaner_is_active) {
		os_thread_sleep(10000);
	}

	srv_start_state_set(SRV_START_STATE_IO);

	if (srv_n_log_files * srv_log_file_size * UNIV_PAGE_SIZE
	    >= 512ULL * 1024ULL * 1024ULL * 1024ULL) {
		/* log_block_convert_lsn_to_no() limits the returned block
		number to 1G and given that OS_FILE_LOG_BLOCK_SIZE is 512
		bytes, then we have a limit of 512 GB. If that limit is to
		be raised, then log_block_convert_lsn_to_no() must be
		modified. */
		ib::error() << "Combined size of log files must be < 512 GB";

		return(srv_init_abort(DB_ERROR));
	}

	if (srv_n_log_files * srv_log_file_size >= ULINT_MAX) {
		/* fil_io() takes ulint as an argument and we are passing
		(next_offset / UNIV_PAGE_SIZE) to it in log_group_write_buf().
		So (next_offset / UNIV_PAGE_SIZE) must be less than ULINT_MAX.
		So next_offset must be < ULINT_MAX * UNIV_PAGE_SIZE. This
		means that we are limited to ULINT_MAX * UNIV_PAGE_SIZE which
		is 64 TB on 32 bit systems. */
		ib::error() << "Combined size of log files must be < "
			<< ULINT_MAX / 1073741824 * UNIV_PAGE_SIZE << " GB";

		return(srv_init_abort(DB_ERROR));
	}

	os_normalize_path(srv_data_home);

	/* Check if the data files exist or not. */
	err = srv_sys_space.check_file_spec(
		&create_new_db, MIN_EXPECTED_TABLESPACE_SIZE);

	if (err != DB_SUCCESS) {
		return(srv_init_abort(DB_ERROR));
	}

	srv_startup_is_before_trx_rollback_phase = !create_new_db;

	/* Check if undo tablespaces and redo log files exist before creating
	a new system tablespace */
	if (create_new_db) {
		err = srv_check_undo_redo_logs_exists();
		if (err != DB_SUCCESS) {
			return(srv_init_abort(DB_ERROR));
		}
		recv_sys_debug_free();
	}

	/* Open or create the data files. */
	ulint	sum_of_new_sizes;

	err = srv_sys_space.open_or_create(
		false, create_new_db, &sum_of_new_sizes, &flushed_lsn);

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
		/* Other errors might come from Datafile::validate_first_page() */
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
			SRV_LOG_SPACE_FIRST_ID,
			fsp_flags_set_page_size(0, univ_page_size),
			FIL_TYPE_LOG);

		ut_a(fil_validate());
		ut_a(log_space);

		/* srv_log_file_size is measured in pages; if page size is 16KB,
		then we have a limit of 64TB on 32 bit systems */
		ut_a(srv_log_file_size <= ULINT_MAX);

		for (unsigned j = 0; j < i; j++) {
			sprintf(logfilename + dirnamelen, "ib_logfile%u", j);

			if (!fil_node_create(logfilename,
					     (ulint) srv_log_file_size,
					     log_space, false, false)) {
				return(srv_init_abort(DB_ERROR));
			}
		}

		if (!log_group_init(0, i, srv_log_file_size * UNIV_PAGE_SIZE,
				    SRV_LOG_SPACE_FIRST_ID)) {
			return(srv_init_abort(DB_ERROR));
		}
	}

files_checked:
	/* Open all log files and data files in the system
	tablespace: we keep them open until database
	shutdown */

	fil_open_log_and_system_tablespace_files();

	err = srv_undo_tablespaces_init(
		create_new_db,
		srv_undo_tablespaces,
		&srv_undo_tablespaces_open);

	/* If the force recovery is set very high then we carry on regardless
	of all errors. Basically this is fingers crossed mode. */

	if (err != DB_SUCCESS
	    && srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {

		return(srv_init_abort(err));
	}

	/* Initialize objects used by dict stats gathering thread, which
	can also be used by recovery if it tries to drop some table */
	if (!srv_read_only_mode) {
		dict_stats_thread_init();
	}

	trx_sys_file_format_init();

	trx_sys_create();

	if (create_new_db) {

		ut_a(!srv_read_only_mode);

		mtr_start(&mtr);

		bool ret = fsp_header_init(0, sum_of_new_sizes, &mtr);

		mtr_commit(&mtr);

		if (!ret) {
			return(srv_init_abort(DB_ERROR));
		}

		/* To maintain backward compatibility we create only
		the first rollback segment before the double write buffer.
		All the remaining rollback segments will be created later,
		after the double write buffer has been created. */
		trx_sys_create_sys_pages();

		purge_queue = trx_sys_init_at_db_start();

		DBUG_EXECUTE_IF("check_no_undo",
				ut_ad(purge_queue->empty());
				);

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, purge_queue);

		err = dict_create();

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		buf_flush_sync_all_buf_pools();

		flushed_lsn = log_get_lsn();

		fil_write_flushed_lsn(flushed_lsn);

		create_log_files_rename(
			logfilename, dirnamelen, flushed_lsn, logfile0);

	} else {

		/* Check if we support the max format that is stamped
		on the system tablespace.
		Note:  We are NOT allowed to make any modifications to
		the TRX_SYS_PAGE_NO page before recovery  because this
		page also contains the max_trx_id etc. important system
		variables that are required for recovery.  We need to
		ensure that we return the system to a state where normal
		recovery is guaranteed to work. We do this by
		invalidating the buffer cache, this will force the
		reread of the page and restoration to its last known
		consistent state, this is REQUIRED for the recovery
		process to work. */
		err = trx_sys_file_format_max_check(
			srv_max_file_format_at_startup);

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		/* Invalidate the buffer pool to ensure that we reread
		the page that we read above, during recovery.
		Note that this is not as heavy weight as it seems. At
		this point there will be only ONE page in the buf_LRU
		and there must be no page in the buf_flush list. */
		buf_pool_invalidate();

		/* Scan and locate truncate log files. Parsed located files
		and add table to truncate information to central vector for
		truncate fix-up action post recovery. */
		err = TruncateLogParser::scan_and_parse(srv_log_group_home_dir);
		if (err != DB_SUCCESS) {

			return(srv_init_abort(DB_ERROR));
		}

		/* We always try to do a recovery, even if the database had
		been shut down normally: this is the normal startup path */

		err = recv_recovery_from_checkpoint_start(flushed_lsn);

		recv_sys->dblwr.pages.clear();

		if (err == DB_SUCCESS) {
			/* Initialize the change buffer. */
			err = dict_boot();
		}

		if (err != DB_SUCCESS) {

			/* A tablespace was not found during recovery. The
			user must force recovery. */

			if (err == DB_TABLESPACE_NOT_FOUND) {

				srv_fatal_error();

				ut_error;
			}

			return(srv_init_abort(DB_ERROR));
		}

		purge_queue = trx_sys_init_at_db_start();

		if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
			/* Apply the hashed log records to the
			respective file pages, for the last batch of
			recv_group_scan_log_recs(). */

			recv_apply_hashed_log_recs(TRUE);
			DBUG_PRINT("ib_log", ("apply completed"));

			if (recv_needed_recovery) {
				trx_sys_print_mysql_binlog_offset();
			}
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

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, purge_queue);

		/* recv_recovery_from_checkpoint_finish needs trx lists which
		are initialized in trx_sys_init_at_db_start(). */

		recv_recovery_from_checkpoint_finish();

		/* Fix-up truncate of tables in the system tablespace
		if server crashed while truncate was active. The non-
		system tables are done after tablespace discovery. Do
		this now because this procedure assumes that no pages
		have changed since redo recovery.  Tablespace discovery
		can do updates to pages in the system tablespace.*/
		err = truncate_t::fixup_tables_in_system_tablespace();

		if (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
			/* Open or Create SYS_TABLESPACES and SYS_DATAFILES
			so that tablespace names and other metadata can be
			found. */
			srv_sys_tablespaces_open = true;
			err = dict_create_or_check_sys_tablespace();
			if (err != DB_SUCCESS) {
				return(srv_init_abort(err));
			}

			/* The following call is necessary for the insert
			buffer to work with multiple tablespaces. We must
			know the mapping between space id's and .ibd file
			names.

			In a crash recovery, we check that the info in data
			dictionary is consistent with what we already know
			about space id's from the calls to fil_ibd_load().

			In a normal startup, we create the space objects for
			every table in the InnoDB data dictionary that has
			an .ibd file.

			We also determine the maximum tablespace id used.

			The 'validate' flag indicates that when a tablespace
			is opened, we also read the header page and validate
			the contents to the data dictionary. This is time
			consuming, especially for databases with lots of ibd
			files.  So only do it after a crash and not forcing
			recovery.  Open rw transactions at this point is not
			a good reason to validate. */
			bool validate = recv_needed_recovery
				&& srv_force_recovery == 0;

			dict_check_tablespaces_and_store_max_id(validate);
		}

		/* Rotate the encryption key for recovery. It's because
		server could crash in middle of key rotation. Some tablespace
		didn't complete key rotation. Here, we will resume the
		rotation. */
		if (!srv_read_only_mode
		    && srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
			fil_encryption_rotate();
		}


		/* Fix-up truncate of table if server crashed while truncate
		was active. */
		err = truncate_t::fixup_tables_in_non_system_tablespace();

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

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
			ut_d(recv_no_log_write = true);
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

		recv_recovery_rollback_active();

		/* It is possible that file_format tag has never
		been set. In this case we initialize it to minimum
		value.  Important to note that we can do it ONLY after
		we have finished the recovery process so that the
		image of TRX_SYS_PAGE_NO is not stale. */
		trx_sys_file_format_tag_init();
	}

	if (!create_new_db && sum_of_new_sizes > 0) {
		/* New data file(s) were added */
		mtr_start(&mtr);

		fsp_header_inc_size(0, sum_of_new_sizes, &mtr);

		mtr_commit(&mtr);

		/* Immediately write the log record about increased tablespace
		size to disk, so that it is durable even if mysqld would crash
		quickly */

		log_buffer_flush_to_disk();
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

	/* Deprecate innodb_undo_logs.  But still use it if it is set to
	non-default and innodb_rollback_segments is default. */
	ut_a(srv_rollback_segments > 0);
	ut_a(srv_rollback_segments <= TRX_SYS_N_RSEGS);
	ut_a(srv_undo_logs > 0);
	ut_a(srv_undo_logs <= TRX_SYS_N_RSEGS);
	if (srv_undo_logs < TRX_SYS_N_RSEGS) {
		ib::warn() << deprecated_undo_logs;
		if (srv_rollback_segments == TRX_SYS_N_RSEGS) {
			srv_rollback_segments = srv_undo_logs;
		}
	}

	/* The number of rsegs that exist in InnoDB is given by status
	variable srv_available_undo_logs. The number of rsegs to use can
	be set using the dynamic global variable srv_rollback_segments. */

	srv_available_undo_logs = trx_sys_create_rsegs(
		srv_undo_tablespaces, srv_rollback_segments, srv_tmp_undo_logs);

	if (srv_available_undo_logs == ULINT_UNDEFINED) {
		/* Can only happen if server is read only. */
		ut_a(srv_read_only_mode);
		srv_rollback_segments = ULONG_UNDEFINED;
	} else if (srv_available_undo_logs < srv_rollback_segments
		   && !srv_force_recovery && !recv_needed_recovery) {
		ib::error() << "System or UNDO tablespace is running of out"
			    << " of space";
		/* Should due to out of file space. */
		return(srv_init_abort(DB_ERROR));
	}

	srv_startup_is_before_trx_rollback_phase = false;

	if (!srv_read_only_mode) {
		/* Create the thread which watches the timeouts
		for lock waits */
		os_thread_create(
			lock_wait_timeout_thread,
			NULL, thread_ids + 2 + SRV_MAX_N_IO_THREADS);

		/* Create the thread which warns of long semaphore waits */
		os_thread_create(
			srv_error_monitor_thread,
			NULL, thread_ids + 3 + SRV_MAX_N_IO_THREADS);

		/* Create the thread which prints InnoDB monitor info */
		os_thread_create(
			srv_monitor_thread,
			NULL, thread_ids + 4 + SRV_MAX_N_IO_THREADS);

		srv_start_state_set(SRV_START_STATE_MONITOR);
	}

	/* Create the SYS_FOREIGN and SYS_FOREIGN_COLS system tables */
	err = dict_create_or_check_foreign_constraint_tables();
	if (err != DB_SUCCESS) {
		return(srv_init_abort(err));
	}

	/* Create the SYS_TABLESPACES system table */
	err = dict_create_or_check_sys_tablespace();
	if (err != DB_SUCCESS) {
		return(srv_init_abort(err));
	}
	srv_sys_tablespaces_open = true;

	/* Create the SYS_VIRTUAL system table */
	err = dict_create_or_check_sys_virtual();
	if (err != DB_SUCCESS) {
		return(srv_init_abort(err));
	}

	srv_is_being_started = false;

	ut_a(trx_purge_state() == PURGE_STATE_INIT);

	/* Create the master thread which does purge and other utility
	operations */

	if (!srv_read_only_mode) {

		os_thread_create(
			srv_master_thread,
			NULL, thread_ids + (1 + SRV_MAX_N_IO_THREADS));

		srv_start_state_set(SRV_START_STATE_MASTER);
	}

	if (!srv_read_only_mode
	    && srv_force_recovery < SRV_FORCE_NO_BACKGROUND) {

		os_thread_create(
			srv_purge_coordinator_thread,
			NULL, thread_ids + 5 + SRV_MAX_N_IO_THREADS);

		ut_a(UT_ARR_SIZE(thread_ids)
		     > 5 + srv_n_purge_threads + SRV_MAX_N_IO_THREADS);

		/* We've already created the purge coordinator thread above. */
		for (i = 1; i < srv_n_purge_threads; ++i) {
			os_thread_create(
				srv_worker_thread, NULL,
				thread_ids + 5 + i + SRV_MAX_N_IO_THREADS);
		}

		srv_start_wait_for_purge_to_start();

		srv_start_state_set(SRV_START_STATE_PURGE);
	} else {
		purge_sys->state = PURGE_STATE_DISABLED;
	}

	/* wake main loop of page cleaner up */
	os_event_set(buf_flush_event);

	sum_of_data_file_sizes = srv_sys_space.get_sum_of_sizes();
	ut_a(sum_of_new_sizes != ULINT_UNDEFINED);

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

	if (srv_force_recovery > 0) {
		ib::info() << "!!! innodb_force_recovery is set to "
			<< srv_force_recovery << " !!!";
	}

	if (srv_force_recovery == 0) {
		/* In the insert buffer we may have even bigger tablespace
		id's, because we may have dropped those tablespaces, but
		insert buffer merge has not had time to clean the records from
		the ibuf tree. */

		ibuf_update_max_tablespace_id();
	}

	if (!srv_read_only_mode) {
		if (create_new_db) {
			srv_buffer_pool_load_at_startup = FALSE;
		}

		/* Create the buffer pool dump/load thread */
		os_thread_create(buf_dump_thread, NULL, NULL);

		/* Create the dict stats gathering thread */
		os_thread_create(dict_stats_thread, NULL, NULL);

		/* Create the thread that will optimize the FTS sub-system. */
		fts_optimize_init();

		srv_start_state_set(SRV_START_STATE_STAT);
	}

	/* Create the buffer pool resize thread */
	os_thread_create(buf_resize_thread, NULL, NULL);

	srv_was_started = TRUE;
	return(DB_SUCCESS);
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

/****************************************************************//**
Shuts down the InnoDB database.
@return DB_SUCCESS or error code */
dberr_t
innobase_shutdown_for_mysql(void)
/*=============================*/
{
	if (!srv_was_started) {
		if (srv_is_being_started) {
			ib::warn() << "Shutting down an improperly started,"
				" or created database!";
		}

		return(DB_SUCCESS);
	}

	if (!srv_read_only_mode) {
		fts_optimize_shutdown();
		dict_stats_shutdown();
	}

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
	}

	if (srv_dict_tmpfile) {
		fclose(srv_dict_tmpfile);
		srv_dict_tmpfile = 0;
	}

	if (srv_misc_tmpfile) {
		fclose(srv_misc_tmpfile);
		srv_misc_tmpfile = 0;
	}

	if (!srv_read_only_mode) {
		dict_stats_thread_deinit();
	}

	/* This must be disabled before closing the buffer pool
	and closing the data dictionary.  */
	btr_search_disable(true);

	ibuf_close();
	log_shutdown();
	trx_sys_file_format_close();
	trx_sys_close();
	lock_sys_close();

	trx_pool_close();

	/* We don't create these mutexes in RO mode because we don't create
	the temp files that the cover. */
	if (!srv_read_only_mode) {
		mutex_free(&srv_monitor_file_mutex);
		mutex_free(&srv_dict_tmpfile_mutex);
		mutex_free(&srv_misc_tmpfile_mutex);
	}

	dict_close();
	btr_search_sys_free();

	/* 3. Free all InnoDB's own mutexes and the os_fast_mutexes inside
	them */
	os_aio_free();
	que_close();
	row_mysql_close();
	srv_free();
	fil_close();

	/* 4. Free all allocated memory */

	pars_lexer_close();
	log_mem_free();
	buf_pool_free(srv_buf_pool_instances);

	/* 6. Free the thread management resoruces. */
	os_thread_free();

	/* 7. Free the synchronisation infrastructure. */
	sync_check_close();

	if (dict_foreign_err_file) {
		fclose(dict_foreign_err_file);
	}

	if (srv_print_verbose_log) {
		ib::info() << "Shutdown completed; log sequence number "
			<< srv_shutdown_lsn;
	}

	srv_was_started = FALSE;
	srv_start_has_been_called = FALSE;

	return(DB_SUCCESS);
}
#endif /* !UNIV_HOTBACKUP */


/********************************************************************
Signal all per-table background threads to shutdown, and wait for them to do
so. */
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

/** Get the meta-data filename from the table name for a
single-table tablespace.
@param[in]	table		table object
@param[out]	filename	filename
@param[in]	max_len		filename max length */
void
srv_get_meta_data_filename(
	dict_table_t*	table,
	char*		filename,
	ulint		max_len)
{
	ulint		len;
	char*		path;

	/* Make sure the data_dir_path is set. */
	dict_get_and_save_data_dir_path(table, false);

	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		ut_a(table->data_dir_path);

		path = fil_make_filepath(
			table->data_dir_path, table->name.m_name, CFG, true);
	} else {
		path = fil_make_filepath(NULL, table->name.m_name, CFG, false);
	}

	ut_a(path);
	len = ut_strlen(path);
	ut_a(max_len >= len);

	strcpy(filename, path);

	ut_free(path);
}

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
	dict_get_and_save_data_dir_path(table, false);

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
