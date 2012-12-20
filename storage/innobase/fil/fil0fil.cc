/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file fil/fil0fil.cc
The tablespace memory cache

Created 10/25/1995 Heikki Tuuri
*******************************************************/

#include "fil0fil.h"

#include <debug_sync.h>
#include <my_dbug.h>

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
#include "trx0sys.h"
#include "row0mysql.h"
#ifndef UNIV_HOTBACKUP
# include "buf0lru.h"
# include "ibuf0ibuf.h"
# include "sync0sync.h"
# include "os0sync.h"
#else /* !UNIV_HOTBACKUP */
# include "srv0srv.h"
static ulint srv_data_read, srv_data_written;
#endif /* !UNIV_HOTBACKUP */
#include "srv0space.h"

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

/** Number of files currently open */
UNIV_INTERN ulint	fil_n_file_opened			= 0;

/** The null file address */
UNIV_INTERN fil_addr_t	fil_addr_null = {FIL_NULL, 0};

#ifdef UNIV_PFS_MUTEX
/* Key to register fil_system_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	fil_system_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#ifdef UNIV_PFS_RWLOCK
/* Key to register file space latch with performance schema */
UNIV_INTERN mysql_pfs_key_t	fil_space_latch_key;
#endif /* UNIV_PFS_RWLOCK */

/** File node of a tablespace or the log data space */
struct fil_node_t {
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
	ibool		being_extended;
				/*!< TRUE if the node is currently
				being extended. */
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

/** Value of fil_node_t::magic_n */
#define	FIL_NODE_MAGIC_N	89389

/** Tablespace or log data space: let us call them by a common name space */
struct fil_space_t {
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
				deleting a single-table tablespace.
				When this is set following new ops
				are not allowed:
				* read IO request
				* ibuf merge
				* file flush
				Note that we can still possibly have
				new write operations because we don't
				check this flag when doing flush
				batches. */
	ulint		purpose;/*!< FIL_TABLESPACE, FIL_LOG, or
				FIL_ARCH_LOG */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;
				/*!< base node for the file chain */
	ulint		size;	/*!< space size in pages; 0 if a single-table
				tablespace whose size we do not know yet;
				last incomplete megabytes in data files may be
				ignored if space == 0 */
	ulint		flags;	/*!< tablespace flags; see
				fsp_flags_is_valid(),
				fsp_flags_get_zip_size() */
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
	bool		is_in_unflushed_spaces;
				/*!< true if this space is currently in
				unflushed_spaces */
	UT_LIST_NODE_T(fil_space_t) space_list;
				/*!< list of all spaces */
	ulint		magic_n;/*!< FIL_SPACE_MAGIC_N */
};

/** Value of fil_space_t::magic_n */
#define	FIL_SPACE_MAGIC_N	89472

/** The tablespace memory cache; also the totality of logs (the log
data space) is stored here; below we talk about tablespaces, but also
the ib_logfiles form a 'space' and it is handled here */
struct fil_system_t {
#ifndef UNIV_HOTBACKUP
	ib_mutex_t		mutex;		/*!< The mutex protecting the cache */
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

/** Determine if (i) is a user tablespace id or not. */
# define fil_is_user_tablespace_id(i) ((i) > srv_undo_tablespaces_open)

/** Determine if user has explicitly disabled fsync(). */
#ifndef __WIN__
# define fil_buffering_disabled(s)	\
	((s)->purpose == FIL_TABLESPACE	\
	 && srv_unix_file_flush_method	\
	 == SRV_UNIX_O_DIRECT_NO_FSYNC)
#else /* __WIN__ */
# define fil_buffering_disabled(s)	(0)
#endif /* __WIN__ */

#ifdef UNIV_DEBUG
/** Try fil_validate() every this many times */
# define FIL_VALIDATE_SKIP	17

/******************************************************************//**
Checks the consistency of the tablespace cache some of the time.
@return	TRUE if ok or the check was skipped */
static
ibool
fil_validate_skip(void)
/*===================*/
{
	/** The fil_validate() call skip counter. Use a signed type
	because of the race condition below. */
	static int fil_validate_count = FIL_VALIDATE_SKIP;

	/* There is a race condition below, but it does not matter,
	because this call is only for heuristic purposes. We want to
	reduce the call frequency of the costly fil_validate() check
	in debug builds. */
	if (--fil_validate_count > 0) {
		return(TRUE);
	}

	fil_validate_count = FIL_VALIDATE_SKIP;
	return(fil_validate());
}
#endif /* UNIV_DEBUG */

/********************************************************************//**
Determines if a file node belongs to the least-recently-used list.
@return TRUE if the file belongs to fil_system->LRU mutex. */
UNIV_INLINE
ibool
fil_space_belongs_in_lru(
/*=====================*/
	const fil_space_t*	space)	/*!< in: file space */
{
	return(space->purpose == FIL_TABLESPACE
	       && fil_is_user_tablespace_id(space->id));
}

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
dberr_t
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
dberr_t
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
	ut_ad(!srv_read_only_mode);

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
@return	true if all are flushed */
static
bool
fil_space_is_flushed(
/*=================*/
	fil_space_t*	space)	/*!< in: space */
{
	fil_node_t*	node;

	ut_ad(mutex_own(&fil_system->mutex));

	node = UT_LIST_GET_FIRST(space->chain);

	while (node) {
		if (node->modification_counter > node->flush_counter) {

			ut_ad(!fil_buffering_disabled(space));
			return(false);
		}

		node = UT_LIST_GET_NEXT(chain, node);
	}

	return(true);
}

/*******************************************************************//**
Appends a new file to the chain of files of a space. File must be closed.
@return pointer to the file name, or NULL on error */
UNIV_INTERN
char*
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

	node = static_cast<fil_node_t*>(mem_zalloc(sizeof(fil_node_t)));

	node->name = mem_strdup(name);

	ut_a(!is_raw || srv_start_raw_disk_in_use);

	node->is_raw_disk = is_raw;
	node->size = size;
	node->magic_n = FIL_NODE_MAGIC_N;

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

		return(NULL);
	}

	space->size += size;

	node->space = space;

	UT_LIST_ADD_LAST(chain, space->chain, node);

	if (id < SRV_LOG_SPACE_FIRST_ID && fil_system->max_assigned_id < id) {

		fil_system->max_assigned_id = id;
	}

	mutex_exit(&fil_system->mutex);

	return(node->name);
}

/********************************************************************//**
Opens a file of a node of a tablespace. The caller must own the fil_system
mutex. */
static
void
fil_node_open_file(
/*===============*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space)	/*!< in: space */
{
	os_offset_t	size_bytes;
	ibool		ret;
	ibool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ulint		flags;
	ulint		page_size;

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
			innodb_file_data_key, node->name, OS_FILE_OPEN,
			OS_FILE_READ_ONLY, &success);
		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(true);

			ut_print_timestamp(stderr);

			fprintf(stderr,
				"  InnoDB: Fatal error: cannot open %s\n."
				"InnoDB: Have you deleted .ibd files"
				" under a running mysqld server?\n",
				node->name);
			ut_a(0);
		}

		size_bytes = os_file_get_size(node->handle);
		ut_a(size_bytes != (os_offset_t) -1);
#ifdef UNIV_HOTBACKUP
		if (space->id == 0) {
			node->size = (ulint) (size_bytes / UNIV_PAGE_SIZE);
			os_file_close(node->handle);
			goto add_size;
		}
#endif /* UNIV_HOTBACKUP */
		ut_a(space->purpose != FIL_LOG);
		ut_a(fil_is_user_tablespace_id(space->id));

		if (size_bytes < FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE) {
			fprintf(stderr,
				"InnoDB: Error: the size of single-table"
				" tablespace file %s\n"
				"InnoDB: is only "UINT64PF","
				" should be at least %lu!\n",
				node->name,
				size_bytes,
				(ulong) (FIL_IBD_FILE_INITIAL_SIZE
					 * UNIV_PAGE_SIZE));

			ut_a(0);
		}

		/* Read the first page of the tablespace */

		buf2 = static_cast<byte*>(ut_malloc(2 * UNIV_PAGE_SIZE));
		/* Align the memory for file i/o if we might have O_DIRECT
		set */
		page = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

		success = os_file_read(node->handle, page, 0, UNIV_PAGE_SIZE);
		space_id = fsp_header_get_space_id(page);
		flags = fsp_header_get_flags(page);
		page_size = fsp_flags_get_page_size(flags);

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
				  || space_id == 0)) {
			fprintf(stderr,
				"InnoDB: Error: tablespace id %lu"
				" in file %s is not sensible\n",
				(ulong) space_id, node->name);

			ut_error;
		}

		if (UNIV_UNLIKELY(fsp_flags_get_page_size(space->flags)
				  != page_size)) {
			fprintf(stderr,
				"InnoDB: Error: tablespace file %s"
				" has page size 0x%lx\n"
				"InnoDB: but the data dictionary"
				" expects page size 0x%lx!\n",
				node->name, flags,
				fsp_flags_get_page_size(space->flags));

			ut_error;
		}

		if (UNIV_UNLIKELY(space->flags != flags)) {
			fprintf(stderr,
				"InnoDB: Error: table flags are 0x%lx"
				" in the data dictionary\n"
				"InnoDB: but the flags in file %s are 0x%lx!\n",
				space->flags, node->name, flags);

			ut_error;
		}

		if (size_bytes >= 1024 * 1024) {
			/* Truncate the size to whole megabytes. */
			size_bytes = ut_2pow_round(size_bytes, 1024 * 1024);
		}

		if (!fsp_flags_is_compressed(flags)) {
			node->size = (ulint) (size_bytes / UNIV_PAGE_SIZE);
		} else {
			node->size = (ulint)
				(size_bytes
				 / fsp_flags_get_zip_size(flags));
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
		node->handle = os_file_create(innodb_file_log_key,
					      node->name, OS_FILE_OPEN,
					      OS_FILE_AIO, OS_LOG_FILE,
					      &ret);
	} else if (node->is_raw_disk) {
		node->handle = os_file_create(innodb_file_data_key,
					      node->name,
					      OS_FILE_OPEN_RAW,
					      OS_FILE_AIO, OS_DATA_FILE,
						     &ret);
	} else {
		node->handle = os_file_create(innodb_file_data_key,
					      node->name, OS_FILE_OPEN,
					      OS_FILE_AIO, OS_DATA_FILE,
					      &ret);
	}

	ut_a(ret);

	node->open = TRUE;

	system->n_open++;
	fil_n_file_opened++;

	if (fil_space_belongs_in_lru(space)) {

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
	ut_a(node->n_pending == 0);
	ut_a(node->n_pending_flushes == 0);
	ut_a(!node->being_extended);
#ifndef UNIV_HOTBACKUP
	ut_a(node->modification_counter == node->flush_counter
	     || srv_fast_shutdown == 2);
#endif /* !UNIV_HOTBACKUP */

	ret = os_file_close(node->handle);
	ut_a(ret);

	/* printf("Closing file %s\n", node->name); */

	node->open = FALSE;
	ut_a(system->n_open > 0);
	system->n_open--;
	fil_n_file_opened--;

	if (fil_space_belongs_in_lru(node->space)) {

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

	if (print_info) {
		fprintf(stderr,
			"InnoDB: fil_sys open file LRU len %lu\n",
			(ulong) UT_LIST_GET_LEN(fil_system->LRU));
	}

	for (node = UT_LIST_GET_LAST(fil_system->LRU);
	     node != NULL;
	     node = UT_LIST_GET_PREV(LRU, node)) {

		if (node->modification_counter == node->flush_counter
		    && node->n_pending_flushes == 0
		    && !node->being_extended) {

			fil_node_close_file(node, fil_system);

			return(TRUE);
		}

		if (!print_info) {
			continue;
		}

		if (node->n_pending_flushes > 0) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr, ", because n_pending_flushes %lu\n",
				(ulong) node->n_pending_flushes);
		}

		if (node->modification_counter != node->flush_counter) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr,
				", because mod_count %ld != fl_count %ld\n",
				(long) node->modification_counter,
				(long) node->flush_counter);

		}

		if (node->being_extended) {
			fputs("InnoDB: cannot close file ", stderr);
			ut_print_filename(stderr, node->name);
			fprintf(stderr, ", because it is being extended\n");
		}
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

	if (space_id == 0 || space_id >= SRV_LOG_SPACE_FIRST_ID) {
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
	ut_a(node->n_pending == 0);
	ut_a(!node->being_extended);

	if (node->open) {
		/* We fool the assertion in fil_node_close_file() to think
		there are no unflushed modifications in the file */

		node->modification_counter = node->flush_counter;

		if (fil_buffering_disabled(space)) {

			ut_ad(!space->is_in_unflushed_spaces);
			ut_ad(fil_space_is_flushed(space));

		} else if (space->is_in_unflushed_spaces
			   && fil_space_is_flushed(space)) {

			space->is_in_unflushed_spaces = false;

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
Creates a space memory object and puts it to the 'fil system' hash table.
If there is an error, prints an error message to the .err log.
@return	TRUE if success */
UNIV_INTERN
ibool
fil_space_create(
/*=============*/
	const char*	name,	/*!< in: space name */
	ulint		id,	/*!< in: space id */
	ulint		flags,	/*!< in: tablespace flags */
	ulint		purpose)/*!< in: FIL_TABLESPACE, or FIL_LOG if log */
{
	fil_space_t*	space;

	DBUG_EXECUTE_IF("fil_space_create_failure", return(false););

	ut_a(fil_system);
	ut_a(fsp_flags_is_valid(flags));

	/* Look for a matching tablespace and if found free it. */
	do {
		mutex_enter(&fil_system->mutex);

		space = fil_space_get_by_name(name);

		if (space != 0) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Tablespace '%s' exists in the cache "
				"with id %lu", name, (ulong) id);

			if (Tablespace::is_system_tablespace(id)
			    || purpose != FIL_TABLESPACE) {

				mutex_exit(&fil_system->mutex);

				return(FALSE);
			}

			ib_logf(IB_LOG_LEVEL_WARN,
				"Freeing existing tablespace '%s' entry "
				"from the cache with id %lu",
				name, (ulong) id);

			ibool	success = fil_space_free(space->id, FALSE);
			ut_a(success);

			mutex_exit(&fil_system->mutex);
		}

	} while (space != 0);

	space = fil_space_get_by_id(id);

	if (space != 0) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Trying to add tablespace '%s' with id %lu "
			"to the tablespace memory cache, but tablespace '%s' "
			"with id %lu already exists in the cache!",
			name, (ulong) id, space->name, (ulong) space->id);

		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	space = static_cast<fil_space_t*>(mem_zalloc(sizeof(*space)));

	space->name = mem_strdup(name);
	space->id = id;

	fil_system->tablespace_version++;
	space->tablespace_version = fil_system->tablespace_version;
	space->mark = FALSE;

	if (purpose == FIL_TABLESPACE && !recv_recovery_on
	    && id > fil_system->max_assigned_id) {

		if (!fil_system->space_id_reuse_warned) {
			fil_system->space_id_reuse_warned = TRUE;

			ib_logf(IB_LOG_LEVEL_WARN,
				"Allocated tablespace %lu, old maximum "
				"was %lu",
				(ulong) id,
				(ulong) fil_system->max_assigned_id);
		}

		fil_system->max_assigned_id = id;
	}

	space->purpose = purpose;
	space->flags = flags;

	space->magic_n = FIL_SPACE_MAGIC_N;

	rw_lock_create(fil_space_latch_key, &space->latch, SYNC_FSP);

	HASH_INSERT(fil_space_t, hash, fil_system->spaces, id, space);

	HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(name), space);
	space->is_in_unflushed_spaces = false;

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

	success = (id < SRV_LOG_SPACE_FIRST_ID);

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
	fil_space_t*	fnamespace;

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

	fnamespace = fil_space_get_by_name(space->name);
	ut_a(fnamespace);
	ut_a(space == fnamespace);

	HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(space->name), space);

	if (space->is_in_unflushed_spaces) {

		ut_ad(!fil_buffering_disabled(space));
		space->is_in_unflushed_spaces = false;

		UT_LIST_REMOVE(unflushed_spaces, fil_system->unflushed_spaces,
			       space);
	}

	UT_LIST_REMOVE(space_list, fil_system->space_list, space);

	ut_a(space->magic_n == FIL_SPACE_MAGIC_N);
	ut_a(0 == space->n_pending_flushes);

	for (fil_node_t* fil_node = UT_LIST_GET_FIRST(space->chain);
	     fil_node != NULL;
	     fil_node = UT_LIST_GET_FIRST(space->chain)) {

		fil_node_free(fil_node, fil_system, space);
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
Returns a pointer to the file_space_t that is in the memory cache
associated with a space id. The caller must lock fil_system->mutex.
@return	file_space_t pointer, NULL if space not found */
UNIV_INLINE
fil_space_t*
fil_space_get_space(
/*================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;
	fil_node_t*	node;

	ut_ad(fil_system);

	space = fil_space_get_by_id(id);
	if (space == NULL) {
		return(NULL);
	}

	if (space->size == 0 && space->purpose == FIL_TABLESPACE) {
		ut_a(id != 0);

		mutex_exit(&fil_system->mutex);

		/* It is possible that the space gets evicted at this point
		before the fil_mutex_enter_and_prepare_for_io() acquires
		the fil_system->mutex. Check for this after completing the
		call to fil_mutex_enter_and_prepare_for_io(). */
		fil_mutex_enter_and_prepare_for_io(id);

		/* We are still holding the fil_system->mutex. Check if
		the space is still in memory cache. */
		space = fil_space_get_by_id(id);
		if (space == NULL) {
			return(NULL);
		}

		/* The following code must change when InnoDB supports
		multiple datafiles per tablespace. */
		ut_a(1 == UT_LIST_GET_LEN(space->chain));

		node = UT_LIST_GET_FIRST(space->chain);

		/* It must be a single-table tablespace and we have not opened
		the file yet; the following calls will open it and update the
		size fields */

		fil_node_prepare_for_io(node, fil_system, space);
		fil_node_complete_io(node, fil_system, OS_FILE_READ);
	}

	return(space);
}

