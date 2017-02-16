/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file include/fil0fil.h
The low-level file system

Created 10/25/1995 Heikki Tuuri
*******************************************************/

#ifndef fil0fil_h
#define fil0fil_h

#include "univ.i"
#include "dict0types.h"
#include "ut0byte.h"
#include "os0file.h"
#ifndef UNIV_HOTBACKUP
#include "sync0rw.h"
#include "ibuf0types.h"
#endif /* !UNIV_HOTBACKUP */
#include "trx0types.h"

/** When mysqld is run, the default directory "." is the mysqld datadir,
but in the MySQL Embedded Server Library and ibbackup it is not the default
directory, and we must set the base file path explicitly */
extern const char*	fil_path_to_mysql_datadir;

/** Initial size of a single-table tablespace in pages */
#define FIL_IBD_FILE_INITIAL_SIZE	4

/** 'null' (undefined) page offset in the context of file spaces */
#define	FIL_NULL	ULINT32_UNDEFINED

/* Space address data type; this is intended to be used when
addresses accurate to a byte are stored in file pages. If the page part
of the address is FIL_NULL, the address is considered undefined. */

typedef	byte	fil_faddr_t;	/*!< 'type' definition in C: an address
				stored in a file page is a string of bytes */
#define FIL_ADDR_PAGE	0	/* first in address is the page offset */
#define	FIL_ADDR_BYTE	4	/* then comes 2-byte byte offset within page*/

#define	FIL_ADDR_SIZE	6	/* address size is 6 bytes */

/** A struct for storing a space address FIL_ADDR, when it is used
in C program data structures. */

typedef struct fil_addr_struct	fil_addr_t;
/** File space address */
struct fil_addr_struct{
	ulint	page;		/*!< page number within a space */
	ulint	boffset;	/*!< byte offset within the page */
};

/** The null file address */
extern fil_addr_t	fil_addr_null;

/** The byte offsets on a file page for various variables @{ */
#define FIL_PAGE_SPACE_OR_CHKSUM 0	/*!< in < MySQL-4.0.14 space id the
					page belongs to (== 0) but in later
					versions the 'new' checksum of the
					page */
#define FIL_PAGE_OFFSET		4	/*!< page offset inside space */
#define FIL_PAGE_PREV		8	/*!< if there is a 'natural'
					predecessor of the page, its
					offset.  Otherwise FIL_NULL.
					This field is not set on BLOB
					pages, which are stored as a
					singly-linked list.  See also
					FIL_PAGE_NEXT. */
#define FIL_PAGE_NEXT		12	/*!< if there is a 'natural' successor
					of the page, its offset.
					Otherwise FIL_NULL.
					B-tree index pages
					(FIL_PAGE_TYPE contains FIL_PAGE_INDEX)
					on the same PAGE_LEVEL are maintained
					as a doubly linked list via
					FIL_PAGE_PREV and FIL_PAGE_NEXT
					in the collation order of the
					smallest user record on each page. */
#define FIL_PAGE_LSN		16	/*!< lsn of the end of the newest
					modification log record to the page */
#define	FIL_PAGE_TYPE		24	/*!< file page type: FIL_PAGE_INDEX,...,
					2 bytes.

					The contents of this field can only
					be trusted in the following case:
					if the page is an uncompressed
					B-tree index page, then it is
					guaranteed that the value is
					FIL_PAGE_INDEX.
					The opposite does not hold.

					In tablespaces created by
					MySQL/InnoDB 5.1.7 or later, the
					contents of this field is valid
					for all uncompressed pages. */
#define FIL_PAGE_FILE_FLUSH_LSN	26	/*!< this is only defined for the
					first page in a system tablespace
					data file (ibdata*, not *.ibd):
					the file has been flushed to disk
					at least up to this lsn */
#define FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID  34 /*!< starting from 4.1.x this
					contains the space id of the page */
