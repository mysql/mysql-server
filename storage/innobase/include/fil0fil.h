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

/* When mysqld is run, the default directory "." is the mysqld datadir, but in
ibbackup we must set it explicitly; the patgh must NOT contain the trailing
'/' or '\' */
extern const char*	fil_path_to_mysql_datadir;

/* Initial size of a single-table tablespace in pages */
#define FIL_IBD_FILE_INITIAL_SIZE	4

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
#define FIL_PAGE_SPACE_OR_CHKSUM 0	/* in < MySQL-4.0.14 space id the
					page belongs to (== 0) but in later
					versions the 'new' checksum of the
					page */
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
#define FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID  34 /* starting from 4.1.x this
					contains the space id of the page */
#define FIL_PAGE_DATA		38	/* start of the data on the page */

/* File page trailer */
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8	/* the low 4 bytes of this are used
					to store the page checksum, the
					last 4 bytes should be identical
					to the last 4 bytes of FIL_PAGE_LSN */
#define FIL_PAGE_DATA_END	8

/* File page types */
#define FIL_PAGE_INDEX		17855
#define FIL_PAGE_UNDO_LOG	2
#define FIL_PAGE_INODE		3
#define FIL_PAGE_IBUF_FREE_LIST	4

/* Space types */
#define FIL_TABLESPACE 		501
#define FIL_LOG			502

extern ulint	fil_n_log_flushes;

extern ulint	fil_n_pending_log_flushes;
extern ulint	fil_n_pending_tablespace_flushes;


/***********************************************************************
Returns the version number of a tablespace, -1 if not found. */

ib_longlong
fil_space_get_version(
/*==================*/
			/* out: version number, -1 if the tablespace does not
			exist in the memory cache */
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
/***********************************************************************
Returns the ibuf data of a file space. */

ibuf_data_t*
fil_space_get_ibuf_data(
/*====================*/
			/* out: ibuf data for this space */
	ulint	id);	/* in: space id */
/***********************************************************************
Appends a new file to the chain of files of a space. File must be closed. */

void
fil_node_create(
/*============*/
	const char*	name,	/* in: file name (file must be closed) */
	ulint		size,	/* in: file size in database blocks, rounded
				downwards to an integer */
	ulint		id,	/* in: space id where to append */
	ibool		is_raw);/* in: TRUE if a raw device or
				a raw disk partition */
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
Creates a space memory object and puts it to the 'fil system' hash table. If
there is an error, prints an error message to the .err log. */

ibool
fil_space_create(
/*=============*/
				/* out: TRUE if success */
	const char*	name,	/* in: space name */
	ulint		id,	/* in: space id */
	ulint		purpose);/* in: FIL_TABLESPACE, or FIL_LOG if log */
/***********************************************************************
Frees a space object from a the tablespace memory cache. Closes the files in
the chain but does not delete them. */

ibool
fil_space_free(
/*===========*/
			/* out: TRUE if success */
	ulint	id);	/* in: space id */
/***********************************************************************
Returns the size of the space in pages. The tablespace must be cached in the
memory cache. */

ulint
fil_space_get_size(
/*===============*/
			/* out: space size, 0 if space not found */
	ulint	id);	/* in: space id */
/***********************************************************************
Checks if the pair space, page_no refers to an existing page in a tablespace
file space. The tablespace must be cached in the memory cache. */

ibool
fil_check_adress_in_tablespace(
/*===========================*/
			/* out: TRUE if the address is meaningful */
	ulint	id,	/* in: space id */
	ulint	page_no);/* in: page number */
/********************************************************************
Initializes the tablespace memory cache. */

void
fil_init(
/*=====*/
	ulint	max_n_open);	/* in: max number of open files */
/***********************************************************************
Opens all log files and system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup after the
space objects for the log and the system tablespace have been created. The
purpose of this operation is to make sure we never run out of file descriptors
if we need to read from the insert buffer or to write to the log. */

void
fil_open_log_and_system_tablespace_files(void);
/*==========================================*/
/***********************************************************************
Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */

void
fil_close_all_files(void);
/*=====================*/
/***********************************************************************
Sets the max tablespace id counter if the given number is bigger than the
previous value. */

void
fil_set_max_space_id_if_bigger(
/*===========================*/
	ulint	max_id);/* in: maximum known id */
/********************************************************************
Initializes the ibuf data structure for space 0 == the system tablespace.
This can be called after the file space headers have been created and the
dictionary system has been initialized. */

void
fil_ibuf_init_at_db_start(void);
/*===========================*/
/********************************************************************
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file in the system tablespace. */

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
#ifdef UNIV_LOG_ARCHIVE
	ulint*	min_arch_log_no,	/* in/out: */
	ulint*	max_arch_log_no,	/* in/out: */
#endif /* UNIV_LOG_ARCHIVE */
	dulint*	min_flushed_lsn,	/* in/out: */
	dulint*	max_flushed_lsn);	/* in/out: */
