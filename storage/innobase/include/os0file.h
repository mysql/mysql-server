/***********************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/

/**************************************************//**
@file include/os0file.h
The interface to the operating system file io

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#ifndef os0file_h
#define os0file_h

#include "univ.i"

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#endif /* !_WIN32 */

/** File node of a tablespace or the log data space */
struct fil_node_t;

extern bool	os_has_said_disk_full;

/** Number of pending read operations */
extern ulint	os_n_pending_reads;
/** Number of pending write operations */
extern ulint	os_n_pending_writes;

/** File offset in bytes */
typedef ib_uint64_t os_offset_t;

#ifdef _WIN32

/**
Gets the operating system version. Currently works only on Windows.
@return OS_WIN95, OS_WIN31, OS_WINNT, OS_WIN2000, OS_WINXP, OS_WINVISTA,
OS_WIN7. */

ulint
os_get_os_version();

typedef HANDLE	os_file_dir_t;	/*!< directory stream */

/** We define always WIN_ASYNC_IO, and check at run-time whether
the OS actually supports it: Win 95 does not, NT does. */
# define WIN_ASYNC_IO

/** Use unbuffered I/O */
# define UNIV_NON_BUFFERED_IO

/** File handle */
# define os_file_t	HANDLE

/** Convert a C file descriptor to a native file handle
@param fd file descriptor
@return native file handle */
# define OS_FILE_FROM_FD(fd) (HANDLE) _get_osfhandle(fd)

#else /* _WIN32 */

typedef DIR*	os_file_dir_t;	/*!< directory stream */

/** File handle */
typedef int	os_file_t;

/** Convert a C file descriptor to a native file handle
@param fd file descriptor
@return native file handle */
# define OS_FILE_FROM_FD(fd) fd

#endif /* _WIN32 */

/** Common file descriptor for file IO instrumentation with PFS
on windows and other platforms */
struct pfs_os_file_t
{
	os_file_t   m_file;
#ifdef UNIV_PFS_IO
	struct PSI_file *m_psi;
#endif
};

static const os_file_t OS_FILE_CLOSED = os_file_t(~0);

/** The next value should be smaller or equal to the smallest sector size used
on any disk. A log block is required to be a portion of disk which is written
so that if the start and the end of a block get written to disk, then the
whole block gets written. This should be true even in most cases of a crash:
if this fails for a log block, then it is equivalent to a media failure in the
log. */

#define OS_FILE_LOG_BLOCK_SIZE		512

/** Options for os_file_create_func @{ */
enum os_file_create_t {
	OS_FILE_OPEN = 51,		/*!< to open an existing file (if
					doesn't exist, error) */
	OS_FILE_CREATE,			/*!< to create new file (if
					exists, error) */
	OS_FILE_OVERWRITE,		/*!< to create a new file, if exists
					the overwrite old file */
	OS_FILE_OPEN_RAW,		/*!< to open a raw device or disk
					partition */
	OS_FILE_CREATE_PATH,		/*!< to create the directories */
	OS_FILE_OPEN_RETRY,		/*!< open with retry */

	/** Flags that can be combined with the above values. Please ensure
	that the above values stay below 128. */

	OS_FILE_ON_ERROR_NO_EXIT = 128,	/*!< do not exit on unknown errors */
	OS_FILE_ON_ERROR_SILENT = 256	/*!< don't print diagnostic messages to
					the log unless it is a fatal error,
					this flag is only used if
					ON_ERROR_NO_EXIT is set */
};

static const ulint OS_FILE_READ_ONLY = 333;
static const ulint OS_FILE_READ_WRITE = 444;

/** Used by MySQLBackup */
static const ulint OS_FILE_READ_ALLOW_DELETE = 555;

/* Options for file_create */
static const ulint OS_FILE_AIO = 61;
static const ulint OS_FILE_NORMAL = 62;
/* @} */

/** Types for file create @{ */
static const ulint OS_DATA_FILE = 100;
static const ulint OS_LOG_FILE = 101;
static const ulint OS_DATA_TEMP_FILE = 102;
/* @} */

/** Error codes from os_file_get_last_error @{ */
static const ulint OS_FILE_NOT_FOUND = 71;
static const ulint OS_FILE_DISK_FULL = 72;
static const ulint OS_FILE_ALREADY_EXISTS = 73;
static const ulint OS_FILE_PATH_ERROR = 74;

/** wait for OS aio resources to become available again */
static const ulint OS_FILE_AIO_RESOURCES_RESERVED = 75;

static const ulint OS_FILE_SHARING_VIOLATION = 76;
static const ulint OS_FILE_ERROR_NOT_SPECIFIED = 77;
static const ulint OS_FILE_INSUFFICIENT_RESOURCE = 78;
static const ulint OS_FILE_AIO_INTERRUPTED = 79;
static const ulint OS_FILE_OPERATION_ABORTED = 80;
static const ulint OS_FILE_ACCESS_VIOLATION = 81;
static const ulint OS_FILE_ERROR_MAX = 100;
/* @} */

/** Compression algorithm. */
struct Compression {

	/** Algorithm types supported */
	enum Type {
		/* Note: During recovery we don't have the compression type
		because the .frm file has not been read yet. Therefore
		we write the recovered pages out without compression. */

		/** No compression */
		NONE = 0,

		/** Use ZLib */
		ZLIB = 1,

		/** Use LZ4 faster variant, usually lower compression. */
		LZ4 = 2
	};

	/** Compressed page meta-data */
	struct meta_t {

		/** Version number */
		uint8_t		m_version;

		/** Algorithm type */
		Type		m_algorithm;

		/** Original page type */
		uint16_t	m_original_type;

		/** Original page size, before compression */
		uint16_t	m_original_size;

		/** Size after compression */
		uint16_t	m_compressed_size;
	};

	/** Default constructor */
	Compression() : m_type(NONE) { };

	/** Specific constructor
	@param[in]	type		Algorithm type */
	explicit Compression(Type type)
		:
		m_type(type)
	{
#ifdef UNIV_DEBUG
		switch (m_type) {
		case NONE:
		case ZLIB:
		case LZ4:

		default:
			ut_error;
		}
#endif /* UNIV_DEBUG */
	}

	/** Check the page header type field.
	@param[in]	page		Page contents
	@return true if it is a compressed page */
	static bool is_compressed_page(const byte* page)
		MY_ATTRIBUTE((warn_unused_result));

        /** Check wether the compression algorithm is supported.
        @param[in]      algorithm       Compression algorithm to check
        @param[out]     type            The type that algorithm maps to
        @return DB_SUCCESS or error code */
	static dberr_t check(const char* algorithm, Compression* type)
		MY_ATTRIBUTE((warn_unused_result));

        /** Validate the algorithm string.
        @param[in]      algorithm       Compression algorithm to check
        @return DB_SUCCESS or error code */
	static dberr_t validate(const char* algorithm)
		MY_ATTRIBUTE((warn_unused_result));

        /** Convert to a "string".
        @param[in]      type            The compression type
        @return the string representation */
        static const char* to_string(Type type)
		MY_ATTRIBUTE((warn_unused_result));

        /** Convert the meta data to a std::string.
        @param[in]      meta		Page Meta data
        @return the string representation */
        static std::string to_string(const meta_t& meta)
		MY_ATTRIBUTE((warn_unused_result));

	/** Deserizlise the page header compression meta-data
	@param[in]	header		Pointer to the page header
	@param[out]	control		Deserialised data */
	static void deserialize_header(
		const byte*	page,
		meta_t*		control);

        /** Check if the string is "empty" or "none".
        @param[in]      algorithm       Compression algorithm to check
        @return true if no algorithm requested */
	static bool is_none(const char* algorithm)
		MY_ATTRIBUTE((warn_unused_result));