#define FIL_PAGE_DATA		38	/*!< start of the data on the page */
#define FIL_PAGE_DATA_ALIGN_32	40
/* @} */
/** File page trailer @{ */
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8	/*!< the low 4 bytes of this are used
					to store the page checksum, the
					last 4 bytes should be identical
					to the last 4 bytes of FIL_PAGE_LSN */
#define FIL_PAGE_DATA_END	8	/*!< size of the page trailer */
/* @} */

/** File page types (values of FIL_PAGE_TYPE) @{ */
#define FIL_PAGE_INDEX		17855	/*!< B-tree node */
#define FIL_PAGE_UNDO_LOG	2	/*!< Undo log page */
#define FIL_PAGE_INODE		3	/*!< Index node */
#define FIL_PAGE_IBUF_FREE_LIST	4	/*!< Insert buffer free list */
/* File page types introduced in MySQL/InnoDB 5.1.7 */
#define FIL_PAGE_TYPE_ALLOCATED	0	/*!< Freshly allocated page */
#define FIL_PAGE_IBUF_BITMAP	5	/*!< Insert buffer bitmap */
#define FIL_PAGE_TYPE_SYS	6	/*!< System page */
#define FIL_PAGE_TYPE_TRX_SYS	7	/*!< Transaction system data */
#define FIL_PAGE_TYPE_FSP_HDR	8	/*!< File space header */
#define FIL_PAGE_TYPE_XDES	9	/*!< Extent descriptor page */
#define FIL_PAGE_TYPE_BLOB	10	/*!< Uncompressed BLOB page */
#define FIL_PAGE_TYPE_ZBLOB	11	/*!< First compressed BLOB page */
#define FIL_PAGE_TYPE_ZBLOB2	12	/*!< Subsequent compressed BLOB page */
#define FIL_PAGE_TYPE_LAST	FIL_PAGE_TYPE_ZBLOB2
					/*!< Last page type */
/* @} */

/** Space types @{ */
#define FIL_TABLESPACE		501	/*!< tablespace */
#define FIL_LOG			502	/*!< redo log */
/* @} */

/** The number of fsyncs done to the log */
extern ulint	fil_n_log_flushes;

/** Number of pending redo log flushes */
extern ulint	fil_n_pending_log_flushes;
/** Number of pending tablespace flushes */
extern ulint	fil_n_pending_tablespace_flushes;


#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Returns the version number of a tablespace, -1 if not found.
@return version number, -1 if the tablespace does not exist in the
memory cache */
UNIV_INTERN
ib_int64_t
fil_space_get_version(
/*==================*/
	ulint	id);	/*!< in: space id */
/*******************************************************************//**
Returns the latch of a file space.
@return	latch protecting storage allocation */
UNIV_INTERN
rw_lock_t*
fil_space_get_latch(
/*================*/
	ulint	id,	/*!< in: space id */
	ulint*	zip_size);/*!< out: compressed page size, or
			0 for uncompressed tablespaces */
/*******************************************************************//**
Returns the type of a file space.
@return	FIL_TABLESPACE or FIL_LOG */
UNIV_INTERN
ulint
fil_space_get_type(
/*===============*/
	ulint	id);	/*!< in: space id */
#endif /* !UNIV_HOTBACKUP */
/*******************************************************************//**
Appends a new file to the chain of files of a space. File must be closed. */
UNIV_INTERN
void
fil_node_create(
/*============*/
	const char*	name,	/*!< in: file name (file must be closed) */
	ulint		size,	/*!< in: file size in database blocks, rounded
				downwards to an integer */
	ulint		id,	/*!< in: space id where to append */
	ibool		is_raw);/*!< in: TRUE if a raw device or
				a raw disk partition */
#ifdef UNIV_LOG_ARCHIVE
/****************************************************************//**
Drops files from the start of a file space, so that its size is cut by
the amount given. */
UNIV_INTERN
void
fil_space_truncate_start(
/*=====================*/
	ulint	id,		/*!< in: space id */
	ulint	trunc_len);	/*!< in: truncate by this much; it is an error
				if this does not equal to the combined size of
				some initial files in the space */