/***********************************************************************
Increments the count of pending insert buffer page merges, if space is not
being deleted. */

ibool
fil_inc_pending_ibuf_merges(
/*========================*/
			/* out: TRUE if being deleted, and ibuf merges should
			be skipped */
	ulint	id);	/* in: space id */
/***********************************************************************
Decrements the count of pending insert buffer page merges. */

void
fil_decr_pending_ibuf_merges(
/*========================*/
	ulint	id);	/* in: space id */
/***********************************************************************
Parses the body of a log record written about an .ibd file operation. That is,
the log record part after the standard (type, space id, page no) header of the
log record.

If desired, also replays the delete or rename operation if the .ibd file
exists and the space id in it matches. Replays the create operation if a file
at that path does not exist yet. If the database directory for the file to be
created does not exist, then we create the directory, too.

Note that ibbackup --apply-log sets fil_path_to_mysql_datadir to point to the
datadir that we should use in replaying the file operations. */

byte*
fil_op_log_parse_or_replay(
/*=======================*/
                        	/* out: end of log record, or NULL if the
				record was not completely contained between
				ptr and end_ptr */
        byte*   ptr,    	/* in: buffer containing the log record body,
				or an initial segment of it, if the record does
				not fir completely between ptr and end_ptr */
        byte*   end_ptr,	/* in: buffer end */
	ulint	type,		/* in: the type of this log record */
	ibool	do_replay,	/* in: TRUE if we want to replay the
				operation, and not just parse the log record */
	ulint	space_id);	/* in: if do_replay is TRUE, the space id of
				the tablespace in question; otherwise
				ignored */
/***********************************************************************
Deletes a single-table tablespace. The tablespace must be cached in the
memory cache. */

ibool
fil_delete_tablespace(
/*==================*/
			/* out: TRUE if success */
	ulint	id);	/* in: space id */
/***********************************************************************
Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but
1) we do not drop the table from the data dictionary;
2) we remove all insert buffer entries for the tablespace immediately; in DROP
TABLE they are only removed gradually in the background;
3) when the user does IMPORT TABLESPACE, the tablespace will have the same id
as it originally had. */

ibool
fil_discard_tablespace(
/*===================*/
			/* out: TRUE if success */
	ulint	id);	/* in: space id */
/***********************************************************************
Renames a single-table tablespace. The tablespace must be cached in the
tablespace memory cache. */

ibool
fil_rename_tablespace(
/*==================*/
					/* out: TRUE if success */
	const char*	old_name,	/* in: old table name in the standard
					databasename/tablename format of
					InnoDB, or NULL if we do the rename
					based on the space id only */
	ulint		id,		/* in: space id */
	const char*	new_name);	/* in: new table name in the standard
					databasename/tablename format
					of InnoDB */

