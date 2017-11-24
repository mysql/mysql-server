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
# include "ibuf0types.h"
#endif /* !UNIV_HOTBACKUP */

#include <list>
#include <vector>

extern const char general_space_name[];

#ifdef UNIV_HOTBACKUP
# include<unordered_set>
using dir_set = std::unordered_set<std::string>;
extern dir_set rem_gen_ts_dirs;
extern bool replay_in_datadir;
#endif /* UNIV_HOTBACKUP */

// Forward declaration
struct trx_t;
class page_id_t;
struct fil_node_t;

using Filenames = std::vector<std::string>;

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

struct fil_node_t;

/** Tablespace or log data space */
struct fil_space_t {

	/** Tablespace name */
	char*		name;

	/** space id */
	space_id_t	id;

	/** LSN of the most recent fil_names_write_if_was_clean().
	Reset to 0 by fil_names_clear().  Protected by log_sys->mutex.
	If and only if this is nonzero, the tablespace will be in
	named_spaces. */
	lsn_t		max_lsn;

	/** true if we want to rename the .ibd file of tablespace and
	want to stop temporarily posting of new i/o requests on the file */
	bool		stop_ios;

	/** we set this true when we start deleting a single-table
	tablespace.  When this is set following new ops are not allowed:
	* read IO request
	* ibuf merge
	* file flush
	Note that we can still possibly have new write operations because we
	don't check this flag when doing flush batches. */
	bool		stop_new_ops;

#ifdef UNIV_DEBUG
	/** reference count for operations who want to skip redo log in
	the file space in order to make fsp_space_modify_check pass. */
	ulint		redo_skipped_count;
#endif

	/** purpose */
	fil_type_t	purpose;

	/** base node for the file chain */
	UT_LIST_BASE_NODE_T(fil_node_t) chain;

	/** tablespace file size in pages; 0 if not known yet */
	page_no_t	size;

	/** FSP_SIZE in the tablespace header; 0 if not known yet */
	page_no_t	size_in_header;

	/** length of the FSP_FREE list */
	ulint		free_len;

	/** contents of FSP_FREE_LIMIT */
	page_no_t	free_limit;

	/** tablespace flags;
	see fsp_flags_is_valid(), page_size_t(ulint) (constructor).
	This is protected by space->latch and tablespace MDL */
	ulint		flags;

	/** number of reserved free extents for ongoing operations like
	B-tree page split */
	ulint		n_reserved_extents;

	/** this is positive when flushing the tablespace to disk;
	dropping of the tablespace is forbidden if this is positive */
	ulint		n_pending_flushes;

	/** this is positive when we have pending operations against this
	tablespace. The pending operations can be ibuf merges or lock
	validation code trying to read a block.  Dropping of the tablespace
	is forbidden if this is positive.  Protected by fil_system->mutex. */
	ulint		n_pending_ops;

	/** hash chain node */
	hash_node_t	hash;

	/** hash chain the name_hash table */
	hash_node_t	name_hash;

#ifndef UNIV_HOTBACKUP
	/** latch protecting the file space storage allocation */
	rw_lock_t	latch;
#endif /* !UNIV_HOTBACKUP */

	/** list of spaces with at least one unflushed file we have
	written to */
	UT_LIST_NODE_T(fil_space_t) unflushed_spaces;

	/** list of spaces for which MLOG_FILE_OPEN records have been issued */
	UT_LIST_NODE_T(fil_space_t) named_spaces;

	/** true if this space is currently in unflushed_spaces */
	bool		is_in_unflushed_spaces;

