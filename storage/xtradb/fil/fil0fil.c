/*****************************************************************************

Copyright (c) 1995, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file fil/fil0fil.c
The tablespace memory cache

Created 10/25/1995 Heikki Tuuri
*******************************************************/

#include "fil0fil.h"

#include "mem0mem.h"
#include "hash0hash.h"
#include "os0file.h"
#include "mach0data.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "log0recv.h"
#include "fsp0fsp.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "dict0dict.h"
#include "page0page.h"
#include "page0zip.h"
#include "trx0trx.h"
#include "trx0sys.h"
#include "pars0pars.h"
#include "row0mysql.h"
#include "row0row.h"
#include "que0que.h"
#include "btr0btr.h"
#include "btr0sea.h"
#ifndef UNIV_HOTBACKUP
# include "buf0lru.h"
# include "ibuf0ibuf.h"
# include "sync0sync.h"
# include "os0sync.h"
#else /* !UNIV_HOTBACKUP */
static ulint srv_data_read, srv_data_written;
#endif /* !UNIV_HOTBACKUP */

/*
		IMPLEMENTATION OF THE TABLESPACE MEMORY CACHE
		=============================================

The tablespace cache is responsible for providing fast read/write access to
tablespaces and logs of the database. File creation and deletion is done
in other modules which know more of the logic of the operation, however.

A tablespace consists of a chain of files. The size of the files does not
have to be divisible by the database block size, because we may just leave
the last incomplete block unused. When a new file is appended to the
tablespace, the maximum size of the file is also specified. At the moment,
we think that it is best to extend the file to its maximum size already at
the creation of the file, because then we can avoid dynamically extending
the file when more space is needed for the tablespace.

A block's position in the tablespace is specified with a 32-bit unsigned
integer. The files in the chain are thought to be catenated, and the block
corresponding to an address n is the nth block in the catenated file (where
the first block is named the 0th block, and the incomplete block fragments
at the end of files are not taken into account). A tablespace can be extended
by appending a new file at the end of the chain.

Our tablespace concept is similar to the one of Oracle.

To acquire more speed in disk transfers, a technique called disk striping is
sometimes used. This means that logical block addresses are divided in a
round-robin fashion across several disks. Windows NT supports disk striping,
so there we do not need to support it in the database. Disk striping is
implemented in hardware in RAID disks. We conclude that it is not necessary
to implement it in the database. Oracle 7 does not support disk striping,
either.

Another trick used at some database sites is replacing tablespace files by
raw disks, that is, the whole physical disk drive, or a partition of it, is
opened as a single file, and it is accessed through byte offsets calculated
from the start of the disk or the partition. This is recommended in some
books on database tuning to achieve more speed in i/o. Using raw disk
certainly prevents the OS from fragmenting disk space, but it is not clear
if it really adds speed. We measured on the Pentium 100 MHz + NT + NTFS file
system + EIDE Conner disk only a negligible difference in speed when reading
from a file, versus reading from a raw disk.

To have fast access to a tablespace or a log file, we put the data structures
to a hash table. Each tablespace and log file is given an unique 32-bit
identifier.

Some operating systems do not support many open files at the same time,
though NT seems to tolerate at least 900 open files. Therefore, we put the
open files in an LRU-list. If we need to open another file, we may close the
file at the end of the LRU-list. When an i/o-operation is pending on a file,
the file cannot be closed. We take the file nodes with pending i/o-operations
out of the LRU-list and keep a count of pending operations. When an operation
completes, we decrement the count and return the file node to the LRU-list if
the count drops to zero. */

/** When mysqld is run, the default directory "." is the mysqld datadir,
but in the MySQL Embedded Server Library and ibbackup it is not the default
directory, and we must set the base file path explicitly */
UNIV_INTERN const char*	fil_path_to_mysql_datadir	= ".";

/** The number of fsyncs done to the log */
UNIV_INTERN ulint	fil_n_log_flushes			= 0;

/** Number of pending redo log flushes */
UNIV_INTERN ulint	fil_n_pending_log_flushes		= 0;
/** Number of pending tablespace flushes */
UNIV_INTERN ulint	fil_n_pending_tablespace_flushes	= 0;

/** The null file address */
UNIV_INTERN fil_addr_t	fil_addr_null = {FIL_NULL, 0};

/** File node of a tablespace or the log data space */
struct fil_node_struct {
	fil_space_t*	space;	/*!< backpointer to the space where this node
				belongs */
	char*		name;	/*!< path to the file */
	ibool		open;	/*!< TRUE if file open */
	os_file_t	handle;	/*!< OS handle to the file, if file open */
	ibool		is_raw_disk;/*!< TRUE if the 'file' is actually a raw
				device or a raw disk partition */
	ulint		size;	/*!< size of the file in database pages, 0 if
				not known yet; the possible last incomplete
				megabyte may be ignored if space == 0 */
	ulint		n_pending;
				/*!< count of pending i/o's on this file;
				closing of the file is not allowed if
				this is > 0 */
	ulint		n_pending_flushes;
				/*!< count of pending flushes on this file;
				closing of the file is not allowed if
				this is > 0 */
	ib_int64_t	modification_counter;/*!< when we write to the file we
				increment this by one */
	ib_int64_t	flush_counter;/*!< up to what
				modification_counter value we have
				flushed the modifications to disk */
	UT_LIST_NODE_T(fil_node_t) chain;
				/*!< link field for the file chain */
	UT_LIST_NODE_T(fil_node_t) LRU;
				/*!< link field for the LRU list */
	ulint		magic_n;/*!< FIL_NODE_MAGIC_N */
};

/** Value of fil_node_struct::magic_n */
#define	FIL_NODE_MAGIC_N	89389

/** Tablespace or log data space: let us call them by a common name space */
struct fil_space_struct {
	char*		name;	/*!< space name = the path to the first file in
				it */
	ulint		id;	/*!< space id */
	ib_int64_t	tablespace_version;
				/*!< in DISCARD/IMPORT this timestamp
				is used to check if we should ignore
				an insert buffer merge request for a
				page because it actually was for the
				previous incarnation of the space */
	ibool		mark;	/*!< this is set to TRUE at database startup if
				the space corresponds to a table in the InnoDB
				data dictionary; so we can print a warning of
				orphaned tablespaces */
	ibool		stop_ios;/*!< TRUE if we want to rename the
				.ibd file of tablespace and want to
				stop temporarily posting of new i/o
				requests on the file */
	ibool		stop_new_ops;
				/*!< we set this TRUE when we start
				deleting a single-table tablespace */
	ibool		is_being_deleted;
				/*!< this is set to TRUE when we start
				deleting a single-table tablespace and its
				file; when this flag is set no further i/o
				or flush requests can be placed on this space,
				though there may be such requests still being
				processed on this space */
	ulint		purpose;/*!< FIL_TABLESPACE, FIL_LOG, or
				FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
				/*!< base node for the file chain */
	ulint		size;	/*!< space size in pages; 0 if a single-table
				tablespace whose size we do not know yet;
				last incomplete megabytes in data files may be
				ignored if space == 0 */
	ulint		flags;	/*!< compressed page size and file format, or 0 */
	ulint		n_reserved_extents;
				/*!< number of reserved free extents for
				ongoing operations like B-tree page split */
	ulint		n_pending_flushes; /*!< this is positive when flushing
				the tablespace to disk; dropping of the
				tablespace is forbidden if this is positive */
	ulint		n_pending_ops;/*!< this is positive when we
				have pending operations against this
				tablespace. The pending operations can
				be ibuf merges or lock validation code
				trying to read a block.
				Dropping of the tablespace is forbidden
				if this is positive */
	hash_node_t	hash;	/*!< hash chain node */
	hash_node_t	name_hash;/*!< hash chain the name_hash table */
#ifndef UNIV_HOTBACKUP
	rw_lock_t	latch;	/*!< latch protecting the file space storage
				allocation */
#endif /* !UNIV_HOTBACKUP */
	UT_LIST_NODE_T(fil_space_t) unflushed_spaces;
				/*!< list of spaces with at least one unflushed
				file we have written to */
	ibool		is_in_unflushed_spaces; /*!< TRUE if this space is
				currently in unflushed_spaces */
	ibool		is_corrupt;
	UT_LIST_NODE_T(fil_space_t) space_list;
				/*!< list of all spaces */
	ulint		magic_n;/*!< FIL_SPACE_MAGIC_N */
};

/** Value of fil_space_struct::magic_n */
#define	FIL_SPACE_MAGIC_N	89472

/** The tablespace memory cache */
typedef	struct fil_system_struct	fil_system_t;

/** The tablespace memory cache; also the totality of logs (the log
data space) is stored here; below we talk about tablespaces, but also
the ib_logfiles form a 'space' and it is handled here */

struct fil_system_struct {
#ifndef UNIV_HOTBACKUP
	mutex_t		mutex;		/*!< The mutex protecting the cache */
	mutex_t		file_extend_mutex;
#endif /* !UNIV_HOTBACKUP */
	hash_table_t*	spaces;		/*!< The hash table of spaces in the
					system; they are hashed on the space
					id */
	hash_table_t*	name_hash;	/*!< hash table based on the space
					name */
	UT_LIST_BASE_NODE_T(fil_node_t) LRU;
					/*!< base node for the LRU list of the
					most recently used open files with no
					pending i/o's; if we start an i/o on
					the file, we first remove it from this
					list, and return it to the start of
					the list when the i/o ends;
					log files and the system tablespace are
					not put to this list: they are opened
					after the startup, and kept open until
					shutdown */
	UT_LIST_BASE_NODE_T(fil_space_t) unflushed_spaces;
					/*!< base node for the list of those
					tablespaces whose files contain
					unflushed writes; those spaces have
					at least one file node where
					modification_counter > flush_counter */
	ulint		n_open;		/*!< number of files currently open */
	ulint		max_n_open;	/*!< n_open is not allowed to exceed
					this */
	ib_int64_t	modification_counter;/*!< when we write to a file we
					increment this by one */
	ulint		max_assigned_id;/*!< maximum space id in the existing
					tables, or assigned during the time
					mysqld has been up; at an InnoDB
					startup we scan the data dictionary
					and set here the maximum of the
					space id's of the tables there */
	ib_int64_t	tablespace_version;
					/*!< a counter which is incremented for
					every space object memory creation;
					every space mem object gets a
					'timestamp' from this; in DISCARD/
					IMPORT this is used to check if we
					should ignore an insert buffer merge
					request */
	UT_LIST_BASE_NODE_T(fil_space_t) space_list;
					/*!< list of all file spaces */
	ibool		space_id_reuse_warned;
					/* !< TRUE if fil_space_create()
					has issued a warning about
					potential space_id reuse */
};

/** The tablespace memory cache. This variable is NULL before the module is
initialized. */
static fil_system_t*	fil_system	= NULL;


/********************************************************************//**
NOTE: you must call fil_mutex_enter_and_prepare_for_io() first!

Prepares a file node for i/o. Opens the file if it is closed. Updates the
pending i/o's field in the node and the system appropriately. Takes the node
off the LRU list if it is in the LRU list. The caller must hold the fil_sys
mutex. */
static
void
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space);	/*!< in: space */
/********************************************************************//**
Updates the data structures when an i/o operation finishes. Updates the
pending i/o's field in the node appropriately. */
static
void
fil_node_complete_io(
/*=================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	ulint		type);	/*!< in: OS_FILE_WRITE or OS_FILE_READ; marks
				the node as modified if
				type == OS_FILE_WRITE */
/*******************************************************************//**
Checks if a single-table tablespace for a given table name exists in the
tablespace memory cache.
@return	space id, ULINT_UNDEFINED if not found */
static
ulint
fil_get_space_id_for_table(
/*=======================*/
	const char*	name);	/*!< in: table name in the standard
				'databasename/tablename' format */
/*******************************************************************//**
Frees a space object from the tablespace memory cache. Closes the files in
the chain but does not delete them. There must not be any pending i/o's or
flushes on the files.
@return TRUE on success */
static
ibool
fil_space_free(
/*===========*/
	ulint		id,		/* in: space id */
	ibool		x_latched);	/* in: TRUE if caller has space->latch
					in X mode */
/********************************************************************//**
Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space.
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
UNIV_INLINE
ulint
fil_read(
/*=====*/
	ibool	sync,		/*!< in: TRUE if synchronous aio is desired */
	ulint	space_id,	/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	ulint	block_offset,	/*!< in: offset in number of blocks */
	ulint	byte_offset,	/*!< in: remainder of offset in bytes; in aio
				this must be divisible by the OS block size */
	ulint	len,		/*!< in: how many bytes to read; this must not
				cross a file boundary; in aio this must be a
				block size multiple */
	void*	buf,		/*!< in/out: buffer where to store data read;
				in aio this must be appropriately aligned */
	void*	message)	/*!< in: message for aio handler if non-sync
				aio used, else ignored */
{
	return(fil_io(OS_FILE_READ, sync, space_id, zip_size, block_offset,
					  byte_offset, len, buf, message));
}

/********************************************************************//**
Writes data to a space from a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space.
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
UNIV_INLINE
ulint
fil_write(
/*======*/
	ibool	sync,		/*!< in: TRUE if synchronous aio is desired */
	ulint	space_id,	/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	ulint	block_offset,	/*!< in: offset in number of blocks */
	ulint	byte_offset,	/*!< in: remainder of offset in bytes; in aio
				this must be divisible by the OS block size */
	ulint	len,		/*!< in: how many bytes to write; this must
				not cross a file boundary; in aio this must
				be a block size multiple */
	void*	buf,		/*!< in: buffer from which to write; in aio
				this must be appropriately aligned */
	void*	message)	/*!< in: message for aio handler if non-sync
				aio used, else ignored */
{
	return(fil_io(OS_FILE_WRITE, sync, space_id, zip_size, block_offset,
					   byte_offset, len, buf, message));
}

/*******************************************************************//**
Returns the table space by a given id, NULL if not found. */
UNIV_INLINE
fil_space_t*
fil_space_get_by_id(
/*================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;

	ut_ad(mutex_own(&fil_system->mutex));

	HASH_SEARCH(hash, fil_system->spaces, id,
		    fil_space_t*, space,
		    ut_ad(space->magic_n == FIL_SPACE_MAGIC_N),
		    space->id == id);

	return(space);
}

/*******************************************************************//**
Returns the table space by a given name, NULL if not found. */
UNIV_INLINE
fil_space_t*
fil_space_get_by_name(
/*==================*/
	const char*	name)	/*!< in: space name */
{
	fil_space_t*	space;
	ulint		fold;

	ut_ad(mutex_own(&fil_system->mutex));

	fold = ut_fold_string(name);

	HASH_SEARCH(name_hash, fil_system->name_hash, fold,
		    fil_space_t*, space,
		    ut_ad(space->magic_n == FIL_SPACE_MAGIC_N),
		    !strcmp(name, space->name));

	return(space);
}

#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Returns the version number of a tablespace, -1 if not found.
@return version number, -1 if the tablespace does not exist in the
memory cache */
UNIV_INTERN
ib_int64_t
fil_space_get_version(
/*==================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;
	ib_int64_t	version		= -1;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space) {
		version = space->tablespace_version;
	}

	mutex_exit(&fil_system->mutex);

	return(version);
}

/*******************************************************************//**
Returns the latch of a file space.
@return	latch protecting storage allocation */
UNIV_INTERN
rw_lock_t*
fil_space_get_latch(
/*================*/
	ulint	id,	/*!< in: space id */
	ulint*	flags)	/*!< out: tablespace flags */
{
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	if (flags) {
		*flags = space->flags;
	}

	mutex_exit(&fil_system->mutex);

	return(&(space->latch));
}

/*******************************************************************//**
Returns the type of a file space.
@return	FIL_TABLESPACE or FIL_LOG */
UNIV_INTERN
ulint
fil_space_get_type(
/*===============*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	mutex_exit(&fil_system->mutex);

	return(space->purpose);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Checks if all the file nodes in a space are flushed. The caller must hold
the fil_system mutex.
@return	TRUE if all are flushed */
static
ibool
fil_space_is_flushed(
/*=================*/
	fil_space_t*	space)	/*!< in: space */
{
	fil_node_t*	node;

	ut_ad(mutex_own(&fil_system->mutex));

	node = UT_LIST_GET_FIRST(space->chain);

	while (node) {
		if (node->modification_counter > node->flush_counter) {

			return(FALSE);
		}

		node = UT_LIST_GET_NEXT(chain, node);
	}

	return(TRUE);
}

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
	ibool		is_raw)	/*!< in: TRUE if a raw device or
				a raw disk partition */
{
	fil_node_t*	node;
	fil_space_t*	space;

	ut_a(fil_system);
	ut_a(name);

	mutex_enter(&fil_system->mutex);

	node = mem_alloc(sizeof(fil_node_t));

	node->name = mem_strdup(name);
	node->open = FALSE;

	ut_a(!is_raw || srv_start_raw_disk_in_use);

	node->is_raw_disk = is_raw;
	node->size = size;
	node->magic_n = FIL_NODE_MAGIC_N;
	node->n_pending = 0;
	node->n_pending_flushes = 0;

	node->modification_counter = 0;
	node->flush_counter = 0;

	space = fil_space_get_by_id(id);

	if (!space) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: Could not find tablespace %lu for\n"
			"InnoDB: file ", (ulong) id);
		ut_print_filename(stderr, name);
		fputs(" in the tablespace memory cache.\n", stderr);
		mem_free(node->name);

		mem_free(node);

		mutex_exit(&fil_system->mutex);

		return;
	}

	space->size += size;

	node->space = space;

	UT_LIST_ADD_LAST(chain, space->chain, node);

	if (id < SRV_EXTRA_SYS_SPACE_FIRST_ID && fil_system->max_assigned_id < id) {

		fil_system->max_assigned_id = id;
	}

	mutex_exit(&fil_system->mutex);
}

