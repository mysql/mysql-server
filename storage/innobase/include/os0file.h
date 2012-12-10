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
@file include/os0file.h
The interface to the operating system file io

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#ifndef os0file_h
#define os0file_h

#include "univ.i"

#ifndef __WIN__
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#endif

/** File node of a tablespace or the log data space */
struct fil_node_t;

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

/** File offset in bytes */
typedef ib_uint64_t os_offset_t;
#ifdef __WIN__
/** File handle */
# define os_file_t	HANDLE
/** Convert a C file descriptor to a native file handle
@param fd	file descriptor
@return		native file handle */
# define OS_FILE_FROM_FD(fd) (HANDLE) _get_osfhandle(fd)
#else
/** File handle */
typedef int	os_file_t;
/** Convert a C file descriptor to a native file handle
@param fd	file descriptor
@return		native file handle */
# define OS_FILE_FROM_FD(fd) fd
#endif

/** Umask for creating files */
extern ulint	os_innodb_umask;

/** The next value should be smaller or equal to the smallest sector size used
on any disk. A log block is required to be a portion of disk which is written
so that if the start and the end of a block get written to disk, then the
whole block gets written. This should be true even in most cases of a crash:
if this fails for a log block, then it is equivalent to a media failure in the
log. */

#define OS_FILE_LOG_BLOCK_SIZE		512

/** Options for os_file_create_func @{ */
enum os_file_create_t {
	OS_FILE_OPEN = 51,		/*!< to open an existing file (if
					doesn't exist, error) */
	OS_FILE_CREATE,			/*!< to create new file (if
					exists, error) */
	OS_FILE_OVERWRITE,		/*!< to create a new file, if exists
					the overwrite old file */
	OS_FILE_OPEN_RAW,		/*!< to open a raw device or disk
					partition */
	OS_FILE_CREATE_PATH,		/*!< to create the directories */
	OS_FILE_OPEN_RETRY,		/*!< open with retry */

	/** Flags that can be combined with the above values. Please ensure
	that the above values stay below 128. */

	OS_FILE_ON_ERROR_NO_EXIT = 128,	/*!< do not exit on unknown errors */
	OS_FILE_ON_ERROR_SILENT = 256	/*!< don't print diagnostic messages to
					the log unless it is a fatal error,
					this flag is only used if
					ON_ERROR_NO_EXIT is set */
};

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
#define	OS_FILE_AIO_INTERRUPTED		79
#define	OS_FILE_OPERATION_ABORTED	80
/* @} */

/** Types for aio operations @{ */
#define OS_FILE_READ	10
#define OS_FILE_WRITE	11

#define OS_FILE_LOG	256	/* This can be ORed to type */
/* @} */

#define OS_AIO_N_PENDING_IOS_PER_THREAD 32	/*!< Win NT does not allow more
						than 64 */

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
#define OS_WINXP	5	/*!< Microsoft Windows XP
				or Windows Server 2003 */
#define OS_WINVISTA	6	/*!< Microsoft Windows Vista
				or Windows Server 2008 */
#define OS_WIN7		7	/*!< Microsoft Windows 7
				or Windows Server 2008 R2 */


extern ulint	os_n_file_reads;
extern ulint	os_n_file_writes;
extern ulint	os_n_fsyncs;

#ifdef UNIV_PFS_IO
/* Keys to register InnoDB I/O with performance schema */
extern mysql_pfs_key_t	innodb_file_data_key;
extern mysql_pfs_key_t	innodb_file_log_key;
extern mysql_pfs_key_t	innodb_file_temp_key;

/* Following four macros are instumentations to register
various file I/O operations with performance schema.
1) register_pfs_file_open_begin() and register_pfs_file_open_end() are
used to register file creation, opening, closing and renaming.
2) register_pfs_file_io_begin() and register_pfs_file_io_end() are
used to register actual file read, write and flush */
# define register_pfs_file_open_begin(state, locker, key, op, name,	\
				      src_file, src_line)		\
