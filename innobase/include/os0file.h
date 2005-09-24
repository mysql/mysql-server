/******************************************************
The interface to the operating system file io

(c) 1995 Innobase Oy

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

typedef	struct fil_node_struct	fil_node_t;

#ifdef UNIV_DO_FLUSH
extern ibool	os_do_not_call_flush_at_each_write;
#endif /* UNIV_DO_FLUSH */
extern ibool	os_has_said_disk_full;
extern ibool	os_aio_print_debug;

extern ulint	os_file_n_pending_preads;
extern ulint	os_file_n_pending_pwrites;

extern ulint    os_n_pending_reads;
extern ulint    os_n_pending_writes;

#ifdef __WIN__

/* We define always WIN_ASYNC_IO, and check at run-time whether
   the OS actually supports it: Win 95 does not, NT does. */
#define WIN_ASYNC_IO

#define UNIV_NON_BUFFERED_IO

#endif

#ifdef __WIN__
#define os_file_t	HANDLE
#else
typedef int	os_file_t;
#endif

extern ulint	os_innodb_umask;

/* If this flag is TRUE, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads */

extern ibool	os_aio_use_native_aio;

#define OS_FILE_SECTOR_SIZE		512

/* The next value should be smaller or equal to the smallest sector size used
on any disk. A log block is required to be a portion of disk which is written
so that if the start and the end of a block get written to disk, then the
whole block gets written. This should be true even in most cases of a crash:
if this fails for a log block, then it is equivalent to a media failure in the
log. */

#define OS_FILE_LOG_BLOCK_SIZE		512

/* Options for file_create */
#define	OS_FILE_OPEN			51
#define	OS_FILE_CREATE			52
#define OS_FILE_OVERWRITE		53
#define OS_FILE_OPEN_RAW		54
#define	OS_FILE_CREATE_PATH		55
#define	OS_FILE_OPEN_RETRY		56	/* for os_file_create() on
						the first ibdata file */

#define OS_FILE_READ_ONLY 		333
#define	OS_FILE_READ_WRITE		444
#define	OS_FILE_READ_ALLOW_DELETE	555	/* for ibbackup */

/* Options for file_create */
#define	OS_FILE_AIO			61
#define	OS_FILE_NORMAL			62

/* Types for file create */
#define	OS_DATA_FILE			100
#define OS_LOG_FILE			101

/* Error codes from os_file_get_last_error */
#define	OS_FILE_NOT_FOUND		71
#define	OS_FILE_DISK_FULL		72
#define	OS_FILE_ALREADY_EXISTS		73
#define OS_FILE_AIO_RESOURCES_RESERVED	74	/* wait for OS aio resources
						to become available again */
#define	OS_FILE_ERROR_NOT_SPECIFIED	75

/* Types for aio operations */
#define OS_FILE_READ	10
#define OS_FILE_WRITE	11

#define OS_FILE_LOG	256	/* This can be ORed to type */

#define OS_AIO_N_PENDING_IOS_PER_THREAD 32	/* Win NT does not allow more
						than 64 */

/* Modes for aio operations */
#define OS_AIO_NORMAL	21	/* Normal asynchronous i/o not for ibuf
				pages or ibuf bitmap pages */
#define OS_AIO_IBUF	22	/* Asynchronous i/o for ibuf pages or ibuf
				bitmap pages */
#define OS_AIO_LOG  	23	/* Asynchronous i/o for the log */
#define OS_AIO_SYNC	24	/* Asynchronous i/o where the calling thread
				will itself wait for the i/o to complete,
				doing also the job of the i/o-handler thread;
				can be used for any pages, ibuf or non-ibuf.
				This is used to save CPU time, as we can do
				with fewer thread switches. Plain synchronous
				i/o is not as good, because it must serialize
				the file seek and read or write, causing a
				bottleneck for parallelism. */

#define OS_AIO_SIMULATED_WAKE_LATER	512 /* This can be ORed to mode
				in the call of os_aio(...),
				if the caller wants to post several i/o
				requests in a batch, and only after that
 				wake the i/o-handler thread; this has
				effect only in simulated aio */ 
#define OS_WIN31     1
#define OS_WIN95     2	
#define OS_WINNT     3
#define OS_WIN2000   4

extern ulint	os_n_file_reads;
extern ulint	os_n_file_writes;
extern ulint	os_n_fsyncs;

/* File types for directory entry data type */

enum os_file_type_enum{
    OS_FILE_TYPE_UNKNOWN = 0,
    OS_FILE_TYPE_FILE,	 		/* regular file */
    OS_FILE_TYPE_DIR,			/* directory */
    OS_FILE_TYPE_LINK 			/* symbolic link */
};
typedef enum os_file_type_enum	  os_file_type_t;

