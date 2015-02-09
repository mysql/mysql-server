/***********************************************************************

Copyright (c) 1995, 2014, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/

/**************************************************//**
@file os/os0file.cc
The interface to the operating system file i/o primitives

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#include "ha_prototypes.h"

#include "os0file.h"

#ifdef UNIV_NONINL
#include "os0file.ic"
#endif

#include "ut0mem.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "fil0fil.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "srv0mon.h"
#ifndef UNIV_HOTBACKUP
# include "os0event.h"
# include "os0thread.h"
#else /* !UNIV_HOTBACKUP */
# ifdef _WIN32
/* Add includes for the _stat() call to compile on Windows */
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <errno.h>
# endif /* _WIN32 */
#endif /* !UNIV_HOTBACKUP */

#if defined(LINUX_NATIVE_AIO)
#include <libaio.h>
#endif

#ifdef UNIV_DEBUG
/** Set when InnoDB has invoked exit(). */
bool	innodb_calling_exit;
#endif /* UNIV_DEBUG */

/** Insert buffer segment id */
static const ulint IO_IBUF_SEGMENT = 0;

/** Log segment id */
static const ulint IO_LOG_SEGMENT = 1;

/** Number of retries for partial I/O's */
static const ulint NUM_RETRIES_ON_PARTIAL_IO = 10;

/* This specifies the file permissions InnoDB uses when it creates files in
Unix; the value of os_innodb_umask is initialized in ha_innodb.cc to
my_umask */

#ifndef _WIN32
/** Umask for creating files */
ulint	os_innodb_umask = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
#else
/** Umask for creating files */
ulint	os_innodb_umask	= 0;
#endif /* _WIN32 */

#ifndef UNIV_HOTBACKUP

/** We use these mutexes to protect lseek + file i/o operation, if the
OS does not provide an atomic pread or pwrite, or similar */
const static ulint	OS_FILE_N_SEEK_MUTEXES = 16;

SysMutex*		os_file_seek_mutexes[OS_FILE_N_SEEK_MUTEXES];

/** In simulated aio, merge at most this many consecutive i/os */
static const ulint	OS_AIO_MERGE_N_CONSECUTIVE	= 64;

/**********************************************************************

InnoDB AIO Implementation:
=========================

We support native AIO for windows and linux. For rest of the platforms
we simulate AIO by special io-threads servicing the IO-requests.

Simulated AIO:
==============

In platforms where we 'simulate' AIO following is a rough explanation
of the high level design.
There are four io-threads (for ibuf, log, read, write).
All synchronous IO requests are serviced by the calling thread using
os_file_write/os_file_read. The Asynchronous requests are queued up
in an array (there are four such arrays) by the calling thread.
Later these requests are picked up by the io-thread and are serviced
synchronously.

Windows native AIO:
==================

If srv_use_native_aio is not set then windows follow the same
code as simulated AIO. If the flag is set then native AIO interface
is used. On windows, one of the limitation is that if a file is opened
for AIO no synchronous IO can be done on it. Therefore we have an
extra fifth array to queue up synchronous IO requests.
There are innodb_file_io_threads helper threads. These threads work
on the four arrays mentioned above in Simulated AIO. No thread is
required for the sync array.
If a synchronous IO request is made, it is first queued in the sync
array. Then the calling thread itself waits on the request, thus
making the call synchronous.
If an AIO request is made the calling thread not only queues it in the
array but also submits the requests. The helper thread then collects
the completed IO request and calls completion routine on it.

Linux native AIO:
=================

If we have libaio installed on the system and innodb_use_native_aio
is set to true we follow the code path of native AIO, otherwise we
do simulated AIO.
There are innodb_file_io_threads helper threads. These threads work
on the four arrays mentioned above in Simulated AIO.
If a synchronous IO request is made, it is handled by calling
os_file_write/os_file_read.
If an AIO request is made the calling thread not only queues it in the
array but also submits the requests. The helper thread then collects
the completed IO request and calls completion routine on it.

**********************************************************************/


#ifdef UNIV_PFS_IO
/* Keys to register InnoDB I/O with performance schema */
mysql_pfs_key_t  innodb_data_file_key;
mysql_pfs_key_t  innodb_log_file_key;
mysql_pfs_key_t  innodb_temp_file_key;
#endif /* UNIV_PFS_IO */

/** The asynchronous i/o array slot structure */
struct os_aio_slot_t{
	ulint		pos;		/*!< index of the slot in the aio
					array */
	bool		is_reserved;	/*!< true if this slot is reserved */
	time_t		reservation_time;/*!< time when reserved */
	ulint		len;		/*!< length of the block to read or
					write */
	byte*		buf;		/*!< buffer used in i/o */
	ulint		type;		/*!< OS_FILE_READ or OS_FILE_WRITE */
	os_offset_t	offset;		/*!< file offset in bytes */
	os_file_t	file;		/*!< file where to read or write */
	const char*	name;		/*!< file name or path */
	bool		io_already_done;/*!< used only in simulated aio:
					true if the physical i/o already
					made and only the slot message
					needs to be passed to the caller
					of os_aio_simulated_handle */
	fil_node_t*	message1;	/*!< message which is given by the */
	void*		message2;	/*!< the requester of an aio operation
					and which can be used to identify
					which pending aio operation was
					completed */
#ifdef WIN_ASYNC_IO
	HANDLE		handle;		/*!< handle object we need in the
					OVERLAPPED struct */
	OVERLAPPED	control;	/*!< Windows control block for the
					aio request */
#elif defined(LINUX_NATIVE_AIO)
	struct iocb	control;	/* Linux control block for aio */
	int		n_bytes;	/* bytes written/read. */
	int		ret;		/* AIO return code */
#endif /* WIN_ASYNC_IO */
};

/** The asynchronous i/o array structure */
struct os_aio_array_t{
	SysMutex	mutex;	/*!< the mutex protecting the aio array */

	os_event_t	not_full;
				/*!< The event which is set to the
				signaled state when there is space in
				the aio outside the ibuf segment */
	os_event_t	is_empty;
				/*!< The event which is set to the
				signaled state when there are no
				pending i/os in this array */
	ulint		n_slots;/*!< Total number of slots in the aio
				array.  This must be divisible by
				n_threads. */
	ulint		n_segments;
				/*!< Number of segments in the aio
				array of pending aio requests. A
				thread can wait separately for any one
				of the segments. */
	ulint		cur_seg;/*!< We reserve IO requests in round
				robin fashion to different segments.
				This points to the segment that is to
				be used to service next IO request. */
	ulint		n_reserved;
				/*!< Number of reserved slots in the
				aio array outside the ibuf segment */
	os_aio_slot_t*	slots;	/*!< Pointer to the slots in the array */
#ifdef _WIN32
	HANDLE*		handles;
				/*!< Pointer to an array of OS native
				event handles where we copied the
				handles from slots, in the same
				order. This can be used in
				WaitForMultipleObjects; used only in
				Windows */
#endif /* _WIN32 */

#if defined(LINUX_NATIVE_AIO)
	io_context_t*		aio_ctx;
				/* completion queue for IO. There is
				one such queue per segment. Each thread
				will work on one ctx exclusively. */
	struct io_event*	aio_events;
				/* The array to collect completed IOs.
				There is one such event for each
				possible pending IO. The size of the
				array is equal to n_slots. */
#endif /* LINUX_NATIV_AIO */
};

#if defined(LINUX_NATIVE_AIO)
/** timeout for each io_getevents() call = 500ms. */
#define OS_AIO_REAP_TIMEOUT	(500000000UL)

/** time to sleep, in microseconds if io_setup() returns EAGAIN. */
#define OS_AIO_IO_SETUP_RETRY_SLEEP	(500000UL)

/** number of attempts before giving up on io_setup(). */
#define OS_AIO_IO_SETUP_RETRY_ATTEMPTS	5
#endif

/** Array of events used in simulated aio */
static os_event_t*	os_aio_segment_wait_events = NULL;

/** The aio arrays for non-ibuf i/o and ibuf i/o, as well as sync aio. These
are NULL when the module has not yet been initialized. @{ */
static os_aio_array_t*	os_aio_read_array	= NULL;	/*!< Reads */
static os_aio_array_t*	os_aio_write_array	= NULL;	/*!< Writes */
static os_aio_array_t*	os_aio_ibuf_array	= NULL;	/*!< Insert buffer */
static os_aio_array_t*	os_aio_log_array	= NULL;	/*!< Redo log */
static os_aio_array_t*	os_aio_sync_array	= NULL;	/*!< Synchronous I/O */
/* @} */

/** Number of asynchronous I/O segments.  Set by os_aio_init(). */
static ulint	os_aio_n_segments	= ULINT_UNDEFINED;

/** If the following is true, read i/o handler threads try to
wait until a batch of new read requests have been posted */
static bool	os_aio_recommend_sleep_for_read_threads = false;
#endif /* !UNIV_HOTBACKUP */

ulint	os_n_file_reads		= 0;
ulint	os_bytes_read_since_printout = 0;
ulint	os_n_file_writes	= 0;
ulint	os_n_fsyncs		= 0;
ulint	os_n_file_reads_old	= 0;
ulint	os_n_file_writes_old	= 0;
ulint	os_n_fsyncs_old		= 0;
time_t	os_last_printout;

bool	os_has_said_disk_full	= false;

/** Number of pending os_file_pread() operations */
ulint	os_file_n_pending_preads  = 0;
/** Number of pending os_file_pwrite() operations */
ulint	os_file_n_pending_pwrites = 0;
/** Number of pending write operations */
ulint	os_n_pending_writes = 0;
/** Number of pending read operations */
ulint	os_n_pending_reads = 0;

#ifdef UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Validates the consistency the aio system some of the time.
@return true if ok or the check was skipped */
bool
os_aio_validate_skip(void)
/*======================*/
{
/** Try os_aio_validate() every this many times */
# define OS_AIO_VALIDATE_SKIP	13

	/** The os_aio_validate() call skip counter.
	Use a signed type because of the race condition below. */
	static int os_aio_validate_count = OS_AIO_VALIDATE_SKIP;

	/* There is a race condition below, but it does not matter,
	because this call is only for heuristic purposes. We want to
	reduce the call frequency of the costly os_aio_validate()
	check in debug builds. */
	if (--os_aio_validate_count > 0) {
		return(true);
	}

	os_aio_validate_count = OS_AIO_VALIDATE_SKIP;
	return(os_aio_validate());
}
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_DEBUG */

/***********************************************************************//**
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@return error number, or OS error number + 100 */
static
ulint
os_file_get_last_error_low(
/*=======================*/
	bool	report_all_errors,	/*!< in: true if we want an error
					message printed of all errors */
	bool	on_error_silent)	/*!< in: true then don't print any
					diagnostic to the log */
{
#ifdef _WIN32

	ulint	err = (ulint) GetLastError();
	if (err == ERROR_SUCCESS) {
		return(0);
	}

	if (report_all_errors
	    || (!on_error_silent
		&& err != ERROR_DISK_FULL
		&& err != ERROR_FILE_EXISTS)) {

		ib::error() << "Operating system error number " << err
			<< " in a file operation.";

		if (err == ERROR_PATH_NOT_FOUND) {
			ib::error() << "The error means the system"
				" cannot find the path specified.";

			if (srv_is_being_started) {
				ib::error() << "If you are installing InnoDB,"
					" remember that you must create"
					" directories yourself, InnoDB"
					" does not create them.";
			}
		} else if (err == ERROR_ACCESS_DENIED) {
			ib::error() << "The error means mysqld does not have"
				" the access rights to"
				" the directory. It may also be"
				" you have created a subdirectory"
				" of the same name as a data file.";
		} else if (err == ERROR_SHARING_VIOLATION
			   || err == ERROR_LOCK_VIOLATION) {
				ib::error() << "The error means that another"
					" program is using InnoDB's files."
					" This might be a backup or antivirus"
					" software or another instance of"
					" MySQL. Please close it to get rid of"
					" this error.";
		} else if (err == ERROR_WORKING_SET_QUOTA
			   || err == ERROR_NO_SYSTEM_RESOURCES) {
			ib::error() << "The error means that there are no"
				" sufficient system resources or quota to"
				" complete the operation.";
		} else if (err == ERROR_OPERATION_ABORTED) {
			ib::error() << "The error means that the I/O"
				" operation has been aborted"
				" because of either a thread exit"
				" or an application request."
				" Retry attempt is made.";
		} else {
			ib::info() << OPERATING_SYSTEM_ERROR_MSG;
		}
	}

	if (err == ERROR_FILE_NOT_FOUND) {
		return(OS_FILE_NOT_FOUND);
	} else if (err == ERROR_DISK_FULL) {
		return(OS_FILE_DISK_FULL);
	} else if (err == ERROR_FILE_EXISTS) {
		return(OS_FILE_ALREADY_EXISTS);
	} else if (err == ERROR_SHARING_VIOLATION
		   || err == ERROR_LOCK_VIOLATION) {
		return(OS_FILE_SHARING_VIOLATION);
	} else if (err == ERROR_WORKING_SET_QUOTA
		   || err == ERROR_NO_SYSTEM_RESOURCES) {
		return(OS_FILE_INSUFFICIENT_RESOURCE);
	} else if (err == ERROR_OPERATION_ABORTED) {
		return(OS_FILE_OPERATION_ABORTED);
	} else if (err == ERROR_ACCESS_DENIED) {
		return(OS_FILE_ACCESS_VIOLATION);
	} else {
		return(OS_FILE_ERROR_MAX + err);
	}
#else
	int err = errno;
	if (err == 0) {
		return(0);
	}

	if (report_all_errors
	    || (err != ENOSPC && err != EEXIST && !on_error_silent)) {

		ib::error() << "Operating system error number " << err
			<< " in a file operation.";

		if (err == ENOENT) {
			ib::error() << "The error means the system"
				" cannot find the path specified.";

			if (srv_is_being_started) {
				ib::error() << "If you are installing InnoDB,"
					" remember that you must create"
					" directories yourself, InnoDB"
					" does not create them.";
			}
		} else if (err == EACCES) {
			ib::error() << "The error means mysqld does not have"
				" the access rights to the directory.";
		} else {
			if (strerror(err) != NULL) {
				ib::error() << "Error number " << err
					<< " means '" << strerror(err) << "'.";
			}

			ib::info() << OPERATING_SYSTEM_ERROR_MSG;
		}
	}

	switch (err) {
	case ENOSPC:
		return(OS_FILE_DISK_FULL);
	case ENOENT:
		return(OS_FILE_NOT_FOUND);
	case EEXIST:
		return(OS_FILE_ALREADY_EXISTS);
	case EXDEV:
	case ENOTDIR:
	case EISDIR:
		return(OS_FILE_PATH_ERROR);
	case EAGAIN:
		if (srv_use_native_aio) {
			return(OS_FILE_AIO_RESOURCES_RESERVED);
		}
		break;
	case EINTR:
		if (srv_use_native_aio) {
			return(OS_FILE_AIO_INTERRUPTED);
		}
		break;
	case EACCES:
		return(OS_FILE_ACCESS_VIOLATION);
	}
	return(OS_FILE_ERROR_MAX + err);
#endif
}

/***********************************************************************//**
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@return error number, or OS error number + 100 */
ulint
os_file_get_last_error(
/*===================*/
	bool	report_all_errors)	/*!< in: true if we want an error
					message printed of all errors */
{
	return(os_file_get_last_error_low(report_all_errors, false));
}

/****************************************************************//**
Does error handling when a file operation fails.
Conditionally exits (calling exit(3)) based on should_exit value and the
error type, if should_exit is true then on_error_silent is ignored.
@return true if we should retry the operation */
static
bool
os_file_handle_error_cond_exit(
/*===========================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation,	/*!< in: operation */
	bool		should_exit,	/*!< in: call exit(3) if unknown error
					and this parameter is true */
	bool		on_error_silent)/*!< in: if true then don't print
					any message to the log iff it is
					an unknown non-fatal error */
{
	ulint	err;

	err = os_file_get_last_error_low(false, on_error_silent);

	switch (err) {
	case OS_FILE_DISK_FULL:
		/* We only print a warning about disk full once */

		if (os_has_said_disk_full) {

			return(false);
		}

		/* Disk full error is reported irrespective of the
		on_error_silent setting. */

		if (name) {
			ib::error() << "Encountered a problem with file "
				<< name;
		}

		ib::error() << "Disk is full. Try to clean the disk to free"
			<< " space.";

		os_has_said_disk_full = true;

		return(false);

	case OS_FILE_AIO_RESOURCES_RESERVED:
	case OS_FILE_AIO_INTERRUPTED:

		return(true);

	case OS_FILE_PATH_ERROR:
	case OS_FILE_ALREADY_EXISTS:
	case OS_FILE_ACCESS_VIOLATION:

		return(false);

	case OS_FILE_SHARING_VIOLATION:

		os_thread_sleep(10000000);  /* 10 sec */
		return(true);

	case OS_FILE_OPERATION_ABORTED:
	case OS_FILE_INSUFFICIENT_RESOURCE:

		os_thread_sleep(100000);	/* 100 ms */
		return(true);

	default:

		/* If it is an operation that can crash on error then it
		is better to ignore on_error_silent and print an error message
		to the log. */

		if (should_exit || !on_error_silent) {
			ib::error() << "File "
				<< (name != NULL ? name : "(unknown)")
				<< ": '" << operation << "'"
				" returned OS error " << err << "."
				<< (should_exit
				    ? " Cannot continue operation" : "");
		}

		if (should_exit) {
			ib::error() << "Cannot continue operation.";
			fflush(stderr);
			ut_d(innodb_calling_exit = true);
			exit(3);
		}
	}

	return(false);
}

