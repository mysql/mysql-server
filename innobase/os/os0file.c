/******************************************************
The interface to the operating system file i/o primitives

(c) 1995 Innobase Oy

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#include "os0file.h"
#include "os0sync.h"
#include "ut0mem.h"


#ifdef POSIX_ASYNC_IO
/* We assume in this case that the OS has standard Posix aio (at least SunOS
2.6, HP-UX 11i and AIX 4.3 have) */

#endif

/* We use these mutexes to protect lseek + file i/o operation, if the
OS does not provide an atomic pread or pwrite, or similar */
#define OS_FILE_N_SEEK_MUTEXES	16
os_mutex_t	os_file_seek_mutexes[OS_FILE_N_SEEK_MUTEXES];

/* In simulated aio, merge at most this many consecutive i/os */
#define OS_AIO_MERGE_N_CONSECUTIVE	32

/* If this flag is TRUE, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads */

ibool	os_aio_use_native_aio	= FALSE;

/* The aio array slot structure */
typedef struct os_aio_slot_struct	os_aio_slot_t;

struct os_aio_slot_struct{
	ibool		is_read;	/* TRUE if a read operation */
	ulint		pos;		/* index of the slot in the aio
					array */
	ibool		reserved;	/* TRUE if this slot is reserved */
	ulint		len;		/* length of the block to read or
					write */
	byte*		buf;		/* buffer used in i/o */
	ulint		type;		/* OS_FILE_READ or OS_FILE_WRITE */
	ulint		offset;		/* 32 low bits of file offset in
					bytes */
	ulint		offset_high;	/* 32 high bits of file offset */
	os_file_t	file;		/* file where to read or write */
	char*		name;		/* file name or path */
	ibool		io_already_done;/* used only in simulated aio:
					TRUE if the physical i/o already
					made and only the slot message
					needs to be passed to the caller
					of os_aio_simulated_handle */
	void*		message1;	/* message which is given by the */
	void*		message2;	/* the requester of an aio operation
					and which can be used to identify
					which pending aio operation was
					completed */
#ifdef WIN_ASYNC_IO
	OVERLAPPED	control;	/* Windows control block for the
					aio request */
#elif defined(POSIX_ASYNC_IO)
	struct aiocb	control;	/* Posix control block for aio
					request */
#endif
};

/* The aio array structure */
typedef struct os_aio_array_struct	os_aio_array_t;

struct os_aio_array_struct{
	os_mutex_t	mutex;	  /* the mutex protecting the aio array */
	os_event_t	not_full; /* The event which is set to signaled
				  state when there is space in the aio
				  outside the ibuf segment */
	ulint		n_slots;  /* Total number of slots in the aio array.
				  This must be divisible by n_threads. */
	ulint		n_segments;/* Number of segments in the aio array of
				  pending aio requests. A thread can wait
				  separately for any one of the segments. */
	ulint		n_reserved;/* Number of reserved slots in the
				  aio array outside the ibuf segment */
	os_aio_slot_t* 	slots;	  /* Pointer to the slots in the array */
	os_event_t*	events;	  /* Pointer to an array of event handles
				  where we copied the handles from slots,
				  in the same order. This can be used in
				  WaitForMultipleObjects; used only in
				  Windows */
};

/* Array of events used in simulated aio */
os_event_t*	os_aio_segment_wait_events	= NULL;

/* The aio arrays for non-ibuf i/o and ibuf i/o, as well as sync aio. These
are NULL when the module has not yet been initialized. */
os_aio_array_t*	os_aio_read_array	= NULL;
os_aio_array_t*	os_aio_write_array	= NULL;
os_aio_array_t*	os_aio_ibuf_array	= NULL;
os_aio_array_t*	os_aio_log_array	= NULL;
os_aio_array_t*	os_aio_sync_array	= NULL;

ulint	os_aio_n_segments	= ULINT_UNDEFINED;

/***************************************************************************
Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned. */

ulint
os_file_get_last_error(void)
/*========================*/
		/* out: error number, or OS error number + 100 */
{
	ulint	err;

#ifdef __WIN__

	err = (ulint) GetLastError();

	if (err == ERROR_FILE_NOT_FOUND) {
		return(OS_FILE_NOT_FOUND);
	} else if (err == ERROR_DISK_FULL) {
		return(OS_FILE_DISK_FULL);
	} else if (err == ERROR_FILE_EXISTS) {
		return(OS_FILE_ALREADY_EXISTS);
	} else {
		return(100 + err);
	}
#else
	err = (ulint) errno;

	if (err == ENOSPC ) {
		return(OS_FILE_DISK_FULL);
#ifdef POSIX_ASYNC_IO
	} else if (err == EAGAIN) {
		return(OS_FILE_AIO_RESOURCES_RESERVED);
#endif
	} else if (err == ENOENT) {
		return(OS_FILE_NOT_FOUND);
	} else if (err == EEXIST) {
		return(OS_FILE_ALREADY_EXISTS);
	} else {
		return(100 + err);
	}
#endif
}

