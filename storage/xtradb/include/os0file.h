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
@file include/os0file.h
The interface to the operating system file io

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#ifndef os0file_h
#define os0file_h

#include "univ.i"
#include "trx0types.h"

#ifndef __WIN__
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#endif

/** File node of a tablespace or the log data space */
typedef	struct fil_node_struct	fil_node_t;

#ifdef UNIV_DO_FLUSH
extern ibool	os_do_not_call_flush_at_each_write;
#endif /* UNIV_DO_FLUSH */
extern ibool	os_has_said_disk_full;
/** Flag: enable debug printout for asynchronous i/o */
extern ibool	os_aio_print_debug;

/** Number of pending os_file_pread() operations */
extern ulint	os_file_n_pending_preads;
/** Number of pending os_file_pwrite() operations */
extern ulint	os_file_n_pending_pwrites;

/** Number of pending read operations */
extern ulint	os_n_pending_reads;
/** Number of pending write operations */
extern ulint	os_n_pending_writes;

#ifdef __WIN__

/** We define always WIN_ASYNC_IO, and check at run-time whether
   the OS actually supports it: Win 95 does not, NT does. */
#define WIN_ASYNC_IO

/** Use unbuffered I/O */
#define UNIV_NON_BUFFERED_IO

#endif

#ifdef __WIN__
/** File handle */
#define os_file_t	HANDLE
/** Convert a C file descriptor to a native file handle
@param fd	file descriptor
@return		native file handle */
#define OS_FILE_FROM_FD(fd) (HANDLE) _get_osfhandle(fd)
#else
/** File handle */
typedef int	os_file_t;
/** Convert a C file descriptor to a native file handle
@param fd	file descriptor
@return		native file handle */
#define OS_FILE_FROM_FD(fd) fd
#endif

/** Umask for creating files */
extern ulint	os_innodb_umask;

/** If this flag is TRUE, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads */

extern ibool	os_aio_use_native_aio;

/** The next value should be smaller or equal to the smallest sector size used
on any disk. A log block is required to be a portion of disk which is written
so that if the start and the end of a block get written to disk, then the
whole block gets written. This should be true even in most cases of a crash:
if this fails for a log block, then it is equivalent to a media failure in the
log. */

#define OS_FILE_LOG_BLOCK_SIZE		srv_log_block_size

/** Options for file_create @{ */
#define	OS_FILE_OPEN			51
#define	OS_FILE_CREATE			52
#define OS_FILE_OVERWRITE		53
#define OS_FILE_OPEN_RAW		54
#define	OS_FILE_CREATE_PATH		55
#define	OS_FILE_OPEN_RETRY		56	/* for os_file_create() on
						the first ibdata file */

#define OS_FILE_READ_ONLY		333
#define	OS_FILE_READ_WRITE		444
#define	OS_FILE_READ_ALLOW_DELETE	555	/* for ibbackup */

/* Options for file_create */
#define	OS_FILE_AIO			61
#define	OS_FILE_NORMAL			62
/* @} */

/** Types for file create @{ */
#define	OS_DATA_FILE			100
#define OS_LOG_FILE			101
/* @} */

/** Error codes from os_file_get_last_error @{ */
#define	OS_FILE_NOT_FOUND		71
#define	OS_FILE_DISK_FULL		72
#define	OS_FILE_ALREADY_EXISTS		73
#define	OS_FILE_PATH_ERROR		74
#define	OS_FILE_AIO_RESOURCES_RESERVED	75	/* wait for OS aio resources
						to become available again */
#define	OS_FILE_SHARING_VIOLATION	76
#define	OS_FILE_ERROR_NOT_SPECIFIED	77
#define	OS_FILE_INSUFFICIENT_RESOURCE	78
#define	OS_FILE_OPERATION_ABORTED	79
/* @} */

/** Types for aio operations @{ */
#define OS_FILE_READ	10
#define OS_FILE_WRITE	11

#define OS_FILE_LOG	256	/* This can be ORed to type */
/* @} */

