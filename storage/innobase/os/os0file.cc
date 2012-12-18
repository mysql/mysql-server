/***********************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.
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

#include "os0file.h"

#ifdef UNIV_NONINL
#include "os0file.ic"
#endif

#include "ut0mem.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "fil0fil.h"
#include "buf0buf.h"
#include "srv0mon.h"
#ifndef UNIV_HOTBACKUP
# include "os0sync.h"
# include "os0thread.h"
#else /* !UNIV_HOTBACKUP */
# ifdef __WIN__
/* Add includes for the _stat() call to compile on Windows */
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <errno.h>
# endif /* __WIN__ */
#endif /* !UNIV_HOTBACKUP */

#if defined(LINUX_NATIVE_AIO)
#include <libaio.h>
#endif

/** Insert buffer segment id */
static const ulint IO_IBUF_SEGMENT = 0;

/** Log segment id */
static const ulint IO_LOG_SEGMENT = 1;

/* This specifies the file permissions InnoDB uses when it creates files in
Unix; the value of os_innodb_umask is initialized in ha_innodb.cc to
my_umask */

#ifndef __WIN__
/** Umask for creating files */
UNIV_INTERN ulint	os_innodb_umask = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
#else
/** Umask for creating files */
UNIV_INTERN ulint	os_innodb_umask	= 0;
#endif /* __WIN__ */

#ifndef UNIV_HOTBACKUP
/* We use these mutexes to protect lseek + file i/o operation, if the
OS does not provide an atomic pread or pwrite, or similar */
#define OS_FILE_N_SEEK_MUTEXES	16
UNIV_INTERN os_ib_mutex_t	os_file_seek_mutexes[OS_FILE_N_SEEK_MUTEXES];

/* In simulated aio, merge at most this many consecutive i/os */
#define OS_AIO_MERGE_N_CONSECUTIVE	64

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
is set to TRUE we follow the code path of native AIO, otherwise we
do simulated AIO.
There are innodb_file_io_threads helper threads. These threads work
on the four arrays mentioned above in Simulated AIO.
If a synchronous IO request is made, it is handled by calling
os_file_write/os_file_read.
If an AIO request is made the calling thread not only queues it in the
array but also submits the requests. The helper thread then collects
the completed IO request and calls completion routine on it.

**********************************************************************/

/** Flag: enable debug printout for asynchronous i/o */
UNIV_INTERN ibool	os_aio_print_debug	= FALSE;

#ifdef UNIV_PFS_IO
/* Keys to register InnoDB I/O with performance schema */
UNIV_INTERN mysql_pfs_key_t  innodb_file_data_key;
UNIV_INTERN mysql_pfs_key_t  innodb_file_log_key;
UNIV_INTERN mysql_pfs_key_t  innodb_file_temp_key;
#endif /* UNIV_PFS_IO */

/** The asynchronous i/o array slot structure */
struct os_aio_slot_t{
	ibool		is_read;	/*!< TRUE if a read operation */
	ulint		pos;		/*!< index of the slot in the aio
					array */
	ibool		reserved;	/*!< TRUE if this slot is reserved */
	time_t		reservation_time;/*!< time when reserved */
	ulint		len;		/*!< length of the block to read or
					write */
	byte*		buf;		/*!< buffer used in i/o */
	ulint		type;		/*!< OS_FILE_READ or OS_FILE_WRITE */
	os_offset_t	offset;		/*!< file offset in bytes */
	os_file_t	file;		/*!< file where to read or write */
	const char*	name;		/*!< file name or path */
	ibool		io_already_done;/*!< used only in simulated aio:
					TRUE if the physical i/o already
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
	os_ib_mutex_t	mutex;	/*!< the mutex protecting the aio array */
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
#ifdef __WIN__
	HANDLE*		handles;
				/*!< Pointer to an array of OS native
				event handles where we copied the
				handles from slots, in the same
				order. This can be used in
				WaitForMultipleObjects; used only in
				Windows */
#endif /* __WIN__ */

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

/** If the following is TRUE, read i/o handler threads try to
wait until a batch of new read requests have been posted */
static ibool	os_aio_recommend_sleep_for_read_threads	= FALSE;
#endif /* !UNIV_HOTBACKUP */

UNIV_INTERN ulint	os_n_file_reads		= 0;
UNIV_INTERN ulint	os_bytes_read_since_printout = 0;
UNIV_INTERN ulint	os_n_file_writes	= 0;
UNIV_INTERN ulint	os_n_fsyncs		= 0;
UNIV_INTERN ulint	os_n_file_reads_old	= 0;
UNIV_INTERN ulint	os_n_file_writes_old	= 0;
UNIV_INTERN ulint	os_n_fsyncs_old		= 0;
UNIV_INTERN time_t	os_last_printout;

UNIV_INTERN ibool	os_has_said_disk_full	= FALSE;

#if !defined(UNIV_HOTBACKUP)	\
    && (!defined(HAVE_ATOMIC_BUILTINS) || UNIV_WORD_SIZE < 8)
/** The mutex protecting the following counts of pending I/O operations */
static os_ib_mutex_t	os_file_count_mutex;
#endif /* !UNIV_HOTBACKUP && (!HAVE_ATOMIC_BUILTINS || UNIV_WORD_SIZE < 8) */

/** Number of pending os_file_pread() operations */
UNIV_INTERN ulint	os_file_n_pending_preads  = 0;
/** Number of pending os_file_pwrite() operations */
UNIV_INTERN ulint	os_file_n_pending_pwrites = 0;
/** Number of pending write operations */
UNIV_INTERN ulint	os_n_pending_writes = 0;
/** Number of pending read operations */
UNIV_INTERN ulint	os_n_pending_reads = 0;

#ifdef UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Validates the consistency the aio system some of the time.
@return	TRUE if ok or the check was skipped */
UNIV_INTERN
ibool
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
		return(TRUE);
	}

	os_aio_validate_count = OS_AIO_VALIDATE_SKIP;
	return(os_aio_validate());
}
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_DEBUG */

#ifdef __WIN__
/***********************************************************************//**
Gets the operating system version. Currently works only on Windows.
@return	OS_WIN95, OS_WIN31, OS_WINNT, OS_WIN2000, OS_WINXP, OS_WINVISTA,
OS_WIN7. */
UNIV_INTERN
ulint
os_get_os_version(void)
/*===================*/
{
	OSVERSIONINFO	os_info;

	os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	ut_a(GetVersionEx(&os_info));

	if (os_info.dwPlatformId == VER_PLATFORM_WIN32s) {
		return(OS_WIN31);
	} else if (os_info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		return(OS_WIN95);
	} else if (os_info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		switch (os_info.dwMajorVersion) {
		case 3:
		case 4:
			return(OS_WINNT);
		case 5:
			return (os_info.dwMinorVersion == 0)
				? OS_WIN2000 : OS_WINXP;
		case 6:
			return (os_info.dwMinorVersion == 0)
				? OS_WINVISTA : OS_WIN7;
		default:
			return(OS_WIN7);
		}
	} else {
		ut_error;
		return(0);
	}
}
#endif /* __WIN__ */

/***********************************************************************//**
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@return	error number, or OS error number + 100 */
static
ulint
os_file_get_last_error_low(
/*=======================*/
	bool	report_all_errors,	/*!< in: TRUE if we want an error
					message printed of all errors */
	bool	on_error_silent)	/*!< in: TRUE then don't print any
					diagnostic to the log */
{
#ifdef __WIN__

	ulint	err = (ulint) GetLastError();
	if (err == ERROR_SUCCESS) {
		return(0);
	}

	if (report_all_errors
	    || (!on_error_silent
		&& err != ERROR_DISK_FULL
		&& err != ERROR_FILE_EXISTS)) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Operating system error number %lu"
			" in a file operation.\n", (ulong) err);

		if (err == ERROR_PATH_NOT_FOUND) {
			fprintf(stderr,
				"InnoDB: The error means the system"
				" cannot find the path specified.\n");

			if (srv_is_being_started) {
				fprintf(stderr,
					"InnoDB: If you are installing InnoDB,"
					" remember that you must create\n"
					"InnoDB: directories yourself, InnoDB"
					" does not create them.\n");
			}
		} else if (err == ERROR_ACCESS_DENIED) {
			fprintf(stderr,
				"InnoDB: The error means mysqld does not have"
				" the access rights to\n"
				"InnoDB: the directory. It may also be"
				" you have created a subdirectory\n"
				"InnoDB: of the same name as a data file.\n");
		} else if (err == ERROR_SHARING_VIOLATION
			   || err == ERROR_LOCK_VIOLATION) {
			fprintf(stderr,
				"InnoDB: The error means that another program"
				" is using InnoDB's files.\n"
				"InnoDB: This might be a backup or antivirus"
				" software or another instance\n"
				"InnoDB: of MySQL."
				" Please close it to get rid of this error.\n");
		} else if (err == ERROR_WORKING_SET_QUOTA
			   || err == ERROR_NO_SYSTEM_RESOURCES) {
			fprintf(stderr,
				"InnoDB: The error means that there are no"
				" sufficient system resources or quota to"
				" complete the operation.\n");
		} else if (err == ERROR_OPERATION_ABORTED) {
			fprintf(stderr,
				"InnoDB: The error means that the I/O"
				" operation has been aborted\n"
				"InnoDB: because of either a thread exit"
				" or an application request.\n"
				"InnoDB: Retry attempt is made.\n");
		} else {
			fprintf(stderr,
				"InnoDB: Some operating system error numbers"
				" are described at\n"
				"InnoDB: "
				REFMAN
				"operating-system-error-codes.html\n");
		}
	}

	fflush(stderr);

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
	} else {
		return(100 + err);
	}
#else
	int err = errno;
	if (err == 0) {
		return(0);
	}

	if (report_all_errors
	    || (err != ENOSPC && err != EEXIST && !on_error_silent)) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Operating system error number %d"
			" in a file operation.\n", err);

		if (err == ENOENT) {
			fprintf(stderr,
				"InnoDB: The error means the system"
				" cannot find the path specified.\n");

			if (srv_is_being_started) {
				fprintf(stderr,
					"InnoDB: If you are installing InnoDB,"
					" remember that you must create\n"
					"InnoDB: directories yourself, InnoDB"
					" does not create them.\n");
			}
		} else if (err == EACCES) {
			fprintf(stderr,
				"InnoDB: The error means mysqld does not have"
				" the access rights to\n"
				"InnoDB: the directory.\n");
		} else {
			if (strerror(err) != NULL) {
				fprintf(stderr,
					"InnoDB: Error number %d"
					" means '%s'.\n",
					err, strerror(err));
			}


			fprintf(stderr,
				"InnoDB: Some operating system"
				" error numbers are described at\n"
				"InnoDB: "
				REFMAN
				"operating-system-error-codes.html\n");
		}
	}

	fflush(stderr);

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
	}
	return(100 + err);
#endif
}

/***********************************************************************//**
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@return	error number, or OS error number + 100 */
UNIV_INTERN
ulint
os_file_get_last_error(
/*===================*/
	bool	report_all_errors)	/*!< in: TRUE if we want an error
					message printed of all errors */
{
	return(os_file_get_last_error_low(report_all_errors, false));
}