	/** Decompress the page data contents. Page type must be
	FIL_PAGE_COMPRESSED, if not then the source contents are
	left unchanged and DB_SUCCESS is returned.
	@param[in]	dblwr_recover	true of double write recovery
					in progress
	@param[in,out]	src		Data read from disk, decompressed
					data will be copied to this page
	@param[in,out]	dst		Scratch area to use for decompression
	@param[in]	dst_len		Size of the scratch area in bytes
	@return DB_SUCCESS or error code */
	static dberr_t deserialize(
		bool		dblwr_recover,
		byte*		src,
		byte*		dst,
		ulint		dst_len)
		MY_ATTRIBUTE((warn_unused_result));

	/** Compression type */
	Type		m_type;
};

/** Encryption key length */
static const ulint ENCRYPTION_KEY_LEN = 32;

/** Encryption magic bytes size */
static const ulint ENCRYPTION_MAGIC_SIZE = 3;

/** Encryption magic bytes for 5.7.11, it's for checking the encryption information
version. */
static const char ENCRYPTION_KEY_MAGIC_V1[] = "lCA";

/** Encryption magic bytes for 5.7.12+, it's for checking the encryption information
version. */
static const char ENCRYPTION_KEY_MAGIC_V2[] = "lCB";

/** Encryption master key prifix */
static const char ENCRYPTION_MASTER_KEY_PRIFIX[] = "INNODBKey";

/** Encryption master key prifix size */
static const ulint ENCRYPTION_MASTER_KEY_PRIFIX_LEN = 9;

/** Encryption master key prifix size */
static const ulint ENCRYPTION_MASTER_KEY_NAME_MAX_LEN = 100;

/** UUID of server instance, it's needed for composing master key name */
static const ulint ENCRYPTION_SERVER_UUID_LEN = 36;

/** Encryption information total size for 5.7.11: magic number + master_key_id +
key + iv + checksum */
static const ulint ENCRYPTION_INFO_SIZE_V1 = (ENCRYPTION_MAGIC_SIZE \
					 + (ENCRYPTION_KEY_LEN * 2) \
					 + 2 * sizeof(ulint));

/** Encryption information total size: magic number + master_key_id +
key + iv + server_uuid + checksum */
static const ulint ENCRYPTION_INFO_SIZE_V2 = (ENCRYPTION_MAGIC_SIZE \
					 + (ENCRYPTION_KEY_LEN * 2) \
					 + ENCRYPTION_SERVER_UUID_LEN \
					 + 2 * sizeof(ulint));

class IORequest;

/** Encryption algorithm. */
struct Encryption {

	/** Algorithm types supported */
	enum Type {

		/** No encryption */
		NONE = 0,

		/** Use AES */
		AES = 1,
	};

	/** Encryption information format version */
	enum Version {

		/** Version in 5.7.11 */
		ENCRYPTION_VERSION_1 = 0,

		/** Version in > 5.7.11 */
		ENCRYPTION_VERSION_2 = 1,
	};

	/** Default constructor */
	Encryption() : m_type(NONE) { };

	/** Specific constructor
	@param[in]	type		Algorithm type */
	explicit Encryption(Type type)
		:
		m_type(type)
	{
#ifdef UNIV_DEBUG
		switch (m_type) {
		case NONE:
		case AES:

		default:
			ut_error;
		}
#endif /* UNIV_DEBUG */
	}

	/** Copy constructor */
	Encryption(const Encryption& other)
		:
		m_type(other.m_type),
		m_key(other.m_key),
		m_klen(other.m_klen),
		m_iv(other.m_iv)
	{ };

	/** Check if page is encrypted page or not
	@param[in]	page	page which need to check
	@return true if it is a encrypted page */
	static bool is_encrypted_page(const byte* page)
		MY_ATTRIBUTE((warn_unused_result));

	/** Check the encryption option and set it
	@param[in]	option		encryption option
	@param[in/out]	encryption	The encryption type
	@return DB_SUCCESS or DB_UNSUPPORTED */
	dberr_t set_algorithm(const char* option, Encryption* type)
		MY_ATTRIBUTE((warn_unused_result));

        /** Validate the algorithm string.
        @param[in]      algorithm       Encryption algorithm to check
        @return DB_SUCCESS or error code */
	static dberr_t validate(const char* algorithm)
		MY_ATTRIBUTE((warn_unused_result));

        /** Convert to a "string".
        @param[in]      type            The encryption type
        @return the string representation */
        static const char* to_string(Type type)
		MY_ATTRIBUTE((warn_unused_result));

        /** Check if the string is "empty" or "none".
        @param[in]      algorithm       Encryption algorithm to check
        @return true if no algorithm requested */
	static bool is_none(const char* algorithm)
		MY_ATTRIBUTE((warn_unused_result));

        /** Generate random encryption value for key and iv.
        @param[in,out]	value	Encryption value */
	static void random_value(byte* value);

	/** Create new master key for key rotation.
        @param[in,out]	master_key	master key */
	static void create_master_key(byte** master_key);

        /** Get master key by key id.
        @param[in]	master_key_id	master key id
	@param[in]	srv_uuid	uuid of server instance
        @param[in,out]	master_key	master key */
	static void get_master_key(ulint master_key_id,
				   char* srv_uuid,
				   byte** master_key);

        /** Get current master key and key id.
        @param[in,out]	master_key_id	master key id
        @param[in,out]	master_key	master key
        @param[in,out]	version		encryption information version */
	static void get_master_key(ulint* master_key_id,
				   byte** master_key,
				   Encryption::Version*  version);

	/** Encrypt the page data contents. Page type can't be
	FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
	FIL_PAGE_ENCRYPTED_RTREE.
	@param[in]	type		IORequest
	@param[in,out]	src		page data which need to encrypt
	@param[in]	src_len		Size of the source in bytes
	@param[in,out]	dst		destination area
	@param[in,out]	dst_len		Size of the destination in bytes
	@return buffer data, dst_len will have the length of the data */
	byte* encrypt(
		const IORequest&	type,
		byte*			src,
		ulint			src_len,
		byte*			dst,
		ulint*			dst_len)
		MY_ATTRIBUTE((warn_unused_result));

	/** Decrypt the page data contents. Page type must be
	FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
	FIL_PAGE_ENCRYPTED_RTREE, if not then the source contents are
	left unchanged and DB_SUCCESS is returned.
	@param[in]	type		IORequest
	@param[in,out]	src		Data read from disk, decrypt
					data will be copied to this page
	@param[in]	src_len		source data length
	@param[in,out]	dst		Scratch area to use for decrypt
	@param[in]	dst_len		Size of the scratch area in bytes
	@return DB_SUCCESS or error code */
	dberr_t decrypt(
		const IORequest&	type,
		byte*			src,
		ulint			src_len,
		byte*			dst,
		ulint			dst_len)
		MY_ATTRIBUTE((warn_unused_result));

	/** Encrypt type */
	Type			m_type;

	/** Encrypt key */
	byte*			m_key;

	/** Encrypt key length*/
	ulint			m_klen;

	/** Encrypt initial vector */
	byte*			m_iv;

	/** Current master key id */
	static ulint		master_key_id;

	/** Current uuid of server instance */
	static char		uuid[ENCRYPTION_SERVER_UUID_LEN + 1];
};

/** Types for AIO operations @{ */

/** No transformations during read/write, write as is. */
#define IORequestRead		IORequest(IORequest::READ)
#define IORequestWrite		IORequest(IORequest::WRITE)
#define IORequestLogRead	IORequest(IORequest::LOG | IORequest::READ)
#define IORequestLogWrite	IORequest(IORequest::LOG | IORequest::WRITE)

/**
The IO Context that is passed down to the low level IO code */
class IORequest {
public:
	/** Flags passed in the request, they can be ORred together. */
	enum {
		READ = 1,
		WRITE = 2,