/********************************************************************
Does error handling when a file operation fails. If we have run out
of disk space, then the user can clean the disk. If we do not find
a specified file, then the user can copy it to disk. */
static
ibool
os_file_handle_error(
/*=================*/
				/* out: TRUE if we should retry the operation */
	os_file_t	file,	/* in: file pointer */
	char*		name)	/* in: name of a file or NULL */
{
	ulint	err;

	UT_NOT_USED(file);

	err = os_file_get_last_error();
	
	if (err == OS_FILE_DISK_FULL) {
		fprintf(stderr, "\n");
		if (name) {
		  fprintf(stderr,
			"InnoDB: Encountered a problem with file %s.\n",
									name);
		}
		fprintf(stderr,
	   "InnoDB: Cannot continue operation.\n"
	   "InnoDB: Disk is full. Try to clean the disk to free space.\n"
	   "InnoDB: Delete possible created file and restart.\n");

		exit(1);

	} else if (err == OS_FILE_AIO_RESOURCES_RESERVED) {

		return(TRUE);
	} else {
		ut_error;
	}

	return(FALSE);	
}

/********************************************************************
Opens an existing file or creates a new. */

os_file_t
os_file_create(
/*===========*/
			/* out, own: handle to the file, not defined if error,
			error number can be retrieved with os_get_last_error */
	char*	name,	/* in: name of the file or path as a null-terminated
			string */
	ulint	create_mode, /* in: OS_FILE_OPEN if an existing file is opened
			(if does not exist, error), or OS_FILE_CREATE if a new
			file is created (if exists, error), OS_FILE_OVERWRITE
			if a new is created or an old overwritten */
	ulint	purpose,/* in: OS_FILE_AIO, if asynchronous, non-buffered i/o
			is desired, OS_FILE_NORMAL, if any normal file */
	ibool*	success)/* out: TRUE if succeed, FALSE if error */
{
#ifdef __WIN__
	os_file_t	file;
	DWORD		create_flag;
	DWORD		attributes;
	ibool		retry;
	
try_again:	
	ut_a(name);

	if (create_mode == OS_FILE_OPEN) {
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
		/* use asynchronous (overlapped) io and no buffering
		of writes in the OS */
		attributes = 0;
#ifdef WIN_ASYNC_IO
		if (os_aio_use_native_aio) {
			attributes = attributes | FILE_FLAG_OVERLAPPED;
		}
#endif			
#ifdef UNIV_NON_BUFFERED_IO
		attributes = attributes | FILE_FLAG_NO_BUFFERING;
#endif
	} else if (purpose == OS_FILE_NORMAL) {
		attributes = 0
#ifdef UNIV_NON_BUFFERED_IO
			 | FILE_FLAG_NO_BUFFERING
#endif
			;
	} else {
		attributes = 0;
		ut_error;
	}

	file = CreateFile(name,
			GENERIC_READ | GENERIC_WRITE, /* read and write
							access */
			FILE_SHARE_READ,/* file can be read by other
					processes */
			NULL,	/* default security attributes */
			create_flag,
			attributes,
			NULL);	/* no template file */

	if (file == INVALID_HANDLE_VALUE) {
		*success = FALSE;

		if (create_mode != OS_FILE_OPEN
		    && os_file_get_last_error() == OS_FILE_DISK_FULL) {

			retry = os_file_handle_error(file, name);

			if (retry) {
				goto try_again;
			}
		}
	} else {
		*success = TRUE;
	}

	return(file);
#else
	os_file_t	file;
	int		create_flag;
	ibool		retry;
	
try_again:	
	ut_a(name);

	if (create_mode == OS_FILE_OPEN) {
		create_flag = O_RDWR;
	} else if (create_mode == OS_FILE_CREATE) {
		create_flag = O_RDWR | O_CREAT | O_EXCL;
	} else if (create_mode == OS_FILE_OVERWRITE) {
		create_flag = O_RDWR | O_CREAT | O_TRUNC;
	} else {
		create_flag = 0;
		ut_error;
	}

	UT_NOT_USED(purpose);

	if (create_mode == OS_FILE_CREATE) {
	        file = open(name, create_flag, S_IRUSR | S_IWUSR | S_IRGRP
			                     | S_IWGRP | S_IROTH | S_IWOTH);
        } else {
                file = open(name, create_flag);
        }
	
	if (file == -1) {
		*success = FALSE;

		if (create_mode != OS_FILE_OPEN
		    && errno == ENOSPC) {

			retry = os_file_handle_error(file, name);

			if (retry) {
				goto try_again;
			}
		}
	} else {
		*success = TRUE;
	}

	return(file);	
#endif
}

/***************************************************************************
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error. */

ibool
os_file_close(
/*==========*/
				/* out: TRUE if success */
	os_file_t	file)	/* in, own: handle to a file */
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

/***************************************************************************
Gets a file size. */

ibool
os_file_get_size(
/*=============*/
				/* out: TRUE if success */
	os_file_t	file,	/* in: handle to a file */
	ulint*		size,	/* out: least significant 32 bits of file
				size */
	ulint*		size_high)/* out: most significant 32 bits of size */
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
	*size = (ulint) lseek(file, 0, SEEK_END);
	*size_high = 0;
	
	return(TRUE);	