do {									\
	locker = PSI_FILE_CALL(get_thread_file_name_locker)(		\
		state, key, op, name, &locker);				\
	if (UNIV_LIKELY(locker != NULL)) {				\
		PSI_FILE_CALL(start_file_open_wait)(			\
			locker, src_file, src_line);			\
	}								\
} while (0)

# define register_pfs_file_open_end(locker, file)			\
do {									\
	if (UNIV_LIKELY(locker != NULL)) {				\
		PSI_FILE_CALL(end_file_open_wait_and_bind_to_descriptor)(\
			locker, file);					\
	}								\
} while (0)

# define register_pfs_file_io_begin(state, locker, file, count, op,	\
				    src_file, src_line)			\
do {									\
	locker = PSI_FILE_CALL(get_thread_file_descriptor_locker)(	\
		state, file, op);					\
	if (UNIV_LIKELY(locker != NULL)) {				\
		PSI_FILE_CALL(start_file_wait)(				\
			locker, count, src_file, src_line);		\
	}								\
} while (0)

# define register_pfs_file_io_end(locker, count)			\
do {									\
	if (UNIV_LIKELY(locker != NULL)) {				\
		PSI_FILE_CALL(end_file_wait)(locker, count);		\
	}								\
} while (0)
#endif /* UNIV_PFS_IO  */

/* Following macros/functions are file I/O APIs that would be performance
schema instrumented if "UNIV_PFS_IO" is defined. They would point to
wrapper functions with performance schema instrumentation in such case.

os_file_create
os_file_create_simple
os_file_create_simple_no_error_handling
os_file_close
os_file_rename
os_aio
os_file_read
os_file_read_no_error_handling
os_file_write

The wrapper functions have the prefix of "innodb_". */

#ifdef UNIV_PFS_IO
# define os_file_create(key, name, create, purpose, type, success)	\
	pfs_os_file_create_func(key, name, create, purpose,	type,	\
				success, __FILE__, __LINE__)

# define os_file_create_simple(key, name, create, access, success)	\
	pfs_os_file_create_simple_func(key, name, create, access,	\
				       success, __FILE__, __LINE__)

# define os_file_create_simple_no_error_handling(			\
		key, name, create_mode, access, success)		\
	pfs_os_file_create_simple_no_error_handling_func(		\
		key, name, create_mode, access, success, __FILE__, __LINE__)

# define os_file_close(file)						\
	pfs_os_file_close_func(file, __FILE__, __LINE__)

# define os_aio(type, mode, name, file, buf, offset,			\
		n, message1, message2)					\
	pfs_os_aio_func(type, mode, name, file, buf, offset,		\
			n, message1, message2, __FILE__, __LINE__)

# define os_file_read(file, buf, offset, n)				\
	pfs_os_file_read_func(file, buf, offset, n, __FILE__, __LINE__)

# define os_file_read_no_error_handling(file, buf, offset, n)		\
	pfs_os_file_read_no_error_handling_func(file, buf, offset, n,	\
						__FILE__, __LINE__)

# define os_file_write(name, file, buf, offset, n)	\
	pfs_os_file_write_func(name, file, buf, offset,	\
			       n, __FILE__, __LINE__)

# define os_file_flush(file)						\
	pfs_os_file_flush_func(file, __FILE__, __LINE__)

# define os_file_rename(key, oldpath, newpath)				\
	pfs_os_file_rename_func(key, oldpath, newpath, __FILE__, __LINE__)
#else /* UNIV_PFS_IO */

/* If UNIV_PFS_IO is not defined, these I/O APIs point
to original un-instrumented file I/O APIs */
# define os_file_create(key, name, create, purpose, type, success)	\
	os_file_create_func(name, create, purpose, type, success)

# define os_file_create_simple(key, name, create_mode, access, success)	\
	os_file_create_simple_func(name, create_mode, access, success)

# define os_file_create_simple_no_error_handling(			\
		key, name, create_mode, access, success)		\
	os_file_create_simple_no_error_handling_func(			\
		name, create_mode, access, success)

# define os_file_close(file)	os_file_close_func(file)

# define os_aio(type, mode, name, file, buf, offset, n, message1, message2) \
	os_aio_func(type, mode, name, file, buf, offset, n,		\
		    message1, message2)

