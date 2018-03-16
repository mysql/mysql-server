/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"

#ifndef UNIV_HOTBACKUP
#include "btr0btr.h"
#include "buf0buf.h"
#include "dict0boot.h"
#include "dict0dict.h"
#include "fsp0file.h"
#include "fsp0fsp.h"
#include "fsp0space.h"
#include "fsp0sysspace.h"
#include "hash0hash.h"
#include "log0recv.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "mtr0log.h"
#include "os0file.h"
#include "page0zip.h"
#include "row0mysql.h"
#include "row0trunc.h"
# include "buf0lru.h"
# include "ibuf0ibuf.h"
# include "os0event.h"
# include "sync0sync.h"
#endif /* !UNIV_HOTBACKUP */
#include "buf0flu.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "ut0new.h"
#include "btr0sea.h"
#include "log0log.h"

/** Tries to close a file in the LRU list. The caller must hold the fil_sys
mutex.
@return true if success, false if should retry later; since i/o's
generally complete in < 100 ms, and as InnoDB writes at most 128 pages
from the buffer pool in a batch, and then immediately flushes the
files, there is a good chance that the next time we find a suitable
node from the LRU list.
@param[in] print_info	if true, prints information why it
                        cannot close a file */
static
bool
fil_try_to_close_file_in_LRU(bool print_info);

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

/** This tablespace name is used internally during recovery to open a
general tablespace before the data dictionary are recovered and available. */
const char general_space_name[] = "innodb_general";

/** Reference to the server data directory. Usually it is the
current working directory ".", but in the MySQL Embedded Server Library
it is an absolute path. */
const char*	fil_path_to_mysql_datadir;
Folder		folder_mysql_datadir;

/** Common InnoDB file extentions */
const char* dot_ext[] = { "", ".ibd", ".isl", ".cfg", ".cfp" };

/** The number of fsyncs done to the log */
ulint	fil_n_log_flushes			= 0;

/** Number of pending redo log flushes */
ulint	fil_n_pending_log_flushes		= 0;
/** Number of pending tablespace flushes */
ulint	fil_n_pending_tablespace_flushes	= 0;

/** Number of files currently open */
ulint	fil_n_file_opened			= 0;

/** The null file address */
fil_addr_t	fil_addr_null = {FIL_NULL, 0};

/** The tablespace memory cache; also the totality of logs (the log
data space) is stored here; below we talk about tablespaces, but also
the ib_logfiles form a 'space' and it is handled here */
struct fil_system_t {
#ifndef UNIV_HOTBACKUP
	ib_mutex_t	mutex;		/*!< The mutex protecting the cache */
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
	int64_t		modification_counter;/*!< when we write to a file we
					increment this by one */
	ulint		max_assigned_id;/*!< maximum space id in the existing
					tables, or assigned during the time
					mysqld has been up; at an InnoDB
					startup we scan the data dictionary
					and set here the maximum of the
					space id's of the tables there */
	UT_LIST_BASE_NODE_T(fil_space_t) space_list;
					/*!< list of all file spaces */
	UT_LIST_BASE_NODE_T(fil_space_t) named_spaces;
					/*!< list of all file spaces
					for which a MLOG_FILE_NAME
					record has been written since
					the latest redo log checkpoint.
					Protected only by log_sys->mutex. */
	bool		space_id_reuse_warned;
					/* !< true if fil_space_create()
					has issued a warning about
					potential space_id reuse */
};

/** The tablespace memory cache. This variable is NULL before the module is
initialized. */
static fil_system_t*	fil_system	= NULL;

#ifdef UNIV_HOTBACKUP
static ulint	srv_data_read;
static ulint	srv_data_written;
#endif /* UNIV_HOTBACKUP */

/** Determine if user has explicitly disabled fsync(). */
#ifndef _WIN32
# define fil_buffering_disabled(s)	\
	((s)->purpose == FIL_TYPE_TABLESPACE	\
	 && srv_unix_file_flush_method	\
	 == SRV_UNIX_O_DIRECT_NO_FSYNC)
#else /* _WIN32 */
# define fil_buffering_disabled(s)	(0)
#endif /* __WIN32 */

/** Determine if the space id is a user tablespace id or not.
@param[in]	space_id	Space ID to check
@return true if it is a user tablespace ID */
UNIV_INLINE
bool
fil_is_user_tablespace_id(
	ulint	space_id)
{
	return(!srv_is_undo_tablespace(space_id)
	       && space_id != srv_tmp_space.space_id());
}

#ifdef UNIV_DEBUG
/** Try fil_validate() every this many times */
# define FIL_VALIDATE_SKIP	17

/******************************************************************//**
Checks the consistency of the tablespace cache some of the time.
@return true if ok or the check was skipped */
static
bool
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
		return(true);
	}

	fil_validate_count = FIL_VALIDATE_SKIP;
	return(fil_validate());
}
#endif /* UNIV_DEBUG */

/********************************************************************//**
Determines if a file node belongs to the least-recently-used list.
@return true if the file belongs to fil_system->LRU mutex. */
UNIV_INLINE
bool
fil_space_belongs_in_lru(
/*=====================*/
	const fil_space_t*	space)	/*!< in: file space */
{
	switch (space->purpose) {
	case FIL_TYPE_LOG:
		return(false);
	case FIL_TYPE_TABLESPACE:
	case FIL_TYPE_TEMPORARY:
	case FIL_TYPE_IMPORT:
		return(fil_is_user_tablespace_id(space->id));
	}

	ut_ad(0);
	return(false);
}

/********************************************************************//**
NOTE: you must call fil_mutex_enter_and_prepare_for_io() first!

Prepares a file node for i/o. Opens the file if it is closed. Updates the
pending i/o's field in the node and the system appropriately. Takes the node
off the LRU list if it is in the LRU list. The caller must hold the fil_sys
mutex.
@return false if the file can't be opened, otherwise true */
static
bool
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space);	/*!< in: space */

/**
Updates the data structures when an i/o operation finishes. Updates the
pending i/o's field in the node appropriately.
@param[in,out] node		file node
@param[in,out] system		tablespace instance
@param[in] type			IO context */
static
void
fil_node_complete_io(
	fil_node_t*		node,
	fil_system_t*		system,
	const IORequest&	type);

/** Reads data from a space to a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	byte_offset	remainder of offset in bytes; in aio this
must be divisible by the OS block size
@param[in]	len		how many bytes to read; this must not cross a
file boundary; in aio this must be a block size multiple
@param[in,out]	buf		buffer where to store data read; in aio this
must be appropriately aligned
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
UNIV_INLINE
dberr_t
fil_read(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ulint			byte_offset,
	ulint			len,
	void*			buf)
{
	return(fil_io(IORequestRead, true, page_id, page_size,
		      byte_offset, len, buf, NULL));
}

/** Writes data to a space from a buffer. Remember that the possible incomplete
blocks at the end of file are ignored: they are not taken into account when
calculating the byte offset within a space.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	byte_offset	remainder of offset in bytes; in aio this
must be divisible by the OS block size
@param[in]	len		how many bytes to write; this must not cross
a file boundary; in aio this must be a block size multiple
@param[in]	buf		buffer from which to write; in aio this must
be appropriately aligned
@return DB_SUCCESS, or DB_TABLESPACE_DELETED if we are trying to do
i/o on a tablespace which does not exist */
UNIV_INLINE
dberr_t
fil_write(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ulint			byte_offset,
	ulint			len,
	void*			buf)
{
	ut_ad(!srv_read_only_mode);

	return(fil_io(IORequestWrite, true, page_id, page_size,
		      byte_offset, len, buf, NULL));
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

/** Look up a tablespace.
The caller should hold an InnoDB table lock or a MDL that prevents
the tablespace from being dropped during the operation,
or the caller should be in single-threaded crash recovery mode
(no user connections that could drop tablespaces).
If this is not the case, fil_space_acquire() and fil_space_release()
should be used instead.
@param[in]	id	tablespace ID
@return tablespace, or NULL if not found */
fil_space_t*
fil_space_get(
	ulint	id)
{
	mutex_enter(&fil_system->mutex);
	fil_space_t*	space = fil_space_get_by_id(id);
	mutex_exit(&fil_system->mutex);
	ut_ad(space == NULL || space->purpose != FIL_TYPE_LOG);
	return(space);
}
#ifndef UNIV_HOTBACKUP
/** Returns the latch of a file space.
@param[in]	id	space id
@param[out]	flags	tablespace flags
@return latch protecting storage allocation */
rw_lock_t*
fil_space_get_latch(
	ulint	id,
	ulint*	flags)
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

#ifdef UNIV_DEBUG
/** Gets the type of a file space.
@param[in]	id	tablespace identifier
@return file type */
fil_type_t
fil_space_get_type(
	ulint	id)
{
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	mutex_exit(&fil_system->mutex);

	return(space->purpose);
}
#endif /* UNIV_DEBUG */

/** Note that a tablespace has been imported.
It is initially marked as FIL_TYPE_IMPORT so that no logging is
done during the import process when the space ID is stamped to each page.
Now we change it to FIL_SPACE_TABLESPACE to start redo and undo logging.
NOTE: temporary tablespaces are never imported.
@param[in]	id	tablespace identifier */
void
fil_space_set_imported(
	ulint	id)
{
	ut_ad(fil_system != NULL);

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(id);

	ut_ad(space->purpose == FIL_TYPE_IMPORT);
	space->purpose = FIL_TYPE_TABLESPACE;

	mutex_exit(&fil_system->mutex);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Checks if all the file nodes in a space are flushed. The caller must hold
the fil_system mutex.
@return true if all are flushed */
static
bool
fil_space_is_flushed(
/*=================*/
	fil_space_t*	space)	/*!< in: space */
{
	ut_ad(mutex_own(&fil_system->mutex));

	for (const fil_node_t* node = UT_LIST_GET_FIRST(space->chain);
	     node != NULL;
	     node = UT_LIST_GET_NEXT(chain, node)) {

		if (node->modification_counter > node->flush_counter) {

			ut_ad(!fil_buffering_disabled(space));
			return(false);
		}
	}

	return(true);
}

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)

#include <sys/ioctl.h>
/** FusionIO atomic write control info */
#define DFS_IOCTL_ATOMIC_WRITE_SET	_IOW(0x95, 2, uint)

/**
Try and enable FusionIO atomic writes.
@param[in] file		OS file handle
@return true if successful */
bool
fil_fusionio_enable_atomic_write(pfs_os_file_t file)
{
	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {

		uint	atomic = 1;
		ut_a(file.m_file != -1);
		if (ioctl(file.m_file, DFS_IOCTL_ATOMIC_WRITE_SET, &atomic) != -1) {

			return(true);
		}
	}

	return(false);
}
#endif /* !NO_FALLOCATE && UNIV_LINUX */

/** Append a file to the chain of files of a space.
@param[in]	name		file name of a file that is not open
@param[in]	size		file size in entire database blocks
@param[in,out]	space		tablespace from fil_space_create()
@param[in]	is_raw		whether this is a raw device or partition
@param[in]	punch_hole	true if supported for this node
@param[in]	atomic_write	true if the file has atomic write enabled
@param[in]	max_pages	maximum number of pages in file,
ULINT_MAX means the file size is unlimited.
@return pointer to the file name
@retval NULL if error */
static
fil_node_t*
fil_node_create_low(
	const char*	name,
	ulint		size,
	fil_space_t*	space,
	bool		is_raw,
	bool		punch_hole,
	bool		atomic_write,
	ulint		max_pages = ULINT_MAX)
{
	fil_node_t*	node;

	ut_ad(name != NULL);
	ut_ad(fil_system != NULL);

	if (space == NULL) {
		return(NULL);
	}

	node = reinterpret_cast<fil_node_t*>(ut_zalloc_nokey(sizeof(*node)));

	node->name = mem_strdup(name);

	ut_a(!is_raw || srv_start_raw_disk_in_use);

	node->sync_event = os_event_create("fsync_event");

	node->is_raw_disk = is_raw;

	node->size = size;

	node->magic_n = FIL_NODE_MAGIC_N;

	node->init_size = size;
	node->max_size = max_pages;

	mutex_enter(&fil_system->mutex);

	space->size += size;

	node->space = space;

	os_file_stat_t	stat_info;

#ifdef UNIV_DEBUG
	dberr_t err =
#endif /* UNIV_DEBUG */

	os_file_get_status(
		node->name, &stat_info, false,
		fsp_is_system_temporary(space->id) ? true : srv_read_only_mode);

	ut_ad(err == DB_SUCCESS);

	node->block_size = stat_info.block_size;

	/* In this debugging mode, we can overcome the limitation of some
	OSes like Windows that support Punch Hole but have a hole size
	effectively too large.  By setting the block size to be half the
	page size, we can bypass one of the checks that would normally
	turn Page Compression off.  This execution mode allows compression
	to be tested even when full punch hole support is not available. */
	DBUG_EXECUTE_IF("ignore_punch_hole",
		node->block_size = ut_min(stat_info.block_size,
					  static_cast<size_t>(UNIV_PAGE_SIZE / 2));
	);

	if (!IORequest::is_punch_hole_supported()
	    || !punch_hole
	    || node->block_size >= srv_page_size) {

		fil_no_punch_hole(node);
	} else {
		node->punch_hole = punch_hole;
	}

	node->atomic_write = atomic_write;

	UT_LIST_ADD_LAST(space->chain, node);
	mutex_exit(&fil_system->mutex);

	return(node);
}

/** Appends a new file to the chain of files of a space. File must be closed.
@param[in]	name		file name (file must be closed)
@param[in]	size		file size in database blocks, rounded downwards to
				an integer
@param[in,out]	space		space where to append
@param[in]	is_raw		true if a raw device or a raw disk partition
@param[in]	atomic_write	true if the file has atomic write enabled
@param[in]	max_pages	maximum number of pages in file,
ULINT_MAX means the file size is unlimited.
@return pointer to the file name
@retval NULL if error */
char*
fil_node_create(
	const char*	name,
	ulint		size,
	fil_space_t*	space,
	bool		is_raw,
	bool		atomic_write,
	ulint		max_pages)
{
	fil_node_t*	node;

	node = fil_node_create_low(
		name, size, space, is_raw, IORequest::is_punch_hole_supported(),
		atomic_write, max_pages);

	return(node == NULL ? NULL : node->name);
}

/** Open a file node of a tablespace.
The caller must own the fil_system mutex.
@param[in,out]	node	File node
@return false if the file can't be opened, otherwise true */
static
bool
fil_node_open_file(
	fil_node_t*	node)
{
	os_offset_t	size_bytes;
	bool		success;
	byte*		buf2;
	byte*		page;
	ulint		space_id;
	ulint		flags;
	ulint		min_size;
	bool		read_only_mode;
	fil_space_t*	space = node->space;

	ut_ad(mutex_own(&fil_system->mutex));
	ut_a(node->n_pending == 0);
	ut_a(!node->is_open);

	read_only_mode = !fsp_is_system_temporary(space->id)
		&& srv_read_only_mode;

	if (node->size == 0
	    || (space->purpose == FIL_TYPE_TABLESPACE
		&& node == UT_LIST_GET_FIRST(space->chain)
		&& !undo::Truncate::was_tablespace_truncated(space->id)
		&& srv_startup_is_before_trx_rollback_phase)) {
		/* We do not know the size of the file yet. First we
		open the file in the normal mode, no async I/O here,
		for simplicity. Then do some checks, and close the
		file again.  NOTE that we could not use the simple
		file read function os_file_read() in Windows to read
		from a file opened for async I/O! */

retry:
		node->handle = os_file_create_simple_no_error_handling(
			innodb_data_file_key, node->name, OS_FILE_OPEN,
			OS_FILE_READ_ONLY, read_only_mode, &success);

		if (!success) {
			/* The following call prints an error message */
			ulint err = os_file_get_last_error(true);
			if (err == EMFILE + 100) {
				if (fil_try_to_close_file_in_LRU(true))
					goto retry;
                       }

			ib::warn() << "Cannot open '" << node->name << "'."
				" Have you deleted .ibd files under a"
				" running mysqld server?";

			return(false);
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
		ut_a(space->purpose != FIL_TYPE_LOG);

		/* Read the first page of the tablespace */

		buf2 = static_cast<byte*>(ut_malloc_nokey(2 * UNIV_PAGE_SIZE));

		/* Align the memory for file i/o if we might have O_DIRECT
		set */
		page = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));
		ut_ad(page == page_align(page));

		IORequest	request(IORequest::READ);

		success = os_file_read(
			request,
			node->handle, page, 0, UNIV_PAGE_SIZE);

		space_id = fsp_header_get_space_id(page);
		flags = fsp_header_get_flags(page);

		/* Close the file now that we have read the space id from it */

		os_file_close(node->handle);

		const page_size_t	page_size(flags);

		min_size = FIL_IBD_FILE_INITIAL_SIZE * page_size.physical();

		if (size_bytes < min_size) {

			ib::error() << "The size of tablespace file "
				<< node->name << " is only " << size_bytes
				<< ", should be at least " << min_size << "!";

			ut_error;
		}

		if (space_id != space->id) {
			ib::fatal() << "Tablespace id is " << space->id
				<< " in the data dictionary but in file "
				<< node->name << " it is " << space_id << "!";
		}

		const page_size_t	space_page_size(space->flags);

		if (!page_size.equals_to(space_page_size)) {
			ib::fatal() << "Tablespace file " << node->name
				<< " has page size " << page_size
				<< " (flags=" << ib::hex(flags) << ") but the"
				" data dictionary expects page size "
				<< space_page_size << " (flags="
				<< ib::hex(space->flags) << ")!";
		}

		if (space->flags != flags) {

			ib::fatal()
				<< "Table flags are "
				<< ib::hex(space->flags) << " in the data"
				" dictionary but the flags in file "
				<< node->name << " are " << ib::hex(flags)
				<< "!";
		}

		{
			ulint	size		= fsp_header_get_field(
				page, FSP_SIZE);
			ulint	free_limit	= fsp_header_get_field(
				page, FSP_FREE_LIMIT);
			ulint	free_len	= flst_get_len(
				FSP_HEADER_OFFSET + FSP_FREE + page);
			ut_ad(space->free_limit == 0
			      || space->free_limit == free_limit);
			ut_ad(space->free_len == 0
			      || space->free_len == free_len);
			space->size_in_header = size;
			space->free_limit = free_limit;
			space->free_len = free_len;
		}

		ut_free(buf2);

		/* For encrypted tablespace, we need to check the
		encrytion key and iv(initial vector) is readed. */
		if (FSP_FLAGS_GET_ENCRYPTION(flags)
		    && !recv_recovery_is_on()) {
			if (space->encryption_type != Encryption::AES) {
				ib::error()
					<< "Can't read encryption"
					<< " key from file "
					<< node->name << "!";
				return(false);
			}
		}

		if (node->size == 0) {
			ulint	extent_size;

			extent_size = page_size.physical() * FSP_EXTENT_SIZE;

			/* After apply-incremental, tablespaces are not extended
			to a whole megabyte. Do not cut off valid data. */
#ifndef UNIV_HOTBACKUP
			/* Truncate the size to a multiple of extent size. */
			if (size_bytes >= extent_size) {
				size_bytes = ut_2pow_round(size_bytes,
							   extent_size);
			}
#endif /* !UNIV_HOTBACKUP */
			node->size = (ulint)
				(size_bytes / page_size.physical());

#ifdef UNIV_HOTBACKUP
add_size:
#endif /* UNIV_HOTBACKUP */
			space->size += node->size;
		}
	}

	/* printf("Opening file %s\n", node->name); */

	/* Open the file for reading and writing, in Windows normally in the
	unbuffered async I/O mode, though global variables may make
	os_file_create() to fall back to the normal file I/O mode. */

	if (space->purpose == FIL_TYPE_LOG) {
		node->handle = os_file_create(
			innodb_log_file_key, node->name, OS_FILE_OPEN,
			OS_FILE_AIO, OS_LOG_FILE, read_only_mode, &success);
	} else if (node->is_raw_disk) {
		node->handle = os_file_create(
			innodb_data_file_key, node->name, OS_FILE_OPEN_RAW,
			OS_FILE_AIO, OS_DATA_FILE, read_only_mode, &success);
	} else {
		node->handle = os_file_create(
			innodb_data_file_key, node->name, OS_FILE_OPEN,
			OS_FILE_AIO, OS_DATA_FILE, read_only_mode, &success);
	}

	ut_a(success);

	node->is_open = true;

	fil_system->n_open++;
	fil_n_file_opened++;

	if (fil_space_belongs_in_lru(space)) {

		/* Put the node to the LRU list */
		UT_LIST_ADD_FIRST(fil_system->LRU, node);
	}

	return(true);
}

/** Close a file node.
@param[in,out]	node	File node */
static
void
fil_node_close_file(
	fil_node_t*	node)
{
	bool	ret;

	ut_ad(mutex_own(&(fil_system->mutex)));
	ut_a(node->is_open);
	ut_a(node->n_pending == 0);
	ut_a(node->n_pending_flushes == 0);
	ut_a(!node->being_extended);
#ifndef UNIV_HOTBACKUP
	ut_a(node->modification_counter == node->flush_counter
	     || node->space->purpose == FIL_TYPE_TEMPORARY
	     || srv_fast_shutdown == 2);
#endif /* !UNIV_HOTBACKUP */

	ret = os_file_close(node->handle);
	ut_a(ret);

	/* printf("Closing file %s\n", node->name); */

	node->is_open = false;
	ut_a(fil_system->n_open > 0);
	fil_system->n_open--;
	fil_n_file_opened--;

	if (fil_space_belongs_in_lru(node->space)) {

		ut_a(UT_LIST_GET_LEN(fil_system->LRU) > 0);

		/* The node is in the LRU list, remove it */
		UT_LIST_REMOVE(fil_system->LRU, node);
	}
}

/** Tries to close a file in the LRU list. The caller must hold the fil_sys
mutex.
@return true if success, false if should retry later; since i/o's
generally complete in < 100 ms, and as InnoDB writes at most 128 pages
from the buffer pool in a batch, and then immediately flushes the
files, there is a good chance that the next time we find a suitable
node from the LRU list.
@param[in] print_info	if true, prints information why it
			cannot close a file*/