/****************************************************************//**
Does error handling when a file operation fails.
@return true if we should retry the operation */
static
bool
os_file_handle_error(
/*=================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation)	/*!< in: operation */
{
	/* exit in case of unknown error */
	return(os_file_handle_error_cond_exit(name, operation, true, false));
}

/****************************************************************//**
Does error handling when a file operation fails.
@return true if we should retry the operation */
static
bool
os_file_handle_error_no_exit(
/*=========================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation,	/*!< in: operation */
	bool		on_error_silent)/*!< in: if true then don't print
					any message to the log. */
{
	/* don't exit in case of unknown error */
	return(os_file_handle_error_cond_exit(
			name, operation, false, on_error_silent));
}

#undef USE_FILE_LOCK
#define USE_FILE_LOCK
#if defined(UNIV_HOTBACKUP) || defined(_WIN32)
/* InnoDB Hot Backup does not lock the data files.
 * On Windows, mandatory locking is used.
 */
# undef USE_FILE_LOCK
#endif
#ifdef USE_FILE_LOCK
/****************************************************************//**
Obtain an exclusive lock on a file.
@return 0 on success */
static
int
os_file_lock(
/*=========*/
	int		fd,	/*!< in: file descriptor */
	const char*	name)	/*!< in: file name */
{
	struct flock lk;

	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = lk.l_len = 0;

	if (fcntl(fd, F_SETLK, &lk) == -1) {

		ib::error() << "Unable to lock " << name << ", error: "
			<< errno;

		if (errno == EAGAIN || errno == EACCES) {
			ib::info() << "Check that you do not already have"
				" another mysqld process using the"
				" same InnoDB data or log files.";
		}

		return(-1);
	}

	return(0);
}
#endif /* USE_FILE_LOCK */

#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Creates the seek mutexes used in positioned reads and writes. */
void
os_io_init_simple(void)
/*===================*/
{
	for (ulint i = 0; i < OS_FILE_N_SEEK_MUTEXES; i++) {
		os_file_seek_mutexes[i] = UT_NEW_NOKEY(SysMutex());
		mutex_create("os_file_seek_mutex", os_file_seek_mutexes[i]);
	}
}

/***********************************************************************//**
Creates a temporary file.  This function is like tmpfile(3), but
the temporary file is created in the MySQL temporary directory.
@return temporary file handle, or NULL on error */
FILE*
os_file_create_tmpfile(void)
/*========================*/
{
	FILE*	file	= NULL;
	int	fd	= innobase_mysql_tmpfile();

	if (fd >= 0) {
		file = fdopen(fd, "w+b");
	}

	if (!file) {
		ib::error() << "Unable to create temporary file; errno: "
			<< errno;
		if (fd >= 0) {
			close(fd);
		}
	}

	return(file);
}
#endif /* !UNIV_HOTBACKUP */

/***********************************************************************//**
The os_file_opendir() function opens a directory stream corresponding to the
directory named by the dirname argument. The directory stream is positioned
at the first entry. In both Unix and Windows we automatically skip the '.'
and '..' items at the start of the directory listing.
@return directory stream, NULL if error */
os_file_dir_t
os_file_opendir(
/*============*/
	const char*	dirname,	/*!< in: directory name; it must not
					contain a trailing '\' or '/' */
	bool		error_is_fatal)	/*!< in: true if we should treat an
					error as a fatal error; if we try to
					open symlinks then we do not wish a
					fatal error if it happens not to be
					a directory */
{
	os_file_dir_t		dir;
#ifdef _WIN32
	WIN32_FIND_DATA	FindFileData;
	char		path[OS_FILE_MAX_PATH + 3];

	ut_a(strlen(dirname) < OS_FILE_MAX_PATH);

	strcpy(path, dirname);
	strcpy(path + strlen(path), "\\*");

	/* Note that in Windows opening the 'directory stream' also retrieves
	the first entry in the directory. Since it is '.', that is no problem,
	as we will skip over the '.' and '..' entries anyway. */

	dir = FindFirstFile((LPCTSTR) path, &FindFileData);

	if (dir == INVALID_HANDLE_VALUE) {

		if (error_is_fatal) {
			os_file_handle_error(dirname, "opendir");
		}

		return(NULL);
	}

	/* Ensure that the first entry opened is indeed "." because we are
	going to skip it (going to call FindNextFile() without considering
	the value of FindFileData) and we do not want to skip some real
	file. */
	ut_ad(strcmp(FindFileData.cFileName, ".") == 0);

	return(dir);
#else
	dir = opendir(dirname);

	if (dir == NULL && error_is_fatal) {
		os_file_handle_error(dirname, "opendir");
	}

	return(dir);
#endif /* _WIN32 */
}

/***********************************************************************//**
Closes a directory stream.
@return 0 if success, -1 if failure */
int
os_file_closedir(
/*=============*/
	os_file_dir_t	dir)	/*!< in: directory stream */
{
#ifdef _WIN32
	BOOL		ret;

	ret = FindClose(dir);

	if (!ret) {
		os_file_handle_error_no_exit(NULL, "closedir", false);

		return(-1);
	}

	return(0);
#else
	int	ret;

	ret = closedir(dir);

	if (ret) {
		os_file_handle_error_no_exit(NULL, "closedir", false);
	}

	return(ret);
#endif /* _WIN32 */
}

/***********************************************************************//**
This function returns information of the next file in the directory. We jump
over the '.' and '..' entries in the directory.
@return 0 if ok, -1 if error, 1 if at the end of the directory */
int
os_file_readdir_next_file(
/*======================*/
	const char*	dirname,/*!< in: directory name or path */
	os_file_dir_t	dir,	/*!< in: directory stream */
	os_file_stat_t*	info)	/*!< in/out: buffer where the info is returned */
{
#ifdef _WIN32
	WIN32_FIND_DATA	FindFileData;
	BOOL		ret;

next_file:
	ret = FindNextFile(dir, &FindFileData);

	if (ret) {
		ut_a(strlen((char*) FindFileData.cFileName)
		     < OS_FILE_MAX_PATH);

		if (strcmp((char*) FindFileData.cFileName, ".") == 0
		    || strcmp((char*) FindFileData.cFileName, "..") == 0) {

			goto next_file;
		}

		strcpy(info->name, (char*) FindFileData.cFileName);

		info->size = static_cast<int64_t>(FindFileData.nFileSizeLow)
			+ (static_cast<int64_t>(FindFileData.nFileSizeHigh)
			   << 32);

		if (FindFileData.dwFileAttributes
		    & FILE_ATTRIBUTE_REPARSE_POINT) {
			/* TODO: test Windows symlinks */
			/* TODO: MySQL has apparently its own symlink
			implementation in Windows, dbname.sym can
			redirect a database directory:
			REFMAN "windows-symbolic-links.html" */
			info->type = OS_FILE_TYPE_LINK;
		} else if (FindFileData.dwFileAttributes
			   & FILE_ATTRIBUTE_DIRECTORY) {
			info->type = OS_FILE_TYPE_DIR;
		} else {
			/* It is probably safest to assume that all other
			file types are normal. Better to check them rather
			than blindly skip them. */

			info->type = OS_FILE_TYPE_FILE;
		}

		return(0);
	}

	if (GetLastError() == ERROR_NO_MORE_FILES) {
		return(1);
	}

	os_file_handle_error_no_exit(NULL, "readdir_next_file", false);
	return(-1);
#else
	struct dirent*	ent;
	char*		full_path;
	int		ret;
	struct stat	statinfo;
#ifdef HAVE_READDIR_R
	char		dirent_buf[sizeof(struct dirent)
				   + _POSIX_PATH_MAX + 100];
	/* In /mysys/my_lib.c, _POSIX_PATH_MAX + 1 is used as
	the max file name len; but in most standards, the
	length is NAME_MAX; we add 100 to be even safer */
#endif

next_file:

#ifdef HAVE_READDIR_R
	ret = readdir_r(dir, (struct dirent*) dirent_buf, &ent);

	if (ret != 0) {
		ib::error() << "Cannot read directory " << dirname
			<< ", error " << ret;

		return(-1);
	}

	if (ent == NULL) {
		/* End of directory */

		return(1);
	}

	ut_a(strlen(ent->d_name) < _POSIX_PATH_MAX + 100 - 1);
#else
	ent = readdir(dir);

	if (ent == NULL) {

		return(1);
	}
#endif
	ut_a(strlen(ent->d_name) < OS_FILE_MAX_PATH);

	if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {

		goto next_file;
	}

	strcpy(info->name, ent->d_name);

	full_path = static_cast<char*>(
		ut_malloc_nokey(strlen(dirname) + strlen(ent->d_name) + 10));

	sprintf(full_path, "%s/%s", dirname, ent->d_name);

	ret = stat(full_path, &statinfo);

	if (ret) {

		if (errno == ENOENT) {
			/* readdir() returned a file that does not exist,
			it must have been deleted in the meantime. Do what
			would have happened if the file was deleted before
			readdir() - ignore and go to the next entry.
			If this is the last entry then info->name will still
			contain the name of the deleted file when this
			function returns, but this is not an issue since the
			caller shouldn't be looking at info when end of
			directory is returned. */

			ut_free(full_path);

			goto next_file;
		}

		os_file_handle_error_no_exit(full_path, "stat", false);

		ut_free(full_path);

		return(-1);
	}

	info->size = static_cast<int64_t>(statinfo.st_size);

	if (S_ISDIR(statinfo.st_mode)) {
		info->type = OS_FILE_TYPE_DIR;
	} else if (S_ISLNK(statinfo.st_mode)) {
		info->type = OS_FILE_TYPE_LINK;
	} else if (S_ISREG(statinfo.st_mode)) {
		info->type = OS_FILE_TYPE_FILE;
	} else {
		info->type = OS_FILE_TYPE_UNKNOWN;
	}

	ut_free(full_path);

	return(0);
#endif
}

/*****************************************************************//**
This function attempts to create a directory named pathname. The new
directory gets default permissions. On Unix the permissions are
(0770 & ~umask). If the directory exists already, nothing is done and
the call succeeds, unless the fail_if_exists arguments is true.
If another error occurs, such as a permission error, this does not crash,
but reports the error and returns false.
@return true if call succeeds, false on error */
bool
os_file_create_directory(
/*=====================*/
	const char*	pathname,	/*!< in: directory name as
					null-terminated string */
	bool		fail_if_exists)	/*!< in: if true, pre-existing directory
					is treated as an error. */
{
#ifdef _WIN32
	BOOL	rcode;

	rcode = CreateDirectory((LPCTSTR) pathname, NULL);
	if (!(rcode != 0
	      || (GetLastError() == ERROR_ALREADY_EXISTS
		  && !fail_if_exists))) {

		os_file_handle_error_no_exit(
			pathname, "CreateDirectory", false);

		return(false);
	}

	return(true);
#else
	int	rcode;

	rcode = mkdir(pathname, 0770);

	if (!(rcode == 0 || (errno == EEXIST && !fail_if_exists))) {
		/* failure */
		os_file_handle_error_no_exit(pathname, "mkdir", false);

		return(false);
	}

	return (true);
#endif /* _WIN32 */
}

/****************************************************************//**
NOTE! Use the corresponding macro os_file_create_simple(), not directly
this function!
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
os_file_t
os_file_create_simple_func(
/*=======================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: create mode */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY or
				OS_FILE_READ_WRITE */
	bool		read_only_mode,
				/*!< in: if true, read only mode
				checks are enforced. */
	bool*		success)/*!< out: true if succeed, false if error */
{
	os_file_t	file;
	bool		retry;

	*success = false;
#ifdef _WIN32
	DWORD		access;
	DWORD		create_flag;
	DWORD		attributes	= 0;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		create_flag = OPEN_EXISTING;

	} else if (read_only_mode) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else if (create_mode == OS_FILE_CREATE_PATH) {

		ut_a(!read_only_mode);

		/* Create subdirs along the path if needed  */
		*success = os_file_create_subdirs_if_needed(name);

		if (!*success) {

			ib::error() << "Unable to create subdirectories '"
				<< name << "'";

			return(OS_FILE_CLOSED);
		}

		create_flag = CREATE_NEW;
		create_mode = OS_FILE_CREATE;

	} else {
		ib::error() << "Unknown file create mode (" << create_mode
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	if (access_type == OS_FILE_READ_ONLY) {
		access = GENERIC_READ;
	} else if (read_only_mode) {

		ib::info() << "Read only mode set. Unable to open file '"
			<< name << "' in RW mode, trying RO mode";

		access = GENERIC_READ;

	} else if (access_type == OS_FILE_READ_WRITE) {
		access = GENERIC_READ | GENERIC_WRITE;
	} else {
		ib::error() << "Unknown file access type (" << access_type
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	do {
		/* Use default security attributes and no template file. */

		file = CreateFile(
			(LPCTSTR) name, access, FILE_SHARE_READ, NULL,
			create_flag, attributes, NULL);

		if (file == INVALID_HANDLE_VALUE) {

			*success = false;

			retry = os_file_handle_error(
				name, create_mode == OS_FILE_OPEN ?
				"open" : "create");

		} else {
			*success = true;
			retry = false;
		}

	} while (retry);

#else /* _WIN32 */
	int		create_flag;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		if (access_type == OS_FILE_READ_ONLY) {
			create_flag = O_RDONLY;
		} else if (read_only_mode) {
			create_flag = O_RDONLY;
		} else {
			create_flag = O_RDWR;
		}

	} else if (read_only_mode) {

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else if (create_mode == OS_FILE_CREATE_PATH) {

		/* Create subdirs along the path if needed  */

		*success = os_file_create_subdirs_if_needed(name);

		if (!*success) {

			ib::error() << "Unable to create subdirectories '"
				<< name << "'";

			return(OS_FILE_CLOSED);
		}

		create_flag = O_RDWR | O_CREAT | O_EXCL;
		create_mode = OS_FILE_CREATE;
	} else {

		ib::error() << "Unknown file create mode (" << create_mode
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	do {
		file = ::open(name, create_flag, os_innodb_umask);

		if (file == -1) {
			*success = false;

			retry = os_file_handle_error(
				name,
				create_mode == OS_FILE_OPEN
				?  "open" : "create");
		} else {
			*success = true;
			retry = false;
		}

	} while (retry);

#ifdef USE_FILE_LOCK
	if (!read_only_mode
	    && *success
	    && access_type == OS_FILE_READ_WRITE
	    && os_file_lock(file, name)) {

		*success = false;
		close(file);
		file = -1;
	}
#endif /* USE_FILE_LOCK */

#endif /* _WIN32 */

	return(file);
}

/****************************************************************//**
NOTE! Use the corresponding macro
os_file_create_simple_no_error_handling(), not directly this function!
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
os_file_t
os_file_create_simple_no_error_handling_func(
/*=========================================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: create mode */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY,
				OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file */
	bool		read_only_mode,
				/*!< in: if true, read only mode
				checks are enforced. */
	bool*		success)/*!< out: true if succeed, false if error */
{
	os_file_t	file;

	*success = false;
#ifdef _WIN32
	DWORD		access;
	DWORD		create_flag;
	DWORD		attributes	= 0;
	DWORD		share_mode	= FILE_SHARE_READ;

	ut_a(name);

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {
		create_flag = OPEN_EXISTING;
	} else if (read_only_mode) {
		create_flag = OPEN_EXISTING;
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = CREATE_NEW;
	} else {

		ib::error() << "Unknown file create mode (" << create_mode
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	if (access_type == OS_FILE_READ_ONLY) {
		access = GENERIC_READ;
	} else if (read_only_mode) {
		access = GENERIC_READ;
	} else if (access_type == OS_FILE_READ_WRITE) {
		access = GENERIC_READ | GENERIC_WRITE;
	} else if (access_type == OS_FILE_READ_ALLOW_DELETE) {

		ut_a(!read_only_mode);

		access = GENERIC_READ;

		/*!< A backup program has to give mysqld the maximum
		freedom to do what it likes with the file */

		share_mode |= FILE_SHARE_DELETE | FILE_SHARE_WRITE;
	} else {
		ib::error() << "Unknown file access type (" << access_type
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	file = CreateFile((LPCTSTR) name,
			  access,
			  share_mode,
			  NULL,			// Security attributes
			  create_flag,
			  attributes,
			  NULL);		// No template file

	*success = (file != INVALID_HANDLE_VALUE);
#else /* _WIN32 */
	int		create_flag;

	ut_a(name);

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		if (access_type == OS_FILE_READ_ONLY) {

			create_flag = O_RDONLY;

		} else if (read_only_mode) {

			create_flag = O_RDONLY;

		} else {

			ut_a(access_type == OS_FILE_READ_WRITE
			     || access_type == OS_FILE_READ_ALLOW_DELETE);

			create_flag = O_RDWR;
		}

	} else if (read_only_mode) {

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else {
		ib::error() << "Unknown file create mode (" << create_mode
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	file = ::open(name, create_flag, os_innodb_umask);

	*success = (file != -1);

#ifdef USE_FILE_LOCK
	if (!read_only_mode
	    && *success
	    && access_type == OS_FILE_READ_WRITE
	    && os_file_lock(file, name)) {

		*success = false;
		close(file);
		file = -1;

	}
#endif /* USE_FILE_LOCK */

#endif /* _WIN32 */

	return(file);
}

/****************************************************************//**
Tries to disable OS caching on an opened file descriptor. */
void
os_file_set_nocache(
/*================*/
	int		fd		/*!< in: file descriptor to alter */
					__attribute__((unused)),
	const char*	file_name	/*!< in: used in the diagnostic
					message */
					__attribute__((unused)),
	const char*	operation_name __attribute__((unused)))
					/*!< in: "open" or "create"; used
					in the diagnostic message */
{
	/* some versions of Solaris may not have DIRECTIO_ON */
#if defined(UNIV_SOLARIS) && defined(DIRECTIO_ON)
	if (directio(fd, DIRECTIO_ON) == -1) {
		int	errno_save = errno;

		ib::error() << "Failed to set DIRECTIO_ON on file "
			<< file_name << ": " << operation_name << ": "
			<< strerror(errno_save) << ", continuing anyway.";
	}
#elif defined(O_DIRECT)
	if (fcntl(fd, F_SETFL, O_DIRECT) == -1) {
		int		errno_save = errno;
		static bool	warning_message_printed = false;
		if (errno_save == EINVAL) {
			if (!warning_message_printed) {
				warning_message_printed = true;
# ifdef UNIV_LINUX
				ib::warn() << "Failed to set O_DIRECT on file "
					<< file_name << ": " << operation_name
					<< ": " << strerror(errno_save) << ","
					" continuing anyway. O_DIRECT is known"
					" to result in 'Invalid argument' on"
					" Linux on tmpfs, see MySQL Bug#26662.";
# else /* UNIV_LINUX */
				goto short_warning;
# endif /* UNIV_LINUX */
			}
		} else {
# ifndef UNIV_LINUX
short_warning:
# endif
			ib::warn() << "Failed to set O_DIRECT on file "
				<< file_name << ": " << operation_name
				<< ": " << strerror(errno_save) << ","
				" continuing anyway.";
		}
	}
#endif /* defined(UNIV_SOLARIS) && defined(DIRECTIO_ON) */
}

/****************************************************************//**
NOTE! Use the corresponding macro os_file_create(), not directly
this function!
Opens an existing file or creates a new.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
os_file_t
os_file_create_func(
/*================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: create mode */
	ulint		purpose,/*!< in: OS_FILE_AIO, if asynchronous,
				non-buffered i/o is desired,
				OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use
				async i/o or unbuffered i/o: look in the
				function source code for the exact rules */
	ulint		type,	/*!< in: OS_DATA_FILE or OS_LOG_FILE */
	bool		read_only_mode,
				/*!< in: if true, read only mode
				checks are enforced. */
	bool*		success)/*!< out: true if succeed, false if error */
{
	os_file_t	file;
	bool		retry;
	bool		on_error_no_exit;
	bool		on_error_silent;

	*success = false;

#ifdef _WIN32
	DBUG_EXECUTE_IF(
		"ib_create_table_fail_disk_full",
		*success = false;
		SetLastError(ERROR_DISK_FULL);
		return(OS_FILE_CLOSED);
	);
#else /* _WIN32 */
	DBUG_EXECUTE_IF(
		"ib_create_table_fail_disk_full",
		*success = false;
		errno = ENOSPC;
		return(OS_FILE_CLOSED);
	);
#endif /* _WIN32 */

#ifdef _WIN32
	DWORD		create_flag;
	DWORD		share_mode	= FILE_SHARE_READ;

	on_error_no_exit = create_mode & OS_FILE_ON_ERROR_NO_EXIT
		? true : false;

	on_error_silent = create_mode & OS_FILE_ON_ERROR_SILENT
		? true : false;

	create_mode &= ~OS_FILE_ON_ERROR_NO_EXIT;
	create_mode &= ~OS_FILE_ON_ERROR_SILENT;

	if (create_mode == OS_FILE_OPEN_RAW) {

		ut_a(!read_only_mode);

		create_flag = OPEN_EXISTING;

		/* On Windows Physical devices require admin privileges and
		have to have the write-share mode set. See the remarks
		section for the CreateFile() function documentation in MSDN. */

		share_mode |= FILE_SHARE_WRITE;

	} else if (create_mode == OS_FILE_OPEN
		   || create_mode == OS_FILE_OPEN_RETRY) {

		create_flag = OPEN_EXISTING;

	} else if (read_only_mode) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else if (create_mode == OS_FILE_OVERWRITE) {

		create_flag = CREATE_ALWAYS;

	} else {
		ib::error() << "Unknown file create mode (" << create_mode
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	DWORD		attributes = 0;

#ifdef UNIV_HOTBACKUP
	attributes |= FILE_FLAG_NO_BUFFERING;
#else
	if (purpose == OS_FILE_AIO) {

#ifdef WIN_ASYNC_IO
		/* If specified, use asynchronous (overlapped) io and no
		buffering of writes in the OS */

		if (srv_use_native_aio) {
			attributes |= FILE_FLAG_OVERLAPPED;
		}
#endif /* WIN_ASYNC_IO */

	} else if (purpose == OS_FILE_NORMAL) {
		/* Use default setting. */
	} else {
		ib::error() << "Unknown purpose flag (" << purpose
			<< ") while opening file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

#ifdef UNIV_NON_BUFFERED_IO
	// TODO: Create a bug, this looks wrong. The flush log
	// parameter is dynamic.
	if (type == OS_LOG_FILE && srv_flush_log_at_trx_commit == 2) {

		/* Do not use unbuffered i/o for the log files because
		value 2 denotes that we do not flush the log at every
		commit, but only once per second */

	} else if (srv_win_file_flush_method == SRV_WIN_IO_UNBUFFERED) {

		attributes |= FILE_FLAG_NO_BUFFERING;
	}
#endif /* UNIV_NON_BUFFERED_IO */

#endif /* UNIV_HOTBACKUP */
	DWORD	access = GENERIC_READ;

	if (!read_only_mode) {
		access |= GENERIC_WRITE;
	}

	do {
		/* Use default security attributes and no template file. */
		file = CreateFile(
			(LPCTSTR) name, access, share_mode, NULL,
			create_flag, attributes, NULL);

		if (file == INVALID_HANDLE_VALUE) {
			const char*	operation;

			operation = (create_mode == OS_FILE_CREATE
				     && !read_only_mode)
				? "create" : "open";

			*success = false;

			if (on_error_no_exit) {
				retry = os_file_handle_error_no_exit(
					name, operation, on_error_silent);
			} else {
				retry = os_file_handle_error(name, operation);
			}
		} else {
			*success = true;
			retry = false;
		}

	} while (retry);

#else /* _WIN32 */
	int		create_flag;
	const char*	mode_str	= NULL;

	on_error_no_exit = create_mode & OS_FILE_ON_ERROR_NO_EXIT
		? true : false;
	on_error_silent = create_mode & OS_FILE_ON_ERROR_SILENT
		? true : false;

	create_mode &= ~OS_FILE_ON_ERROR_NO_EXIT;
	create_mode &= ~OS_FILE_ON_ERROR_SILENT;

	if (create_mode == OS_FILE_OPEN
	    || create_mode == OS_FILE_OPEN_RAW
	    || create_mode == OS_FILE_OPEN_RETRY) {

		mode_str = "OPEN";

		create_flag = read_only_mode ? O_RDONLY : O_RDWR;

	} else if (read_only_mode) {

		mode_str = "OPEN";

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		mode_str = "CREATE";
		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else if (create_mode == OS_FILE_OVERWRITE) {

		mode_str = "OVERWRITE";
		create_flag = O_RDWR | O_CREAT | O_TRUNC;

	} else {
		ib::error() << "Unknown file create mode (" << create_mode
			<< ") for file '" << name << "'";

		return(OS_FILE_CLOSED);
	}

	ut_a(type == OS_LOG_FILE
	     || type == OS_DATA_FILE
	     || type == OS_DATA_TEMP_FILE);
	ut_a(purpose == OS_FILE_AIO || purpose == OS_FILE_NORMAL);

#ifdef O_SYNC
	/* We let O_SYNC only affect log files; note that we map O_DSYNC to
	O_SYNC because the datasync options seemed to corrupt files in 2001
	in both Linux and Solaris */

	if (!read_only_mode
	    && type == OS_LOG_FILE
	    && srv_unix_file_flush_method == SRV_UNIX_O_DSYNC) {

		create_flag |= O_SYNC;
	}
#endif /* O_SYNC */

	do {
		file = ::open(name, create_flag, os_innodb_umask);

		if (file == -1) {
			const char*	operation;

			operation = (create_mode == OS_FILE_CREATE
				     && !read_only_mode)
				? "create" : "open";

			*success = false;

			if (on_error_no_exit) {
				retry = os_file_handle_error_no_exit(
					name, operation, on_error_silent);
			} else {
				retry = os_file_handle_error(name, operation);
			}
		} else {
			*success = true;
			retry = false;
		}

	} while (retry);

	/* We disable OS caching (O_DIRECT) only on data files */

	if (!read_only_mode
	    && *success
	    && (type != OS_LOG_FILE && type != OS_DATA_TEMP_FILE)
	    && (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT
		|| srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC)) {

		os_file_set_nocache(file, name, mode_str);
	}

#ifdef USE_FILE_LOCK
	if (!read_only_mode
	    && *success
	    && create_mode != OS_FILE_OPEN_RAW
	    && os_file_lock(file, name)) {

		if (create_mode == OS_FILE_OPEN_RETRY) {

			ut_a(!read_only_mode);

			ib::info() << "Retrying to lock the first data file";

			for (int i = 0; i < DBUG_EVALUATE_IF("innodb_lock_no_retry", 0, 100);
					 i++) {
				os_thread_sleep(1000000);

				if (!os_file_lock(file, name)) {
					*success = true;
					return(file);
				}
			}

			ib::info() << "Unable to open the first data file";
		}

		*success = false;
		close(file);
		file = -1;
	}
#endif /* USE_FILE_LOCK */

#endif /* _WIN32 */

	return(file);
}

/***********************************************************************//**
Deletes a file if it exists. The file has to be closed before calling this.
@return true if success */
bool
os_file_delete_if_exists_func(
/*==========================*/
	const char*	name,	/*!< in: file path as a null-terminated
				string */
	bool*		exist)	/*!< out: indicate if file pre-exist.
				If not-NULL, set to true if pre-exist
				or false if doesn't pre-exist.
				If NULL, then ignore setting this value. */
{
#ifdef _WIN32
	bool	ret;
	ulint	count	= 0;
	if (exist) {
		*exist = true;
	}
loop:
	/* In Windows, deleting an .ibd file may fail if mysqlbackup is copying
	it */

	ret = DeleteFile((LPCTSTR) name);

	if (ret) {
		return(true);
	}

	DWORD lasterr = GetLastError();
	if (lasterr == ERROR_FILE_NOT_FOUND
	    || lasterr == ERROR_PATH_NOT_FOUND) {
		/* the file does not exist, this not an error */
		if (exist) {
			*exist = false;
		}
		return(true);
	}

	count++;

	if (count > 100 && 0 == (count % 10)) {
		os_file_get_last_error(true); /* print error information */

		ib::warn() <<  "Delete of file " << name << " failed.";
	}

	os_thread_sleep(500000);	/* sleep for 0.5 second */

	if (count > 2000) {

		return(false);
	}

	goto loop;
#else
	int	ret;
	if (exist) {
		*exist = true;
	}

	ret = unlink(name);

	if (ret != 0 && errno == ENOENT) {
		if (exist) {
			*exist = false;
		}
	} else if (ret != 0 && errno != ENOENT) {
		os_file_handle_error_no_exit(name, "delete", false);

		return(false);
	}

	return(true);
#endif /* _WIN32 */
}

/***********************************************************************//**
Deletes a file. The file has to be closed before calling this.
@return true if success */
bool
os_file_delete_func(
/*================*/
	const char*	name)	/*!< in: file path as a null-terminated
				string */
{
#ifdef _WIN32
	BOOL	ret;
	ulint	count	= 0;
loop:
	/* In Windows, deleting an .ibd file may fail if mysqlbackup is copying
	it */

	ret = DeleteFile((LPCTSTR) name);

	if (ret) {
		return(true);
	}

	if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		/* If the file does not exist, we classify this as a 'mild'
		error and return */

		return(false);
	}

	count++;

	if (count > 100 && 0 == (count % 10)) {
		os_file_get_last_error(true); /* print error information */

		ib::warn() << "Cannot delete file " << name << ". Are you"
			" running mysqlbackup to back up the file?";
	}

	os_thread_sleep(1000000);	/* sleep for a second */

	if (count > 2000) {

		return(false);
	}

	goto loop;
#else
	int	ret;

	ret = unlink(name);

	if (ret != 0) {
		os_file_handle_error_no_exit(name, "delete", false);

		return(false);
	}

	return(true);
#endif
}

/***********************************************************************//**
NOTE! Use the corresponding macro os_file_rename(), not directly this function!
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return true if success */
bool
os_file_rename_func(
/*================*/
	const char*	oldpath,/*!< in: old file path as a null-terminated
				string */
	const char*	newpath)/*!< in: new file path */
{
#ifdef UNIV_DEBUG
	os_file_type_t	type;
	bool		exists;

	/* New path must not exist. */
	ut_ad(os_file_status(newpath, &exists, &type));
	ut_ad(!exists);

	/* Old path must exist. */
	ut_ad(os_file_status(oldpath, &exists, &type));
	ut_ad(exists);
#endif /* UNIV_DEBUG */

#ifdef _WIN32
	BOOL	ret;

	ret = MoveFile((LPCTSTR) oldpath, (LPCTSTR) newpath);

	if (ret) {
		return(true);
	}

	os_file_handle_error_no_exit(oldpath, "rename", false);

	return(false);
#else
	int	ret;

	ret = rename(oldpath, newpath);

	if (ret != 0) {
		os_file_handle_error_no_exit(oldpath, "rename", false);

		return(false);
	}

	return(true);
#endif /* _WIN32 */
}

/***********************************************************************//**
NOTE! Use the corresponding macro os_file_close(), not directly this function!
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@return true if success */
bool
os_file_close_func(
/*===============*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef _WIN32
	BOOL	ret;

	ut_a(file);

	ret = CloseHandle(file);

	if (ret) {
		return(true);
	}

	os_file_handle_error(NULL, "close");

	return(false);
#else
	int	ret;

	ret = close(file);

	if (ret == -1) {
		os_file_handle_error(NULL, "close");

		return(false);
	}

	return(true);
#endif /* _WIN32 */
}

#ifdef UNIV_HOTBACKUP
/***********************************************************************//**
Closes a file handle.
@return true if success */
bool
os_file_close_no_error_handling(
/*============================*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef _WIN32
	BOOL	ret;

	ut_a(file);

	ret = CloseHandle(file);

	if (ret) {
		return(true);
	}

	return(false);
#else
	int	ret;

	ret = close(file);

	if (ret == -1) {

		return(false);
	}

	return(true);
#endif /* _WIN32 */
}
#endif /* UNIV_HOTBACKUP */

/***********************************************************************//**
Gets a file size.
@return file size, or (os_offset_t) -1 on failure */
os_offset_t
os_file_get_size(
/*=============*/
	os_file_t	file)	/*!< in: handle to a file */
{
#ifdef _WIN32
	DWORD		high;
	DWORD		low = GetFileSize(file, &high);

	if (low == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
		return((os_offset_t) -1);
	}

	return(os_offset_t(low | (os_offset_t(high) << 32)));
#else
	/* Store current position */
	os_offset_t	pos = lseek(file, 0, SEEK_CUR);
	os_offset_t	file_size = lseek(file, 0, SEEK_END);

	/* Restore current position as the function should not change it */
	lseek(file, pos, SEEK_SET);

	return(file_size);
#endif /* _WIN32 */
}

/***********************************************************************//**
Write the specified number of zeros to a newly created file.
@return true if success */
bool
os_file_set_size(
/*=============*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	os_offset_t	size,	/*!< in: file size */
	bool		read_only_mode)
				/*!< in: if true, read only mode
				checks are enforced. */
{
	os_offset_t	current_size;
	bool		ret;
	byte*		buf;
	byte*		buf2;
	ulint		buf_size;

	current_size = 0;

	/* Write up to 1 megabyte at a time. */
	buf_size = ut_min(static_cast<ulint>(64),
			  static_cast<ulint>(size / UNIV_PAGE_SIZE))
		* UNIV_PAGE_SIZE;
	buf2 = static_cast<byte*>(ut_malloc_nokey(buf_size + UNIV_PAGE_SIZE));

	/* Align the buffer for possible raw i/o */
	buf = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

	/* Write buffer full of zeros */
	memset(buf, 0, buf_size);

	if (size >= (os_offset_t) 100 << 20) {

		ib::info() << "Progress in MB:";
	}

	while (current_size < size) {
		ulint	n_bytes;

		if (size - current_size < (os_offset_t) buf_size) {
			n_bytes = (ulint) (size - current_size);
		} else {
			n_bytes = buf_size;
		}

#ifdef UNIV_HOTBACKUP
		ret = os_file_write(name, file, buf, current_size, n_bytes);
#else
		/* Using OS_AIO_SYNC mode on *unix will result in fall back
		to os_file_write/read for Windows there it will use special
		mechanism to wait before it returns back. */
		ret = os_aio(
			OS_FILE_WRITE, OS_AIO_SYNC, name, file, buf,
			current_size, n_bytes, read_only_mode, NULL, NULL);
#endif /* UNIV_HOTBACKUP */
		if (!ret) {
			ut_free(buf2);
			goto error_handling;
		}

		/* Print about progress for each 100 MB written */
		if ((current_size + n_bytes) / (100 << 20)
		    != current_size / (100 << 20)) {

			fprintf(stderr, " %lu00",
				(ulong) ((current_size + n_bytes)
					 / (100 << 20)));
		}

		current_size += n_bytes;
	}

	if (size >= (os_offset_t) 100 << 20) {

		fprintf(stderr, "\n");
	}

	ut_free(buf2);

	ret = os_file_flush(file);

	if (ret) {
		return(true);
	}

error_handling:
	return(false);
}

/***********************************************************************//**
Truncates a file at its current position.
@return true if success */
bool
os_file_set_eof(
/*============*/
	FILE*		file)	/*!< in: file to be truncated */
{
#ifdef _WIN32
	HANDLE h = (HANDLE) _get_osfhandle(fileno(file));
	return(SetEndOfFile(h));
#else /* _WIN32 */
	return(!ftruncate(fileno(file), ftell(file)));
#endif /* _WIN32 */
}

/** Truncates a file to a specified size in bytes.
Do nothing if the size to preserve is greater or equal to the current
size of the file.
@param[in]	pathname	file path
@param[in]	file		file to be truncated
@param[in]	size		size to preserve in bytes
@return true if success */
bool
os_file_truncate(
	const char*     pathname,
	os_file_t       file,
	os_offset_t	size)
{
	/* Do nothing if the size preserved is larger than or equal to the
	current size of file */
	os_offset_t	size_bytes = os_file_get_size(file);
	if (size >= size_bytes) {
		return(true);
	}

#ifdef _WIN32
	LARGE_INTEGER    length;
	length.QuadPart = size;

	BOOL	success = SetFilePointerEx(file, length, NULL, FILE_BEGIN);
	if (!success) {
		os_file_handle_error_no_exit(
			pathname, "SetFilePointerEx", false);
	} else {
		success = SetEndOfFile(file);
		if (!success) {
			os_file_handle_error_no_exit(
				pathname, "SetEndOfFile", false);
		}
	}
	return(success);
#else /* _WIN32 */
	int	res = ftruncate(file, size);
	if (res == -1) {
		os_file_handle_error_no_exit(pathname, "truncate", false);
	}

	return(res == 0);
#endif /* _WIN32 */
}

#ifndef _WIN32
/***********************************************************************//**
Wrapper to fsync(2) that retries the call on some errors.
Returns the value 0 if successful; otherwise the value -1 is returned and
the global variable errno is set to indicate the error.
@return 0 if success, -1 otherwise */

static
int
os_file_fsync(
/*==========*/
	os_file_t	file)	/*!< in: handle to a file */
{
	int	ret;
	int	failures;
	bool	retry;

	failures = 0;

	do {
		ret = fsync(file);

		os_n_fsyncs++;

		if (ret == -1 && errno == ENOLCK) {

			if (failures % 100 == 0) {

				ib::warn() << "fsync(): No locks available;"
					" retrying";
			}

			os_thread_sleep(200000 /* 0.2 sec */);

			failures++;

			retry = true;
		} else {

			retry = false;
		}
	} while (retry);

	return(ret);
}
#endif /* !_WIN32 */

/***********************************************************************//**
NOTE! Use the corresponding macro os_file_flush(), not directly this function!
Flushes the write buffers of a given file to the disk.
@return true if success */
bool
os_file_flush_func(
/*===============*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef _WIN32
	BOOL	ret;

	ut_a(file);

	os_n_fsyncs++;

	ret = FlushFileBuffers(file);

	if (ret) {
		return(true);
	}

	/* Since Windows returns ERROR_INVALID_FUNCTION if the 'file' is
	actually a raw device, we choose to ignore that error if we are using
	raw disks */

	if (srv_start_raw_disk_in_use && GetLastError()
	    == ERROR_INVALID_FUNCTION) {
		return(true);
	}

	os_file_handle_error(NULL, "flush");

	/* It is a fatal error if a file flush does not succeed, because then
	the database can get corrupt on disk */
	ut_error;

	return(false);
#else
	int	ret;

	ret = os_file_fsync(file);

	if (ret == 0) {
		return(true);
	}

	/* Since Linux returns EINVAL if the 'file' is actually a raw device,
	we choose to ignore that error if we are using raw disks */

	if (srv_start_raw_disk_in_use && errno == EINVAL) {

		return(true);
	}

	ib::error() << "The OS said file flush did not succeed";

	os_file_handle_error(NULL, "flush");

	/* It is a fatal error if a file flush does not succeed, because then
	the database can get corrupt on disk */
	ut_error;

	return(false);
#endif /* _WIN32 */
}

#ifndef _WIN32
/*******************************************************************//**
Does a syncronous read or write depending upon the type specified
In case of partial reads/writes the function tries
NUM_RETRIES_ON_PARTIAL_IO times to read/write the complete data.
@return number of bytes read/written, -1 if error */
static __attribute__((warn_unused_result))
ssize_t
os_file_io(
/*==========*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read/write */
	ulint		n,	/*!< in: number of bytes to read/write */
	off_t		offset,	/*!< in: file offset from where to read/write */
	ulint		type)   /*!< in: type for read or write */
{
	ssize_t bytes_returned = 0;
	ssize_t n_bytes;
	ulint i;

	for (i = 0; i < NUM_RETRIES_ON_PARTIAL_IO; ++i) {
		if (type == OS_FILE_READ ) {
			n_bytes = pread(file, buf, n, offset);
		} else {
			ut_ad(type == OS_FILE_WRITE);
			n_bytes = pwrite(file, buf, n, offset);
		}

		if ((ulint) n_bytes == n) {
			bytes_returned += n_bytes;
			return(bytes_returned);
		} else if (n_bytes > 0 && (ulint) n_bytes < n) {
			/* For partial read/write scenario */
			if (type == OS_FILE_READ) {
				ib::warn() << n << " bytes should have been"
					" read. Only " << n_bytes << " bytes"
					" read. Retrying again to read"
					" the remaining bytes.";
			} else {
				ib::warn() << n << " bytes should have been"
					" written. Only " << n_bytes
					<< " bytes written. Retrying again to"
					" write the remaining bytes.";
			}

			buf = (uchar*) buf + (ulint) n_bytes;
			n -=  (ulint) n_bytes;
			offset += n_bytes;
			bytes_returned += (ulint) n_bytes;

		} else {
			/* System call failure */
			if (type == OS_FILE_READ) {
				ib::error() << "Error in system call pread()."
				" The operating system error number is"
				" " << errno << ".";
			} else {
				ib::error() << "Error in system call pwrite()."
				" The operating system error number is"
				" " << errno << ".";
			}

			if (strerror(errno) != NULL) {
				ib::error() << "Error number "
				<< errno << " means '" <<
				strerror(errno) << "'.";
			}

			ib::info() << OPERATING_SYSTEM_ERROR_MSG;
			break;
		}
	}

	if (i > 0) {
		/* Print the warning only if retrying was attempted */
		ib::warn() << "Retry attempts for "
			<< (type == OS_FILE_READ ? "reading" : "writing")
			<< " partial data failed.";
	}

	return(bytes_returned);
}

/*******************************************************************//**
Does a synchronous read operation in Posix.
@return number of bytes read, -1 if error */
static __attribute__((warn_unused_result))
ssize_t
os_file_pread(
/*==========*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	ulint		n,	/*!< in: number of bytes to read */
	os_offset_t	offset)	/*!< in: file offset from where to read */
{
	off_t		offs;
	ssize_t		read_bytes;

	ut_ad(n);

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */
	offs = (off_t) offset;

	if (sizeof(off_t) <= 4 && offset != (os_offset_t) offs) {
		ib::error() << "File read at offset > 4 GB";
	}

	os_n_file_reads++;

	(void) os_atomic_increment_ulint(&os_n_pending_reads, 1);
	(void) os_atomic_increment_ulint(&os_file_n_pending_preads, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_READS);

	read_bytes = os_file_io(file, buf, n, offs, OS_FILE_READ);

	(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
	(void) os_atomic_decrement_ulint(&os_file_n_pending_preads, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);

	return(read_bytes);
}

/*******************************************************************//**
Does a synchronous write operation in Posix.
@return number of bytes written, -1 if error */
static __attribute__((warn_unused_result))
ssize_t
os_file_pwrite(
/*===========*/
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from where to write */
	ulint		n,	/*!< in: number of bytes to write */
	os_offset_t	offset)	/*!< in: file offset where to write */
{
	off_t		offs;
	ssize_t		written_bytes;

	ut_ad(n);

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */
	offs = (off_t) offset;

	if (sizeof(off_t) <= 4 && offset != (os_offset_t) offs) {
		ib::error() << "file write at offset > 4 GB.";
	}

	os_n_file_writes++;

	(void) os_atomic_increment_ulint(&os_n_pending_writes, 1);
	(void) os_atomic_increment_ulint(&os_file_n_pending_pwrites, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_WRITES);

	written_bytes = os_file_io(
		file, (void*) buf, n, offs, OS_FILE_WRITE);

	(void) os_atomic_decrement_ulint(&os_n_pending_writes, 1);
	(void) os_atomic_decrement_ulint(&os_file_n_pending_pwrites, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_WRITES);

	return(written_bytes);
}

# endif /* _WIN32*/

/*******************************************************************//**
NOTE! Use the corresponding macro os_file_read(), not directly this
function!
Requests a synchronous positioned read operation.
@return true if request was successful, false if fail */
bool
os_file_read_func(
/*==============*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n)	/*!< in: number of bytes to read */
{
#ifdef _WIN32
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		low;
	DWORD		high;
	bool		retry;
#ifndef UNIV_HOTBACKUP
	ulint		i;
#endif /* !UNIV_HOTBACKUP */

	/* On 64-bit Windows, ulint is 64 bits. But offset and n should be
	no more than 32 bits. */
	ut_a((n & 0xFFFFFFFFUL) == n);

	os_n_file_reads++;
	os_bytes_read_since_printout += n;

try_again:
	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);

	low = (DWORD) offset & 0xFFFFFFFF;
	high = (DWORD) (offset >> 32);

	(void) os_atomic_increment_ulint(&os_n_pending_reads, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_READS);

#ifndef UNIV_HOTBACKUP
	/* Protect the seek / read operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

	mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	ret2 = SetFilePointer(
		file, low, reinterpret_cast<PLONG>(&high), FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {

#ifndef UNIV_HOTBACKUP
		mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
		MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);

		goto error_handling;
	}

	ret = ReadFile(file, buf, (DWORD) n, &len, NULL);

#ifndef UNIV_HOTBACKUP
	mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);

	if (ret && len == n) {
		return(true);
	}
#else /* _WIN32 */
	bool	retry;
	ssize_t	ret;

	os_bytes_read_since_printout += n;

try_again:
	ret = os_file_pread(file, buf, n, offset);

	if ((ulint) ret == n) {
		return(true);
	}

	ib::error() << "Tried to read " << n << " bytes at offset " << offset
		<< " was only able to read " << ret << ".";

#endif /* _WIN32 */
#ifdef _WIN32
error_handling:
#endif
	retry = os_file_handle_error(NULL, "read");

	if (retry) {
#ifndef _WIN32
		if (ret > 0 && (ulint) ret < n) {
			buf = (uchar*) buf + (ulint) ret;
			offset += (ulint) ret;
			n -= (ulint) ret;
		}
#endif
		goto try_again;
	}

	ib::fatal() << "Cannot read from file. OS error number "
#ifdef _WIN32
		<< GetLastError()
#else
		<< errno
#endif
		<< ".";

	return(false);
}

/*******************************************************************//**
NOTE! Use the corresponding macro os_file_read_no_error_handling(),
not directly this function!
Requests a synchronous positioned read operation. This function does not do
any error handling. In case of error it returns false.
@return true if request was successful, false if fail */
bool
os_file_read_no_error_handling_func(
/*================================*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n)	/*!< in: number of bytes to read */
{
#ifdef _WIN32
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		low;
	DWORD		high;
	bool		retry;
#ifndef UNIV_HOTBACKUP
	ulint		i;
#endif /* !UNIV_HOTBACKUP */

	/* On 64-bit Windows, ulint is 64 bits. But offset and n should be
	no more than 32 bits. */
	ut_a((n & 0xFFFFFFFFUL) == n);

	os_n_file_reads++;
	os_bytes_read_since_printout += n;

try_again:
	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);

	low = (DWORD) offset & 0xFFFFFFFF;
	high = (DWORD) (offset >> 32);

	(void) os_atomic_increment_ulint(&os_n_pending_reads, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_READS);

#ifndef UNIV_HOTBACKUP
	/* Protect the seek / read operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

	mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	ret2 = SetFilePointer(
		file, low, reinterpret_cast<PLONG>(&high), FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {

#ifndef UNIV_HOTBACKUP
		mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
		MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);

		goto error_handling;
	}

	ret = ReadFile(file, buf, (DWORD) n, &len, NULL);

#ifndef UNIV_HOTBACKUP
	mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);

	if (ret && len == n) {
		return(true);
	}
#else /* _WIN32 */
	bool	retry;
	ssize_t	ret;

	os_bytes_read_since_printout += n;

try_again:
	ret = os_file_pread(file, buf, n, offset);

	if ((ulint) ret == n) {

		return(true);
	}

#endif /* _WIN32 */
#ifdef _WIN32
error_handling:
#endif
	retry = os_file_handle_error_no_exit(NULL, "read", false);

	if (retry) {
#ifndef _WIN32
		if (ret > 0 && (ulint) ret < n) {
			buf = (uchar*) buf + (ulint) ret;
			offset += ret;
			n -= (ulint) ret;
		}
#endif /* _WIN32 */
		goto try_again;
	}

	return(false);
}

/*******************************************************************//**
Rewind file to its start, read at most size - 1 bytes from it to str, and
NUL-terminate str. All errors are silently ignored. This function is
mostly meant to be used with temporary files. */
void
os_file_read_string(
/*================*/
	FILE*	file,	/*!< in: file to read from */
	char*	str,	/*!< in: buffer where to read */
	ulint	size)	/*!< in: size of buffer */
{
	size_t	flen;

	if (size == 0) {
		return;
	}

	rewind(file);
	flen = fread(str, 1, size - 1, file);
	str[flen] = '\0';
}

/*******************************************************************//**
NOTE! Use the corresponding macro os_file_write(), not directly
this function!
Requests a synchronous write operation.
@return true if request was successful, false if fail */
bool
os_file_write_func(
/*===============*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from which to write */
	os_offset_t	offset,	/*!< in: file offset where to write */
	ulint		n)	/*!< in: number of bytes to write */
{
#ifdef _WIN32
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		low;
	DWORD		high;
	ulint		n_retries	= 0;
	ulint		err;
#ifndef UNIV_HOTBACKUP
	ulint		i;
#endif /* !UNIV_HOTBACKUP */

	/* On 64-bit Windows, ulint is 64 bits. But offset and n should be
	no more than 32 bits. */
	ut_a((n & 0xFFFFFFFFUL) == n);

	os_n_file_writes++;

	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);
retry:
	low = (DWORD) offset & 0xFFFFFFFF;
	high = (DWORD) (offset >> 32);

	(void) os_atomic_increment_ulint(&os_n_pending_writes, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_WRITES);

#ifndef UNIV_HOTBACKUP
	/* Protect the seek / write operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

	mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	ret2 = SetFilePointer(
		file, low, reinterpret_cast<PLONG>(&high), FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {

#ifndef UNIV_HOTBACKUP
		mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		(void) os_atomic_decrement_ulint(&os_n_pending_writes, 1);
		MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_WRITES);

		ib::error() << "File pointer positioning to file " << name
			<< " failed at offset " << offset << ". Operating"
			" system error number " << GetLastError() << ". "
			<< OPERATING_SYSTEM_ERROR_MSG;
		return(false);
	}

	ret = WriteFile(file, buf, (DWORD) n, &len, NULL);

#ifndef UNIV_HOTBACKUP
	mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	(void) os_atomic_decrement_ulint(&os_n_pending_writes, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_WRITES);

	if (ret && len == n) {

		return(true);
	}

	/* If some background file system backup tool is running, then, at
	least in Windows 2000, we may get here a specific error. Let us
	retry the operation 100 times, with 1 second waits. */

	if (GetLastError() == ERROR_LOCK_VIOLATION && n_retries < 100) {

		os_thread_sleep(1000000);

		n_retries++;

		goto retry;
	}

	if (!os_has_said_disk_full) {

		err = (ulint) GetLastError();

		ib::error() << "Write to file " << name << " failed at offset "
			<< offset << ". " << n << " bytes should have been"
			" written, only " << len << " were written."
			" Operating system error number " << err << "."
			" Check that your OS and file system support files of"
			" this size. Check also that the disk is not full"
			" or a disk quota exceeded.";

		if (strerror((int) err) != NULL) {
			ib::error() << "Error number " << err << " means '"
				<< strerror((int) err) << "'.";
		}

		ib::info() << OPERATING_SYSTEM_ERROR_MSG;

		os_has_said_disk_full = true;
	}

	return(false);
#else
	ssize_t	ret;

	ret = os_file_pwrite(file, buf, n, offset);

	if ((ulint) ret == n) {
		return(true);
	}

	if (!os_has_said_disk_full) {

		ib::error() << "Write to file " << name << " failed at offset "
			<< offset << ". " << n << " bytes should have been"
			" written, only " << ret << " were written. Check that"
			" your OS and file system support files of  this size."
			" Check also that the disk is not full or a disk quota"
			"  exceeded.";

		os_has_said_disk_full = true;
	}

	return(false);
#endif
}

/*******************************************************************//**
Check the existence and type of the given file.
@return true if call succeeded */
bool
os_file_status(
/*===========*/
	const char*	path,	/*!< in: pathname of the file */
	bool*		exists,	/*!< out: true if file exists */
	os_file_type_t* type)	/*!< out: type of the file (if it exists) */
{
#ifdef _WIN32
	int		ret;
	struct _stat64	statinfo;

	ret = _stat64(path, &statinfo);

	*exists = !ret;

	if (!ret) {
		/* file exists, everything OK */

	} else if (errno == ENOENT || errno == ENOTDIR) {
		/* file does not exist */
		return(true);

	} else {
		/* file exists, but stat call failed */
		os_file_handle_error_no_exit(path, "stat", false);
		return(false);
	}

	if (_S_IFDIR & statinfo.st_mode) {
		*type = OS_FILE_TYPE_DIR;

	} else if (_S_IFREG & statinfo.st_mode) {
		*type = OS_FILE_TYPE_FILE;

	} else {
		*type = OS_FILE_TYPE_UNKNOWN;
	}

	return(true);
#else
	int		ret;
	struct stat	statinfo;

	ret = stat(path, &statinfo);

	*exists = !ret;

	if (!ret) {
		/* file exists, everything OK */

	} else if (errno == ENOENT || errno == ENOTDIR) {
		/* file does not exist */
		return(true);

	} else {
		/* file exists, but stat call failed */
		os_file_handle_error_no_exit(path, "stat", false);
		return(false);
	}

	if (S_ISDIR(statinfo.st_mode)) {
		*type = OS_FILE_TYPE_DIR;

	} else if (S_ISLNK(statinfo.st_mode)) {
		*type = OS_FILE_TYPE_LINK;

	} else if (S_ISREG(statinfo.st_mode)) {
		*type = OS_FILE_TYPE_FILE;

	} else {
		*type = OS_FILE_TYPE_UNKNOWN;
	}

	return(true);
#endif
}

/*******************************************************************//**
This function returns information about the specified file
@return DB_SUCCESS if all OK */
dberr_t
os_file_get_status(
/*===============*/
	const char*	path,		/*!< in:	pathname of the file */
	os_file_stat_t* stat_info,	/*!< information of a file in a
					directory */
	bool		check_rw_perm,	/*!< in: for testing whether the
					file can be opened in RW mode */
	bool		read_only_mode)	/*!< in: if true, read only mode
					checks are enforced. */
{
	int		ret;

#ifdef _WIN32
	struct _stat64	statinfo;

	ret = _stat64(path, &statinfo);

	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(DB_NOT_FOUND);

	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", false);

		return(DB_FAIL);

	} else if (_S_IFDIR & statinfo.st_mode) {
		stat_info->type = OS_FILE_TYPE_DIR;
	} else if (_S_IFREG & statinfo.st_mode) {

		DWORD	access = GENERIC_READ;

		if (!read_only_mode) {
			access |= GENERIC_WRITE;
		}

		stat_info->type = OS_FILE_TYPE_FILE;

		/* Check if we can open it in read-only mode. */

		if (check_rw_perm) {
			HANDLE	fh;

			fh = CreateFile(
				(LPCTSTR) path,		// File to open
				access,
				0,			// No sharing
				NULL,			// Default security
				OPEN_EXISTING,		// Existing file only
				FILE_ATTRIBUTE_NORMAL,	// Normal file
				NULL);			// No attr. template

			if (fh == INVALID_HANDLE_VALUE) {
				stat_info->rw_perm = false;
			} else {
				stat_info->rw_perm = true;
				CloseHandle(fh);
			}
		}
	} else {
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}
#else
	struct stat	statinfo;

	ret = stat(path, &statinfo);

	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(DB_NOT_FOUND);

	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", false);

		return(DB_FAIL);

	}

	switch (statinfo.st_mode & S_IFMT) {
	case S_IFDIR:
		stat_info->type = OS_FILE_TYPE_DIR;
		break;
	case S_IFLNK:
		stat_info->type = OS_FILE_TYPE_LINK;
		break;
	case S_IFBLK:
		stat_info->type = OS_FILE_TYPE_BLOCK;
		break;
	case S_IFREG:
		stat_info->type = OS_FILE_TYPE_FILE;
		break;
	default:
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}

	if (check_rw_perm && (stat_info->type == OS_FILE_TYPE_FILE
			      || stat_info->type == OS_FILE_TYPE_BLOCK)) {
		int	fh;
		int	access;

		access = !read_only_mode ? O_RDWR : O_RDONLY;

		fh = ::open(path, access, os_innodb_umask);

		if (fh == -1) {
			stat_info->rw_perm = false;
		} else {
			stat_info->rw_perm = true;
			close(fh);
		}
	}

#endif /* _WIN_ */

	stat_info->ctime = statinfo.st_ctime;
	stat_info->atime = statinfo.st_atime;
	stat_info->mtime = statinfo.st_mtime;
	stat_info->size  = statinfo.st_size;

	return(DB_SUCCESS);
}

/****************************************************************//**
This function returns a new path name after replacing the basename
in an old path with a new basename.  The old_path is a full path
name including the extension.  The tablename is in the normal
form "databasename/tablename".  The new base name is found after
the forward slash.  Both input strings are null terminated.

This function allocates memory to be returned.  It is the callers
responsibility to free the return value after it is no longer needed.

@return own: new full pathname */
char*
os_file_make_new_pathname(
/*======================*/
	const char*	old_path,	/*!< in: pathname */
	const char*	tablename)	/*!< in: contains new base name */
{
	ulint		dir_len;
	char*		last_slash;
	char*		base_name;
	char*		new_path;
	ulint		new_path_len;

	/* Split the tablename into its database and table name components.
	They are separated by a '/'. */
	last_slash = strrchr((char*) tablename, '/');
	base_name = last_slash ? last_slash + 1 : (char*) tablename;

	/* Find the offset of the last slash. We will strip off the
	old basename.ibd which starts after that slash. */
	last_slash = strrchr((char*) old_path, OS_PATH_SEPARATOR);
	dir_len = last_slash ? last_slash - old_path : strlen(old_path);

	/* allocate a new path and move the old directory path to it. */
	new_path_len = dir_len + strlen(base_name) + sizeof "/.ibd";
	new_path = static_cast<char*>(ut_malloc_nokey(new_path_len));
	memcpy(new_path, old_path, dir_len);

	ut_snprintf(new_path + dir_len,
		    new_path_len - dir_len,
		    "%c%s.ibd",
		    OS_PATH_SEPARATOR,
		    base_name);

	return(new_path);
}

/****************************************************************//**
This function reduces a null-terminated full remote path name into
the path that is sent by MySQL for DATA DIRECTORY clause.  It replaces
the 'databasename/tablename.ibd' found at the end of the path with just
'tablename'.

Since the result is always smaller than the path sent in, no new memory
is allocated. The caller should allocate memory for the path sent in.
This function manipulates that path in place.

If the path format is not as expected, just return.  The result is used
to inform a SHOW CREATE TABLE command. */
void
os_file_make_data_dir_path(
/*========================*/
	char*	data_dir_path)	/*!< in/out: full path/data_dir_path */
{
	char*	ptr;
	char*	tablename;
	ulint	tablename_len;

	/* Replace the period before the extension with a null byte. */
	ptr = strrchr((char*) data_dir_path, '.');
	if (!ptr) {
		return;
	}
	ptr[0] = '\0';

	/* The tablename starts after the last slash. */
	ptr = strrchr((char*) data_dir_path, OS_PATH_SEPARATOR);
	if (!ptr) {
		return;
	}
	ptr[0] = '\0';
	tablename = ptr + 1;

	/* The databasename starts after the next to last slash. */
	ptr = strrchr((char*) data_dir_path, OS_PATH_SEPARATOR);
	if (!ptr) {
		return;
	}
	tablename_len = ut_strlen(tablename);

	ut_memmove(++ptr, tablename, tablename_len);

	ptr[tablename_len] = '\0';
}

/****************************************************************//**
The function os_file_dirname returns a directory component of a
null-terminated pathname string. In the usual case, dirname returns
the string up to, but not including, the final '/', and basename
is the component following the final '/'. Trailing '/' characters
are not counted as part of the pathname.

If path does not contain a slash, dirname returns the string ".".

Concatenating the string returned by dirname, a "/", and the basename
yields a complete pathname.

The return value is a copy of the directory component of the pathname.
The copy is allocated from heap. It is the caller responsibility
to free it after it is no longer needed.

The following list of examples (taken from SUSv2) shows the strings
returned by dirname and basename for different paths:

       path	      dirname	     basename
       "/usr/lib"     "/usr"	     "lib"
       "/usr/"	      "/"	     "usr"
       "usr"	      "."	     "usr"
       "/"	      "/"	     "/"
       "."	      "."	     "."
       ".."	      "."	     ".."

@return own: directory component of the pathname */
char*
os_file_dirname(
/*============*/
	const char*	path)	/*!< in: pathname */
{
	/* Find the offset of the last slash */
	const char* last_slash = strrchr(path, OS_PATH_SEPARATOR);
	if (!last_slash) {
		/* No slash in the path, return "." */

		return(mem_strdup("."));
	}

	/* Ok, there is a slash */

	if (last_slash == path) {
		/* last slash is the first char of the path */

		return(mem_strdup("/"));
	}

	/* Non-trivial directory component */

	return(mem_strdupl(path, last_slash - path));
}

/****************************************************************//**
Creates all missing subdirectories along the given path.
@return true if call succeeded, false otherwise */
bool
os_file_create_subdirs_if_needed(
/*=============================*/
	const char*	path)	/*!< in: path name */
{
	if (srv_read_only_mode) {

		ib::error() << "read only mode set. Can't create"
			" subdirectories '" << path << "'";

		return(false);

	}

	char*	subdir = os_file_dirname(path);

	if (strlen(subdir) == 1
	    && (*subdir == OS_PATH_SEPARATOR || *subdir == '.')) {
		/* subdir is root or cwd, nothing to do */
		ut_free(subdir);

		return(true);
	}

	/* Test if subdir exists */
	os_file_type_t	type;
	bool	subdir_exists;
	bool	success = os_file_status(subdir, &subdir_exists, &type);

	if (success && !subdir_exists) {

		/* subdir does not exist, create it */
		success = os_file_create_subdirs_if_needed(subdir);

		if (!success) {
			ut_free(subdir);

			return(false);
		}

		success = os_file_create_directory(subdir, false);
	}

	ut_free(subdir);

	return(success);
}

#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Returns a pointer to the nth slot in the aio array.
@return pointer to slot */
static
os_aio_slot_t*
os_aio_array_get_nth_slot(
/*======================*/
	os_aio_array_t*		array,	/*!< in: aio array */
	ulint			index)	/*!< in: index of the slot */
{
	ut_a(index < array->n_slots);

	return(&array->slots[index]);
}

#if defined(LINUX_NATIVE_AIO)
/******************************************************************//**
Creates an io_context for native linux AIO.
@return true on success. */
static
bool
os_aio_linux_create_io_ctx(
/*=======================*/
	ulint		max_events,	/*!< in: number of events. */
	io_context_t*	io_ctx)		/*!< out: io_ctx to initialize. */
{
	int	ret;
	ulint	retries = 0;

retry:
	memset(io_ctx, 0x0, sizeof(*io_ctx));

	/* Initialize the io_ctx. Tell it how many pending
	IO requests this context will handle. */

	ret = io_setup(max_events, io_ctx);
	if (ret == 0) {
#if defined(UNIV_AIO_DEBUG)
		ib::info() << "Linux native AIO: initialized io_ctx for"
			<< " segment";
#endif
		/* Success. Return now. */
		return(true);
	}

	/* If we hit EAGAIN we'll make a few attempts before failing. */

	switch (ret) {
	case -EAGAIN:
		if (retries == 0) {
			/* First time around. */
			ib::warn() << "io_setup() failed with EAGAIN. Will"
				" make " << OS_AIO_IO_SETUP_RETRY_ATTEMPTS
				<< " attempts before giving up.";
		}

		if (retries < OS_AIO_IO_SETUP_RETRY_ATTEMPTS) {
			++retries;
			ib::warn() << "io_setup() attempt " << retries
				<< " failed.";
			os_thread_sleep(OS_AIO_IO_SETUP_RETRY_SLEEP);
			goto retry;
		}

		/* Have tried enough. Better call it a day. */
		ib::error() << "io_setup() failed with EAGAIN after "
			<< OS_AIO_IO_SETUP_RETRY_ATTEMPTS << " attempts.";
		break;

	case -ENOSYS:
		ib::error() << "Linux Native AIO interface"
			" is not supported on this platform. Please"
			" check your OS documentation and install"
			" appropriate binary of InnoDB.";

		break;

	default:
		ib::error() << "Linux Native AIO setup"
			" returned following error[" << -ret << "]";
		break;
	}

	ib::info() << "You can disable Linux Native AIO by"
		" setting innodb_use_native_aio = 0 in my.cnf";
	return(false);
}

/******************************************************************//**
Checks if the system supports native linux aio. On some kernel
versions where native aio is supported it won't work on tmpfs. In such
cases we can't use native aio as it is not possible to mix simulated
and native aio.
@return: true if supported, false otherwise. */
static
bool
os_aio_native_aio_supported(void)
/*=============================*/
{
	int			fd;
	io_context_t		io_ctx;
	char			name[1000];

	if (!os_aio_linux_create_io_ctx(1, &io_ctx)) {
		/* The platform does not support native aio. */
		return(false);
	} else if (!srv_read_only_mode) {
		/* Now check if tmpdir supports native aio ops. */
		fd = innobase_mysql_tmpfile();

		if (fd < 0) {
			ib::warn() << "Unable to create temp file to check"
				" native AIO support.";

			return(false);
		}
	} else {

		os_normalize_path_for_win(srv_log_group_home_dir);

		ulint	dirnamelen = strlen(srv_log_group_home_dir);
		ut_a(dirnamelen < (sizeof name) - 10 - sizeof "ib_logfile");
		memcpy(name, srv_log_group_home_dir, dirnamelen);

		/* Add a path separator if needed. */
		if (dirnamelen && name[dirnamelen - 1] != OS_PATH_SEPARATOR) {
			name[dirnamelen++] = OS_PATH_SEPARATOR;
		}

		strcpy(name + dirnamelen, "ib_logfile0");

		fd = ::open(name, O_RDONLY);

		if (fd == -1) {

			ib::warn() << "Unable to open \"" << name << "\" to check"
				" native AIO read support.";

			return(false);
		}
	}

	struct io_event	io_event;

	memset(&io_event, 0x0, sizeof(io_event));

	byte*	buf = static_cast<byte*>(ut_malloc_nokey(UNIV_PAGE_SIZE * 2));
	byte*	ptr = static_cast<byte*>(ut_align(buf, UNIV_PAGE_SIZE));

	struct iocb	iocb;

	/* Suppress valgrind warning. */
	memset(buf, 0x00, UNIV_PAGE_SIZE * 2);
	memset(&iocb, 0x0, sizeof(iocb));

	struct iocb*	p_iocb = &iocb;

	if (!srv_read_only_mode) {
		io_prep_pwrite(p_iocb, fd, ptr, UNIV_PAGE_SIZE, 0);
	} else {
		ut_a(UNIV_PAGE_SIZE >= 512);
		io_prep_pread(p_iocb, fd, ptr, 512, 0);
	}

	int	err = io_submit(io_ctx, 1, &p_iocb);

	if (err >= 1) {
		/* Now collect the submitted IO request. */
		err = io_getevents(io_ctx, 1, 1, &io_event, NULL);
	}

	ut_free(buf);
	close(fd);

	switch (err) {
	case 1:
		return(true);

	case -EINVAL:
	case -ENOSYS:
		ib::error() << "Linux Native AIO not supported. You can either"
			" move " << (srv_read_only_mode ? name : "tmpdir") <<
			" to a file system that supports native"
			" AIO or you can set innodb_use_native_aio to"
			" FALSE to avoid this message.";

		/* fall through. */
	default:
		ib::error() << "Linux Native AIO check on "
			<< (srv_read_only_mode ? name : "tmpdir")
			<< " returned error[" << -err << "]";
	}

	return(false);
}
#endif /* LINUX_NATIVE_AIO */

/******************************************************************//**
Creates an aio wait array. Note that we return NULL in case of failure.
We don't care about freeing memory here because we assume that a
failure will result in server refusing to start up.
@return own: aio array, NULL on failure */
static
os_aio_array_t*
os_aio_array_create(
/*================*/
	ulint	n,		/*!< in: maximum number of pending aio
				operations allowed; n must be
				divisible by n_segments */
	ulint	n_segments)	/*!< in: number of segments in the aio array */
{
	os_aio_array_t*	array;
#ifdef WIN_ASYNC_IO
	OVERLAPPED*	over;
#elif defined(LINUX_NATIVE_AIO)
	struct io_event*	io_event = NULL;
#endif /* WIN_ASYNC_IO */
	ut_a(n > 0);
	ut_a(n_segments > 0);

	array = static_cast<os_aio_array_t*>(ut_zalloc_nokey(sizeof(*array)));

	mutex_create("os_aio_mutex", &array->mutex);

	array->not_full = os_event_create("aio_not_full");
	array->is_empty = os_event_create("aio_is_empty");

	os_event_set(array->is_empty);

	array->n_slots = n;
	array->n_segments = n_segments;

	array->slots = static_cast<os_aio_slot_t*>(
		ut_zalloc_nokey(n * sizeof(*array->slots)));

#ifdef _WIN32
	array->handles = static_cast<HANDLE*>(
		ut_malloc_nokey(n * sizeof(HANDLE)));
#endif /* _WIN32 */

#if defined(LINUX_NATIVE_AIO)
	array->aio_ctx = NULL;
	array->aio_events = NULL;

	/* If we are not using native aio interface then skip this
	part of initialization. */
	if (!srv_use_native_aio) {
		goto skip_native_aio;
	}

	/* Initialize the io_context array. One io_context
	per segment in the array. */

	array->aio_ctx = static_cast<io_context**>(
		ut_malloc_nokey(n_segments * sizeof(*array->aio_ctx)));

	for (ulint i = 0; i < n_segments; ++i) {
		if (!os_aio_linux_create_io_ctx(n/n_segments,
						&array->aio_ctx[i])) {
			/* If something bad happened during aio setup
			we should call it a day and return right away.
			We don't care about any leaks because a failure
			to initialize the io subsystem means that the
			server (or atleast the innodb storage engine)
			is not going to startup. */
			return(NULL);
		}
	}

	/* Initialize the event array. One event per slot. */
	io_event = static_cast<struct io_event*>(
		ut_zalloc_nokey(n * sizeof(*io_event)));

	array->aio_events = io_event;

skip_native_aio:
#endif /* LINUX_NATIVE_AIO */
	for (ulint i = 0; i < n; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i);

		slot->pos = i;
		slot->is_reserved = false;
#ifdef WIN_ASYNC_IO
		slot->handle = CreateEvent(NULL, TRUE, FALSE, NULL);

		over = &slot->control;

		over->hEvent = slot->handle;

		array->handles[i] = over->hEvent;

#elif defined(LINUX_NATIVE_AIO)
		memset(&slot->control, 0x0, sizeof(slot->control));
		slot->n_bytes = 0;
		slot->ret = 0;
#endif /* WIN_ASYNC_IO */
	}

	return(array);
}

/************************************************************************//**
Frees an aio wait array. */
static
void
os_aio_array_free(
/*==============*/
	os_aio_array_t*& array)	/*!< in, own: array to free */
{
#ifdef WIN_ASYNC_IO
	for (ulint i = 0; i < array->n_slots; i++) {
		os_aio_slot_t*	slot = os_aio_array_get_nth_slot(array, i);
		CloseHandle(slot->handle);
	}
#endif /* WIN_ASYNC_IO */

#ifdef _WIN32
	ut_free(array->handles);
#endif /* _WIN32 */

	mutex_destroy(&array->mutex);

	os_event_destroy(array->not_full);
	os_event_destroy(array->is_empty);

#if defined(LINUX_NATIVE_AIO)
	if (srv_use_native_aio) {
		ut_free(array->aio_events);
		ut_free(array->aio_ctx);
	}
#endif /* LINUX_NATIVE_AIO */

	ut_free(array->slots);
	ut_free(array);

	array = 0;
}

/***********************************************************************
Initializes the asynchronous io system. Creates one array each for ibuf
and log i/o. Also creates one array each for read and write where each
array is divided logically into n_read_segs and n_write_segs
respectively. The caller must create an i/o handler thread for each
segment in these arrays. This function also creates the sync array.
No i/o handler thread needs to be created for that */
bool
os_aio_init(
/*========*/
	ulint	n_per_seg,	/*<! in: maximum number of pending aio
				operations allowed per segment */
	ulint	n_read_segs,	/*<! in: number of reader threads */
	ulint	n_write_segs,	/*<! in: number of writer threads */
	ulint	n_slots_sync)	/*<! in: number of slots in the sync aio
				array */
{
	os_io_init_simple();

#if defined(LINUX_NATIVE_AIO)
	/* Check if native aio is supported on this system and tmpfs */
	if (srv_use_native_aio && !os_aio_native_aio_supported()) {

		ib::warn() << "Linux Native AIO disabled.";

		srv_use_native_aio = FALSE;
	}
#endif /* LINUX_NATIVE_AIO */

	srv_reset_io_thread_op_info();

	/* Initialize ibuf and log aio segment. */
	ulint	n_segments = 0;
	if (!srv_read_only_mode) {

		os_aio_log_array = os_aio_array_create(n_per_seg, 1);

		if (os_aio_log_array == NULL) {
			return(false);
		}

		++n_segments;

		srv_io_thread_function[1] = "log thread";

		os_aio_ibuf_array = os_aio_array_create(n_per_seg, 1);

		if (os_aio_ibuf_array == NULL) {
			return(false);
		}

		++n_segments;

		srv_io_thread_function[0] = "insert buffer thread";
	}

	/* Initialize read aio segment. */
	os_aio_read_array = os_aio_array_create(
		n_read_segs * n_per_seg, n_read_segs);

	if (os_aio_read_array == NULL) {
		return(false);
	}

	for (ulint i = n_segments; i < (n_read_segs + n_segments); ++i) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "read thread";
	}

	n_segments += n_read_segs;

	/* Initialize write aio segment. */
	os_aio_write_array = os_aio_array_create(
		n_write_segs * n_per_seg, n_write_segs);

	if (os_aio_write_array == NULL) {
		return(false);
	}

	for (ulint i = n_segments; i < (n_write_segs + n_segments); ++i) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "write thread";
	}

	n_segments += n_write_segs;

	ut_ad(n_segments >= static_cast<ulint>(srv_read_only_mode ? 2 : 4));

	/* Initialize AIO sync array. */
	os_aio_sync_array = os_aio_array_create(n_slots_sync, 1);

	if (os_aio_sync_array == NULL) {
		return(false);
	}

	os_aio_n_segments = n_segments;

	os_aio_validate();

	os_aio_segment_wait_events = static_cast<os_event_t*>(
		ut_malloc_nokey(n_segments
				* sizeof *os_aio_segment_wait_events));

	for (ulint i = 0; i < n_segments; ++i) {
		os_aio_segment_wait_events[i] = os_event_create(0);
	}

	os_last_printout = ut_time();

	return(true);
}

