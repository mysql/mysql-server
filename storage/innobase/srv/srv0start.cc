/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All rights reserved.
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

#include "ha_prototypes.h"

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
#include "srv0space.h"
#ifndef UNIV_HOTBACKUP
# include "trx0rseg.h"
# include "os0proc.h"
# include "sync0mutex.h"
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
# include "btr0pcur.h"
# include "os0event.h"
# include "zlib.h"
# include "ut0crc32.h"

/** Log sequence number immediately after startup */
lsn_t	srv_start_lsn;
/** Log sequence number at shutdown */
lsn_t	srv_shutdown_lsn;

/** TRUE if a raw partition is in use */
ibool	srv_start_raw_disk_in_use = FALSE;

/** TRUE if the server is being started, before rolling back any
incomplete transactions */
ibool	srv_startup_is_before_trx_rollback_phase = FALSE;
/** TRUE if the server is being started */
ibool	srv_is_being_started = FALSE;
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
enum srv_shutdown_state	srv_shutdown_state = SRV_SHUTDOWN_NONE;

/** Files comprising the system tablespace */
static os_file_t	files[1000];

/** io_handler_thread parameters for thread identification */
static ulint		n[SRV_MAX_N_IO_THREADS + 6];
/** io_handler_thread identifiers, 32 is the maximum number of purge threads  */
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6 + 32];

/** Name of srv_monitor_file */
static char*	srv_monitor_file_name;
#endif /* !UNIV_HOTBACKUP */

/** Minimum expected tablespace size. (10M) */
static const ulint MIN_EXPECTED_TABLESPACE_SIZE = 5 * 1024 * 1024;

/** Default undo tablespace size in UNIV_PAGEs count (10MB). */
static const ulint SRV_UNDO_TABLESPACE_SIZE_IN_PAGES =
	((1024 * 1024) * 10) / UNIV_PAGE_SIZE_DEF;

/** */
#define SRV_N_PENDING_IOS_PER_THREAD	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

#ifdef UNIV_PFS_THREAD
/* Keys to register InnoDB threads with performance schema */
mysql_pfs_key_t	io_ibuf_thread_key;
mysql_pfs_key_t	io_log_thread_key;
mysql_pfs_key_t	io_read_thread_key;
mysql_pfs_key_t	io_write_thread_key;
mysql_pfs_key_t	io_handler_thread_key;
mysql_pfs_key_t	srv_lock_timeout_thread_key;
mysql_pfs_key_t	srv_error_monitor_thread_key;
mysql_pfs_key_t	srv_monitor_thread_key;
mysql_pfs_key_t	srv_master_thread_key;
mysql_pfs_key_t	srv_purge_thread_key;
#endif /* UNIV_PFS_THREAD */

/*********************************************************************//**
Check if a file can be opened in read-write mode.
@return	true if it doesn't exist or can be opened in rw mode. */
static
bool
srv_file_check_mode(
/*================*/
	const char*	name)		/*!< in: filename to check */
{
	os_file_stat_t	stat;

	memset(&stat, 0x0, sizeof(stat));

	dberr_t		err = os_file_get_status(name, &stat, true);

	if (err == DB_FAIL) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"os_file_get_status() failed on '%s'. Can't determine "
			"file permissions", name);

		return(false);

	} else if (err == DB_SUCCESS) {

		/* Note: stat.rw_perm is only valid of files */

		if (stat.type == OS_FILE_TYPE_FILE) {
			if (!stat.rw_perm) {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"%s can't be opened in %s mode",
					name,
					srv_read_only_mode
					? "read" : "read-write");

				return(false);
			}
		} else {
			/* Not a regular file, bail out. */

			ib_logf(IB_LOG_LEVEL_ERROR,
				"'%s' not a regular file.", name);

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
@return	OS_THREAD_DUMMY_RETURN */
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
	fprintf(stderr, "Io handler thread %lu starts, id %lu\n", segment,
		os_thread_pf(os_thread_get_curr_id()));
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

	while (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS) {
		fil_aio_wait(segment);
	}

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit.
	The thread actually never comes here because it is exited in an
	os_event_wait(). */

	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}
#endif /* !UNIV_HOTBACKUP */

/*********************************************************************//**
Normalizes a directory path for Windows: converts slashes to backslashes. */

void
srv_normalize_path_for_win(
/*=======================*/
	char*	str __attribute__((unused)))	/*!< in/out: null-terminated
						character string */
{
#ifdef _WIN32
	for (; *str; str++) {

		if (*str == '/') {
			*str = '\\';
		}
	}
#endif
}

#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Creates a log file.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
create_log_file(
/*============*/
	os_file_t*	file,	/*!< out: file handle */
	const char*	name)	/*!< in: log file name */
{
	ibool		ret;

	*file = os_file_create(
		innodb_log_file_key, name,
		OS_FILE_CREATE, OS_FILE_NORMAL, OS_LOG_FILE, &ret);

	ib_logf(IB_LOG_LEVEL_INFO,
		"Setting log file %s size to %lu MB",
		name, (ulong) srv_log_file_size
		>> (20 - UNIV_PAGE_SIZE_SHIFT));

	ret = os_file_set_size(name, *file,
			       (os_offset_t) srv_log_file_size
			       << UNIV_PAGE_SIZE_SHIFT);
	if (!ret) {
		ib_logf(IB_LOG_LEVEL_ERROR, "Error in creating %s", name);
		return(DB_ERROR);
	}

	ret = os_file_close(*file);
	ut_a(ret);

	return(DB_SUCCESS);
}

/** Initial number of the first redo log file */
#define INIT_LOG_FILE0	(SRV_N_LOG_FILES_MAX + 1)

#ifdef DBUG_OFF
# define RECOVERY_CRASH(x) do {} while(0)
#else
# define RECOVERY_CRASH(x) do {						\
	if (srv_force_recovery_crash == x) {				\
		fprintf(stderr, "innodb_force_recovery_crash=%lu\n",	\
			srv_force_recovery_crash);			\
		fflush(stderr);						\
		exit(3);						\
	}								\
} while (0)
#endif

