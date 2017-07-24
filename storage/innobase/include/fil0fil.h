/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/fil0fil.h
The low-level file system

Created 10/25/1995 Heikki Tuuri
*******************************************************/

#ifndef fil0fil_h
#define fil0fil_h

#include "univ.i"

#include "log0recv.h"
#include "dict0types.h"
#include "page0size.h"
#include "fil0types.h"
#ifndef UNIV_HOTBACKUP
#include "ibuf0types.h"
#endif /* !UNIV_HOTBACKUP */

#include <list>
#include <vector>

extern const char general_space_name[];
extern volatile bool	recv_recovery_on;

// Forward declaration
struct trx_t;
class page_id_t;

using Filenames = std::vector<std::string, ut_allocator<std::string>>;
using Space_ids = std::vector<space_id_t, ut_allocator<space_id_t>>;

typedef std::list<char*, ut_allocator<char*> >	space_name_list_t;

/** File types */
enum fil_type_t : uint8_t {
	/** temporary tablespace (temporary undo log or tables) */
	FIL_TYPE_TEMPORARY = 1,
	/** a tablespace that is being imported (no logging until finished) */
	FIL_TYPE_IMPORT = 2,
	/** persistent tablespace (for system, undo log or tables) */
	FIL_TYPE_TABLESPACE = 4,
	/** redo log covering changes to files of FIL_TYPE_TABLESPACE */
	FIL_TYPE_LOG = 8
};

/** Result of comparing a path. */
enum class Fil_path {
	/** The path matches what was found during the scan. */
	MATCHES,

	/** No MLOG_FILE_DELETE record and the file could not be found. */
	MISSING,

	/** A MLOG_FILE_DELETE was found, file was deleted. */
	DELETED,

	/** Space ID matches but the paths don't match. */
	MOVED
};

/** Check if fil_type is any of FIL_TYPE_TEMPORARY, FIL_TYPE_IMPORT
or FIL_TYPE_TABLESPACE.
@param[in]	type	variable of type fil_type_t
@return true if any of FIL_TYPE_TEMPORARY, FIL_TYPE_IMPORT
or FIL_TYPE_TABLESPACE */
inline
bool
fil_type_is_data(fil_type_t type)
{
	return(type == FIL_TYPE_TEMPORARY
	       || type == FIL_TYPE_IMPORT
	       || type == FIL_TYPE_TABLESPACE);
}

struct fil_space_t;

/** File node of a tablespace or the log data space */
struct fil_node_t {

	using List_node = UT_LIST_NODE_T(fil_node_t);

	/** tablespace containing this file */
	fil_space_t*		space;

	/** file name; protected by Fil_shard::m_mutex and log_sys->mutex. */
	char*			name;

	/** whether this file is open. Note: We set the is_open flag after
	we increase the write the MLOG_FILE_OPEN record to redo log. Therefore
	we increment the in_use reference count before setting the OPEN flag. */
	bool			is_open;

	/** file handle (valid if is_open) */
	pfs_os_file_t		handle;

	/** event that groups and serializes calls to fsync */
	os_event_t		sync_event;

	/** whether the file actually is a raw device or disk partition */
	bool			is_raw_disk;

	/** size of the file in database pages (0 if not known yet);
	the possible last incomplete megabyte may be ignored
	if space->id == 0 */
	page_no_t		size;

	/** initial size of the file in database pages;
	FIL_IBD_FILE_INITIAL_SIZE by default */
	page_no_t		init_size;

	/** maximum size of the file in database pages */
	page_no_t		max_size;

	/** count of pending i/o's; is_open must be true if nonzero */
	size_t			n_pending;

	/** count of pending flushes; is_open must be true if nonzero */
	size_t			n_pending_flushes;

	/** e.g., when a file is being extended or just opened. */
	size_t			in_use;

	/** number of writes to the file since the system was started */
	int64_t			modification_counter;

	/** the modification_counter of the latest flush to disk */
	int64_t			flush_counter;

	/** link to the fil_system->LRU list (keeping track of open files) */
	List_node		LRU;

	/** whether the file system of this file supports PUNCH HOLE */
	bool			punch_hole;

	/** block size to use for punching holes */
	size_t			block_size;

	/** whether atomic write is enabled for this file */
	bool			atomic_write;

	/** FIL_NODE_MAGIC_N */
	size_t			magic_n;
};

/** Tablespace or log data space */
struct fil_space_t {

	using List_node = UT_LIST_NODE_T(fil_space_t);
	using Files = std::vector<fil_node_t, ut_allocator<fil_node_t>>;

	/** Tablespace name */
	char*			name;

	/** Tablespace ID */
	space_id_t		id;

	/** LSN of the most recent fil_names_write_if_was_clean().
	Reset to 0 by fil_names_clear().  Protected by log_sys->mutex.
	If and only if this is nonzero, the tablespace will be in
	named_spaces. */
	lsn_t			max_lsn;

	/** True if we want to rename the .ibd file of tablespace and
	want to stop temporarily posting of new i/o requests on the file */
	bool			stop_ios;

	/** We set this true when we start deleting a single-table
	tablespace.  When this is set following new ops are not allowed:
	* read IO request
	* ibuf merge
	* file flush
	Note that we can still possibly have new write operations because we
	don't check this flag when doing flush batches. */
	bool			stop_new_ops;

#ifdef UNIV_DEBUG
	/** Reference count for operations who want to skip redo log in
	the file space in order to make fsp_space_modify_check pass. */
	ulint			redo_skipped_count;
#endif /* UNIV_DEBUG */

	/** Purpose */
	fil_type_t		purpose;

	/** Files attached to this tablespace. Note: Only the system tablespace
	can have multiple files, this is a legacy issue. */
	Files			files;

	/** Tablespace file size in pages; 0 if not known yet */
	page_no_t		size;

	/** FSP_SIZE in the tablespace header; 0 if not known yet */
	page_no_t		size_in_header;

	/** Length of the FSP_FREE list */
	uint32_t		free_len;

	/** Contents of FSP_FREE_LIMIT */
	page_no_t		free_limit;