/****************************************************************//**
Does error handling when a file operation fails.
Conditionally exits (calling exit(3)) based on should_exit value and the
error type, if should_exit is TRUE then on_error_silent is ignored.
@return	TRUE if we should retry the operation */
static
ibool
os_file_handle_error_cond_exit(
/*===========================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation,	/*!< in: operation */
	ibool		should_exit,	/*!< in: call exit(3) if unknown error
					and this parameter is TRUE */
	ibool		on_error_silent)/*!< in: if TRUE then don't print
					any message to the log iff it is
					an unknown non-fatal error */
{
	ulint	err;

	err = os_file_get_last_error_low(false, on_error_silent);

	switch (err) {
	case OS_FILE_DISK_FULL:
		/* We only print a warning about disk full once */

		if (os_has_said_disk_full) {

			return(FALSE);
		}

		/* Disk full error is reported irrespective of the
		on_error_silent setting. */

		if (name) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Encountered a problem with"
				" file %s\n", name);
		}

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Disk is full. Try to clean the disk"
			" to free space.\n");

		os_has_said_disk_full = TRUE;

		fflush(stderr);

		return(FALSE);

	case OS_FILE_AIO_RESOURCES_RESERVED:
	case OS_FILE_AIO_INTERRUPTED:

		return(TRUE);

	case OS_FILE_PATH_ERROR:
	case OS_FILE_ALREADY_EXISTS:

		return(FALSE);

	case OS_FILE_SHARING_VIOLATION:

		os_thread_sleep(10000000);  /* 10 sec */
		return(TRUE);

	case OS_FILE_OPERATION_ABORTED:
	case OS_FILE_INSUFFICIENT_RESOURCE:

		os_thread_sleep(100000);	/* 100 ms */
		return(TRUE);

	default:

		/* If it is an operation that can crash on error then it
		is better to ignore on_error_silent and print an error message
		to the log. */

		if (should_exit || !on_error_silent) {
			if (name) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: File name %s\n", name);
			}

			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: File operation call: "
				"'%s' returned OS error " ULINTPF ".\n",
				operation, err);
		}

		if (should_exit) {
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: Cannot continue "
				"operation.\n");

			fflush(stderr);

			ut_ad(0);  /* Report call stack, etc only in debug code. */
			exit(1);
		}
	}

	return(FALSE);
}

/****************************************************************//**
Does error handling when a file operation fails.
@return	TRUE if we should retry the operation */
static
ibool
os_file_handle_error(
/*=================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation)	/*!< in: operation */
{
	/* exit in case of unknown error */
	return(os_file_handle_error_cond_exit(name, operation, TRUE, FALSE));
}

/****************************************************************//**
Does error handling when a file operation fails.
@return	TRUE if we should retry the operation */
static
ibool
os_file_handle_error_no_exit(
/*=========================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation,	/*!< in: operation */
	ibool		on_error_silent)/*!< in: if TRUE then don't print
					any message to the log. */
{
	/* don't exit in case of unknown error */
	return(os_file_handle_error_cond_exit(
			name, operation, FALSE, on_error_silent));
}

#undef USE_FILE_LOCK
#define USE_FILE_LOCK
#if defined(UNIV_HOTBACKUP) || defined(__WIN__)
/* InnoDB Hot Backup does not lock the data files.
 * On Windows, mandatory locking is used.
 */
# undef USE_FILE_LOCK
#endif
#ifdef USE_FILE_LOCK
/****************************************************************//**
Obtain an exclusive lock on a file.
@return	0 on success */
static
int
os_file_lock(
/*=========*/
	int		fd,	/*!< in: file descriptor */
	const char*	name)	/*!< in: file name */
{
	struct flock lk;

	ut_ad(!srv_read_only_mode);

	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = lk.l_len = 0;

	if (fcntl(fd, F_SETLK, &lk) == -1) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unable to lock %s, error: %d", name, errno);

		if (errno == EAGAIN || errno == EACCES) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Check that you do not already have "
				"another mysqld process using the "
				"same InnoDB data or log files.");
		}

		return(-1);
	}

	return(0);
}
#endif /* USE_FILE_LOCK */

#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Creates the seek mutexes used in positioned reads and writes. */
UNIV_INTERN
void
os_io_init_simple(void)
/*===================*/
{
#if !defined(HAVE_ATOMIC_BUILTINS) || UNIV_WORD_SIZE < 8
	os_file_count_mutex = os_mutex_create();
#endif /* !HAVE_ATOMIC_BUILTINS || UNIV_WORD_SIZE < 8 */

	for (ulint i = 0; i < OS_FILE_N_SEEK_MUTEXES; i++) {
		os_file_seek_mutexes[i] = os_mutex_create();
	}
}

/***********************************************************************//**
Creates a temporary file.  This function is like tmpfile(3), but
the temporary file is created in the MySQL temporary directory.
@return	temporary file handle, or NULL on error */
UNIV_INTERN
FILE*
os_file_create_tmpfile(void)
/*========================*/
{
	FILE*	file	= NULL;
	int	fd	= innobase_mysql_tmpfile();

	ut_ad(!srv_read_only_mode);

	if (fd >= 0) {
		file = fdopen(fd, "w+b");
	}

	if (!file) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: unable to create temporary file;"
			" errno: %d\n", errno);
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
@return	directory stream, NULL if error */
UNIV_INTERN
os_file_dir_t
os_file_opendir(
/*============*/
	const char*	dirname,	/*!< in: directory name; it must not
					contain a trailing '\' or '/' */
	ibool		error_is_fatal)	/*!< in: TRUE if we should treat an
					error as a fatal error; if we try to
					open symlinks then we do not wish a
					fatal error if it happens not to be
					a directory */
{
	os_file_dir_t		dir;
#ifdef __WIN__
	LPWIN32_FIND_DATA	lpFindFileData;
	char			path[OS_FILE_MAX_PATH + 3];

	ut_a(strlen(dirname) < OS_FILE_MAX_PATH);

	strcpy(path, dirname);
	strcpy(path + strlen(path), "\\*");

	/* Note that in Windows opening the 'directory stream' also retrieves
	the first entry in the directory. Since it is '.', that is no problem,
	as we will skip over the '.' and '..' entries anyway. */

	lpFindFileData = static_cast<LPWIN32_FIND_DATA>(
		ut_malloc(sizeof(WIN32_FIND_DATA)));

	dir = FindFirstFile((LPCTSTR) path, lpFindFileData);

	ut_free(lpFindFileData);

	if (dir == INVALID_HANDLE_VALUE) {

		if (error_is_fatal) {
			os_file_handle_error(dirname, "opendir");
		}

		return(NULL);
	}

	return(dir);
#else
	dir = opendir(dirname);

	if (dir == NULL && error_is_fatal) {
		os_file_handle_error(dirname, "opendir");
	}

	return(dir);
#endif /* __WIN__ */
}

/***********************************************************************//**
Closes a directory stream.
@return	0 if success, -1 if failure */
UNIV_INTERN
int
os_file_closedir(
/*=============*/
	os_file_dir_t	dir)	/*!< in: directory stream */
{
#ifdef __WIN__
	BOOL		ret;

	ret = FindClose(dir);

	if (!ret) {
		os_file_handle_error_no_exit(NULL, "closedir", FALSE);

		return(-1);
	}

	return(0);
#else
	int	ret;

	ret = closedir(dir);

	if (ret) {
		os_file_handle_error_no_exit(NULL, "closedir", FALSE);
	}

	return(ret);
#endif /* __WIN__ */
}

/***********************************************************************//**
This function returns information of the next file in the directory. We jump
over the '.' and '..' entries in the directory.
@return	0 if ok, -1 if error, 1 if at the end of the directory */
UNIV_INTERN
int
os_file_readdir_next_file(
/*======================*/
	const char*	dirname,/*!< in: directory name or path */
	os_file_dir_t	dir,	/*!< in: directory stream */
	os_file_stat_t*	info)	/*!< in/out: buffer where the info is returned */
{
#ifdef __WIN__
	LPWIN32_FIND_DATA	lpFindFileData;
	BOOL			ret;

	lpFindFileData = static_cast<LPWIN32_FIND_DATA>(
		ut_malloc(sizeof(WIN32_FIND_DATA)));
next_file:
	ret = FindNextFile(dir, lpFindFileData);

	if (ret) {
		ut_a(strlen((char*) lpFindFileData->cFileName)
		     < OS_FILE_MAX_PATH);

		if (strcmp((char*) lpFindFileData->cFileName, ".") == 0
		    || strcmp((char*) lpFindFileData->cFileName, "..") == 0) {

			goto next_file;
		}

		strcpy(info->name, (char*) lpFindFileData->cFileName);

		info->size = (ib_int64_t)(lpFindFileData->nFileSizeLow)
			+ (((ib_int64_t)(lpFindFileData->nFileSizeHigh))
			   << 32);

		if (lpFindFileData->dwFileAttributes
		    & FILE_ATTRIBUTE_REPARSE_POINT) {
			/* TODO: test Windows symlinks */
			/* TODO: MySQL has apparently its own symlink
			implementation in Windows, dbname.sym can
			redirect a database directory:
			REFMAN "windows-symbolic-links.html" */
			info->type = OS_FILE_TYPE_LINK;
		} else if (lpFindFileData->dwFileAttributes
			   & FILE_ATTRIBUTE_DIRECTORY) {
			info->type = OS_FILE_TYPE_DIR;
		} else {
			/* It is probably safest to assume that all other
			file types are normal. Better to check them rather
			than blindly skip them. */

			info->type = OS_FILE_TYPE_FILE;
		}
	}

	ut_free(lpFindFileData);

	if (ret) {
		return(0);
	} else if (GetLastError() == ERROR_NO_MORE_FILES) {

		return(1);
	} else {
		os_file_handle_error_no_exit(NULL, "readdir_next_file", FALSE);
		return(-1);
	}
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

	if (ret != 0
#ifdef UNIV_AIX
	    /* On AIX, only if we got non-NULL 'ent' (result) value and
	    a non-zero 'ret' (return) value, it indicates a failed
	    readdir_r() call. An NULL 'ent' with an non-zero 'ret'
	    would indicate the "end of the directory" is reached. */
	    && ent != NULL
#endif
	   ) {
		fprintf(stderr,
			"InnoDB: cannot read directory %s, error %lu\n",
			dirname, (ulong) ret);

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
		ut_malloc(strlen(dirname) + strlen(ent->d_name) + 10));

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

		os_file_handle_error_no_exit(full_path, "stat", FALSE);

		ut_free(full_path);

		return(-1);
	}

	info->size = (ib_int64_t) statinfo.st_size;

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
but reports the error and returns FALSE.
@return	TRUE if call succeeds, FALSE on error */
UNIV_INTERN
ibool
os_file_create_directory(
/*=====================*/
	const char*	pathname,	/*!< in: directory name as
					null-terminated string */
	ibool		fail_if_exists)	/*!< in: if TRUE, pre-existing directory
					is treated as an error. */
{
#ifdef __WIN__
	BOOL	rcode;

	rcode = CreateDirectory((LPCTSTR) pathname, NULL);
	if (!(rcode != 0
	      || (GetLastError() == ERROR_ALREADY_EXISTS
		  && !fail_if_exists))) {

		os_file_handle_error_no_exit(
			pathname, "CreateDirectory", FALSE);

		return(FALSE);
	}

	return(TRUE);
#else
	int	rcode;

	rcode = mkdir(pathname, 0770);

	if (!(rcode == 0 || (errno == EEXIST && !fail_if_exists))) {
		/* failure */
		os_file_handle_error_no_exit(pathname, "mkdir", FALSE);

		return(FALSE);
	}

	return (TRUE);
#endif /* __WIN__ */
}

/****************************************************************//**
NOTE! Use the corresponding macro os_file_create_simple(), not directly
this function!
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INTERN
os_file_t
os_file_create_simple_func(
/*=======================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: create mode */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY or
				OS_FILE_READ_WRITE */
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
	os_file_t	file;
	ibool		retry;