static
bool
fil_try_to_close_file_in_LRU(

	bool	print_info)
{
	fil_node_t*	node;

	ut_ad(mutex_own(&fil_system->mutex));

	if (print_info) {
		ib::info() << "fil_sys open file LRU len "
			<< UT_LIST_GET_LEN(fil_system->LRU);
	}

	for (node = UT_LIST_GET_LAST(fil_system->LRU);
	     node != NULL;
	     node = UT_LIST_GET_PREV(LRU, node)) {

		if (node->modification_counter == node->flush_counter
		    && node->n_pending_flushes == 0
		    && !node->being_extended) {

			fil_node_close_file(node);

			return(true);
		}

		if (!print_info) {
			continue;
		}

		if (node->n_pending_flushes > 0) {

			ib::info() << "Cannot close file " << node->name
				<< ", because n_pending_flushes "
				<< node->n_pending_flushes;
		}

		if (node->modification_counter != node->flush_counter) {
			ib::warn() << "Cannot close file " << node->name
				<< ", because modification count "
				<< node->modification_counter <<
				" != flush count " << node->flush_counter;
		}

		if (node->being_extended) {
			ib::info() << "Cannot close file " << node->name
				<< ", because it is being extended";
		}
	}

	return(false);
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
	bool		success;
	bool		print_info	= false;
	ulint		count		= 0;
	ulint		count2		= 0;

	for (;;) {
		mutex_enter(&fil_system->mutex);

		if (space_id == 0 || space_id >= SRV_LOG_SPACE_FIRST_ID) {
			/* We keep log files and system tablespace files always
			open; this is important in preventing deadlocks in this
			module, as a page read completion often performs
			another read from the insert buffer. The insert buffer
			is in tablespace 0, and we cannot end up waiting in
			this function. */
			return;
		}

		space = fil_space_get_by_id(space_id);

		if (space != NULL && space->stop_ios) {
			/* We are going to do a rename file and want to stop
			new i/o's for a while. */

			if (count2 > 20000) {
				ib::warn() << "Tablespace " << space->name
					<< " has i/o ops stopped for a long"
					" time " << count2;
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
			fil_flush_file_spaces(FIL_TYPE_TABLESPACE);

			os_thread_sleep(20000);

			count2++;

			continue;
		}

		if (fil_system->n_open < fil_system->max_n_open) {

			return;
		}

		/* If the file is already open, no need to do anything; if the
		space does not exist, we handle the situation in the function
		which called this function. */

		if (space == NULL || UT_LIST_GET_FIRST(space->chain)->is_open) {

			return;
		}

		if (count > 1) {
			print_info = true;
		}

		/* Too many files are open, try to close some */
		do {
			success = fil_try_to_close_file_in_LRU(print_info);

		} while (success
			 && fil_system->n_open >= fil_system->max_n_open);

		if (fil_system->n_open < fil_system->max_n_open) {
			/* Ok */
			return;
		}

		if (count >= 2) {
			ib::warn() << "Too many (" << fil_system->n_open
				<< ") files stay open while the maximum"
				" allowed value would be "
				<< fil_system->max_n_open << ". You may need"
				" to raise the value of innodb_open_files in"
				" my.cnf.";

			return;
		}

		mutex_exit(&fil_system->mutex);

#ifndef UNIV_HOTBACKUP
		/* Wake the i/o-handler threads to make sure pending i/o's are
		performed */
		os_aio_simulated_wake_handler_threads();

		os_thread_sleep(20000);
#endif /* !UNIV_HOTBACKUP */
		/* Flush tablespaces so that we can close modified files in
		the LRU list. */

		fil_flush_file_spaces(FIL_TYPE_TABLESPACE);

		count++;
	}
}

/** Prepare to free a file node object from a tablespace memory cache.
@param[in,out]	node	file node
@param[in]	space	tablespace */
static
void
fil_node_close_to_free(
	fil_node_t*	node,
	fil_space_t*	space)
{
	ut_ad(mutex_own(&fil_system->mutex));
	ut_a(node->magic_n == FIL_NODE_MAGIC_N);
	ut_a(node->n_pending == 0);
	ut_a(!node->being_extended);

	if (node->is_open) {
		/* We fool the assertion in fil_node_close_file() to think
		there are no unflushed modifications in the file */

		node->modification_counter = node->flush_counter;
		os_event_set(node->sync_event);

		if (fil_buffering_disabled(space)) {

			ut_ad(!space->is_in_unflushed_spaces);
			ut_ad(fil_space_is_flushed(space));

		} else if (space->is_in_unflushed_spaces
			   && fil_space_is_flushed(space)) {

			space->is_in_unflushed_spaces = false;

			UT_LIST_REMOVE(fil_system->unflushed_spaces, space);
		}

		fil_node_close_file(node);
	}
}

/** Detach a space object from the tablespace memory cache.
Closes the files in the chain but does not delete them.
There must not be any pending i/o's or flushes on the files.
@param[in,out]	space		tablespace */
static
void
fil_space_detach(
	fil_space_t*	space)
{
	ut_ad(mutex_own(&fil_system->mutex));

	HASH_DELETE(fil_space_t, hash, fil_system->spaces, space->id, space);

	fil_space_t*	fnamespace = fil_space_get_by_name(space->name);

	ut_a(space == fnamespace);

	HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(space->name), space);

	if (space->is_in_unflushed_spaces) {

		ut_ad(!fil_buffering_disabled(space));
		space->is_in_unflushed_spaces = false;

		UT_LIST_REMOVE(fil_system->unflushed_spaces, space);
	}

	UT_LIST_REMOVE(fil_system->space_list, space);

	ut_a(space->magic_n == FIL_SPACE_MAGIC_N);
	ut_a(space->n_pending_flushes == 0);

	for (fil_node_t* fil_node = UT_LIST_GET_FIRST(space->chain);
	     fil_node != NULL;
	     fil_node = UT_LIST_GET_NEXT(chain, fil_node)) {

		fil_node_close_to_free(fil_node, space);
	}
}

/** Free a tablespace object on which fil_space_detach() was invoked.
There must not be any pending i/o's or flushes on the files.
@param[in,out]	space		tablespace */
static
void
fil_space_free_low(
	fil_space_t*	space)
{
	/* The tablespace must not be in fil_system->named_spaces. */
	ut_ad(srv_fast_shutdown == 2 || space->max_lsn == 0);

	for (fil_node_t* node = UT_LIST_GET_FIRST(space->chain);
	     node != NULL; ) {
		ut_d(space->size -= node->size);
		os_event_destroy(node->sync_event);
		ut_free(node->name);
		fil_node_t* old_node = node;
		node = UT_LIST_GET_NEXT(chain, node);
		ut_free(old_node);
	}

	ut_ad(space->size == 0);

	rw_lock_free(&space->latch);

	ut_free(space->name);
	ut_free(space);
}

/** Frees a space object from the tablespace memory cache.
Closes the files in the chain but does not delete them.
There must not be any pending i/o's or flushes on the files.
@param[in]	id		tablespace identifier
@param[in]	x_latched	whether the caller holds X-mode space->latch
@return true if success */
bool
fil_space_free(
	ulint		id,
	bool		x_latched)
{
	ut_ad(id != TRX_SYS_SPACE);

	mutex_enter(&fil_system->mutex);
	fil_space_t*	space = fil_space_get_by_id(id);

	if (space != NULL) {
		fil_space_detach(space);
	}

	mutex_exit(&fil_system->mutex);

	if (space != NULL) {
		if (x_latched) {
			rw_lock_x_unlock(&space->latch);
		}

		bool	need_mutex = !recv_recovery_on;

		if (need_mutex) {
			log_mutex_enter();
		}

		ut_ad(log_mutex_own());

		if (space->max_lsn != 0) {
			ut_d(space->max_lsn = 0);
			UT_LIST_REMOVE(fil_system->named_spaces, space);
		}

		if (need_mutex) {
			log_mutex_exit();
		}

		fil_space_free_low(space);
	}

	return(space != NULL);
}

/** Create a space memory object and put it to the fil_system hash table.
The tablespace name is independent from the tablespace file-name.
Error messages are issued to the server log.
@param[in]	name	Tablespace name
@param[in]	id	Tablespace identifier
@param[in]	flags	Tablespace flags
@param[in]	purpose	Tablespace purpose
@return pointer to created tablespace, to be filled in with fil_node_create()
@retval NULL on failure (such as when the same tablespace exists) */
fil_space_t*
fil_space_create(
	const char*	name,
	ulint		id,
	ulint		flags,
	fil_type_t	purpose)
{
	fil_space_t*	space;

	ut_ad(fil_system);
	ut_ad(fsp_flags_is_valid(flags));
	ut_ad(srv_page_size == UNIV_PAGE_SIZE_ORIG || flags != 0);

	DBUG_EXECUTE_IF("fil_space_create_failure", return(NULL););

	mutex_enter(&fil_system->mutex);

	/* Look for a matching tablespace. */
	space = fil_space_get_by_name(name);

	if (space != NULL) {
		mutex_exit(&fil_system->mutex);

		ib::warn() << "Tablespace '" << name << "' exists in the"
			" cache with id " << space->id << " != " << id;

		return(NULL);
	}

	space = fil_space_get_by_id(id);

	if (space != NULL) {
		ib::error() << "Trying to add tablespace '" << name
			<< "' with id " << id
			<< " to the tablespace memory cache, but tablespace '"
			<< space->name << "' already exists in the cache!";
		mutex_exit(&fil_system->mutex);
		return(NULL);
	}

	space = static_cast<fil_space_t*>(ut_zalloc_nokey(sizeof(*space)));

	space->id = id;
	space->name = mem_strdup(name);

	UT_LIST_INIT(space->chain, &fil_node_t::chain);

	/* This warning is not applicable while MEB scanning the redo logs */
#ifndef UNIV_HOTBACKUP
	if (fil_type_is_data(purpose)
	    && !recv_recovery_on
	    && id > fil_system->max_assigned_id) {

		if (!fil_system->space_id_reuse_warned) {
			fil_system->space_id_reuse_warned = true;

			ib::warn() << "Allocated tablespace ID " << id
				<< " for " << name << ", old maximum was "
				<< fil_system->max_assigned_id;
		}

		fil_system->max_assigned_id = id;
	}
#endif /* !UNIV_HOTBACKUP */
	space->purpose = purpose;
	space->flags = flags;

	space->magic_n = FIL_SPACE_MAGIC_N;

	space->encryption_type = Encryption::NONE;

	rw_lock_create(fil_space_latch_key, &space->latch, SYNC_FSP);

	if (space->purpose == FIL_TYPE_TEMPORARY) {
#ifndef UNIV_HOTBACKUP
		ut_d(space->latch.set_temp_fsp());
#endif /* !UNIV_HOTBACKUP */
	}

	HASH_INSERT(fil_space_t, hash, fil_system->spaces, id, space);

	HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
		    ut_fold_string(name), space);

	UT_LIST_ADD_LAST(fil_system->space_list, space);

	if (id < SRV_LOG_SPACE_FIRST_ID && id > fil_system->max_assigned_id) {

		fil_system->max_assigned_id = id;
	}

	mutex_exit(&fil_system->mutex);

	return(space);
}

/*******************************************************************//**
Assigns a new space id for a new single-table tablespace. This works simply by
incrementing the global counter. If 4 billion id's is not enough, we may need
to recycle id's.
@return true if assigned, false if not */
bool
fil_assign_new_space_id(
/*====================*/
	ulint*	space_id)	/*!< in/out: space id */
{
	ulint	id;
	bool	success;

	mutex_enter(&fil_system->mutex);

	id = *space_id;

	if (id < fil_system->max_assigned_id) {
		id = fil_system->max_assigned_id;
	}

	id++;

	if (id > (SRV_LOG_SPACE_FIRST_ID / 2) && (id % 1000000UL == 0)) {
		ib::warn() << "You are running out of new single-table"
			" tablespace id's. Current counter is " << id
			<< " and it must not exceed" << SRV_LOG_SPACE_FIRST_ID
			<< "! To reset the counter to zero you have to dump"
			" all your tables and recreate the whole InnoDB"
			" installation.";
	}

	success = (id < SRV_LOG_SPACE_FIRST_ID);

	if (success) {
		*space_id = fil_system->max_assigned_id = id;
	} else {
		ib::warn() << "You have run out of single-table tablespace"
			" id's! Current counter is " << id
			<< ". To reset the counter to zero"
			" you have to dump all your tables and"
			" recreate the whole InnoDB installation.";
		*space_id = ULINT_UNDEFINED;
	}

	mutex_exit(&fil_system->mutex);

	return(success);
}

/*******************************************************************//**
Returns a pointer to the fil_space_t that is in the memory cache
associated with a space id. The caller must lock fil_system->mutex.
@return file_space_t pointer, NULL if space not found */
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
	if (space == NULL || space->size != 0) {
		return(space);
	}

	switch (space->purpose) {
	case FIL_TYPE_LOG:
		break;
	case FIL_TYPE_TEMPORARY:
	case FIL_TYPE_TABLESPACE:
	case FIL_TYPE_IMPORT:
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

		if (!fil_node_prepare_for_io(node, fil_system, space)) {
			/* The single-table tablespace can't be opened,
			because the ibd file is missing. */
			return(NULL);
		}

		fil_node_complete_io(node, fil_system, IORequestRead);
	}

	return(space);
}

/** Returns the path from the first fil_node_t found with this space ID.
The caller is responsible for freeing the memory allocated here for the
value returned.
@param[in]	id	Tablespace ID
@return own: A copy of fil_node_t::path, NULL if space ID is zero
or not found. */
char*
fil_space_get_first_path(
	ulint		id)
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
@return space size, 0 if space not found */
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
@return flags, ULINT_UNDEFINED if space not found */
ulint
fil_space_get_flags(
/*================*/
	ulint	id)	/*!< in: space id */
{
	fil_space_t*	space;
	ulint		flags;

	ut_ad(fil_system);

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

/** Check if table is mark for truncate.
@param[in]	id	space id
@return true if tablespace is marked for truncate. */
bool
fil_space_is_being_truncated(
	ulint id)
{
	bool	mark_for_truncate;
	mutex_enter(&fil_system->mutex);
	mark_for_truncate = fil_space_get_by_id(id)->is_being_truncated;
	mutex_exit(&fil_system->mutex);
	return(mark_for_truncate);
}

/** Open each fil_node_t of a named fil_space_t if not already open.
@param[in]	name	Tablespace name
@return true if all nodes are open  */
bool
fil_space_open(
	const char*	name)
{
	ut_ad(fil_system != NULL);

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_name(name);
	fil_node_t*	node;

	for (node = UT_LIST_GET_FIRST(space->chain);
	     node != NULL;
	     node = UT_LIST_GET_NEXT(chain, node)) {

		if (!node->is_open
		    && !fil_node_open_file(node)) {
			mutex_exit(&fil_system->mutex);
			return(false);
		}
	}

	mutex_exit(&fil_system->mutex);

	return(true);
}

/** Close each fil_node_t of a named fil_space_t if open.
@param[in]	name	Tablespace name */
void
fil_space_close(
	const char*	name)
{
	if (fil_system == NULL) {
		return;
	}

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_name(name);
	if (space == NULL) {
		mutex_exit(&fil_system->mutex);
		return;
	}

	for (fil_node_t* node = UT_LIST_GET_FIRST(space->chain);
	     node != NULL;
	     node = UT_LIST_GET_NEXT(chain, node)) {

		if (node->is_open) {
			fil_node_close_file(node);
		}
	}

	mutex_exit(&fil_system->mutex);
}

/** Returns the page size of the space and whether it is compressed or not.
The tablespace must be cached in the memory cache.
@param[in]	id	space id
@param[out]	found	true if tablespace was found
@return page size */
const page_size_t
fil_space_get_page_size(
	ulint	id,
	bool*	found)
{
	const ulint	flags = fil_space_get_flags(id);

	if (flags == ULINT_UNDEFINED) {
		*found = false;
		return(univ_page_size);
	}

	*found = true;

	return(page_size_t(flags));
}

/****************************************************************//**
Initializes the tablespace memory cache. */
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
		ut_zalloc_nokey(sizeof(*fil_system)));

	mutex_create(LATCH_ID_FIL_SYSTEM, &fil_system->mutex);

	fil_system->spaces = hash_create(hash_size);
	fil_system->name_hash = hash_create(hash_size);

	UT_LIST_INIT(fil_system->LRU, &fil_node_t::LRU);
	UT_LIST_INIT(fil_system->space_list, &fil_space_t::space_list);
	UT_LIST_INIT(fil_system->unflushed_spaces,
		     &fil_space_t::unflushed_spaces);
	UT_LIST_INIT(fil_system->named_spaces, &fil_space_t::named_spaces);

	fil_system->max_n_open = max_n_open;
}

/*******************************************************************//**
Opens all log files and system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup after the
space objects for the log and the system tablespace have been created. The
purpose of this operation is to make sure we never run out of file descriptors
if we need to read from the insert buffer or to write to the log. */
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

			if (!node->is_open) {
				if (!fil_node_open_file(node)) {
					/* This func is called during server's
					startup. If some file of log or system
					tablespace is missing, the server
					can't start successfully. So we should
					assert for it. */
					ut_a(0);
				}
			}

			if (fil_system->max_n_open < 10 + fil_system->n_open) {

				ib::warn() << "You must raise the value of"
					" innodb_open_files in my.cnf!"
					" Remember that InnoDB keeps all"
					" log files and all system"
					" tablespace files open"
					" for the whole time mysqld is"
					" running, and needs to open also"
					" some .ibd files if the"
					" file-per-table storage model is used."
					" Current open files "
					<< fil_system->n_open
					<< ", max allowed open files "
					<< fil_system->max_n_open
					<< ".";
			}
		}
	}

	mutex_exit(&fil_system->mutex);
}

/*******************************************************************//**
Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
void
fil_close_all_files(void)
/*=====================*/
{
	fil_space_t*	space;

	/* At shutdown, we should not have any files in this list. */
	ut_ad(srv_fast_shutdown == 2
	      || UT_LIST_GET_LEN(fil_system->named_spaces) == 0);

	mutex_enter(&fil_system->mutex);

	for (space = UT_LIST_GET_FIRST(fil_system->space_list);
	     space != NULL; ) {
		fil_node_t*	node;
		fil_space_t*	prev_space = space;

		for (node = UT_LIST_GET_FIRST(space->chain);
		     node != NULL;
		     node = UT_LIST_GET_NEXT(chain, node)) {

			if (node->is_open) {
				fil_node_close_file(node);
			}
		}

		space = UT_LIST_GET_NEXT(space_list, space);
		fil_space_detach(prev_space);
		fil_space_free_low(prev_space);
	}

	mutex_exit(&fil_system->mutex);

	ut_ad(srv_fast_shutdown == 2
	      || UT_LIST_GET_LEN(fil_system->named_spaces) == 0);
}

/*******************************************************************//**
Closes the redo log files. There must not be any pending i/o's or not
flushed modifications in the files. */
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

		if (space->purpose != FIL_TYPE_LOG) {
			space = UT_LIST_GET_NEXT(space_list, space);
			continue;
		}

		/* Log files are not in the fil_system->named_spaces list. */
		ut_ad(space->max_lsn == 0);

		for (node = UT_LIST_GET_FIRST(space->chain);
		     node != NULL;
		     node = UT_LIST_GET_NEXT(chain, node)) {

			if (node->is_open) {
				fil_node_close_file(node);
			}
		}

		space = UT_LIST_GET_NEXT(space_list, space);

		if (free) {
			fil_space_detach(prev_space);
			fil_space_free_low(prev_space);
		}
	}

	mutex_exit(&fil_system->mutex);
}

/*******************************************************************//**
Sets the max tablespace id counter if the given number is bigger than the
previous value. */
void
fil_set_max_space_id_if_bigger(
/*===========================*/
	ulint	max_id)	/*!< in: maximum known id */
{
	if (max_id >= SRV_LOG_SPACE_FIRST_ID) {
		ib::fatal() << "Max tablespace id is too high, " << max_id;
	}

	mutex_enter(&fil_system->mutex);

	if (fil_system->max_assigned_id < max_id) {

		fil_system->max_assigned_id = max_id;
	}

	mutex_exit(&fil_system->mutex);
}

/** Write the flushed LSN to the page header of the first page in the
system tablespace.
@param[in]	lsn	flushed LSN
@return DB_SUCCESS or error number */
dberr_t
fil_write_flushed_lsn(
	lsn_t	lsn)
{
	byte*	buf1;
	byte*	buf;
	dberr_t	err;

	buf1 = static_cast<byte*>(ut_malloc_nokey(2 * UNIV_PAGE_SIZE));
	buf = static_cast<byte*>(ut_align(buf1, UNIV_PAGE_SIZE));

	const page_id_t	page_id(TRX_SYS_SPACE, 0);

	err = fil_read(page_id, univ_page_size, 0, univ_page_size.physical(),
		       buf);

	if (err == DB_SUCCESS) {
		mach_write_to_8(buf + FIL_PAGE_FILE_FLUSH_LSN, lsn);

		err = fil_write(page_id, univ_page_size, 0,
				univ_page_size.physical(), buf);

		fil_flush_file_spaces(FIL_TYPE_TABLESPACE);
	}

	ut_free(buf1);

	return(err);
}
#ifndef UNIV_HOTBACKUP
/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	id	tablespace ID
@param[in]	silent	whether to silently ignore missing tablespaces
@return the tablespace, or NULL if missing or being deleted */
inline
fil_space_t*
fil_space_acquire_low(
	ulint	id,
	bool	silent)
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	if (space == NULL) {
		if (!silent) {
			ib::warn() << "Trying to access missing"
				" tablespace " << id;
		}
	} else if (space->stop_new_ops || space->is_being_truncated) {
		space = NULL;
	} else {
		space->n_pending_ops++;
	}

	mutex_exit(&fil_system->mutex);

	return(space);
}

/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	id	tablespace ID
@return the tablespace, or NULL if missing or being deleted */
fil_space_t*
fil_space_acquire(
	ulint	id)
{
	return(fil_space_acquire_low(id, false));
}

/** Acquire a tablespace that may not exist.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	id	tablespace ID
@return the tablespace, or NULL if missing or being deleted */
fil_space_t*
fil_space_acquire_silent(
	ulint	id)
{
	return(fil_space_acquire_low(id, true));
}

/** Release a tablespace acquired with fil_space_acquire().
@param[in,out]	space	tablespace to release  */
void
fil_space_release(
	fil_space_t*	space)
{
	mutex_enter(&fil_system->mutex);
	ut_ad(space->magic_n == FIL_SPACE_MAGIC_N);
	ut_ad(space->n_pending_ops > 0);
	space->n_pending_ops--;
	mutex_exit(&fil_system->mutex);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************//**
Creates the database directory for a table if it does not exist yet. */
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
	path = static_cast<char*>(ut_malloc_nokey(len + (namend - name) + 2));

	memcpy(path, fil_path_to_mysql_datadir, len);
	path[len] = '/';
	memcpy(path + len + 1, name, namend - name);
	path[len + (namend - name) + 1] = 0;

	os_normalize_path(path);

	bool	success = os_file_create_directory(path, false);
	ut_a(success);

	ut_free(path);
}

/** Write a log record about an operation on a tablespace file.
@param[in]	type		MLOG_FILE_NAME or MLOG_FILE_DELETE
or MLOG_FILE_CREATE2 or MLOG_FILE_RENAME2
@param[in]	space_id	tablespace identifier
@param[in]	first_page_no	first page number in the file
@param[in]	path		file path
@param[in]	new_path	if type is MLOG_FILE_RENAME2, the new name
@param[in]	flags		if type is MLOG_FILE_CREATE2, the space flags
@param[in,out]	mtr		mini-transaction */
static
void
fil_op_write_log(
	mlog_id_t	type,
	ulint		space_id,
	ulint		first_page_no,
	const char*	path,
	const char*	new_path,
	ulint		flags,
	mtr_t*		mtr)
{
	byte*		log_ptr;
	ulint		len;

	ut_ad(first_page_no == 0);

	/* fil_name_parse() requires that there be at least one path
	separator and that the file path end with ".ibd". */
	ut_ad(strchr(path, OS_PATH_SEPARATOR) != NULL);
	ut_ad(strcmp(&path[strlen(path) - strlen(DOT_IBD)], DOT_IBD) == 0);

	log_ptr = mlog_open(mtr, 11 + 4 + 2 + 1);

	if (log_ptr == NULL) {
		/* Logging in mtr is switched off during crash recovery:
		in that case mlog_open returns NULL */
		return;
	}

	log_ptr = mlog_write_initial_log_record_low(
		type, space_id, first_page_no, log_ptr, mtr);

	if (type == MLOG_FILE_CREATE2) {
		mach_write_to_4(log_ptr, flags);
		log_ptr += 4;
	}

	/* Let us store the strings as null-terminated for easier readability
	and handling */

	len = strlen(path) + 1;

	mach_write_to_2(log_ptr, len);
	log_ptr += 2;
	mlog_close(mtr, log_ptr);

	mlog_catenate_string(
		mtr, reinterpret_cast<const byte*>(path), len);

	switch (type) {
	case MLOG_FILE_RENAME2:
		ut_ad(strchr(new_path, OS_PATH_SEPARATOR) != NULL);
		len = strlen(new_path) + 1;
		log_ptr = mlog_open(mtr, 2 + len);
		ut_a(log_ptr);
		mach_write_to_2(log_ptr, len);
		log_ptr += 2;
		mlog_close(mtr, log_ptr);

		mlog_catenate_string(
			mtr, reinterpret_cast<const byte*>(new_path), len);
		break;
	case MLOG_FILE_NAME:
	case MLOG_FILE_DELETE:
	case MLOG_FILE_CREATE2:
		break;
	default:
		ut_ad(0);
	}
}
#ifndef UNIV_HOTBACKUP
/** Write redo log for renaming a file.
@param[in]	space_id	tablespace id
@param[in]	first_page_no	first page number in the file
@param[in]	old_name	tablespace file name
@param[in]	new_name	tablespace file name after renaming
@param[in,out]	mtr		mini-transaction */
static
void
fil_name_write_rename(
	ulint		space_id,
	ulint		first_page_no,
	const char*	old_name,
	const char*	new_name,
	mtr_t*		mtr)
{
	ut_ad(!is_predefined_tablespace(space_id));

	fil_op_write_log(
		MLOG_FILE_RENAME2,
		space_id, first_page_no, old_name, new_name, 0, mtr);
}
#endif /* !UNIV_HOTBACKUP */
/** Write MLOG_FILE_NAME for a file.
@param[in]	space_id	tablespace id
@param[in]	first_page_no	first page number in the file
@param[in]	name		tablespace file name
@param[in,out]	mtr		mini-transaction */
static
void
fil_name_write(
	ulint		space_id,
	ulint		first_page_no,
	const char*	name,
	mtr_t*		mtr)
{
	fil_op_write_log(
		MLOG_FILE_NAME, space_id, first_page_no, name, NULL, 0, mtr);
}

/** Write MLOG_FILE_NAME for a file.
@param[in]	space		tablespace
@param[in]	first_page_no	first page number in the file
@param[in]	file		tablespace file
@param[in,out]	mtr		mini-transaction */
static
void
fil_name_write(
	const fil_space_t*	space,
	ulint			first_page_no,
	const fil_node_t*	file,
	mtr_t*			mtr)
{
	fil_name_write(space->id, first_page_no, file->name, mtr);
}