/********************************************************************//**
Opens a the file of a node of a tablespace. The caller must own the fil_system
mutex. */
static
void
fil_node_open_file(
/*===============*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space)	/*!< in: space */
{
	ib_uint64_t	size_bytes;
	ulint		size_low;
	ulint		size_high;
	ibool		ret;
	ibool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ulint		flags;

	ut_ad(mutex_own(&(system->mutex)));
	ut_a(node->n_pending == 0);
	ut_a(node->open == FALSE);

	if (node->size == 0) {
		/* It must be a single-table tablespace and we do not know the
		size of the file yet. First we open the file in the normal
		mode, no async I/O here, for simplicity. Then do some checks,
		and close the file again.
		NOTE that we could not use the simple file read function
		os_file_read() in Windows to read from a file opened for
		async I/O! */

		node->handle = os_file_create_simple_no_error_handling(
			node->name, OS_FILE_OPEN, OS_FILE_READ_ONLY, &success);
		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(TRUE);

			ut_print_timestamp(stderr);

			fprintf(stderr,
				"  InnoDB: Fatal error: cannot open %s\n."
				"InnoDB: Have you deleted .ibd files"
				" under a running mysqld server?\n",
				node->name);
			ut_a(0);
		}

		os_file_get_size(node->handle, &size_low, &size_high);

		size_bytes = (((ib_uint64_t)size_high) << 32)
			+ (ib_uint64_t)size_low;
#ifdef UNIV_HOTBACKUP
		if (trx_sys_sys_space(space->id)) {
			node->size = (ulint) (size_bytes / UNIV_PAGE_SIZE);
			os_file_close(node->handle);
			goto add_size;
		}
#endif /* UNIV_HOTBACKUP */
		ut_a(space->purpose != FIL_LOG);
		ut_a(!trx_sys_sys_space(space->id));

		if (size_bytes < (ib_uint64_t) (FIL_IBD_FILE_INITIAL_SIZE * (lint)UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Error: the size of single-table"
				" tablespace file %s\n"
				"InnoDB: is only %lu %lu,"
				" should be at least %lu!\n",
				node->name,
				(ulong) size_high,
				(ulong) size_low,
				(ulong) (FIL_IBD_FILE_INITIAL_SIZE
					 * UNIV_PAGE_SIZE));

			ut_a(0);
		}

		/* Read the first page of the tablespace */

		buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
		/* Align the memory for file i/o if we might have O_DIRECT
		set */
		page = ut_align(buf2, UNIV_PAGE_SIZE);

		success = os_file_read(node->handle, page, 0, 0,
				       UNIV_PAGE_SIZE);
		space_id = fsp_header_get_space_id(page);
		flags = fsp_header_get_flags(page);

		ut_free(buf2);

		/* Close the file now that we have read the space id from it */

		os_file_close(node->handle);

		if (UNIV_UNLIKELY(space_id != space->id)) {
			fprintf(stderr,
				"InnoDB: Error: tablespace id is %lu"
				" in the data dictionary\n"
				"InnoDB: but in file %s it is %lu!\n",
				space->id, node->name, space_id);

			ut_error;
		}

		if (UNIV_UNLIKELY(space_id == ULINT_UNDEFINED
				  || trx_sys_sys_space(space_id))) {
			fprintf(stderr,
				"InnoDB: Error: tablespace id %lu"
				" in file %s is not sensible\n",
				(ulong) space_id, node->name);

			ut_error;
		}

		if (UNIV_UNLIKELY(space->flags != flags)) {
			fprintf(stderr,
				"InnoDB: Error: table flags are %lx"
				" in the data dictionary\n"
				"InnoDB: but the flags in file %s are %lx!\n",
				space->flags, node->name, flags);

			ut_error;
		}

		if (size_bytes >= 1024 * 1024) {
			/* Truncate the size to whole megabytes. */
			size_bytes = ut_2pow_round(size_bytes, 1024 * 1024);
		}

		if (!(flags & DICT_TF_ZSSIZE_MASK)) {
			node->size = (ulint) (size_bytes / UNIV_PAGE_SIZE);
		} else {
			node->size = (ulint)
				(size_bytes
				 / dict_table_flags_to_zip_size(flags));
		}

#ifdef UNIV_HOTBACKUP
add_size:
#endif /* UNIV_HOTBACKUP */
		space->size += node->size;
	}

	/* printf("Opening file %s\n", node->name); */

	/* Open the file for reading and writing, in Windows normally in the
	unbuffered async I/O mode, though global variables may make
	os_file_create() to fall back to the normal file I/O mode. */

	if (space->purpose == FIL_LOG) {
		node->handle = os_file_create(node->name, OS_FILE_OPEN,
					      OS_FILE_AIO, OS_LOG_FILE, &ret);
	} else if (node->is_raw_disk) {
		node->handle = os_file_create(node->name,
					      OS_FILE_OPEN_RAW,
					      OS_FILE_AIO, OS_DATA_FILE, &ret);
	} else {
		node->handle = os_file_create(node->name, OS_FILE_OPEN,
					      OS_FILE_AIO, OS_DATA_FILE, &ret);
	}

	ut_a(ret);

	node->open = TRUE;

	system->n_open++;

	if (space->purpose == FIL_TABLESPACE && !trx_sys_sys_space(space->id)) {
		/* Put the node to the LRU list */
		UT_LIST_ADD_FIRST(LRU, system->LRU, node);
	}
}

/**********************************************************************//**
Closes a file. */
static
void
fil_node_close_file(
/*================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system)	/*!< in: tablespace memory cache */
{
	ibool	ret;

	ut_ad(node && system);
	ut_ad(mutex_own(&(system->mutex)));
	ut_a(node->open);
	ut_a(node->n_pending == 0 || node->space->is_being_deleted);
	ut_a(node->n_pending_flushes == 0);
	ut_a(node->modification_counter == node->flush_counter);

	ret = os_file_close(node->handle);
	ut_a(ret);

	/* printf("Closing file %s\n", node->name); */

	node->open = FALSE;
	ut_a(system->n_open > 0);
	system->n_open--;

	if (node->n_pending == 0 && node->space->purpose == FIL_TABLESPACE && !trx_sys_sys_space(node->space->id)) {
		ut_a(UT_LIST_GET_LEN(system->LRU) > 0);

		/* The node is in the LRU list, remove it */
		UT_LIST_REMOVE(LRU, system->LRU, node);
	}
}

/********************************************************************//**
Tries to close a file in the LRU list. The caller must hold the fil_sys
mutex.
@return TRUE if success, FALSE if should retry later; since i/o's
generally complete in < 100 ms, and as InnoDB writes at most 128 pages
from the buffer pool in a batch, and then immediately flushes the
files, there is a good chance that the next time we find a suitable
node from the LRU list */
static
ibool
fil_try_to_close_file_in_LRU(
/*=========================*/
	ibool	print_info)	/*!< in: if TRUE, prints information why it
				cannot close a file */
{
	fil_node_t*	node;

	ut_ad(mutex_own(&fil_system->mutex));

	node = UT_LIST_GET_LAST(fil_system->LRU);

	if (print_info) {
		fprintf(stderr,
			"InnoDB: fil_sys open file LRU len %lu\n",
			(ulong) UT_LIST_GET_LEN(fil_system->LRU));
	}

	while (node != NULL) {
		if (node->modification_counter == node->flush_counter
		    && node->n_pending_flushes == 0) {

			fil_node_close_file(node, fil_system);

			return(TRUE);
		}

		if (print_info && node->n_pending_flushes > 0) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr, ", because n_pending_flushes %lu\n",
				(ulong) node->n_pending_flushes);
		}

		if (print_info
		    && node->modification_counter != node->flush_counter) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr,
				", because mod_count %ld != fl_count %ld\n",
				(long) node->modification_counter,
				(long) node->flush_counter);
		}

		node = UT_LIST_GET_PREV(LRU, node);
	}

	return(FALSE);
}

/*******************************************************************//**
Reserves the fil_system mutex and tries to make sure we can open at least one
file while holding it. This should be called before calling
fil_node_prepare_for_io(), because that function may need to open a file. */
static
void
fil_mutex_enter_and_prepare_for_io(
/*===============================*/
	ulint	space_id)	/*!< in: space id */
{
	fil_space_t*	space;
	ibool		success;
	ibool		print_info	= FALSE;
	ulint		count		= 0;
	ulint		count2		= 0;

retry:
	mutex_enter(&fil_system->mutex);

	if (trx_sys_sys_space(space_id) || space_id >= SRV_LOG_SPACE_FIRST_ID) {
		/* We keep log files and system tablespace files always open;
		this is important in preventing deadlocks in this module, as
		a page read completion often performs another read from the
		insert buffer. The insert buffer is in tablespace 0, and we
		cannot end up waiting in this function. */

		return;
	}

	space = fil_space_get_by_id(space_id);

	if (space != NULL && space->stop_ios) {
		/* We are going to do a rename file and want to stop new i/o's
		for a while */

		if (count2 > 20000) {
			fputs("InnoDB: Warning: tablespace ", stderr);
			ut_print_filename(stderr, space->name);
			fprintf(stderr,
				" has i/o ops stopped for a long time %lu\n",
				(ulong) count2);
		}

		mutex_exit(&fil_system->mutex);

#ifndef UNIV_HOTBACKUP

		/* Wake the i/o-handler threads to make sure pending
		i/o's are performed */
		os_aio_simulated_wake_handler_threads();

		/* The sleep here is just to give IO helper threads a
		bit of time to do some work. It is not required that
		all IO related to the tablespace being renamed must
		be flushed here as we do fil_flush() in
		fil_rename_tablespace() as well. */
		os_thread_sleep(20000);

#endif /* UNIV_HOTBACKUP */

		/* Flush tablespaces so that we can close modified
		files in the LRU list */
		fil_flush_file_spaces(FIL_TABLESPACE);

		os_thread_sleep(20000);

		count2++;

		goto retry;
	}

	if (fil_system->n_open < fil_system->max_n_open) {

		return;
	}

	/* If the file is already open, no need to do anything; if the space
	does not exist, we handle the situation in the function which called
	this function */

	if (!space || UT_LIST_GET_FIRST(space->chain)->open) {

		return;
	}

	if (count > 1) {
		print_info = TRUE;
	}

	/* Too many files are open, try to close some */
close_more:
	success = fil_try_to_close_file_in_LRU(print_info);

	if (success && fil_system->n_open >= fil_system->max_n_open) {

		goto close_more;
	}

	if (fil_system->n_open < fil_system->max_n_open) {
		/* Ok */

		return;
	}

	if (count >= 2) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Warning: too many (%lu) files stay open"
			" while the maximum\n"
			"InnoDB: allowed value would be %lu.\n"
			"InnoDB: You may need to raise the value of"
			" innodb_open_files in\n"
			"InnoDB: my.cnf.\n",
			(ulong) fil_system->n_open,
			(ulong) fil_system->max_n_open);

		return;
	}

	mutex_exit(&fil_system->mutex);

#ifndef UNIV_HOTBACKUP
	/* Wake the i/o-handler threads to make sure pending i/o's are
	performed */
	os_aio_simulated_wake_handler_threads();

	os_thread_sleep(20000);
#endif
	/* Flush tablespaces so that we can close modified files in the LRU
	list */

	fil_flush_file_spaces(FIL_TABLESPACE);

	count++;

	goto retry;
}

/*******************************************************************//**
Frees a file node object from a tablespace memory cache. */
static
void
fil_node_free(
/*==========*/
	fil_node_t*	node,	/*!< in, own: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space)	/*!< in: space where the file node is chained */
{
	ut_ad(node && system && space);
	ut_ad(mutex_own(&(system->mutex)));
	ut_a(node->magic_n == FIL_NODE_MAGIC_N);
	ut_a(node->n_pending == 0 || space->is_being_deleted);

	if (node->open) {
		/* We fool the assertion in fil_node_close_file() to think
		there are no unflushed modifications in the file */

		node->modification_counter = node->flush_counter;

		if (space->is_in_unflushed_spaces
		    && fil_space_is_flushed(space)) {

			space->is_in_unflushed_spaces = FALSE;

			UT_LIST_REMOVE(unflushed_spaces,
				       system->unflushed_spaces,
				       space);
		}

		fil_node_close_file(node, system);
	}

	space->size -= node->size;

	UT_LIST_REMOVE(chain, space->chain, node);

	mem_free(node->name);
	mem_free(node);
}

#ifdef UNIV_LOG_ARCHIVE
/****************************************************************//**
Drops files from the start of a file space, so that its size is cut by
the amount given. */
UNIV_INTERN
void
fil_space_truncate_start(
/*=====================*/
	ulint	id,		/*!< in: space id */
	ulint	trunc_len)	/*!< in: truncate by this much; it is an error
				if this does not equal to the combined size of
				some initial files in the space */
{
	fil_node_t*	node;
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	while (trunc_len > 0) {
		node = UT_LIST_GET_FIRST(space->chain);

		ut_a(node->size * UNIV_PAGE_SIZE <= trunc_len);

		trunc_len -= node->size * UNIV_PAGE_SIZE;

		fil_node_free(node, fil_system, space);
	}

	mutex_exit(&fil_system->mutex);
}
#endif /* UNIV_LOG_ARCHIVE */

/*******************************************************************//**
Creates a space memory object and puts it to the tablespace memory cache. If
there is an error, prints an error message to the .err log.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_space_create(
/*=============*/
	const char*	name,	/*!< in: space name */
	ulint		id,	/*!< in: space id */
	ulint		flags,	/*!< in: compressed page size
				and file format, or 0 */
	ulint		purpose)/*!< in: FIL_TABLESPACE, or FIL_LOG if log */
{
	fil_space_t*	space;

	/* The tablespace flags (FSP_SPACE_FLAGS) should be 0 for
	ROW_FORMAT=COMPACT
	((table->flags & ~(~0 << DICT_TF_BITS)) == DICT_TF_COMPACT) and
	ROW_FORMAT=REDUNDANT (table->flags == 0).  For any other
	format, the tablespace flags should equal
	(table->flags & ~(~0 << DICT_TF_BITS)). */
	ut_a(flags != DICT_TF_COMPACT);
	ut_a(!(flags & (~0UL << DICT_TF_BITS)));

try_again:
	/*printf(
	"InnoDB: Adding tablespace %lu of name %s, purpose %lu\n", id, name,
	purpose);*/

	ut_a(fil_system);
	ut_a(name);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_name(name);

	if (UNIV_LIKELY_NULL(space)) {
		ibool	success;
		ulint	namesake_id;

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Warning: trying to init to the"
			" tablespace memory cache\n"
			"InnoDB: a tablespace %lu of name ", (ulong) id);
		ut_print_filename(stderr, name);
		fprintf(stderr, ",\n"
			"InnoDB: but a tablespace %lu of the same name\n"
			"InnoDB: already exists in the"
			" tablespace memory cache!\n",
			(ulong) space->id);

		if (trx_sys_sys_space(id) || purpose != FIL_TABLESPACE) {

			mutex_exit(&fil_system->mutex);

			return(FALSE);
		}

		fprintf(stderr,
			"InnoDB: We assume that InnoDB did a crash recovery,"
			" and you had\n"
			"InnoDB: an .ibd file for which the table"
			" did not exist in the\n"
			"InnoDB: InnoDB internal data dictionary in the"
			" ibdata files.\n"
			"InnoDB: We assume that you later removed the"
			" .ibd and .frm files,\n"
			"InnoDB: and are now trying to recreate the table."
			" We now remove the\n"
			"InnoDB: conflicting tablespace object"
			" from the memory cache and try\n"
			"InnoDB: the init again.\n");

		namesake_id = space->id;

		success = fil_space_free(namesake_id, FALSE);
		ut_a(success);

		mutex_exit(&fil_system->mutex);

		goto try_again;
	}

	space = fil_space_get_by_id(id);

	if (UNIV_LIKELY_NULL(space)) {
		fprintf(stderr,
			"InnoDB: Error: trying to add tablespace %lu"
			" of name ", (ulong) id);
		ut_print_filename(stderr, name);
		fprintf(stderr, "\n"
			"InnoDB: to the tablespace memory cache,"
			" but tablespace\n"
			"InnoDB: %lu of name ", (ulong) space->id);
		ut_print_filename(stderr, space->name);
		fputs(" already exists in the tablespace\n"
		      "InnoDB: memory cache!\n", stderr);

		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	space = mem_alloc(sizeof(fil_space_t));

	space->name = mem_strdup(name);
	space->id = id;

	fil_system->tablespace_version++;
	space->tablespace_version = fil_system->tablespace_version;
	space->mark = FALSE;

	if (UNIV_LIKELY(purpose == FIL_TABLESPACE && !recv_recovery_on)
	    && UNIV_UNLIKELY(id < SRV_EXTRA_SYS_SPACE_FIRST_ID)
	    && UNIV_UNLIKELY(id > fil_system->max_assigned_id)) {
		if (!fil_system->space_id_reuse_warned) {
			fil_system->space_id_reuse_warned = TRUE;

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Warning: allocated tablespace %lu,"
				" old maximum was %lu\n",
				(ulong) id,
				(ulong) fil_system->max_assigned_id);
		}

		fil_system->max_assigned_id = id;
	}

	space->stop_ios = FALSE;
	space->stop_new_ops = FALSE;
	space->is_being_deleted = FALSE;
	space->purpose = purpose;
	space->size = 0;
	space->flags = flags;

	space->n_reserved_extents = 0;

	space->n_pending_flushes = 0;
	space->n_pending_ops = 0;

	UT_LIST_INIT(space->chain);
	space->magic_n = FIL_SPACE_MAGIC_N;

	rw_lock_create(&space->latch, SYNC_FSP);

	HASH_INSERT(fil_space_t, hash, fil_system->spaces, id, space);

	HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(name), space);
	space->is_in_unflushed_spaces = FALSE;

	space->is_corrupt = FALSE;

	UT_LIST_ADD_LAST(space_list, fil_system->space_list, space);

	mutex_exit(&fil_system->mutex);

	return(TRUE);
}

/*******************************************************************//**
Assigns a new space id for a new single-table tablespace. This works simply by
incrementing the global counter. If 4 billion id's is not enough, we may need
to recycle id's.
@return	TRUE if assigned, FALSE if not */
UNIV_INTERN
ibool
fil_assign_new_space_id(
/*====================*/
	ulint*	space_id)	/*!< in/out: space id */
{
	ulint	id;
	ibool	success;

	mutex_enter(&fil_system->mutex);

	id = *space_id;

	if (id < fil_system->max_assigned_id) {
		id = fil_system->max_assigned_id;
	}

	id++;

	if (id > (SRV_LOG_SPACE_FIRST_ID / 2) && (id % 1000000UL == 0)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"InnoDB: Warning: you are running out of new"
			" single-table tablespace id's.\n"
			"InnoDB: Current counter is %lu and it"
			" must not exceed %lu!\n"
			"InnoDB: To reset the counter to zero"
			" you have to dump all your tables and\n"
			"InnoDB: recreate the whole InnoDB installation.\n",
			(ulong) id,
			(ulong) SRV_LOG_SPACE_FIRST_ID);
	}

	success = (id < SRV_EXTRA_SYS_SPACE_FIRST_ID);

	if (success) {
		*space_id = fil_system->max_assigned_id = id;
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"InnoDB: You have run out of single-table"
			" tablespace id's!\n"
			"InnoDB: Current counter is %lu.\n"
			"InnoDB: To reset the counter to zero you"
			" have to dump all your tables and\n"
			"InnoDB: recreate the whole InnoDB installation.\n",
			(ulong) id);
		*space_id = ULINT_UNDEFINED;
	}

	mutex_exit(&fil_system->mutex);

	return(success);
}

/*******************************************************************//**
Frees a space object from the tablespace memory cache. Closes the files in
the chain but does not delete them. There must not be any pending i/o's or
flushes on the files.
@return	TRUE if success */
static
ibool
fil_space_free(
/*===========*/
					/* out: TRUE if success */
	ulint		id,		/* in: space id */
	ibool		x_latched)	/* in: TRUE if caller has space->latch
					in X mode */
{
	fil_space_t*	space;
	fil_space_t*	namespace;
	fil_node_t*	fil_node;

	ut_ad(mutex_own(&fil_system->mutex));

	space = fil_space_get_by_id(id);

	if (!space) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: trying to remove tablespace %lu"
			" from the cache but\n"
			"InnoDB: it is not there.\n", (ulong) id);

		return(FALSE);
	}

	HASH_DELETE(fil_space_t, hash, fil_system->spaces, id, space);

	namespace = fil_space_get_by_name(space->name);
	ut_a(namespace);
	ut_a(space == namespace);

	HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(space->name), space);

	if (space->is_in_unflushed_spaces) {
		space->is_in_unflushed_spaces = FALSE;

		UT_LIST_REMOVE(unflushed_spaces, fil_system->unflushed_spaces,
			       space);
	}

	UT_LIST_REMOVE(space_list, fil_system->space_list, space);

	ut_a(space->magic_n == FIL_SPACE_MAGIC_N);
	ut_a(0 == space->n_pending_flushes);

	fil_node = UT_LIST_GET_FIRST(space->chain);

	while (fil_node != NULL) {
		fil_node_free(fil_node, fil_system, space);

		fil_node = UT_LIST_GET_FIRST(space->chain);
	}

	ut_a(0 == UT_LIST_GET_LEN(space->chain));

	if (x_latched) {
		rw_lock_x_unlock(&space->latch);
	}

	rw_lock_free(&(space->latch));

	mem_free(space->name);
	mem_free(space);

	return(TRUE);
}

/*******************************************************************//**
Returns the size of the space in pages. The tablespace must be cached in the
memory cache.
@return	space size, 0 if space not found */
UNIV_INTERN
ulint
fil_space_get_size(
/*===============*/
	ulint	id)	/*!< in: space id */
{
	fil_node_t*	node;
	fil_space_t*	space;
	ulint		size;

	ut_ad(fil_system);

	fil_mutex_enter_and_prepare_for_io(id);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		mutex_exit(&fil_system->mutex);

		return(0);
	}

	if (space->size == 0 && space->purpose == FIL_TABLESPACE) {
		ut_a(id != 0);

		ut_a(1 == UT_LIST_GET_LEN(space->chain));

		node = UT_LIST_GET_FIRST(space->chain);

		/* It must be a single-table tablespace and we have not opened
		the file yet; the following calls will open it and update the
		size fields */

		fil_node_prepare_for_io(node, fil_system, space);
		fil_node_complete_io(node, fil_system, OS_FILE_READ);
	}

	size = space->size;

	mutex_exit(&fil_system->mutex);

	return(size);
}