/*******************************************************************//**
Returns the path from the first fil_node_t found for the space ID sent.
The caller is responsible for freeing the memory allocated here for the
value returned.
@return	own: A copy of fil_node_t::path, NULL if space ID is zero
or not found. */
UNIV_INTERN
char*
fil_space_get_first_path(
/*=====================*/
	ulint		id)	/*!< in: space id */
{
	fil_space_t*	space;
	fil_node_t*	node;
	char*		path;

	ut_ad(fil_system);
	ut_a(id);

	fil_mutex_enter_and_prepare_for_io(id);

	space = fil_space_get_space(id);

	if (space == NULL) {
		mutex_exit(&fil_system->mutex);

		return(NULL);
	}

	ut_ad(mutex_own(&fil_system->mutex));

	node = UT_LIST_GET_FIRST(space->chain);

	path = mem_strdup(node->name);

	mutex_exit(&fil_system->mutex);

	return(path);
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
	fil_space_t*	space;
	ulint		size;

	ut_ad(fil_system);
	mutex_enter(&fil_system->mutex);

	space = fil_space_get_space(id);

	size = space ? space->size : 0;

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
	fil_space_t*	space;
	ulint		flags;

	ut_ad(fil_system);

	if (!id) {
		return(0);
	}

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_space(id);

	if (space == NULL) {
		mutex_exit(&fil_system->mutex);

		return(ULINT_UNDEFINED);
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

		return(fsp_flags_get_zip_size(flags));
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

	fil_system = static_cast<fil_system_t*>(
		mem_zalloc(sizeof(fil_system_t)));

	mutex_create(fil_system_mutex_key,
		     &fil_system->mutex, SYNC_ANY_LATCH);

	fil_system->spaces = hash_create(hash_size);
	fil_system->name_hash = hash_create(hash_size);

	UT_LIST_INIT(fil_system->LRU);

	fil_system->max_n_open = max_n_open;
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

	mutex_enter(&fil_system->mutex);

	for (space = UT_LIST_GET_FIRST(fil_system->space_list);
	     space != NULL;
	     space = UT_LIST_GET_NEXT(space_list, space)) {

		fil_node_t*	node;

		if (fil_space_belongs_in_lru(space)) {

			continue;
		}

		for (node = UT_LIST_GET_FIRST(space->chain);
		     node != NULL;
		     node = UT_LIST_GET_NEXT(chain, node)) {

			if (!node->open) {
				fil_node_open_file(node, fil_system, space);
			}

			if (fil_system->max_n_open < 10 + fil_system->n_open) {

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
		}
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
Closes the redo log files. There must not be any pending i/o's or not
flushed modifications in the files. */
UNIV_INTERN
void
fil_close_log_files(
/*================*/
	bool	free)	/*!< in: whether to free the memory object */
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = UT_LIST_GET_FIRST(fil_system->space_list);

	while (space != NULL) {
		fil_node_t*	node;
		fil_space_t*	prev_space = space;

		if (space->purpose != FIL_LOG) {
			space = UT_LIST_GET_NEXT(space_list, space);
			continue;
		}

		for (node = UT_LIST_GET_FIRST(space->chain);
		     node != NULL;
		     node = UT_LIST_GET_NEXT(chain, node)) {

			if (node->open) {
				fil_node_close_file(node, fil_system);
			}
		}

		space = UT_LIST_GET_NEXT(space_list, space);

		if (free) {
			fil_space_free(prev_space->id, FALSE);
		}
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
static __attribute__((warn_unused_result))
dberr_t
fil_write_lsn_and_arch_no_to_file(
/*==============================*/
	ulint	space,		/*!< in: space to write to */
	ulint	sum_of_sizes,	/*!< in: combined size of previous files
				in space, in database pages */
	lsn_t	lsn,		/*!< in: lsn to write */
	ulint	arch_log_no __attribute__((unused)))
				/*!< in: archived log number to write */
{
	byte*	buf1;
	byte*	buf;
	dberr_t	err;

	buf1 = static_cast<byte*>(mem_alloc(2 * UNIV_PAGE_SIZE));
	buf = static_cast<byte*>(ut_align(buf1, UNIV_PAGE_SIZE));

	err = fil_read(TRUE, space, 0, sum_of_sizes, 0,
		       UNIV_PAGE_SIZE, buf, NULL);
	if (err == DB_SUCCESS) {
		mach_write_to_8(buf + FIL_PAGE_FILE_FLUSH_LSN, lsn);

		err = fil_write(TRUE, space, 0, sum_of_sizes, 0,
				UNIV_PAGE_SIZE, buf, NULL);
	}

	mem_free(buf1);

	return(err);
}

/****************************************************************//**
Writes the flushed lsn and the latest archived log number to the page
header of the first page of each data file in the system tablespace.
@return	DB_SUCCESS or error number */
UNIV_INTERN
dberr_t
fil_write_flushed_lsn_to_data_files(
/*================================*/
	lsn_t	lsn,		/*!< in: lsn to write */
	ulint	arch_log_no)	/*!< in: latest archived log file number */
{
	fil_space_t*	space;
	fil_node_t*	node;
	dberr_t		err;

	mutex_enter(&fil_system->mutex);

	for (space = UT_LIST_GET_FIRST(fil_system->space_list);
	     space != NULL;
	     space = UT_LIST_GET_NEXT(space_list, space)) {

		/* We only write the lsn to all existing data files which have
		been open during the lifetime of the mysqld process; they are
		represented by the space objects in the tablespace memory
		cache. Note that all data files in the system tablespace 0
		and the UNDO log tablespaces (if separate) are always open. */

		if (space->purpose == FIL_TABLESPACE
		    && !fil_is_user_tablespace_id(space->id)) {
			ulint	sum_of_sizes = 0;

			for (node = UT_LIST_GET_FIRST(space->chain);
			     node != NULL;
			     node = UT_LIST_GET_NEXT(chain, node)) {

				mutex_exit(&fil_system->mutex);

				err = fil_write_lsn_and_arch_no_to_file(
					space->id, sum_of_sizes, lsn,
					arch_log_no);

				if (err != DB_SUCCESS) {

					return(err);
				}

				mutex_enter(&fil_system->mutex);

				sum_of_sizes += node->size;
			}
		}
	}

	mutex_exit(&fil_system->mutex);

	return(DB_SUCCESS);
}

/*******************************************************************//**
Reads the flushed lsn and tablespace flag fields from a data
file at database startup. */
UNIV_INTERN
void
fil_read_first_page(
/*================*/
	os_file_t	data_file,		/*!< in: open data file */
	ulint*		flags,			/*!< out: tablespace flags */
	ulint*		space_id,		/*!< out: tablespace ID */
	lsn_t*		min_flushed_lsn,	/*!< out: min of flushed
						lsn values in data files */
	lsn_t*		max_flushed_lsn)	/*!< out: max of flushed
						lsn values in data files */
{
	byte*	buf = static_cast<byte*>(ut_malloc(2 * UNIV_PAGE_SIZE));

	/* Align the memory for a possible read from a raw device */

	byte*	page = static_cast<byte*>(ut_align(buf, UNIV_PAGE_SIZE));

	os_file_read(data_file, page, 0, UNIV_PAGE_SIZE);

	*flags = fsp_header_get_flags(page);

	*space_id = fsp_header_get_space_id(page);

	lsn_t	flushed_lsn = mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN);

	ut_free(buf);

	if (*min_flushed_lsn > flushed_lsn) {
		*min_flushed_lsn = flushed_lsn;
	}

	if (*max_flushed_lsn < flushed_lsn) {
		*max_flushed_lsn = flushed_lsn;
	}
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
	path = static_cast<char*>(mem_alloc(len + (namend - name) + 2));

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

InnoDB recovery does not replay these fully since it always sets the space id
to zero. But ibbackup does replay them.  TODO: If remote tablespaces are used,
ibbackup will only create tables in the default directory since MLOG_FILE_CREATE
and MLOG_FILE_CREATE2 only know the tablename, not the path.

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
	fil0fil.cc data structures.

	NOTE that our algorithm is not guaranteed to work correctly if there
	were renames of tables during the backup. See ibbackup code for more
	on the problem. */

	switch (type) {
	case MLOG_FILE_DELETE:
		if (fil_tablespace_exists_in_mem(space_id)) {
			dberr_t	err = fil_delete_tablespace(
				space_id, BUF_REMOVE_FLUSH_NO_WRITE);
			ut_a(err == DB_SUCCESS);
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
				/* We do not care about the old name, that
				is why we pass NULL as the first argument. */
				if (!fil_rename_tablespace(NULL, space_id,
							   new_name, NULL)) {
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
			const char*	path = NULL;

			/* Create the database directory for name, if it does
			not exist yet */
			fil_create_directory_for_tablename(name);

			if (fil_create_new_single_table_tablespace(
				    space_id, name, path, flags,
				    DICT_TF2_USE_TABLESPACE,
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
Allocates a file name for the EXPORT/IMPORT config file name.  The
string must be freed by caller with mem_free().
@return own: file name */
static
char*
fil_make_cfg_name(
/*==============*/
	const char*	filepath)	/*!< in: .ibd file name */
{
	char*	cfg_name;

	/* Create a temporary file path by replacing the .ibd suffix
	with .cfg. */

	ut_ad(strlen(filepath) > 4);

	cfg_name = mem_strdup(filepath);
	ut_snprintf(cfg_name + strlen(cfg_name) - 3, 4, "cfg");
	return(cfg_name);
}

/*******************************************************************//**
Check for change buffer merges.
@return 0 if no merges else count + 1. */
static
ulint
fil_ibuf_check_pending_ops(
/*=======================*/
	fil_space_t*	space,	/*!< in/out: Tablespace to check */
	ulint		count)	/*!< in: number of attempts so far */
{
	ut_ad(mutex_own(&fil_system->mutex));

	if (space != 0 && space->n_pending_ops != 0) {

		if (count > 5000) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Trying to close/delete tablespace "
				"'%s' but there are %lu pending change "
				"buffer merges on it.",
				space->name,
				(ulong) space->n_pending_ops);
		}

		return(count + 1);
	}

	return(0);
}

/*******************************************************************//**
Check for pending IO.
@return 0 if no pending else count + 1. */
static
ulint
fil_check_pending_io(
/*=================*/
	fil_space_t*	space,	/*!< in/out: Tablespace to check */
	fil_node_t**	node,	/*!< out: Node in space list */
	ulint		count)	/*!< in: number of attempts so far */
{
	ut_ad(mutex_own(&fil_system->mutex));
	ut_a(space->n_pending_ops == 0);

	/* The following code must change when InnoDB supports
	multiple datafiles per tablespace. */
	ut_a(UT_LIST_GET_LEN(space->chain) == 1);

	*node = UT_LIST_GET_FIRST(space->chain);

	if (space->n_pending_flushes > 0 || (*node)->n_pending > 0) {

		ut_a(!(*node)->being_extended);

		if (count > 1000) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Trying to close/delete tablespace '%s' "
				"but there are %lu flushes "
				" and %lu pending i/o's on it.",
				space->name,
				(ulong) space->n_pending_flushes,
				(ulong) (*node)->n_pending);
		}

		return(count + 1);
	}

	return(0);
}

/*******************************************************************//**
Check pending operations on a tablespace.
@return DB_SUCCESS or error failure. */
static
dberr_t
fil_check_pending_operations(
/*=========================*/
	ulint		id,	/*!< in: space id */
	fil_space_t**	space,	/*!< out: tablespace instance in memory */
	char**		path)	/*!< out/own: tablespace path */
{
	ulint		count = 0;

	ut_a(!Tablespace::is_system_tablespace(id));
	ut_ad(space);

	*space = 0;

	mutex_enter(&fil_system->mutex);
	fil_space_t* sp = fil_space_get_by_id(id);
	if (sp) {
		sp->stop_new_ops = TRUE;
	}
	mutex_exit(&fil_system->mutex);

	/* Check for pending change buffer merges. */

	do {
		mutex_enter(&fil_system->mutex);

		sp = fil_space_get_by_id(id);

		count = fil_ibuf_check_pending_ops(sp, count);

		mutex_exit(&fil_system->mutex);

		if (count > 0) {
			os_thread_sleep(20000);
		}

	} while (count > 0);

	/* Check for pending IO. */

	*path = 0;

	do {
		mutex_enter(&fil_system->mutex);

		sp = fil_space_get_by_id(id);

		if (sp == NULL) {
			mutex_exit(&fil_system->mutex);
			return(DB_TABLESPACE_NOT_FOUND);
		}

		fil_node_t*	node;

		count = fil_check_pending_io(sp, &node, count);

		if (count == 0) {
			*path = mem_strdup(node->name);
		}

		mutex_exit(&fil_system->mutex);

		if (count > 0) {
			os_thread_sleep(20000);
		}

	} while (count > 0);

	ut_ad(sp);

	*space = sp;
	return(DB_SUCCESS);
}

/*******************************************************************//**
Closes a single-table tablespace. The tablespace must be cached in the
memory cache. Free all pages used by the tablespace.
@return	DB_SUCCESS or error */
UNIV_INTERN
dberr_t
fil_close_tablespace(
/*=================*/
	trx_t*		trx,	/*!< in/out: Transaction covering the close */
	ulint		id)	/*!< in: space id */
{
	char*		path = 0;
	fil_space_t*	space = 0;

	ut_a(!Tablespace::is_system_tablespace(id));

	dberr_t		err = fil_check_pending_operations(id, &space, &path);

	if (err != DB_SUCCESS) {
		return(err);
	}

	ut_a(space);
	ut_a(path != 0);

	rw_lock_x_lock(&space->latch);

#ifndef UNIV_HOTBACKUP
	/* Invalidate in the buffer pool all pages belonging to the
	tablespace. Since we have set space->stop_new_ops = TRUE, readahead
	or ibuf merge can no longer read more pages of this tablespace to the
	buffer pool. Thus we can clean the tablespace out of the buffer pool
	completely and permanently. The flag stop_new_ops also prevents
	fil_flush() from being applied to this tablespace. */

	buf_LRU_flush_or_remove_pages(id, BUF_REMOVE_FLUSH_WRITE, trx);
#endif
	mutex_enter(&fil_system->mutex);

	/* If the free is successful, the X lock will be released before
	the space memory data structure is freed. */

	if (!fil_space_free(id, TRUE)) {
		rw_lock_x_unlock(&space->latch);
		err = DB_TABLESPACE_NOT_FOUND;
	} else {
		err = DB_SUCCESS;
	}

	mutex_exit(&fil_system->mutex);

	/* If it is a delete then also delete any generated files, otherwise
	when we drop the database the remove directory will fail. */

	char*	cfg_name = fil_make_cfg_name(path);

	os_file_delete_if_exists(cfg_name);

	mem_free(path);
	mem_free(cfg_name);

	return(err);
}

/*******************************************************************//**
Deletes a single-table tablespace. The tablespace must be cached in the
memory cache.
@return	DB_SUCCESS or error */
UNIV_INTERN
dberr_t
fil_delete_tablespace(
/*==================*/
	ulint		id,		/*!< in: space id */
	buf_remove_t	buf_remove)	/*!< in: specify the action to take
					on the tables pages in the buffer
					pool */
{
	char*		path = 0;
	fil_space_t*	space = 0;

	ut_a(!Tablespace::is_system_tablespace(id));

	dberr_t		err = fil_check_pending_operations(id, &space, &path);

	if (err != DB_SUCCESS) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot delete tablespace %lu because it is not "
			"found in the tablespace memory cache.",
			(ulong) id);

		return(err);
	}

	ut_a(space);
	ut_a(path != 0);

	/* Important: We rely on the data dictionary mutex to ensure
	that a race is not possible here. It should serialize the tablespace
	drop/free. We acquire an X latch only to avoid a race condition
	when accessing the tablespace instance via:

	  fsp_get_available_space_in_free_extents().

	There our main motivation is to reduce the contention on the
	dictionary mutex. */

	rw_lock_x_lock(&space->latch);

#ifndef UNIV_HOTBACKUP
	/* IMPORTANT: Because we have set space::stop_new_ops there
	can't be any new ibuf merges, reads or flushes. We are here
	because node::n_pending was zero above. However, it is still
	possible to have pending read and write requests:

	A read request can happen because the reader thread has
	gone through the ::stop_new_ops check in buf_page_init_for_read()
	before the flag was set and has not yet incremented ::n_pending
	when we checked it above.

	A write request can be issued any time because we don't check
	the ::stop_new_ops flag when queueing a block for write.

	We deal with pending write requests in the following function
	where we'd minimally evict all dirty pages belonging to this
	space from the flush_list. Not that if a block is IO-fixed
	we'll wait for IO to complete.

	To deal with potential read requests by checking the
	::stop_new_ops flag in fil_io() */

	buf_LRU_flush_or_remove_pages(id, buf_remove, 0);

#endif /* !UNIV_HOTBACKUP */

	/* If it is a delete then also delete any generated files, otherwise
	when we drop the database the remove directory will fail. */
	{
		char*	cfg_name = fil_make_cfg_name(path);
		os_file_delete_if_exists(cfg_name);
		mem_free(cfg_name);
	}

	/* Delete the link file pointing to the ibd file we are deleting. */
	if (FSP_FLAGS_HAS_DATA_DIR(space->flags)) {
		fil_delete_link_file(space->name);
	}

	mutex_enter(&fil_system->mutex);

	/* Double check the sanity of pending ops after reacquiring
	the fil_system::mutex. */
	if (fil_space_get_by_id(id)) {
		ut_a(space->n_pending_ops == 0);
		ut_a(UT_LIST_GET_LEN(space->chain) == 1);
		fil_node_t* node = UT_LIST_GET_FIRST(space->chain);
		ut_a(node->n_pending == 0);
	}

	if (!fil_space_free(id, TRUE)) {
		err = DB_TABLESPACE_NOT_FOUND;
	}

	mutex_exit(&fil_system->mutex);

	if (err != DB_SUCCESS) {
		rw_lock_x_unlock(&space->latch);
	} else if (!os_file_delete(path) && !os_file_delete_if_exists(path)) {

		/* Note: This is because we have removed the
		tablespace instance from the cache. */

		err = DB_IO_ERROR;
	}

	if (err == DB_SUCCESS) {
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
		err = DB_SUCCESS;
	}

	mem_free(path);

	return(err);
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

	is_being_deleted = space->stop_new_ops;

	mutex_exit(&fil_system->mutex);

	return(is_being_deleted);
}

#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but

 1. We do not drop the table from the data dictionary;

 2. We remove all insert buffer entries for the tablespace immediately;
    in DROP TABLE they are only removed gradually in the background;

 3. Free all the pages in use by the tablespace.
@return	DB_SUCCESS or error */
UNIV_INTERN
dberr_t
fil_discard_tablespace(
/*===================*/
	ulint	id)	/*!< in: space id */
{
	dberr_t	err;

	switch (err = fil_delete_tablespace(id, BUF_REMOVE_ALL_NO_WRITE)) {
	case DB_SUCCESS:
		break;

	case DB_IO_ERROR:
		ib_logf(IB_LOG_LEVEL_WARN,
			"While deleting tablespace %lu in DISCARD TABLESPACE."
			" File rename/delete failed: %s",
			(ulong) id, ut_strerr(err));
		break;

	case DB_TABLESPACE_NOT_FOUND:
		ib_logf(IB_LOG_LEVEL_WARN,
			"Cannot delete tablespace %lu in DISCARD "
			"TABLESPACE. %s",
			(ulong) id, ut_strerr(err));
		break;

	default:
		ut_error;
	}

	/* Remove all insert buffer entries for the tablespace */

	ibuf_delete_for_discarded_space(id);

	return(err);
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
	const char*	new_name,	/*!< in: new name */
	const char*	new_path)	/*!< in: new file path */
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

	space2 = fil_space_get_by_name(new_name);
	if (space2 != NULL) {
		fputs("InnoDB: Error: ", stderr);
		ut_print_filename(stderr, new_name);
		fputs(" is already in tablespace memory cache\n", stderr);

		return(FALSE);
	}

	HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(space->name), space);
	mem_free(space->name);
	mem_free(node->name);

	space->name = mem_strdup(new_name);
	node->name = mem_strdup(new_path);

	HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(new_name), space);
	return(TRUE);
}

/*******************************************************************//**
Allocates a file name for a single-table tablespace. The string must be freed
by caller with mem_free().
@return	own: file name */
UNIV_INTERN
char*
fil_make_ibd_name(
/*==============*/
	const char*	name,		/*!< in: table name or a dir path */
	bool		is_full_path)	/*!< in: TRUE if it is a dir path */
{
	char*	filename;
	ulint	namelen		= strlen(name);
	ulint	dirlen		= strlen(fil_path_to_mysql_datadir);
	ulint	pathlen		= dirlen + namelen + sizeof "/.ibd";

	filename = static_cast<char*>(mem_alloc(pathlen));

	if (is_full_path) {
		memcpy(filename, name, namelen);
		memcpy(filename + namelen, ".ibd", sizeof ".ibd");
	} else {
		ut_snprintf(filename, pathlen, "%s/%s.ibd",
			fil_path_to_mysql_datadir, name);

	}

	srv_normalize_path_for_win(filename);

	return(filename);
}

/*******************************************************************//**
Allocates a file name for a tablespace ISL file (InnoDB Symbolic Link).
The string must be freed by caller with mem_free().
@return	own: file name */
UNIV_INTERN
char*
fil_make_isl_name(
/*==============*/
	const char*	name)	/*!< in: table name */
{
	char*	filename;
	ulint	namelen		= strlen(name);
	ulint	dirlen		= strlen(fil_path_to_mysql_datadir);
	ulint	pathlen		= dirlen + namelen + sizeof "/.isl";

	filename = static_cast<char*>(mem_alloc(pathlen));

	ut_snprintf(filename, pathlen, "%s/%s.isl",
		fil_path_to_mysql_datadir, name);

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
	const char*	old_name_in,	/*!< in: old table name in the
					standard databasename/tablename
					format of InnoDB, or NULL if we
					do the rename based on the space
					id only */
	ulint		id,		/*!< in: space id */
	const char*	new_name,	/*!< in: new table name in the
					standard databasename/tablename
					format of InnoDB */
	const char*	new_path_in)	/*!< in: new full datafile path
					if the tablespace is remotely
					located, or NULL if it is located
					in the normal data directory. */
{
	ibool		success;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		count		= 0;
	char*		new_path;
	char*		old_name;
	char*		old_path;
	const char*	not_given	= "(name not specified)";

	ut_a(id != 0);

retry:
	count++;

	if (!(count % 1000)) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Warning: problems renaming ", stderr);
		ut_print_filename(stderr,
				  old_name_in ? old_name_in : not_given);
		fputs(" to ", stderr);
		ut_print_filename(stderr, new_name);
		fprintf(stderr, ", %lu iterations\n", (ulong) count);
	}

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	DBUG_EXECUTE_IF("fil_rename_tablespace_failure_1", space = NULL; );

	if (space == NULL) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot find space id %lu in the tablespace "
			"memory cache, though the table '%s' in a "
			"rename operation should have that id.",
			(ulong) id, old_name_in ? old_name_in : not_given);
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

	/* The following code must change when InnoDB supports
	multiple datafiles per tablespace. */
	ut_a(UT_LIST_GET_LEN(space->chain) == 1);
	node = UT_LIST_GET_FIRST(space->chain);

	if (node->n_pending > 0
	    || node->n_pending_flushes > 0
	    || node->being_extended) {
		/* There are pending i/o's or flushes or the file is
		currently being extended, sleep for a while and
		retry */

		mutex_exit(&fil_system->mutex);

		os_thread_sleep(20000);

		goto retry;

	} else if (node->modification_counter > node->flush_counter) {
		/* Flush the space */

		mutex_exit(&fil_system->mutex);

		os_thread_sleep(20000);

		fil_flush(id);

		goto retry;

	} else if (node->open) {
		/* Close the file */

		fil_node_close_file(node, fil_system);
	}

	/* Check that the old name in the space is right */

	if (old_name_in) {
		old_name = mem_strdup(old_name_in);
		ut_a(strcmp(space->name, old_name) == 0);
	} else {
		old_name = mem_strdup(space->name);
	}
	old_path = mem_strdup(node->name);

	/* Rename the tablespace and the node in the memory cache */
	new_path = new_path_in ? mem_strdup(new_path_in)
		: fil_make_ibd_name(new_name, false);

	success = fil_rename_tablespace_in_mem(
		space, node, new_name, new_path);

	if (success) {

		DBUG_EXECUTE_IF("fil_rename_tablespace_failure_2",
			goto skip_second_rename; );

		success = os_file_rename(
			innodb_file_data_key, old_path, new_path);

		DBUG_EXECUTE_IF("fil_rename_tablespace_failure_2",
skip_second_rename:
			success = FALSE; );

		if (!success) {
			/* We have to revert the changes we made
			to the tablespace memory cache */

			ut_a(fil_rename_tablespace_in_mem(
					space, node, old_name, old_path));
		}
	}

	space->stop_ios = FALSE;

	mutex_exit(&fil_system->mutex);