#endif /* UNIV_LOG_ARCHIVE */
/*******************************************************************//**
Creates a space memory object and puts it to the 'fil system' hash table. If
there is an error, prints an error message to the .err log.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_space_create(
/*=============*/
	const char*	name,	/*!< in: space name */
	ulint		id,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size, or
				0 for uncompressed tablespaces */
	ulint		purpose);/*!< in: FIL_TABLESPACE, or FIL_LOG if log */
/*******************************************************************//**
Assigns a new space id for a new single-table tablespace. This works simply by
incrementing the global counter. If 4 billion id's is not enough, we may need
to recycle id's.
@return	TRUE if assigned, FALSE if not */
UNIV_INTERN
ibool
fil_assign_new_space_id(
/*====================*/
	ulint*	space_id);	/*!< in/out: space id */
/*******************************************************************//**
Returns the size of the space in pages. The tablespace must be cached in the
memory cache.
@return	space size, 0 if space not found */
UNIV_INTERN
ulint
fil_space_get_size(
/*===============*/
	ulint	id);	/*!< in: space id */
/*******************************************************************//**
Returns the flags of the space. The tablespace must be cached
in the memory cache.
@return	flags, ULINT_UNDEFINED if space not found */
UNIV_INTERN
ulint
fil_space_get_flags(
/*================*/
	ulint	id);	/*!< in: space id */
/*******************************************************************//**
Returns the compressed page size of the space, or 0 if the space
is not compressed. The tablespace must be cached in the memory cache.
@return	compressed page size, ULINT_UNDEFINED if space not found */
UNIV_INTERN
ulint
fil_space_get_zip_size(
/*===================*/
	ulint	id);	/*!< in: space id */
/*******************************************************************//**
Checks if the pair space, page_no refers to an existing page in a tablespace
file space. The tablespace must be cached in the memory cache.
@return	TRUE if the address is meaningful */
UNIV_INTERN
ibool
fil_check_adress_in_tablespace(
/*===========================*/
	ulint	id,	/*!< in: space id */
	ulint	page_no);/*!< in: page number */
/****************************************************************//**
Initializes the tablespace memory cache. */
UNIV_INTERN
void
fil_init(
/*=====*/
	ulint	hash_size,	/*!< in: hash table size */
	ulint	max_n_open);	/*!< in: max number of open files */
/*******************************************************************//**
Initializes the tablespace memory cache. */
UNIV_INTERN
void
fil_close(void);
/*===========*/
/*******************************************************************//**
Opens all log files and system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup after the
space objects for the log and the system tablespace have been created. The
purpose of this operation is to make sure we never run out of file descriptors
if we need to read from the insert buffer or to write to the log. */
UNIV_INTERN
void
fil_open_log_and_system_tablespace_files(void);
/*==========================================*/
/*******************************************************************//**
Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
UNIV_INTERN
void
fil_close_all_files(void);
/*=====================*/
/*******************************************************************//**
Sets the max tablespace id counter if the given number is bigger than the
previous value. */
UNIV_INTERN
void
fil_set_max_space_id_if_bigger(
/*===========================*/
	ulint	max_id);/*!< in: maximum known id */
#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file in the system tablespace.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
fil_write_flushed_lsn_to_data_files(
/*================================*/
	ib_uint64_t	lsn,		/*!< in: lsn to write */
	ulint		arch_log_no);	/*!< in: latest archived log
					file number */
