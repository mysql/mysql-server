/******************************************************
The interface to the operating system file io

(c) 1995 Innobase Oy

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#ifndef os0file_h
#define os0file_h

#include "univ.i"

#ifdef __WIN__

#include <windows.h>
#if (defined(__NT__) || defined(__WIN2000__))

#define WIN_ASYNC_IO

#endif

#define UNIV_NON_BUFFERED_IO

#else

#if defined(HAVE_AIO_H) && defined(HAVE_LIBRT)
#define POSIX_ASYNC_IO
#endif

#endif

#ifdef __WIN__
typedef	HANDLE	os_file_t;
#else
typedef int	os_file_t;
#endif

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

/* Options for file_create */
#define	OS_FILE_AIO			61
#define	OS_FILE_NORMAL			62

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
				
/********************************************************************
Opens an existing file or creates a new. */

os_file_t
os_file_create(
/*===========*/
			/* out, own: handle to the file, not defined if error,
			error number can be retrieved with os_get_last_error */
	char*	name,	/* in: name of the file or path as a null-terminated
			string */
	ulint	create_mode,/* in: OS_FILE_OPEN if an existing file is opened
			(if does not exist, error), or OS_FILE_CREATE if a new
			file is created (if exists, error), OS_FILE_OVERWRITE
			if a new file is created or an old overwritten */
	ulint	purpose,/* in: OS_FILE_AIO, if asynchronous, non-buffered i/o
			is desired, OS_FILE_NORMAL, if any normal file */
	ibool*	success);/* out: TRUE if succeed, FALSE if error */
/***************************************************************************
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error. */

ibool
os_file_close(
/*==========*/
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
Sets a file size. This function can be used to extend or truncate a file. */

ibool
os_file_set_size(
/*=============*/
				/* out: TRUE if success */
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	ulint		size,	/* in: least significant 32 bits of file
				size */
	ulint		size_high);/* in: most significant 32 bits of size */
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
os_file_get_last_error(void);
/*========================*/
		/* out: error number, or OS error number + 100 */
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
Requests a synchronous write operation. */

ibool
os_file_write(
/*==========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer from which to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to write */
	ulint		offset_high,/* in: most significant 32 bits of
				offset */
	ulint		n);	/* in: number of bytes to write */	
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
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read or from which
				to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read or write */
	ulint		offset_high, /* in: most significant 32 bits of
				offset */
	ulint		n,	/* in: number of bytes to read or write */	
	void*		message1,/* in: messages for the aio handler (these
				can be used to identify a completed aio
				operation); if mode is OS_AIO_SYNC, these
				are ignored */
	void*		message2);
/**************************************************************************
Wakes up simulated aio i/o-handler threads if they have something to do. */

void
os_aio_simulated_wake_handler_threads(void);
/*=======================================*/

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
	void**	message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2);
#endif
#ifdef POSIX_ASYNC_IO
/**************************************************************************
This function is only used in Posix asynchronous i/o. Waits for an aio
operation to complete. */

ibool
os_aio_posix_handle(
/*================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	array_no,	/* in: array number 0 - 3 */
	void**	message1,	/* out: the messages passed with the aio
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
	void**	message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2);
/**************************************************************************
Validates the consistency of the aio system. */

ibool
os_aio_validate(void);
/*=================*/
				/* out: TRUE if ok */
/**************************************************************************
Prints info of the aio arrays. */

void
os_aio_print(void);
/*==============*/
/**************************************************************************
Checks that all slots in the system have been freed, that is, there are
no pending io operations. */

ibool
os_aio_all_slots_free(void);
/*=======================*/
				/* out: TRUE if all free */
#endif 