#ifndef UNIV_HOTBACKUP
	if (success && !recv_recovery_on) {
		mtr_t		mtr;

		mtr_start(&mtr);

		fil_op_write_log(MLOG_FILE_RENAME, id, 0, 0, old_name, new_name,
				 &mtr);
		mtr_commit(&mtr);
	}
#endif /* !UNIV_HOTBACKUP */

	mem_free(new_path);
	mem_free(old_path);
	mem_free(old_name);

	return(success);
}

/*******************************************************************//**
Creates a new InnoDB Symbolic Link (ISL) file.  It is always created
under the 'datadir' of MySQL. The datadir is the directory of a
running mysqld program. We can refer to it by simply using the path '.'.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fil_create_link_file(
/*=================*/
	const char*	tablename,	/*!< in: tablename */
	const char*	filepath)	/*!< in: pathname of tablespace */
{
	os_file_t	file;
	ibool		success;
	dberr_t		err = DB_SUCCESS;
	char*		link_filepath;
	char*		prev_filepath = fil_read_link_file(tablename);

	ut_ad(!srv_read_only_mode);

	if (prev_filepath) {
		/* Truncate will call this with an existing
		link file which contains the same filepath. */
		if (0 == strcmp(prev_filepath, filepath)) {
			mem_free(prev_filepath);
			return(DB_SUCCESS);
		}
		mem_free(prev_filepath);
	}

	link_filepath = fil_make_isl_name(tablename);

	file = os_file_create_simple_no_error_handling(
		innodb_file_data_key, link_filepath,
		OS_FILE_CREATE, OS_FILE_READ_WRITE, &success);

	if (!success) {
		/* The following call will print an error message */
		ulint	error = os_file_get_last_error(true);

		ut_print_timestamp(stderr);
		fputs("  InnoDB: Cannot create file ", stderr);
		ut_print_filename(stderr, link_filepath);
		fputs(".\n", stderr);

		if (error == OS_FILE_ALREADY_EXISTS) {
			fputs("InnoDB: The link file: ", stderr);
			ut_print_filename(stderr, filepath);
			fputs(" already exists.\n", stderr);
			err = DB_TABLESPACE_EXISTS;

		} else if (error == OS_FILE_DISK_FULL) {
			err = DB_OUT_OF_FILE_SPACE;

		} else {
			err = DB_ERROR;
		}

		/* file is not open, no need to close it. */
		mem_free(link_filepath);
		return(err);
	}

	if (!os_file_write(link_filepath, file, filepath, 0,
			    strlen(filepath))) {
		err = DB_ERROR;
	}

	/* Close the file, we only need it at startup */
	os_file_close(file);

	mem_free(link_filepath);

	return(err);
}

