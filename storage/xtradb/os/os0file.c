/***********************************************************************

Copyright (c) 1995, 2010, Innobase Oy. All Rights Reserved.
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

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

***********************************************************************/

/**************************************************//**
@file os/os0file.c
The interface to the operating system file i/o primitives

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#include "os0file.h"
#include "ut0mem.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "fil0fil.h"
#include "buf0buf.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "log0recv.h"
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

#ifdef _WIN32
#define IOCP_SHUTDOWN_KEY (ULONG_PTR)-1
#endif

/* This specifies the file permissions InnoDB uses when it creates files in
Unix; the value of os_innodb_umask is initialized in ha_innodb.cc to
my_umask */

#ifndef __WIN__
/** Umask for creating files */
UNIV_INTERN ulint	os_innodb_umask
			= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
#else
/** Umask for creating files */
UNIV_INTERN ulint	os_innodb_umask		= 0;
#endif

#ifdef UNIV_DO_FLUSH
/* If the following is set to TRUE, we do not call os_file_flush in every
os_file_write. We can set this TRUE when the doublewrite buffer is used. */
UNIV_INTERN ibool	os_do_not_call_flush_at_each_write	= FALSE;
#else
/* We do not call os_file_flush in every os_file_write. */
#endif /* UNIV_DO_FLUSH */

#ifdef UNIV_HOTBACKUP
# define os_aio_use_native_aio	FALSE
#else /* UNIV_HOTBACKUP */
/* We use these mutexes to protect lseek + file i/o operation, if the
OS does not provide an atomic pread or pwrite, or similar */
#define OS_FILE_N_SEEK_MUTEXES	16
UNIV_INTERN os_mutex_t	os_file_seek_mutexes[OS_FILE_N_SEEK_MUTEXES];

/* In simulated aio, merge at most this many consecutive i/os */
#define OS_AIO_MERGE_N_CONSECUTIVE	64

/** If this flag is TRUE, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads */

UNIV_INTERN ibool	os_aio_use_native_aio	= FALSE;

/** Flag: enable debug printout for asynchronous i/o */
UNIV_INTERN ibool	os_aio_print_debug	= FALSE;

/* State for the state of an IO request in simulated AIO.
   Protocol for simulated aio:
     client requests IO: find slot with reserved = FALSE. Add entry with
                         status = OS_AIO_NOT_ISSUED.
     IO thread wakes: find adjacent slots with reserved = TRUE and status =
                      OS_AIO_NOT_ISSUED. Change status for slots to
                      OS_AIO_ISSUED.
     IO operation completes: set status for slots to OS_AIO_DONE. set status
                             for the first slot to OS_AIO_CLAIMED and return
                             result for that slot.
   When there are multiple read and write threads, they all compete to execute
   the requests in the array (os_aio_array_t). This avoids the need to load
   balance requests at the time the request is made at the cost of waking all
   threads when a request is available.
*/
typedef enum {
	OS_AIO_NOT_ISSUED, /* Available to be processed by an IO thread. */
	OS_AIO_ISSUED,     /* Being processed by an IO thread. */
	OS_AIO_DONE,       /* Request processed. */
	OS_AIO_CLAIMED     /* Result being returned to client. */
} os_aio_status;

/** The asynchronous i/o array slot structure */
typedef struct os_aio_slot_struct	os_aio_slot_t;

/** The asynchronous i/o array slot structure */
struct os_aio_slot_struct{
#ifdef WIN_ASYNC_IO
	OVERLAPPED	control;	/*!< Windows control block for the
					aio request, MUST be first element in the structure*/
	void *arr;				/*!< Array this slot belongs to*/
#endif

	ibool		is_read;	/*!< TRUE if a read operation */
	ulint		pos;		/*!< index of the slot in the aio
					array */
	ibool		reserved;	/*!< TRUE if this slot is reserved */
	os_aio_status   status;		/* Status for current request. Valid when reserved
					is TRUE. Used only in simulated aio. */
	time_t		reservation_time;/*!< time when reserved */
	ulint		len;		/*!< length of the block to read or
					write */
	byte*		buf;		/*!< buffer used in i/o */
	ulint		type;		/*!< OS_FILE_READ or OS_FILE_WRITE */
	ulint		offset;		/*!< 32 low bits of file offset in
					bytes */
	ulint		offset_high;	/*!< 32 high bits of file offset */
	os_file_t	file;		/*!< file where to read or write */
	const char*	name;		/*!< file name or path */
//	ibool		io_already_done;/*!< used only in simulated aio:
//					TRUE if the physical i/o already
//					made and only the slot message
//					needs to be passed to the caller
//					of os_aio_simulated_handle */
	ulint		space_id;
	fil_node_t*	message1;	/*!< message which is given by the */
	void*		message2;	/*!< the requester of an aio operation
					and which can be used to identify
					which pending aio operation was
					completed */
};

/** The asynchronous i/o array structure */
typedef struct os_aio_array_struct	os_aio_array_t;

/** The asynchronous i/o array structure */
struct os_aio_array_struct{
	os_mutex_t	mutex;	/*!< the mutex protecting the aio array */
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
	ulint		n_reserved;
				/*!< Number of reserved slots in the
				aio array outside the ibuf segment */
	os_aio_slot_t*	slots;	/*!< Pointer to the slots in the array */
};

/** Array of events used in simulated aio */
static os_event_t*	os_aio_segment_wait_events	= NULL;

/* Number for the first global segment for reading. */
const ulint os_aio_first_read_segment = 2;

/* Number for the first global segment for writing. Set to
2 + os_aio_read_write_threads. */
ulint os_aio_first_write_segment = 0;

/** The aio arrays for non-ibuf i/o and ibuf i/o, as well as sync aio. These
are NULL when the module has not yet been initialized. @{ */
static os_aio_array_t*	os_aio_read_array	= NULL;	/*!< Reads */
static os_aio_array_t*	os_aio_write_array	= NULL;	/*!< Writes */
static os_aio_array_t*	os_aio_ibuf_array	= NULL;	/*!< Insert buffer */
static os_aio_array_t*	os_aio_log_array	= NULL;	/*!< Redo log */
static os_aio_array_t*	os_aio_sync_array	= NULL;	/*!< Synchronous I/O */
/* @} */

/* Per thread buffer used for merged IO requests. Used by
os_aio_simulated_handle so that a buffer doesn't have to be allocated
for each request. */
static byte* os_aio_thread_buffer[SRV_MAX_N_IO_THREADS];
static ulint os_aio_thread_buffer_size[SRV_MAX_N_IO_THREADS];

/** Number of asynchronous I/O segments.  Set by os_aio_init(). */
static ulint	os_aio_n_segments	= ULINT_UNDEFINED;

/** If the following is TRUE, read i/o handler threads try to
wait until a batch of new read requests have been posted */
static volatile ibool	os_aio_recommend_sleep_for_read_threads	= FALSE;
#endif /* UNIV_HOTBACKUP */

UNIV_INTERN ulint	os_n_file_reads		= 0;
UNIV_INTERN ulint	os_bytes_read_since_printout = 0;
UNIV_INTERN ulint	os_n_file_writes	= 0;
UNIV_INTERN ulint	os_n_fsyncs		= 0;
UNIV_INTERN ulint	os_n_file_reads_old	= 0;
UNIV_INTERN ulint	os_n_file_writes_old	= 0;
UNIV_INTERN ulint	os_n_fsyncs_old		= 0;
UNIV_INTERN time_t	os_last_printout;

UNIV_INTERN ibool	os_has_said_disk_full	= FALSE;

#ifndef UNIV_HOTBACKUP
/** The mutex protecting the following counts of pending I/O operations */
static os_mutex_t	os_file_count_mutex;
#endif /* !UNIV_HOTBACKUP */
/** Number of pending os_file_pread() operations */
UNIV_INTERN ulint	os_file_n_pending_preads  = 0;
/** Number of pending os_file_pwrite() operations */
UNIV_INTERN ulint	os_file_n_pending_pwrites = 0;
/** Number of pending write operations */
UNIV_INTERN ulint	os_n_pending_writes = 0;
/** Number of pending read operations */
UNIV_INTERN ulint	os_n_pending_reads = 0;


#ifdef _WIN32
/** IO completion port used by background io threads */
static HANDLE completion_port;
/** IO completion port used by background io READ threads */
static HANDLE read_completion_port;
/** Thread local storage index for the per-thread event used for synchronous IO */
static DWORD tls_sync_io = TLS_OUT_OF_INDEXES;
#endif

/***********************************************************************//**
Gets the operating system version. Currently works only on Windows.
@return	OS_WIN95, OS_WIN31, OS_WINNT, OS_WIN2000 */
UNIV_INTERN
ulint
os_get_os_version(void)
/*===================*/
{
#ifdef __WIN__
	OSVERSIONINFO	  os_info;

	os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	ut_a(GetVersionEx(&os_info));

	if (os_info.dwPlatformId == VER_PLATFORM_WIN32s) {
		return(OS_WIN31);
	} else if (os_info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		return(OS_WIN95);
	} else if (os_info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		switch(os_info.dwMajorVersion){
			case 3:
			case 4:
				return OS_WINNT;
			case 5:
				return (os_info.dwMinorVersion == 0)?OS_WIN2000 : OS_WINXP;
			case 6:
				return (os_info.dwMinorVersion == 0)?OS_WINVISTA : OS_WIN7;
			default:
				return OS_WIN7;
		}
	} else {
		ut_error;
		return(0);
	}
#else
	ut_error;

	return(0);
#endif
}


#ifdef _WIN32
/*
Windows : Handling synchronous IO on files opened asynchronously.

If file is opened for asynchronous IO (FILE_FLAG_OVERLAPPED) and also bound to 
a completion port, then every IO on this file would normally be enqueued to the
completion port. Sometimes however we would like to do a synchronous IO. This is
possible if we initialitze have overlapped.hEvent with a valid event and set its
lowest order bit to 1 (see MSDN ReadFile and WriteFile description for more info)

We'll create this special event once for each thread and store in thread local 
storage.
*/


/***********************************************************************//**
Initialize tls index.for event handle used for synchronized IO on files that 
might be opened with FILE_FLAG_OVERLAPPED.
*/
static void win_init_syncio_event()
{
	tls_sync_io = TlsAlloc();
	ut_a(tls_sync_io != TLS_OUT_OF_INDEXES);
}

/***********************************************************************//**
Retrieve per-thread event for doing synchronous io on asyncronously opened files
*/
static HANDLE win_get_syncio_event()
{
	HANDLE h;
	if(tls_sync_io == TLS_OUT_OF_INDEXES){
		win_init_syncio_event();
	}

	h = (HANDLE)TlsGetValue(tls_sync_io);
	if (h)
		return h;
	h = CreateEventA(NULL, FALSE, FALSE, NULL);
	ut_a(h);
	h = (HANDLE)((uintptr_t)h | 1);
	TlsSetValue(tls_sync_io, h);
	return h;
}

/*
  TLS destructor, inspired by Chromium code
  http://src.chromium.org/svn/trunk/src/base/threading/thread_local_storage_win.cc
*/

static void win_free_syncio_event()
{
	HANDLE h = win_get_syncio_event();
	if (h) {
		CloseHandle(h);
	}
}

static void NTAPI win_tls_thread_exit(PVOID module, DWORD reason, PVOID reserved) {
	if (DLL_THREAD_DETACH == reason || DLL_PROCESS_DETACH == reason)
		win_free_syncio_event();
}