#ifdef __WIN__
	DWORD		access;
	DWORD		create_flag;
	DWORD		attributes	= 0;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		create_flag = OPEN_EXISTING;

	} else if (srv_read_only_mode) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else if (create_mode == OS_FILE_CREATE_PATH) {

		ut_a(!srv_read_only_mode);

		/* Create subdirs along the path if needed  */
		*success = os_file_create_subdirs_if_needed(name);

		if (!*success) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Unable to create subdirectories '%s'",
				name);

			return((os_file_t) -1);
		}

		create_flag = CREATE_NEW;
		create_mode = OS_FILE_CREATE;

	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file create mode (%lu) for file '%s'",
			create_mode, name);

		return((os_file_t) -1);
	}

	if (access_type == OS_FILE_READ_ONLY) {
		access = GENERIC_READ;
	} else if (srv_read_only_mode) {

		ib_logf(IB_LOG_LEVEL_INFO,
			"read only mode set. Unable to "
			"open file '%s' in RW mode, trying RO mode", name);

		access = GENERIC_READ;

	} else if (access_type == OS_FILE_READ_WRITE) {
		access = GENERIC_READ | GENERIC_WRITE;
	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file access type (%lu) for file '%s'",
			access_type, name);

		return((os_file_t) -1);
	}

	do {
		/* Use default security attributes and no template file. */

		file = CreateFile(
			(LPCTSTR) name, access, FILE_SHARE_READ, NULL,
			create_flag, attributes, NULL);

		if (file == INVALID_HANDLE_VALUE) {

			*success = FALSE;

			retry = os_file_handle_error(
				name, create_mode == OS_FILE_OPEN ?
				"open" : "create");

		} else {
			*success = TRUE;
			retry = false;
		}

	} while (retry);

#else /* __WIN__ */
	int		create_flag;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		if (access_type == OS_FILE_READ_ONLY) {
			create_flag = O_RDONLY;
		} else if (srv_read_only_mode) {
			create_flag = O_RDONLY;
		} else {
			create_flag = O_RDWR;
		}

	} else if (srv_read_only_mode) {

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else if (create_mode == OS_FILE_CREATE_PATH) {

		/* Create subdirs along the path if needed  */

		*success = os_file_create_subdirs_if_needed(name);

		if (!*success) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Unable to create subdirectories '%s'",
				name);

			return((os_file_t) -1);
		}

		create_flag = O_RDWR | O_CREAT | O_EXCL;
		create_mode = OS_FILE_CREATE;
	} else {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file create mode (%lu) for file '%s'",
			create_mode, name);

		return((os_file_t) -1);
	}

	do {
		file = ::open(name, create_flag, os_innodb_umask);

		if (file == -1) {
			*success = FALSE;

			retry = os_file_handle_error(
				name,
				create_mode == OS_FILE_OPEN
				?  "open" : "create");
		} else {
			*success = TRUE;
			retry = false;
		}

	} while (retry);

#ifdef USE_FILE_LOCK
	if (!srv_read_only_mode
	    && *success
	    && access_type == OS_FILE_READ_WRITE
	    && os_file_lock(file, name)) {

		*success = FALSE;
		close(file);
		file = -1;
	}
#endif /* USE_FILE_LOCK */

#endif /* __WIN__ */

	return(file);
}

/****************************************************************//**
NOTE! Use the corresponding macro
os_file_create_simple_no_error_handling(), not directly this function!
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INTERN
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
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
	os_file_t	file;

#ifdef __WIN__
	DWORD		access;
	DWORD		create_flag;
	DWORD		attributes	= 0;
	DWORD		share_mode	= FILE_SHARE_READ;

	ut_a(name);

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {
		create_flag = OPEN_EXISTING;
	} else if (srv_read_only_mode) {
		create_flag = OPEN_EXISTING;
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = CREATE_NEW;
	} else {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file create mode (%lu) for file '%s'",
			create_mode, name);

		return((os_file_t) -1);
	}

	if (access_type == OS_FILE_READ_ONLY) {
		access = GENERIC_READ;
	} else if (srv_read_only_mode) {
		access = GENERIC_READ;
	} else if (access_type == OS_FILE_READ_WRITE) {
		access = GENERIC_READ | GENERIC_WRITE;
	} else if (access_type == OS_FILE_READ_ALLOW_DELETE) {

		ut_a(!srv_read_only_mode);

		access = GENERIC_READ;

		/*!< A backup program has to give mysqld the maximum
		freedom to do what it likes with the file */

		share_mode |= FILE_SHARE_DELETE | FILE_SHARE_WRITE;
	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file access type (%lu) for file '%s'",
			access_type, name);

		return((os_file_t) -1);
	}

	file = CreateFile((LPCTSTR) name,
			  access,
			  share_mode,
			  NULL,			// Security attributes
			  create_flag,
			  attributes,
			  NULL);		// No template file

	*success = (file != INVALID_HANDLE_VALUE);
#else /* __WIN__ */
	int		create_flag;

	ut_a(name);

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		if (access_type == OS_FILE_READ_ONLY) {

			create_flag = O_RDONLY;

		} else if (srv_read_only_mode) {

			create_flag = O_RDONLY;

		} else {

			ut_a(access_type == OS_FILE_READ_WRITE
			     || access_type == OS_FILE_READ_ALLOW_DELETE);

			create_flag = O_RDWR;
		}

	} else if (srv_read_only_mode) {

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file create mode (%lu) for file '%s'",
			create_mode, name);

		return((os_file_t) -1);
	}

	file = ::open(name, create_flag, os_innodb_umask);

	*success = file == -1 ? FALSE : TRUE;

#ifdef USE_FILE_LOCK
	if (!srv_read_only_mode
	    && *success
	    && access_type == OS_FILE_READ_WRITE
	    && os_file_lock(file, name)) {

		*success = FALSE;
		close(file);
		file = -1;

	}
#endif /* USE_FILE_LOCK */

#endif /* __WIN__ */

	return(file);
}

/****************************************************************//**
Tries to disable OS caching on an opened file descriptor. */
UNIV_INTERN
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

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Failed to set DIRECTIO_ON on file %s: %s: %s, "
			"continuing anyway.",
			file_name, operation_name, strerror(errno_save));
	}
#elif defined(O_DIRECT)
	if (fcntl(fd, F_SETFL, O_DIRECT) == -1) {
		int	errno_save = errno;

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Failed to set O_DIRECT on file %s: %s: %s, "
			"continuing anyway",
			file_name, operation_name, strerror(errno_save));

		if (errno_save == EINVAL) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"O_DIRECT is known to result in 'Invalid "
				"argument' on Linux on tmpfs, see MySQL "
				"Bug#26662");
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
UNIV_INTERN
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
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
	os_file_t	file;
	ibool		retry;
	ibool		on_error_no_exit;
	ibool		on_error_silent;

#ifdef __WIN__
	DBUG_EXECUTE_IF(
		"ib_create_table_fail_disk_full",
		*success = FALSE;
		SetLastError(ERROR_DISK_FULL);
		return((os_file_t) -1);
	);
#else /* __WIN__ */
	DBUG_EXECUTE_IF(
		"ib_create_table_fail_disk_full",
		*success = FALSE;
		errno = ENOSPC;
		return((os_file_t) -1);
	);
#endif /* __WIN__ */

#ifdef __WIN__
	DWORD		create_flag;
	DWORD		share_mode	= FILE_SHARE_READ;

	on_error_no_exit = create_mode & OS_FILE_ON_ERROR_NO_EXIT
		? TRUE : FALSE;

	on_error_silent = create_mode & OS_FILE_ON_ERROR_SILENT
		? TRUE : FALSE;

	create_mode &= ~OS_FILE_ON_ERROR_NO_EXIT;
	create_mode &= ~OS_FILE_ON_ERROR_SILENT;

	if (create_mode == OS_FILE_OPEN_RAW) {

		ut_a(!srv_read_only_mode);

		create_flag = OPEN_EXISTING;

		/* On Windows Physical devices require admin privileges and
		have to have the write-share mode set. See the remarks
		section for the CreateFile() function documentation in MSDN. */

		share_mode |= FILE_SHARE_WRITE;

	} else if (create_mode == OS_FILE_OPEN
		   || create_mode == OS_FILE_OPEN_RETRY) {

		create_flag = OPEN_EXISTING;

	} else if (srv_read_only_mode) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else if (create_mode == OS_FILE_OVERWRITE) {

		create_flag = CREATE_ALWAYS;

	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file create mode (%lu) for file '%s'",
			create_mode, name);

		return((os_file_t) -1);
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
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown purpose flag (%lu) while opening file '%s'",
			purpose, name);

		return((os_file_t)(-1));
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

	if (!srv_read_only_mode) {
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
				     && !srv_read_only_mode)
				? "create" : "open";

			*success = FALSE;

			if (on_error_no_exit) {
				retry = os_file_handle_error_no_exit(
					name, operation, on_error_silent);
			} else {
				retry = os_file_handle_error(name, operation);
			}
		} else {
			*success = TRUE;
			retry = FALSE;
		}

	} while (retry);

#else /* __WIN__ */
	int		create_flag;
	const char*	mode_str	= NULL;

	on_error_no_exit = create_mode & OS_FILE_ON_ERROR_NO_EXIT
		? TRUE : FALSE;
	on_error_silent = create_mode & OS_FILE_ON_ERROR_SILENT
		? TRUE : FALSE;

	create_mode &= ~OS_FILE_ON_ERROR_NO_EXIT;
	create_mode &= ~OS_FILE_ON_ERROR_SILENT;

	if (create_mode == OS_FILE_OPEN
	    || create_mode == OS_FILE_OPEN_RAW
	    || create_mode == OS_FILE_OPEN_RETRY) {

		mode_str = "OPEN";

		create_flag = srv_read_only_mode ? O_RDONLY : O_RDWR;

	} else if (srv_read_only_mode) {

		mode_str = "OPEN";

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		mode_str = "CREATE";
		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else if (create_mode == OS_FILE_OVERWRITE) {

		mode_str = "OVERWRITE";
		create_flag = O_RDWR | O_CREAT | O_TRUNC;

	} else {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Unknown file create mode (%lu) for file '%s'",
			create_mode, name);

		return((os_file_t) -1);
	}

	ut_a(type == OS_LOG_FILE || type == OS_DATA_FILE);
	ut_a(purpose == OS_FILE_AIO || purpose == OS_FILE_NORMAL);

#ifdef O_SYNC
	/* We let O_SYNC only affect log files; note that we map O_DSYNC to
	O_SYNC because the datasync options seemed to corrupt files in 2001
	in both Linux and Solaris */

	if (!srv_read_only_mode
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
				     && !srv_read_only_mode)
				? "create" : "open";

			*success = FALSE;

			if (on_error_no_exit) {
				retry = os_file_handle_error_no_exit(
					name, operation, on_error_silent);
			} else {
				retry = os_file_handle_error(name, operation);
			}
		} else {
			*success = TRUE;
			retry = false;
		}

	} while (retry);

	/* We disable OS caching (O_DIRECT) only on data files */

	if (!srv_read_only_mode
	    && *success
	    && type != OS_LOG_FILE
	    && (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT
		|| srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC)) {

		os_file_set_nocache(file, name, mode_str);
	}

#ifdef USE_FILE_LOCK
	if (!srv_read_only_mode
	    && *success
	    && create_mode != OS_FILE_OPEN_RAW
	    && os_file_lock(file, name)) {

		if (create_mode == OS_FILE_OPEN_RETRY) {

			ut_a(!srv_read_only_mode);

			ib_logf(IB_LOG_LEVEL_INFO,
				"Retrying to lock the first data file");

			for (int i = 0; i < 100; i++) {
				os_thread_sleep(1000000);

				if (!os_file_lock(file, name)) {
					*success = TRUE;
					return(file);
				}
			}

			ib_logf(IB_LOG_LEVEL_INFO,
				"Unable to open the first data file");
		}

		*success = FALSE;
		close(file);
		file = -1;
	}