/*******************************************************************//**
Returns the flags of the space. The tablespace must be cached
in the memory cache.
@return	flags, ULINT_UNDEFINED if space not found */
UNIV_INTERN
ulint
fil_space_get_flags(
/*================*/
	ulint	id)	/*!< in: space id */
{
	fil_node_t*	node;
	fil_space_t*	space;
	ulint		flags;

	ut_ad(fil_system);

	if (UNIV_UNLIKELY(!id)) {
		return(0);
	}

	fil_mutex_enter_and_prepare_for_io(id);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		mutex_exit(&fil_system->mutex);

		return(ULINT_UNDEFINED);
	}

	if (space->size == 0 && space->purpose == FIL_TABLESPACE) {
		ut_a(id != 0);

		ut_a(1 == UT_LIST_GET_LEN(space->chain));

		node = UT_LIST_GET_FIRST(space->chain);

		/* It must be a single-table tablespace and we have not opened
		the file yet; the following calls will open it and update the
		size fields */

		fil_node_prepare_for_io(node, fil_system, space);
		fil_node_complete_io(node, fil_system, OS_FILE_READ);
	}

	flags = space->flags;

	mutex_exit(&fil_system->mutex);

	return(flags);
}

/*******************************************************************//**
Returns the compressed page size of the space, or 0 if the space
is not compressed. The tablespace must be cached in the memory cache.
@return	compressed page size, ULINT_UNDEFINED if space not found */
UNIV_INTERN
ulint
fil_space_get_zip_size(
/*===================*/
	ulint	id)	/*!< in: space id */
{
	ulint	flags;

	flags = fil_space_get_flags(id);

	if (flags && flags != ULINT_UNDEFINED) {

		return(dict_table_flags_to_zip_size(flags));
	}

	return(flags);
}

/*******************************************************************//**
Checks if the pair space, page_no refers to an existing page in a tablespace
file space. The tablespace must be cached in the memory cache.
@return	TRUE if the address is meaningful */
UNIV_INTERN
ibool
fil_check_adress_in_tablespace(
/*===========================*/
	ulint	id,	/*!< in: space id */
	ulint	page_no)/*!< in: page number */
{
	if (fil_space_get_size(id) > page_no) {

		return(TRUE);
	}

	return(FALSE);
}

/****************************************************************//**
Initializes the tablespace memory cache. */
UNIV_INTERN
void
fil_init(
/*=====*/
	ulint	hash_size,	/*!< in: hash table size */
	ulint	max_n_open)	/*!< in: max number of open files */
{
	ut_a(fil_system == NULL);

	ut_a(hash_size > 0);
	ut_a(max_n_open > 0);

	fil_system = mem_zalloc(sizeof(fil_system_t));

	mutex_create(&fil_system->mutex, SYNC_ANY_LATCH);
	mutex_create(&fil_system->file_extend_mutex, SYNC_OUTER_ANY_LATCH);

	fil_system->spaces = hash_create(hash_size);
	fil_system->name_hash = hash_create(hash_size);

	UT_LIST_INIT(fil_system->LRU);

	fil_system->max_n_open = max_n_open;

	fil_system->max_assigned_id = TRX_SYS_SPACE_MAX;
}

/*******************************************************************//**
Opens all log files and system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup after the
space objects for the log and the system tablespace have been created. The
purpose of this operation is to make sure we never run out of file descriptors
if we need to read from the insert buffer or to write to the log. */
UNIV_INTERN
void
fil_open_log_and_system_tablespace_files(void)
/*==========================================*/
{
	fil_space_t*	space;
	fil_node_t*	node;

	mutex_enter(&fil_system->mutex);

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	while (space != NULL) {
		if (space->purpose != FIL_TABLESPACE || trx_sys_sys_space(space->id)) {
			node = UT_LIST_GET_FIRST(space->chain);

			while (node != NULL) {
				if (!node->open) {
					fil_node_open_file(node, fil_system,
							   space);
				}
				if (fil_system->max_n_open
				    < 10 + fil_system->n_open) {
					fprintf(stderr,
						"InnoDB: Warning: you must"
						" raise the value of"
						" innodb_open_files in\n"
						"InnoDB: my.cnf! Remember that"
						" InnoDB keeps all log files"
						" and all system\n"
						"InnoDB: tablespace files open"
						" for the whole time mysqld is"
						" running, and\n"
						"InnoDB: needs to open also"
						" some .ibd files if the"
						" file-per-table storage\n"
						"InnoDB: model is used."
						" Current open files %lu,"
						" max allowed"
						" open files %lu.\n",
						(ulong) fil_system->n_open,
						(ulong) fil_system->max_n_open);
				}
				node = UT_LIST_GET_NEXT(chain, node);
			}
		}
		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&fil_system->mutex);
}

/*******************************************************************//**
Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
UNIV_INTERN
void
fil_close_all_files(void)
/*=====================*/
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	while (space != NULL) {
		fil_node_t*	node;
		fil_space_t*	prev_space = space;

		for (node = UT_LIST_GET_FIRST(space->chain);
		     node != NULL;
		     node = UT_LIST_GET_NEXT(chain, node)) {

			if (node->open) {
				fil_node_close_file(node, fil_system);
			}
		}

		space = UT_LIST_GET_NEXT(space_list, space);

		fil_space_free(prev_space->id, FALSE);
	}

	mutex_exit(&fil_system->mutex);
}

/*******************************************************************//**
Sets the max tablespace id counter if the given number is bigger than the
previous value. */
UNIV_INTERN
void
fil_set_max_space_id_if_bigger(
/*===========================*/
	ulint	max_id)	/*!< in: maximum known id */
{
	if (max_id >= SRV_LOG_SPACE_FIRST_ID) {
		fprintf(stderr,
			"InnoDB: Fatal error: max tablespace id"
			" is too high, %lu\n", (ulong) max_id);
		ut_error;
	}

	if (max_id >= SRV_EXTRA_SYS_SPACE_FIRST_ID) {
		return;
	}

	mutex_enter(&fil_system->mutex);

	if (fil_system->max_assigned_id < max_id) {

		fil_system->max_assigned_id = max_id;
	}

	mutex_exit(&fil_system->mutex);
}

/****************************************************************//**
Writes the flushed lsn and the latest archived log number to the page header
of the first page of a data file of the system tablespace (space 0),
which is uncompressed. */
static
ulint
fil_write_lsn_and_arch_no_to_file(
/*==============================*/
	ulint		space_id,
	ulint		sum_of_sizes,	/*!< in: combined size of previous files
					in space, in database pages */
	ib_uint64_t	lsn,		/*!< in: lsn to write */
	ulint		arch_log_no __attribute__((unused)))
					/*!< in: archived log number to write */
{
	byte*	buf1;
	byte*	buf;

	ut_a(trx_sys_sys_space(space_id));

	buf1 = mem_alloc(2 * UNIV_PAGE_SIZE);
	buf = ut_align(buf1, UNIV_PAGE_SIZE);

	fil_read(TRUE, space_id, 0, sum_of_sizes, 0, UNIV_PAGE_SIZE, buf, NULL);

	mach_write_ull(buf + FIL_PAGE_FILE_FLUSH_LSN, lsn);

	fil_write(TRUE, space_id, 0, sum_of_sizes, 0, UNIV_PAGE_SIZE, buf, NULL);

	mem_free(buf1);

	return(DB_SUCCESS);
}

/****************************************************************//**
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file in the system tablespace.
@return	DB_SUCCESS or error number */
UNIV_INTERN
ulint
fil_write_flushed_lsn_to_data_files(
/*================================*/
	ib_uint64_t	lsn,		/*!< in: lsn to write */
	ulint		arch_log_no)	/*!< in: latest archived log
					file number */
{
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		sum_of_sizes;
	ulint		err;

	mutex_enter(&fil_system->mutex);

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	while (space) {
		/* We only write the lsn to all existing data files which have
		been open during the lifetime of the mysqld process; they are
		represented by the space objects in the tablespace memory
		cache. Note that all data files in the system tablespace 0 are
		always open. */

		if (space->purpose == FIL_TABLESPACE
		    && trx_sys_sys_space(space->id)) {
			sum_of_sizes = 0;

			node = UT_LIST_GET_FIRST(space->chain);
			while (node) {
				mutex_exit(&fil_system->mutex);

				err = fil_write_lsn_and_arch_no_to_file(
					space->id, sum_of_sizes, lsn, arch_log_no);
				if (err != DB_SUCCESS) {

					return(err);
				}

				mutex_enter(&fil_system->mutex);

				sum_of_sizes += node->size;
				node = UT_LIST_GET_NEXT(chain, node);
			}
		}
		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&fil_system->mutex);

	return(DB_SUCCESS);
}

/*******************************************************************//**
Reads the flushed lsn and arch no fields from a data file at database
startup. */
UNIV_INTERN
void
fil_read_flushed_lsn_and_arch_log_no(
/*=================================*/
	os_file_t	data_file,		/*!< in: open data file */
	ibool		one_read_already,	/*!< in: TRUE if min and max
						parameters below already
						contain sensible data */
#ifdef UNIV_LOG_ARCHIVE
	ulint*		min_arch_log_no,	/*!< in/out: */
	ulint*		max_arch_log_no,	/*!< in/out: */
#endif /* UNIV_LOG_ARCHIVE */
	ib_uint64_t*	min_flushed_lsn,	/*!< in/out: */
	ib_uint64_t*	max_flushed_lsn)	/*!< in/out: */
{
	byte*		buf;
	byte*		buf2;
	ib_uint64_t	flushed_lsn;

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for a possible read from a raw device */
	buf = ut_align(buf2, UNIV_PAGE_SIZE);

	os_file_read(data_file, buf, 0, 0, UNIV_PAGE_SIZE);

	flushed_lsn = mach_read_ull(buf + FIL_PAGE_FILE_FLUSH_LSN);

	ut_free(buf2);

	if (!one_read_already) {
		*min_flushed_lsn = flushed_lsn;
		*max_flushed_lsn = flushed_lsn;
#ifdef UNIV_LOG_ARCHIVE
		*min_arch_log_no = arch_log_no;
		*max_arch_log_no = arch_log_no;
#endif /* UNIV_LOG_ARCHIVE */
		return;
	}

	if (*min_flushed_lsn > flushed_lsn) {
		*min_flushed_lsn = flushed_lsn;
	}
	if (*max_flushed_lsn < flushed_lsn) {
		*max_flushed_lsn = flushed_lsn;
	}
#ifdef UNIV_LOG_ARCHIVE
	if (*min_arch_log_no > arch_log_no) {
		*min_arch_log_no = arch_log_no;
	}
	if (*max_arch_log_no < arch_log_no) {
		*max_arch_log_no = arch_log_no;
	}
#endif /* UNIV_LOG_ARCHIVE */
}

/*================ SINGLE-TABLE TABLESPACES ==========================*/

#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Increments the count of pending operation, if space is not being deleted.
@return	TRUE if being deleted, and operation should be skipped */
UNIV_INTERN
ibool
fil_inc_pending_ops(
/*================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		fprintf(stderr,
			"InnoDB: Error: trying to do an operation on a"
			" dropped tablespace %lu\n",
			(ulong) id);
	}

	if (space == NULL || space->stop_new_ops) {
		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	space->n_pending_ops++;

	mutex_exit(&fil_system->mutex);

	return(FALSE);
}

/*******************************************************************//**
Decrements the count of pending operations. */
UNIV_INTERN
void
fil_decr_pending_ops(
/*=================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		fprintf(stderr,
			"InnoDB: Error: decrementing pending operation"
			" of a dropped tablespace %lu\n",
			(ulong) id);
	}

	if (space != NULL) {
		space->n_pending_ops--;
	}

	mutex_exit(&fil_system->mutex);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************//**
Creates the database directory for a table if it does not exist yet. */
static
void
fil_create_directory_for_tablename(
/*===============================*/
	const char*	name)	/*!< in: name in the standard
				'databasename/tablename' format */
{
	const char*	namend;
	char*		path;
	ulint		len;

	len = strlen(fil_path_to_mysql_datadir);
	namend = strchr(name, '/');
	ut_a(namend);
	path = mem_alloc(len + (namend - name) + 2);

	memcpy(path, fil_path_to_mysql_datadir, len);
	path[len] = '/';
	memcpy(path + len + 1, name, namend - name);
	path[len + (namend - name) + 1] = 0;

	srv_normalize_path_for_win(path);

	ut_a(os_file_create_directory(path, FALSE));
	mem_free(path);
}

#ifndef UNIV_HOTBACKUP
/********************************************************//**
Writes a log record about an .ibd file create/rename/delete. */
static
void
fil_op_write_log(
/*=============*/
	ulint		type,		/*!< in: MLOG_FILE_CREATE,
					MLOG_FILE_CREATE2,
					MLOG_FILE_DELETE, or
					MLOG_FILE_RENAME */
	ulint		space_id,	/*!< in: space id */
	ulint		log_flags,	/*!< in: redo log flags (stored
					in the page number field) */
	ulint		flags,		/*!< in: compressed page size
					and file format
					if type==MLOG_FILE_CREATE2, or 0 */
	const char*	name,		/*!< in: table name in the familiar
					'databasename/tablename' format, or
					the file path in the case of
					MLOG_FILE_DELETE */
	const char*	new_name,	/*!< in: if type is MLOG_FILE_RENAME,
					the new table name in the
					'databasename/tablename' format */
	mtr_t*		mtr)		/*!< in: mini-transaction handle */
{
	byte*	log_ptr;
	ulint	len;

	log_ptr = mlog_open(mtr, 11 + 2 + 1);

	if (!log_ptr) {
		/* Logging in mtr is switched off during crash recovery:
		in that case mlog_open returns NULL */
		return;
	}

	log_ptr = mlog_write_initial_log_record_for_file_op(
		type, space_id, log_flags, log_ptr, mtr);
	if (type == MLOG_FILE_CREATE2) {
		mach_write_to_4(log_ptr, flags);
		log_ptr += 4;
	}
	/* Let us store the strings as null-terminated for easier readability
	and handling */

	len = strlen(name) + 1;

	mach_write_to_2(log_ptr, len);
	log_ptr += 2;
	mlog_close(mtr, log_ptr);

	mlog_catenate_string(mtr, (byte*) name, len);

	if (type == MLOG_FILE_RENAME) {
		len = strlen(new_name) + 1;
		log_ptr = mlog_open(mtr, 2 + len);
		ut_a(log_ptr);
		mach_write_to_2(log_ptr, len);
		log_ptr += 2;
		mlog_close(mtr, log_ptr);

		mlog_catenate_string(mtr, (byte*) new_name, len);
	}
}
#endif

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
	ulint	log_flags)	/*!< in: redo log flags
				(stored in the page number parameter) */
{
	ulint		name_len;
	ulint		new_name_len;
	const char*	name;
	const char*	new_name	= NULL;
	ulint		flags		= 0;

	if (type == MLOG_FILE_CREATE2) {
		if (end_ptr < ptr + 4) {

			return(NULL);
		}

		flags = mach_read_from_4(ptr);
		ptr += 4;
	}

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	name_len = mach_read_from_2(ptr);

	ptr += 2;

	if (end_ptr < ptr + name_len) {

		return(NULL);
	}

	name = (const char*) ptr;

	ptr += name_len;

	if (type == MLOG_FILE_RENAME) {
		if (end_ptr < ptr + 2) {

			return(NULL);
		}

		new_name_len = mach_read_from_2(ptr);

		ptr += 2;

		if (end_ptr < ptr + new_name_len) {

			return(NULL);
		}

		new_name = (const char*) ptr;

		ptr += new_name_len;
	}

	/* We managed to parse a full log record body */
	/*
	printf("Parsed log rec of type %lu space %lu\n"
	"name %s\n", type, space_id, name);

	if (type == MLOG_FILE_RENAME) {
	printf("new name %s\n", new_name);
	}
	*/
	if (!space_id) {

		return(ptr);
	}

	/* Let us try to perform the file operation, if sensible. Note that
	ibbackup has at this stage already read in all space id info to the
	fil0fil.c data structures.

	NOTE that our algorithm is not guaranteed to work correctly if there
	were renames of tables during the backup. See ibbackup code for more
	on the problem. */

	switch (type) {
	case MLOG_FILE_DELETE:
		if (fil_tablespace_exists_in_mem(space_id)) {
			ut_a(fil_delete_tablespace(space_id));
		}

		break;

	case MLOG_FILE_RENAME:
		/* We do the rename based on space id, not old file name;
		this should guarantee that after the log replay each .ibd file
		has the correct name for the latest log sequence number; the
		proof is left as an exercise :) */

		if (fil_tablespace_exists_in_mem(space_id)) {
			/* Create the database directory for the new name, if
			it does not exist yet */
			fil_create_directory_for_tablename(new_name);

			/* Rename the table if there is not yet a tablespace
			with the same name */

			if (fil_get_space_id_for_table(new_name)
			    == ULINT_UNDEFINED) {
				/* We do not care of the old name, that is
				why we pass NULL as the first argument */
				if (!fil_rename_tablespace(NULL, space_id,
							   new_name)) {
					ut_error;
				}
			}
		}

		break;

	case MLOG_FILE_CREATE:
	case MLOG_FILE_CREATE2:
		if (fil_tablespace_exists_in_mem(space_id)) {
			/* Do nothing */
		} else if (fil_get_space_id_for_table(name)
			   != ULINT_UNDEFINED) {
			/* Do nothing */
		} else if (log_flags & MLOG_FILE_FLAG_TEMP) {
			/* Temporary table, do nothing */
		} else {
			/* Create the database directory for name, if it does
			not exist yet */
			fil_create_directory_for_tablename(name);

			if (fil_create_new_single_table_tablespace(
				    space_id, name, FALSE, flags,
				    FIL_IBD_FILE_INITIAL_SIZE) != DB_SUCCESS) {
				ut_error;
			}
		}

		break;

	default:
		ut_error;
	}

	return(ptr);
}

/*******************************************************************//**
Deletes a single-table tablespace. The tablespace must be cached in the
memory cache.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_delete_tablespace(
/*==================*/
	ulint	id)	/*!< in: space id */
{
	ibool		success;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		count		= 0;
	char*		path;

	ut_a(id != 0);
stop_new_ops:
	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space != NULL) {
		space->stop_new_ops = TRUE;

		if (space->n_pending_ops == 0) {
			mutex_exit(&fil_system->mutex);

			count = 0;

			goto try_again;
		} else {
			if (count > 5000) {
				ut_print_timestamp(stderr);
				fputs("  InnoDB: Warning: trying to"
				      " delete tablespace ", stderr);
				ut_print_filename(stderr, space->name);
				fprintf(stderr, ",\n"
					"InnoDB: but there are %lu pending"
					" operations (most likely ibuf merges)"
					" on it.\n"
					"InnoDB: Loop %lu.\n",
					(ulong) space->n_pending_ops,
					(ulong) count);
			}

			mutex_exit(&fil_system->mutex);

			os_thread_sleep(20000);
			count++;

			goto stop_new_ops;
		}
	}

	mutex_exit(&fil_system->mutex);
	count = 0;

try_again:
	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: cannot delete tablespace %lu\n"
			"InnoDB: because it is not found in the"
			" tablespace memory cache.\n",
			(ulong) id);

		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	ut_a(space);
	ut_a(space->n_pending_ops == 0);

	space->is_being_deleted = TRUE;

	ut_a(UT_LIST_GET_LEN(space->chain) == 1);
	node = UT_LIST_GET_FIRST(space->chain);

	if (space->n_pending_flushes > 0 || node->n_pending > 0) {
		if (count > 1000) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Warning: trying to"
			      " delete tablespace ", stderr);
			ut_print_filename(stderr, space->name);
			fprintf(stderr, ",\n"
				"InnoDB: but there are %lu flushes"
				" and %lu pending i/o's on it\n"
				"InnoDB: Loop %lu.\n",
				(ulong) space->n_pending_flushes,
				(ulong) node->n_pending,
				(ulong) count);
		}
		mutex_exit(&fil_system->mutex);
		os_thread_sleep(20000);

		count++;

		goto try_again;
	}

	path = mem_strdup(space->name);

	mutex_exit(&fil_system->mutex);

	/* Important: We rely on the data dictionary mutex to ensure
	that a race is not possible here. It should serialize the tablespace
	drop/free. We acquire an X latch only to avoid a race condition
	when accessing the tablespace instance via:

	  fsp_get_available_space_in_free_extents().

	There our main motivation is to reduce the contention on the
	dictionary mutex. */

	rw_lock_x_lock(&space->latch);

