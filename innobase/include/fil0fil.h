/******************************************************
The low-level file system

(c) 1995 Innobase Oy

Created 10/25/1995 Heikki Tuuri
*******************************************************/

#ifndef fil0fil_h
#define fil0fil_h

#include "univ.i"
#include "sync0rw.h"
#include "dict0types.h"
#include "ibuf0types.h"
#include "ut0byte.h"
#include "os0file.h"

/* 'null' (undefined) page offset in the context of file spaces */
#define	FIL_NULL	ULINT32_UNDEFINED

/* Space address data type; this is intended to be used when
addresses accurate to a byte are stored in file pages. If the page part
of the address is FIL_NULL, the address is considered undefined. */

typedef	byte	fil_faddr_t;	/* 'type' definition in C: an address
				stored in a file page is a string of bytes */
#define FIL_ADDR_PAGE	0	/* first in address is the page offset */
#define	FIL_ADDR_BYTE	4	/* then comes 2-byte byte offset within page*/

#define	FIL_ADDR_SIZE	6	/* address size is 6 bytes */

/* A struct for storing a space address FIL_ADDR, when it is used
in C program data structures. */

typedef struct fil_addr_struct	fil_addr_t;
struct fil_addr_struct{
	ulint	page;		/* page number within a space */
	ulint	boffset;	/* byte offset within the page */
};

/* Null file address */
extern fil_addr_t	fil_addr_null;

/* The byte offsets on a file page for various variables */
#define FIL_PAGE_SPACE		0	/* space id the page belongs to */
#define FIL_PAGE_OFFSET		4	/* page offset inside space */
#define FIL_PAGE_PREV		8	/* if there is a 'natural' predecessor
					of the page, its offset */
#define FIL_PAGE_NEXT		12	/* if there is a 'natural' successor
					of the page, its offset */
#define FIL_PAGE_LSN		16	/* lsn of the end of the newest
					modification log record to the page */
#define	FIL_PAGE_TYPE		24	/* file page type: FIL_PAGE_INDEX,...,
					2 bytes */
#define FIL_PAGE_FILE_FLUSH_LSN	26	/* this is only defined for the
					first page in a data file: the file
					has been flushed to disk at least up
					to this lsn */
#define FIL_PAGE_ARCH_LOG_NO	34	/* this is only defined for the
					first page in a data file: the latest
					archived log file number when the
					flush lsn above was written */
#define FIL_PAGE_DATA		38	/* start of the data on the page */

/* File page trailer */
#define FIL_PAGE_END_LSN	8	/* this should be same as
					FIL_PAGE_LSN */
#define FIL_PAGE_DATA_END	8

/* File page types */
#define FIL_PAGE_INDEX		17855
#define FIL_PAGE_UNDO_LOG	2

/* Space types */
#define FIL_TABLESPACE 		501
#define FIL_LOG			502

/***********************************************************************
Reserves a right to open a single file. The right must be released with
fil_release_right_to_open. */

void
fil_reserve_right_to_open(void);
/*===========================*/
/***********************************************************************
Releases a right to open a single file. */

void
fil_release_right_to_open(void);
/*===========================*/
/************************************************************************
Returns TRUE if file address is undefined. */
ibool
fil_addr_is_null(
/*=============*/
				/* out: TRUE if undefined */
	fil_addr_t	addr);	/* in: address */
/********************************************************************
Initializes the file system of this module. */

void
fil_init(
/*=====*/
	ulint	max_n_open);	/* in: max number of open files */
/********************************************************************
Initializes the ibuf indexes at a database start. This can be called
after the file space headers have been created and the dictionary system
has been initialized. */

void
fil_ibuf_init_at_db_start(void);
/*===========================*/
/***********************************************************************
Creates a space object and puts it to the file system. */

void
fil_space_create(
/*=============*/
	char*	name,	/* in: space name */
	ulint	id,	/* in: space id */
	ulint	purpose);/* in: FIL_TABLESPACE, or FIL_LOG if log */
/********************************************************************
Drops files from the start of a file space, so that its size is cut by
the amount given. */

void
fil_space_truncate_start(
/*=====================*/
	ulint	id,		/* in: space id */
	ulint	trunc_len);	/* in: truncate by this much; it is an error
				if this does not equal to the combined size of
				some initial files in the space */
/***********************************************************************
Frees a space object from a file system. Closes the files in the chain
but does not delete them. */

void
fil_space_free(
/*===========*/
	ulint	id);	/* in: space id */
/***********************************************************************
Returns the latch of a file space. */

rw_lock_t*
fil_space_get_latch(
/*================*/
			/* out: latch protecting storage allocation */
	ulint	id);	/* in: space id */
/***********************************************************************
Returns the type of a file space. */

ulint
fil_space_get_type(
/*===============*/
			/* out: FIL_TABLESPACE or FIL_LOG */
	ulint	id);	/* in: space id */
/********************************************************************
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file. */

ulint
fil_write_flushed_lsn_to_data_files(
/*================================*/
				/* out: DB_SUCCESS or error number */
	dulint	lsn,		/* in: lsn to write */
	ulint	arch_log_no);	/* in: latest archived log file number */
/***********************************************************************
Reads the flushed lsn and arch no fields from a data file at database
startup. */

void
fil_read_flushed_lsn_and_arch_log_no(
/*=================================*/
	os_file_t data_file,		/* in: open data file */
	ibool	one_read_already,	/* in: TRUE if min and max parameters
					below already contain sensible data */
	dulint*	min_flushed_lsn,	/* in/out: */
	ulint*	min_arch_log_no,	/* in/out: */
	dulint*	max_flushed_lsn,	/* in/out: */
	ulint*	max_arch_log_no);	/* in/out: */