#endif
}

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
	ulint		size_high)/* in: most significant 32 bits of size */
{
	ulint   offset;
	ulint   n_bytes;
	ulint	low;
	ibool	ret;
	ibool	retry;
	ulint   i;
	byte*   buf;

try_again:
	buf = ut_malloc(UNIV_PAGE_SIZE * 64);

	/* Write buffer full of zeros */
	for (i = 0; i < UNIV_PAGE_SIZE * 64; i++) {
	        buf[i] = '\0';
	}

	offset = 0;
	low = size;
#if (UNIV_WORD_SIZE == 8)
	low = low + (size_high << 32);
#else
	UT_NOT_USED(size_high);
#endif
	while (offset < low) {
	        if (low - offset < UNIV_PAGE_SIZE * 64) {
	                 n_bytes = low - offset;
	        } else {
	                 n_bytes = UNIV_PAGE_SIZE * 64;
	        }
	  
	        ret = os_file_write(name, file, buf, offset, 0, n_bytes);

	        if (!ret) {
			ut_free(buf);
	         	goto error_handling;
	        }
	        offset += n_bytes;
	}

	ut_free(buf);

	ret = os_file_flush(file);

	if (ret) {
	        return(TRUE);
	}

error_handling:
	retry = os_file_handle_error(file, name); 

	if (retry) {
		goto try_again;
	}
	
	ut_error;

	return(FALSE);
}

/***************************************************************************
Flushes the write buffers of a given file to the disk. */

ibool
os_file_flush(
/*==========*/
				/* out: TRUE if success */
	os_file_t	file)	/* in, own: handle to a file */
{
#ifdef __WIN__
	BOOL	ret;

	ut_a(file);

	ret = FlushFileBuffers(file);

	if (ret) {
		return(TRUE);
	}

	return(FALSE);
#else
	int	ret;
	
	ret = fsync(file);

	if (ret == 0) {
		return(TRUE);
	}
	
	return(FALSE);
#endif
}


#ifndef __WIN__
/***********************************************************************
Does a synchronous read operation in Posix. */
static
ssize_t
os_file_pread(
/*==========*/
				/* out: number of bytes read, -1 if error */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read */
	ulint		n,	/* in: number of bytes to read */	
	ulint		offset)	/* in: offset from where to read */
{
        off_t     offs = (off_t)offset;

#ifdef HAVE_PREAD
	return(pread(file, buf, n, offs));
#else
	ssize_t	ret;
	ulint	i;

	/* Protect the seek / read operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;
	
	os_mutex_enter(os_file_seek_mutexes[i]);

	ret = lseek(file, offs, 0);

	if (ret < 0) {
		os_mutex_exit(os_file_seek_mutexes[i]);

		return(ret);
	}
	
	ret = read(file, buf, n);

	os_mutex_exit(os_file_seek_mutexes[i]);

	return(ret);
#endif
}

/***********************************************************************
Does a synchronous write operation in Posix. */
static
ssize_t
os_file_pwrite(
/*===========*/
				/* out: number of bytes written, -1 if error */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer from where to write */
	ulint		n,	/* in: number of bytes to write */	
	ulint		offset)	/* in: offset where to write */
{
	ssize_t	ret;
	off_t   offs    = (off_t)offset;

#ifdef HAVE_PWRITE
	ret = pwrite(file, buf, n, offs);

	/* Always do fsync to reduce the probability that when the OS crashes,
	a database page is only partially physically written to disk. */

	ut_a(TRUE == os_file_flush(file));

        return(ret);
#else
	ulint	i;

	/* Protect the seek / write operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;
	
	os_mutex_enter(os_file_seek_mutexes[i]);

	ret = lseek(file, offs, 0);

	if (ret < 0) {
		os_mutex_exit(os_file_seek_mutexes[i]);

		return(ret);
	}
	
	ret = write(file, buf, n);

	/* Always do fsync to reduce the probability that when the OS crashes,
	a database page is only partially physically written to disk. */

	ut_a(TRUE == os_file_flush(file));

	os_mutex_exit(os_file_seek_mutexes[i]);

	return(ret);
#endif
}
#endif

/***********************************************************************
Requests a synchronous positioned read operation. */

ibool
os_file_read(
/*=========*/
				/* out: TRUE if request was
				successful, FALSE if fail */
	os_file_t	file,	/* in: handle to a file */
	void*		buf,	/* in: buffer where to read */
	ulint		offset,	/* in: least significant 32 bits of file
				offset where to read */
	ulint		offset_high, /* in: most significant 32 bits of
				offset */
	ulint		n)	/* in: number of bytes to read */	
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		err;
	DWORD		low;
	DWORD		high;
	ibool		retry;
	ulint		i;
	
try_again:	
	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);

	low = offset;
	high = offset_high;

	/* Protect the seek / read operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;
	
	os_mutex_enter(os_file_seek_mutexes[i]);

	ret2 = SetFilePointer(file, low, &high, FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
		err = GetLastError();

		os_mutex_exit(os_file_seek_mutexes[i]);

		goto error_handling;
	} 
	
	ret = ReadFile(file, buf, n, &len, NULL);

	os_mutex_exit(os_file_seek_mutexes[i]);
	
	if (ret && len == n) {
		return(TRUE);
	}		

	err = GetLastError();
#else
	ibool	retry;
	ssize_t	ret;
	
#if (UNIV_WORD_SIZE == 8)
	offset = offset + (offset_high << 32);
#else
	UT_NOT_USED(offset_high);
#endif	
try_again:
	ret = os_file_pread(file, buf, n, offset);

	if ((ulint)ret == n) {

		return(TRUE);
	}
#endif	
error_handling:
	retry = os_file_handle_error(file, NULL); 

	if (retry) {
		goto try_again;
	}
	
	ut_error;

	return(FALSE);
}

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
	ulint		offset_high, /* in: most significant 32 bits of
				offset */
	ulint		n)	/* in: number of bytes to write */	
{
#ifdef __WIN__
	BOOL		ret;
	DWORD		len;
	DWORD		ret2;
	DWORD		err;
	DWORD		low;
	DWORD		high;
	ibool		retry;
	ulint		i;

try_again:	
	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);

	low = offset;
	high = offset_high;
	
	/* Protect the seek / write operation with a mutex */
	i = ((ulint) file) % OS_FILE_N_SEEK_MUTEXES;
	
	os_mutex_enter(os_file_seek_mutexes[i]);

	ret2 = SetFilePointer(file, low, &high, FILE_BEGIN);

	if (ret2 == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
		err = GetLastError();

		os_mutex_exit(os_file_seek_mutexes[i]);
		
		goto error_handling;
	} 

	ret = WriteFile(file, buf, n, &len, NULL);
	
	/* Always do fsync to reduce the probability that when the OS crashes,
	a database page is only partially physically written to disk. */

	ut_a(TRUE == os_file_flush(file));

	os_mutex_exit(os_file_seek_mutexes[i]);

	if (ret && len == n) {
		return(TRUE);
	}