/*******************************************************************//**
Deletes an InnoDB Symbolic Link (ISL) file. */
UNIV_INTERN
void
fil_delete_link_file(
/*=================*/
	const char*	tablename)	/*!< in: name of table */
{
	char* link_filepath = fil_make_isl_name(tablename);

	os_file_delete_if_exists(link_filepath);

	mem_free(link_filepath);
}

/*******************************************************************//**
Reads an InnoDB Symbolic Link (ISL) file.
It is always created under the 'datadir' of MySQL.  The name is of the
form {databasename}/{tablename}. and the isl file is expected to be in a
'{databasename}' directory called '{tablename}.isl'. The caller must free
the memory of the null-terminated path returned if it is not null.
@return	own: filepath found in link file, NULL if not found. */
UNIV_INTERN
char*
fil_read_link_file(
/*===============*/
	const char*	name)		/*!< in: tablespace name */
{
	char*		filepath = NULL;
	char*		link_filepath;
	FILE*		file = NULL;

	/* The .isl file is in the 'normal' tablespace location. */
	link_filepath = fil_make_isl_name(name);

	file = fopen(link_filepath, "r+b");

	mem_free(link_filepath);

	if (file) {
		filepath = static_cast<char*>(mem_alloc(OS_FILE_MAX_PATH));

		os_file_read_string(file, filepath, OS_FILE_MAX_PATH);
		fclose(file);

		if (strlen(filepath)) {
			/* Trim whitespace from end of filepath */
			ulint lastch = strlen(filepath) - 1;
			while (lastch > 4 && filepath[lastch] <= 0x20) {
				filepath[lastch--] = 0x00;
			}
			srv_normalize_path_for_win(filepath);
		}
	}

	return(filepath);
}

/*******************************************************************//**
Opens a handle to the file linked to in an InnoDB Symbolic Link file.
@return	TRUE if remote linked tablespace file is found and opened. */
UNIV_INTERN
ibool
fil_open_linked_file(
/*===============*/
	const char*	tablename,	/*!< in: database/tablename */
	char**		remote_filepath,/*!< out: remote filepath */
	os_file_t*	remote_file)	/*!< out: remote file handle */

{
	ibool		success;

	*remote_filepath = fil_read_link_file(tablename);
	if (*remote_filepath == NULL) {
		return(FALSE);
	}

	/* The filepath provided is different from what was
	found in the link file. */
	*remote_file = os_file_create_simple_no_error_handling(
		innodb_file_data_key, *remote_filepath,
		OS_FILE_OPEN, OS_FILE_READ_ONLY,
		&success);

	if (!success) {
		char*	link_filepath = fil_make_isl_name(tablename);

		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"A link file was found named '%s' "
			"but the linked tablespace '%s' "
			"could not be opened.",
			link_filepath, *remote_filepath);

		mem_free(link_filepath);
		mem_free(*remote_filepath);
		*remote_filepath = NULL;
	}

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
dberr_t
fil_create_new_single_table_tablespace(
/*===================================*/
	ulint		space_id,	/*!< in: space id */
	const char*	tablename,	/*!< in: the table name in the usual
					databasename/tablename format
					of InnoDB */
	const char*	dir_path,	/*!< in: NULL or a dir path */
	ulint		flags,		/*!< in: tablespace flags */
	ulint		flags2,		/*!< in: table flags2 */
	ulint		size)		/*!< in: the initial size of the
					tablespace file in pages,
					must be >= FIL_IBD_FILE_INITIAL_SIZE */
{
	os_file_t	file;
	ibool		ret;
	dberr_t		err;
	byte*		buf2;
	byte*		page;
	char*		path;
	ibool		success;
	/* TRUE if a table is created with CREATE TEMPORARY TABLE */
	bool		is_temp = !!(flags2 & DICT_TF2_TEMPORARY);
	bool		has_data_dir = FSP_FLAGS_HAS_DATA_DIR(flags);

	ut_a(space_id > 0);
	ut_ad(!srv_read_only_mode);
	ut_a(space_id < SRV_LOG_SPACE_FIRST_ID);
	ut_a(size >= FIL_IBD_FILE_INITIAL_SIZE);
	ut_a(fsp_flags_is_valid(flags));

	if (is_temp) {
		/* Temporary table filepath */
		ut_ad(dir_path);
		path = fil_make_ibd_name(dir_path, true);
	} else if (has_data_dir) {
		ut_ad(dir_path);
		path = os_file_make_remote_pathname(dir_path, tablename, "ibd");

		/* Since this tablespace file will be created in a
		remote directory, let's create the subdirectories
		in the path, if they are not there already. */
		success = os_file_create_subdirs_if_needed(path);
		if (!success) {
			err = DB_ERROR;
			goto error_exit_3;
		}
	} else {
		path = fil_make_ibd_name(tablename, false);
	}

	file = os_file_create(
		innodb_file_data_key, path,
		OS_FILE_CREATE | OS_FILE_ON_ERROR_NO_EXIT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		&ret);

	if (ret == FALSE) {
		/* The following call will print an error message */
		ulint	error = os_file_get_last_error(true);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot create file '%s'\n", path);

		if (error == OS_FILE_ALREADY_EXISTS) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"The file '%s' already exists though the "
				"corresponding table did not exist "
				"in the InnoDB data dictionary. "
				"Have you moved InnoDB .ibd files "
				"around without using the SQL commands "
				"DISCARD TABLESPACE and IMPORT TABLESPACE, "
				"or did mysqld crash in the middle of "
				"CREATE TABLE? "
				"You can resolve the problem by removing "
				"the file '%s' under the 'datadir' of MySQL.",
				path, path);

			err = DB_TABLESPACE_EXISTS;
			goto error_exit_3;
		}

		if (error == OS_FILE_DISK_FULL) {
			err = DB_OUT_OF_FILE_SPACE;
			goto error_exit_3;
		}

		err = DB_ERROR;
		goto error_exit_3;
	}

	ret = os_file_set_size(path, file, size * UNIV_PAGE_SIZE);

	if (!ret) {
		err = DB_OUT_OF_FILE_SPACE;
		goto error_exit_2;
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

	buf2 = static_cast<byte*>(ut_malloc(3 * UNIV_PAGE_SIZE));
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

	memset(page, '\0', UNIV_PAGE_SIZE);

	/* Add the UNIV_PAGE_SIZE to the table flags and write them to the
	tablespace header. */
	flags = fsp_flags_set_page_size(flags, UNIV_PAGE_SIZE);
	fsp_header_init_fields(page, space_id, flags);
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

	if (!(fsp_flags_is_compressed(flags))) {
		buf_flush_init_for_writing(page, NULL, 0);
		ret = os_file_write(path, file, page, 0, UNIV_PAGE_SIZE);
	} else {
		page_zip_des_t	page_zip;
		ulint		zip_size;

		zip_size = fsp_flags_get_zip_size(flags);

		page_zip_set_size(&page_zip, zip_size);
		page_zip.data = page + UNIV_PAGE_SIZE;
#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
			page_zip.m_end = page_zip.m_nonempty =
			page_zip.n_blobs = 0;
		buf_flush_init_for_writing(page, &page_zip, 0);
		ret = os_file_write(path, file, page_zip.data, 0, zip_size);
	}

	ut_free(buf2);

	if (!ret) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Could not write the first page to tablespace "
			"'%s'", path);

		err = DB_ERROR;
		goto error_exit_2;
	}

	ret = os_file_flush(file);

	if (!ret) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"File flush of tablespace '%s' failed", path);
		err = DB_ERROR;
		goto error_exit_2;
	}

	if (has_data_dir) {
		/* Now that the IBD file is created, make the ISL file. */
		err = fil_create_link_file(tablename, path);
		if (err != DB_SUCCESS) {
			goto error_exit_2;
		}
	}

	success = fil_space_create(tablename, space_id, flags, FIL_TABLESPACE);
	if (!success || !fil_node_create(path, size, space_id, FALSE)) {
		err = DB_ERROR;
		goto error_exit_1;
	}

#ifndef UNIV_HOTBACKUP
	{
		mtr_t		mtr;
		ulint		mlog_file_flag = 0;

		if (is_temp) {
			mlog_file_flag |= MLOG_FILE_FLAG_TEMP;
		}

		mtr_start(&mtr);

		fil_op_write_log(flags
				 ? MLOG_FILE_CREATE2
				 : MLOG_FILE_CREATE,
				 space_id, mlog_file_flag, flags,
				 tablename, NULL, &mtr);

		mtr_commit(&mtr);
	}
#endif
	err = DB_SUCCESS;

	/* Error code is set.  Cleanup the various variables used.
	These labels reflect the order in which variables are assigned or
	actions are done. */
error_exit_1:
	if (has_data_dir && err != DB_SUCCESS) {
		fil_delete_link_file(tablename);
	}
error_exit_2:
	os_file_close(file);
	if (err != DB_SUCCESS) {
		os_file_delete(path);
	}
error_exit_3:
	mem_free(path);

	return(err);
}

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Report information about a bad tablespace. */
static
void
fil_report_bad_tablespace(
/*======================*/
	char*		filepath,	/*!< in: filepath */
	ulint		found_id,	/*!< in: found space ID */
	ulint		found_flags,	/*!< in: found flags */
	ulint		expected_id,	/*!< in: expected space id */
	ulint		expected_flags)	/*!< in: expected flags */
{
	ib_logf(IB_LOG_LEVEL_ERROR,
		"In file '%s', tablespace id and flags are %lu and %lu, "
		"but in the InnoDB data dictionary they are %lu and %lu. "
		"Have you moved InnoDB .ibd files around without using the "
		"commands DISCARD TABLESPACE and IMPORT TABLESPACE? "
		"Please refer to "
		REFMAN "innodb-troubleshooting-datadict.html "
		"for how to resolve the issue.",
		filepath, (ulong) found_id, (ulong) found_flags,
		(ulong) expected_id, (ulong) expected_flags);
}

struct fsp_open_info {
	ibool		success;	/*!< Has the tablespace been opened? */
	ibool		valid;		/*!< Is the tablespace valid? */
	os_file_t	file;		/*!< File handle */
	char*		filepath;	/*!< File path to open */
	lsn_t		lsn;		/*!< Flushed LSN from header page */
	ulint		id;		/*!< Space ID */
	ulint		flags;		/*!< Tablespace flags */
#ifdef UNIV_LOG_ARCHIVE
	ulint		arch_log_no;	/*!< latest archived log file number */
#endif /* UNIV_LOG_ARCHIVE */
};