#ifdef _WIN64
#pragma comment(linker, "/INCLUDE:_tls_used")
#pragma comment(linker, "/INCLUDE:p_thread_callback_base")
#pragma const_seg(".CRT$XLB")
extern const PIMAGE_TLS_CALLBACK p_thread_callback_base;
const PIMAGE_TLS_CALLBACK p_thread_callback_base = win_tls_thread_exit;
#pragma data_seg()
#else
#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_p_thread_callback_base")
#pragma data_seg(".CRT$XLB")
PIMAGE_TLS_CALLBACK p_thread_callback_base = win_tls_thread_exit;
#pragma data_seg()
#endif 
#endif /*_WIN32 */

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
	ibool	report_all_errors)	/*!< in: TRUE if we want an error message
					printed of all errors */
{
	ulint	err;

#ifdef __WIN__

	err = (ulint) GetLastError();

	if (report_all_errors
	    || (err != ERROR_DISK_FULL && err != ERROR_FILE_EXISTS)) {

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
	err = (ulint) errno;

	if (report_all_errors
	    || (err != ENOSPC && err != EEXIST)) {

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Operating system error number %lu"
			" in a file operation.\n", (ulong) err);

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
			if (strerror((int)err) != NULL) {
				fprintf(stderr,
					"InnoDB: Error number %lu"
					" means '%s'.\n",
					err, strerror((int)err));
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

	if (err == ENOSPC) {
		return(OS_FILE_DISK_FULL);
	} else if (err == ENOENT) {
		return(OS_FILE_NOT_FOUND);
	} else if (err == EEXIST) {
		return(OS_FILE_ALREADY_EXISTS);
	} else if (err == EXDEV || err == ENOTDIR || err == EISDIR) {
		return(OS_FILE_PATH_ERROR);
	} else {
		return(100 + err);
	}
#endif
}

/****************************************************************//**
Does error handling when a file operation fails.
Conditionally exits (calling exit(3)) based on should_exit value and the
error type
@return	TRUE if we should retry the operation */
static
ibool
os_file_handle_error_cond_exit(
/*===========================*/
	const char*	name,		/*!< in: name of a file or NULL */
	const char*	operation,	/*!< in: operation */
	ibool		should_exit)	/*!< in: call exit(3) if unknown error
					and this parameter is TRUE */
{
	ulint	err;

	err = os_file_get_last_error(FALSE);

	if (err == OS_FILE_DISK_FULL) {
		/* We only print a warning about disk full once */

		if (os_has_said_disk_full) {

			return(FALSE);
		}

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
	} else if (err == OS_FILE_AIO_RESOURCES_RESERVED) {

		return(TRUE);
	} else if (err == OS_FILE_ALREADY_EXISTS
		   || err == OS_FILE_PATH_ERROR) {

		return(FALSE);
	} else if (err == OS_FILE_SHARING_VIOLATION) {

		os_thread_sleep(10000000);  /* 10 sec */
		return(TRUE);
	} else if (err == OS_FILE_INSUFFICIENT_RESOURCE) {

		os_thread_sleep(100000);	/* 100 ms */
		return(TRUE);
	} else if (err == OS_FILE_OPERATION_ABORTED) {

		os_thread_sleep(100000);	/* 100 ms */
		return(TRUE);
	} else {
		if (name) {
			fprintf(stderr, "InnoDB: File name %s\n", name);
		}

		fprintf(stderr, "InnoDB: File operation call: '%s'.\n",
			operation);

		if (should_exit) {
			fprintf(stderr, "InnoDB: Cannot continue operation.\n");

			fflush(stderr);

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
	const char*	name,	/*!< in: name of a file or NULL */
	const char*	operation)/*!< in: operation */
{
	/* exit in case of unknown error */
	return(os_file_handle_error_cond_exit(name, operation, TRUE));
}

/****************************************************************//**
Does error handling when a file operation fails.
@return	TRUE if we should retry the operation */
static
ibool
os_file_handle_error_no_exit(
/*=========================*/
	const char*	name,	/*!< in: name of a file or NULL */
	const char*	operation)/*!< in: operation */
{
	/* don't exit in case of unknown error */
	return(os_file_handle_error_cond_exit(name, operation, FALSE));
}

#undef USE_FILE_LOCK
#define USE_FILE_LOCK
#if defined(UNIV_HOTBACKUP) || defined(__WIN__) || defined(__NETWARE__)
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
	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = lk.l_len = 0;
	if (fcntl(fd, F_SETLK, &lk) == -1) {
		fprintf(stderr,
			"InnoDB: Unable to lock %s, error: %d\n", name, errno);

		if (errno == EAGAIN || errno == EACCES) {
			fprintf(stderr,
				"InnoDB: Check that you do not already have"
				" another mysqld process\n"
				"InnoDB: using the same InnoDB data"
				" or log files.\n");
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
	ulint	i;

	os_file_count_mutex = os_mutex_create(NULL);

	for (i = 0; i < OS_FILE_N_SEEK_MUTEXES; i++) {
		os_file_seek_mutexes[i] = os_mutex_create(NULL);
	}
#ifdef _WIN32
	win_init_syncio_event();
#endif
}

/***********************************************************************//**
Creates a temporary file.  This function is like tmpfile(3), but
the temporary file is created in the MySQL temporary directory.
On Netware, this function is like tmpfile(3), because the C run-time
library of Netware does not expose the delete-on-close flag.
@return	temporary file handle, or NULL on error */
UNIV_INTERN
FILE*
os_file_create_tmpfile(void)
/*========================*/
{
#ifdef __NETWARE__
	FILE*	file	= tmpfile();
#else /* __NETWARE__ */
	FILE*	file	= NULL;
	int	fd	= innobase_mysql_tmpfile();

	if (fd >= 0) {
		file = fdopen(fd, "w+b");
	}
#endif /* __NETWARE__ */

	if (!file) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: unable to create temporary file;"
			" errno: %d\n", errno);
#ifndef __NETWARE__
		if (fd >= 0) {
			close(fd);
		}
#endif /* !__NETWARE__ */
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

	lpFindFileData = ut_malloc(sizeof(WIN32_FIND_DATA));

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
#endif
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
		os_file_handle_error_no_exit(NULL, "closedir");

		return(-1);
	}

	return(0);
#else
	int	ret;

	ret = closedir(dir);

	if (ret) {
		os_file_handle_error_no_exit(NULL, "closedir");
	}

	return(ret);
#endif
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

	lpFindFileData = ut_malloc(sizeof(WIN32_FIND_DATA));
next_file:
	ret = FindNextFile(dir, lpFindFileData);

	if (ret) {
		ut_a(strlen((char *) lpFindFileData->cFileName)
		     < OS_FILE_MAX_PATH);

		if (strcmp((char *) lpFindFileData->cFileName, ".") == 0
		    || strcmp((char *) lpFindFileData->cFileName, "..") == 0) {

			goto next_file;
		}

		strcpy(info->name, (char *) lpFindFileData->cFileName);

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
		os_file_handle_error_no_exit(dirname,
					     "readdir_next_file");
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
	ret = readdir_r(dir, (struct dirent*)dirent_buf, &ent);

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
			dirname, (ulong)ret);

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

	full_path = ut_malloc(strlen(dirname) + strlen(ent->d_name) + 10);

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

		os_file_handle_error_no_exit(full_path, "stat");

		ut_free(full_path);

		return(-1);
	}

	info->size = (ib_int64_t)statinfo.st_size;

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
This function attempts to create a directory named pathname. The new directory
gets default permissions. On Unix the permissions are (0770 & ~umask). If the
directory exists already, nothing is done and the call succeeds, unless the
fail_if_exists arguments is true.
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
		/* failure */
		os_file_handle_error(pathname, "CreateDirectory");

		return(FALSE);
	}

	return (TRUE);
#else
	int	rcode;

	rcode = mkdir(pathname, 0770);

	if (!(rcode == 0 || (errno == EEXIST && !fail_if_exists))) {
		/* failure */
		os_file_handle_error(pathname, "mkdir");

		return(FALSE);
	}

	return (TRUE);
#endif
}

/****************************************************************//**
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INTERN
os_file_t
os_file_create_simple(
/*==================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: OS_FILE_OPEN if an existing file is
				opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error), or
				OS_FILE_CREATE_PATH if new file
				(if exists, error) and subdirectories along
				its path are created (if needed)*/
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY or
				OS_FILE_READ_WRITE */
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
#ifdef __WIN__
	os_file_t	file;
	DWORD		create_flag;
	DWORD		access;
	DWORD		attributes	= 0;
	ibool		retry;

try_again:
	ut_a(name);

	if (create_mode == OS_FILE_OPEN) {
		create_flag = OPEN_EXISTING;
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = CREATE_NEW;
	} else if (create_mode == OS_FILE_CREATE_PATH) {
		/* create subdirs along the path if needed  */
		*success = os_file_create_subdirs_if_needed(name);
		if (!*success) {
			ut_error;
		}
		create_flag = CREATE_NEW;
		create_mode = OS_FILE_CREATE;
	} else {
		create_flag = 0;
		ut_error;
	}

	if (access_type == OS_FILE_READ_ONLY) {
		access = GENERIC_READ;
	} else if (access_type == OS_FILE_READ_WRITE) {
		access = GENERIC_READ | GENERIC_WRITE;
	} else {
		access = 0;
		ut_error;
	}

	file = CreateFile((LPCTSTR) name,
			  access,
			  FILE_SHARE_READ | FILE_SHARE_WRITE,
			  /* file can be read and written also
			  by other processes */
			  NULL,	/* default security attributes */
			  create_flag,
			  attributes,
			  NULL);	/*!< no template file */

	if (file == INVALID_HANDLE_VALUE) {
		*success = FALSE;

		retry = os_file_handle_error(name,
					     create_mode == OS_FILE_OPEN ?
					     "open" : "create");
		if (retry) {
			goto try_again;
		}
	} else {
		*success = TRUE;
	}

	return(file);
#else /* __WIN__ */
	os_file_t	file;
	int		create_flag;
	ibool		retry;

try_again:
	ut_a(name);

	if (create_mode == OS_FILE_OPEN) {
		if (access_type == OS_FILE_READ_ONLY) {
			create_flag = O_RDONLY;
		} else {
			create_flag = O_RDWR;
		}
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = O_RDWR | O_CREAT | O_EXCL;
	} else if (create_mode == OS_FILE_CREATE_PATH) {
		/* create subdirs along the path if needed  */
		*success = os_file_create_subdirs_if_needed(name);
		if (!*success) {
			return (-1);
		}
		create_flag = O_RDWR | O_CREAT | O_EXCL;
		create_mode = OS_FILE_CREATE;
	} else {
		create_flag = 0;
		ut_error;
	}

	if (create_mode == OS_FILE_CREATE) {
		file = open(name, create_flag, S_IRUSR | S_IWUSR
			    | S_IRGRP | S_IWGRP);
	} else {
		file = open(name, create_flag);
	}

	if (file == -1) {
		*success = FALSE;

		retry = os_file_handle_error(name,
					     create_mode == OS_FILE_OPEN ?
					     "open" : "create");
		if (retry) {
			goto try_again;
		}
#ifdef USE_FILE_LOCK
	} else if (access_type == OS_FILE_READ_WRITE
		   && os_file_lock(file, name)) {
		*success = FALSE;
		close(file);
		file = -1;
#endif
	} else {
		*success = TRUE;
	}

	return(file);
#endif /* __WIN__ */
}