#endif /* USE_FILE_LOCK */

#endif /* __WIN__ */

	return(file);
}

/***********************************************************************//**
Deletes a file if it exists. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
bool
os_file_delete_if_exists(
/*=====================*/
	const char*	name)	/*!< in: file path as a null-terminated
				string */
{
#ifdef __WIN__
	bool	ret;
	ulint	count	= 0;
loop:
	/* In Windows, deleting an .ibd file may fail if ibbackup is copying
	it */

	ret = DeleteFile((LPCTSTR) name);

	if (ret) {
		return(true);
	}

	DWORD lasterr = GetLastError();
	if (lasterr == ERROR_FILE_NOT_FOUND
	    || lasterr == ERROR_PATH_NOT_FOUND) {
		/* the file does not exist, this not an error */

		return(true);
	}

	count++;

	if (count > 100 && 0 == (count % 10)) {
		os_file_get_last_error(true); /* print error information */

		ib_logf(IB_LOG_LEVEL_WARN, "Delete of file %s failed.", name);
	}

	os_thread_sleep(1000000);	/* sleep for a second */

	if (count > 2000) {

		return(false);
	}

	goto loop;
#else
	int	ret;

	ret = unlink(name);

	if (ret != 0 && errno != ENOENT) {
		os_file_handle_error_no_exit(name, "delete", FALSE);

		return(false);
	}

	return(true);
#endif /* __WIN__ */
}

/***********************************************************************//**
Deletes a file. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
bool
os_file_delete(
/*===========*/
	const char*	name)	/*!< in: file path as a null-terminated
				string */
{
#ifdef __WIN__
	BOOL	ret;
	ulint	count	= 0;
loop:
	/* In Windows, deleting an .ibd file may fail if ibbackup is copying
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

		fprintf(stderr,
			"InnoDB: Warning: cannot delete file %s\n"
			"InnoDB: Are you running ibbackup"
			" to back up the file?\n", name);
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
		os_file_handle_error_no_exit(name, "delete", FALSE);

		return(false);
	}

	return(true);
#endif
}

/***********************************************************************//**
NOTE! Use the corresponding macro os_file_rename(), not directly this function!
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_rename_func(
/*================*/
	const char*	oldpath,/*!< in: old file path as a null-terminated
				string */
	const char*	newpath)/*!< in: new file path */
{
#ifdef UNIV_DEBUG
	os_file_type_t	type;
	ibool		exists;

	/* New path must not exist. */
	ut_ad(os_file_status(newpath, &exists, &type));
	ut_ad(!exists);

	/* Old path must exist. */
	ut_ad(os_file_status(oldpath, &exists, &type));
	ut_ad(exists);
#endif /* UNIV_DEBUG */

#ifdef __WIN__
	BOOL	ret;

	ret = MoveFile((LPCTSTR) oldpath, (LPCTSTR) newpath);

	if (ret) {
		return(TRUE);
	}

	os_file_handle_error_no_exit(oldpath, "rename", FALSE);

	return(FALSE);
#else
	int	ret;

	ret = rename(oldpath, newpath);

	if (ret != 0) {
		os_file_handle_error_no_exit(oldpath, "rename", FALSE);

		return(FALSE);
	}

	return(TRUE);
#endif /* __WIN__ */
}

/***********************************************************************//**
NOTE! Use the corresponding macro os_file_close(), not directly this function!
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_close_func(
/*===============*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef __WIN__
	BOOL	ret;

	ut_a(file);

	ret = CloseHandle(file);

	if (ret) {
		return(TRUE);
	}

	os_file_handle_error(NULL, "close");

	return(FALSE);
#else
	int	ret;

	ret = close(file);

	if (ret == -1) {
		os_file_handle_error(NULL, "close");

		return(FALSE);
	}

	return(TRUE);
#endif /* __WIN__ */
}

#ifdef UNIV_HOTBACKUP
/***********************************************************************//**
Closes a file handle.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_close_no_error_handling(
/*============================*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef __WIN__
	BOOL	ret;

	ut_a(file);

	ret = CloseHandle(file);

	if (ret) {
		return(TRUE);
	}

	return(FALSE);
#else
	int	ret;

	ret = close(file);

	if (ret == -1) {

		return(FALSE);
	}

	return(TRUE);
#endif /* __WIN__ */
}
#endif /* UNIV_HOTBACKUP */

/***********************************************************************//**
Gets a file size.
@return	file size, or (os_offset_t) -1 on failure */
UNIV_INTERN
os_offset_t
os_file_get_size(
/*=============*/
	os_file_t	file)	/*!< in: handle to a file */
{
#ifdef __WIN__
	os_offset_t	offset;
	DWORD		high;
	DWORD		low;

	low = GetFileSize(file, &high);

	if ((low == 0xFFFFFFFF) && (GetLastError() != NO_ERROR)) {
		return((os_offset_t) -1);
	}

	offset = (os_offset_t) low | ((os_offset_t) high << 32);

	return(offset);
#else
	return((os_offset_t) lseek(file, 0, SEEK_END));
#endif /* __WIN__ */
}

/***********************************************************************//**
Write the specified number of zeros to a newly created file.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_set_size(
/*=============*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	os_offset_t	size)	/*!< in: file size */
{
	os_offset_t	current_size;
	ibool		ret;
	byte*		buf;
	byte*		buf2;
	ulint		buf_size;

	current_size = 0;

	/* Write up to 1 megabyte at a time. */
	buf_size = ut_min(64, (ulint) (size / UNIV_PAGE_SIZE))
		* UNIV_PAGE_SIZE;
	buf2 = static_cast<byte*>(ut_malloc(buf_size + UNIV_PAGE_SIZE));

	/* Align the buffer for possible raw i/o */
	buf = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

	/* Write buffer full of zeros */
	memset(buf, 0, buf_size);

	if (size >= (os_offset_t) 100 << 20) {

		fprintf(stderr, "InnoDB: Progress in MB:");
	}

	while (current_size < size) {
		ulint	n_bytes;

		if (size - current_size < (os_offset_t) buf_size) {
			n_bytes = (ulint) (size - current_size);
		} else {
			n_bytes = buf_size;
		}

		ret = os_file_write(name, file, buf, current_size, n_bytes);
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
		return(TRUE);
	}

error_handling:
	return(FALSE);
}

/***********************************************************************//**
Truncates a file at its current position.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_set_eof(
/*============*/
	FILE*		file)	/*!< in: file to be truncated */
{
#ifdef __WIN__
	HANDLE h = (HANDLE) _get_osfhandle(fileno(file));
	return(SetEndOfFile(h));
#else /* __WIN__ */
	return(!ftruncate(fileno(file), ftell(file)));
#endif /* __WIN__ */
}

#ifndef __WIN__
/***********************************************************************//**
Wrapper to fsync(2) that retries the call on some errors.
Returns the value 0 if successful; otherwise the value -1 is returned and
the global variable errno is set to indicate the error.
@return	0 if success, -1 otherwise */

static
int
os_file_fsync(
/*==========*/
	os_file_t	file)	/*!< in: handle to a file */
{
	int	ret;
	int	failures;
	ibool	retry;

	failures = 0;

	do {
		ret = fsync(file);

		os_n_fsyncs++;

		if (ret == -1 && errno == ENOLCK) {

			if (failures % 100 == 0) {

				ut_print_timestamp(stderr);
				fprintf(stderr,
					" InnoDB: fsync(): "
					"No locks available; retrying\n");
			}

			os_thread_sleep(200000 /* 0.2 sec */);

			failures++;

			retry = TRUE;
		} else {

			retry = FALSE;
		}
	} while (retry);

	return(ret);
}
#endif /* !__WIN__ */

/***********************************************************************//**
NOTE! Use the corresponding macro os_file_flush(), not directly this function!
Flushes the write buffers of a given file to the disk.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_flush_func(
/*===============*/
	os_file_t	file)	/*!< in, own: handle to a file */
{
#ifdef __WIN__
	BOOL	ret;

	ut_a(file);

	os_n_fsyncs++;

	ret = FlushFileBuffers(file);

	if (ret) {
		return(TRUE);
	}

	/* Since Windows returns ERROR_INVALID_FUNCTION if the 'file' is
	actually a raw device, we choose to ignore that error if we are using
	raw disks */

	if (srv_start_raw_disk_in_use && GetLastError()
	    == ERROR_INVALID_FUNCTION) {
		return(TRUE);
	}

	os_file_handle_error(NULL, "flush");

	/* It is a fatal error if a file flush does not succeed, because then
	the database can get corrupt on disk */
	ut_error;

	return(FALSE);
#else
	int	ret;

#if defined(HAVE_DARWIN_THREADS)
# ifndef F_FULLFSYNC
	/* The following definition is from the Mac OS X 10.3 <sys/fcntl.h> */
#  define F_FULLFSYNC 51 /* fsync + ask the drive to flush to the media */
# elif F_FULLFSYNC != 51
#  error "F_FULLFSYNC != 51: ABI incompatibility with Mac OS X 10.3"
# endif
	/* Apple has disabled fsync() for internal disk drives in OS X. That
	caused corruption for a user when he tested a power outage. Let us in
	OS X use a nonstandard flush method recommended by an Apple
	engineer. */

	if (!srv_have_fullfsync) {
		/* If we are not on an operating system that supports this,
		then fall back to a plain fsync. */

		ret = os_file_fsync(file);
	} else {
		ret = fcntl(file, F_FULLFSYNC, NULL);

		if (ret) {
			/* If we are not on a file system that supports this,
			then fall back to a plain fsync. */
			ret = os_file_fsync(file);
		}
	}
#else
	ret = os_file_fsync(file);
#endif

	if (ret == 0) {
		return(TRUE);
	}

	/* Since Linux returns EINVAL if the 'file' is actually a raw device,
	we choose to ignore that error if we are using raw disks */

	if (srv_start_raw_disk_in_use && errno == EINVAL) {

		return(TRUE);
	}

	ib_logf(IB_LOG_LEVEL_ERROR, "The OS said file flush did not succeed");

	os_file_handle_error(NULL, "flush");

	/* It is a fatal error if a file flush does not succeed, because then
	the database can get corrupt on disk */
	ut_error;

	return(FALSE);
#endif
}

#ifndef __WIN__
/*******************************************************************//**
Does a synchronous read operation in Posix.
@return	number of bytes read, -1 if error */
static __attribute__((nonnull, warn_unused_result))
ssize_t
os_file_pread(
/*==========*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	ulint		n,	/*!< in: number of bytes to read */
	os_offset_t	offset)	/*!< in: file offset from where to read */
{
	off_t	offs;
#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	ssize_t	n_bytes;
#endif /* HAVE_PREAD && !HAVE_BROKEN_PREAD */

	ut_ad(n);

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */
	offs = (off_t) offset;

	if (sizeof(off_t) <= 4) {
		if (offset != (os_offset_t) offs) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"File read at offset > 4 GB");
		}
	}

	os_n_file_reads++;

#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
#if defined(HAVE_ATOMIC_BUILTINS) && UNIV_WORD_SIZE == 8
	(void) os_atomic_increment_ulint(&os_n_pending_reads, 1);
	(void) os_atomic_increment_ulint(&os_file_n_pending_preads, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_READS);
#else
	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_preads++;
	os_n_pending_reads++;
	MONITOR_INC(MONITOR_OS_PENDING_READS);
	os_mutex_exit(os_file_count_mutex);
#endif /* HAVE_ATOMIC_BUILTINS && UNIV_WORD == 8 */

	n_bytes = pread(file, buf, n, offs);