		/** Double write buffer recovery. */
		DBLWR_RECOVER = 4,

		/** Enumarations below can be ORed to READ/WRITE above*/

		/** Data file */
		DATA_FILE = 8,

		/** Log file request*/
		LOG = 16,

		/** Disable partial read warnings */
		DISABLE_PARTIAL_IO_WARNINGS = 32,

		/** Do not to wake i/o-handler threads, but the caller will do
		the waking explicitly later, in this way the caller can post
		several requests in a batch; NOTE that the batch must not be
		so big that it exhausts the slots in AIO arrays! NOTE that
		a simulated batch may introduce hidden chances of deadlocks,
		because I/Os are not actually handled until all
		have been posted: use with great caution! */
		DO_NOT_WAKE = 64,

		/** Ignore failed reads of non-existent pages */
		IGNORE_MISSING = 128,

		/** Use punch hole if available, only makes sense if
		compression algorithm != NONE. Ignored if not set */
		PUNCH_HOLE = 256,

		/** Force raw read, do not try to compress/decompress.
		This can be used to force a read and write without any
		compression e.g., for redo log, merge sort temporary files
		and the truncate redo log. */
		NO_COMPRESSION = 512
	};

	/** Default constructor */
	IORequest()
		:
		m_block_size(UNIV_SECTOR_SIZE),
		m_type(READ),
		m_compression(),
		m_encryption()
	{
		/* No op */
	}

	/**
	@param[in]	type		Request type, can be a value that is
					ORed from the above enum */
	explicit IORequest(ulint type)
		:
		m_block_size(UNIV_SECTOR_SIZE),
		m_type(static_cast<uint16_t>(type)),
		m_compression(),
		m_encryption()
	{
		if (is_log()) {
			disable_compression();
		}

		if (!is_punch_hole_supported()) {
			clear_punch_hole();
		}
	}

	/** Destructor */
	~IORequest() { }

	/** @return true if ignore missing flag is set */
	static bool ignore_missing(ulint type)
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((type & IGNORE_MISSING) == IGNORE_MISSING);
	}

	/** @return true if it is a read request */
	bool is_read() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & READ) == READ);
	}

	/** @return true if it is a write request */
	bool is_write() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & WRITE) == WRITE);
	}

	/** @return true if it is a redo log write */
	bool is_log() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & LOG) == LOG);
	}

	/** @return true if the simulated AIO thread should be woken up */
	bool is_wake() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & DO_NOT_WAKE) == 0);
	}

	/** @return true if partial read warning disabled */
	bool is_partial_io_warning_disabled() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & DISABLE_PARTIAL_IO_WARNINGS)
		       == DISABLE_PARTIAL_IO_WARNINGS);
	}

	/** Disable partial read warnings */
	void disable_partial_io_warnings()
	{
		m_type |= DISABLE_PARTIAL_IO_WARNINGS;
	}

	/** @return true if missing files should be ignored */
	bool ignore_missing() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(ignore_missing(m_type));
	}

	/** @return true if punch hole should be used */
	bool punch_hole() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & PUNCH_HOLE) == PUNCH_HOLE);
	}

	/** @return true if the read should be validated */
	bool validate() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_a(is_read() ^ is_write());

		return(!is_read() || !punch_hole());
	}

	/** Set the punch hole flag */
	void set_punch_hole()
	{
		if (is_punch_hole_supported()) {
			m_type |= PUNCH_HOLE;
		}
	}

	/** Clear the do not wake flag */
	void clear_do_not_wake()
	{
		m_type &= ~DO_NOT_WAKE;
	}

	/** Clear the punch hole flag */
	void clear_punch_hole()
	{
		m_type &= ~PUNCH_HOLE;
	}

	/** @return the block size to use for IO */
	ulint block_size() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(m_block_size);
	}

	/** Set the block size for IO
	@param[in] block_size		Block size to set */
	void block_size(ulint block_size)
	{
		m_block_size = static_cast<uint32_t>(block_size);
	}

	/** Clear all compression related flags */
	void clear_compressed()
	{
		clear_punch_hole();

		m_compression.m_type  = Compression::NONE;
	}

	/** Compare two requests
	@reutrn true if the are equal */
	bool operator==(const IORequest& rhs) const
	{
		return(m_type == rhs.m_type);
	}

	/** Set compression algorithm
	@param[in] compression	The compression algorithm to use */
	void compression_algorithm(Compression::Type type)
	{
		if (type == Compression::NONE) {
			return;
		}

		set_punch_hole();

		m_compression.m_type = type;
	}

	/** Get the compression algorithm.
	@return the compression algorithm */
	Compression compression_algorithm() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(m_compression);
	}

	/** @return true if the page should be compressed */
	bool is_compressed() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(compression_algorithm().m_type != Compression::NONE);
	}

	/** @return true if the page read should not be transformed. */
	bool is_compression_enabled() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & NO_COMPRESSION) == 0);
	}

	/** Disable transformations. */
	void disable_compression()
	{
		m_type |= NO_COMPRESSION;
	}

	/** Set encryption algorithm
	@param[in] type		The encryption algorithm to use */
	void encryption_algorithm(Encryption::Type type)
	{
		if (type == Encryption::NONE) {
			return;
		}

		m_encryption.m_type = type;
	}

	/** Set encryption key and iv
	@param[in] key		The encryption key to use
	@param[in] key_len	length of the encryption key
	@param[in] iv		The encryption iv to use */
	void encryption_key(byte* key,
			    ulint key_len,
			    byte* iv)
	{
		m_encryption.m_key = key;
		m_encryption.m_klen = key_len;
		m_encryption.m_iv = iv;
	}

	/** Get the encryption algorithm.
	@return the encryption algorithm */
	Encryption encryption_algorithm() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(m_encryption);
	}

	/** @return true if the page should be encrypted. */
	bool is_encrypted() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(m_encryption.m_type != Encryption::NONE);
	}

	/** Clear all encryption related flags */
	void clear_encrypted()
	{
		m_encryption.m_key = NULL;
		m_encryption.m_klen = 0;
		m_encryption.m_iv = NULL;
		m_encryption.m_type = Encryption::NONE;
	}

	/** Note that the IO is for double write recovery. */
	void dblwr_recover()
	{
		m_type |= DBLWR_RECOVER;
	}

	/** @return true if the request is from the dblwr recovery */
	bool is_dblwr_recover() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return((m_type & DBLWR_RECOVER) == DBLWR_RECOVER);
	}

	/** @return true if punch hole is supported */
	static bool is_punch_hole_supported()
	{

		/* In this debugging mode, we act as if punch hole is supported,
		and then skip any calls to actually punch a hole here.
		In this way, Transparent Page Compression is still being tested. */
		DBUG_EXECUTE_IF("ignore_punch_hole",
			return(true);
		);

#if defined(HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE) || defined(_WIN32)
		return(true);
#else
		return(false);
#endif /* HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE || _WIN32 */
	}

private:
	/* File system best block size */
	uint32_t		m_block_size;

	/** Request type bit flags */
	uint16_t		m_type;

	/** Compression algorithm */
	Compression		m_compression;

	/** Encryption algorithm */
	Encryption		m_encryption;
};

/* @} */

/** Sparse file size information. */
struct os_file_size_t {
	/** Total size of file in bytes */
	os_offset_t	m_total_size;

	/** If it is a sparse file then this is the number of bytes
	actually allocated for the file. */
	os_offset_t	m_alloc_size;
};

/** Win NT does not allow more than 64 */
static const ulint OS_AIO_N_PENDING_IOS_PER_THREAD = 32;

/** Modes for aio operations @{ */
/** Normal asynchronous i/o not for ibuf pages or ibuf bitmap pages */
static const ulint OS_AIO_NORMAL = 21;