/*********************************************************************//**
Creates all log files.
@return	DB_SUCCESS or error code */
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
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot create log files in read-only mode");
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

	fil_space_create(
		logfilename, SRV_LOG_SPACE_FIRST_ID,
		fsp_flags_set_page_size(0, UNIV_PAGE_SIZE),
		FIL_LOG);
	ut_a(fil_validate());

	logfile0 = fil_node_create(
		logfilename, (ulint) srv_log_file_size,
		SRV_LOG_SPACE_FIRST_ID, FALSE);
	ut_a(logfile0);

	for (unsigned i = 1; i < srv_n_log_files; i++) {
		sprintf(logfilename + dirnamelen, "ib_logfile%u", i);

		if (!fil_node_create(logfilename,
				     (ulint) srv_log_file_size,
				     SRV_LOG_SPACE_FIRST_ID, FALSE)) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Cannot create file node for log file %s",
				logfilename);
			return(DB_ERROR);
		}
	}

	if (!log_group_init(0, srv_n_log_files,
			    srv_log_file_size * UNIV_PAGE_SIZE,
			    SRV_LOG_SPACE_FIRST_ID,
			    SRV_LOG_SPACE_FIRST_ID + 1)) {
		return(DB_ERROR);
	}

	fil_open_log_and_system_tablespace_files();

	/* Create a log checkpoint. */
	mutex_enter(&log_sys->mutex);
	ut_d(recv_no_log_write = FALSE);
	recv_reset_logs(lsn);
	mutex_exit(&log_sys->mutex);

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

	ib_logf(IB_LOG_LEVEL_INFO,
		"Renaming log file %s to %s", logfile0, logfilename);

	mutex_enter(&log_sys->mutex);
	ut_ad(strlen(logfile0) == 2 + strlen(logfilename));
	ibool success = os_file_rename(
		innodb_log_file_key, logfile0, logfilename);
	ut_a(success);

	RECOVERY_CRASH(10);

	/* Replace the first file with ib_logfile0. */
	strcpy(logfile0, logfilename);
	mutex_exit(&log_sys->mutex);

	fil_open_log_and_system_tablespace_files();

	ib_logf(IB_LOG_LEVEL_WARN, "New log files created, LSN=" LSN_PF, lsn);
}

/*********************************************************************//**
Opens a log file.
@return	DB_SUCCESS or error code */
static __attribute__((nonnull, warn_unused_result))
dberr_t
open_log_file(
/*==========*/
	os_file_t*	file,	/*!< out: file handle */
	const char*	name,	/*!< in: log file name */
	os_offset_t*	size)	/*!< out: file size */
{
	ibool	ret;

	*file = os_file_create(innodb_log_file_key, name,
			       OS_FILE_OPEN, OS_FILE_AIO,
			       OS_LOG_FILE, &ret);
	if (!ret) {
		ib_logf(IB_LOG_LEVEL_ERROR, "Unable to open '%s'", name);
		return(DB_ERROR);
	}

	*size = os_file_get_size(*file);

	ret = os_file_close(*file);
	ut_a(ret);
	return(DB_SUCCESS);
}

/*********************************************************************//**
Create undo tablespace.
@return	DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespace_create(
/*=======================*/
	const char*	name,		/*!< in: tablespace name */
	ulint		size)		/*!< in: tablespace size in pages */
{
	os_file_t	fh;
	ibool		ret;
	dberr_t		err = DB_SUCCESS;

	os_file_create_subdirs_if_needed(name);

	fh = os_file_create(
		innodb_data_file_key,
		name,
		srv_read_only_mode ? OS_FILE_OPEN : OS_FILE_CREATE,
		OS_FILE_NORMAL, OS_DATA_FILE, &ret);

	if (srv_read_only_mode && ret) {
		ib_logf(IB_LOG_LEVEL_INFO,
			"%s opened in read-only mode", name);
	} else if (ret == FALSE
		   && os_file_get_last_error(false) != OS_FILE_ALREADY_EXISTS) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Can't create UNDO tablespace %s", name);

		err = DB_ERROR;
	} else {
		ut_a(!srv_read_only_mode);

		/* We created the data file and now write it full of zeros */

		ib_logf(IB_LOG_LEVEL_INFO,
			"Data file %s did not exist: new to be created",
			name);

		ib_logf(IB_LOG_LEVEL_INFO,
			"Setting file %s size to %lu MB",
			name, size >> (20 - UNIV_PAGE_SIZE_SHIFT));

		ib_logf(IB_LOG_LEVEL_INFO,
			"Database physically writes the file full: wait...");

		ret = os_file_set_size(name, fh, size << UNIV_PAGE_SIZE_SHIFT);

		if (!ret) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Error in creating %s: probably out of "
				"disk space", name);

			err = DB_ERROR;
		}

		os_file_close(fh);
	}

	return(err);
}
/*********************************************************************//**
Open an undo tablespace.
@return	DB_SUCCESS or error code */
static
dberr_t
srv_undo_tablespace_open(
/*=====================*/
	const char*	name,		/*!< in: tablespace name */
	ulint		space)		/*!< in: tablespace id */
{
	os_file_t	fh;
	dberr_t		err	= DB_ERROR;
	ibool		ret;
	ulint		flags;

	if (!srv_file_check_mode(name)) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"UNDO tablespaces must be %s!",
			srv_read_only_mode ? "writable" : "readable");

		return(DB_ERROR);
	}

	fh = os_file_create(
		innodb_data_file_key, name,
		OS_FILE_OPEN_RETRY
		| OS_FILE_ON_ERROR_NO_EXIT
		| OS_FILE_ON_ERROR_SILENT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		&ret);

	/* If the file open was successful then load the tablespace. */

	if (ret) {
		os_offset_t	size;

		size = os_file_get_size(fh);
		ut_a(size != (os_offset_t) -1);

		ret = os_file_close(fh);
		ut_a(ret);

		/* Load the tablespace into InnoDB's internal
		data structures. */

		/* We set the biggest space id to the undo tablespace
		because InnoDB hasn't opened any other tablespace apart
		from the system tablespace. */

		fil_set_max_space_id_if_bigger(space);

		/* Set the compressed page size to 0 (non-compressed) */
		flags = fsp_flags_set_page_size(0, UNIV_PAGE_SIZE);
		fil_space_create(name, space, flags, FIL_TABLESPACE);

		ut_a(fil_validate());

		os_offset_t	n_pages = size / UNIV_PAGE_SIZE;

		/* On 64 bit Windows ulint can be 32 bit and os_offset_t
		is 64 bit. It is OK to cast the n_pages to ulint because
		the unit has been scaled to pages and they are always
		32 bit. */
		if (fil_node_create(name, (ulint) n_pages, space, FALSE)) {
			err = DB_SUCCESS;
		}
	}

	return(err);
}