/****************************************************************//**
A simple function to open or create a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INTERN
os_file_t
os_file_create_simple_no_error_handling(
/*====================================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: OS_FILE_OPEN if an existing file
				is opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error) */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY,
				OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file */
	ibool*		success)/*!< out: TRUE if succeed, FALSE if error */
{
#ifdef __WIN__
	os_file_t	file;
	DWORD		create_flag;
	DWORD		access;
	DWORD		attributes	= 0;
	DWORD		share_mode	= FILE_SHARE_READ | FILE_SHARE_WRITE;

	ut_a(name);

	if (create_mode == OS_FILE_OPEN) {
		create_flag = OPEN_EXISTING;
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = CREATE_NEW;
	} else {
		create_flag = 0;
		ut_error;
	}

	if (access_type == OS_FILE_READ_ONLY) {
		access = GENERIC_READ;
	} else if (access_type == OS_FILE_READ_WRITE) {
		access = GENERIC_READ | GENERIC_WRITE;
	} else if (access_type == OS_FILE_READ_ALLOW_DELETE) {
		access = GENERIC_READ;
		share_mode = FILE_SHARE_DELETE | FILE_SHARE_READ
			| FILE_SHARE_WRITE;	/*!< A backup program has to give
						mysqld the maximum freedom to
						do what it likes with the
						file */
	} else {
		access = 0;
		ut_error;
	}

	file = CreateFile((LPCTSTR) name,
			  access,
			  share_mode,
			  NULL,	/* default security attributes */
			  create_flag,
			  attributes,
			  NULL);	/*!< no template file */

	if (file == INVALID_HANDLE_VALUE) {
		*success = FALSE;
	} else {
		*success = TRUE;
	}

	return(file);
#else /* __WIN__ */
	os_file_t	file;
	int		create_flag;

	ut_a(name);

	if (create_mode == OS_FILE_OPEN) {
		if (access_type == OS_FILE_READ_ONLY) {
			create_flag = O_RDONLY;
		} else {
			create_flag = O_RDWR;
		}
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = O_RDWR | O_CREAT | O_EXCL;
	} else {
		create_flag = 0;
		ut_error;
	}

	if (create_mode == OS_FILE_CREATE) {
		file = open(name, create_flag, S_IRUSR | S_IWUSR
			    | S_IRGRP | S_IWGRP);
	} else {
		file = open(name, create_flag);
	}

	if (file == -1) {
		*success = FALSE;
#ifdef USE_FILE_LOCK
	} else if (access_type == OS_FILE_READ_WRITE
		   && os_file_lock(file, name)) {
		*success = FALSE;
		close(file);
		file = -1;
#endif
	} else {
		*success = TRUE;
	}

	return(file);
#endif /* __WIN__ */
}

/****************************************************************//**
Tries to disable OS caching on an opened file descriptor. */
UNIV_INTERN
void
os_file_set_nocache(
/*================*/
	int		fd		/*!< in: file descriptor to alter */
	__attribute__((unused)),
	const char*	file_name	/*!< in: used in the diagnostic message */
	__attribute__((unused)),
	const char*	operation_name __attribute__((unused)))
					/*!< in: "open" or "create"; used in the
					diagnostic message */
{
	/* some versions of Solaris may not have DIRECTIO_ON */
#if defined(UNIV_SOLARIS) && defined(DIRECTIO_ON)
	if (directio(fd, DIRECTIO_ON) == -1) {
		int	errno_save;
		errno_save = (int)errno;
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Failed to set DIRECTIO_ON "
			"on file %s: %s: %s, continuing anyway\n",
			file_name, operation_name, strerror(errno_save));
	}
#elif defined(O_DIRECT)
	if (fcntl(fd, F_SETFL, O_DIRECT) == -1) {
		int	errno_save;
		errno_save = (int)errno;
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Failed to set O_DIRECT "
			"on file %s: %s: %s, continuing anyway\n",
			file_name, operation_name, strerror(errno_save));
		if (errno_save == EINVAL) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: O_DIRECT is known to result in "
				"'Invalid argument' on Linux on tmpfs, "
				"see MySQL Bug#26662\n");
		}
	}
#endif
}

/****************************************************************//**
Opens an existing file or creates a new.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INTERN
os_file_t
os_file_create(
/*===========*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: OS_FILE_OPEN if an existing file
				is opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error),
				OS_FILE_OVERWRITE if a new file is created
				or an old overwritten;
				OS_FILE_OPEN_RAW, if a raw device or disk
				partition should be opened */
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
#ifdef __WIN__
	os_file_t	file;
	DWORD		share_mode	= FILE_SHARE_READ;
	DWORD		create_flag;
	DWORD		attributes;
	ibool		retry;
try_again:
	ut_a(name);

	if (create_mode == OS_FILE_OPEN_RAW) {
		create_flag = OPEN_EXISTING;
		share_mode = FILE_SHARE_WRITE;
	} else if (create_mode == OS_FILE_OPEN
		   || create_mode == OS_FILE_OPEN_RETRY) {
		create_flag = OPEN_EXISTING;
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = CREATE_NEW;
	} else if (create_mode == OS_FILE_OVERWRITE) {
		create_flag = CREATE_ALWAYS;
	} else {
		create_flag = 0;
		ut_error;
	}

	if (purpose == OS_FILE_AIO) {
		/* If specified, use asynchronous (overlapped) io and no
		buffering of writes in the OS */
		attributes = 0;
#ifdef WIN_ASYNC_IO
		if (os_aio_use_native_aio) {
			attributes = attributes | FILE_FLAG_OVERLAPPED;
		}
#endif
#ifdef UNIV_NON_BUFFERED_IO
# ifndef UNIV_HOTBACKUP
		if (type == OS_LOG_FILE && srv_flush_log_at_trx_commit == 2) {
			/* Do not use unbuffered i/o to log files because
			value 2 denotes that we do not flush the log at every
			commit, but only once per second */
		} else if (srv_win_file_flush_method
			   == SRV_WIN_IO_UNBUFFERED) {
			attributes = attributes | FILE_FLAG_NO_BUFFERING;
		}
# else /* !UNIV_HOTBACKUP */
		attributes = attributes | FILE_FLAG_NO_BUFFERING;
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_NON_BUFFERED_IO */
	} else if (purpose == OS_FILE_NORMAL) {
		attributes = 0;
#ifdef UNIV_NON_BUFFERED_IO
# ifndef UNIV_HOTBACKUP
		if (type == OS_LOG_FILE && srv_flush_log_at_trx_commit == 2) {
			/* Do not use unbuffered i/o to log files because
			value 2 denotes that we do not flush the log at every
			commit, but only once per second */
		} else if (srv_win_file_flush_method
			   == SRV_WIN_IO_UNBUFFERED) {
			attributes = attributes | FILE_FLAG_NO_BUFFERING;
		}
# else /* !UNIV_HOTBACKUP */
		attributes = attributes | FILE_FLAG_NO_BUFFERING;
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_NON_BUFFERED_IO */
	} else {
		attributes = 0;
		ut_error;
	}

	if (type == OS_LOG_FILE) {
		if (srv_unix_file_flush_method == SRV_UNIX_O_DSYNC) {
			/* Map O_DSYNC to WRITE_THROUGH */
			attributes |= FILE_FLAG_WRITE_THROUGH;
		} else if (srv_unix_file_flush_method == SRV_UNIX_ALL_O_DIRECT) {
			/* Open log file without buffering */
			attributes |= FILE_FLAG_NO_BUFFERING;
		}
	}

	file = CreateFile((LPCTSTR) name,
			  GENERIC_READ | GENERIC_WRITE, /* read and write
							access */
			  share_mode,	/* File can be read also by other
					processes; we must give the read
					permission because of ibbackup. We do
					not give the write permission to
					others because if one would succeed to
					start 2 instances of mysqld on the
					SAME files, that could cause severe
					database corruption! When opening
					raw disk partitions, Microsoft manuals
					say that we must give also the write
					permission. */
			  NULL,	/* default security attributes */
			  create_flag,
			  attributes,
			  NULL);	/*!< no template file */

	if (file == INVALID_HANDLE_VALUE) {
		*success = FALSE;

		/* When srv_file_per_table is on, file creation failure may not
		be critical to the whole instance. Do not crash the server in
		case of unknown errors.
		Please note "srv_file_per_table" is a global variable with
		no explicit synchronization protection. It could be
		changed during this execution path. It might not have the
		same value as the one when building the table definition */
		if (srv_file_per_table) {
			retry = os_file_handle_error_no_exit(name,
						create_mode == OS_FILE_CREATE ?
						"create" : "open");
		} else {
			retry = os_file_handle_error(name,
						create_mode == OS_FILE_CREATE ?
						"create" : "open");
		}

		if (retry) {
			goto try_again;
		}
	} else {
		*success = TRUE;
		if (os_aio_use_native_aio && ((attributes & FILE_FLAG_OVERLAPPED) != 0)) {
			ut_a(CreateIoCompletionPort(file, completion_port, 0, 0));
		}
	}

	return(file);
#else /* __WIN__ */
	os_file_t	file;
	int		create_flag;
	ibool		retry;
	const char*	mode_str	= NULL;

try_again:
	ut_a(name);

	if (create_mode == OS_FILE_OPEN || create_mode == OS_FILE_OPEN_RAW
	    || create_mode == OS_FILE_OPEN_RETRY) {
		mode_str = "OPEN";
		create_flag = O_RDWR;
	} else if (create_mode == OS_FILE_CREATE) {
		mode_str = "CREATE";
		create_flag = O_RDWR | O_CREAT | O_EXCL;
	} else if (create_mode == OS_FILE_OVERWRITE) {
		mode_str = "OVERWRITE";
		create_flag = O_RDWR | O_CREAT | O_TRUNC;
	} else {
		create_flag = 0;
		ut_error;
	}

	ut_a(type == OS_LOG_FILE || type == OS_DATA_FILE);
	ut_a(purpose == OS_FILE_AIO || purpose == OS_FILE_NORMAL);

#ifdef O_SYNC
	/* We let O_SYNC only affect log files; note that we map O_DSYNC to
	O_SYNC because the datasync options seemed to corrupt files in 2001
	in both Linux and Solaris */
	if (type == OS_LOG_FILE
	    && srv_unix_file_flush_method == SRV_UNIX_O_DSYNC) {

# if 0
		fprintf(stderr, "Using O_SYNC for file %s\n", name);
# endif

		create_flag = create_flag | O_SYNC;
	}
#endif /* O_SYNC */

	file = open(name, create_flag, os_innodb_umask);

	if (file == -1) {
		*success = FALSE;

		/* When srv_file_per_table is on, file creation failure may not
		be critical to the whole instance. Do not crash the server in
		case of unknown errors.
		Please note "srv_file_per_table" is a global variable with
		no explicit synchronization protection. It could be
		changed during this execution path. It might not have the
		same value as the one when building the table definition */
		if (srv_file_per_table) {
			retry = os_file_handle_error_no_exit(name,
						create_mode == OS_FILE_CREATE ?
						"create" : "open");
		} else {
			retry = os_file_handle_error(name,
						create_mode == OS_FILE_CREATE ?
						"create" : "open");
		}

		if (retry) {
			goto try_again;
		} else {
			return(file /* -1 */);
		}
	}
	/* else */

	*success = TRUE;

	/* We disable OS caching (O_DIRECT) only on data files */
	if (type != OS_LOG_FILE
	    && srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		
		os_file_set_nocache(file, name, mode_str);
	}

	/* ALL_O_DIRECT: O_DIRECT also for transaction log file */
	if (srv_unix_file_flush_method == SRV_UNIX_ALL_O_DIRECT) {
		os_file_set_nocache(file, name, mode_str);
	}