/**  Asynchronous i/o for ibuf pages or ibuf bitmap pages */
static const ulint OS_AIO_IBUF = 22;

/** Asynchronous i/o for the log */
static const ulint OS_AIO_LOG = 23;

/** Asynchronous i/o where the calling thread will itself wait for
the i/o to complete, doing also the job of the i/o-handler thread;
can be used for any pages, ibuf or non-ibuf.  This is used to save
CPU time, as we can do with fewer thread switches. Plain synchronous
I/O is not as good, because it must serialize the file seek and read
or write, causing a bottleneck for parallelism. */
static const ulint OS_AIO_SYNC = 24;
/* @} */

extern ulint	os_n_file_reads;
extern ulint	os_n_file_writes;
extern ulint	os_n_fsyncs;

/* File types for directory entry data type */

enum os_file_type_t {
	OS_FILE_TYPE_UNKNOWN = 0,
	OS_FILE_TYPE_FILE,			/* regular file */
	OS_FILE_TYPE_DIR,			/* directory */
	OS_FILE_TYPE_LINK,			/* symbolic link */
	OS_FILE_TYPE_BLOCK			/* block device */
};

/* Maximum path string length in bytes when referring to tables with in the
'./databasename/tablename.ibd' path format; we can allocate at least 2 buffers
of this size from the thread stack; that is why this should not be made much
bigger than 4000 bytes.  The maximum path length used by any storage engine
in the server must be at least this big. */
#define OS_FILE_MAX_PATH	4000
#if (FN_REFLEN_SE < OS_FILE_MAX_PATH)
# error "(FN_REFLEN_SE < OS_FILE_MAX_PATH)"
#endif

/** Struct used in fetching information of a file in a directory */
struct os_file_stat_t {
	char		name[OS_FILE_MAX_PATH];	/*!< path to a file */
	os_file_type_t	type;			/*!< file type */
	os_offset_t	size;			/*!< file size in bytes */
	os_offset_t	alloc_size;		/*!< Allocated size for
						sparse files in bytes */
	size_t		block_size;		/*!< Block size to use for IO
						in bytes*/
	time_t		ctime;			/*!< creation time */
	time_t		mtime;			/*!< modification time */
	time_t		atime;			/*!< access time */
	bool		rw_perm;		/*!< true if can be opened
						in read-write mode. Only valid
						if type == OS_FILE_TYPE_FILE */
};

#ifndef UNIV_HOTBACKUP
/** Create a temporary file. This function is like tmpfile(3), but
the temporary file is created in the given parameter path. If the path
is null then it will create the file in the mysql server configuration
parameter (--tmpdir).
@param[in]	path	location for creating temporary file
@return temporary file handle, or NULL on error */
FILE*
os_file_create_tmpfile(
	const char*	path);
#endif /* !UNIV_HOTBACKUP */

/** The os_file_opendir() function opens a directory stream corresponding to the
directory named by the dirname argument. The directory stream is positioned
at the first entry. In both Unix and Windows we automatically skip the '.'
and '..' items at the start of the directory listing.

@param[in]	dirname		directory name; it must not contain a trailing
				'\' or '/'
@param[in]	is_fatal	true if we should treat an error as a fatal
				error; if we try to open symlinks then we do
				not wish a fatal error if it happens not to be
				a directory
@return directory stream, NULL if error */
os_file_dir_t
os_file_opendir(
	const char*	dirname,
	bool		is_fatal);

/**
Closes a directory stream.
@param[in] dir	directory stream
@return 0 if success, -1 if failure */
int
os_file_closedir(
	os_file_dir_t	dir);

/** This function returns information of the next file in the directory. We jump
over the '.' and '..' entries in the directory.
@param[in]	dirname		directory name or path
@param[in]	dir		directory stream
@param[out]	info		buffer where the info is returned
@return 0 if ok, -1 if error, 1 if at the end of the directory */
int
os_file_readdir_next_file(
	const char*	dirname,
	os_file_dir_t	dir,
	os_file_stat_t*	info);

/**
This function attempts to create a directory named pathname. The new directory
gets default permissions. On Unix, the permissions are (0770 & ~umask). If the
directory exists already, nothing is done and the call succeeds, unless the
fail_if_exists arguments is true.

@param[in]	pathname	directory name as null-terminated string
@param[in]	fail_if_exists	if true, pre-existing directory is treated
				as an error.
@return true if call succeeds, false on error */
bool
os_file_create_directory(
	const char*	pathname,
	bool		fail_if_exists);

/** NOTE! Use the corresponding macro os_file_create_simple(), not directly
this function!
A simple function to open or create a file.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY or OS_FILE_READ_WRITE
@param[in]	read_only	if true read only mode checks are enforced
@param[out]	success		true if succeed, false if error
@return own: handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_simple_func(
	const char*	name,
	ulint		create_mode,
	ulint		access_type,
	bool		read_only,
	bool*		success);

/** NOTE! Use the corresponding macro
os_file_create_simple_no_error_handling(), not directly this function!
A simple function to open or create a file.
@param[in]	name		name of the file or path as a null-terminated string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY, OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option
				is used by a backup program reading the file
@param[in]	read_only	if true read only mode checks are enforced
@param[out]	success		true if succeeded
@return own: handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_simple_no_error_handling_func(
	const char*	name,
	ulint		create_mode,
	ulint		access_type,
	bool		read_only,
	bool*		success)
	MY_ATTRIBUTE((warn_unused_result));

/** Tries to disable OS caching on an opened file descriptor.
@param[in]	fd		file descriptor to alter
@param[in]	file_name	file name, used in the diagnostic message
@param[in]	name		"open" or "create"; used in the diagnostic
				message */
void
os_file_set_nocache(
	int		fd,
	const char*	file_name,
	const char*	operation_name);

/** NOTE! Use the corresponding macro os_file_create(), not directly
this function!
Opens an existing file or creates a new.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	purpose		OS_FILE_AIO, if asynchronous, non-buffered I/O
				is desired, OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use
				async I/O or unbuffered I/O: look in the
				function source code for the exact rules
@param[in]	type		OS_DATA_FILE or OS_LOG_FILE
@param[in]	read_only	if true read only mode checks are enforced
@param[in]	success		true if succeeded
@return own: handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_func(
	const char*	name,
	ulint		create_mode,
	ulint		purpose,
	ulint		type,
	bool		read_only,
	bool*		success)
	MY_ATTRIBUTE((warn_unused_result));

/** Deletes a file. The file has to be closed before calling this.
@param[in]	name		file path as a null-terminated string
@return true if success */
bool
os_file_delete_func(const char* name);

/** Deletes a file if it exists. The file has to be closed before calling this.
@param[in]	name		file path as a null-terminated string
@param[out]	exist		indicate if file pre-exist
@return true if success */
bool
os_file_delete_if_exists_func(const char* name, bool* exist);

/** NOTE! Use the corresponding macro os_file_rename(), not directly
this function!
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@param[in]	oldpath		old file path as a null-terminated string
@param[in]	newpath		new file path
@return true if success */
bool
os_file_rename_func(const char* oldpath, const char* newpath);

/** NOTE! Use the corresponding macro os_file_close(), not directly this
function!
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@param[in]	file		own: handle to a file
@return true if success */
bool
os_file_close_func(os_file_t file);

#ifdef UNIV_PFS_IO

/* Keys to register InnoDB I/O with performance schema */
extern mysql_pfs_key_t	innodb_data_file_key;
extern mysql_pfs_key_t	innodb_log_file_key;
extern mysql_pfs_key_t	innodb_temp_file_key;