/********************************************************************
Opens the configured number of undo tablespaces.
@return	DB_SUCCESS or error code */
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
	ulint		i;
	dberr_t		err = DB_SUCCESS;
	ulint		prev_space_id = 0;
	ulint		n_undo_tablespaces;
	ulint		undo_tablespace_ids[TRX_SYS_N_RSEGS + 1];

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
		char	name[OS_FILE_MAX_PATH];

		ut_snprintf(
			name, sizeof(name),
			"%s%cundo%03lu",
			srv_undo_dir, SRV_PATH_SEPARATOR, i + 1);

		/* Undo space ids start from 1. */
		err = srv_undo_tablespace_create(
			name, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES);

		if (err != DB_SUCCESS) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Could not create undo tablespace '%s'.",
				name);

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
	} else {
		n_undo_tablespaces = n_conf_tablespaces;

		for (i = 1; i <= n_undo_tablespaces; ++i) {
			undo_tablespace_ids[i - 1] = i;
		}

		undo_tablespace_ids[i] = ULINT_UNDEFINED;
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
			srv_undo_dir, SRV_PATH_SEPARATOR,
			undo_tablespace_ids[i]);

		/* Should be no gaps in undo tablespace ids. */
		ut_a(prev_space_id + 1 == undo_tablespace_ids[i]);

		/* The system space id should not be in this array. */
		ut_a(undo_tablespace_ids[i] != 0);
		ut_a(undo_tablespace_ids[i] != ULINT_UNDEFINED);

		/* Undo space ids start from 1. */

		err = srv_undo_tablespace_open(name, undo_tablespace_ids[i]);

		if (err != DB_SUCCESS) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Unable to open undo tablespace '%s'.", name);

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
			"%s%cundo%03lu", srv_undo_dir, SRV_PATH_SEPARATOR, i);

		/* Undo space ids start from 1. */
		err = srv_undo_tablespace_open(name, i);

		if (err != DB_SUCCESS) {
			break;
		}

		++n_undo_tablespaces;

		++*n_opened;
	}

	/* If the user says that there are fewer than what we find we
	tolerate that discrepancy but not the inverse. Because there could
	be unused undo tablespaces for future use. */

	if (n_conf_tablespaces > n_undo_tablespaces) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Expected to open %lu undo "
			"tablespaces but was able\n",
			n_conf_tablespaces);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: to find only %lu undo "
			"tablespaces.\n", n_undo_tablespaces);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Set the "
			"innodb_undo_tablespaces parameter to "
			"the\n");
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: correct value and retry. Suggested "
			"value is %lu\n", n_undo_tablespaces);

		return(err != DB_SUCCESS ? err : DB_ERROR);

	} else  if (n_undo_tablespaces > 0) {

		ib_logf(IB_LOG_LEVEL_INFO, "Opened %lu undo tablespaces",
			n_undo_tablespaces);

		if (n_conf_tablespaces == 0) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Using the system tablespace for all UNDO "
				"logging because innodb_undo_tablespaces=0");
		}
	}

	if (create_new_db) {
		mtr_t	mtr;

		mtr_start(&mtr);

		/* The undo log tablespace */
		for (i = 1; i <= n_undo_tablespaces; ++i) {

			fsp_header_init(
				i, SRV_UNDO_TABLESPACE_SIZE_IN_PAGES, &mtr);
		}

		mtr_commit(&mtr);
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
			ib_logf(IB_LOG_LEVEL_INFO,
				"Waiting for purge to start");

			os_thread_sleep(50000);
			break;

		case PURGE_STATE_EXIT:
		case PURGE_STATE_DISABLED:
			ut_error;
		}
	}
}

/********************************************************************
Create the temporary file tablespace.
@return DB_SUCCESS or error code. */
static
dberr_t
srv_open_tmp_tablespace(
/*====================*/
	Tablespace*	tmp_space)		/*!< in/out: Tablespace */
{
	if (srv_read_only_mode) {
		return(DB_SUCCESS);
	}

	/* Will try to remove if there is existing file left-over by last
	unclean shutdown */
	tmp_space->set_sanity_check_status(true);
	tmp_space->delete_files();

	ib_logf(IB_LOG_LEVEL_INFO,
		"Creating shared tablespace for temporary tables");

	bool	create_new_temp_space;
	ulint	temp_space_id = ULINT_UNDEFINED;

	dict_hdr_get_new_id(NULL, NULL, &temp_space_id, NULL, true);

	tmp_space->set_space_id(temp_space_id);

	RECOVERY_CRASH(100);

	dberr_t	err = tmp_space->check_file_spec(
			&create_new_temp_space, 12 * 1024 * 1024);

	if (err == DB_FAIL) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"The system temp tablespace must be writable!");

		err = DB_ERROR;

	} else if (err != DB_SUCCESS) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Could not create the system temp tablespace.");

	} else if ((err = tmp_space->open(0)) != DB_SUCCESS) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unable to create shared temporary tablespace");

	} else {

		mtr_t	mtr;
		ulint	size = tmp_space->get_sum_of_sizes();

		ut_a(tmp_space->space_id() == temp_space_id
		     && temp_space_id != ULINT_UNDEFINED);

		mtr_start(&mtr);

		fsp_header_init(tmp_space->space_id(), size, &mtr);

		mtr_commit(&mtr);
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

/****************************************************************//**
Shutdown all background threads created by InnoDB. */

void
srv_shutdown_all_bg_threads()
/*=========================*/
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
			os_aio_wake_all_threads_at_shutdown();
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
		ib_logf(IB_LOG_LEVEL_WARN,
			"%lu threads created by InnoDB"
			" had not exited at shutdown!",
			(ulong) os_thread_count);
	} else {
		/* Reset the start state. */
		srv_start_state = SRV_START_STATE_NONE;
	}
}

#define srv_init_abort(_db_err) srv_init_abort_low(create_new_db, _db_err)

/********************************************************************
Innobase start-up aborted. Perform cleanup actions.
@return DB_SUCCESS or error code. */
static
dberr_t
srv_init_abort_low(
/*===============*/
	bool	create_new_db,	/*!< in: TRUE if new db being created */
	dberr_t	err)		/*!< in: reason for abort */
{
	if (create_new_db) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"InnoDB Database creation was aborted. You may"
			" need to delete the ibdata1 file before trying"
			" to start up again.");
	}

	srv_shutdown_all_bg_threads();
	return(err);
}

/********************************************************************
Starts InnoDB and creates a new database if database files
are not found and the user wants.
@return	DB_SUCCESS or error code */

dberr_t
innobase_start_or_create_for_mysql(void)
/*====================================*/
{
	bool		create_new_db = false;
	lsn_t		min_flushed_lsn;
	lsn_t		max_flushed_lsn;
	ulint		sum_of_data_file_sizes;
	ulint		tablespace_size_in_header;
	dberr_t		err;
	ulint		srv_n_log_files_found = srv_n_log_files;
	ulint		io_limit;
	mtr_t		mtr;
	purge_pq_t*	purge_queue;
	ulint		n_recovered_trx;
	char		logfilename[10000];
	char*		logfile0	= NULL;
	size_t		dirnamelen;
	unsigned	i = 0;

	/* Reset the start state. */
	srv_start_state = SRV_START_STATE_NONE;

	if (srv_read_only_mode) {
		ib_logf(IB_LOG_LEVEL_INFO, "Started in read only mode");
	}

	if (sizeof(ulint) != sizeof(void*)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: size of InnoDB's ulint is %lu, "
			"but size of void*\n", (ulong) sizeof(ulint));
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: is %lu. The sizes should be the same "
			"so that on a 64-bit\n",
			(ulong) sizeof(void*));
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: platforms you can allocate more than 4 GB "
			"of memory.\n");
	}