	/** Tablespace flags; see fsp_flags_is_valid() and
	page_size_t(ulint) (constructor).
	This is protected by space->latch and tablespace MDL */
	uint32_t		flags;

	/** Number of reserved free extents for ongoing operations like
	B-tree page split */
	uint32_t		n_reserved_extents;

	/** This is positive when flushing the tablespace to disk;
	dropping of the tablespace is forbidden if this is positive */
	uint32_t		n_pending_flushes;

	/** This is positive when we have pending operations against this
	tablespace. The pending operations can be ibuf merges or lock
	validation code trying to read a block.  Dropping of the tablespace
	is forbidden if this is positive.  Protected by Fil_shard::m_mutex. */
	uint32_t		n_pending_ops;

#ifndef UNIV_HOTBACKUP
	/** Latch protecting the file space storage allocation */
	rw_lock_t		latch;
#endif /* !UNIV_HOTBACKUP */

	/** List of spaces with at least one unflushed file we have
	written to */
	List_node		unflushed_spaces;

	/** true if this space is currently in unflushed_spaces */
	bool			is_in_unflushed_spaces;

	/** Compression algorithm */
	Compression::Type	compression_type;

	/** Encryption algorithm */
	Encryption::Type	encryption_type;

	/** Encrypt key */
	byte			encryption_key[ENCRYPTION_KEY_LEN];

	/** Encrypt key length*/
	ulint			encryption_klen;

	/** Encrypt initial vector */
	byte			encryption_iv[ENCRYPTION_KEY_LEN];

	/** Release the reserved free extents.
	@param[in]	n_reserved	number of reserved extents */
	void release_free_extents(ulint n_reserved);

	/** FIL_SPACE_MAGIC_N */
	ulint			magic_n;

	/** System tablespace */
	static fil_space_t*	s_sys_space;

#ifdef UNIV_DEBUG
	/** Print the extent descriptor pages of this tablespace into
	the given output stream.
	@param[in]	out	the output stream.
	@return	the output stream. */
	std::ostream& print_xdes_pages(std::ostream& out) const;

	/** Print the extent descriptor pages of this tablespace into
	the given file.
	@param[in]	filename	the output file name. */
	void print_xdes_pages(const char* filename) const;
#endif /* UNIV_DEBUG */
};

/** Value of fil_space_t::magic_n */
constexpr size_t FIL_SPACE_MAGIC_N = 89472;

/** Value of fil_node_t::magic_n */
constexpr size_t FIL_NODE_MAGIC_N = 89389;

/** Common InnoDB file extentions */
enum ib_extention {
	NO_EXT = 0,
	IBD = 1,
	CFG = 2,
	CFP = 3
};

extern const char* dot_ext[];

#define DOT_IBD dot_ext[IBD]
#define DOT_CFG dot_ext[CFG]
#define DOT_CFP dot_ext[CFP]

#ifdef _WIN32
/* Initialization of m_abs_path() produces warning C4351:
"new behavior: elements of array '...' will be default initialized."
See https://msdn.microsoft.com/en-us/library/1ywe7hcy.aspx */
#pragma warning(disable:4351)
#endif /* _WIN32 */

/** Wrapper for a path to a directory that may or may not exist. */
class Folder {
public:
	/** Default constructor */
	Folder() : m_folder(), m_folder_len(), m_abs_path(), m_abs_len()
	{}

	/** Constructor
	@param[in]	path	pathname (not necessarily NUL-terminated)
	@param[in]	len	length of the path, in bytes */
	Folder(const char* path, size_t len);

	/** Assignment operator
	@param[in]	path	folder string provided */
	Folder& operator=(const char* path);

	/** Destructor */
	~Folder()
	{
		ut_free(m_folder);
	}

	/** Implicit type conversion
	@return the wrapped object */
	operator const char*() const
	{
		return(m_folder);
	}

	/** Explicit type conversion
	@return the wrapped object */
	const char* operator()() const
	{
		return(m_folder);
	}

	/** return the length of m_folder
	@return the length of m_folder */
	size_t len() const
	{
		return(m_folder_len);
	}

	/** Determine if this folder is equal to the other folder.
	@param[in]	other	folder to compare to
	@return whether the folders are equal */
	bool operator==(const Folder& other) const
	{
		return(m_abs_len == other.m_abs_len
		       && !memcmp(m_abs_path, other.m_abs_path, m_abs_len));
	}

	/** Determine if this folder is an ancestor of (contains)
	the other folder.
	@param[in]	other	folder to compare to
	@return whether this is an ancestor of the other folder */
	bool operator>(const Folder& other) const
	{
		return(m_abs_len < other.m_abs_len
		       && (!memcmp(other.m_abs_path, m_abs_path, m_abs_len)));
	}

	/** Determine if the directory referenced by m_folder exists.
	@return whether the directory exists */
	bool exists();

	/** Return the absolute path */
	std::string abs_path() const
	{
		return(std::string(m_abs_path, m_abs_len));
	}

private:
	/** Build the basic folder name from the path and length provided
	@param[in]	path	pathname (not necessarily NUL-terminated)
	@param[in]	len	length of the path, in bytes */
	inline void make_path(const char* path, size_t len);

	/** Resolve a relative path in m_folder to an absolute path
	in m_abs_path setting m_abs_len. */
	void make_abs_path();

	/** The wrapped folder string */
	char*	m_folder;

	/** Length of m_folder */
	size_t	m_folder_len;

	/** A full absolute path to the same file. */
	char	m_abs_path[FN_REFLEN + 2];

	/** Length of m_abs_path to the deepest folder */
	size_t	m_abs_len;
};

/** When mysqld is run, the default directory "." is the mysqld datadir,
but in the MySQL Embedded Server Library and mysqlbackup it is not the default
directory, and we must set the base file path explicitly */
extern const char*	fil_path_to_mysql_datadir;
extern Folder		folder_mysql_datadir;

/** Initial size of a single-table tablespace in pages */
constexpr size_t	FIL_IBD_FILE_INITIAL_SIZE = 6;