	/** list of all spaces */
	UT_LIST_NODE_T(fil_space_t) space_list;

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
#define	FIL_SPACE_MAGIC_N	89472

/** File node of a tablespace or the log data space */
struct fil_node_t {
	/** tablespace containing this file */
	fil_space_t*	space;
	/** file name; protected by fil_system->mutex and log_sys->mutex. */
	char*		name;
	/** whether this file is open. Note: We set the is_open flag after
	we increase the write the MLOG_FILE_OPEN record to redo log. Therefore
	we increment the in_use reference count before setting the OPEN flag. */
	bool		is_open;
	/** file handle (valid if is_open) */
	pfs_os_file_t	handle;
	/** event that groups and serializes calls to fsync */
	os_event_t	sync_event;
	/** whether the file actually is a raw device or disk partition */
	bool		is_raw_disk;
	/** size of the file in database pages (0 if not known yet);
	the possible last incomplete megabyte may be ignored
	if space->id == 0 */
	page_no_t	size;
	/** initial size of the file in database pages;
	FIL_IBD_FILE_INITIAL_SIZE by default */
	page_no_t	init_size;
	/** maximum size of the file in database pages */
	page_no_t	max_size;
	/** count of pending i/o's; is_open must be true if nonzero */
	ulint		n_pending;
	/** count of pending flushes; is_open must be true if nonzero */
	ulint		n_pending_flushes;
	/** e.g., when a file is being extended or just opened. */
	size_t		in_use;
	/** number of writes to the file since the system was started */
	int64_t		modification_counter;
	/** the modification_counter of the latest flush to disk */
	int64_t		flush_counter;
	/** link to other files in this tablespace */
	UT_LIST_NODE_T(fil_node_t) chain;
	/** link to the fil_system->LRU list (keeping track of open files) */
	UT_LIST_NODE_T(fil_node_t) LRU;

	/** whether the file system of this file supports PUNCH HOLE */
	bool		punch_hole;

	/** block size to use for punching holes */
	ulint           block_size;

	/** whether atomic write is enabled for this file */
	bool		atomic_write;

	/** FIL_NODE_MAGIC_N */
	ulint		magic_n;
};

/** Value of fil_node_t::magic_n */
#define	FIL_NODE_MAGIC_N	89389

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
#define FIL_IBD_FILE_INITIAL_SIZE	5

/** 'null' (undefined) page offset in the context of file spaces */
constexpr page_no_t FIL_NULL = std::numeric_limits<page_no_t>::max();

/** Maximum Page Number, one less than FIL_NULL */
constexpr page_no_t PAGE_NO_MAX = std::numeric_limits<page_no_t>::max() - 1;

/** Unknown space id */
constexpr space_id_t SPACE_UNKNOWN = std::numeric_limits<space_id_t>::max();

/* Space address data type; this is intended to be used when
addresses accurate to a byte are stored in file pages. If the page part
of the address is FIL_NULL, the address is considered undefined. */

typedef	byte	fil_faddr_t;	/*!< 'type' definition in C: an address
				stored in a file page is a string of bytes */
#define FIL_ADDR_PAGE	0	/* first in address is the page offset */
#define	FIL_ADDR_BYTE	4	/* then comes 2-byte byte offset within page*/
#define	FIL_ADDR_SIZE	6	/* address size is 6 bytes */

/** File space address */
struct fil_addr_t {

	fil_addr_t() : page(FIL_NULL), boffset(0) {}
	fil_addr_t(page_no_t p, ulint off) : page(p), boffset(off) {}

	page_no_t	page;		/*!< page number within a space */
	ulint		boffset;	/*!< byte offset within the page */

	bool is_equal(const fil_addr_t& that) const {
		return((page == that.page) && (boffset == that.boffset));
	}

	/** Check if the file address is null.
	@return true if null, false otherwise. */
	bool is_null() const {
		return(page == FIL_NULL && boffset == 0);
	}

	std::ostream& print(std::ostream& out) const
	{
		out << "[fil_addr_t: page=" << page << ", boffset="
			<< boffset << "]";
		return(out);
	}
};

inline
std::ostream&
operator<<(std::ostream& out, const fil_addr_t&	obj)
{
	return(obj.print(out));
}

/** The null file address */
extern fil_addr_t	fil_addr_null;
typedef	uint16_t	page_type_t;

/** File page types (values of FIL_PAGE_TYPE) @{ */
#define FIL_PAGE_INDEX		17855	/*!< B-tree node */
#define FIL_PAGE_RTREE		17854	/*!< R-tree node */
#define FIL_PAGE_SDI		17853	/*!< Tablespace SDI Index page */
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
#define FIL_PAGE_TYPE_UNKNOWN	13	/*!< In old tablespaces, garbage
					in FIL_PAGE_TYPE is replaced with this
					value when flushing pages. */
#define FIL_PAGE_COMPRESSED	14	/*!< Compressed page */
#define FIL_PAGE_ENCRYPTED	15	/*!< Encrypted page */
#define FIL_PAGE_COMPRESSED_AND_ENCRYPTED 16
					/*!< Compressed and Encrypted page */
#define FIL_PAGE_ENCRYPTED_RTREE 17	/*!< Encrypted R-tree page */
#define FIL_PAGE_SDI_BLOB	18	/*!< Uncompressed SDI BLOB page */
#define FIL_PAGE_SDI_ZBLOB	19	/*!< Commpressed SDI BLOB page */
#define FIL_PAGE_TYPE_UNUSED	20	/*!< Available for future use. */
#define FIL_PAGE_TYPE_RSEG_ARRAY 21	/*!< Rollback Segment Array page */

/** Index pages of uncompressed LOB */
#define FIL_PAGE_TYPE_LOB_INDEX		22

/** Data pages of uncompressed LOB */
#define FIL_PAGE_TYPE_LOB_DATA		23

/** The first page of an uncompressed LOB */
#define FIL_PAGE_TYPE_LOB_FIRST		24

/** The first page of a compressed LOB */
#define FIL_PAGE_TYPE_ZLOB_FIRST	25

/** Data pages of compressed LOB */
#define FIL_PAGE_TYPE_ZLOB_DATA		26

/** Index pages of compressed LOB. This page contains an array of
z_index_entry_t objects.*/
#define FIL_PAGE_TYPE_ZLOB_INDEX	27

/** Fragment pages of compressed LOB. */
#define FIL_PAGE_TYPE_ZLOB_FRAG		28

/** Index pages of fragment pages (compressed LOB). */
#define FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY	29

/** Used by i_s.cc to index into the text description. */
#define FIL_PAGE_TYPE_LAST		FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY
					/*!< Last page type */
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
	space_id_t	id)
	MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_HOTBACKUP