/***********************************************************************
Returns the ibuf data of a file space. */

ibuf_data_t*
fil_space_get_ibuf_data(
/*====================*/
			/* out: ibuf data for this space */
	ulint	id);	/* in: space id */
/***********************************************************************
Returns the size of the space in pages. */

ulint
fil_space_get_size(
/*===============*/
			/* out: space size */
	ulint	id);	/* in: space id */
/***********************************************************************
Appends a new file to the chain of files of a space.
File must be closed. */

void
fil_node_create(
/*============*/
	char*	name,	/* in: file name (file must be closed) */
	ulint	size,	/* in: file size in database blocks, rounded downwards
			to an integer */
	ulint	id);	/* in: space id where to append */
/************************************************************************
Reads or writes data. This operation is asynchronous (aio). */

void
fil_io(
/*===*/
	ulint	type,		/* in: OS_FILE_READ or OS_FILE_WRITE,
				ORed to OS_FILE_LOG, if a log i/o
				and ORed to OS_AIO_SIMULATED_WAKE_LATER
				if simulated aio and we want to post a
				batch of i/os; NOTE that a simulated batch
				may introduce hidden chances of deadlocks,
				because i/os are not actually handled until
				all have been posted: use with great
				caution! */
	ibool	sync,		/* in: TRUE if synchronous aio is desired */
	ulint	space_id,	/* in: space id */
	ulint	block_offset,	/* in: offset in number of blocks */
	ulint	byte_offset,	/* in: remainder of offset in bytes; in
				aio this must be divisible by the OS block
				size */
	ulint	len,		/* in: how many bytes to read; this must
				not cross a file boundary; in aio this must
				be a block size multiple */
	void*	buf,		/* in/out: buffer where to store read data
				or from where to write; in aio this must be
				appropriately aligned */
	void*	message);	/* in: message for aio handler if non-sync
				aio used, else ignored */
/************************************************************************
Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of a file are ignored: they are not taken into account when
calculating the byte offset within a space. */

void
fil_read(
/*=====*/
	ibool	sync,		/* in: TRUE if synchronous aio is desired */
	ulint	space_id,	/* in: space id */
	ulint	block_offset,	/* in: offset in number of blocks */
	ulint	byte_offset,	/* in: remainder of offset in bytes; in aio
				this must be divisible by the OS block size */
	ulint	len,		/* in: how many bytes to read; this must not
				cross a file boundary; in aio this must be a
				block size multiple */
	void*	buf,		/* in/out: buffer where to store data read;
				in aio this must be appropriately aligned */
	void*	message);	/* in: message for aio handler if non-sync
				aio used, else ignored */
/************************************************************************
Writes data to a space from a buffer. Remember that the possible incomplete
blocks at the end of a file are ignored: they are not taken into account when
calculating the byte offset within a space. */

void
fil_write(
/*======*/
	ibool	sync,		/* in: TRUE if synchronous aio is desired */
	ulint	space_id,	/* in: space id */
	ulint	block_offset,	/* in: offset in number of blocks */
	ulint	byte_offset,	/* in: remainder of offset in bytes; in aio
				this must be divisible by the OS block size */
	ulint	len,		/* in: how many bytes to write; this must
				not cross a file boundary; in aio this must
				be a block size multiple */
	void*	buf,		/* in: buffer from which to write; in aio
				this must be appropriately aligned */
	void*	message);	/* in: message for aio handler if non-sync
				aio used, else ignored */
/**************************************************************************
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.c for more info). The thread specifies which
segment it wants to wait for. */

void
fil_aio_wait(
/*=========*/
	ulint	segment);	/* in: the number of the segment in the aio
				array to wait for */ 
/**************************************************************************
Flushes to disk possible writes cached by the OS. */

void
fil_flush(
/*======*/
	ulint	space_id);	/* in: file space id (this can be a group of
				log files or a tablespace of the database) */
/**************************************************************************
Flushes to disk writes in file spaces of the given type possibly cached by
the OS. */

void
fil_flush_file_spaces(
/*==================*/
	ulint	purpose);	/* in: FIL_TABLESPACE, FIL_LOG */
/**********************************************************************
Checks the consistency of the file system. */

ibool
fil_validate(void);
/*==============*/
			/* out: TRUE if ok */
/************************************************************************
Accessor functions for a file page */

ulint
fil_page_get_prev(byte*	page);
ulint
fil_page_get_next(byte*	page);
/*************************************************************************
Sets the file page type. */

void
fil_page_set_type(
/*==============*/
	byte* 	page,	/* in: file page */
	ulint	type);	/* in: type */
/*************************************************************************
Gets the file page type. */

ulint
fil_page_get_type(
/*==============*/
			/* out: type; NOTE that if the type has not been
			written to page, the return value not defined */
	byte* 	page);	/* in: file page */
/***********************************************************************
Tries to reserve free extents in a file space. */

ibool
fil_space_reserve_free_extents(
/*===========================*/
				/* out: TRUE if succeed */
	ulint	id,		/* in: space id */
	ulint	n_free_now,	/* in: number of free extents now */
	ulint	n_to_reserve);	/* in: how many one wants to reserve */
/***********************************************************************
Releases free extents in a file space. */

void
fil_space_release_free_extents(
/*===========================*/
	ulint	id,		/* in: space id */
	ulint	n_reserved);	/* in: how many one reserved */

typedef	struct fil_space_struct	fil_space_t;

#endif