/* Maximum path string length in bytes when referring to tables with in the
'./databasename/tablename.ibd' path format; we can allocate at least 2 buffers
of this size from the thread stack; that is why this should not be made much
bigger than 4000 bytes */
#define OS_FILE_MAX_PATH	4000

/* Struct used in fetching information of a file in a directory */
struct os_file_stat_struct{
	char		name[OS_FILE_MAX_PATH];	/* path to a file */
	os_file_type_t	type;			/* file type */
	ib_longlong	size;			/* file size */
	time_t          ctime;			/* creation time */
	time_t		mtime;			/* modification time */
	time_t		atime;			/* access time */
};
typedef struct os_file_stat_struct	os_file_stat_t;

#ifdef __WIN__
typedef HANDLE  os_file_dir_t;	/* directory stream */
#else
typedef DIR*	os_file_dir_t;	/* directory stream */
#endif

/***************************************************************************
Gets the operating system version. Currently works only on Windows. */

ulint
os_get_os_version(void);
/*===================*/
                  /* out: OS_WIN95, OS_WIN31, OS_WINNT, or OS_WIN2000 */
/********************************************************************
Creates the seek mutexes used in positioned reads and writes. */

void
os_io_init_simple(void);
/*===================*/
/***************************************************************************
Creates a temporary file. */

FILE*
os_file_create_tmpfile(void);
/*========================*/
			/* out: temporary file handle (never NULL) */
/***************************************************************************
The os_file_opendir() function opens a directory stream corresponding to the
directory named by the dirname argument. The directory stream is positioned
at the first entry. In both Unix and Windows we automatically skip the '.'
and '..' items at the start of the directory listing. */

os_file_dir_t
os_file_opendir(
/*============*/
					/* out: directory stream, NULL if
					error */
	const char*	dirname,	/* in: directory name; it must not
					contain a trailing '\' or '/' */
	ibool		error_is_fatal);/* in: TRUE if we should treat an
					error as a fatal error; if we try to
					open symlinks then we do not wish a
					fatal error if it happens not to be
					a directory */
/***************************************************************************
Closes a directory stream. */

int
os_file_closedir(
/*=============*/
				/* out: 0 if success, -1 if failure */
	os_file_dir_t	dir);	/* in: directory stream */
/***************************************************************************
This function returns information of the next file in the directory. We jump
over the '.' and '..' entries in the directory. */

int
os_file_readdir_next_file(
/*======================*/
				/* out: 0 if ok, -1 if error, 1 if at the end
				of the directory */
	const char*	dirname,/* in: directory name or path */
	os_file_dir_t	dir,	/* in: directory stream */
	os_file_stat_t*	info);	/* in/out: buffer where the info is returned */
/*********************************************************************
This function attempts to create a directory named pathname. The new directory
gets default permissions. On Unix, the permissions are (0770 & ~umask). If the
directory exists already, nothing is done and the call succeeds, unless the
fail_if_exists arguments is true. */

ibool
os_file_create_directory(
/*=====================*/
					/* out: TRUE if call succeeds,
					FALSE on error */
	const char*	pathname,	/* in: directory name as
					null-terminated string */
	ibool		fail_if_exists);/* in: if TRUE, pre-existing directory
					is treated as an error. */
/********************************************************************
A simple function to open or create a file. */

os_file_t
os_file_create_simple(
/*==================*/
				/* out, own: handle to the file, not defined
				if error, error number can be retrieved with
				os_file_get_last_error */
	const char*	name,	/* in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/* in: OS_FILE_OPEN if an existing file is
				opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error), or
	                        OS_FILE_CREATE_PATH if new file
				(if exists, error) and subdirectories along
				its path are created (if needed)*/
	ulint		access_type,/* in: OS_FILE_READ_ONLY or
				OS_FILE_READ_WRITE */
	ibool*		success);/* out: TRUE if succeed, FALSE if error */
/********************************************************************
A simple function to open or create a file. */

os_file_t
os_file_create_simple_no_error_handling(
/*====================================*/
				/* out, own: handle to the file, not defined
				if error, error number can be retrieved with
				os_file_get_last_error */
	const char*	name,	/* in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/* in: OS_FILE_OPEN if an existing file
				is opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error) */
	ulint		access_type,/* in: OS_FILE_READ_ONLY,
				OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file */
	ibool*		success);/* out: TRUE if succeed, FALSE if error */
/********************************************************************
Opens an existing file or creates a new. */