/** Returns the latch of a file space.
@param[in]	id	space id
@param[out]	flags	tablespace flags
@return latch protecting storage allocation */
rw_lock_t*
fil_space_get_latch(
	space_id_t	id,
	ulint*		flags);

#ifdef UNIV_DEBUG
/** Gets the type of a file space.
@param[in]	id	tablespace identifier
@return file type */
fil_type_t
fil_space_get_type(space_id_t id);
#endif /* UNIV_DEBUG */

/** Note that a tablespace has been imported.
It is initially marked as FIL_TYPE_IMPORT so that no logging is
done during the import process when the space ID is stamped to each page.
Now we change it to FIL_SPACE_TABLESPACE to start redo and undo logging.
NOTE: temporary tablespaces are never imported.
@param[in]	id	tablespace identifier */
void
fil_space_set_imported(
	space_id_t	id);

# ifdef UNIV_DEBUG
/** Determine if a tablespace is temporary.
@param[in]	id	tablespace identifier
@return whether it is a temporary tablespace */
bool
fsp_is_temporary(ulint id)
MY_ATTRIBUTE((warn_unused_result, pure));
# endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

/** Append a file to the chain of files of a space.
@param[in]	name		file name of a file that is not open
@param[in]	size		file size in entire database blocks
@param[in,out]	space		tablespace from fil_space_create()
@param[in]	is_raw		whether this is a raw device or partition
@param[in]	atomic_write	true if atomic write enabled
@param[in]	max_pages	maximum number of pages in file
@return pointer to the file name
@retval NULL if error */
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
@param[in]	name	tablespace name
@param[in]	id	tablespace identifier
@param[in]	flags	tablespace flags
@param[in]	purpose	tablespace purpose
@return pointer to created tablespace, to be filled in with fil_node_create()
@retval NULL on failure (such as when the same tablespace exists) */
fil_space_t*
fil_space_create(
	const char*	name,
	space_id_t	id,
	ulint		flags,
	fil_type_t	purpose)
	MY_ATTRIBUTE((warn_unused_result));

/*******************************************************************//**
Assigns a new space id for a new single-table tablespace. This works simply by
incrementing the global counter. If 4 billion id's is not enough, we may need
to recycle id's.
@return true if assigned, false if not */
bool
fil_assign_new_space_id(
/*====================*/
	space_id_t*	space_id);	/*!< in/out: space id */

/** Returns the path from the first fil_node_t found with this space ID.
The caller is responsible for freeing the memory allocated here for the
value returned.
@param[in]	id	Tablespace ID
@return own: A copy of fil_node_t::path, NULL if space ID is zero
or not found. */
char*
fil_space_get_first_path(space_id_t id);

/*******************************************************************//**
Returns the size of the space in pages. The tablespace must be cached in the
memory cache.
@return space size, 0 if space not found */
page_no_t
fil_space_get_size(
/*===============*/
	space_id_t	id);	/*!< in: space id */