#ifndef UNIV_HOTBACKUP
/********************************************************//**
Recreates table indexes by applying
TRUNCATE log record during recovery.
@return DB_SUCCESS or error code */
dberr_t
fil_recreate_table(
/*===============*/
	ulint		space_id,	/*!< in: space id */
	ulint		format_flags,	/*!< in: page format */
	ulint		flags,		/*!< in: tablespace flags */
	const char*	name,		/*!< in: table name */
	truncate_t&	truncate)	/*!< in: The information of
					TRUNCATE log record */
{
	dberr_t			err = DB_SUCCESS;
	bool			found;
	const page_size_t	page_size(fil_space_get_page_size(space_id,
								  &found));

	if (!found) {
		ib::info() << "Missing .ibd file for table '" << name
			<< "' with tablespace " << space_id;
		return(DB_ERROR);
	}

	ut_ad(!truncate_t::s_fix_up_active);
	truncate_t::s_fix_up_active = true;

	/* Step-1: Scan for active indexes from REDO logs and drop
	all the indexes using low level function that take root_page_no
	and space-id. */
	truncate.drop_indexes(space_id);

	/* Step-2: Scan for active indexes and re-create them. */
	err = truncate.create_indexes(
		name, space_id, page_size, flags, format_flags);
	if (err != DB_SUCCESS) {
		ib::info() << "Failed to create indexes for the table '"
			<< name << "' with tablespace " << space_id
			<< " while fixing up truncate action";
		return(err);
	}

	truncate_t::s_fix_up_active = false;

	return(err);
}

/********************************************************//**
Recreates the tablespace and table indexes by applying
TRUNCATE log record during recovery.
@return DB_SUCCESS or error code */
dberr_t
fil_recreate_tablespace(
/*====================*/
	ulint		space_id,	/*!< in: space id */
	ulint		format_flags,	/*!< in: page format */
	ulint		flags,		/*!< in: tablespace flags */
	const char*	name,		/*!< in: table name */
	truncate_t&	truncate,	/*!< in: The information of
					TRUNCATE log record */
	lsn_t		recv_lsn)	/*!< in: the end LSN of
						the log record */
{
	dberr_t		err = DB_SUCCESS;
	mtr_t		mtr;

	ut_ad(!truncate_t::s_fix_up_active);
	truncate_t::s_fix_up_active = true;

	/* Step-1: Invalidate buffer pool pages belonging to the tablespace
	to re-create. */
	buf_LRU_flush_or_remove_pages(space_id, BUF_REMOVE_ALL_NO_WRITE, 0);

	/* Remove all insert buffer entries for the tablespace */
	ibuf_delete_for_discarded_space(space_id);

	/* Step-2: truncate tablespace (reset the size back to original or
	default size) of tablespace. */
	err = truncate.truncate(
		space_id, truncate.get_dir_path(), name, flags, true);

	if (err != DB_SUCCESS) {

		ib::info() << "Cannot access .ibd file for table '"
			<< name << "' with tablespace " << space_id
			<< " while truncating";
		return(DB_ERROR);
	}

	bool			found;
	const page_size_t&	page_size =
		fil_space_get_page_size(space_id, &found);

	if (!found) {
		ib::info() << "Missing .ibd file for table '" << name
			<< "' with tablespace " << space_id;
		return(DB_ERROR);
	}

	/* Step-3: Initialize Header. */
	if (page_size.is_compressed()) {
		byte*	buf;
		page_t*	page;

		buf = static_cast<byte*>(ut_zalloc_nokey(3 * UNIV_PAGE_SIZE));

		/* Align the memory for file i/o */
		page = static_cast<byte*>(ut_align(buf, UNIV_PAGE_SIZE));

		flags = fsp_flags_set_page_size(flags, univ_page_size);

		fsp_header_init_fields(page, space_id, flags);

		mach_write_to_4(
			page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

		page_zip_des_t  page_zip;
		page_zip_set_size(&page_zip, page_size.physical());
		page_zip.data = page + UNIV_PAGE_SIZE;

#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
		page_zip.m_end = page_zip.m_nonempty = page_zip.n_blobs = 0;
		buf_flush_init_for_writing(
			NULL, page, &page_zip, 0,
			fsp_is_checksum_disabled(space_id));

		err = fil_write(page_id_t(space_id, 0), page_size, 0,
				page_size.physical(), page_zip.data);

		ut_free(buf);

		if (err != DB_SUCCESS) {
			ib::info() << "Failed to clean header of the"
				" table '" << name << "' with tablespace "
				<< space_id;
			return(err);
		}
	}

	mtr_start(&mtr);
	/* Don't log the operation while fixing up table truncate operation
	as crash at this level can still be sustained with recovery restarting
	from last checkpoint. */
	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

	/* Initialize the first extent descriptor page and
	the second bitmap page for the new tablespace. */
	fsp_header_init(space_id, FIL_IBD_FILE_INITIAL_SIZE, &mtr);
	mtr_commit(&mtr);

	/* Step-4: Re-Create Indexes to newly re-created tablespace.
	This operation will restore tablespace back to what it was
	when it was created during CREATE TABLE. */
	err = truncate.create_indexes(
		name, space_id, page_size, flags, format_flags);
	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Step-5: Write new created pages into ibd file handle and
	flush it to disk for the tablespace, in case i/o-handler thread
	deletes the bitmap page from buffer. */
	mtr_start(&mtr);

	mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(space_id);

	mutex_exit(&fil_system->mutex);

	fil_node_t*	node = UT_LIST_GET_FIRST(space->chain);

	for (ulint page_no = 0; page_no < node->size; ++page_no) {

		const page_id_t	cur_page_id(space_id, page_no);

		buf_block_t*	block = buf_page_get(cur_page_id, page_size,
						     RW_X_LATCH, &mtr);

		byte*	page = buf_block_get_frame(block);

		if (!fsp_flags_is_compressed(flags)) {

			ut_ad(!page_size.is_compressed());

			buf_flush_init_for_writing(
				block, page, NULL, recv_lsn,
				fsp_is_checksum_disabled(space_id));

			err = fil_write(cur_page_id, page_size, 0,
					page_size.physical(), page);
		} else {
			ut_ad(page_size.is_compressed());

			/* We don't want to rewrite empty pages. */

			if (fil_page_get_type(page) != 0) {
				page_zip_des_t*  page_zip =
					buf_block_get_page_zip(block);

				buf_flush_init_for_writing(
					block, page, page_zip, recv_lsn,
					fsp_is_checksum_disabled(space_id));

				err = fil_write(cur_page_id, page_size, 0,
						page_size.physical(),
						page_zip->data);
			} else {
#ifdef UNIV_DEBUG
				const byte*	data = block->page.zip.data;

				/* Make sure that the page is really empty */
				for (ulint i = 0;
				     i < page_size.physical();
				     ++i) {

					ut_a(data[i] == 0);
				}
#endif /* UNIV_DEBUG */
			}
		}

		if (err != DB_SUCCESS) {
			ib::info() << "Cannot write page " << page_no
				<< " into a .ibd file for table '"
				<< name << "' with tablespace " << space_id;
		}
	}

	mtr_commit(&mtr);

	truncate_t::s_fix_up_active = false;

	return(err);
}
#endif /* UNIV_HOTBACKUP */
/** Replay a file rename operation if possible.
@param[in]	space_id	tablespace identifier
@param[in]	first_page_no	first page number in the file
@param[in]	name		old file name
@param[in]	new_name	new file name
@return	whether the operation was successfully applied
(the name did not exist, or new_name did not exist and
name was successfully renamed to new_name)  */
bool
fil_op_replay_rename(
	ulint		space_id,
	ulint		first_page_no,
	const char*	name,
	const char*	new_name)
{
#ifdef UNIV_HOTBACKUP
	ut_ad(recv_replay_file_ops);
#endif /* UNIV_HOTBACKUP */
	ut_ad(first_page_no == 0);

	/* In order to replay the rename, the following must hold:
	* The new name is not already used.
	* A tablespace exists with the old name.
	* The space ID for that tablepace matches this log entry.
	This will prevent unintended renames during recovery. */
	fil_space_t*	space = fil_space_get(space_id);

	if (space == NULL) {
		return(true);
	}

	const bool name_match
		= strcmp(name, UT_LIST_GET_FIRST(space->chain)->name) == 0;

	if (!name_match) {
		return(true);
	}

	/* Create the database directory for the new name, if
	it does not exist yet */

	const char*	namend = strrchr(new_name, OS_PATH_SEPARATOR);
	ut_a(namend != NULL);

	char*		dir = static_cast<char*>(
		ut_malloc_nokey(namend - new_name + 1));

	memcpy(dir, new_name, namend - new_name);
	dir[namend - new_name] = '\0';

	bool		success = os_file_create_directory(dir, false);
	ut_a(success);

	ulint		dirlen = 0;

	if (const char* dirend = strrchr(dir, OS_PATH_SEPARATOR)) {
		dirlen = dirend - dir + 1;
	}

	ut_free(dir);

	/* New path must not exist. */
	dberr_t		err = fil_rename_tablespace_check(
		space_id, name, new_name, false);
	if (err != DB_SUCCESS) {
		ib::error() << " Cannot replay file rename."
			" Remove either file and try again.";
		return(false);
	}

	char*		new_table = mem_strdupl(
		new_name + dirlen,
		strlen(new_name + dirlen)
		- 4 /* remove ".ibd" */);

	ut_ad(new_table[namend - new_name - dirlen]
	      == OS_PATH_SEPARATOR);
#if OS_PATH_SEPARATOR != '/'
	new_table[namend - new_name - dirlen] = '/';
#endif

	if (!fil_rename_tablespace(
		    space_id, name, new_table, new_name)) {
		ut_error;
	}

	ut_free(new_table);
	return(true);
}

/** File operations for tablespace */
enum fil_operation_t {
	FIL_OPERATION_DELETE,	/*!< delete a single-table tablespace */
	FIL_OPERATION_CLOSE,	/*!< close a single-table tablespace */
	FIL_OPERATION_TRUNCATE	/*!< truncate a single-table tablespace */
};

/** Check for pending operations.
@param[in]	space	tablespace
@param[in]	count	number of attempts so far
@return 0 if no operations else count + 1. */
static
ulint
fil_check_pending_ops(
	fil_space_t*	space,
	ulint		count)
{
	ut_ad(mutex_own(&fil_system->mutex));

	const ulint	n_pending_ops = space ? space->n_pending_ops : 0;

	if (n_pending_ops) {

		if (count > 5000) {
			ib::warn() << "Trying to close/delete/truncate"
				" tablespace '" << space->name
				<< "' but there are " << n_pending_ops
				<< " pending operations on it.";
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
	fil_operation_t	operation,	/*!< in: File operation */
	fil_space_t*	space,		/*!< in/out: Tablespace to check */
	fil_node_t**	node,		/*!< out: Node in space list */
	ulint		count)		/*!< in: number of attempts so far */
{
	ut_ad(mutex_own(&fil_system->mutex));
	ut_a(space->n_pending_ops == 0);

	switch (operation) {
	case FIL_OPERATION_DELETE:
	case FIL_OPERATION_CLOSE:
		break;
	case FIL_OPERATION_TRUNCATE:
		space->is_being_truncated = true;
		break;
	}

	/* The following code must change when InnoDB supports
	multiple datafiles per tablespace. */
	ut_a(UT_LIST_GET_LEN(space->chain) == 1);

	*node = UT_LIST_GET_FIRST(space->chain);

	if (space->n_pending_flushes > 0 || (*node)->n_pending > 0) {

		ut_a(!(*node)->being_extended);

		if (count > 1000) {
			ib::warn() << "Trying to delete/close/truncate"
				" tablespace '" << space->name
				<< "' but there are "
				<< space->n_pending_flushes
				<< " flushes and " << (*node)->n_pending
				<< " pending i/o's on it.";
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
	ulint		id,		/*!< in: space id */
	fil_operation_t	operation,	/*!< in: File operation */
	fil_space_t**	space,		/*!< out: tablespace instance
					in memory */
	char**		path)		/*!< out/own: tablespace path */
{
	ulint		count = 0;

	ut_a(!is_system_tablespace(id));
	ut_ad(space);

	*space = 0;

	mutex_enter(&fil_system->mutex);
	fil_space_t* sp = fil_space_get_by_id(id);
	if (sp) {
		sp->stop_new_ops = true;
	}
	mutex_exit(&fil_system->mutex);

	/* Check for pending operations. */

	do {
		mutex_enter(&fil_system->mutex);

		sp = fil_space_get_by_id(id);

		count = fil_check_pending_ops(sp, count);

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

		count = fil_check_pending_io(operation, sp, &node, count);

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
@return DB_SUCCESS or error */
dberr_t
fil_close_tablespace(
/*=================*/
	trx_t*		trx,	/*!< in/out: Transaction covering the close */
	ulint		id)	/*!< in: space id */
{
	char*		path = 0;
	fil_space_t*	space = 0;
	dberr_t		err;

	ut_a(!is_system_tablespace(id));

	err = fil_check_pending_operations(id, FIL_OPERATION_CLOSE,
					   &space, &path);

	if (err != DB_SUCCESS) {
		return(err);
	}

	ut_a(space);
	ut_a(path != 0);

	rw_lock_x_lock(&space->latch);

	/* Invalidate in the buffer pool all pages belonging to the
	tablespace. Since we have set space->stop_new_ops = true, readahead
	or ibuf merge can no longer read more pages of this tablespace to the
	buffer pool. Thus we can clean the tablespace out of the buffer pool
	completely and permanently. The flag stop_new_ops also prevents
	fil_flush() from being applied to this tablespace. */

	buf_LRU_flush_or_remove_pages(id, BUF_REMOVE_FLUSH_WRITE, trx);

	/* If the free is successful, the X lock will be released before
	the space memory data structure is freed. */

	if (!fil_space_free(id, true)) {
		rw_lock_x_unlock(&space->latch);
		err = DB_TABLESPACE_NOT_FOUND;
	} else {
		err = DB_SUCCESS;
	}

	/* If it is a delete then also delete any generated files, otherwise
	when we drop the database the remove directory will fail. */

	char*	cfg_name = fil_make_filepath(path, NULL, CFG, false);
	if (cfg_name != NULL) {
		os_file_delete_if_exists(innodb_data_file_key, cfg_name, NULL);
		ut_free(cfg_name);
	}

	char*	cfp_name = fil_make_filepath(path, NULL, CFP, false);
	if (cfp_name != NULL) {
		os_file_delete_if_exists(innodb_data_file_key, cfp_name, NULL);
		ut_free(cfp_name);
	}

	ut_free(path);

	return(err);
}

/** Deletes an IBD tablespace, either general or single-table.
The tablespace must be cached in the memory cache. This will delete the
datafile, fil_space_t & fil_node_t entries from the file_system_t cache.
@param[in]	space_id	Tablespace id
@param[in]	buf_remove	Specify the action to take on the pages
for this table in the buffer pool.
@return DB_SUCCESS or error */
dberr_t
fil_delete_tablespace(
	ulint		id,
	buf_remove_t	buf_remove)
{
	char*		path = 0;
	fil_space_t*	space = 0;

	ut_a(!is_system_tablespace(id));

	dberr_t err = fil_check_pending_operations(
		id, FIL_OPERATION_DELETE, &space, &path);

	if (err != DB_SUCCESS) {

		ib::error() << "Cannot delete tablespace " << id
			<< " because it is not found in the tablespace"
			" memory cache.";

		return(err);
	}

	ut_a(space);
	ut_a(path != 0);

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
	space from the flush_list. Note that if a block is IO-fixed
	we'll wait for IO to complete.

	To deal with potential read requests, we will check the
	::stop_new_ops flag in fil_io(). */

	buf_LRU_flush_or_remove_pages(id, buf_remove, 0);

#endif /* !UNIV_HOTBACKUP */

	/* If it is a delete then also delete any generated files, otherwise
	when we drop the database the remove directory will fail. */
	{
#ifdef UNIV_HOTBACKUP
		/* When replaying the operation in MySQL Enterprise
		Backup, we do not try to write any log record. */
#else /* UNIV_HOTBACKUP */
		/* Before deleting the file, write a log record about
		it, so that InnoDB crash recovery will expect the file
		to be gone. */
		mtr_t		mtr;

		mtr_start(&mtr);
		fil_op_write_log(MLOG_FILE_DELETE, id, 0, path, NULL, 0, &mtr);
		mtr_commit(&mtr);
		/* Even if we got killed shortly after deleting the
		tablespace file, the record must have already been
		written to the redo log. */
		log_write_up_to(mtr.commit_lsn(), true);
#endif /* UNIV_HOTBACKUP */

		char*	cfg_name = fil_make_filepath(path, NULL, CFG, false);
		if (cfg_name != NULL) {
			os_file_delete_if_exists(innodb_data_file_key, cfg_name, NULL);
			ut_free(cfg_name);
		}

		char*	cfp_name = fil_make_filepath(path, NULL, CFP, false);
		if (cfp_name != NULL) {
			os_file_delete_if_exists(innodb_data_file_key, cfp_name, NULL);
			ut_free(cfp_name);
		}
	}

	/* Delete the link file pointing to the ibd file we are deleting. */
	if (FSP_FLAGS_HAS_DATA_DIR(space->flags)) {

		RemoteDatafile::delete_link_file(space->name);

	} else if (FSP_FLAGS_GET_SHARED(space->flags)) {

		RemoteDatafile::delete_link_file(base_name(path));

	}

	mutex_enter(&fil_system->mutex);

	/* Double check the sanity of pending ops after reacquiring
	the fil_system::mutex. */
	if (const fil_space_t* s = fil_space_get_by_id(id)) {
		ut_a(s == space);
		ut_a(space->n_pending_ops == 0);
		ut_a(UT_LIST_GET_LEN(space->chain) == 1);
		fil_node_t* node = UT_LIST_GET_FIRST(space->chain);
		ut_a(node->n_pending == 0);

		fil_space_detach(space);
		mutex_exit(&fil_system->mutex);

		log_mutex_enter();

		if (space->max_lsn != 0) {
			ut_d(space->max_lsn = 0);
			UT_LIST_REMOVE(fil_system->named_spaces, space);
		}

		log_mutex_exit();
		fil_space_free_low(space);

		if (!os_file_delete(innodb_data_file_key, path)
		    && !os_file_delete_if_exists(
			    innodb_data_file_key, path, NULL)) {

			/* Note: This is because we have removed the
			tablespace instance from the cache. */

			err = DB_IO_ERROR;
		}
	} else {
		mutex_exit(&fil_system->mutex);
		err = DB_TABLESPACE_NOT_FOUND;
	}

	ut_free(path);

	return(err);
}
#ifndef UNIV_HOTBACKUP
/** Truncate the tablespace to needed size.
@param[in]	space_id	id of tablespace to truncate
@param[in]	size_in_pages	truncate size.
@return true if truncate was successful. */
bool
fil_truncate_tablespace(
	ulint		space_id,
	ulint		size_in_pages)
{
	/* Step-1: Prepare tablespace for truncate. This involves
	stopping all the new operations + IO on that tablespace
	and ensuring that related pages are flushed to disk. */
	if (fil_prepare_for_truncate(space_id) != DB_SUCCESS) {
		return(false);
	}

	/* Step-2: Invalidate buffer pool pages belonging to the tablespace
	to re-create. Remove all insert buffer entries for the tablespace */
	buf_LRU_flush_or_remove_pages(space_id, BUF_REMOVE_ALL_NO_WRITE, 0);

	/* Step-3: Truncate the tablespace and accordingly update
	the fil_space_t handler that is used to access this tablespace. */
	mutex_enter(&fil_system->mutex);
	fil_space_t*	space = fil_space_get_by_id(space_id);

	/* The following code must change when InnoDB supports
	multiple datafiles per tablespace. */
	ut_a(UT_LIST_GET_LEN(space->chain) == 1);

	fil_node_t*	node = UT_LIST_GET_FIRST(space->chain);

	ut_ad(node->is_open);

	space->size = node->size = size_in_pages;

	bool success = os_file_truncate(node->name, node->handle, 0);
	if (success) {

		os_offset_t	size = size_in_pages * UNIV_PAGE_SIZE;

		success = os_file_set_size(
			node->name, node->handle, size, srv_read_only_mode);

		if (success) {
			space->stop_new_ops = false;
			space->is_being_truncated = false;
		}
	}

	mutex_exit(&fil_system->mutex);

	return(success);
}

/*******************************************************************//**
Prepare for truncating a single-table tablespace.
1) Check pending operations on a tablespace;
2) Remove all insert buffer entries for the tablespace;
@return DB_SUCCESS or error */
dberr_t
fil_prepare_for_truncate(
/*=====================*/
	ulint	id)		/*!< in: space id */
{
	char*		path = 0;
	fil_space_t*	space = 0;

	ut_a(!is_system_tablespace(id));

	dberr_t	err = fil_check_pending_operations(
		id, FIL_OPERATION_TRUNCATE, &space, &path);

	ut_free(path);

	if (err == DB_TABLESPACE_NOT_FOUND) {
		ib::error() << "Cannot truncate tablespace " << id
			<< " because it is not found in the tablespace"
			" memory cache.";
	}

	return(err);
}

/** Reinitialize the original tablespace header with the same space id
for single tablespace
@param[in]      table		table belongs to tablespace
@param[in]      size            size in blocks
@param[in]      trx             Transaction covering truncate */
void
fil_reinit_space_header_for_table(
	dict_table_t*	table,
	ulint		size,
	trx_t*		trx)
{
	ulint	id = table->space;

	ut_a(!is_system_tablespace(id));

	/* Invalidate in the buffer pool all pages belonging
	to the tablespace. The buffer pool scan may take long
	time to complete, therefore we release dict_sys->mutex
	and the dict operation lock during the scan and aquire
	it again after the buffer pool scan.*/

	/* Release the lock on the indexes too. So that
	they won't violate the latch ordering. */
	dict_table_x_unlock_indexes(table);
	row_mysql_unlock_data_dictionary(trx);

	/* Lock the search latch in shared mode to prevent user
	from disabling AHI during the scan */
	btr_search_s_lock_all();
	DEBUG_SYNC_C("simulate_buffer_pool_scan");
	buf_LRU_flush_or_remove_pages(id, BUF_REMOVE_ALL_NO_WRITE, 0);
	btr_search_s_unlock_all();

	row_mysql_lock_data_dictionary(trx);

	dict_table_x_lock_indexes(table);

	/* Remove all insert buffer entries for the tablespace */
	ibuf_delete_for_discarded_space(id);

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(id);

	/* The following code must change when InnoDB supports
	multiple datafiles per tablespace. */
	ut_a(UT_LIST_GET_LEN(space->chain) == 1);

	fil_node_t*	node = UT_LIST_GET_FIRST(space->chain);

	space->size = node->size = size;

	mutex_exit(&fil_system->mutex);

	mtr_t	mtr;

	mtr_start(&mtr);
	mtr.set_named_space(id);

	fsp_header_init(id, size, &mtr);

	mtr_commit(&mtr);
}

#ifdef UNIV_DEBUG
/** Increase redo skipped count for a tablespace.
@param[in]	id	space id */
void
fil_space_inc_redo_skipped_count(
	ulint		id)
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space != NULL);

	space->redo_skipped_count++;

	mutex_exit(&fil_system->mutex);
}

/** Decrease redo skipped count for a tablespace.
@param[in]	id	space id */
void
fil_space_dec_redo_skipped_count(
	ulint		id)
{
	fil_space_t*	space;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space != NULL);
	ut_a(space->redo_skipped_count > 0);

	space->redo_skipped_count--;

	mutex_exit(&fil_system->mutex);
}

/**
Check whether a single-table tablespace is redo skipped.
@param[in]	id	space id
@return true if redo skipped */
bool
fil_space_is_redo_skipped(
	ulint		id)
{
	fil_space_t*	space;
	bool		is_redo_skipped;

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space != NULL);

	is_redo_skipped = space->redo_skipped_count > 0;

	mutex_exit(&fil_system->mutex);

	return(is_redo_skipped);
}
#endif

/*******************************************************************//**
Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but

 1. We do not drop the table from the data dictionary;

 2. We remove all insert buffer entries for the tablespace immediately;
    in DROP TABLE they are only removed gradually in the background;

 3. Free all the pages in use by the tablespace.
@return DB_SUCCESS or error */
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
		ib::warn() << "While deleting tablespace " << id
			<< " in DISCARD TABLESPACE. File rename/delete"
			" failed: " << ut_strerr(err);
		break;

	case DB_TABLESPACE_NOT_FOUND:
		ib::warn() << "Cannot delete tablespace " << id
			<< " in DISCARD TABLESPACE: " << ut_strerr(err);
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
Allocates and builds a file name from a path, a table or tablespace name
and a suffix. The string must be freed by caller with ut_free().
@param[in] path NULL or the direcory path or the full path and filename.
@param[in] name NULL if path is full, or Table/Tablespace name
@param[in] suffix NULL or the file extention to use.
@param[in] trim_name true if the last name on the path should be trimmed.
@return own: file name */
char*
fil_make_filepath(
	const char*	path,
	const char*	name,
	ib_extention	ext,
	bool		trim_name)
{
	/* The path may contain the basename of the file, if so we do not
	need the name.  If the path is NULL, we can use the default path,
	but there needs to be a name. */
	ut_ad(path != NULL || name != NULL);

	/* If we are going to strip a name off the path, there better be a
	path and a new name to put back on. */
	ut_ad(!trim_name || (path != NULL && name != NULL));

	if (path == NULL) {
		path = fil_path_to_mysql_datadir;
	}

	ulint	len		= 0;	/* current length */
	ulint	path_len	= strlen(path);
	ulint	name_len	= (name ? strlen(name) : 0);
	const char* suffix	= dot_ext[ext];
	ulint	suffix_len	= strlen(suffix);
	ulint	full_len	= path_len + 1 + name_len + suffix_len + 1;

	char*	full_name = static_cast<char*>(ut_malloc_nokey(full_len));
	if (full_name == NULL) {
		return NULL;
	}

	/* If the name is a relative path, do not prepend "./". */
	if (path[0] == '.'
	    && (path[1] == '\0' || path[1] == OS_PATH_SEPARATOR)
	    && name != NULL && name[0] == '.') {
		path = NULL;
		path_len = 0;
	}

	if (path != NULL) {
		memcpy(full_name, path, path_len);
		len = path_len;
		full_name[len] = '\0';
		os_normalize_path(full_name);
	}

	if (trim_name) {
		/* Find the offset of the last DIR separator and set it to
		null in order to strip off the old basename from this path. */
		char* last_dir_sep = strrchr(full_name, OS_PATH_SEPARATOR);
		if (last_dir_sep) {
			last_dir_sep[0] = '\0';
			len = strlen(full_name);
		}
	}

	if (name != NULL) {
		if (len && full_name[len - 1] != OS_PATH_SEPARATOR) {
			/* Add a DIR separator */
			full_name[len] = OS_PATH_SEPARATOR;
			full_name[++len] = '\0';
		}

		char*	ptr = &full_name[len];
		memcpy(ptr, name, name_len);
		len += name_len;
		full_name[len] = '\0';
		os_normalize_path(ptr);
	}

	/* Make sure that the specified suffix is at the end of the filepath
	string provided. This assumes that the suffix starts with '.'.
	If the first char of the suffix is found in the filepath at the same
	length as the suffix from the end, then we will assume that there is
	a previous suffix that needs to be replaced. */
	if (suffix != NULL) {
		/* Need room for the trailing null byte. */
		ut_ad(len < full_len);

		if ((len > suffix_len)
		   && (full_name[len - suffix_len] == suffix[0])) {
			/* Another suffix exists, make it the one requested. */
			memcpy(&full_name[len - suffix_len], suffix, suffix_len);

		} else {
			/* No previous suffix, add it. */
			ut_ad(len + suffix_len < full_len);
			memcpy(&full_name[len], suffix, suffix_len);
			full_name[len + suffix_len] = '\0';
		}
	}

	return(full_name);
}

/** Test if a tablespace file can be renamed to a new filepath by checking
if that the old filepath exists and the new filepath does not exist.
@param[in]	space_id	tablespace id
@param[in]	old_path	old filepath
@param[in]	new_path	new filepath
@param[in]	is_discarded	whether the tablespace is discarded
@return innodb error code */
dberr_t
fil_rename_tablespace_check(
	ulint		space_id,
	const char*	old_path,
	const char*	new_path,
	bool		is_discarded)
{
	bool	exists = false;
	os_file_type_t	ftype;

	if (!is_discarded
	    && os_file_status(old_path, &exists, &ftype)
	    && !exists) {
		ib::error() << "Cannot rename '" << old_path
			<< "' to '" << new_path
			<< "' for space ID " << space_id
			<< " because the source file"
			<< " does not exist.";
		return(DB_TABLESPACE_NOT_FOUND);
	}

	exists = false;
	if (!os_file_status(new_path, &exists, &ftype) || exists) {
		ib::error() << "Cannot rename '" << old_path
			<< "' to '" << new_path
			<< "' for space ID " << space_id
			<< " because the target file exists."
			" Remove the target file and try again.";
		return(DB_TABLESPACE_EXISTS);
	}

	return(DB_SUCCESS);
}

/** Rename a single-table tablespace.
The tablespace must exist in the memory cache.
@param[in]	id		tablespace identifier
@param[in]	old_path	old file name
@param[in]	new_name	new table name in the
databasename/tablename format
@param[in]	new_path_in	new file name,
or NULL if it is located in the normal data directory
@return true if success */
bool
fil_rename_tablespace(
	ulint		id,
	const char*	old_path,
	const char*	new_name,
	const char*	new_path_in)
{
	bool		sleep		= false;
	bool		flush		= false;
	fil_space_t*	space;
	fil_node_t*	node;
	ulint		count		= 0;
	ut_a(id != 0);

	ut_ad(strchr(new_name, '/') != NULL);
retry:
	count++;

	if (!(count % 1000)) {
		ib::warn() << "Cannot rename file " << old_path
			<< " (space id " << id << "), retried " << count
			<< " times."
			" There are either pending IOs or flushes or"
			" the file is being extended.";
	}

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	DBUG_EXECUTE_IF("fil_rename_tablespace_failure_1", space = NULL; );

	if (space == NULL) {
		ib::error() << "Cannot find space id " << id
			<< " in the tablespace memory cache, though the file '"
			<< old_path
			<< "' in a rename operation should have that id.";
func_exit:
		mutex_exit(&fil_system->mutex);
		return(false);
	}

	if (count > 25000) {
		space->stop_ios = false;
		goto func_exit;
	}

	if (space != fil_space_get_by_name(space->name)) {
		ib::error() << "Cannot find " << space->name
			<< " in tablespace memory cache";
		space->stop_ios = false;
		goto func_exit;
	}

	if (fil_space_get_by_name(new_name)) {
		ib::error() << new_name
			<< " is already in tablespace memory cache";
		space->stop_ios = false;
		goto func_exit;
	}

	/* We temporarily close the .ibd file because we do not trust that
	operating systems can rename an open file. For the closing we have to
	wait until there are no pending i/o's or flushes on the file. */

	space->stop_ios = true;

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
		sleep = true;

	} else if (node->modification_counter > node->flush_counter) {
		/* Flush the space */
		sleep = flush = true;

	} else if (node->is_open) {
		/* Close the file */

		fil_node_close_file(node);
	}

	mutex_exit(&fil_system->mutex);

	if (sleep) {
		os_thread_sleep(20000);

		if (flush) {
			fil_flush(id);
		}

		sleep = flush = false;
		goto retry;
	}

	ut_ad(space->stop_ios);

	char*	new_file_name = new_path_in == NULL
		? fil_make_filepath(NULL, new_name, IBD, false)
		: mem_strdup(new_path_in);
	char*	old_file_name = node->name;
	char*	new_space_name = mem_strdup(new_name);
	char*	old_space_name = space->name;
	ulint	old_fold = ut_fold_string(old_space_name);
	ulint	new_fold = ut_fold_string(new_space_name);

	ut_ad(strchr(old_file_name, OS_PATH_SEPARATOR) != NULL);
	ut_ad(strchr(new_file_name, OS_PATH_SEPARATOR) != NULL);

#ifndef UNIV_HOTBACKUP
	if (!recv_recovery_on) {
		mtr_t		mtr;

		mtr.start();
		fil_name_write_rename(
			id, 0, old_file_name, new_file_name, &mtr);
		mtr.commit();
		log_mutex_enter();
	}
#endif /* !UNIV_HOTBACKUP */

	/* log_sys->mutex is above fil_system->mutex in the latching order */
	ut_ad(log_mutex_own());
	mutex_enter(&fil_system->mutex);

	ut_ad(space->name == old_space_name);
	/* We already checked these. */
	ut_ad(space == fil_space_get_by_name(old_space_name));
	ut_ad(!fil_space_get_by_name(new_space_name));
	ut_ad(node->name == old_file_name);

	bool	success;

	DBUG_EXECUTE_IF("fil_rename_tablespace_failure_2",
			goto skip_rename; );

	success = os_file_rename(
		innodb_data_file_key, old_file_name, new_file_name);

	DBUG_EXECUTE_IF("fil_rename_tablespace_failure_2",
			skip_rename: success = false; );

	ut_ad(node->name == old_file_name);

	if (success) {
		node->name = new_file_name;
	}

#ifndef UNIV_HOTBACKUP
	if (!recv_recovery_on) {
		log_mutex_exit();
	}
#endif /* !UNIV_HOTBACKUP */

	ut_ad(space->name == old_space_name);

	if (success) {
		HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
			    old_fold, space);
		space->name = new_space_name;
		HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
			    new_fold, space);
	} else {
		/* Because nothing was renamed, we must free the new
		names, not the old ones. */
		old_file_name = new_file_name;
		old_space_name = new_space_name;
	}

	ut_ad(space->stop_ios);
	space->stop_ios = false;
	mutex_exit(&fil_system->mutex);

	ut_free(old_file_name);
	ut_free(old_space_name);

	return(success);
}