#ifdef UNIV_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_DEBUG switched on !!!!!!!!!\n");
#endif

#ifdef UNIV_IBUF_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_IBUF_DEBUG switched on !!!!!!!!!\n");
# ifdef UNIV_IBUF_COUNT_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_IBUF_COUNT_DEBUG switched on "
		"!!!!!!!!!\n");
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: Crash recovery will fail with UNIV_IBUF_COUNT_DEBUG\n");
# endif
#endif

#ifdef UNIV_BLOB_DEBUG
	fprintf(stderr,
		"InnoDB: !!!!!!!! UNIV_BLOB_DEBUG switched on !!!!!!!!!\n"
		"InnoDB: Server restart may fail with UNIV_BLOB_DEBUG\n");
#endif /* UNIV_BLOB_DEBUG */

#ifdef UNIV_SYNC_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_SYNC_DEBUG switched on !!!!!!!!!\n");
#endif

#ifdef UNIV_SEARCH_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_SEARCH_DEBUG switched on !!!!!!!!!\n");
#endif

#ifdef UNIV_LOG_LSN_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_LOG_LSN_DEBUG switched on !!!!!!!!!\n");
#endif /* UNIV_LOG_LSN_DEBUG */
#ifdef UNIV_MEM_DEBUG
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: !!!!!!!! UNIV_MEM_DEBUG switched on !!!!!!!!!\n");
#endif

	if (srv_use_sys_malloc) {
		ib_logf(IB_LOG_LEVEL_INFO,
			"The InnoDB memory heap is disabled");
	}

#if defined(COMPILER_HINTS_ENABLED)
	ib_logf(IB_LOG_LEVEL_INFO,
		" InnoDB: Compiler hints enabled.");
#endif /* defined(COMPILER_HINTS_ENABLED) */

	ib_logf(IB_LOG_LEVEL_INFO,
		"" IB_ATOMICS_STARTUP_MSG "");

	ib_logf(IB_LOG_LEVEL_INFO,
		"Compressed tables use zlib " ZLIB_VERSION
#ifdef UNIV_ZIP_DEBUG
	      " with validation"
#endif /* UNIV_ZIP_DEBUG */
	      );
#ifdef UNIV_ZIP_COPY
	ib_logf(IB_LOG_LEVEL_INFO, "and extra copying");
#endif /* UNIV_ZIP_COPY */


	/* Since InnoDB does not currently clean up all its internal data
	structures in MySQL Embedded Server Library server_end(), we
	print an error message if someone tries to start up InnoDB a
	second time during the process lifetime. */

	if (srv_start_has_been_called) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Error: startup called second time "
			"during the process\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: lifetime. In the MySQL Embedded "
			"Server Library you\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: cannot call server_init() more "
			"than once during the\n");
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: process lifetime.\n");
	}

	srv_start_has_been_called = TRUE;

#ifdef UNIV_DEBUG
	log_do_write = TRUE;
#endif /* UNIV_DEBUG */
	/*	yydebug = TRUE; */

	srv_is_being_started = TRUE;
	srv_startup_is_before_trx_rollback_phase = TRUE;

#ifdef _WIN32
	switch (os_get_os_version()) {
	case OS_WIN95:
	case OS_WIN31:
	case OS_WINNT:
		/* On Win 95, 98, ME, Win32 subsystem for Windows 3.1,
		and NT use simulated aio. In NT Windows provides async i/o,
		but when run in conjunction with InnoDB Hot Backup, it seemed
		to corrupt the data files. */

		srv_use_native_aio = FALSE;
		break;

	case OS_WIN2000:
	case OS_WINXP:
		/* On 2000 and XP, async IO is available. */
		srv_use_native_aio = TRUE;
		break;

	default:
		/* Vista and later have both async IO and condition variables */
		srv_use_native_aio = TRUE;
		srv_use_native_conditions = true;
		break;
	}

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		ib_logf(IB_LOG_LEVEL_INFO, "Using Linux native AIO");
	}
#else
	/* Currently native AIO is supported only on windows and linux
	and that also when the support is compiled in. In all other
	cases, we ignore the setting of innodb_use_native_aio. */
	srv_use_native_aio = FALSE;
#endif /* _WIN32 */

	if (srv_file_flush_method_str == NULL) {
		/* These are the default options */

		srv_unix_file_flush_method = SRV_UNIX_FSYNC;

		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#ifndef _WIN32
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
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unrecognized value %s for innodb_flush_method",
			srv_file_flush_method_str);
		return(srv_init_abort(DB_ERROR));
	}

	/* Note that the call srv_boot() also changes the values of
	some variables to the units used by InnoDB internally */

	/* Set the maximum number of threads which can wait for a semaphore
	inside InnoDB: this is the 'sync wait array' size, as well as the
	maximum number of threads that can wait in the 'srv_conc array' for
	their time to enter InnoDB. */