#if defined(HAVE_ATOMIC_BUILTINS) && UNIV_WORD_SIZE == 8
	(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
	(void) os_atomic_decrement_ulint(&os_file_n_pending_preads, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);
#else
	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_preads--;
	os_n_pending_reads--;
	MONITOR_DEC(MONITOR_OS_PENDING_READS);
	os_mutex_exit(os_file_count_mutex);
#endif /* !HAVE_ATOMIC_BUILTINS || UNIV_WORD == 8 */

	return(n_bytes);
#else
	{
		off_t	ret_offset;
		ssize_t	ret;
#ifndef UNIV_HOTBACKUP
		ulint	i;
#endif /* !UNIV_HOTBACKUP */

#if defined(HAVE_ATOMIC_BUILTINS) && UNIV_WORD_SIZE == 8
		(void) os_atomic_increment_ulint(&os_n_pending_reads, 1);
		MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_READS);
#else
		os_mutex_enter(os_file_count_mutex);
		os_n_pending_reads++;
		MONITOR_INC(MONITOR_OS_PENDING_READS);
		os_mutex_exit(os_file_count_mutex);
#endif /* HAVE_ATOMIC_BUILTINS && UNIV_WORD == 8 */
#ifndef UNIV_HOTBACKUP
		/* Protect the seek / read operation with a mutex */
		i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

		os_mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		ret_offset = lseek(file, offs, SEEK_SET);

		if (ret_offset < 0) {
			ret = -1;
		} else {
			ret = read(file, buf, (ssize_t) n);
		}

#ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

#if defined(HAVE_ATOMIC_BUILTINS) && UNIV_WORD_SIZE == 8
		(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
		MONITOR_ATOIC_DEC(MONITOR_OS_PENDING_READS);
#else
		os_mutex_enter(os_file_count_mutex);
		os_n_pending_reads--;
		MONITOR_DEC(MONITOR_OS_PENDING_READS);
		os_mutex_exit(os_file_count_mutex);
#endif /* HAVE_ATOMIC_BUILTINS && UNIV_WORD_SIZE == 8 */

		return(ret);
	}
#endif
}

/*******************************************************************//**
Does a synchronous write operation in Posix.
@return	number of bytes written, -1 if error */
static __attribute__((nonnull, warn_unused_result))
ssize_t
os_file_pwrite(
/*===========*/
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from where to write */
	ulint		n,	/*!< in: number of bytes to write */
	os_offset_t	offset)	/*!< in: file offset where to write */
{
	ssize_t	ret;
	off_t	offs;

	ut_ad(n);
	ut_ad(!srv_read_only_mode);

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */
	offs = (off_t) offset;

	if (sizeof(off_t) <= 4) {
		if (offset != (os_offset_t) offs) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"File write at offset > 4 GB.");
		}
	}

	os_n_file_writes++;

#if defined(HAVE_PWRITE) && !defined(HAVE_BROKEN_PREAD)
#if !defined(HAVE_ATOMIC_BUILTINS) || UNIV_WORD_SIZE < 8
	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_pwrites++;
	os_n_pending_writes++;
	MONITOR_INC(MONITOR_OS_PENDING_WRITES);
	os_mutex_exit(os_file_count_mutex);
#else
	(void) os_atomic_increment_ulint(&os_n_pending_writes, 1);
	(void) os_atomic_increment_ulint(&os_file_n_pending_pwrites, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_WRITES);
#endif /* !HAVE_ATOMIC_BUILTINS || UNIV_WORD < 8 */

	ret = pwrite(file, buf, (ssize_t) n, offs);

#if !defined(HAVE_ATOMIC_BUILTINS) || UNIV_WORD_SIZE < 8
	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_pwrites--;
	os_n_pending_writes--;
	MONITOR_DEC(MONITOR_OS_PENDING_WRITES);
	os_mutex_exit(os_file_count_mutex);
#else
	(void) os_atomic_decrement_ulint(&os_n_pending_writes, 1);
	(void) os_atomic_decrement_ulint(&os_file_n_pending_pwrites, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_WRITES);
#endif /* !HAVE_ATOMIC_BUILTINS || UNIV_WORD < 8 */

	return(ret);
#else
	{
		off_t	ret_offset;
# ifndef UNIV_HOTBACKUP
		ulint	i;
# endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_writes++;
		MONITOR_INC(MONITOR_OS_PENDING_WRITES);
		os_mutex_exit(os_file_count_mutex);

# ifndef UNIV_HOTBACKUP
		/* Protect the seek / write operation with a mutex */
		i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

		os_mutex_enter(os_file_seek_mutexes[i]);
# endif /* UNIV_HOTBACKUP */

		ret_offset = lseek(file, offs, SEEK_SET);

		if (ret_offset < 0) {
			ret = -1;

			goto func_exit;
		}

		ret = write(file, buf, (ssize_t) n);

func_exit:
# ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
# endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_writes--;
		MONITOR_DEC(MONITOR_OS_PENDING_WRITES);
		os_mutex_exit(os_file_count_mutex);

		return(ret);
	}
#endif /* !UNIV_HOTBACKUP */
}
#endif

/*******************************************************************//**
NOTE! Use the corresponding macro os_file_read(), not directly this
function!
Requests a synchronous positioned read operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
os_file_read_func(
/*==============*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n)	/*!< in: number of bytes to read */
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		low;
	DWORD		high;
	ibool		retry;
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

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads++;
	MONITOR_INC(MONITOR_OS_PENDING_READS);
	os_mutex_exit(os_file_count_mutex);

#ifndef UNIV_HOTBACKUP
	/* Protect the seek / read operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

	os_mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	ret2 = SetFilePointer(
		file, low, reinterpret_cast<PLONG>(&high), FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {

#ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_reads--;
		MONITOR_DEC(MONITOR_OS_PENDING_READS);
		os_mutex_exit(os_file_count_mutex);

		goto error_handling;
	}

	ret = ReadFile(file, buf, (DWORD) n, &len, NULL);

#ifndef UNIV_HOTBACKUP
	os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads--;
	MONITOR_DEC(MONITOR_OS_PENDING_READS);
	os_mutex_exit(os_file_count_mutex);

	if (ret && len == n) {
		return(TRUE);
	}
#else /* __WIN__ */
	ibool	retry;
	ssize_t	ret;

	os_bytes_read_since_printout += n;

try_again:
	ret = os_file_pread(file, buf, n, offset);

	if ((ulint) ret == n) {

		return(TRUE);
	}

	ib_logf(IB_LOG_LEVEL_ERROR,
		"Tried to read "ULINTPF" bytes at offset " UINT64PF". "
		"Was only able to read %ld.", n, offset, (lint) ret);
#endif /* __WIN__ */
#ifdef __WIN__
error_handling:
#endif
	retry = os_file_handle_error(NULL, "read");

	if (retry) {
		goto try_again;
	}

	fprintf(stderr,
		"InnoDB: Fatal error: cannot read from file."
		" OS error number %lu.\n",
#ifdef __WIN__
		(ulong) GetLastError()
#else
		(ulong) errno
#endif /* __WIN__ */
		);
	fflush(stderr);

	ut_error;

	return(FALSE);
}

/*******************************************************************//**
NOTE! Use the corresponding macro os_file_read_no_error_handling(),
not directly this function!
Requests a synchronous positioned read operation. This function does not do
any error handling. In case of error it returns FALSE.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
os_file_read_no_error_handling_func(
/*================================*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n)	/*!< in: number of bytes to read */
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		low;
	DWORD		high;
	ibool		retry;
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

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads++;
	MONITOR_INC(MONITOR_OS_PENDING_READS);
	os_mutex_exit(os_file_count_mutex);

#ifndef UNIV_HOTBACKUP
	/* Protect the seek / read operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

	os_mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	ret2 = SetFilePointer(
		file, low, reinterpret_cast<PLONG>(&high), FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {

#ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_reads--;
		MONITOR_DEC(MONITOR_OS_PENDING_READS);
		os_mutex_exit(os_file_count_mutex);

		goto error_handling;
	}

	ret = ReadFile(file, buf, (DWORD) n, &len, NULL);

#ifndef UNIV_HOTBACKUP
	os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads--;
	MONITOR_DEC(MONITOR_OS_PENDING_READS);
	os_mutex_exit(os_file_count_mutex);

	if (ret && len == n) {
		return(TRUE);
	}
#else /* __WIN__ */
	ibool	retry;
	ssize_t	ret;

	os_bytes_read_since_printout += n;

try_again:
	ret = os_file_pread(file, buf, n, offset);

	if ((ulint) ret == n) {

		return(TRUE);
	}
#endif /* __WIN__ */
#ifdef __WIN__
error_handling:
#endif
	retry = os_file_handle_error_no_exit(NULL, "read", FALSE);

	if (retry) {
		goto try_again;
	}

	return(FALSE);
}

/*******************************************************************//**
Rewind file to its start, read at most size - 1 bytes from it to str, and
NUL-terminate str. All errors are silently ignored. This function is
mostly meant to be used with temporary files. */
UNIV_INTERN
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
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
os_file_write_func(
/*===============*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from which to write */
	os_offset_t	offset,	/*!< in: file offset where to write */
	ulint		n)	/*!< in: number of bytes to write */
{
	ut_ad(!srv_read_only_mode);

#ifdef __WIN__
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

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_writes++;
	MONITOR_INC(MONITOR_OS_PENDING_WRITES);
	os_mutex_exit(os_file_count_mutex);

#ifndef UNIV_HOTBACKUP
	/* Protect the seek / write operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

	os_mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	ret2 = SetFilePointer(
		file, low, reinterpret_cast<PLONG>(&high), FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {

#ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_writes--;
		MONITOR_DEC(MONITOR_OS_PENDING_WRITES);
		os_mutex_exit(os_file_count_mutex);

		ut_print_timestamp(stderr);

		fprintf(stderr,
			" InnoDB: Error: File pointer positioning to"
			" file %s failed at\n"
			"InnoDB: offset %llu. Operating system"
			" error number %lu.\n"
			"InnoDB: Some operating system error numbers"
			" are described at\n"
			"InnoDB: "
			REFMAN "operating-system-error-codes.html\n",
			name, offset, (ulong) GetLastError());

		return(FALSE);
	}

	ret = WriteFile(file, buf, (DWORD) n, &len, NULL);

#ifndef UNIV_HOTBACKUP
	os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_writes--;
	MONITOR_DEC(MONITOR_OS_PENDING_WRITES);
	os_mutex_exit(os_file_count_mutex);

	if (ret && len == n) {

		return(TRUE);
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

		ut_print_timestamp(stderr);

		fprintf(stderr,
			" InnoDB: Error: Write to file %s failed"
			" at offset %llu.\n"
			"InnoDB: %lu bytes should have been written,"
			" only %lu were written.\n"
			"InnoDB: Operating system error number %lu.\n"
			"InnoDB: Check that your OS and file system"
			" support files of this size.\n"
			"InnoDB: Check also that the disk is not full"
			" or a disk quota exceeded.\n",
			name, offset,
			(ulong) n, (ulong) len, (ulong) err);

		if (strerror((int) err) != NULL) {
			fprintf(stderr,
				"InnoDB: Error number %lu means '%s'.\n",
				(ulong) err, strerror((int) err));
		}

		fprintf(stderr,
			"InnoDB: Some operating system error numbers"
			" are described at\n"
			"InnoDB: "
			REFMAN "operating-system-error-codes.html\n");

		os_has_said_disk_full = TRUE;
	}

	return(FALSE);
#else
	ssize_t	ret;

	ret = os_file_pwrite(file, buf, n, offset);

	if ((ulint) ret == n) {

		return(TRUE);
	}

	if (!os_has_said_disk_full) {

		ut_print_timestamp(stderr);

		fprintf(stderr,
			" InnoDB: Error: Write to file %s failed"
			" at offset "UINT64PF".\n"
			"InnoDB: %lu bytes should have been written,"
			" only %ld were written.\n"
			"InnoDB: Operating system error number %lu.\n"
			"InnoDB: Check that your OS and file system"
			" support files of this size.\n"
			"InnoDB: Check also that the disk is not full"
			" or a disk quota exceeded.\n",
			name, offset, n, (lint) ret,
			(ulint) errno);
		if (strerror(errno) != NULL) {
			fprintf(stderr,
				"InnoDB: Error number %d means '%s'.\n",
				errno, strerror(errno));
		}

		fprintf(stderr,
			"InnoDB: Some operating system error numbers"
			" are described at\n"
			"InnoDB: "
			REFMAN "operating-system-error-codes.html\n");

		os_has_said_disk_full = TRUE;
	}

	return(FALSE);
#endif
}

/*******************************************************************//**
Check the existence and type of the given file.
@return	TRUE if call succeeded */
UNIV_INTERN
ibool
os_file_status(
/*===========*/
	const char*	path,	/*!< in: pathname of the file */
	ibool*		exists,	/*!< out: TRUE if file exists */
	os_file_type_t* type)	/*!< out: type of the file (if it exists) */
{
#ifdef __WIN__
	int		ret;
	struct _stat64	statinfo;

	ret = _stat64(path, &statinfo);
	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */
		*exists = FALSE;
		return(TRUE);
	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", FALSE);

		return(FALSE);
	}

	if (_S_IFDIR & statinfo.st_mode) {
		*type = OS_FILE_TYPE_DIR;
	} else if (_S_IFREG & statinfo.st_mode) {
		*type = OS_FILE_TYPE_FILE;
	} else {
		*type = OS_FILE_TYPE_UNKNOWN;
	}

	*exists = TRUE;

	return(TRUE);
#else
	int		ret;
	struct stat	statinfo;

	ret = stat(path, &statinfo);
	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */
		*exists = FALSE;
		return(TRUE);
	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", FALSE);

		return(FALSE);
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

	*exists = TRUE;

	return(TRUE);
#endif
}