/** Create a new General or Single-Table tablespace
@param[in]	space_id	Tablespace ID
@param[in]	name		Tablespace name in dbname/tablename format.
For general tablespaces, the 'dbname/' part may be missing.
@param[in]	path		Path and filename of the datafile to create.
@param[in]	flags		Tablespace flags
@param[in]	size		Initial size of the tablespace file in pages,
must be >= FIL_IBD_FILE_INITIAL_SIZE
@return DB_SUCCESS or error code */
dberr_t
fil_ibd_create(
	ulint		space_id,
	const char*	name,
	const char*	path,
	ulint		flags,
	ulint		size)
{
	pfs_os_file_t	file;
	dberr_t		err;
	byte*		buf2;
	byte*		page;
	bool		success;
	bool		is_temp = FSP_FLAGS_GET_TEMPORARY(flags);
	bool		has_data_dir = FSP_FLAGS_HAS_DATA_DIR(flags);
	bool		has_shared_space = FSP_FLAGS_GET_SHARED(flags);
	fil_space_t*	space = NULL;

	ut_ad(!is_system_tablespace(space_id));
	ut_ad(!srv_read_only_mode);
	ut_a(space_id < SRV_LOG_SPACE_FIRST_ID);
	ut_a(size >= FIL_IBD_FILE_INITIAL_SIZE);
	ut_a(fsp_flags_is_valid(flags));

	/* Create the subdirectories in the path, if they are
	not there already. */
	if (!has_shared_space) {
		err = os_file_create_subdirs_if_needed(path);
		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	file = os_file_create(
		innodb_data_file_key, path,
		OS_FILE_CREATE | OS_FILE_ON_ERROR_NO_EXIT,
		OS_FILE_NORMAL,
		OS_DATA_FILE,
		srv_read_only_mode,
		&success);

	if (!success) {
		/* The following call will print an error message */
		ulint	error = os_file_get_last_error(true);

		ib::error() << "Cannot create file '" << path << "'";

		if (error == OS_FILE_ALREADY_EXISTS) {
			ib::error() << "The file '" << path << "'"
				" already exists though the"
				" corresponding table did not exist"
				" in the InnoDB data dictionary."
				" Have you moved InnoDB .ibd files"
				" around without using the SQL commands"
				" DISCARD TABLESPACE and IMPORT TABLESPACE,"
				" or did mysqld crash in the middle of"
				" CREATE TABLE?"
				" You can resolve the problem by removing"
				" the file '" << path
				<< "' under the 'datadir' of MySQL.";

			return(DB_TABLESPACE_EXISTS);
		}

		if (error == OS_FILE_DISK_FULL) {
			return(DB_OUT_OF_FILE_SPACE);
		}

		return(DB_ERROR);
	}

	bool	atomic_write;

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
	if (fil_fusionio_enable_atomic_write(file)) {

		/* This is required by FusionIO HW/Firmware */
		int     ret = posix_fallocate(file.m_file, 0, size * UNIV_PAGE_SIZE);

		if (ret != 0) {

			ib::error() <<
				"posix_fallocate(): Failed to preallocate"
				" data for file " << path
				<< ", desired size "
				<< size * UNIV_PAGE_SIZE
				<< " Operating system error number " << ret
				<< ". Check"
				" that the disk is not full or a disk quota"
				" exceeded. Make sure the file system supports"
				" this function. Some operating system error"
				" numbers are described at " REFMAN
				" operating-system-error-codes.html";

			success = false;
		} else {
			success = true;
		}

		atomic_write = true;
	} else {
		atomic_write = false;

		success = os_file_set_size(
			path, file, size * UNIV_PAGE_SIZE, srv_read_only_mode);
	}
#else
	atomic_write = false;

	success = os_file_set_size(
		path, file, size * UNIV_PAGE_SIZE, srv_read_only_mode);

#endif /* !NO_FALLOCATE && UNIV_LINUX */

	if (!success) {
		os_file_close(file);
		os_file_delete(innodb_data_file_key, path);
		return(DB_OUT_OF_FILE_SPACE);
	}

	/* Note: We are actually punching a hole, previous contents will
	be lost after this call, if it succeeds. In this case the file
	should be full of NULs. */

	bool	punch_hole = os_is_sparse_file_supported(path, file);

	if (punch_hole) {

		dberr_t	punch_err;
		punch_err = os_file_punch_hole(file.m_file, 0, size * UNIV_PAGE_SIZE);
		if (punch_err != DB_SUCCESS) {
			punch_hole = false;
		}
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

	buf2 = static_cast<byte*>(ut_malloc_nokey(3 * UNIV_PAGE_SIZE));
	/* Align the memory for file i/o if we might have O_DIRECT set */
	page = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

	memset(page, '\0', UNIV_PAGE_SIZE);
#ifndef UNIV_HOTBACKUP
	/* Add the UNIV_PAGE_SIZE to the table flags and write them to the
	tablespace header. */
	flags = fsp_flags_set_page_size(flags, univ_page_size);
#endif /* !UNIV_HOTBACKUP */
	fsp_header_init_fields(page, space_id, flags);
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, space_id);

	const page_size_t	page_size(flags);
	IORequest		request(IORequest::WRITE);

	if (!page_size.is_compressed()) {

		buf_flush_init_for_writing(
			NULL, page, NULL, 0,
			fsp_is_checksum_disabled(space_id));

		err = os_file_write(
			request, path, file, page, 0, page_size.physical());

		ut_ad(err != DB_IO_NO_PUNCH_HOLE);

	} else {
		page_zip_des_t	page_zip;

		page_zip_set_size(&page_zip, page_size.physical());
		page_zip.data = page + UNIV_PAGE_SIZE;
#ifdef UNIV_DEBUG
		page_zip.m_start =
#endif /* UNIV_DEBUG */
			page_zip.m_end = page_zip.m_nonempty =
			page_zip.n_blobs = 0;

		buf_flush_init_for_writing(
			NULL, page, &page_zip, 0,
			fsp_is_checksum_disabled(space_id));

		err = os_file_write(
			request, path, file, page_zip.data, 0,
			page_size.physical());

		ut_a(err != DB_IO_NO_PUNCH_HOLE);

		punch_hole = false;
	}

	ut_free(buf2);

	if (err != DB_SUCCESS) {

		ib::error()
			<< "Could not write the first page to"
			<< " tablespace '" << path << "'";

		os_file_close(file);
		os_file_delete(innodb_data_file_key, path);

		return(DB_ERROR);
	}

	success = os_file_flush(file);

	if (!success) {
		ib::error() << "File flush of tablespace '"
			<< path << "' failed";
		os_file_close(file);
		os_file_delete(innodb_data_file_key, path);
		return(DB_ERROR);
	}

	/* MEB creates isl files during copy-back, hence they
	should not be created during apply log operation. */
#ifndef UNIV_HOTBACKUP
	if (has_data_dir || has_shared_space) {
		/* Make the ISL file if the IBD file is not
		in the default location. */
		err = RemoteDatafile::create_link_file(name, path,
						       has_shared_space);
		if (err != DB_SUCCESS) {
			os_file_close(file);
			os_file_delete(innodb_data_file_key, path);
			return(err);
		}
	}
#endif /* !UNIV_HOTBACKUP */
	space = fil_space_create(name, space_id, flags, is_temp
				 ? FIL_TYPE_TEMPORARY : FIL_TYPE_TABLESPACE);

	DEBUG_SYNC_C("fil_ibd_created_space");

	if (!fil_node_create_low(
			path, size, space, false, punch_hole, atomic_write)) {

		err = DB_ERROR;
		goto error_exit_1;
	}

	/* For encryption tablespace, initial encryption information. */
	if (FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
		err = fil_set_encryption(space->id,
					 Encryption::AES,
					 NULL,
					 NULL);
		ut_ad(err == DB_SUCCESS);
	}

#ifndef UNIV_HOTBACKUP
	if (!is_temp) {
		mtr_t			mtr;
		const fil_node_t*	file = UT_LIST_GET_FIRST(space->chain);

		mtr_start(&mtr);
		fil_op_write_log(
			MLOG_FILE_CREATE2, space_id, 0, file->name,
			NULL, space->flags, &mtr);
		fil_name_write(space, 0, file, &mtr);
		mtr_commit(&mtr);
	}
#endif /* !UNIV_HOTBACKUP */
	err = DB_SUCCESS;

	/* Error code is set.  Cleanup the various variables used.
	These labels reflect the order in which variables are assigned or
	actions are done. */
error_exit_1:
	if (err != DB_SUCCESS && (has_data_dir || has_shared_space)) {
		RemoteDatafile::delete_link_file(name);
	}

	os_file_close(file);
	if (err != DB_SUCCESS) {
		os_file_delete(innodb_data_file_key, path);
	}

	return(err);
}

#ifndef UNIV_HOTBACKUP
/** Try to open a single-table tablespace and optionally check that the
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
the first page of the file.  This boolean may be initially false, but if
a remote tablespace is found it will be changed to true.

If the fix_dict boolean is set, then it is safe to use an internal SQL
statement to update the dictionary tables if they are incorrect.

@param[in]	validate	true if we should validate the tablespace
@param[in]	fix_dict	true if the dictionary is available to be fixed
@param[in]	purpose		FIL_TYPE_TABLESPACE or FIL_TYPE_TEMPORARY
@param[in]	id		tablespace ID
@param[in]	flags		tablespace flags
@param[in]	space_name	tablespace name of the datafile
If file-per-table, it is the table name in the databasename/tablename format
@param[in]	path_in		expected filepath, usually read from dictionary
@return DB_SUCCESS or error code */
dberr_t
fil_ibd_open(
	bool		validate,
	bool		fix_dict,
	fil_type_t	purpose,
	ulint		id,
	ulint		flags,
	const char*	space_name,
	const char*	path_in)
{
	dberr_t		err = DB_SUCCESS;
	bool		dict_filepath_same_as_default = false;
	bool		link_file_found = false;
	bool		link_file_is_bad = false;
	bool		is_shared = FSP_FLAGS_GET_SHARED(flags);
	bool		is_encrypted = FSP_FLAGS_GET_ENCRYPTION(flags);
	Datafile	df_default;	/* default location */
	Datafile	df_dict;	/* dictionary location */
	RemoteDatafile	df_remote;	/* remote location */
	ulint		tablespaces_found = 0;
	ulint		valid_tablespaces_found = 0;
	bool		for_import = (purpose == FIL_TYPE_IMPORT);

	ut_ad(!fix_dict || rw_lock_own(dict_operation_lock, RW_LOCK_X));

	ut_ad(!fix_dict || mutex_own(&dict_sys->mutex));
	ut_ad(!fix_dict || !srv_read_only_mode);
	ut_ad(!fix_dict || srv_log_file_size != 0);
	ut_ad(fil_type_is_data(purpose));

	if (!fsp_flags_is_valid(flags)) {
		return(DB_CORRUPTION);
	}

	df_default.init(space_name, flags);
	df_dict.init(space_name, flags);
	df_remote.init(space_name, flags);

	/* Discover the correct file by looking in three possible locations
	while avoiding unecessary effort. */

	if (is_shared) {
		/* Shared tablespaces will have a path_in since the filename
		is not generated from the tablespace name. Use the basename
		from this path_in with the default datadir as a filepath to
		the default location */
		ut_a(path_in);
		const char*	sep = strrchr(path_in, OS_PATH_SEPARATOR);
		const char*	basename = (sep == NULL) ? path_in : &sep[1];
		df_default.make_filepath(NULL, basename, IBD);

		/* Always validate shared tablespaces. */
		validate = true;

		/* Set the ISL filepath in the default location. */
		df_remote.set_link_filepath(path_in);
	} else {
		/* We will always look for an ibd in the default location. */
		df_default.make_filepath(NULL, space_name, IBD);
	}

	/* Look for a filepath embedded in an ISL where the default file
	would be. */
	if (df_remote.open_read_only(true) == DB_SUCCESS) {
		ut_ad(df_remote.is_open());

		/* Always validate a file opened from an ISL pointer */
		validate = true;
		++tablespaces_found;
		link_file_found = true;
	} else if (df_remote.filepath() != NULL) {
		/* An ISL file was found but contained a bad filepath in it.
		Better validate anything we do find. */
		validate = true;
	}

	/* Attempt to open the tablespace at the dictionary filepath. */
	if (path_in) {
		if (df_default.same_filepath_as(path_in)) {
			dict_filepath_same_as_default = true;
		} else {
			/* Dict path is not the default path. Always validate
			remote files. If default is opened, it was moved. */
			validate = true;

			df_dict.set_filepath(path_in);
			if (df_dict.open_read_only(true) == DB_SUCCESS) {
				ut_ad(df_dict.is_open());
				++tablespaces_found;
			}
		}
	}

	/* Always look for a file at the default location. But don't log
	an error if the tablespace is already open in remote or dict. */
	ut_a(df_default.filepath());
	const bool	strict = (tablespaces_found == 0);
	if (df_default.open_read_only(strict) == DB_SUCCESS) {
		ut_ad(df_default.is_open());
		++tablespaces_found;
	}

	/* Check if multiple locations point to the same file. */
	if (tablespaces_found > 1 && df_default.same_as(df_remote)) {
		/* A link file was found with the default path in it.
		Use the default path and delete the link file. */
		--tablespaces_found;
		df_remote.delete_link_file();
		df_remote.close();
	}
	if (tablespaces_found > 1 && df_default.same_as(df_dict)) {
		--tablespaces_found;
		df_dict.close();
	}
	if (tablespaces_found > 1 && df_remote.same_as(df_dict)) {
		--tablespaces_found;
		df_dict.close();
	}

	bool	atomic_write;

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
	if (!srv_use_doublewrite_buf && df_default.is_open()) {

		atomic_write = fil_fusionio_enable_atomic_write(
			df_default.handle());

	} else {
		atomic_write = false;
	}
#else
	atomic_write = false;
#endif /* !NO_FALLOCATE && UNIV_LINUX */

	/*  We have now checked all possible tablespace locations and
	have a count of how many unique files we found.  If things are
	normal, we only found 1. */
	/* For encrypted tablespace, we need to check the
	encryption in header of first page. */
	if (!validate && tablespaces_found == 1 && !is_encrypted) {

		goto skip_validate;
	}

	/* Read and validate the first page of these three tablespace
	locations, if found. */
	valid_tablespaces_found +=
		(df_remote.validate_to_dd(id, flags, for_import)
			== DB_SUCCESS) ? 1 : 0;

	valid_tablespaces_found +=
		(df_default.validate_to_dd(id, flags, for_import)
			== DB_SUCCESS) ? 1 : 0;

	valid_tablespaces_found +=
		(df_dict.validate_to_dd(id, flags, for_import)
			== DB_SUCCESS) ? 1 : 0;

	/* Make sense of these three possible locations.
	First, bail out if no tablespace files were found. */
	if (valid_tablespaces_found == 0) {
		if (!is_encrypted) {
			/* The following call prints an error message.
			For encrypted tablespace we skip print, since it should
			be keyring plugin issues. */
			os_file_get_last_error(true);
			ib::error() << "Could not find a valid tablespace file for `"
				<< space_name << "`. " << TROUBLESHOOT_DATADICT_MSG;
		}

		return(DB_CORRUPTION);
	}

	if (!validate && !is_encrypted) {
		return(DB_SUCCESS);
	}

	if (validate && is_encrypted && fil_space_get(id)) {
		return(DB_SUCCESS);
	}

	/* Do not open any tablespaces if more than one tablespace with
	the correct space ID and flags were found. */
	if (tablespaces_found > 1) {
		ib::error() << "A tablespace for `" << space_name
			<< "` has been found in multiple places;";

		if (df_default.is_open()) {
			ib::error() << "Default location: "
				<< df_default.filepath()
				<< ", Space ID=" << df_default.space_id()
				<< ", Flags=" << df_default.flags();
		}
		if (df_remote.is_open()) {
			ib::error() << "Remote location: "
				<< df_remote.filepath()
				<< ", Space ID=" << df_remote.space_id()
				<< ", Flags=" << df_remote.flags();
		}
		if (df_dict.is_open()) {
			ib::error() << "Dictionary location: "
				<< df_dict.filepath()
				<< ", Space ID=" << df_dict.space_id()
				<< ", Flags=" << df_dict.flags();
		}

		/* Force-recovery will allow some tablespaces to be
		skipped by REDO if there was more than one file found.
		Unlike during the REDO phase of recovery, we now know
		if the tablespace is valid according to the dictionary,
		which was not available then. So if we did not force
		recovery and there is only one good tablespace, ignore
		any bad tablespaces. */
		if (valid_tablespaces_found > 1 || srv_force_recovery > 0) {
			ib::error() << "Will not open tablespace `"
				<< space_name << "`";

			/* If the file is not open it cannot be valid. */
			ut_ad(df_default.is_open() || !df_default.is_valid());
			ut_ad(df_dict.is_open()    || !df_dict.is_valid());
			ut_ad(df_remote.is_open()  || !df_remote.is_valid());

			/* Having established that, this is an easy way to
			look for corrupted data files. */
			if (df_default.is_open() != df_default.is_valid()
			    || df_dict.is_open() != df_dict.is_valid()
			    || df_remote.is_open() != df_remote.is_valid()) {
				return(DB_CORRUPTION);
			}
			return(DB_ERROR);
		}

		/* There is only one valid tablespace found and we did
		not use srv_force_recovery during REDO.  Use this one
		tablespace and clean up invalid tablespace pointers */
		if (df_default.is_open() && !df_default.is_valid()) {
			df_default.close();
			tablespaces_found--;
		}
		if (df_dict.is_open() && !df_dict.is_valid()) {
			df_dict.close();
			/* Leave dict.filepath so that SYS_DATAFILES
			can be corrected below. */
			tablespaces_found--;
		}
		if (df_remote.is_open() && !df_remote.is_valid()) {
			df_remote.close();
			tablespaces_found--;
			link_file_is_bad = true;
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

	/* We may need to update what is stored in SYS_DATAFILES or
	SYS_TABLESPACES or adjust the link file.  Since a failure to
	update SYS_TABLESPACES or SYS_DATAFILES does not prevent opening
	and using the tablespace either this time or the next, we do not
	check the return code or fail to open the tablespace. But if it
	fails, dict_update_filepath() will issue a warning to the log. */
	if (df_dict.filepath()) {
		ut_ad(path_in != NULL);
		ut_ad(df_dict.same_filepath_as(path_in));

		if (df_remote.is_open()) {
			if (!df_remote.same_filepath_as(path_in)) {
				dict_update_filepath(id, df_remote.filepath());
			}

		} else if (df_default.is_open()) {
			ut_ad(!dict_filepath_same_as_default);
			dict_update_filepath(id, df_default.filepath());
			if (link_file_is_bad) {
				RemoteDatafile::delete_link_file(space_name);
			}

		} else if (!is_shared
			   && (!link_file_found || link_file_is_bad)) {
			ut_ad(df_dict.is_open());
			/* Fix the link file if we got our filepath
			from the dictionary but a link file did not
			exist or it did not point to a valid file. */
			RemoteDatafile::delete_link_file(space_name);
			RemoteDatafile::create_link_file(
				space_name, df_dict.filepath());
		}

	} else if (df_remote.is_open()) {
		if (dict_filepath_same_as_default) {
			dict_update_filepath(id, df_remote.filepath());

		} else if (path_in == NULL) {
			/* SYS_DATAFILES record for this space ID
			was not found. */
			dict_replace_tablespace_and_filepath(
				id, space_name, df_remote.filepath(), flags);
		}

	} else if (df_default.is_open()) {
		/* We opened the tablespace in the default location.
		SYS_DATAFILES.PATH needs to be updated if it is different
		from this default path or if the SYS_DATAFILES.PATH was not
		supplied and it should have been. Also update the dictionary
		if we found an ISL file (since !df_remote.is_open).  Since
		path_in is not suppled for file-per-table, we must assume
		that it matched the ISL. */
		if ((path_in != NULL && !dict_filepath_same_as_default)
		    || (path_in == NULL
		        && (DICT_TF_HAS_DATA_DIR(flags)
		            || DICT_TF_HAS_SHARED_SPACE(flags)))
		    || df_remote.filepath() != NULL) {
			dict_replace_tablespace_and_filepath(
				id, space_name, df_default.filepath(), flags);
		}
	}

skip_validate:
	if (err == DB_SUCCESS) {
		fil_space_t*	space = fil_space_create(
			space_name, id, flags, purpose);

		/* We do not measure the size of the file, that is why
		we pass the 0 below */

		if (fil_node_create_low(
			    df_remote.is_open() ? df_remote.filepath() :
			    df_dict.is_open() ? df_dict.filepath() :
			    df_default.filepath(), 0, space, false,
			    true, atomic_write) == NULL) {

			err = DB_ERROR;
		}

		/* For encryption tablespace, initialize encryption
		information.*/
		if (err == DB_SUCCESS && is_encrypted && !for_import) {
			Datafile& df_current = df_remote.is_open() ?
				df_remote: df_dict.is_open() ?
				df_dict : df_default;

			byte*	key = df_current.m_encryption_key;
			byte*	iv = df_current.m_encryption_iv;
			ut_ad(key && iv);

			err = fil_set_encryption(space->id, Encryption::AES,
						 key, iv);
			ut_ad(err == DB_SUCCESS);
		}
	}

	return(err);
}
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_HOTBACKUP
/*******************************************************************//**
Allocates a file name for an old version of a single-table tablespace.
The string must be freed by caller with ut_free()!
@return own: file name */
static
char*
fil_make_ibbackup_old_name(
/*=======================*/
	const char*	name)		/*!< in: original file name */
{
	static const char	suffix[] = "_ibbackup_old_vers_";
	char*			path;
	ulint			len = strlen(name);

	path = static_cast<char*>(ut_malloc_nokey(len + 15 + sizeof(suffix)));

	memcpy(path, name, len);
	memcpy(path + len, suffix, sizeof(suffix) - 1);
	ut_sprintf_timestamp_without_extra_chars(
		path + len + sizeof(suffix) - 1);
	return(path);
}
#endif /* UNIV_HOTBACKUP */

/** Looks for a pre-existing fil_space_t with the given tablespace ID
and, if found, returns the name and filepath in newly allocated buffers
that the caller must free.
@param[in]	space_id	The tablespace ID to search for.
@param[out]	name		Name of the tablespace found.
@param[out]	filepath	The filepath of the first datafile for the
tablespace.
@return true if tablespace is found, false if not. */
bool
fil_space_read_name_and_filepath(
	ulint	space_id,
	char**	name,
	char**	filepath)
{
	bool	success = false;
	*name = NULL;
	*filepath = NULL;

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(space_id);

	if (space != NULL) {
		*name = mem_strdup(space->name);

		fil_node_t* node = UT_LIST_GET_FIRST(space->chain);
		*filepath = mem_strdup(node->name);

		success = true;
	}

	mutex_exit(&fil_system->mutex);

	return(success);
}

/** Convert a file name to a tablespace name.
@param[in]	filename	directory/databasename/tablename.ibd
@return database/tablename string, to be freed with ut_free() */
char*
fil_path_to_space_name(
	const char*	filename)
{
	/* Strip the file name prefix and suffix, leaving
	only databasename/tablename. */
	ulint		filename_len	= strlen(filename);
	const char*	end		= filename + filename_len;
#ifdef HAVE_MEMRCHR
	const char*	tablename	= 1 + static_cast<const char*>(
		memrchr(filename, OS_PATH_SEPARATOR,
			filename_len));
	const char*	dbname		= 1 + static_cast<const char*>(
		memrchr(filename, OS_PATH_SEPARATOR,
			tablename - filename - 1));
#else /* HAVE_MEMRCHR */
	const char*	tablename	= filename;
	const char*	dbname		= NULL;

	while (const char* t = static_cast<const char*>(
		       memchr(tablename, OS_PATH_SEPARATOR,
			      end - tablename))) {
		dbname = tablename;
		tablename = t + 1;
	}
#endif /* HAVE_MEMRCHR */

	ut_ad(dbname != NULL);
	ut_ad(tablename > dbname);
	ut_ad(tablename < end);
	ut_ad(end - tablename > 4);
	ut_ad(memcmp(end - 4, DOT_IBD, 4) == 0);

	char*	name = mem_strdupl(dbname, end - dbname - 4);

	ut_ad(name[tablename - dbname - 1] == OS_PATH_SEPARATOR);
#if OS_PATH_SEPARATOR != '/'
	/* space->name uses '/', not OS_PATH_SEPARATOR. */
	name[tablename - dbname - 1] = '/';
#endif

	return(name);
}

/** Discover the correct IBD file to open given a remote or missing
filepath from the REDO log.  MEB and administrators can move a crashed
database to another location on the same machine and try to recover it.
Remote IBD files might be moved as well to the new location.
    The problem with this is that the REDO log contains the old location
which may be still accessible.  During recovery, if files are found in
both locations, we can chose on based on these priorities;
1. Default location
2. ISL location
3. REDO location
@param[in]	space_id	tablespace ID
@param[in]	df		Datafile object with path from redo
@return true if a valid datafile was found, false if not */
bool
fil_ibd_discover(
	ulint		space_id,
	Datafile&	df)
{
	Datafile	df_def_gen;	/* default general datafile */
	Datafile	df_def_per;	/* default file-per-table datafile */
	RemoteDatafile	df_rem_gen;	/* remote general datafile*/
	RemoteDatafile	df_rem_per;	/* remote file-per-table datafile */

	/* Look for the datafile in the default location. If it is
	a general tablespace, it will be in the datadir. */
	const char*	filename = df.filepath();
	const char*	basename = base_name(filename);
	df_def_gen.init(basename, 0);
	df_def_gen.make_filepath(NULL, basename, IBD);
	if (df_def_gen.open_read_only(false) == DB_SUCCESS
	    && df_def_gen.validate_for_recovery() == DB_SUCCESS
	    && df_def_gen.space_id() == space_id) {
		df.set_filepath(df_def_gen.filepath());
		df.open_read_only(false);
		return(true);
	}

	/* If this datafile is file-per-table it will have a schema dir. */
	ulint		sep_found = 0;
	const char*	db = basename;
	for (; db > filename && sep_found < 2; db--) {
		if (db[0] == OS_PATH_SEPARATOR) {
			sep_found++;
		}
	}
	if (sep_found == 2) {
		db += 2;
		df_def_per.init(db, 0);
		df_def_per.make_filepath(NULL, db, IBD);
		if (df_def_per.open_read_only(false) == DB_SUCCESS
		    && df_def_per.validate_for_recovery() == DB_SUCCESS
		    && df_def_per.space_id() == space_id) {
			df.set_filepath(df_def_per.filepath());
			df.open_read_only(false);
			return(true);
		}
	}

	/* Did not find a general or file-per-table datafile in the
	default location.  Look for a remote general tablespace. */
	df_rem_gen.set_name(basename);
	if (df_rem_gen.open_link_file() == DB_SUCCESS) {

		/* An ISL file was found with contents. */
		if (df_rem_gen.open_read_only(false) != DB_SUCCESS
		    || df_rem_gen.validate_for_recovery() != DB_SUCCESS) {

			/* Assume that this ISL file is intended to be used.
			Do not continue looking for another if this file
			cannot be opened or is not a valid IBD file. */
			ib::error() << "ISL file '"
				<< df_rem_gen.link_filepath()
				<< "' was found but the linked file '"
				<< df_rem_gen.filepath()
				<< "' could not be opened or is not correct.";
			return(false);
		}

		/* Use this file if it has the space_id from the MLOG
		record. */
		if (df_rem_gen.space_id() == space_id) {
			df.set_filepath(df_rem_gen.filepath());
			df.open_read_only(false);
			return(true);
		}

		/* Since old MLOG records can use the same basename in
		multiple CREATE/DROP sequences, this ISL file could be
		pointing to a later version of this basename.ibd file
		which has a different space_id. Keep looking. */
	}

	/* Look for a remote file-per-table tablespace. */
	if (sep_found == 2) {
		df_rem_per.set_name(db);
		if (df_rem_per.open_link_file() == DB_SUCCESS) {

			/* An ISL file was found with contents. */
			if (df_rem_per.open_read_only(false) != DB_SUCCESS
				|| df_rem_per.validate_for_recovery()
				   != DB_SUCCESS) {

				/* Assume that this ISL file is intended to
				be used. Do not continue looking for another
				if this file cannot be opened or is not
				a valid IBD file. */
				ib::error() << "ISL file '"
					<< df_rem_per.link_filepath()
					<< "' was found but the linked file '"
					<< df_rem_per.filepath()
					<< "' could not be opened or is"
					" not correct.";
				return(false);
			}

			/* Use this file if it has the space_id from the
			MLOG record. */
			if (df_rem_per.space_id() == space_id) {
				df.set_filepath(df_rem_per.filepath());
				df.open_read_only(false);
				return(true);
			}

			/* Since old MLOG records can use the same basename
			in multiple CREATE/DROP TABLE sequences, this ISL
			file could be pointing to a later version of this
			basename.ibd file which has a different space_id.
			Keep looking. */
		}
	}

	/* No ISL files were found in the default location. Use the location
	given in the redo log. */
	if (df.open_read_only(false) == DB_SUCCESS
	    && df.validate_for_recovery() == DB_SUCCESS
	    && df.space_id() == space_id) {
		return(true);
	}

	/* A datafile was not discovered for the filename given. */
	return(false);
}

/** Open an ibd tablespace and add it to the InnoDB data structures.
This is similar to fil_ibd_open() except that it is used while processing
the REDO log, so the data dictionary is not available and very little
validation is done. The tablespace name is extracred from the
dbname/tablename.ibd portion of the filename, which assumes that the file
is a file-per-table tablespace.  Any name will do for now.  General
tablespace names will be read from the dictionary after it has been
recovered.  The tablespace flags are read at this time from the first page
of the file in validate_for_recovery().
@param[in]	space_id	tablespace ID
@param[in]	filename	path/to/databasename/tablename.ibd
@param[out]	space		the tablespace, or NULL on error
@return status of the operation */
enum fil_load_status
fil_ibd_load(
	ulint		space_id,
	const char*	filename,
	fil_space_t*&	space)
{
	/* If the a space is already in the file system cache with this
	space ID, then there is nothing to do. */
	mutex_enter(&fil_system->mutex);
	space = fil_space_get_by_id(space_id);
	mutex_exit(&fil_system->mutex);

	if (space != NULL) {
		/* Compare the filename we are trying to open with the
		filename from the first node of the tablespace we opened
		previously. Fail if it is different. */
		fil_node_t* node = UT_LIST_GET_FIRST(space->chain);

		if (0 != strcmp(innobase_basename(filename),
				innobase_basename(node->name))) {
#ifdef  UNIV_HOTBACKUP
			ib::trace()
#else
			ib::info()
#endif /* UNIV_HOTBACKUP */
				<< "Ignoring data file '" << filename
				<< "' with space ID " << space->id
				<< ". Another data file called " << node->name
				<< " exists with the same space ID.";

				space = NULL;
				return(FIL_LOAD_ID_CHANGED);
		}
		return(FIL_LOAD_OK);
	}

	/* If the filepath in the redo log is a default location in or
	under the datadir, then just try to open it there. */
	Datafile	file;
	file.set_filepath(filename);

	Folder		folder(filename, dirname_length(filename));
	if (folder_mysql_datadir >= folder) {
		file.open_read_only(false);
	}

	if (!file.is_open()) {
		/* The file has been moved or it is a remote datafile. */
		if (!fil_ibd_discover(space_id, file)
		    || !file.is_open()) {
			return(FIL_LOAD_NOT_FOUND);
		}
	}

	os_offset_t	size;

	/* Read and validate the first page of the tablespace.
	Assign a tablespace name based on the tablespace type. */
	switch (file.validate_for_recovery()) {
		os_offset_t	minimum_size;
	case DB_SUCCESS:
		if (file.space_id() != space_id) {
#ifdef UNIV_HOTBACKUP
			ib::trace()
#else /* !UNIV_HOTBACKUP */
			ib::info()
#endif /* UNIV_HOTBACKUP */
				<< "Ignoring data file '"
				<< file.filepath()
				<< "' with space ID " << file.space_id()
				<< ", since the redo log references "
				<< file.filepath() << " with space ID "
				<< space_id << ".";
			return(FIL_LOAD_ID_CHANGED);
		}

		/* Get and test the file size. */
		size = os_file_get_size(file.handle());

		/* Every .ibd file is created >= 4 pages in size.
		Smaller files cannot be OK. */
		minimum_size = FIL_IBD_FILE_INITIAL_SIZE * UNIV_PAGE_SIZE;

		if (size == static_cast<os_offset_t>(-1)) {
			/* The following call prints an error message */
			os_file_get_last_error(true);

			ib::error() << "Could not measure the size of"
				" single-table tablespace file '"
				<< file.filepath() << "'";

		} else if (size < minimum_size) {
#ifndef UNIV_HOTBACKUP
			ib::error() << "The size of tablespace file '"
				<< file.filepath() << "' is only " << size
				<< ", should be at least " << minimum_size
				<< "!";
#else
			/* In MEB, we work around this error. */
			file.set_space_id(ULINT_UNDEFINED);
			file.set_flags(0);
			break;
#endif /* !UNIV_HOTBACKUP */
		} else {
			/* Everything is fine so far. */
			break;
		}

		/* Fall through to error handling */

	case DB_TABLESPACE_EXISTS:
#ifdef UNIV_HOTBACKUP
		if (file.flags() == ~(ulint)0) {
			return FIL_LOAD_OK;
		}
#endif /* UNIV_HOTBACKUP */

		return(FIL_LOAD_INVALID);

	default:
		return(FIL_LOAD_NOT_FOUND);
	}

	ut_ad(space == NULL);

#ifdef UNIV_HOTBACKUP
	if (file.space_id() == ULINT_UNDEFINED || file.space_id() == 0) {
		char*	new_path;

		ib::info() << "Renaming tablespace file '" << file.filepath()
			<< "' with space ID " << file.space_id() << " to "
			<< file.name() << "_ibbackup_old_vers_<timestamp>"
			" because its size " << size() << " is too small"
			" (< 4 pages 16 kB each), or the space id in the"
			" file header is not sensible. This can happen in"
			" an mysqlbackup run, and is not dangerous.";
		file.close();

		new_path = fil_make_ibbackup_old_name(file.filepath());

		bool	success = os_file_rename(
			innodb_data_file_key, file.filepath(), new_path);

		ut_a(success);

		ut_free(new_path);

		return(FIL_LOAD_ID_CHANGED);
	}

	/* A backup may contain the same space several times, if the space got
	renamed at a sensitive time. Since it is enough to have one version of
	the space, we rename the file if a space with the same space id
	already exists in the tablespace memory cache. We rather rename the
	file than delete it, because if there is a bug, we do not want to
	destroy valuable data. */

	mutex_enter(&fil_system->mutex);
	space = fil_space_get_by_id(space_id);
	mutex_exit(&fil_system->mutex);

	if (space != NULL) {
		ib::info() << "Renaming data file '" << file.filepath()
			<< "' with space ID " << space_id << " to "
			<< file.name()
			<< "_ibbackup_old_vers_<timestamp> because space "
			<< space->name << " with the same id was scanned"
			" earlier. This can happen if you have renamed tables"
			" during an mysqlbackup run.";
		file.close();

		char*	new_path = fil_make_ibbackup_old_name(file.filepath());

		bool	success = os_file_rename(
			innodb_data_file_key, file.filepath(), new_path);

		ut_a(success);

		ut_free(new_path);
		return(FIL_LOAD_OK);
	}
#endif /* UNIV_HOTBACKUP */

	bool is_temp = FSP_FLAGS_GET_TEMPORARY(file.flags());
	space = fil_space_create(
		file.name(), space_id, file.flags(),
		is_temp ? FIL_TYPE_TEMPORARY : FIL_TYPE_TABLESPACE);

	if (space == NULL) {
		return(FIL_LOAD_INVALID);
	}

	ut_ad(space->id == file.space_id());
	ut_ad(space->id == space_id);

	/* We do not use the size information we have about the file, because
	the rounding formula for extents and pages is somewhat complex; we
	let fil_node_open() do that task. */

	if (!fil_node_create_low(file.filepath(), 0, space,
				 false, true, false)) {
		ut_error;
	}

	/* For encryption tablespace, initial encryption information. */
	if (FSP_FLAGS_GET_ENCRYPTION(space->flags)
	    && file.m_encryption_key != NULL) {
		dberr_t err = fil_set_encryption(space->id,
						 Encryption::AES,
						 file.m_encryption_key,
						 file.m_encryption_iv);
		if (err != DB_SUCCESS) {
			ib::error() << "Can't set encryption information for"
				" tablespace " << space->name << "!";
		}
	}


	return(FIL_LOAD_OK);
}

/***********************************************************************//**
A fault-tolerant function that tries to read the next file name in the
directory. We retry 100 times if os_file_readdir_next_file() returns -1. The
idea is to read as much good data as we can and jump over bad data.
@return 0 if ok, -1 if error even after the retries, 1 if at the end
of the directory */
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

		ib::error() << "os_file_readdir_next_file() returned -1 in"
			" directory " << dirname
			<< ", crash recovery may have failed"
			" for some .ibd files!";

		*err = DB_ERROR;
	}

	return(-1);
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
	ib::error() << "Table " << name
		<< " in the InnoDB data dictionary has tablespace id "
		<< space_id << ","
		" but tablespace with that id or name does not exist. Have"
		" you deleted or moved .ibd files? This may also be a table"
		" created with CREATE TEMPORARY TABLE whose .ibd and .frm"
		" files MySQL automatically removed, but the table still"
		" exists in the InnoDB internal data dictionary.";
}

#ifndef UNIV_HOTBACKUP
/** Returns true if a matching tablespace exists in the InnoDB tablespace
memory cache. Note that if we have not done a crash recovery at the database
startup, there may be many tablespaces which are not yet in the memory cache.
@param[in]	id		Tablespace ID
@param[in]	name		Tablespace name used in fil_space_create().
@param[in]	print_error_if_does_not_exist
				Print detailed error information to the
error log if a matching tablespace is not found from memory.
@param[in]	adjust_space	Whether to adjust space id on mismatch
@param[in]	heap		Heap memory
@param[in]	table_id	table id
@return true if a matching tablespace exists in the memory cache */
bool
fil_space_for_table_exists_in_mem(
	ulint		id,
	const char*	name,
	bool		print_error_if_does_not_exist,
	bool		adjust_space,
	mem_heap_t*	heap,
	table_id_t	table_id)
{
	fil_space_t*	fnamespace = NULL;
	fil_space_t*	space;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	/* Look if there is a space with the same id */

	space = fil_space_get_by_id(id);

	if (space != NULL
	    && FSP_FLAGS_GET_SHARED(space->flags)
	    && adjust_space
	    && srv_sys_tablespaces_open
	    && 0 == strncmp(space->name, general_space_name,
			    strlen(general_space_name))) {
		/* This name was assigned during recovery in fil_ibd_load().
		This general tablespace was opened from an MLOG_FILE_NAME log
		entry where the tablespace name does not exist.  Replace the
		temporary name with this name and return this space. */
		HASH_DELETE(fil_space_t, name_hash, fil_system->name_hash,
			    ut_fold_string(space->name), space);
		ut_free(space->name);
		space->name = mem_strdup(name);
		HASH_INSERT(fil_space_t, name_hash, fil_system->name_hash,
			    ut_fold_string(space->name), space);

		mutex_exit(&fil_system->mutex);

		return(true);
	}

	if (space != NULL) {
		if (FSP_FLAGS_GET_SHARED(space->flags)
		    && !srv_sys_tablespaces_open) {

			/* No need to check the name */
			mutex_exit(&fil_system->mutex);
			return(true);
		}

		/* If this space has the expected name, use it. */
		fnamespace = fil_space_get_by_name(name);
		if (space == fnamespace) {
			/* Found */

			mutex_exit(&fil_system->mutex);

			return(true);
		}
	}

	/* Info from "fnamespace" comes from the ibd file itself, it can
	be different from data obtained from System tables since file
	operations are not transactional. If adjust_space is set, and the
	mismatching space are between a user table and its temp table, we
	shall adjust the ibd file name according to system table info */
	if (adjust_space
	    && space != NULL
	    && row_is_mysql_tmp_table_name(space->name)
	    && !row_is_mysql_tmp_table_name(name)) {

		mutex_exit(&fil_system->mutex);

		DBUG_EXECUTE_IF("ib_crash_before_adjust_fil_space",
				DBUG_SUICIDE(););

		if (fnamespace) {
			const char*	tmp_name;

			tmp_name = dict_mem_create_temporary_tablename(
				heap, name, table_id);

			fil_rename_tablespace(
				fnamespace->id,
				UT_LIST_GET_FIRST(fnamespace->chain)->name,
				tmp_name, NULL);
		}

		DBUG_EXECUTE_IF("ib_crash_after_adjust_one_fil_space",
				DBUG_SUICIDE(););

		fil_rename_tablespace(
			id, UT_LIST_GET_FIRST(space->chain)->name,
			name, NULL);

		DBUG_EXECUTE_IF("ib_crash_after_adjust_fil_space",
				DBUG_SUICIDE(););

		mutex_enter(&fil_system->mutex);
		fnamespace = fil_space_get_by_name(name);
		ut_ad(space == fnamespace);
		mutex_exit(&fil_system->mutex);

		return(true);
	}

	if (!print_error_if_does_not_exist) {

		mutex_exit(&fil_system->mutex);

		return(false);
	}

	if (space == NULL) {
		if (fnamespace == NULL) {
			if (print_error_if_does_not_exist) {
				fil_report_missing_tablespace(name, id);
			}
		} else {
			ib::error() << "Table " << name << " in InnoDB data"
				" dictionary has tablespace id " << id
				<< ", but a tablespace with that id does not"
				" exist. There is a tablespace of name "
				<< fnamespace->name << " and id "
				<< fnamespace->id << ", though. Have you"
				" deleted or moved .ibd files?";
		}
error_exit:
		ib::warn() << TROUBLESHOOT_DATADICT_MSG;

		mutex_exit(&fil_system->mutex);

		return(false);
	}

	if (0 != strcmp(space->name, name)) {

		ib::error() << "Table " << name << " in InnoDB data dictionary"
			" has tablespace id " << id << ", but the tablespace"
			" with that id has name " << space->name << "."
			" Have you deleted or moved .ibd files?";

		if (fnamespace != NULL) {
			ib::error() << "There is a tablespace with the right"
				" name: " << fnamespace->name << ", but its id"
				" is " << fnamespace->id << ".";
		}

		goto error_exit;
	}

	mutex_exit(&fil_system->mutex);

	return(false);
}
#endif /* !UNIV_HOTBACKUP */
/** Return the space ID based on the tablespace name.
The tablespace must be found in the tablespace memory cache.
This call is made from external to this module, so the mutex is not owned.
@param[in]	tablespace	Tablespace name
@return space ID if tablespace found, ULINT_UNDEFINED if space not. */
ulint
fil_space_get_id_by_name(
	const char*	tablespace)
{
	mutex_enter(&fil_system->mutex);

	/* Search for a space with the same name. */
	fil_space_t*	space = fil_space_get_by_name(tablespace);
	ulint		id = (space == NULL) ? ULINT_UNDEFINED : space->id;

	mutex_exit(&fil_system->mutex);

	return(id);
}

/**
Fill the pages with NULs
@param[in] node		File node
@param[in] page_size	physical page size
@param[in] start	Offset from the start of the file in bytes
@param[in] len		Length in bytes
@param[in] read_only_mode
			if true, then read only mode checks are enforced.
@return DB_SUCCESS or error code */
static
dberr_t
fil_write_zeros(
	const fil_node_t*	node,
	ulint			page_size,
	os_offset_t		start,
	ulint			len,
	bool			read_only_mode)
{
	ut_a(len > 0);

	/* Extend at most 1M at a time */
	ulint	n_bytes = ut_min(static_cast<ulint>(1024 * 1024), len);
	byte*	ptr = reinterpret_cast<byte*>(ut_zalloc_nokey(n_bytes
							      + page_size));
	byte*	buf = reinterpret_cast<byte*>(ut_align(ptr, page_size));

	os_offset_t		offset = start;
	dberr_t			err = DB_SUCCESS;
	const os_offset_t	end = start + len;
	IORequest		request(IORequest::WRITE);

	while (offset < end) {

#ifdef UNIV_HOTBACKUP
		err = os_file_write(
			request, node->name, node->handle, buf, offset,
			n_bytes);
#else
		err = os_aio_func(
			request, OS_AIO_SYNC, node->name,
			node->handle, buf, offset, n_bytes, read_only_mode,
			NULL, NULL);
#endif /* UNIV_HOTBACKUP */

		if (err != DB_SUCCESS) {
			break;
		}

		offset += n_bytes;

		n_bytes = ut_min(n_bytes, static_cast<ulint>(end - offset));

		DBUG_EXECUTE_IF("ib_crash_during_tablespace_extension",
				DBUG_SUICIDE(););
	}

	ut_free(ptr);

	return(err);
}

/** Try to extend a tablespace if it is smaller than the specified size.
@param[in,out]	space	tablespace
@param[in]	size	desired size in pages
@return whether the tablespace is at least as big as requested */
bool
fil_space_extend(
	fil_space_t*	space,
	ulint		size)
{
	/* In read-only mode we allow write to shared temporary tablespace
	as intrinsic table created by Optimizer reside in this tablespace. */
	ut_ad(!srv_read_only_mode || fsp_is_system_temporary(space->id));

retry:

#ifdef UNIV_HOTBACKUP
	page_size_t	page_length(space->flags);
	ulint   actual_size = space->size;
	ib::trace() << "space id : " << space->id << ", space name : "
		<< space->name << ", space size : " << actual_size << " pages,"
		<< " desired space size : " << size << " pages,"
		<< " page size : " << page_length.physical();
#endif /* UNIV_HOTBACKUP */

	bool		success = true;

	fil_mutex_enter_and_prepare_for_io(space->id);

	if (space->size >= size) {
		/* Space already big enough */
		mutex_exit(&fil_system->mutex);
		return(true);
	}

	page_size_t	pageSize(space->flags);
	const ulint	page_size = pageSize.physical();
	fil_node_t*	node = UT_LIST_GET_LAST(space->chain);

	if (!node->being_extended) {
		/* Mark this node as undergoing extension. This flag
		is used by other threads to wait for the extension
		opereation to finish. */
		node->being_extended = true;
	} else {
		/* Another thread is currently extending the file. Wait
		for it to finish.  It'd have been better to use an event
		driven mechanism but the entire module is peppered with
		polling code. */

		mutex_exit(&fil_system->mutex);
		os_thread_sleep(100000);
		goto retry;
	}

	if (!fil_node_prepare_for_io(node, fil_system, space)) {
		/* The tablespace data file, such as .ibd file, is missing */
		node->being_extended = false;
		mutex_exit(&fil_system->mutex);

		return(false);
	}

	/* At this point it is safe to release fil_system mutex. No
	other thread can rename, delete or close the file because
	we have set the node->being_extended flag. */
	mutex_exit(&fil_system->mutex);

	ulint		pages_added;

	/* Note: This code is going to be executed independent of FusionIO HW
	if the OS supports posix_fallocate() */

	ut_ad(size > space->size);

	os_offset_t	node_start = os_file_get_size(node->handle);
	ut_a(node_start != (os_offset_t) -1);

	/* Node first page number */
	ulint		node_first_page = space->size - node->size;

	/* Number of physical pages in the node/file */
	ulint		n_node_physical_pages
		= static_cast<ulint>(node_start) / page_size;

	/* Number of pages to extend in the node/file */
	lint		n_node_extend;

	n_node_extend = size - (node_first_page + node->size);

	/* If we already have enough physical pages to satisfy the
	extend request on the node then ignore it */
	if (node->size + n_node_extend > n_node_physical_pages) {

		DBUG_EXECUTE_IF("ib_crash_during_tablespace_extension",
				DBUG_SUICIDE(););

		os_offset_t     len;
		dberr_t		err = DB_SUCCESS;

		len = ((node->size + n_node_extend) * page_size) - node_start;
		ut_ad(len > 0);

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
		int     ret = posix_fallocate(node->handle.m_file, node_start, len);

		DBUG_EXECUTE_IF("ib_posix_fallocate_fail_eintr",
				ret = EINTR;);

		DBUG_EXECUTE_IF("ib_posix_fallocate_fail_einval",
				ret = EINVAL;);

		if (ret != 0) {
			/* We already pass the valid offset and len in,
			if EINVAL is returned, it could only mean that the
			file system doesn't support fallocate(), currently
			one known case is ext3 with O_DIRECT.

			Also because above call could be interrupted,
			in this case, simply go to plan B by writing zeroes.

			Both error messages for above two scenarios are
			skipped in case of flooding error messages, because
			they can be ignored by users. */
			if (ret != EINTR && ret != EINVAL) {
				ib::error()
					<< "posix_fallocate(): Failed to"
					" preallocate data for file "
					<< node->name << ", desired size "
					<< len << " bytes."
					" Operating system error number "
					<< ret << ". Check"
					" that the disk is not full or a disk"
					" quota exceeded. Make sure the file"
					" system supports this function."
					" Some operating system error"
					" numbers are described at " REFMAN
					"operating-system-error-codes.html";
			}

			err = DB_IO_ERROR;
		}
#endif /* NO_FALLOCATE || !UNIV_LINUX */

		if (!node->atomic_write || err == DB_IO_ERROR) {

			bool	read_only_mode;

			read_only_mode = (space->purpose != FIL_TYPE_TEMPORARY
					  ? false : srv_read_only_mode);

			err = fil_write_zeros(
				node, page_size, node_start,
				static_cast<ulint>(len), read_only_mode);

			if (err != DB_SUCCESS) {

				ib::warn()
					<< "Error while writing " << len
					<< " zeroes to " << node->name
					<< " starting at offset " << node_start;
			}
		}

		/* Check how many pages actually added */
		os_offset_t	end = os_file_get_size(node->handle);
		ut_a(end != static_cast<os_offset_t>(-1) && end >= node_start);

		os_has_said_disk_full = !(success = (end == node_start + len));

		pages_added = static_cast<ulint>(end - node_start) / page_size;

	} else {
		success = true;
		pages_added = n_node_extend;
		os_has_said_disk_full = FALSE;
	}

	mutex_enter(&fil_system->mutex);

	ut_a(node->being_extended);

	node->size += pages_added;
	space->size += pages_added;
	node->being_extended = false;

	fil_node_complete_io(node, fil_system, IORequestWrite);

#ifndef UNIV_HOTBACKUP
	/* Keep the last data file size info up to date, rounded to
	full megabytes */
	ulint	pages_per_mb = (1024 * 1024) / page_size;
	ulint	size_in_pages = ((node->size / pages_per_mb) * pages_per_mb);

	if (space->id == srv_sys_space.space_id()) {
		srv_sys_space.set_last_file_size(size_in_pages);
	} else if (space->id == srv_tmp_space.space_id()) {
		srv_tmp_space.set_last_file_size(size_in_pages);
	}
#else
	ib::trace() << "extended space : " << space->name << " from "
		<< actual_size << " pages to " << space->size << " pages "
		<< ", desired space size : " << size << " pages.";
#endif /* !UNIV_HOTBACKUP */

	mutex_exit(&fil_system->mutex);

	fil_flush(space->id);

	return(success);
}

#ifdef UNIV_HOTBACKUP
/********************************************************************//**
Extends all tablespaces to the size stored in the space header. During the
mysqlbackup --apply-log phase we extended the spaces on-demand so that log
records could be applied, but that may have left spaces still too small
compared to the size stored in the space header. */
void
fil_extend_tablespaces_to_stored_len(void)
/*======================================*/
{
	byte*		buf;
	ulint		actual_size;
	ulint		size_in_header;
	dberr_t		error;
	bool		success;

	buf = (byte*)ut_malloc_nokey(UNIV_PAGE_SIZE);

	mutex_enter(&fil_system->mutex);

	for (fil_space_t* space = UT_LIST_GET_FIRST(fil_system->space_list);
	     space != NULL;
	     space = UT_LIST_GET_NEXT(space_list, space)) {

		ut_a(space->purpose == FIL_TYPE_TABLESPACE);

		mutex_exit(&fil_system->mutex); /* no need to protect with a
					      mutex, because this is a
					      single-threaded operation */
		error = fil_read(
			page_id_t(space->id, 0),
			page_size_t(space->flags),
			0, univ_page_size.physical(), buf);

		ut_a(error == DB_SUCCESS);

		size_in_header = fsp_header_get_field(buf, FSP_SIZE);

		success = fil_space_extend(space, size_in_header);
		if (!success) {
			ib::error() << "Could not extend the tablespace of "
				<< space->name  << " to the size stored in"
				" header, " << size_in_header << " pages;"
				" size after extension " << actual_size
				<< " pages. Check that you have free disk"
				" space and retry!";
			ut_a(success);
		}

		mutex_enter(&fil_system->mutex);
	}

	mutex_exit(&fil_system->mutex);

	ut_free(buf);
}
#endif

/*========== RESERVE FREE EXTENTS (for a B-tree split, for example) ===*/

/*******************************************************************//**
Tries to reserve free extents in a file space.
@return true if succeed */
bool
fil_space_reserve_free_extents(
/*===========================*/
	ulint	id,		/*!< in: space id */
	ulint	n_free_now,	/*!< in: number of free extents now */
	ulint	n_to_reserve)	/*!< in: how many one wants to reserve */
{
	fil_space_t*	space;
	bool		success;

	ut_ad(fil_system);

	mutex_enter(&fil_system->mutex);

	space = fil_space_get_by_id(id);

	ut_a(space);

	if (space->n_reserved_extents + n_to_reserve > n_free_now) {
		success = false;
	} else {
		space->n_reserved_extents += n_to_reserve;
		success = true;
	}

	mutex_exit(&fil_system->mutex);

	return(success);
}

/*******************************************************************//**
Releases free extents in a file space. */
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
mutex.
@return false if the file can't be opened, otherwise true */
static
bool
fil_node_prepare_for_io(
/*====================*/
	fil_node_t*	node,	/*!< in: file node */
	fil_system_t*	system,	/*!< in: tablespace memory cache */
	fil_space_t*	space)	/*!< in: space */
{
	ut_ad(node && system && space);
	ut_ad(mutex_own(&(system->mutex)));

	if (system->n_open > system->max_n_open + 5) {
		ib::warn() << "Open files " << system->n_open
			<< " exceeds the limit " << system->max_n_open;
	}

	if (!node->is_open) {
		/* File is closed: open it */
		ut_a(node->n_pending == 0);

		if (!fil_node_open_file(node)) {
			return(false);
		}
	}

	if (node->n_pending == 0 && fil_space_belongs_in_lru(space)) {
		/* The node is in the LRU list, remove it */

		ut_a(UT_LIST_GET_LEN(system->LRU) > 0);

		UT_LIST_REMOVE(system->LRU, node);
	}

	node->n_pending++;

	return(true);
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
	const IORequest&type)	/*!< in: IO_TYPE_*, marks the node as
				modified if TYPE_IS_WRITE() */
{
	ut_ad(mutex_own(&system->mutex));
	ut_a(node->n_pending > 0);

	--node->n_pending;

	ut_ad(type.validate());

	if (type.is_write()) {

		ut_ad(!srv_read_only_mode
		      || fsp_is_system_temporary(node->space->id));

		++system->modification_counter;

		node->modification_counter = system->modification_counter;

		if (fil_buffering_disabled(node->space)) {

			/* We don't need to keep track of unflushed
			changes as user has explicitly disabled
			buffering. */
			ut_ad(!node->space->is_in_unflushed_spaces);
			node->flush_counter = node->modification_counter;

		} else if (!node->space->is_in_unflushed_spaces) {

			node->space->is_in_unflushed_spaces = true;

			UT_LIST_ADD_FIRST(
				system->unflushed_spaces, node->space);
		}
	}

	if (node->n_pending == 0 && fil_space_belongs_in_lru(node->space)) {

		/* The node must be put back to the LRU list */
		UT_LIST_ADD_FIRST(system->LRU, node);
	}
}

/** Report information about an invalid page access. */
static
void
fil_report_invalid_page_access(
	ulint		block_offset,	/*!< in: block offset */
	ulint		space_id,	/*!< in: space id */
	const char*	space_name,	/*!< in: space name */
	ulint		byte_offset,	/*!< in: byte offset */
	ulint		len,		/*!< in: I/O length */
	bool		is_read)	/*!< in: I/O type */
{
	ib::error()
		<< "Trying to access page number " << block_offset << " in"
		" space " << space_id << ", space name " << space_name << ","
		" which is outside the tablespace bounds. Byte offset "
		<< byte_offset << ", len " << len << ", i/o type " <<
		(is_read ? "read" : "write")
		<< ". If you get this error at mysqld startup, please check"
		" that your my.cnf matches the ibdata files that you have in"
		" the MySQL server.";

	ib::error() << "Server exits"
#ifdef UNIV_DEBUG
		<< " at " << __FILE__ << "[" << __LINE__ << "]"
#endif
		<< ".";

	_exit(1);
}

/** Set encryption information for IORequest.
@param[in,out]	req_type	IO request
@param[in]	page_id		page id
@param[in]	space		table space */
inline
void
fil_io_set_encryption(
	IORequest&		req_type,
	const page_id_t&	page_id,
	fil_space_t*		space)
{
	/* Don't encrypt the log, page 0 of all tablespaces, all pages
	from the system tablespace. */
	if (!req_type.is_log() && page_id.page_no() > 0
	    && space->encryption_type != Encryption::NONE)
	{
		req_type.encryption_key(space->encryption_key,
					space->encryption_klen,
					space->encryption_iv);
		req_type.encryption_algorithm(Encryption::AES);
	} else {
		req_type.clear_encrypted();
	}
}

/** Reads or writes data. This operation could be asynchronous (aio).

@param[in,out] type	IO context
@param[in] sync		true if synchronous aio is desired
@param[in] page_id	page id
@param[in] page_size	page size
@param[in] byte_offset	remainder of offset in bytes; in aio this
			must be divisible by the OS block size
@param[in] len		how many bytes to read or write; this must
			not cross a file boundary; in aio this must
			be a block size multiple
@param[in,out] buf	buffer where to store read data or from where
			to write; in aio this must be appropriately
			aligned
@param[in] message	message for aio handler if non-sync aio
			used, else ignored

@return DB_SUCCESS, DB_TABLESPACE_DELETED or DB_TABLESPACE_TRUNCATED
	if we are trying to do i/o on a tablespace which does not exist */
dberr_t
fil_io(
	const IORequest&	type,
	bool			sync,
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ulint			byte_offset,
	ulint			len,
	void*			buf,
	void*			message)
{
	os_offset_t		offset;
	IORequest		req_type(type);

	ut_ad(req_type.validate());

	ut_ad(len > 0);
	ut_ad(byte_offset < UNIV_PAGE_SIZE);
	ut_ad(!page_size.is_compressed() || byte_offset == 0);
	ut_ad(UNIV_PAGE_SIZE == (ulong)(1 << UNIV_PAGE_SIZE_SHIFT));
#if (1 << UNIV_PAGE_SIZE_SHIFT_MAX) != UNIV_PAGE_SIZE_MAX
# error "(1 << UNIV_PAGE_SIZE_SHIFT_MAX) != UNIV_PAGE_SIZE_MAX"
#endif
#if (1 << UNIV_PAGE_SIZE_SHIFT_MIN) != UNIV_PAGE_SIZE_MIN
# error "(1 << UNIV_PAGE_SIZE_SHIFT_MIN) != UNIV_PAGE_SIZE_MIN"
#endif
	ut_ad(fil_validate_skip());

#ifndef UNIV_HOTBACKUP

	/* ibuf bitmap pages must be read in the sync AIO mode: */
	ut_ad(recv_no_ibuf_operations
	      || req_type.is_write()
	      || !ibuf_bitmap_page(page_id, page_size)
	      || sync
	      || req_type.is_log());

	ulint	mode;

	if (sync) {

		mode = OS_AIO_SYNC;

	} else if (req_type.is_log()) {

		mode = OS_AIO_LOG;

	} else if (req_type.is_read()
		   && !recv_no_ibuf_operations
		   && ibuf_page(page_id, page_size, NULL)) {

		mode = OS_AIO_IBUF;

		/* Reduce probability of deadlock bugs in connection with ibuf:
		do not let the ibuf i/o handler sleep */

		req_type.clear_do_not_wake();
	} else {
		mode = OS_AIO_NORMAL;
	}
#else /* !UNIV_HOTBACKUP */
	ut_a(sync);
	ulint mode = OS_AIO_SYNC;
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
	if (req_type.is_read()) {

		srv_stats.data_read.add(len);

	} else if (req_type.is_write()) {

		ut_ad(!srv_read_only_mode
		      || fsp_is_system_temporary(page_id.space()));

		srv_stats.data_written.add(len);
	}
#endif /* !UNIV_HOTBACKUP */

	/* Reserve the fil_system mutex and make sure that we can open at
	least one file while holding it, if the file is not already open */

	fil_mutex_enter_and_prepare_for_io(page_id.space());

	fil_space_t*	space = fil_space_get_by_id(page_id.space());

	/* If we are deleting a tablespace we don't allow async read operations
	on that. However, we do allow write operations and sync read operations. */
	if (space == NULL
	    || (req_type.is_read()
		&& !sync
		&& space->stop_new_ops
		&& !space->is_being_truncated)) {

		mutex_exit(&fil_system->mutex);

		if (!req_type.ignore_missing()) {
			ib::error()
				<< "Trying to do I/O to a tablespace which"
				" does not exist. I/O type: "
				<< (req_type.is_read() ? "read" : "write")
				<< ", page: " << page_id
				<< ", I/O length: " << len << " bytes";
		}

		return(DB_TABLESPACE_DELETED);
	}

	ut_ad(mode != OS_AIO_IBUF || fil_type_is_data(space->purpose));

	ulint		cur_page_no = page_id.page_no();
	fil_node_t*	node = UT_LIST_GET_FIRST(space->chain);

	for (;;) {

		if (node == NULL) {

			if (req_type.ignore_missing()) {
				mutex_exit(&fil_system->mutex);
				return(DB_ERROR);
			}

			fil_report_invalid_page_access(
				page_id.page_no(), page_id.space(),
				space->name, byte_offset, len,
				req_type.is_read());

		} else if (fil_is_user_tablespace_id(space->id)
			   && node->size == 0) {

			/* We do not know the size of a single-table tablespace
			before we open the file */
			break;

		} else if (node->size > cur_page_no) {
			/* Found! */
			break;

		} else {
			if (space->id != srv_sys_space.space_id()
			    && UT_LIST_GET_LEN(space->chain) == 1
			    && (srv_is_tablespace_truncated(space->id)
				|| space->is_being_truncated
				|| srv_was_tablespace_truncated(space))
			    && req_type.is_read()) {

				/* Handle page which is outside the truncated
				tablespace bounds when recovering from a crash
				happened during a truncation */
				mutex_exit(&fil_system->mutex);
				return(DB_TABLESPACE_TRUNCATED);
			}

			cur_page_no -= node->size;

			node = UT_LIST_GET_NEXT(chain, node);
		}
	}

	/* Open file if closed */
	if (!fil_node_prepare_for_io(node, fil_system, space)) {
		if (fil_type_is_data(space->purpose)
		    && fil_is_user_tablespace_id(space->id)) {
			mutex_exit(&fil_system->mutex);

			if (!req_type.ignore_missing()) {
				ib::error()
					<< "Trying to do I/O to a tablespace"
					" which exists without .ibd data file."
					" I/O type: "
					<< (req_type.is_read()
					    ? "read" : "write")
					<< ", page: "
					<< page_id_t(page_id.space(),
						     cur_page_no)
					<< ", I/O length: " << len << " bytes";
			}

			return(DB_TABLESPACE_DELETED);
		}

		/* The tablespace is for log. Currently, we just assert here
		to prevent handling errors along the way fil_io returns.
		Also, if the log files are missing, it would be hard to
		promise the server can continue running. */
		ut_a(0);
	}

	/* Check that at least the start offset is within the bounds of a
	single-table tablespace, including rollback tablespaces. */
	if (node->size <= cur_page_no
	    && space->id != srv_sys_space.space_id()
	    && fil_type_is_data(space->purpose)) {

		if (req_type.ignore_missing()) {
			/* If we can tolerate the non-existent pages, we
			should return with DB_ERROR and let caller decide
			what to do. */
			fil_node_complete_io(node, fil_system, req_type);
			mutex_exit(&fil_system->mutex);
			return(DB_ERROR);
		}

		fil_report_invalid_page_access(
			page_id.page_no(), page_id.space(),
			space->name, byte_offset, len, req_type.is_read());
	}

	/* Now we have made the changes in the data structures of fil_system */
	mutex_exit(&fil_system->mutex);

	/* Calculate the low 32 bits and the high 32 bits of the file offset */

	if (!page_size.is_compressed()) {

		offset = ((os_offset_t) cur_page_no
			  << UNIV_PAGE_SIZE_SHIFT) + byte_offset;

		ut_a(node->size - cur_page_no
		     >= ((byte_offset + len + (UNIV_PAGE_SIZE - 1))
			 / UNIV_PAGE_SIZE));
	} else {
		ulint	size_shift;

		switch (page_size.physical()) {
		case 1024: size_shift = 10; break;
		case 2048: size_shift = 11; break;
		case 4096: size_shift = 12; break;
		case 8192: size_shift = 13; break;
		case 16384: size_shift = 14; break;
		case 32768: size_shift = 15; break;
		case 65536: size_shift = 16; break;
		default: ut_error;
		}

		offset = ((os_offset_t) cur_page_no << size_shift)
			+ byte_offset;

		ut_a(node->size - cur_page_no
		     >= (len + (page_size.physical() - 1))
		     / page_size.physical());
	}

	/* Do AIO */

	ut_a(byte_offset % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_a((len % OS_FILE_LOG_BLOCK_SIZE) == 0);

	/* Don't compress the log, page 0 of all tablespaces, tables
	compresssed with the old scheme and all pages from the system
	tablespace. */

	if (req_type.is_write()
	    && !req_type.is_log()
	    && !page_size.is_compressed()
	    && page_id.page_no() > 0
	    && IORequest::is_punch_hole_supported()
	    && node->punch_hole) {

		ut_ad(!req_type.is_log());

		req_type.set_punch_hole();

		req_type.compression_algorithm(space->compression_type);

	} else {
		req_type.clear_compressed();
	}

	/* Set encryption information. */
	fil_io_set_encryption(req_type, page_id, space);

	req_type.block_size(node->block_size);

	dberr_t	err;

#ifdef UNIV_HOTBACKUP
	/* In mysqlbackup do normal i/o, not aio */
	if (req_type.is_read()) {

		err = os_file_read(req_type, node->handle, buf, offset, len);

	} else {

		ut_ad(!srv_read_only_mode
		      || fsp_is_system_temporary(page_id.space()));

		err = os_file_write(
			req_type, node->name, node->handle, buf, offset, len);
	}
#else /* UNIV_HOTBACKUP */
	/* Queue the aio request */
	err = os_aio(
		req_type,
		mode, node->name, node->handle, buf, offset, len,
		fsp_is_system_temporary(page_id.space())
		? false : srv_read_only_mode,
		node, message);

#endif /* UNIV_HOTBACKUP */

	if (err == DB_IO_NO_PUNCH_HOLE) {

		err = DB_SUCCESS;

		if (node->punch_hole) {

			ib::warn()
				<< "Punch hole failed for '"
				<< node->name << "'";
		}

		fil_no_punch_hole(node);
	}

	/* We an try to recover the page from the double write buffer if
	the decompression fails or the page is corrupt. */

	ut_a(req_type.is_dblwr_recover() || err == DB_SUCCESS);

	if (sync) {
		/* The i/o operation is already completed when we return from
		os_aio: */

		mutex_enter(&fil_system->mutex);

		fil_node_complete_io(node, fil_system, req_type);

		mutex_exit(&fil_system->mutex);

		ut_ad(fil_validate_skip());
	}

	return(err);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.cc for more info). The thread specifies which
segment it wants to wait for. */
void
fil_aio_wait(
/*=========*/
	ulint	segment)	/*!< in: the number of the segment in the aio
				array to wait for */
{
	fil_node_t*	node;
	IORequest	type;
	void*		message;

	ut_ad(fil_validate_skip());

	dberr_t	err = os_aio_handler(segment, &node, &message, &type);

	ut_a(err == DB_SUCCESS);

	if (node == NULL) {
		ut_ad(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS);
		return;
	}

	srv_set_io_thread_op_info(segment, "complete io for fil node");

	mutex_enter(&fil_system->mutex);

	fil_node_complete_io(node, fil_system, type);

	mutex_exit(&fil_system->mutex);

	ut_ad(fil_validate_skip());

	/* Do the i/o handling */
	/* IMPORTANT: since i/o handling for reads will read also the insert
	buffer in tablespace 0, you have to be very careful not to introduce
	deadlocks in the i/o system. We keep tablespace 0 data files always
	open, and use a special i/o thread to serve insert buffer requests. */

	switch (node->space->purpose) {
	case FIL_TYPE_TABLESPACE:
	case FIL_TYPE_TEMPORARY:
	case FIL_TYPE_IMPORT:
		srv_set_io_thread_op_info(segment, "complete io for buf page");

		/* async single page writes from the dblwr buffer don't have
		access to the page */
		if (message != NULL) {
			buf_page_io_complete(static_cast<buf_page_t*>(message));
		}
		return;
	case FIL_TYPE_LOG:
		srv_set_io_thread_op_info(segment, "complete io for log");
		log_io_complete(static_cast<log_group_t*>(message));
		return;
	}

	ut_ad(0);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Flushes to disk possible writes cached by the OS. If the space does not exist
or is being dropped, does not do anything. */
void
fil_flush(
/*======*/
	ulint	space_id)	/*!< in: file space id (this can be a group of
				log files or a tablespace of the database) */
{
	fil_node_t*	node;
	pfs_os_file_t	file;

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(space_id);

	if (space == NULL
	    || space->purpose == FIL_TYPE_TEMPORARY
	    || space->stop_new_ops
	    || space->is_being_truncated) {
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
	for (node = UT_LIST_GET_FIRST(space->chain);
	     node != NULL;
	     node = UT_LIST_GET_NEXT(chain, node)) {

		int64_t	old_mod_counter = node->modification_counter;

		if (old_mod_counter <= node->flush_counter) {
			continue;
		}

		ut_a(node->is_open);

		switch (space->purpose) {
		case FIL_TYPE_TEMPORARY:
			ut_ad(0); // we already checked for this
		case FIL_TYPE_TABLESPACE:
		case FIL_TYPE_IMPORT:
			fil_n_pending_tablespace_flushes++;
			break;
		case FIL_TYPE_LOG:
			fil_n_pending_log_flushes++;
			fil_n_log_flushes++;
			break;
		}
#ifdef _WIN32
		if (node->is_raw_disk) {

			goto skip_flush;
		}
#endif /* _WIN32 */
retry:
		if (node->n_pending_flushes > 0) {
			/* We want to avoid calling os_file_flush() on
			the file twice at the same time, because we do
			not know what bugs OS's may contain in file
			i/o */

#ifndef UNIV_HOTBACKUP
			int64_t	sig_count = os_event_reset(node->sync_event);
#endif /* !UNIV_HOTBACKUP */

			mutex_exit(&fil_system->mutex);

			os_event_wait_low(node->sync_event, sig_count);

			mutex_enter(&fil_system->mutex);

			if (node->flush_counter >= old_mod_counter) {

				goto skip_flush;
			}

			goto retry;
		}

		ut_a(node->is_open);
		file = node->handle;
		node->n_pending_flushes++;

		mutex_exit(&fil_system->mutex);

		os_file_flush(file);

		mutex_enter(&fil_system->mutex);

		os_event_set(node->sync_event);

		node->n_pending_flushes--;
skip_flush:
		if (node->flush_counter < old_mod_counter) {
			node->flush_counter = old_mod_counter;

			if (space->is_in_unflushed_spaces
			    && fil_space_is_flushed(space)) {

				space->is_in_unflushed_spaces = false;

				UT_LIST_REMOVE(
					fil_system->unflushed_spaces,
					space);
			}
		}

		switch (space->purpose) {
		case FIL_TYPE_TEMPORARY:
			ut_ad(0); // we already checked for this
		case FIL_TYPE_TABLESPACE:
		case FIL_TYPE_IMPORT:
			fil_n_pending_tablespace_flushes--;
			continue;
		case FIL_TYPE_LOG:
			fil_n_pending_log_flushes--;
			continue;
		}

		ut_ad(0);
	}

	space->n_pending_flushes--;

	mutex_exit(&fil_system->mutex);
}

/** Flush to disk the writes in file spaces of the given type
possibly cached by the OS.
@param[in]	purpose	FIL_TYPE_TABLESPACE or FIL_TYPE_LOG */
void
fil_flush_file_spaces(
	fil_type_t	purpose)
{
	fil_space_t*	space;
	ulint*		space_ids;
	ulint		n_space_ids;

	ut_ad(purpose == FIL_TYPE_TABLESPACE || purpose == FIL_TYPE_LOG);

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
		ut_malloc_nokey(n_space_ids * sizeof(*space_ids)));

	n_space_ids = 0;

	for (space = UT_LIST_GET_FIRST(fil_system->unflushed_spaces);
	     space;
	     space = UT_LIST_GET_NEXT(unflushed_spaces, space)) {

		if (space->purpose == purpose
		    && !space->stop_new_ops
		    && !space->is_being_truncated) {

			space_ids[n_space_ids++] = space->id;
		}
	}

	mutex_exit(&fil_system->mutex);

	/* Flush the spaces.  It will not hurt to call fil_flush() on
	a non-existing space id. */
	for (ulint i = 0; i < n_space_ids; i++) {

		fil_flush(space_ids[i]);
	}

	ut_free(space_ids);
}

/** Functor to validate the file node list of a tablespace. */
struct	Check {
	/** Total size of file nodes visited so far */
	ulint	size;
	/** Total number of open files visited so far */
	ulint	n_open;

	/** Constructor */
	Check() : size(0), n_open(0) {}

	/** Visit a file node
	@param[in]	elem	file node to visit */
	void	operator()(const fil_node_t* elem)
	{
		ut_a(elem->is_open || !elem->n_pending);
		n_open += elem->is_open;
		size += elem->size;
	}

	/** Validate a tablespace.
	@param[in]	space	tablespace to validate
	@return		number of open file nodes */
	static ulint validate(const fil_space_t* space)
	{
		ut_ad(mutex_own(&fil_system->mutex));
		Check	check;
		ut_list_validate(space->chain, check);
		ut_a(space->size == check.size);
		return(check.n_open);
	}
};

/******************************************************************//**
Checks the consistency of the tablespace cache.
@return true if ok */
bool
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

			n_open += Check::validate(space);
		}
	}

	ut_a(fil_system->n_open == n_open);

	UT_LIST_CHECK(fil_system->LRU);

	for (fil_node = UT_LIST_GET_FIRST(fil_system->LRU);
	     fil_node != 0;
	     fil_node = UT_LIST_GET_NEXT(LRU, fil_node)) {

		ut_a(fil_node->n_pending == 0);
		ut_a(!fil_node->being_extended);
		ut_a(fil_node->is_open);
		ut_a(fil_space_belongs_in_lru(fil_node->space));
	}

	mutex_exit(&fil_system->mutex);

	return(true);
}