/*******************************************************************//**
Reads the flushed lsn, arch no, and tablespace flag fields from a data
file at database startup.
@retval NULL on success, or if innodb_force_recovery is set
@return pointer to an error message string */
UNIV_INTERN
const char*
fil_read_first_page(
/*================*/
	os_file_t	data_file,		/*!< in: open data file */
	ibool		one_read_already,	/*!< in: TRUE if min and max
						parameters below already
						contain sensible data */
	ulint*		flags,			/*!< out: tablespace flags */
#ifdef UNIV_LOG_ARCHIVE
	ulint*		min_arch_log_no,	/*!< out: min of archived
						log numbers in data files */
	ulint*		max_arch_log_no,	/*!< out: max of archived
						log numbers in data files */
#endif /* UNIV_LOG_ARCHIVE */
	ib_uint64_t*	min_flushed_lsn,	/*!< out: min of flushed
						lsn values in data files */
	ib_uint64_t*	max_flushed_lsn)	/*!< out: max of flushed
						lsn values in data files */
	__attribute__((warn_unused_result));
/*******************************************************************//**
Increments the count of pending operation, if space is not being deleted.
@return	TRUE if being deleted, and operation should be skipped */
UNIV_INTERN
ibool
fil_inc_pending_ops(
/*================*/
	ulint	id);	/*!< in: space id */
/*******************************************************************//**
Decrements the count of pending operations. */
UNIV_INTERN
void
fil_decr_pending_ops(
/*=================*/
	ulint	id);	/*!< in: space id */
#endif /* !UNIV_HOTBACKUP */
/*******************************************************************//**
Parses the body of a log record written about an .ibd file operation. That is,
the log record part after the standard (type, space id, page no) header of the
log record.

If desired, also replays the delete or rename operation if the .ibd file
exists and the space id in it matches. Replays the create operation if a file
at that path does not exist yet. If the database directory for the file to be
created does not exist, then we create the directory, too.

Note that ibbackup --apply-log sets fil_path_to_mysql_datadir to point to the
datadir that we should use in replaying the file operations.
@return end of log record, or NULL if the record was not completely
contained between ptr and end_ptr */
UNIV_INTERN
byte*
fil_op_log_parse_or_replay(
/*=======================*/
	byte*	ptr,		/*!< in: buffer containing the log record body,
				or an initial segment of it, if the record does
				not fir completely between ptr and end_ptr */
	byte*	end_ptr,	/*!< in: buffer end */
	ulint	type,		/*!< in: the type of this log record */
	ulint	space_id,	/*!< in: the space id of the tablespace in
				question, or 0 if the log record should
				only be parsed but not replayed */
	ulint	log_flags);	/*!< in: redo log flags
				(stored in the page number parameter) */
/*******************************************************************//**
Deletes a single-table tablespace. The tablespace must be cached in the
memory cache.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_delete_tablespace(
/*==================*/
	ulint	id,		/*!< in: space id */
	ibool	evict_all);	/*!< in: TRUE if we want all pages
				evicted from LRU. */
#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but
1) we do not drop the table from the data dictionary;
2) we remove all insert buffer entries for the tablespace immediately; in DROP
TABLE they are only removed gradually in the background;
3) when the user does IMPORT TABLESPACE, the tablespace will have the same id
as it originally had.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_discard_tablespace(
/*===================*/
	ulint	id);	/*!< in: space id */
#endif /* !UNIV_HOTBACKUP */
/*******************************************************************//**
Renames a single-table tablespace. The tablespace must be cached in the
tablespace memory cache.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_rename_tablespace(
/*==================*/
	const char*	old_name,	/*!< in: old table name in the standard
					databasename/tablename format of
					InnoDB, or NULL if we do the rename
					based on the space id only */
	ulint		id,		/*!< in: space id */
	const char*	new_name);	/*!< in: new table name in the standard
					databasename/tablename format
					of InnoDB */