#define OS_AIO_N_PENDING_IOS_PER_THREAD 256	/*!< Windows might be able to handle
more */

/** Modes for aio operations @{ */
#define OS_AIO_NORMAL	21	/*!< Normal asynchronous i/o not for ibuf
				pages or ibuf bitmap pages */
#define OS_AIO_IBUF	22	/*!< Asynchronous i/o for ibuf pages or ibuf
				bitmap pages */
#define OS_AIO_LOG	23	/*!< Asynchronous i/o for the log */
#define OS_AIO_SYNC	24	/*!< Asynchronous i/o where the calling thread
				will itself wait for the i/o to complete,
				doing also the job of the i/o-handler thread;
				can be used for any pages, ibuf or non-ibuf.
				This is used to save CPU time, as we can do
				with fewer thread switches. Plain synchronous
				i/o is not as good, because it must serialize
				the file seek and read or write, causing a
				bottleneck for parallelism. */

#define OS_AIO_SIMULATED_WAKE_LATER	512 /*!< This can be ORed to mode
				in the call of os_aio(...),
				if the caller wants to post several i/o
				requests in a batch, and only after that
				wake the i/o-handler thread; this has
				effect only in simulated aio */
/* @} */

#define OS_WIN31	1	/*!< Microsoft Windows 3.x */
#define OS_WIN95	2	/*!< Microsoft Windows 95 */
#define OS_WINNT	3	/*!< Microsoft Windows NT 3.x */
#define OS_WIN2000	4	/*!< Microsoft Windows 2000 */
#define OS_WINXP	5	/*!< Microsoft Windows XP */
#define OS_WINVISTA	6	/*!< Microsoft Windows Vista */
#define OS_WIN7		7	/*!< Microsoft Windows 7 */


extern ulint	os_n_file_reads;
extern ulint	os_n_file_writes;
extern ulint	os_n_fsyncs;

extern ulint	srv_log_block_size;

/* File types for directory entry data type */

enum os_file_type_enum{
	OS_FILE_TYPE_UNKNOWN = 0,
	OS_FILE_TYPE_FILE,			/* regular file */
	OS_FILE_TYPE_DIR,			/* directory */
	OS_FILE_TYPE_LINK			/* symbolic link */
};
typedef enum os_file_type_enum	  os_file_type_t;

/* Maximum path string length in bytes when referring to tables with in the
'./databasename/tablename.ibd' path format; we can allocate at least 2 buffers
of this size from the thread stack; that is why this should not be made much
bigger than 4000 bytes */
#define OS_FILE_MAX_PATH	4000

/* Struct used in fetching information of a file in a directory */
struct os_file_stat_struct{
	char		name[OS_FILE_MAX_PATH];	/*!< path to a file */
	os_file_type_t	type;			/*!< file type */
	ib_int64_t	size;			/*!< file size */
	time_t		ctime;			/*!< creation time */
	time_t		mtime;			/*!< modification time */
	time_t		atime;			/*!< access time */
};
typedef struct os_file_stat_struct	os_file_stat_t;

#ifdef __WIN__
typedef HANDLE	os_file_dir_t;	/*!< directory stream */
#else
typedef DIR*	os_file_dir_t;	/*!< directory stream */
#endif

/***********************************************************************//**
Gets the operating system version. Currently works only on Windows.
@return	OS_WIN95, OS_WIN31, OS_WINNT, or OS_WIN2000 */
UNIV_INTERN
ulint
os_get_os_version(void);
/*===================*/
#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Creates the seek mutexes used in positioned reads and writes. */
UNIV_INTERN
void
os_io_init_simple(void);
/*===================*/
/***********************************************************************//**
Creates a temporary file.  This function is like tmpfile(3), but
the temporary file is created in the MySQL temporary directory.
On Netware, this function is like tmpfile(3), because the C run-time
library of Netware does not expose the delete-on-close flag.
@return	temporary file handle, or NULL on error */

FILE*
os_file_create_tmpfile(void);
/*========================*/
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
	ibool		error_is_fatal);/*!< in: TRUE if we should treat an
					error as a fatal error; if we try to
					open symlinks then we do not wish a
					fatal error if it happens not to be
					a directory */