/** 'null' (undefined) page offset in the context of file spaces */
constexpr page_no_t	FIL_NULL = std::numeric_limits<page_no_t>::max();

/** Maximum Page Number, one less than FIL_NULL */
constexpr page_no_t	PAGE_NO_MAX = std::numeric_limits<page_no_t>::max() - 1;

/** Unknown space id */
constexpr space_id_t	SPACE_UNKNOWN = std::numeric_limits<space_id_t>::max();

/* Space address data type; this is intended to be used when
addresses accurate to a byte are stored in file pages. If the page part
of the address is FIL_NULL, the address is considered undefined. */

/** 'type' definition in C: an address stored in a file page is a
string of bytes */
using fil_faddr_t = byte;

/** File space address */
struct fil_addr_t {

	/** Page number within a space */
	page_no_t	page;

	/** Byte offset within the page */
	ulint		boffset;

	/** Print a string representation.
	@param[in,out]	out		Stream to write to */
	std::ostream& print(std::ostream& out) const
	{
		out
			<< "[fil_addr_t: page=" << page << ", boffset="
			<< boffset << "]";

		return(out);
	}
};

/* For printing fil_addr_t to a stream.
@param[in,out]	out		Stream to write to
@param[in]	obj		fil_addr_t instance to write */
inline
std::ostream&
operator<<(std::ostream& out, const fil_addr_t&	obj)
{
	return(obj.print(out));
}

/** The null file address */
extern fil_addr_t	fil_addr_null;

using page_type_t = uint16_t;

/** File page types (values of FIL_PAGE_TYPE) @{ */
/** B-tree node */
constexpr page_type_t FIL_PAGE_INDEX = 17855;

/** R-tree node */
constexpr page_type_t FIL_PAGE_RTREE = 17854;

/** Tablespace SDI Index page */
constexpr page_type_t FIL_PAGE_SDI = 17853;

/** Undo log page */
constexpr page_type_t FIL_PAGE_UNDO_LOG = 2;

/** Index node */
constexpr page_type_t FIL_PAGE_INODE = 3;

/** Insert buffer free list */
constexpr page_type_t FIL_PAGE_IBUF_FREE_LIST = 4;

/* File page types introduced in MySQL/InnoDB 5.1.7 */
/** Freshly allocated page */
constexpr page_type_t FIL_PAGE_TYPE_ALLOCATED = 0;

/** Insert buffer bitmap */
constexpr page_type_t FIL_PAGE_IBUF_BITMAP = 5;

/** System page */
constexpr page_type_t FIL_PAGE_TYPE_SYS = 6;

/** Transaction system data */
constexpr page_type_t FIL_PAGE_TYPE_TRX_SYS = 7;

/** File space header */
constexpr page_type_t FIL_PAGE_TYPE_FSP_HDR = 8;

/** Extent descriptor page */
constexpr page_type_t FIL_PAGE_TYPE_XDES = 9;

/** Uncompressed BLOB page */
constexpr page_type_t FIL_PAGE_TYPE_BLOB = 10;

/** First compressed BLOB page */
constexpr page_type_t FIL_PAGE_TYPE_ZBLOB = 11;

/** Subsequent compressed BLOB page */
constexpr page_type_t FIL_PAGE_TYPE_ZBLOB2 = 12;

/** In old tablespaces, garbage in FIL_PAGE_TYPE is replaced with
this value when flushing pages. */
constexpr page_type_t FIL_PAGE_TYPE_UNKNOWN = 13;

/** Compressed page */
constexpr page_type_t FIL_PAGE_COMPRESSED = 14;

/** Encrypted page */
constexpr page_type_t FIL_PAGE_ENCRYPTED = 15;

/** Compressed and Encrypted page */
constexpr page_type_t FIL_PAGE_COMPRESSED_AND_ENCRYPTED = 16;

/** Encrypted R-tree page */
constexpr page_type_t FIL_PAGE_ENCRYPTED_RTREE = 17;

/** Uncompressed SDI BLOB page */
constexpr page_type_t FIL_PAGE_SDI_BLOB = 18;

/** Commpressed SDI BLOB page */
constexpr page_type_t FIL_PAGE_SDI_ZBLOB = 19;

/** Independently compressed LOB page*/
constexpr page_type_t FIL_PAGE_TYPE_ZBLOB3 = 20;

/** Rollback Segment Array page */
constexpr page_type_t FIL_PAGE_TYPE_RSEG_ARRAY = 21;

/** Used by i_s.cc to index into the text description. */
constexpr page_type_t FIL_PAGE_TYPE_LAST = FIL_PAGE_TYPE_RSEG_ARRAY;
/* @} */

/** Check whether the page type is index (Btree or Rtree or SDI) type */
#define fil_page_type_is_index(page_type)                          \
	(page_type == FIL_PAGE_INDEX || page_type == FIL_PAGE_SDI  \
	 || page_type == FIL_PAGE_RTREE)

/** Check whether the page is index page (either regular Btree index or Rtree
index */
#define fil_page_index_page_check(page)                         \
        fil_page_type_is_index(fil_page_get_type(page))

/** The number of fsyncs done to the log */
extern ulint	fil_n_log_flushes;

/** Number of pending redo log flushes */
extern ulint	fil_n_pending_log_flushes;
/** Number of pending tablespace flushes */
extern ulint	fil_n_pending_tablespace_flushes;

/** Number of files currently open */
extern ulint	fil_n_file_opened;