#else
	ibool	retry;
	ssize_t	ret;
	
#if (UNIV_WORD_SIZE == 8)
	offset = offset + (offset_high << 32);
#else
	UT_NOT_USED(offset_high);
#endif	
try_again:
	ret = os_file_pwrite(file, buf, n, offset);

	if ((ulint)ret == n) {
		return(TRUE);
	}
#endif
error_handling:		
	retry = os_file_handle_error(file, name); 

	if (retry) {
		goto try_again;
	}
	
	ut_error;

	return(FALSE);
}

/********************************************************************
Returns a pointer to the nth slot in the aio array. */
static
os_aio_slot_t*
os_aio_array_get_nth_slot(
/*======================*/
					/* out: pointer to slot */
	os_aio_array_t*		array,	/* in: aio array */
	ulint			index)	/* in: index of the slot */
{
	ut_a(index < array->n_slots);

	return((array->slots) + index);
}

/****************************************************************************
Creates an aio wait array. */
static
os_aio_array_t*
os_aio_array_create(
/*================*/
				/* out, own: aio array */
	ulint	n,		/* in: maximum number of pending aio operations
				allowed; n must be divisible by n_segments */
	ulint	n_segments) 	/* in: number of segments in the aio array */
{
	os_aio_array_t*	array;
	ulint		i;
	os_aio_slot_t*	slot;
#ifdef WIN_ASYNC_IO
	OVERLAPPED*	over;
#endif	
	ut_a(n > 0);
	ut_a(n_segments > 0);
	ut_a(n % n_segments == 0);

	array = ut_malloc(sizeof(os_aio_array_t));

	array->mutex 		= os_mutex_create(NULL);
	array->not_full		= os_event_create(NULL);
	array->n_slots  	= n;
	array->n_segments	= n_segments;
	array->n_reserved	= 0;
	array->slots		= ut_malloc(n * sizeof(os_aio_slot_t));
	array->events		= ut_malloc(n * sizeof(os_event_t));
	
	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		slot->pos = i;
		slot->reserved = FALSE;
#ifdef WIN_ASYNC_IO
		over = &(slot->control);

		over->hEvent = os_event_create(NULL);

		*((array->events) + i) = over->hEvent;
#endif
	}
	
	return(array);
}

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
	ulint	n_slots_sync)	/* in: number of slots in the sync aio array */
{
	ulint	n_read_segs;
	ulint	n_write_segs;
	ulint	n_per_seg;
	ulint	i;
#ifdef POSIX_ASYNC_IO
	sigset_t   sigset;
#endif
	ut_ad(n % n_segments == 0);
	ut_ad(n_segments >= 4);

	n_per_seg = n / n_segments;
	n_write_segs = (n_segments - 2) / 2;
	n_read_segs = n_segments - 2 - n_write_segs;
	
	/* printf("Array n per seg %lu\n", n_per_seg); */

	os_aio_read_array = os_aio_array_create(n_read_segs * n_per_seg,
							n_read_segs);
	os_aio_write_array = os_aio_array_create(n_write_segs * n_per_seg,
							n_write_segs);
	os_aio_ibuf_array = os_aio_array_create(n_per_seg, 1);

	os_aio_log_array = os_aio_array_create(n_per_seg, 1);

	os_aio_sync_array = os_aio_array_create(n_slots_sync, 1);

	os_aio_n_segments = n_segments;

	os_aio_validate();

	for (i = 0; i < OS_FILE_N_SEEK_MUTEXES; i++) {
		os_file_seek_mutexes[i] = os_mutex_create(NULL);
	}

	os_aio_segment_wait_events = ut_malloc(n_segments * sizeof(void*));

	for (i = 0; i < n_segments; i++) {
		os_aio_segment_wait_events[i] = os_event_create(NULL);
	}

#ifdef POSIX_ASYNC_IO
	/* Block aio signals from the current thread and its children:
	for this to work, the current thread must be the first created
	in the database, so that all its children will inherit its
	signal mask */
	
	/* TODO: to work MySQL needs the SIGALARM signal; the following
	will not work yet! */
        sigemptyset(&sigset);
	sigaddset(&sigset, SIGRTMIN + 1 + 0);
	sigaddset(&sigset, SIGRTMIN + 1 + 1);
	sigaddset(&sigset, SIGRTMIN + 1 + 2);
	sigaddset(&sigset, SIGRTMIN + 1 + 3);

	pthread_sigmask(SIG_BLOCK, &sigset, NULL); */
#endif
}
				