/********************************************************************//**
Tries to open a single-table tablespace and optionally checks that the
space id in it is correct. If this does not succeed, print an error message
to the .err log. This function is used to open a tablespace when we start
mysqld after the dictionary has been booted, and also in IMPORT TABLESPACE.

NOTE that we assume this operation is used either at the database startup
or under the protection of the dictionary mutex, so that two users cannot
race here. This operation does not leave the file associated with the
tablespace open, but closes it after we have looked at the space id in it.

If the validate boolean is set, we read the first page of the file and
check that the space id in the file is what we expect. We assume that
this function runs much faster if no check is made, since accessing the
file inode probably is much faster (the OS caches them) than accessing
the first page of the file.  This boolean may be initially FALSE, but if
a remote tablespace is found it will be changed to true.

If the fix_dict boolean is set, then it is safe to use an internal SQL
statement to update the dictionary tables if they are incorrect.

@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fil_open_single_table_tablespace(
/*=============================*/
	bool		validate,	/*!< in: Do we validate tablespace? */
	bool		fix_dict,	/*!< in: Can we fix the dictionary? */
	ulint		id,		/*!< in: space id */
	ulint		flags,		/*!< in: tablespace flags */
	const char*	tablename,	/*!< in: table name in the
					databasename/tablename format */
	const char*	path_in)	/*!< in: tablespace filepath */
{
	dberr_t		err = DB_SUCCESS;
	bool		dict_filepath_same_as_default = false;
	bool		link_file_found = false;
	bool		link_file_is_bad = false;
	fsp_open_info	def;
	fsp_open_info	dict;
	fsp_open_info	remote;
	ulint		tablespaces_found = 0;
	ulint		valid_tablespaces_found = 0;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!fix_dict || rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!fix_dict || mutex_own(&(dict_sys->mutex)));

	if (!fsp_flags_is_valid(flags)) {
		return(DB_CORRUPTION);
	}

	/* If the tablespace was relocated, we do not
	compare the DATA_DIR flag */
	ulint mod_flags = flags & ~FSP_FLAGS_MASK_DATA_DIR;

	memset(&def, 0, sizeof(def));
	memset(&dict, 0, sizeof(dict));
	memset(&remote, 0, sizeof(remote));

	/* Discover the correct filepath.  We will always look for an ibd
	in the default location. If it is remote, it should not be here. */
	def.filepath = fil_make_ibd_name(tablename, false);

	/* The path_in was read from SYS_DATAFILES. */
	if (path_in) {
		if (strcmp(def.filepath, path_in)) {
			dict.filepath = mem_strdup(path_in);
			/* possibility of multiple files. */
			validate = true;
		} else {
			dict_filepath_same_as_default = true;
		}
	}

	link_file_found = fil_open_linked_file(
		tablename, &remote.filepath, &remote.file);
	remote.success = link_file_found;
	if (remote.success) {
		/* possibility of multiple files. */
		validate = true;
		tablespaces_found++;

		/* A link file was found. MySQL does not allow a DATA
		DIRECTORY to be be the same as the default filepath. */
		ut_a(strcmp(def.filepath, remote.filepath));

		/* If there was a filepath found in SYS_DATAFILES,
		we hope it was the same as this remote.filepath found
		in the ISL file. */
		if (dict.filepath
		    && (0 == strcmp(dict.filepath, remote.filepath))) {
			remote.success = FALSE;
			os_file_close(remote.file);
			mem_free(remote.filepath);
			remote.filepath = NULL;
			tablespaces_found--;
		}
	}

	/* Attempt to open the tablespace at other possible filepaths. */
	if (dict.filepath) {
		dict.file = os_file_create_simple_no_error_handling(
			innodb_file_data_key, dict.filepath, OS_FILE_OPEN,
			OS_FILE_READ_ONLY, &dict.success);
		if (dict.success) {
			/* possibility of multiple files. */
			validate = true;
			tablespaces_found++;
		}
	}

	/* Always look for a file at the default location. */
	ut_a(def.filepath);
	def.file = os_file_create_simple_no_error_handling(
		innodb_file_data_key, def.filepath, OS_FILE_OPEN,
		OS_FILE_READ_ONLY, &def.success);
	if (def.success) {
		tablespaces_found++;
	}

	/*  We have now checked all possible tablespace locations and
	have a count of how many we found.  If things are normal, we
	only found 1. */
	if (!validate && tablespaces_found == 1) {
		goto skip_validate;
	}

	/* Read the first page of the datadir tablespace, if found. */
	if (def.success) {
		fil_read_first_page(
			def.file, &def.flags, &def.id, &def.lsn, &def.lsn);

		/* Validate this single-table-tablespace with SYS_TABLES,
		but do not compare the DATA_DIR flag, in case the
		tablespace was relocated. */
		ulint mod_def_flags = def.flags & ~FSP_FLAGS_MASK_DATA_DIR;
		if (def.id == id && mod_def_flags == mod_flags) {
			valid_tablespaces_found++;
			def.valid = TRUE;
		} else {
			/* Do not use this tablespace. */
			fil_report_bad_tablespace(
				def.filepath, def.id,
				def.flags, id, flags);
		}
	}

	/* Read the first page of the remote tablespace */
	if (remote.success) {
		fil_read_first_page(
			remote.file, &remote.flags, &remote.id,
			&remote.lsn, &remote.lsn);

		/* Validate this single-table-tablespace with SYS_TABLES,
		but do not compare the DATA_DIR flag, in case the
		tablespace was relocated. */
		ulint mod_remote_flags =
			remote.flags & ~FSP_FLAGS_MASK_DATA_DIR;

		if (remote.id == id && mod_remote_flags == mod_flags) {
			valid_tablespaces_found++;
			remote.valid = TRUE;
		} else {
			/* Do not use this linked tablespace. */
			fil_report_bad_tablespace(
				remote.filepath, remote.id,
				remote.flags, id, flags);
			link_file_is_bad = true;
		}
	}

	/* Read the first page of the datadir tablespace, if found. */
	if (dict.success) {
		fil_read_first_page(
			dict.file, &dict.flags, &dict.id, &dict.lsn, &dict.lsn);

		/* Validate this single-table-tablespace with SYS_TABLES,
		but do not compare the DATA_DIR flag, in case the
		tablespace was relocated. */
		ulint mod_dict_flags = dict.flags & ~FSP_FLAGS_MASK_DATA_DIR;
		if (dict.id == id && mod_dict_flags == mod_flags) {
			valid_tablespaces_found++;
			dict.valid = TRUE;
		} else {
			/* Do not use this tablespace. */
			fil_report_bad_tablespace(
				dict.filepath, dict.id,
				dict.flags, id, flags);
		}
	}

	/* Make sense of these three possible locations.
	First, bail out if no tablespace files were found. */
	if (valid_tablespaces_found == 0) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Could not find a valid tablespace file for '%s'. "
			"See " REFMAN "innodb-troubleshooting-datadict.html "
			"for how to resolve the issue.",
			tablename);

		err = DB_CORRUPTION;

		goto cleanup_and_exit;
	}

	/* Do not open any tablespaces if more than one tablespace with
	the correct space ID and flags were found. */
	if (tablespaces_found > 1) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"A tablespace for %s has been found in "
			"multiple places;", tablename);
		if (def.success) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Default location; %s, LSN=" LSN_PF
				", Space ID=%lu, Flags=%lu",
				def.filepath, def.lsn,
				(ulong) def.id, (ulong) def.flags);
		}
		if (remote.success) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Remote location; %s, LSN=" LSN_PF
				", Space ID=%lu, Flags=%lu",
				remote.filepath, remote.lsn,
				(ulong) remote.id, (ulong) remote.flags);
		}
		if (dict.success) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Dictionary location; %s, LSN=" LSN_PF
				", Space ID=%lu, Flags=%lu",
				dict.filepath, dict.lsn,
				(ulong) dict.id, (ulong) dict.flags);
		}

		/* Force-recovery will allow some tablespaces to be
		skipped by REDO if there was more than one file found.
		Unlike during the REDO phase of recovery, we now know
		if the tablespace is valid according to the dictionary,
		which was not available then. So if we did not force
		recovery and there is only one good tablespace, ignore
		any bad tablespaces. */
		if (valid_tablespaces_found > 1 || srv_force_recovery > 0) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Will not open the tablespace for '%s'",
				tablename);

			if (def.success != def.valid
			    || dict.success != dict.valid
			    || remote.success != remote.valid) {
				err = DB_CORRUPTION;
			} else {
				err = DB_ERROR;
			}
			goto cleanup_and_exit;
		}

		/* There is only one valid tablespace found and we did
		not use srv_force_recovery during REDO.  Use this one
		tablespace and clean up invalid tablespace pointers */
		if (def.success && !def.valid) {
			def.success = false;
			os_file_close(def.file);
			tablespaces_found--;
		}
		if (dict.success && !dict.valid) {
			dict.success = false;
			os_file_close(dict.file);
			/* Leave dict.filepath so that SYS_DATAFILES
			can be corrected below. */
			tablespaces_found--;
		}
		if (remote.success && !remote.valid) {
			remote.success = false;
			os_file_close(remote.file);
			mem_free(remote.filepath);
			remote.filepath = NULL;
			tablespaces_found--;
		}
	}

	/* At this point, there should be only one filepath. */
	ut_a(tablespaces_found == 1);
	ut_a(valid_tablespaces_found == 1);

	/* Only fix the dictionary at startup when there is only one thread.
	Calls to dict_load_table() can be done while holding other latches. */
	if (!fix_dict) {
		goto skip_validate;
	}

	/* We may need to change what is stored in SYS_DATAFILES or
	SYS_TABLESPACES or adjust the link file.
	Since a failure to update SYS_TABLESPACES or SYS_DATAFILES does
	not prevent opening and using the single_table_tablespace either
	this time or the next, we do not check the return code or fail
	to open the tablespace. But dict_update_filepath() will issue a
	warning to the log. */
	if (dict.filepath) {
		if (remote.success) {
			dict_update_filepath(id, remote.filepath);
		} else if (def.success) {
			dict_update_filepath(id, def.filepath);
			if (link_file_is_bad) {
				fil_delete_link_file(tablename);
			}
		} else if (!link_file_found || link_file_is_bad) {
			ut_ad(dict.success);
			/* Fix the link file if we got our filepath
			from the dictionary but a link file did not
			exist or it did not point to a valid file. */
			fil_delete_link_file(tablename);
			fil_create_link_file(tablename, dict.filepath);
		}

	} else if (remote.success && dict_filepath_same_as_default) {
		dict_update_filepath(id, remote.filepath);

	} else if (remote.success && path_in == NULL) {
		/* SYS_DATAFILES record for this space ID was not found. */
		dict_insert_tablespace_and_filepath(
			id, tablename, remote.filepath, flags);
	}

skip_validate:
	if (err != DB_SUCCESS) {
		; // Don't load the tablespace into the cache
	} else if (!fil_space_create(tablename, id, flags, FIL_TABLESPACE)) {
		err = DB_ERROR;
	} else {
		/* We do not measure the size of the file, that is why
		we pass the 0 below */

		if (!fil_node_create(remote.success ? remote.filepath :
				     dict.success ? dict.filepath :
				     def.filepath, 0, id, FALSE)) {
			err = DB_ERROR;
		}
	}

cleanup_and_exit:
	if (remote.success) {
		os_file_close(remote.file);
	}
	if (remote.filepath) {
		mem_free(remote.filepath);
	}
	if (dict.success) {
		os_file_close(dict.file);
	}
	if (dict.filepath) {
		mem_free(dict.filepath);
	}
	if (def.success) {
		os_file_close(def.file);
	}
	mem_free(def.filepath);

	return(err);
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
	char*	path;
	ulint	len	= strlen(name);

	path = static_cast<char*>(mem_alloc(len + (15 + sizeof suffix)));

	memcpy(path, name, len);
	memcpy(path + len, suffix, (sizeof suffix) - 1);
	ut_sprintf_timestamp_without_extra_chars(
		path + len + ((sizeof suffix) - 1));
	return(path);
}
#endif /* UNIV_HOTBACKUP */

/********************************************************************//**
Opens an .ibd file and adds the associated single-table tablespace to the
InnoDB fil0fil.cc data structures.
Set fsp->success to TRUE if tablespace is valid, FALSE if not. */
static
void
fil_validate_single_table_tablespace(
/*=================================*/
	const char*	tablename,	/*!< in: database/tablename */
	fsp_open_info*	fsp)		/*!< in/out: tablespace info */
{
	fil_read_first_page(
		fsp->file, &fsp->flags, &fsp->id, &fsp->lsn, &fsp->lsn);

	if (fsp->id == ULINT_UNDEFINED || fsp->id == 0) {
		fprintf(stderr,
		" InnoDB: Error: Tablespace is not sensible;"
		" Table: %s  Space ID: %lu  Filepath: %s\n",
		tablename, (ulong) fsp->id, fsp->filepath);
		fsp->success = FALSE;
		return;
	}

	mutex_enter(&fil_system->mutex);
	fil_space_t* space = fil_space_get_by_id(fsp->id);
	mutex_exit(&fil_system->mutex);
	if (space != NULL) {
		char* prev_filepath = fil_space_get_first_path(fsp->id);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Attempted to open a previously opened tablespace. "
			"Previous tablespace %s uses space ID: %lu at "
			"filepath: %s. Cannot open tablespace %s which uses "
			"space ID: %lu at filepath: %s",
			space->name, (ulong) space->id, prev_filepath,
			tablename, (ulong) fsp->id, fsp->filepath);

		mem_free(prev_filepath);
		fsp->success = FALSE;
		return;
	}

	fsp->success = TRUE;
}