/** Returns the flags of the space. The tablespace must be cached
in the memory cache.
@param[in]	space_id	Tablespace ID for which to get the flags
@return flags, ULINT_UNDEFINED if space not found */
ulint
fil_space_get_flags(space_id_t space_id);

/** Sets the flags of the tablespace. The tablespace must be locked
in MDL_EXCLUSIVE MODE.
@param[in]	space	tablespace in-memory struct
@param[in]	flags	tablespace flags */
void
fil_space_set_flags(
	fil_space_t*	space,
	ulint		flags);

/** Open each file of a tablespace if not already open.
@param[in]	space_id	tablespace identifier
@retval	true	if all file nodes were opened
@retval	false	on failure */
bool
fil_space_open(space_id_t space_id);

/** Close each file of a tablespace if open.
@param[in]	space_id	tablespace identifier */
void
fil_space_close(space_id_t space_id);

/** Returns the page size of the space and whether it is compressed or not.
The tablespace must be cached in the memory cache.
@param[in]	id	space id
@param[out]	found	true if tablespace was found
@return page size */
const page_size_t
fil_space_get_page_size(
	space_id_t	id,
	bool*		found);

/****************************************************************//**
Initializes the tablespace memory cache. */
void
fil_init(
/*=====*/
	ulint	hash_size,	/*!< in: hash table size */
	ulint	max_n_open);	/*!< in: max number of open files */
/*******************************************************************//**
Initializes the tablespace memory cache. */
void
fil_close(void);
/*===========*/
/*******************************************************************//**
Opens all log files and system tablespace data files. They stay open until the
database server shutdown. This should be called at a server startup after the
space objects for the log and the system tablespace have been created. The
purpose of this operation is to make sure we never run out of file descriptors
if we need to read from the insert buffer or to write to the log. */
void
fil_open_log_and_system_tablespace_files(void);
/*==========================================*/
/*******************************************************************//**
Closes all open files. There must not be any pending i/o's or not flushed
modifications in the files. */
void
fil_close_all_files(void);
/*=====================*/
/*******************************************************************//**
Closes the redo log files. There must not be any pending i/o's or not
flushed modifications in the files. */
void
fil_close_log_files(
/*================*/
	bool	free);	/*!< in: whether to free the memory object */

/** File Node Iterator callback. */
using fil_node_cbk_t = dberr_t (fil_node_t* node, void* context);

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
	fil_node_cbk_t*	callback);

/*******************************************************************//**
Sets the max tablespace id counter if the given number is bigger than the
previous value. */
void
fil_set_max_space_id_if_bigger(
/*===========================*/
	space_id_t	max_id);/*!< in: maximum known id */
#ifndef UNIV_HOTBACKUP
/** Write the flushed LSN to the page header of the first page in the
system tablespace.
@param[in]	lsn	flushed LSN
@return DB_SUCCESS or error number */
dberr_t
fil_write_flushed_lsn(
	lsn_t	lsn);
#endif /* !UNIV_HOTBACKUP */

/** Acquire a tablespace when it could be dropped concurrently.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	id	tablespace ID
@return the tablespace, or NULL if missing or being deleted */
fil_space_t*
fil_space_acquire(
	space_id_t	id)
	MY_ATTRIBUTE((warn_unused_result));

/** Acquire a tablespace that may not exist.
Used by background threads that do not necessarily hold proper locks
for concurrency control.
@param[in]	id	tablespace ID
@return the tablespace, or NULL if missing or being deleted */
fil_space_t*
fil_space_acquire_silent(
	space_id_t	id)
	MY_ATTRIBUTE((warn_unused_result));

/** Release a tablespace acquired with fil_space_acquire().
@param[in,out]	space	tablespace to release  */
void
fil_space_release(
	fil_space_t*	space);

#ifndef UNIV_HOTBACKUP
/** Wrapper with reference-counting for a fil_space_t. */
class FilSpace
{
public:
	/** Default constructor: Use this when reference counting
	is done outside this wrapper. */
	FilSpace() : m_space(NULL) {}

	/** Constructor: Look up the tablespace and increment the
	referece count if found.
	@param[in]	space_id	tablespace ID */
	explicit FilSpace(space_id_t space_id)
		: m_space(fil_space_acquire(space_id)) {}

	/** Assignment operator: This assumes that fil_space_acquire()
	has already been done for the fil_space_t. The caller must
	assign NULL if it calls fil_space_release().
	@param[in]	space	tablespace to assign */
	class FilSpace& operator=(
		fil_space_t*	space)
	{
		/* fil_space_acquire() must have been invoked. */
		ut_ad(space == NULL || space->n_pending_ops > 0);
		m_space = space;
		return(*this);
	}