/*******************************************************************//**
This function returns information about the specified file
@return	DB_SUCCESS if all OK */
UNIV_INTERN
dberr_t
os_file_get_status(
/*===============*/
	const char*	path,		/*!< in:	pathname of the file */
	os_file_stat_t* stat_info,	/*!< information of a file in a
					directory */
	bool		check_rw_perm)	/*!< in: for testing whether the
					file can be opened in RW mode */
{
	int		ret;

#ifdef __WIN__
	struct _stat64	statinfo;

	ret = _stat64(path, &statinfo);

	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(DB_NOT_FOUND);

	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", FALSE);

		return(DB_FAIL);

	} else if (_S_IFDIR & statinfo.st_mode) {
		stat_info->type = OS_FILE_TYPE_DIR;
	} else if (_S_IFREG & statinfo.st_mode) {

		DWORD	access = GENERIC_READ;

		if (!srv_read_only_mode) {
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

		os_file_handle_error_no_exit(path, "stat", FALSE);

		return(DB_FAIL);

	} else if (S_ISDIR(statinfo.st_mode)) {
		stat_info->type = OS_FILE_TYPE_DIR;
	} else if (S_ISLNK(statinfo.st_mode)) {
		stat_info->type = OS_FILE_TYPE_LINK;
	} else if (S_ISREG(statinfo.st_mode)) {
		stat_info->type = OS_FILE_TYPE_FILE;

		if (check_rw_perm) {
			int	fh;
			int	access;

			access = !srv_read_only_mode ? O_RDWR : O_RDONLY;

			fh = ::open(path, access, os_innodb_umask);

			if (fh == -1) {
				stat_info->rw_perm = false;
			} else {
				stat_info->rw_perm = true;
				close(fh);
			}
		}
	} else {
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}

#endif /* _WIN_ */

	stat_info->ctime = statinfo.st_ctime;
	stat_info->atime = statinfo.st_atime;
	stat_info->mtime = statinfo.st_mtime;
	stat_info->size  = statinfo.st_size;

	return(DB_SUCCESS);
}

/* path name separator character */
#ifdef __WIN__
#  define OS_FILE_PATH_SEPARATOR	'\\'
#else
#  define OS_FILE_PATH_SEPARATOR	'/'
#endif

/****************************************************************//**
This function returns a new path name after replacing the basename
in an old path with a new basename.  The old_path is a full path
name including the extension.  The tablename is in the normal
form "databasename/tablename".  The new base name is found after
the forward slash.  Both input strings are null terminated.

This function allocates memory to be returned.  It is the callers
responsibility to free the return value after it is no longer needed.

@return	own: new full pathname */
UNIV_INTERN
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
	last_slash = strrchr((char*) old_path, OS_FILE_PATH_SEPARATOR);
	dir_len = last_slash ? last_slash - old_path : strlen(old_path);

	/* allocate a new path and move the old directory path to it. */
	new_path_len = dir_len + strlen(base_name) + sizeof "/.ibd";
	new_path = static_cast<char*>(mem_alloc(new_path_len));
	memcpy(new_path, old_path, dir_len);

	ut_snprintf(new_path + dir_len,
		    new_path_len - dir_len,
		    "%c%s.ibd",
		    OS_FILE_PATH_SEPARATOR,
		    base_name);

	return(new_path);
}

/****************************************************************//**
This function returns a remote path name by combining a data directory
path provided in a DATA DIRECTORY clause with the tablename which is
in the form 'database/tablename'.  It strips the file basename (which
is the tablename) found after the last directory in the path provided.
The full filepath created will include the database name as a directory
under the path provided.  The filename is the tablename with the '.ibd'
extension. All input and output strings are null-terminated.

This function allocates memory to be returned.  It is the callers
responsibility to free the return value after it is no longer needed.

@return	own: A full pathname; data_dir_path/databasename/tablename.ibd */
UNIV_INTERN
char*
os_file_make_remote_pathname(
/*=========================*/
	const char*	data_dir_path,	/*!< in: pathname */
	const char*	tablename,	/*!< in: tablename */
	const char*	extention)	/*!< in: file extention; ibd,cfg */
{
	ulint		data_dir_len;
	char*		last_slash;
	char*		new_path;
	ulint		new_path_len;

	ut_ad(extention && strlen(extention) == 3);

	/* Find the offset of the last slash. We will strip off the
	old basename or tablename which starts after that slash. */
	last_slash = strrchr((char*) data_dir_path, OS_FILE_PATH_SEPARATOR);
	data_dir_len = last_slash ? last_slash - data_dir_path : strlen(data_dir_path);

	/* allocate a new path and move the old directory path to it. */
	new_path_len = data_dir_len + strlen(tablename)
		       + sizeof "/." + strlen(extention);
	new_path = static_cast<char*>(mem_alloc(new_path_len));
	memcpy(new_path, data_dir_path, data_dir_len);
	ut_snprintf(new_path + data_dir_len,
		    new_path_len - data_dir_len,
		    "%c%s.%s",
		    OS_FILE_PATH_SEPARATOR,
		    tablename,
		    extention);

	srv_normalize_path_for_win(new_path);

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
UNIV_INTERN
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
	ptr = strrchr((char*) data_dir_path, OS_FILE_PATH_SEPARATOR);
	if (!ptr) {
		return;
	}
	ptr[0] = '\0';
	tablename = ptr + 1;

	/* The databasename starts after the next to last slash. */
	ptr = strrchr((char*) data_dir_path, OS_FILE_PATH_SEPARATOR);
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

@return	own: directory component of the pathname */
UNIV_INTERN
char*
os_file_dirname(
/*============*/
	const char*	path)	/*!< in: pathname */
{
	/* Find the offset of the last slash */
	const char* last_slash = strrchr(path, OS_FILE_PATH_SEPARATOR);
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
@return	TRUE if call succeeded FALSE otherwise */
UNIV_INTERN
ibool
os_file_create_subdirs_if_needed(
/*=============================*/
	const char*	path)	/*!< in: path name */
{
	if (srv_read_only_mode) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"read only mode set. Can't create subdirectories '%s'",
			path);

		return(FALSE);

	}

	char*	subdir = os_file_dirname(path);

	if (strlen(subdir) == 1
	    && (*subdir == OS_FILE_PATH_SEPARATOR || *subdir == '.')) {
		/* subdir is root or cwd, nothing to do */
		mem_free(subdir);

		return(TRUE);
	}

	/* Test if subdir exists */
	os_file_type_t	type;
	ibool	subdir_exists;
	ibool	success = os_file_status(subdir, &subdir_exists, &type);

	if (success && !subdir_exists) {

		/* subdir does not exist, create it */
		success = os_file_create_subdirs_if_needed(subdir);

		if (!success) {
			mem_free(subdir);

			return(FALSE);
		}

		success = os_file_create_directory(subdir, FALSE);
	}

	mem_free(subdir);

	return(success);
}

#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Returns a pointer to the nth slot in the aio array.
@return	pointer to slot */
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
@return	TRUE on success. */
static
ibool
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
		fprintf(stderr,
			"InnoDB: Linux native AIO:"
			" initialized io_ctx for segment\n");
#endif
		/* Success. Return now. */
		return(TRUE);
	}

	/* If we hit EAGAIN we'll make a few attempts before failing. */

	switch (ret) {
	case -EAGAIN:
		if (retries == 0) {
			/* First time around. */
			ut_print_timestamp(stderr);
			fprintf(stderr,
				" InnoDB: Warning: io_setup() failed"
				" with EAGAIN. Will make %d attempts"
				" before giving up.\n",
				OS_AIO_IO_SETUP_RETRY_ATTEMPTS);
		}

		if (retries < OS_AIO_IO_SETUP_RETRY_ATTEMPTS) {
			++retries;
			fprintf(stderr,
				"InnoDB: Warning: io_setup() attempt"
				" %lu failed.\n",
				retries);
			os_thread_sleep(OS_AIO_IO_SETUP_RETRY_SLEEP);
			goto retry;
		}

		/* Have tried enough. Better call it a day. */
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: io_setup() failed"
			" with EAGAIN after %d attempts.\n",
			OS_AIO_IO_SETUP_RETRY_ATTEMPTS);
		break;

	case -ENOSYS:
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: Linux Native AIO interface"
			" is not supported on this platform. Please"
			" check your OS documentation and install"
			" appropriate binary of InnoDB.\n");

		break;

	default:
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Error: Linux Native AIO setup"
			" returned following error[%d]\n", -ret);
		break;
	}

	fprintf(stderr,
		"InnoDB: You can disable Linux Native AIO by"
		" setting innodb_use_native_aio = 0 in my.cnf\n");
	return(FALSE);
}