# define os_file_read(file, buf, offset, n)	\
	os_file_read_func(file, buf, offset, n)

# define os_file_read_no_error_handling(file, buf, offset, n)		\
	os_file_read_no_error_handling_func(file, buf, offset, n)

# define os_file_write(name, file, buf, offset, n)			\
	os_file_write_func(name, file, buf, offset, n)

# define os_file_flush(file)	os_file_flush_func(file)

# define os_file_rename(key, oldpath, newpath)				\
	os_file_rename_func(oldpath, newpath)

#endif /* UNIV_PFS_IO */

/* File types for directory entry data type */

enum os_file_type_t {
	OS_FILE_TYPE_UNKNOWN = 0,
	OS_FILE_TYPE_FILE,			/* regular file */
	OS_FILE_TYPE_DIR,			/* directory */
	OS_FILE_TYPE_LINK			/* symbolic link */
};

/* Maximum path string length in bytes when referring to tables with in the
'./databasename/tablename.ibd' path format; we can allocate at least 2 buffers
of this size from the thread stack; that is why this should not be made much
bigger than 4000 bytes */
#define OS_FILE_MAX_PATH	4000

/** Struct used in fetching information of a file in a directory */
struct os_file_stat_t {
	char		name[OS_FILE_MAX_PATH];	/*!< path to a file */
	os_file_type_t	type;			/*!< file type */
	ib_int64_t	size;			/*!< file size */
	time_t		ctime;			/*!< creation time */
	time_t		mtime;			/*!< modification time */
	time_t		atime;			/*!< access time */
	bool		rw_perm;		/*!< true if can be opened
						in read-write mode. Only valid
						if type == OS_FILE_TYPE_FILE */
};

#ifdef __WIN__
typedef HANDLE	os_file_dir_t;	/*!< directory stream */
#else
typedef DIR*	os_file_dir_t;	/*!< directory stream */
#endif

#ifdef __WIN__
/***********************************************************************//**
Gets the operating system version. Currently works only on Windows.
@return	OS_WIN95, OS_WIN31, OS_WINNT, OS_WIN2000, OS_WINXP, OS_WINVISTA,
OS_WIN7. */
UNIV_INTERN
ulint
os_get_os_version(void);
/*===================*/
#endif /* __WIN__ */
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
	ibool*		success);/*!< out: TRUE if succeed, FALSE if error */
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
	__attribute__((nonnull, warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/***********************************************************************//**
Deletes a file. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
bool
os_file_delete(
/*===========*/
	const char*	name);	/*!< in: file path as a null-terminated
				string */

/***********************************************************************//**
Deletes a file if it exists. The file has to be closed before calling this.
@return	TRUE if success */
UNIV_INTERN
bool
os_file_delete_if_exists(
/*=====================*/
	const char*	name);	/*!< in: file path as a null-terminated
				string */
/***********************************************************************//**
NOTE! Use the corresponding macro os_file_rename(), not directly
this function!
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_rename_func(
/*================*/
	const char*	oldpath,	/*!< in: old file path as a
					null-terminated string */
	const char*	newpath);	/*!< in: new file path */
/***********************************************************************//**
NOTE! Use the corresponding macro os_file_close(), not directly this
function!
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_close_func(
/*===============*/
	os_file_t	file);	/*!< in, own: handle to a file */

#ifdef UNIV_PFS_IO
/****************************************************************//**
NOTE! Please use the corresponding macro os_file_create_simple(),
not directly this function!
A performance schema instrumented wrapper function for
os_file_create_simple() which opens or creates a file.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INLINE
os_file_t
pfs_os_file_create_simple_func(
/*===========================*/
	mysql_pfs_key_t key,	/*!< in: Performance Schema Key */
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: create mode */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY or
				OS_FILE_READ_WRITE */
	ibool*		success,/*!< out: TRUE if succeed, FALSE if error */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line)/*!< in: line where the func invoked */
	__attribute__((nonnull, warn_unused_result));