/***********************************************************************
Frees the asynchronous io system. */
void
os_aio_free(void)
/*=============*/
{
	if (os_aio_ibuf_array != 0) {
		os_aio_array_free(os_aio_ibuf_array);
	}

	if (os_aio_log_array != 0) {
		os_aio_array_free(os_aio_log_array);
	}

	if (os_aio_write_array != 0) {
		os_aio_array_free(os_aio_write_array);
	}

	if (os_aio_sync_array != 0) {
		os_aio_array_free(os_aio_sync_array);
	}

	os_aio_array_free(os_aio_read_array);

#ifndef UNIV_HOTBACKUP
	for (ulint i = 0; i < OS_FILE_N_SEEK_MUTEXES; i++) {
		mutex_free(os_file_seek_mutexes[i]);
		UT_DELETE(os_file_seek_mutexes[i]);
	}
#endif /* !UNIV_HOTBACKUP */

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_event_destroy(os_aio_segment_wait_events[i]);
	}

	ut_free(os_aio_segment_wait_events);
	os_aio_segment_wait_events = 0;
	os_aio_n_segments = 0;
}

#ifdef WIN_ASYNC_IO
/************************************************************************//**
Wakes up all async i/o threads in the array in Windows async i/o at
shutdown. */
static
void
os_aio_array_wake_win_aio_at_shutdown(
/*==================================*/
	os_aio_array_t*	array)	/*!< in: aio array */
{
	for (ulint i = 0; i < array->n_slots; i++) {

		SetEvent((array->slots + i)->handle);
	}
}
#endif