#ifdef USE_FILE_LOCK
	if (create_mode != OS_FILE_OPEN_RAW && os_file_lock(file, name)) {

		if (create_mode == OS_FILE_OPEN_RETRY) {
			int i;
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Retrying to lock"
			      " the first data file\n",
			      stderr);
			for (i = 0; i < 100; i++) {
				os_thread_sleep(1000000);
				if (!os_file_lock(file, name)) {
					*success = TRUE;
					return(file);
				}
			}
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Unable to open the first data file\n",
			      stderr);
		}

		*success = FALSE;
		close(file);
		file = -1;
	}
#endif /* USE_FILE_LOCK */

	return(file);
#endif /* __WIN__ */
}

/***********************************************************************//**
Deletes a file if it exists. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_delete_if_exists(
/*=====================*/
	const char*	name)	/*!< in: file path as a null-terminated string */
{
#ifdef __WIN__
	BOOL	ret;
	ulint	count	= 0;
loop:
	/* In Windows, deleting an .ibd file may fail if ibbackup is copying
	it */

	ret = DeleteFile((LPCTSTR)name);

	if (ret) {
		return(TRUE);
	}

	if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		/* the file does not exist, this not an error */

		return(TRUE);
	}

	count++;

	if (count > 100 && 0 == (count % 10)) {
		fprintf(stderr,
			"InnoDB: Warning: cannot delete file %s\n"
			"InnoDB: Are you running ibbackup"
			" to back up the file?\n", name);

		os_file_get_last_error(TRUE); /* print error information */
	}

	os_thread_sleep(1000000);	/* sleep for a second */

	if (count > 2000) {

		return(FALSE);
	}

	goto loop;
#else
	int	ret;

	ret = unlink(name);

	if (ret != 0 && errno != ENOENT) {
		os_file_handle_error_no_exit(name, "delete");

		return(FALSE);
	}

	return(TRUE);
#endif
}

/***********************************************************************//**
Deletes a file. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_delete(
/*===========*/
	const char*	name)	/*!< in: file path as a null-terminated string */
{
#ifdef __WIN__
	BOOL	ret;
	ulint	count	= 0;
loop:
	/* In Windows, deleting an .ibd file may fail if ibbackup is copying
	it */

	ret = DeleteFile((LPCTSTR)name);

	if (ret) {
		return(TRUE);
	}

	if (GetLastError() == ERROR_FILE_NOT_FOUND) {
		/* If the file does not exist, we classify this as a 'mild'
		error and return */

		return(FALSE);
	}

	count++;

	if (count > 100 && 0 == (count % 10)) {
		fprintf(stderr,
			"InnoDB: Warning: cannot delete file %s\n"
			"InnoDB: Are you running ibbackup"
			" to back up the file?\n", name);

		os_file_get_last_error(TRUE); /* print error information */
	}

	os_thread_sleep(1000000);	/* sleep for a second */

	if (count > 2000) {

		return(FALSE);
	}

	goto loop;
#else
	int	ret;

	ret = unlink(name);

	if (ret != 0) {
		os_file_handle_error_no_exit(name, "delete");

		return(FALSE);
	}

	return(TRUE);
#endif
}

/***********************************************************************//**
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_rename(
/*===========*/
	const char*	oldpath,/*!< in: old file path as a null-terminated
				string */
	const char*	newpath)/*!< in: new file path */
{
#ifdef __WIN__
	BOOL	ret;

	ret = MoveFile((LPCTSTR)oldpath, (LPCTSTR)newpath);

	if (ret) {
		return(TRUE);
	}

	os_file_handle_error_no_exit(oldpath, "rename");

	return(FALSE);
#else
	int	ret;

	ret = rename(oldpath, newpath);

	if (ret != 0) {
		os_file_handle_error_no_exit(oldpath, "rename");

		return(FALSE);
	}

	return(TRUE);
#endif
}

/***********************************************************************//**
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_close(
/*==========*/
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
#endif
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
#endif
}
#endif /* UNIV_HOTBACKUP */

/***********************************************************************//**
Gets a file size.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_get_size(
/*=============*/
	os_file_t	file,	/*!< in: handle to a file */
	ulint*		size,	/*!< out: least significant 32 bits of file
				size */
	ulint*		size_high)/*!< out: most significant 32 bits of size */
{
#ifdef __WIN__
	DWORD	high;
	DWORD	low;

	low = GetFileSize(file, &high);

	if ((low == 0xFFFFFFFF) && (GetLastError() != NO_ERROR)) {
		return(FALSE);
	}

	*size = low;
	*size_high = high;

	return(TRUE);
#else
	off_t	offs;

	offs = lseek(file, 0, SEEK_END);

	if (offs == ((off_t)-1)) {

		return(FALSE);
	}

	if (sizeof(off_t) > 4) {
		*size = (ulint)(offs & 0xFFFFFFFFUL);
		*size_high = (ulint)(offs >> 32);
	} else {
		*size = (ulint) offs;
		*size_high = 0;
	}

	return(TRUE);
#endif
}

/***********************************************************************//**
Gets file size as a 64-bit integer ib_int64_t.
@return	size in bytes, -1 if error */
UNIV_INTERN
ib_int64_t
os_file_get_size_as_iblonglong(
/*===========================*/
	os_file_t	file)	/*!< in: handle to a file */
{
	ulint	size;
	ulint	size_high;
	ibool	success;

	success = os_file_get_size(file, &size, &size_high);

	if (!success) {

		return(-1);
	}

	return((((ib_int64_t)size_high) << 32) + (ib_int64_t)size);
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
	ulint		size,	/*!< in: least significant 32 bits of file
				size */
	ulint		size_high)/*!< in: most significant 32 bits of size */
{
	ib_int64_t	current_size;
	ib_int64_t	desired_size;
	ibool		ret;
	byte*		buf;
	byte*		buf2;
	ulint		buf_size;

	ut_a(size == (size & 0xFFFFFFFF));

	current_size = 0;
	desired_size = (ib_int64_t)size + (((ib_int64_t)size_high) << 32);

	/* Write up to 1 megabyte at a time. */
	buf_size = ut_min(64, (ulint) (desired_size / UNIV_PAGE_SIZE))
		* UNIV_PAGE_SIZE;
	buf2 = ut_malloc(buf_size + UNIV_PAGE_SIZE);

	/* Align the buffer for possible raw i/o */
	buf = ut_align(buf2, UNIV_PAGE_SIZE);

	/* Write buffer full of zeros */
	memset(buf, 0, buf_size);

	if (desired_size >= (ib_int64_t)(100 * 1024 * 1024)) {

		fprintf(stderr, "InnoDB: Progress in MB:");
	}

	while (current_size < desired_size) {
		ulint	n_bytes;

		if (desired_size - current_size < (ib_int64_t) buf_size) {
			n_bytes = (ulint) (desired_size - current_size);
		} else {
			n_bytes = buf_size;
		}

		ret = os_file_write(name, file, buf,
				    (ulint)(current_size & 0xFFFFFFFF),
				    (ulint)(current_size >> 32),
				    n_bytes);
		if (!ret) {
			ut_free(buf2);
			goto error_handling;
		}

		/* Print about progress for each 100 MB written */
		if ((ib_int64_t) (current_size + n_bytes) / (ib_int64_t)(100 * 1024 * 1024)
		    != current_size / (ib_int64_t)(100 * 1024 * 1024)) {

			fprintf(stderr, " %lu00",
				(ulong) ((current_size + n_bytes)
					 / (ib_int64_t)(100 * 1024 * 1024)));
		}

		current_size += n_bytes;
	}

	if (desired_size >= (ib_int64_t)(100 * 1024 * 1024)) {

		fprintf(stderr, "\n");
	}

	ut_free(buf2);

	ret = os_file_flush(file, TRUE);

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

/***********************************************************************//**
Truncates a file at the specified position.
@return TRUE if success */
UNIV_INTERN
ibool
os_file_set_eof_at(
	os_file_t	file, /*!< in: handle to a file */
	ib_uint64_t	new_len)/*!< in: new file length */
{
#ifdef __WIN__
	/* TODO: untested! */
	return(!_chsize_s(file, new_len));
#else
	/* TODO: works only with -D_FILE_OFFSET_BITS=64 ? */
	return(!ftruncate(file, new_len));
#endif
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
	os_file_t	file,	/*!< in: handle to a file */
	ibool		metadata)
{
	int	ret;
	int	failures;
	ibool	retry;

	failures = 0;

	do {
#if defined(HAVE_FDATASYNC) && HAVE_DECL_FDATASYNC
		if (metadata) {
			ret = fsync(file);
		} else {
			ret = fdatasync(file);
		}
#else
		(void) metadata;
		ret = fsync(file);
#endif

		os_n_fsyncs++;

		if (ret == -1 && errno == ENOLCK) {

			if (failures % 100 == 0) {

				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: fsync(): "
					"No locks available; retrying\n");
			}

			os_thread_sleep(200000 /* 0.2 sec */);

			failures++;

			retry = TRUE;
		} else if (ret == -1 && errno == EINTR) {
			/* Handle signal interruptions correctly */
			retry = TRUE;
		} else {

			retry = FALSE;
		}
	} while (retry);

	return(ret);
}
#endif /* !__WIN__ */

/***********************************************************************//**
Flushes the write buffers of a given file to the disk.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_flush(
/*==========*/
	os_file_t	file,	/*!< in, own: handle to a file */
	ibool		metadata)
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

		ret = os_file_fsync(file, metadata);
	} else {
		ret = fcntl(file, F_FULLFSYNC, NULL);

		if (ret) {
			/* If we are not on a file system that supports this,
			then fall back to a plain fsync. */
			ret = os_file_fsync(file, metadata);
		}
	}
#else
	ret = os_file_fsync(file, metadata);
#endif

	if (ret == 0) {
		return(TRUE);
	}

	/* Since Linux returns EINVAL if the 'file' is actually a raw device,
	we choose to ignore that error if we are using raw disks */

	if (srv_start_raw_disk_in_use && errno == EINVAL) {

		return(TRUE);
	}

	ut_print_timestamp(stderr);

	fprintf(stderr,
		"  InnoDB: Error: the OS said file flush did not succeed\n");

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
#define os_file_pread(file, buf, n, offset, offset_high)        \
		_os_file_pread(file, buf, n, offset, offset_high, NULL);