/*******************************************************************//**
Creates a new single-table tablespace to a database directory of MySQL.
Database directories are under the 'datadir' of MySQL. The datadir is the
directory of a running mysqld program. We can refer to it by simply the
path '.'. Tables created with CREATE TEMPORARY TABLE we place in the temp
dir of the mysqld server.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
fil_create_new_single_table_tablespace(
/*===================================*/
	ulint		space_id,	/*!< in: space id */
	const char*	tablename,	/*!< in: the table name in the usual
					databasename/tablename format
					of InnoDB, or a dir path to a temp
					table */
	ibool		is_temp,	/*!< in: TRUE if a table created with
					CREATE TEMPORARY TABLE */
	ulint		flags,		/*!< in: tablespace flags */
	ulint		size);		/*!< in: the initial size of the
					tablespace file in pages,
					must be >= FIL_IBD_FILE_INITIAL_SIZE */
#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Tries to open a single-table tablespace and optionally checks the space id is
right in it. If does not succeed, prints an error message to the .err log. This
function is used to open a tablespace when we start up mysqld, and also in
IMPORT TABLESPACE.
NOTE that we assume this operation is used either at the database startup
or under the protection of the dictionary mutex, so that two users cannot
race here. This operation does not leave the file associated with the
tablespace open, but closes it after we have looked at the space id in it.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_open_single_table_tablespace(
/*=============================*/
	ibool		check_space_id,	/*!< in: should we check that the space
					id in the file is right; we assume
					that this function runs much faster
					if no check is made, since accessing
					the file inode probably is much
					faster (the OS caches them) than
					accessing the first page of the file */
	ulint		id,		/*!< in: space id */
	ulint		flags,		/*!< in: tablespace flags */
	const char*	name,		/*!< in: table name in the
					databasename/tablename format */
	trx_t*		trx);		/*!< in: transaction. This is only used
					for IMPORT TABLESPACE, must be NULL
					otherwise */
/********************************************************************//**
It is possible, though very improbable, that the lsn's in the tablespace to be
imported have risen above the current system lsn, if a lengthy purge, ibuf
merge, or rollback was performed on a backup taken with ibbackup. If that is
the case, reset page lsn's in the file. We assume that mysqld was shut down
after it performed these cleanup operations on the .ibd file, so that it at
the shutdown stamped the latest lsn to the FIL_PAGE_FILE_FLUSH_LSN in the
first page of the .ibd file, and we can determine whether we need to reset the
lsn's just by looking at that flush lsn.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_reset_too_high_lsns(
/*====================*/
	const char*	name,		/*!< in: table name in the
					databasename/tablename format */
	ib_uint64_t	current_lsn);	/*!< in: reset lsn's if the lsn stamped
					to FIL_PAGE_FILE_FLUSH_LSN in the
					first page is too high */
#endif /* !UNIV_HOTBACKUP */
/********************************************************************//**
At the server startup, if we need crash recovery, scans the database
directories under the MySQL datadir, looking for .ibd files. Those files are
single-table tablespaces. We need to know the space id in each of them so that
we know into which file we should look to check the contents of a page stored
in the doublewrite buffer, also to know where to apply log records where the
space id is != 0.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
fil_load_single_table_tablespaces(void);
/*===================================*/
/*******************************************************************//**
Returns TRUE if a single-table tablespace does not exist in the memory cache,
or is being deleted there.
@return	TRUE if does not exist or is being\ deleted */
UNIV_INTERN
ibool
fil_tablespace_deleted_or_being_deleted_in_mem(
/*===========================================*/
	ulint		id,	/*!< in: space id */
	ib_int64_t	version);/*!< in: tablespace_version should be this; if
				you pass -1 as the value of this, then this
				parameter is ignored */
/*******************************************************************//**
Returns TRUE if a single-table tablespace exists in the memory cache.
@return	TRUE if exists */
UNIV_INTERN
ibool
fil_tablespace_exists_in_mem(
/*=========================*/
	ulint	id);	/*!< in: space id */