/********************************************************************//**
Opens an .ibd file and adds the associated single-table tablespace to the
InnoDB fil0fil.cc data structures. */
static
void
fil_load_single_table_tablespace(
/*=============================*/
	const char*	dbname,		/*!< in: database name */
	const char*	filename)	/*!< in: file name (not a path),
					including the .ibd or .isl extension */
{
	char*		tablename;
	ulint		tablename_len;
	ulint		dbname_len = strlen(dbname);
	ulint		filename_len = strlen(filename);
	fsp_open_info	def;
	fsp_open_info	remote;
	os_offset_t	size;
#ifdef UNIV_HOTBACKUP
	fil_space_t*	space;
#endif

	memset(&def, 0, sizeof(def));
	memset(&remote, 0, sizeof(remote));

	/* The caller assured that the extension is ".ibd" or ".isl". */
	ut_ad(0 == memcmp(filename + filename_len - 4, ".ibd", 4)
	      || 0 == memcmp(filename + filename_len - 4, ".isl", 4));

	/* Build up the tablename in the standard form database/table. */
	tablename = static_cast<char*>(
		mem_alloc(dbname_len + filename_len + 2));
	sprintf(tablename, "%s/%s", dbname, filename);
	tablename_len = strlen(tablename) - strlen(".ibd");
	tablename[tablename_len] = '\0';

	/* There may be both .ibd and .isl file in the directory.
	And it is possible that the .isl file refers to a different
	.ibd file.  If so, we open and compare them the first time
	one of them is sent to this function.  So if this table has
	already been loaded, there is nothing to do.*/
	mutex_enter(&fil_system->mutex);
	if (fil_space_get_by_name(tablename)) {
		mem_free(tablename);
		mutex_exit(&fil_system->mutex);
		return;
	}
	mutex_exit(&fil_system->mutex);

	/* Build up the filepath of the .ibd tablespace in the datadir.
	This must be freed independent of def.success. */
	def.filepath = fil_make_ibd_name(tablename, false);

#ifdef __WIN__
# ifndef UNIV_HOTBACKUP
	/* If lower_case_table_names is 0 or 2, then MySQL allows database
	directory names with upper case letters. On Windows, all table and
	database names in InnoDB are internally always in lower case. Put the
	file path to lower case, so that we are consistent with InnoDB's
	internal data dictionary. */

	dict_casedn_str(def.filepath);
# endif /* !UNIV_HOTBACKUP */
#endif

	/* Check for a link file which locates a remote tablespace. */
	remote.success = fil_open_linked_file(
		tablename, &remote.filepath, &remote.file);

	/* Read the first page of the remote tablespace */
	if (remote.success) {
		fil_validate_single_table_tablespace(tablename, &remote);
		if (!remote.success) {
			os_file_close(remote.file);
			mem_free(remote.filepath);
		}
	}


	/* Try to open the tablespace in the datadir. */
	def.file = os_file_create_simple_no_error_handling(
		innodb_file_data_key, def.filepath, OS_FILE_OPEN,
		OS_FILE_READ_ONLY, &def.success);

	/* Read the first page of the remote tablespace */
	if (def.success) {
		fil_validate_single_table_tablespace(tablename, &def);
		if (!def.success) {
			os_file_close(def.file);
		}
	}

	if (!def.success && !remote.success) {
		/* The following call prints an error message */
		os_file_get_last_error(true);
		fprintf(stderr,
			"InnoDB: Error: could not open single-table"
			" tablespace file %s\n", def.filepath);
no_good_file:
		fprintf(stderr,
			"InnoDB: We do not continue the crash recovery,"
			" because the table may become\n"
			"InnoDB: corrupt if we cannot apply the log"
			" records in the InnoDB log to it.\n"
			"InnoDB: To fix the problem and start mysqld:\n"
			"InnoDB: 1) If there is a permission problem"
			" in the file and mysqld cannot\n"
			"InnoDB: open the file, you should"
			" modify the permissions.\n"
			"InnoDB: 2) If the table is not needed, or you"
			" can restore it from a backup,\n"
			"InnoDB: then you can remove the .ibd file,"
			" and InnoDB will do a normal\n"
			"InnoDB: crash recovery and ignore that table.\n"
			"InnoDB: 3) If the file system or the"
			" disk is broken, and you cannot remove\n"
			"InnoDB: the .ibd file, you can set"
			" innodb_force_recovery > 0 in my.cnf\n"
			"InnoDB: and force InnoDB to continue crash"
			" recovery here.\n");
will_not_choose:
		mem_free(tablename);
		if (remote.success) {
			mem_free(remote.filepath);
		}
		mem_free(def.filepath);

		if (srv_force_recovery > 0) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"innodb_force_recovery was set to %lu. "
				"Continuing crash recovery even though we "
				"cannot access the .ibd file of this table.",
				srv_force_recovery);
			return;
		}

		/* If debug code, cause a core dump and call stack. For
		release builds just exit and rely on the messages above. */
		ut_ad(0);
		exit(1);
	}

	if (def.success && remote.success) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Tablespaces for %s have been found in two places;\n"
			"Location 1: SpaceID: %lu  LSN: %lu  File: %s\n"
			"Location 2: SpaceID: %lu  LSN: %lu  File: %s\n"
			"You must delete one of them.",
			tablename, (ulong) def.id, (ulong) def.lsn,
			def.filepath, (ulong) remote.id, (ulong) remote.lsn,
			remote.filepath);

		def.success = FALSE;
		os_file_close(def.file);
		os_file_close(remote.file);
		goto will_not_choose;
	}

	/* At this point, only one tablespace is open */
	ut_a(def.success == !remote.success);

	fsp_open_info*	fsp = def.success ? &def : &remote;

	/* Get and test the file size. */
	size = os_file_get_size(fsp->file);

	if (size == (os_offset_t) -1) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"could not measure the size of single-table "
			"tablespace file %s", fsp->filepath);

		os_file_close(fsp->file);
		goto no_good_file;
	}

	/* Every .ibd file is created >= 4 pages in size. Smaller files
	cannot be ok. */
	ulong minimum_size = FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE;
	if (size < minimum_size) {
#ifndef UNIV_HOTBACKUP
		ib_logf(IB_LOG_LEVEL_ERROR,
			"The size of single-table tablespace file %s "
			"is only " UINT64PF ", should be at least %lu!",
			fsp->filepath, size, minimum_size);
		os_file_close(fsp->file);
		goto no_good_file;
#else
		fsp->id = ULINT_UNDEFINED;
		fsp->flags = 0;
#endif /* !UNIV_HOTBACKUP */
	}

#ifdef UNIV_HOTBACKUP
	if (fsp->id == ULINT_UNDEFINED || fsp->id == 0) {
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
			fsp->filepath, fsp->id, fsp->filepath, size);
		os_file_close(fsp->file);

		new_path = fil_make_ibbackup_old_name(fsp->filepath);

		bool	success = os_file_rename(
			innodb_file_data_key, fsp->filepath, new_path));

		ut_a(success);

		mem_free(new_path);

		goto func_exit_after_close;
	}

	/* A backup may contain the same space several times, if the space got
	renamed at a sensitive time. Since it is enough to have one version of
	the space, we rename the file if a space with the same space id
	already exists in the tablespace memory cache. We rather rename the
	file than delete it, because if there is a bug, we do not want to
	destroy valuable data. */

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(fsp->id);

	if (space) {
		char*	new_path;

		fprintf(stderr,
			"InnoDB: Renaming tablespace %s of id %lu,\n"
			"InnoDB: to %s_ibbackup_old_vers_<timestamp>\n"
			"InnoDB: because space %s with the same id\n"
			"InnoDB: was scanned earlier. This can happen"
			" if you have renamed tables\n"
			"InnoDB: during an ibbackup run.\n",
			fsp->filepath, fsp->id, fsp->filepath,
			space->name);
		os_file_close(fsp->file);

		new_path = fil_make_ibbackup_old_name(fsp->filepath);

		mutex_exit(&fil_system->mutex);

		bool	success = os_file_rename(
			innodb_file_data_key, fsp->filepath, new_path);

		ut_a(success);

		mem_free(new_path);

		goto func_exit_after_close;
	}
	mutex_exit(&fil_system->mutex);
#endif /* UNIV_HOTBACKUP */
	ibool file_space_create_success = fil_space_create(
		tablename, fsp->id, fsp->flags, FIL_TABLESPACE);

	if (!file_space_create_success) {
		if (srv_force_recovery > 0) {
			fprintf(stderr,
				"InnoDB: innodb_force_recovery was set"
				" to %lu. Continuing crash recovery\n"
				"InnoDB: even though the tablespace"
				" creation of this table failed.\n",
				srv_force_recovery);
			goto func_exit;
		}

		/* Exit here with a core dump, stack, etc. */
		ut_a(file_space_create_success);
	}

	/* We do not use the size information we have about the file, because
	the rounding formula for extents and pages is somewhat complex; we
	let fil_node_open() do that task. */

	if (!fil_node_create(fsp->filepath, 0, fsp->id, FALSE)) {
		ut_error;
	}

func_exit:
	os_file_close(fsp->file);

#ifdef UNIV_HOTBACKUP
func_exit_after_close:
#else
	ut_ad(!mutex_own(&fil_system->mutex));