	/** Destructor - Decrement the reference count if a fil_space_t
	is still assigned. */
	~FilSpace()
	{
		if (m_space != NULL) {
			fil_space_release(m_space);
		}
	}

	/** Implicit type conversion
	@return the wrapped object */
	operator const fil_space_t*() const
	{
		return(m_space);
	}

	/** Explicit type conversion
	@return the wrapped object */
	const fil_space_t* operator()() const
	{
		return(m_space);
	}

private:
	/** The wrapped pointer */
	fil_space_t*	m_space;
};
#endif /* !UNIV_HOTBACKUP */

/** Deletes an IBD tablespace, either general or single-table.
The tablespace must be cached in the memory cache. This will delete the
datafile, fil_space_t & fil_node_t entries from the file_system_t cache.
@param[in]	id		Tablespace id
@param[in]	buf_remove	Specify the action to take on the pages
for this table in the buffer pool.
@return true if success */
dberr_t
fil_delete_tablespace(
	space_id_t	id,
	buf_remove_t	buf_remove);

/* Convert the paths into absolute paths and compare them.
@param[in]	lhs		Filename to compare
@param[in]	rhs		Filename to compare
@return true if they are the same */
bool
fil_paths_equal(const char* lhs, const char* rhs);

/** Fetch the file name opened for a space_id during recovery
from the file map.
@param[in]	space_id	undo tablespace id
@return file name that was opened */
std::string
fil_system_open_fetch(space_id_t space_id);

/** Truncate the tablespace to needed size.
@param[in]	space_id	id of tablespace to truncate
@param[in]	size_in_pages	truncate size.
@return true if truncate was successful. */
bool
fil_truncate_tablespace(
	space_id_t	space_id,
	page_no_t	size_in_pages);

/*******************************************************************//**
Closes a single-table tablespace. The tablespace must be cached in the
memory cache. Free all pages used by the tablespace.
@return DB_SUCCESS or error */
dberr_t
fil_close_tablespace(
/*=================*/
	trx_t*		trx,	/*!< in/out: Transaction covering the close */
	space_id_t	id);	/*!< in: space id */
/*******************************************************************//**
Discards a single-table tablespace. The tablespace must be cached in the
memory cache. Discarding is like deleting a tablespace, but

 1. We do not drop the table from the data dictionary;

 2. We remove all insert buffer entries for the tablespace immediately;
    in DROP TABLE they are only removed gradually in the background;

 3. When the user does IMPORT TABLESPACE, the tablespace will have the
    same id as it originally had.

 4. Free all the pages in use by the tablespace if rename=true.
@return DB_SUCCESS or error */
dberr_t
fil_discard_tablespace(
/*===================*/
	space_id_t	id)	/*!< in: space id */
	MY_ATTRIBUTE((warn_unused_result));

/** Test if a tablespace file can be renamed to a new filepath by checking
if that the old filepath exists and the new filepath does not exist.
@param[in]	space_id	tablespace id
@param[in]	old_path	old filepath
@param[in]	new_path	new filepath
@param[in]	is_discarded	whether the tablespace is discarded
@return innodb error code */
dberr_t
fil_rename_tablespace_check(
	space_id_t	space_id,
	const char*	old_path,
	const char*	new_path,
	bool		is_discarded);

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
	space_id_t	id,
	const char*	old_path,
	const char*	new_name,
	const char*	new_path_in);

/** Allocate and build a file name from a path, a table or tablespace name
and a suffix.
@param[in]	path	NULL or the direcory path or the full path and filename
@param[in]	name	NULL if path is full, or Table/Tablespace name
@param[in]	ext	the file extension to use
@param[in]	trim	whether last name on the path should be trimmed
@return own: file name; must be freed by ut_free() */
char*
fil_make_filepath(
	const char*	path,
	const char*	name,
	ib_extention	ext,
	bool		trim);