/***********************************************************************
Creates a new single-table tablespace to a database directory of MySQL.
Database directories are under the 'datadir' of MySQL. The datadir is the
directory of a running mysqld program. We can refer to it by simply the
path '.'. Tables created with CREATE TEMPORARY TABLE we place in the temp
dir of the mysqld server. */

ulint
fil_create_new_single_table_tablespace(
/*===================================*/
					/* out: DB_SUCCESS or error code */
	ulint*		space_id,	/* in/out: space id; if this is != 0,
					then this is an input parameter,
					otherwise output */
	const char*	tablename,	/* in: the table name in the usual
					databasename/tablename format
					of InnoDB, or a dir path to a temp
					table */
	ibool		is_temp,	/* in: TRUE if a table created with
					CREATE TEMPORARY TABLE */
	ulint		size);		/* in: the initial size of the
					tablespace file in pages,
					must be >= FIL_IBD_FILE_INITIAL_SIZE */
/************************************************************************
Tries to open a single-table tablespace and optionally checks the space id is
right in it. If does not succeed, prints an error message to the .err log. This
function is used to open a tablespace when we start up mysqld, and also in
IMPORT TABLESPACE.
NOTE that we assume this operation is used either at the database startup
or under the protection of the dictionary mutex, so that two users cannot
race here. This operation does not leave the file associated with the
tablespace open, but closes it after we have looked at the space id in it. */

ibool
fil_open_single_table_tablespace(
/*=============================*/
					/* out: TRUE if success */
	ibool		check_space_id,	/* in: should we check that the space
					id in the file is right; we assume
					that this function runs much faster
					if no check is made, since accessing
					the file inode probably is much
					faster (the OS caches them) than
					accessing the first page of the file */
	ulint		id,		/* in: space id */
	const char*	name);		/* in: table name in the
					databasename/tablename format */
/************************************************************************
It is possible, though very improbable, that the lsn's in the tablespace to be
imported have risen above the current system lsn, if a lengthy purge, ibuf
merge, or rollback was performed on a backup taken with ibbackup. If that is
the case, reset page lsn's in the file. We assume that mysqld was shut down
after it performed these cleanup operations on the .ibd file, so that it at
the shutdown stamped the latest lsn to the FIL_PAGE_FILE_FLUSH_LSN in the
first page of the .ibd file, and we can determine whether we need to reset the
lsn's just by looking at that flush lsn. */

ibool
fil_reset_too_high_lsns(
/*====================*/
					/* out: TRUE if success */
	const char*	name,		/* in: table name in the
					databasename/tablename format */
	dulint		current_lsn);	/* in: reset lsn's if the lsn stamped
					to FIL_PAGE_FILE_FLUSH_LSN in the
					first page is too high */
/************************************************************************
At the server startup, if we need crash recovery, scans the database
directories under the MySQL datadir, looking for .ibd files. Those files are
single-table tablespaces. We need to know the space id in each of them so that
we know into which file we should look to check the contents of a page stored
in the doublewrite buffer, also to know where to apply log records where the
space id is != 0. */

ulint
fil_load_single_table_tablespaces(void);
/*===================================*/
			/* out: DB_SUCCESS or error number */
/************************************************************************
If we need crash recovery, and we have called
fil_load_single_table_tablespaces() and dict_load_single_table_tablespaces(),
we can call this function to print an error message of orphaned .ibd files
for which there is not a data dictionary entry with a matching table name
and space id. */

void
fil_print_orphaned_tablespaces(void);
/*================================*/
/***********************************************************************
Returns TRUE if a single-table tablespace does not exist in the memory cache,
or is being deleted there. */

ibool
fil_tablespace_deleted_or_being_deleted_in_mem(
/*===========================================*/
				/* out: TRUE if does not exist or is being\
				deleted */
	ulint		id,	/* in: space id */
	ib_longlong	version);/* in: tablespace_version should be this; if
				you pass -1 as the value of this, then this
				parameter is ignored */