/********************************************************************//**
Returns true if file address is undefined.
@return true if undefined */
bool
fil_addr_is_null(
/*=============*/
	fil_addr_t	addr)	/*!< in: address */
{
	return(addr.page == FIL_NULL);
}

/********************************************************************//**
Get the predecessor of a file page.
@return FIL_PAGE_PREV */
ulint
fil_page_get_prev(
/*==============*/
	const byte*	page)	/*!< in: file page */
{
	return(mach_read_from_4(page + FIL_PAGE_PREV));
}

/********************************************************************//**
Get the successor of a file page.
@return FIL_PAGE_NEXT */
ulint
fil_page_get_next(
/*==============*/
	const byte*	page)	/*!< in: file page */
{
	return(mach_read_from_4(page + FIL_PAGE_NEXT));
}

/*********************************************************************//**
Sets the file page type. */
void
fil_page_set_type(
/*==============*/
	byte*	page,	/*!< in/out: file page */
	ulint	type)	/*!< in: type */
{
	ut_ad(page);

	mach_write_to_2(page + FIL_PAGE_TYPE, type);
}

#ifndef UNIV_HOTBACKUP
/** Reset the page type.
Data files created before MySQL 5.1 may contain garbage in FIL_PAGE_TYPE.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in]	page_id	page number
@param[in,out]	page	page with invalid FIL_PAGE_TYPE
@param[in]	type	expected page type
@param[in,out]	mtr	mini-transaction */
void
fil_page_reset_type(
	const page_id_t&	page_id,
	byte*			page,
	ulint			type,
	mtr_t*			mtr)
{
	ib::info()
		<< "Resetting invalid page " << page_id << " type "
		<< fil_page_get_type(page) << " to " << type << ".";
	mlog_write_ulint(page + FIL_PAGE_TYPE, type, MLOG_2BYTES, mtr);
}
#endif /* !UNIV_HOTBACKUP */