static
ssize_t
_os_file_pread(
/*==========*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	ulint		n,	/*!< in: number of bytes to read */
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset from where to read */
	ulint		offset_high, /*!< in: most significant 32 bits of
				offset */
	trx_t*		trx)
{
	off_t	offs;
#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	ssize_t	n_bytes;
	ssize_t n_read;
#endif /* HAVE_PREAD && !HAVE_BROKEN_PREAD */
	ulint		sec;
	ulint		ms;
	ib_uint64_t	start_time;
	ib_uint64_t	finish_time;

	ut_a((offset & 0xFFFFFFFFUL) == offset);

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */

	if (sizeof(off_t) > 4) {
		offs = (off_t)offset + (((off_t)offset_high) << 32);

	} else {
		offs = (off_t)offset;

		if (offset_high > 0) {
			fprintf(stderr,
				"InnoDB: Error: file read at offset > 4 GB\n");
		}
	}

	os_n_file_reads++;

	if (innobase_get_slow_log() && trx && trx->take_stats)
	{
	        trx->io_reads++;
		trx->io_read += n;
		ut_usectime(&sec, &ms);
		start_time = (ib_uint64_t)sec * 1000000 + ms;
	} else {
		start_time = 0;
	}
#if defined(HAVE_PREAD) && !defined(HAVE_BROKEN_PREAD)
	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_preads++;
	os_n_pending_reads++;
	os_mutex_exit(os_file_count_mutex);

	/* Handle signal interruptions correctly */
	for (n_bytes = 0; n_bytes < (ssize_t) n; ) {
		n_read = pread(file, buf, (ssize_t)n, offs);
		if (n_read > 0) {
			n_bytes += n_read;
			offs += n_read;
		} else if (n_read == -1 && errno == EINTR) {
			continue;
		} else {
			break;
		}
	}

	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_preads--;
	os_n_pending_reads--;
	os_mutex_exit(os_file_count_mutex);

	if (innobase_get_slow_log() && trx && trx->take_stats && start_time)
	{
		ut_usectime(&sec, &ms);
		finish_time = (ib_uint64_t)sec * 1000000 + ms;
		trx->io_reads_wait_timer += (ulint)(finish_time - start_time);
	}

	return(n_bytes);
#else
	{
		off_t	ret_offset;
		ssize_t	ret;
		ssize_t n_read;
#ifndef UNIV_HOTBACKUP
		ulint	i;
#endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_reads++;
		os_mutex_exit(os_file_count_mutex);

#ifndef UNIV_HOTBACKUP
		/* Protect the seek / read operation with a mutex */
		i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;

		os_mutex_enter(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		ret_offset = lseek(file, offs, SEEK_SET);

		if (ret_offset < 0) {
			ret = -1;
		} else {
			/* Handle signal interruptions correctly */
			for (ret = 0; ret < (ssize_t) n; ) {
				n_read = read(file, buf, (ssize_t)n);
				if (n_read > 0) {
					ret += n_read;
				} else if (n_read == -1 && errno == EINTR) {
					continue;
				} else {
					break;
				}
			}
		}

#ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
#endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_reads--;
		os_mutex_exit(os_file_count_mutex);

		if (innobase_get_slow_log() && trx && trx->take_stats && start_time)
		{
			ut_usectime(&sec, &ms);
			finish_time = (ib_uint64_t)sec * 1000000 + ms;
			trx->io_reads_wait_timer += (ulint)(finish_time - start_time);
		}

		return(ret);
	}
#endif
}

/*******************************************************************//**
Does a synchronous write operation in Posix.
@return	number of bytes written, -1 if error */
static
ssize_t
os_file_pwrite(
/*===========*/
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from where to write */
	ulint		n,	/*!< in: number of bytes to write */
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset where to write */
	ulint		offset_high) /*!< in: most significant 32 bits of
				offset */
{
	ssize_t	ret;
	ssize_t n_written;
	off_t	offs;

	ut_a((offset & 0xFFFFFFFFUL) == offset);

	/* If off_t is > 4 bytes in size, then we assume we can pass a
	64-bit address */

	if (sizeof(off_t) > 4) {
		offs = (off_t)offset + (((off_t)offset_high) << 32);
	} else {
		offs = (off_t)offset;

		if (offset_high > 0) {
			fprintf(stderr,
				"InnoDB: Error: file write"
				" at offset > 4 GB\n");
		}
	}

	os_n_file_writes++;

#if defined(HAVE_PWRITE) && !defined(HAVE_BROKEN_PREAD)
	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_pwrites++;
	os_n_pending_writes++;
	os_mutex_exit(os_file_count_mutex);

	/* Handle signal interruptions correctly */
	for (ret = 0; ret < (ssize_t) n; ) {
		n_written = pwrite(file, buf, (ssize_t)n, offs);
		if (n_written > 0) {
			ret += n_written;
			offs += n_written;
		} else if (n_written == -1 && errno == EINTR) {
			continue;
		} else {
			break;
		}
	}

	os_mutex_enter(os_file_count_mutex);
	os_file_n_pending_pwrites--;
	os_n_pending_writes--;
	os_mutex_exit(os_file_count_mutex);

# ifdef UNIV_DO_FLUSH
	if (srv_unix_file_flush_method != SRV_UNIX_LITTLESYNC
	    && srv_unix_file_flush_method != SRV_UNIX_NOSYNC
	    && !os_do_not_call_flush_at_each_write) {

		/* Always do fsync to reduce the probability that when
		the OS crashes, a database page is only partially
		physically written to disk. */

		ut_a(TRUE == os_file_flush(file, TRUE));
	}
# endif /* UNIV_DO_FLUSH */

	return(ret);
#else
	{
		off_t	ret_offset;
# ifndef UNIV_HOTBACKUP
		ulint	i;
# endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_writes++;
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

		/* Handle signal interruptions correctly */
		for (ret = 0; ret < (ssize_t) n; ) {
			n_written = write(file, buf, (ssize_t)n);
			if (n_written > 0) {
				ret += n_written;
			} else if (n_written == -1 && errno == EINTR) {
				continue;
			} else {
				break;
			}
		}

# ifdef UNIV_DO_FLUSH
		if (srv_unix_file_flush_method != SRV_UNIX_LITTLESYNC
		    && srv_unix_file_flush_method != SRV_UNIX_NOSYNC
		    && !os_do_not_call_flush_at_each_write) {

			/* Always do fsync to reduce the probability that when
			the OS crashes, a database page is only partially
			physically written to disk. */

			ut_a(TRUE == os_file_flush(file, TRUE));
		}
# endif /* UNIV_DO_FLUSH */

func_exit:
# ifndef UNIV_HOTBACKUP
		os_mutex_exit(os_file_seek_mutexes[i]);
# endif /* !UNIV_HOTBACKUP */

		os_mutex_enter(os_file_count_mutex);
		os_n_pending_writes--;
		os_mutex_exit(os_file_count_mutex);

		return(ret);
	}
#endif
}
#endif

/*******************************************************************//**
Requests a synchronous positioned read operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
_os_file_read(
/*=========*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high, /*!< in: most significant 32 bits of
				offset */
	ulint		n,	/*!< in: number of bytes to read */
	trx_t*		trx)
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	ibool		retry;
	OVERLAPPED overlapped;


	/* On 64-bit Windows, ulint is 64 bits. But offset and n should be
	no more than 32 bits. */
	ut_a((offset & 0xFFFFFFFFUL) == offset);
	ut_a((n & 0xFFFFFFFFUL) == n);

	os_n_file_reads++;
	os_bytes_read_since_printout += n;

try_again:
	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads++;
	os_mutex_exit(os_file_count_mutex);

	memset (&overlapped, 0, sizeof (overlapped));
	overlapped.Offset = (DWORD)offset;
	overlapped.OffsetHigh = (DWORD)offset_high;
	overlapped.hEvent = win_get_syncio_event();
	ret = ReadFile(file, buf, n, NULL, &overlapped);
	if (ret) {
		ret = GetOverlappedResult(file, &overlapped, (DWORD *)&len, FALSE);
	}
	else if(GetLastError() == ERROR_IO_PENDING) {
		ret = GetOverlappedResult(file, &overlapped, (DWORD *)&len, TRUE);
  }
	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads--;
	os_mutex_exit(os_file_count_mutex);

	if (ret && len == n) {
		return(TRUE);
	}
#else /* __WIN__ */
	ibool	retry;
	ssize_t	ret;

	os_bytes_read_since_printout += n;

try_again:
	ret = _os_file_pread(file, buf, n, offset, offset_high, trx);

	if ((ulint)ret == n) {

		return(TRUE);
	}

	fprintf(stderr,
		"InnoDB: Error: tried to read %lu bytes at offset %lu %lu.\n"
		"InnoDB: Was only able to read %ld.\n",
		(ulong)n, (ulong)offset_high,
		(ulong)offset, (long)ret);
#endif /* __WIN__ */
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
#endif
		);
	fflush(stderr);

	ut_error;

	return(FALSE);
}

/*******************************************************************//**
Requests a synchronous positioned read operation. This function does not do
any error handling. In case of error it returns FALSE.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
os_file_read_no_error_handling(
/*===========================*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high, /*!< in: most significant 32 bits of
				offset */
	ulint		n)	/*!< in: number of bytes to read */
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	ibool		retry;
	OVERLAPPED overlapped;
	overlapped.Offset = (DWORD)offset;
	overlapped.OffsetHigh = (DWORD)offset_high;


	/* On 64-bit Windows, ulint is 64 bits. But offset and n should be
	no more than 32 bits. */
	ut_a((offset & 0xFFFFFFFFUL) == offset);
	ut_a((n & 0xFFFFFFFFUL) == n);

	os_n_file_reads++;
	os_bytes_read_since_printout += n;

try_again:
	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads++;
	os_mutex_exit(os_file_count_mutex);

	memset (&overlapped, 0, sizeof (overlapped));
	overlapped.Offset = (DWORD)offset;
	overlapped.OffsetHigh = (DWORD)offset_high;
	overlapped.hEvent = win_get_syncio_event();
	ret = ReadFile(file, buf, n, NULL, &overlapped);
	if (ret) {
		ret = GetOverlappedResult(file, &overlapped, (DWORD *)&len, FALSE);
	}
	else if(GetLastError() == ERROR_IO_PENDING) {
		ret = GetOverlappedResult(file, &overlapped, (DWORD *)&len, TRUE);
  }
	os_mutex_enter(os_file_count_mutex);
	os_n_pending_reads--;
	os_mutex_exit(os_file_count_mutex);

	if (ret && len == n) {
		return(TRUE);
	}
#else /* __WIN__ */
	ibool	retry;
	ssize_t	ret;

	os_bytes_read_since_printout += n;

try_again:
	ret = os_file_pread(file, buf, n, offset, offset_high);

	if ((ulint)ret == n) {

		return(TRUE);
	}
#endif /* __WIN__ */
	retry = os_file_handle_error_no_exit(NULL, "read");

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
Requests a synchronous write operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
os_file_write(
/*==========*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from which to write */
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset where to write */
	ulint		offset_high, /*!< in: most significant 32 bits of
				offset */
	ulint		n)	/*!< in: number of bytes to write */
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	ulint		n_retries	= 0;
	ulint		err;
	OVERLAPPED overlapped;

	/* On 64-bit Windows, ulint is 64 bits. But offset and n should be
	no more than 32 bits. */
	ut_a((offset & 0xFFFFFFFFUL) == offset);
	ut_a((n & 0xFFFFFFFFUL) == n);

	os_n_file_writes++;

	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);