/**************************************************************************
Calculates segment number for a slot. */
static
ulint
os_aio_get_segment_no_from_slot(
/*============================*/
				/* out: segment number (which is the number
				used by, for example, i/o-handler threads) */
	os_aio_array_t*	array,	/* in: aio wait array */
	os_aio_slot_t*	slot)	/* in: slot in this array */
{
	ulint	segment;
	ulint	seg_len;

	if (array == os_aio_ibuf_array) {
		segment = 0;

	} else if (array == os_aio_log_array) {
		segment = 1;
		
	} else if (array == os_aio_read_array) {
		seg_len = os_aio_read_array->n_slots /
				os_aio_read_array->n_segments;

		segment = 2 + slot->pos / seg_len;
	} else {
		ut_a(array == os_aio_write_array);
		seg_len = os_aio_write_array->n_slots /
				os_aio_write_array->n_segments;

		segment = os_aio_read_array->n_segments + 2
				+ slot->pos / seg_len;
	}

	return(segment);
}

/**************************************************************************
Calculates local segment number and aio array from global segment number. */
static
ulint
os_aio_get_array_and_local_segment(
/*===============================*/
					/* out: local segment number within
					the aio array */
	os_aio_array_t** array,		/* out: aio wait array */
	ulint		 global_segment)/* in: global segment number */
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

/***********************************************************************
Gets an integer value designating a specified aio array. This is used
to give numbers to signals in Posix aio. */
static
ulint
os_aio_get_array_no(
/*================*/
	os_aio_array_t*	array)	/* in: aio array */
{	
	if (array == os_aio_ibuf_array) {
	
		return(0);

	} else if (array == os_aio_log_array) {

		return(1);

	} else if (array == os_aio_read_array) {

		return(2);
	} else if (array == os_aio_write_array) {

		return(3);
	} else {
		ut_a(0);

		return(0);
	}
}

/***********************************************************************
Gets the aio array for its number. */
static
os_aio_array_t*
os_aio_get_array_from_no(
/*=====================*/
			/* out: aio array */
	ulint	n)	/* in: array number */
{	
	if (n == 0) {
		return(os_aio_ibuf_array);
	} else if (n == 1) {

		return(os_aio_log_array);
	} else if (n == 2) {

		return(os_aio_read_array);
	} else if (n == 3) {

		return(os_aio_write_array);
	} else {
		ut_a(0);

		return(NULL);
	}
}

/***********************************************************************
Requests for a slot in the aio array. If no slot is available, waits until
not_full-event becomes signaled. */
static
os_aio_slot_t*
os_aio_array_reserve_slot(
/*======================*/
				/* out: pointer to slot */
	ulint		type,	/* in: OS_FILE_READ or OS_FILE_WRITE */
	os_aio_array_t*	array,	/* in: aio array */
	void*		message1,/* in: message to be passed along with
				the aio operation */
	void*		message2,/* in: message to be passed along with
				the aio operation */
	os_file_t	file,	/* in: file handle */
	char*		name,	/* in: name of the file or path as a
				null-terminated string */
	void*		buf,	/* in: buffer where to read or from which
				to write */
	ulint		offset,	/* in: least significant 32 bits of file
				offset */
	ulint		offset_high, /* in: most significant 32 bits of
				offset */
	ulint		len)	/* in: length of the block to read or write */
{
	os_aio_slot_t*	slot;
#ifdef WIN_ASYNC_IO
	OVERLAPPED*	control;

#elif defined(POSIX_ASYNC_IO)

	struct aiocb*	control;
#endif
	ulint		i;
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

	for (i = 0;; i++) {
		slot = os_aio_array_get_nth_slot(array, i);

		if (slot->reserved == FALSE) {
			break;
		}
	}

	array->n_reserved++;

	if (array->n_reserved == array->n_slots) {
		os_event_reset(array->not_full);
	}
	
	slot->reserved = TRUE;
	slot->message1 = message1;
	slot->message2 = message2;
	slot->file     = file;
	slot->name     = name;
	slot->len      = len;
	slot->type     = type;
	slot->buf      = buf;
	slot->offset   = offset;
	slot->offset_high = offset_high;
	slot->io_already_done = FALSE;
	
#ifdef WIN_ASYNC_IO		
	control = &(slot->control);
	control->Offset = (DWORD)offset;
	control->OffsetHigh = (DWORD)offset_high;
	os_event_reset(control->hEvent);

#elif defined(POSIX_ASYNC_IO)

#if (UNIV_WORD_SIZE == 8)
	offset = offset + (offset_high << 32);
#else
	ut_a(offset_high == 0);
#endif 
	control = &(slot->control);
	control->aio_fildes = file;
	control->aio_buf = buf;
	control->aio_nbytes = len;
	control->aio_offset = offset;
	control->aio_reqprio = 0;
	control->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	control->aio_sigevent.sigev_signo =
			SIGRTMIN + 1 + os_aio_get_array_no(array);
			/* TODO: How to choose the signal numbers? */
/*
	printf("AIO signal number %lu\n", (ulint) control->aio_sigevent.sigev_signo);
*/
	control->aio_sigevent.sigev_value.sival_ptr = slot;
#endif
	os_mutex_exit(array->mutex);

	return(slot);
}