/** Create a tablespace file.
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
	space_id_t	space_id,
	const char*	name,
	const char*	path,
	ulint		flags,
	page_no_t	size)
	MY_ATTRIBUTE((warn_unused_result));
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
@param[in]	id		tablespace ID
@param[in]	flags		tablespace flags
@param[in]	space_name	tablespace name of the datafile
If file-per-table, it is the table name in the databasename/tablename format
@param[in]	table_name	table name in case need to build filename from it
@param[in]	path_in		expected filepath, usually read from dictionary
@param[in]	strict		whether to report error when open ibd failed
@param[in]	old_space	whether it is a 5.7 tablespace opening
				by upgrade
@return DB_SUCCESS or error code */
dberr_t
fil_ibd_open(
	bool		validate,
	fil_type_t	purpose,
	space_id_t	id,
	ulint		flags,
	const char*	space_name,
	const char*	table_name,
	const char*	path_in,
	bool		strict,
	bool		old_space)
	MY_ATTRIBUTE((warn_unused_result));

#ifndef UNIV_HOTBACKUP
/** Returns true if a matching tablespace exists in the InnoDB tablespace
memory cache. Note that if we have not done a crash recovery at the database
startup, there may be many tablespaces which are not yet in the memory cache.
@param[in]	id			Tablespace ID
@param[in]	name			Tablespace name used in
					fil_space_create().
@param[in]	print_err_if_not_exist	Print detailed error information to the
					error log if a matching tablespace is
					not found from memory.
@param[in]	adjust_space		Whether to adjust spaceid on mismatch
@param[in]	heap			Heap memory
@param[in]	table_id		table id
@return true if a matching tablespace exists in the memory cache */
bool
fil_space_for_table_exists_in_mem(
	space_id_t	id,
	const char*	name,
	bool		print_err_if_not_exist,
	bool		adjust_space,
	mem_heap_t*	heap,
	table_id_t	table_id);
#else /* !UNIV_HOTBACKUP */
/** Extends all tablespaces to the size stored in the space header.
During the mysqlbackup --apply-log phase we extended the spaces
on-demand so that log records could be appllied, but that may have left
spaces still too small compared to the size stored in the space
header. */
void
meb_extend_tablespaces_to_stored_len(void);
#endif /* !UNIV_HOTBACKUP */
/** Try to extend a tablespace if it is smaller than the specified size.
@param[in,out]	space	tablespace
@param[in]	size	desired size in pages
@return whether the tablespace is at least as big as requested */
bool
fil_space_extend(
	fil_space_t*	space,
	page_no_t	size);
/*******************************************************************//**
Tries to reserve free extents in a file space.
@return true if succeed */
bool
fil_space_reserve_free_extents(
/*===========================*/
	space_id_t	id,		/*!< in: space id */
	ulint		n_free_now,	/*!< in: number of free extents now */
	ulint		n_to_reserve);	/*!< in: how many one wants to reserve */
/*******************************************************************//**
Releases free extents in a file space. */
void
fil_space_release_free_extents(
/*===========================*/
	space_id_t	id,		/*!< in: space id */
	ulint		n_reserved);	/*!< in: how many one reserved */
/*******************************************************************//**
Gets the number of reserved extents. If the database is silent, this number
should be zero. */
ulint
fil_space_get_n_reserved_extents(
/*=============================*/
	space_id_t	id);		/*!< in: space id */

/** Read or write data. This operation could be asynchronous (aio).
@param[in,out]	type		IO context
@param[in]	sync		whether synchronous aio is desired
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	byte_offset	remainder of offset in bytes; in aio this
must be divisible by the OS block size
@param[in]	len		how many bytes to read or write; this must
not cross a file boundary; in aio this must be a block size multiple
@param[in,out]	buf		buffer where to store read data or from where
to write; in aio this must be appropriately aligned
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
	void*			message);

/**********************************************************************//**
Waits for an aio operation to complete. This function is used to write the
handler for completed requests. The aio array of pending requests is divided
into segments (see os0file.cc for more info). The thread specifies which
segment it wants to wait for. */
void
fil_aio_wait(
/*=========*/
	ulint	segment);	/*!< in: the number of the segment in the aio
				array to wait for */
/**********************************************************************//**
Flushes to disk possible writes cached by the OS. If the space does not exist
or is being dropped, does not do anything. */
void
fil_flush(
/*======*/
	space_id_t	space_id);	/*!< in: file space id (this can be
					a group of log files or a tablespace
					of the database) */
/** Flush to disk the writes in file spaces of the given type
possibly cached by the OS.
@param[in]	purpose		FIL_TYPE_TABLESPACE or FIL_TYPE_LOG, can
				be ORred. */