/****************************************************************//**
Closes the tablespace memory cache. */
void
fil_close(void)
/*===========*/
{
	hash_table_free(fil_system->spaces);

	hash_table_free(fil_system->name_hash);

	ut_a(UT_LIST_GET_LEN(fil_system->LRU) == 0);
	ut_a(UT_LIST_GET_LEN(fil_system->unflushed_spaces) == 0);
	ut_a(UT_LIST_GET_LEN(fil_system->space_list) == 0);

	mutex_free(&fil_system->mutex);

	ut_free(fil_system);
	fil_system = NULL;
}

#ifndef UNIV_HOTBACKUP
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
	pfs_os_file_t	file;			/*!< File handle */
	const char*	filepath;		/*!< File path name */
	os_offset_t	start;			/*!< From where to start */
	os_offset_t	end;			/*!< Where to stop */
	os_offset_t	file_size;		/*!< File size in bytes */
	ulint		page_size;		/*!< Page size */
	ulint		n_io_buffers;		/*!< Number of pages to use
						for IO */
	byte*		io_buffer;		/*!< Buffer to use for IO */
	byte*		encryption_key;		/*!< Encryption key */
	byte*		encryption_iv;		/*!< Encryption iv */
};

/********************************************************************//**
TODO: This can be made parallel trivially by chunking up the file and creating
a callback per thread. Main benefit will be to use multiple CPUs for
checksums and compressed tables. We have to do compressed tables block by
block right now. Secondly we need to decompress/compress and copy too much
of data. These are CPU intensive.

Iterate over all the pages in the tablespace.
@param iter Tablespace iterator
@param block block to use for IO
@param callback Callback to inspect and update page contents
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

	/* For old style compressed tables we do a lot of useless copying
	for non-index pages. Unfortunately, it is required by
	buf_zip_decompress() */

	ulint	read_type = IORequest::READ;
	ulint	write_type = IORequest::WRITE;

	for (offset = iter.start; offset < iter.end; offset += n_bytes) {

		byte*	io_buffer = iter.io_buffer;

		block->frame = io_buffer;

		if (callback.get_page_size().is_compressed()) {
			page_zip_des_init(&block->page.zip);
			page_zip_set_size(&block->page.zip, iter.page_size);

			block->page.size.copy_from(
				page_size_t(iter.page_size,
					    univ_page_size.logical(),
					    true));

			block->page.zip.data = block->frame + UNIV_PAGE_SIZE;
			ut_d(block->page.zip.m_external = true);
			ut_ad(iter.page_size
			      == callback.get_page_size().physical());

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

		dberr_t		err;
		IORequest	read_request(read_type);

		/* For encrypted table, set encryption information. */
		if (iter.encryption_key != NULL && offset != 0) {
			read_request.encryption_key(iter.encryption_key,
						    ENCRYPTION_KEY_LEN,
						    iter.encryption_iv);
			read_request.encryption_algorithm(Encryption::AES);
		}

		err = os_file_read(
			read_request, iter.file, io_buffer, offset,
			(ulint) n_bytes);

		if (err != DB_SUCCESS) {

			ib::error() << "os_file_read() failed";

			return(err);
		}

		bool		updated = false;
		os_offset_t	page_off = offset;
		ulint		n_pages_read = (ulint) n_bytes / iter.page_size;

		for (ulint i = 0; i < n_pages_read; ++i) {

			buf_block_set_file_page(
				block, page_id_t(space_id, page_no++));

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

		IORequest	write_request(write_type);

		/* For encrypted table, set encryption information. */
		if (iter.encryption_key != NULL && offset != 0) {
			write_request.encryption_key(iter.encryption_key,
						     ENCRYPTION_KEY_LEN,
						     iter.encryption_iv);
			write_request.encryption_algorithm(Encryption::AES);
		}

		/* A page was updated in the set, write back to disk.
		Note: We don't have the compression algorithm, we write
		out the imported file as uncompressed. */

		if (updated
		    && (err = os_file_write(
				write_request,
				iter.filepath, iter.file, io_buffer,
				offset, (ulint) n_bytes)) != DB_SUCCESS) {

			/* This is not a hard error */
			if (err == DB_IO_NO_PUNCH_HOLE) {

				err = DB_SUCCESS;
				write_type &= ~IORequest::PUNCH_HOLE;

			} else {
				ib::error() << "os_file_write() failed";

				return(err);
			}
		}
	}

	return(DB_SUCCESS);
}

/********************************************************************//**
Iterate over all the pages in the tablespace.
@param table the table definiton in the server
@param n_io_buffers number of blocks to read and write together
@param callback functor that will do the page updates
@return DB_SUCCESS or error code */
dberr_t
fil_tablespace_iterate(
/*===================*/
	dict_table_t*	table,
	ulint		n_io_buffers,
	PageCallback&	callback)
{
	dberr_t		err;
	pfs_os_file_t	file;
	char*		filepath;
	bool		success;

	ut_a(n_io_buffers > 0);
	ut_ad(!srv_read_only_mode);

	DBUG_EXECUTE_IF("ib_import_trigger_corruption_1",
			return(DB_CORRUPTION););

	/* Make sure the data_dir_path is set. */
	dict_get_and_save_data_dir_path(table, false);

	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		ut_a(table->data_dir_path);

		filepath = fil_make_filepath(
			table->data_dir_path, table->name.m_name, IBD, true);
	} else {
		filepath = fil_make_filepath(
			NULL, table->name.m_name, IBD, false);
	}

	if (filepath == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	file = os_file_create_simple_no_error_handling(
		innodb_data_file_key, filepath,
		OS_FILE_OPEN, OS_FILE_READ_WRITE, srv_read_only_mode, &success);

	DBUG_EXECUTE_IF("fil_tablespace_iterate_failure",
	{
		static bool once;

		if (!once || ut_rnd_interval(0, 10) == 5) {
			once = true;
			success = false;
			os_file_close(file);
		}
	});

	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib::error() << "Trying to import a tablespace, but could not"
			" open the tablespace file " << filepath;

		ut_free(filepath);

		return(DB_TABLESPACE_NOT_FOUND);

	} else {
		err = DB_SUCCESS;
	}

	callback.set_file(filepath, file);

	os_offset_t	file_size = os_file_get_size(file);
	ut_a(file_size != (os_offset_t) -1);

	/* The block we will use for every physical page */
	buf_block_t*	block;

	block = reinterpret_cast<buf_block_t*>(ut_zalloc_nokey(sizeof(*block)));

	mutex_create(LATCH_ID_BUF_BLOCK_MUTEX, &block->mutex);

	/* Allocate a page to read in the tablespace header, so that we
	can determine the page size and zip size (if it is compressed).
	We allocate an extra page in case it is a compressed table. One
	page is to ensure alignement. */

	void*	page_ptr = ut_malloc_nokey(3 * UNIV_PAGE_SIZE);
	byte*	page = static_cast<byte*>(ut_align(page_ptr, UNIV_PAGE_SIZE));

	fil_buf_block_init(block, page);

	/* Read the first page and determine the page and zip size. */

	IORequest	request(IORequest::READ);

	err = os_file_read(request, file, page, 0, UNIV_PAGE_SIZE);

	if (err != DB_SUCCESS) {

		err = DB_IO_ERROR;

	} else if ((err = callback.init(file_size, block)) == DB_SUCCESS) {
		fil_iterator_t	iter;

		iter.file = file;
		iter.start = 0;
		iter.end = file_size;
		iter.filepath = filepath;
		iter.file_size = file_size;
		iter.n_io_buffers = n_io_buffers;
		iter.page_size = callback.get_page_size().physical();

		/* Set encryption info. */
		iter.encryption_key = table->encryption_key;
		iter.encryption_iv = table->encryption_iv;

		/* Check encryption is matched or not. */
		ulint	space_flags = callback.get_space_flags();
		if (FSP_FLAGS_GET_ENCRYPTION(space_flags)) {
			ut_ad(table->encryption_key != NULL);

			if (!dict_table_is_encrypted(table)) {
				ib::error() << "Table is not in an encrypted"
					" tablespace, but the data file which"
					" trying to import is an encrypted"
					" tablespace";
				err = DB_IO_NO_ENCRYPT_TABLESPACE;
			}
		}

		if (err == DB_SUCCESS) {

			/* Compressed pages can't be optimised for block IO
			for now.  We do the IMPORT page by page. */

			if (callback.get_page_size().is_compressed()) {
				iter.n_io_buffers = 1;
				ut_a(iter.page_size
				     == callback.get_page_size().physical());
			}

			/** Add an extra page for compressed page scratch
			area. */
			void*	io_buffer = ut_malloc_nokey(
				(2 + iter.n_io_buffers) * UNIV_PAGE_SIZE);

			iter.io_buffer = static_cast<byte*>(
				ut_align(io_buffer, UNIV_PAGE_SIZE));

			err = fil_iterate(iter, block, callback);

			ut_free(io_buffer);
		}
	}

	if (err == DB_SUCCESS) {

		ib::info() << "Sync to disk";

		if (!os_file_flush(file)) {
			ib::info() << "os_file_flush() failed!";
			err = DB_IO_ERROR;
		} else {
			ib::info() << "Sync to disk - done!";
		}
	}

	os_file_close(file);

	ut_free(page_ptr);
	ut_free(filepath);

	mutex_free(&block->mutex);

	ut_free(block);

	return(err);
}
#endif /* !UNIV_HOTBACKUP */