#ifndef UNIV_HOTBACKUP
	/* Invalidate in the buffer pool all pages belonging to the
	tablespace. Since we have set space->is_being_deleted = TRUE, readahead
	or ibuf merge can no longer read more pages of this tablespace to the
	buffer pool. Thus we can clean the tablespace out of the buffer pool
	completely and permanently. The flag is_being_deleted also prevents
	fil_flush() from being applied to this tablespace. */

	if (srv_lazy_drop_table) {
		buf_LRU_mark_space_was_deleted(id);
	} else {
	buf_LRU_invalidate_tablespace(id);
	}
#endif
	/* printf("Deleting tablespace %s id %lu\n", space->name, id); */

	mutex_enter(&fil_system->mutex);

	success = fil_space_free(id, TRUE);

	mutex_exit(&fil_system->mutex);

	if (success) {
		success = os_file_delete(path);

		if (!success) {
			success = os_file_delete_if_exists(path);
		}
	} else {
		rw_lock_x_unlock(&space->latch);
	}

	if (success) {
#ifndef UNIV_HOTBACKUP
		/* Write a log record about the deletion of the .ibd
		file, so that ibbackup can replay it in the
		--apply-log phase. We use a dummy mtr and the familiar
		log write mechanism. */
		mtr_t		mtr;

		/* When replaying the operation in ibbackup, do not try
		to write any log record */
		mtr_start(&mtr);

		fil_op_write_log(MLOG_FILE_DELETE, id, 0, 0, path, NULL, &mtr);
		mtr_commit(&mtr);
#endif
		mem_free(path);

		return(TRUE);
	}

	mem_free(path);

	return(FALSE);
}

/*******************************************************************//**
Returns TRUE if a single-table tablespace is being deleted.
@return TRUE if being deleted */
UNIV_INTERN
ibool
fil_tablespace_is_being_deleted(
/*============================*/
	ulint		id)	/*!< in: space id */
{
	fil_space_t*	space;
	ibool		is_being_deleted;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space != NULL);

	is_being_deleted = space->is_being_deleted;

	mutex_exit(&fil_system->mutex);

	return(is_being_deleted);
}

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
	ulint	id)	/*!< in: space id */
{
	ibool	success;

	success = fil_delete_tablespace(id);

	if (!success) {
		fprintf(stderr,
			"InnoDB: Warning: cannot delete tablespace %lu"
			" in DISCARD TABLESPACE.\n"
			"InnoDB: But let us remove the"
			" insert buffer entries for this tablespace.\n",
			(ulong) id);
	}

	/* Remove all insert buffer entries for the tablespace */

	ibuf_delete_for_discarded_space(id);

	return(success);
}
#endif /* !UNIV_HOTBACKUP */

/*******************************************************************//**
Renames the memory cache structures of a single-table tablespace.
@return	TRUE if success */
static
ibool
fil_rename_tablespace_in_mem(
/*=========================*/
	fil_space_t*	space,	/*!< in: tablespace memory object */
	fil_node_t*	node,	/*!< in: file node of that tablespace */
	const char*	path)	/*!< in: new name */
{
	fil_space_t*	space2;
	const char*	old_name	= space->name;

	ut_ad(mutex_own(&fil_system->mutex));

	space2 = fil_space_get_by_name(old_name);
	if (space != space2) {
		fputs("InnoDB: Error: cannot find ", stderr);
		ut_print_filename(stderr, old_name);
		fputs(" in tablespace memory cache\n", stderr);

		return(FALSE);
	}

	space2 = fil_space_get_by_name(path);
	if (space2 != NULL) {
		fputs("InnoDB: Error: ", stderr);
		ut_print_filename(stderr, path);
		fputs(" is already in tablespace memory cache\n", stderr);

		return(FALSE);
	}

	HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(space->name), space);
	mem_free(space->name);
	mem_free(node->name);

	space->name = mem_strdup(path);
	node->name = mem_strdup(path);

	HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(path), space);
	return(TRUE);
}

/*******************************************************************//**
Allocates a file name for a single-table tablespace. The string must be freed
by caller with mem_free().
@return	own: file name */
static
char*
fil_make_ibd_name(
/*==============*/
	const char*	name,		/*!< in: table name or a dir path of a
					TEMPORARY table */
	ibool		is_temp)	/*!< in: TRUE if it is a dir path */
{
	ulint	namelen		= strlen(name);
	ulint	dirlen		= strlen(fil_path_to_mysql_datadir);
	char*	filename	= mem_alloc(namelen + dirlen + sizeof "/.ibd");

	if (is_temp) {
		memcpy(filename, name, namelen);
		memcpy(filename + namelen, ".ibd", sizeof ".ibd");
	} else {
		memcpy(filename, fil_path_to_mysql_datadir, dirlen);
		filename[dirlen] = '/';

		memcpy(filename + dirlen + 1, name, namelen);
		memcpy(filename + dirlen + namelen + 1, ".ibd", sizeof ".ibd");
	}

	srv_normalize_path_for_win(filename);

	return(filename);
}

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
	const char*	new_name)	/*!< in: new table name in the standard
					databasename/tablename format
					of InnoDB */
{
	ibool		success;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		count		= 0;
	char*		path;
	ibool		old_name_was_specified		= TRUE;
	char*		old_path;

	ut_a(id != 0);

	if (old_name == NULL) {
		old_name = "(name not specified)";
		old_name_was_specified = FALSE;
	}
retry:
	count++;

	if (!(count % 1000)) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Warning: problems renaming ", stderr);
		ut_print_filename(stderr, old_name);
		fputs(" to ", stderr);
		ut_print_filename(stderr, new_name);
		fprintf(stderr, ", %lu iterations\n", (ulong) count);
	}

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		fprintf(stderr,
			"InnoDB: Error: cannot find space id %lu"
			" in the tablespace memory cache\n"
			"InnoDB: though the table ", (ulong) id);
		ut_print_filename(stderr, old_name);
		fputs(" in a rename operation should have that id\n", stderr);
		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	if (count > 25000) {
		space->stop_ios = FALSE;
		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	/* We temporarily close the .ibd file because we do not trust that
	operating systems can rename an open file. For the closing we have to
	wait until there are no pending i/o's or flushes on the file. */

	space->stop_ios = TRUE;

	ut_a(UT_LIST_GET_LEN(space->chain) == 1);
	node = UT_LIST_GET_FIRST(space->chain);

	if (node->n_pending > 0 || node->n_pending_flushes > 0) {
		/* There are pending i/o's or flushes, sleep for a while and
		retry */

		mutex_exit(&fil_system->mutex);

		os_thread_sleep(20000);

		goto retry;

	} else if (node->modification_counter > node->flush_counter) {
		/* Flush the space */

		mutex_exit(&fil_system->mutex);

		os_thread_sleep(20000);

		fil_flush(id, TRUE);

		goto retry;

	} else if (node->open) {
		/* Close the file */

		fil_node_close_file(node, fil_system);
	}

	/* Check that the old name in the space is right */

	if (old_name_was_specified) {
		old_path = fil_make_ibd_name(old_name, FALSE);

		ut_a(strcmp(space->name, old_path) == 0);
		ut_a(strcmp(node->name, old_path) == 0);
	} else {
		old_path = mem_strdup(space->name);
	}

	/* Rename the tablespace and the node in the memory cache */
	path = fil_make_ibd_name(new_name, FALSE);
	success = fil_rename_tablespace_in_mem(space, node, path);

	if (success) {
		success = os_file_rename(old_path, path);

		if (!success) {
			/* We have to revert the changes we made
			to the tablespace memory cache */

			ut_a(fil_rename_tablespace_in_mem(space, node,
							  old_path));
		}
	}

	mem_free(path);
	mem_free(old_path);

	space->stop_ios = FALSE;

	mutex_exit(&fil_system->mutex);

#ifndef UNIV_HOTBACKUP
	if (success) {
		mtr_t		mtr;

		mtr_start(&mtr);

		fil_op_write_log(MLOG_FILE_RENAME, id, 0, 0, old_name, new_name,
				 &mtr);
		mtr_commit(&mtr);
	}
#endif
	return(success);
}

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
	ulint		size)		/*!< in: the initial size of the
					tablespace file in pages,
					must be >= FIL_IBD_FILE_INITIAL_SIZE */
{
	os_file_t	file;
	ibool		ret;
	ulint		err;
	byte*		buf2;
	byte*		page;
	ibool		success;
	char*		path;

	ut_a(space_id > 0);
	ut_a(space_id < SRV_LOG_SPACE_FIRST_ID);
	ut_a(size >= FIL_IBD_FILE_INITIAL_SIZE);
	/* The tablespace flags (FSP_SPACE_FLAGS) should be 0 for
	ROW_FORMAT=COMPACT
	((table->flags & ~(~0 << DICT_TF_BITS)) == DICT_TF_COMPACT) and
	ROW_FORMAT=REDUNDANT (table->flags == 0).  For any other
	format, the tablespace flags should equal
	(table->flags & ~(~0 << DICT_TF_BITS)). */
	ut_a(flags != DICT_TF_COMPACT);
	ut_a(!(flags & (~0UL << DICT_TF_BITS)));

	path = fil_make_ibd_name(tablename, is_temp);

	file = os_file_create(path, OS_FILE_CREATE, OS_FILE_NORMAL,
			      OS_DATA_FILE, &ret);
	if (ret == FALSE) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error creating file ", stderr);
		ut_print_filename(stderr, path);
		fputs(".\n", stderr);

		/* The following call will print an error message */

		err = os_file_get_last_error(TRUE);

		if (err == OS_FILE_ALREADY_EXISTS) {
			fputs("InnoDB: The file already exists though"
			      " the corresponding table did not\n"
			      "InnoDB: exist in the InnoDB data dictionary."
			      " Have you moved InnoDB\n"
			      "InnoDB: .ibd files around without using the"
			      " SQL commands\n"
			      "InnoDB: DISCARD TABLESPACE and"
			      " IMPORT TABLESPACE, or did\n"
			      "InnoDB: mysqld crash in the middle of"
			      " CREATE TABLE? You can\n"
			      "InnoDB: resolve the problem by"
			      " removing the file ", stderr);
			ut_print_filename(stderr, path);
			fputs("\n"
			      "InnoDB: under the 'datadir' of MySQL.\n",
			      stderr);

			mem_free(path);
			return(DB_TABLESPACE_ALREADY_EXISTS);
		}

		if (err == OS_FILE_DISK_FULL) {

			mem_free(path);
			return(DB_OUT_OF_FILE_SPACE);
		}

		mem_free(path);
		return(DB_ERROR);
	}

	ret = os_file_set_size(path, file, size * UNIV_PAGE_SIZE, 0);

	if (!ret) {
		err = DB_OUT_OF_FILE_SPACE;
error_exit:
		os_file_close(file);
error_exit2:
		os_file_delete(path);

		mem_free(path);
		return(err);
	}

	/* printf("Creating tablespace %s id %lu\n", path, space_id); */

	/* We have to write the space id to the file immediately and flush the
	file to disk. This is because in crash recovery we must be aware what
	tablespaces exist and what are their space id's, so that we can apply
	the log records to the right file. It may take quite a while until
	buffer pool flush algorithms write anything to the file and flush it to
	disk. If we would not write here anything, the file would be filled
	with zeros from the call of os_file_set_size(), until a buffer pool
	flush would write to it. */

	buf2 = ut_malloc(3 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	memset(page, '\0', UNIV_PAGE_SIZE);

	fsp_header_init_fields(page, space_id, flags);
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

	if (!(flags & DICT_TF_ZSSIZE_MASK)) {
		buf_flush_init_for_writing(page, NULL, 0);
		ret = os_file_write(path, file, page, 0, 0, UNIV_PAGE_SIZE);
	} else {
		page_zip_des_t	page_zip;
		ulint		zip_size;

		zip_size = ((PAGE_ZIP_MIN_SIZE >> 1)
			    << ((flags & DICT_TF_ZSSIZE_MASK)
				>> DICT_TF_ZSSIZE_SHIFT));

		page_zip_set_size(&page_zip, zip_size);
		page_zip.data = page + UNIV_PAGE_SIZE;
#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
			page_zip.m_end = page_zip.m_nonempty =
			page_zip.n_blobs = 0;
		buf_flush_init_for_writing(page, &page_zip, 0);
		ret = os_file_write(path, file, page_zip.data, 0, 0, zip_size);
	}

	ut_free(buf2);

	if (!ret) {
		fputs("InnoDB: Error: could not write the first page"
		      " to tablespace ", stderr);
		ut_print_filename(stderr, path);
		putc('\n', stderr);
		err = DB_ERROR;
		goto error_exit;
	}

	ret = os_file_flush(file, TRUE);

	if (!ret) {
		fputs("InnoDB: Error: file flush of tablespace ", stderr);
		ut_print_filename(stderr, path);
		fputs(" failed\n", stderr);
		err = DB_ERROR;
		goto error_exit;
	}

	os_file_close(file);

	success = fil_space_create(path, space_id, flags, FIL_TABLESPACE);

	if (!success) {
		err = DB_ERROR;
		goto error_exit2;
	}

	fil_node_create(path, size, space_id, FALSE);

#ifndef UNIV_HOTBACKUP
	{
		mtr_t		mtr;

		mtr_start(&mtr);

		fil_op_write_log(flags
				 ? MLOG_FILE_CREATE2
				 : MLOG_FILE_CREATE,
				 space_id,
				 is_temp ? MLOG_FILE_FLAG_TEMP : 0,
				 flags,
				 tablename, NULL, &mtr);

		mtr_commit(&mtr);
	}
#endif
	mem_free(path);
	return(DB_SUCCESS);
}

#ifndef UNIV_HOTBACKUP
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
	ib_uint64_t	current_lsn)	/*!< in: reset lsn's if the lsn stamped
					to FIL_PAGE_FILE_FLUSH_LSN in the
					first page is too high */
{
	os_file_t	file;
	char*		filepath;
	byte*		page;
	byte*		buf2;
	ib_uint64_t	flush_lsn;
	ulint		space_id;
	ib_int64_t	file_size;
	ib_int64_t	offset;
	ulint		zip_size;
	ibool		success;
	page_zip_des_t	page_zip;

	filepath = fil_make_ibd_name(name, FALSE);

	file = os_file_create_simple_no_error_handling(
		filepath, OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		ut_print_timestamp(stderr);

		fputs("  InnoDB: Error: trying to open a table,"
		      " but could not\n"
		      "InnoDB: open the tablespace file ", stderr);
		ut_print_filename(stderr, filepath);
		fputs("!\n", stderr);
		mem_free(filepath);

		return(FALSE);
	}

	/* Read the first page of the tablespace */

	buf2 = ut_malloc(3 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);
	if (!success) {

		goto func_exit;
	}

	/* We have to read the file flush lsn from the header of the file */

	flush_lsn = mach_read_ull(page + FIL_PAGE_FILE_FLUSH_LSN);

	if (current_lsn >= flush_lsn) {
		/* Ok */
		success = TRUE;

		goto func_exit;
	}

	space_id = fsp_header_get_space_id(page);
	zip_size = fsp_header_get_zip_size(page);

	page_zip_des_init(&page_zip);
	page_zip_set_size(&page_zip, zip_size);
	if (zip_size) {
		page_zip.data = page + UNIV_PAGE_SIZE;
	}

	ut_print_timestamp(stderr);
	fprintf(stderr,
		"  InnoDB: Flush lsn in the tablespace file %lu"
		" to be imported\n"
		"InnoDB: is %llu, which exceeds current"
		" system lsn %llu.\n"
		"InnoDB: We reset the lsn's in the file ",
		(ulong) space_id,
		flush_lsn, current_lsn);
	ut_print_filename(stderr, filepath);
	fputs(".\n", stderr);

	ut_a(ut_is_2pow(zip_size));
	ut_a(zip_size <= UNIV_PAGE_SIZE);

	/* Loop through all the pages in the tablespace and reset the lsn and
	the page checksum if necessary */

	file_size = os_file_get_size_as_iblonglong(file);

	for (offset = 0; offset < file_size;
	     offset += zip_size ? zip_size : UNIV_PAGE_SIZE) {
		success = os_file_read(file, page,
				       (ulint)(offset & 0xFFFFFFFFUL),
				       (ulint)(offset >> 32),
				       zip_size ? zip_size : UNIV_PAGE_SIZE);
		if (!success) {

			goto func_exit;
		}
		if (mach_read_ull(page + FIL_PAGE_LSN) > current_lsn) {
			/* We have to reset the lsn */

			if (zip_size) {
				memcpy(page_zip.data, page, zip_size);
				buf_flush_init_for_writing(
					page, &page_zip, current_lsn);
				success = os_file_write(
					filepath, file, page_zip.data,
					(ulint) offset & 0xFFFFFFFFUL,
					(ulint) (offset >> 32), zip_size);
			} else {
				buf_flush_init_for_writing(
					page, NULL, current_lsn);
				success = os_file_write(
					filepath, file, page,
					(ulint)(offset & 0xFFFFFFFFUL),
					(ulint)(offset >> 32),
					UNIV_PAGE_SIZE);
			}

			if (!success) {

				goto func_exit;
			}
		}
	}

	success = os_file_flush(file, TRUE);
	if (!success) {

		goto func_exit;
	}

	/* We now update the flush_lsn stamp at the start of the file */
	success = os_file_read(file, page, 0, 0,
			       zip_size ? zip_size : UNIV_PAGE_SIZE);
	if (!success) {

		goto func_exit;
	}

	mach_write_ull(page + FIL_PAGE_FILE_FLUSH_LSN, current_lsn);

	success = os_file_write(filepath, file, page, 0, 0,
				zip_size ? zip_size : UNIV_PAGE_SIZE);
	if (!success) {

		goto func_exit;
	}
	success = os_file_flush(file, TRUE);
func_exit:
	os_file_close(file);
	ut_free(buf2);
	mem_free(filepath);

	return(success);
}

/********************************************************************//**
Checks if a page is corrupt. (for offline page)
*/
static
ibool
fil_page_buf_page_is_corrupted_offline(
/*===================================*/
	const byte*	page,		/*!< in: a database page */
	ulint		zip_size)	/*!< in: size of compressed page;
					0 for uncompressed pages */
{
	ulint		checksum_field;
	ulint		old_checksum_field;

	if (!zip_size
	    && memcmp(page + FIL_PAGE_LSN + 4,
		      page + UNIV_PAGE_SIZE
		      - FIL_PAGE_END_LSN_OLD_CHKSUM + 4, 4)) {
		return(TRUE);
	}

	checksum_field = mach_read_from_4(page
					  + FIL_PAGE_SPACE_OR_CHKSUM);

	if (zip_size) {
		return(checksum_field != BUF_NO_CHECKSUM_MAGIC
		       && checksum_field
		       != page_zip_calc_checksum(page, zip_size));
	}

	old_checksum_field = mach_read_from_4(
		page + UNIV_PAGE_SIZE
		- FIL_PAGE_END_LSN_OLD_CHKSUM);

	if (old_checksum_field != mach_read_from_4(page
						   + FIL_PAGE_LSN)
	    && old_checksum_field != BUF_NO_CHECKSUM_MAGIC
	    && old_checksum_field
	    != buf_calc_page_old_checksum(page)) {
		return(TRUE);
	}

	if (!srv_fast_checksum
	    && checksum_field != 0
	    && checksum_field != BUF_NO_CHECKSUM_MAGIC
	    && checksum_field
	    != buf_calc_page_new_checksum(page)) {
		return(TRUE);
	}

	if (srv_fast_checksum
	    && checksum_field != 0
	    && checksum_field != BUF_NO_CHECKSUM_MAGIC
	    && checksum_field
	    != buf_calc_page_new_checksum_32(page)
	    && checksum_field
	    != buf_calc_page_new_checksum(page)) {
		return(TRUE);
	}

	return(FALSE);
}