retry:

	os_mutex_enter(os_file_count_mutex);
	os_n_pending_writes++;
	os_mutex_exit(os_file_count_mutex);

	memset (&overlapped, 0, sizeof (overlapped));
	overlapped.Offset = (DWORD)offset;
	overlapped.OffsetHigh = (DWORD)offset_high;

	overlapped.hEvent = win_get_syncio_event();
	ret = WriteFile(file, buf, n, NULL, &overlapped);
	if (ret) {
		ret = GetOverlappedResult(file, &overlapped, (DWORD *)&len, FALSE);
	}
	else if(GetLastError() == ERROR_IO_PENDING) {
		ret = GetOverlappedResult(file, &overlapped, (DWORD *)&len, TRUE);
	}
	os_mutex_enter(os_file_count_mutex);
	os_n_pending_writes--;
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

		err = (ulint)GetLastError();

		ut_print_timestamp(stderr);

		fprintf(stderr,
			"  InnoDB: Error: Write to file %s failed"
			" at offset %lu %lu.\n"
			"InnoDB: %lu bytes should have been written,"
			" only %lu were written.\n"
			"InnoDB: Operating system error number %lu.\n"
			"InnoDB: Check that your OS and file system"
			" support files of this size.\n"
			"InnoDB: Check also that the disk is not full"
			" or a disk quota exceeded.\n",
			name, (ulong) offset_high, (ulong) offset,
			(ulong) n, ret ? len : 0, (ulong) err);

		if (strerror((int)err) != NULL) {
			fprintf(stderr,
				"InnoDB: Error number %lu means '%s'.\n",
				(ulong) err, strerror((int)err));
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

	ret = os_file_pwrite(file, buf, n, offset, offset_high);

	if ((ulint)ret == n) {

		return(TRUE);
	}

	if (!os_has_said_disk_full) {

		ut_print_timestamp(stderr);

		fprintf(stderr,
			"  InnoDB: Error: Write to file %s failed"
			" at offset %lu %lu.\n"
			"InnoDB: %lu bytes should have been written,"
			" only %ld were written.\n"
			"InnoDB: Operating system error number %lu.\n"
			"InnoDB: Check that your OS and file system"
			" support files of this size.\n"
			"InnoDB: Check also that the disk is not full"
			" or a disk quota exceeded.\n",
			name, offset_high, offset, n, (long int)ret,
			(ulint)errno);
		if (strerror(errno) != NULL) {
			fprintf(stderr,
				"InnoDB: Error number %lu means '%s'.\n",
				(ulint)errno, strerror(errno));
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
	const char*	path,	/*!< in:	pathname of the file */
	ibool*		exists,	/*!< out: TRUE if file exists */
	os_file_type_t* type)	/*!< out: type of the file (if it exists) */
{
#ifdef __WIN__
	int		ret;
	struct _stat	statinfo;

	ret = _stat(path, &statinfo);
	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */
		*exists = FALSE;
		return(TRUE);
	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat");

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

		os_file_handle_error_no_exit(path, "stat");

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
@return	TRUE if stat information found */
UNIV_INTERN
ibool
os_file_get_status(
/*===============*/
	const char*	path,		/*!< in:	pathname of the file */
	os_file_stat_t* stat_info)	/*!< information of a file in a
					directory */
{
#ifdef __WIN__
	int		ret;
	struct _stat	statinfo;

	ret = _stat(path, &statinfo);
	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(FALSE);
	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat");

		return(FALSE);
	}
	if (_S_IFDIR & statinfo.st_mode) {
		stat_info->type = OS_FILE_TYPE_DIR;
	} else if (_S_IFREG & statinfo.st_mode) {
		stat_info->type = OS_FILE_TYPE_FILE;
	} else {
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}

	stat_info->ctime = statinfo.st_ctime;
	stat_info->atime = statinfo.st_atime;
	stat_info->mtime = statinfo.st_mtime;
	stat_info->size	 = statinfo.st_size;

	return(TRUE);
#else
	int		ret;
	struct stat	statinfo;

	ret = stat(path, &statinfo);

	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(FALSE);
	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat");

		return(FALSE);
	}

	if (S_ISDIR(statinfo.st_mode)) {
		stat_info->type = OS_FILE_TYPE_DIR;
	} else if (S_ISLNK(statinfo.st_mode)) {
		stat_info->type = OS_FILE_TYPE_LINK;
	} else if (S_ISREG(statinfo.st_mode)) {
		stat_info->type = OS_FILE_TYPE_FILE;
	} else {
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}

	stat_info->ctime = statinfo.st_ctime;
	stat_info->atime = statinfo.st_atime;
	stat_info->mtime = statinfo.st_mtime;
	stat_info->size	 = statinfo.st_size;

	return(TRUE);
#endif
}

/* path name separator character */
#ifdef __WIN__
#  define OS_FILE_PATH_SEPARATOR	'\\'
#else
#  define OS_FILE_PATH_SEPARATOR	'/'
#endif

/****************************************************************//**
The function os_file_dirname returns a directory component of a
null-terminated pathname string.  In the usual case, dirname returns
the string up to, but not including, the final '/', and basename
is the component following the final '/'.  Trailing '/' charac­
ters are not counted as part of the pathname.

If path does not contain a slash, dirname returns the string ".".

Concatenating the string returned by dirname, a "/", and the basename
yields a complete pathname.

The return value is  a copy of the directory component of the pathname.
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
	char*		subdir;
	ibool		success, subdir_exists;
	os_file_type_t	type;

	subdir = os_file_dirname(path);
	if (strlen(subdir) == 1
	    && (*subdir == OS_FILE_PATH_SEPARATOR || *subdir == '.')) {
		/* subdir is root or cwd, nothing to do */
		mem_free(subdir);

		return(TRUE);
	}

	/* Test if subdir exists */
	success = os_file_status(subdir, &subdir_exists, &type);
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

	return((array->slots) + index);
}

/************************************************************************//**
Creates an aio wait array.
@return	own: aio array */
static
os_aio_array_t*
os_aio_array_create(
/*================*/
	ulint	n,		/*!< in: maximum number of pending aio operations
				allowed; n must be divisible by n_segments */
	ulint	n_segments)	/*!< in: number of segments in the aio array */
{
	os_aio_array_t*	array;
	ulint		i;
	os_aio_slot_t*	slot;
	ut_a(n > 0);
	ut_a(n_segments > 0);

	array = ut_malloc(sizeof(os_aio_array_t));

	array->mutex		= os_mutex_create(NULL);
	array->not_full		= os_event_create(NULL);
	array->is_empty		= os_event_create(NULL);

	os_event_set(array->is_empty);

	array->n_slots		= n;
	array->n_segments	= n_segments;
	array->n_reserved	= 0;
	array->slots		= ut_malloc(n * sizeof(os_aio_slot_t));

	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i);
		slot->pos = i;
		slot->reserved = FALSE;

	}

	return(array);
}

/************************************************************************//**
Frees an aio wait array. */
static
void
os_aio_array_free(
/*==============*/
	os_aio_array_t*	array)	/*!< in, own: array to free */
{

	os_mutex_free(array->mutex);
	os_event_free(array->not_full);
	os_event_free(array->is_empty);

	ut_free(array->slots);
	ut_free(array);
}

/***********************************************************************
Initializes the asynchronous io system. Creates one array each for ibuf
and log i/o. Also creates one array each for read and write where each
array is divided logically into n_read_segs and n_write_segs
respectively. The caller must create an i/o handler thread for each
segment in these arrays. This function also creates the sync array.
No i/o handler thread needs to be created for that */
UNIV_INTERN
void
os_aio_init(
/*========*/
	ulint	n_per_seg,	/*<! in: maximum number of pending aio
				operations allowed per segment */
	ulint	n_read_segs,	/*<! in: number of reader threads */
	ulint	n_write_segs,	/*<! in: number of writer threads */
	ulint	n_slots_sync)	/*<! in: number of slots in the sync aio
				array */
{
	ulint	i;
	ulint 	n_segments = 2 + n_read_segs + n_write_segs;

	ut_ad(n_segments >= 4);

	os_io_init_simple();

	for (i = 0; i < n_segments; i++) {
		srv_set_io_thread_op_info(i, "not started yet");
		os_aio_thread_buffer[i] = 0;
		os_aio_thread_buffer_size[i] = 0;
	}


	/* fprintf(stderr, "Array n per seg %lu\n", n_per_seg); */

	os_aio_first_write_segment = os_aio_first_read_segment + n_read_segs;
	os_aio_ibuf_array = os_aio_array_create(n_per_seg, 1);

	srv_io_thread_function[0] = "insert buffer thread";

	os_aio_log_array = os_aio_array_create(n_per_seg, 1);

	srv_io_thread_function[1] = "log thread";

	os_aio_read_array = os_aio_array_create(n_per_seg,
						n_read_segs);
	for (i = 2; i < 2 + n_read_segs; i++) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "read thread";
	}

	os_aio_write_array = os_aio_array_create(n_per_seg,
						 n_write_segs);
	for (i = 2 + n_read_segs; i < n_segments; i++) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "write thread";
	}

	os_aio_sync_array = os_aio_array_create(n_slots_sync, 1);

	os_aio_n_segments = n_segments;

	os_aio_validate();

	os_aio_segment_wait_events = ut_malloc(n_segments * sizeof(void*));

	for (i = 0; i < n_segments; i++) {
		os_aio_segment_wait_events[i] = os_event_create(NULL);
	}

	os_last_printout = time(NULL);
#ifdef _WIN32
	ut_a(completion_port == 0 && read_completion_port == 0);
	completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	read_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	ut_a(completion_port && read_completion_port);
#endif
}

/***********************************************************************
Frees the asynchronous io system. */
UNIV_INTERN
void
os_aio_free(void)
/*=============*/
{
	ulint	i;

	os_aio_array_free(os_aio_ibuf_array);
	os_aio_ibuf_array = NULL;
	os_aio_array_free(os_aio_log_array);
	os_aio_log_array = NULL;
	os_aio_array_free(os_aio_read_array);
	os_aio_read_array = NULL;
	os_aio_array_free(os_aio_write_array);
	os_aio_write_array = NULL;
	os_aio_array_free(os_aio_sync_array);
	os_aio_sync_array = NULL;

	for (i = 0; i < os_aio_n_segments; i++) {
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
	if(completion_port)
	{
		PostQueuedCompletionStatus(completion_port, 0, IOCP_SHUTDOWN_KEY, NULL);
		PostQueuedCompletionStatus(read_completion_port, 0, IOCP_SHUTDOWN_KEY, NULL);
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
	ulint	i;

#ifdef WIN_ASYNC_IO
	/* This code wakes up all ai/o threads in Windows native aio */
	os_aio_array_wake_win_aio_at_shutdown(os_aio_read_array);
	os_aio_array_wake_win_aio_at_shutdown(os_aio_write_array);
	os_aio_array_wake_win_aio_at_shutdown(os_aio_ibuf_array);
	os_aio_array_wake_win_aio_at_shutdown(os_aio_log_array);
#endif
	/* This loop wakes up all simulated ai/o threads */

	for (i = 0; i < os_aio_n_segments; i++) {

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
		segment = 0;

	} else if (array == os_aio_log_array) {
		segment = 1;

	} else if (array == os_aio_read_array) {
		seg_len = os_aio_read_array->n_slots
			/ os_aio_read_array->n_segments;

		segment = 2 + slot->pos / seg_len;
	} else {
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
	ulint	segment;

	ut_a(global_segment < os_aio_n_segments);

	if (global_segment == 0) {
		*array = os_aio_ibuf_array;
		segment = 0;

	} else if (global_segment == 1) {
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
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset */
	ulint		offset_high, /*!< in: most significant 32 bits of
				offset */
	ulint		len,	/*!< in: length of the block to read or write */
	ulint		space_id)
{
	os_aio_slot_t*	slot;
	ulint		i;
	ulint		slots_per_seg;
	ulint		local_seg;
#ifdef WIN_ASYNC_IO
	OVERLAPPED*	control;

	ut_a((len & 0xFFFFFFFFUL) == len);
#endif

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

		if (!os_aio_use_native_aio) {
			/* If the handler threads are suspended, wake them
			so that we get more slots */

			os_aio_simulated_wake_handler_threads();
		}

		os_event_wait(array->not_full);

		goto loop;
	}

	/* First try to find a slot in the preferred local segment */
	for (i = local_seg * slots_per_seg; i < array->n_slots; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved == FALSE) {
			goto found;
		}
	}

	/* Fall back to a full scan. We are guaranteed to find a slot */
	for (i = 0;; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved == FALSE) {
			goto found;
		}
	}

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
	slot->reservation_time = time(NULL);
	slot->message1 = message1;
	slot->message2 = message2;
	slot->file     = file;
	slot->name     = name;
	slot->len      = len;
	slot->type     = type;
	slot->buf      = buf;
	slot->offset   = offset;
	slot->offset_high = offset_high;
//	slot->io_already_done = FALSE;
	slot->status = OS_AIO_NOT_ISSUED;
	slot->space_id = space_id;

#ifdef WIN_ASYNC_IO
	control = &(slot->control);
	control->Offset = (DWORD)offset;
	control->OffsetHigh = (DWORD)offset_high;
	control->hEvent = 0;
	slot->arr = array;
#endif

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
	ut_ad(array);
	ut_ad(slot);

	os_mutex_enter(array->mutex);

	ut_ad(slot->reserved);

	slot->reserved = FALSE;
	slot->status = OS_AIO_NOT_ISSUED;

	array->n_reserved--;

	if (array->n_reserved == array->n_slots - 1) {
		os_event_set(array->not_full);
	}

	if (array->n_reserved == 0) {
		os_event_set(array->is_empty);
	}

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
	os_aio_slot_t*	slot;
	ulint		n;
	ulint		i;

	ut_ad(!os_aio_use_native_aio);

	os_aio_get_array_and_local_segment(&array, global_segment);

	n = array->n_slots;

	/* Look through n slots after the segment * n'th slot */

	os_mutex_enter(array->mutex);

	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved &&
		    (slot->status == OS_AIO_NOT_ISSUED ||
		     slot->status == OS_AIO_DONE)) {
			/* Found an i/o request */

			break;
		}
	}

	os_mutex_exit(array->mutex);

	if (i < n) {
		if (array == os_aio_ibuf_array) {
			os_event_set(os_aio_segment_wait_events[0]);

		} else if (array == os_aio_log_array) {
			os_event_set(os_aio_segment_wait_events[1]);

		} else if (array == os_aio_read_array) {
			ulint	x;
			for (x = os_aio_first_read_segment; x < os_aio_first_write_segment; x++)
				os_event_set(os_aio_segment_wait_events[x]);

		} else if (array == os_aio_write_array) {
			ulint	x;
			for (x = os_aio_first_write_segment; x < os_aio_n_segments; x++)
				os_event_set(os_aio_segment_wait_events[x]);

		} else {
			ut_a(0);
		}
	}
}