/* Following four macros are instumentations to register
various file I/O operations with performance schema.
1) register_pfs_file_open_begin() and register_pfs_file_open_end() are
used to register file creation, opening, closing and renaming.
2) register_pfs_file_rename_begin() and  register_pfs_file_rename_end()
are used to register file renaming
3) register_pfs_file_io_begin() and register_pfs_file_io_end() are
used to register actual file read, write and flush
3) register_pfs_file_close_begin() and register_pfs_file_close_end()
are used to register file deletion operations*/
# define register_pfs_file_open_begin(state, locker, key, op, name,	\
				      src_file, src_line)		\
do {									\
	locker = PSI_FILE_CALL(get_thread_file_name_locker)(		\
		state, key.m_value, op, name, &locker);			\
	if (locker != NULL) {						\
		PSI_FILE_CALL(start_file_open_wait)(			\
			locker, src_file, static_cast<uint>(src_line));	\
	}								\
} while (0)

# define register_pfs_file_open_end(locker, file, result)		\
do {									\
	if (locker != NULL) {						\
		file.m_psi = PSI_FILE_CALL(				\
		end_file_open_wait)(					\
			locker, result);				\
	}								\
} while (0)

# define register_pfs_file_rename_begin(state, locker, key, op, name,	\
					src_file, src_line)             \
	register_pfs_file_open_begin(					\
		state, locker, key, op, name,				\
		src_file, static_cast<uint>(src_line))			\

# define register_pfs_file_rename_end(locker, from, to, result)		\
do {									\
	if (locker != NULL) {						\
		 PSI_FILE_CALL(						\
			end_file_rename_wait)(				\
			locker, from, to, result);			\
	}								\
}while(0)

# define register_pfs_file_close_begin(state, locker, key, op, name,	\
				       src_file, src_line)		\
do {									\
	locker = PSI_FILE_CALL(get_thread_file_name_locker)(		\
		state, key.m_value, op, name, &locker);			\
	if (locker != NULL) {						\
		PSI_FILE_CALL(start_file_close_wait)(			\
			locker, src_file, static_cast<uint>(src_line));	\
	}								\
} while (0)

# define register_pfs_file_close_end(locker, result)			\
do {									\
	if (locker != NULL) {						\
		PSI_FILE_CALL(end_file_close_wait)(			\
			locker, result);				\
	}								\
} while (0)

# define register_pfs_file_io_begin(state, locker, file, count, op,	\
				    src_file, src_line)			\
do {									\
	locker = PSI_FILE_CALL(get_thread_file_stream_locker)(		\
		state, file.m_psi, op);					\
	if (locker != NULL) {						\
		PSI_FILE_CALL(start_file_wait)(				\
			locker, count,					\
			src_file, static_cast<uint>(src_line));		\
	}								\
} while (0)

# define register_pfs_file_io_end(locker, count)			\
do {									\
	if (locker != NULL) {						\
		PSI_FILE_CALL(end_file_wait)(locker, count);		\
	}								\
} while (0)

/* Following macros/functions are file I/O APIs that would be performance
schema instrumented if "UNIV_PFS_IO" is defined. They would point to
wrapper functions with performance schema instrumentation in such case.

os_file_create
os_file_create_simple
os_file_create_simple_no_error_handling
os_file_close
os_file_rename
os_aio
os_file_read
os_file_read_no_error_handling
os_file_read_no_error_handling_int_fd
os_file_write

The wrapper functions have the prefix of "innodb_". */

# define os_file_create(key, name, create, purpose, type, read_only,	\
			success)					\
	pfs_os_file_create_func(key, name, create, purpose,	type,	\
				read_only, success, __FILE__, __LINE__)

# define os_file_create_simple(key, name, create, access,		\
		read_only, success)					\
	pfs_os_file_create_simple_func(key, name, create, access,	\
		read_only, success, __FILE__, __LINE__)

# define os_file_create_simple_no_error_handling(			\
		key, name, create_mode, access, read_only, success)	\
	pfs_os_file_create_simple_no_error_handling_func(		\
		key, name, create_mode, access,				\
		read_only, success, __FILE__, __LINE__)

# define os_file_close_pfs(file)						\
	pfs_os_file_close_func(file, __FILE__, __LINE__)

# define os_aio(type, mode, name, file, buf, offset,			\
		n, read_only, message1, message2)			\
	pfs_os_aio_func(type, mode, name, file, buf, offset,		\
			n, read_only, message1, message2,		\
			__FILE__, __LINE__)

# define os_file_read_pfs(type, file, buf, offset, n)			\
	pfs_os_file_read_func(type, file, buf, offset, n, __FILE__, __LINE__)

# define os_file_read_no_error_handling_pfs(type, file, buf, offset, n, o)	\
	pfs_os_file_read_no_error_handling_func(			\
		type, file, buf, offset, n, o, __FILE__, __LINE__)

# define os_file_read_no_error_handling_int_fd(                         \
		type, file, buf, offset, n, o)				\
	pfs_os_file_read_no_error_handling_int_fd_func(                 \
		type, file, buf, offset, n, o, __FILE__, __LINE__)

# define os_file_write_pfs(type, name, file, buf, offset, n)	\
	pfs_os_file_write_func(type, name, file, buf, offset,	\
			       n, __FILE__, __LINE__)

# define os_file_write_int_fd(type, name, file, buf, offset, n)		\
	pfs_os_file_write_int_fd_func(type, name, file, buf, offset,	\
				n, __FILE__, __LINE__)

# define os_file_flush_pfs(file)						\
	pfs_os_file_flush_func(file, __FILE__, __LINE__)

# define os_file_rename(key, oldpath, newpath)				\
	pfs_os_file_rename_func(key, oldpath, newpath, __FILE__, __LINE__)

# define os_file_delete(key, name)					\
	pfs_os_file_delete_func(key, name, __FILE__, __LINE__)

# define os_file_delete_if_exists(key, name, exist)			\
	pfs_os_file_delete_if_exists_func(key, name, exist, __FILE__, __LINE__)



/** NOTE! Please use the corresponding macro os_file_create_simple(),
not directly this function!
A performance schema instrumented wrapper function for
os_file_create_simple() which opens or creates a file.
@param[in]	key		Performance Schema Key
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY or OS_FILE_READ_WRITE
@param[in]	read_only	if true read only mode checks are enforced
@param[out]	success		true if succeeded
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return own: handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
UNIV_INLINE
pfs_os_file_t
pfs_os_file_create_simple_func(
	mysql_pfs_key_t key,
	const char*	name,
	ulint		create_mode,
	ulint		access_type,
	bool		read_only,
	bool*		success,
	const char*	src_file,
	ulint		src_line)
	MY_ATTRIBUTE((warn_unused_result));

/** NOTE! Please use the corresponding macro
os_file_create_simple_no_error_handling(), not directly this function!
A performance schema instrumented wrapper function for
os_file_create_simple_no_error_handling(). Add instrumentation to
monitor file creation/open.
@param[in]	key		Performance Schema Key
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY, OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file
@param[in]	read_only	if true read only mode checks are enforced
@param[out]	success		true if succeeded
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return own: handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
UNIV_INLINE
pfs_os_file_t
pfs_os_file_create_simple_no_error_handling_func(
	mysql_pfs_key_t key,
	const char*	name,
	ulint		create_mode,
	ulint		access_type,
	bool		read_only,
	bool*		success,
	const char*	src_file,
	ulint		src_line)
	MY_ATTRIBUTE((warn_unused_result));

/** NOTE! Please use the corresponding macro os_file_create(), not directly
this function!
A performance schema wrapper function for os_file_create().
Add instrumentation to monitor file creation/open.
@param[in]	key		Performance Schema Key
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	purpose		OS_FILE_AIO, if asynchronous, non-buffered I/O
				is desired, OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use
				async I/O or unbuffered I/O: look in the
				function source code for the exact rules
@param[in]	read_only	if true read only mode checks are enforced
@param[out]	success		true if succeeded
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return own: handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
UNIV_INLINE
pfs_os_file_t
pfs_os_file_create_func(
	mysql_pfs_key_t key,
	const char*	name,
	ulint		create_mode,
	ulint		purpose,
	ulint		type,
	bool		read_only,
	bool*		success,
	const char*	src_file,
	ulint		src_line)
	MY_ATTRIBUTE((warn_unused_result));

/** NOTE! Please use the corresponding macro os_file_close(), not directly
this function!
A performance schema instrumented wrapper function for os_file_close().
@param[in]	file		handle to a file
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return true if success */
UNIV_INLINE
bool
pfs_os_file_close_func(
	pfs_os_file_t	file,
	const char*	src_file,
	ulint		src_line);