/************************************************************************//**
Wakes up all async i/o threads so that they know to exit themselves in
shutdown. */
void
os_aio_wake_all_threads_at_shutdown(void)
/*=====================================*/
{
#ifdef WIN_ASYNC_IO
	/* This code wakes up all ai/o threads in Windows native aio */
	os_aio_array_wake_win_aio_at_shutdown(os_aio_read_array);
	if (os_aio_write_array != 0) {
		os_aio_array_wake_win_aio_at_shutdown(os_aio_write_array);
	}

	if (os_aio_ibuf_array != 0) {
		os_aio_array_wake_win_aio_at_shutdown(os_aio_ibuf_array);
	}

	if (os_aio_log_array != 0) {
		os_aio_array_wake_win_aio_at_shutdown(os_aio_log_array);
	}

#elif defined(LINUX_NATIVE_AIO)

	/* When using native AIO interface the io helper threads
	wait on io_getevents with a timeout value of 500ms. At
	each wake up these threads check the server status.
	No need to do anything to wake them up. */

	if (srv_use_native_aio) {
		return;
	}

	/* Fall through to simulated AIO handler wakeup if we are
	not using native AIO. */
#endif /* !WIN_ASYNC_AIO */

	/* This loop wakes up all simulated ai/o threads */

	for (ulint i = 0; i < os_aio_n_segments; i++) {

		os_event_set(os_aio_segment_wait_events[i]);
	}
}