/**********************************************************************//**
Wakes up simulated aio i/o-handler threads if they have something to do. */
UNIV_INTERN
void
os_aio_simulated_wake_handler_threads(void)
/*=======================================*/
{
	if (os_aio_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= FALSE;

	os_aio_simulated_wake_handler_thread(0);
	os_aio_simulated_wake_handler_thread(1);
	os_aio_simulated_wake_handler_thread(os_aio_first_read_segment);
	os_aio_simulated_wake_handler_thread(os_aio_first_write_segment);
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
	ulint		g;

	if (os_aio_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= TRUE;

	for (g = 0; g < os_aio_n_segments; g++) {
		os_aio_get_array_and_local_segment(&array, g);

		if (array == os_aio_read_array) {

			os_event_reset(os_aio_segment_wait_events[g]);
		}
	}
#endif /* __WIN__ */
}

/*******************************************************************//**
Requests an asynchronous i/o operation.
@return	TRUE if request was queued successfully, FALSE if fail */
UNIV_INTERN
ibool
os_aio(
/*===*/
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
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset where to read or write */
	ulint		offset_high, /*!< in: most significant 32 bits of
				offset */
	ulint		n,	/*!< in: number of bytes to read or write */
	fil_node_t*	message1,/*!< in: message for the aio handler
				(can be used to identify a completed
				aio operation); ignored if mode is
				OS_AIO_SYNC */
	void*		message2,/*!< in: message for the aio handler
				(can be used to identify a completed
				aio operation); ignored if mode is
				OS_AIO_SYNC */
	ulint		space_id,
	trx_t*		trx)
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
#ifdef WIN_ASYNC_IO
	DWORD		len		= (DWORD) n;
	BOOL	ret;
#endif
	ulint		err		= 0;
	ibool		retry;
	ulint		wake_later;

	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);
	ut_ad(n % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(os_aio_validate());
#ifdef WIN_ASYNC_IO
	ut_ad((n & 0xFFFFFFFFUL) == n);
#endif

	wake_later = mode & OS_AIO_SIMULATED_WAKE_LATER;
	mode = mode & (~OS_AIO_SIMULATED_WAKE_LATER);

	if (mode == OS_AIO_SYNC) 
	{
		ibool ret;
		/* This is actually an ordinary synchronous read or write:
		no need to use an i/o-handler thread */

		if (type == OS_FILE_READ) {
			ret = _os_file_read(file, buf, offset,
							offset_high, n, trx);
		}
		else {
			ut_a(type == OS_FILE_WRITE);

			ret = os_file_write(name, file, buf, offset, offset_high, n);
		}
		ut_a(ret);
		return ret;
	}

try_again:
	if (mode == OS_AIO_NORMAL) {
		if (type == OS_FILE_READ) {
			array = os_aio_read_array;
		} else {
			array = os_aio_write_array;
		}
	} else if (mode == OS_AIO_IBUF) {
		ut_ad(type == OS_FILE_READ);
		/* Reduce probability of deadlock bugs in connection with ibuf:
		do not let the ibuf i/o handler sleep */

		wake_later = FALSE;

		array = os_aio_ibuf_array;
	} else if (mode == OS_AIO_LOG) {

		array = os_aio_log_array;
	} else if (mode == OS_AIO_SYNC) {
		array = os_aio_sync_array;
	} else {
		array = NULL; /* Eliminate compiler warning */
		ut_error;
	}

	if (trx && type == OS_FILE_READ)
	{
		trx->io_reads++;
		trx->io_read += n;
	}
	slot = os_aio_array_reserve_slot(type, array, message1, message2, file,
					 name, buf, offset, offset_high, n, space_id);
	if (type == OS_FILE_READ) {
		if (os_aio_use_native_aio) {
#ifdef WIN_ASYNC_IO
			os_n_file_reads++;
			os_bytes_read_since_printout += len;

			ret = ReadFile(file, buf, (DWORD)n, &len,
				       &(slot->control));
			if(!ret && GetLastError() != ERROR_IO_PENDING)
				err = 1;
#endif
		} else {
			if (!wake_later) {
				os_aio_simulated_wake_handler_thread(
					os_aio_get_segment_no_from_slot(
						array, slot));
			}
		}
	} else if (type == OS_FILE_WRITE) {
		if (os_aio_use_native_aio) {
#ifdef WIN_ASYNC_IO
			os_n_file_writes++;
			ret = WriteFile(file, buf, (DWORD)n, &len,
					&(slot->control));
			if(!ret && GetLastError() != ERROR_IO_PENDING)
				err = 1;
#endif
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


	if (err == 0) {
		/* aio was queued successfully! */

		return(TRUE);
	}

	os_aio_array_free_slot(array, slot);

	retry = os_file_handle_error(name,
				     type == OS_FILE_READ
				     ? "aio read" : "aio write");
	if (retry) {

		goto try_again;
	}

	return(FALSE);
}

#ifdef WIN_ASYNC_IO
#define READ_SEGMENT(x) (x < srv_n_read_io_threads)
#define WRITE_SEGMENT(x) !READ_SEGMENT(x)

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
	ulint*	type,		/*!< out: OS_FILE_WRITE or ..._READ */
	ulint*	space_id)
{
	ulint		orig_seg	= segment;
	os_aio_slot_t*	slot;
	ibool		ret_val;
	BOOL		ret;
	DWORD		len;
	BOOL		retry		= FALSE;
	ULONG_PTR key;
	HANDLE port = READ_SEGMENT(segment)? read_completion_port : completion_port;

	for(;;) {
		ret = GetQueuedCompletionStatus(port, &len, &key, 
			(OVERLAPPED **)&slot, INFINITE);

		/* If shutdown key was received, repost the shutdown message and exit */
		if (ret && (key == IOCP_SHUTDOWN_KEY)) {
			PostQueuedCompletionStatus(port, 0, key, NULL);
			os_thread_exit(NULL);
		}

		if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {
			os_thread_exit(NULL);
		}

		if(WRITE_SEGMENT(segment)&& slot->type == OS_FILE_READ) {
			/*
			Redirect read completions  to the dedicated completion port 
			and thread. We need to split read and write threads. If we do not
			do that, and just allow all io threads process all IO, it is possible 
			to get stuck in a deadlock in buffer pool code,

			Currently, the problem is solved this way - "write io" threads  
			always get all completion notifications, from both async reads and
			writes. Write completion is handled in the same thread that gets it.
			Read completion is forwarded via PostQueueCompletionStatus())
			to the second completion port dedicated solely to reads. One of the
			"read io" threads waiting on this port will finally handle the IO.

			Forwarding IO completion this way costs a context switch , and this 
			seems tolerable  since asynchronous reads are by far less frequent.
			*/
			ut_a(PostQueuedCompletionStatus(read_completion_port, len, key,
				&slot->control));
		}
		else {
			break;
		}
	}


	*message1 = slot->message1;
	*message2 = slot->message2;

	*type = slot->type;
	*space_id = slot->space_id;

	if (ret && len == slot->len) {
		ret_val = TRUE;

#ifdef UNIV_DO_FLUSH
		if (slot->type == OS_FILE_WRITE
		    && !os_do_not_call_flush_at_each_write) {
			ut_a(TRUE == os_file_flush(slot->file, TRUE));
		}
#endif /* UNIV_DO_FLUSH */
	} else if (os_file_handle_error(slot->name, "Windows aio")) {

		retry = TRUE;
	} else {

		ret_val = FALSE;
	}

	if (retry) {
		/* retry failed read/write operation synchronously.
		No need to hold array->mutex. */

		ut_a((slot->len & 0xFFFFFFFFUL) == slot->len);

		switch (slot->type) {
		case OS_FILE_WRITE:
			ret_val = os_file_write(slot->name, slot->file, slot->buf, 
				slot->control.Offset, slot->control.OffsetHigh, slot->len);
			break;
		case OS_FILE_READ:
			ret_val = os_file_read(slot->file, slot->buf, 
				 slot->control.Offset, slot->control.OffsetHigh, slot->len);
			break;
		default:
			ut_error;
		}
	}

	os_aio_array_free_slot((os_aio_array_t *)slot->arr, slot);

	return(ret_val);
}
#endif

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
	ulint*	type,		/*!< out: OS_FILE_WRITE or ..._READ */
	ulint*	space_id)
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	os_aio_slot_t*	slot2;
	os_aio_slot_t*	consecutive_ios[OS_AIO_MERGE_N_CONSECUTIVE];
	os_aio_slot_t*  lowest_request;
	os_aio_slot_t*	oldest_request;
	ulint		n_consecutive;
	ulint		total_len;
	ulint		offs;
	ulint		lowest_offset;
	ulint		oldest_offset;
	ulint		biggest_age;
	ulint		age;
	byte*		combined_buf;
	byte*		combined_buf2;
	ibool		ret;
	ulint		n;
	ulint		i;
	time_t          now;

	/* Fix compiler warning */
	*consecutive_ios = NULL;

	os_aio_get_array_and_local_segment(&array, global_segment);

restart:
	/* NOTE! We only access constant fields in os_aio_array. Therefore
	we do not have to acquire the protecting mutex yet */

	srv_set_io_thread_op_info(global_segment,
				  "looking for i/o requests (a)");
	ut_ad(os_aio_validate());

	n = array->n_slots;

	/* Look through n slots after the segment * n'th slot */

	if (array == os_aio_read_array
	    && os_aio_recommend_sleep_for_read_threads) {

		/* Give other threads chance to add several i/os to the array
		at once. */

		goto recommended_sleep;
	}

	os_mutex_enter(array->mutex);

	srv_set_io_thread_op_info(global_segment,
				  "looking for i/o requests (b)");

	/* Check if there is a slot for which the i/o has already been
	done */

	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved && slot->status == OS_AIO_DONE) {

			if (os_aio_print_debug) {
				fprintf(stderr,
					"InnoDB: i/o for slot %lu"
					" already done, returning\n",
					(ulong) i);
			}

			ret = TRUE;

			goto slot_io_done;
		}
	}

	n_consecutive = 0;

	/* If there are at least 2 seconds old requests, then pick the oldest
	one to prevent starvation. If several requests have the same age,
	then pick the one at the lowest offset. */

	biggest_age = 0;
	now = time(NULL);
	oldest_request = lowest_request = NULL;
	oldest_offset = lowest_offset = ULINT_MAX;

	/* Find the oldest request and the request with the smallest offset */
	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved && slot->status == OS_AIO_NOT_ISSUED) {
			age = (ulint)difftime(now, slot->reservation_time);

			if ((age >= 2 && age > biggest_age)
			    || (age >= 2 && age == biggest_age
				&& slot->offset < oldest_offset)) {

				/* Found an i/o request */
				biggest_age = age;
				oldest_request = slot;
				oldest_offset = slot->offset;
			}

			/* Look for an i/o request at the lowest offset in the array
			 * (we ignore the high 32 bits of the offset) */
			if (slot->offset < lowest_offset) {
				/* Found an i/o request */
				lowest_request = slot;
				lowest_offset = slot->offset;
			}
		}
	}

	if (!lowest_request && !oldest_request) {

		/* No i/o requested at the moment */

		goto wait_for_io;
	}

	if (oldest_request) {
		slot = oldest_request;
	} else {
		slot = lowest_request;
	}
	consecutive_ios[0] = slot;
	n_consecutive = 1;

	/* Check if there are several consecutive blocks to read or write */