/** NOTE! Please use the corresponding macro os_file_read(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_read() which requests a synchronous read operation.
@param[in, out]	type		IO request context
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return DB_SUCCESS if request was successful */
UNIV_INLINE
dberr_t
pfs_os_file_read_func(
	IORequest&	type,
	pfs_os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n,
	const char*	src_file,
	ulint		src_line);

/** NOTE! Please use the corresponding macro os_file_read_no_error_handling(),
not directly this function!
This is the performance schema instrumented wrapper function for
os_file_read_no_error_handling_func() which requests a synchronous
read operation.
@param[in, out]	type		IO request context
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@param[out]	o		number of bytes actually read
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return DB_SUCCESS if request was successful */
UNIV_INLINE
dberr_t
pfs_os_file_read_no_error_handling_func(
	IORequest&	type,
	pfs_os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n,
	ulint*		o,
	const char*	src_file,
	ulint		src_line);

/** NOTE! Please use the corresponding macro
os_file_read_no_error_handling_int_fd(), not directly this function!
This is the performance schema instrumented wrapper function for
os_file_read_no_error_handling_int_fd_func() which requests a
synchronous  read operation on files with int type descriptors.
@param[in, out] type            IO request context
@param[in]      file            Open file handle
@param[out]     buf             buffer where to read
@param[in]      offset          file offset where to read
@param[in]      n               number of bytes to read
@param[out]     o               number of bytes actually read
@param[in]      src_file        file name where func invoked
@param[in]      src_line        line where the func invoked
@return DB_SUCCESS if request was successful */

UNIV_INLINE
dberr_t
pfs_os_file_read_no_error_handling_int_fd_func(
        IORequest&      type,
        int             file,
        void*           buf,
        os_offset_t     offset,
        ulint           n,
        ulint*          o,
        const char*     src_file,
        ulint           src_line);

/** NOTE! Please use the corresponding macro os_aio(), not directly this
function!
Performance schema wrapper function of os_aio() which requests
an asynchronous I/O operation.
@param[in]	type		IO request context
@param[in]	mode		IO mode
@param[in]	name		Name of the file or path as NUL terminated
				string
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@param[in]	read_only	if true read only mode checks are enforced
@param[in,out]	m1		Message for the AIO handler, (can be used to
				identify a completed AIO operation); ignored
				if mode is OS_AIO_SYNC
@param[in,out]	m2		message for the AIO handler (can be used to
				identify a completed AIO operation); ignored
				if mode is OS_AIO_SYNC
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return DB_SUCCESS if request was queued successfully, FALSE if fail */
UNIV_INLINE
dberr_t
pfs_os_aio_func(
	IORequest&	type,
	ulint		mode,
	const char*	name,
	pfs_os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n,
	bool		read_only,
	fil_node_t*	m1,
	void*		m2,
	const char*	src_file,
	ulint		src_line);

/** NOTE! Please use the corresponding macro os_file_write(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_write() which requests a synchronous write operation.
@param[in, out]	type		IO request context
@param[in]	name		Name of the file or path as NUL terminated
				string
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return DB_SUCCESS if request was successful */
UNIV_INLINE
dberr_t
pfs_os_file_write_func(
	IORequest&	type,
	const char*	name,
	pfs_os_file_t	file,
	const void*	buf,
	os_offset_t	offset,
	ulint		n,
	const char*	src_file,
	ulint		src_line);

/** NOTE! Please use the corresponding macro os_file_write(), not
directly this function!
This is the performance schema instrumented wrapper function for
os_file_write() which requests a synchronous write operation
on files with int type descriptors.
@param[in, out] type            IO request context
@param[in]      name            Name of the file or path as NUL terminated
				string
@param[in]      file            Open file handle
@param[out]     buf             buffer where to read
@param[in]      offset          file offset where to read
@param[in]      n		number of bytes to read
@param[in]      src_file        file name where func invoked
@param[in]      src_line        line where the func invoked
@return DB_SUCCESS if request was successful */
UNIV_INLINE
dberr_t
pfs_os_file_write_int_fd_func(
        IORequest&      type,
        const char*     name,
        int		file,
        const void*     buf,
        os_offset_t     offset,
        ulint           n,
        const char*     src_file,
        ulint           src_line);

/** NOTE! Please use the corresponding macro os_file_flush(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_flush() which flushes the write buffers of a given file to the disk.
Flushes the write buffers of a given file to the disk.
@param[in]	file		Open file handle
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return TRUE if success */
UNIV_INLINE
bool
pfs_os_file_flush_func(
	pfs_os_file_t	file,
	const char*	src_file,
	ulint		src_line);

/** NOTE! Please use the corresponding macro os_file_rename(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_rename()
@param[in]	key		Performance Schema Key
@param[in]	oldpath		old file path as a null-terminated string
@param[in]	newpath		new file path
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return true if success */
UNIV_INLINE
bool
pfs_os_file_rename_func(
	mysql_pfs_key_t	key,
	const char*	oldpath,
	const char*	newpath,
	const char*	src_file,
	ulint		src_line);

/**
NOTE! Please use the corresponding macro os_file_delete(), not directly
this function!
This is the performance schema instrumented wrapper function for
os_file_delete()
@param[in]	key		Performance Schema Key
@param[in]	name		old file path as a null-terminated string
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return true if success */
UNIV_INLINE
bool
pfs_os_file_delete_func(
	mysql_pfs_key_t	key,
	const char*	name,
	const char*	src_file,
	ulint		src_line);

/**
NOTE! Please use the corresponding macro os_file_delete_if_exists(), not
directly this function!
This is the performance schema instrumented wrapper function for
os_file_delete_if_exists()
@param[in]	key		Performance Schema Key
@param[in]	name		old file path as a null-terminated string
@param[in]	exist		indicate if file pre-exist
@param[in]	src_file	file name where func invoked
@param[in]	src_line	line where the func invoked
@return true if success */
UNIV_INLINE
bool
pfs_os_file_delete_if_exists_func(
	mysql_pfs_key_t	key,
	const char*	name,
	bool*		exist,
	const char*	src_file,
	ulint		src_line);

#else /* UNIV_PFS_IO */

/* If UNIV_PFS_IO is not defined, these I/O APIs point
to original un-instrumented file I/O APIs */
# define os_file_create(key, name, create, purpose, type, read_only,	\
			success)					\
	os_file_create_func(name, create, purpose, type, read_only,	\
			success)

# define os_file_create_simple(key, name, create_mode, access,		\
		read_only, success)					\
	os_file_create_simple_func(name, create_mode, access,		\
		read_only, success)

# define os_file_create_simple_no_error_handling(			\
		key, name, create_mode, access, read_only, success)	\
	os_file_create_simple_no_error_handling_func(			\
		name, create_mode, access, read_only, success)

# define os_file_close_pfs(file)	os_file_close_func(file)

# define os_aio(type, mode, name, file, buf, offset,			\
		n, read_only, message1, message2)			\
	os_aio_func(type, mode, name, file, buf, offset,		\
		n, read_only, message1, message2)