/******************************************************************//**
Checks if the system supports native linux aio. On some kernel
versions where native aio is supported it won't work on tmpfs. In such
cases we can't use native aio as it is not possible to mix simulated
and native aio.
@return: TRUE if supported, FALSE otherwise. */
static
ibool
os_aio_native_aio_supported(void)
/*=============================*/
{
	int			fd;
	io_context_t		io_ctx;
	char			name[1000];

	if (!os_aio_linux_create_io_ctx(1, &io_ctx)) {
		/* The platform does not support native aio. */
		return(FALSE);
	} else if (!srv_read_only_mode) {
		/* Now check if tmpdir supports native aio ops. */
		fd = innobase_mysql_tmpfile();

		if (fd < 0) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Unable to create temp file to check "
				"native AIO support.");

			return(FALSE);
		}
	} else {

		srv_normalize_path_for_win(srv_log_group_home_dir);

		ulint	dirnamelen = strlen(srv_log_group_home_dir);
		ut_a(dirnamelen < (sizeof name) - 10 - sizeof "ib_logfile");
		memcpy(name, srv_log_group_home_dir, dirnamelen);

		/* Add a path separator if needed. */
		if (dirnamelen && name[dirnamelen - 1] != SRV_PATH_SEPARATOR) {
			name[dirnamelen++] = SRV_PATH_SEPARATOR;
		}

		strcpy(name + dirnamelen, "ib_logfile0");

		fd = ::open(name, O_RDONLY);

		if (fd == -1) {

			ib_logf(IB_LOG_LEVEL_WARN,
				"Unable to open \"%s\" to check "
				"native AIO read support.", name);

			return(FALSE);
		}
	}

	struct io_event	io_event;

	memset(&io_event, 0x0, sizeof(io_event));

	byte*	buf = static_cast<byte*>(ut_malloc(UNIV_PAGE_SIZE * 2));
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
		return(TRUE);

	case -EINVAL:
	case -ENOSYS:
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Linux Native AIO not supported. You can either "
			"move %s to a file system that supports native "
			"AIO or you can set innodb_use_native_aio to "
			"FALSE to avoid this message.",
			srv_read_only_mode ? name : "tmpdir");

		/* fall through. */
	default:
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Linux Native AIO check on %s returned error[%d]",
			srv_read_only_mode ? name : "tmpdir", -err);
	}

	return(FALSE);
}
#endif /* LINUX_NATIVE_AIO */

/******************************************************************//**
Creates an aio wait array. Note that we return NULL in case of failure.
We don't care about freeing memory here because we assume that a
failure will result in server refusing to start up.
@return	own: aio array, NULL on failure */
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

	array = static_cast<os_aio_array_t*>(ut_malloc(sizeof(*array)));
	memset(array, 0x0, sizeof(*array));

	array->mutex = os_mutex_create();
	array->not_full = os_event_create();
	array->is_empty = os_event_create();

	os_event_set(array->is_empty);

	array->n_slots = n;
	array->n_segments = n_segments;

	array->slots = static_cast<os_aio_slot_t*>(
		ut_malloc(n * sizeof(*array->slots)));

	memset(array->slots, 0x0, sizeof(n * sizeof(*array->slots)));
#ifdef __WIN__
	array->handles = static_cast<HANDLE*>(ut_malloc(n * sizeof(HANDLE)));
#endif /* __WIN__ */

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
		ut_malloc(n_segments * sizeof(*array->aio_ctx)));

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
		ut_malloc(n * sizeof(*io_event)));

	memset(io_event, 0x0, sizeof(*io_event) * n);
	array->aio_events = io_event;

skip_native_aio:
#endif /* LINUX_NATIVE_AIO */
	for (ulint i = 0; i < n; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i);

		slot->pos = i;
		slot->reserved = FALSE;
#ifdef WIN_ASYNC_IO
		slot->handle = CreateEvent(NULL,TRUE, FALSE, NULL);

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
	ulint	i;

	for (i = 0; i < array->n_slots; i++) {
		os_aio_slot_t*	slot = os_aio_array_get_nth_slot(array, i);
		CloseHandle(slot->handle);
	}
#endif /* WIN_ASYNC_IO */

#ifdef __WIN__
	ut_free(array->handles);
#endif /* __WIN__ */
	os_mutex_free(array->mutex);
	os_event_free(array->not_full);
	os_event_free(array->is_empty);

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
UNIV_INTERN
ibool
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

		ib_logf(IB_LOG_LEVEL_WARN, "Linux Native AIO disabled.");

		srv_use_native_aio = FALSE;
	}
#endif /* LINUX_NATIVE_AIO */

	srv_reset_io_thread_op_info();

	os_aio_read_array = os_aio_array_create(
		n_read_segs * n_per_seg, n_read_segs);

	if (os_aio_read_array == NULL) {
		return(FALSE);
	}

	ulint	start = (srv_read_only_mode) ? 0 : 2;
	ulint	n_segs = n_read_segs + start;

	/* 0 is the ibuf segment and 1 is the insert buffer segment. */
	for (ulint i = start; i < n_segs; ++i) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "read thread";
	}

	ulint	n_segments = n_read_segs;

	if (!srv_read_only_mode) {

		os_aio_log_array = os_aio_array_create(n_per_seg, 1);

		if (os_aio_log_array == NULL) {
			return(FALSE);
		}

		++n_segments;

		srv_io_thread_function[1] = "log thread";

		os_aio_ibuf_array = os_aio_array_create(n_per_seg, 1);

		if (os_aio_ibuf_array == NULL) {
			return(FALSE);
		}

		++n_segments;

		srv_io_thread_function[0] = "insert buffer thread";

		os_aio_write_array = os_aio_array_create(
			n_write_segs * n_per_seg, n_write_segs);

		if (os_aio_write_array == NULL) {
			return(FALSE);
		}

		n_segments += n_write_segs;

		for (ulint i = start + n_read_segs; i < n_segments; ++i) {
			ut_a(i < SRV_MAX_N_IO_THREADS);
			srv_io_thread_function[i] = "write thread";
		}

		ut_ad(n_segments >= 4);
	} else {
		ut_ad(n_segments > 0);
	}

	os_aio_sync_array = os_aio_array_create(n_slots_sync, 1);

	if (os_aio_sync_array == NULL) {
		return(FALSE);
	}

	os_aio_n_segments = n_segments;

	os_aio_validate();

	os_aio_segment_wait_events = static_cast<os_event_t*>(
		ut_malloc(n_segments * sizeof *os_aio_segment_wait_events));

	for (ulint i = 0; i < n_segments; ++i) {
		os_aio_segment_wait_events[i] = os_event_create();
	}

	os_last_printout = ut_time();

	return(TRUE);

}

/***********************************************************************
Frees the asynchronous io system. */
UNIV_INTERN
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

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_event_free(os_aio_segment_wait_events[i]);
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
	ulint	i;

	for (i = 0; i < array->n_slots; i++) {

		SetEvent((array->slots + i)->handle);
	}
}
#endif

/************************************************************************//**
Wakes up all async i/o threads so that they know to exit themselves in
shutdown. */
UNIV_INTERN
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
UNIV_INTERN
void
os_aio_wait_until_no_pending_writes(void)
/*=====================================*/
{
	ut_ad(!srv_read_only_mode);
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

	if (array == os_aio_ibuf_array) {
		ut_ad(!srv_read_only_mode);

		segment = IO_IBUF_SEGMENT;

	} else if (array == os_aio_log_array) {
		ut_ad(!srv_read_only_mode);

		segment = IO_LOG_SEGMENT;

	} else if (array == os_aio_read_array) {
		seg_len = os_aio_read_array->n_slots
			/ os_aio_read_array->n_segments;

		segment = 2 + slot->pos / seg_len;
	} else {
		ut_ad(!srv_read_only_mode);
		ut_a(array == os_aio_write_array);

		seg_len = os_aio_write_array->n_slots
			/ os_aio_write_array->n_segments;

		segment = os_aio_read_array->n_segments + 2
			+ slot->pos / seg_len;
	}

	return(segment);
}

/**********************************************************************//**
Calculates local segment number and aio array from global segment number.
@return	local segment number within the aio array */
static
ulint
os_aio_get_array_and_local_segment(
/*===============================*/
	os_aio_array_t** array,		/*!< out: aio wait array */
	ulint		 global_segment)/*!< in: global segment number */
{
	ulint		segment;

	ut_a(global_segment < os_aio_n_segments);

	if (srv_read_only_mode) {
		*array = os_aio_read_array;

		return(global_segment);
	} else if (global_segment == IO_IBUF_SEGMENT) {
		*array = os_aio_ibuf_array;
		segment = 0;

	} else if (global_segment == IO_LOG_SEGMENT) {
		*array = os_aio_log_array;
		segment = 0;

	} else if (global_segment < os_aio_read_array->n_segments + 2) {
		*array = os_aio_read_array;

		segment = global_segment - 2;
	} else {
		*array = os_aio_write_array;

		segment = global_segment - (os_aio_read_array->n_segments + 2);
	}

	return(segment);
}

/*******************************************************************//**
Requests for a slot in the aio array. If no slot is available, waits until
not_full-event becomes signaled.
@return	pointer to slot */
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
	os_mutex_enter(array->mutex);

	if (array->n_reserved == array->n_slots) {
		os_mutex_exit(array->mutex);

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

		if (slot->reserved == FALSE) {
			goto found;
		}
	}

	/* We MUST always be able to get hold of a reserved slot. */
	ut_error;

found:
	ut_a(slot->reserved == FALSE);
	array->n_reserved++;

	if (array->n_reserved == 1) {
		os_event_reset(array->is_empty);
	}

	if (array->n_reserved == array->n_slots) {
		os_event_reset(array->not_full);
	}

	slot->reserved = TRUE;
	slot->reservation_time = ut_time();
	slot->message1 = message1;
	slot->message2 = message2;
	slot->file     = file;
	slot->name     = name;
	slot->len      = len;
	slot->type     = type;
	slot->buf      = static_cast<byte*>(buf);
	slot->offset   = offset;
	slot->io_already_done = FALSE;

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
	os_mutex_exit(array->mutex);

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
	os_mutex_enter(array->mutex);

	ut_ad(slot->reserved);

	slot->reserved = FALSE;

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
	os_mutex_exit(array->mutex);
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

	os_mutex_enter(array->mutex);

	for (ulint i = 0; i < n; ++i) {
		const os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, segment + i);

		if (slot->reserved) {

			/* Found an i/o request */

			os_mutex_exit(array->mutex);

			os_event_t	event;

			event = os_aio_segment_wait_events[global_segment];

			os_event_set(event);

			return;
		}
	}

	os_mutex_exit(array->mutex);
}

/**********************************************************************//**
Wakes up simulated aio i/o-handler threads if they have something to do. */
UNIV_INTERN
void
os_aio_simulated_wake_handler_threads(void)
/*=======================================*/
{
	if (srv_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= FALSE;

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_aio_simulated_wake_handler_thread(i);
	}
}

/**********************************************************************//**
This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
UNIV_INTERN
void
os_aio_simulated_put_read_threads_to_sleep(void)
/*============================================*/
{

/* The idea of putting background IO threads to sleep is only for
Windows when using simulated AIO. Windows XP seems to schedule
background threads too eagerly to allow for coalescing during
readahead requests. */
#ifdef __WIN__
	os_aio_array_t*	array;

	if (srv_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= TRUE;

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_aio_get_array_and_local_segment(&array, i);

		if (array == os_aio_read_array) {

			os_event_reset(os_aio_segment_wait_events[i]);
		}
	}
#endif /* __WIN__ */
}

#if defined(LINUX_NATIVE_AIO)
/*******************************************************************//**
Dispatch an AIO request to the kernel.
@return	TRUE on success. */
static
ibool
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

	ut_a(slot->reserved);

	/* Find out what we are going to work with.
	The iocb struct is directly in the slot.
	The io_context is one per segment. */

	iocb = &slot->control;
	io_ctx_index = (slot->pos * array->n_segments) / array->n_slots;

	ret = io_submit(array->aio_ctx[io_ctx_index], 1, &iocb);

#if defined(UNIV_AIO_DEBUG)
	fprintf(stderr,
		"io_submit[%c] ret[%d]: slot[%p] ctx[%p] seg[%lu]\n",
		(slot->type == OS_FILE_WRITE) ? 'w' : 'r', ret, slot,
		array->aio_ctx[io_ctx_index], (ulong) io_ctx_index);
#endif

	/* io_submit returns number of successfully
	queued requests or -errno. */
	if (UNIV_UNLIKELY(ret != 1)) {
		errno = -ret;
		return(FALSE);
	}

	return(TRUE);
}
#endif /* LINUX_NATIVE_AIO */