#define BUF_POOL_SIZE_THRESHOLD	(1024 * 1024 * 1024)

	if (srv_buf_pool_size >= BUF_POOL_SIZE_THRESHOLD) {
		srv_max_n_threads = 50000;

		if (srv_buf_pool_instances == SRV_BUF_POOL_INSTANCES_NOT_SET) {
#if defined(_WIN32) && !defined(_WIN64)
			/* Do not allocate too large of a buffer pool on
			Windows 32-bit systems, which can have trouble
			allocating larger single contiguous memory blocks. */
			srv_buf_pool_instances = ut_min(
				MAX_BUFFER_POOLS,
				(long) (srv_buf_pool_size
					/ (128 * 1024 * 1024)));
#else /* defined(_WIN32) && !defined(_WIN64) */
			/* Default to 8 instances when size > 1GB. */
			srv_buf_pool_instances = 8;
#endif /* defined(_WIN32) && !defined(_WIN64) */
		}
	} else {
		/* If buffer pool is less than 1 GiB, assume fewer
		threads. Also use only one buffer pool instance. */
		if (srv_buf_pool_instances != SRV_BUF_POOL_INSTANCES_NOT_SET
		    && srv_buf_pool_instances != 1) {
			/* We can't distinguish whether the user has explicitly
			started mysqld with --innodb-buffer-pool-instances=0,
			(SRV_BUF_POOL_INSTANCES_NOT_SET is 0) or has not
			specified that option at all. Thus we have the
			limitation that if the user started with =0, we
			will not emit a warning here, but we should actually
			do so. */
			ib_logf(IB_LOG_LEVEL_WARN,
				"Adjusting innodb_buffer_pool_instances from "
				"%lu to 1 since innodb_buffer_pool_size is "
				"less than %d MiB",
				srv_buf_pool_instances,
				BUF_POOL_SIZE_THRESHOLD / (1024 * 1024));
		}

		srv_buf_pool_instances = 1;

		if (srv_buf_pool_size >= 8 * 1024 * 1024) {
			srv_max_n_threads = 10000;
		} else {
			/* Saves several MB of memory, especially in
			64-bit computers */
			srv_max_n_threads = 1000;
		}
	}

	srv_boot();

	ib_logf(IB_LOG_LEVEL_INFO,
		"%s CPU crc32 instructions",
		ut_crc32_sse2_enabled ? "Using" : "Not using");

	if (!srv_read_only_mode) {

		mutex_create("srv_monitor_file", &srv_monitor_file_mutex);

		if (srv_innodb_status) {

			srv_monitor_file_name = static_cast<char*>(
				mem_alloc(
					strlen(fil_path_to_mysql_datadir)
					+ 20 + sizeof "/innodb_status."));

			sprintf(srv_monitor_file_name, "%s/innodb_status.%lu",
				fil_path_to_mysql_datadir,
				os_proc_get_number());

			srv_monitor_file = fopen(srv_monitor_file_name, "w+");

			if (!srv_monitor_file) {
				ib_logf(IB_LOG_LEVEL_ERROR,
					"Unable to create %s: %s",
					srv_monitor_file_name,
					strerror(errno));
				return(srv_init_abort(DB_ERROR));
			}
		} else {

			srv_monitor_file_name = NULL;
			srv_monitor_file = os_file_create_tmpfile();

			if (!srv_monitor_file) {
				return(srv_init_abort(DB_ERROR));
			}
		}

		mutex_create("srv_dict_tmpfile", &srv_dict_tmpfile_mutex);

		srv_dict_tmpfile = os_file_create_tmpfile();

		if (!srv_dict_tmpfile) {
			return(srv_init_abort(DB_ERROR));
		}

		mutex_create("srv_misc_tmpfile", &srv_misc_tmpfile_mutex);

		srv_misc_tmpfile = os_file_create_tmpfile();

		if (!srv_misc_tmpfile) {
			return(srv_init_abort(DB_ERROR));
		}
	}

	/* If user has set the value of innodb_file_io_threads then
	we'll emit a message telling the user that this parameter
	is now deprecated. */
	if (srv_n_file_io_threads != 4) {
		ib_logf(IB_LOG_LEVEL_WARN,
			"innodb_file_io_threads is deprecated. Please use "
			"innodb_read_io_threads and innodb_write_io_threads "
			"instead");
	}

	/* Now overwrite the value on srv_n_file_io_threads */
	srv_n_file_io_threads = srv_n_read_io_threads;

	if (!srv_read_only_mode) {
		/* Add the log and ibuf IO threads. */
		srv_n_file_io_threads += 2;
		srv_n_file_io_threads += srv_n_write_io_threads;
	} else {
		ib_logf(IB_LOG_LEVEL_INFO,
			"Disabling background IO write threads.");

		srv_n_write_io_threads = 0;
	}

	ut_a(srv_n_file_io_threads <= SRV_MAX_N_IO_THREADS);

	io_limit = 8 * SRV_N_PENDING_IOS_PER_THREAD;

	/* On Windows when using native aio the number of aio requests
	that a thread can handle at a given time is limited to 32
	i.e.: SRV_N_PENDING_IOS_PER_THREAD */
# ifdef _WIN32
	if (srv_use_native_aio) {
		io_limit = SRV_N_PENDING_IOS_PER_THREAD;
	}
# endif /* _WIN32 */

	if (!os_aio_init(io_limit,
			 srv_n_read_io_threads,
			 srv_n_write_io_threads,
			 SRV_MAX_N_PENDING_SYNC_IOS)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Fatal : Cannot initialize AIO sub-system");

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

	ib_logf(IB_LOG_LEVEL_INFO,
		"Initializing buffer pool, total size = %.1f%c, "
		"instances = %lu",
		size, unit, srv_buf_pool_instances);

	err = buf_pool_init(srv_buf_pool_size, srv_buf_pool_instances);

	if (err != DB_SUCCESS) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot allocate memory for the buffer pool");

		return(srv_init_abort(DB_ERROR));
	}

	ib_logf(IB_LOG_LEVEL_INFO,
		"Completed initialization of buffer pool");

#ifdef UNIV_DEBUG
	/* We have observed deadlocks with a 5MB buffer pool but
	the actual lower limit could very well be a little higher. */

	if (srv_buf_pool_size <= 5 * 1024 * 1024) {

		ib_logf(IB_LOG_LEVEL_INFO,
			"Small buffer pool size (%luM), the flst_validate() "
			"debug function can cause a deadlock if the "
			"buffer pool fills up.",
			srv_buf_pool_size / 1024 / 1024);
	}