# define os_file_read_pfs(type, file, buf, offset, n)			\
	os_file_read_func(type, file, buf, offset, n)

# define os_file_read_no_error_handling_pfs(type, file, buf, offset, n, o)	\
	os_file_read_no_error_handling_func(type, file, buf, offset, n, o)

# define os_file_read_no_error_handling_int_fd(type, file, buf, offset, n, o)  \
	os_file_read_no_error_handling_func(type, file, buf, offset, n, o)

# define os_file_write_pfs(type, name, file, buf, offset, n)		\
	os_file_write_func(type, name, file, buf, offset, n)

# define os_file_write_int_fd(type, name, file, buf, offset, n)            \
	os_file_write_func(type, name, file, buf, offset, n)

# define os_file_flush_pfs(file)	os_file_flush_func(file)

# define os_file_rename(key, oldpath, newpath)				\
	os_file_rename_func(oldpath, newpath)

# define os_file_delete(key, name)	os_file_delete_func(name)

# define os_file_delete_if_exists(key, name, exist)			\
	os_file_delete_if_exists_func(name, exist)

#endif	/* UNIV_PFS_IO */

#ifdef UNIV_PFS_IO
	#define os_file_close(file) os_file_close_pfs(file)
#else
	#define os_file_close(file) os_file_close_pfs((file).m_file)
#endif

#ifdef UNIV_PFS_IO
	#define os_file_read(type, file, buf, offset, n)                \
		os_file_read_pfs(type, file, buf, offset, n)
#else
	#define os_file_read(type, file, buf, offset, n)                \
                os_file_read_pfs(type, file.m_file, buf, offset, n)
#endif

#ifdef UNIV_PFS_IO
	#define os_file_flush(file)	os_file_flush_pfs(file)
#else
	#define os_file_flush(file)	os_file_flush_pfs(file.m_file)
#endif

#ifdef UNIV_PFS_IO
	#define os_file_write(type, name, file, buf, offset, n)         \
		os_file_write_pfs(type, name, file, buf, offset, n)
#else
	#define os_file_write(type, name, file, buf, offset, n)         \
                os_file_write_pfs(type, name, file.m_file, buf, offset, n)
#endif

#ifdef UNIV_PFS_IO
	#define os_file_read_no_error_handling(type, file, buf, offset, n, o)  \
		 os_file_read_no_error_handling_pfs(type, file, buf, offset, n, o)
#else
	#define os_file_read_no_error_handling(type, file, buf, offset, n, o) \
                 os_file_read_no_error_handling_pfs(			      \
			type, file.m_file, buf, offset, n, o)
#endif

#ifdef UNIV_HOTBACKUP
/** Closes a file handle.
@param[in] file		handle to a file
@return true if success */
bool
os_file_close_no_error_handling(os_file_t file);
#endif /* UNIV_HOTBACKUP */

/** Gets a file size.
@param[in]	file		handle to a file
@return file size if OK, else set m_total_size to ~0 and m_alloc_size
	to errno */
os_file_size_t
os_file_get_size(
	const char*	filename)
	MY_ATTRIBUTE((warn_unused_result));

/** Gets a file size.
@param[in]	file		handle to a file
@return file size, or (os_offset_t) -1 on failure */
os_offset_t
os_file_get_size(
	pfs_os_file_t	file)
	MY_ATTRIBUTE((warn_unused_result));

/** Write the specified number of zeros to a newly created file.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	file		handle to a file
@param[in]	size		file size
@param[in]	read_only	Enable read-only checks if true
@return true if success */
bool
os_file_set_size(
	const char*	name,
	pfs_os_file_t	file,
	os_offset_t	size,
	bool		read_only)
	MY_ATTRIBUTE((warn_unused_result));

/** Truncates a file at its current position.
@param[in/out]	file	file to be truncated
@return true if success */
bool
os_file_set_eof(
	FILE*		file);	/*!< in: file to be truncated */

/** Truncates a file to a specified size in bytes. Do nothing if the size
preserved is smaller or equal than current size of file.
@param[in]	pathname	file path
@param[in]	file		file to be truncated
@param[in]	size		size preserved in bytes
@return true if success */
bool
os_file_truncate(
	const char*	pathname,
	pfs_os_file_t	file,
	os_offset_t	size);

/** NOTE! Use the corresponding macro os_file_flush(), not directly this
function!
Flushes the write buffers of a given file to the disk.
@param[in]	file		handle to a file
@return true if success */
bool
os_file_flush_func(
	os_file_t	file);

/** Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@param[in]	report		true if we want an error message printed
				for all errors
@return error number, or OS error number + 100 */
ulint
os_file_get_last_error(
	bool		report);

/** NOTE! Use the corresponding macro os_file_read(), not directly this
function!
Requests a synchronous read operation.
@param[in]	type		IO request context
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@return DB_SUCCESS if request was successful */
dberr_t
os_file_read_func(
	IORequest&	type,
	os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n)
	MY_ATTRIBUTE((warn_unused_result));

/** Rewind file to its start, read at most size - 1 bytes from it to str, and
NUL-terminate str. All errors are silently ignored. This function is
mostly meant to be used with temporary files.
@param[in,out]	file		file to read from
@param[in,out]	str		buffer where to read
@param[in]	size		size of buffer */
void
os_file_read_string(
	FILE*		file,
	char*		str,
	ulint		size);

/** NOTE! Use the corresponding macro os_file_read_no_error_handling(),
not directly this function!
Requests a synchronous positioned read operation. This function does not do
any error handling. In case of error it returns FALSE.
@param[in]	type		IO request context
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@param[out]	o		number of bytes actually read
@return DB_SUCCESS or error code */
dberr_t
os_file_read_no_error_handling_func(
	IORequest&	type,
	os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n,
	ulint*		o)
	MY_ATTRIBUTE((warn_unused_result));

/** NOTE! Use the corresponding macro os_file_write(), not directly this
function!
Requests a synchronous write operation.
@param[in,out]	type		IO request context
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@return DB_SUCCESS if request was successful */
dberr_t
os_file_write_func(
	IORequest&	type,
	const char*	name,
	os_file_t	file,
	const void*	buf,
	os_offset_t	offset,
	ulint		n)
	MY_ATTRIBUTE((warn_unused_result));

/** Check the existence and type of the given file.
@param[in]	path		pathname of the file
@param[out]	exists		true if file exists
@param[out]	type		type of the file (if it exists)
@return true if call succeeded */
bool
os_file_status(
	const char*	path,
	bool*		exists,
	os_file_type_t* type);

/** This function returns a new path name after replacing the basename
in an old path with a new basename.  The old_path is a full path
name including the extension.  The tablename is in the normal
form "databasename/tablename".  The new base name is found after
the forward slash.  Both input strings are null terminated.

This function allocates memory to be returned.  It is the callers
responsibility to free the return value after it is no longer needed.

@param[in]	old_path		pathname
@param[in]	new_name		new file name
@return own: new full pathname */
char*
os_file_make_new_pathname(
	const char*	old_path,
	const char*	new_name);

/** This function reduces a null-terminated full remote path name into
the path that is sent by MySQL for DATA DIRECTORY clause.  It replaces
the 'databasename/tablename.ibd' found at the end of the path with just
'tablename'.

Since the result is always smaller than the path sent in, no new memory
is allocated. The caller should allocate memory for the path sent in.
This function manipulates that path in place.

If the path format is not as expected, just return.  The result is used
to inform a SHOW CREATE TABLE command.
@param[in,out]	data_dir_path		Full path/data_dir_path */
void
os_file_make_data_dir_path(
	char*	data_dir_path);

/** Create all missing subdirectories along the given path.
@return DB_SUCCESS if OK, otherwise error code. */
dberr_t
os_file_create_subdirs_if_needed(
	const char*	path);