void
fil_flush_file_spaces(uint8_t purpose);
/******************************************************************//**
Checks the consistency of the tablespace cache.
@return true if ok */
bool
fil_validate(void);
/*==============*/
/********************************************************************//**
Returns true if file address is undefined.
@return true if undefined */
bool
fil_addr_is_null(
/*=============*/
	fil_addr_t	addr);	/*!< in: address */
/********************************************************************//**
Get the predecessor of a file page.
@return FIL_PAGE_PREV */
page_no_t
fil_page_get_prev(
/*==============*/
	const byte*	page);	/*!< in: file page */
/********************************************************************//**
Get the successor of a file page.
@return FIL_PAGE_NEXT */
page_no_t
fil_page_get_next(
/*==============*/
	const byte*	page);	/*!< in: file page */
/*********************************************************************//**
Sets the file page type. */
void
fil_page_set_type(
/*==============*/
	byte*	page,	/*!< in/out: file page */
	ulint	type);	/*!< in: type */
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
	mtr_t*			mtr);
/** Get the file page type.
@param[in]	page	file page
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
@param[in]	page_id	page number
@param[in,out]	page	page with possibly invalid FIL_PAGE_TYPE
@param[in]	type	expected page type
@param[in,out]	mtr	mini-transaction */
inline
void
fil_page_check_type(
	const page_id_t&	page_id,
	byte*			page,
	ulint			type,
	mtr_t*			mtr)
{
	ulint	page_type	= fil_page_get_type(page);

	if (page_type != type) {
		fil_page_reset_type(page_id, page, type, mtr);
	}
}

/** Check (and if needed, reset) the page type.
Data files created before MySQL 5.1 may contain
garbage in the FIL_PAGE_TYPE field.
In MySQL 3.23.53, only undo log pages and index pages were tagged.
Any other pages were written with uninitialized bytes in FIL_PAGE_TYPE.
@param[in,out]	block	block with possibly invalid FIL_PAGE_TYPE
@param[in]	type	expected page type
@param[in,out]	mtr	mini-transaction */
#define fil_block_check_type(block, type, mtr)				\
	fil_page_check_type(block->page.id, block->frame, type, mtr)

#ifdef UNIV_DEBUG
/** Increase redo skipped of a tablespace.
@param[in]	id	space id */
void
fil_space_inc_redo_skipped_count(
	space_id_t	id);

/** Decrease redo skipped of a tablespace.
@param[in]	id	space id */
void
fil_space_dec_redo_skipped_count(
	space_id_t	id);

/*******************************************************************//**
Check whether a single-table tablespace is redo skipped.
@return true if redo skipped */
bool
fil_space_is_redo_skipped(
/*======================*/
	space_id_t	id);	/*!< in: space id */
#endif

/********************************************************************//**
Delete the tablespace file and any related files like .cfg.
This should not be called for temporary tables. */
void
fil_delete_file(
/*============*/
	const char*	path);	/*!< in: filepath of the ibd tablespace */

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

protected:
	// Disable copying
	PageCallback(const PageCallback&);
	PageCallback& operator=(const PageCallback&);
};

/********************************************************************//**
Iterate over all the pages in the tablespace.
@param table the table definiton in the server
@param n_io_buffers number of blocks to read and write together
@param callback functor that will do the page updates
@return DB_SUCCESS or error code */
dberr_t
fil_tablespace_iterate(
/*===================*/
	dict_table_t*		table,
	ulint			n_io_buffers,
	PageCallback&		callback)
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Looks for a pre-existing fil_space_t with the given tablespace ID
and, if found, returns the name and filepath in newly allocated buffers that the caller must free.
@param[in] space_id The tablespace ID to search for.
@param[out] name Name of the tablespace found.
@param[out] filepath The filepath of the first datafile for thtablespace found.
@return true if tablespace is found, false if not. */
bool
fil_space_read_name_and_filepath(
	space_id_t	space_id,
	char**		name,
	char**		filepath);

/** Convert a file name to a tablespace name.
@param[in]	filename	directory/databasename/tablename.ibd
@return database/tablename string, to be freed with ut_free() */
char*
fil_path_to_space_name(
	const char*	filename);

/** Returns the space ID based on the tablespace name.
The tablespace must be found in the tablespace memory cache.
This call is made from external to this module, so the mutex is not owned.
@param[in]	tablespace	Tablespace name
@return space ID if tablespace found, SPACE_UNKNOWN if space not. */
space_id_t
fil_space_get_id_by_name(
	const char*	tablespace);