consecutive_loop:
	for (i = 0; i < n; i++) {
		slot2 = os_aio_array_get_nth_slot(array, i);

		if (slot2->reserved && slot2 != slot
		    && slot2->offset == slot->offset + slot->len
		    /* check that sum does not wrap over */
		    && slot->offset + slot->len > slot->offset
		    && slot2->offset_high == slot->offset_high
		    && slot2->type == slot->type
		    && slot2->file == slot->file
		    && slot2->status == OS_AIO_NOT_ISSUED) {

			/* Found a consecutive i/o request */

			consecutive_ios[n_consecutive] = slot2;
			n_consecutive++;

			slot = slot2;

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
	slot = consecutive_ios[0];

	for (i = 0; i < n_consecutive; i++) {
		total_len += consecutive_ios[i]->len;
		ut_a(consecutive_ios[i]->status == OS_AIO_NOT_ISSUED);
		consecutive_ios[i]->status = OS_AIO_ISSUED;
	}

	if (n_consecutive == 1) {
		/* We can use the buffer of the i/o request */
		combined_buf = slot->buf;
		combined_buf2 = NULL;
	} else {
		if ((total_len + UNIV_PAGE_SIZE) > os_aio_thread_buffer_size[global_segment]) {
			if (os_aio_thread_buffer[global_segment])
				ut_free(os_aio_thread_buffer[global_segment]);

			os_aio_thread_buffer[global_segment] = ut_malloc(total_len + UNIV_PAGE_SIZE);
			os_aio_thread_buffer_size[global_segment] = total_len + UNIV_PAGE_SIZE;
		}
		combined_buf2 = os_aio_thread_buffer[global_segment];

		ut_a(combined_buf2);

		combined_buf = ut_align(combined_buf2, UNIV_PAGE_SIZE);
	}

	/* We release the array mutex for the time of the i/o: NOTE that
	this assumes that there is just one i/o-handler thread serving
	a single segment of slots! */

	ut_a(slot->reserved);
	ut_a(slot->status == OS_AIO_ISSUED);

	os_mutex_exit(array->mutex);

	if (slot->type == OS_FILE_WRITE && n_consecutive > 1) {
		/* Copy the buffers to the combined buffer */
		offs = 0;

		for (i = 0; i < n_consecutive; i++) {

			ut_memcpy(combined_buf + offs, consecutive_ios[i]->buf,
				  consecutive_ios[i]->len);
			offs += consecutive_ios[i]->len;
		}
	}

	srv_set_io_thread_op_info(global_segment, "doing file i/o");

	if (os_aio_print_debug) {
		fprintf(stderr,
			"InnoDB: doing i/o of type %lu at offset %lu %lu,"
			" length %lu\n",
			(ulong) slot->type, (ulong) slot->offset_high,
			(ulong) slot->offset, (ulong) total_len);
	}

	/* Do the i/o with ordinary, synchronous i/o functions: */
	if (slot->type == OS_FILE_WRITE) {
		ret = os_file_write(slot->name, slot->file, combined_buf,
				    slot->offset, slot->offset_high,
				    total_len);
	} else {
		ret = os_file_read(slot->file, combined_buf,
				   slot->offset, slot->offset_high, total_len);
	}

	ut_a(ret);
	srv_set_io_thread_op_info(global_segment, "file i/o done");

#if 0
	fprintf(stderr,
		"aio: %lu consecutive %lu:th segment, first offs %lu blocks\n",
		n_consecutive, global_segment, slot->offset / UNIV_PAGE_SIZE);
#endif

	if (slot->type == OS_FILE_READ && n_consecutive > 1) {
		/* Copy the combined buffer to individual buffers */
		offs = 0;

		for (i = 0; i < n_consecutive; i++) {

			ut_memcpy(consecutive_ios[i]->buf, combined_buf + offs,
				  consecutive_ios[i]->len);
			offs += consecutive_ios[i]->len;
		}
	}

	if (srv_recovery_stats && recv_recovery_is_on() && n_consecutive) {
		mutex_enter(&(recv_sys->mutex));
		if (slot->type == OS_FILE_READ) {
			recv_sys->stats_read_io_pages += n_consecutive;
			recv_sys->stats_read_io_consecutive[n_consecutive - 1]++;
		} else if (slot->type == OS_FILE_WRITE) {
			recv_sys->stats_write_io_pages += n_consecutive;
			recv_sys->stats_write_io_consecutive[n_consecutive - 1]++;
		}
		mutex_exit(&(recv_sys->mutex));
	}

	os_mutex_enter(array->mutex);

	/* Mark the i/os done in slots */

	for (i = 0; i < n_consecutive; i++) {
		ut_a(consecutive_ios[i]->status == OS_AIO_ISSUED);
		consecutive_ios[i]->status = OS_AIO_DONE;
	}

	/* We return the messages for the first slot now, and if there were
	several slots, the messages will be returned with subsequent calls
	of this function */

slot_io_done:

	ut_a(slot->reserved);
	ut_a(slot->status == OS_AIO_DONE);
	slot->status = OS_AIO_CLAIMED;

	*message1 = slot->message1;
	*message2 = slot->message2;

	*type = slot->type;
	*space_id = slot->space_id;

	os_mutex_exit(array->mutex);

	os_aio_array_free_slot(array, slot);

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

	if (os_aio_print_debug) {
		fprintf(stderr,
			"InnoDB: i/o handler thread for i/o"
			" segment %lu wakes up\n",
			(ulong) global_segment);
	}

	goto restart;
}

/**********************************************************************//**
Validates the consistency of an aio array.
@return	TRUE if ok */
static
ibool
os_aio_array_validate(
/*==================*/
	os_aio_array_t*	array)	/*!< in: aio wait array */
{
	os_aio_slot_t*	slot;
	ulint		n_reserved	= 0;
	ulint		i;

	ut_a(array);

	os_mutex_enter(array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);

	for (i = 0; i < array->n_slots; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved) {
			n_reserved++;
			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	os_mutex_exit(array->mutex);

	return(TRUE);
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
	os_aio_array_validate(os_aio_write_array);
	os_aio_array_validate(os_aio_ibuf_array);
	os_aio_array_validate(os_aio_log_array);
	os_aio_array_validate(os_aio_sync_array);

	return(TRUE);
}

/**********************************************************************//**
Prints info of the aio arrays. */
UNIV_INTERN
void
os_aio_print(
/*=========*/
	FILE*	file)	/*!< in: file where to print */
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	ulint		n_reserved;
	time_t		current_time;
	double		time_elapsed;
	double		avg_bytes_read;
	ulint		i;

	for (i = 0; i < srv_n_file_io_threads; i++) {
		fprintf(file, "I/O thread %lu state: %s (%s)", (ulong) i,
			srv_io_thread_op_info[i],
			srv_io_thread_function[i]);

#ifndef __WIN__
		if (os_aio_segment_wait_events[i]->is_set) {
			fprintf(file, " ev set");
		}
#endif

		fprintf(file, "\n");
	}

	fputs("Pending normal aio reads:", file);

	array = os_aio_read_array;
loop:
	ut_a(array);

	os_mutex_enter(array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);

	n_reserved = 0;

	for (i = 0; i < array->n_slots; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved) {
			n_reserved++;
#if 0
			fprintf(stderr, "Reserved slot, messages %p %p\n",
				(void*) slot->message1,
				(void*) slot->message2);
#endif
			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	fprintf(file, " %lu", (ulong) n_reserved);

	os_mutex_exit(array->mutex);

	if (array == os_aio_read_array) {
		fputs(", aio writes:", file);

		array = os_aio_write_array;

		goto loop;
	}

	if (array == os_aio_write_array) {
		fputs(",\n ibuf aio reads:", file);
		array = os_aio_ibuf_array;

		goto loop;
	}

	if (array == os_aio_ibuf_array) {
		fputs(", log i/o's:", file);
		array = os_aio_log_array;

		goto loop;
	}

	if (array == os_aio_log_array) {
		fputs(", sync i/o's:", file);
		array = os_aio_sync_array;

		goto loop;
	}

	putc('\n', file);
	current_time = time(NULL);
	time_elapsed = 0.001 + difftime(current_time, os_last_printout);

	fprintf(file,
		"Pending flushes (fsync) log: %lu; buffer pool: %lu\n"
		"%lu OS file reads, %lu OS file writes, %lu OS fsyncs\n",
		(ulong) fil_n_pending_log_flushes,
		(ulong) fil_n_pending_tablespace_flushes,
		(ulong) os_n_file_reads, (ulong) os_n_file_writes,
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
		(ulong)avg_bytes_read,
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

	array = os_aio_write_array;

	os_mutex_enter(array->mutex);

	n_res += array->n_reserved;

	os_mutex_exit(array->mutex);

	array = os_aio_ibuf_array;

	os_mutex_enter(array->mutex);

	n_res += array->n_reserved;

	os_mutex_exit(array->mutex);

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