/****************************************************************//**
NOTE! Please use the corresponding macro
os_file_create_simple_no_error_handling(), not directly this function!
A performance schema instrumented wrapper function for
os_file_create_simple_no_error_handling(). Add instrumentation to
monitor file creation/open.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INLINE
os_file_t
pfs_os_file_create_simple_no_error_handling_func(
/*=============================================*/
	mysql_pfs_key_t key,	/*!< in: Performance Schema Key */
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode, /*!< in: file create mode */
	ulint		access_type,/*!< in: OS_FILE_READ_ONLY,
				OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file */
	ibool*		success,/*!< out: TRUE if succeed, FALSE if error */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line)/*!< in: line where the func invoked */
	__attribute__((nonnull, warn_unused_result));

/****************************************************************//**
NOTE! Please use the corresponding macro os_file_create(), not directly
this function!
A performance schema wrapper function for os_file_create().
Add instrumentation to monitor file creation/open.
@return own: handle to the file, not defined if error, error number
can be retrieved with os_file_get_last_error */
UNIV_INLINE
os_file_t
pfs_os_file_create_func(
/*====================*/
	mysql_pfs_key_t key,	/*!< in: Performance Schema Key */
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/*!< in: file create mode */
	ulint		purpose,/*!< in: OS_FILE_AIO, if asynchronous,
				non-buffered i/o is desired,
				OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use
				async i/o or unbuffered i/o: look in the
				function source code for the exact rules */
	ulint		type,	/*!< in: OS_DATA_FILE or OS_LOG_FILE */
	ibool*		success,/*!< out: TRUE if succeed, FALSE if error */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line)/*!< in: line where the func invoked */
	__attribute__((nonnull, warn_unused_result));

/***********************************************************************//**
NOTE! Please use the corresponding macro os_file_close(), not directly
this function!
A performance schema instrumented wrapper function for os_file_close().
@return TRUE if success */
UNIV_INLINE
ibool
pfs_os_file_close_func(
/*===================*/
        os_file_t	file,	/*!< in, own: handle to a file */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */
/*******************************************************************//**
NOTE! Please use the corresponding macro os_file_read(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_read() which requests a synchronous read operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INLINE
ibool
pfs_os_file_read_func(
/*==================*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n,	/*!< in: number of bytes to read */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */

/*******************************************************************//**
NOTE! Please use the corresponding macro os_file_read_no_error_handling(),
not directly this function!
This is the performance schema instrumented wrapper function for
os_file_read_no_error_handling_func() which requests a synchronous
read operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INLINE
ibool
pfs_os_file_read_no_error_handling_func(
/*====================================*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n,	/*!< in: number of bytes to read */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */

/*******************************************************************//**
NOTE! Please use the corresponding macro os_aio(), not directly this
function!
Performance schema wrapper function of os_aio() which requests
an asynchronous i/o operation.
@return TRUE if request was queued successfully, FALSE if fail */
UNIV_INLINE
ibool
pfs_os_aio_func(
/*============*/
	ulint		type,	/*!< in: OS_FILE_READ or OS_FILE_WRITE */
	ulint		mode,	/*!< in: OS_AIO_NORMAL etc. I/O mode */
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
	void*		message2,/*!< in: message for the aio handler
				(can be used to identify a completed
				aio operation); ignored if mode is
                                OS_AIO_SYNC */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */
/*******************************************************************//**
NOTE! Please use the corresponding macro os_file_write(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_write() which requests a synchronous write operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INLINE
ibool
pfs_os_file_write_func(
/*===================*/
	const char*	name,	/*!< in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/*!< in: handle to a file */
	const void*	buf,	/*!< in: buffer from which to write */
	os_offset_t	offset,	/*!< in: file offset where to write */
	ulint		n,	/*!< in: number of bytes to write */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */
/***********************************************************************//**
NOTE! Please use the corresponding macro os_file_flush(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_flush() which flushes the write buffers of a given file to the disk.
Flushes the write buffers of a given file to the disk.
@return TRUE if success */
UNIV_INLINE
ibool
pfs_os_file_flush_func(
/*===================*/
	os_file_t	file,	/*!< in, own: handle to a file */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */

/***********************************************************************//**
NOTE! Please use the corresponding macro os_file_rename(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_rename()
@return TRUE if success */
UNIV_INLINE
ibool
pfs_os_file_rename_func(
/*====================*/
	mysql_pfs_key_t	key,	/*!< in: Performance Schema Key */
	const char*	oldpath,/*!< in: old file path as a null-terminated
				string */
	const char*	newpath,/*!< in: new file path */
	const char*	src_file,/*!< in: file name where func invoked */
	ulint		src_line);/*!< in: line where the func invoked */
#endif	/* UNIV_PFS_IO */

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
@return	file size, or (os_offset_t) -1 on failure */
UNIV_INTERN
os_offset_t
os_file_get_size(
/*=============*/
	os_file_t	file)	/*!< in: handle to a file */
	__attribute__((warn_unused_result));
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
	__attribute__((nonnull, warn_unused_result));
/***********************************************************************//**
Truncates a file at its current position.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_set_eof(
/*============*/
	FILE*		file);	/*!< in: file to be truncated */
/***********************************************************************//**
Truncates a file to a specified size in bytes. Do nothing if the size
preserved is smaller or equal than current size of file.
@return TRUE if success */
UNIV_INTERN
ibool
os_file_truncate(
/*=============*/
	const char*	pathname,	/*!< in: file path */
	os_file_t	file,		/*!< in: file to be truncated */
	ulint		size);		/*!< in: size preserved in bytes */
/***********************************************************************//**
NOTE! Use the corresponding macro os_file_flush(), not directly this function!
Flushes the write buffers of a given file to the disk.
@return	TRUE if success */
UNIV_INTERN
ibool
os_file_flush_func(
/*===============*/
	os_file_t	file);	/*!< in, own: handle to a file */
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
	bool	report_all_errors);	/*!< in: TRUE if we want an error message
					printed of all errors */
/*******************************************************************//**
NOTE! Use the corresponding macro os_file_read(), not directly this function!
Requests a synchronous read operation.
@return	TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
os_file_read_func(
/*==============*/
	os_file_t	file,	/*!< in: handle to a file */
	void*		buf,	/*!< in: buffer where to read */
	os_offset_t	offset,	/*!< in: file offset where to read */
	ulint		n);	/*!< in: number of bytes to read */
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
	ulint		n);	/*!< in: number of bytes to read */