/***********************************************************************
Frees a slot in the aio array. */
static
void
os_aio_array_free_slot(
/*===================*/
	os_aio_array_t*	array,	/* in: aio array */
	os_aio_slot_t*	slot)	/* in: pointer to slot */
{
	ut_ad(array);
	ut_ad(slot);

	os_mutex_enter(array->mutex);

	ut_ad(slot->reserved);
	
	slot->reserved = FALSE;

	array->n_reserved--;

	if (array->n_reserved == array->n_slots - 1) {
		os_event_set(array->not_full);
	}

#ifdef WIN_ASYNC_IO		
	os_event_reset(slot->control.hEvent);
#endif
	os_mutex_exit(array->mutex);
}

/**************************************************************************
Wakes up a simulated aio i/o-handler thread if it has something to do. */
static
void
os_aio_simulated_wake_handler_thread(
/*=================================*/
	ulint	global_segment)	/* in: the number of the segment in the aio
				arrays */
{
	os_aio_array_t*	array;
	ulint		segment;
	os_aio_slot_t*	slot;
	ulint		n;
	ulint		i;

	ut_ad(!os_aio_use_native_aio);

	segment = os_aio_get_array_and_local_segment(&array, global_segment);

	n = array->n_slots / array->n_segments;

	/* Look through n slots after the segment * n'th slot */

	os_mutex_enter(array->mutex);

	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (slot->reserved) {
			/* Found an i/o request */
			
			break;
		}
	}

	os_mutex_exit(array->mutex);

	if (i < n) {
		os_event_set(os_aio_segment_wait_events[global_segment]);
	}
}

/**************************************************************************
Wakes up simulated aio i/o-handler threads if they have something to do. */

void
os_aio_simulated_wake_handler_threads(void)
/*=======================================*/
{
	ulint	i;

	if (os_aio_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	for (i = 0; i < os_aio_n_segments; i++) {
		os_aio_simulated_wake_handler_thread(i);
	}
}

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
	void*		message2)
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
#ifdef WIN_ASYNC_IO
	BOOL		ret		= TRUE;
	DWORD		len		= n;
	void*		dummy_mess1;
	void*		dummy_mess2;
#endif
	ulint		err		= 0;
	ibool		retry;
	ulint		wake_later;

	ut_ad(file);
	ut_ad(buf);
	ut_ad(n > 0);
	ut_ad(n % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad((ulint)buf % OS_FILE_LOG_BLOCK_SIZE == 0)
	ut_ad(offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(os_aio_validate());

	wake_later = mode & OS_AIO_SIMULATED_WAKE_LATER;
	mode = mode & (~OS_AIO_SIMULATED_WAKE_LATER);
	
	if (mode == OS_AIO_SYNC
#ifdef WIN_ASYNC_IO
				&& !os_aio_use_native_aio
#endif
	) {
		/* This is actually an ordinary synchronous read or write:
		no need to use an i/o-handler thread. NOTE that if we use
		Windows async i/o, Windows does not allow us to use
		ordinary synchronous os_file_read etc. on the same file,
		therefore we have built a special mechanism for synchronous
		wait in the Windows case. */

		if (type == OS_FILE_READ) {
			return(os_file_read(file, buf, offset, offset_high, n));
		}

		ut_a(type == OS_FILE_WRITE);

		return(os_file_write(name, file, buf, offset, offset_high, n));
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
		ut_error;
	}
	
	slot = os_aio_array_reserve_slot(type, array, message1, message2, file,
					name, buf, offset, offset_high, n);
	if (type == OS_FILE_READ) {
		if (os_aio_use_native_aio) {
#ifdef WIN_ASYNC_IO
			ret = ReadFile(file, buf, (DWORD)n, &len,
							&(slot->control));
#elif defined(POSIX_ASYNC_IO)
			slot->control.aio_lio_opcode = LIO_READ;
			err = (ulint) aio_read(&(slot->control));
			printf("Starting Posix aio read %lu\n", err);
#endif
		} else {
			if (!wake_later) {
				os_aio_simulated_wake_handler_thread(
				 os_aio_get_segment_no_from_slot(array, slot));
			}
		}
	} else if (type == OS_FILE_WRITE) {
		if (os_aio_use_native_aio) {
#ifdef WIN_ASYNC_IO
			ret = WriteFile(file, buf, (DWORD)n, &len,
							&(slot->control));
#elif defined(POSIX_ASYNC_IO)
			slot->control.aio_lio_opcode = LIO_WRITE;
			err = (ulint) aio_write(&(slot->control));
			printf("Starting Posix aio write %lu\n", err);
#endif
		} else {
			if (!wake_later) {
				os_aio_simulated_wake_handler_thread(
				 os_aio_get_segment_no_from_slot(array, slot));
			}
		}
	} else {
		ut_error;
	}

#ifdef WIN_ASYNC_IO
	if (os_aio_use_native_aio) {
		if ((ret && len == n)
			|| (!ret && GetLastError() == ERROR_IO_PENDING)) {	

			/* aio was queued successfully! */
		
	    		if (mode == OS_AIO_SYNC) {
	    		    /* We want a synchronous i/o operation on a file
	    		    where we also use async i/o: in Windows we must
	    		    use the same wait mechanism as for async i/o */
	    		
	    		    return(os_aio_windows_handle(ULINT_UNDEFINED,
						slot->pos,
		    				&dummy_mess1, &dummy_mess2));
	    		}

			return(TRUE);
		}

		err = 1; /* Fall through the next if */
	}
#endif
	if (err == 0) {
		/* aio was queued successfully! */

		return(TRUE);
	}

	os_aio_array_free_slot(array, slot);

	retry = os_file_handle_error(file, name);

	if (retry) {

		goto try_again;
	}	

	ut_error;
	
	return(FALSE);
}

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
	void**	message2)
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	ulint		n;
	ulint		i;
	ibool		ret_val;
	ulint		err;
	BOOL		ret;
	DWORD		len;

	if (segment == ULINT_UNDEFINED) {
		array = os_aio_sync_array;
		segment = 0;
	} else {
		segment = os_aio_get_array_and_local_segment(&array, segment);
	}
	
	/* NOTE! We only access constant fields in os_aio_array. Therefore
	we do not have to acquire the protecting mutex yet */

	ut_ad(os_aio_validate());
	ut_ad(segment < array->n_segments);

	n = array->n_slots / array->n_segments;

	if (array == os_aio_sync_array) {
		ut_ad(pos < array->n_slots); 
		os_event_wait(array->events[pos]);
		i = pos;
	} else {
		i = os_event_wait_multiple(n, (array->events) + segment * n);
	}

	os_mutex_enter(array->mutex);

	slot = os_aio_array_get_nth_slot(array, i + segment * n);

	ut_a(slot->reserved);

	ret = GetOverlappedResult(slot->file, &(slot->control), &len, TRUE);

	*message1 = slot->message1;
	*message2 = slot->message2;

	if (ret && len == slot->len) {
		ret_val = TRUE;

		if (slot->type == OS_FILE_WRITE) {
		         ut_a(TRUE == os_file_flush(slot->file));
		}
	} else {
		err = GetLastError();
		ut_error;

		ret_val = FALSE;
	}		  

	os_mutex_exit(array->mutex);

	os_aio_array_free_slot(array, slot);
	
	return(ret_val);
}
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
	void**	message2)
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	siginfo_t	info;
	sigset_t	sigset;
	sigset_t        proc_sigset;
	sigset_t        thr_sigset;
	int		ret;
	int             i;
	int             sig;
	
        sigemptyset(&sigset);
	sigaddset(&sigset, SIGRTMIN + 1 + array_no);

	pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

	/*
	sigprocmask(0, NULL, &proc_sigset);
	pthread_sigmask(0, NULL, &thr_sigset);

	for (i = 32 ; i < 40; i++) {
	  printf("%lu : %lu %lu\n", (ulint)i,
		 (ulint)sigismember(&proc_sigset, i),
		 (ulint)sigismember(&thr_sigset, i));
	}
	*/

	ret = sigwaitinfo(&sigset, &info);

	if (sig != SIGRTMIN + 1 + array_no) {

		ut_a(0);
	
		return(FALSE);
	}
	
	printf("Handling Posix aio\n");

	array = os_aio_get_array_from_no(array_no);

	os_mutex_enter(array->mutex);

	slot = info.si_value.sival_ptr;

	ut_a(slot->reserved);

	*message1 = slot->message1;
	*message2 = slot->message2;

	if (slot->type == OS_FILE_WRITE) {
		ut_a(TRUE == os_file_flush(slot->file));
	}

	os_mutex_exit(array->mutex);

	os_aio_array_free_slot(array, slot);
	
	return(TRUE);
}
#endif