/**
Iterate over all the spaces in the space list and fetch the
tablespace names. It will return a copy of the name that must be
freed by the caller using: delete[].
@return DB_SUCCESS if all OK. */
dberr_t
fil_get_space_names(
/*================*/
	space_name_list_t&	space_name_list)
				/*!< in/out: Vector for collecting the names. */
	MY_ATTRIBUTE((warn_unused_result));

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
	const fil_node_t*	prev_node);

#ifndef UNIV_HOTBACKUP
/** Check if swapping two .ibd files can be done without failure
@param[in]	old_table	old table
@param[in]	new_table	new table
@param[in]	tmp_name	temporary table name
@return innodb error code */
dberr_t
fil_rename_precheck(
	const dict_table_t*	old_table,
	const dict_table_t*	new_table,
	const char*		tmp_name);

#endif /* !UNIV_HOTBACKUP */
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

/**
@return true if the re-encrypt success */
bool
fil_encryption_rotate();

extern volatile bool	recv_recovery_on;

/** During crash recovery, open a tablespace if it had not been opened
yet, to get valid size and flags.
@param[in,out]	space	tablespace */
inline
void
fil_space_open_if_needed(
	fil_space_t*	space)
{
	if (space->size == 0) {
		/* Initially, size and flags will be set to 0,
		until the files are opened for the first time.
		fil_space_get_size() will open the file
		and adjust the size and flags. */
#ifdef UNIV_DEBUG
		ulint		size	=
#endif /* UNIV_DEBUG */
			fil_space_get_size(space->id);
		ut_ad(size == space->size);
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
@param[in,out]	node		Node to set */
void fil_no_punch_hole(fil_node_t* node);

#ifdef UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH
void test_make_filepath();
#endif /* UNIV_ENABLE_UNIT_TEST_MAKE_FILEPATH */

/** @return the system tablespace instance */
#define fil_space_get_sys_space() (fil_space_t::s_sys_space)

/** Write the open table (space_id -> name) mapping to disk */
void
fil_tablespace_open_sync_to_disk();

/** Parse or process a MLOG_FILE_* record.
@param[in]	ptr		redo log record
@param[in]	end		end of the redo log buffer
@param[in]	page_id		Tablespace Id and first page in file
@param[in]	type		MLOG_FILE_OPEN or MLOG_FILE_DELETE
				or MLOG_FILE_CREATE2 or MLOG_FILE_RENAME2
@param[in]	parsed_bytes	Number of bytes parsed so far
@return pointer to next redo log record
@retval NULL if this log record was truncated */
byte*
fil_tablespace_name_recover(
	byte*		ptr,
	const byte*	end,
	const page_id_t&page_id,
	mlog_id_t	type,
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
@param[in]	space_id	Tablespace ID to lookup */
void
fil_tablespace_open_for_recovery(space_id_t space_id);

/** Clear the tablspace ID to filename mapping. */
void
fil_tablespace_open_clear();

/** Create tablespaces.open.* files */
void
fil_tablespace_open_create();

/** Replay a file rename operation if possible.
@param[in]	page_id		Space ID and first page number in the file
@param[in]	name		old file name
@param[in]	new_name	new file name
@return	whether the operation was successfully applied
(the name did not exist, or new_name did not exist and
name was successfully renamed to new_name)  */
bool
fil_op_replay_rename(
	const page_id_t&	page_id,
	const char*		name,
	const char*		new_name);

/** Replay a file rename operation for ddl replay.
@param[in]	page_id		Space ID and first page number in the file
@param[in]	name		old file name
@param[in]	new_name	new file name
@return	whether the operation was successfully applied
(the name did not exist, or new_name did not exist and
name was successfully renamed to new_name)  */
bool
fil_op_replay_rename_for_ddl(
	const page_id_t&	page_id,
	const char*		name,
	const char*		new_name);

/** Rename a tablespace by its name only
@param[in]	old_name	old tablespace name
@param[in]	new_name	new tablespace name
@return DB_SUCCESS on success */
dberr_t
fil_rename_tablespace_by_name(
        const char*	old_name,
        const char*     new_name);

/** Update the tablespace name. Incase, the new name
and old name are same, no update done.
@param[in,out]	space		tablespace object on which name
				will be updated
@param[in]	name		new name for tablespace
@param[in]	has_fil_sys	true if fil_system mutex is
				acquired */
void
fil_space_update_name(
	fil_space_t*	space,
	const char*	name,
	bool		has_fil_sys);

#endif /* fil0fil_h */