os_file_t
os_file_create(
/*===========*/
				/* out, own: handle to the file, not defined
				if error, error number can be retrieved with
				os_file_get_last_error */
	const char*	name,	/* in: name of the file or path as a
				null-terminated string */
	ulint		create_mode,/* in: OS_FILE_OPEN if an existing file
				is opened (if does not exist, error), or
				OS_FILE_CREATE if a new file is created
				(if exists, error),
				OS_FILE_OVERWRITE if a new file is created
				or an old overwritten;
				OS_FILE_OPEN_RAW, if a raw device or disk
				partition should be opened */
	ulint		purpose,/* in: OS_FILE_AIO, if asynchronous,
				non-buffered i/o is desired,
				OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use
				async i/o or unbuffered i/o: look in the
				function source code for the exact rules */
	ulint		type,	/* in: OS_DATA_FILE or OS_LOG_FILE */
	ibool*		success);/* out: TRUE if succeed, FALSE if error */
/***************************************************************************
Deletes a file. The file has to be closed before calling this. */

ibool
os_file_delete(
/*===========*/
				/* out: TRUE if success */
	const char*	name);	/* in: file path as a null-terminated string */

/***************************************************************************
Deletes a file if it exists. The file has to be closed before calling this. */

ibool
os_file_delete_if_exists(
/*=====================*/
				/* out: TRUE if success */
	const char*	name);	/* in: file path as a null-terminated string */
/***************************************************************************
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function. */

ibool
os_file_rename(
/*===========*/
					/* out: TRUE if success */
	const char*	oldpath,	/* in: old file path as a
					null-terminated string */
	const char*	newpath);	/* in: new file path */
/***************************************************************************
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error. */

ibool
os_file_close(
/*==========*/
				/* out: TRUE if success */
	os_file_t	file);	/* in, own: handle to a file */
/***************************************************************************
Closes a file handle. */

ibool
os_file_close_no_error_handling(
/*============================*/
				/* out: TRUE if success */
	os_file_t	file);	/* in, own: handle to a file */
/***************************************************************************
Gets a file size. */

ibool
os_file_get_size(
/*=============*/
				/* out: TRUE if success */
	os_file_t	file,	/* in: handle to a file */
	ulint*		size,	/* out: least significant 32 bits of file
				size */
	ulint*		size_high);/* out: most significant 32 bits of size */
/***************************************************************************
Gets file size as a 64-bit integer ib_longlong. */

ib_longlong
os_file_get_size_as_iblonglong(
/*===========================*/
				/* out: size in bytes, -1 if error */
	os_file_t	file);	/* in: handle to a file */
/***************************************************************************
Write the specified number of zeros to a newly created file. */

ibool
os_file_set_size(
/*=============*/
				/* out: TRUE if success */
	const char*	name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	ulint		size,	/* in: least significant 32 bits of file
				size */
	ulint		size_high);/* in: most significant 32 bits of size */
/***************************************************************************
Truncates a file at its current position. */

ibool
os_file_set_eof(
/*============*/
				/* out: TRUE if success */
	FILE*		file);	/* in: file to be truncated */
/***************************************************************************
Flushes the write buffers of a given file to the disk. */

ibool
os_file_flush(
/*==========*/
				/* out: TRUE if success */
	os_file_t	file);	/* in, own: handle to a file */
/***************************************************************************
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned. */

ulint
os_file_get_last_error(
/*===================*/
					/* out: error number, or OS error
					number + 100 */
	ibool	report_all_errors);	/* in: TRUE if we want an error message
					printed of all errors */
/***********************************************************************
Requests a synchronous read operation. */

ibool
os_file_read(
/*=========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high,/* in: most significant 32 bits of
				offset */
	ulint		n);	/* in: number of bytes to read */	
/***********************************************************************
Rewind file to its start, read at most size - 1 bytes from it to str, and
NUL-terminate str. All errors are silently ignored. This function is
mostly meant to be used with temporary files. */

void
os_file_read_string(
/*================*/
	FILE*	file,	/* in: file to read from */
	char*	str,	/* in: buffer where to read */
	ulint	size);	/* in: size of buffer */
/***********************************************************************
Requests a synchronous positioned read operation. This function does not do
any error handling. In case of error it returns FALSE. */

ibool
os_file_read_no_error_handling(
/*===========================*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high,/* in: most significant 32 bits of
				offset */
	ulint		n);	/* in: number of bytes to read */	

/***********************************************************************
Requests a synchronous write operation. */

ibool
os_file_write(
/*==========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	const char*	name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	const void*	buf,	/* in: buffer from which to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to write */
	ulint		offset_high,/* in: most significant 32 bits of
				offset */
	ulint		n);	/* in: number of bytes to write */	
/***********************************************************************
Check the existence and type of the given file. */

ibool
os_file_status(
/*===========*/
				/* out: TRUE if call succeeded */
	const char*	path,	/* in:  pathname of the file */
	ibool*		exists,	/* out: TRUE if file exists */
	os_file_type_t* type);	/* out: type of the file (if it exists) */
/********************************************************************
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

       path           dirname        basename
       "/usr/lib"     "/usr"         "lib"
       "/usr/"        "/"            "usr"
       "usr"          "."            "usr"
       "/"            "/"            "/"
       "."            "."            "."
       ".."           "."            ".."
*/