/************************************************************************//**
Waits until there are no pending writes in os_aio_write_array. There can
be other, synchronous, pending writes. */
void
os_aio_wait_until_no_pending_writes(void)
/*=====================================*/
{
	os_event_wait(os_aio_write_array->is_empty);
}

/**********************************************************************//**
Calculates segment number for a slot.
@return segment number (which is the number used by, for example,
i/o-handler threads) */
static
ulint
os_aio_get_segment_no_from_slot(
/*============================*/
	os_aio_array_t*	array,	/*!< in: aio wait array */
	os_aio_slot_t*	slot)	/*!< in: slot in this array */
{
	ulint	segment;
	ulint	seg_len;
	ulint	n_extra_segs = srv_read_only_mode ? 0 : 2;

	if (array == os_aio_ibuf_array) {
		ut_ad(!srv_read_only_mode);

		segment = IO_IBUF_SEGMENT;

	} else if (array == os_aio_log_array) {
		ut_ad(!srv_read_only_mode);

		segment = IO_LOG_SEGMENT;

	} else if (array == os_aio_read_array) {
		seg_len = os_aio_read_array->n_slots
			/ os_aio_read_array->n_segments;

		segment = n_extra_segs + slot->pos / seg_len;
	} else {
		ut_a(array == os_aio_write_array);

		seg_len = os_aio_write_array->n_slots
			/ os_aio_write_array->n_segments;

		segment = os_aio_read_array->n_segments
			  + n_extra_segs + slot->pos / seg_len;
	}

	return(segment);
}