/**************************************************************************
Does simulated aio. This function should be called by an i/o-handler
thread. */

ibool
os_aio_simulated_handle(
/*====================*/
				/* out: TRUE if the aio operation succeeded */
	ulint	global_segment,	/* in: the number of the segment in the aio
				arrays to wait for; segment 0 is the ibuf
				i/o thread, segment 1 the log i/o thread,
				then follow the non-ibuf read threads, and as
				the last are the non-ibuf write threads */
	void**	message1,	/* out: the messages passed with the aio
				request; note that also in the case where
				the aio operation failed, these output
				parameters are valid and can be used to
				restart the operation, for example */
	void**	message2)
{
	os_aio_array_t*	array;
	ulint		segment;
	os_aio_slot_t*	slot;
	os_aio_slot_t*	slot2;
	os_aio_slot_t*	consecutive_ios[OS_AIO_MERGE_N_CONSECUTIVE];
	ulint		n_consecutive;
	ulint		total_len;
	ulint		offs;
	ulint		lowest_offset;
	byte*		combined_buf;
	ibool		ret;
	ulint		n;
	ulint		i;

	segment = os_aio_get_array_and_local_segment(&array, global_segment);
	
restart:
	/* Give other threads chance to add several i/os to the array
	at once */
	
	os_thread_yield();

	/* NOTE! We only access constant fields in os_aio_array. Therefore
	we do not have to acquire the protecting mutex yet */

	ut_ad(os_aio_validate());
	ut_ad(segment < array->n_segments);

	n = array->n_slots / array->n_segments;

	/* Look through n slots after the segment * n'th slot */

	os_mutex_enter(array->mutex);

	/* Check if there is a slot for which the i/o has already been
	done */
	
	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (slot->reserved && slot->io_already_done) {

			ret = TRUE;
			
			goto slot_io_done;
		}
	}

	n_consecutive = 0;

	/* Look for an i/o request at the lowest offset in the array */

	lowest_offset = ULINT_MAX;
	
	for (i = 0; i < n; i++) {
		slot = os_aio_array_get_nth_slot(array, i + segment * n);

		if (slot->reserved && slot->offset < lowest_offset) {

			/* Found an i/o request */
			consecutive_ios[0] = slot;

			n_consecutive = 1;

			lowest_offset = slot->offset;
		}
	}

	if (n_consecutive == 0) {

		/* No i/o requested at the moment */

		goto wait_for_io;
	}

	slot = consecutive_ios[0];

	/* Check if there are several consecutive blocks to read or write */