char*
os_file_dirname(
/*============*/
				/* out, own: directory component of the
				pathname */
	const char*	path);	/* in: pathname */
/********************************************************************
Creates all missing subdirectories along the given path. */
	
ibool
os_file_create_subdirs_if_needed(
/*=============================*/
				/* out: TRUE if call succeeded
				   FALSE otherwise */
	const char*	path);	/* in: path name */
/****************************************************************************
Initializes the asynchronous io system. Creates separate aio array for
non-ibuf read and write, a third aio array for the ibuf i/o, with just one
segment, two aio arrays for log reads and writes with one segment, and a
synchronous aio array of the specified size. The combined number of segments
in the three first aio arrays is the parameter n_segments given to the
function. The caller must create an i/o handler thread for each segment in
the four first arrays, but not for the sync aio array. */

void
os_aio_init(
/*========*/
	ulint	n,		/* in: maximum number of pending aio operations
				allowed; n must be divisible by n_segments */
	ulint	n_segments,	/* in: combined number of segments in the four
				first aio arrays; must be >= 4 */
	ulint	n_slots_sync);	/* in: number of slots in the sync aio array */
/***********************************************************************
Requests an asynchronous i/o operation. */

ibool
os_aio(
/*===*/
				/* out: TRUE if request was queued
				successfully, FALSE if fail */
	ulint		type,	/* in: OS_FILE_READ or OS_FILE_WRITE */
	ulint		mode,	/* in: OS_AIO_NORMAL, ..., possibly ORed
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
	const char*	name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read or from which
				to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read or write */
	ulint		offset_high, /* in: most significant 32 bits of
				offset */
	ulint		n,	/* in: number of bytes to read or write */	
	fil_node_t*	message1,/* in: messages for the aio handler (these
				can be used to identify a completed aio
				operation); if mode is OS_AIO_SYNC, these
				are ignored */
	void*		message2);
/****************************************************************************
Wakes up all async i/o threads so that they know to exit themselves in
shutdown. */

void
os_aio_wake_all_threads_at_shutdown(void);
/*=====================================*/
/****************************************************************************
Waits until there are no pending writes in os_aio_write_array. There can
be other, synchronous, pending writes. */

void
os_aio_wait_until_no_pending_writes(void);
/*=====================================*/
/**************************************************************************
Wakes up simulated aio i/o-handler threads if they have something to do. */

void
os_aio_simulated_wake_handler_threads(void);
/*=======================================*/
/**************************************************************************
This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */

void
os_aio_simulated_put_read_threads_to_sleep(void);
/*============================================*/

#ifdef WIN_ASYNC_IO
/**************************************************************************
This function is only used in Windows asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing! */

ibool
os_aio_windows_handle(
/*==================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	segment,	/* in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads; if
				this is ULINT_UNDEFINED, then it means that
				sync aio is used, and this parameter is
				ignored */
	ulint	pos,		/* this parameter is used only in sync aio:
				wait for the aio slot at this position */  
	fil_node_t**message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2,
	ulint*	type);		/* out: OS_FILE_WRITE or ..._READ */
#endif

/* Currently we do not use Posix async i/o */
#ifdef POSIX_ASYNC_IO
/**************************************************************************
This function is only used in Posix asynchronous i/o. Waits for an aio
operation to complete. */

ibool
os_aio_posix_handle(
/*================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	array_no,	/* in: array number 0 - 3 */
	fil_node_t**message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2);
#endif
/**************************************************************************
Does simulated aio. This function should be called by an i/o-handler
thread. */

ibool
os_aio_simulated_handle(
/*====================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	segment,	/* in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads */
	fil_node_t**message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2,
	ulint*	type);		/* out: OS_FILE_WRITE or ..._READ */
/**************************************************************************
Validates the consistency of the aio system. */

ibool
os_aio_validate(void);
/*=================*/
				/* out: TRUE if ok */
/**************************************************************************
Prints info of the aio arrays. */

void
os_aio_print(
/*=========*/
	FILE*	file);	/* in: file where to print */
/**************************************************************************
Refreshes the statistics used to print per-second averages. */

void
os_aio_refresh_stats(void);
/*======================*/

#ifdef UNIV_DEBUG
/**************************************************************************
Checks that all slots in the system have been freed, that is, there are
no pending io operations. */

ibool
os_aio_all_slots_free(void);
/*=======================*/
#endif /* UNIV_DEBUG */

/***********************************************************************
This function returns information about the specified file */
ibool
os_file_get_status(
/*===============*/
					/* out: TRUE if stat information found */
	const char*     path,		/* in:  pathname of the file */
	os_file_stat_t* stat_info);	/* information of a file in a directory */

#endif 