#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Returns TRUE if a matching tablespace exists in the InnoDB tablespace memory
cache. Note that if we have not done a crash recovery at the database startup,
there may be many tablespaces which are not yet in the memory cache.
@return	TRUE if a matching tablespace exists in the memory cache */
UNIV_INTERN
ibool
fil_space_for_table_exists_in_mem(
/*==============================*/
	ulint		id,		/*!< in: space id */
	const char*	name,		/*!< in: table name in the standard
					'databasename/tablename' format or
					the dir path to a temp table */
	ibool		is_temp,	/*!< in: TRUE if created with CREATE
					TEMPORARY TABLE */
	ibool		mark_space,	/*!< in: in crash recovery, at database
					startup we mark all spaces which have
					an associated table in the InnoDB
					data dictionary, so that
					we can print a warning about orphaned
					tablespaces */
	ibool		print_error_if_does_not_exist);
					/*!< in: print detailed error
					information to the .err log if a
					matching tablespace is not found from
					memory */
#else /* !UNIV_HOTBACKUP */
/********************************************************************//**
Extends all tablespaces to the size stored in the space header. During the
ibbackup --apply-log phase we extended the spaces on-demand so that log records
could be appllied, but that may have left spaces still too small compared to
the size stored in the space header. */
UNIV_INTERN
void
fil_extend_tablespaces_to_stored_len(void);
/*======================================*/
#endif /* !UNIV_HOTBACKUP */
/**********************************************************************//**
Tries to extend a data file so that it would accommodate the number of pages
given. The tablespace must be cached in the memory cache. If the space is big
enough already, does nothing.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_extend_space_to_desired_size(
/*=============================*/
	ulint*	actual_size,	/*!< out: size of the space after extension;
				if we ran out of disk space this may be lower
				than the desired size */
	ulint	space_id,	/*!< in: space id */
	ulint	size_after_extend);/*!< in: desired size in pages after the
				extension; if the current space size is bigger
				than this already, the function does nothing */
/*******************************************************************//**
Tries to reserve free extents in a file space.
@return	TRUE if succeed */
UNIV_INTERN
ibool
fil_space_reserve_free_extents(
/*===========================*/
	ulint	id,		/*!< in: space id */
	ulint	n_free_now,	/*!< in: number of free extents now */
	ulint	n_to_reserve);	/*!< in: how many one wants to reserve */
/*******************************************************************//**
Releases free extents in a file space. */
UNIV_INTERN
void
fil_space_release_free_extents(
/*===========================*/
	ulint	id,		/*!< in: space id */
	ulint	n_reserved);	/*!< in: how many one reserved */
/*******************************************************************//**
Gets the number of reserved extents. If the database is silent, this number
should be zero. */
UNIV_INTERN
ulint
fil_space_get_n_reserved_extents(
/*=============================*/
	ulint	id);		/*!< in: space id */
/********************************************************************//**
Reads or writes data. This operation is asynchronous (aio).
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
#define fil_io(type, sync, space_id, zip_size, block_offset, byte_offset, len, buf, message) \
	_fil_io(type, sync, space_id, zip_size, block_offset, byte_offset, len, buf, message, NULL)

UNIV_INTERN
ulint
_fil_io(
/*===*/
	ulint	type,		/*!< in: OS_FILE_READ or OS_FILE_WRITE,
				ORed to OS_FILE_LOG, if a log i/o
				and ORed to OS_AIO_SIMULATED_WAKE_LATER
				if simulated aio and we want to post a
				batch of i/os; NOTE that a simulated batch
				may introduce hidden chances of deadlocks,
				because i/os are not actually handled until
				all have been posted: use with great
				caution! */
	ibool	sync,		/*!< in: TRUE if synchronous aio is desired */
	ulint	space_id,	/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	ulint	block_offset,	/*!< in: offset in number of blocks */
	ulint	byte_offset,	/*!< in: remainder of offset in bytes; in
				aio this must be divisible by the OS block
				size */
	ulint	len,		/*!< in: how many bytes to read or write; this
				must not cross a file boundary; in aio this
				must be a block size multiple */
	void*	buf,		/*!< in/out: buffer where to store read data
				or from where to write; in aio this must be
				appropriately aligned */
	void*	message,	/*!< in: message for aio handler if non-sync
				aio used, else ignored */
	trx_t*	trx);