/** Set the tablespace table size.
@param[in]	page	a page belonging to the tablespace */
void
PageCallback::set_page_size(
	const buf_frame_t*	page) UNIV_NOTHROW
{
	m_page_size.copy_from(fsp_header_get_page_size(page));
}

/********************************************************************//**
Delete the tablespace file and any related files like .cfg.
This should not be called for temporary tables.
@param[in] ibd_filepath File path of the IBD tablespace */
void
fil_delete_file(
/*============*/
	const char*	ibd_filepath)
{
	/* Force a delete of any stale .ibd files that are lying around. */

	ib::info() << "Deleting " << ibd_filepath;

	os_file_delete_if_exists(innodb_data_file_key, ibd_filepath, NULL);

	char*	cfg_filepath = fil_make_filepath(
		ibd_filepath, NULL, CFG, false);
	if (cfg_filepath != NULL) {
		os_file_delete_if_exists(
			innodb_data_file_key, cfg_filepath, NULL);
		ut_free(cfg_filepath);
	}

	char*	cfp_filepath = fil_make_filepath(
		ibd_filepath, NULL, CFP, false);
	if (cfp_filepath != NULL) {
		os_file_delete_if_exists(
			innodb_data_file_key, cfp_filepath, NULL);
		ut_free(cfp_filepath);
	}
}