/********************************************************************//**
*/
static
void
fil_page_buf_page_store_checksum(
/*=============================*/
	byte*	page,
	ulint	zip_size)
{
	if (!zip_size) {
		mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM,
				srv_use_checksums
				? (!srv_fast_checksum
				   ? buf_calc_page_new_checksum(page)
				   : buf_calc_page_new_checksum_32(page))
						: BUF_NO_CHECKSUM_MAGIC);
		mach_write_to_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
				srv_use_checksums
				? buf_calc_page_old_checksum(page)
						: BUF_NO_CHECKSUM_MAGIC);
	} else {
		mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM,
				srv_use_checksums
				? page_zip_calc_checksum(page, zip_size)
				: BUF_NO_CHECKSUM_MAGIC);
	}
}

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
	trx_t*		trx)		/*!< in: transaction. This is only used
					for IMPORT TABLESPACE, must be NULL
					otherwise */
{
	os_file_t	file;
	char*		filepath;
	ibool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ulint		space_flags;

	filepath = fil_make_ibd_name(name, FALSE);

	/* The tablespace flags (FSP_SPACE_FLAGS) should be 0 for
	ROW_FORMAT=COMPACT
	((table->flags & ~(~0 << DICT_TF_BITS)) == DICT_TF_COMPACT) and
	ROW_FORMAT=REDUNDANT (table->flags == 0).  For any other
	format, the tablespace flags should equal
	(table->flags & ~(~0 << DICT_TF_BITS)). */
	ut_a(flags != DICT_TF_COMPACT);
	ut_a(!(flags & (~0UL << DICT_TF_BITS)));

	file = os_file_create_simple_no_error_handling(
		filepath, OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		ut_print_timestamp(stderr);

		fputs("  InnoDB: Error: trying to open a table,"
		      " but could not\n"
		      "InnoDB: open the tablespace file ", stderr);
		ut_print_filename(stderr, filepath);
		fputs("!\n"
		      "InnoDB: Have you moved InnoDB .ibd files around"
		      " without using the\n"
		      "InnoDB: commands DISCARD TABLESPACE and"
		      " IMPORT TABLESPACE?\n"
		      "InnoDB: It is also possible that this is"
		      " a temporary table #sql...,\n"
		      "InnoDB: and MySQL removed the .ibd file for this.\n"
		      "InnoDB: Please refer to\n"
		      "InnoDB: " REFMAN "innodb-troubleshooting-datadict.html\n"
		      "InnoDB: for how to resolve the issue.\n", stderr);

		mem_free(filepath);

		return(FALSE);
	}

	if (!check_space_id) {
		space_id = id;

		goto skip_check;
	}

	/* Read the first page of the tablespace */

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);

	/* We have to read the tablespace id and flags from the file. */

	space_id = fsp_header_get_space_id(page);
	space_flags = fsp_header_get_flags(page);

	if (srv_expand_import) {

		ibool		file_is_corrupt = FALSE;
		byte*		buf3;
		byte*		descr_page;
		ibool		descr_is_corrupt = FALSE;
		dulint		old_id[31];
		dulint		new_id[31];
		ulint		root_page[31];
		ulint		n_index;
		os_file_t	info_file = (os_file_t) -1;
		char*		info_file_path;
		ulint	i;
		int		len;
		ib_uint64_t	current_lsn;
		ulint		size_low, size_high, size, free_limit;
		ib_int64_t	size_bytes, free_limit_bytes;
		dict_table_t*	table;
		dict_index_t*	index;
		fil_system_t*	system;
		fil_node_t*	node = NULL;
		fil_space_t*	space;
		ulint		zip_size;

		buf3 = ut_malloc(2 * UNIV_PAGE_SIZE);
		descr_page = ut_align(buf3, UNIV_PAGE_SIZE);

		current_lsn = log_get_lsn();

		/* check the header page's consistency */
		if (buf_page_is_corrupted(page,
					  dict_table_flags_to_zip_size(space_flags))) {
			fprintf(stderr, "InnoDB: page 0 of %s seems corrupt.\n", filepath);
			file_is_corrupt = TRUE;
			descr_is_corrupt = TRUE;
		}

		/* store as first descr page */
		memcpy(descr_page, page, UNIV_PAGE_SIZE);

		zip_size = dict_table_flags_to_zip_size(flags);
		ut_a(zip_size == dict_table_flags_to_zip_size(space_flags));

		/* get free limit (page number) of the table space */
/* these should be same to the definition in fsp0fsp.c */
#define FSP_HEADER_OFFSET	FIL_PAGE_DATA
#define	FSP_FREE_LIMIT		12
		free_limit = mach_read_from_4(FSP_HEADER_OFFSET + FSP_FREE_LIMIT + page);
		free_limit_bytes = (ib_int64_t)free_limit * (ib_int64_t)(zip_size ? zip_size : UNIV_PAGE_SIZE);

		/* overwrite fsp header */
		fsp_header_init_fields(page, id, flags);
		mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, id);
		space_id = id;
		space_flags = flags;
		if (mach_read_ull(page + FIL_PAGE_FILE_FLUSH_LSN) > current_lsn)
			mach_write_ull(page + FIL_PAGE_FILE_FLUSH_LSN, current_lsn);

		fil_page_buf_page_store_checksum(page, zip_size);

		success = os_file_write(filepath, file, page, 0, 0, UNIV_PAGE_SIZE);

		/* get file size */
		os_file_get_size(file, &size_low, &size_high);
		size_bytes = (((ib_int64_t)size_high) << 32)
				+ (ib_int64_t)size_low;

		if (size_bytes < free_limit_bytes) {
			free_limit_bytes = size_bytes;
			if (size_bytes >= (ib_int64_t) (FSP_EXTENT_SIZE * (zip_size ? zip_size : UNIV_PAGE_SIZE))) {
				fprintf(stderr, "InnoDB: free limit of %s is larger than its real size.\n", filepath);
				file_is_corrupt = TRUE;
			}
		}

		/* get cruster index information */
		table = dict_table_get_low(name);
		index = dict_table_get_first_index(table);
		ut_a(index->page==3);

		/* read metadata from .exp file */
		n_index = 0;
		memset(old_id, 0, sizeof(old_id));
		memset(new_id, 0, sizeof(new_id));
		memset(root_page, 0, sizeof(root_page));

		info_file_path = fil_make_ibd_name(name, FALSE);
		len = strlen(info_file_path);
		info_file_path[len - 3] = 'e';
		info_file_path[len - 2] = 'x';
		info_file_path[len - 1] = 'p';

		info_file = os_file_create_simple_no_error_handling(
				info_file_path, OS_FILE_OPEN, OS_FILE_READ_ONLY, &success);
		if (!success) {
			fprintf(stderr, "InnoDB: cannot open %s\n", info_file_path);
			file_is_corrupt = TRUE;
			goto skip_info;
		}
		success = os_file_read(info_file, page, 0, 0, UNIV_PAGE_SIZE);
		if (!success) {
			fprintf(stderr, "InnoDB: cannot read %s\n", info_file_path);
			file_is_corrupt = TRUE;
			goto skip_info;
		}
		if (mach_read_from_4(page) != 0x78706f72UL
		    || mach_read_from_4(page + 4) != 0x74696e66UL) {
			fprintf(stderr, "InnoDB: %s seems not to be a correct .exp file\n", info_file_path);
			file_is_corrupt = TRUE;
			goto skip_info;
		}

		fprintf(stderr, "InnoDB: import: extended import of %s is started.\n", name);

		n_index = mach_read_from_4(page + 8);
		fprintf(stderr, "InnoDB: import: %lu indexes are detected.\n", (ulong)n_index);
		for (i = 0; i < n_index; i++) {
			new_id[i] =
				dict_table_get_index_on_name(table,
						(char*)(page + (i + 1) * 512 + 12))->id;
			old_id[i] = mach_read_from_8(page + (i + 1) * 512);
			root_page[i] = mach_read_from_4(page + (i + 1) * 512 + 8);
		}

skip_info:
		if (info_file != (os_file_t) -1)
			os_file_close(info_file);

		/*
		if (size_bytes >= 1024 * 1024) {
			size_bytes = ut_2pow_round(size_bytes, 1024 * 1024);
		}
		*/

		if (zip_size) {
			fprintf(stderr, "InnoDB: Warning: importing compressed table is still EXPERIMENTAL, currently.\n");
		}

		{
			mem_heap_t*	heap = NULL;
			ulint		offsets_[REC_OFFS_NORMAL_SIZE];
			ulint*		offsets = offsets_;
			ib_int64_t	offset;

			size = (ulint) (size_bytes / (zip_size ? zip_size : UNIV_PAGE_SIZE));
			/* over write space id of all pages */
			rec_offs_init(offsets_);

			/* Unlock the data dictionary to not block queries
			accessing other tables */
			ut_a(trx);
			row_mysql_unlock_data_dictionary(trx);

			fprintf(stderr, "InnoDB: Progress in %%:");

			for (offset = 0; offset < free_limit_bytes;
			     offset += zip_size ? zip_size : UNIV_PAGE_SIZE) {
				ibool		page_is_corrupt;
				ibool		is_descr_page = FALSE;

				success = os_file_read(file, page,
							(ulint)(offset & 0xFFFFFFFFUL),
							(ulint)(offset >> 32),
							zip_size ? zip_size : UNIV_PAGE_SIZE);

				page_is_corrupt = FALSE;

				/* check consistency */
				if (fil_page_buf_page_is_corrupted_offline(page, zip_size)) {
					page_is_corrupt = TRUE;
				}

				if (mach_read_from_4(page + FIL_PAGE_OFFSET)
				    != offset / (zip_size ? zip_size : UNIV_PAGE_SIZE)) {

					page_is_corrupt = TRUE;
				}

				/* if it is free page, inconsistency is acceptable */
				if (!offset) {
					/* header page*/
					/* it should be overwritten already */
					ut_a(!page_is_corrupt);

				} else if (!((offset / (zip_size ? zip_size : UNIV_PAGE_SIZE))
					     % (zip_size ? zip_size : UNIV_PAGE_SIZE))) {
					/* descr page (not header) */
					if (page_is_corrupt) {
						file_is_corrupt = TRUE;
						descr_is_corrupt = TRUE;
					} else {

						descr_is_corrupt = FALSE;
					}

					/* store as descr page */
					memcpy(descr_page, page, (zip_size ? zip_size : UNIV_PAGE_SIZE));
					is_descr_page = TRUE;

				} else if (descr_is_corrupt) {
					/* unknown state of the page */
					if (page_is_corrupt) {
						file_is_corrupt = TRUE;
					}

				} else {
					/* check free page or not */
					/* These definitions should be same to fsp0fsp.c */
#define	FSP_HEADER_SIZE		(32 + 5 * FLST_BASE_NODE_SIZE)

#define	XDES_BITMAP		(FLST_NODE_SIZE + 12)
#define	XDES_BITS_PER_PAGE	2
#define	XDES_FREE_BIT		0
#define	XDES_SIZE							\
	(XDES_BITMAP + UT_BITS_IN_BYTES(FSP_EXTENT_SIZE * XDES_BITS_PER_PAGE))
#define	XDES_ARR_OFFSET		(FSP_HEADER_OFFSET + FSP_HEADER_SIZE)

					/*descr = descr_page + XDES_ARR_OFFSET + XDES_SIZE * xdes_calc_descriptor_index(zip_size, offset)*/
					/*xdes_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr)*/
					byte*	descr;
					ulint	index;
					ulint	byte_index;
					ulint	bit_index;

					descr = descr_page + XDES_ARR_OFFSET
						+ XDES_SIZE * (ut_2pow_remainder(
							(offset / (zip_size ? zip_size : UNIV_PAGE_SIZE)),
							(zip_size ? zip_size : UNIV_PAGE_SIZE)) / FSP_EXTENT_SIZE);

					index = XDES_FREE_BIT
						+ XDES_BITS_PER_PAGE * ((offset / (zip_size ? zip_size : UNIV_PAGE_SIZE)) % FSP_EXTENT_SIZE);
					byte_index = index / 8;
					bit_index = index % 8;

					if (ut_bit_get_nth(mach_read_from_1(descr + XDES_BITMAP + byte_index), bit_index)) {
						/* free page */
						if (page_is_corrupt) {
							goto skip_write;
						}
					} else {
						/* not free */
						if (page_is_corrupt) {
							file_is_corrupt = TRUE;
						}
					}
				}

				if (page_is_corrupt) {
					fprintf(stderr, " [errp:%ld]", (long) (offset / (zip_size ? zip_size : UNIV_PAGE_SIZE)));

					/* cannot treat corrupt page */
					goto skip_write;
				}

				if (mach_read_from_4(page + FIL_PAGE_OFFSET) || !offset) {
					mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, id);

					for (i = 0; (ulint) i < n_index; i++) {
						if ((ulint) (offset / (zip_size ? zip_size : UNIV_PAGE_SIZE)) == root_page[i]) {
							if (fil_page_get_type(page) != FIL_PAGE_INDEX) {
								file_is_corrupt = TRUE;
								fprintf(stderr, " [etyp:%ld]",
									(long) (offset / (zip_size ? zip_size : UNIV_PAGE_SIZE)));
								goto skip_write;
							}
							/* this is index root page */
							mach_write_to_4(page + FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF
											+ FSEG_HDR_SPACE, id);
							mach_write_to_4(page + FIL_PAGE_DATA + PAGE_BTR_SEG_TOP
											+ FSEG_HDR_SPACE, id);
							break;
						}
					}

					if (fil_page_get_type(page) ==
					    FIL_PAGE_INDEX && !is_descr_page) {
						dulint tmp = mach_read_from_8(page + (PAGE_HEADER + PAGE_INDEX_ID));

						for (i = 0; i < n_index; i++) {
							if (ut_dulint_cmp(old_id[i], tmp) == 0) {
								mach_write_to_8(page + (PAGE_HEADER + PAGE_INDEX_ID), new_id[i]);
								break;
							}
						}

						if (!zip_size && mach_read_from_2(page + PAGE_HEADER + PAGE_LEVEL) == 0
						    && ut_dulint_cmp(old_id[0], tmp) == 0) {
							/* leaf page of cluster index, reset trx_id of records */
							rec_t*	rec;
							rec_t*	supremum;
							ulint	n_recs;

							supremum = page_get_supremum_rec(page);
							rec = page_rec_get_next(page_get_infimum_rec(page));
							n_recs = page_get_n_recs(page);

							while (rec && rec != supremum && n_recs > 0) {
								ulint	n_fields;
								ulint	i;
								ulint	offset = index->trx_id_offset;
								offsets = rec_get_offsets(rec, index, offsets,
										ULINT_UNDEFINED, &heap);
								n_fields = rec_offs_n_fields(offsets);
								if (!offset) {
									offset = row_get_trx_id_offset(index, offsets);
								}
								trx_write_trx_id(rec + offset, ut_dulint_create(0, 1));

								for (i = 0; i < n_fields; i++) {
									if (rec_offs_nth_extern(offsets, i)) {
										ulint	local_len;
										byte*	data;

										data = rec_get_nth_field(rec, offsets, i, &local_len);

										local_len -= BTR_EXTERN_FIELD_REF_SIZE;

										mach_write_to_4(data + local_len + BTR_EXTERN_SPACE_ID, id);
									}
								}

								rec = page_rec_get_next(rec);
								n_recs--;
							}
						} else if (mach_read_from_2(page + PAGE_HEADER + PAGE_LEVEL) == 0
							   && ut_dulint_cmp(old_id[0], tmp) != 0) {
							mach_write_to_8(page + (PAGE_HEADER + PAGE_MAX_TRX_ID), ut_dulint_create(0, 1));
						}
					}

					if (mach_read_ull(page + FIL_PAGE_LSN) > current_lsn) {
						mach_write_ull(page + FIL_PAGE_LSN, current_lsn);
						if (!zip_size) {
							mach_write_ull(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
									current_lsn);
						}
					}

					fil_page_buf_page_store_checksum(page, zip_size);

					success = os_file_write(filepath, file, page,
								(ulint)(offset & 0xFFFFFFFFUL),
								(ulint)(offset >> 32),
								zip_size ? zip_size : UNIV_PAGE_SIZE);
				}

skip_write:
				if (free_limit_bytes
				    && ((ib_int64_t)((offset + (zip_size ? zip_size : UNIV_PAGE_SIZE)) * 100) / free_limit_bytes)
					!= ((offset * 100) / free_limit_bytes)) {
					fprintf(stderr, " %lu",
						(ulong)((ib_int64_t)((offset + (zip_size ? zip_size : UNIV_PAGE_SIZE)) * 100) / free_limit_bytes));
				}
			}

			fprintf(stderr, " done.\n");

			/* Reacquire the data dictionary lock */
			row_mysql_lock_data_dictionary(trx);

			/* update SYS_INDEXES set root page */
			index = dict_table_get_first_index(table);
			while (index) {
				for (i = 0; i < n_index; i++) {
					if (ut_dulint_cmp(new_id[i], index->id) == 0) {
						break;
					}
				}

				if (i != n_index
				    && root_page[i] != index->page) {
					/* must update */
					ulint	error;
					trx_t*	trx;
					pars_info_t*	info = NULL;

					trx = trx_allocate_for_mysql();
					trx->op_info = "extended import";

					info = pars_info_create();

					pars_info_add_dulint_literal(info, "indexid", new_id[i]);
					pars_info_add_int4_literal(info, "new_page", (lint) root_page[i]);

					error = que_eval_sql(info,
						"PROCEDURE UPDATE_INDEX_PAGE () IS\n"
						"BEGIN\n"
						"UPDATE SYS_INDEXES"
						" SET PAGE_NO = :new_page"
						" WHERE ID = :indexid;\n"
						"COMMIT WORK;\n"
						"END;\n",
						FALSE, trx);

					if (error != DB_SUCCESS) {
						fprintf(stderr, "InnoDB: failed to update SYS_INDEXES\n");
					}

					trx_commit_for_mysql(trx);

					trx_free_for_mysql(trx);

					index->page = root_page[i];
				}

				index = dict_table_get_next_index(index);
			}
			if (UNIV_LIKELY_NULL(heap)) {
				mem_heap_free(heap);
			}
		}
		/* .exp file should be removed */
		success = os_file_delete(info_file_path);
		if (!success) {
			success = os_file_delete_if_exists(info_file_path);
		}
		mem_free(info_file_path);

		system	= fil_system;
		mutex_enter(&(system->mutex));
		space = fil_space_get_by_id(id);
		if (space)
			node = UT_LIST_GET_FIRST(space->chain);
		if (node && node->size < size) {
			space->size += (size - node->size);
			node->size = size;
		}
		mutex_exit(&(system->mutex));

		ut_free(buf3);

		if (file_is_corrupt) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: file ",
			      stderr);
			ut_print_filename(stderr, filepath);
			fprintf(stderr, " seems to be corrupt.\n"
				"InnoDB: anyway, all not corrupt pages were tried to be converted to salvage.\n"
				"InnoDB: ##### CAUTION #####\n"
				"InnoDB: ## The .ibd must cause to crash InnoDB, though re-import would seem to be succeeded.\n"
				"InnoDB: ## If you don't have knowledge about salvaging data from .ibd, you should not use the file.\n"
				"InnoDB: ###################\n");
			success = FALSE;

			ut_free(buf2);

			goto func_exit;
		}
	}

	ut_free(buf2);

	if (UNIV_UNLIKELY(space_id != id
			  || space_flags != (flags & ~(~0 << DICT_TF_BITS)))) {
		ut_print_timestamp(stderr);

		fputs("  InnoDB: Error: tablespace id and flags in file ",
		      stderr);
		ut_print_filename(stderr, filepath);
		fprintf(stderr, " are %lu and %lu, but in the InnoDB\n"
			"InnoDB: data dictionary they are %lu and %lu.\n"
			"InnoDB: Have you moved InnoDB .ibd files"
			" around without using the\n"
			"InnoDB: commands DISCARD TABLESPACE and"
			" IMPORT TABLESPACE?\n"
			"InnoDB: Please refer to\n"
			"InnoDB: " REFMAN "innodb-troubleshooting-datadict.html\n"
			"InnoDB: for how to resolve the issue.\n",
			(ulong) space_id, (ulong) space_flags,
			(ulong) id, (ulong) flags);

		success = FALSE;

		goto func_exit;
	}