/********************************************************************//**
Confirm whether the parameters are valid or not */
UNIV_INTERN
ibool
fil_is_exist(
/*==============*/
	ulint	space_id,	/*!< in: space id */
	ulint	block_offset);	/*!< in: offset in number of blocks */
/**********************************************************************//**
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.c for more info). The thread specifies which
segment it wants to wait for. */
UNIV_INTERN
void
fil_aio_wait(
/*=========*/
	ulint	segment);	/*!< in: the number of the segment in the aio
				array to wait for */
/**********************************************************************//**
Flushes to disk possible writes cached by the OS. If the space does not exist
or is being dropped, does not do anything. */
UNIV_INTERN
void
fil_flush(
/*======*/
	ulint	space_id,	/*!< in: file space id (this can be a group of
				log files or a tablespace of the database) */
	ibool	metadata);
/**********************************************************************//**
Flushes to disk writes in file spaces of the given type possibly cached by
the OS. */
UNIV_INTERN
void
fil_flush_file_spaces(
/*==================*/
	ulint	purpose);	/*!< in: FIL_TABLESPACE, FIL_LOG */
/******************************************************************//**
Checks the consistency of the tablespace cache.
@return	TRUE if ok */
UNIV_INTERN
ibool
fil_validate(void);
/*==============*/
/********************************************************************//**
Returns TRUE if file address is undefined.
@return	TRUE if undefined */
UNIV_INTERN
ibool
fil_addr_is_null(
/*=============*/
	fil_addr_t	addr);	/*!< in: address */
/********************************************************************//**
Get the predecessor of a file page.
@return	FIL_PAGE_PREV */
UNIV_INTERN
ulint
fil_page_get_prev(
/*==============*/
	const byte*	page);	/*!< in: file page */
/********************************************************************//**
Get the successor of a file page.
@return	FIL_PAGE_NEXT */
UNIV_INTERN
ulint
fil_page_get_next(
/*==============*/
	const byte*	page);	/*!< in: file page */
/*********************************************************************//**
Sets the file page type. */
UNIV_INTERN
void
fil_page_set_type(
/*==============*/
	byte*	page,	/*!< in/out: file page */
	ulint	type);	/*!< in: type */
/*********************************************************************//**
Gets the file page type.
@return type; NOTE that if the type has not been written to page, the
return value not defined */
UNIV_INTERN
ulint
fil_page_get_type(
/*==============*/
	const byte*	page);	/*!< in: file page */

/*******************************************************************//**
Returns TRUE if a single-table tablespace is being deleted.
@return TRUE if being deleted */
UNIV_INTERN
ibool
fil_tablespace_is_being_deleted(
/*============================*/
	ulint		id);	/*!< in: space id */

/*************************************************************************
Return local hash table informations. */

ulint
fil_system_hash_cells(void);
/*========================*/

ulint
fil_system_hash_nodes(void);
/*========================*/

/*************************************************************************
functions to access is_corrupt flag of fil_space_t*/

ibool
fil_space_is_corrupt(
/*=================*/
	ulint	space_id);

void
fil_space_set_corrupt(
/*==================*/
	ulint	space_id);

/****************************************************************//**
Generate redo logs for swapping two .ibd files */
UNIV_INTERN
void
fil_mtr_rename_log(
/*===============*/
	ulint		old_space_id,	/*!< in: tablespace id of the old
					table. */
	const char*	old_name,	/*!< in: old table name */
	ulint		new_space_id,	/*!< in: tablespace id of the new
					table */
	const char*	new_name,	/*!< in: new table name */
	const char*	tmp_name);	/*!< in: temp table name used while
					swapping */


typedef	struct fil_space_struct	fil_space_t;

/*******************************************************************//**
Returns the table space name for a given id, NULL if not found. */
const char*
fil_space_get_name(
/*================*/
	ulint	id);	/*!< in: space id */

#endif