/***********************************************************************//**
Closes a directory stream.
@return	0 if success, -1 if failure */
UNIV_INTERN
int
os_file_closedir(
/*=============*/
	os_file_dir_t	dir);	/*!< in: directory stream */
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
	os_file_stat_t*	info);	/*!< in/out: buffer where the info is returned */
/*****************************************************************//**
This function attempts to create a directory named pathname. The new directory
gets default permissions. On Unix, the permissions are (0770 & ~umask). If the
directory exists already, nothing is done and the call succeeds, unless the
fail_if_exists arguments is true.
@return	TRUE if call succeeds, FALSE on error */
UNIV_INTERN
ibool
os_file_create_directory(
/*=====================*/
	const char*	pathname,	/*!< in: directory name as
					null-terminated string */
	ibool		fail_if_exists);/*!< in: if TRUE, pre-existing directory
					is treated as an error. */
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
	ibool*		success);/*!< out: TRUE if succeed, FALSE if error */
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
	ibool*		success);/*!< out: TRUE if succeed, FALSE if error */
/****************************************************************//**
Tries to disable OS caching on an opened file descriptor. */
UNIV_INTERN
void
os_file_set_nocache(
/*================*/
	int		fd,		/*!< in: file descriptor to alter */
	const char*	file_name,	/*!< in: file name, used in the
					diagnostic message */
	const char*	operation_name);/*!< in: "open" or "create"; used in the
					diagnostic message */
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
	ibool*		success);/*!< out: TRUE if succeed, FALSE if error */
/***********************************************************************//**
Deletes a file. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_delete(
/*===========*/
	const char*	name);	/*!< in: file path as a null-terminated string */

/***********************************************************************//**
Deletes a file if it exists. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_delete_if_exists(
/*=====================*/
	const char*	name);	/*!< in: file path as a null-terminated string */
/***********************************************************************//**
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_rename(
/*===========*/
	const char*	oldpath,	/*!< in: old file path as a
					null-terminated string */
	const char*	newpath);	/*!< in: new file path */
/***********************************************************************//**
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_close(
/*==========*/
	os_file_t	file);	/*!< in, own: handle to a file */
#ifdef UNIV_HOTBACKUP
/***********************************************************************//**
Closes a file handle.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_close_no_error_handling(
/*============================*/
	os_file_t	file);	/*!< in, own: handle to a file */
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
	ulint*		size_high);/*!< out: most significant 32 bits of size */
/***********************************************************************//**
Gets file size as a 64-bit integer ib_int64_t.
@return	size in bytes, -1 if error */
UNIV_INTERN
ib_int64_t
os_file_get_size_as_iblonglong(
/*===========================*/
	os_file_t	file);	/*!< in: handle to a file */
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
	ulint		size_high);/*!< in: most significant 32 bits of size */
/***********************************************************************//**
Truncates a file at its current position.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_set_eof(
/*============*/
	FILE*		file);	/*!< in: file to be truncated */
/***********************************************************************//**
Truncates a file at the specified position.
@return TRUE if success */
UNIV_INTERN
ibool
os_file_set_eof_at(
	os_file_t	file, /*!< in: handle to a file */
	ib_uint64_t	new_len);/*!< in: new file length */
/***********************************************************************//**
Flushes the write buffers of a given file to the disk.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_flush(
/*==========*/
	os_file_t	file,	/*!< in, own: handle to a file */
	ibool		metadata);
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
	ibool	report_all_errors);	/*!< in: TRUE if we want an error message
					printed of all errors */
/*******************************************************************//**
Requests a synchronous read operation.
@return	TRUE if request was successful, FALSE if fail */
#define os_file_read(file, buf, offset, offset_high, n)         \
		_os_file_read(file, buf, offset, offset_high, n, NULL)