skip_check:
	success = fil_space_create(filepath, space_id, flags, FIL_TABLESPACE);

	if (!success) {
		goto func_exit;
	}

	/* We do not measure the size of the file, that is why we pass the 0
	below */

	fil_node_create(filepath, 0, space_id, FALSE);
func_exit:
	os_file_close(file);
	mem_free(filepath);

	if (srv_expand_import && dict_table_flags_to_zip_size(flags)) {
		ulint		page_no;
		ulint		zip_size;
		ulint		height;
		rec_t*		node_ptr;
		dict_table_t*	table;
		dict_index_t*	index;
		buf_block_t*	block;
		page_t*		page;
		page_zip_des_t*	page_zip;
		mtr_t		mtr;

		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*		offsets		= offsets_;

		rec_offs_init(offsets_);

		zip_size = dict_table_flags_to_zip_size(flags);

		table = dict_table_get_low(name);
		index = dict_table_get_first_index(table);
		page_no = dict_index_get_page(index);
		ut_a(page_no == 3);

		fprintf(stderr, "InnoDB: It is compressed .ibd file. need to convert additionaly on buffer pool.\n");

		/* down to leaf */
		mtr_start(&mtr);
		mtr_set_log_mode(&mtr, MTR_LOG_NONE);

		height = ULINT_UNDEFINED;

		for (;;) {
			block = buf_page_get(space_id, zip_size, page_no,
					     RW_NO_LATCH, &mtr);
			page = buf_block_get_frame(block);

			block->check_index_page_at_flush = TRUE;

			if (height == ULINT_UNDEFINED) {
				height = btr_page_get_level(page, &mtr);
			}

			if (height == 0) {
				break;
			}

			node_ptr = page_rec_get_next(page_get_infimum_rec(page));

			height--;

			offsets = rec_get_offsets(node_ptr, index, offsets, ULINT_UNDEFINED, &heap);
			page_no = btr_node_ptr_get_child_page_no(node_ptr, offsets);
		}

		mtr_commit(&mtr);

		fprintf(stderr, "InnoDB: pages needs split are ...");

		/* scan reaf pages */
		while (page_no != FIL_NULL) {
			rec_t*	rec;
			rec_t*	supremum;
			ulint	n_recs;

			mtr_start(&mtr);

			block = buf_page_get(space_id, zip_size, page_no,
					     RW_X_LATCH, &mtr);
			page = buf_block_get_frame(block);
			page_zip = buf_block_get_page_zip(block);

			if (!page_zip) {
				/*something wrong*/
				fprintf(stderr, "InnoDB: Something wrong with reading page %lu.\n", page_no);
convert_err_exit:
				mtr_commit(&mtr);
				mutex_enter(&fil_system->mutex);
				fil_space_free(space_id, FALSE);
				mutex_exit(&fil_system->mutex);
				success = FALSE;
				goto convert_exit;
			}

			supremum = page_get_supremum_rec(page);
			rec = page_rec_get_next(page_get_infimum_rec(page));
			n_recs = page_get_n_recs(page);

			/* illegal operation as InnoDB online system. so not logged */
			while (rec && rec != supremum && n_recs > 0) {
				ulint	n_fields;
				ulint	i;
				ulint	offset = index->trx_id_offset;

				offsets = rec_get_offsets(rec, index, offsets,
						ULINT_UNDEFINED, &heap);
				n_fields = rec_offs_n_fields(offsets);
				if (!offset) {
					offset = row_get_trx_id_offset(index, offsets);
				}
				trx_write_trx_id(rec + offset, ut_dulint_create(0, 1));

				for (i = 0; i < n_fields; i++) {
					if (rec_offs_nth_extern(offsets, i)) {
						ulint	local_len;
						byte*	data;

						data = rec_get_nth_field(rec, offsets, i, &local_len);

						local_len -= BTR_EXTERN_FIELD_REF_SIZE;

						mach_write_to_4(data + local_len + BTR_EXTERN_SPACE_ID, id);
					}
				}

				rec = page_rec_get_next(rec);
				n_recs--;
			}

			/* dummy logged update for along with modified page path */
			if (ut_dulint_cmp(index->id, btr_page_get_index_id(page)) != 0) {
				/* this should be adjusted already */
				fprintf(stderr, "InnoDB: The page %lu seems to be converted wrong.\n", page_no);
				goto convert_err_exit;
			}
			btr_page_set_index_id(page, page_zip, index->id, &mtr);

			/* confirm whether fits to the page size or not */
			if (!page_zip_compress(page_zip, page, index, &mtr)
			    && !btr_page_reorganize(block, index, &mtr)) {
				buf_block_t*	new_block;
				page_t*		new_page;
				page_zip_des_t*	new_page_zip;
				rec_t*		split_rec;
				ulint		n_uniq;

				/* split page is needed */
				fprintf(stderr, " %lu", page_no);

				mtr_x_lock(dict_index_get_lock(index), &mtr);

				n_uniq = dict_index_get_n_unique_in_tree(index);

				if(page_get_n_recs(page) < 2) {
					/* no way to make smaller */
					fprintf(stderr, "InnoDB: The page %lu cannot be store to the page size.\n", page_no);
					goto convert_err_exit;
				}

				if (UNIV_UNLIKELY(page_no == dict_index_get_page(index))) {
					ulint		new_page_no;
					dtuple_t*	node_ptr;
					ulint		level;
					rec_t*		node_ptr_rec;
					page_cur_t	page_cursor;

					/* it is root page, need to raise before split */

					level = btr_page_get_level(page, &mtr);

					new_block = btr_page_alloc(index, 0, FSP_NO_DIR, level, &mtr, &mtr);
					new_page = buf_block_get_frame(new_block);
					new_page_zip = buf_block_get_page_zip(new_block);
					btr_page_create(new_block, new_page_zip, index, level, &mtr);

					btr_page_set_next(new_page, new_page_zip, FIL_NULL, &mtr);
					btr_page_set_prev(new_page, new_page_zip, FIL_NULL, &mtr);

					page_zip_copy_recs(new_page_zip, new_page,
							   page_zip, page, index, &mtr);
					btr_search_move_or_delete_hash_entries(new_block, block, index);

					rec = page_rec_get_next(page_get_infimum_rec(new_page));
					new_page_no = buf_block_get_page_no(new_block);

					node_ptr = dict_index_build_node_ptr(index, rec, new_page_no, heap,
									     level);
					dtuple_set_info_bits(node_ptr,
							     dtuple_get_info_bits(node_ptr)
							     | REC_INFO_MIN_REC_FLAG);
					btr_page_empty(block, page_zip, index, level + 1, &mtr);

					btr_page_set_next(page, page_zip, FIL_NULL, &mtr);
					btr_page_set_prev(page, page_zip, FIL_NULL, &mtr);

					page_cur_set_before_first(block, &page_cursor);

					node_ptr_rec = page_cur_tuple_insert(&page_cursor, node_ptr,
									     index, 0, &mtr);
					ut_a(node_ptr_rec);

					if (!btr_page_reorganize(block, index, &mtr)) {
						fprintf(stderr, "InnoDB: failed to store the page %lu.\n", page_no);
						goto convert_err_exit;
					}

					/* move to the raised page */
					page_no = new_page_no;
					block = new_block;
					page = new_page;
					page_zip = new_page_zip;

					fprintf(stderr, "(raise_to:%lu)", page_no);
				}

				split_rec = page_get_middle_rec(page);

				new_block = btr_page_alloc(index, page_no + 1, FSP_UP,
							   btr_page_get_level(page, &mtr), &mtr, &mtr);
				new_page = buf_block_get_frame(new_block);
				new_page_zip = buf_block_get_page_zip(new_block);
				btr_page_create(new_block, new_page_zip, index,
						btr_page_get_level(page, &mtr), &mtr);

				offsets = rec_get_offsets(split_rec, index, offsets, n_uniq, &heap);

				btr_attach_half_pages(index, block,
						      split_rec, new_block, FSP_UP, &mtr);

				page_zip_copy_recs(new_page_zip, new_page,
						   page_zip, page, index, &mtr);
				page_delete_rec_list_start(split_rec - page + new_page,
							   new_block, index, &mtr);
				btr_search_move_or_delete_hash_entries(new_block, block, index);
				page_delete_rec_list_end(split_rec, block, index,
							 ULINT_UNDEFINED, ULINT_UNDEFINED, &mtr);

				fprintf(stderr, "(new:%lu)", buf_block_get_page_no(new_block));

				/* Are they needed? */
				if (!btr_page_reorganize(block, index, &mtr)) {
					fprintf(stderr, "InnoDB: failed to store the page %lu.\n", page_no);
					goto convert_err_exit;
				}
				if (!btr_page_reorganize(new_block, index, &mtr)) {
					fprintf(stderr, "InnoDB: failed to store the page %lu.\n", buf_block_get_page_no(new_block));
					goto convert_err_exit;
				}
			}

			page_no = btr_page_get_next(page, &mtr);

			mtr_commit(&mtr);

			if (heap) {
				mem_heap_empty(heap);
			}
		}

		fprintf(stderr, "...done.\nInnoDB: waiting the flush batch of the additional conversion.\n");

		/* should wait for the not-logged changes are all flushed */
		buf_flush_batch(BUF_FLUSH_LIST, ULINT_MAX, mtr.end_lsn + 1);
		buf_flush_wait_batch_end(BUF_FLUSH_LIST);

		fprintf(stderr, "InnoDB: done.\n");
convert_exit:
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}

	return(success);
}
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_HOTBACKUP
/*******************************************************************//**
Allocates a file name for an old version of a single-table tablespace.
The string must be freed by caller with mem_free()!
@return	own: file name */
static
char*
fil_make_ibbackup_old_name(
/*=======================*/
	const char*	name)		/*!< in: original file name */
{
	static const char suffix[] = "_ibbackup_old_vers_";
	ulint	len	= strlen(name);
	char*	path	= mem_alloc(len + (15 + sizeof suffix));

	memcpy(path, name, len);
	memcpy(path + len, suffix, (sizeof suffix) - 1);
	ut_sprintf_timestamp_without_extra_chars(path + len + sizeof suffix);
	return(path);
}
#endif /* UNIV_HOTBACKUP */

/********************************************************************//**
Opens an .ibd file and adds the associated single-table tablespace to the
InnoDB fil0fil.c data structures. */
static
void
fil_load_single_table_tablespace(
/*=============================*/
	const char*	dbname,		/*!< in: database name */
	const char*	filename)	/*!< in: file name (not a path),
					including the .ibd extension */
{
	os_file_t	file;
	char*		filepath;
	ibool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ulint		flags;
	ulint		size_low;
	ulint		size_high;
	ib_uint64_t	size;
#ifdef UNIV_HOTBACKUP
	fil_space_t*	space;
#endif
	filepath = mem_alloc(strlen(dbname) + strlen(filename)
			     + strlen(fil_path_to_mysql_datadir) + 3);

	sprintf(filepath, "%s/%s/%s", fil_path_to_mysql_datadir, dbname,
		filename);
	srv_normalize_path_for_win(filepath);
#ifdef __WIN__
# ifndef UNIV_HOTBACKUP
	/* If lower_case_table_names is 0 or 2, then MySQL allows database
	directory names with upper case letters. On Windows, all table and
	database names in InnoDB are internally always in lower case. Put the
	file path to lower case, so that we are consistent with InnoDB's
	internal data dictionary. */

	dict_casedn_str(filepath);
# endif /* !UNIV_HOTBACKUP */
#endif
	file = os_file_create_simple_no_error_handling(
		filepath, OS_FILE_OPEN, OS_FILE_READ_ONLY, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		fprintf(stderr,
			"InnoDB: Error: could not open single-table tablespace"
			" file\n"
			"InnoDB: %s!\n"
			"InnoDB: We do not continue the crash recovery,"
			" because the table may become\n"
			"InnoDB: corrupt if we cannot apply the log records"
			" in the InnoDB log to it.\n"
			"InnoDB: To fix the problem and start mysqld:\n"
			"InnoDB: 1) If there is a permission problem"
			" in the file and mysqld cannot\n"
			"InnoDB: open the file, you should"
			" modify the permissions.\n"
			"InnoDB: 2) If the table is not needed, or you can"
			" restore it from a backup,\n"
			"InnoDB: then you can remove the .ibd file,"
			" and InnoDB will do a normal\n"
			"InnoDB: crash recovery and ignore that table.\n"
			"InnoDB: 3) If the file system or the"
			" disk is broken, and you cannot remove\n"
			"InnoDB: the .ibd file, you can set"
			" innodb_force_recovery > 0 in my.cnf\n"
			"InnoDB: and force InnoDB to continue crash"
			" recovery here.\n", filepath);

		mem_free(filepath);

		if (srv_force_recovery > 0) {
			fprintf(stderr,
				"InnoDB: innodb_force_recovery"
				" was set to %lu. Continuing crash recovery\n"
				"InnoDB: even though we cannot access"
				" the .ibd file of this table.\n",
				srv_force_recovery);
			return;
		}

		exit(1);
	}

	success = os_file_get_size(file, &size_low, &size_high);

	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		fprintf(stderr,
			"InnoDB: Error: could not measure the size"
			" of single-table tablespace file\n"
			"InnoDB: %s!\n"
			"InnoDB: We do not continue crash recovery,"
			" because the table will become\n"
			"InnoDB: corrupt if we cannot apply the log records"
			" in the InnoDB log to it.\n"
			"InnoDB: To fix the problem and start mysqld:\n"
			"InnoDB: 1) If there is a permission problem"
			" in the file and mysqld cannot\n"
			"InnoDB: access the file, you should"
			" modify the permissions.\n"
			"InnoDB: 2) If the table is not needed,"
			" or you can restore it from a backup,\n"
			"InnoDB: then you can remove the .ibd file,"
			" and InnoDB will do a normal\n"
			"InnoDB: crash recovery and ignore that table.\n"
			"InnoDB: 3) If the file system or the disk is broken,"
			" and you cannot remove\n"
			"InnoDB: the .ibd file, you can set"
			" innodb_force_recovery > 0 in my.cnf\n"
			"InnoDB: and force InnoDB to continue"
			" crash recovery here.\n", filepath);

		os_file_close(file);
		mem_free(filepath);

		if (srv_force_recovery > 0) {
			fprintf(stderr,
				"InnoDB: innodb_force_recovery"
				" was set to %lu. Continuing crash recovery\n"
				"InnoDB: even though we cannot access"
				" the .ibd file of this table.\n",
				srv_force_recovery);
			return;
		}

		exit(1);
	}

	/* TODO: What to do in other cases where we cannot access an .ibd
	file during a crash recovery? */

	/* Every .ibd file is created >= 4 pages in size. Smaller files
	cannot be ok. */

	size = (((ib_uint64_t)size_high) << 32) + (ib_uint64_t)size_low;
#ifndef UNIV_HOTBACKUP
	if (size < (ib_uint64_t) (FIL_IBD_FILE_INITIAL_SIZE * (lint)UNIV_PAGE_SIZE)) {
		fprintf(stderr,
			"InnoDB: Error: the size of single-table tablespace"
			" file %s\n"
			"InnoDB: is only %lu %lu, should be at least %lu!",
			filepath,
			(ulong) size_high,
			(ulong) size_low, (ulong) (4 * UNIV_PAGE_SIZE));
		os_file_close(file);
		mem_free(filepath);

		return;
	}
#endif
	/* Read the first page of the tablespace if the size big enough */

	buf2 = ut_malloc(2 * UNIV_PAGE_SIZE);
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = ut_align(buf2, UNIV_PAGE_SIZE);

	if (size >= (ib_uint64_t) (FIL_IBD_FILE_INITIAL_SIZE * (lint)UNIV_PAGE_SIZE)) {
		success = os_file_read(file, page, 0, 0, UNIV_PAGE_SIZE);

		/* We have to read the tablespace id from the file */

		space_id = fsp_header_get_space_id(page);
		flags = fsp_header_get_flags(page);
	} else {
		space_id = ULINT_UNDEFINED;
		flags = 0;
	}

#ifndef UNIV_HOTBACKUP
	if (space_id == ULINT_UNDEFINED || trx_sys_sys_space(space_id)) {
		fprintf(stderr,
			"InnoDB: Error: tablespace id %lu in file %s"
			" is not sensible\n",
			(ulong) space_id,
			filepath);
		goto func_exit;
	}
#else
	if (space_id == ULINT_UNDEFINED || trx_sys_sys_space(space_id)) {
		char*	new_path;

		fprintf(stderr,
			"InnoDB: Renaming tablespace %s of id %lu,\n"
			"InnoDB: to %s_ibbackup_old_vers_<timestamp>\n"
			"InnoDB: because its size %" PRId64 " is too small"
			" (< 4 pages 16 kB each),\n"
			"InnoDB: or the space id in the file header"
			" is not sensible.\n"
			"InnoDB: This can happen in an ibbackup run,"
			" and is not dangerous.\n",
			filepath, space_id, filepath, size);
		os_file_close(file);

		new_path = fil_make_ibbackup_old_name(filepath);
		ut_a(os_file_rename(filepath, new_path));

		ut_free(buf2);
		mem_free(filepath);
		mem_free(new_path);

		return;
	}

	/* A backup may contain the same space several times, if the space got
	renamed at a sensitive time. Since it is enough to have one version of
	the space, we rename the file if a space with the same space id
	already exists in the tablespace memory cache. We rather rename the
	file than delete it, because if there is a bug, we do not want to
	destroy valuable data. */

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(space_id);

	if (space) {
		char*	new_path;

		fprintf(stderr,
			"InnoDB: Renaming tablespace %s of id %lu,\n"
			"InnoDB: to %s_ibbackup_old_vers_<timestamp>\n"
			"InnoDB: because space %s with the same id\n"
			"InnoDB: was scanned earlier. This can happen"
			" if you have renamed tables\n"
			"InnoDB: during an ibbackup run.\n",
			filepath, space_id, filepath,
			space->name);
		os_file_close(file);

		new_path = fil_make_ibbackup_old_name(filepath);

		mutex_exit(&fil_system->mutex);

		ut_a(os_file_rename(filepath, new_path));

		ut_free(buf2);
		mem_free(filepath);
		mem_free(new_path);

		return;
	}
	mutex_exit(&fil_system->mutex);
#endif
	success = fil_space_create(filepath, space_id, flags, FIL_TABLESPACE);

	if (!success) {

		if (srv_force_recovery > 0) {
			fprintf(stderr,
				"InnoDB: innodb_force_recovery"
				" was set to %lu. Continuing crash recovery\n"
				"InnoDB: even though the tablespace creation"
				" of this table failed.\n",
				srv_force_recovery);
			goto func_exit;
		}

		exit(1);
	}

	/* We do not use the size information we have about the file, because
	the rounding formula for extents and pages is somewhat complex; we
	let fil_node_open() do that task. */

	fil_node_create(filepath, 0, space_id, FALSE);
func_exit:
	os_file_close(file);
	ut_free(buf2);
	mem_free(filepath);
}