#endif
	mem_free(tablename);
	if (remote.success) {
		mem_free(remote.filepath);
	}
	mem_free(def.filepath);
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
	dberr_t*	err,	/*!< out: this is set to DB_ERROR if an error
				was encountered, otherwise not changed */
	const char*	dirname,/*!< in: directory name or path */
	os_file_dir_t	dir,	/*!< in: directory stream */
	os_file_stat_t*	info)	/*!< in/out: buffer where the
				info is returned */
{
	for (ulint i = 0; i < 100; i++) {
		int	ret = os_file_readdir_next_file(dirname, dir, info);

		if (ret != -1) {

			return(ret);
		}

		ib_logf(IB_LOG_LEVEL_ERROR,
			"os_file_readdir_next_file() returned -1 in "
			"directory %s, crash recovery may have failed "
			"for some .ibd files!", dirname);

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
dberr_t
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
	dberr_t		err		= DB_SUCCESS;

	/* The datadir of MySQL is always the default directory of mysqld */

	dir = os_file_opendir(fil_path_to_mysql_datadir, TRUE);

	if (dir == NULL) {

		return(DB_ERROR);
	}

	dbpath = static_cast<char*>(mem_alloc(dbpath_len));

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

			dbpath = static_cast<char*>(mem_alloc(dbpath_len));
		}
		ut_snprintf(dbpath, dbpath_len,
			    "%s/%s", fil_path_to_mysql_datadir, dbinfo.name);
		srv_normalize_path_for_win(dbpath);

		dbdir = os_file_opendir(dbpath, FALSE);

		if (dbdir != NULL) {

			/* We found a database directory; loop through it,
			looking for possible .ibd files in it */

			ret = fil_file_readdir_next_file(&err, dbpath, dbdir,
							 &fileinfo);
			while (ret == 0) {

				if (fileinfo.type == OS_FILE_TYPE_DIR) {

					goto next_file_item;
				}

				/* We found a symlink or a file */
				if (strlen(fileinfo.name) > 4
				    && (0 == strcmp(fileinfo.name
						   + strlen(fileinfo.name) - 4,
						   ".ibd")
					|| 0 == strcmp(fileinfo.name
						   + strlen(fileinfo.name) - 4,
						   ".isl"))) {
					/* The name ends in .ibd or .isl;
					try opening the file */
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
@return	TRUE if does not exist or is being deleted */
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

	if (space == NULL || space->stop_new_ops) {
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
Report that a tablespace for a table was not found. */
static
void
fil_report_missing_tablespace(
/*===========================*/
	const char*	name,			/*!< in: table name */
	ulint		space_id)		/*!< in: table's space id */
{
	char index_name[MAX_FULL_NAME_LEN + 1];

	innobase_format_name(index_name, sizeof(index_name), name, TRUE);

	ib_logf(IB_LOG_LEVEL_ERROR,
		"Table %s in the InnoDB data dictionary has tablespace id %lu, "
		"but tablespace with that id or name does not exist. Have "
		"you deleted or moved .ibd files? This may also be a table "
		"created with CREATE TEMPORARY TABLE whose .ibd and .frm "
		"files MySQL automatically removed, but the table still "
		"exists in the InnoDB internal data dictionary.",
		name, space_id);
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
	const char*	name,		/*!< in: table name used in
					fil_space_create().  Either the
					standard 'dbname/tablename' format
					or table->dir_path_of_temp_table */
	ibool		mark_space,	/*!< in: in crash recovery, at database
					startup we mark all spaces which have
					an associated table in the InnoDB
					data dictionary, so that
					we can print a warning about orphaned
					tablespaces */
	ibool		print_error_if_does_not_exist,
					/*!< in: print detailed error
					information to the .err log if a
					matching tablespace is not found from
					memory */
	bool		adjust_space,	/*!< in: whether to adjust space id
					when find table space mismatch */
	mem_heap_t*	heap,		/*!< in: heap memory */
	table_id_t	table_id)	/*!< in: table id */
{
	fil_space_t*	fnamespace;
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	/* Look if there is a space with the same id */

	space = fil_space_get_by_id(id);

	/* Look if there is a space with the same name; the name is the
	directory path from the datadir to the file */

	fnamespace = fil_space_get_by_name(name);
	if (space && space == fnamespace) {
		/* Found */

		if (mark_space) {
			space->mark = TRUE;
		}

		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	/* Info from "fnamespace" comes from the ibd file itself, it can
	be different from data obtained from System tables since it is
	not transactional. If adjust_space is set, and the mismatching
	space are between a user table and its temp table, we shall
	adjust the ibd file name according to system table info */
	if (adjust_space
	    && space != NULL
	    && row_is_mysql_tmp_table_name(space->name)
	    && !row_is_mysql_tmp_table_name(name)) {

		mutex_exit(&fil_system->mutex);

		DBUG_EXECUTE_IF("ib_crash_before_adjust_fil_space",
				DBUG_SUICIDE(););

		if (fnamespace) {
			char*	tmp_name;

			tmp_name = dict_mem_create_temporary_tablename(
				heap, name, table_id);

			fil_rename_tablespace(fnamespace->name, fnamespace->id,
					      tmp_name, NULL);
		}

		DBUG_EXECUTE_IF("ib_crash_after_adjust_one_fil_space",
				DBUG_SUICIDE(););

		fil_rename_tablespace(space->name, id, name, NULL);

		DBUG_EXECUTE_IF("ib_crash_after_adjust_fil_space",
				DBUG_SUICIDE(););

		mutex_enter(&fil_system->mutex);
		fnamespace = fil_space_get_by_name(name);
		ut_ad(space == fnamespace);
		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	if (!print_error_if_does_not_exist) {

		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	if (space == NULL) {
		if (fnamespace == NULL) {
			if (print_error_if_does_not_exist) {
				fil_report_missing_tablespace(name, id);
			}
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
				(ulong) id, fnamespace->name,
				(ulong) fnamespace->id);
		}
error_exit:
		fputs("InnoDB: Please refer to\n"
		      "InnoDB: " REFMAN "innodb-troubleshooting-datadict.html\n"
		      "InnoDB: for how to resolve the issue.\n", stderr);

		mutex_exit(&fil_system->mutex);

		return(FALSE);
	}

	if (0 != strcmp(space->name, name)) {
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

		if (fnamespace != NULL) {
			fputs("InnoDB: There is a tablespace"
			      " with the right name\n"
			      "InnoDB: ", stderr);
			ut_print_filename(stderr, fnamespace->name);
			fprintf(stderr, ", but its id is %lu.\n",
				(ulong) fnamespace->id);
		}

		goto error_exit;
	}

	mutex_exit(&fil_system->mutex);

	return(FALSE);
}

/*******************************************************************//**
Checks if a single-table tablespace for a given table name exists in the
tablespace memory cache.
@return	space id, ULINT_UNDEFINED if not found */
UNIV_INTERN
ulint
fil_get_space_id_for_table(
/*=======================*/
	const char*	tablename)	/*!< in: table name in the standard
				'databasename/tablename' format */
{
	fil_space_t*	fnamespace;
	ulint		id		= ULINT_UNDEFINED;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	/* Look if there is a space with the same name. */

	fnamespace = fil_space_get_by_name(tablename);

	if (fnamespace) {
		id = fnamespace->id;
	}

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
	ulint		page_size;
	ulint		pages_added;
	ibool		success;

	ut_ad(!srv_read_only_mode);

retry:
	pages_added = 0;
	success = TRUE;

	fil_mutex_enter_and_prepare_for_io(space_id);

	space = fil_space_get_by_id(space_id);
	ut_a(space);

	if (space->size >= size_after_extend) {
		/* Space already big enough */

		*actual_size = space->size;

		mutex_exit(&fil_system->mutex);

		return(TRUE);
	}

	page_size = fsp_flags_get_zip_size(space->flags);
	if (!page_size) {
		page_size = UNIV_PAGE_SIZE;
	}

	node = UT_LIST_GET_LAST(space->chain);

	if (!node->being_extended) {
		/* Mark this node as undergoing extension. This flag
		is used by other threads to wait for the extension
		opereation to finish. */
		node->being_extended = TRUE;
	} else {
		/* Another thread is currently extending the file. Wait
		for it to finish.
		It'd have been better to use event driven mechanism but
		the entire module is peppered with polling stuff. */
		mutex_exit(&fil_system->mutex);
		os_thread_sleep(100000);
		goto retry;
	}

	fil_node_prepare_for_io(node, fil_system, space);

	/* At this point it is safe to release fil_system mutex. No
	other thread can rename, delete or close the file because
	we have set the node->being_extended flag. */
	mutex_exit(&fil_system->mutex);

	start_page_no = space->size;
	file_start_page_no = space->size - node->size;

	/* Extend at most 64 pages at a time */
	buf_size = ut_min(64, size_after_extend - start_page_no) * page_size;
	buf2 = static_cast<byte*>(mem_alloc(buf_size + page_size));
	buf = static_cast<byte*>(ut_align(buf2, page_size));

	memset(buf, 0, buf_size);

	while (start_page_no < size_after_extend) {
		ulint		n_pages
			= ut_min(buf_size / page_size,
				 size_after_extend - start_page_no);

		os_offset_t	offset
			= ((os_offset_t) (start_page_no - file_start_page_no))
			* page_size;
#ifdef UNIV_HOTBACKUP
		success = os_file_write(node->name, node->handle, buf,
					offset, page_size * n_pages);
#else
		success = os_aio(OS_FILE_WRITE, OS_AIO_SYNC,
				 node->name, node->handle, buf,
				 offset, page_size * n_pages,
				 NULL, NULL);
#endif /* UNIV_HOTBACKUP */
		if (success) {
			os_has_said_disk_full = FALSE;
		} else {
			/* Let us measure the size of the file to determine
			how much we were able to extend it */
			os_offset_t	size;

			size = os_file_get_size(node->handle);
			ut_a(size != (os_offset_t) -1);

			n_pages = ((ulint) (size / page_size))
				- node->size - pages_added;

			pages_added += n_pages;
			break;
		}

		start_page_no += n_pages;
		pages_added += n_pages;
	}

	mem_free(buf2);

	mutex_enter(&fil_system->mutex);

	ut_a(node->being_extended);

	space->size += pages_added;
	node->size += pages_added;
	node->being_extended = FALSE;

	fil_node_complete_io(node, fil_system, OS_FILE_WRITE);

	*actual_size = space->size;

#ifndef UNIV_HOTBACKUP
	/* Keep the last data file size info up to date, rounded to
	full megabytes */
	ulint pages_per_mb = (1024 * 1024) / page_size;
	ulint size_in_pages = ((node->size / pages_per_mb) * pages_per_mb);

	if (space_id == srv_sys_space.space_id()) {
		srv_sys_space.set_last_file_size(size_in_pages);
	} else if (space_id == srv_tmp_space.space_id()) {
		srv_tmp_space.set_last_file_size(size_in_pages);
	}
#endif /* !UNIV_HOTBACKUP */

	/*
	printf("Extended %s to %lu, actual size %lu pages\n", space->name,
	size_after_extend, *actual_size); */
	mutex_exit(&fil_system->mutex);

	fil_flush(space_id);

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
	dberr_t		error;
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
				 fsp_flags_get_zip_size(space->flags),
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
			ut_a(success);
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

	if (node->n_pending == 0 && fil_space_belongs_in_lru(space)) {
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
		ut_ad(!srv_read_only_mode);
		system->modification_counter++;
		node->modification_counter = system->modification_counter;

		if (fil_buffering_disabled(node->space)) {

			/* We don't need to keep track of unflushed
			changes as user has explicitly disabled
			buffering. */
			ut_ad(!node->space->is_in_unflushed_spaces);
			node->flush_counter = node->modification_counter;

		} else if (!node->space->is_in_unflushed_spaces) {

			node->space->is_in_unflushed_spaces = true;
			UT_LIST_ADD_FIRST(unflushed_spaces,
					  system->unflushed_spaces,
					  node->space);
		}
	}

	if (node->n_pending == 0 && fil_space_belongs_in_lru(node->space)) {

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
dberr_t
fil_io(
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
	void*	message)	/*!< in: message for aio handler if non-sync
				aio used, else ignored */
{
	ulint		mode;
	fil_space_t*	space;
	fil_node_t*	node;
	ibool		ret;
	ulint		is_log;
	ulint		wake_later;
	os_offset_t	offset;
	ibool		ignore_nonexistent_pages;

	is_log = type & OS_FILE_LOG;
	type = type & ~OS_FILE_LOG;

	wake_later = type & OS_AIO_SIMULATED_WAKE_LATER;
	type = type & ~OS_AIO_SIMULATED_WAKE_LATER;

	ignore_nonexistent_pages = type & BUF_READ_IGNORE_NONEXISTENT_PAGES;
	type &= ~BUF_READ_IGNORE_NONEXISTENT_PAGES;

	ut_ad(byte_offset < UNIV_PAGE_SIZE);
	ut_ad(!zip_size || !byte_offset);
	ut_ad(ut_is_2pow(zip_size));
	ut_ad(buf);
	ut_ad(len > 0);
	ut_ad(UNIV_PAGE_SIZE == (ulong)(1 << UNIV_PAGE_SIZE_SHIFT));
#if (1 << UNIV_PAGE_SIZE_SHIFT_MAX) != UNIV_PAGE_SIZE_MAX
# error "(1 << UNIV_PAGE_SIZE_SHIFT_MAX) != UNIV_PAGE_SIZE_MAX"
#endif
#if (1 << UNIV_PAGE_SIZE_SHIFT_MIN) != UNIV_PAGE_SIZE_MIN
# error "(1 << UNIV_PAGE_SIZE_SHIFT_MIN) != UNIV_PAGE_SIZE_MIN"
#endif
	ut_ad(fil_validate_skip());
#ifndef UNIV_HOTBACKUP
# ifndef UNIV_LOG_DEBUG
	/* ibuf bitmap pages must be read in the sync aio mode: */
	ut_ad(recv_no_ibuf_operations
	      || type == OS_FILE_WRITE
	      || !ibuf_bitmap_page(zip_size, block_offset)
	      || sync
	      || is_log);
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
		srv_stats.data_read.add(len);
	} else if (type == OS_FILE_WRITE) {
		ut_ad(!srv_read_only_mode);
		srv_stats.data_written.add(len);
	}

	/* Reserve the fil_system mutex and make sure that we can open at
	least one file while holding it, if the file is not already open */

	fil_mutex_enter_and_prepare_for_io(space_id);

	space = fil_space_get_by_id(space_id);

	/* If we are deleting a tablespace we don't allow any read
	operations on that. However, we do allow write operations. */
	if (space == 0 || (type == OS_FILE_READ && space->stop_new_ops)) {
		mutex_exit(&fil_system->mutex);

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Trying to do i/o to a tablespace which does "
			"not exist. i/o type %lu, space id %lu, "
			"page no. %lu, i/o length %lu bytes",
			(ulong) type, (ulong) space_id, (ulong) block_offset,
			(ulong) len);

		return(DB_TABLESPACE_DELETED);
	}

	ut_ad(mode != OS_AIO_IBUF || space->purpose == FIL_TABLESPACE);

	node = UT_LIST_GET_FIRST(space->chain);

	for (;;) {
		if (node == NULL) {
			if (ignore_nonexistent_pages) {
				mutex_exit(&fil_system->mutex);
				return(DB_ERROR);
			}

			fil_report_invalid_page_access(
				block_offset, space_id, space->name,
				byte_offset, len, type);

			ut_error;

		} else  if (fil_is_user_tablespace_id(space->id)
			   && node->size == 0) {

			/* We do not know the size of a single-table tablespace
			before we open the file */
			break;
		} else if (node->size > block_offset) {
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
	single-table tablespace, including rollback tablespaces. */
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
		offset = ((os_offset_t) block_offset << UNIV_PAGE_SIZE_SHIFT)
			+ byte_offset;

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
		offset = ((os_offset_t) block_offset << zip_size_shift)
			+ byte_offset;
		ut_a(node->size - block_offset
		     >= (len + (zip_size - 1)) / zip_size);
	}

	/* Do aio */

	ut_a(byte_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a((len % OS_FILE_LOG_BLOCK_SIZE) == 0);

#ifdef UNIV_HOTBACKUP
	/* In ibbackup do normal i/o, not aio */
	if (type == OS_FILE_READ) {
		ret = os_file_read(node->handle, buf, offset, len);
	} else {
		ut_ad(!srv_read_only_mode);
		ret = os_file_write(node->name, node->handle, buf,
				    offset, len);
	}
#else
	/* Queue the aio request */
	ret = os_aio(type, mode | wake_later, node->name, node->handle, buf,
		     offset, len, node, message);
#endif /* UNIV_HOTBACKUP */
	ut_a(ret);

	if (mode == OS_AIO_SYNC) {
		/* The i/o operation is already completed when we return from
		os_aio: */

		mutex_enter(&fil_system->mutex);

		fil_node_complete_io(node, fil_system, type);

		mutex_exit(&fil_system->mutex);

		ut_ad(fil_validate_skip());
	}

	return(DB_SUCCESS);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.cc for more info). The thread specifies which
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

	ut_ad(fil_validate_skip());

	if (srv_use_native_aio) {
		srv_set_io_thread_op_info(segment, "native aio handle");
#ifdef WIN_ASYNC_IO
		ret = os_aio_windows_handle(
			segment, 0, &fil_node, &message, &type);
#elif defined(LINUX_NATIVE_AIO)
		ret = os_aio_linux_handle(
			segment, &fil_node, &message, &type);
#else
		ut_error;
		ret = 0; /* Eliminate compiler warning */
#endif /* WIN_ASYNC_IO */
	} else {
		srv_set_io_thread_op_info(segment, "simulated aio handle");

		ret = os_aio_simulated_handle(
			segment, &fil_node, &message, &type);
	}

	ut_a(ret);
	if (fil_node == NULL) {
		ut_ad(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS);
		return;
	}

	srv_set_io_thread_op_info(segment, "complete io for fil node");

	mutex_enter(&fil_system->mutex);

	fil_node_complete_io(fil_node, fil_system, type);

	mutex_exit(&fil_system->mutex);

	ut_ad(fil_validate_skip());

	/* Do the i/o handling */
	/* IMPORTANT: since i/o handling for reads will read also the insert
	buffer in tablespace 0, you have to be very careful not to introduce
	deadlocks in the i/o system. We keep tablespace 0 data files always
	open, and use a special i/o thread to serve insert buffer requests. */

	if (fil_node->space->purpose == FIL_TABLESPACE) {
		srv_set_io_thread_op_info(segment, "complete io for buf page");
		buf_page_io_complete(static_cast<buf_page_t*>(message));
	} else {
		srv_set_io_thread_op_info(segment, "complete io for log");
		log_io_complete(static_cast<log_group_t*>(message));
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
	ulint	space_id)	/*!< in: file space id (this can be a group of
				log files or a tablespace of the database) */
{
	fil_space_t*	space;
	fil_node_t*	node;
	os_file_t	file;
	ib_int64_t	old_mod_counter;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(space_id);

	if (!space || space->stop_new_ops) {
		mutex_exit(&fil_system->mutex);

		return;
	}

	if (fil_buffering_disabled(space)) {

		/* No need to flush. User has explicitly disabled
		buffering. */
		ut_ad(!space->is_in_unflushed_spaces);
		ut_ad(fil_space_is_flushed(space));
		ut_ad(space->n_pending_flushes == 0);

#ifdef UNIV_DEBUG
		for (node = UT_LIST_GET_FIRST(space->chain);
		     node != NULL;
		     node = UT_LIST_GET_NEXT(chain, node)) {
			ut_ad(node->modification_counter
			      == node->flush_counter);
			ut_ad(node->n_pending_flushes == 0);
		}
#endif /* UNIV_DEBUG */

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
#endif /* __WIN__ */
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

			os_file_flush(file);

			mutex_enter(&fil_system->mutex);

			node->n_pending_flushes--;
skip_flush:
			if (node->flush_counter < old_mod_counter) {
				node->flush_counter = old_mod_counter;

				if (space->is_in_unflushed_spaces
				    && fil_space_is_flushed(space)) {

					space->is_in_unflushed_spaces = false;

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
	space_ids = static_cast<ulint*>(
		mem_alloc(n_space_ids * sizeof *space_ids));

	n_space_ids = 0;

	for (space = UT_LIST_GET_FIRST(fil_system->unflushed_spaces);
	     space;
	     space = UT_LIST_GET_NEXT(unflushed_spaces, space)) {

		if (space->purpose == purpose && !space->stop_new_ops) {

			space_ids[n_space_ids++] = space->id;
		}
	}

	mutex_exit(&fil_system->mutex);

	/* Flush the spaces.  It will not hurt to call fil_flush() on
	a non-existing space id. */
	for (i = 0; i < n_space_ids; i++) {

		fil_flush(space_ids[i]);
	}

	mem_free(space_ids);
}

/** Functor to validate the space list. */
struct	Check {
	void	operator()(const fil_node_t* elem)
	{
		ut_a(elem->open || !elem->n_pending);
	}
};

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

	mutex_enter(&fil_system->mutex);

	/* Look for spaces in the hash table */

	for (ulint i = 0; i < hash_get_n_cells(fil_system->spaces); i++) {

		for (space = static_cast<fil_space_t*>(
				HASH_GET_FIRST(fil_system->spaces, i));
		     space != 0;
		     space = static_cast<fil_space_t*>(
			     	HASH_GET_NEXT(hash, space))) {

			UT_LIST_VALIDATE(
				chain, fil_node_t, space->chain, Check());

			for (fil_node = UT_LIST_GET_FIRST(space->chain);
			     fil_node != 0;
			     fil_node = UT_LIST_GET_NEXT(chain, fil_node)) {

				if (fil_node->n_pending > 0) {
					ut_a(fil_node->open);
				}

				if (fil_node->open) {
					n_open++;
				}
			}
		}
	}

	ut_a(fil_system->n_open == n_open);

	UT_LIST_CHECK(LRU, fil_node_t, fil_system->LRU);

	for (fil_node = UT_LIST_GET_FIRST(fil_system->LRU);
	     fil_node != 0;
	     fil_node = UT_LIST_GET_NEXT(LRU, fil_node)) {

		ut_a(fil_node->n_pending == 0);
		ut_a(!fil_node->being_extended);
		ut_a(fil_node->open);
		ut_a(fil_space_belongs_in_lru(fil_node->space));
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
Closes the tablespace memory cache. */
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

/********************************************************************//**
Initializes a buffer control block when the buf_pool is created. */
static
void
fil_buf_block_init(
/*===============*/
	buf_block_t*	block,		/*!< in: pointer to control block */
	byte*		frame)		/*!< in: pointer to buffer frame */
{
	UNIV_MEM_DESC(frame, UNIV_PAGE_SIZE);

	block->frame = frame;

	block->page.io_fix = BUF_IO_NONE;
	/* There are assertions that check for this. */
	block->page.buf_fix_count = 1;
	block->page.state = BUF_BLOCK_READY_FOR_USE;

	page_zip_des_init(&block->page.zip);
}

struct fil_iterator_t {
	os_file_t	file;			/*!< File handle */
	const char*	filepath;		/*!< File path name */
	os_offset_t	start;			/*!< From where to start */
	os_offset_t	end;			/*!< Where to stop */
	os_offset_t	file_size;		/*!< File size in bytes */
	ulint		page_size;		/*!< Page size */
	ulint		n_io_buffers;		/*!< Number of pages to use
						for IO */
	byte*		io_buffer;		/*!< Buffer to use for IO */
};

/********************************************************************//**
TODO: This can be made parallel trivially by chunking up the file and creating
a callback per thread. . Main benefit will be to use multiple CPUs for
checksums and compressed tables. We have to do compressed tables block by
block right now. Secondly we need to decompress/compress and copy too much
of data. These are CPU intensive.

Iterate over all the pages in the tablespace.
@param iter - Tablespace iterator
@param block - block to use for IO
@param callback - Callback to inspect and update page contents
@retval DB_SUCCESS or error code */
static
dberr_t
fil_iterate(
/*========*/
	const fil_iterator_t&	iter,
	buf_block_t*		block,
	PageCallback&		callback)
{
	os_offset_t		offset;
	ulint			page_no = 0;
	ulint			space_id = callback.get_space_id();
	ulint			n_bytes = iter.n_io_buffers * iter.page_size;

	ut_ad(!srv_read_only_mode);

	/* TODO: For compressed tables we do a lot of useless
	copying for non-index pages. Unfortunately, it is
	required by buf_zip_decompress() */

	for (offset = iter.start; offset < iter.end; offset += n_bytes) {

		byte*		io_buffer = iter.io_buffer;

		block->frame = io_buffer;

		if (callback.get_zip_size() > 0) {
			page_zip_des_init(&block->page.zip);
			page_zip_set_size(&block->page.zip, iter.page_size);
			block->page.zip.data = block->frame + UNIV_PAGE_SIZE;
			ut_d(block->page.zip.m_external = true);
			ut_ad(iter.page_size == callback.get_zip_size());

			/* Zip IO is done in the compressed page buffer. */
			io_buffer = block->page.zip.data;
		} else {
			io_buffer = iter.io_buffer;
		}

		/* We have to read the exact number of bytes. Otherwise the
		InnoDB IO functions croak on failed reads. */

		n_bytes = static_cast<ulint>(
			ut_min(static_cast<os_offset_t>(n_bytes),
			       iter.end - offset));

		ut_ad(n_bytes > 0);
		ut_ad(!(n_bytes % iter.page_size));

		if (!os_file_read(iter.file, io_buffer, offset,
				  (ulint) n_bytes)) {

			ib_logf(IB_LOG_LEVEL_ERROR, "os_file_read() failed");

			return(DB_IO_ERROR);
		}

		bool		updated = false;
		os_offset_t	page_off = offset;
		ulint		n_pages_read = (ulint) n_bytes / iter.page_size;

		for (ulint i = 0; i < n_pages_read; ++i) {

			buf_block_set_file_page(block, space_id, page_no++);

			dberr_t	err;

			if ((err = callback(page_off, block)) != DB_SUCCESS) {

				return(err);

			} else if (!updated) {
				updated = buf_block_get_state(block)
					== BUF_BLOCK_FILE_PAGE;
			}

			buf_block_set_state(block, BUF_BLOCK_NOT_USED);
			buf_block_set_state(block, BUF_BLOCK_READY_FOR_USE);

			page_off += iter.page_size;
			block->frame += iter.page_size;
		}

		/* A page was updated in the set, write back to disk. */
		if (updated
		    && !os_file_write(
				iter.filepath, iter.file, io_buffer,
				offset, (ulint) n_bytes)) {

			ib_logf(IB_LOG_LEVEL_ERROR, "os_file_write() failed");

			return(DB_IO_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/********************************************************************//**
Iterate over all the pages in the tablespace.
@param table - the table definiton in the server
@param n_io_buffers - number of blocks to read and write together
@param callback - functor that will do the page updates
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
fil_tablespace_iterate(
/*===================*/
	dict_table_t*	table,
	ulint		n_io_buffers,
	PageCallback&	callback)
{
	dberr_t		err;
	os_file_t	file;
	char*		filepath;

	ut_a(n_io_buffers > 0);
	ut_ad(!srv_read_only_mode);

	DBUG_EXECUTE_IF("ib_import_trigger_corruption_1",
			return(DB_CORRUPTION););

	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		dict_get_and_save_data_dir_path(table, false);
		ut_a(table->data_dir_path);

		filepath = os_file_make_remote_pathname(
			table->data_dir_path, table->name, "ibd");
	} else {
		filepath = fil_make_ibd_name(table->name, false);
	}

	{
		ibool	success;

		file = os_file_create_simple_no_error_handling(
			innodb_file_data_key, filepath,
			OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);

		DBUG_EXECUTE_IF("fil_tablespace_iterate_failure",
		{
			static bool once;

			if (!once || ut_rnd_interval(0, 10) == 5) {
				once = true;
				success = FALSE;
				os_file_close(file);
			}
		});

		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(true);

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Trying to import a tablespace, but could not "
				"open the tablespace file %s", filepath);

			mem_free(filepath);

			return(DB_TABLESPACE_NOT_FOUND);

		} else {
			err = DB_SUCCESS;
		}
	}

	callback.set_file(filepath, file);

	os_offset_t	file_size = os_file_get_size(file);
	ut_a(file_size != (os_offset_t) -1);

	/* The block we will use for every physical page */
	buf_block_t	block;

	memset(&block, 0x0, sizeof(block));

	/* Allocate a page to read in the tablespace header, so that we
	can determine the page size and zip_size (if it is compressed).
	We allocate an extra page in case it is a compressed table. One
	page is to ensure alignement. */

	void*	page_ptr = mem_alloc(3 * UNIV_PAGE_SIZE);
	byte*	page = static_cast<byte*>(ut_align(page_ptr, UNIV_PAGE_SIZE));

	fil_buf_block_init(&block, page);

	/* Read the first page and determine the page and zip size. */

	if (!os_file_read(file, page, 0, UNIV_PAGE_SIZE)) {

		err = DB_IO_ERROR;

	} else if ((err = callback.init(file_size, &block)) == DB_SUCCESS) {
		fil_iterator_t	iter;

		iter.file = file;
		iter.start = 0;
		iter.end = file_size;
		iter.filepath = filepath;
		iter.file_size = file_size;
		iter.n_io_buffers = n_io_buffers;
		iter.page_size = callback.get_page_size();

		/* Compressed pages can't be optimised for block IO for now.
		We do the IMPORT page by page. */

		if (callback.get_zip_size() > 0) {
			iter.n_io_buffers = 1;
			ut_a(iter.page_size == callback.get_zip_size());
		}

		/** Add an extra page for compressed page scratch area. */

		void*	io_buffer = mem_alloc(
			(2 + iter.n_io_buffers) * UNIV_PAGE_SIZE);

		iter.io_buffer = static_cast<byte*>(
			ut_align(io_buffer, UNIV_PAGE_SIZE));

		err = fil_iterate(iter, &block, callback);

		mem_free(io_buffer);
	}

	if (err == DB_SUCCESS) {

		ib_logf(IB_LOG_LEVEL_INFO, "Sync to disk");

		if (!os_file_flush(file)) {
			ib_logf(IB_LOG_LEVEL_INFO, "os_file_flush() failed!");
			err = DB_IO_ERROR;
		} else {
			ib_logf(IB_LOG_LEVEL_INFO, "Sync to disk - done!");
		}
	}

	os_file_close(file);

	mem_free(page_ptr);
	mem_free(filepath);

	return(err);
}

/**
Set the tablespace compressed table size.
@return DB_SUCCESS if it is valie or DB_CORRUPTION if not */
dberr_t
PageCallback::set_zip_size(const buf_frame_t* page) UNIV_NOTHROW
{
	m_zip_size = fsp_header_get_zip_size(page);

	if (!ut_is_2pow(m_zip_size) || m_zip_size > UNIV_ZIP_SIZE_MAX) {
		return(DB_CORRUPTION);
	}

	return(DB_SUCCESS);
}

/********************************************************************//**
Delete the tablespace file and any related files like .cfg.
This should not be called for temporary tables. */
UNIV_INTERN
void
fil_delete_file(
/*============*/
	const char*	ibd_name)	/*!< in: filepath of the ibd
					tablespace */
{
	/* Force a delete of any stale .ibd files that are lying around. */

	ib_logf(IB_LOG_LEVEL_INFO, "Deleting %s", ibd_name);

	os_file_delete_if_exists(ibd_name);

	char*	cfg_name = fil_make_cfg_name(ibd_name);

	os_file_delete_if_exists(cfg_name);

	mem_free(cfg_name);
}

/**
Iterate over all the spaces in the space list and fetch the
tablespace names. It will return a copy of the name that must be
freed by the caller using: delete[].
@return DB_SUCCESS if all OK. */
UNIV_INTERN
dberr_t
fil_get_space_names(
/*================*/
	space_name_list_t&	space_name_list)
				/*!< in/out: List to append to */
{
	fil_space_t*	space;
	dberr_t		err = DB_SUCCESS;

	mutex_enter(&fil_system->mutex);

	for (space = UT_LIST_GET_FIRST(fil_system->space_list);
	     space != NULL;
	     space = UT_LIST_GET_NEXT(space_list, space)) {

		if (space->purpose == FIL_TABLESPACE) {
			ulint	len;
			char*	name;

			len = strlen(space->name);
			name = new(std::nothrow) char[len + 1];

			if (name == 0) {
				/* Caller to free elements allocated so far. */
				err = DB_OUT_OF_MEMORY;
				break;
			}

			memcpy(name, space->name, len);
			name[len] = 0;

			space_name_list.push_back(name);
		}
	}

	mutex_exit(&fil_system->mutex);

	return(err);
}

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
	const char*	tmp_name,	/*!< in: temp table name used while
					swapping */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	if (old_space_id != TRX_SYS_SPACE) {
		fil_op_write_log(MLOG_FILE_RENAME, old_space_id,
				 0, 0, old_name, tmp_name, mtr);
	}

	if (new_space_id != TRX_SYS_SPACE) {
		fil_op_write_log(MLOG_FILE_RENAME, new_space_id,
				 0, 0, new_name, old_name, mtr);
	}
}