UNIV_INTERN
ibool
_os_file_read(
/*=========*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	ulint		offset,	/*!< in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high,/*!< in: most significant 32 bits of
				offset */
	ulint		n,	/*!< in: number of bytes to read */
	trx_t*		trx);
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
	ulint	size);	/*!< in: size of buffer */
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
	ulint		offset_high,/*!< in: most significant 32 bits of
				offset */
	ulint		n);	/*!< in: number of bytes to read */

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
	ulint		offset_high,/*!< in: most significant 32 bits of
				offset */
	ulint		n);	/*!< in: number of bytes to write */
/*******************************************************************//**
Check the existence and type of the given file.
@return	TRUE if call succeeded */
UNIV_INTERN
ibool
os_file_status(
/*===========*/
	const char*	path,	/*!< in:	pathname of the file */
	ibool*		exists,	/*!< out: TRUE if file exists */
	os_file_type_t* type);	/*!< out: type of the file (if it exists) */
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
	const char*	path);	/*!< in: pathname */
/****************************************************************//**
Creates all missing subdirectories along the given path.
@return	TRUE if call succeeded FALSE otherwise */
UNIV_INTERN
ibool
os_file_create_subdirs_if_needed(
/*=============================*/
	const char*	path);	/*!< in: path name */
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
	ulint	n_slots_sync);	/*<! in: number of slots in the sync aio
				array */
/***********************************************************************
Frees the asynchronous io system. */
UNIV_INTERN
void
os_aio_free(void);
/*=============*/

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
	trx_t*		trx);
/************************************************************************//**
Wakes up all async i/o threads so that they know to exit themselves in
shutdown. */
UNIV_INTERN
void
os_aio_wake_all_threads_at_shutdown(void);
/*=====================================*/
/************************************************************************//**
Waits until there are no pending writes in os_aio_write_array. There can
be other, synchronous, pending writes. */
UNIV_INTERN
void
os_aio_wait_until_no_pending_writes(void);
/*=====================================*/
/**********************************************************************//**
Wakes up simulated aio i/o-handler threads if they have something to do. */
UNIV_INTERN
void
os_aio_simulated_wake_handler_threads(void);
/*=======================================*/
/**********************************************************************//**
This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
UNIV_INTERN
void
os_aio_simulated_put_read_threads_to_sleep(void);
/*============================================*/

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
	ulint*	type,		/*!< out: OS_FILE_WRITE or ..._READ */
	ulint*	space_id);
#endif

/**********************************************************************//**
Does simulated aio. This function should be called by an i/o-handler
thread.
@return	TRUE if the aio operation succeeded */
UNIV_INTERN
ibool
os_aio_simulated_handle(
/*====================*/
	ulint	segment,	/*!< in: the number of the segment in the aio
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
	ulint*	space_id);
/**********************************************************************//**
Validates the consistency of the aio system.
@return	TRUE if ok */
UNIV_INTERN
ibool
os_aio_validate(void);
/*=================*/
/**********************************************************************//**
Prints info of the aio arrays. */
UNIV_INTERN
void
os_aio_print(
/*=========*/
	FILE*	file);	/*!< in: file where to print */
/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
UNIV_INTERN
void
os_aio_refresh_stats(void);
/*======================*/

#ifdef UNIV_DEBUG
/**********************************************************************//**
Checks that all slots in the system have been freed, that is, there are
no pending io operations. */
UNIV_INTERN
ibool
os_aio_all_slots_free(void);
/*=======================*/
#endif /* UNIV_DEBUG */

/*******************************************************************//**
This function returns information about the specified file
@return	TRUE if stat information found */
UNIV_INTERN
ibool
os_file_get_status(
/*===============*/
	const char*	path,		/*!< in:	pathname of the file */
	os_file_stat_t* stat_info);	/*!< information of a file in a
					directory */

#if !defined(UNIV_HOTBACKUP) && !defined(__NETWARE__)
/*********************************************************************//**
Creates a temporary file that will be deleted on close.
This function is defined in ha_innodb.cc.
@return	temporary file descriptor, or < 0 on error */
UNIV_INTERN
int
innobase_mysql_tmpfile(void);
/*========================*/
#endif /* !UNIV_HOTBACKUP && !__NETWARE__ */

#endif