/***********************************************************************//**
A fault-tolerant function that tries to read the next file name in the
directory. We retry 100 times if os_file_readdir_next_file() returns -1. The
idea is to read as much good data as we can and jump over bad data.
@return 0 if ok, -1 if error even after the retries, 1 if at the end
of the directory */
static
int
fil_file_readdir_next_file(
/*=======================*/
	ulint*		err,	/*!< out: this is set to DB_ERROR if an error
				was encountered, otherwise not changed */
	const char*	dirname,/*!< in: directory name or path */
	os_file_dir_t	dir,	/*!< in: directory stream */
	os_file_stat_t*	info)	/*!< in/out: buffer where the info is returned */
{
	ulint	i;
	int	ret;

	for (i = 0; i < 100; i++) {
		ret = os_file_readdir_next_file(dirname, dir, info);

		if (ret != -1) {

			return(ret);
		}

		fprintf(stderr,
			"InnoDB: Error: os_file_readdir_next_file()"
			" returned -1 in\n"
			"InnoDB: directory %s\n"
			"InnoDB: Crash recovery may have failed"
			" for some .ibd files!\n", dirname);

		*err = DB_ERROR;
	}

	return(-1);
}

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
fil_load_single_table_tablespaces(void)
/*===================================*/
{
	int		ret;
	char*		dbpath		= NULL;
	ulint		dbpath_len	= 100;
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;
	ulint		err		= DB_SUCCESS;

	/* The datadir of MySQL is always the default directory of mysqld */

	dir = os_file_opendir(fil_path_to_mysql_datadir, TRUE);

	if (dir == NULL) {

		return(DB_ERROR);
	}

	dbpath = mem_alloc(dbpath_len);

	/* Scan all directories under the datadir. They are the database
	directories of MySQL. */

	ret = fil_file_readdir_next_file(&err, fil_path_to_mysql_datadir, dir,
					 &dbinfo);
	while (ret == 0) {
		ulint len;
		/* printf("Looking at %s in datadir\n", dbinfo.name); */

		if (dbinfo.type == OS_FILE_TYPE_FILE
		    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

			goto next_datadir_item;
		}

		/* We found a symlink or a directory; try opening it to see
		if a symlink is a directory */

		len = strlen(fil_path_to_mysql_datadir)
			+ strlen (dbinfo.name) + 2;
		if (len > dbpath_len) {
			dbpath_len = len;

			if (dbpath) {
				mem_free(dbpath);
			}

			dbpath = mem_alloc(dbpath_len);
		}
		sprintf(dbpath, "%s/%s", fil_path_to_mysql_datadir,
			dbinfo.name);
		srv_normalize_path_for_win(dbpath);

		dbdir = os_file_opendir(dbpath, FALSE);

		if (dbdir != NULL) {
			/* printf("Opened dir %s\n", dbinfo.name); */

			/* We found a database directory; loop through it,
			looking for possible .ibd files in it */

			ret = fil_file_readdir_next_file(&err, dbpath, dbdir,
							 &fileinfo);
			while (ret == 0) {
				/* printf(
				"     Looking at file %s\n", fileinfo.name); */

				if (fileinfo.type == OS_FILE_TYPE_DIR) {

					goto next_file_item;
				}

				/* We found a symlink or a file */
				if (strlen(fileinfo.name) > 4
				    && 0 == strcmp(fileinfo.name
						   + strlen(fileinfo.name) - 4,
						   ".ibd")) {
					/* The name ends in .ibd; try opening
					the file */
					fil_load_single_table_tablespace(
						dbinfo.name, fileinfo.name);
				}
next_file_item:
				ret = fil_file_readdir_next_file(&err,
								 dbpath, dbdir,
								 &fileinfo);
			}

			if (0 != os_file_closedir(dbdir)) {
				fputs("InnoDB: Warning: could not"
				      " close database directory ", stderr);
				ut_print_filename(stderr, dbpath);
				putc('\n', stderr);

				err = DB_ERROR;
			}
		}

next_datadir_item:
		ret = fil_file_readdir_next_file(&err,
						 fil_path_to_mysql_datadir,
						 dir, &dbinfo);
	}

	mem_free(dbpath);

	if (0 != os_file_closedir(dir)) {
		fprintf(stderr,
			"InnoDB: Error: could not close MySQL datadir\n");

		return(DB_ERROR);
	}

	return(err);
}

/*******************************************************************//**
Returns TRUE if a single-table tablespace does not exist in the memory cache,
or is being deleted there.
@return	TRUE if does not exist or is being\ deleted */
UNIV_INTERN
ibool
fil_tablespace_deleted_or_being_deleted_in_mem(
/*===========================================*/
	ulint		id,	/*!< in: space id */
	ib_int64_t	version)/*!< in: tablespace_version should be this; if
				you pass -1 as the value of this, then this
				parameter is ignored */
{
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space == NULL || space->is_being_deleted) {
		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	if (version != ((ib_int64_t)-1)
	    && space->tablespace_version != version) {
		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	mutex_exit(&fil_system->mutex);

	return(FALSE);
}

/*******************************************************************//**
Returns TRUE if a single-table tablespace exists in the memory cache.
@return	TRUE if exists */
UNIV_INTERN
ibool
fil_tablespace_exists_in_mem(
/*=========================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	mutex_exit(&fil_system->mutex);

	return(space != NULL);
}

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
	ibool		print_error_if_does_not_exist)
					/*!< in: print detailed error
					information to the .err log if a
					matching tablespace is not found from
					memory */
{
	fil_space_t*	namespace;
	fil_space_t*	space;
	char*		path;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	path = fil_make_ibd_name(name, is_temp);

	/* Look if there is a space with the same id */

	space = fil_space_get_by_id(id);

	/* Look if there is a space with the same name; the name is the
	directory path from the datadir to the file */

	namespace = fil_space_get_by_name(path);
	if (space && space == namespace) {
		/* Found */

		if (mark_space) {
			space->mark = TRUE;
		}

		mem_free(path);
		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	if (!print_error_if_does_not_exist) {

		mem_free(path);
		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	if (space == NULL) {
		if (namespace == NULL) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary"
				" has tablespace id %lu,\n"
				"InnoDB: but tablespace with that id"
				" or name does not exist. Have\n"
				"InnoDB: you deleted or moved .ibd files?\n"
				"InnoDB: This may also be a table created with"
				" CREATE TEMPORARY TABLE\n"
				"InnoDB: whose .ibd and .frm files"
				" MySQL automatically removed, but the\n"
				"InnoDB: table still exists in the"
				" InnoDB internal data dictionary.\n",
				(ulong) id);
		} else {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary has"
				" tablespace id %lu,\n"
				"InnoDB: but a tablespace with that id"
				" does not exist. There is\n"
				"InnoDB: a tablespace of name %s and id %lu,"
				" though. Have\n"
				"InnoDB: you deleted or moved .ibd files?\n",
				(ulong) id, namespace->name,
				(ulong) namespace->id);
		}
error_exit:
		fputs("InnoDB: Please refer to\n"
		      "InnoDB: " REFMAN "innodb-troubleshooting-datadict.html\n"
		      "InnoDB: for how to resolve the issue.\n", stderr);

		mem_free(path);
		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	if (0 != strcmp(space->name, path)) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Error: table ", stderr);
		ut_print_filename(stderr, name);
		fprintf(stderr, "\n"
			"InnoDB: in InnoDB data dictionary has"
			" tablespace id %lu,\n"
			"InnoDB: but the tablespace with that id"
			" has name %s.\n"
			"InnoDB: Have you deleted or moved .ibd files?\n",
			(ulong) id, space->name);

		if (namespace != NULL) {
			fputs("InnoDB: There is a tablespace"
			      " with the right name\n"
			      "InnoDB: ", stderr);
			ut_print_filename(stderr, namespace->name);
			fprintf(stderr, ", but its id is %lu.\n",
				(ulong) namespace->id);
		}

		goto error_exit;
	}

	mem_free(path);
	mutex_exit(&fil_system->mutex);

	return(FALSE);
}

/*******************************************************************//**
Checks if a single-table tablespace for a given table name exists in the
tablespace memory cache.
@return	space id, ULINT_UNDEFINED if not found */
static
ulint
fil_get_space_id_for_table(
/*=======================*/
	const char*	name)	/*!< in: table name in the standard
				'databasename/tablename' format */
{
	fil_space_t*	namespace;
	ulint		id		= ULINT_UNDEFINED;
	char*		path;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	path = fil_make_ibd_name(name, FALSE);

	/* Look if there is a space with the same name; the name is the
	directory path to the file */

	namespace = fil_space_get_by_name(path);

	if (namespace) {
		id = namespace->id;
	}

	mem_free(path);

	mutex_exit(&fil_system->mutex);

	return(id);
}

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
	ulint	size_after_extend)/*!< in: desired size in pages after the
				extension; if the current space size is bigger
				than this already, the function does nothing */
{
	fil_node_t*	node;
	fil_space_t*	space;
	byte*		buf2;
	byte*		buf;
	ulint		buf_size;
	ulint		start_page_no;
	ulint		file_start_page_no;
	ulint		offset_high;
	ulint		offset_low;
	ulint		page_size;
	ibool		success		= TRUE;

	/* file_extend_mutex is for http://bugs.mysql.com/56433 */
	/* to protect from the other fil_extend_space_to_desired_size() */
	/* during temprary releasing &fil_system->mutex */
	mutex_enter(&fil_system->file_extend_mutex);
	fil_mutex_enter_and_prepare_for_io(space_id);

	space = fil_space_get_by_id(space_id);
	ut_a(space);

	if (space->size >= size_after_extend) {
		/* Space already big enough */

		*actual_size = space->size;

		mutex_exit(&fil_system->mutex);
		mutex_exit(&fil_system->file_extend_mutex);

		return(TRUE);
	}

	page_size = dict_table_flags_to_zip_size(space->flags);
	if (!page_size) {
		page_size = UNIV_PAGE_SIZE;
	}

	node = UT_LIST_GET_LAST(space->chain);

	fil_node_prepare_for_io(node, fil_system, space);

	start_page_no = space->size;
	file_start_page_no = space->size - node->size;

	/* Extend at most 64 pages at a time */
	buf_size = ut_min(64, size_after_extend - start_page_no) * page_size;
	buf2 = mem_alloc(buf_size + page_size);
	buf = ut_align(buf2, page_size);

	memset(buf, 0, buf_size);

	while (start_page_no < size_after_extend) {
		ulint	n_pages = ut_min(buf_size / page_size,
					 size_after_extend - start_page_no);

		offset_high = (start_page_no - file_start_page_no)
			/ (4096 * ((1024 * 1024) / page_size));
		offset_low  = ((start_page_no - file_start_page_no)
			       % (4096 * ((1024 * 1024) / page_size)))
			* page_size;

		mutex_exit(&fil_system->mutex);
#ifdef UNIV_HOTBACKUP
		success = os_file_write(node->name, node->handle, buf,
					offset_low, offset_high,
					page_size * n_pages);
#else
		success = os_aio(OS_FILE_WRITE, OS_AIO_SYNC,
				 node->name, node->handle, buf,
				 offset_low, offset_high,
				 page_size * n_pages,
				 NULL, NULL, space_id, NULL);
#endif
		mutex_enter(&fil_system->mutex);

		if (success) {
			node->size += n_pages;
			space->size += n_pages;

			os_has_said_disk_full = FALSE;
		} else {
			/* Let us measure the size of the file to determine
			how much we were able to extend it */

			n_pages = ((ulint)
				   (os_file_get_size_as_iblonglong(
					   node->handle)
				    / page_size)) - node->size;

			node->size += n_pages;
			space->size += n_pages;

			break;
		}

		start_page_no += n_pages;
	}

	mem_free(buf2);

	fil_node_complete_io(node, fil_system, OS_FILE_WRITE);

	*actual_size = space->size;

#ifndef UNIV_HOTBACKUP
	if (space_id == 0) {
		ulint pages_per_mb = (1024 * 1024) / page_size;

		/* Keep the last data file size info up to date, rounded to
		full megabytes */

		srv_data_file_sizes[srv_n_data_files - 1]
			= (node->size / pages_per_mb) * pages_per_mb;
	}
#endif /* !UNIV_HOTBACKUP */

	/*
	printf("Extended %s to %lu, actual size %lu pages\n", space->name,
	size_after_extend, *actual_size); */
	mutex_exit(&fil_system->mutex);
	mutex_exit(&fil_system->file_extend_mutex);

	fil_flush(space_id, TRUE);

	return(success);
}

#ifdef UNIV_HOTBACKUP
/********************************************************************//**
Extends all tablespaces to the size stored in the space header. During the
ibbackup --apply-log phase we extended the spaces on-demand so that log records
could be applied, but that may have left spaces still too small compared to
the size stored in the space header. */
UNIV_INTERN
void
fil_extend_tablespaces_to_stored_len(void)
/*======================================*/
{
	fil_space_t*	space;
	byte*		buf;
	ulint		actual_size;
	ulint		size_in_header;
	ulint		error;
	ibool		success;

	buf = mem_alloc(UNIV_PAGE_SIZE);

	mutex_enter(&fil_system->mutex);

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	while (space) {
		ut_a(space->purpose == FIL_TABLESPACE);

		mutex_exit(&fil_system->mutex); /* no need to protect with a
					      mutex, because this is a
					      single-threaded operation */
		error = fil_read(TRUE, space->id,
				 dict_table_flags_to_zip_size(space->flags),
				 0, 0, UNIV_PAGE_SIZE, buf, NULL);
		ut_a(error == DB_SUCCESS);

		size_in_header = fsp_get_size_low(buf);

		success = fil_extend_space_to_desired_size(
			&actual_size, space->id, size_in_header);
		if (!success) {
			fprintf(stderr,
				"InnoDB: Error: could not extend the"
				" tablespace of %s\n"
				"InnoDB: to the size stored in header,"
				" %lu pages;\n"
				"InnoDB: size after extension %lu pages\n"
				"InnoDB: Check that you have free disk space"
				" and retry!\n",
				space->name, size_in_header, actual_size);
			exit(1);
		}

		mutex_enter(&fil_system->mutex);

		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&fil_system->mutex);

	mem_free(buf);
}
#endif

/*========== RESERVE FREE EXTENTS (for a B-tree split, for example) ===*/

/*******************************************************************//**
Tries to reserve free extents in a file space.
@return	TRUE if succeed */
UNIV_INTERN
ibool
fil_space_reserve_free_extents(
/*===========================*/
	ulint	id,		/*!< in: space id */
	ulint	n_free_now,	/*!< in: number of free extents now */
	ulint	n_to_reserve)	/*!< in: how many one wants to reserve */
{
	fil_space_t*	space;
	ibool		success;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	if (space->n_reserved_extents + n_to_reserve > n_free_now) {
		success = FALSE;
	} else {
		space->n_reserved_extents += n_to_reserve;
		success = TRUE;
	}

	mutex_exit(&fil_system->mutex);

	return(success);
}

/*******************************************************************//**
Releases free extents in a file space. */
UNIV_INTERN
void
fil_space_release_free_extents(
/*===========================*/
	ulint	id,		/*!< in: space id */
	ulint	n_reserved)	/*!< in: how many one reserved */
{
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);
	ut_a(space->n_reserved_extents >= n_reserved);

	space->n_reserved_extents -= n_reserved;

	mutex_exit(&fil_system->mutex);
}

/*******************************************************************//**
Gets the number of reserved extents. If the database is silent, this number
should be zero. */
UNIV_INTERN
ulint
fil_space_get_n_reserved_extents(
/*=============================*/
	ulint	id)		/*!< in: space id */
{
	fil_space_t*	space;
	ulint		n;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	n = space->n_reserved_extents;

	mutex_exit(&fil_system->mutex);

	return(n);
}

/*============================ FILE I/O ================================*/

/********************************************************************//**
NOTE: you must call fil_mutex_enter_and_prepare_for_io() first!

Prepares a file node for i/o. Opens the file if it is closed. Updates the
pending i/o's field in the node and the system appropriately. Takes the node
off the LRU list if it is in the LRU list. The caller must hold the fil_sys
mutex. */
static
void
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space)	/*!< in: space */
{
	ut_ad(node && system && space);
	ut_ad(mutex_own(&(system->mutex)));

	if (system->n_open > system->max_n_open + 5) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Warning: open files %lu"
			" exceeds the limit %lu\n",
			(ulong) system->n_open,
			(ulong) system->max_n_open);
	}

	if (node->open == FALSE) {
		/* File is closed: open it */
		ut_a(node->n_pending == 0);

		fil_node_open_file(node, system, space);
	}

	if (node->n_pending == 0 && space->purpose == FIL_TABLESPACE
	    && !trx_sys_sys_space(space->id)) {
		/* The node is in the LRU list, remove it */

		ut_a(UT_LIST_GET_LEN(system->LRU) > 0);

		UT_LIST_REMOVE(LRU, system->LRU, node);
	}

	node->n_pending++;
}