/**********************************************************************//**
Calculates local segment number and aio array from global segment number.
@return local segment number within the aio array */
static
ulint
os_aio_get_array_and_local_segment(
/*===============================*/
	os_aio_array_t** array,		/*!< out: aio wait array */
	ulint		 global_segment)/*!< in: global segment number */
{
	ulint	segment;
	ulint	n_extra_segs = (srv_read_only_mode) ? 0 : 2;

	ut_a(global_segment < os_aio_n_segments);

	if (!srv_read_only_mode && global_segment < n_extra_segs) {
		if (global_segment == IO_IBUF_SEGMENT) {
			*array = os_aio_ibuf_array;
			segment = 0;

		} else if (global_segment == IO_LOG_SEGMENT) {
			*array = os_aio_log_array;
			segment = 0;
		} else {
			*array = NULL;
			segment = 0;
		}
	} else if (global_segment <
			os_aio_read_array->n_segments + n_extra_segs) {
		*array = os_aio_read_array;

		segment = global_segment - (n_extra_segs);
	} else {
		*array = os_aio_write_array;

		segment = global_segment -
				(os_aio_read_array->n_segments + n_extra_segs);
	}

	return(segment);
}

/*******************************************************************//**
Requests for a slot in the aio array. If no slot is available, waits until
not_full-event becomes signaled.
@return pointer to slot */
static
os_aio_slot_t*
os_aio_array_reserve_slot(
/*======================*/
	ulint		type,	/*!< in: OS_FILE_READ or OS_FILE_WRITE */
	os_aio_array_t*	array,	/*!< in: aio array */
	fil_node_t*	message1,/*!< in: message to be passed along with
				the aio operation */
	void*		message2,/*!< in: message to be passed along with
				the aio operation */
	os_file_t	file,	/*!< in: file handle */
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	void*		buf,	/*!< in: buffer where to read or from which
				to write */
	os_offset_t	offset,	/*!< in: file offset */
	ulint		len)	/*!< in: length of the block to read or write */
{
	os_aio_slot_t*	slot = NULL;
#ifdef WIN_ASYNC_IO
	OVERLAPPED*	control;

#elif defined(LINUX_NATIVE_AIO)

	struct iocb*	iocb;
	off_t		aio_offset;

#endif /* WIN_ASYNC_IO */
	ulint		i;
	ulint		counter;
	ulint		slots_per_seg;
	ulint		local_seg;

#ifdef WIN_ASYNC_IO
	ut_a((len & 0xFFFFFFFFUL) == len);
#endif /* WIN_ASYNC_IO */

	/* No need of a mutex. Only reading constant fields */
	slots_per_seg = array->n_slots / array->n_segments;

	/* We attempt to keep adjacent blocks in the same local
	segment. This can help in merging IO requests when we are
	doing simulated AIO */
	local_seg = (offset >> (UNIV_PAGE_SIZE_SHIFT + 6))
		% array->n_segments;

loop:
	mutex_enter(&array->mutex);

	if (array->n_reserved == array->n_slots) {
		mutex_exit(&array->mutex);

		if (!srv_use_native_aio) {
			/* If the handler threads are suspended, wake them
			so that we get more slots */

			os_aio_simulated_wake_handler_threads();
		}

		os_event_wait(array->not_full);

		goto loop;
	}

	/* We start our search for an available slot from our preferred
	local segment and do a full scan of the array. We are
	guaranteed to find a slot in full scan. */
	for (i = local_seg * slots_per_seg, counter = 0;
	     counter < array->n_slots;
	     i++, counter++) {

		i %= array->n_slots;

		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->is_reserved == false) {
			goto found;
		}
	}

	/* We MUST always be able to get hold of a reserved slot. */
	ut_error;

found:
	ut_a(slot->is_reserved == false);
	array->n_reserved++;

	if (array->n_reserved == 1) {
		os_event_reset(array->is_empty);
	}

	if (array->n_reserved == array->n_slots) {
		os_event_reset(array->not_full);
	}

	slot->is_reserved = true;
	slot->reservation_time = ut_time();
	slot->message1 = message1;
	slot->message2 = message2;
	slot->file     = file;
	slot->name     = name;
	slot->len      = len;
	slot->type     = type;
	slot->buf      = static_cast<byte*>(buf);
	slot->offset   = offset;
	slot->io_already_done = false;

#ifdef WIN_ASYNC_IO
	control = &slot->control;
	control->Offset = (DWORD) offset & 0xFFFFFFFF;
	control->OffsetHigh = (DWORD) (offset >> 32);
	ResetEvent(slot->handle);

#elif defined(LINUX_NATIVE_AIO)

	/* If we are not using native AIO skip this part. */
	if (!srv_use_native_aio) {
		goto skip_native_aio;
	}

	/* Check if we are dealing with 64 bit arch.
	If not then make sure that offset fits in 32 bits. */
	aio_offset = (off_t) offset;

	ut_a(sizeof(aio_offset) >= sizeof(offset)
	     || ((os_offset_t) aio_offset) == offset);

	iocb = &slot->control;

	if (type == OS_FILE_READ) {
		io_prep_pread(iocb, file, buf, len, aio_offset);
	} else {
		ut_a(type == OS_FILE_WRITE);
		io_prep_pwrite(iocb, file, buf, len, aio_offset);
	}

	iocb->data = (void*) slot;
	slot->n_bytes = 0;
	slot->ret = 0;

skip_native_aio:
#endif /* LINUX_NATIVE_AIO */
	mutex_exit(&array->mutex);

	return(slot);
}

/*******************************************************************//**
Frees a slot in the aio array. */
static
void
os_aio_array_free_slot(
/*===================*/
	os_aio_array_t*	array,	/*!< in: aio array */
	os_aio_slot_t*	slot)	/*!< in: pointer to slot */
{
	mutex_enter(&array->mutex);

	ut_ad(slot->is_reserved);

	slot->is_reserved = false;

	array->n_reserved--;

	if (array->n_reserved == array->n_slots - 1) {
		os_event_set(array->not_full);
	}

	if (array->n_reserved == 0) {
		os_event_set(array->is_empty);
	}

#ifdef WIN_ASYNC_IO

	ResetEvent(slot->handle);

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		memset(&slot->control, 0x0, sizeof(slot->control));
		slot->n_bytes = 0;
		slot->ret = 0;
		/*fprintf(stderr, "Freed up Linux native slot.\n");*/
	} else {
		/* These fields should not be used if we are not
		using native AIO. */
		ut_ad(slot->n_bytes == 0);
		ut_ad(slot->ret == 0);
	}

#endif
	mutex_exit(&array->mutex);
}

/**********************************************************************//**
Wakes up a simulated aio i/o-handler thread if it has something to do. */
static
void
os_aio_simulated_wake_handler_thread(
/*=================================*/
	ulint	global_segment)	/*!< in: the number of the segment in the aio
				arrays */
{
	os_aio_array_t*	array;
	ulint		segment;

	ut_ad(!srv_use_native_aio);

	segment = os_aio_get_array_and_local_segment(&array, global_segment);

	ulint	n = array->n_slots / array->n_segments;

	segment *= n;

	/* Look through n slots after the segment * n'th slot */

	mutex_enter(&array->mutex);

	for (ulint i = 0; i < n; ++i) {
		const os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, segment + i);

		if (slot->is_reserved) {

			/* Found an i/o request */

			mutex_exit(&array->mutex);

			os_event_t	event;

			event = os_aio_segment_wait_events[global_segment];

			os_event_set(event);

			return;
		}
	}

	mutex_exit(&array->mutex);
}

/**********************************************************************//**
Wakes up simulated aio i/o-handler threads if they have something to do. */
void
os_aio_simulated_wake_handler_threads(void)
/*=======================================*/
{
	if (srv_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= false;

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_aio_simulated_wake_handler_thread(i);
	}
}

/**********************************************************************//**
This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
void
os_aio_simulated_put_read_threads_to_sleep(void)
/*============================================*/
{

/* The idea of putting background IO threads to sleep is only for
Windows when using simulated AIO. Windows XP seems to schedule
background threads too eagerly to allow for coalescing during
readahead requests. */
#ifdef _WIN32
	os_aio_array_t*	array;

	if (srv_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= true;

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_aio_get_array_and_local_segment(&array, i);

		if (array == os_aio_read_array) {

			os_event_reset(os_aio_segment_wait_events[i]);
		}
	}
#endif /* _WIN32 */
}

#if defined(LINUX_NATIVE_AIO)
/*******************************************************************//**
Dispatch an AIO request to the kernel.
@return true on success. */
static
bool
os_aio_linux_dispatch(
/*==================*/
	os_aio_array_t*	array,	/*!< in: io request array. */
	os_aio_slot_t*	slot)	/*!< in: an already reserved slot. */
{
	int		ret;
	ulint		io_ctx_index;
	struct iocb*	iocb;

	ut_ad(slot != NULL);
	ut_ad(array);

	ut_a(slot->is_reserved);

	/* Find out what we are going to work with.
	The iocb struct is directly in the slot.
	The io_context is one per segment. */

	iocb = &slot->control;
	io_ctx_index = (slot->pos * array->n_segments) / array->n_slots;

	ret = io_submit(array->aio_ctx[io_ctx_index], 1, &iocb);

#if defined(UNIV_AIO_DEBUG)
	const char	r = (slot->type == OS_FILE_WRITE) ? 'w' : 'r';
	ib::info()
		<< "io_submit[" << r << "] ret[" << ret << "]: slot[" << slot
		<< "] ctx[" << array->aio_ctx[io_ctx_index] << "] seg["
		<< io_ctx_index << "]";
#endif

	/* io_submit returns number of successfully
	queued requests or -errno. */
	if (UNIV_UNLIKELY(ret != 1)) {
		errno = -ret;
		return(false);
	}

	return(true);
}
#endif /* LINUX_NATIVE_AIO */


/*******************************************************************//**
NOTE! Use the corresponding macro os_aio(), not directly this function!
Requests an asynchronous i/o operation.
@return true if request was queued successfully, false if fail */
bool
os_aio_func(
/*========*/
	ulint		type,	/*!< in: OS_FILE_READ or OS_FILE_WRITE */
	ulint		mode,	/*!< in: OS_AIO_NORMAL, ..., possibly ORed
				to OS_AIO_SIMULATED_WAKE_LATER: the
				last flag advises this function not to wake
				i/o-handler threads, but the caller will
				do the waking explicitly later, in this
				way the caller can post several requests in
				a batch; NOTE that the batch must not be
				so big that it exhausts the slots in aio
				arrays! NOTE that a simulated batch
				may introduce hidden chances of deadlocks,
				because i/os are not actually handled until
				all have been posted: use with great
				caution! */
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read or from which
				to write */
	os_offset_t	offset,	/*!< in: file offset where to read or write */
	ulint		n,	/*!< in: number of bytes to read or write */
	bool		read_only_mode,
				/*!< in: if true, read only mode
				checks are enforced. */
	fil_node_t*	message1,/*!< in: message for the aio handler
				(can be used to identify a completed
				aio operation); ignored if mode is
				OS_AIO_SYNC */
	void*		message2)/*!< in: message for the aio handler
				(can be used to identify a completed
				aio operation); ignored if mode is
				OS_AIO_SYNC */
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
#ifdef WIN_ASYNC_IO
	bool		retval;
	BOOL		ret		= TRUE;
	DWORD		len		= (DWORD) n;
	struct fil_node_t* dummy_mess1;
	void*		dummy_mess2;
	ulint		dummy_type;