#ifndef UNIV_HOTBACKUP
/** Look up a tablespace.
The caller should hold an InnoDB table lock or a MDL that prevents
the tablespace from being dropped during the operation,
or the caller should be in single-threaded crash recovery mode
(no user connections that could drop tablespaces).
If this is not the case, fil_space_acquire() and fil_space_release()
should be used instead.
@param[in]	space_id	Tablespace ID
@return tablespace, or nullptr if not found */
fil_space_t*
fil_space_get(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Returns the latch of a file space.
@param[in]	space_id	Tablespace ID
@param[out]	flags	tablespace flags
@return latch protecting storage allocation */
rw_lock_t*
fil_space_get_latch(space_id_t id, ulint* flags)
	MY_ATTRIBUTE((warn_unused_result));

#ifdef UNIV_DEBUG
/** Gets the type of a file space.
@param[in]	space_id	Tablespace ID
@return file type */
fil_type_t
fil_space_get_type(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */

/** Note that a tablespace has been imported.
It is initially marked as FIL_TYPE_IMPORT so that no logging is
done during the import process when the space ID is stamped to each page.
Now we change it to FIL_SPACE_TABLESPACE to start redo and undo logging.
NOTE: temporary tablespaces are never imported.
@param[in]	space_id	Tablespace ID */
void
fil_space_set_imported(space_id_t space_id);
#endif /* !UNIV_HOTBACKUP */

/** Append a file to the chain of files of a space.
@param[in]	name		file name of a file that is not open
@param[in]	size		file size in entire database blocks
@param[in,out]	space		tablespace from fil_space_create()
@param[in]	is_raw		whether this is a raw device or partition
@param[in]	atomic_write	true if atomic write enabled
@param[in]	max_pages	maximum number of pages in file
@return pointer to the file name
@retval nullptr if error */
char*
fil_node_create(
	const char*	name,
	page_no_t	size,
	fil_space_t*	space,
	bool		is_raw,
	bool		atomic_write,
	page_no_t	max_pages = PAGE_NO_MAX)
	MY_ATTRIBUTE((warn_unused_result));

/** Create a space memory object and put it to the fil_system hash table.
The tablespace name is independent from the tablespace file-name.
Error messages are issued to the server log.
@param[in]	name		Tablespace name
@param[in]	space_id	Tablespace ID
@param[in]	flags		tablespace flags
@param[in]	purpose		tablespace purpose
@return pointer to created tablespace, to be filled in with fil_node_create()
@retval nullptr on failure (such as when the same tablespace exists) */
fil_space_t*
fil_space_create(
	const char*	name,
	space_id_t	space_id,
	ulint		flags,
	fil_type_t	purpose)
	MY_ATTRIBUTE((warn_unused_result));

/** Assigns a new space id for a new single-table tablespace.
This works simply by incrementing the global counter. If 4 billion id's
is not enough, we may need to recycle id's.
@param[in,out]	space_id		New space ID
@return true if assigned, false if not */
bool
fil_assign_new_space_id(space_id_t* space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Returns the path from the first fil_node_t found with this space ID.
The caller is responsible for freeing the memory allocated here for the
value returned.
@param[in]	space_id	Tablespace ID
@return own: A copy of fil_node_t::path, nullptr if space ID is zero
	or not found. */
char*
fil_space_get_first_path(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Returns the size of the space in pages. The tablespace must be cached
in the memory cache.
@param[in]	space_id	Tablespace ID
@return space size, 0 if space not found */
page_no_t
fil_space_get_size(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Returns the flags of the space. The tablespace must be cached
in the memory cache.
@param[in]	space_id	Tablespace ID for which to get the flags
@return flags, ULINT_UNDEFINED if space not found */
ulint
fil_space_get_flags(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Sets the flags of the tablespace. The tablespace must be locked
in MDL_EXCLUSIVE MODE.
@param[in]	space		tablespace in-memory struct
@param[in]	flags		tablespace flags */
void
fil_space_set_flags(
	fil_space_t*	space,
	ulint		flags);

/** Open each file of a tablespace if not already open.
@param[in]	space_id	Tablespace ID
@retval	true	if all file nodes were opened
@retval	false	on failure */
bool
fil_space_open(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Close each file of a tablespace if open.
@param[in]	space_id	Tablespace ID */
void
fil_space_close(space_id_t space_id);

/** Returns the page size of the space and whether it is compressed or not.
The tablespace must be cached in the memory cache.
@param[in]	space_id	Tablespace ID
@param[out]	found		true if tablespace was found
@return page size */
const page_size_t
fil_space_get_page_size(
	space_id_t	space_id,
	bool*		found)
	MY_ATTRIBUTE((warn_unused_result));

/** Initializes the tablespace memory cache.
@param[in]	max_n_open	Max number of open files. */
void
fil_init(ulint max_n_open);

/** Initializes the tablespace memory cache. */
void
fil_close();

/** Opens all log files and system tablespace data files.
They stay open until the database server shutdown. This should be called
at a server startup after the space objects for the log and the system
tablespace have been created. The purpose of this operation is to make
sure we never run out of file descriptors if we need to read from the
insert buffer or to write to the log. */
void
fil_open_log_and_system_tablespace_files();

/** Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
void
fil_close_all_files();

/** Closes the redo log files. There must not be any pending i/o's or not
flushed modifications in the files.
@param[in]	free_all	Whether to free the instances. */
void
fil_close_log_files(bool free_all);

/** File node iterator callback. */
using fil_node_cbk_t = dberr_t (fil_node_t* file, void* context);

/** Iterate through all persistent tablespace files (FIL_TYPE_TABLESPACE)
returning the nodes via callback function cbk.
@param[in]	include_log	include log files
@param[in]	context		callback function context
@param[in]	callback	callback function
@return any error returned by the callback function. */
dberr_t
fil_iterate_tablespace_files(
	bool		include_log,
	void*		context,
	fil_node_cbk_t*	callback)
	MY_ATTRIBUTE((warn_unused_result));

/** Sets the max tablespace id counter if the given number is bigger than the
previous value.
@param[in]	max_id		Maximum known tablespace ID */
void
fil_set_max_space_id_if_bigger(space_id_t max_id);

#ifndef UNIV_HOTBACKUP
/** Write the flushed LSN to the page header of the first page in the
system tablespace.
@param[in]	lsn		Flushed LSN
@return DB_SUCCESS or error number */
dberr_t
fil_write_flushed_lsn(lsn_t lsn)
	MY_ATTRIBUTE((warn_unused_result));

/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	space_id	Tablespace ID
@return the tablespace, or nullptr if missing or being deleted */
fil_space_t*
fil_space_acquire(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Acquire a tablespace that may not exist.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	space_id	Tablespace ID
@return the tablespace, or nullptr if missing or being deleted */
fil_space_t*
fil_space_acquire_silent(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Release a tablespace acquired with fil_space_acquire().
@param[in,out]	space		Tablespace to release  */
void
fil_space_release(fil_space_t* space);

#endif /* !UNIV_HOTBACKUP */

/** Deletes an IBD tablespace, either general or single-table.
The tablespace must be cached in the memory cache. This will delete the
datafile, fil_space_t & fil_node_t entries from the file_system_t cache.
@param[in]	space_id	Tablespace ID
@param[in]	buf_remove	Specify the action to take on the pages
for this table in the buffer pool.
@return DB_SUCCESS, DB_TABLESPCE_NOT_FOUND or DB_IO_ERROR */
dberr_t
fil_delete_tablespace(
	space_id_t	space_id,
	buf_remove_t	buf_remove)
	MY_ATTRIBUTE((warn_unused_result));

#ifndef UNIV_HOTBACKUP
/* Convert the paths into absolute paths and compare them.
@param[in]	lhs		Filename to compare
@param[in]	rhs		Filename to compare
@return true if they are the same */
bool
fil_paths_equal(const char* lhs, const char* rhs)
	MY_ATTRIBUTE((warn_unused_result));

/** Fetch the file name opened for a space_id during recovery
from the file map.
@param[in]	space_id	Undo tablespace ID
@return file name that was opened, empty string if space ID not found. */
std::string
fil_system_open_fetch(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Truncate the tablespace to needed size.
@param[in]	space_id	Id of tablespace to truncate
@param[in]	size_in_pages	Truncate size.
@return true if truncate was successful. */
bool
fil_truncate_tablespace(
	space_id_t	space_id,
	page_no_t	size_in_pages)
	MY_ATTRIBUTE((warn_unused_result));

/** Closes a single-table tablespace. The tablespace must be cached in the
memory cache. Free all pages used by the tablespace.
@param[in,out]	trx		Transaction covering the close
@param[in]	space_id	Tablespace ID
@return DB_SUCCESS or error */
dberr_t
fil_close_tablespace(trx_t* trx, space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but

 1. We do not drop the table from the data dictionary;

 2. We remove all insert buffer entries for the tablespace immediately;
    in DROP TABLE they are only removed gradually in the background;

 3. When the user does IMPORT TABLESPACE, the tablespace will have the
    same id as it originally had.

 4. Free all the pages in use by the tablespace if rename=true.
@param[in]	space_id	Tablespace ID
@return DB_SUCCESS or error */
dberr_t
fil_discard_tablespace(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */

/** Test if a tablespace file can be renamed to a new filepath by checking
if that the old filepath exists and the new filepath does not exist.
@param[in]	space_id	Tablespace ID
@param[in]	old_path	Old filepath
@param[in]	new_path	New filepath
@param[in]	is_discarded	Whether the tablespace is discarded
@return innodb error code */
dberr_t
fil_rename_tablespace_check(
	space_id_t	space_id,
	const char*	old_path,
	const char*	new_path,
	bool		is_discarded)
	MY_ATTRIBUTE((warn_unused_result));

/** Rename a single-table tablespace.
The tablespace must exist in the memory cache.
@param[in]	space_id	Tablespace ID
@param[in]	old_path	Old file name
@param[in]	new_name	New table name in the databasename/tablename
				format
@param[in]	new_path_in	New file name, or nullptr if it is located in
				The normal data directory
@return true if success */
bool
fil_rename_tablespace(
	space_id_t	space_id,
	const char*	old_path,
	const char*	new_name,
	const char*	new_path_in)
	MY_ATTRIBUTE((warn_unused_result));

/** Allocate and build a file name from a path, a table or tablespace name
and a suffix.
@param[in]	path		nullptr or the direcory path or the full path
				and filename
@param[in]	name		nullptr if path is full, or Table/Tablespace
				name
@param[in]	ext		The file extension to use
@param[in]	trim		Whether last name on the path should be trimmed
@return own: file name; must be freed by ut_free() */
char*
fil_make_filepath(
	const char*	path,
	const char*	name,
	ib_extention	ext,
	bool		trim)
	MY_ATTRIBUTE((warn_unused_result));

/** Create a tablespace file.
@param[in]	space_id	Tablespace ID
@param[in]	name		Tablespace name in dbname/tablename format.
				For general tablespaces, the 'dbname/' part
				may be missing.
@param[in]	path		Path and filename of the datafile to create.
@param[in]	flags		Tablespace flags
@param[in]	size		Initial size of the tablespace file in pages,
				must be >= FIL_IBD_FILE_INITIAL_SIZE
@return DB_SUCCESS or error code */
dberr_t
fil_ibd_create(
	space_id_t	space_id,
	const char*	name,
	const char*	path,
	ulint		flags,
	page_no_t	size)
	MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_HOTBACKUP

/** Open a single-table tablespace and optionally check the space id is
right in it. If not successful, print an error message to the error log. This
function is used to open a tablespace when we start up mysqld, and also in
IMPORT TABLESPACE.
NOTE that we assume this operation is used either at the database startup
or under the protection of the dictionary mutex, so that two users cannot
race here.

The fil_node_t::handle will not be left open.

@param[in]	validate	whether we should validate the tablespace
				(read the first page of the file and
				check that the space id in it matches id)
@param[in]	purpose		FIL_TYPE_TABLESPACE or FIL_TYPE_TEMPORARY
@param[in]	space_id	Tablespace ID
@param[in]	flags		tablespace flags
@param[in]	space_name	tablespace name of the datafile
If file-per-table, it is the table name in the databasename/tablename format
@param[in]	path_in		expected filepath, usually read from dictionary
@return DB_SUCCESS or error code */
dberr_t
fil_ibd_open(
	bool		validate,
	fil_type_t	purpose,
	space_id_t	space_id,
	ulint		flags,
	const char*	space_name,
	const char*	path_in)
	MY_ATTRIBUTE((warn_unused_result));

/** Returns true if a matching tablespace exists in the InnoDB tablespace
memory cache. Note that if we have not done a crash recovery at the database
startup, there may be many tablespaces which are not yet in the memory cache.
@param[in]	space_id	Tablespace ID
@param[in]	name		Tablespace name used in
				fil_space_create().
@param[in]	print_err	detailed error information to the
				error log if a matching tablespace is
				not found from memory.
@param[in]	adjust_space	Whether to adjust spaceid on mismatch
@param[in]	heap		Heap memory
@param[in]	table_id	table id
@return true if a matching tablespace exists in the memory cache */
bool
fil_space_for_table_exists_in_mem(
	space_id_t	space_id,
	const char*	name,
	bool		print_err,
	bool		adjust_space,
	mem_heap_t*	heap,
	table_id_t	table_id)
	MY_ATTRIBUTE((warn_unused_result));
#else /* !UNIV_HOTBACKUP */

/** Extends all tablespaces to the size stored in the space header. During the
mysqlbackup --apply-log phase we extended the spaces on-demand so that log
records could be appllied, but that may have left spaces still too small
compared to the size stored in the space header. */
void
fil_extend_tablespaces_to_stored_len();

#endif /* !UNIV_HOTBACKUP */

/** Try to extend a tablespace if it is smaller than the specified size.
@param[in,out]	space		Tablespace ID
@param[in]	size		desired size in pages
@return whether the tablespace is at least as big as requested */
bool
fil_space_extend(
	fil_space_t*	space,
	page_no_t	size)
	MY_ATTRIBUTE((warn_unused_result));

/** Tries to reserve free extents in a file space.
@param[in]	space_id	Tablespace ID
@param[in]	n_free_now	Number of free extents now
@param[in]	n_to_reserve	How many one wants to reserve
@return true if succeed */
bool
fil_space_reserve_free_extents(
	space_id_t	space_id,
	ulint		n_free_now,
	ulint		n_to_reserve)
	MY_ATTRIBUTE((warn_unused_result));

/** Releases free extents in a file space.
@param[in]	space_id	Tablespace ID
@param[in]	n_reserved	How many were reserved */
void
fil_space_release_free_extents(
	space_id_t	space_id,
	ulint		n_reserved);

/** Gets the number of reserved extents. If the database is silent, this
number should be zero.
@param[in]	space_id	Tablespace ID
@return the number of reserved extents */
ulint
fil_space_get_n_reserved_extents(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Read or write data. This operation could be asynchronous (aio).
@param[in,out]	type		IO context
@param[in]	sync		whether synchronous aio is desired
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	byte_offset	remainder of offset in bytes; in aio this
				must be divisible by the OS block size
@param[in]	len		how many bytes to read or write; this must
				not cross a file boundary; in aio this must
				be a block size multiple
@param[in,out]	buf		buffer where to store read data or from where
				to write; in aio this must be appropriately
				aligned
@param[in]	message		message for aio handler if !sync, else ignored
@return error code
@retval DB_SUCCESS on success
@retval DB_TABLESPACE_DELETED if the tablespace does not exist */
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
	MY_ATTRIBUTE((warn_unused_result));

/** Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.cc for more info). The thread specifies which
segment it wants to wait for.
@param[in]	segment		The number of the segment in the AIO array
				to wait for */
void
fil_aio_wait(ulint	segment);

/** Flushes to disk possible writes cached by the OS. If the space does
not exist or is being dropped, does not do anything.
@param[in]	space_id	Tablespace ID (this can be a group of log files
				or a tablespace of the database) */
void
fil_flush(space_id_t space_id);

/** Flush to disk the writes in file spaces of the given type
possibly cached by the OS.
@param[in]	purpose		FIL_TYPE_TABLESPACE or FIL_TYPE_LOG, can
				be ORred. */
void
fil_flush_file_spaces(uint8_t purpose);

#ifdef UNIV_DEBUG
/** Checks the consistency of the tablespace cache.
@return true if ok */
bool
fil_validate();
#endif /* UNIV_DEBUG */

/** Returns true if file address is undefined.
@param[in]	addr		File address to check
@return true if undefined */
bool
fil_addr_is_null(fil_addr_t addr)
	MY_ATTRIBUTE((warn_unused_result));

/** Get the predecessor of a file page.
@param[in]	page		File page
@return FIL_PAGE_PREV */
page_no_t
fil_page_get_prev(const byte* page)
	MY_ATTRIBUTE((warn_unused_result));

/** Get the successor of a file page.
@param[in]	page		File page
@return FIL_PAGE_NEXT */
page_no_t
fil_page_get_next(const byte* page)
	MY_ATTRIBUTE((warn_unused_result));

/** Sets the file page type.
@param[in,out]	page		File page
@param[in]	type		File page type to set */
void
fil_page_set_type(byte* page, ulint type);

/** Reset the page type.
Data files created before MySQL 5.1 may contain garbage in FIL_PAGE_TYPE.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in]	page_id		page number
@param[in,out]	page		page with invalid FIL_PAGE_TYPE
@param[in]	type		expected page type
@param[in,out]	mtr		mini-transaction */
void
fil_page_reset_type(
	const page_id_t&	page_id,
	byte*			page,
	ulint			type,
	mtr_t*			mtr);

/** Get the file page type.
@param[in]	page		File page
@return page type */
inline
page_type_t
fil_page_get_type(
	const byte*	page)
{
	return(static_cast<page_type_t>(
			mach_read_from_2(page + FIL_PAGE_TYPE)));
}
/** Check (and if needed, reset) the page type.
Data files created before MySQL 5.1 may contain
garbage in the FIL_PAGE_TYPE field.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in]	page_id		page number
@param[in,out]	page		page with possibly invalid FIL_PAGE_TYPE
@param[in]	type		expected page type
@param[in,out]	mtr		mini-transaction */
inline
void
fil_page_check_type(
	const page_id_t&	page_id,
	byte*			page,
	ulint			type,
	mtr_t*			mtr)
{
	ulint	page_type = fil_page_get_type(page);

	if (page_type != type) {
		fil_page_reset_type(page_id, page, type, mtr);
	}
}

/** Check (and if needed, reset) the page type.
Data files created before MySQL 5.1 may contain
garbage in the FIL_PAGE_TYPE field.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in,out]	block		block with possibly invalid FIL_PAGE_TYPE
@param[in]	type		expected page type
@param[in,out]	mtr		mini-transaction */
#define fil_block_check_type(block, type, mtr)				\
	fil_page_check_type(block->page.id, block->frame, type, mtr)

#ifdef UNIV_DEBUG
/** Increase redo skipped of a tablespace.
@param[in]	space_id	Tablespace ID */
void
fil_space_inc_redo_skipped_count(space_id_t space_id);

/** Decrease redo skipped of a tablespace.
@param[in]	space_id	Tablespace ID */
void
fil_space_dec_redo_skipped_count(space_id_t space_id);

/** Check whether a single-table tablespace is redo skipped.
@param[in]	space_id	Tablespace ID
@return true if redo skipped */
bool
fil_space_is_redo_skipped(space_id_t space_id);
#endif /* UNIV_DEBUG */

/** Delete the tablespace file and any related files like .cfg.
This should not be called for temporary tables.
@param[in]	path		File path of the tablespace */
void
fil_delete_file(const char* path);

/** Callback functor. */
struct PageCallback {

	/** Default constructor */
	PageCallback()
		:
		m_page_size(0, 0, false),
		m_filepath() UNIV_NOTHROW {}

	virtual ~PageCallback() UNIV_NOTHROW {}

	/** Called for page 0 in the tablespace file at the start.
	@param file_size size of the file in bytes
	@param block contents of the first page in the tablespace file
	@retval DB_SUCCESS or error code. */
	virtual dberr_t init(
		os_offset_t		file_size,
		const buf_block_t*	block) UNIV_NOTHROW = 0;

	/** Called for every page in the tablespace. If the page was not
	updated then its state must be set to BUF_PAGE_NOT_USED. For
	compressed tables the page descriptor memory will be at offset:
	block->frame + UNIV_PAGE_SIZE;
	@param offset physical offset within the file
	@param block block read from file, note it is not from the buffer pool
	@retval DB_SUCCESS or error code. */
	virtual dberr_t operator()(
		os_offset_t	offset,
		buf_block_t*	block) UNIV_NOTHROW = 0;

	/** Set the name of the physical file and the file handle that is used
	to open it for the file that is being iterated over.
	@param filename then physical name of the tablespace file.
	@param file OS file handle */
	void set_file(const char* filename, pfs_os_file_t file) UNIV_NOTHROW
	{
		m_file = file;
		m_filepath = filename;
	}

	/**
	@return the space id of the tablespace */
	virtual space_id_t get_space_id() const UNIV_NOTHROW = 0;

	/**
	@retval the space flags of the tablespace being iterated over */
	virtual ulint get_space_flags() const UNIV_NOTHROW = 0;

	/** Set the tablespace table size.
	@param[in] page a page belonging to the tablespace */
	void set_page_size(const buf_frame_t* page) UNIV_NOTHROW;

	/** The compressed page size
	@return the compressed page size */
	const page_size_t& get_page_size() const
	{
		return(m_page_size);
	}

	/** The tablespace page size. */
	page_size_t		m_page_size;

	/** File handle to the tablespace */
	pfs_os_file_t		m_file;

	/** Physical file path. */
	const char*		m_filepath;

	// Disable copying
	PageCallback(const PageCallback&) = delete;
	PageCallback& operator=(const PageCallback&) = delete;
};

/** Iterate over all the pages in the tablespace.
@param table the table definiton in the server
@param n_io_buffers number of blocks to read and write together
@param callback functor that will do the page updates
@return DB_SUCCESS or error code */
dberr_t
fil_tablespace_iterate(
	dict_table_t*		table,
	ulint			n_io_buffers,
	PageCallback&		callback)
	MY_ATTRIBUTE((warn_unused_result));

/** Looks for a pre-existing fil_space_t with the given tablespace ID
and, if found, returns the name and filepath in newly allocated buffers that
the caller must free.
@param[in] space_id The tablespace ID to search for.
@param[out] name Name of the tablespace found.
@param[out] filepath The filepath of the first datafile for thtablespace found.
@return true if tablespace is found, false if not. */
bool
fil_space_read_name_and_filepath(
	space_id_t	space_id,
	char**		name,
	char**		filepath)
	MY_ATTRIBUTE((warn_unused_result));

/** Convert a file name to a tablespace name.
@param[in]	filename	directory/databasename/tablename.ibd
@return database/tablename string, to be freed with ut_free() */
char*
fil_path_to_space_name(const char* filename)
	MY_ATTRIBUTE((warn_unused_result));

/** Returns the space ID based on the tablespace name.
The tablespace must be found in the tablespace memory cache.
This call is made from external to this module, so the mutex is not owned.
@param[in]	space		Tablespace name
@return space ID if tablespace found, SPACE_UNKNOWN if space not. */
space_id_t
fil_space_get_id_by_name(const char* space)
	MY_ATTRIBUTE((warn_unused_result));

/** Iterate over all the spaces in the space list and fetch the
tablespace names. It will return a copy of the name that must be
freed by the caller using: delete[].
@param[in,out]	space_name_list	Container for collecting the names
@return DB_SUCCESS if all OK. */
dberr_t
fil_get_space_names(
	space_name_list_t&	space_name_list)
	MY_ATTRIBUTE((warn_unused_result));

/** Set the compression type for the tablespace of a table
@param[in]	table		Table that should be compressesed
@param[in]	algorithm	Text representation of the algorithm
@return DB_SUCCESS or error code */
dberr_t
fil_set_compression(
	dict_table_t*	table,
	const char*	algorithm)
	MY_ATTRIBUTE((warn_unused_result));

/** Get the compression type for the tablespace
@param[in]	space_id	Space ID to check
@return the compression algorithm */
Compression::Type
fil_get_compression(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Set encryption.
@param[in,out]	req_type	IO request
@param[in]	page_id		Page address for IO
@param[in,out]	space		Tablespace instance */
void
fil_io_set_encryption(
	IORequest&		req_type,
	const page_id_t&	page_id,
	fil_space_t*		space);

/** Set the encryption type for the tablespace
@param[in] space_id		Space ID of tablespace for which to set
@param[in] algorithm		Encryption algorithm
@param[in] key			Encryption key
@param[in] iv			Encryption iv
@return DB_SUCCESS or error code */
dberr_t
fil_set_encryption(
	space_id_t		space_id,
	Encryption::Type	algorithm,
	byte*			key,
	byte*			iv)
	MY_ATTRIBUTE((warn_unused_result));

/** @return true if the re-encrypt success */
bool
fil_encryption_rotate()
	MY_ATTRIBUTE((warn_unused_result));

/** During crash recovery, open a tablespace if it had not been opened
yet, to get valid size and flags.
@param[in,out]	space		Tablespace instance */
inline
void
fil_space_open_if_needed(fil_space_t* space)
{
	if (space->size == 0) {
		/* Initially, size and flags will be set to 0,
		until the files are opened for the first time.
		fil_space_get_size() will open the file
		and adjust the size and flags. */
		page_no_t	size = fil_space_get_size(space->id);

		ut_a(size == space->size);
	}
}

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
/**
Try and enable FusionIO atomic writes.
@param[in] file		OS file handle
@return true if successful */
bool
fil_fusionio_enable_atomic_write(pfs_os_file_t file)
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !NO_FALLOCATE && UNIV_LINUX */

/** Note that the file system where the file resides doesn't support PUNCH HOLE
@param[in,out]	file		File node to set */
void fil_no_punch_hole(fil_node_t* file);

#ifdef UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH
void test_make_filepath();
#endif /* UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH */

/** @return the system tablespace instance */
#define fil_space_get_sys_space() (fil_space_t::s_sys_space)

/** Redo a tablespace create
@param[in]	ptr		redo log record
@param[in]	end		end of the redo log buffer
@param[in]	page_id		Tablespace Id and first page in file
@param[in]	parsed_bytes	Number of bytes parsed so far
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
byte*
fil_tablespace_redo_create(
	byte*		ptr,
	const byte*	end,
	const page_id_t&page_id,
	ulint		parsed_bytes)
	MY_ATTRIBUTE((warn_unused_result));

/** Redo a tablespace drop
@param[in]	ptr		redo log record
@param[in]	end		end of the redo log buffer
@param[in]	page_id		Tablespace Id and first page in file
@param[in]	parsed_bytes	Number of bytes parsed so far
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
byte*
fil_tablespace_redo_delete(
	byte*		ptr,
	const byte*	end,
	const page_id_t&page_id,
	ulint		parsed_bytes)
	MY_ATTRIBUTE((warn_unused_result));

/** Redo a tablespace rename
@param[in]	ptr		redo log record
@param[in]	end		end of the redo log buffer
@param[in]	page_id		Tablespace Id and first page in file
@param[in]	parsed_bytes	Number of bytes parsed so far
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
byte*
fil_tablespace_redo_rename(
	byte*		ptr,
	const byte*	end,
	const page_id_t&page_id,
	ulint		parsed_bytes)
	MY_ATTRIBUTE((warn_unused_result));

/** Read the tablespace id to path mapping from the file
@param[in]	recovery	true if called from crash recovery */
void
fil_tablespace_open_init_for_recovery(bool recovery);

/** Lookup the space ID.
@param[in]	space_id	Tablespace ID to lookup
@return true if space ID is known and open */
bool
fil_tablespace_lookup_for_recovery(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Lookup the tablespace ID and return the path to the file.
@param[in]	space_id	Tablespace ID to lookup
@param[in]	path		Path in the data dictionary
@return path to new filename if it doesn't match else "" */
Fil_path
fil_tablespace_path_equals(
	space_id_t	space_id,
	const char*	path,
	std::string*	new_path)
	MY_ATTRIBUTE((warn_unused_result));

/** This function should be called after recovery has completed.
Check for tablespace files for which we did not see any MLOG_FILE_DELETE
or MLOG_FILE_RENAME record. These could not be recovered
@return true if there were some filenames missing for which we had to
ignore redo log records during the apply phase */
bool
fil_check_missing_tablespaces()
	MY_ATTRIBUTE((warn_unused_result));

/** Discover tablespaces by reading the header from .ibd files.
@param[in]	directories	Directories to scan
@return DB_SUCCESS if all goes well */
dberr_t
fil_scan_for_tablespaces(const std::string& directories);

/** Open the tabelspace and also get the tablespace filenames, space_id must
already be known.
@param[in]	space_id	Tablespace ID to lookup
@return true if open was successful */
bool
fil_tablespace_open_for_recovery(space_id_t space_id)
	MY_ATTRIBUTE((warn_unused_result));

/** Callback to check tablespace size with space header size and extend
@param[in]	file	file node
@param[in]	context	callers context, currently unused
@return	error code */
dberr_t
fil_check_extend_space(
	fil_node_t*	file,
	void* 		context MY_ATTRIBUTE((unused)))
	MY_ATTRIBUTE((warn_unused_result));

/** Get the space IDs active in the system.
@param[out]	space_ids	All the registered tablespace IDs */
void
fil_space_ids_get(Space_ids* space_ids);

/** Get the filenames for a tablespace ID and increment pending ops.
@param[in]	space_id	Tablespace ID
@param[out]	files		Files for a tablespace ID */
void
fil_node_fetch(
	space_id_t		space_id,
	fil_space_t::Files*	files);

/** Releases the tablespace instance by decrementing pending ops.
@param[in]	space_id	Tablespace ID to release. */
void
fil_node_release(space_id_t space_id);

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
	MY_ATTRIBUTE((warn_unused_result));

/** Free the Tablespace_files instance. */
void
fil_open_for_business();

#endif /* fil0fil_h */