/*******************************************************************//**
NOTE! Use the corresponding macro os_aio(), not directly this function!
Requests an asynchronous i/o operation.
@return	TRUE if request was queued successfully, FALSE if fail */
UNIV_INTERN
ibool
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
	ibool		retval;
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

		ut_ad(!srv_read_only_mode);
		ut_a(type == OS_FILE_WRITE);

		return(os_file_write_func(name, file, buf, offset, n));
	}

try_again:
	switch (mode) {
	case OS_AIO_NORMAL:
		if (type == OS_FILE_READ) {
			array = os_aio_read_array;
		} else {
			ut_ad(!srv_read_only_mode);
			array = os_aio_write_array;
		}
		break;
	case OS_AIO_IBUF:
		ut_ad(type == OS_FILE_READ);
		/* Reduce probability of deadlock bugs in connection with ibuf:
		do not let the ibuf i/o handler sleep */

		wake_later = FALSE;

		if (srv_read_only_mode) {
			array = os_aio_read_array;
		} else {
			array = os_aio_ibuf_array;
		}
		break;
	case OS_AIO_LOG:
		if (srv_read_only_mode) {
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
		ut_ad(!srv_read_only_mode);
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

			return(TRUE);
		}

		goto err_exit;
	}
#endif /* WIN_ASYNC_IO */
	/* aio was queued successfully! */
	return(TRUE);

#if defined LINUX_NATIVE_AIO || defined WIN_ASYNC_IO
err_exit:
#endif /* LINUX_NATIVE_AIO || WIN_ASYNC_IO */
	os_aio_array_free_slot(array, slot);

	if (os_file_handle_error(
		name,type == OS_FILE_READ ? "aio read" : "aio write")) {

		goto try_again;
	}

	return(FALSE);
}

#ifdef WIN_ASYNC_IO
/**********************************************************************//**
This function is only used in Windows asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@return	TRUE if the aio operation succeeded */
UNIV_INTERN
ibool
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
	ibool		ret_val;
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

	os_mutex_enter(array->mutex);

	if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
	    && array->n_reserved == 0) {
		*message1 = NULL;
		*message2 = NULL;
		os_mutex_exit(array->mutex);
		return(TRUE);
	}

	ut_a(i >= WAIT_OBJECT_0 && i <= WAIT_OBJECT_0 + n);

	slot = os_aio_array_get_nth_slot(array, i + segment * n);

	ut_a(slot->reserved);

	if (orig_seg != ULINT_UNDEFINED) {
		srv_set_io_thread_op_info(
			orig_seg, "get windows aio return value");
	}

	ret = GetOverlappedResult(slot->file, &(slot->control), &len, TRUE);

	*message1 = slot->message1;
	*message2 = slot->message2;

	*type = slot->type;

	if (ret && len == slot->len) {

		ret_val = TRUE;
	} else if (os_file_handle_error(slot->name, "Windows aio")) {

		retry = TRUE;
	} else {

		ret_val = FALSE;
	}

	os_mutex_exit(array->mutex);

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
			ut_a(slot->reserved);

#if defined(UNIV_AIO_DEBUG)
			fprintf(stderr,
				"io_getevents[%c]: slot[%p] ctx[%p]"
				" seg[%lu]\n",
				(slot->type == OS_FILE_WRITE) ? 'w' : 'r',
				slot, io_ctx, segment);
#endif

			/* We are not scribbling previous segment. */
			ut_a(slot->pos >= start_pos);

			/* We have not overstepped to next segment. */
			ut_a(slot->pos < end_pos);

			/* Mark this request as completed. The error handling
			will be done in the calling function. */
			os_mutex_enter(array->mutex);
			slot->n_bytes = events[i].res;
			slot->ret = events[i].res2;
			slot->io_already_done = TRUE;
			os_mutex_exit(array->mutex);
		}
		return;
	}

	if (UNIV_UNLIKELY(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS)) {
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
	ut_print_timestamp(stderr);
	fprintf(stderr,
		" InnoDB: unexpected ret_code[%d] from io_getevents()!\n",
		ret);
	ut_error;
}

/**********************************************************************//**
This function is only used in Linux native asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait for
the completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@return	TRUE if the IO was successful */
UNIV_INTERN
ibool
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
	ibool		ret = FALSE;

	/* Should never be doing Sync IO here. */
	ut_a(global_seg != ULINT_UNDEFINED);

	/* Find the array and the local segment. */
	segment = os_aio_get_array_and_local_segment(&array, global_seg);
	n = array->n_slots / array->n_segments;

	/* Loop until we have found a completed request. */
	for (;;) {
		ibool	any_reserved = FALSE;
		os_mutex_enter(array->mutex);
		for (i = 0; i < n; ++i) {
			slot = os_aio_array_get_nth_slot(
				array, i + segment * n);
			if (!slot->reserved) {
				continue;
			} else if (slot->io_already_done) {
				/* Something for us to work on. */
				goto found;
			} else {
				any_reserved = TRUE;
			}
		}

		os_mutex_exit(array->mutex);

		/* There is no completed request.
		If there is no pending request at all,
		and the system is being shut down, exit. */
		if (UNIV_UNLIKELY
		    (!any_reserved
		     && srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS)) {
			*message1 = NULL;
			*message2 = NULL;
			return(TRUE);
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
	ut_ad(slot->reserved);
	ut_ad(slot->io_already_done);

	*message1 = slot->message1;
	*message2 = slot->message2;

	*type = slot->type;

	if (slot->ret == 0 && slot->n_bytes == (long) slot->len) {

		ret = TRUE;
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

		ret = FALSE;
	}

	os_mutex_exit(array->mutex);

	os_aio_array_free_slot(array, slot);

	return(ret);
}
#endif /* LINUX_NATIVE_AIO */

/**********************************************************************//**
Does simulated aio. This function should be called by an i/o-handler
thread.
@return	TRUE if the aio operation succeeded */
UNIV_INTERN
ibool
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
	ibool		ret;
	ibool		any_reserved;
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
	any_reserved = FALSE;

	os_mutex_enter(array->mutex);

	for (ulint i = 0; i < n; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (!slot->reserved) {
			continue;
		} else if (slot->io_already_done) {

			if (os_aio_print_debug) {
				fprintf(stderr,
					"InnoDB: i/o for slot %lu"
					" already done, returning\n",
					(ulong) i);
			}

			aio_slot = slot;
			ret = TRUE;
			goto slot_io_done;
		} else {
			any_reserved = TRUE;
		}
	}

	/* There is no completed request.
	If there is no pending request at all,
	and the system is being shut down, exit. */
	if (!any_reserved && srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {
		os_mutex_exit(array->mutex);
		*message1 = NULL;
		*message2 = NULL;
		return(TRUE);
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

		if (slot->reserved) {

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

			if (slot->reserved && slot->offset < lowest_offset) {

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

		if (slot->reserved
		    && slot != aio_slot
		    && slot->offset == slot->offset + aio_slot->len
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
			ut_malloc(total_len + UNIV_PAGE_SIZE));

		ut_a(combined_buf2);

		combined_buf = static_cast<byte*>(
			ut_align(combined_buf2, UNIV_PAGE_SIZE));
	}

	/* We release the array mutex for the time of the i/o: NOTE that
	this assumes that there is just one i/o-handler thread serving
	a single segment of slots! */

	os_mutex_exit(array->mutex);

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
		ut_ad(!srv_read_only_mode);
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

	os_mutex_enter(array->mutex);

	/* Mark the i/os done in slots */

	for (ulint i = 0; i < n_consecutive; i++) {
		consecutive_ios[i]->io_already_done = TRUE;
	}

	/* We return the messages for the first slot now, and if there were
	several slots, the messages will be returned with subsequent calls
	of this function */

slot_io_done:

	ut_a(aio_slot->reserved);

	*message1 = aio_slot->message1;
	*message2 = aio_slot->message2;

	*type = aio_slot->type;

	os_mutex_exit(array->mutex);

	os_aio_array_free_slot(array, aio_slot);

	return(ret);

wait_for_io:
	srv_set_io_thread_op_info(global_segment, "resetting wait event");

	/* We wait here until there again can be i/os in the segment
	of this thread */

	os_event_reset(os_aio_segment_wait_events[global_segment]);

	os_mutex_exit(array->mutex);

recommended_sleep:
	srv_set_io_thread_op_info(global_segment, "waiting for i/o request");

	os_event_wait(os_aio_segment_wait_events[global_segment]);

	goto restart;
}

/**********************************************************************//**
Validates the consistency of an aio array.
@return	true if ok */
static
bool
os_aio_array_validate(
/*==================*/
	os_aio_array_t*	array)	/*!< in: aio wait array */
{
	ulint		i;
	ulint		n_reserved	= 0;

	os_mutex_enter(array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);

	for (i = 0; i < array->n_slots; i++) {
		os_aio_slot_t*	slot;

		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved) {
			n_reserved++;
			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	os_mutex_exit(array->mutex);

	return(true);
}

/**********************************************************************//**
Validates the consistency the aio system.
@return	TRUE if ok */
UNIV_INTERN
ibool
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

	return(TRUE);
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
UNIV_INTERN
void
os_aio_print_array(
/*==============*/
	FILE*		file,	/*!< in: file where to print */
	os_aio_array_t*	array)	/*!< in: aio array to print */
{
	ulint			n_reserved = 0;
	ulint			n_res_seg[SRV_MAX_N_IO_THREADS];

	os_mutex_enter(array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);

	memset(n_res_seg, 0x0, sizeof(n_res_seg));

	for (ulint i = 0; i < array->n_slots; ++i) {
		os_aio_slot_t*	slot;
		ulint		seg_no;

		slot = os_aio_array_get_nth_slot(array, i);

		seg_no = (i * array->n_segments) / array->n_slots;

		if (slot->reserved) {
			++n_reserved;
			++n_res_seg[seg_no];

			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	fprintf(file, " %lu", (ulong) n_reserved);

	os_aio_print_segment_info(file, n_res_seg, array);

	os_mutex_exit(array->mutex);
}

/**********************************************************************//**
Prints info of the aio arrays. */
UNIV_INTERN
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

#ifndef __WIN__
		if (os_aio_segment_wait_events[i]->is_set) {
			fprintf(file, " ev set");
		}
#endif /* __WIN__ */

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
UNIV_INTERN
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

#ifdef UNIV_DEBUG
/**********************************************************************//**
Checks that all slots in the system have been freed, that is, there are
no pending io operations.
@return	TRUE if all free */
UNIV_INTERN
ibool
os_aio_all_slots_free(void)
/*=======================*/
{
	os_aio_array_t*	array;
	ulint		n_res	= 0;

	array = os_aio_read_array;

	os_mutex_enter(array->mutex);

	n_res += array->n_reserved;

	os_mutex_exit(array->mutex);

	if (!srv_read_only_mode) {
		ut_a(os_aio_write_array == 0);

		array = os_aio_write_array;

		os_mutex_enter(array->mutex);

		n_res += array->n_reserved;

		os_mutex_exit(array->mutex);

		ut_a(os_aio_ibuf_array == 0);

		array = os_aio_ibuf_array;

		os_mutex_enter(array->mutex);

		n_res += array->n_reserved;

		os_mutex_exit(array->mutex);
	}

	ut_a(os_aio_log_array == 0);

	array = os_aio_log_array;

	os_mutex_enter(array->mutex);

	n_res += array->n_reserved;

	os_mutex_exit(array->mutex);

	array = os_aio_sync_array;

	os_mutex_enter(array->mutex);

	n_res += array->n_reserved;

	os_mutex_exit(array->mutex);

	if (n_res == 0) {

		return(TRUE);
	}

	return(FALSE);
}
#endif /* UNIV_DEBUG */

#endif /* !UNIV_HOTBACKUP */