/********************************************************************//**
Updates the data structures when an i/o operation finishes. Updates the
pending i/o's field in the node appropriately. */
static
void
fil_node_complete_io(
/*=================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	ulint		type)	/*!< in: OS_FILE_WRITE or OS_FILE_READ; marks
				the node as modified if
				type == OS_FILE_WRITE */
{
	ut_ad(node);
	ut_ad(system);
	ut_ad(mutex_own(&(system->mutex)));

	ut_a(node->n_pending > 0);

	node->n_pending--;

	if (type == OS_FILE_WRITE) {
		system->modification_counter++;
		node->modification_counter = system->modification_counter;

		if (!node->space->is_in_unflushed_spaces) {

			node->space->is_in_unflushed_spaces = TRUE;
			UT_LIST_ADD_FIRST(unflushed_spaces,
					  system->unflushed_spaces,
					  node->space);
		}
	}

	if (node->n_pending == 0 && node->space->purpose == FIL_TABLESPACE
	    && !trx_sys_sys_space(node->space->id)) {
		/* The node must be put back to the LRU list */
		UT_LIST_ADD_FIRST(LRU, system->LRU, node);
	}
}

/********************************************************************//**
Report information about an invalid page access. */
static
void
fil_report_invalid_page_access(
/*===========================*/
	ulint		block_offset,	/*!< in: block offset */
	ulint		space_id,	/*!< in: space id */
	const char*	space_name,	/*!< in: space name */
	ulint		byte_offset,	/*!< in: byte offset */
	ulint		len,		/*!< in: I/O length */
	ulint		type)		/*!< in: I/O type */
{
	fprintf(stderr,
		"InnoDB: Error: trying to access page number %lu"
		" in space %lu,\n"
		"InnoDB: space name %s,\n"
		"InnoDB: which is outside the tablespace bounds.\n"
		"InnoDB: Byte offset %lu, len %lu, i/o type %lu.\n"
		"InnoDB: If you get this error at mysqld startup,"
		" please check that\n"
		"InnoDB: your my.cnf matches the ibdata files"
		" that you have in the\n"
		"InnoDB: MySQL server.\n",
		(ulong) block_offset, (ulong) space_id, space_name,
		(ulong) byte_offset, (ulong) len, (ulong) type);
}

/********************************************************************//**
Reads or writes data. This operation is asynchronous (aio).
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
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
	trx_t*	trx)
{
	ulint		mode;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		offset_high;
	ulint		offset_low;
	ibool		ret;
	ulint		is_log;
	ulint		wake_later;

	is_log = type & OS_FILE_LOG;
	type = type & ~OS_FILE_LOG;

	wake_later = type & OS_AIO_SIMULATED_WAKE_LATER;
	type = type & ~OS_AIO_SIMULATED_WAKE_LATER;

	ut_ad(byte_offset < UNIV_PAGE_SIZE);
	ut_ad(!zip_size || !byte_offset);
	ut_ad(ut_is_2pow(zip_size));
	ut_ad(buf);
	ut_ad(len > 0);
//#if (1 << UNIV_PAGE_SIZE_SHIFT) != UNIV_PAGE_SIZE
//# error "(1 << UNIV_PAGE_SIZE_SHIFT) != UNIV_PAGE_SIZE"
//#endif
	ut_ad(fil_validate());
#ifndef UNIV_HOTBACKUP
# ifndef UNIV_LOG_DEBUG
	/* ibuf bitmap pages must be read in the sync aio mode: */
	ut_ad(recv_no_ibuf_operations || (type == OS_FILE_WRITE)
	      || !ibuf_bitmap_page(zip_size, block_offset)
	      || sync || is_log);
	ut_ad(!ibuf_inside() || is_log || (type == OS_FILE_WRITE)
	      || ibuf_page(space_id, zip_size, block_offset, NULL));
# endif /* UNIV_LOG_DEBUG */
	if (sync) {
		mode = OS_AIO_SYNC;
	} else if (is_log) {
		mode = OS_AIO_LOG;
	} else if (type == OS_FILE_READ
		   && !recv_no_ibuf_operations
		   && ibuf_page(space_id, zip_size, block_offset, NULL)) {
		mode = OS_AIO_IBUF;
	} else {
		mode = OS_AIO_NORMAL;
	}
#else /* !UNIV_HOTBACKUP */
	ut_a(sync);
	mode = OS_AIO_SYNC;
#endif /* !UNIV_HOTBACKUP */

	if (type == OS_FILE_READ) {
		srv_data_read+= len;
	} else if (type == OS_FILE_WRITE) {
		srv_data_written+= len;
	}

	/* if the table space was already deleted, space might not exist already. */
	if (message
	    && space_id < SRV_LOG_SPACE_FIRST_ID
	    && ((buf_page_t*)message)->space_was_being_deleted) {

		if (mode == OS_AIO_NORMAL) {
			buf_page_io_complete(message);
			return(DB_SUCCESS); /*fake*/
		}
		if (type == OS_FILE_READ) {
			return(DB_TABLESPACE_DELETED);
		} else {
			return(DB_SUCCESS); /*fake*/
		}
	}

	/* Reserve the fil_system mutex and make sure that we can open at
	least one file while holding it, if the file is not already open */

	fil_mutex_enter_and_prepare_for_io(space_id);

	space = fil_space_get_by_id(space_id);

	if (!space) {
		mutex_exit(&fil_system->mutex);

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Error: trying to do i/o"
			" to a tablespace which does not exist.\n"
			"InnoDB: i/o type %lu, space id %lu,"
			" page no. %lu, i/o length %lu bytes\n",
			(ulong) type, (ulong) space_id, (ulong) block_offset,
			(ulong) len);

		return(DB_TABLESPACE_DELETED);
	}

	ut_ad((mode != OS_AIO_IBUF) || (space->purpose == FIL_TABLESPACE));

	node = UT_LIST_GET_FIRST(space->chain);

	for (;;) {
		if (UNIV_UNLIKELY(node == NULL)) {
			fil_report_invalid_page_access(
				block_offset, space_id, space->name,
				byte_offset, len, type);

			ut_error;
		}

		if (space->id != 0 && node->size == 0) {
			/* We do not know the size of a single-table tablespace
			before we open the file */

			break;
		}

		if (node->size > block_offset) {
			/* Found! */
			break;
		} else {
			block_offset -= node->size;
			node = UT_LIST_GET_NEXT(chain, node);
		}
	}

	/* Open file if closed */
	fil_node_prepare_for_io(node, fil_system, space);

	/* Check that at least the start offset is within the bounds of a
	single-table tablespace */
	if (UNIV_UNLIKELY(node->size <= block_offset)
	    && space->id != 0 && space->purpose == FIL_TABLESPACE) {

		fil_report_invalid_page_access(
			block_offset, space_id, space->name, byte_offset,
			len, type);

		ut_error;
	}

	/* Now we have made the changes in the data structures of fil_system */
	mutex_exit(&fil_system->mutex);

	/* Calculate the low 32 bits and the high 32 bits of the file offset */

	if (!zip_size) {
		offset_high = (block_offset >> (32 - UNIV_PAGE_SIZE_SHIFT));
		offset_low  = ((block_offset << UNIV_PAGE_SIZE_SHIFT)
			       & 0xFFFFFFFFUL) + byte_offset;

		ut_a(node->size - block_offset
		     >= ((byte_offset + len + (UNIV_PAGE_SIZE - 1))
			 / UNIV_PAGE_SIZE));
	} else {
		ulint	zip_size_shift;
		switch (zip_size) {
		case 1024: zip_size_shift = 10; break;
		case 2048: zip_size_shift = 11; break;
		case 4096: zip_size_shift = 12; break;
		case 8192: zip_size_shift = 13; break;
		case 16384: zip_size_shift = 14; break;
		default: ut_error;
		}
		offset_high = block_offset >> (32 - zip_size_shift);
		offset_low = (block_offset << zip_size_shift & 0xFFFFFFFFUL)
			+ byte_offset;
		ut_a(node->size - block_offset
		     >= (len + (zip_size - 1)) / zip_size);
	}

	/* Do aio */

	ut_a(byte_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a((len % OS_FILE_LOG_BLOCK_SIZE) == 0);

	if (srv_pass_corrupt_table == 1 && space->is_corrupt) {
		/* should ignore i/o for the crashed space */
		mutex_enter(&fil_system->mutex);
		fil_node_complete_io(node, fil_system, type);
		mutex_exit(&fil_system->mutex);
		if (mode == OS_AIO_NORMAL) {
			ut_a(space->purpose == FIL_TABLESPACE);
			buf_page_io_complete(message);
		}
		if (type == OS_FILE_READ) {
			return(DB_TABLESPACE_DELETED);
		} else {
			return(DB_SUCCESS);
		}
	} else {
		if (srv_pass_corrupt_table > 1 && space->is_corrupt) {
			/* should ignore write i/o for the crashed space */
			if (type == OS_FILE_WRITE) {
				mutex_enter(&fil_system->mutex);
				fil_node_complete_io(node, fil_system, type);
				mutex_exit(&fil_system->mutex);
				if (mode == OS_AIO_NORMAL) {
					ut_a(space->purpose == FIL_TABLESPACE);
					buf_page_io_complete(message);
				}
				return(DB_SUCCESS);
			}
		}
#ifdef UNIV_HOTBACKUP
	/* In ibbackup do normal i/o, not aio */
	if (type == OS_FILE_READ) {
		ret = os_file_read(node->handle, buf, offset_low, offset_high,
				   len);
	} else {
		ret = os_file_write(node->name, node->handle, buf,
				    offset_low, offset_high, len);
	}
#else
	/* Queue the aio request */
	ret = os_aio(type, mode | wake_later, node->name, node->handle, buf,
		     offset_low, offset_high, len, node, message, space_id, trx);
#endif
	} /**/

	/* if the table space was already deleted, space might not exist already. */
	if (message
	    && space_id < SRV_LOG_SPACE_FIRST_ID
	    && ((buf_page_t*)message)->space_was_being_deleted) {

		if (mode == OS_AIO_SYNC) {
			if (type == OS_FILE_READ) {
				return(DB_TABLESPACE_DELETED);
			} else {
				return(DB_SUCCESS); /*fake*/
			}
		}
	}

	ut_a(ret);

	if (mode == OS_AIO_SYNC) {
		/* The i/o operation is already completed when we return from
		os_aio: */

		mutex_enter(&fil_system->mutex);

		fil_node_complete_io(node, fil_system, type);

		mutex_exit(&fil_system->mutex);

		ut_ad(fil_validate());
	}

	return(DB_SUCCESS);
}

/********************************************************************//**
Confirm whether the parameters are valid or not */
UNIV_INTERN
ibool
fil_is_exist(
/*==============*/
	ulint	space_id,	/*!< in: space id */
	ulint	block_offset)	/*!< in: offset in number of blocks */
{
	fil_space_t*	space;
	fil_node_t*	node;

	/* Reserve the fil_system mutex and make sure that we can open at
	least one file while holding it, if the file is not already open */

	fil_mutex_enter_and_prepare_for_io(space_id);

	space = fil_space_get_by_id(space_id);

	if (!space) {
		mutex_exit(&fil_system->mutex);
		return(FALSE);
	}

	node = UT_LIST_GET_FIRST(space->chain);

	for (;;) {
		if (UNIV_UNLIKELY(node == NULL)) {
			mutex_exit(&fil_system->mutex);
			return(FALSE);
		}

		if (space->id != 0 && node->size == 0) {
			/* We do not know the size of a single-table tablespace
			before we open the file */

			break;
		}

		if (node->size > block_offset) {
			/* Found! */
			break;
		} else {
			block_offset -= node->size;
			node = UT_LIST_GET_NEXT(chain, node);
		}
	}

	/* Open file if closed */
	fil_node_prepare_for_io(node, fil_system, space);
	fil_node_complete_io(node, fil_system, OS_FILE_READ);

	/* Check that at least the start offset is within the bounds of a
	single-table tablespace */
	if (UNIV_UNLIKELY(node->size <= block_offset)
	    && space->id != 0 && space->purpose == FIL_TABLESPACE) {
		mutex_exit(&fil_system->mutex);
		return(FALSE);
	}

	mutex_exit(&fil_system->mutex);
	return(TRUE);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.c for more info). The thread specifies which
segment it wants to wait for. */
UNIV_INTERN
void
fil_aio_wait(
/*=========*/
	ulint	segment)	/*!< in: the number of the segment in the aio
				array to wait for */
{
	ibool		ret;
	fil_node_t*	fil_node;
	void*		message;
	ulint		type;
	ulint		space_id = 0;

	ut_ad(fil_validate());

	if (os_aio_use_native_aio) {
		srv_set_io_thread_op_info(segment, "native aio handle");
#ifdef WIN_ASYNC_IO
		ret = os_aio_windows_handle(segment, 0, &fil_node,
					    &message, &type, &space_id);
#else
		ret = 0; /* Eliminate compiler warning */
		ut_error;
#endif
	} else {
		srv_set_io_thread_op_info(segment, "simulated aio handle");

		ret = os_aio_simulated_handle(segment, &fil_node,
					      &message, &type, &space_id);
	}

	/* if the table space was already deleted, fil_node might not exist already. */
	if (message
	    && space_id < SRV_LOG_SPACE_FIRST_ID
	    && ((buf_page_t*)message)->space_was_being_deleted) {

		/* intended not to be uncompress read page */
		ut_a(buf_page_get_io_fix(message) == BUF_IO_WRITE
		     || !buf_page_get_zip_size(message)
		     || buf_page_get_state(message) != BUF_BLOCK_FILE_PAGE);

		srv_set_io_thread_op_info(segment, "complete io for buf page");
		buf_page_io_complete(message);
		return;
	}

	ut_a(ret);

	srv_set_io_thread_op_info(segment, "complete io for fil node");

	mutex_enter(&fil_system->mutex);

	fil_node_complete_io(fil_node, fil_system, type);

	mutex_exit(&fil_system->mutex);

	ut_ad(fil_validate());

	/* Do the i/o handling */
	/* IMPORTANT: since i/o handling for reads will read also the insert
	buffer in tablespace 0, you have to be very careful not to introduce
	deadlocks in the i/o system. We keep tablespace 0 data files always
	open, and use a special i/o thread to serve insert buffer requests. */

	if (fil_node->space->purpose == FIL_TABLESPACE) {
		srv_set_io_thread_op_info(segment, "complete io for buf page");
		buf_page_io_complete(message);
	} else {
		srv_set_io_thread_op_info(segment, "complete io for log");
		log_io_complete(message);
	}
}
#endif /* UNIV_HOTBACKUP */

/**********************************************************************//**
Flushes to disk possible writes cached by the OS. If the space does not exist
or is being dropped, does not do anything. */
UNIV_INTERN
void
fil_flush(
/*======*/
	ulint	space_id,	/*!< in: file space id (this can be a group of
				log files or a tablespace of the database) */
	ibool	metadata)
{
	fil_space_t*	space;
	fil_node_t*	node;
	os_file_t	file;
	ib_int64_t	old_mod_counter;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(space_id);

	if (!space || space->is_being_deleted) {
		mutex_exit(&fil_system->mutex);

		return;
	}

	space->n_pending_flushes++;	/*!< prevent dropping of the space while
					we are flushing */
	node = UT_LIST_GET_FIRST(space->chain);

	while (node) {
		if (node->modification_counter > node->flush_counter) {
			ut_a(node->open);

			/* We want to flush the changes at least up to
			old_mod_counter */
			old_mod_counter = node->modification_counter;

			if (space->purpose == FIL_TABLESPACE) {
				fil_n_pending_tablespace_flushes++;
			} else {
				fil_n_pending_log_flushes++;
				fil_n_log_flushes++;
			}
#ifdef __WIN__
			if (node->is_raw_disk) {

				goto skip_flush;
			}
#endif
retry:
			if (node->n_pending_flushes > 0) {
				/* We want to avoid calling os_file_flush() on
				the file twice at the same time, because we do
				not know what bugs OS's may contain in file
				i/o; sleep for a while */

				mutex_exit(&fil_system->mutex);

				os_thread_sleep(20000);

				mutex_enter(&fil_system->mutex);

				if (node->flush_counter >= old_mod_counter) {

					goto skip_flush;
				}

				goto retry;
			}

			ut_a(node->open);
			file = node->handle;
			node->n_pending_flushes++;

			mutex_exit(&fil_system->mutex);

			/* fprintf(stderr, "Flushing to file %s\n",
			node->name); */

			os_file_flush(file, metadata);

			mutex_enter(&fil_system->mutex);

			node->n_pending_flushes--;
skip_flush:
			if (node->flush_counter < old_mod_counter) {
				node->flush_counter = old_mod_counter;

				if (space->is_in_unflushed_spaces
				    && fil_space_is_flushed(space)) {

					space->is_in_unflushed_spaces = FALSE;

					UT_LIST_REMOVE(
						unflushed_spaces,
						fil_system->unflushed_spaces,
						space);
				}
			}

			if (space->purpose == FIL_TABLESPACE) {
				fil_n_pending_tablespace_flushes--;
			} else {
				fil_n_pending_log_flushes--;
			}
		}

		node = UT_LIST_GET_NEXT(chain, node);
	}

	space->n_pending_flushes--;

	mutex_exit(&fil_system->mutex);
}

/**********************************************************************//**
Flushes to disk the writes in file spaces of the given type possibly cached by
the OS. */
UNIV_INTERN
void
fil_flush_file_spaces(
/*==================*/
	ulint	purpose)	/*!< in: FIL_TABLESPACE, FIL_LOG */
{
	fil_space_t*	space;
	ulint*		space_ids;
	ulint		n_space_ids;
	ulint		i;

	mutex_enter(&fil_system->mutex);

	n_space_ids = UT_LIST_GET_LEN(fil_system->unflushed_spaces);
	if (n_space_ids == 0) {

		mutex_exit(&fil_system->mutex);
		return;
	}

	/* Assemble a list of space ids to flush.  Previously, we
	traversed fil_system->unflushed_spaces and called UT_LIST_GET_NEXT()
	on a space that was just removed from the list by fil_flush().
	Thus, the space could be dropped and the memory overwritten. */
	space_ids = mem_alloc(n_space_ids * sizeof *space_ids);

	n_space_ids = 0;

	for (space = UT_LIST_GET_FIRST(fil_system->unflushed_spaces);
	     space;
	     space = UT_LIST_GET_NEXT(unflushed_spaces, space)) {

		if (space->purpose == purpose && !space->is_being_deleted) {

			space_ids[n_space_ids++] = space->id;
		}
	}

	mutex_exit(&fil_system->mutex);

	/* Flush the spaces.  It will not hurt to call fil_flush() on
	a non-existing space id. */
	for (i = 0; i < n_space_ids; i++) {

		fil_flush(space_ids[i], TRUE);
	}

	mem_free(space_ids);
}

/******************************************************************//**
Checks the consistency of the tablespace cache.
@return	TRUE if ok */
UNIV_INTERN
ibool
fil_validate(void)
/*==============*/
{
	fil_space_t*	space;
	fil_node_t*	fil_node;
	ulint		n_open		= 0;
	ulint		i;

	mutex_enter(&fil_system->mutex);

	/* Look for spaces in the hash table */

	for (i = 0; i < hash_get_n_cells(fil_system->spaces); i++) {

		space = HASH_GET_FIRST(fil_system->spaces, i);

		while (space != NULL) {
			UT_LIST_VALIDATE(chain, fil_node_t, space->chain,
					 ut_a(ut_list_node_313->open
					      || !ut_list_node_313->n_pending));

			fil_node = UT_LIST_GET_FIRST(space->chain);

			while (fil_node != NULL) {
				if (fil_node->n_pending > 0) {
					ut_a(fil_node->open);
				}

				if (fil_node->open) {
					n_open++;
				}
				fil_node = UT_LIST_GET_NEXT(chain, fil_node);
			}
			space = HASH_GET_NEXT(hash, space);
		}
	}

	ut_a(fil_system->n_open == n_open);

	UT_LIST_VALIDATE(LRU, fil_node_t, fil_system->LRU, (void) 0);

	fil_node = UT_LIST_GET_FIRST(fil_system->LRU);

	while (fil_node != NULL) {
		ut_a(fil_node->n_pending == 0);
		ut_a(fil_node->open);
		ut_a(fil_node->space->purpose == FIL_TABLESPACE);
		ut_a(!trx_sys_sys_space(fil_node->space->id));

		fil_node = UT_LIST_GET_NEXT(LRU, fil_node);
	}

	mutex_exit(&fil_system->mutex);

	return(TRUE);
}

/********************************************************************//**
Returns TRUE if file address is undefined.
@return	TRUE if undefined */
UNIV_INTERN
ibool
fil_addr_is_null(
/*=============*/
	fil_addr_t	addr)	/*!< in: address */
{
	return(addr.page == FIL_NULL);
}

/********************************************************************//**
Get the predecessor of a file page.
@return	FIL_PAGE_PREV */
UNIV_INTERN
ulint
fil_page_get_prev(
/*==============*/
	const byte*	page)	/*!< in: file page */
{
	return(mach_read_from_4(page + FIL_PAGE_PREV));
}

/********************************************************************//**
Get the successor of a file page.
@return	FIL_PAGE_NEXT */
UNIV_INTERN
ulint
fil_page_get_next(
/*==============*/
	const byte*	page)	/*!< in: file page */
{
	return(mach_read_from_4(page + FIL_PAGE_NEXT));
}

/*********************************************************************//**
Sets the file page type. */
UNIV_INTERN
void
fil_page_set_type(
/*==============*/
	byte*	page,	/*!< in/out: file page */
	ulint	type)	/*!< in: type */
{
	ut_ad(page);

	mach_write_to_2(page + FIL_PAGE_TYPE, type);
}

/*********************************************************************//**
Gets the file page type.
@return type; NOTE that if the type has not been written to page, the
return value not defined */
UNIV_INTERN
ulint
fil_page_get_type(
/*==============*/
	const byte*	page)	/*!< in: file page */
{
	ut_ad(page);

	return(mach_read_from_2(page + FIL_PAGE_TYPE));
}

/****************************************************************//**
Initializes the tablespace memory cache. */
UNIV_INTERN
void
fil_close(void)
/*===========*/
{
#ifndef UNIV_HOTBACKUP
	/* The mutex should already have been freed. */
	ut_ad(fil_system->mutex.magic_n == 0);
#endif /* !UNIV_HOTBACKUP */

	hash_table_free(fil_system->spaces);

	hash_table_free(fil_system->name_hash);

	ut_a(UT_LIST_GET_LEN(fil_system->LRU) == 0);
	ut_a(UT_LIST_GET_LEN(fil_system->unflushed_spaces) == 0);
	ut_a(UT_LIST_GET_LEN(fil_system->space_list) == 0);

	mem_free(fil_system);

	fil_system = NULL;
}

/*************************************************************************
Return local hash table informations. */

ulint
fil_system_hash_cells(void)
/*=======================*/
{
       if (fil_system) {
               return (fil_system->spaces->n_cells
                       + fil_system->name_hash->n_cells);
       } else {
               return 0;
       }
}

ulint
fil_system_hash_nodes(void)
/*=======================*/
{
       if (fil_system) {
               return (UT_LIST_GET_LEN(fil_system->space_list)
                       * (sizeof(fil_space_t) + MEM_BLOCK_HEADER_SIZE));
       } else {
               return 0;
       }
}

/*************************************************************************
functions to access is_corrupt flag of fil_space_t*/

ibool
fil_space_is_corrupt(
/*=================*/
	ulint	space_id)
{
	fil_space_t*	space;
	ibool		ret = FALSE;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(space_id);

	if (space && space->is_corrupt) {
		ret = TRUE;
	}

	mutex_exit(&fil_system->mutex);

	return(ret);
}

void
fil_space_set_corrupt(
/*==================*/
	ulint	space_id)
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(space_id);

	if (space) {
		space->is_corrupt = TRUE;
	}

	mutex_exit(&fil_system->mutex);
}