consecutive_loop:	
	for (i = 0; i < n; i++) {
		slot2 = os_aio_array_get_nth_slot(array, i + segment * n);

		if (slot2->reserved && slot2 != slot
		    && slot2->offset == slot->offset + slot->len
		    && slot->offset + slot->len > slot->offset /* check that
						sum does not wrap over */
		    && slot2->offset_high == slot->offset_high
		    && slot2->type == slot->type
		    && slot2->file == slot->file) {

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

	/* We have now collected n_consecutive i/o requests in the array;
	allocate a single buffer which can hold all data, and perform the
	i/o */

	total_len = 0;
	slot = consecutive_ios[0];
	
	for (i = 0; i < n_consecutive; i++) {
		total_len += consecutive_ios[i]->len;
	}

	if (n_consecutive == 1) {
		/* We can use the buffer of the i/o request */
		combined_buf = slot->buf;
	} else {
		combined_buf = ut_malloc(total_len);

		ut_a(combined_buf);
	}
	
	/* We release the array mutex for the time of the i/o: NOTE that
	this assumes that there is just one i/o-handler thread serving
	a single segment of slots! */

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

	/* Do the i/o with ordinary, synchronous i/o functions: */
	if (slot->type == OS_FILE_WRITE) {
		ret = os_file_write(slot->name, slot->file, combined_buf,
				slot->offset, slot->offset_high, total_len);
	} else {
		ret = os_file_read(slot->file, combined_buf,
				slot->offset, slot->offset_high, total_len);
	}

	ut_a(ret);
	
/* printf("aio: %lu consecutive %lu:th segment, first offs %lu blocks\n",
			n_consecutive, global_segment, slot->offset
					/ UNIV_PAGE_SIZE); */

	if (slot->type == OS_FILE_READ && n_consecutive > 1) {
		/* Copy the combined buffer to individual buffers */
		offs = 0;
		
		for (i = 0; i < n_consecutive; i++) {

			ut_memcpy(consecutive_ios[i]->buf, combined_buf + offs, 
						consecutive_ios[i]->len);
			offs += consecutive_ios[i]->len;
		}
	}

	if (n_consecutive > 1) {
		ut_free(combined_buf);
	}

	os_mutex_enter(array->mutex);

	/* Mark the i/os done in slots */

	for (i = 0; i < n_consecutive; i++) {
		consecutive_ios[i]->io_already_done = TRUE;
	}

	/* We return the messages for the first slot now, and if there were
	several slots, the messages will be returned with subsequent calls
	of this function */
	
slot_io_done:

	ut_a(slot->reserved);

	*message1 = slot->message1;
	*message2 = slot->message2;

	os_mutex_exit(array->mutex);

	os_aio_array_free_slot(array, slot);
	
	return(ret);

wait_for_io:
	/* We wait here until there again can be i/os in the segment
	of this thread */
	
	os_event_reset(os_aio_segment_wait_events[global_segment]);

	os_mutex_exit(array->mutex);

	os_event_wait(os_aio_segment_wait_events[global_segment]);

	goto restart;
}

/**************************************************************************
Validates the consistency of an aio array. */
static
ibool
os_aio_array_validate(
/*==================*/
				/* out: TRUE if ok */
	os_aio_array_t*	array)	/* in: aio wait array */
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

/**************************************************************************
Validates the consistency the aio system. */

ibool
os_aio_validate(void)
/*=================*/
				/* out: TRUE if ok */
{
	os_aio_array_validate(os_aio_read_array);
	os_aio_array_validate(os_aio_write_array);
	os_aio_array_validate(os_aio_ibuf_array);
	os_aio_array_validate(os_aio_log_array);
	os_aio_array_validate(os_aio_sync_array);

	return(TRUE);
}

/**************************************************************************
Prints info of the aio arrays. */

void
os_aio_print(void)
/*==============*/
{
	os_aio_array_t*	array;
	os_aio_slot_t*	slot;
	ulint		n_reserved;
	ulint		i;
	
	array = os_aio_read_array;
loop:
	ut_a(array);
	
	printf("INFO OF AN AIO ARRAY\n");

	os_mutex_enter(array->mutex);

	ut_a(array->n_slots > 0);
	ut_a(array->n_segments > 0);
	
	n_reserved = 0;

	for (i = 0; i < array->n_slots; i++) {
		slot = os_aio_array_get_nth_slot(array, i);
	
		if (slot->reserved) {
			n_reserved++;
			printf("Reserved slot, messages %lx %lx\n",
					(ulint)slot->message1,
					(ulint)slot->message2);
			ut_a(slot->len > 0);
		}
	}

	ut_a(array->n_reserved == n_reserved);

	printf("Total of %lu reserved aio slots\n", n_reserved);
	
	os_mutex_exit(array->mutex);

	if (array == os_aio_read_array) {
		array = os_aio_write_array;

		goto loop;
	}

	if (array == os_aio_write_array) {
		array = os_aio_ibuf_array;

		goto loop;
	}

	if (array == os_aio_ibuf_array) {
		array = os_aio_log_array;

		goto loop;
	}

	if (array == os_aio_log_array) {
		array = os_aio_sync_array;

		goto loop;
	}
}

/**************************************************************************
Checks that all slots in the system have been freed, that is, there are
no pending io operations. */

ibool
os_aio_all_slots_free(void)
/*=======================*/
				/* out: TRUE if all free */
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