#ifdef UNIV_ENABLE_UNIT_TEST_GET_PARENT_DIR
/* Test the function os_file_get_parent_dir. */
void
unit_test_os_file_get_parent_dir();
#endif /* UNIV_ENABLE_UNIT_TEST_GET_PARENT_DIR */

/** Initializes the asynchronous io system. Creates one array each for ibuf
and log i/o. Also creates one array each for read and write where each
array is divided logically into n_read_segs and n_write_segs
respectively. The caller must create an i/o handler thread for each
segment in these arrays. This function also creates the sync array.
No i/o handler thread needs to be created for that
@param[in]	n_read_segs	number of reader threads
@param[in]	n_write_segs	number of writer threads
@param[in]	n_slots_sync	number of slots in the sync aio array */

bool
os_aio_init(
	ulint		n_read_segs,
	ulint		n_write_segs,
	ulint		n_slots_sync);

/**
Frees the asynchronous io system. */
void
os_aio_free();

/**
NOTE! Use the corresponding macro os_aio(), not directly this function!
Requests an asynchronous i/o operation.
@param[in]	type		IO request context
@param[in]	mode		IO mode
@param[in]	name		Name of the file or path as NUL terminated
				string
@param[in]	file		Open file handle
@param[out]	buf		buffer where to read
@param[in]	offset		file offset where to read
@param[in]	n		number of bytes to read
@param[in]	read_only	if true read only mode checks are enforced
@param[in,out]	m1		Message for the AIO handler, (can be used to
				identify a completed AIO operation); ignored
				if mode is OS_AIO_SYNC
@param[in,out]	m2		message for the AIO handler (can be used to
				identify a completed AIO operation); ignored
				if mode is OS_AIO_SYNC
@return DB_SUCCESS or error code */
dberr_t
os_aio_func(
	IORequest&	type,
	ulint		mode,
	const char*	name,
	pfs_os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n,
	bool		read_only,
	fil_node_t*	m1,
	void*		m2);

/** Wakes up all async i/o threads so that they know to exit themselves in
shutdown. */
void
os_aio_wake_all_threads_at_shutdown();

/** Waits until there are no pending writes in os_aio_write_array. There can
be other, synchronous, pending writes. */
void
os_aio_wait_until_no_pending_writes();

/** Wakes up simulated aio i/o-handler threads if they have something to do. */
void
os_aio_simulated_wake_handler_threads();

/** This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
void
os_aio_simulated_put_read_threads_to_sleep();

/** This is the generic AIO handler interface function.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The AIO array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@param[in]	segment		the number of the segment in the aio arrays to
				wait for; segment 0 is the ibuf I/O thread,
				segment 1 the log I/O thread, then follow the
				non-ibuf read threads, and as the last are the
				non-ibuf write threads; if this is
				ULINT_UNDEFINED, then it means that sync AIO
				is used, and this parameter is ignored
@param[out]	m1		the messages passed with the AIO request;
				note that also in the case where the AIO
				operation failed, these output parameters
				are valid and can be used to restart the
				operation, for example
@param[out]	m2		callback message
@param[out]	type		OS_FILE_WRITE or ..._READ
@return DB_SUCCESS or error code */
dberr_t
os_aio_handler(
	ulint		segment,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	type);

/** Prints info of the aio arrays.
@param[in/out]	file		file where to print */
void
os_aio_print(FILE* file);

/** Refreshes the statistics used to print per-second averages. */
void
os_aio_refresh_stats();

/** Checks that all slots in the system have been freed, that is, there are
no pending io operations. */
bool
os_aio_all_slots_free();

#ifdef UNIV_DEBUG

/** Prints all pending IO
@param[in]	file	file where to print */
void
os_aio_print_pending_io(FILE* file);

#endif /* UNIV_DEBUG */

/** This function returns information about the specified file
@param[in]	path		pathname of the file
@param[in]	stat_info	information of a file in a directory
@param[in]	check_rw_perm	for testing whether the file can be opened
				in RW mode
@param[in]	read_only	if true read only mode checks are enforced
@return DB_SUCCESS if all OK */
dberr_t
os_file_get_status(
	const char*	path,
	os_file_stat_t* stat_info,
	bool		check_rw_perm,
	bool		read_only);

#if !defined(UNIV_HOTBACKUP)
/** Creates a temporary file in the location specified by the parameter
path. If the path is NULL then it will be created on --tmpdir location.
This function is defined in ha_innodb.cc.
@param[in]	path	location for creating temporary file
@return temporary file descriptor, or < 0 on error */
int
innobase_mysql_tmpfile(
	const char*	path);
#endif /* !UNIV_HOTBACKUP */


/** If it is a compressed page return the compressed page data + footer size
@param[in]	buf		Buffer to check, must include header + 10 bytes
@return ULINT_UNDEFINED if the page is not a compressed page or length
	of the compressed data (including footer) if it is a compressed page */
ulint
os_file_compressed_page_size(const byte* buf);

/** If it is a compressed page return the original page data + footer size
@param[in]	buf		Buffer to check, must include header + 10 bytes
@return ULINT_UNDEFINED if the page is not a compressed page or length
	of the original data + footer if it is a compressed page */
ulint
os_file_original_page_size(const byte* buf);

/** Set the file create umask
@param[in]	umask		The umask to use for file creation. */
void
os_file_set_umask(ulint umask);

/** Free storage space associated with a section of the file.
@param[in]	fh		Open file handle
@param[in]	off		Starting offset (SEEK_SET)
@param[in]	len		Size of the hole
@return DB_SUCCESS or error code */
dberr_t
os_file_punch_hole(
	os_file_t	fh,
	os_offset_t	off,
	os_offset_t	len)
	MY_ATTRIBUTE((warn_unused_result));

/** Check if the file system supports sparse files.

Warning: On POSIX systems we try and punch a hole from offset 0 to
the system configured page size. This should only be called on an empty
file.

Note: On Windows we use the name and on Unices we use the file handle.

@param[in]	name		File name
@param[in]	fh		File handle for the file - if opened
@return true if the file system supports sparse files */
bool
os_is_sparse_file_supported(
	const char*	path,
	pfs_os_file_t	fh)
	MY_ATTRIBUTE((warn_unused_result));

/** Decompress the page data contents. Page type must be FIL_PAGE_COMPRESSED, if
not then the source contents are left unchanged and DB_SUCCESS is returned.
@param[in]	dblwr_recover	true of double write recovery in progress
@param[in,out]	src		Data read from disk, decompressed data will be
				copied to this page
@param[in,out]	dst		Scratch area to use for decompression
@param[in]	dst_len		Size of the scratch area in bytes
@return DB_SUCCESS or error code */

dberr_t
os_file_decompress_page(
	bool		dblwr_recover,
	byte*		src,
	byte*		dst,
	ulint		dst_len)
	MY_ATTRIBUTE((warn_unused_result));

/** Normalizes a directory path for the current OS:
On Windows, we convert '/' to '\', else we convert '\' to '/'.
@param[in,out] str A null-terminated directory and file path */
void os_normalize_path(char*	str);

/* Determine if a path is an absolute path or not.
@param[in]	OS directory or file path to evaluate
@retval true if an absolute path
@retval false if a relative path */
UNIV_INLINE
bool
is_absolute_path(
	const char*	path)
{
	if (path[0] == OS_PATH_SEPARATOR) {
		return(true);
	}

#ifdef _WIN32
	if (path[1] == ':' && path[2] == OS_PATH_SEPARATOR) {
		return(true);
	}
#endif /* _WIN32 */

	return(false);
}

#ifndef UNIV_NONINL
#include "os0file.ic"
#endif /* UNIV_NONINL */

#endif /* os0file_h */