/**
Iterate over all the spaces in the space list and fetch the
tablespace names. It will return a copy of the name that must be
freed by the caller using: delete[].
@return DB_SUCCESS if all OK. */
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

		if (space->purpose == FIL_TYPE_TABLESPACE) {
			ulint	len;
			char*	name;

			len = ::strlen(space->name);
			name = UT_NEW_ARRAY_NOKEY(char, len + 1);

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

#ifndef UNIV_HOTBACKUP
/** Return the next fil_node_t in the current or next fil_space_t.
Once started, the caller must keep calling this until it returns NULL.
fil_space_acquire() and fil_space_release() are invoked here which
blocks a concurrent operation from dropping the tablespace.
@param[in]	prev_node	Pointer to the previous fil_node_t.
If NULL, use the first fil_space_t on fil_system->space_list.
@return pointer to the next fil_node_t.
@retval NULL if this was the last file node */
const fil_node_t*
fil_node_next(
	const fil_node_t*	prev_node)
{
	fil_space_t*		space;
	const fil_node_t*	node = prev_node;

	mutex_enter(&fil_system->mutex);

	if (node == NULL) {
		space = UT_LIST_GET_FIRST(fil_system->space_list);

		/* We can trust that space is not NULL because at least the
		system tablespace is always present and loaded first. */
		space->n_pending_ops++;

		node = UT_LIST_GET_FIRST(space->chain);
		ut_ad(node != NULL);
	} else {
		space = node->space;
		ut_ad(space->n_pending_ops > 0);
		node = UT_LIST_GET_NEXT(chain, node);

		if (node == NULL) {
			/* Move on to the next fil_space_t */
			space->n_pending_ops--;
			space = UT_LIST_GET_NEXT(space_list, space);

			/* Skip spaces that are being
			created by fil_ibd_create(),
			or dropped or truncated. */
			while (space != NULL
			       && (UT_LIST_GET_LEN(space->chain) == 0
				   || space->stop_new_ops
				   || space->is_being_truncated)) {
				space = UT_LIST_GET_NEXT(space_list, space);
			}

			if (space != NULL) {
				space->n_pending_ops++;
				node = UT_LIST_GET_FIRST(space->chain);
				ut_ad(node != NULL);
			}
		}
	}

	mutex_exit(&fil_system->mutex);

	return(node);
}

/** Generate redo log for swapping two .ibd files
@param[in]	old_table	old table
@param[in]	new_table	new table
@param[in]	tmp_name	temporary table name
@param[in,out]	mtr		mini-transaction
@return innodb error code */
dberr_t
fil_mtr_rename_log(
	const dict_table_t*	old_table,
	const dict_table_t*	new_table,
	const char*		tmp_name,
	mtr_t*			mtr)
{
	dberr_t	err;

	bool	old_is_file_per_table =
		!is_system_tablespace(old_table->space)
		&& !DICT_TF_HAS_SHARED_SPACE(old_table->flags);

	bool	new_is_file_per_table =
		!is_system_tablespace(new_table->space)
		&& !DICT_TF_HAS_SHARED_SPACE(new_table->flags);

	/* If neither table is file-per-table,
	there will be no renaming of files. */
	if (!old_is_file_per_table && !new_is_file_per_table) {
		return(DB_SUCCESS);
	}

	const char*	old_dir = DICT_TF_HAS_DATA_DIR(old_table->flags)
		? old_table->data_dir_path
		: NULL;

	char*	old_path = fil_make_filepath(
		old_dir, old_table->name.m_name, IBD, (old_dir != NULL));
	if (old_path == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	if (old_is_file_per_table) {
		char*	tmp_path = fil_make_filepath(
			old_dir, tmp_name, IBD, (old_dir != NULL));
		if (tmp_path == NULL) {
			ut_free(old_path);
			return(DB_OUT_OF_MEMORY);
		}

		/* Temp filepath must not exist. */
		err = fil_rename_tablespace_check(
			old_table->space, old_path, tmp_path,
			dict_table_is_discarded(old_table));
		if (err != DB_SUCCESS) {
			ut_free(old_path);
			ut_free(tmp_path);
			return(err);
		}

		fil_name_write_rename(
			old_table->space, 0, old_path, tmp_path, mtr);

		ut_free(tmp_path);
	}

	if (new_is_file_per_table) {
		const char*	new_dir = DICT_TF_HAS_DATA_DIR(new_table->flags)
			? new_table->data_dir_path
			: NULL;
		char*	new_path = fil_make_filepath(
				new_dir, new_table->name.m_name,
				IBD, (new_dir != NULL));
		if (new_path == NULL) {
			ut_free(old_path);
			return(DB_OUT_OF_MEMORY);
		}

		/* Destination filepath must not exist unless this ALTER
		TABLE starts and ends with a file_per-table tablespace. */
		if (!old_is_file_per_table) {
			err = fil_rename_tablespace_check(
				new_table->space, new_path, old_path,
				dict_table_is_discarded(new_table));
			if (err != DB_SUCCESS) {
				ut_free(old_path);
				ut_free(new_path);
				return(err);
			}
		}

		fil_name_write_rename(
			new_table->space, 0, new_path, old_path, mtr);

		ut_free(new_path);
	}

	ut_free(old_path);

	return(DB_SUCCESS);
}
#endif /* !UNIV_HOTBACKUP */
#ifdef UNIV_DEBUG
/** Check that a tablespace is valid for mtr_commit().
@param[in]	space	persistent tablespace that has been changed */
static
void
fil_space_validate_for_mtr_commit(
	const fil_space_t*	space)
{
	ut_ad(!mutex_own(&fil_system->mutex));
	ut_ad(space != NULL);
	ut_ad(space->purpose == FIL_TYPE_TABLESPACE);
	ut_ad(!is_predefined_tablespace(space->id));

	/* We are serving mtr_commit(). While there is an active
	mini-transaction, we should have !space->stop_new_ops. This is
	guaranteed by meta-data locks or transactional locks, or
	dict_operation_lock (X-lock in DROP, S-lock in purge).

	However, a file I/O thread can invoke change buffer merge
	while fil_check_pending_operations() is waiting for operations
	to quiesce. This is not a problem, because
	ibuf_merge_or_delete_for_page() would call
	fil_space_acquire() before mtr_start() and
	fil_space_release() after mtr_commit(). This is why
	n_pending_ops should not be zero if stop_new_ops is set. */
	ut_ad(!space->stop_new_ops
	      || space->is_being_truncated /* TRUNCATE sets stop_new_ops */
	      || space->n_pending_ops > 0);
}
#endif /* UNIV_DEBUG */

/** Write a MLOG_FILE_NAME record for a persistent tablespace.
@param[in]	space	tablespace
@param[in,out]	mtr	mini-transaction */
static
void
fil_names_write(
	const fil_space_t*	space,
	mtr_t*			mtr)
{
	ut_ad(UT_LIST_GET_LEN(space->chain) == 1);
	fil_name_write(space, 0, UT_LIST_GET_FIRST(space->chain), mtr);
}

/** Note that a non-predefined persistent tablespace has been modified
by redo log.
@param[in,out]	space	tablespace */
void
fil_names_dirty(
	fil_space_t*	space)
{
	ut_ad(log_mutex_own());
	ut_ad(recv_recovery_is_on());
	ut_ad(log_sys->lsn != 0);
	ut_ad(space->max_lsn == 0);
	ut_d(fil_space_validate_for_mtr_commit(space));

	UT_LIST_ADD_LAST(fil_system->named_spaces, space);
	space->max_lsn = log_sys->lsn;
}

/** Write MLOG_FILE_NAME records when a non-predefined persistent
tablespace was modified for the first time since the latest
fil_names_clear().
@param[in,out]	space	tablespace
@param[in,out]	mtr	mini-transaction */
void
fil_names_dirty_and_write(
	fil_space_t*	space,
	mtr_t*		mtr)
{
	ut_ad(log_mutex_own());
	ut_d(fil_space_validate_for_mtr_commit(space));
	ut_ad(space->max_lsn == log_sys->lsn);

	UT_LIST_ADD_LAST(fil_system->named_spaces, space);
	fil_names_write(space, mtr);

	DBUG_EXECUTE_IF("fil_names_write_bogus",
			{
				char bogus_name[] = "./test/bogus file.ibd";
				os_normalize_path(bogus_name);
				fil_name_write(
					SRV_LOG_SPACE_FIRST_ID, 0,
					bogus_name, mtr);
			});
}
#ifndef UNIV_HOTBACKUP
/** On a log checkpoint, reset fil_names_dirty_and_write() flags
and write out MLOG_FILE_NAME and MLOG_CHECKPOINT if needed.
@param[in]	lsn		checkpoint LSN
@param[in]	do_write	whether to always write MLOG_CHECKPOINT
@return whether anything was written to the redo log
@retval false	if no flags were set and nothing written
@retval true	if anything was written to the redo log */
bool
fil_names_clear(
	lsn_t	lsn,
	bool	do_write)
{
	mtr_t	mtr;
	ulint	mtr_checkpoint_size = LOG_CHECKPOINT_FREE_PER_THREAD;

	DBUG_EXECUTE_IF(
		"increase_mtr_checkpoint_size",
		mtr_checkpoint_size = 75 * 1024;
		);

	ut_ad(log_mutex_own());

	if (log_sys->append_on_checkpoint) {
		mtr_write_log(log_sys->append_on_checkpoint);
		do_write = true;
	}

	mtr.start();

	for (fil_space_t* space = UT_LIST_GET_FIRST(fil_system->named_spaces);
	     space != NULL; ) {
		fil_space_t*	next = UT_LIST_GET_NEXT(named_spaces, space);

		ut_ad(space->max_lsn > 0);
		if (space->max_lsn < lsn) {
			/* The tablespace was last dirtied before the
			checkpoint LSN. Remove it from the list, so
			that if the tablespace is not going to be
			modified any more, subsequent checkpoints will
			avoid calling fil_names_write() on it. */
			space->max_lsn = 0;
			UT_LIST_REMOVE(fil_system->named_spaces, space);
		}

		/* max_lsn is the last LSN where fil_names_dirty_and_write()
		was called. If we kept track of "min_lsn" (the first LSN
		where max_lsn turned nonzero), we could avoid the
		fil_names_write() call if min_lsn > lsn. */

		fil_names_write(space, &mtr);
		do_write = true;

		const mtr_buf_t* mtr_log = mtr_get_log(&mtr);

		/** If the mtr buffer size exceeds the size of
		LOG_CHECKPOINT_FREE_PER_THREAD then commit the multi record
		mini-transaction, start the new mini-transaction to
		avoid the parsing buffer overflow error during recovery. */

		if (mtr_log->size() > mtr_checkpoint_size) {
			ut_ad(mtr_log->size() < (RECV_PARSING_BUF_SIZE / 2));
			mtr.commit_checkpoint(lsn, false);
			mtr.start();
		}

		space = next;
	}

	if (do_write) {
		mtr.commit_checkpoint(lsn, true);
	} else {
		ut_ad(!mtr.has_modifications());
	}

	return(do_write);
}

/** Truncate a single-table tablespace. The tablespace must be cached
in the memory cache.
@param space_id			space id
@param dir_path			directory path
@param tablename		the table name in the usual
				databasename/tablename format of InnoDB
@param flags			tablespace flags
@param trunc_to_default		truncate to default size if tablespace
				is being newly re-initialized.
@return DB_SUCCESS or error */
dberr_t
truncate_t::truncate(
/*=================*/
	ulint		space_id,
	const char*	dir_path,
	const char*	tablename,
	ulint		flags,
	bool		trunc_to_default)
{
	dberr_t		err = DB_SUCCESS;
	char*		path;
	bool		has_data_dir = FSP_FLAGS_HAS_DATA_DIR(flags);

	ut_a(!is_system_tablespace(space_id));

	if (has_data_dir) {
		ut_ad(dir_path != NULL);

		path = fil_make_filepath(dir_path, tablename, IBD, true);

	} else {
		path = fil_make_filepath(NULL, tablename, IBD, false);
	}

	if (path == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(space_id);

	/* The following code must change when InnoDB supports
	multiple datafiles per tablespace. */
	ut_a(UT_LIST_GET_LEN(space->chain) == 1);

	fil_node_t*	node = UT_LIST_GET_FIRST(space->chain);

	if (trunc_to_default) {
		space->size = node->size = FIL_IBD_FILE_INITIAL_SIZE;
	}

	const bool already_open = node->is_open;

	if (!already_open) {

		bool	ret;

		node->handle = os_file_create_simple_no_error_handling(
			innodb_data_file_key, path, OS_FILE_OPEN,
			OS_FILE_READ_WRITE,
			fsp_is_system_temporary(space_id)
			? false : srv_read_only_mode, &ret);

		if (!ret) {
			ib::error() << "Failed to open tablespace file "
				<< path << ".";

			ut_free(path);

			return(DB_ERROR);
		}

		node->is_open = true;
	}

	os_offset_t	trunc_size = trunc_to_default
		? FIL_IBD_FILE_INITIAL_SIZE
		: space->size;

	const bool success = os_file_truncate(
		path, node->handle, trunc_size * UNIV_PAGE_SIZE);

	if (!success) {
		ib::error() << "Cannot truncate file " << path
			<< " in TRUNCATE TABLESPACE.";
		err = DB_ERROR;
	}

	space->stop_new_ops = false;
	space->is_being_truncated = false;

	/* If we opened the file in this function, close it. */
	if (!already_open) {
		bool	closed = os_file_close(node->handle);

		if (!closed) {

			ib::error() << "Failed to close tablespace file "
				<< path << ".";

			err = DB_ERROR;
		} else {
			node->is_open = false;
		}
	}

	mutex_exit(&fil_system->mutex);

	ut_free(path);

	return(err);
}
#endif /* !UNIV_HOTBACKUP */

/**
Note that the file system where the file resides doesn't support PUNCH HOLE.
Called from AIO handlers when IO returns DB_IO_NO_PUNCH_HOLE
@param[in,out]	node		Node to set */
void
fil_no_punch_hole(fil_node_t* node)
{
	node->punch_hole = false;
}

/** Set the compression type for the tablespace of a table
@param[in]	table		The table that should be compressed
@param[in]	algorithm	Text representation of the algorithm
@return DB_SUCCESS or error code */
dberr_t
fil_set_compression(
	dict_table_t*	table,
	const char*	algorithm)
{
	ut_ad(table != NULL);

	/* We don't support Page Compression for the system tablespace,
	the temporary tablespace, or any general tablespace because
	COMPRESSION is set by TABLE DDL, not TABLESPACE DDL. There is
	no other technical reason.  Also, do not use it for missing
	tables or tables with compressed row_format. */
	if (table->ibd_file_missing
	    || !DICT_TF2_FLAG_IS_SET(table, DICT_TF2_USE_FILE_PER_TABLE)
	    || DICT_TF2_FLAG_IS_SET(table, DICT_TF2_TEMPORARY)
	    || page_size_t(table->flags).is_compressed()) {

		return(DB_IO_NO_PUNCH_HOLE_TABLESPACE);
	}

	dberr_t		err;
	Compression	compression;

	if (algorithm == NULL || strlen(algorithm) == 0) {

#ifndef UNIV_DEBUG
		compression.m_type = Compression::NONE;
#else
		/* This is a Debug tool for setting compression on all
		compressible tables not otherwise specified. */
		switch (srv_debug_compress) {
		case Compression::LZ4:
		case Compression::ZLIB:
		case Compression::NONE:

			compression.m_type =
				static_cast<Compression::Type>(
					srv_debug_compress);
			break;

		default:
			compression.m_type = Compression::NONE;
		}

#endif /* UNIV_DEBUG */

		err = DB_SUCCESS;

	} else {

		err = Compression::check(algorithm, &compression);
	}

	fil_space_t*	space = fil_space_get(table->space);

	if (space == NULL) {
		return(DB_NOT_FOUND);
	}

	space->compression_type = compression.m_type;

	if (space->compression_type != Compression::NONE) {

		const fil_node_t* node;

		node = UT_LIST_GET_FIRST(space->chain);

		if (!node->punch_hole) {

			return(DB_IO_NO_PUNCH_HOLE_FS);
		}
	}

	return(err);
}

/** Get the compression algorithm for a tablespace.
@param[in]	space_id	Space ID to check
@return the compression algorithm */
Compression::Type
fil_get_compression(
	ulint	space_id)
{
	fil_space_t*	space = fil_space_get(space_id);

	return(space == NULL ? Compression::NONE : space->compression_type);
}

/** Set the encryption type for the tablespace
@param[in] space_id		Space ID of tablespace for which to set
@param[in] algorithm		Encryption algorithm
@param[in] key			Encryption key
@param[in] iv			Encryption iv
@return DB_SUCCESS or error code */
dberr_t
fil_set_encryption(
	ulint			space_id,
	Encryption::Type	algorithm,
	byte*			key,
	byte*			iv)
{
	ut_ad(!is_system_or_undo_tablespace(space_id));

	if (is_system_tablespace(space_id)) {
		return(DB_IO_NO_ENCRYPT_TABLESPACE);
	}

	mutex_enter(&fil_system->mutex);

	fil_space_t*	space = fil_space_get_by_id(space_id);

	if (space == NULL) {
		mutex_exit(&fil_system->mutex);
		return(DB_NOT_FOUND);
	}

	ut_ad(algorithm != Encryption::NONE);
	space->encryption_type = algorithm;
	if (key == NULL) {
		Encryption::random_value(space->encryption_key);
	} else {
		memcpy(space->encryption_key,
		       key, ENCRYPTION_KEY_LEN);
	}

	space->encryption_klen = ENCRYPTION_KEY_LEN;
	if (iv == NULL) {
		Encryption::random_value(space->encryption_iv);
	} else {
		memcpy(space->encryption_iv,
		       iv, ENCRYPTION_KEY_LEN);
	}

	mutex_exit(&fil_system->mutex);

	return(DB_SUCCESS);
}

/** Rotate the tablespace keys by new master key.
@return true if the re-encrypt suceeds */
bool
fil_encryption_rotate()
{
	fil_space_t*	space;
	mtr_t		mtr;
	byte		encrypt_info[ENCRYPTION_INFO_SIZE_V2];

	for (space = UT_LIST_GET_FIRST(fil_system->space_list);
	     space != NULL; ) {
		/* Skip unencypted tablespaces. */
		if (is_system_or_undo_tablespace(space->id)
		    || fsp_is_system_temporary(space->id)
		    || space->purpose == FIL_TYPE_LOG) {
			space = UT_LIST_GET_NEXT(space_list, space);
			continue;
		}

		if (space->encryption_type != Encryption::NONE) {
			mtr_start(&mtr);
			mtr.set_named_space(space->id);

			space = mtr_x_lock_space(space->id, &mtr);

			memset(encrypt_info, 0, ENCRYPTION_INFO_SIZE_V2);

			if (!fsp_header_rotate_encryption(space,
							  encrypt_info,
							  &mtr)) {
				mtr_commit(&mtr);
				return(false);
			}

			mtr_commit(&mtr);
		}

		space = UT_LIST_GET_NEXT(space_list, space);
		DBUG_EXECUTE_IF("ib_crash_during_rotation_for_encryption",
				DBUG_SUICIDE(););
	}

	return(true);
}

/** Build the basic folder name from the path and length provided
@param[in]	path	pathname (may also include the file basename)
@param[in]	len	length of the path, in bytes */
void
Folder::make_path(const char* path, size_t len)
{
	if (is_absolute_path(path)) {
		m_folder = mem_strdupl(path, len);
		m_folder_len = len;
	}
	else {
		size_t n = 2 + len + strlen(fil_path_to_mysql_datadir);
		m_folder = static_cast<char*>(ut_malloc_nokey(n));
		m_folder_len = 0;

		if (path != fil_path_to_mysql_datadir) {
			/* Put the mysqld datadir into m_folder first. */
			ut_ad(fil_path_to_mysql_datadir[0] != '\0');
			m_folder_len = strlen(fil_path_to_mysql_datadir);
			memcpy(m_folder, fil_path_to_mysql_datadir,
			       m_folder_len);
			if (m_folder[m_folder_len - 1] != OS_PATH_SEPARATOR) {
				m_folder[m_folder_len++] = OS_PATH_SEPARATOR;
			}
		}

		/* Append the path. */
		memcpy(m_folder + m_folder_len, path, len);
		m_folder_len += len;
		m_folder[m_folder_len] = '\0';
	}

	os_normalize_path(m_folder);
}

/** Resolve a relative path in m_folder to an absolute path
in m_abs_path setting m_abs_len. */
void
Folder::make_abs_path()
{
	my_realpath(m_abs_path, m_folder, MYF(0));
	m_abs_len = strlen(m_abs_path);

	ut_ad(m_abs_len + 1 < sizeof(m_abs_path));

	/* Folder::related_to() needs a trailing separator. */
	if (m_abs_path[m_abs_len - 1] != OS_PATH_SEPARATOR) {
		m_abs_path[m_abs_len] = OS_PATH_SEPARATOR;
		m_abs_path[++m_abs_len] = '\0';
	}
}

/** Constructor
@param[in]	path	pathname (may also include the file basename)
@param[in]	len	length of the path, in bytes */
Folder::Folder(const char* path, size_t len)
{
	make_path(path, len);
	make_abs_path();
}

/** Assignment operator
@param[in]	folder	folder string provided */
class Folder&
Folder::operator=(const char* path)
{
	ut_free(m_folder);
	make_path(path, strlen(path));
	make_abs_path();

	return(*this);
}

/** Determine if two folders are equal
@param[in]	other	folder to compare to
@return whether the folders are equal */
bool Folder::operator==(const Folder& other) const
{
	return(m_abs_len == other.m_abs_len
	       && !memcmp(m_abs_path, other.m_abs_path, m_abs_len));
}

/** Determine if the left folder is the same or an ancestor of
(contains) the right folder.
@param[in]	other	folder to compare to
@return whether this is the same or an ancestor of the other folder. */
bool Folder::operator>=(const Folder& other) const
{
	return(m_abs_len <= other.m_abs_len
		&& (!memcmp(other.m_abs_path, m_abs_path, m_abs_len)));
}

/** Determine if the left folder is an ancestor of (contains)
the right folder.
@param[in]	other	folder to compare to
@return whether this is an ancestor of the other folder */
bool Folder::operator>(const Folder& other) const
{
	return(m_abs_len < other.m_abs_len
	       && (!memcmp(other.m_abs_path, m_abs_path, m_abs_len)));
}

/** Determine if the directory referenced by m_folder exists.
@return whether the directory exists */
bool
Folder::exists()
{
	bool		exists;
	os_file_type_t	type;

#ifdef _WIN32
	/* Temporarily strip the trailing_separator since it will cause
	_stat64() to fail on Windows unless the path is the root of some
	drive; like "c:\".  _stat64() will fail if it is "c:". */
	size_t	len = strlen(m_abs_path);
	if (m_abs_path[m_abs_len - 1] == OS_PATH_SEPARATOR
	    && m_abs_path[m_abs_len - 2] != ':') {
		m_abs_path[m_abs_len - 1] = '\0';
	}
#endif /* WIN32 */

	bool ret = os_file_status(m_abs_path, &exists, &type);

#ifdef _WIN32
	/* Put the separator back on. */
	if (m_abs_path[m_abs_len - 1] == '\0') {
		m_abs_path[m_abs_len - 1] = OS_PATH_SEPARATOR;
	}
#endif /* WIN32 */

	return(ret && exists && type == OS_FILE_TYPE_DIR);
}

/* Unit Tests */
#ifdef UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH
#define MF  fil_make_filepath
#define DISPLAY ib::info() << path
void
test_make_filepath()
{
	char* path;
	const char* long_path =
		"this/is/a/very/long/path/including/a/very/"
		"looooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooooo"
		"oooooooooooooooooooooooooooooooooooooooooooooooong"
		"/folder/name";
	path = MF("/this/is/a/path/with/a/filename", NULL, IBD, false); DISPLAY;
	path = MF("/this/is/a/path/with/a/filename", NULL, ISL, false); DISPLAY;
	path = MF("/this/is/a/path/with/a/filename", NULL, CFG, false); DISPLAY;
	path = MF("/this/is/a/path/with/a/filename", NULL, CFP, false); DISPLAY;
	path = MF("/this/is/a/path/with/a/filename.ibd", NULL, IBD, false); DISPLAY;
	path = MF("/this/is/a/path/with/a/filename.ibd", NULL, IBD, false); DISPLAY;
	path = MF("/this/is/a/path/with/a/filename.dat", NULL, IBD, false); DISPLAY;
	path = MF(NULL, "tablespacename", NO_EXT, false); DISPLAY;
	path = MF(NULL, "tablespacename", IBD, false); DISPLAY;
	path = MF(NULL, "dbname/tablespacename", NO_EXT, false); DISPLAY;
	path = MF(NULL, "dbname/tablespacename", IBD, false); DISPLAY;
	path = MF(NULL, "dbname/tablespacename", ISL, false); DISPLAY;
	path = MF(NULL, "dbname/tablespacename", CFG, false); DISPLAY;
	path = MF(NULL, "dbname/tablespacename", CFP, false); DISPLAY;
	path = MF(NULL, "dbname\\tablespacename", NO_EXT, false); DISPLAY;
	path = MF(NULL, "dbname\\tablespacename", IBD, false); DISPLAY;
	path = MF("/this/is/a/path", "dbname/tablespacename", IBD, false); DISPLAY;
	path = MF("/this/is/a/path", "dbname/tablespacename", IBD, true); DISPLAY;
	path = MF("./this/is/a/path", "dbname/tablespacename.ibd", IBD, true); DISPLAY;
	path = MF("this\\is\\a\\path", "dbname/tablespacename", IBD, true); DISPLAY;
	path = MF("/this/is/a/path", "dbname\\tablespacename", IBD, true); DISPLAY;
	path = MF(long_path, NULL, IBD, false); DISPLAY;
	path = MF(long_path, "tablespacename", IBD, false); DISPLAY;
	path = MF(long_path, "tablespacename", IBD, true); DISPLAY;
}
#endif /* UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH */
/* @} */

/** Release the reserved free extents.
@param[in]	n_reserved	number of reserved extents */
void
fil_space_t::release_free_extents(ulint	n_reserved)
{
	ut_ad(rw_lock_own(&latch, RW_LOCK_X));

	ut_a(n_reserved_extents >= n_reserved);
	n_reserved_extents -= n_reserved;
}