/***********************************************************************
Returns TRUE if a single-table tablespace exists in the memory cache. */

ibool
fil_tablespace_exists_in_mem(
/*=========================*/
			/* out: TRUE if exists */
	ulint	id);	/* in: space id */
/***********************************************************************
Returns TRUE if a matching tablespace exists in the InnoDB tablespace memory
cache. Note that if we have not done a crash recovery at the database startup,
there may be many tablespaces which are not yet in the memory cache. */

ibool
fil_space_for_table_exists_in_mem(
/*==============================*/
					/* out: TRUE if a matching tablespace
					exists in the memory cache */
	ulint		id,		/* in: space id */
	const char*	name,		/* in: table name in the standard
					'databasename/tablename' format or
					the dir path to a temp table */
	ibool		is_temp,	/* in: TRUE if created with CREATE
					TEMPORARY TABLE */
	ibool		mark_space,	/* in: in crash recovery, at database
					startup we mark all spaces which have
					an associated table in the InnoDB
					data dictionary, so that
					we can print a warning about orphaned
					tablespaces */
	ibool		print_error_if_does_not_exist);
					/* in: print detailed error
					information to the .err log if a
					matching tablespace is not found from
					memory */
/**************************************************************************
Tries to extend a data file so that it would accommodate the number of pages
given. The tablespace must be cached in the memory cache. If the space is big
enough already, does nothing. */

ibool
fil_extend_space_to_desired_size(
/*=============================*/
				/* out: TRUE if success */
	ulint*	actual_size,	/* out: size of the space after extension;
				if we ran out of disk space this may be lower
				than the desired size */
	ulint	space_id,	/* in: space id */
	ulint	size_after_extend);/* in: desired size in pages after the
				extension; if the current space size is bigger
				than this already, the function does nothing */
#ifdef UNIV_HOTBACKUP
/************************************************************************
Extends all tablespaces to the size stored in the space header. During the
ibbackup --apply-log phase we extended the spaces on-demand so that log records
could be appllied, but that may have left spaces still too small compared to
the size stored in the space header. */

void
fil_extend_tablespaces_to_stored_len(void);
/*======================================*/
#endif
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
/***********************************************************************
Gets the number of reserved extents. If the database is silent, this number
should be zero. */

ulint
fil_space_get_n_reserved_extents(
/*=============================*/
	ulint	id);		/* in: space id */
/************************************************************************
Reads or writes data. This operation is asynchronous (aio). */

ulint
fil_io(
/*===*/
				/* out: DB_SUCCESS, or DB_TABLESPACE_DELETED
				if we are trying to do i/o on a tablespace
				which does not exist */
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
	ulint	len,		/* in: how many bytes to read or write; this
				must not cross a file boundary; in aio this
				must be a block size multiple */
	void*	buf,		/* in/out: buffer where to store read data
				or from where to write; in aio this must be
				appropriately aligned */
	void*	message);	/* in: message for aio handler if non-sync
				aio used, else ignored */
/************************************************************************
Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space. */

ulint
fil_read(
/*=====*/
				/* out: DB_SUCCESS, or DB_TABLESPACE_DELETED
				if we are trying to do i/o on a tablespace
				which does not exist */
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
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space. */

ulint
fil_write(
/*======*/
				/* out: DB_SUCCESS, or DB_TABLESPACE_DELETED
				if we are trying to do i/o on a tablespace
				which does not exist */
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
Flushes to disk possible writes cached by the OS. If the space does not exist
or is being dropped, does not do anything. */

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
Checks the consistency of the tablespace cache. */

ibool
fil_validate(void);
/*==============*/
			/* out: TRUE if ok */
/************************************************************************
Returns TRUE if file address is undefined. */

ibool
fil_addr_is_null(
/*=============*/
				/* out: TRUE if undefined */
	fil_addr_t	addr);	/* in: address */
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


typedef	struct fil_space_struct	fil_space_t;

#endif