#endif /* UNIV_DEBUG */

	fsp_init();
	log_init();

	lock_sys_create(srv_lock_table_size);
	srv_start_state_set(SRV_START_STATE_LOCK_SYS);

	/* Create i/o-handler threads: */

	for (ulint t = 0; t < srv_n_file_io_threads; ++t) {

		n[t] = t;

		os_thread_create(io_handler_thread, n + t, thread_ids + t);
	}

	srv_start_state_set(SRV_START_STATE_IO);

	if (srv_n_log_files * srv_log_file_size * UNIV_PAGE_SIZE
	    >= 512ULL * 1024ULL * 1024ULL * 1024ULL) {
		/* log_block_convert_lsn_to_no() limits the returned block
		number to 1G and given that OS_FILE_LOG_BLOCK_SIZE is 512
		bytes, then we have a limit of 512 GB. If that limit is to
		be raised, then log_block_convert_lsn_to_no() must be
		modified. */
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Combined size of log files must be < 512 GB");

		return(srv_init_abort(DB_ERROR));
	}

	if (srv_n_log_files * srv_log_file_size >= ULINT_MAX) {
		/* fil_io() takes ulint as an argument and we are passing
		(next_offset / UNIV_PAGE_SIZE) to it in log_group_write_buf().
		So (next_offset / UNIV_PAGE_SIZE) must be less than ULINT_MAX.
		So next_offset must be < ULINT_MAX * UNIV_PAGE_SIZE. This
		means that we are limited to ULINT_MAX * UNIV_PAGE_SIZE which
		is 64 TB on 32 bit systems. */
		fprintf(stderr,
			" InnoDB: Error: combined size of log files"
			" must be < %lu GB\n",
			ULINT_MAX / 1073741824 * UNIV_PAGE_SIZE);

		return(srv_init_abort(DB_ERROR));
	}

	srv_normalize_path_for_win(srv_data_home);

	/* Check if the data files exist or not. */
	err = srv_sys_space.check_file_spec(
		&create_new_db, MIN_EXPECTED_TABLESPACE_SIZE);

	if (err != DB_SUCCESS) {
		return(srv_init_abort(DB_ERROR));
	}

	/* Open or create the data files. */
	ulint	sum_of_new_sizes;

	sum_of_new_sizes = srv_sys_space.get_sum_of_sizes();

	err = srv_sys_space.open(&sum_of_new_sizes);

	if (err != DB_SUCCESS) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Could not open or create the system tablespace. If "
			"you tried to add new data files to the system "
			"tablespace, and it failed here, you should now "
			"edit innodb_data_file_path in my.cnf back to what "
			"it was, and remove the new ibdata files InnoDB "
			"created in this failed attempt. InnoDB only wrote "
			"those files full of zeros, but did not yet use "
			"them in any way. But be careful: do not remove "
			"old data files which contain your precious data!");

		return(srv_init_abort(err));
	}

	if (!create_new_db) {
		/* Read the values from the header page. */
		err = srv_sys_space.read_lsn_and_check_flags(
			&min_flushed_lsn, &max_flushed_lsn);
		if (err != DB_SUCCESS) {
			return(srv_init_abort(DB_ERROR));
		}
	}

	dirnamelen = strlen(srv_log_group_home_dir);
	ut_a(dirnamelen < (sizeof logfilename) - 10 - sizeof "ib_logfile");
	memcpy(logfilename, srv_log_group_home_dir, dirnamelen);

	/* Add a path separator if needed. */
	if (dirnamelen && logfilename[dirnamelen - 1] != SRV_PATH_SEPARATOR) {
		logfilename[dirnamelen++] = SRV_PATH_SEPARATOR;
	}

	srv_log_file_size_requested = srv_log_file_size;

	if (create_new_db) {

		buf_flush_sync_all_buf_pools();

		min_flushed_lsn = max_flushed_lsn = log_get_lsn();

		err = create_log_files(
			logfilename, dirnamelen, max_flushed_lsn, logfile0);

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
				logfilename, &stat_info, false);

			if (err == DB_NOT_FOUND) {
				if (i == 0) {
					if (max_flushed_lsn
					    != min_flushed_lsn) {
						ib_logf(IB_LOG_LEVEL_ERROR,
							"Cannot create"
							" log files because"
							" data files are"
							" corrupt or"
							" not in sync"
							" with each other");
						return(srv_init_abort(
							DB_ERROR));
					}

					if (max_flushed_lsn < (lsn_t) 1000) {
						ib_logf(IB_LOG_LEVEL_ERROR,
							"Cannot create"
							" log files because"
							" data files are"
							" corrupt or the"
							" database was not"
							" shut down cleanly"
							" after creating"
							" the data files.");
						return(srv_init_abort(
							DB_ERROR));
					}

					err = create_log_files(
						logfilename, dirnamelen,
						max_flushed_lsn, logfile0);

					if (err != DB_SUCCESS) {
						return(srv_init_abort(err));
					}

					create_log_files_rename(
						logfilename, dirnamelen,
						max_flushed_lsn, logfile0);

					/* Suppress the message about
					crash recovery. */
					max_flushed_lsn = min_flushed_lsn
						= log_get_lsn();
					goto files_checked;
				} else if (i < 2) {
					/* must have at least 2 log files */
					ib_logf(IB_LOG_LEVEL_ERROR,
						"Only one log file found.");
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
				ib_logf(IB_LOG_LEVEL_ERROR,
					"Log file %s size "
					UINT64PF " is not a multiple of"
					" innodb_page_size",
					logfilename, size);
				return(srv_init_abort(DB_ERROR));
			}

			size >>= UNIV_PAGE_SIZE_SHIFT;

			if (i == 0) {
				srv_log_file_size = size;
			} else if (size != srv_log_file_size) {
				ib_logf(IB_LOG_LEVEL_ERROR,
					"Log file %s is"
					" of different size "UINT64PF" bytes"
					" than other log"
					" files "UINT64PF" bytes!",
					logfilename,
					size << UNIV_PAGE_SIZE_SHIFT,
					(os_offset_t) srv_log_file_size
					<< UNIV_PAGE_SIZE_SHIFT);
				return(srv_init_abort(DB_ERROR));
			}
		}

		srv_n_log_files_found = i;

		/* Create the in-memory file space objects. */

		sprintf(logfilename + dirnamelen, "ib_logfile%u", 0);

		fil_space_create(logfilename,
				 SRV_LOG_SPACE_FIRST_ID,
				 fsp_flags_set_page_size(0, UNIV_PAGE_SIZE),
				 FIL_LOG);

		ut_a(fil_validate());

		/* srv_log_file_size is measured in pages; if page size is 16KB,
		then we have a limit of 64TB on 32 bit systems */
		ut_a(srv_log_file_size <= ULINT_MAX);

		for (unsigned j = 0; j < i; j++) {
			sprintf(logfilename + dirnamelen, "ib_logfile%u", j);

			if (!fil_node_create(logfilename,
					     (ulint) srv_log_file_size,
					     SRV_LOG_SPACE_FIRST_ID, FALSE)) {
				return(srv_init_abort(DB_ERROR));
			}
		}

		if (!log_group_init(0, i, srv_log_file_size * UNIV_PAGE_SIZE,
				    SRV_LOG_SPACE_FIRST_ID,
				    SRV_LOG_SPACE_FIRST_ID + 1)) {
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

		fsp_header_init(0, sum_of_new_sizes, &mtr);

		mtr_commit(&mtr);

		/* To maintain backward compatibility we create only
		the first rollback segment before the double write buffer.
		All the remaining rollback segments will be created later,
		after the double write buffer has been created. */
		trx_sys_create_sys_pages();

		purge_queue = trx_sys_init_at_db_start();
		n_recovered_trx = UT_LIST_GET_LEN(trx_sys->rw_trx_list);

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, purge_queue);

		err = dict_create();

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		srv_startup_is_before_trx_rollback_phase = FALSE;

		buf_flush_sync_all_buf_pools();

		min_flushed_lsn = max_flushed_lsn = log_get_lsn();

		/* Stamp the LSN to the data files. */
		fil_write_flushed_lsn_to_data_files(max_flushed_lsn, 0);

		fil_flush_file_spaces(FIL_TABLESPACE);

		create_log_files_rename(logfilename, dirnamelen,
					max_flushed_lsn, logfile0);
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

		/* We always try to do a recovery, even if the database had
		been shut down normally: this is the normal startup path */

		err = recv_recovery_from_checkpoint_start(
			min_flushed_lsn, max_flushed_lsn);

		if (err != DB_SUCCESS) {

			return(srv_init_abort(DB_ERROR));
		}

		/* Since the insert buffer init is in dict_boot, and the
		insert buffer is needed in any disk i/o, first we call
		dict_boot(). Note that trx_sys_init_at_db_start() only needs
		to access space 0, and the insert buffer at this stage already
		works for space 0. */

		err = dict_boot();

		if (err != DB_SUCCESS) {
			return(srv_init_abort(err));
		}

		purge_queue = trx_sys_init_at_db_start();
		n_recovered_trx = UT_LIST_GET_LEN(trx_sys->rw_trx_list);

		/* The purge system needs to create the purge view and
		therefore requires that the trx_sys is inited. */

		trx_purge_sys_create(srv_n_purge_threads, purge_queue);

		/* recv_recovery_from_checkpoint_finish needs trx lists which
		are initialized in trx_sys_init_at_db_start(). */

		recv_recovery_from_checkpoint_finish();

		if (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
			/* The following call is necessary for the insert
			buffer to work with multiple tablespaces. We must
			know the mapping between space id's and .ibd file
			names.

			In a crash recovery, we check that the info in data
			dictionary is consistent with what we already know
			about space id's from the call of
			fil_load_single_table_tablespaces().

			In a normal startup, we create the space objects for
			every table in the InnoDB data dictionary that has
			an .ibd file.

			We also determine the maximum tablespace id used. */
			dict_check_t	dict_check;

			if (recv_needed_recovery) {
				dict_check = DICT_CHECK_ALL_LOADED;
			} else if (n_recovered_trx) {
				dict_check = DICT_CHECK_SOME_LOADED;
			} else {
				dict_check = DICT_CHECK_NONE_LOADED;
			}

			dict_check_tablespaces_and_store_max_id(dict_check);
		}

		if (!srv_force_recovery
		    && !recv_sys->found_corrupt_log
		    && (srv_log_file_size_requested != srv_log_file_size
			|| srv_n_log_files_found != srv_n_log_files)) {
			/* Prepare to replace the redo log files. */

			if (srv_read_only_mode) {
				ib_logf(IB_LOG_LEVEL_ERROR,
					"Cannot resize log files "
					"in read-only mode.");
				return(srv_init_abort(DB_READ_ONLY));
			}

			/* Clean the buffer pool. */
			buf_flush_sync_all_buf_pools();

			RECOVERY_CRASH(1);

			min_flushed_lsn = max_flushed_lsn = log_get_lsn();

			ib_logf(IB_LOG_LEVEL_WARN,
				"Resizing redo log from %u*%u to %u*%u pages"
				", LSN=" LSN_PF,
				(unsigned) i,
				(unsigned) srv_log_file_size,
				(unsigned) srv_n_log_files,
				(unsigned) srv_log_file_size_requested,
				max_flushed_lsn);

			/* Flush the old log files. */
			log_buffer_flush_to_disk();
			/* If innodb_flush_method=O_DSYNC,
			we need to explicitly flush the log buffers. */
			fil_flush(SRV_LOG_SPACE_FIRST_ID);

			ut_ad(max_flushed_lsn == log_get_lsn());

			/* Prohibit redo log writes from any other
			threads until creating a log checkpoint at the
			end of create_log_files(). */
			ut_d(recv_no_log_write = TRUE);
			ut_ad(!buf_pool_check_no_pending_io());

			RECOVERY_CRASH(3);

			/* Stamp the LSN to the data files. */
			fil_write_flushed_lsn_to_data_files(
				max_flushed_lsn, 0);

			fil_flush_file_spaces(FIL_TABLESPACE);

			RECOVERY_CRASH(4);

			/* Close and free the redo log files, so that
			we can replace them. */
			fil_close_log_files(true);

			RECOVERY_CRASH(5);

			/* Free the old log file space. */
			log_group_close_all();

			ib_logf(IB_LOG_LEVEL_WARN,
				"Starting to delete and rewrite log files.");

			srv_log_file_size = srv_log_file_size_requested;

			err = create_log_files(logfilename, dirnamelen,
					       max_flushed_lsn, logfile0);

			if (err != DB_SUCCESS) {
				return(srv_init_abort(err));
			}

			create_log_files_rename(logfilename, dirnamelen,
						max_flushed_lsn, logfile0);
		}

		srv_startup_is_before_trx_rollback_phase = FALSE;
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

	err = srv_open_tmp_tablespace(&srv_tmp_space);

	if (err != DB_SUCCESS) {
		return(srv_init_abort(err));
	}

	/* Will open temp-tablespace and will keep it open till server lifetime */
	fil_open_log_and_system_tablespace_files();

	/* fprintf(stderr, "Max allowed record size %lu\n",
	page_get_free_space_of_empty() / 2); */

	if (buf_dblwr == NULL) {
		/* Create the doublewrite buffer to a new tablespace */
		if (!buf_dblwr_create()) {
			return(srv_init_abort(DB_ERROR));
		}

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

	ut_a(srv_undo_logs > 0);
	ut_a(srv_undo_logs <= TRX_SYS_N_RSEGS);

	/* The number of rsegs that exist in InnoDB is given by status
	variable srv_available_undo_logs. The number of rsegs to use can
	be set using the dynamic global variable srv_undo_logs. */

	srv_available_undo_logs = trx_sys_create_rsegs(
		srv_undo_tablespaces, srv_undo_logs, srv_tmp_undo_logs);

	if (srv_available_undo_logs == ULINT_UNDEFINED) {
		/* Can only happen if force recovery is set. */
		ut_a(srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO
		     || srv_read_only_mode);
		srv_undo_logs = ULONG_UNDEFINED;
	}

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

	srv_is_being_started = FALSE;

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

	if (!srv_read_only_mode) {
		os_thread_create(buf_flush_page_cleaner_thread, NULL, NULL);
	}

	sum_of_data_file_sizes = srv_sys_space.get_sum_of_sizes();
	ut_a(sum_of_new_sizes != ULINT_UNDEFINED);

	tablespace_size_in_header = fsp_header_get_tablespace_size();

	if (!srv_read_only_mode
	    && !srv_sys_space.can_auto_extend_last_file()
	    && sum_of_data_file_sizes != tablespace_size_in_header) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: tablespace size"
			" stored in header is %lu pages, but\n",
			(ulong) tablespace_size_in_header);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"InnoDB: the sum of data file sizes is %lu pages\n",
			(ulong) sum_of_data_file_sizes);

		if (srv_force_recovery == 0
		    && sum_of_data_file_sizes < tablespace_size_in_header) {
			/* This is a fatal error, the tail of a tablespace is
			missing */

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Cannot start InnoDB."
				" The tail of the system tablespace is\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: missing. Have you edited"
				" innodb_data_file_path in my.cnf in an\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: inappropriate way, removing"
				" ibdata files from there?\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: You can set innodb_force_recovery=1"
				" in my.cnf to force\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: a startup if you are trying"
				" to recover a badly corrupt database.\n");

			return(srv_init_abort(DB_ERROR));
		}
	}

	if (!srv_read_only_mode
	    && srv_sys_space.can_auto_extend_last_file()
	    && sum_of_data_file_sizes < tablespace_size_in_header) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: tablespace size stored in header"
			" is %lu pages, but\n",
			(ulong) tablespace_size_in_header);
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: the sum of data file sizes"
			" is only %lu pages\n",
			(ulong) sum_of_data_file_sizes);

		if (srv_force_recovery == 0) {

			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Cannot start InnoDB. The tail of"
				" the system tablespace is\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: missing. Have you edited"
				" innodb_data_file_path in my.cnf in an\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: inappropriate way, removing"
				" ibdata files from there?\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: You can set innodb_force_recovery=1"
				" in my.cnf to force\n");
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: a startup if you are trying to"
				" recover a badly corrupt database.\n");

			return(srv_init_abort(DB_ERROR));
		}
	}

	{
		/* We use this mutex to test the return value of
		pthread_mutex_trylock on successful locking. HP-UX
		does NOT return 0, though Linux et al do. */

		SysMutex	mutex;

		/* Check that OS utexes work as expected */
		mutex_create("test_mutex", &mutex);

		if (mutex_enter_nowait(&mutex) != 0) {

			ib_logf(IB_LOG_LEVEL_FATAL,
				"pthread_mutex_trylock returns "
				"an unexpected value on success! "
				"Cannot continue.");

			exit(EXIT_FAILURE);
		}

		mutex_exit(&mutex);

		mutex_enter(&mutex);

		mutex_exit(&mutex);

		mutex_free(&mutex);
	}

	if (srv_print_verbose_log) {
		ib_logf(IB_LOG_LEVEL_INFO,
			"%s started; log sequence number " LSN_PF "",
			INNODB_VERSION_STR, srv_start_lsn);
	}

	if (srv_force_recovery > 0) {
		ib_logf(IB_LOG_LEVEL_INFO,
			"!!! innodb_force_recovery is set to %lu !!!",
			(ulong) srv_force_recovery);
	}

	if (srv_force_recovery == 0) {
		/* In the insert buffer we may have even bigger tablespace
		id's, because we may have dropped those tablespaces, but
		insert buffer merge has not had time to clean the records from
		the ibuf tree. */

		ibuf_update_max_tablespace_id();
	}

	if (!srv_read_only_mode) {
		/* Create the buffer pool dump/load thread */
		os_thread_create(buf_dump_thread, NULL, NULL);

		/* Create the dict stats gathering thread */
		os_thread_create(dict_stats_thread, NULL, NULL);

		/* Create the thread that will optimize the FTS sub-system. */
		fts_optimize_init();

		srv_start_state_set(SRV_START_STATE_STAT);
	}

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
@return	DB_SUCCESS or error code */