#endif /* WIN_ASYNC_IO */
	ulint		wake_later;

	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);
	ut_ad(n % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(os_aio_validate_skip());
#ifdef WIN_ASYNC_IO
	ut_ad((n & 0xFFFFFFFFUL) == n);
#endif

	wake_later = mode & OS_AIO_SIMULATED_WAKE_LATER;
	mode = mode & (~OS_AIO_SIMULATED_WAKE_LATER);

	if (mode == OS_AIO_SYNC
#ifdef WIN_ASYNC_IO
	    && !srv_use_native_aio
#endif /* WIN_ASYNC_IO */
	    ) {
		/* This is actually an ordinary synchronous read or write:
		no need to use an i/o-handler thread. NOTE that if we use
		Windows async i/o, Windows does not allow us to use
		ordinary synchronous os_file_read etc. on the same file,
		therefore we have built a special mechanism for synchronous
		wait in the Windows case.
		Also note that the Performance Schema instrumentation has
		been performed by current os_aio_func()'s wrapper function
		pfs_os_aio_func(). So we would no longer need to call
		Performance Schema instrumented os_file_read() and
		os_file_write(). Instead, we should use os_file_read_func()
		and os_file_write_func() */

		if (type == OS_FILE_READ) {
			return(os_file_read_func(file, buf, offset, n));
		}

		ut_ad(!read_only_mode);
		ut_a(type == OS_FILE_WRITE);

		return(os_file_write_func(name, file, buf, offset, n));
	}

try_again:
	switch (mode) {
	case OS_AIO_NORMAL:
		if (type == OS_FILE_READ) {
			array = os_aio_read_array;
		} else {
			ut_ad(!read_only_mode);
			array = os_aio_write_array;
		}
		break;
	case OS_AIO_IBUF:
		ut_ad(type == OS_FILE_READ);
		/* Reduce probability of deadlock bugs in connection with ibuf:
		do not let the ibuf i/o handler sleep */

		wake_later = FALSE;

		if (read_only_mode) {
			array = os_aio_read_array;
		} else {
			array = os_aio_ibuf_array;
		}
		break;
	case OS_AIO_LOG:
		if (read_only_mode) {
			array = os_aio_read_array;
		} else {
			array = os_aio_log_array;
		}
		break;
	case OS_AIO_SYNC:
		array = os_aio_sync_array;
#if defined(LINUX_NATIVE_AIO)
		/* In Linux native AIO we don't use sync IO array. */
		ut_a(!srv_use_native_aio);
#endif /* LINUX_NATIVE_AIO */
		break;
	default:
		ut_error;
		array = NULL; /* Eliminate compiler warning */
	}

	slot = os_aio_array_reserve_slot(type, array, message1, message2, file,
					 name, buf, offset, n);
	if (type == OS_FILE_READ) {
		if (srv_use_native_aio) {
			os_n_file_reads++;
			os_bytes_read_since_printout += n;
#ifdef WIN_ASYNC_IO
			ret = ReadFile(file, buf, (DWORD) n, &len,
				       &(slot->control));

#elif defined(LINUX_NATIVE_AIO)
			if (!os_aio_linux_dispatch(array, slot)) {
				goto err_exit;
			}
#endif /* WIN_ASYNC_IO */
		} else {
			if (!wake_later) {
				os_aio_simulated_wake_handler_thread(
					os_aio_get_segment_no_from_slot(
						array, slot));
			}
		}
	} else if (type == OS_FILE_WRITE) {
		ut_ad(!read_only_mode);
		if (srv_use_native_aio) {
			os_n_file_writes++;
#ifdef WIN_ASYNC_IO
			ret = WriteFile(file, buf, (DWORD) n, &len,
					&(slot->control));

#elif defined(LINUX_NATIVE_AIO)
			if (!os_aio_linux_dispatch(array, slot)) {
				goto err_exit;
			}
#endif /* WIN_ASYNC_IO */
		} else {
			if (!wake_later) {
				os_aio_simulated_wake_handler_thread(
					os_aio_get_segment_no_from_slot(
						array, slot));
			}
		}
	} else {
		ut_error;
	}

#ifdef WIN_ASYNC_IO
	if (srv_use_native_aio) {
		if ((ret && len == n)
		    || (!ret && GetLastError() == ERROR_IO_PENDING)) {
			/* aio was queued successfully! */

			if (mode == OS_AIO_SYNC) {
				/* We want a synchronous i/o operation on a
				file where we also use async i/o: in Windows
				we must use the same wait mechanism as for
				async i/o */

				retval = os_aio_windows_handle(
					ULINT_UNDEFINED, slot->pos,
					&dummy_mess1, &dummy_mess2,
					&dummy_type);

				return(retval);
			}

			return(true);
		}

		goto err_exit;
	}
#endif /* WIN_ASYNC_IO */
	/* aio was queued successfully! */
	return(true);

#if defined LINUX_NATIVE_AIO || defined WIN_ASYNC_IO
err_exit:
#endif /* LINUX_NATIVE_AIO || WIN_ASYNC_IO */
	os_aio_array_free_slot(array, slot);

	if (os_file_handle_error(
		name,type == OS_FILE_READ ? "aio read" : "aio write")) {

		goto try_again;
	}

	return(false);
}

#ifdef WIN_ASYNC_IO
/**********************************************************************//**
This function is only used in Windows asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@return true if the aio operation succeeded */
bool
os_aio_windows_handle(
/*==================*/
	ulint	segment,	/*!< in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads; if
				this is ULINT_UNDEFINED, then it means that
				sync aio is used, and this parameter is
				ignored */
	ulint	pos,		/*!< this parameter is used only in sync aio:
				wait for the aio slot at this position */
	fil_node_t**message1,	/*!< out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2,
	ulint*	type)		/*!< out: OS_FILE_WRITE or ..._READ */
{
	ulint		orig_seg	= segment;
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	ulint		n;
	ulint		i;
	bool		ret_val;
	BOOL		ret;
	DWORD		len;
	BOOL		retry		= FALSE;

	if (segment == ULINT_UNDEFINED) {
		segment = 0;
		array = os_aio_sync_array;
	} else {
		segment = os_aio_get_array_and_local_segment(&array, segment);
	}

	/* NOTE! We only access constant fields in os_aio_array. Therefore
	we do not have to acquire the protecting mutex yet */

	ut_ad(os_aio_validate_skip());
	ut_ad(segment < array->n_segments);

	n = array->n_slots / array->n_segments;

	if (array == os_aio_sync_array) {

		WaitForSingleObject(
			os_aio_array_get_nth_slot(array, pos)->handle,
			INFINITE);

		i = pos;

	} else {
		if (orig_seg != ULINT_UNDEFINED) {
			srv_set_io_thread_op_info(orig_seg, "wait Windows aio");
		}

		i = WaitForMultipleObjects(
			(DWORD) n, array->handles + segment * n,
			FALSE, INFINITE);
	}

	mutex_enter(&array->mutex);

	if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
	    && array->n_reserved == 0
	    && !buf_page_cleaner_is_active) {
		*message1 = NULL;
		*message2 = NULL;
		mutex_exit(&array->mutex);
		return(true);
	}

	ut_a(i >= WAIT_OBJECT_0 && i <= WAIT_OBJECT_0 + n);

	slot = os_aio_array_get_nth_slot(array, i + segment * n);

	ut_a(slot->is_reserved);

	if (orig_seg != ULINT_UNDEFINED) {
		srv_set_io_thread_op_info(
			orig_seg, "get windows aio return value");
	}

	ret = GetOverlappedResult(slot->file, &(slot->control), &len, TRUE);

	*message1 = slot->message1;
	*message2 = slot->message2;

	*type = slot->type;

	if (ret && len == slot->len) {

		ret_val = true;
	} else if (os_file_handle_error(slot->name, "Windows aio")) {

		retry = true;
	} else {

		ret_val = false;
	}

	mutex_exit(&array->mutex);

	if (retry) {
		/* retry failed read/write operation synchronously.
		No need to hold array->mutex. */

#ifdef UNIV_PFS_IO
		/* This read/write does not go through os_file_read
		and os_file_write APIs, need to register with
		performance schema explicitly here. */
		struct PSI_file_locker* locker = NULL;
		register_pfs_file_io_begin(locker, slot->file, slot->len,
					   (slot->type == OS_FILE_WRITE)
						? PSI_FILE_WRITE
						: PSI_FILE_READ,
					    __FILE__, __LINE__);
#endif

		ut_a((slot->len & 0xFFFFFFFFUL) == slot->len);

		switch (slot->type) {
		case OS_FILE_WRITE:
			ret = WriteFile(slot->file, slot->buf,
					(DWORD) slot->len, &len,
					&(slot->control));

			break;
		case OS_FILE_READ:
			ret = ReadFile(slot->file, slot->buf,
				       (DWORD) slot->len, &len,
				       &(slot->control));

			break;
		default:
			ut_error;
		}

#ifdef UNIV_PFS_IO
		register_pfs_file_io_end(locker, len);
#endif

		if (!ret && GetLastError() == ERROR_IO_PENDING) {
			/* aio was queued successfully!
			We want a synchronous i/o operation on a
			file where we also use async i/o: in Windows
			we must use the same wait mechanism as for
			async i/o */

			ret = GetOverlappedResult(slot->file,
						  &(slot->control),
						  &len, TRUE);
		}

		ret_val = ret && len == slot->len;
	}

	os_aio_array_free_slot(array, slot);

	return(ret_val);
}
#endif

#if defined(LINUX_NATIVE_AIO)
/******************************************************************//**
This function is only used in Linux native asynchronous i/o. This is
called from within the io-thread. If there are no completed IO requests
in the slot array, the thread calls this function to collect more
requests from the kernel.
The io-thread waits on io_getevents(), which is a blocking call, with
a timeout value. Unless the system is very heavy loaded, keeping the
io-thread very busy, the io-thread will spend most of its time waiting
in this function.
The io-thread also exits in this function. It checks server status at
each wakeup and that is why we use timed wait in io_getevents(). */
static
void
os_aio_linux_collect(
/*=================*/
	os_aio_array_t* array,		/*!< in/out: slot array. */
	ulint		segment,	/*!< in: local segment no. */
	ulint		seg_size)	/*!< in: segment size. */
{
	int			i;
	int			ret;
	ulint			start_pos;
	ulint			end_pos;
	struct timespec		timeout;
	struct io_event*	events;
	struct io_context*	io_ctx;

	/* sanity checks. */
	ut_ad(array != NULL);
	ut_ad(seg_size > 0);
	ut_ad(segment < array->n_segments);

	/* Which part of event array we are going to work on. */
	events = &array->aio_events[segment * seg_size];

	/* Which io_context we are going to use. */
	io_ctx = array->aio_ctx[segment];

	/* Starting point of the segment we will be working on. */
	start_pos = segment * seg_size;

	/* End point. */
	end_pos = start_pos + seg_size;

retry:

	/* Initialize the events. The timeout value is arbitrary.
	We probably need to experiment with it a little. */
	memset(events, 0, sizeof(*events) * seg_size);
	timeout.tv_sec = 0;
	timeout.tv_nsec = OS_AIO_REAP_TIMEOUT;

	ret = io_getevents(io_ctx, 1, seg_size, events, &timeout);

	if (ret > 0) {
		for (i = 0; i < ret; i++) {
			os_aio_slot_t*	slot;
			struct iocb*	control;

			control = (struct iocb*) events[i].obj;
			ut_a(control != NULL);

			slot = (os_aio_slot_t*) control->data;

			/* Some sanity checks. */
			ut_a(slot != NULL);
			ut_a(slot->is_reserved);

#if defined(UNIV_AIO_DEBUG)
			const char	r
				= (slot->type == OS_FILE_WRITE) ? 'w' : 'r';
			ib::info() << "io_getevents[" << r << "]: slot["
				<< slot << "] ctx[" << io_ctx << "] seg["
				<< segment << "]";
#endif

			/* We are not scribbling previous segment. */
			ut_a(slot->pos >= start_pos);

			/* We have not overstepped to next segment. */
			ut_a(slot->pos < end_pos);

			/* Mark this request as completed. The error handling
			will be done in the calling function. */
			mutex_enter(&array->mutex);
			slot->n_bytes = events[i].res;
			slot->ret = events[i].res2;
			slot->io_already_done = true;
			mutex_exit(&array->mutex);
		}
		return;
	}

	if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
	    && !buf_page_cleaner_is_active) {
		return;
	}

	/* This error handling is for any error in collecting the
	IO requests. The errors, if any, for any particular IO
	request are simply passed on to the calling routine. */

	switch (ret) {
	case -EAGAIN:
		/* Not enough resources! Try again. */
	case -EINTR:
		/* Interrupted! I have tested the behaviour in case of an
		interrupt. If we have some completed IOs available then
		the return code will be the number of IOs. We get EINTR only
		if there are no completed IOs and we have been interrupted. */
	case 0:
		/* No pending request! Go back and check again. */
		goto retry;
	}

	/* All other errors should cause a trap for now. */
	ib::fatal() << "Unexpected ret_code[" << ret
		<< "] from io_getevents()!";
}

/**********************************************************************//**
This function is only used in Linux native asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait for
the completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@return true if the IO was successful */
bool
os_aio_linux_handle(
/*================*/
	ulint	global_seg,	/*!< in: segment number in the aio array
				to wait for; segment 0 is the ibuf
				i/o thread, segment 1 is log i/o thread,
				then follow the non-ibuf read threads,
				and the last are the non-ibuf write
				threads. */
	fil_node_t**message1,	/*!< out: the messages passed with the */
	void**	message2,	/*!< aio request; note that in case the
				aio operation failed, these output
				parameters are valid and can be used to
				restart the operation. */
	ulint*	type)		/*!< out: OS_FILE_WRITE or ..._READ */
{
	ulint		segment;
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	ulint		n;
	ulint		i;
	bool		ret = false;

	/* Should never be doing Sync IO here. */
	ut_a(global_seg != ULINT_UNDEFINED);

	/* Find the array and the local segment. */
	segment = os_aio_get_array_and_local_segment(&array, global_seg);
	n = array->n_slots / array->n_segments;

wait_for_event:
	/* Loop until we have found a completed request. */
	for (;;) {
		bool	any_reserved = false;
		mutex_enter(&array->mutex);
		for (i = 0; i < n; ++i) {
			slot = os_aio_array_get_nth_slot(
				array, i + segment * n);
			if (!slot->is_reserved) {
				continue;
			} else if (slot->io_already_done) {
				/* Something for us to work on. */
				goto found;
			} else {
				any_reserved = true;
			}
		}

		mutex_exit(&array->mutex);

		/* There is no completed request.
		If there is no pending request at all,
		and the system is being shut down, exit. */
		if (UNIV_UNLIKELY
		    (!any_reserved
		     && srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
		     && !buf_page_cleaner_is_active )) {
			*message1 = NULL;
			*message2 = NULL;
			return(true);
		}

		/* Wait for some request. Note that we return
		from wait iff we have found a request. */

		srv_set_io_thread_op_info(global_seg,
			"waiting for completed aio requests");
		os_aio_linux_collect(array, segment, n);
	}

found:
	/* Note that it may be that there are more then one completed
	IO requests. We process them one at a time. We may have a case
	here to improve the performance slightly by dealing with all
	requests in one sweep. */
	srv_set_io_thread_op_info(global_seg,
				"processing completed aio requests");

	/* Ensure that we are scribbling only our segment. */
	ut_a(i < n);

	ut_ad(slot != NULL);
	ut_ad(slot->is_reserved);
	ut_ad(slot->io_already_done);

	*message1 = slot->message1;
	*message2 = slot->message2;

	*type = slot->type;

	if (slot->ret == 0 && slot->n_bytes == (long) slot->len) {

		ret = true;
	} else if ((slot->ret == 0) && (slot->n_bytes > 0)
		   &&  (slot->n_bytes < (long) slot->len)) {
		/* Partial read or write scenario */
		int submit_ret;
		struct iocb*    iocb;
		slot->buf = (byte*)slot->buf + slot->n_bytes;
		slot->offset = slot->offset + slot->n_bytes;
		slot->len = slot->len - slot->n_bytes;
		/* Resetting the bytes read/written */
		slot->n_bytes = 0;
		slot->io_already_done = false;
		iocb = &(slot->control);

		if (slot->type == OS_FILE_READ) {
			io_prep_pread(&slot->control, slot->file, slot->buf,
				      slot->len, (off_t) slot->offset);
		} else {
			ut_a(slot->type == OS_FILE_WRITE);
			io_prep_pwrite(&slot->control, slot->file, slot->buf,
				       slot->len, (off_t) slot->offset);
		}
		/* Resubmit an I/O request */
		submit_ret = io_submit(array->aio_ctx[segment], 1, &iocb);
		if (submit_ret < 0 ) {
			/* Aborting in case of submit failure */
			ib::fatal()
				<< "Native Linux AIO interface. io_submit()"
				" call failed when resubmitting a partial I/O"
				" request on the file " << slot->name << ".";
		} else {
			ret = false;
			mutex_exit(&array->mutex);
			goto wait_for_event;
		}
	} else {
		errno = -slot->ret;

		/* os_file_handle_error does tell us if we should retry
		this IO. As it stands now, we don't do this retry when
		reaping requests from a different context than
		the dispatcher. This non-retry logic is the same for
		windows and linux native AIO.
		We should probably look into this to transparently
		re-submit the IO. */
		os_file_handle_error(slot->name, "Linux aio");

		ret = false;
	}

	mutex_exit(&array->mutex);

	os_aio_array_free_slot(array, slot);

	return(ret);
}
#endif /* LINUX_NATIVE_AIO */