/*******************************************************************//**
NOTE! Use the corresponding macro os_file_write(), not directly this
function!
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
	const char*	new_name);	/*!< in: new file name */
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
	const char*	extention);	/*!< in: file extention; ibd,cfg*/
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
	char*	data_dir_path);	/*!< in/out: full path/data_dir_path */
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
ibool
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
	void*		message2);/*!< in: message for the aio handler
				(can be used to identify a completed
				aio operation); ignored if mode is
				OS_AIO_SYNC */
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
	ulint*	type);		/*!< out: OS_FILE_WRITE or ..._READ */
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
	ulint*	type);		/*!< out: OS_FILE_WRITE or ..._READ */
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
@return	DB_SUCCESS if all OK */
UNIV_INTERN
dberr_t
os_file_get_status(
/*===============*/
	const char*	path,		/*!< in: pathname of the file */
	os_file_stat_t* stat_info,	/*!< information of a file in a
					directory */
	bool		check_rw_perm);	/*!< in: for testing whether the
					file can be opened in RW mode */

#if !defined(UNIV_HOTBACKUP)
/*********************************************************************//**
Creates a temporary file that will be deleted on close.
This function is defined in ha_innodb.cc.
@return	temporary file descriptor, or < 0 on error */
UNIV_INTERN
int
innobase_mysql_tmpfile(void);
/*========================*/
#endif /* !UNIV_HOTBACKUP */


#if defined(LINUX_NATIVE_AIO)
/**************************************************************************
This function is only used in Linux native asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
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
	ulint*	type);		/*!< out: OS_FILE_WRITE or ..._READ */
#endif /* LINUX_NATIVE_AIO */

#ifndef UNIV_NONINL
#include "os0file.ic"
#endif

#endif