dberr_t
innobase_shutdown_for_mysql(void)
/*=============================*/
{
	if (!srv_was_started) {
		if (srv_is_being_started) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Shutting down an improperly started, "
				"or created database!");
		}

		return(DB_SUCCESS);
	}

	if (!srv_read_only_mode) {
		/* Shutdown the FTS optimize sub system. */
		fts_optimize_start_shutdown();

		fts_optimize_end();
	}

	/* 1. Flush the buffer pool to disk, write the current lsn to
	the tablespace header(s), and copy all log data to archive.
	The step 1 is the real InnoDB shutdown. The remaining steps 2 - ...
	just free data structures after the shutdown. */

	logs_empty_and_mark_files_at_shutdown();

	if (srv_conc_get_active_threads() != 0) {
		ib_logf(IB_LOG_LEVEL_WARN,
			"Query counter shows %ld queries still "
			"inside InnoDB at shutdown",
			srv_conc_get_active_threads());
	}

	/* 2. Make all threads created by InnoDB to exit */
	srv_shutdown_all_bg_threads();


	if (srv_monitor_file) {
		fclose(srv_monitor_file);
		srv_monitor_file = 0;
		if (srv_monitor_file_name) {
			unlink(srv_monitor_file_name);
			mem_free(srv_monitor_file_name);
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
	btr_search_disable();

	ibuf_close();
	log_shutdown();
	lock_sys_close();
	trx_sys_file_format_close();
	trx_sys_close();

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
	srv_mon_free();
	srv_free();
	fil_close();

	/* 4. Free all allocated memory */

	pars_lexer_close();
	log_mem_free();
	buf_pool_free(srv_buf_pool_instances);

	mem_close();

	/* 6. Free the thread management resoruces. */
	os_thread_free();

	/* 7. Free the synchronisation infrastructure. */
	sync_check_close();

	/* ut_free_all_mem() frees all allocated memory not freed yet
	in shutdown, and it will also free the ut_list_mutex, so it
	should be the last one for all operation */
	ut_free_all_mem();

	if (dict_foreign_err_file) {
		fclose(dict_foreign_err_file);
	}

	if (srv_print_verbose_log) {
		ib_logf(IB_LOG_LEVEL_INFO,
			"Shutdown completed; log sequence number " LSN_PF "",
			srv_shutdown_lsn);
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

/*****************************************************************//**
Get the meta-data filename from the table name. */

void
srv_get_meta_data_filename(
/*=======================*/
	dict_table_t*	table,		/*!< in: table */
	char*			filename,	/*!< out: filename */
	ulint			max_len)	/*!< in: filename max length */
{
	ulint			len;
	char*			path;
	char*			suffix;
	static const ulint	suffix_len = strlen(".cfg");

	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		dict_get_and_save_data_dir_path(table, false);
		ut_a(table->data_dir_path);

		path = os_file_make_remote_pathname(
			table->data_dir_path, table->name, "cfg");
	} else {
		path = fil_make_ibd_name(table->name, false);
	}

	ut_a(path);
	len = ut_strlen(path);
	ut_a(max_len >= len);

	suffix = path + (len - suffix_len);
	if (strncmp(suffix, ".cfg", suffix_len) == 0) {
		strcpy(filename, path);
	} else {
		ut_ad(strncmp(suffix, ".ibd", suffix_len) == 0);

		strncpy(filename, path, len - suffix_len);
		suffix = filename + (len - suffix_len);
		strcpy(suffix, ".cfg");
	}

	mem_free(path);

	srv_normalize_path_for_win(filename);
}