/**********************************************************************//**
Does simulated aio. This function should be called by an i/o-handler
thread.
@return true if the aio operation succeeded */
bool
os_aio_simulated_handle(
/*====================*/
	ulint	global_segment,	/*!< in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads */
	fil_node_t**message1,	/*!< out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2,
	ulint*	type)		/*!< out: OS_FILE_WRITE or ..._READ */
{
	os_aio_array_t*	array;
	ulint		segment;
	os_aio_slot_t*	consecutive_ios[OS_AIO_MERGE_N_CONSECUTIVE];
	ulint		n_consecutive;
	ulint		total_len;
	ulint		offs;
	os_offset_t	lowest_offset;
	ulint		biggest_age;
	ulint		age;
	byte*		combined_buf;
	byte*		combined_buf2;
	bool		ret;
	bool		any_reserved;
	ulint		n;
	os_aio_slot_t*	aio_slot;

	/* Fix compiler warning */
	*consecutive_ios = NULL;

	segment = os_aio_get_array_and_local_segment(&array, global_segment);

restart:
	/* NOTE! We only access constant fields in os_aio_array. Therefore
	we do not have to acquire the protecting mutex yet */

	srv_set_io_thread_op_info(global_segment,
				  "looking for i/o requests (a)");
	ut_ad(os_aio_validate_skip());
	ut_ad(segment < array->n_segments);

	n = array->n_slots / array->n_segments;

	/* Look through n slots after the segment * n'th slot */

	if (array == os_aio_read_array
	    && os_aio_recommend_sleep_for_read_threads) {

		/* Give other threads chance to add several i/os to the array
		at once. */

		goto recommended_sleep;
	}

	srv_set_io_thread_op_info(global_segment,
				  "looking for i/o requests (b)");

	/* Check if there is a slot for which the i/o has already been
	done */
	any_reserved = false;

	mutex_enter(&array->mutex);

	for (ulint i = 0; i < n; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (!slot->is_reserved) {
			continue;
		} else if (slot->io_already_done) {
			aio_slot = slot;
			ret = true;
			goto slot_io_done;
		} else {
			any_reserved = true;
		}
	}

	/* There is no completed request.
	If there is no pending request at all,
	and the system is being shut down, exit. */
	if (!any_reserved
	    && srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
	    && !buf_page_cleaner_is_active) {
		mutex_exit(&array->mutex);
		*message1 = NULL;
		*message2 = NULL;
		return(true);
	}

	n_consecutive = 0;

	/* If there are at least 2 seconds old requests, then pick the oldest
	one to prevent starvation. If several requests have the same age,
	then pick the one at the lowest offset. */

	biggest_age = 0;
	lowest_offset = IB_UINT64_MAX;

	for (ulint i = 0; i < n; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (slot->is_reserved) {

			age = (ulint) difftime(
				ut_time(), slot->reservation_time);

			if ((age >= 2 && age > biggest_age)
			    || (age >= 2 && age == biggest_age
				&& slot->offset < lowest_offset)) {

				/* Found an i/o request */
				consecutive_ios[0] = slot;

				n_consecutive = 1;

				biggest_age = age;
				lowest_offset = slot->offset;
			}
		}
	}

	if (n_consecutive == 0) {
		/* There were no old requests. Look for an i/o request at the
		lowest offset in the array (we ignore the high 32 bits of the
		offset in these heuristics) */

		lowest_offset = IB_UINT64_MAX;

		for (ulint i = 0; i < n; i++) {
			os_aio_slot_t*	slot;

			slot = os_aio_array_get_nth_slot(
				array, i + segment * n);

			if (slot->is_reserved && slot->offset < lowest_offset) {

				/* Found an i/o request */
				consecutive_ios[0] = slot;

				n_consecutive = 1;

				lowest_offset = slot->offset;
			}
		}
	}

	if (n_consecutive == 0) {

		/* No i/o requested at the moment */

		goto wait_for_io;
	}

	/* if n_consecutive != 0, then we have assigned
	something valid to consecutive_ios[0] */
	ut_ad(n_consecutive != 0);
	ut_ad(consecutive_ios[0] != NULL);

	aio_slot = consecutive_ios[0];

	/* Check if there are several consecutive blocks to read or write */

consecutive_loop:
	for (ulint i = 0; i < n; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (slot->is_reserved
		    && slot != aio_slot
		    && slot->offset == aio_slot->offset + aio_slot->len
		    && slot->type == aio_slot->type
		    && slot->file == aio_slot->file) {

			/* Found a consecutive i/o request */

			consecutive_ios[n_consecutive] = slot;
			n_consecutive++;

			aio_slot = slot;

			if (n_consecutive < OS_AIO_MERGE_N_CONSECUTIVE) {

				goto consecutive_loop;
			} else {
				break;
			}
		}
	}

	srv_set_io_thread_op_info(global_segment, "consecutive i/o requests");

	/* We have now collected n_consecutive i/o requests in the array;
	allocate a single buffer which can hold all data, and perform the
	i/o */

	total_len = 0;
	aio_slot = consecutive_ios[0];

	for (ulint i = 0; i < n_consecutive; i++) {
		total_len += consecutive_ios[i]->len;
	}

	if (n_consecutive == 1) {
		/* We can use the buffer of the i/o request */
		combined_buf = aio_slot->buf;
		combined_buf2 = NULL;
	} else {
		combined_buf2 = static_cast<byte*>(
			ut_malloc_nokey(total_len + UNIV_PAGE_SIZE));

		ut_a(combined_buf2);

		combined_buf = static_cast<byte*>(
			ut_align(combined_buf2, UNIV_PAGE_SIZE));
	}

	/* We release the array mutex for the time of the i/o: NOTE that
	this assumes that there is just one i/o-handler thread serving
	a single segment of slots! */

	mutex_exit(&array->mutex);

	if (aio_slot->type == OS_FILE_WRITE && n_consecutive > 1) {
		/* Copy the buffers to the combined buffer */
		offs = 0;

		for (ulint i = 0; i < n_consecutive; i++) {

			ut_memcpy(combined_buf + offs, consecutive_ios[i]->buf,
				  consecutive_ios[i]->len);

			offs += consecutive_ios[i]->len;
		}
	}

	srv_set_io_thread_op_info(global_segment, "doing file i/o");

	/* Do the i/o with ordinary, synchronous i/o functions: */
	if (aio_slot->type == OS_FILE_WRITE) {
		ret = os_file_write(
			aio_slot->name, aio_slot->file, combined_buf,
			aio_slot->offset, total_len);
	} else {
		ret = os_file_read(
			aio_slot->file, combined_buf,
			aio_slot->offset, total_len);
	}

	ut_a(ret);
	srv_set_io_thread_op_info(global_segment, "file i/o done");

	if (aio_slot->type == OS_FILE_READ && n_consecutive > 1) {
		/* Copy the combined buffer to individual buffers */
		offs = 0;

		for (ulint i = 0; i < n_consecutive; i++) {

			ut_memcpy(consecutive_ios[i]->buf, combined_buf + offs,
				  consecutive_ios[i]->len);
			offs += consecutive_ios[i]->len;
		}
	}

	if (combined_buf2) {
		ut_free(combined_buf2);
	}

	mutex_enter(&array->mutex);

	/* Mark the i/os done in slots */

	for (ulint i = 0; i < n_consecutive; i++) {
		consecutive_ios[i]->io_already_done = true;
	}

	/* We return the messages for the first slot now, and if there were
	several slots, the messages will be returned with subsequent calls
	of this function */

slot_io_done:

	ut_a(aio_slot->is_reserved);

	*message1 = aio_slot->message1;
	*message2 = aio_slot->message2;

	*type = aio_slot->type;

	mutex_exit(&array->mutex);

	os_aio_array_free_slot(array, aio_slot);

	return(ret);

wait_for_io:
	srv_set_io_thread_op_info(global_segment, "resetting wait event");

	/* We wait here until there again can be i/os in the segment
	of this thread */

	os_event_reset(os_aio_segment_wait_events[global_segment]);

	mutex_exit(&array->mutex);

recommended_sleep:
	srv_set_io_thread_op_info(global_segment, "waiting for i/o request");

	os_event_wait(os_aio_segment_wait_events[global_segment]);

	goto restart;
}

/**********************************************************************//**
Validates the consistency of an aio array.
@return true if ok */
static
bool
os_aio_array_validate(
/*==================*/
	os_aio_array_t*	array)	/*!< in: aio wait array */
{
	ulint		i;
	ulint		n_reserved	= 0;

	mutex_enter(&array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);

	for (i = 0; i < array->n_slots; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->is_reserved) {
			n_reserved++;
			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	mutex_exit(&array->mutex);

	return(true);
}

/**********************************************************************//**
Validates the consistency the aio system.
@return true if ok */
bool
os_aio_validate(void)
/*=================*/
{
	os_aio_array_validate(os_aio_read_array);

	if (os_aio_write_array != 0) {
		os_aio_array_validate(os_aio_write_array);
	}

	if (os_aio_ibuf_array != 0) {
		os_aio_array_validate(os_aio_ibuf_array);
	}

	if (os_aio_log_array != 0) {
		os_aio_array_validate(os_aio_log_array);
	}

	if (os_aio_sync_array != 0) {
		os_aio_array_validate(os_aio_sync_array);
	}

	return(true);
}

/**********************************************************************//**
Prints pending IO requests per segment of an aio array.
We probably don't need per segment statistics but they can help us
during development phase to see if the IO requests are being
distributed as expected. */
static
void
os_aio_print_segment_info(
/*======================*/
	FILE*		file,	/*!< in: file where to print */
	ulint*		n_seg,	/*!< in: pending IO array */
	os_aio_array_t*	array)	/*!< in: array to process */
{
	ulint	i;

	ut_ad(array);
	ut_ad(n_seg);
	ut_ad(array->n_segments > 0);

	if (array->n_segments == 1) {
		return;
	}

	fprintf(file, " [");
	for (i = 0; i < array->n_segments; i++) {
		if (i != 0) {
			fprintf(file, ", ");
		}

		fprintf(file, "%lu", n_seg[i]);
	}
	fprintf(file, "] ");
}

/**********************************************************************//**
Prints info about the aio array. */
void
os_aio_print_array(
/*==============*/
	FILE*		file,	/*!< in: file where to print */
	os_aio_array_t*	array)	/*!< in: aio array to print */
{
	ulint			n_reserved = 0;
	ulint			n_res_seg[SRV_MAX_N_IO_THREADS];

	mutex_enter(&array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);

	memset(n_res_seg, 0x0, sizeof(n_res_seg));

	for (ulint i = 0; i < array->n_slots; ++i) {
		os_aio_slot_t*	slot;
		ulint		seg_no;

		slot = os_aio_array_get_nth_slot(array, i);

		seg_no = (i * array->n_segments) / array->n_slots;

		if (slot->is_reserved) {
			++n_reserved;
			++n_res_seg[seg_no];

			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	fprintf(file, " %lu", (ulong) n_reserved);

	os_aio_print_segment_info(file, n_res_seg, array);

	mutex_exit(&array->mutex);
}

/**********************************************************************//**
Prints info of the aio arrays. */
void
os_aio_print(
/*=========*/
	FILE*	file)	/*!< in: file where to print */
{
	time_t		current_time;
	double		time_elapsed;
	double		avg_bytes_read;

	for (ulint i = 0; i < srv_n_file_io_threads; ++i) {
		fprintf(file, "I/O thread %lu state: %s (%s)",
			(ulong) i,
			srv_io_thread_op_info[i],
			srv_io_thread_function[i]);

#ifndef _WIN32
		if (os_event_is_set(os_aio_segment_wait_events[i])) {
			fprintf(file, " ev set");
		}
#endif /* _WIN32 */

		fprintf(file, "\n");
	}

	fputs("Pending normal aio reads:", file);

	os_aio_print_array(file, os_aio_read_array);

	if (os_aio_write_array != 0) {
		fputs(", aio writes:", file);
		os_aio_print_array(file, os_aio_write_array);
	}

	if (os_aio_ibuf_array != 0) {
		fputs(",\n ibuf aio reads:", file);
		os_aio_print_array(file, os_aio_ibuf_array);
	}

	if (os_aio_log_array != 0) {
		fputs(", log i/o's:", file);
		os_aio_print_array(file, os_aio_log_array);
	}

	if (os_aio_sync_array != 0) {
		fputs(", sync i/o's:", file);
		os_aio_print_array(file, os_aio_sync_array);
	}

	putc('\n', file);
	current_time = ut_time();
	time_elapsed = 0.001 + difftime(current_time, os_last_printout);

	fprintf(file,
		"Pending flushes (fsync) log: %lu; buffer pool: %lu\n"
		"%lu OS file reads, %lu OS file writes, %lu OS fsyncs\n",
		(ulong) fil_n_pending_log_flushes,
		(ulong) fil_n_pending_tablespace_flushes,
		(ulong) os_n_file_reads,
		(ulong) os_n_file_writes,
		(ulong) os_n_fsyncs);

	if (os_file_n_pending_preads != 0 || os_file_n_pending_pwrites != 0) {
		fprintf(file,
			"%lu pending preads, %lu pending pwrites\n",
			(ulong) os_file_n_pending_preads,
			(ulong) os_file_n_pending_pwrites);
	}

	if (os_n_file_reads == os_n_file_reads_old) {
		avg_bytes_read = 0.0;
	} else {
		avg_bytes_read = (double) os_bytes_read_since_printout
			/ (os_n_file_reads - os_n_file_reads_old);
	}

	fprintf(file,
		"%.2f reads/s, %lu avg bytes/read,"
		" %.2f writes/s, %.2f fsyncs/s\n",
		(os_n_file_reads - os_n_file_reads_old)
		/ time_elapsed,
		(ulong) avg_bytes_read,
		(os_n_file_writes - os_n_file_writes_old)
		/ time_elapsed,
		(os_n_fsyncs - os_n_fsyncs_old)
		/ time_elapsed);

	os_n_file_reads_old = os_n_file_reads;
	os_n_file_writes_old = os_n_file_writes;
	os_n_fsyncs_old = os_n_fsyncs;
	os_bytes_read_since_printout = 0;

	os_last_printout = current_time;
}

/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
void
os_aio_refresh_stats(void)
/*======================*/
{
	os_n_file_reads_old = os_n_file_reads;
	os_n_file_writes_old = os_n_file_writes;
	os_n_fsyncs_old = os_n_fsyncs;
	os_bytes_read_since_printout = 0;

	os_last_printout = time(NULL);
}

/**********************************************************************//**
Checks that all slots in the system have been freed, that is, there are
no pending io operations.
@return true if all free */
bool
os_aio_all_slots_free(void)
/*=======================*/
{
	os_aio_array_t*	array;
	ulint		n_res	= 0;

	/* Read Array */
	array = os_aio_read_array;

	mutex_enter(&array->mutex);

	n_res += array->n_reserved;

	mutex_exit(&array->mutex);

	/* Write Array */
	array = os_aio_write_array;

	mutex_enter(&array->mutex);

	n_res += array->n_reserved;

	mutex_exit(&array->mutex);

	/* IBuf and Log Array */
	if (!srv_read_only_mode) {
		array = os_aio_ibuf_array;

		mutex_enter(&array->mutex);

		n_res += array->n_reserved;

		mutex_exit(&array->mutex);

		array = os_aio_log_array;

		mutex_enter(&array->mutex);

		n_res += array->n_reserved;

		mutex_exit(&array->mutex);
	}

	array = os_aio_sync_array;

	mutex_enter(&array->mutex);

	n_res += array->n_reserved;

	mutex_exit(&array->mutex);

	if (n_res == 0) {

		return(true);
	}

	return(false);
}

#ifdef UNIV_DEBUG
/** Prints all pending IO for the array
@param[in]	file	file where to print
@param[in]	array	array to process */
static
void
os_aio_print_pending_io_array(
	FILE*		file,
	os_aio_array_t*	array)
{
	mutex_enter(&array->mutex);
	fprintf(file, " %lu\n", (ulong) array->n_reserved);
	for (ulint i = 0; i < array->n_slots; i++) {
		os_aio_slot_t* slot = array->slots + i;
		if (slot->is_reserved) {
			fprintf(file,
				"%s IO for %s (offset=" UINT64PF
				", size=%lu)\n",
				slot->type == OS_FILE_READ ? "read" : "write",
				slot->name, slot->offset, slot->len);
		}
	}
	mutex_exit(&array->mutex);
}

/** Prints all pending IO
@param[in]	file	file where to print */
void
os_aio_print_pending_io(
	FILE*	file)
{
	fputs("Pending normal aio reads:", file);
	os_aio_print_pending_io_array(file, os_aio_read_array);

	if (os_aio_write_array != 0) {
		fputs("Pending normal aio writes:", file);
		os_aio_print_pending_io_array(file, os_aio_write_array);
	}

	if (os_aio_ibuf_array != 0) {
		fputs("Pending ibuf aio reads:", file);
		os_aio_print_pending_io_array(file, os_aio_ibuf_array);
	}

	if (os_aio_log_array != 0) {
		fputs("Pending log i/o's:", file);
		os_aio_print_pending_io_array(file, os_aio_log_array);
	}

	if (os_aio_sync_array != 0) {
		fputs("Pending sync i/o's:", file);
		os_aio_print_pending_io_array(file, os_aio_sync_array);
	}
}
#endif /* UNIV_DEBUG */

#ifdef _WIN32
/** Normalizes a directory path for Windows: converts '/' to '\'.
@param[in,out] str A null-terminated Windows directory and file path */
void
os_normalize_path_for_win(
	char*	str __attribute__((unused)))
{
	for (; *str; str++) {
		if (*str == '/') {
			*str = '\\';
		}
	}
}
#endif

#endif /* !UNIV_HOTBACKUP */
