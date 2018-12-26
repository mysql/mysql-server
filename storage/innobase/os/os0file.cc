/***********************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
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
@file os/os0file.cc
The interface to the operating system file i/o primitives

Created 10/21/1995 Heikki Tuuri
*******************************************************/

#ifndef UNIV_INNOCHECKSUM

#include "ha_prototypes.h"
#include "sql_const.h"

#include "os0file.h"

#ifdef UNIV_NONINL
#include "os0file.ic"
#endif

#include "srv0srv.h"
#include "srv0start.h"
#include "fil0fil.h"
#ifndef UNIV_HOTBACKUP
# include "os0event.h"
# include "os0thread.h"
#else /* !UNIV_HOTBACKUP */
# ifdef _WIN32
/* Add includes for the _stat() call to compile on Windows */
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <errno.h>
# endif /* _WIN32 */
#endif /* !UNIV_HOTBACKUP */

#include <vector>
#include <functional>

#ifdef LINUX_NATIVE_AIO
#include <libaio.h>
#endif /* LINUX_NATIVE_AIO */

#ifdef HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
# include <fcntl.h>
# include <linux/falloc.h>
#endif /* HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE */

#include <lz4.h>
#include <zlib.h>

#ifdef UNIV_DEBUG
/** Set when InnoDB has invoked exit(). */
bool	innodb_calling_exit;
#endif /* UNIV_DEBUG */

#include <my_aes.h>
#include <my_rnd.h>
#include <mysqld.h>
#include <mysql/service_mysql_keyring.h>

/** Insert buffer segment id */
static const ulint IO_IBUF_SEGMENT = 0;

/** Log segment id */
static const ulint IO_LOG_SEGMENT = 1;

/** Number of retries for partial I/O's */
static const ulint NUM_RETRIES_ON_PARTIAL_IO = 10;

/** Blocks for doing IO, used in the transparent compression
and encryption code. */
struct Block {
	/** Default constructor */
	Block() : m_ptr(), m_in_use() { }

	byte*		m_ptr;

	byte		pad[CACHE_LINE_SIZE - sizeof(ulint)];
	lock_word_t	m_in_use;
};

/** For storing the allocated blocks */
typedef std::vector<Block> Blocks;

/** Block collection */
static Blocks*	block_cache;

/** Number of blocks to allocate for sync read/writes */
static const size_t	MAX_BLOCKS = 128;

/** Block buffer size */
#define BUFFER_BLOCK_SIZE ((ulint)(UNIV_PAGE_SIZE * 1.3))

/** Disk sector size of aligning write buffer for DIRECT_IO */
static ulint	os_io_ptr_align = UNIV_SECTOR_SIZE;

/* This specifies the file permissions InnoDB uses when it creates files in
Unix; the value of os_innodb_umask is initialized in ha_innodb.cc to
my_umask */

#ifndef _WIN32
/** Umask for creating files */
static ulint	os_innodb_umask = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
#else
/** Umask for creating files */
static ulint	os_innodb_umask	= 0;

/* On Windows when using native AIO the number of AIO requests
that a thread can handle at a given time is limited to 32
i.e.: SRV_N_PENDING_IOS_PER_THREAD */
#define SRV_N_PENDING_IOS_PER_THREAD    OS_AIO_N_PENDING_IOS_PER_THREAD

#endif /* _WIN32 */

#ifndef UNIV_HOTBACKUP

/** In simulated aio, merge at most this many consecutive i/os */
static const ulint	OS_AIO_MERGE_N_CONSECUTIVE = 64;

/** Flag indicating if the page_cleaner is in active state. */
extern bool buf_page_cleaner_is_active;

/**********************************************************************

InnoDB AIO Implementation:
=========================

We support native AIO for Windows and Linux. For rest of the platforms
we simulate AIO by special IO-threads servicing the IO-requests.

Simulated AIO:
==============

On platforms where we 'simulate' AIO, the following is a rough explanation
of the high level design.
There are four io-threads (for ibuf, log, read, write).
All synchronous IO requests are serviced by the calling thread using
os_file_write/os_file_read. The Asynchronous requests are queued up
in an array (there are four such arrays) by the calling thread.
Later these requests are picked up by the IO-thread and are serviced
synchronously.

Windows native AIO:
==================

If srv_use_native_aio is not set then Windows follow the same
code as simulated AIO. If the flag is set then native AIO interface
is used. On windows, one of the limitation is that if a file is opened
for AIO no synchronous IO can be done on it. Therefore we have an
extra fifth array to queue up synchronous IO requests.
There are innodb_file_io_threads helper threads. These threads work
on the four arrays mentioned above in Simulated AIO. No thread is
required for the sync array.
If a synchronous IO request is made, it is first queued in the sync
array. Then the calling thread itself waits on the request, thus
making the call synchronous.
If an AIO request is made the calling thread not only queues it in the
array but also submits the requests. The helper thread then collects
the completed IO request and calls completion routine on it.

Linux native AIO:
=================

If we have libaio installed on the system and innodb_use_native_aio
is set to true we follow the code path of native AIO, otherwise we
do simulated AIO.
There are innodb_file_io_threads helper threads. These threads work
on the four arrays mentioned above in Simulated AIO.
If a synchronous IO request is made, it is handled by calling
os_file_write/os_file_read.
If an AIO request is made the calling thread not only queues it in the
array but also submits the requests. The helper thread then collects
the completed IO request and calls completion routine on it.

**********************************************************************/


#ifdef UNIV_PFS_IO
/* Keys to register InnoDB I/O with performance schema */
mysql_pfs_key_t  innodb_data_file_key;
mysql_pfs_key_t  innodb_log_file_key;
mysql_pfs_key_t  innodb_temp_file_key;
#endif /* UNIV_PFS_IO */

/** The asynchronous I/O context */
struct Slot {
	Slot() { memset(this, 0, sizeof(*this)); }

	/** index of the slot in the aio array */
	uint16_t		pos;

	/** true if this slot is reserved */
	bool			is_reserved;

	/** time when reserved */
	time_t			reservation_time;

	/** buffer used in i/o */
	byte*			buf;

	/** Buffer pointer used for actual IO. We advance this
	when partial IO is required and not buf */
	byte*			ptr;

	/** OS_FILE_READ or OS_FILE_WRITE */
	IORequest		type;

	/** file offset in bytes */
	os_offset_t		offset;

	/** file where to read or write */
	pfs_os_file_t		file;

	/** file name or path */
	const char*		name;

	/** used only in simulated aio: true if the physical i/o
	already made and only the slot message needs to be passed
	to the caller of os_aio_simulated_handle */
	bool			io_already_done;

	/** The file node for which the IO is requested. */
	fil_node_t*		m1;

	/** the requester of an aio operation and which can be used
	to identify which pending aio operation was completed */
	void*			m2;

	/** AIO completion status */
	dberr_t			err;

#ifdef WIN_ASYNC_IO
	/** handle object we need in the OVERLAPPED struct */
	HANDLE			handle;

	/** Windows control block for the aio request */
	OVERLAPPED		control;

	/** bytes written/read */
	DWORD			n_bytes;

	/** length of the block to read or write */
	DWORD			len;

#elif defined(LINUX_NATIVE_AIO)
	/** Linux control block for aio */
	struct iocb		control;

	/** AIO return code */
	int			ret;

	/** bytes written/read. */
	ssize_t			n_bytes;

	/** length of the block to read or write */
	ulint			len;
#else
	/** length of the block to read or write */
	ulint			len;

	/** bytes written/read. */
	ulint			n_bytes;
#endif /* WIN_ASYNC_IO */

	/** Length of the block before it was compressed */
	uint32			original_len;

	/** Buffer block for compressed pages or encrypted pages */
	Block*			buf_block;

	/** true, if we shouldn't punch a hole after writing the page */
	bool			skip_punch_hole;
};

/** The asynchronous i/o array structure */
class AIO {
public:
	/** Constructor
	@param[in]	id		Latch ID
	@param[in]	n_slots		Number of slots to configure
	@param[in]	segments	Number of segments to configure */
	AIO(latch_id_t id, ulint n_slots, ulint segments);

	/** Destructor */
	~AIO();

	/** Initialize the instance
	@return DB_SUCCESS or error code */
	dberr_t init();

	/** Requests for a slot in the aio array. If no slot is available, waits
	until not_full-event becomes signaled.

	@param[in,out]	type	IO context
	@param[in,out]	m1	message to be passed along with the AIO
				operation
	@param[in,out]	m2	message to be passed along with the AIO
				operation
	@param[in]	file	file handle
	@param[in]	name	name of the file or path as a null-terminated
				string
	@param[in,out]	buf	buffer where to read or from which to write
	@param[in]	offset	file offset, where to read from or start writing
	@param[in]	len	length of the block to read or write
	@return pointer to slot */
	Slot* reserve_slot(
		IORequest&	type,
		fil_node_t*	m1,
		void*		m2,
		pfs_os_file_t	file,
		const char*	name,
		void*		buf,
		os_offset_t	offset,
		ulint		len)
		MY_ATTRIBUTE((warn_unused_result));

	/** @return number of reserved slots */
	ulint pending_io_count() const;

	/** Returns a pointer to the nth slot in the aio array.
	@param[in]	index	Index of the slot in the array
	@return pointer to slot */
	const Slot* at(ulint i) const
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_a(i < m_slots.size());

		return(&m_slots[i]);
	}

	/** Non const version */
	Slot* at(ulint i)
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_a(i < m_slots.size());

		return(&m_slots[i]);
	}

	/** Frees a slot in the AIO array, assumes caller owns the mutex.
	@param[in,out]	slot	Slot to release */
	void release(Slot* slot);

	/** Frees a slot in the AIO array, assumes caller doesn't own the mutex.
	@param[in,out]	slot	Slot to release */
	void release_with_mutex(Slot* slot);

	/** Prints info about the aio array.
	@param[in,out]	file	Where to print */
	void print(FILE* file);

	/** @return the number of slots per segment */
	ulint slots_per_segment() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(m_slots.size() / m_n_segments);
	}

	/** @return accessor for n_segments */
	ulint get_n_segments() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(m_n_segments);
	}

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex */
	bool is_mutex_owned() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(mutex_own(&m_mutex));
	}
#endif /* UNIV_DEBUG */

	/** Acquire the mutex */
	void acquire() const
	{
		mutex_enter(&m_mutex);
	}

	/** Release the mutex */
	void release() const
	{
		mutex_exit(&m_mutex);
	}

	/** Write out the state to the file/stream
	@param[in, out]	file	File to write to */
	void to_file(FILE* file) const;

#ifdef LINUX_NATIVE_AIO
	/** Dispatch an AIO request to the kernel.
	@param[in,out]	slot	an already reserved slot
	@return true on success. */
	bool linux_dispatch(Slot* slot)
		MY_ATTRIBUTE((warn_unused_result));

	/** Accessor for an AIO event
	@param[in]	index	Index into the array
	@return the event at the index */
	io_event* io_events(ulint index)
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_a(index < m_events.size());

		return(&m_events[index]);
	}

	/** Accessor for the AIO context
	@param[in]	segment	Segment for which to get the context
	@return the AIO context for the segment */
	io_context* io_ctx(ulint segment)
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_ad(segment < get_n_segments());

		return(m_aio_ctx[segment]);
	}

	/** Creates an io_context for native linux AIO.
	@param[in]	max_events	number of events
	@param[out]	io_ctx		io_ctx to initialize.
	@return true on success. */
	static bool linux_create_io_ctx(ulint max_events, io_context_t* io_ctx)
		MY_ATTRIBUTE((warn_unused_result));

	/** Checks if the system supports native linux aio. On some kernel
	versions where native aio is supported it won't work on tmpfs. In such
	cases we can't use native aio as it is not possible to mix simulated
	and native aio.
	@return true if supported, false otherwise. */
	static bool is_linux_native_aio_supported()
		MY_ATTRIBUTE((warn_unused_result));
#endif /* LINUX_NATIVE_AIO */

#ifdef WIN_ASYNC_IO
	/** Wakes up all async i/o threads in the array in Windows async I/O at
	shutdown. */
	void signal()
	{
		for (ulint i = 0; i < m_slots.size(); ++i) {
			SetEvent(m_slots[i].handle);
		}
	}

	/** Wake up all AIO threads in Windows native aio */
	static void wake_at_shutdown()
	{
		s_reads->signal();

		if (s_writes != NULL) {
			s_writes->signal();
		}

		if (s_ibuf != NULL) {
			s_ibuf->signal();
		}

		if (s_log != NULL) {
			s_log->signal();
		}
	}
#endif /* WIN_ASYNC_IO */

#ifdef _WIN32
	/** This function can be called if one wants to post a batch of reads
	and prefers an I/O - handler thread to handle them all at once later.You
	must call os_aio_simulated_wake_handler_threads later to ensure the
	threads are not left sleeping! */
	static void simulated_put_read_threads_to_sleep();

	/** The non asynchronous IO array.
	@return the synchronous AIO array instance. */
	static AIO* sync_array()
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(s_sync);
	}

	/**
	Get the AIO handles for a segment.
	@param[in]	segment		The local segment.
	@return the handles for the segment. */
	HANDLE* handles(ulint segment)
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_ad(segment < m_handles->size() / slots_per_segment());

		return(&(*m_handles)[segment * slots_per_segment()]);
	}

	/** @return true if no slots are reserved */
	bool is_empty() const
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_ad(is_mutex_owned());
		return(m_n_reserved == 0);
	}
#endif /* _WIN32 */

	/** Create an instance using new(std::nothrow)
	@param[in]	id		Latch ID
	@param[in]	n_slots		The number of AIO request slots
	@param[in]	segments	The number of segments
	@return a new AIO instance */
	static AIO* create(
		latch_id_t	id,
		ulint		n_slots,
		ulint		segments)
		MY_ATTRIBUTE((warn_unused_result));

	/** Initializes the asynchronous io system. Creates one array each
	for ibuf and log I/O. Also creates one array each for read and write
	where each array is divided logically into n_readers and n_writers
	respectively. The caller must create an i/o handler thread for each
	segment in these arrays. This function also creates the sync array.
	No I/O handler thread needs to be created for that
	@param[in]	n_per_seg	maximum number of pending aio
					operations allowed per segment
	@param[in]	n_readers	number of reader threads
	@param[in]	n_writers	number of writer threads
	@param[in]	n_slots_sync	number of slots in the sync aio array
	@return true if AIO sub-system was started successfully */
	static bool start(
		ulint		n_per_seg,
		ulint		n_readers,
		ulint		n_writers,
		ulint		n_slots_sync)
		MY_ATTRIBUTE((warn_unused_result));

	/** Free the AIO arrays */
	static void shutdown();

	/** Print all the AIO segments
	@param[in,out]	file		Where to print */
	static void print_all(FILE* file);

	/** Calculates local segment number and aio array from global
	segment number.
	@param[out]	array		AIO wait array
	@param[in]	segment		global segment number
	@return local segment number within the aio array */
	static ulint get_array_and_local_segment(
		AIO**		array,
		ulint		segment)
		MY_ATTRIBUTE((warn_unused_result));

	/** Select the IO slot array
	@param[in]	type		Type of IO, READ or WRITE
	@param[in]	read_only	true if running in read-only mode
	@param[in]	mode		IO mode
	@return slot array or NULL if invalid mode specified */
	static AIO* select_slot_array(
		IORequest&	type,
		bool		read_only,
		ulint		mode)
		MY_ATTRIBUTE((warn_unused_result));

	/** Calculates segment number for a slot.
	@param[in]	array		AIO wait array
	@param[in]	slot		slot in this array
	@return segment number (which is the number used by, for example,
		I/O handler threads) */
	static ulint get_segment_no_from_slot(
		const AIO*	array,
		const Slot*	slot)
		MY_ATTRIBUTE((warn_unused_result));

	/** Wakes up a simulated AIO I/O-handler thread if it has something
	to do.
	@param[in]	global_segment	the number of the segment in the
					AIO arrays */
	static void wake_simulated_handler_thread(ulint global_segment);

	/** Check if it is a read request
	@param[in]	aio		The AIO instance to check
	@return true if the AIO instance is for reading. */
	static bool is_read(const AIO* aio)
		MY_ATTRIBUTE((warn_unused_result))
	{
		return(s_reads == aio);
	}

	/** Wait on an event until no pending writes */
	static void wait_until_no_pending_writes()
	{
		os_event_wait(AIO::s_writes->m_is_empty);
	}

	/** Print to file
	@param[in]	file		File to write to */
	static void print_to_file(FILE* file);

	/** Check for pending IO. Gets the count and also validates the
	data structures.
	@return count of pending IO requests */
	static ulint total_pending_io_count();

private:
	/** Initialise the slots
	@return DB_SUCCESS or error code */
	dberr_t init_slots()
		MY_ATTRIBUTE((warn_unused_result));

	/** Wakes up a simulated AIO I/O-handler thread if it has something
	to do for a local segment in the AIO array.
	@param[in]	global_segment	the number of the segment in the
					AIO arrays
	@param[in]	segment		the local segment in the AIO array */
	void wake_simulated_handler_thread(ulint global_segment, ulint segment);

	/** Prints pending IO requests per segment of an aio array.
	We probably don't need per segment statistics but they can help us
	during development phase to see if the IO requests are being
	distributed as expected.
	@param[in,out]	file		file where to print
	@param[in]	segments	pending IO array */
	void print_segment_info(
		FILE*		file,
		const ulint*	segments);

#ifdef LINUX_NATIVE_AIO
	/** Initialise the Linux native AIO data structures
	@return DB_SUCCESS or error code */
	dberr_t init_linux_native_aio()
		MY_ATTRIBUTE((warn_unused_result));
#endif /* LINUX_NATIVE_AIO */

private:
	typedef std::vector<Slot> Slots;

	/** the mutex protecting the aio array */
	mutable SysMutex	m_mutex;

	/** Pointer to the slots in the array.
	Number of elements must be divisible by n_threads. */
	Slots			m_slots;

	/** Number of segments in the aio array of pending aio requests.
	A thread can wait separately for any one of the segments. */
	ulint			m_n_segments;

	/** The event which is set to the signaled state when
	there is space in the aio outside the ibuf segment */
	os_event_t		m_not_full;

	/** The event which is set to the signaled state when
	there are no pending i/os in this array */
	os_event_t		m_is_empty;

	/** Number of reserved slots in the AIO array outside
	the ibuf segment */
	ulint			m_n_reserved;

#ifdef _WIN32
	typedef std::vector<HANDLE, ut_allocator<HANDLE> > Handles;

	/** Pointer to an array of OS native event handles where
	we copied the handles from slots, in the same order. This
	can be used in WaitForMultipleObjects; used only in Windows */
	Handles*		m_handles;
#endif /* _WIN32 */

#if defined(LINUX_NATIVE_AIO)
	typedef std::vector<io_event> IOEvents;

	/** completion queue for IO. There is one such queue per
	segment. Each thread will work on one ctx exclusively. */
	io_context_t*		m_aio_ctx;

	/** The array to collect completed IOs. There is one such
	event for each possible pending IO. The size of the array
	is equal to m_slots.size(). */
	IOEvents		m_events;
#endif /* LINUX_NATIV_AIO */

	/** The aio arrays for non-ibuf i/o and ibuf i/o, as well as
	sync AIO. These are NULL when the module has not yet been
	initialized. */

	/** Insert buffer */
	static AIO*		s_ibuf;

	/** Redo log */
	static AIO*		s_log;

	/** Reads */
	static AIO*		s_reads;

	/** Writes */
	static AIO*		s_writes;

	/** Synchronous I/O */
	static AIO*		s_sync;
};

/** Static declarations */
AIO*	AIO::s_reads;
AIO*	AIO::s_writes;
AIO*	AIO::s_ibuf;
AIO*	AIO::s_log;
AIO*	AIO::s_sync;

#if defined(LINUX_NATIVE_AIO)
/** timeout for each io_getevents() call = 500ms. */
static const ulint	OS_AIO_REAP_TIMEOUT = 500000000UL;

/** time to sleep, in microseconds if io_setup() returns EAGAIN. */
static const ulint	OS_AIO_IO_SETUP_RETRY_SLEEP = 500000UL;

/** number of attempts before giving up on io_setup(). */
static const int	OS_AIO_IO_SETUP_RETRY_ATTEMPTS = 5;
#endif /* LINUX_NATIVE_AIO */

/** Array of events used in simulated AIO */
static os_event_t*	os_aio_segment_wait_events = NULL;

/** Number of asynchronous I/O segments.  Set by os_aio_init(). */
static ulint		os_aio_n_segments = ULINT_UNDEFINED;

/** If the following is true, read i/o handler threads try to
wait until a batch of new read requests have been posted */
static bool		os_aio_recommend_sleep_for_read_threads = false;
#endif /* !UNIV_HOTBACKUP */

ulint	os_n_file_reads		= 0;
ulint	os_bytes_read_since_printout = 0;
ulint	os_n_file_writes	= 0;
ulint	os_n_fsyncs		= 0;
ulint	os_n_file_reads_old	= 0;
ulint	os_n_file_writes_old	= 0;
ulint	os_n_fsyncs_old		= 0;
/** Number of pending write operations */
ulint	os_n_pending_writes = 0;
/** Number of pending read operations */
ulint	os_n_pending_reads = 0;

time_t	os_last_printout;
bool	os_has_said_disk_full	= false;

/** Default Zip compression level */
extern uint page_zip_level;

#if DATA_TRX_ID_LEN > 6
#error "COMPRESSION_ALGORITHM will not fit"
#endif /* DATA_TRX_ID_LEN */

/** Validates the consistency of the aio system.
@return true if ok */
static
bool
os_aio_validate();

/** Does error handling when a file operation fails.
@param[in]	name		File name or NULL
@param[in]	operation	Name of operation e.g., "read", "write"
@return true if we should retry the operation */
static
bool
os_file_handle_error(
	const char*	name,
	const char*	operation);

/** Free storage space associated with a section of the file.
@param[in]      fh              Open file handle
@param[in]      off             Starting offset (SEEK_SET)
@param[in]      len             Size of the hole
@return DB_SUCCESS or error code */
dberr_t
os_file_punch_hole(
        os_file_t   fh,
        os_offset_t     off,
        os_offset_t     len);

/**
Does error handling when a file operation fails.
@param[in]	name		File name or NULL
@param[in]	operation	Name of operation e.g., "read", "write"
@param[in]	silent	if true then don't print any message to the log.
@return true if we should retry the operation */
static
bool
os_file_handle_error_no_exit(
	const char*	name,
	const char*	operation,
	bool		silent);

/** Decompress after a read and punch a hole in the file if it was a write
@param[in]	type		IO context
@param[in]	fh		Open file handle
@param[in,out]	buf		Buffer to transform
@param[in,out]	scratch		Scratch area for read decompression
@param[in]	src_len		Length of the buffer before compression
@param[in]	len		Compressed buffer length for write and size
				of buf len for read
@return DB_SUCCESS or error code */
static
dberr_t
os_file_io_complete(
	const IORequest&type,
	os_file_t	fh,
	byte*		buf,
	byte*		scratch,
	ulint		src_len,
	ulint		offset,
	ulint		len);

/** Does simulated AIO. This function should be called by an i/o-handler
thread.

@param[in]	segment	The number of the segment in the aio arrays to wait
			for; segment 0 is the ibuf i/o thread, segment 1 the
			log i/o thread, then follow the non-ibuf read threads,
			and as the last are the non-ibuf write threads
@param[out]	m1	the messages passed with the AIO request; note that
			also in the case where the AIO operation failed, these
			output parameters are valid and can be used to restart
			the operation, for example
@param[out]	m2	Callback argument
@param[in]	type	IO context
@return DB_SUCCESS or error code */
static
dberr_t
os_aio_simulated_handler(
	ulint		global_segment,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	type);

#ifdef WIN_ASYNC_IO
/** This function is only used in Windows asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@param[in]	segment		The number of the segment in the aio arrays to
wait for; segment 0 is the ibuf I/O thread,
segment 1 the log I/O thread, then follow the
non-ibuf read threads, and as the last are the
non-ibuf write threads; if this is
ULINT_UNDEFINED, then it means that sync AIO
is used, and this parameter is ignored
@param[in]	pos		this parameter is used only in sync AIO:
wait for the aio slot at this position
@param[out]	m1		the messages passed with the AIO request; note
that also in the case where the AIO operation
failed, these output parameters are valid and
can be used to restart the operation,
for example
@param[out]	m2		callback message
@param[out]	type		OS_FILE_WRITE or ..._READ
@return DB_SUCCESS or error code */
static
dberr_t
os_aio_windows_handler(
	ulint		segment,
	ulint		pos,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	type);
#endif /* WIN_ASYNC_IO */

/** Allocate a page for sync IO
@return pointer to page */
static
Block*
os_alloc_block()
{
	size_t		pos;
	Blocks&		blocks = *block_cache;
	size_t		i = static_cast<size_t>(my_timer_cycles());
	const size_t	size = blocks.size();
	ulint		retry = 0;
	Block*		block;

	DBUG_EXECUTE_IF("os_block_cache_busy", retry = MAX_BLOCKS * 3;);

	for (;;) {

		/* After go through the block cache for 3 times,
		allocate a new temporary block. */
		if (retry == MAX_BLOCKS * 3) {
			byte*	ptr;

			ptr = static_cast<byte*>(
				ut_malloc_nokey(sizeof(*block)
						+ BUFFER_BLOCK_SIZE));

			block = new (ptr) Block();
			block->m_ptr = static_cast<byte*>(
				ptr + sizeof(*block));
			block->m_in_use = 1;

			break;
		}

		pos = i++ % size;

		if (TAS(&blocks[pos].m_in_use, 1) == 0) {
			block = &blocks[pos];
			break;
		}

		os_thread_yield();

		++retry;
	}

	ut_a(block->m_in_use != 0);

	return(block);
}

/** Free a page after sync IO
@param[in,own]	block		The block to free/release */
static
void
os_free_block(Block* block)
{
	ut_ad(block->m_in_use == 1);

	TAS(&block->m_in_use, 0);

	/* When this block is not in the block cache, and it's
	a temporary block, we need to free it directly. */
	if (std::less<Block*>()(block, &block_cache->front())
	    || std::greater<Block*>()(block, &block_cache->back())) {
		ut_free(block);
	}
}

/** Generic AIO Handler methods. Currently handles IO post processing. */
class AIOHandler {
public:
	/** Do any post processing after a read/write
	@return DB_SUCCESS or error code. */
	static dberr_t post_io_processing(Slot* slot);

	/** Decompress after a read and punch a hole in the file if
	it was a write */
	static dberr_t io_complete(const Slot* slot)
	{
		ut_a(slot->offset > 0);
		ut_a(slot->type.is_read() || !slot->skip_punch_hole);
		return(os_file_io_complete(
				slot->type, slot->file.m_file, slot->buf,
				NULL, slot->original_len,
				static_cast<ulint>(slot->offset),
				slot->len));
	}

private:
	/** Check whether the page was encrypted.
	@param[in]	slot		The slot that contains the IO request
	@return true if it was an encyrpted page */
	static bool is_encrypted_page(const Slot* slot)
	{
		return(Encryption::is_encrypted_page(slot->buf));
	}

	/** Check whether the page was compressed.
	@param[in]	slot		The slot that contains the IO request
	@return true if it was a compressed page */
	static bool is_compressed_page(const Slot* slot)
	{
		const byte*	src = slot->buf;

		ulint	page_type = mach_read_from_2(src + FIL_PAGE_TYPE);

		return(page_type == FIL_PAGE_COMPRESSED);
	}

	/** Get the compressed page size.
	@param[in]	slot		The slot that contains the IO request
	@return number of bytes to read for a successful decompress */
	static ulint compressed_page_size(const Slot* slot)
	{
		ut_ad(slot->type.is_read());
		ut_ad(is_compressed_page(slot));

		ulint		size;
		const byte*	src = slot->buf;

		size = mach_read_from_2(src + FIL_PAGE_COMPRESS_SIZE_V1);

		return(size + FIL_PAGE_DATA);
	}

	/** Check if the page contents can be decompressed.
	@param[in]	slot		The slot that contains the IO request
	@return true if the data read has all the compressed data */
	static bool can_decompress(const Slot* slot)
	{
		ut_ad(slot->type.is_read());
		ut_ad(is_compressed_page(slot));

		ulint		version;
		const byte*	src = slot->buf;

		version = mach_read_from_1(src + FIL_PAGE_VERSION);

		ut_a(version == 1);

		/* Includes the page header size too */
		ulint		size = compressed_page_size(slot);

		return(size <= (slot->ptr - slot->buf) + (ulint) slot->n_bytes);
	}

	/** Check if we need to read some more data.
	@param[in]	slot		The slot that contains the IO request
	@param[in]	n_bytes		Total bytes read so far
	@return DB_SUCCESS or error code */
	static dberr_t check_read(Slot* slot, ulint n_bytes);
};

/** Helper class for doing synchronous file IO. Currently, the objective
is to hide the OS specific code, so that the higher level functions aren't
peppered with #ifdef. Makes the code flow difficult to follow.  */
class SyncFileIO {
public:
	/** Constructor
	@param[in]	fh	File handle
	@param[in,out]	buf	Buffer to read/write
	@param[in]	n	Number of bytes to read/write
	@param[in]	offset	Offset where to read or write */
	SyncFileIO(os_file_t fh, void* buf, ulint n, os_offset_t offset)
		:
		m_fh(fh),
		m_buf(buf),
		m_n(static_cast<ssize_t>(n)),
		m_offset(offset)
	{
		ut_ad(m_n > 0);
	}

	/** Destructor */
	~SyncFileIO()
	{
		/* No op */
	}

	/** Do the read/write
	@param[in]	request	The IO context and type
	@return the number of bytes read/written or negative value on error */
	ssize_t execute(const IORequest& request);

	/** Do the read/write
	@param[in,out]	slot	The IO slot, it has the IO context
	@return the number of bytes read/written or negative value on error */
	static ssize_t execute(Slot* slot);

	/** Move the read/write offset up to where the partial IO succeeded.
	@param[in]	n_bytes	The number of bytes to advance */
	void advance(ssize_t n_bytes)
	{
		m_offset += n_bytes;

		ut_ad(m_n >= n_bytes);

		m_n -=  n_bytes;

		m_buf = reinterpret_cast<uchar*>(m_buf) + n_bytes;
	}

private:
	/** Open file handle */
	os_file_t		m_fh;

	/** Buffer to read/write */
	void*			m_buf;

	/** Number of bytes to read/write */
	ssize_t			m_n;

	/** Offset from where to read/write */
	os_offset_t		m_offset;
};

/** If it is a compressed page return the compressed page data + footer size
@param[in]	buf		Buffer to check, must include header + 10 bytes
@return ULINT_UNDEFINED if the page is not a compressed page or length
	of the compressed data (including footer) if it is a compressed page */
ulint
os_file_compressed_page_size(const byte* buf)
{
	ulint	type = mach_read_from_2(buf + FIL_PAGE_TYPE);

	if (type == FIL_PAGE_COMPRESSED) {
		ulint	version = mach_read_from_1(buf + FIL_PAGE_VERSION);
		ut_a(version == 1);
		return(mach_read_from_2(buf + FIL_PAGE_COMPRESS_SIZE_V1));
	}

	return(ULINT_UNDEFINED);
}

/** If it is a compressed page return the original page data + footer size
@param[in] buf		Buffer to check, must include header + 10 bytes
@return ULINT_UNDEFINED if the page is not a compressed page or length
	of the original data + footer if it is a compressed page */
ulint
os_file_original_page_size(const byte* buf)
{
	ulint	type = mach_read_from_2(buf + FIL_PAGE_TYPE);

	if (type == FIL_PAGE_COMPRESSED) {

		ulint	version = mach_read_from_1(buf + FIL_PAGE_VERSION);
		ut_a(version == 1);

		return(mach_read_from_2(buf + FIL_PAGE_ORIGINAL_SIZE_V1));
	}

	return(ULINT_UNDEFINED);
}

/** Check if we need to read some more data.
@param[in]	slot		The slot that contains the IO request
@param[in]	n_bytes		Total bytes read so far
@return DB_SUCCESS or error code */
dberr_t
AIOHandler::check_read(Slot* slot, ulint n_bytes)
{
	dberr_t	err;

	ut_ad(slot->type.is_read());
	ut_ad(slot->original_len > slot->len);

	if (is_compressed_page(slot)) {

		if (can_decompress(slot)) {

			ut_a(slot->offset > 0);

			slot->len = slot->original_len;
#ifdef _WIN32
			slot->n_bytes = static_cast<DWORD>(n_bytes);
#else
			slot->n_bytes = static_cast<ulint>(n_bytes);
#endif /* _WIN32 */

			err = io_complete(slot);
			ut_a(err == DB_SUCCESS);
		} else {
			/* Read the next block in */
			ut_ad(compressed_page_size(slot) >= n_bytes);

			err = DB_FAIL;
		}
	} else if (is_encrypted_page(slot)) {
			ut_a(slot->offset > 0);

			slot->len = slot->original_len;
#ifdef _WIN32
			slot->n_bytes = static_cast<DWORD>(n_bytes);
#else
			slot->n_bytes = static_cast<ulint>(n_bytes);
#endif /* _WIN32 */

			err = io_complete(slot);
			ut_a(err == DB_SUCCESS);

	} else {
		err = DB_FAIL;
	}

	if (slot->buf_block != NULL) {
		os_free_block(slot->buf_block);
		slot->buf_block = NULL;
	}

	return(err);
}

/** Do any post processing after a read/write
@return DB_SUCCESS or error code. */
dberr_t
AIOHandler::post_io_processing(Slot* slot)
{
	dberr_t	err;

	ut_ad(slot->is_reserved);

	/* Total bytes read so far */
	ulint	n_bytes = (slot->ptr - slot->buf) + slot->n_bytes;

	/* Compressed writes can be smaller than the original length.
	Therefore they can be processed without further IO. */
	if (n_bytes == slot->original_len
	    || (slot->type.is_write()
		&& slot->type.is_compressed()
		&& slot->len == static_cast<ulint>(slot->n_bytes))) {

		if (!slot->type.is_log()
		    && (is_compressed_page(slot)
			|| is_encrypted_page(slot))) {

			ut_a(slot->offset > 0);

			if (slot->type.is_read()) {
				slot->len = slot->original_len;
			}

			/* The punch hole has been done on collect() */

			if (slot->type.is_read()) {
				err = io_complete(slot);
			} else {
				err = DB_SUCCESS;
			}

			ut_ad(err == DB_SUCCESS
			      || err == DB_UNSUPPORTED
			      || err == DB_CORRUPTION
			      || err == DB_IO_DECOMPRESS_FAIL);
		} else {

			err = DB_SUCCESS;
		}

		if (slot->buf_block != NULL) {
			os_free_block(slot->buf_block);
			slot->buf_block = NULL;
		}

	} else if ((ulint) slot->n_bytes == (ulint) slot->len) {

		/* It *must* be a partial read. */
		ut_ad(slot->len < slot->original_len);

		/* Has to be a read request, if it is less than
		the original length. */
		ut_ad(slot->type.is_read());
		err = check_read(slot, n_bytes);

	} else {
		err = DB_FAIL;
	}

	return(err);
}

/** Count the number of free slots
@return number of reserved slots */
ulint
AIO::pending_io_count() const
{
	acquire();

#ifdef UNIV_DEBUG
	ut_a(m_n_segments > 0);
	ut_a(!m_slots.empty());

	ulint	count = 0;

	for (ulint i = 0; i < m_slots.size(); ++i) {

		const Slot&	slot = m_slots[i];

		if (slot.is_reserved) {
			++count;
			ut_a(slot.len > 0);
		}
	}

	ut_a(m_n_reserved == count);
#endif /* UNIV_DEBUG */

	ulint	reserved = m_n_reserved;

	release();

	return(reserved);
}

/** Compress a data page
#param[in]	block_size	File system block size
@param[in]	src		Source contents to compress
@param[in]	src_len		Length in bytes of the source
@param[out]	dst		Compressed page contents
@param[out]	dst_len		Length in bytes of dst contents
@return buffer data, dst_len will have the length of the data */
static
byte*
os_file_compress_page(
	Compression	compression,
	ulint		block_size,
	byte*		src,
	ulint		src_len,
	byte*		dst,
	ulint*		dst_len)
{
	ulint		len = 0;
	ulint		compression_level = page_zip_level;
	ulint		page_type = mach_read_from_2(src + FIL_PAGE_TYPE);

	/* The page size must be a multiple of the OS punch hole size. */
	ut_ad(!(src_len % block_size));

	/* Shouldn't compress an already compressed page. */
	ut_ad(page_type != FIL_PAGE_COMPRESSED);

	/* The page must be at least twice as large as the file system
	block size if we are to save any space. Ignore R-Tree pages for now,
	they repurpose the same 8 bytes in the page header. No point in
	compressing if the file system block size >= our page size. */

	if (page_type == FIL_PAGE_RTREE
	    || block_size == ULINT_UNDEFINED
	    || compression.m_type == Compression::NONE
	    || src_len < block_size * 2) {

		*dst_len = src_len;

		return(src);
	}

	/* Leave the header alone when compressing. */
	ut_ad(block_size >= FIL_PAGE_DATA * 2);

	ut_ad(src_len > FIL_PAGE_DATA + block_size);

	/* Must compress to <= N-1 FS blocks. */
	ulint		out_len = src_len - (FIL_PAGE_DATA + block_size);

	/* This is the original data page size - the page header. */
	ulint		content_len = src_len - FIL_PAGE_DATA;

	ut_ad(out_len >= block_size - FIL_PAGE_DATA);
	ut_ad(out_len <= src_len - (block_size + FIL_PAGE_DATA));

	/* Only compress the data + trailer, leave the header alone */

	switch (compression.m_type) {
	case Compression::NONE:
		ut_error;

	case Compression::ZLIB: {

		uLongf	zlen = static_cast<uLongf>(out_len);

		if (compress2(
			dst + FIL_PAGE_DATA,
			&zlen,
			src + FIL_PAGE_DATA,
			static_cast<uLong>(content_len),
			static_cast<int>(compression_level)) != Z_OK) {

			*dst_len = src_len;

			return(src);
		}

		len = static_cast<ulint>(zlen);

		break;
	}

	case Compression::LZ4:

		len = LZ4_compress_default(
			reinterpret_cast<char*>(src) + FIL_PAGE_DATA,
			reinterpret_cast<char*>(dst) + FIL_PAGE_DATA,
			static_cast<int>(content_len),
			static_cast<int>(out_len));

		ut_a(len <= src_len - FIL_PAGE_DATA);

		if (len == 0  || len >= out_len) {

			*dst_len = src_len;

			return(src);
		}

		break;

	default:
		*dst_len = src_len;
		return(src);
	}

	ut_a(len <= out_len);

	ut_ad(memcmp(src + FIL_PAGE_LSN + 4,
		     src + src_len - FIL_PAGE_END_LSN_OLD_CHKSUM + 4, 4)
	      == 0);

	/* Copy the header as is. */
	memmove(dst, src, FIL_PAGE_DATA);

	/* Add compression control information. Required for decompressing. */
	mach_write_to_2(dst + FIL_PAGE_TYPE, FIL_PAGE_COMPRESSED);

	mach_write_to_1(dst + FIL_PAGE_VERSION, 1);

	mach_write_to_1(dst + FIL_PAGE_ALGORITHM_V1, compression.m_type);

	mach_write_to_2(dst + FIL_PAGE_ORIGINAL_TYPE_V1, page_type);

	mach_write_to_2(dst + FIL_PAGE_ORIGINAL_SIZE_V1, content_len);

	mach_write_to_2(dst + FIL_PAGE_COMPRESS_SIZE_V1, len);

	/* Round to the next full block size */

	len += FIL_PAGE_DATA;

	*dst_len = ut_calc_align(len, block_size);

	ut_ad(*dst_len >= len && *dst_len <= out_len + FIL_PAGE_DATA);

	/* Clear out the unused portion of the page. */
	if (len % block_size) {
		memset(dst + len, 0x0, block_size - (len % block_size));
	}

	return(dst);
}

#ifdef UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
/** Validates the consistency the aio system some of the time.
@return true if ok or the check was skipped */
bool
os_aio_validate_skip()
{
/** Try os_aio_validate() every this many times */
# define OS_AIO_VALIDATE_SKIP	13

	/** The os_aio_validate() call skip counter.
	Use a signed type because of the race condition below. */
	static int os_aio_validate_count = OS_AIO_VALIDATE_SKIP;

	/* There is a race condition below, but it does not matter,
	because this call is only for heuristic purposes. We want to
	reduce the call frequency of the costly os_aio_validate()
	check in debug builds. */
	--os_aio_validate_count;

	if (os_aio_validate_count > 0) {
		return(true);
	}

	os_aio_validate_count = OS_AIO_VALIDATE_SKIP;
	return(os_aio_validate());
}
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_DEBUG */

#undef USE_FILE_LOCK
#define USE_FILE_LOCK
#if defined(UNIV_HOTBACKUP) || defined(_WIN32)
/* InnoDB Hot Backup does not lock the data files.
 * On Windows, mandatory locking is used.
 */
# undef USE_FILE_LOCK
#endif
#ifdef USE_FILE_LOCK
/** Obtain an exclusive lock on a file.
@param[in]	fd		file descriptor
@param[in]	name		file name
@return 0 on success */
static
int
os_file_lock(
	int		fd,
	const char*	name)
{
	struct flock lk;

	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = lk.l_len = 0;

	if (fcntl(fd, F_SETLK, &lk) == -1) {

		ib::error()
			<< "Unable to lock " << name
			<< " error: " << errno;

		if (errno == EAGAIN || errno == EACCES) {

			ib::info()
				<< "Check that you do not already have"
				" another mysqld process using the"
				" same InnoDB data or log files.";
		}

		return(-1);
	}

	return(0);
}
#endif /* USE_FILE_LOCK */

#ifndef UNIV_HOTBACKUP

/** Calculates local segment number and aio array from global segment number.
@param[out]	array		aio wait array
@param[in]	segment		global segment number
@return local segment number within the aio array */
ulint
AIO::get_array_and_local_segment(
	AIO**		array,
	ulint		segment)
{
	ulint		local_segment;
	ulint		n_extra_segs = (srv_read_only_mode) ? 0 : 2;

	ut_a(segment < os_aio_n_segments);

	if (!srv_read_only_mode && segment < n_extra_segs) {

		/* We don't support ibuf/log IO during read only mode. */

		if (segment == IO_IBUF_SEGMENT) {

			*array = s_ibuf;

		} else if (segment == IO_LOG_SEGMENT) {

			*array = s_log;

		} else {
			*array = NULL;
		}

		local_segment = 0;

	} else if (segment < s_reads->m_n_segments + n_extra_segs) {

		*array = s_reads;
		local_segment = segment - n_extra_segs;

	} else {
		*array = s_writes;

		local_segment = segment
			      - (s_reads->m_n_segments + n_extra_segs);
	}

	return(local_segment);
}

/** Frees a slot in the aio array. Assumes caller owns the mutex.
@param[in,out]	slot		Slot to release */
void
AIO::release(Slot* slot)
{
	ut_ad(is_mutex_owned());

	ut_ad(slot->is_reserved);

	slot->is_reserved = false;

	--m_n_reserved;

	if (m_n_reserved == m_slots.size() - 1) {
		os_event_set(m_not_full);
	}

	if (m_n_reserved == 0) {
		os_event_set(m_is_empty);
	}

#ifdef WIN_ASYNC_IO

	ResetEvent(slot->handle);

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		memset(&slot->control, 0x0, sizeof(slot->control));
		slot->ret = 0;
		slot->n_bytes = 0;
	} else {
		/* These fields should not be used if we are not
		using native AIO. */
		ut_ad(slot->n_bytes == 0);
		ut_ad(slot->ret == 0);
	}

#endif /* WIN_ASYNC_IO */
}

/** Frees a slot in the AIO array. Assumes caller doesn't own the mutex.
@param[in,out]	slot		Slot to release */
void
AIO::release_with_mutex(Slot* slot)
{
	acquire();

	release(slot);

	release();
}

/** Creates a temporary file.  This function is like tmpfile(3), but
the temporary file is created in the given parameter path. If the path
is NULL then it will create the file in the MySQL server configuration
parameter (--tmpdir).
@param[in]	path	location for creating temporary file
@return temporary file handle, or NULL on error */
FILE*
os_file_create_tmpfile(
	const char*	path)
{
	FILE*	file	= NULL;
	int	fd	= innobase_mysql_tmpfile(path);

	if (fd >= 0) {
		file = fdopen(fd, "w+b");
	}

	if (file == NULL) {

		ib::error()
			<< "Unable to create temporary file; errno: "
			<< errno;

		if (fd >= 0) {
			close(fd);
		}
	}

	return(file);
}

/** Rewind file to its start, read at most size - 1 bytes from it to str, and
NUL-terminate str. All errors are silently ignored. This function is
mostly meant to be used with temporary files.
@param[in,out]	file		File to read from
@param[in,out]	str		Buffer where to read
@param[in]	size		Size of buffer */
void
os_file_read_string(
	FILE*		file,
	char*		str,
	ulint		size)
{
	if (size != 0) {
		rewind(file);

		size_t	flen = fread(str, 1, size - 1, file);

		str[flen] = '\0';
	}
}

/** Decompress after a read and punch a hole in the file if it was a write
@param[in]	type		IO context
@param[in]	fh		Open file handle
@param[in,out]	buf		Buffer to transform
@param[in,out]	scratch		Scratch area for read decompression
@param[in]	src_len		Length of the buffer before compression
@param[in]	len		Used buffer length for write and output
				buf len for read
@return DB_SUCCESS or error code */
static
dberr_t
os_file_io_complete(
	const IORequest&type,
	os_file_t	fh,
	byte*		buf,
	byte*		scratch,
	ulint		src_len,
	ulint		offset,
	ulint		len)
{
	/* We never compress/decompress the first page */
	ut_a(offset > 0);
	ut_ad(type.validate());

	if (!type.is_compression_enabled()) {

		return(DB_SUCCESS);

	} else if (type.is_read()) {
		dberr_t		ret;
		Encryption	encryption(type.encryption_algorithm());

		ut_ad(!type.is_log());

		ret = encryption.decrypt(type, buf, src_len, scratch, len);
		if (ret == DB_SUCCESS) {
			return(os_file_decompress_page(
					type.is_dblwr_recover(),
					buf, scratch, len));
		} else {
			return(ret);
		}

	} else if (type.punch_hole()) {

		ut_ad(len <= src_len);
		ut_ad(!type.is_log());
		ut_ad(type.is_write());
		ut_ad(type.is_compressed());

		/* Nothing to do. */
		if (len == src_len) {
			return(DB_SUCCESS);
		}

#ifdef UNIV_DEBUG
		const ulint	block_size = type.block_size();
#endif /* UNIV_DEBUG */

		/* We don't support multiple page sizes in the server
		at the moment. */
		ut_ad(src_len == srv_page_size);

		/* Must be a multiple of the compression unit size. */
		ut_ad((len % block_size) == 0);
		ut_ad((offset % block_size) == 0);

		ut_ad(len + block_size <= src_len);

		offset += len;

		return(os_file_punch_hole(fh, offset, src_len - len));
	}

	ut_ad(!type.is_log());

	return(DB_SUCCESS);
}

#endif /* !UNIV_HOTBACKUP */

/** This function returns a new path name after replacing the basename
in an old path with a new basename.  The old_path is a full path
name including the extension.  The tablename is in the normal
form "databasename/tablename".  The new base name is found after
the forward slash.  Both input strings are null terminated.

This function allocates memory to be returned.  It is the callers
responsibility to free the return value after it is no longer needed.

@param[in]	old_path		Pathname
@param[in]	tablename		Contains new base name
@return own: new full pathname */
char*
os_file_make_new_pathname(
	const char*	old_path,
	const char*	tablename)
{
	ulint		dir_len;
	char*		last_slash;
	char*		base_name;
	char*		new_path;
	ulint		new_path_len;

	/* Split the tablename into its database and table name components.
	They are separated by a '/'. */
	last_slash = strrchr((char*) tablename, '/');
	base_name = last_slash ? last_slash + 1 : (char*) tablename;

	/* Find the offset of the last slash. We will strip off the
	old basename.ibd which starts after that slash. */
	last_slash = strrchr((char*) old_path, OS_PATH_SEPARATOR);
	dir_len = last_slash ? last_slash - old_path : strlen(old_path);

	/* allocate a new path and move the old directory path to it. */
	new_path_len = dir_len + strlen(base_name) + sizeof "/.ibd";
	new_path = static_cast<char*>(ut_malloc_nokey(new_path_len));
	memcpy(new_path, old_path, dir_len);

	ut_snprintf(new_path + dir_len,
		    new_path_len - dir_len,
		    "%c%s.ibd",
		    OS_PATH_SEPARATOR,
		    base_name);

	return(new_path);
}

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
	char*	data_dir_path)
{
	/* Replace the period before the extension with a null byte. */
	char*	ptr = strrchr((char*) data_dir_path, '.');

	if (ptr == NULL) {
		return;
	}

	ptr[0] = '\0';

	/* The tablename starts after the last slash. */
	ptr = strrchr((char*) data_dir_path, OS_PATH_SEPARATOR);

	if (ptr == NULL) {
		return;
	}

	ptr[0] = '\0';

	char*	tablename = ptr + 1;

	/* The databasename starts after the next to last slash. */
	ptr = strrchr((char*) data_dir_path, OS_PATH_SEPARATOR);

	if (ptr == NULL) {
		return;
	}

	ulint	tablename_len = ut_strlen(tablename);

	ut_memmove(++ptr, tablename, tablename_len);

	ptr[tablename_len] = '\0';
}

/** Check if the path refers to the root of a drive using a pointer
to the last directory separator that the caller has fixed.
@param[in]	path	path name
@param[in]	path	last directory separator in the path
@return true if this path is a drive root, false if not */
UNIV_INLINE
bool
os_file_is_root(
	const char*	path,
	const char*	last_slash)
{
	return(
#ifdef _WIN32
	       (last_slash == path + 2 && path[1] == ':') ||
#endif /* _WIN32 */
	       last_slash == path);
}

/** Return the parent directory component of a null-terminated path.
Return a new buffer containing the string up to, but not including,
the final component of the path.
The path returned will not contain a trailing separator.
Do not return a root path, return NULL instead.
The final component trimmed off may be a filename or a directory name.
If the final component is the only component of the path, return NULL.
It is the caller's responsibility to free the returned string after it
is no longer needed.
@param[in]	path		Path name
@return own: parent directory of the path */
static
char*
os_file_get_parent_dir(
	const char*	path)
{
	bool	has_trailing_slash = false;

	/* Find the offset of the last slash */
	const char* last_slash = strrchr(path, OS_PATH_SEPARATOR);

	if (!last_slash) {
		/* No slash in the path, return NULL */
		return(NULL);
	}

	/* Ok, there is a slash. Is there anything after it? */
	if (static_cast<size_t>(last_slash - path + 1) == strlen(path)) {
		has_trailing_slash = true;
	}

	/* Reduce repetative slashes. */
	while (last_slash > path
		&& last_slash[-1] == OS_PATH_SEPARATOR) {
		last_slash--;
	}

	/* Check for the root of a drive. */
	if (os_file_is_root(path, last_slash)) {
		return(NULL);
	}

	/* If a trailing slash prevented the first strrchr() from trimming
	the last component of the path, trim that component now. */
	if (has_trailing_slash) {
		/* Back up to the previous slash. */
		last_slash--;
		while (last_slash > path
		       && last_slash[0] != OS_PATH_SEPARATOR) {
			last_slash--;
		}

		/* Reduce repetative slashes. */
		while (last_slash > path
			&& last_slash[-1] == OS_PATH_SEPARATOR) {
			last_slash--;
		}
	}

	/* Check for the root of a drive. */
	if (os_file_is_root(path, last_slash)) {
		return(NULL);
	}

	/* Non-trivial directory component */

	return(mem_strdupl(path, last_slash - path));
}
#ifdef UNIV_ENABLE_UNIT_TEST_GET_PARENT_DIR

/* Test the function os_file_get_parent_dir. */
void
test_os_file_get_parent_dir(
	const char*	child_dir,
	const char*	expected_dir)
{
	char* child = mem_strdup(child_dir);
	char* expected = expected_dir == NULL ? NULL
			 : mem_strdup(expected_dir);

	/* os_file_get_parent_dir() assumes that separators are
	converted to OS_PATH_SEPARATOR. */
	os_normalize_path(child);
	os_normalize_path(expected);

	char* parent = os_file_get_parent_dir(child);

	bool unexpected = (expected == NULL
			  ? (parent != NULL)
			  : (0 != strcmp(parent, expected)));
	if (unexpected) {
		ib::fatal() << "os_file_get_parent_dir('" << child
			<< "') returned '" << parent
			<< "', instead of '" << expected << "'.";
	}
	ut_free(parent);
	ut_free(child);
	ut_free(expected);
}

/* Test the function os_file_get_parent_dir. */
void
unit_test_os_file_get_parent_dir()
{
	test_os_file_get_parent_dir("/usr/lib/a", "/usr/lib");
	test_os_file_get_parent_dir("/usr/", NULL);
	test_os_file_get_parent_dir("//usr//", NULL);
	test_os_file_get_parent_dir("usr", NULL);
	test_os_file_get_parent_dir("usr//", NULL);
	test_os_file_get_parent_dir("/", NULL);
	test_os_file_get_parent_dir("//", NULL);
	test_os_file_get_parent_dir(".", NULL);
	test_os_file_get_parent_dir("..", NULL);
# ifdef _WIN32
	test_os_file_get_parent_dir("D:", NULL);
	test_os_file_get_parent_dir("D:/", NULL);
	test_os_file_get_parent_dir("D:\\", NULL);
	test_os_file_get_parent_dir("D:/data", NULL);
	test_os_file_get_parent_dir("D:/data/", NULL);
	test_os_file_get_parent_dir("D:\\data\\", NULL);
	test_os_file_get_parent_dir("D:///data/////", NULL);
	test_os_file_get_parent_dir("D:\\\\\\data\\\\\\\\", NULL);
	test_os_file_get_parent_dir("D:/data//a", "D:/data");
	test_os_file_get_parent_dir("D:\\data\\\\a", "D:\\data");
	test_os_file_get_parent_dir("D:///data//a///b/", "D:///data//a");
	test_os_file_get_parent_dir("D:\\\\\\data\\\\a\\\\\\b\\", "D:\\\\\\data\\\\a");
#endif  /* _WIN32 */
}
#endif /* UNIV_ENABLE_UNIT_TEST_GET_PARENT_DIR */


/** Creates all missing subdirectories along the given path.
@param[in]	path		Path name
@return DB_SUCCESS if OK, otherwise error code. */
dberr_t
os_file_create_subdirs_if_needed(
	const char*	path)
{
	if (srv_read_only_mode) {

		ib::error()
			<< "read only mode set. Can't create "
			<< "subdirectories '" << path << "'";

		return(DB_READ_ONLY);

	}

	char*	subdir = os_file_get_parent_dir(path);

	if (subdir == NULL) {
		/* subdir is root or cwd, nothing to do */
		return(DB_SUCCESS);
	}

	/* Test if subdir exists */
	os_file_type_t	type;
	bool	subdir_exists;
	bool	success = os_file_status(subdir, &subdir_exists, &type);

	if (success && !subdir_exists) {

		/* Subdir does not exist, create it */
		dberr_t	err = os_file_create_subdirs_if_needed(subdir);

		if (err != DB_SUCCESS) {

			ut_free(subdir);

			return(err);
		}

		success = os_file_create_directory(subdir, false);
	}

	ut_free(subdir);

	return(success ? DB_SUCCESS : DB_ERROR);
}

/** Allocate the buffer for IO on a transparently compressed table.
@param[in]	type		IO flags
@param[out]	buf		buffer to read or write
@param[in,out]	n		number of bytes to read/write, starting from
				offset
@return pointer to allocated page, compressed data is written to the offset
	that is aligned on the disk sector size */
static
Block*
os_file_compress_page(
	IORequest&	type,
	void*&		buf,
	ulint*		n)
{
	ut_ad(!type.is_log());
	ut_ad(type.is_write());
	ut_ad(type.is_compressed());

	ulint	n_alloc = *n * 2;

	ut_a(n_alloc <= UNIV_PAGE_SIZE_MAX * 2);
	ut_a(type.compression_algorithm().m_type != Compression::LZ4
	     || static_cast<ulint>(LZ4_COMPRESSBOUND(*n)) < n_alloc);

	Block*  block = os_alloc_block();

	ulint	old_compressed_len;
	ulint	compressed_len = *n;

	old_compressed_len = mach_read_from_2(
		reinterpret_cast<byte*>(buf)
		+ FIL_PAGE_COMPRESS_SIZE_V1);

	if (old_compressed_len > 0) {
		old_compressed_len = ut_calc_align(
			old_compressed_len + FIL_PAGE_DATA,
			type.block_size());
	} else {
		old_compressed_len = *n;
	}

	byte*	compressed_page;

	compressed_page = static_cast<byte*>(
		ut_align(block->m_ptr, os_io_ptr_align));

	byte*	buf_ptr;

	buf_ptr = os_file_compress_page(
		type.compression_algorithm(),
		type.block_size(),
		reinterpret_cast<byte*>(buf),
		*n,
		compressed_page,
		&compressed_len);

	if (buf_ptr != buf) {
		/* Set new compressed size to uncompressed page. */
		memcpy(reinterpret_cast<byte*>(buf) + FIL_PAGE_COMPRESS_SIZE_V1,
		       buf_ptr + FIL_PAGE_COMPRESS_SIZE_V1, 2);

		buf = buf_ptr;
		*n = compressed_len;

		if (compressed_len >= old_compressed_len) {

			ut_ad(old_compressed_len <= UNIV_PAGE_SIZE);

			type.clear_punch_hole();
		}
	}

	return(block);
}

/** Encrypt a page content when write it to disk.
@param[in]	type		IO flags
@param[out]	buf		buffer to read or write
@param[in,out]	n		number of bytes to read/write, starting from
				offset
@return pointer to the encrypted page */
static
Block*
os_file_encrypt_page(
	const IORequest&	type,
	void*&			buf,
	ulint*			n)
{

	byte*		encrypted_page;
	ulint		encrypted_len = *n;
	byte*		buf_ptr;
	Encryption	encryption(type.encryption_algorithm());

	ut_ad(!type.is_log());
	ut_ad(type.is_write());
	ut_ad(type.is_encrypted());

	Block*  block = os_alloc_block();

	encrypted_page = static_cast<byte*>(
		ut_align(block->m_ptr, os_io_ptr_align));

	buf_ptr = encryption.encrypt(type,
				     reinterpret_cast<byte*>(buf), *n,
				     encrypted_page, &encrypted_len);

	bool	encrypted = buf_ptr != buf;

	if (encrypted) {

		buf = buf_ptr;
		*n = encrypted_len;
	}

	return(block);
}

#ifndef _WIN32

/** Do the read/write
@param[in]	request	The IO context and type
@return the number of bytes read/written or negative value on error */
ssize_t
SyncFileIO::execute(const IORequest& request)
{
	ssize_t	n_bytes;

	if (request.is_read()) {
		n_bytes = pread(m_fh, m_buf, m_n, m_offset);
	} else {
		ut_ad(request.is_write());
		n_bytes = pwrite(m_fh, m_buf, m_n, m_offset);
	}

	return(n_bytes);
}

/** Free storage space associated with a section of the file.
@param[in]	fh		Open file handle
@param[in]	off		Starting offset (SEEK_SET)
@param[in]	len		Size of the hole
@return DB_SUCCESS or error code */
static
dberr_t
os_file_punch_hole_posix(
	os_file_t	fh,
	os_offset_t	off,
	os_offset_t	len)
{
#ifdef HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE
	const int	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;

	int             ret = fallocate(fh, mode, off, len);

	if (ret == 0) {
		return(DB_SUCCESS);
	}

	ut_a(ret == -1);

	if (errno == ENOTSUP) {
		return(DB_IO_NO_PUNCH_HOLE);
	}

	ib::warn()
		<< "fallocate(" << fh
		<<", FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, "
		<< off << ", " << len << ") returned errno: "
		<<  errno;

	return(DB_IO_ERROR);

#elif defined(UNIV_SOLARIS)

	// Use F_FREESP

#endif /* HAVE_FALLOC_PUNCH_HOLE_AND_KEEP_SIZE */

	return(DB_IO_NO_PUNCH_HOLE);
}

#if defined(LINUX_NATIVE_AIO)

/** Linux native AIO handler */
class LinuxAIOHandler {
public:
	/**
	@param[in] global_segment	The global segment*/
	LinuxAIOHandler(ulint global_segment)
		:
		m_global_segment(global_segment)
	{
		/* Should never be doing Sync IO here. */
		ut_a(m_global_segment != ULINT_UNDEFINED);

		/* Find the array and the local segment. */

		m_segment = AIO::get_array_and_local_segment(
			&m_array, m_global_segment);

		m_n_slots = m_array->slots_per_segment();
	}

	/** Destructor */
	~LinuxAIOHandler()
	{
		// No op
	}

	/**
	Process a Linux AIO request
	@param[out]	m1		the messages passed with the
	@param[out]	m2		AIO request; note that in case the
					AIO operation failed, these output
					parameters are valid and can be used to
					restart the operation.
	@param[out]	request		IO context
	@return DB_SUCCESS or error code */
	dberr_t poll(fil_node_t** m1, void** m2, IORequest* request);

private:
	/** Resubmit an IO request that was only partially successful
	@param[in,out]	slot		Request to resubmit
	@return DB_SUCCESS or DB_FAIL if the IO resubmit request failed */
	dberr_t	resubmit(Slot* slot);

	/** Check if the AIO succeeded
	@param[in,out]	slot		The slot to check
	@return DB_SUCCESS, DB_FAIL if the operation should be retried or
		DB_IO_ERROR on all other errors */
	dberr_t	check_state(Slot* slot);

	/** @return true if a shutdown was detected */
	bool is_shutdown() const
	{
		return(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
		       && !buf_page_cleaner_is_active);
	}

	/** If no slot was found then the m_array->m_mutex will be released.
	@param[out]	n_pending	The number of pending IOs
	@return NULL or a slot that has completed IO */
	Slot* find_completed_slot(ulint* n_pending);

	/** This is called from within the IO-thread. If there are no completed
	IO requests in the slot array, the thread calls this function to
	collect more requests from the Linux kernel.
	The IO-thread waits on io_getevents(), which is a blocking call, with
	a timeout value. Unless the system is very heavy loaded, keeping the
	IO-thread very busy, the io-thread will spend most of its time waiting
	in this function.
	The IO-thread also exits in this function. It checks server status at
	each wakeup and that is why we use timed wait in io_getevents(). */
	void collect();

private:
	/** Slot array */
	AIO*			m_array;

	/** Number of slots inthe local segment */
	ulint			m_n_slots;

	/** The local segment to check */
	ulint			m_segment;

	/** The global segment */
	ulint			m_global_segment;
};

/** Resubmit an IO request that was only partially successful
@param[in,out]	slot		Request to resubmit
@return DB_SUCCESS or DB_FAIL if the IO resubmit request failed */
dberr_t
LinuxAIOHandler::resubmit(Slot* slot)
{
#ifdef UNIV_DEBUG
	/* Bytes already read/written out */
	ulint	n_bytes = slot->ptr - slot->buf;

	ut_ad(m_array->is_mutex_owned());

	ut_ad(n_bytes < slot->original_len);
	ut_ad(static_cast<ulint>(slot->n_bytes) < slot->original_len - n_bytes);
	/* Partial read or write scenario */
	ut_ad(slot->len >= static_cast<ulint>(slot->n_bytes));
#endif /* UNIV_DEBUG */

	slot->len -= slot->n_bytes;
	slot->ptr += slot->n_bytes;
	slot->offset += slot->n_bytes;

	/* Resetting the bytes read/written */
	slot->n_bytes = 0;
	slot->io_already_done = false;

	struct iocb*	iocb = &slot->control;
	if (slot->type.is_read()) {
		io_prep_pread(
			iocb,
			slot->file.m_file,
			slot->ptr,
			slot->len,
			static_cast<off_t>(slot->offset));

	} else {

		ut_a(slot->type.is_write());

		io_prep_pwrite(
			iocb,
			slot->file.m_file,
			slot->ptr,
			slot->len,
			static_cast<off_t>(slot->offset));
	}

	iocb->data = slot;

	/* Resubmit an I/O request */
	int	ret = io_submit(m_array->io_ctx(m_segment), 1, &iocb);

	if (ret < -1)  {
		errno = -ret;
	}

	return(ret < 0 ? DB_IO_PARTIAL_FAILED : DB_SUCCESS);
}

/** Check if the AIO succeeded
@param[in,out]	slot		The slot to check
@return DB_SUCCESS, DB_FAIL if the operation should be retried or
	DB_IO_ERROR on all other errors */
dberr_t
LinuxAIOHandler::check_state(Slot* slot)
{
	ut_ad(m_array->is_mutex_owned());

	/* Note that it may be that there is more then one completed
	IO requests. We process them one at a time. We may have a case
	here to improve the performance slightly by dealing with all
	requests in one sweep. */

	srv_set_io_thread_op_info(
		m_global_segment, "processing completed aio requests");

	ut_ad(slot->io_already_done);

	dberr_t	err;

	if (slot->ret == 0) {

		err = AIOHandler::post_io_processing(slot);

	} else {
		errno = -slot->ret;

		/* os_file_handle_error does tell us if we should retry
		this IO. As it stands now, we don't do this retry when
		reaping requests from a different context than
		the dispatcher. This non-retry logic is the same for
		Windows and Linux native AIO.
		We should probably look into this to transparently
		re-submit the IO. */
		os_file_handle_error(slot->name, "Linux aio");

		err = DB_IO_ERROR;
	}

	return(err);
}

/** If no slot was found then the m_array->m_mutex will be released.
@param[out]	n_pending		The number of pending IOs
@return NULL or a slot that has completed IO */
Slot*
LinuxAIOHandler::find_completed_slot(ulint* n_pending)
{
	ulint	offset = m_n_slots * m_segment;

	*n_pending = 0;

	m_array->acquire();

	Slot*	slot = m_array->at(offset);

	for (ulint i = 0; i < m_n_slots; ++i, ++slot) {

		if (slot->is_reserved) {

			++*n_pending;

			if (slot->io_already_done) {

				/* Something for us to work on.
				Note: We don't release the mutex. */
				return(slot);
			}
		}
	}

	m_array->release();

	return(NULL);
}

/** This function is only used in Linux native asynchronous i/o. This is
called from within the io-thread. If there are no completed IO requests
in the slot array, the thread calls this function to collect more
requests from the kernel.
The io-thread waits on io_getevents(), which is a blocking call, with
a timeout value. Unless the system is very heavy loaded, keeping the
io-thread very busy, the io-thread will spend most of its time waiting
in this function.
The io-thread also exits in this function. It checks server status at
each wakeup and that is why we use timed wait in io_getevents(). */
void
LinuxAIOHandler::collect()
{
	ut_ad(m_n_slots > 0);
	ut_ad(m_array != NULL);
	ut_ad(m_segment < m_array->get_n_segments());

	/* Which io_context we are going to use. */
	io_context*	io_ctx = m_array->io_ctx(m_segment);

	/* Starting point of the m_segment we will be working on. */
	ulint	start_pos = m_segment * m_n_slots;

	/* End point. */
	ulint	end_pos = start_pos + m_n_slots;

	for (;;) {
		struct io_event*	events;

		/* Which part of event array we are going to work on. */
		events = m_array->io_events(m_segment * m_n_slots);

		/* Initialize the events. */
		memset(events, 0, sizeof(*events) * m_n_slots);

		/* The timeout value is arbitrary. We probably need
		to experiment with it a little. */
		struct timespec		timeout;

		timeout.tv_sec = 0;
		timeout.tv_nsec = OS_AIO_REAP_TIMEOUT;

		int	ret;

		ret = io_getevents(io_ctx, 1, m_n_slots, events, &timeout);

		for (int i = 0; i < ret; ++i) {

			struct iocb*	iocb;

			iocb = reinterpret_cast<struct iocb*>(events[i].obj);
			ut_a(iocb != NULL);

			Slot*	slot = reinterpret_cast<Slot*>(iocb->data);

			/* Some sanity checks. */
			ut_a(slot != NULL);
			ut_a(slot->is_reserved);

			/* We are not scribbling previous segment. */
			ut_a(slot->pos >= start_pos);

			/* We have not overstepped to next segment. */
			ut_a(slot->pos < end_pos);

			/* We never compress/decompress the first page */

			if (slot->offset > 0
			    && !slot->skip_punch_hole
			    && slot->type.is_compression_enabled()
			    && !slot->type.is_log()
			    && slot->type.is_write()
			    && slot->type.is_compressed()
			    && slot->type.punch_hole()) {

				slot->err = AIOHandler::io_complete(slot);
			} else {
				slot->err = DB_SUCCESS;
			}

			/* Mark this request as completed. The error handling
			will be done in the calling function. */
			m_array->acquire();

			slot->ret = events[i].res2;
			slot->io_already_done = true;
			slot->n_bytes = events[i].res;

			m_array->release();
		}

		if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
		    || !buf_page_cleaner_is_active
		    || ret > 0) {

			break;
		}

		/* This error handling is for any error in collecting the
		IO requests. The errors, if any, for any particular IO
		request are simply passed on to the calling routine. */

		switch (ret) {
		case -EAGAIN:
			/* Not enough resources! Try again. */

		case -EINTR:
			/* Interrupted! The behaviour in case of an interrupt.
			If we have some completed IOs available then the
			return code will be the number of IOs. We get EINTR
			only if there are no completed IOs and we have been
			interrupted. */

		case 0:
			/* No pending request! Go back and check again. */

			continue;
		}

		/* All other errors should cause a trap for now. */
		ib::fatal()
			<< "Unexpected ret_code[" << ret
			<< "] from io_getevents()!";

		break;
	}
}

/** Process a Linux AIO request
@param[out]	m1		the messages passed with the
@param[out]	m2		AIO request; note that in case the
				AIO operation failed, these output
				parameters are valid and can be used to
				restart the operation.
@param[out]	request		IO context
@return DB_SUCCESS or error code */
dberr_t
LinuxAIOHandler::poll(fil_node_t** m1, void** m2, IORequest* request)
{
	dberr_t		err;
	Slot*		slot;

	/* Loop until we have found a completed request. */
	for (;;) {

		ulint	n_pending;

		slot = find_completed_slot(&n_pending);

		if (slot != NULL) {

			ut_ad(m_array->is_mutex_owned());

			err = check_state(slot);

			/* DB_FAIL is not a hard error, we should retry */
			if (err != DB_FAIL) {
				break;
			}

			/* Partial IO, resubmit request for
			remaining bytes to read/write */
			err = resubmit(slot);

			if (err != DB_SUCCESS) {
				break;
			}

			m_array->release();

		} else if (is_shutdown() && n_pending == 0) {

			/* There is no completed request. If there is
			no pending request at all, and the system is
			being shut down, exit. */

			*m1 = NULL;
			*m2 = NULL;

			return(DB_SUCCESS);

		} else {

			/* Wait for some request. Note that we return
			from wait if we have found a request. */

			srv_set_io_thread_op_info(
				m_global_segment,
				"waiting for completed aio requests");

			collect();
		}
	}

	if (err == DB_IO_PARTIAL_FAILED) {
		/* Aborting in case of submit failure */
		ib::fatal()
			<< "Native Linux AIO interface. "
			"io_submit() call failed when "
			"resubmitting a partial I/O "
			"request on the file " << slot->name
			<< ".";
	}

	*m1 = slot->m1;
	*m2 = slot->m2;

	*request = slot->type;

	m_array->release(slot);

	m_array->release();

	return(err);
}

/** This function is only used in Linux native asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait for
the completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!

@param[in]	global_seg	segment number in the aio array
				to wait for; segment 0 is the ibuf
				i/o thread, segment 1 is log i/o thread,
				then follow the non-ibuf read threads,
				and the last are the non-ibuf write
				threads.
@param[out]	m1		the messages passed with the
@param[out]	m2			AIO request; note that in case the
				AIO operation failed, these output
				parameters are valid and can be used to
				restart the operation.
@param[out]xi	 request	IO context
@return DB_SUCCESS if the IO was successful */
static
dberr_t
os_aio_linux_handler(
	ulint		global_segment,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	request)
{
	LinuxAIOHandler	handler(global_segment);

	dberr_t	err = handler.poll(m1, m2, request);

	if (err == DB_IO_NO_PUNCH_HOLE) {
		fil_no_punch_hole(*m1);
		err = DB_SUCCESS;
	}

	return(err);
}

/** Dispatch an AIO request to the kernel.
@param[in,out]	slot		an already reserved slot
@return true on success. */
bool
AIO::linux_dispatch(Slot* slot)
{
	ut_a(slot->is_reserved);
	ut_ad(slot->type.validate());

	/* Find out what we are going to work with.
	The iocb struct is directly in the slot.
	The io_context is one per segment. */

	ulint		io_ctx_index;
	struct iocb*	iocb = &slot->control;

	io_ctx_index = (slot->pos * m_n_segments) / m_slots.size();

	int	ret = io_submit(m_aio_ctx[io_ctx_index], 1, &iocb);

	/* io_submit() returns number of successfully queued requests
	or -errno. */

	if (ret != 1) {
		errno = -ret;
	}

	return(ret == 1);
}

/** Creates an io_context for native linux AIO.
@param[in]	max_events	number of events
@param[out]	io_ctx		io_ctx to initialize.
@return true on success. */
bool
AIO::linux_create_io_ctx(
	ulint		max_events,
	io_context_t*	io_ctx)
{
	ssize_t		n_retries = 0;

	for (;;) {

		memset(io_ctx, 0x0, sizeof(*io_ctx));

		/* Initialize the io_ctx. Tell it how many pending
		IO requests this context will handle. */

		int	ret = io_setup(max_events, io_ctx);

		if (ret == 0) {
			/* Success. Return now. */
			return(true);
		}

		/* If we hit EAGAIN we'll make a few attempts before failing. */

		switch (ret) {
		case -EAGAIN:
			if (n_retries == 0) {
				/* First time around. */
				ib::warn()
					<< "io_setup() failed with EAGAIN."
					" Will make "
					<< OS_AIO_IO_SETUP_RETRY_ATTEMPTS
					<< " attempts before giving up.";
			}

			if (n_retries < OS_AIO_IO_SETUP_RETRY_ATTEMPTS) {

				++n_retries;

				ib::warn()
					<< "io_setup() attempt "
					<< n_retries << ".";

				os_thread_sleep(OS_AIO_IO_SETUP_RETRY_SLEEP);

				continue;
			}

			/* Have tried enough. Better call it a day. */
			ib::error()
				<< "io_setup() failed with EAGAIN after "
				<< OS_AIO_IO_SETUP_RETRY_ATTEMPTS
				<< " attempts.";
			break;

		case -ENOSYS:
			ib::error()
				<< "Linux Native AIO interface"
				" is not supported on this platform. Please"
				" check your OS documentation and install"
				" appropriate binary of InnoDB.";

			break;

		default:
			ib::error()
				<< "Linux Native AIO setup"
				<< " returned following error["
				<< ret << "]";
			break;
		}

		ib::info()
			<< "You can disable Linux Native AIO by"
			" setting innodb_use_native_aio = 0 in my.cnf";

		break;
	}

	return(false);
}

/** Checks if the system supports native linux aio. On some kernel
versions where native aio is supported it won't work on tmpfs. In such
cases we can't use native aio as it is not possible to mix simulated
and native aio.
@return: true if supported, false otherwise. */
bool
AIO::is_linux_native_aio_supported()
{
	int		fd;
	io_context_t	io_ctx;
	char		name[1000];

	if (!linux_create_io_ctx(1, &io_ctx)) {

		/* The platform does not support native aio. */

		return(false);

	} else if (!srv_read_only_mode) {

		/* Now check if tmpdir supports native aio ops. */
		fd = innobase_mysql_tmpfile(NULL);

		if (fd < 0) {
			ib::warn()
				<< "Unable to create temp file to check"
				" native AIO support.";

			return(false);
		}
	} else {

		os_normalize_path(srv_log_group_home_dir);

		ulint	dirnamelen = strlen(srv_log_group_home_dir);

		ut_a(dirnamelen < (sizeof name) - 10 - sizeof "ib_logfile");

		memcpy(name, srv_log_group_home_dir, dirnamelen);

		/* Add a path separator if needed. */
		if (dirnamelen && name[dirnamelen - 1] != OS_PATH_SEPARATOR) {

			name[dirnamelen++] = OS_PATH_SEPARATOR;
		}

		strcpy(name + dirnamelen, "ib_logfile0");

		fd = ::open(name, O_RDONLY);

		if (fd == -1) {

			ib::warn()
				<< "Unable to open"
				<< " \"" << name << "\" to check native"
				<< " AIO read support.";

			return(false);
		}
	}

	struct io_event	io_event;

	memset(&io_event, 0x0, sizeof(io_event));

	byte*	buf = static_cast<byte*>(ut_malloc_nokey(UNIV_PAGE_SIZE * 2));
	byte*	ptr = static_cast<byte*>(ut_align(buf, UNIV_PAGE_SIZE));

	struct iocb	iocb;

	/* Suppress valgrind warning. */
	memset(buf, 0x00, UNIV_PAGE_SIZE * 2);
	memset(&iocb, 0x0, sizeof(iocb));

	struct iocb*	p_iocb = &iocb;

	if (!srv_read_only_mode) {

		io_prep_pwrite(p_iocb, fd, ptr, UNIV_PAGE_SIZE, 0);

	} else {
		ut_a(UNIV_PAGE_SIZE >= 512);
		io_prep_pread(p_iocb, fd, ptr, 512, 0);
	}

	int	err = io_submit(io_ctx, 1, &p_iocb);

	if (err >= 1) {
		/* Now collect the submitted IO request. */
		err = io_getevents(io_ctx, 1, 1, &io_event, NULL);
	}

	ut_free(buf);
	close(fd);

	switch (err) {
	case 1:
		return(true);

	case -EINVAL:
	case -ENOSYS:
		ib::error()
			<< "Linux Native AIO not supported. You can either"
			" move "
			<< (srv_read_only_mode ? name : "tmpdir")
			<< " to a file system that supports native"
			" AIO or you can set innodb_use_native_aio to"
			" FALSE to avoid this message.";

		/* fall through. */
	default:
		ib::error()
			<< "Linux Native AIO check on "
			<< (srv_read_only_mode ? name : "tmpdir")
			<< "returned error[" << -err << "]";
	}

	return(false);
}

#endif /* LINUX_NATIVE_AIO */

/** Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@param[in]	report_all_errors	true if we want an error message
					printed of all errors
@param[in]	on_error_silent		true then don't print any diagnostic
					to the log
@return error number, or OS error number + 100 */
static
ulint
os_file_get_last_error_low(
	bool	report_all_errors,
	bool	on_error_silent)
{
	int	err = errno;

	if (err == 0) {
		return(0);
	}

	if (report_all_errors
	    || (err != ENOSPC && err != EEXIST && !on_error_silent)) {

		ib::error()
			<< "Operating system error number "
			<< err
			<< " in a file operation.";

		if (err == ENOENT) {

			ib::error()
				<< "The error means the system"
				" cannot find the path specified.";

			if (srv_is_being_started) {

				ib::error()
					<< "If you are installing InnoDB,"
					" remember that you must create"
					" directories yourself, InnoDB"
					" does not create them.";
			}
		} else if (err == EACCES) {

			ib::error()
				<< "The error means mysqld does not have"
				" the access rights to the directory.";

		} else {
			if (strerror(err) != NULL) {

				ib::error()
					<< "Error number " << err << " means '"
					<< strerror(err) << "'";
			}

			ib::info() << OPERATING_SYSTEM_ERROR_MSG;
		}
	}

	switch (err) {
	case ENOSPC:
		return(OS_FILE_DISK_FULL);
	case ENOENT:
		return(OS_FILE_NOT_FOUND);
	case EEXIST:
		return(OS_FILE_ALREADY_EXISTS);
	case EXDEV:
	case ENOTDIR:
	case EISDIR:
		return(OS_FILE_PATH_ERROR);
	case EAGAIN:
		if (srv_use_native_aio) {
			return(OS_FILE_AIO_RESOURCES_RESERVED);
		}
		break;
	case EINTR:
		if (srv_use_native_aio) {
			return(OS_FILE_AIO_INTERRUPTED);
		}
		break;
	case EACCES:
		return(OS_FILE_ACCESS_VIOLATION);
	}
	return(OS_FILE_ERROR_MAX + err);
}

/** Wrapper to fsync(2) that retries the call on some errors.
Returns the value 0 if successful; otherwise the value -1 is returned and
the global variable errno is set to indicate the error.
@param[in]	file		open file handle
@return 0 if success, -1 otherwise */
static
int
os_file_fsync_posix(
	os_file_t	file)
{
	ulint		failures = 0;

	for (;;) {

		++os_n_fsyncs;

		int	ret = fsync(file);

		if (ret == 0) {
			return(ret);
		}

		switch(errno) {
		case ENOLCK:

			++failures;
			ut_a(failures < 1000);

			if (!(failures % 100)) {

				ib::warn()
					<< "fsync(): "
					<< "No locks available; retrying";
			}

			/* 0.2 sec */
			os_thread_sleep(200000);
			break;

		case EIO:

                        ib::fatal()
				<< "fsync() returned EIO, aborting.";
			break;

		case EINTR:

			++failures;
			ut_a(failures < 2000);
			break;

		default:
			ut_error;
			break;
		}
	}

	ut_error;

	return(-1);
}

/** Check the existence and type of the given file.
@param[in]	path		path name of file
@param[out]	exists		true if the file exists
@param[out]	type		Type of the file, if it exists
@return true if call succeeded */
bool
os_file_status_posix(
	const char*	path,
	bool*		exists,
	os_file_type_t* type)
{
	struct stat	statinfo;

	int	ret = stat(path, &statinfo);

	*exists = !ret;

	if (!ret) {
		/* file exists, everything OK */

	} else if (errno == ENOENT || errno == ENOTDIR
		   || errno == ENAMETOOLONG) {
		/* file does not exist */
		return(true);

	} else {
		/* file exists, but stat call failed */
		os_file_handle_error_no_exit(path, "stat", false);
		return(false);
	}

	if (S_ISDIR(statinfo.st_mode)) {
		*type = OS_FILE_TYPE_DIR;

	} else if (S_ISLNK(statinfo.st_mode)) {
		*type = OS_FILE_TYPE_LINK;

	} else if (S_ISREG(statinfo.st_mode)) {
		*type = OS_FILE_TYPE_FILE;

	} else {
		*type = OS_FILE_TYPE_UNKNOWN;
	}

	return(true);
}

/** NOTE! Use the corresponding macro os_file_flush(), not directly this
function!
Flushes the write buffers of a given file to the disk.
@param[in]	file		handle to a file
@return true if success */
bool
os_file_flush_func(
	os_file_t	file)
{
	int	ret;

	ret = os_file_fsync_posix(file);

	if (ret == 0) {
		return(true);
	}

	/* Since Linux returns EINVAL if the 'file' is actually a raw device,
	we choose to ignore that error if we are using raw disks */

	if (srv_start_raw_disk_in_use && errno == EINVAL) {

		return(true);
	}

	ib::error() << "The OS said file flush did not succeed";

	os_file_handle_error(NULL, "flush");

	/* It is a fatal error if a file flush does not succeed, because then
	the database can get corrupt on disk */
	ut_error;

	return(false);
}

/** NOTE! Use the corresponding macro os_file_create_simple(), not directly
this function!
A simple function to open or create a file.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY or OS_FILE_READ_WRITE
@param[in]	read_only	if true, read only checks are enforced
@param[out]	success		true if succeed, false if error
@return handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_simple_func(
	const char*	name,
	ulint		create_mode,
	ulint		access_type,
	bool		read_only,
	bool*		success)
{
	pfs_os_file_t	file;

	*success = false;

	int		create_flag;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		if (access_type == OS_FILE_READ_ONLY) {

			create_flag = O_RDONLY;

		} else if (read_only) {

			create_flag = O_RDONLY;

		} else {
			create_flag = O_RDWR;
		}

	} else if (read_only) {

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else if (create_mode == OS_FILE_CREATE_PATH) {

		/* Create subdirs along the path if needed. */

		*success = os_file_create_subdirs_if_needed(name);

		if (!*success) {

			ib::error()
				<< "Unable to create subdirectories '"
				<< name << "'";

			file.m_file = OS_FILE_CLOSED;
			return(file);
		}

		create_flag = O_RDWR | O_CREAT | O_EXCL;
		create_mode = OS_FILE_CREATE;
	} else {

		ib::error()
			<< "Unknown file create mode ("
			<< create_mode
			<< " for file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	bool	retry;

	do {
		file.m_file = ::open(name, create_flag, os_innodb_umask);

		if (file.m_file == -1) {
			*success = false;

			retry = os_file_handle_error(
				name,
				create_mode == OS_FILE_OPEN
				? "open" : "create");
		} else {
			*success = true;
			retry = false;
		}

	} while (retry);

#ifdef USE_FILE_LOCK
	if (!read_only
	    && *success
	    && access_type == OS_FILE_READ_WRITE
	    && os_file_lock(file.m_file, name)) {

		*success = false;
		close(file.m_file);
		file.m_file = -1;
	}
#endif /* USE_FILE_LOCK */

	return(file);
}

/** This function attempts to create a directory named pathname. The new
directory gets default permissions. On Unix the permissions are
(0770 & ~umask). If the directory exists already, nothing is done and
the call succeeds, unless the fail_if_exists arguments is true.
If another error occurs, such as a permission error, this does not crash,
but reports the error and returns false.
@param[in]	pathname	directory name as null-terminated string
@param[in]	fail_if_exists	if true, pre-existing directory is treated as
				an error.
@return true if call succeeds, false on error */
bool
os_file_create_directory(
	const char*	pathname,
	bool		fail_if_exists)
{
	int	rcode = mkdir(pathname, 0770);

	if (!(rcode == 0 || (errno == EEXIST && !fail_if_exists))) {
		/* failure */
		os_file_handle_error_no_exit(pathname, "mkdir", false);

		return(false);
	}

	return(true);
}

/**
The os_file_opendir() function opens a directory stream corresponding to the
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
	bool		error_is_fatal)
{
	os_file_dir_t		dir;
	dir = opendir(dirname);

	if (dir == NULL && error_is_fatal) {
		os_file_handle_error(dirname, "opendir");
	}

	return(dir);
}

/** Closes a directory stream.
@param[in]	dir		directory stream
@return 0 if success, -1 if failure */
int
os_file_closedir(
	os_file_dir_t	dir)
{
	int	ret = closedir(dir);

	if (ret != 0) {
		os_file_handle_error_no_exit(NULL, "closedir", false);
	}

	return(ret);
}

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
	os_file_stat_t*	info)
{
	struct dirent*	ent;
	char*		full_path;
	int		ret;
	struct stat	statinfo;

#ifdef HAVE_READDIR_R
	char		dirent_buf[sizeof(struct dirent)
				   + _POSIX_PATH_MAX + 100];
	/* In /mysys/my_lib.c, _POSIX_PATH_MAX + 1 is used as
	the max file name len; but in most standards, the
	length is NAME_MAX; we add 100 to be even safer */
#endif /* HAVE_READDIR_R */

next_file:

#ifdef HAVE_READDIR_R
	ret = readdir_r(dir, (struct dirent*) dirent_buf, &ent);

	if (ret != 0) {

		ib::error()
			<< "Cannot read directory " << dirname
			<< " error: " << ret;

		return(-1);
	}

	if (ent == NULL) {
		/* End of directory */

		return(1);
	}

	ut_a(strlen(ent->d_name) < _POSIX_PATH_MAX + 100 - 1);
#else
	ent = readdir(dir);

	if (ent == NULL) {

		return(1);
	}
#endif /* HAVE_READDIR_R */
	ut_a(strlen(ent->d_name) < OS_FILE_MAX_PATH);

	if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {

		goto next_file;
	}

	strcpy(info->name, ent->d_name);

	full_path = static_cast<char*>(
		ut_malloc_nokey(strlen(dirname) + strlen(ent->d_name) + 10));

	sprintf(full_path, "%s/%s", dirname, ent->d_name);

	ret = stat(full_path, &statinfo);

	if (ret) {

		if (errno == ENOENT) {
			/* readdir() returned a file that does not exist,
			it must have been deleted in the meantime. Do what
			would have happened if the file was deleted before
			readdir() - ignore and go to the next entry.
			If this is the last entry then info->name will still
			contain the name of the deleted file when this
			function returns, but this is not an issue since the
			caller shouldn't be looking at info when end of
			directory is returned. */

			ut_free(full_path);

			goto next_file;
		}

		os_file_handle_error_no_exit(full_path, "stat", false);

		ut_free(full_path);

		return(-1);
	}

	info->size = statinfo.st_size;

	if (S_ISDIR(statinfo.st_mode)) {
		info->type = OS_FILE_TYPE_DIR;
	} else if (S_ISLNK(statinfo.st_mode)) {
		info->type = OS_FILE_TYPE_LINK;
	} else if (S_ISREG(statinfo.st_mode)) {
		info->type = OS_FILE_TYPE_FILE;
	} else {
		info->type = OS_FILE_TYPE_UNKNOWN;
	}

	ut_free(full_path);

	return(0);
}

/** NOTE! Use the corresponding macro os_file_create(), not directly
this function!
Opens an existing file or creates a new.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	purpose		OS_FILE_AIO, if asynchronous, non-buffered I/O
				is desired, OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use async
				I/O or unbuffered I/O: look in the function
				source code for the exact rules
@param[in]	type		OS_DATA_FILE or OS_LOG_FILE
@param[in]	read_only	true, if read only checks should be enforcedm
@param[in]	success		true if succeeded
@return handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_func(
	const char*	name,
	ulint		create_mode,
	ulint		purpose,
	ulint		type,
	bool		read_only,
	bool*		success)
{
	bool		on_error_no_exit;
	bool		on_error_silent;
	pfs_os_file_t	file;

	*success = false;

	DBUG_EXECUTE_IF(
		"ib_create_table_fail_disk_full",
		*success = false;
		errno = ENOSPC;
		file.m_file = OS_FILE_CLOSED;
		return(file);
	);

	int		create_flag;
	const char*	mode_str	= NULL;

	on_error_no_exit = create_mode & OS_FILE_ON_ERROR_NO_EXIT
		? true : false;
	on_error_silent = create_mode & OS_FILE_ON_ERROR_SILENT
		? true : false;

	create_mode &= ~OS_FILE_ON_ERROR_NO_EXIT;
	create_mode &= ~OS_FILE_ON_ERROR_SILENT;

	if (create_mode == OS_FILE_OPEN
	    || create_mode == OS_FILE_OPEN_RAW
	    || create_mode == OS_FILE_OPEN_RETRY) {

		mode_str = "OPEN";

		create_flag = read_only ? O_RDONLY : O_RDWR;

	} else if (read_only) {

		mode_str = "OPEN";

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		mode_str = "CREATE";
		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else if (create_mode == OS_FILE_OVERWRITE) {

		mode_str = "OVERWRITE";
		create_flag = O_RDWR | O_CREAT | O_TRUNC;

	} else {
		ib::error()
			<< "Unknown file create mode (" << create_mode << ")"
			<< " for file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	ut_a(type == OS_LOG_FILE
	     || type == OS_DATA_FILE
	     || type == OS_DATA_TEMP_FILE);

	ut_a(purpose == OS_FILE_AIO || purpose == OS_FILE_NORMAL);

#ifdef O_SYNC
	/* We let O_SYNC only affect log files; note that we map O_DSYNC to
	O_SYNC because the datasync options seemed to corrupt files in 2001
	in both Linux and Solaris */

	if (!read_only
	    && type == OS_LOG_FILE
	    && srv_unix_file_flush_method == SRV_UNIX_O_DSYNC) {

		create_flag |= O_SYNC;
	}
#endif /* O_SYNC */

	bool		retry;

	do {
		file.m_file = ::open(name, create_flag, os_innodb_umask);

		if (file.m_file == -1) {
			const char*	operation;

			operation = (create_mode == OS_FILE_CREATE
				     && !read_only) ? "create" : "open";

			*success = false;

			if (on_error_no_exit) {
				retry = os_file_handle_error_no_exit(
					name, operation, on_error_silent);
			} else {
				retry = os_file_handle_error(name, operation);
			}
		} else {
			*success = true;
			retry = false;
		}

	} while (retry);

	/* We disable OS caching (O_DIRECT) only on data files */

	if (!read_only
	    && *success
	    && (type != OS_LOG_FILE && type != OS_DATA_TEMP_FILE)
	    && (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT
		|| srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC)) {

		os_file_set_nocache(file.m_file, name, mode_str);
	}

#ifdef USE_FILE_LOCK
	if (!read_only
	    && *success
	    && create_mode != OS_FILE_OPEN_RAW
	    && os_file_lock(file.m_file, name)) {

		if (create_mode == OS_FILE_OPEN_RETRY) {

			ib::info()
				<< "Retrying to lock the first data file";

			for (int i = 0; i < 100; i++) {
				os_thread_sleep(1000000);

				if (!os_file_lock(file.m_file, name)) {
					*success = true;
					return(file);
				}
			}

			ib::info()
				<< "Unable to open the first data file";
		}

		*success = false;
		close(file.m_file);
		file.m_file = -1;
	}
#endif /* USE_FILE_LOCK */

	return(file);
}

/** NOTE! Use the corresponding macro
os_file_create_simple_no_error_handling(), not directly this function!
A simple function to open or create a file.
@param[in]	name		name of the file or path as a null-terminated
				string
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
{
	pfs_os_file_t	file;
	int		create_flag;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	*success = false;

	if (create_mode == OS_FILE_OPEN) {

		if (access_type == OS_FILE_READ_ONLY) {

			create_flag = O_RDONLY;

		} else if (read_only) {

			create_flag = O_RDONLY;

		} else {

			ut_a(access_type == OS_FILE_READ_WRITE
			     || access_type == OS_FILE_READ_ALLOW_DELETE);

			create_flag = O_RDWR;
		}

	} else if (read_only) {

		create_flag = O_RDONLY;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = O_RDWR | O_CREAT | O_EXCL;

	} else {

		ib::error()
			<< "Unknown file create mode "
			<< create_mode << " for file '" << name << "'";
		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	file.m_file = ::open(name, create_flag, os_innodb_umask);

	*success = (file.m_file != -1);

#ifdef USE_FILE_LOCK
	if (!read_only
	    && *success
	    && access_type == OS_FILE_READ_WRITE
	    && os_file_lock(file.m_file, name)) {

		*success = false;
		close(file.m_file);
		file.m_file = -1;

	}
#endif /* USE_FILE_LOCK */

	return(file);
}

/** Deletes a file if it exists. The file has to be closed before calling this.
@param[in]	name		file path as a null-terminated string
@param[out]	exist		indicate if file pre-exist
@return true if success */
bool
os_file_delete_if_exists_func(
	const char*	name,
	bool*		exist)
{
	if (exist != NULL) {
		*exist = true;
	}

	int	ret = unlink(name);

	if (ret != 0 && errno == ENOENT) {
		if (exist != NULL) {
			*exist = false;
		}
	} else if (ret != 0 && errno != ENOENT) {
		os_file_handle_error_no_exit(name, "delete", false);

		return(false);
	}

	return(true);
}

/** Deletes a file. The file has to be closed before calling this.
@param[in]	name		file path as a null-terminated string
@return true if success */
bool
os_file_delete_func(
	const char*	name)
{
	int	ret = unlink(name);

	if (ret != 0) {
		os_file_handle_error_no_exit(name, "delete", false);

		return(false);
	}

	return(true);
}

/** NOTE! Use the corresponding macro os_file_rename(), not directly this
function!
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@param[in]	oldpath		old file path as a null-terminated string
@param[in]	newpath		new file path
@return true if success */
bool
os_file_rename_func(
	const char*	oldpath,
	const char*	newpath)
{
#ifdef UNIV_DEBUG
	os_file_type_t	type;
	bool		exists;

	/* New path must not exist. */
	ut_ad(os_file_status(newpath, &exists, &type));
	ut_ad(!exists);

	/* Old path must exist. */
	ut_ad(os_file_status(oldpath, &exists, &type));
	ut_ad(exists);
#endif /* UNIV_DEBUG */

	int	ret = rename(oldpath, newpath);

	if (ret != 0) {
		os_file_handle_error_no_exit(oldpath, "rename", false);

		return(false);
	}

	return(true);
}

/** NOTE! Use the corresponding macro os_file_close(), not directly this
function!
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@param[in]	file		Handle to close
@return true if success */
bool
os_file_close_func(
	os_file_t	file)
{
	int	ret = close(file);

	if (ret == -1) {
		os_file_handle_error(NULL, "close");

		return(false);
	}

	return(true);
}

/** Gets a file size.
@param[in]	file		handle to an open file
@return file size, or (os_offset_t) -1 on failure */
os_offset_t
os_file_get_size(
	pfs_os_file_t	file)
{
	/* Store current position */
	os_offset_t	pos = lseek(file.m_file, 0, SEEK_CUR);
	os_offset_t	file_size = lseek(file.m_file, 0, SEEK_END);

	/* Restore current position as the function should not change it */
	lseek(file.m_file, pos, SEEK_SET);

	return(file_size);
}

/** Gets a file size.
@param[in]	filename	Full path to the filename to check
@return file size if OK, else set m_total_size to ~0 and m_alloc_size to
	errno */
os_file_size_t
os_file_get_size(
	const char*	filename)
{
	struct stat	s;
	os_file_size_t	file_size;

	int	ret = stat(filename, &s);

	if (ret == 0) {
		file_size.m_total_size = s.st_size;
		/* st_blocks is in 512 byte sized blocks */
		file_size.m_alloc_size = s.st_blocks * 512;
	} else {
		file_size.m_total_size = ~0;
		file_size.m_alloc_size = (os_offset_t) errno;
	}

	return(file_size);
}

/** This function returns information about the specified file
@param[in]	path		pathname of the file
@param[out]	stat_info	information of a file in a directory
@param[in,out]	statinfo	information of a file in a directory
@param[in]	check_rw_perm	for testing whether the file can be opened
				in RW mode
@param[in]	read_only	if true read only mode checks are enforced
@return DB_SUCCESS if all OK */
static
dberr_t
os_file_get_status_posix(
	const char*	path,
	os_file_stat_t* stat_info,
	struct stat*	statinfo,
	bool		check_rw_perm,
	bool		read_only)
{
	int	ret = stat(path, statinfo);

	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(DB_NOT_FOUND);

	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", false);

		return(DB_FAIL);
	}

	switch (statinfo->st_mode & S_IFMT) {
	case S_IFDIR:
		stat_info->type = OS_FILE_TYPE_DIR;
		break;
	case S_IFLNK:
		stat_info->type = OS_FILE_TYPE_LINK;
		break;
	case S_IFBLK:
		/* Handle block device as regular file. */
	case S_IFCHR:
		/* Handle character device as regular file. */
	case S_IFREG:
		stat_info->type = OS_FILE_TYPE_FILE;
		break;
	default:
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}

	stat_info->size = statinfo->st_size;
	stat_info->block_size = statinfo->st_blksize;
	stat_info->alloc_size = statinfo->st_blocks * 512;

	if (check_rw_perm
	    && (stat_info->type == OS_FILE_TYPE_FILE
		|| stat_info->type == OS_FILE_TYPE_BLOCK)) {

		int	access = !read_only ? O_RDWR : O_RDONLY;
		int	fh = ::open(path, access, os_innodb_umask);

		if (fh == -1) {
			stat_info->rw_perm = false;
		} else {
			stat_info->rw_perm = true;
			close(fh);
		}
	}

	return(DB_SUCCESS);
}

/** Truncates a file to a specified size in bytes.
Do nothing if the size to preserve is greater or equal to the current
size of the file.
@param[in]	pathname	file path
@param[in]	file		file to be truncated
@param[in]	size		size to preserve in bytes
@return true if success */
static
bool
os_file_truncate_posix(
	const char*	pathname,
	pfs_os_file_t	file,
	os_offset_t	size)
{
	int     res = ftruncate(file.m_file, size);
	if (res == -1) {

		bool	retry;

		retry = os_file_handle_error_no_exit(
			pathname, "truncate", false);

		if (retry) {
			ib::warn()
				<< "Truncate failed for '"
				<< pathname << "'";
		}
	}

	return(res == 0);
}

/** Truncates a file at its current position.
@return true if success */
bool
os_file_set_eof(
	FILE*		file)	/*!< in: file to be truncated */
{
	return(!ftruncate(fileno(file), ftell(file)));
}

#ifdef UNIV_HOTBACKUP
/** Closes a file handle.
@param[in]	file		Handle to a file
@return true if success */
bool
os_file_close_no_error_handling(
	os_file_t	file)
{
	return(close(file) != -1);
}
#endif /* UNIV_HOTBACKUP */

/** This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
void
os_aio_simulated_put_read_threads_to_sleep()
{
	/* No op on non Windows */
}

#else /* !_WIN32 */

#include <WinIoCtl.h>

/** Do the read/write
@param[in]	request	The IO context and type
@return the number of bytes read/written or negative value on error */
ssize_t
SyncFileIO::execute(const IORequest& request)
{
	OVERLAPPED	seek;

	memset(&seek, 0x0, sizeof(seek));

	seek.Offset = (DWORD) m_offset & 0xFFFFFFFF;
	seek.OffsetHigh = (DWORD) (m_offset >> 32);

	BOOL	ret;
	DWORD	n_bytes;

	if (request.is_read()) {
		ret = ReadFile(m_fh, m_buf,
			static_cast<DWORD>(m_n), &n_bytes, &seek);

	} else {
		ut_ad(request.is_write());
		ret = WriteFile(m_fh, m_buf,
			static_cast<DWORD>(m_n), &n_bytes, &seek);
	}

	return(ret ? static_cast<ssize_t>(n_bytes) : -1);
}

/** Do the read/write
@param[in,out]	slot	The IO slot, it has the IO context
@return the number of bytes read/written or negative value on error */
ssize_t
SyncFileIO::execute(Slot* slot)
{
	BOOL	ret;

	if (slot->type.is_read()) {
		ret = ReadFile(
			slot->file.m_file, slot->ptr, slot->len,
			&slot->n_bytes, &slot->control);
	} else {
		ut_ad(slot->type.is_write());
		ret = WriteFile(
			slot->file.m_file, slot->ptr, slot->len,
			&slot->n_bytes, &slot->control);
	}

	return(ret ? static_cast<ssize_t>(slot->n_bytes) : -1);
}

/** Check if the file system supports sparse files.
@param[in]	 name		File name
@return true if the file system supports sparse files */
static
bool
os_is_sparse_file_supported_win32(const char* filename)
{
	char	volname[MAX_PATH];
	BOOL	result = GetVolumePathName(filename, volname, MAX_PATH);

	if (!result) {

		ib::error()
			<< "os_is_sparse_file_supported: "
			<< "Failed to get the volume path name for: "
			<< filename
			<< "- OS error number " << GetLastError();

		return(false);
	}

	DWORD	flags;

	GetVolumeInformation(
		volname, NULL, MAX_PATH, NULL, NULL,
		&flags, NULL, MAX_PATH);

	return(flags & FILE_SUPPORTS_SPARSE_FILES) ? true : false;
}

/** Free storage space associated with a section of the file.
@param[in]	fh		Open file handle
@param[in]	page_size	Tablespace page size
@param[in]	block_size	File system block size
@param[in]	off		Starting offset (SEEK_SET)
@param[in]	len		Size of the hole
@return 0 on success or errno */
static
dberr_t
os_file_punch_hole_win32(
	os_file_t	fh,
	os_offset_t	off,
	os_offset_t	len)
{
	FILE_ZERO_DATA_INFORMATION	punch;

	punch.FileOffset.QuadPart = off;
	punch.BeyondFinalZero.QuadPart = off + len;

	/* If lpOverlapped is NULL, lpBytesReturned cannot be NULL,
	therefore we pass a dummy parameter. */
	DWORD	temp;

	BOOL	result = DeviceIoControl(
		fh, FSCTL_SET_ZERO_DATA, &punch, sizeof(punch),
		NULL, 0, &temp, NULL);

	return(!result ? DB_IO_NO_PUNCH_HOLE : DB_SUCCESS);
}

/** Check the existence and type of the given file.
@param[in]	path		path name of file
@param[out]	exists		true if the file exists
@param[out]	type		Type of the file, if it exists
@return true if call succeeded */
bool
os_file_status_win32(
	const char*	path,
	bool*		exists,
	os_file_type_t* type)
{
	int		ret;
	struct _stat64	statinfo;

	ret = _stat64(path, &statinfo);

	*exists = !ret;

	if (!ret) {
		/* file exists, everything OK */

	} else if (errno == ENOENT || errno == ENOTDIR
		  || errno == ENAMETOOLONG) {
		/* file does not exist */
		return(true);

	} else {
		/* file exists, but stat call failed */
		os_file_handle_error_no_exit(path, "stat", false);
		return(false);
	}

	if (_S_IFDIR & statinfo.st_mode) {
		*type = OS_FILE_TYPE_DIR;

	} else if (_S_IFREG & statinfo.st_mode) {
		*type = OS_FILE_TYPE_FILE;

	} else {
		*type = OS_FILE_TYPE_UNKNOWN;
	}

	return(true);
}

/** NOTE! Use the corresponding macro os_file_flush(), not directly this
function!
Flushes the write buffers of a given file to the disk.
@param[in]	file		handle to a file
@return true if success */
bool
os_file_flush_func(
	os_file_t	file)
{
	++os_n_fsyncs;

	BOOL	ret = FlushFileBuffers(file);

	if (ret) {
		return(true);
	}

	/* Since Windows returns ERROR_INVALID_FUNCTION if the 'file' is
	actually a raw device, we choose to ignore that error if we are using
	raw disks */

	if (srv_start_raw_disk_in_use && GetLastError()
	    == ERROR_INVALID_FUNCTION) {
		return(true);
	}

	os_file_handle_error(NULL, "flush");

	/* It is a fatal error if a file flush does not succeed, because then
	the database can get corrupt on disk */
	ut_error;

	return(false);
}

/** Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@param[in]	report_all_errors	true if we want an error message printed
					of all errors
@param[in]	on_error_silent		true then don't print any diagnostic
					to the log
@return error number, or OS error number + 100 */
static
ulint
os_file_get_last_error_low(
	bool	report_all_errors,
	bool	on_error_silent)
{
	ulint	err = (ulint) GetLastError();

	if (err == ERROR_SUCCESS) {
		return(0);
	}

	if (report_all_errors
	    || (!on_error_silent
		&& err != ERROR_DISK_FULL
		&& err != ERROR_FILE_EXISTS)) {

		ib::error()
			<< "Operating system error number " << err
			<< " in a file operation.";

		if (err == ERROR_PATH_NOT_FOUND) {
			ib::error()
				<< "The error means the system"
				" cannot find the path specified.";

			if (srv_is_being_started) {
				ib::error()
					<< "If you are installing InnoDB,"
					" remember that you must create"
					" directories yourself, InnoDB"
					" does not create them.";
			}

		} else if (err == ERROR_ACCESS_DENIED) {

			ib::error()
				<< "The error means mysqld does not have"
				" the access rights to"
				" the directory. It may also be"
				" you have created a subdirectory"
				" of the same name as a data file.";

		} else if (err == ERROR_SHARING_VIOLATION
			   || err == ERROR_LOCK_VIOLATION) {

			ib::error()
				<< "The error means that another program"
				" is using InnoDB's files."
				" This might be a backup or antivirus"
				" software or another instance"
				" of MySQL."
				" Please close it to get rid of this error.";

		} else if (err == ERROR_WORKING_SET_QUOTA
			   || err == ERROR_NO_SYSTEM_RESOURCES) {

			ib::error()
				<< "The error means that there are no"
				" sufficient system resources or quota to"
				" complete the operation.";

		} else if (err == ERROR_OPERATION_ABORTED) {

			ib::error()
				<< "The error means that the I/O"
				" operation has been aborted"
				" because of either a thread exit"
				" or an application request."
				" Retry attempt is made.";
		} else {

			ib::info() << OPERATING_SYSTEM_ERROR_MSG;
		}
	}

	if (err == ERROR_FILE_NOT_FOUND) {
		return(OS_FILE_NOT_FOUND);
	} else if (err == ERROR_DISK_FULL) {
		return(OS_FILE_DISK_FULL);
	} else if (err == ERROR_FILE_EXISTS) {
		return(OS_FILE_ALREADY_EXISTS);
	} else if (err == ERROR_SHARING_VIOLATION
		   || err == ERROR_LOCK_VIOLATION) {
		return(OS_FILE_SHARING_VIOLATION);
	} else if (err == ERROR_WORKING_SET_QUOTA
		   || err == ERROR_NO_SYSTEM_RESOURCES) {
		return(OS_FILE_INSUFFICIENT_RESOURCE);
	} else if (err == ERROR_OPERATION_ABORTED) {
		return(OS_FILE_OPERATION_ABORTED);
	} else if (err == ERROR_ACCESS_DENIED) {
		return(OS_FILE_ACCESS_VIOLATION);
	}

	return(OS_FILE_ERROR_MAX + err);
}

/** NOTE! Use the corresponding macro os_file_create_simple(), not directly
this function!
A simple function to open or create a file.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY or OS_FILE_READ_WRITE
@param[in]	read_only	if true read only mode checks are enforced
@param[out]	success		true if succeed, false if error
@return handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_simple_func(
	const char*	name,
	ulint		create_mode,
	ulint		access_type,
	bool		read_only,
	bool*		success)
{
	pfs_os_file_t	file;

	*success = false;

	DWORD		access;
	DWORD		create_flag;
	DWORD		attributes = 0;

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		create_flag = OPEN_EXISTING;

	} else if (read_only) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else if (create_mode == OS_FILE_CREATE_PATH) {

		/* Create subdirs along the path if needed. */
		*success = os_file_create_subdirs_if_needed(name);

		if (!*success) {

			ib::error()
				<< "Unable to create subdirectories '"
				<< name << "'";
			file.m_file = OS_FILE_CLOSED;
			return(file);
		}

		create_flag = CREATE_NEW;
		create_mode = OS_FILE_CREATE;

	} else {

		ib::error()
			<< "Unknown file create mode ("
			<< create_mode << ") for file '"
			<< name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	if (access_type == OS_FILE_READ_ONLY) {

		access = GENERIC_READ;

	} else if (read_only) {

		ib::info()
			<< "Read only mode set. Unable to"
			" open file '" << name << "' in RW mode, "
			<< "trying RO mode", name;

		access = GENERIC_READ;

	} else if (access_type == OS_FILE_READ_WRITE) {

		access = GENERIC_READ | GENERIC_WRITE;

	} else {

		ib::error()
			<< "Unknown file access type (" << access_type << ") "
			"for file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	bool	retry;

	do {
		/* Use default security attributes and no template file. */

		file.m_file = CreateFile(
			(LPCTSTR) name, access, FILE_SHARE_READ, NULL,
			create_flag, attributes, NULL);

		if (file.m_file == INVALID_HANDLE_VALUE) {

			*success = false;

			retry = os_file_handle_error(
				name, create_mode == OS_FILE_OPEN ?
				"open" : "create");

		} else {

			retry = false;

			*success = true;

			DWORD	temp;

			/* This is a best effort use case, if it fails then
			we will find out when we try and punch the hole. */

			DeviceIoControl(
				file.m_file, FSCTL_SET_SPARSE, NULL, 0, NULL, 0,
				&temp, NULL);
		}

	} while (retry);

	return(file);
}

/** This function attempts to create a directory named pathname. The new
directory gets default permissions. On Unix the permissions are
(0770 & ~umask). If the directory exists already, nothing is done and
the call succeeds, unless the fail_if_exists arguments is true.
If another error occurs, such as a permission error, this does not crash,
but reports the error and returns false.
@param[in]	pathname	directory name as null-terminated string
@param[in]	fail_if_exists	if true, pre-existing directory is treated
				as an error.
@return true if call succeeds, false on error */
bool
os_file_create_directory(
	const char*	pathname,
	bool		fail_if_exists)
{
	BOOL	rcode;

	rcode = CreateDirectory((LPCTSTR) pathname, NULL);
	if (!(rcode != 0
	      || (GetLastError() == ERROR_ALREADY_EXISTS
		  && !fail_if_exists))) {

		os_file_handle_error_no_exit(
			pathname, "CreateDirectory", false);

		return(false);
	}

	return(true);
}

/** The os_file_opendir() function opens a directory stream corresponding to the
directory named by the dirname argument. The directory stream is positioned
at the first entry. In both Unix and Windows we automatically skip the '.'
and '..' items at the start of the directory listing.
@param[in]	dirname		directory name; it must not contain a trailing
				'\' or '/'
@param[in]	is_fatal	true if we should treat an error as a fatal
				error; if we try to open symlinks then we do
				not wish a fatal error if it happens not to
				be a directory
@return directory stream, NULL if error */
os_file_dir_t
os_file_opendir(
	const char*	dirname,
	bool		error_is_fatal)
{
	os_file_dir_t		dir;
	LPWIN32_FIND_DATA	lpFindFileData;
	char			path[OS_FILE_MAX_PATH + 3];

	ut_a(strlen(dirname) < OS_FILE_MAX_PATH);

	strcpy(path, dirname);
	strcpy(path + strlen(path), "\\*");

	/* Note that in Windows opening the 'directory stream' also retrieves
	the first entry in the directory. Since it is '.', that is no problem,
	as we will skip over the '.' and '..' entries anyway. */

	lpFindFileData = static_cast<LPWIN32_FIND_DATA>(
		ut_malloc_nokey(sizeof(WIN32_FIND_DATA)));

	dir = FindFirstFile((LPCTSTR) path, lpFindFileData);

	ut_free(lpFindFileData);

	if (dir == INVALID_HANDLE_VALUE) {

		if (error_is_fatal) {
			os_file_handle_error(dirname, "opendir");
		}

		return(NULL);
	}

	return(dir);
}

/** Closes a directory stream.
@param[in]	dir	directory stream
@return 0 if success, -1 if failure */
int
os_file_closedir(
	os_file_dir_t	dir)
{
	BOOL		ret;

	ret = FindClose(dir);

	if (!ret) {
		os_file_handle_error_no_exit(NULL, "closedir", false);

		return(-1);
	}

	return(0);
}

/** This function returns information of the next file in the directory. We
jump over the '.' and '..' entries in the directory.
@param[in]	dirname		directory name or path
@param[in]	dir		directory stream
@param[out]	info		buffer where the info is returned
@return 0 if ok, -1 if error, 1 if at the end of the directory */
int
os_file_readdir_next_file(
	const char*	dirname,
	os_file_dir_t	dir,
	os_file_stat_t*	info)
{
	BOOL		ret;
	int		status;
	WIN32_FIND_DATA	find_data;

next_file:

	ret = FindNextFile(dir, &find_data);

	if (ret > 0) {

		const char* name;

		name = static_cast<const char*>(find_data.cFileName);

		ut_a(strlen(name) < OS_FILE_MAX_PATH);

		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {

			goto next_file;
		}

		strcpy(info->name, name);

		info->size = find_data.nFileSizeHigh;
		info->size <<= 32;
		info->size |= find_data.nFileSizeLow;

		if (find_data.dwFileAttributes
		    & FILE_ATTRIBUTE_REPARSE_POINT) {

			/* TODO: test Windows symlinks */
			/* TODO: MySQL has apparently its own symlink
			implementation in Windows, dbname.sym can
			redirect a database directory:
			REFMAN "windows-symbolic-links.html" */

			info->type = OS_FILE_TYPE_LINK;

		} else if (find_data.dwFileAttributes
			   & FILE_ATTRIBUTE_DIRECTORY) {

			info->type = OS_FILE_TYPE_DIR;

		} else {

			/* It is probably safest to assume that all other
			file types are normal. Better to check them rather
			than blindly skip them. */

			info->type = OS_FILE_TYPE_FILE;
		}

		status = 0;

	} else if (GetLastError() == ERROR_NO_MORE_FILES) {

		status = 1;

	} else {

		os_file_handle_error_no_exit(NULL, "readdir_next_file", false);

		status = -1;
	}

	return(status);
}

/** NOTE! Use the corresponding macro os_file_create(), not directly
this function!
Opens an existing file or creates a new.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	purpose		OS_FILE_AIO, if asynchronous, non-buffered I/O
				is desired, OS_FILE_NORMAL, if any normal file;
				NOTE that it also depends on type, os_aio_..
				and srv_.. variables whether we really use async
				I/O or unbuffered I/O: look in the function
				source code for the exact rules
@param[in]	type		OS_DATA_FILE or OS_LOG_FILE
@param[in]	success		true if succeeded
@return handle to the file, not defined if error, error number
	can be retrieved with os_file_get_last_error */
pfs_os_file_t
os_file_create_func(
	const char*	name,
	ulint		create_mode,
	ulint		purpose,
	ulint		type,
	bool		read_only,
	bool*		success)
{
	pfs_os_file_t	file;
	bool		retry;
	bool		on_error_no_exit;
	bool		on_error_silent;

	*success = false;

	DBUG_EXECUTE_IF(
		"ib_create_table_fail_disk_full",
		*success = false;
		SetLastError(ERROR_DISK_FULL);
		file.m_file = OS_FILE_CLOSED;
		return(file);
	);

	DWORD		create_flag;
	DWORD		share_mode = FILE_SHARE_READ;

	on_error_no_exit = create_mode & OS_FILE_ON_ERROR_NO_EXIT
		? true : false;

	on_error_silent = create_mode & OS_FILE_ON_ERROR_SILENT
		? true : false;

	create_mode &= ~OS_FILE_ON_ERROR_NO_EXIT;
	create_mode &= ~OS_FILE_ON_ERROR_SILENT;

	if (create_mode == OS_FILE_OPEN_RAW) {

		ut_a(!read_only);

		create_flag = OPEN_EXISTING;

		/* On Windows Physical devices require admin privileges and
		have to have the write-share mode set. See the remarks
		section for the CreateFile() function documentation in MSDN. */

		share_mode |= FILE_SHARE_WRITE;

	} else if (create_mode == OS_FILE_OPEN
		   || create_mode == OS_FILE_OPEN_RETRY) {

		create_flag = OPEN_EXISTING;

	} else if (read_only) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else if (create_mode == OS_FILE_OVERWRITE) {

		create_flag = CREATE_ALWAYS;

	} else {
		ib::error()
			<< "Unknown file create mode (" << create_mode << ") "
			<< " for file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	DWORD		attributes = 0;

#ifdef UNIV_HOTBACKUP
	attributes |= FILE_FLAG_NO_BUFFERING;
#else
	if (purpose == OS_FILE_AIO) {

#ifdef WIN_ASYNC_IO
		/* If specified, use asynchronous (overlapped) io and no
		buffering of writes in the OS */

		if (srv_use_native_aio) {
			attributes |= FILE_FLAG_OVERLAPPED;
		}
#endif /* WIN_ASYNC_IO */

	} else if (purpose == OS_FILE_NORMAL) {

		/* Use default setting. */

	} else {

		ib::error()
			<< "Unknown purpose flag (" << purpose << ") "
			<< "while opening file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

#ifdef UNIV_NON_BUFFERED_IO
	// TODO: Create a bug, this looks wrong. The flush log
	// parameter is dynamic.
	if (type == OS_LOG_FILE && srv_flush_log_at_trx_commit == 2) {

		/* Do not use unbuffered i/o for the log files because
		value 2 denotes that we do not flush the log at every
		commit, but only once per second */

	} else if (srv_win_file_flush_method == SRV_WIN_IO_UNBUFFERED) {

		attributes |= FILE_FLAG_NO_BUFFERING;
	}
#endif /* UNIV_NON_BUFFERED_IO */

#endif /* UNIV_HOTBACKUP */
	DWORD	access = GENERIC_READ;

	if (!read_only) {
		access |= GENERIC_WRITE;
	}

	do {
		/* Use default security attributes and no template file. */
		file.m_file = CreateFile(
			(LPCTSTR) name, access, share_mode, NULL,
			create_flag, attributes, NULL);

		if (file.m_file == INVALID_HANDLE_VALUE) {
			const char*	operation;

			operation = (create_mode == OS_FILE_CREATE
				     && !read_only)
				? "create" : "open";

			*success = false;

			if (on_error_no_exit) {
				retry = os_file_handle_error_no_exit(
					name, operation, on_error_silent);
			} else {
				retry = os_file_handle_error(name, operation);
			}
		} else {

			retry = false;

			*success = true;

			DWORD	temp;

			/* This is a best effort use case, if it fails then
			we will find out when we try and punch the hole. */
			DeviceIoControl(
				file.m_file, FSCTL_SET_SPARSE, NULL, 0, NULL, 0,
				&temp, NULL);
		}

	} while (retry);

	return(file);
}

/** NOTE! Use the corresponding macro os_file_create_simple_no_error_handling(),
not directly this function!
A simple function to open or create a file.
@param[in]	name		name of the file or path as a null-terminated
				string
@param[in]	create_mode	create mode
@param[in]	access_type	OS_FILE_READ_ONLY, OS_FILE_READ_WRITE, or
				OS_FILE_READ_ALLOW_DELETE; the last option is
				used by a backup program reading the file
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
{
	pfs_os_file_t	file;

	*success = false;

	DWORD		access;
	DWORD		create_flag;
	DWORD		attributes	= 0;
	DWORD		share_mode	= FILE_SHARE_READ;

	ut_a(name);

	ut_a(!(create_mode & OS_FILE_ON_ERROR_SILENT));
	ut_a(!(create_mode & OS_FILE_ON_ERROR_NO_EXIT));

	if (create_mode == OS_FILE_OPEN) {

		create_flag = OPEN_EXISTING;

	} else if (read_only) {

		create_flag = OPEN_EXISTING;

	} else if (create_mode == OS_FILE_CREATE) {

		create_flag = CREATE_NEW;

	} else {

		ib::error()
			<< "Unknown file create mode (" << create_mode << ") "
			<< " for file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	if (access_type == OS_FILE_READ_ONLY) {

		access = GENERIC_READ;

	} else if (read_only) {

		access = GENERIC_READ;

	} else if (access_type == OS_FILE_READ_WRITE) {

		access = GENERIC_READ | GENERIC_WRITE;

	} else if (access_type == OS_FILE_READ_ALLOW_DELETE) {

		ut_a(!read_only);

		access = GENERIC_READ;

		/*!< A backup program has to give mysqld the maximum
		freedom to do what it likes with the file */

		share_mode |= FILE_SHARE_DELETE | FILE_SHARE_WRITE;
	} else {

		ib::error()
			<< "Unknown file access type (" << access_type << ") "
			<< "for file '" << name << "'";

		file.m_file = OS_FILE_CLOSED;
		return(file);
	}

	file.m_file = CreateFile((LPCTSTR) name,
			  access,
			  share_mode,
			  NULL,			// Security attributes
			  create_flag,
			  attributes,
			  NULL);		// No template file

	*success = (file.m_file != INVALID_HANDLE_VALUE);

	return(file);
}

/** Deletes a file if it exists. The file has to be closed before calling this.
@param[in]	name		file path as a null-terminated string
@param[out]	exist		indicate if file pre-exist
@return true if success */
bool
os_file_delete_if_exists_func(
	const char*	name,
	bool*		exist)
{
	ulint	count	= 0;

	if (exist != NULL) {
		*exist = true;
	}

	for (;;) {
		/* In Windows, deleting an .ibd file may fail if ibbackup
		is copying it */

		bool	ret = DeleteFile((LPCTSTR) name);

		if (ret) {
			return(true);
		}

		DWORD	lasterr = GetLastError();

		if (lasterr == ERROR_FILE_NOT_FOUND
		    || lasterr == ERROR_PATH_NOT_FOUND) {

			/* the file does not exist, this not an error */
			if (exist != NULL) {
				*exist = false;
			}

			return(true);
		}

		++count;

		if (count > 100 && 0 == (count % 10)) {

			/* Print error information */
			os_file_get_last_error(true);

			ib::warn() << "Delete of file '" << name << "' failed.";
		}

		/* Sleep for a second */
		os_thread_sleep(1000000);

		if (count > 2000) {

			return(false);
		}
	}
}

/** Deletes a file. The file has to be closed before calling this.
@param[in]	name		File path as NUL terminated string
@return true if success */
bool
os_file_delete_func(
	const char*	name)
{
	ulint	count	= 0;

	for (;;) {
		/* In Windows, deleting an .ibd file may fail if ibbackup
		is copying it */

		BOOL	ret = DeleteFile((LPCTSTR) name);

		if (ret) {
			return(true);
		}

		if (GetLastError() == ERROR_FILE_NOT_FOUND) {
			/* If the file does not exist, we classify this as
			a 'mild' error and return */

			return(false);
		}

		++count;

		if (count > 100 && 0 == (count % 10)) {

			/* print error information */
			os_file_get_last_error(true);

			ib::warn()
				<< "Cannot delete file '" << name << "'. Are "
				<< "you running ibbackup to back up the file?";
		}

		/* sleep for a second */
		os_thread_sleep(1000000);

		if (count > 2000) {

			return(false);
		}
	}

	ut_error;
	return(false);
}

/** NOTE! Use the corresponding macro os_file_rename(), not directly this
function!
Renames a file (can also move it to another directory). It is safest that the
file is closed before calling this function.
@param[in]	oldpath		old file path as a null-terminated string
@param[in]	newpath		new file path
@return true if success */
bool
os_file_rename_func(
	const char*	oldpath,
	const char*	newpath)
{
#ifdef UNIV_DEBUG
	os_file_type_t	type;
	bool		exists;

	/* New path must not exist. */
	ut_ad(os_file_status(newpath, &exists, &type));
	ut_ad(!exists);

	/* Old path must exist. */
	ut_ad(os_file_status(oldpath, &exists, &type));
	ut_ad(exists);
#endif /* UNIV_DEBUG */

	if (MoveFile((LPCTSTR) oldpath, (LPCTSTR) newpath)) {
		return(true);
	}

	os_file_handle_error_no_exit(oldpath, "rename", false);

	return(false);
}

/** NOTE! Use the corresponding macro os_file_close(), not directly
this function!
Closes a file handle. In case of error, error number can be retrieved with
os_file_get_last_error.
@param[in,own]	file		Handle to a file
@return true if success */
bool
os_file_close_func(
	os_file_t	file)
{
	ut_a(file > 0);

	if (CloseHandle(file)) {
		return(true);
	}

	os_file_handle_error(NULL, "close");

	return(false);
}

/** Gets a file size.
@param[in]	file		Handle to a file
@return file size, or (os_offset_t) -1 on failure */
os_offset_t
os_file_get_size(
	pfs_os_file_t	file)
{
	DWORD		high;
	DWORD		low;

	low = GetFileSize(file.m_file, &high);

	if (low == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
		return((os_offset_t) -1);
	}

	return(os_offset_t(low | (os_offset_t(high) << 32)));
}

/** Gets a file size.
@param[in]	filename	Full path to the filename to check
@return file size if OK, else set m_total_size to ~0 and m_alloc_size to
	errno */
os_file_size_t
os_file_get_size(
	const char*	filename)
{
	struct __stat64	s;
	os_file_size_t	file_size;

	int		ret = _stat64(filename, &s);

	if (ret == 0) {

		file_size.m_total_size = s.st_size;

		DWORD	low_size;
		DWORD	high_size;

		low_size = GetCompressedFileSize(filename, &high_size);

		if (low_size != INVALID_FILE_SIZE) {

			file_size.m_alloc_size = high_size;
			file_size.m_alloc_size <<= 32;
			file_size.m_alloc_size |= low_size;

		} else {
			ib::error()
				<< "GetCompressedFileSize("
				<< filename << ", ..) failed.";

			file_size.m_alloc_size = (os_offset_t) -1;
		}
	} else {
		file_size.m_total_size = ~0;
		file_size.m_alloc_size = (os_offset_t) ret;
	}

	return(file_size);
}

/** This function returns information about the specified file
@param[in]	path		pathname of the file
@param[out]	stat_info	information of a file in a directory
@param[in,out]	statinfo	information of a file in a directory
@param[in]	check_rw_perm	for testing whether the file can be opened
				in RW mode
@param[in]	read_only	true if the file is opened in read-only mode
@return DB_SUCCESS if all OK */
static
dberr_t
os_file_get_status_win32(
	const char*	path,
	os_file_stat_t* stat_info,
	struct _stat64*	statinfo,
	bool		check_rw_perm,
	bool		read_only)
{
	int	ret = _stat64(path, statinfo);

	if (ret && (errno == ENOENT || errno == ENOTDIR)) {
		/* file does not exist */

		return(DB_NOT_FOUND);

	} else if (ret) {
		/* file exists, but stat call failed */

		os_file_handle_error_no_exit(path, "stat", false);

		return(DB_FAIL);

	} else if (_S_IFDIR & statinfo->st_mode) {

		stat_info->type = OS_FILE_TYPE_DIR;

	} else if (_S_IFREG & statinfo->st_mode) {

		DWORD	access = GENERIC_READ;

		if (!read_only) {
			access |= GENERIC_WRITE;
		}

		stat_info->type = OS_FILE_TYPE_FILE;

		/* Check if we can open it in read-only mode. */

		if (check_rw_perm) {
			HANDLE	fh;

			fh = CreateFile(
				(LPCTSTR) path,		// File to open
				access,
				0,			// No sharing
				NULL,			// Default security
				OPEN_EXISTING,		// Existing file only
				FILE_ATTRIBUTE_NORMAL,	// Normal file
				NULL);			// No attr. template

			if (fh == INVALID_HANDLE_VALUE) {
				stat_info->rw_perm = false;
			} else {
				stat_info->rw_perm = true;
				CloseHandle(fh);
			}
		}

		char	volname[MAX_PATH];
		BOOL	result = GetVolumePathName(path, volname, MAX_PATH);

		if (!result) {

			ib::error()
				<< "os_file_get_status_win32: "
				<< "Failed to get the volume path name for: "
				<< path
				<< "- OS error number " << GetLastError();

			return(DB_FAIL);
		}

		DWORD	sectorsPerCluster;
		DWORD	bytesPerSector;
		DWORD	numberOfFreeClusters;
		DWORD	totalNumberOfClusters;

		result = GetDiskFreeSpace(
			(LPCSTR) volname,
			&sectorsPerCluster,
			&bytesPerSector,
			&numberOfFreeClusters,
			&totalNumberOfClusters);

		if (!result) {

			ib::error()
				<< "GetDiskFreeSpace(" << volname << ",...) "
				<< "failed "
				<< "- OS error number " << GetLastError();

			return(DB_FAIL);
		}

		stat_info->block_size = bytesPerSector * sectorsPerCluster;

		/* On Windows the block size is not used as the allocation
		unit for sparse files. The underlying infra-structure for
		sparse files is based on NTFS compression. The punch hole
		is done on a "compression unit". This compression unit
		is based on the cluster size. You cannot punch a hole if
		the cluster size >= 8K. For smaller sizes the table is
		as follows:

		Cluster Size	Compression Unit
		512 Bytes		 8 KB
		  1 KB			16 KB
		  2 KB			32 KB
		  4 KB			64 KB

		Default NTFS cluster size is 4K, compression unit size of 64K.
		Therefore unless the user has created the file system with
		a smaller cluster size and used larger page sizes there is
		little benefit from compression out of the box. */

		stat_info->block_size = (stat_info->block_size <= 4096)
			?  stat_info->block_size * 16 : ULINT_UNDEFINED;
	} else {
		stat_info->type = OS_FILE_TYPE_UNKNOWN;
	}

	return(DB_SUCCESS);
}

/** Truncates a file to a specified size in bytes.
Do nothing if the size to preserve is greater or equal to the current
size of the file.
@param[in]	pathname	file path
@param[in]	file		file to be truncated
@param[in]	size		size to preserve in bytes
@return true if success */
static
bool
os_file_truncate_win32(
	const char*	pathname,
	pfs_os_file_t	file,
	os_offset_t	size)
{
	LARGE_INTEGER	length;

	length.QuadPart = size;
	BOOL	success = SetFilePointerEx(file.m_file, length, NULL, FILE_BEGIN);
	if (!success) {
		os_file_handle_error_no_exit(
			pathname, "SetFilePointerEx", false);
	} else {
		success = SetEndOfFile(file.m_file);
		if (!success) {
			os_file_handle_error_no_exit(
				pathname, "SetEndOfFile", false);
		}
	}
	return(success);
}

/** Truncates a file at its current position.
@param[in]	file		Handle to be truncated
@return true if success */
bool
os_file_set_eof(
	FILE*		file)
{
	HANDLE	h = (HANDLE) _get_osfhandle(fileno(file));

	return(SetEndOfFile(h));
}

#ifdef UNIV_HOTBACKUP
/** Closes a file handle.
@param[in]	file		Handle to close
@return true if success */
bool
os_file_close_no_error_handling(
	os_file_t	file)
{
	return(CloseHandle(file) ? true : false);
}
#endif /* UNIV_HOTBACKUP */

/** This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
void
os_aio_simulated_put_read_threads_to_sleep()
{
	AIO::simulated_put_read_threads_to_sleep();
}

/** This function can be called if one wants to post a batch of reads and
prefers an i/o-handler thread to handle them all at once later. You must
call os_aio_simulated_wake_handler_threads later to ensure the threads
are not left sleeping! */
void
AIO::simulated_put_read_threads_to_sleep()
{
	/* The idea of putting background IO threads to sleep is only for
	Windows when using simulated AIO. Windows XP seems to schedule
	background threads too eagerly to allow for coalescing during
	readahead requests. */

	if (srv_use_native_aio) {
		/* We do not use simulated AIO: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= true;

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		AIO*	array;

		get_array_and_local_segment(&array, i);

		if (array == s_reads) {

			os_event_reset(os_aio_segment_wait_events[i]);
		}
	}
}

#endif /* !_WIN32*/

/** Does a syncronous read or write depending upon the type specified
In case of partial reads/writes the function tries
NUM_RETRIES_ON_PARTIAL_IO times to read/write the complete data.
@param[in]	type,		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer where to read
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
@param[out]	err		DB_SUCCESS or error code
@return number of bytes read/written, -1 if error */
static MY_ATTRIBUTE((warn_unused_result))
ssize_t
os_file_io(
	const IORequest&in_type,
	os_file_t	file,
	void*		buf,
	ulint		n,
	os_offset_t	offset,
	dberr_t*	err)
{
	Block*		block;
	ulint		original_n = n;
	IORequest	type = in_type;
	ssize_t		bytes_returned = 0;

	if (type.is_compressed()) {

		/* We don't compress the first page of any file. */
		ut_ad(offset > 0);

		block = os_file_compress_page(type, buf, &n);
	} else {
		block = NULL;
	}

	/* We do encryption after compression, since if we do encryption
	before compression, the encrypted data will cause compression fail
	or low compression rate. */
        if (type.is_encrypted() && type.is_write()) {
		/* We don't encrypt the first page of any file. */
		Block*	compressed_block = block;
		ut_ad(offset > 0);

		block = os_file_encrypt_page(type, buf, &n);

		if (compressed_block != NULL) {
			os_free_block(compressed_block);
		}
        }

	SyncFileIO	sync_file_io(file, buf, n, offset);

	for (ulint i = 0; i < NUM_RETRIES_ON_PARTIAL_IO; ++i) {

		ssize_t	n_bytes = sync_file_io.execute(type);

		/* Check for a hard error. Not much we can do now. */
		if (n_bytes < 0) {

			break;

		} else if ((ulint) n_bytes + bytes_returned == n) {

			bytes_returned += n_bytes;

			if (offset > 0
			    && (type.is_compressed() || type.is_read())) {

				*err = os_file_io_complete(
					type, file,
					reinterpret_cast<byte*>(buf),
					NULL, original_n,
					static_cast<ulint>(offset), n);
			} else {

				*err = DB_SUCCESS;
			}

			if (block != NULL) {
				os_free_block(block);
			}

			return(original_n);
		}

		/* Handle partial read/write. */

		ut_ad((ulint) n_bytes + bytes_returned < n);

		bytes_returned += (ulint) n_bytes;

		if (!type.is_partial_io_warning_disabled()) {

			const char*	op = type.is_read()
				? "read" : "written";

			ib::warn()
				<< n
				<< " bytes should have been " << op << ". Only "
				<< bytes_returned
				<< " bytes " << op << ". Retrying"
				<< " for the remaining bytes.";
		}

		/* Advance the offset and buffer by n_bytes */
		sync_file_io.advance(n_bytes);
	}

	if (block != NULL) {
		os_free_block(block);
	}

	*err = DB_IO_ERROR;

	if (!type.is_partial_io_warning_disabled()) {
		ib::warn()
			<< "Retry attempts for "
			<< (type.is_read() ? "reading" : "writing")
			<< " partial data failed.";
	}

	return(bytes_returned);
}

/** Does a synchronous write operation in Posix.
@param[in]	type		IO context
@param[in]	file		handle to an open file
@param[out]	buf		buffer from which to write
@param[in]	n		number of bytes to read, starting from offset
@param[in]	offset		file offset from the start where to read
@param[out]	err		DB_SUCCESS or error code
@return number of bytes written, -1 if error */
static MY_ATTRIBUTE((warn_unused_result))
ssize_t
os_file_pwrite(
	IORequest&	type,
	os_file_t	file,
	const byte*	buf,
	ulint		n,
	os_offset_t	offset,
	dberr_t*	err)
{
	ut_ad(type.validate());

	++os_n_file_writes;

	(void) os_atomic_increment_ulint(&os_n_pending_writes, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_WRITES);

	ssize_t	n_bytes = os_file_io(type, file, (void*) buf, n, offset, err);

	(void) os_atomic_decrement_ulint(&os_n_pending_writes, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_WRITES);

	return(n_bytes);
}

/** Requests a synchronous write operation.
@param[in]	type		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer from which to write
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
@return DB_SUCCESS if request was successful, false if fail */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
os_file_write_page(
	IORequest&	type,
	const char*	name,
	os_file_t	file,
	const byte*	buf,
	os_offset_t	offset,
	ulint		n)
{
	dberr_t		err;
	ut_ad(type.validate());
	ut_ad(n > 0);

	ssize_t n_bytes = os_file_pwrite(type, file, buf, n, offset, &err);

	if ((ulint) n_bytes != n && !os_has_said_disk_full) {

		ib::error()
			<< "Write to file " << name << "failed at offset "
			<< offset << ", " << n
			<< " bytes should have been written,"
			" only " << n_bytes << " were written."
			" Operating system error number " << errno << "."
			" Check that your OS and file system"
			" support files of this size."
			" Check also that the disk is not full"
			" or a disk quota exceeded.";

		if (strerror(errno) != NULL) {

			ib::error()
				<< "Error number " << errno
				<< " means '" << strerror(errno) << "'";
		}

		ib::info() << OPERATING_SYSTEM_ERROR_MSG;

		os_has_said_disk_full = true;
	}

	return(err);
}

/** Does a synchronous read operation in Posix.
@param[in]	type		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer where to read
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
@param[out]	err		DB_SUCCESS or error code
@return number of bytes read, -1 if error */
static MY_ATTRIBUTE((warn_unused_result))
ssize_t
os_file_pread(
	IORequest&	type,
	os_file_t	file,
	void*		buf,
	ulint		n,
	os_offset_t	offset,
	dberr_t*	err)
{
	++os_n_file_reads;

	(void) os_atomic_increment_ulint(&os_n_pending_reads, 1);
	MONITOR_ATOMIC_INC(MONITOR_OS_PENDING_READS);

	ssize_t	n_bytes = os_file_io(type, file, buf, n, offset, err);

	(void) os_atomic_decrement_ulint(&os_n_pending_reads, 1);
	MONITOR_ATOMIC_DEC(MONITOR_OS_PENDING_READS);

	return(n_bytes);
}

/** Requests a synchronous positioned read operation.
@return DB_SUCCESS if request was successful, false if fail
@param[in]	type		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer where to read
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
@param[out]	o		number of bytes actually read
@param[in]	exit_on_err	if true then exit on error
@return DB_SUCCESS or error code */
static MY_ATTRIBUTE((warn_unused_result))
dberr_t
os_file_read_page(
	IORequest&	type,
	os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n,
	ulint*		o,
	bool		exit_on_err)
{
	dberr_t		err;

	os_bytes_read_since_printout += n;

	ut_ad(type.validate());
	ut_ad(n > 0);

	for (;;) {
		ssize_t	n_bytes;

		n_bytes = os_file_pread(type, file, buf, n, offset, &err);

		if (o != NULL) {
			*o = n_bytes;
		}

		if (err != DB_SUCCESS && !exit_on_err) {

			return(err);

		} else if ((ulint) n_bytes == n) {

			/** The read will succeed but decompress can fail
			for various reasons. */

			if (type.is_compression_enabled()
			    && !Compression::is_compressed_page(
				    static_cast<byte*>(buf))) {

				return(DB_SUCCESS);

			} else {
				return(err);
			}
		}

		ib::error() << "Tried to read " << n
			<< " bytes at offset " << offset
			<< ", but was only able to read " << n_bytes;

		if (exit_on_err) {

			if (!os_file_handle_error(NULL, "read")) {
				/* Hard error */
				break;
			}

		} else if (!os_file_handle_error_no_exit(NULL, "read", false)) {

			/* Hard error */
			break;
		}

		if (n_bytes > 0 && (ulint) n_bytes < n) {
			n -= (ulint) n_bytes;
			offset += (ulint) n_bytes;
			buf = reinterpret_cast<uchar*>(buf) + (ulint) n_bytes;
		}
	}

	ib::fatal()
		<< "Cannot read from file. OS error number "
		<< errno << ".";

	return(err);
}

/** Retrieves the last error number if an error occurs in a file io function.
The number should be retrieved before any other OS calls (because they may
overwrite the error number). If the number is not known to this program,
the OS error number + 100 is returned.
@param[in]	report_all_errors	true if we want an error printed
					for all errors
@return error number, or OS error number + 100 */
ulint
os_file_get_last_error(
	bool	report_all_errors)
{
	return(os_file_get_last_error_low(report_all_errors, false));
}

/** Does error handling when a file operation fails.
Conditionally exits (calling srv_fatal_error()) based on should_exit value
and the error type, if should_exit is true then on_error_silent is ignored.
@param[in]	name		name of a file or NULL
@param[in]	operation	operation
@param[in]	should_exit	call srv_fatal_error() on an unknown error,
				if this parameter is true
@param[in]	on_error_silent	if true then don't print any message to the log
				iff it is an unknown non-fatal error
@return true if we should retry the operation */
static MY_ATTRIBUTE((warn_unused_result))
bool
os_file_handle_error_cond_exit(
	const char*	name,
	const char*	operation,
	bool		should_exit,
	bool		on_error_silent)
{
	ulint	err;

	err = os_file_get_last_error_low(false, on_error_silent);

	switch (err) {
	case OS_FILE_DISK_FULL:
		/* We only print a warning about disk full once */

		if (os_has_said_disk_full) {

			return(false);
		}

		/* Disk full error is reported irrespective of the
		on_error_silent setting. */

		if (name) {

			ib::error()
				<< "Encountered a problem with file '"
				<< name << "'";
		}

		ib::error()
			<< "Disk is full. Try to clean the disk to free space.";

		os_has_said_disk_full = true;

		return(false);

	case OS_FILE_AIO_RESOURCES_RESERVED:
	case OS_FILE_AIO_INTERRUPTED:

		return(true);

	case OS_FILE_PATH_ERROR:
	case OS_FILE_ALREADY_EXISTS:
	case OS_FILE_ACCESS_VIOLATION:

		return(false);

	case OS_FILE_SHARING_VIOLATION:

		os_thread_sleep(10000000);	/* 10 sec */
		return(true);

	case OS_FILE_OPERATION_ABORTED:
	case OS_FILE_INSUFFICIENT_RESOURCE:

		os_thread_sleep(100000);	/* 100 ms */
		return(true);

	default:

		/* If it is an operation that can crash on error then it
		is better to ignore on_error_silent and print an error message
		to the log. */

		if (should_exit || !on_error_silent) {
			ib::error() << "File "
				<< (name != NULL ? name : "(unknown)")
				<< ": '" << operation << "'"
				" returned OS error " << err << "."
				<< (should_exit
				    ? " Cannot continue operation" : "");
		}

		if (should_exit) {
			srv_fatal_error();
		}
	}

	return(false);
}

/** Does error handling when a file operation fails.
@param[in]	name		name of a file or NULL
@param[in]	operation	operation name that failed
@return true if we should retry the operation */
static
bool
os_file_handle_error(
	const char*	name,
	const char*	operation)
{
	/* Exit in case of unknown error */
	return(os_file_handle_error_cond_exit(name, operation, true, false));
}

/** Does error handling when a file operation fails.
@param[in]	name		name of a file or NULL
@param[in]	operation	operation name that failed
@param[in]	on_error_silent	if true then don't print any message to the log.
@return true if we should retry the operation */
static
bool
os_file_handle_error_no_exit(
	const char*	name,
	const char*	operation,
	bool		on_error_silent)
{
	/* Don't exit in case of unknown error */
	return(os_file_handle_error_cond_exit(
			name, operation, false, on_error_silent));
}

/** Tries to disable OS caching on an opened file descriptor.
@param[in]	fd		file descriptor to alter
@param[in]	file_name	file name, used in the diagnostic message
@param[in]	name		"open" or "create"; used in the diagnostic
				message */
void
os_file_set_nocache(
	int		fd		MY_ATTRIBUTE((unused)),
	const char*	file_name	MY_ATTRIBUTE((unused)),
	const char*	operation_name	MY_ATTRIBUTE((unused)))
{
	/* some versions of Solaris may not have DIRECTIO_ON */
#if defined(UNIV_SOLARIS) && defined(DIRECTIO_ON)
	if (directio(fd, DIRECTIO_ON) == -1) {
		int	errno_save = errno;

		ib::error()
			<< "Failed to set DIRECTIO_ON on file "
			<< file_name << ": " << operation_name
			<< strerror(errno_save) << ","
			" continuing anyway.";
	}
#elif defined(O_DIRECT)
	if (fcntl(fd, F_SETFL, O_DIRECT) == -1) {
		int		errno_save = errno;
		static bool	warning_message_printed = false;
		if (errno_save == EINVAL) {
			if (!warning_message_printed) {
				warning_message_printed = true;
# ifdef UNIV_LINUX
				ib::warn()
					<< "Failed to set O_DIRECT on file"
					<< file_name << ";" << operation_name
					<< ": " << strerror(errno_save) << ", "
					<< "continuing anyway. O_DIRECT is "
					"known to result in 'Invalid argument' "
					"on Linux on tmpfs, "
					"see MySQL Bug#26662.";
# else /* UNIV_LINUX */
				goto short_warning;
# endif /* UNIV_LINUX */
			}
		} else {
# ifndef UNIV_LINUX
short_warning:
# endif
			ib::warn()
				<< "Failed to set O_DIRECT on file "
				<< file_name << "; " << operation_name
				<< " : " << strerror(errno_save)
				<< " continuing anyway.";
		}
	}
#endif /* defined(UNIV_SOLARIS) && defined(DIRECTIO_ON) */
}

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
{
	/* Write up to 1 megabyte at a time. */
	ulint	buf_size = ut_min(
		static_cast<ulint>(64),
		static_cast<ulint>(size / UNIV_PAGE_SIZE));

	buf_size *= UNIV_PAGE_SIZE;

	/* Align the buffer for possible raw i/o */
	byte*	buf2;

	buf2 = static_cast<byte*>(ut_malloc_nokey(buf_size + UNIV_PAGE_SIZE));

	byte*	buf = static_cast<byte*>(ut_align(buf2, UNIV_PAGE_SIZE));

	/* Write buffer full of zeros */
	memset(buf, 0, buf_size);

	if (size >= (os_offset_t) 100 << 20) {

		ib::info() << "Progress in MB:";
	}

	os_offset_t	current_size = 0;

	while (current_size < size) {
		ulint	n_bytes;

		if (size - current_size < (os_offset_t) buf_size) {
			n_bytes = (ulint) (size - current_size);
		} else {
			n_bytes = buf_size;
		}

		dberr_t		err;
		IORequest	request(IORequest::WRITE);

#ifdef UNIV_HOTBACKUP

		err = os_file_write(
			request, name, file, buf, current_size, n_bytes);
#else
		/* Using OS_AIO_SYNC mode on POSIX systems will result in
		fall back to os_file_write/read. On Windows it will use
		special mechanism to wait before it returns back. */

		err = os_aio(
			request,
			OS_AIO_SYNC, name,
			file, buf, current_size, n_bytes,
			read_only, NULL, NULL);
#endif /* UNIV_HOTBACKUP */

		if (err != DB_SUCCESS) {

			ut_free(buf2);
			return(false);
		}

		/* Print about progress for each 100 MB written */
		if ((current_size + n_bytes) / (100 << 20)
		    != current_size / (100 << 20)) {

			fprintf(stderr, " %lu00",
				(ulong) ((current_size + n_bytes)
					 / (100 << 20)));
		}

		current_size += n_bytes;
	}

	if (size >= (os_offset_t) 100 << 20) {

		fprintf(stderr, "\n");
	}

	ut_free(buf2);

	return(os_file_flush(file));
}

/** Truncates a file to a specified size in bytes.
Do nothing if the size to preserve is greater or equal to the current
size of the file.
@param[in]	pathname	file path
@param[in]	file		file to be truncated
@param[in]	size		size to preserve in bytes
@return true if success */
bool
os_file_truncate(
	const char*	pathname,
	pfs_os_file_t	file,
	os_offset_t	size)
{
	/* Do nothing if the size preserved is larger than or equal to the
	current size of file */
	os_offset_t	size_bytes = os_file_get_size(file);

	if (size >= size_bytes) {
		return(true);
	}

#ifdef _WIN32
	return(os_file_truncate_win32(pathname, file, size));
#else /* _WIN32 */
	return(os_file_truncate_posix(pathname, file, size));
#endif /* _WIN32 */
}

/** NOTE! Use the corresponding macro os_file_read(), not directly this
function!
Requests a synchronous positioned read operation.
@return DB_SUCCESS if request was successful, DB_IO_ERROR on failure
@param[in]	type		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer where to read
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
@return DB_SUCCESS or error code */
dberr_t
os_file_read_func(
	IORequest&	type,
	os_file_t	file,
	void*		buf,
	os_offset_t	offset,
	ulint		n)
{
	ut_ad(type.is_read());

	return(os_file_read_page(type, file, buf, offset, n, NULL, true));
}

/** NOTE! Use the corresponding macro os_file_read_no_error_handling(),
not directly this function!
Requests a synchronous positioned read operation.
@return DB_SUCCESS if request was successful, DB_IO_ERROR on failure
@param[in]	type		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer where to read
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
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
{
	ut_ad(type.is_read());

	return(os_file_read_page(type, file, buf, offset, n, o, false));
}

/** NOTE! Use the corresponding macro os_file_write(), not directly
Requests a synchronous write operation.
@param[in]	type		IO flags
@param[in]	file		handle to an open file
@param[out]	buf		buffer from which to write
@param[in]	offset		file offset from the start where to read
@param[in]	n		number of bytes to read, starting from offset
@return DB_SUCCESS if request was successful, false if fail */
dberr_t
os_file_write_func(
	IORequest&	type,
	const char*	name,
	os_file_t	file,
	const void*	buf,
	os_offset_t	offset,
	ulint		n)
{
	ut_ad(type.validate());
	ut_ad(type.is_write());

	/* We never compress the first page.
	Note: This assumes we always do block IO. */
	if (offset == 0) {
		type.clear_compressed();
	}

	const byte*	ptr = reinterpret_cast<const byte*>(buf);

	return(os_file_write_page(type, name, file, ptr, offset, n));
}

/** Check the existence and type of the given file.
@param[in]	path		path name of file
@param[out]	exists		true if the file exists
@param[out]	type		Type of the file, if it exists
@return true if call succeeded */
bool
os_file_status(
	const char*	path,
	bool*		exists,
	os_file_type_t* type)
{
#ifdef _WIN32
	return(os_file_status_win32(path, exists, type));
#else
	return(os_file_status_posix(path, exists, type));
#endif /* _WIN32 */
}

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
{
	/* In this debugging mode, we act as if punch hole is supported,
	and then skip any calls to actually punch a hole here.
	In this way, Transparent Page Compression is still being tested. */
	DBUG_EXECUTE_IF("ignore_punch_hole",
		return(DB_SUCCESS);
	);

#ifdef _WIN32
	return(os_file_punch_hole_win32(fh, off, len));
#else
	return(os_file_punch_hole_posix(fh, off, len));
#endif /* _WIN32 */
}

/** Check if the file system supports sparse files.

Warning: On POSIX systems we try and punch a hole from offset 0 to
the system configured page size. This should only be called on an empty
file.

Note: On Windows we use the name and on Unices we use the file handle.

@param[in]	name		File name
@param[in]	fh		File handle for the file - if opened
@return true if the file system supports sparse files */
bool
os_is_sparse_file_supported(const char* path, pfs_os_file_t fh)
{
	/* In this debugging mode, we act as if punch hole is supported,
	then we skip any calls to actually punch a hole.  In this way,
	Transparent Page Compression is still being tested. */
	DBUG_EXECUTE_IF("ignore_punch_hole",
		return(true);
	);

#ifdef _WIN32
	return(os_is_sparse_file_supported_win32(path));
#else
	dberr_t	err;

	/* We don't know the FS block size, use the sector size. The FS
	will do the magic. */
	err = os_file_punch_hole(fh.m_file, 0, UNIV_PAGE_SIZE);

	return(err == DB_SUCCESS);
#endif /* _WIN32 */
}

/** This function returns information about the specified file
@param[in]	path		pathname of the file
@param[out]	stat_info	information of a file in a directory
@param[in]	check_rw_perm	for testing whether the file can be opened
				in RW mode
@param[in]	read_only	true if file is opened in read-only mode
@return DB_SUCCESS if all OK */
dberr_t
os_file_get_status(
	const char*	path,
	os_file_stat_t* stat_info,
	bool		check_rw_perm,
	bool		read_only)
{
	dberr_t	ret;

#ifdef _WIN32
	struct _stat64	info;

	ret = os_file_get_status_win32(
		path, stat_info, &info, check_rw_perm, read_only);

#else
	struct stat	info;

	ret = os_file_get_status_posix(
		path, stat_info, &info, check_rw_perm, read_only);

#endif /* _WIN32 */

	if (ret == DB_SUCCESS) {
		stat_info->ctime = info.st_ctime;
		stat_info->atime = info.st_atime;
		stat_info->mtime = info.st_mtime;
		stat_info->size  = info.st_size;
	}

	return(ret);
}

/**
Waits for an AIO operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@param[in]	segment		The number of the segment in the aio arrays to
				wait for; segment 0 is the ibuf I/O thread,
				segment 1 the log I/O thread, then follow the
				non-ibuf read threads, and as the last are the
				non-ibuf write threads; if this is
				ULINT_UNDEFINED, then it means that sync AIO
				is used, and this parameter is ignored
@param[out]	m1		the messages passed with the AIO request; note
				that also in the case where the AIO operation
				failed, these output parameters are valid and
				can be used to restart the operation,
				for example
@param[out]	m2		callback message
@param[out]	type		OS_FILE_WRITE or ..._READ
@return DB_SUCCESS or error code */
dberr_t
os_aio_handler(
	ulint		segment,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	request)
{
	dberr_t	err;

	if (srv_use_native_aio) {
		srv_set_io_thread_op_info(segment, "native aio handle");

#ifdef WIN_ASYNC_IO

		err = os_aio_windows_handler(segment, 0, m1, m2, request);

#elif defined(LINUX_NATIVE_AIO)

		err = os_aio_linux_handler(segment, m1, m2, request);

#else
		ut_error;

		err = DB_ERROR; /* Eliminate compiler warning */

#endif /* WIN_ASYNC_IO */

	} else {
		srv_set_io_thread_op_info(segment, "simulated aio handle");

		err = os_aio_simulated_handler(segment, m1, m2, request);
	}

	return(err);
}

/** Constructor
@param[in]	id		The latch ID
@param[in]	n		Number of AIO slots
@param[in]	segments	Number of segments */
AIO::AIO(
	latch_id_t	id,
	ulint		n,
	ulint		segments)
	:
	m_slots(n),
	m_n_segments(segments),
	m_n_reserved()
# ifdef LINUX_NATIVE_AIO
	,m_aio_ctx(),
	m_events(m_slots.size())
# elif defined(_WIN32)
	,m_handles()
# endif /* LINUX_NATIVE_AIO */
{
	ut_a(n > 0);
	ut_a(m_n_segments > 0);

	mutex_create(id, &m_mutex);

	m_not_full = os_event_create("aio_not_full");
	m_is_empty = os_event_create("aio_is_empty");

	std::uninitialized_fill(m_slots.begin(), m_slots.end(), Slot());
#ifdef LINUX_NATIVE_AIO
	memset(&m_events[0], 0x0, sizeof(m_events[0]) * m_events.size());
#endif /* LINUX_NATIVE_AIO */

	os_event_set(m_is_empty);
}

/** Initialise the slots */
dberr_t
AIO::init_slots()
{
	for (ulint i = 0; i < m_slots.size(); ++i) {
		Slot&	slot = m_slots[i];

		slot.pos = static_cast<uint16_t>(i);

		slot.is_reserved = false;

#ifdef WIN_ASYNC_IO

		slot.handle = CreateEvent(NULL, TRUE, FALSE, NULL);

		OVERLAPPED*	over = &slot.control;

		over->hEvent = slot.handle;

		(*m_handles)[i] = over->hEvent;

#elif defined(LINUX_NATIVE_AIO)

		slot.ret = 0;

		slot.n_bytes = 0;

		memset(&slot.control, 0x0, sizeof(slot.control));

#endif /* WIN_ASYNC_IO */
	}

	return(DB_SUCCESS);
}

#ifdef LINUX_NATIVE_AIO
/** Initialise the Linux Native AIO interface */
dberr_t
AIO::init_linux_native_aio()
{
	/* Initialize the io_context array. One io_context
	per segment in the array. */

	ut_a(m_aio_ctx == NULL);

	m_aio_ctx = static_cast<io_context**>(
		ut_zalloc_nokey(m_n_segments * sizeof(*m_aio_ctx)));

	if (m_aio_ctx == NULL) {
		return(DB_OUT_OF_MEMORY);
	}

	io_context**	ctx = m_aio_ctx;
	ulint		max_events = slots_per_segment();

	for (ulint i = 0; i < m_n_segments; ++i, ++ctx) {

		if (!linux_create_io_ctx(max_events, ctx)) {
			/* If something bad happened during aio setup
			we should call it a day and return right away.
			We don't care about any leaks because a failure
			to initialize the io subsystem means that the
			server (or atleast the innodb storage engine)
			is not going to startup. */
			return(DB_IO_ERROR);
		}
	}

	return(DB_SUCCESS);
}
#endif /* LINUX_NATIVE_AIO */

/** Initialise the array */
dberr_t
AIO::init()
{
	ut_a(!m_slots.empty());

#ifdef _WIN32
	ut_a(m_handles == NULL);

	m_handles = UT_NEW_NOKEY(Handles(m_slots.size()));
#endif /* _WIN32 */

	if (srv_use_native_aio) {
#ifdef LINUX_NATIVE_AIO
		dberr_t	err = init_linux_native_aio();

		if (err != DB_SUCCESS) {
			return(err);
		}

#endif /* LINUX_NATIVE_AIO */
	}

	return(init_slots());
}

/** Creates an aio wait array. Note that we return NULL in case of failure.
We don't care about freeing memory here because we assume that a
failure will result in server refusing to start up.
@param[in]	id		Latch ID
@param[in]	n		maximum number of pending AIO operations
				allowed; n must be divisible by m_n_segments
@param[in]	n_segments	number of segments in the AIO array
@return own: AIO array, NULL on failure */
AIO*
AIO::create(
	latch_id_t	id,
	ulint		n,
	ulint		n_segments)
{
	if ((n % n_segments)) {

		ib::error()
			<< "Maximum number of AIO operations must be "
			<< "divisible by number of segments";

		return(NULL);
	}

	AIO*	array = UT_NEW_NOKEY(AIO(id, n, n_segments));

	if (array != NULL && array->init() != DB_SUCCESS) {

		UT_DELETE(array);

		array = NULL;
	}

	return(array);
}

/** AIO destructor */
AIO::~AIO()
{
#ifdef WIN_ASYNC_IO
	for (ulint i = 0; i < m_slots.size(); ++i) {
		CloseHandle(m_slots[i].handle);
	}
#endif /* WIN_ASYNC_IO */

#ifdef _WIN32
	UT_DELETE(m_handles);
#endif /* _WIN32 */

	mutex_destroy(&m_mutex);

	os_event_destroy(m_not_full);
	os_event_destroy(m_is_empty);

#if defined(LINUX_NATIVE_AIO)
	if (srv_use_native_aio) {
		m_events.clear();
		ut_free(m_aio_ctx);
	}
#endif /* LINUX_NATIVE_AIO */

	m_slots.clear();
}

/** Initializes the asynchronous io system. Creates one array each for ibuf
and log i/o. Also creates one array each for read and write where each
array is divided logically into n_readers and n_writers
respectively. The caller must create an i/o handler thread for each
segment in these arrays. This function also creates the sync array.
No i/o handler thread needs to be created for that
@param[in]	n_per_seg	maximum number of pending aio
				operations allowed per segment
@param[in]	n_readers	number of reader threads
@param[in]	n_writers	number of writer threads
@param[in]	n_slots_sync	number of slots in the sync aio array
@return true if the AIO sub-system was started successfully */
bool
AIO::start(
	ulint		n_per_seg,
	ulint		n_readers,
	ulint		n_writers,
	ulint		n_slots_sync)
{
#if defined(LINUX_NATIVE_AIO)
	/* Check if native aio is supported on this system and tmpfs */
	if (srv_use_native_aio && !is_linux_native_aio_supported()) {

		ib::warn() << "Linux Native AIO disabled.";

		srv_use_native_aio = FALSE;
	}
#endif /* LINUX_NATIVE_AIO */

	srv_reset_io_thread_op_info();

	s_reads = create(
		LATCH_ID_OS_AIO_READ_MUTEX, n_readers * n_per_seg, n_readers);

	if (s_reads == NULL) {
		return(false);
	}

	ulint	start = srv_read_only_mode ? 0 : 2;
	ulint	n_segs = n_readers + start;

	/* 0 is the ibuf segment and 1 is the redo log segment. */
	for (ulint i = start; i < n_segs; ++i) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "read thread";
	}

	ulint	n_segments = n_readers;

	if (!srv_read_only_mode) {

		s_ibuf = create(LATCH_ID_OS_AIO_IBUF_MUTEX, n_per_seg, 1);

		if (s_ibuf == NULL) {
			return(false);
		}

		++n_segments;

		srv_io_thread_function[0] = "insert buffer thread";

		s_log = create(LATCH_ID_OS_AIO_LOG_MUTEX, n_per_seg, 1);

		if (s_log == NULL) {
			return(false);
		}

		++n_segments;

		srv_io_thread_function[1] = "log thread";

	} else {
		s_ibuf = s_log = NULL;
	}

	s_writes = create(
		LATCH_ID_OS_AIO_WRITE_MUTEX, n_writers * n_per_seg, n_writers);

	if (s_writes == NULL) {
		return(false);
	}

	n_segments += n_writers;

	for (ulint i = start + n_readers; i < n_segments; ++i) {
		ut_a(i < SRV_MAX_N_IO_THREADS);
		srv_io_thread_function[i] = "write thread";
	}

	ut_ad(n_segments >= static_cast<ulint>(srv_read_only_mode ? 2 : 4));

	s_sync = create(LATCH_ID_OS_AIO_SYNC_MUTEX, n_slots_sync, 1);

	if (s_sync == NULL) {

		return(false);
	}

	os_aio_n_segments = n_segments;

	os_aio_validate();

	os_aio_segment_wait_events = static_cast<os_event_t*>(
		ut_zalloc_nokey(
			n_segments * sizeof *os_aio_segment_wait_events));

	if (os_aio_segment_wait_events == NULL) {

		return(false);
	}

	for (ulint i = 0; i < n_segments; ++i) {
		os_aio_segment_wait_events[i] = os_event_create(0);
	}

	os_last_printout = ut_time();

	return(true);
}

/** Free the AIO arrays */
void
AIO::shutdown()
{
	UT_DELETE(s_ibuf);
	s_ibuf = NULL;

	UT_DELETE(s_log);
	s_log = NULL;

	UT_DELETE(s_writes);
	s_writes = NULL;

	UT_DELETE(s_sync);
	s_sync = NULL;

	UT_DELETE(s_reads);
	s_reads = NULL;
}

#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)

/** Max disk sector size */
static const ulint	MAX_SECTOR_SIZE = 4096;

/**
Try and get the FusionIO sector size. */
void
os_fusionio_get_sector_size()
{
	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT
	    || srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC) {
		ulint		sector_size = UNIV_SECTOR_SIZE;
		char*		path = srv_data_home;
		os_file_t	check_file;
		byte*		ptr;
		byte*		block_ptr;
		char		current_dir[3];
		char*		dir_end;
		ulint		dir_len;
		ulint		check_path_len;
		char*		check_file_name;
		ssize_t		ret;

		/* If the srv_data_home is empty, set the path to
		current dir. */
		if (*path == 0) {
			current_dir[0] = FN_CURLIB;
			current_dir[1] = FN_LIBCHAR;
			current_dir[2] = 0;
			path = current_dir;
		}

		/* Get the path of data file */
		dir_end = strrchr(path, OS_PATH_SEPARATOR);
		dir_len = dir_end? dir_end - path : strlen(path);

		/* allocate a new path and move the directory path to it. */
		check_path_len = dir_len + sizeof "/check_sector_size";
		check_file_name = static_cast<char*>(
			ut_zalloc_nokey(check_path_len));
		memcpy(check_file_name, path, dir_len);

		/* Construct a check file name. */
		strcat(check_file_name + dir_len, "/check_sector_size");

		/* Create a tmp file for checking sector size. */
		check_file = ::open(check_file_name,
				    O_CREAT|O_TRUNC|O_WRONLY|O_DIRECT,
				    S_IRWXU);

		if (check_file == -1) {
			ib::error()
				<< "Failed to create check sector file, errno:"
				<< errno << " Please confirm O_DIRECT is"
				<< " supported and remove the file "
				<< check_file_name << " if it exists.";
			ut_free(check_file_name);
			errno = 0;
			return;
		}

		/* Try to write the file with different sector size
		alignment. */
		ptr = static_cast<byte*>(ut_malloc_nokey(2 * MAX_SECTOR_SIZE));

		while (sector_size <= MAX_SECTOR_SIZE) {
			block_ptr = static_cast<byte*>(
				ut_align(ptr, sector_size));
			ret = pwrite(check_file, block_ptr,
				    sector_size, 0);
			if (ret > 0 && (ulint) ret == sector_size) {
				break;
			}
			sector_size *= 2;
		}

		/* The sector size should <= MAX_SECTOR_SIZE. */
		ut_ad(sector_size <= MAX_SECTOR_SIZE);

		close(check_file);
		unlink(check_file_name);

		ut_free(check_file_name);
		ut_free(ptr);
		errno = 0;

		os_io_ptr_align = sector_size;
	}
}
#endif /* !NO_FALLOCATE && UNIV_LINUX */

/** Initializes the asynchronous io system. Creates one array each for ibuf
and log i/o. Also creates one array each for read and write where each
array is divided logically into n_readers and n_writers
respectively. The caller must create an i/o handler thread for each
segment in these arrays. This function also creates the sync array.
No i/o handler thread needs to be created for that
@param[in]	n_readers	number of reader threads
@param[in]	n_writers	number of writer threads
@param[in]	n_slots_sync	number of slots in the sync aio array */
bool
os_aio_init(
	ulint		n_readers,
	ulint		n_writers,
	ulint		n_slots_sync)
{
	/* Maximum number of pending aio operations allowed per segment */
	ulint		limit = 8 * OS_AIO_N_PENDING_IOS_PER_THREAD;

#ifdef _WIN32
	if (srv_use_native_aio) {
		limit = SRV_N_PENDING_IOS_PER_THREAD;
	}
#endif /* _WIN32 */

	ut_a(block_cache == NULL);

	block_cache = UT_NEW_NOKEY(Blocks(MAX_BLOCKS));

	for (Blocks::iterator it = block_cache->begin();
	     it != block_cache->end();
	     ++it) {

		ut_a(it->m_in_use == 0);
		ut_a(it->m_ptr == NULL);

		/* Allocate double of max page size memory, since
		compress could generate more bytes than orgininal
		data. */
		it->m_ptr = static_cast<byte*>(
			ut_malloc_nokey(BUFFER_BLOCK_SIZE));

		ut_a(it->m_ptr != NULL);
	}

	/* Get sector size for DIRECT_IO. In this case, we need to
	know the sector size for aligning the write buffer. */
#if !defined(NO_FALLOCATE) && defined(UNIV_LINUX)
	os_fusionio_get_sector_size();
#endif /* !NO_FALLOCATE && UNIV_LINUX */

	return(AIO::start(limit, n_readers, n_writers, n_slots_sync));
}

/** Frees the asynchronous io system. */
void
os_aio_free()
{
	AIO::shutdown();

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		os_event_destroy(os_aio_segment_wait_events[i]);
	}

	ut_free(os_aio_segment_wait_events);
	os_aio_segment_wait_events = 0;
	os_aio_n_segments = 0;

	for (Blocks::iterator it = block_cache->begin();
	     it != block_cache->end();
	     ++it) {

		ut_a(it->m_in_use == 0);
		ut_free(it->m_ptr);
	}

	UT_DELETE(block_cache);

	block_cache = NULL;
}

/** Wakes up all async i/o threads so that they know to exit themselves in
shutdown. */
void
os_aio_wake_all_threads_at_shutdown()
{
#ifdef WIN_ASYNC_IO

	AIO::wake_at_shutdown();

#elif defined(LINUX_NATIVE_AIO)

	/* When using native AIO interface the io helper threads
	wait on io_getevents with a timeout value of 500ms. At
	each wake up these threads check the server status.
	No need to do anything to wake them up. */

	if (srv_use_native_aio) {
		return;
	}

#endif /* !WIN_ASYNC_AIO */

	/* Fall through to simulated AIO handler wakeup if we are
	not using native AIO. */

	/* This loop wakes up all simulated ai/o threads */

	for (ulint i = 0; i < os_aio_n_segments; ++i) {

		os_event_set(os_aio_segment_wait_events[i]);
	}
}

/** Waits until there are no pending writes in AIO::s_writes. There can
be other, synchronous, pending writes. */
void
os_aio_wait_until_no_pending_writes()
{
	AIO::wait_until_no_pending_writes();
}

/** Calculates segment number for a slot.
@param[in]	array		AIO wait array
@param[in]	slot		slot in this array
@return segment number (which is the number used by, for example,
	I/O-handler threads) */
ulint
AIO::get_segment_no_from_slot(
	const AIO*	array,
	const Slot*	slot)
{
	ulint	segment;
	ulint	seg_len;

	if (array == s_ibuf) {
		ut_ad(!srv_read_only_mode);

		segment = IO_IBUF_SEGMENT;

	} else if (array == s_log) {
		ut_ad(!srv_read_only_mode);

		segment = IO_LOG_SEGMENT;

	} else if (array == s_reads) {
		seg_len = s_reads->slots_per_segment();

		segment = (srv_read_only_mode ? 0 : 2) + slot->pos / seg_len;
	} else {
		ut_a(array == s_writes);

		seg_len = s_writes->slots_per_segment();

		segment = s_reads->m_n_segments
			+ (srv_read_only_mode ? 0 : 2) + slot->pos / seg_len;
	}

	return(segment);
}

/** Requests for a slot in the aio array. If no slot is available, waits until
not_full-event becomes signaled.

@param[in,out]	type		IO context
@param[in,out]	m1		message to be passed along with the AIO
				operation
@param[in,out]	m2		message to be passed along with the AIO
				operation
@param[in]	file		file handle
@param[in]	name		name of the file or path as a NUL-terminated
				string
@param[in,out]	buf		buffer where to read or from which to write
@param[in]	offset		file offset, where to read from or start writing
@param[in]	len		length of the block to read or write
@return pointer to slot */
Slot*
AIO::reserve_slot(
	IORequest&	type,
	fil_node_t*	m1,
	void*		m2,
	pfs_os_file_t	file,
	const char*	name,
	void*		buf,
	os_offset_t	offset,
	ulint		len)
{
#ifdef WIN_ASYNC_IO
	ut_a((len & 0xFFFFFFFFUL) == len);
#endif /* WIN_ASYNC_IO */

	/* No need of a mutex. Only reading constant fields */
	ulint		slots_per_seg;

	ut_ad(type.validate());

	slots_per_seg = slots_per_segment();

	/* We attempt to keep adjacent blocks in the same local
	segment. This can help in merging IO requests when we are
	doing simulated AIO */
	ulint		local_seg;

	local_seg = (offset >> (UNIV_PAGE_SIZE_SHIFT + 6)) % m_n_segments;

	for (;;) {

		acquire();

		if (m_n_reserved != m_slots.size()) {
			break;
		}

		release();

		if (!srv_use_native_aio) {
			/* If the handler threads are suspended,
			wake them so that we get more slots */

			os_aio_simulated_wake_handler_threads();
		}

		os_event_wait(m_not_full);
	}

	ulint	counter = 0;
	Slot*	slot = NULL;

	/* We start our search for an available slot from our preferred
	local segment and do a full scan of the array. We are
	guaranteed to find a slot in full scan. */
	for (ulint i = local_seg * slots_per_seg;
	     counter < m_slots.size();
	     ++i, ++counter) {

		i %= m_slots.size();

		slot = at(i);

		if (slot->is_reserved == false) {
			break;
		}
	}

	/* We MUST always be able to get hold of a reserved slot. */
	ut_a(counter < m_slots.size());

	ut_a(slot->is_reserved == false);

	++m_n_reserved;

	if (m_n_reserved == 1) {
		os_event_reset(m_is_empty);
	}

	if (m_n_reserved == m_slots.size()) {
		os_event_reset(m_not_full);
	}

	slot->is_reserved = true;
	slot->reservation_time = ut_time();
	slot->m1       = m1;
	slot->m2       = m2;
	slot->file     = file;
	slot->name     = name;
#ifdef _WIN32
	slot->len      = static_cast<DWORD>(len);
#else
	slot->len      = static_cast<ulint>(len);
#endif /* _WIN32 */
	slot->type     = type;
	slot->buf      = static_cast<byte*>(buf);
	slot->ptr      = slot->buf;
	slot->offset   = offset;
	slot->err      = DB_SUCCESS;
	slot->original_len = static_cast<uint32>(len);
	slot->io_already_done = false;
	slot->buf_block = NULL;

	if (srv_use_native_aio
	    && offset > 0
	    && type.is_write()
	    && type.is_compressed()) {
		ulint	compressed_len = len;

		ut_ad(!type.is_log());

		release();

		void* src_buf = slot->buf;
		slot->buf_block = os_file_compress_page(
			type,
			src_buf,
			&compressed_len);

		slot->buf = static_cast<byte*>(src_buf);
		slot->ptr = slot->buf;
#ifdef _WIN32
		slot->len = static_cast<DWORD>(compressed_len);
#else
		slot->len = static_cast<ulint>(compressed_len);
#endif /* _WIN32 */
		slot->skip_punch_hole = !type.punch_hole();

		acquire();
	}

	/* We do encryption after compression, since if we do encryption
	before compression, the encrypted data will cause compression fail
	or low compression rate. */
	if (srv_use_native_aio
	    && offset > 0
	    && type.is_write()
	    && type.is_encrypted()) {
		ulint		encrypted_len = slot->len;
		Block*		encrypted_block;

		ut_ad(!type.is_log());

		release();

		void* src_buf = slot->buf;
		encrypted_block = os_file_encrypt_page(
			type,
			src_buf,
			&encrypted_len);

		if (slot->buf_block != NULL) {
			os_free_block(slot->buf_block);
		}

		slot->buf_block = encrypted_block;
		slot->buf = static_cast<byte*>(src_buf);
		slot->ptr = slot->buf;

#ifdef _WIN32
		slot->len = static_cast<DWORD>(encrypted_len);
#else
		slot->len = static_cast<ulint>(encrypted_len);
#endif /* _WIN32 */

		acquire();
        }

#ifdef WIN_ASYNC_IO
	{
		OVERLAPPED*	control;

		control = &slot->control;
		control->Offset = (DWORD) offset & 0xFFFFFFFF;
		control->OffsetHigh = (DWORD) (offset >> 32);

		ResetEvent(slot->handle);
	}
#elif defined(LINUX_NATIVE_AIO)

	/* If we are not using native AIO skip this part. */
	if (srv_use_native_aio) {

		off_t		aio_offset;

		/* Check if we are dealing with 64 bit arch.
		If not then make sure that offset fits in 32 bits. */
		aio_offset = (off_t) offset;

		ut_a(sizeof(aio_offset) >= sizeof(offset)
		     || ((os_offset_t) aio_offset) == offset);

		struct iocb*	iocb = &slot->control;

		if (type.is_read()) {
			io_prep_pread(
				iocb, file.m_file, slot->ptr, slot->len, aio_offset);
		} else {
			ut_ad(type.is_write());
			io_prep_pwrite(
				iocb, file.m_file, slot->ptr, slot->len, aio_offset);
		}

		iocb->data = slot;

		slot->n_bytes = 0;
		slot->ret = 0;
	}
#endif /* LINUX_NATIVE_AIO */

	release();

	return(slot);
}

/** Wakes up a simulated aio i/o-handler thread if it has something to do.
@param[in]	global_segment	The number of the segment in the AIO arrays */
void
AIO::wake_simulated_handler_thread(ulint global_segment)
{
	ut_ad(!srv_use_native_aio);

	AIO*	array;
	ulint	segment = get_array_and_local_segment(&array, global_segment);

	array->wake_simulated_handler_thread(global_segment, segment);
}

/** Wakes up a simulated AIO I/O-handler thread if it has something to do
for a local segment in the AIO array.
@param[in]	global_segment	The number of the segment in the AIO arrays
@param[in]	segment		The local segment in the AIO array */
void
AIO::wake_simulated_handler_thread(ulint global_segment, ulint segment)
{
	ut_ad(!srv_use_native_aio);

	ulint	n = slots_per_segment();
	ulint	offset = segment * n;

	/* Look through n slots after the segment * n'th slot */

	acquire();

	const Slot*	slot = at(offset);

	for (ulint i = 0; i < n; ++i, ++slot) {

		if (slot->is_reserved) {

			/* Found an i/o request */

			release();

			os_event_t	event;

			event = os_aio_segment_wait_events[global_segment];

			os_event_set(event);

			return;
		}
	}

	release();
}

/** Wakes up simulated aio i/o-handler threads if they have something to do. */
void
os_aio_simulated_wake_handler_threads()
{
	if (srv_use_native_aio) {
		/* We do not use simulated aio: do nothing */

		return;
	}

	os_aio_recommend_sleep_for_read_threads	= false;

	for (ulint i = 0; i < os_aio_n_segments; i++) {
		AIO::wake_simulated_handler_thread(i);
	}
}

/** Select the IO slot array
@param[in]	type		Type of IO, READ or WRITE
@param[in]	read_only	true if running in read-only mode
@param[in]	mode		IO mode
@return slot array or NULL if invalid mode specified */
AIO*
AIO::select_slot_array(IORequest& type, bool read_only, ulint mode)
{
	AIO*	array;

	ut_ad(type.validate());

	switch (mode) {
	case OS_AIO_NORMAL:

		array = type.is_read() ? AIO::s_reads : AIO::s_writes;
		break;

	case OS_AIO_IBUF:
		ut_ad(type.is_read());

		/* Reduce probability of deadlock bugs in connection with ibuf:
		do not let the ibuf i/o handler sleep */

		type.clear_do_not_wake();

		array = read_only ? AIO::s_reads : AIO::s_ibuf;
		break;

	case OS_AIO_LOG:

		array = read_only ? AIO::s_reads : AIO::s_log;
		break;

	case OS_AIO_SYNC:

		array = AIO::s_sync;
#if defined(LINUX_NATIVE_AIO)
		/* In Linux native AIO we don't use sync IO array. */
		ut_a(!srv_use_native_aio);
#endif /* LINUX_NATIVE_AIO */
		break;

	default:
		ut_error;
		array = NULL; /* Eliminate compiler warning */
	}

	return(array);
}

#ifdef WIN_ASYNC_IO
/** This function is only used in Windows asynchronous i/o.
Waits for an aio operation to complete. This function is used to wait the
for completed requests. The aio array of pending requests is divided
into segments. The thread specifies which segment or slot it wants to wait
for. NOTE: this function will also take care of freeing the aio slot,
therefore no other thread is allowed to do the freeing!
@param[in]	segment		The number of the segment in the aio arrays to
				wait for; segment 0 is the ibuf I/O thread,
				segment 1 the log I/O thread, then follow the
				non-ibuf read threads, and as the last are the
				non-ibuf write threads; if this is
				ULINT_UNDEFINED, then it means that sync AIO
				is used, and this parameter is ignored
@param[in]	pos		this parameter is used only in sync AIO:
				wait for the aio slot at this position
@param[out]	m1		the messages passed with the AIO request; note
				that also in the case where the AIO operation
				failed, these output parameters are valid and
				can be used to restart the operation,
				for example
@param[out]	m2		callback message
@param[out]	type		OS_FILE_WRITE or ..._READ
@return DB_SUCCESS or error code */
static
dberr_t
os_aio_windows_handler(
	ulint		segment,
	ulint		pos,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	type)
{
	Slot*		slot;
	dberr_t		err;
	AIO*		array;
	ulint		orig_seg = segment;

	if (segment == ULINT_UNDEFINED) {
		segment = 0;
		array = AIO::sync_array();
	} else {
		segment = AIO::get_array_and_local_segment(&array, segment);
	}

	/* NOTE! We only access constant fields in os_aio_array. Therefore
	we do not have to acquire the protecting mutex yet */

	ut_ad(os_aio_validate_skip());

	if (array == AIO::sync_array()) {

		WaitForSingleObject(array->at(pos)->handle, INFINITE);

	} else {
		if (orig_seg != ULINT_UNDEFINED) {
			srv_set_io_thread_op_info(orig_seg, "wait Windows aio");
		}

		pos = WaitForMultipleObjects(
			(DWORD) array->slots_per_segment(),
			array->handles(segment),
			FALSE, INFINITE);
	}

	array->acquire();

	if (srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS
	    && array->is_empty()
	    && !buf_page_cleaner_is_active) {

		*m1 = NULL;
		*m2 = NULL;

		array->release();

		return(DB_SUCCESS);
	}

	ulint	n = array->slots_per_segment();

	ut_a(pos >= WAIT_OBJECT_0 && pos <= WAIT_OBJECT_0 + n);

	slot = array->at(pos + segment * n);

	ut_a(slot->is_reserved);

	if (orig_seg != ULINT_UNDEFINED) {
		srv_set_io_thread_op_info(
			orig_seg, "get windows aio return value");
	}

	BOOL	ret;
	ret = GetOverlappedResult(
		slot->file.m_file, &slot->control, &slot->n_bytes, TRUE);
	*m1 = slot->m1;
	*m2 = slot->m2;

	*type = slot->type;

	BOOL	retry = FALSE;

	if (ret && slot->n_bytes == slot->len) {

		err = DB_SUCCESS;

	} else if (os_file_handle_error(slot->name, "Windows aio")) {

		retry = true;

	} else {

		err = DB_IO_ERROR;
	}

	array->release();

	if (retry) {
		/* Retry failed read/write operation synchronously.
		No need to hold array->m_mutex. */

#ifdef UNIV_PFS_IO
		/* This read/write does not go through os_file_read
		and os_file_write APIs, need to register with
		performance schema explicitly here. */
		struct PSI_file_locker* locker = NULL;
		PSI_file_locker_state   state;
		register_pfs_file_io_begin(
			&state, locker, slot->file, slot->len,
			slot->type.is_write()
			? PSI_FILE_WRITE : PSI_FILE_READ, __FILE__, __LINE__);
#endif /* UNIV_PFS_IO */

		ut_a((slot->len & 0xFFFFFFFFUL) == slot->len);

		ssize_t	n_bytes = SyncFileIO::execute(slot);

#ifdef UNIV_PFS_IO
		register_pfs_file_io_end(locker, slot->len);
#endif /* UNIV_PFS_IO */

		if (n_bytes < 0 && GetLastError() == ERROR_IO_PENDING) {
			/* AIO was queued successfully!
			We want a synchronous I/O operation on a
			file where we also use async I/O: in Windows
			we must use the same wait mechanism as for
			async I/O */

			BOOL	ret;
			ret = GetOverlappedResult(
				slot->file.m_file, &slot->control, &slot->n_bytes,
				TRUE);
			n_bytes = ret ? slot->n_bytes : -1;
		}

		err = (n_bytes == slot->len) ? DB_SUCCESS : DB_IO_ERROR;
	}

	if (err == DB_SUCCESS) {
		err = AIOHandler::post_io_processing(slot);
	}

	array->release_with_mutex(slot);

	return(err);
}
#endif /* WIN_ASYNC_IO */

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
	void*		m2)
{
#ifdef WIN_ASYNC_IO
	BOOL		ret = TRUE;
#endif /* WIN_ASYNC_IO */

	ut_ad(n > 0);
	ut_ad((n % OS_FILE_LOG_BLOCK_SIZE) == 0);
	ut_ad((offset % OS_FILE_LOG_BLOCK_SIZE) == 0);
	ut_ad(os_aio_validate_skip());

#ifdef WIN_ASYNC_IO
	ut_ad((n & 0xFFFFFFFFUL) == n);
#endif /* WIN_ASYNC_IO */

	if (mode == OS_AIO_SYNC
#ifdef WIN_ASYNC_IO
	    && !srv_use_native_aio
#endif /* WIN_ASYNC_IO */
	    ) {
		/* This is actually an ordinary synchronous read or write:
		no need to use an i/o-handler thread. NOTE that if we use
		Windows async i/o, Windows does not allow us to use
		ordinary synchronous os_file_read etc. on the same file,
		therefore we have built a special mechanism for synchronous
		wait in the Windows case.
		Also note that the Performance Schema instrumentation has
		been performed by current os_aio_func()'s wrapper function
		pfs_os_aio_func(). So we would no longer need to call
		Performance Schema instrumented os_file_read() and
		os_file_write(). Instead, we should use os_file_read_func()
		and os_file_write_func() */

		if (type.is_read()) {
			return(os_file_read_func(type, file.m_file, buf, offset, n));
		}

		ut_ad(type.is_write());
		return(os_file_write_func(type, name, file.m_file, buf, offset, n));
	}

try_again:

	AIO*	array;

	array = AIO::select_slot_array(type, read_only, mode);

	Slot*	slot;

	slot = array->reserve_slot(type, m1, m2, file, name, buf, offset, n);

	if (type.is_read()) {

		if (srv_use_native_aio) {

			++os_n_file_reads;

			os_bytes_read_since_printout += n;
#ifdef WIN_ASYNC_IO
			ret = ReadFile(
				file.m_file, slot->ptr, slot->len,
				&slot->n_bytes, &slot->control);
#elif defined(LINUX_NATIVE_AIO)
			if (!array->linux_dispatch(slot)) {
				goto err_exit;
			}
#endif /* WIN_ASYNC_IO */
		} else if (type.is_wake()) {
			AIO::wake_simulated_handler_thread(
				AIO::get_segment_no_from_slot(array, slot));
		}
	} else if (type.is_write()) {

		if (srv_use_native_aio) {
			++os_n_file_writes;

#ifdef WIN_ASYNC_IO
			ret = WriteFile(
				file.m_file, slot->ptr, slot->len,
				&slot->n_bytes, &slot->control);
#elif defined(LINUX_NATIVE_AIO)
			if (!array->linux_dispatch(slot)) {
				goto err_exit;
			}
#endif /* WIN_ASYNC_IO */

		} else if (type.is_wake()) {
			AIO::wake_simulated_handler_thread(
				AIO::get_segment_no_from_slot(array, slot));
		}
	} else {
		ut_error;
	}

#ifdef WIN_ASYNC_IO
	if (srv_use_native_aio) {
		if ((ret && slot->len == slot->n_bytes)
		     || (!ret && GetLastError() == ERROR_IO_PENDING)) {
			/* aio was queued successfully! */

			if (mode == OS_AIO_SYNC) {
				IORequest	dummy_type;
				void*		dummy_mess2;
				struct fil_node_t* dummy_mess1;

				/* We want a synchronous i/o operation on a
				file where we also use async i/o: in Windows
				we must use the same wait mechanism as for
				async i/o */

				return(os_aio_windows_handler(
					ULINT_UNDEFINED, slot->pos,
					&dummy_mess1, &dummy_mess2,
					&dummy_type));
			}

			return(DB_SUCCESS);
		}

		goto err_exit;
	}
#endif /* WIN_ASYNC_IO */

	/* AIO request was queued successfully! */
	return(DB_SUCCESS);

#if defined LINUX_NATIVE_AIO || defined WIN_ASYNC_IO
err_exit:
#endif /* LINUX_NATIVE_AIO || WIN_ASYNC_IO */

	array->release_with_mutex(slot);

	if (os_file_handle_error(
		name, type.is_read() ? "aio read" : "aio write")) {

		goto try_again;
	}

	return(DB_IO_ERROR);
}

/** Simulated AIO handler for reaping IO requests */
class SimulatedAIOHandler {

public:

	/** Constructor
	@param[in,out]	array	The AIO array
	@param[in]	segment	Local segment in the array */
	SimulatedAIOHandler(AIO* array, ulint segment)
		:
		m_oldest(),
		m_n_elems(),
		m_lowest_offset(IB_UINT64_MAX),
		m_array(array),
		m_n_slots(),
		m_segment(segment),
		m_ptr(),
		m_buf()
	{
		ut_ad(m_segment < 100);

		m_slots.resize(OS_AIO_MERGE_N_CONSECUTIVE);
	}

	/** Destructor */
	~SimulatedAIOHandler()
	{
		if (m_ptr != NULL) {
			ut_free(m_ptr);
		}
	}

	/** Reset the state of the handler
	@param[in]	n_slots	Number of pending AIO operations supported */
	void init(ulint n_slots)
	{
		m_oldest = 0;
		m_n_elems = 0;
		m_n_slots = n_slots;
		m_lowest_offset = IB_UINT64_MAX;

		if (m_ptr != NULL) {
			ut_free(m_ptr);
			m_ptr = m_buf = NULL;
		}

		m_slots[0] = NULL;
	}

	/** Check if there is a slot for which the i/o has already been done
	@param[out]	n_reserved	Number of reserved slots
	@return the first completed slot that is found. */
	Slot* check_completed(ulint* n_reserved)
	{
		ulint	offset = m_segment * m_n_slots;

		*n_reserved = 0;

		Slot*	slot;

		slot = m_array->at(offset);

		for (ulint i = 0; i < m_n_slots; ++i, ++slot) {

			if (slot->is_reserved) {

				if (slot->io_already_done) {

					ut_a(slot->is_reserved);

					return(slot);
				}

				++*n_reserved;
			}
		}

		return(NULL);
	}

	/** If there are at least 2 seconds old requests, then pick the
	oldest one to prevent starvation.  If several requests have the
	same age, then pick the one at the lowest offset.
	@return true if request was selected */
	bool select()
	{
		if (!select_oldest()) {

			return(select_lowest_offset());
		}

		return(true);
	}

	/** Check if there are several consecutive blocks
	to read or write. Merge them if found. */
	void merge()
	{
		/* if m_n_elems != 0, then we have assigned
		something valid to consecutive_ios[0] */
		ut_ad(m_n_elems != 0);
		ut_ad(first_slot() != NULL);

		Slot*	slot = first_slot();

		while (!merge_adjacent(slot)) {
			/* No op */
		}
	}

	/** We have now collected n_consecutive I/O requests
	in the array; allocate a single buffer which can hold
	all data, and perform the I/O
	@return the length of the buffer */
	ulint allocate_buffer()
		MY_ATTRIBUTE((warn_unused_result))
	{
		ulint	len;
		Slot*	slot = first_slot();

		ut_ad(m_ptr == NULL);

		if (slot->type.is_read() && m_n_elems > 1) {

			len = 0;

			for (ulint i = 0; i < m_n_elems; ++i) {
				len += m_slots[i]->len;
			}

			m_ptr = static_cast<byte*>(
				ut_malloc_nokey(len + UNIV_PAGE_SIZE));

			m_buf = static_cast<byte*>(
				ut_align(m_ptr, UNIV_PAGE_SIZE));

		} else {
			len = first_slot()->len;
			m_buf = first_slot()->buf;
		}

		return(len);
	}

	/** We have to compress the individual pages and punch
	holes in them on a page by page basis when writing to
	tables that can be compresed at the IO level.
	@param[in]	len		Value returned by allocate_buffer */
	void copy_to_buffer(ulint len)
	{
		Slot*	slot = first_slot();

		if (len > slot->len && slot->type.is_write()) {

			byte*	ptr = m_buf;

			ut_ad(ptr != slot->buf);

			/* Copy the buffers to the combined buffer */
			for (ulint i = 0; i < m_n_elems; ++i) {

				slot = m_slots[i];

				memmove(ptr, slot->buf, slot->len);

				ptr += slot->len;
			}
		}
	}

	/** Do the I/O with ordinary, synchronous i/o functions:
	@param[in]	len		Length of buffer for IO */
	void io()
	{
		if (first_slot()->type.is_write()) {

			for (ulint i = 0; i < m_n_elems; ++i) {
				write(m_slots[i]);
			}

		} else {

			for (ulint i = 0; i < m_n_elems; ++i) {
				read(m_slots[i]);
			}
		}
	}

	/** Do the decompression of the pages read in */
	void io_complete()
	{
		// Note: For non-compressed tables. Not required
		// for correctness.
	}

	/** Mark the i/os done in slots */
	void done()
	{
		for (ulint i = 0; i < m_n_elems; ++i) {
			m_slots[i]->io_already_done = true;
		}
	}

	/** @return the first slot in the consecutive array */
	Slot* first_slot()
		MY_ATTRIBUTE((warn_unused_result))
	{
		ut_a(m_n_elems > 0);

		return(m_slots[0]);
	}

	/** Wait for I/O requests
	@param[in]	global_segment	The global segment
	@param[in,out]	event		Wait on event if no active requests
	@return the number of slots */
	ulint check_pending(
		ulint		global_segment,
		os_event_t	event)
		MY_ATTRIBUTE((warn_unused_result));
private:

	/** Do the file read
	@param[in,out]	slot		Slot that has the IO context */
	void read(Slot* slot)
	{
		dberr_t	err = os_file_read_func(
			slot->type,
			slot->file.m_file,
			slot->ptr,
			slot->offset,
			slot->len);
		ut_a(err == DB_SUCCESS);
	}

	/** Do the file read
	@param[in,out]	slot		Slot that has the IO context */
	void write(Slot* slot)
	{
		dberr_t	err = os_file_write_func(
			slot->type,
			slot->name,
			slot->file.m_file,
			slot->ptr,
			slot->offset,
			slot->len);
		ut_a(err == DB_SUCCESS || err == DB_IO_NO_PUNCH_HOLE);
	}

	/** @return true if the slots are adjacent and can be merged */
	bool adjacent(const Slot* s1, const Slot* s2) const
	{
		return(s1 != s2
		       && s1->file.m_file == s2->file.m_file
		       && s2->offset == s1->offset + s1->len
		       && s1->type == s2->type);
	}

	/** @return true if merge limit reached or no adjacent slots found. */
	bool merge_adjacent(Slot*& current)
	{
		Slot*	slot;
		ulint	offset = m_segment * m_n_slots;

		slot = m_array->at(offset);

		for (ulint i = 0; i < m_n_slots; ++i, ++slot) {

			if (slot->is_reserved && adjacent(current, slot)) {

				current = slot;

				/* Found a consecutive i/o request */

				m_slots[m_n_elems] = slot;

				++m_n_elems;

				return(m_n_elems >= m_slots.capacity());
			}
		}

		return(true);
	}

	/** There were no old requests. Look for an I/O request at the lowest
	offset in the array (we ignore the high 32 bits of the offset in these
	heuristics) */
	bool select_lowest_offset()
	{
		ut_ad(m_n_elems == 0);

		ulint	offset = m_segment * m_n_slots;

		m_lowest_offset = IB_UINT64_MAX;

		for (ulint i = 0; i < m_n_slots; ++i) {
			Slot*	slot;

			slot = m_array->at(i + offset);

			if (slot->is_reserved
			    && slot->offset < m_lowest_offset) {

				/* Found an i/o request */
				m_slots[0] = slot;

				m_n_elems = 1;

				m_lowest_offset = slot->offset;
			}
		}

		return(m_n_elems > 0);
	}

	/** Select the slot if it is older than the current oldest slot.
	@param[in]	slot		The slot to check */
	void select_if_older(Slot* slot)
	{
		ulint	age;

		age = (ulint) difftime(ut_time(), slot->reservation_time);

		if ((age >= 2 && age > m_oldest)
		    || (age >= 2
			&& age == m_oldest
			&& slot->offset < m_lowest_offset)) {

			/* Found an i/o request */
			m_slots[0] = slot;

			m_n_elems = 1;

			m_oldest = age;

			m_lowest_offset = slot->offset;
		}
	}

	/** Select th oldest slot in the array
	@return true if oldest slot found */
	bool select_oldest()
	{
		ut_ad(m_n_elems == 0);

		Slot*	slot;
		ulint	offset = m_n_slots * m_segment;

		slot = m_array->at(offset);

		for (ulint i = 0; i < m_n_slots; ++i, ++slot) {

			if (slot->is_reserved) {
				select_if_older(slot);
			}
		}

		return(m_n_elems > 0);
	}

	typedef std::vector<Slot*> slots_t;

private:
	ulint		m_oldest;
	ulint		m_n_elems;
	os_offset_t	m_lowest_offset;

	AIO*		m_array;
	ulint		m_n_slots;
	ulint		m_segment;

	slots_t		m_slots;

	byte*		m_ptr;
	byte*		m_buf;
};

/** Wait for I/O requests
@return the number of slots */
ulint
SimulatedAIOHandler::check_pending(
	ulint		global_segment,
	os_event_t	event)
{
	/* NOTE! We only access constant fields in os_aio_array.
	Therefore we do not have to acquire the protecting mutex yet */

	ut_ad(os_aio_validate_skip());

	ut_ad(m_segment < m_array->get_n_segments());

	/* Look through n slots after the segment * n'th slot */

	if (AIO::is_read(m_array)
	    && os_aio_recommend_sleep_for_read_threads) {

		/* Give other threads chance to add several
		I/Os to the array at once. */

		srv_set_io_thread_op_info(
			global_segment, "waiting for i/o request");

		os_event_wait(event);

		return(0);
	}

	return(m_array->slots_per_segment());
}

/** Does simulated AIO. This function should be called by an i/o-handler
thread.

@param[in]	segment	The number of the segment in the aio arrays to wait
			for; segment 0 is the ibuf i/o thread, segment 1 the
			log i/o thread, then follow the non-ibuf read threads,
			and as the last are the non-ibuf write threads
@param[out]	m1	the messages passed with the AIO request; note that
			also in the case where the AIO operation failed, these
			output parameters are valid and can be used to restart
			the operation, for example
@param[out]	m2	Callback argument
@param[in]	type	IO context
@return DB_SUCCESS or error code */
static
dberr_t
os_aio_simulated_handler(
	ulint		global_segment,
	fil_node_t**	m1,
	void**		m2,
	IORequest*	type)
{
	Slot*		slot;
	AIO*		array;
	ulint		segment;
	os_event_t	event = os_aio_segment_wait_events[global_segment];

	segment = AIO::get_array_and_local_segment(&array, global_segment);

	SimulatedAIOHandler	handler(array, segment);

	for (;;) {

		srv_set_io_thread_op_info(
			global_segment, "looking for i/o requests (a)");

		ulint	n_slots = handler.check_pending(global_segment, event);

		if (n_slots == 0) {
			continue;
		}

		handler.init(n_slots);

		srv_set_io_thread_op_info(
			global_segment, "looking for i/o requests (b)");

		array->acquire();

		ulint	n_reserved;

		slot = handler.check_completed(&n_reserved);

		if (slot != NULL) {

			break;

		} else if (n_reserved == 0
			   && !buf_page_cleaner_is_active
			   && srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS) {

			/* There is no completed request. If there
			are no pending request at all, and the system
			is being shut down, exit. */

			array->release();

			*m1 = NULL;

			*m2 = NULL;

			return(DB_SUCCESS);

		} else if (handler.select()) {

			break;
		}

		/* No I/O requested at the moment */

		srv_set_io_thread_op_info(
			global_segment, "resetting wait event");

		/* We wait here until tbere are more IO requests
		for this segment. */

		os_event_reset(event);

		array->release();

		srv_set_io_thread_op_info(
			global_segment, "waiting for i/o request");

		os_event_wait(event);
	}

	/** Found a slot that has already completed its IO */

	if (slot == NULL) {
		/* Merge adjacent requests */
		handler.merge();

		/* Check if there are several consecutive blocks
		to read or write */

		srv_set_io_thread_op_info(
			global_segment, "consecutive i/o requests");

		// Note: We don't support write combining for simulated AIO.
		//ulint	total_len = handler.allocate_buffer();

		/* We release the array mutex for the time of the I/O: NOTE that
		this assumes that there is just one i/o-handler thread serving
		a single segment of slots! */

		array->release();

		// Note: We don't support write combining for simulated AIO.
		//handler.copy_to_buffer(total_len);

		srv_set_io_thread_op_info(global_segment, "doing file i/o");

		handler.io();

		srv_set_io_thread_op_info(global_segment, "file i/o done");

		handler.io_complete();

		array->acquire();

		handler.done();

		/* We return the messages for the first slot now, and if there
		were several slots, the messages will be returned with
		subsequent calls of this function */

		slot = handler.first_slot();
	}

	ut_ad(slot->is_reserved);

	*m1 = slot->m1;
	*m2 = slot->m2;

	*type = slot->type;

	array->release(slot);

	array->release();

	return(DB_SUCCESS);
}

/** Get the total number of pending IOs
@return the total number of pending IOs */
ulint
AIO::total_pending_io_count()
{
	ulint	count = s_reads->pending_io_count();

	if (s_writes != NULL) {
		count += s_writes->pending_io_count();
	}

	if (s_ibuf != NULL) {
		count += s_ibuf->pending_io_count();
	}

	if (s_log != NULL) {
		count += s_log->pending_io_count();
	}

	if (s_sync != NULL) {
		count += s_sync->pending_io_count();
	}

	return(count);
}

/** Validates the consistency the aio system.
@return true if ok */
static
bool
os_aio_validate()
{
	/* The methods countds and validates, we ignore the count. */
	AIO::total_pending_io_count();

	return(true);
}

/** Prints pending IO requests per segment of an aio array.
We probably don't need per segment statistics but they can help us
during development phase to see if the IO requests are being
distributed as expected.
@param[in,out]	file		File where to print
@param[in]	segments	Pending IO array */
void
AIO::print_segment_info(
	FILE*		file,
	const ulint*	segments)
{
	ut_ad(m_n_segments > 0);

	if (m_n_segments > 1) {

		fprintf(file, " [");

		for (ulint i = 0; i < m_n_segments; ++i, ++segments) {

			if (i != 0) {
				fprintf(file, ", ");
			}

			fprintf(file, ULINTPF, *segments);
		}

		fprintf(file, "] ");
	}
}

/** Prints info about the aio array.
@param[in,out]	file		Where to print */
void
AIO::print(FILE* file)
{
	ulint	count = 0;
	ulint	n_res_seg[SRV_MAX_N_IO_THREADS];

	mutex_enter(&m_mutex);

	ut_a(!m_slots.empty());
	ut_a(m_n_segments > 0);

	memset(n_res_seg, 0x0, sizeof(n_res_seg));

	for (ulint i = 0; i < m_slots.size(); ++i) {
		Slot&	slot = m_slots[i];
		ulint	segment = (i * m_n_segments) / m_slots.size();

		if (slot.is_reserved) {

			++count;

			++n_res_seg[segment];

			ut_a(slot.len > 0);
		}
	}

	ut_a(m_n_reserved == count);

	print_segment_info(file, n_res_seg);

	mutex_exit(&m_mutex);
}

/** Print all the AIO segments
@param[in,out]	file		Where to print */
void
AIO::print_all(FILE* file)
{
	s_reads->print(file);

	if (s_writes != NULL) {
		fputs(", aio writes:", file);
		s_writes->print(file);
	}

	if (s_ibuf != NULL) {
		fputs(",\n ibuf aio reads:", file);
		s_ibuf->print(file);
	}

	if (s_log != NULL) {
		fputs(", log i/o's:", file);
		s_log->print(file);
	}

	if (s_sync != NULL) {
		fputs(", sync i/o's:", file);
		s_sync->print(file);
	}
}

/** Prints info of the aio arrays.
@param[in,out]	file		file where to print */
void
os_aio_print(FILE*	file)
{
	time_t		current_time;
	double		time_elapsed;
	double		avg_bytes_read;

	for (ulint i = 0; i < srv_n_file_io_threads; ++i) {
		fprintf(file, "I/O thread %lu state: %s (%s)",
			(ulong) i,
			srv_io_thread_op_info[i],
			srv_io_thread_function[i]);

#ifndef _WIN32
		if (os_event_is_set(os_aio_segment_wait_events[i])) {
			fprintf(file, " ev set");
		}
#endif /* _WIN32 */

		fprintf(file, "\n");
	}

	fputs("Pending normal aio reads:", file);

	AIO::print_all(file);

	putc('\n', file);
	current_time = ut_time();
	time_elapsed = 0.001 + difftime(current_time, os_last_printout);

	fprintf(file,
		"Pending flushes (fsync) log: " ULINTPF "; "
		"buffer pool: " ULINTPF "\n"
		ULINTPF " OS file reads, "
		ULINTPF " OS file writes, "
		ULINTPF " OS fsyncs\n",
		fil_n_pending_log_flushes,
		fil_n_pending_tablespace_flushes,
		os_n_file_reads,
		os_n_file_writes,
		os_n_fsyncs);

	if (os_n_pending_writes != 0 || os_n_pending_reads != 0) {
		fprintf(file,
			ULINTPF " pending preads, "
			ULINTPF " pending pwrites\n",
			os_n_pending_reads,
			os_n_pending_writes);
	}

	if (os_n_file_reads == os_n_file_reads_old) {
		avg_bytes_read = 0.0;
	} else {
		avg_bytes_read = (double) os_bytes_read_since_printout
			/ (os_n_file_reads - os_n_file_reads_old);
	}

	fprintf(file,
		"%.2f reads/s, %lu avg bytes/read,"
		" %.2f writes/s, %.2f fsyncs/s\n",
		(os_n_file_reads - os_n_file_reads_old)
		/ time_elapsed,
		(ulong) avg_bytes_read,
		(os_n_file_writes - os_n_file_writes_old)
		/ time_elapsed,
		(os_n_fsyncs - os_n_fsyncs_old)
		/ time_elapsed);

	os_n_file_reads_old = os_n_file_reads;
	os_n_file_writes_old = os_n_file_writes;
	os_n_fsyncs_old = os_n_fsyncs;
	os_bytes_read_since_printout = 0;

	os_last_printout = current_time;
}

/** Refreshes the statistics used to print per-second averages. */
void
os_aio_refresh_stats()
{
	os_n_fsyncs_old = os_n_fsyncs;

	os_bytes_read_since_printout = 0;

	os_n_file_reads_old = os_n_file_reads;

	os_n_file_writes_old = os_n_file_writes;

	os_n_fsyncs_old = os_n_fsyncs;

	os_bytes_read_since_printout = 0;

	os_last_printout = ut_time();
}

/** Checks that all slots in the system have been freed, that is, there are
no pending io operations.
@return true if all free */
bool
os_aio_all_slots_free()
{
	return(AIO::total_pending_io_count() == 0);
}

#ifdef UNIV_DEBUG
/** Prints all pending IO for the array
@param[in]	file	file where to print
@param[in]	array	array to process */
void
AIO::to_file(FILE* file) const
{
	acquire();

	fprintf(file, " %lu\n", static_cast<ulong>(m_n_reserved));

	for (ulint i = 0; i < m_slots.size(); ++i) {

		const Slot&	slot = m_slots[i];

		if (slot.is_reserved) {

			fprintf(file,
				"%s IO for %s (offset=" UINT64PF
				", size=%lu)\n",
				slot.type.is_read() ? "read" : "write",
				slot.name, slot.offset, slot.len);
		}
	}

	release();
}

/** Print pending IOs for all arrays */
void
AIO::print_to_file(FILE* file)
{
	fprintf(file, "Pending normal aio reads:");

	s_reads->to_file(file);

	if (s_writes != NULL) {
		fprintf(file, "Pending normal aio writes:");
		s_writes->to_file(file);
	}

	if (s_ibuf != NULL) {
		fprintf(file, "Pending ibuf aio reads:");
		s_ibuf->to_file(file);
	}

	if (s_log != NULL) {
		fprintf(file, "Pending log i/o's:");
		s_log->to_file(file);
	}

	if (s_sync != NULL) {
		fprintf(file, "Pending sync i/o's:");
		s_sync->to_file(file);
	}
}

/** Prints all pending IO
@param[in]	file		File where to print */
void
os_aio_print_pending_io(
	FILE*	file)
{
	AIO::print_to_file(file);
}

#endif /* UNIV_DEBUG */

/**
Set the file create umask
@param[in]	umask		The umask to use for file creation. */
void
os_file_set_umask(ulint umask)
{
	os_innodb_umask = umask;
}
#else

#include "univ.i"
#include "db0err.h"
#include "mach0data.h"
#include "fil0fil.h"
#include "os0file.h"

#include <lz4.h>
#include <zlib.h>

#include <my_aes.h>
#include <my_rnd.h>
#include <mysqld.h>
#include <mysql/service_mysql_keyring.h>

typedef byte	Block;

/** Allocate a page for sync IO
@return pointer to page */
static
Block*
os_alloc_block()
{
	return(reinterpret_cast<byte*>(malloc(UNIV_PAGE_SIZE_MAX * 2)));
}

/** Free a page after sync IO
@param[in,own]	block		The block to free/release */
static
void
os_free_block(Block* block)
{
	ut_free(block);
}

#endif /* !UNIV_INNOCHECKSUM */

/**
@param[in]      type            The compression type
@return the string representation */
const char*
Compression::to_string(Type type)
{
        switch(type) {
        case NONE:
                return("None");
        case ZLIB:
                return("Zlib");
        case LZ4:
                return("LZ4");
        }

        ut_ad(0);

        return("<UNKNOWN>");
}

/**
@param[in]      meta		Page Meta data
@return the string representation */
std::string Compression::to_string(const Compression::meta_t& meta)
{
	std::ostringstream	stream;

	stream	<< "version: " << int(meta.m_version) << " "
		<< "algorithm: " << meta.m_algorithm << " "
		<< "(" << to_string(meta.m_algorithm) << ") "
		<< "orginal_type: " << meta.m_original_type << " "
		<< "original_size: " << meta.m_original_size << " "
		<< "compressed_size: " << meta.m_compressed_size;

	return(stream.str());
}

/** @return true if it is a compressed page */
bool
Compression::is_compressed_page(const byte* page)
{
	return(mach_read_from_2(page + FIL_PAGE_TYPE) == FIL_PAGE_COMPRESSED);
}

/** Deserizlise the page header compression meta-data
@param[in]	page		Pointer to the page header
@param[out]	control		Deserialised data */
void
Compression::deserialize_header(
	const byte*		page,
	Compression::meta_t*	control)
{
	ut_ad(is_compressed_page(page));

	control->m_version = static_cast<uint8_t>(
		mach_read_from_1(page + FIL_PAGE_VERSION));

	control->m_original_type = static_cast<uint16_t>(
		mach_read_from_2(page + FIL_PAGE_ORIGINAL_TYPE_V1));

	control->m_compressed_size = static_cast<uint16_t>(
		mach_read_from_2(page + FIL_PAGE_COMPRESS_SIZE_V1));

	control->m_original_size = static_cast<uint16_t>(
		mach_read_from_2(page + FIL_PAGE_ORIGINAL_SIZE_V1));

	control->m_algorithm = static_cast<Type>(
		mach_read_from_1(page + FIL_PAGE_ALGORITHM_V1));
}

/** Decompress the page data contents. Page type must be FIL_PAGE_COMPRESSED, if
not then the source contents are left unchanged and DB_SUCCESS is returned.
@param[in]	dblwr_recover	true of double write recovery in progress
@param[in,out]	src		Data read from disk, decompressed data will be
				copied to this page
@param[in,out]	dst		Scratch area to use for decompression
@param[in]	dst_len		Size of the scratch area in bytes
@return DB_SUCCESS or error code */
dberr_t
Compression::deserialize(
	bool		dblwr_recover,
	byte*		src,
	byte*		dst,
	ulint		dst_len)
{
	if (!is_compressed_page(src)) {
		/* There is nothing we can do. */
		return(DB_SUCCESS);
	}

	meta_t	header;

	deserialize_header(src, &header);

	byte*	ptr = src + FIL_PAGE_DATA;

	ut_ad(header.m_version == 1);

	if (header.m_version != 1
	    || header.m_original_size < UNIV_PAGE_SIZE_MIN - (FIL_PAGE_DATA + 8)
	    || header.m_original_size > UNIV_PAGE_SIZE_MAX - FIL_PAGE_DATA
	    || dst_len < header.m_original_size + FIL_PAGE_DATA) {

		/* The last check could potentially return DB_OVERFLOW,
		the caller should be able to retry with a larger buffer. */

		return(DB_CORRUPTION);
	}

	Block*	block;

	/* The caller doesn't know what to expect */
	if (dst == NULL) {

		block = os_alloc_block();

#ifdef UNIV_INNOCHECKSUM
		dst = block;
#else
		dst = block->m_ptr;
#endif /* UNIV_INNOCHECKSUM */

	} else {
		block = NULL;
	}

	int		ret;
	Compression	compression;
	ulint		len = header.m_original_size;

	compression.m_type = static_cast<Compression::Type>(header.m_algorithm);

	switch(compression.m_type) {
	case Compression::ZLIB: {

		uLongf	zlen = header.m_original_size;

		if (uncompress(dst, &zlen, ptr, header.m_compressed_size)
		    != Z_OK) {

			if (block != NULL) {
				os_free_block(block);
			}

			return(DB_IO_DECOMPRESS_FAIL);
		}

		len = static_cast<ulint>(zlen);

		break;
	}

	case Compression::LZ4:

		if (dblwr_recover) {

			ret = LZ4_decompress_safe(
				reinterpret_cast<char*>(ptr),
				reinterpret_cast<char*>(dst),
				header.m_compressed_size,
				header.m_original_size);

		} else {

			/* This can potentially read beyond the input
			buffer if the data is malformed. According to
			the LZ4 documentation it is a little faster
			than the above function. When recovering from
			the double write buffer we can afford to us the
			slower function above. */

			ret = LZ4_decompress_fast(
				reinterpret_cast<char*>(ptr),
				reinterpret_cast<char*>(dst),
				header.m_original_size);
		}

		if (ret < 0) {

			if (block != NULL) {
				os_free_block(block);
			}

			return(DB_IO_DECOMPRESS_FAIL);
		}

		break;

	default:
#if !defined(UNIV_INNOCHECKSUM)
		ib::error()
			<< "Compression algorithm support missing: "
			<< Compression::to_string(compression.m_type);
#else
		fprintf(stderr, "Compression algorithm support missing: %s\n",
			Compression::to_string(compression.m_type));
#endif /* !UNIV_INNOCHECKSUM */

		if (block != NULL) {
			os_free_block(block);
		}

		return(DB_UNSUPPORTED);
	}

	/* Leave the header alone */
	memmove(src + FIL_PAGE_DATA, dst, len);

	mach_write_to_2(src + FIL_PAGE_TYPE, header.m_original_type);

	ut_ad(dblwr_recover
	      || memcmp(src + FIL_PAGE_LSN + 4,
			src + (header.m_original_size + FIL_PAGE_DATA)
			- FIL_PAGE_END_LSN_OLD_CHKSUM + 4, 4) == 0);

	if (block != NULL) {
		os_free_block(block);
	}

	return(DB_SUCCESS);
}

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
{
	return(Compression::deserialize(dblwr_recover, src, dst, dst_len));
}

/**
@param[in]      type            The encryption type
@return the string representation */
const char*
Encryption::to_string(Type type)
{
        switch(type) {
        case NONE:
                return("N");
        case AES:
                return("Y");
        }

        ut_ad(0);

        return("<UNKNOWN>");
}

/** Generate random encryption value for key and iv.
@param[in,out]	value	Encryption value */
void Encryption::random_value(byte* value)
{
	ut_ad(value != NULL);

	my_rand_buffer(value, ENCRYPTION_KEY_LEN);
}

/** Create new master key for key rotation.
@param[in,out]	master_key	master key */
void
Encryption::create_master_key(byte** master_key)
{
#ifndef UNIV_INNOCHECKSUM
	char*	key_type = NULL;
	size_t	key_len;
	char	key_name[ENCRYPTION_MASTER_KEY_NAME_MAX_LEN];
	int	ret;

	/* If uuid does not match with current server uuid,
	set uuid as current server uuid. */
	if (strcmp(uuid, server_uuid) != 0) {
		memcpy(uuid, server_uuid, ENCRYPTION_SERVER_UUID_LEN);
	}
	memset(key_name, 0, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN);

	/* Generate new master key */
	ut_snprintf(key_name, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN,
		    "%s-%s-%lu", ENCRYPTION_MASTER_KEY_PRIFIX,
		    uuid, master_key_id + 1);

	/* We call key ring API to generate master key here. */
	ret = my_key_generate(key_name, "AES",
			      NULL, ENCRYPTION_KEY_LEN);

	/* We call key ring API to get master key here. */
	ret = my_key_fetch(key_name, &key_type, NULL,
			   reinterpret_cast<void**>(master_key),
			   &key_len);

	if (ret || *master_key == NULL) {
		ib::error() << "Encryption can't find master key, please check"
				" the keyring plugin is loaded.";
		*master_key = NULL;
	} else {
		master_key_id++;
	}

	if (key_type) {
		my_free(key_type);
	}
#endif
}

/** Get master key by key id.
@param[in]	master_key_id	master key id
@param[in]	srv_uuid	uuid of server instance
@param[in,out]	master_key	master key */
void
Encryption::get_master_key(ulint master_key_id,
			   char* srv_uuid,
			   byte** master_key)
{
#ifndef UNIV_INNOCHECKSUM
	char*	key_type = NULL;
	size_t	key_len;
	char	key_name[ENCRYPTION_MASTER_KEY_NAME_MAX_LEN];
	int	ret;

	memset(key_name, 0, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN);

	if (srv_uuid != NULL) {
		ut_snprintf(key_name, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN,
			    "%s-%s-%lu", ENCRYPTION_MASTER_KEY_PRIFIX,
			    srv_uuid, master_key_id);
	} else {
		/* For compitable with 5.7.11, we need to get master key with
		server id. */
		memset(key_name, 0, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN);
		ut_snprintf(key_name, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN,
			    "%s-%lu-%lu", ENCRYPTION_MASTER_KEY_PRIFIX,
			    server_id, master_key_id);
	}

	/* We call key ring API to get master key here. */
	ret = my_key_fetch(key_name, &key_type, NULL,
			   reinterpret_cast<void**>(master_key), &key_len);

	if (key_type) {
		my_free(key_type);
	}

	if (ret) {
		*master_key = NULL;
		ib::error() << "Encryption can't find master key, please check"
				" the keyring plugin is loaded.";
	}

#ifdef UNIV_ENCRYPT_DEBUG
	if (!ret && *master_key) {
		fprintf(stderr, "Fetched master key:%lu ", master_key_id);
		ut_print_buf(stderr, *master_key, key_len);
		fprintf(stderr, "\n");
	}
#endif /* DEBUG_TDE */

#endif
}

/** Current master key id */
ulint	Encryption::master_key_id = 0;

/** Current uuid of server instance */
char	Encryption::uuid[ENCRYPTION_SERVER_UUID_LEN + 1] = {0};

/** Get current master key and master key id
@param[in,out]	master_key_id	master key id
@param[in,out]	master_key	master key
@param[in,out]	version		encryption information version */
void
Encryption::get_master_key(ulint* master_key_id,
			   byte** master_key,
			   Encryption::Version*  version)
{
#ifndef UNIV_INNOCHECKSUM
	char*	key_type = NULL;
	size_t	key_len;
	char	key_name[ENCRYPTION_MASTER_KEY_NAME_MAX_LEN];
	int	ret;

	memset(key_name, 0, ENCRYPTION_KEY_LEN);
	*version = Encryption::ENCRYPTION_VERSION_2;

	if (Encryption::master_key_id == 0) {
		/* If m_master_key is 0, means there's no encrypted
		tablespace, we need to generate the first master key,
		and store it to key ring. */
		memset(uuid, 0, ENCRYPTION_SERVER_UUID_LEN + 1);
		memcpy(uuid, server_uuid, ENCRYPTION_SERVER_UUID_LEN);

		/* Prepare the server uuid. */
		ut_snprintf(key_name, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN,
			    "%s-%s-1", ENCRYPTION_MASTER_KEY_PRIFIX,
			    uuid);

		/* We call key ring API to generate master key here. */
		ret = my_key_generate(key_name, "AES",
				      NULL, ENCRYPTION_KEY_LEN);

		/* We call key ring API to get master key here. */
		ret = my_key_fetch(key_name, &key_type, NULL,
				   reinterpret_cast<void**>(master_key),
				   &key_len);

		if (!ret && *master_key != NULL) {
			Encryption::master_key_id++;
			*master_key_id = Encryption::master_key_id;
		}
#ifdef UNIV_ENCRYPT_DEBUG
		if (!ret && *master_key) {
			fprintf(stderr, "Generated new master key:");
			ut_print_buf(stderr, *master_key, key_len);
			fprintf(stderr, "\n");
		}
#endif
	} else {
		*master_key_id = Encryption::master_key_id;

		ut_snprintf(key_name, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN,
			    "%s-%s-%lu", ENCRYPTION_MASTER_KEY_PRIFIX,
			    uuid, *master_key_id);

		/* We call key ring API to get master key here. */
		ret = my_key_fetch(key_name, &key_type, NULL,
				   reinterpret_cast<void**>(master_key),
				   &key_len);

		/* For compitable with 5.7.11, we need to try to get master key with
		server id when get master key with server uuid failure. */
		if (ret || *master_key == NULL) {
			if (key_type) {
				my_free(key_type);
			}

			memset(key_name, 0,
			       ENCRYPTION_MASTER_KEY_NAME_MAX_LEN);
			ut_snprintf(key_name, ENCRYPTION_MASTER_KEY_NAME_MAX_LEN,
				    "%s-%lu-%lu", ENCRYPTION_MASTER_KEY_PRIFIX,
				    server_id, *master_key_id);

			ret = my_key_fetch(key_name, &key_type, NULL,
					   reinterpret_cast<void**>(master_key),
					   &key_len);
			*version = Encryption::ENCRYPTION_VERSION_1;
		}
#ifdef UNIV_ENCRYPT_DEBUG
		if (!ret && *master_key) {
			fprintf(stderr, "Fetched master key:%lu ",
				*master_key_id);
			ut_print_buf(stderr, *master_key, key_len);
			fprintf(stderr, "\n");
		}
#endif
	}

	if (ret) {
		*master_key = NULL;
		ib::error() << "Encryption can't find master key, please check"
				" the keyring plugin is loaded.";
	}

	if (key_type) {
		my_free(key_type);
	}
#endif
}

/** Check if page is encrypted page or not
@param[in]	page	page which need to check
@return true if it is a encrypted page */
bool
Encryption::is_encrypted_page(const byte* page)
{
	ulint	page_type = mach_read_from_2(page + FIL_PAGE_TYPE);

	return(page_type == FIL_PAGE_ENCRYPTED
	       || page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED
	       || page_type == FIL_PAGE_ENCRYPTED_RTREE);
}

/** Encrypt the page data contents. Page type can't be
FIL_PAGE_ENCRYPTED, FIL_PAGE_COMPRESSED_AND_ENCRYPTED,
FIL_PAGE_ENCRYPTED_RTREE.
@param[in]	type		IORequest
@param[in,out]	src		page data which need to encrypt
@param[in]	src_len		Size of the source in bytes
@param[in,out]	dst		destination area
@param[in,out]	dst_len		Size of the destination in bytes
@return buffer data, dst_len will have the length of the data */
byte*
Encryption::encrypt(
	const IORequest&	type,
	byte*			src,
	ulint			src_len,
	byte*			dst,
	ulint*			dst_len)
{
	ulint		len = 0;
	ulint		page_type = mach_read_from_2(src + FIL_PAGE_TYPE);
	ulint		data_len;
	ulint		main_len;
	ulint		remain_len;
	byte		remain_buf[MY_AES_BLOCK_SIZE * 2];

#ifdef UNIV_ENCRYPT_DEBUG
	ulint space_id =
		mach_read_from_4(src + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	ulint page_no = mach_read_from_4(src + FIL_PAGE_OFFSET);

	fprintf(stderr, "Encrypting page:%lu.%lu len:%lu\n",
		space_id, page_no, src_len);
#endif

	/* Shouldn't encrypte an already encrypted page. */
	ut_ad(page_type != FIL_PAGE_ENCRYPTED
	      && page_type != FIL_PAGE_COMPRESSED_AND_ENCRYPTED
	      && page_type != FIL_PAGE_ENCRYPTED_RTREE);

	ut_ad(m_type != Encryption::NONE);

	/* This is data size which need to encrypt. */
	data_len = src_len - FIL_PAGE_DATA;
	main_len = (data_len / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
	remain_len = data_len - main_len;

	/* Only encrypt the data + trailer, leave the header alone */

	switch (m_type) {
	case Encryption::NONE:
		ut_error;

	case Encryption::AES: {
		lint			elen;

		ut_ad(m_klen == ENCRYPTION_KEY_LEN);

		elen = my_aes_encrypt(
			src + FIL_PAGE_DATA,
			static_cast<uint32>(main_len),
			dst + FIL_PAGE_DATA,
			reinterpret_cast<unsigned char*>(m_key),
			static_cast<uint32>(m_klen),
			my_aes_256_cbc,
			reinterpret_cast<unsigned char*>(m_iv),
			false);

		if (elen == MY_AES_BAD_DATA) {
			ulint	page_no =mach_read_from_4(
				src + FIL_PAGE_OFFSET);
			ulint	space_id = mach_read_from_4(
				src + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
			*dst_len = src_len;
#ifndef UNIV_INNOCHECKSUM
				ib::warn()
					<< " Can't encrypt data of page,"
					<< " page no:" << page_no
					<< " space id:" << space_id;
#else
				fprintf(stderr, " Can't encrypt data of page,"
					" page no:" ULINTPF
					" space id:" ULINTPF,
					page_no, space_id);
#endif /* !UNIV_INNOCHECKSUM */
			return(src);
		}

		len = static_cast<ulint>(elen);
		ut_ad(len == main_len);

		/* Copy remain bytes and page tailer. */
		memcpy(dst + FIL_PAGE_DATA + len,
		       src + FIL_PAGE_DATA + len,
		       src_len - FIL_PAGE_DATA - len);

		/* Encrypt the remain bytes. */
		if (remain_len != 0) {
			remain_len = MY_AES_BLOCK_SIZE * 2;

			elen = my_aes_encrypt(
				dst + FIL_PAGE_DATA + data_len - remain_len,
				static_cast<uint32>(remain_len),
				remain_buf,
				reinterpret_cast<unsigned char*>(m_key),
				static_cast<uint32>(m_klen),
				my_aes_256_cbc,
				reinterpret_cast<unsigned char*>(m_iv),
				false);

			if (elen == MY_AES_BAD_DATA) {
				ulint	page_no =mach_read_from_4(
					src + FIL_PAGE_OFFSET);
				ulint	space_id = mach_read_from_4(
					src + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
#ifndef UNIV_INNOCHECKSUM
				ib::warn()
					<< " Can't encrypt data of page,"
					<< " page no:" << page_no
					<< " space id:" << space_id;
#else
				fprintf(stderr, " Can't encrypt data of page,"
					" page no:" ULINTPF
					" space id:" ULINTPF,
					page_no, space_id);
#endif /* !UNIV_INNOCHECKSUM */
				*dst_len = src_len;
				return(src);
			}

			memcpy(dst + FIL_PAGE_DATA + data_len - remain_len,
			       remain_buf, remain_len);
		}


		break;
	}

	default:
		ut_error;
	}

	/* Copy the header as is. */
	memmove(dst, src, FIL_PAGE_DATA);
	ut_ad(memcmp(src, dst, FIL_PAGE_DATA) == 0);

	/* Add encryption control information. Required for decrypting. */
	if (page_type == FIL_PAGE_COMPRESSED) {
		/* If the page is compressed, we don't need to save the
		original type, since it is done in compression already. */
		mach_write_to_2(dst + FIL_PAGE_TYPE,
				FIL_PAGE_COMPRESSED_AND_ENCRYPTED);
		ut_ad(memcmp(src+FIL_PAGE_TYPE+2,
			     dst+FIL_PAGE_TYPE+2,
			     FIL_PAGE_DATA-FIL_PAGE_TYPE-2) == 0);
	} else if (page_type == FIL_PAGE_RTREE) {
		/* If the page is R-tree page, we need to save original
		type. */
		mach_write_to_2(dst + FIL_PAGE_TYPE, FIL_PAGE_ENCRYPTED_RTREE);
	} else{
		mach_write_to_2(dst + FIL_PAGE_TYPE, FIL_PAGE_ENCRYPTED);
		mach_write_to_2(dst + FIL_PAGE_ORIGINAL_TYPE_V1, page_type);
	}

#ifdef UNIV_ENCRYPT_DEBUG
#ifndef UNIV_INNOCHECKSUM
#if 0
	byte*	check_buf = static_cast<byte*>(ut_malloc_nokey(src_len));
	byte*	buf2 = static_cast<byte*>(ut_malloc_nokey(src_len));

	memcpy(check_buf, dst, src_len);

	dberr_t err = decrypt(type, check_buf, src_len, buf2, src_len);
	if (err != DB_SUCCESS || memcmp(src + FIL_PAGE_DATA,
					check_buf + FIL_PAGE_DATA,
					src_len - FIL_PAGE_DATA) != 0) {
		ut_print_buf(stderr, src, src_len);
		ut_print_buf(stderr, check_buf, src_len);
		ut_ad(0);
	}
	ut_free(buf2);
	ut_free(check_buf);
#endif
	fprintf(stderr, "Encrypted page:%lu.%lu\n", space_id, page_no);
#endif
#endif
	*dst_len = src_len;


	return(dst);
}

/** Decrypt the page data contents. Page type must be FIL_PAGE_ENCRYPTED,
if not then the source contents are left unchanged and DB_SUCCESS is returned.
@param[in]	type		IORequest
@param[in,out]	src		Data read from disk, decrypted data will be
				copied to this page
@param[in]	src_len		source data length
@param[in,out]	dst		Scratch area to use for decryption
@param[in]	dst_len		Size of the scratch area in bytes
@return DB_SUCCESS or error code */
dberr_t
Encryption::decrypt(
	const IORequest&	type,
	byte*			src,
	ulint			src_len,
	byte*			dst,
	ulint			dst_len)
{
	ulint		data_len;
	ulint		main_len;
	ulint		remain_len;
	ulint		original_type;
	ulint		page_type;
	byte		remain_buf[MY_AES_BLOCK_SIZE * 2];
	Block*		block;

	/* Do nothing if it's not an encrypted table. */
	if (!is_encrypted_page(src)) {
		return(DB_SUCCESS);
	}

	/* For compressed page, we need to get the compressed size
	for decryption */
	page_type = mach_read_from_2(src + FIL_PAGE_TYPE);
	if (page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED) {
		src_len = static_cast<uint16_t>(
			mach_read_from_2(src + FIL_PAGE_COMPRESS_SIZE_V1))
			+ FIL_PAGE_DATA;
#ifndef UNIV_INNOCHECKSUM
		src_len = ut_calc_align(src_len, type.block_size());
#endif
	}
#ifdef UNIV_ENCRYPT_DEBUG
	ulint space_id =
		mach_read_from_4(src + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	ulint page_no = mach_read_from_4(src + FIL_PAGE_OFFSET);

	fprintf(stderr, "Decrypting page:%lu.%lu len:%lu\n",
		space_id, page_no, src_len);
#endif

	original_type = static_cast<uint16_t>(
		mach_read_from_2(src + FIL_PAGE_ORIGINAL_TYPE_V1));

	byte*	ptr = src + FIL_PAGE_DATA;

	/* The caller doesn't know what to expect */
	if (dst == NULL) {

		block = os_alloc_block();
#ifdef UNIV_INNOCHECKSUM
		dst = block;
#else
		dst = block->m_ptr;
#endif /* UNIV_INNOCHECKSUM */

	} else {
		block = NULL;
	}

	data_len = src_len - FIL_PAGE_DATA;
	main_len = (data_len / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
	remain_len = data_len - main_len;

	switch(m_type) {
	case Encryption::AES: {
		lint			elen;

		/* First decrypt the last 2 blocks data of data, since
		data is no block aligned. */
		if (remain_len != 0) {
			ut_ad(m_klen == ENCRYPTION_KEY_LEN);

			remain_len = MY_AES_BLOCK_SIZE * 2;

			/* Copy the last 2 blocks. */
			memcpy(remain_buf,
			       ptr + data_len - remain_len,
			       remain_len);

			elen = my_aes_decrypt(
				remain_buf,
				static_cast<uint32>(remain_len),
				dst + data_len - remain_len,
				reinterpret_cast<unsigned char*>(m_key),
				static_cast<uint32>(m_klen),
				my_aes_256_cbc,
				reinterpret_cast<unsigned char*>(m_iv),
				false);
			if (elen == MY_AES_BAD_DATA) {
				if (block != NULL) {
					os_free_block(block);
				}

				return(DB_IO_DECRYPT_FAIL);
			}

			/* Copy the other data bytes to temp area. */
			memcpy(dst, ptr, data_len - remain_len);
		} else {
			ut_ad(data_len == main_len);

			/* Copy the data bytes to temp area. */
			memcpy(dst, ptr, data_len);
		}

		/* Then decrypt the main data */
		elen = my_aes_decrypt(
				dst,
				static_cast<uint32>(main_len),
				ptr,
				reinterpret_cast<unsigned char*>(m_key),
				static_cast<uint32>(m_klen),
				my_aes_256_cbc,
				reinterpret_cast<unsigned char*>(m_iv),
				false);
		if (elen == MY_AES_BAD_DATA) {

			if (block != NULL) {
				os_free_block(block);
			}

			return(DB_IO_DECRYPT_FAIL);
		}

		ut_ad(static_cast<ulint>(elen) == main_len);

		/* Copy the remain bytes. */
		memcpy(ptr + main_len, dst + main_len, data_len - main_len);

		break;
	}

	default:
		if (!type.is_dblwr_recover()) {
#if !defined(UNIV_INNOCHECKSUM)
			ib::error()
				<< "Encryption algorithm support missing: "
				<< Encryption::to_string(m_type);
#else
			fprintf(stderr, "Encryption algorithm support missing: %s\n",
				Encryption::to_string(m_type));
#endif /* !UNIV_INNOCHECKSUM */
		}

		if (block != NULL) {
			os_free_block(block);
		}

		return(DB_UNSUPPORTED);
	}

	/* Restore the original page type. If it's a compressed and
	encrypted page, just reset it as compressed page type, since
	we will do uncompress later. */

	if (page_type == FIL_PAGE_ENCRYPTED) {
		mach_write_to_2(src + FIL_PAGE_TYPE, original_type);
		mach_write_to_2(src + FIL_PAGE_ORIGINAL_TYPE_V1, 0);
	} else if (page_type == FIL_PAGE_ENCRYPTED_RTREE) {
		mach_write_to_2(src + FIL_PAGE_TYPE, FIL_PAGE_RTREE);
	} else {
		ut_ad(page_type == FIL_PAGE_COMPRESSED_AND_ENCRYPTED);
		mach_write_to_2(src + FIL_PAGE_TYPE, FIL_PAGE_COMPRESSED);
	}

	if (block != NULL) {
		os_free_block(block);
	}

#ifdef UNIV_ENCRYPT_DEBUG
	fprintf(stderr, "Decrypted page:%lu.%lu\n", space_id, page_no);
#endif

	DBUG_EXECUTE_IF("ib_crash_during_decrypt_page", DBUG_SUICIDE(););

	return(DB_SUCCESS);
}

/** Normalizes a directory path for the current OS:
On Windows, we convert '/' to '\', else we convert '\' to '/'.
@param[in,out] str A null-terminated directory and file path */
void
os_normalize_path(
	char*	str)
{
	if (str != NULL) {
		for (; *str; str++) {
			if (*str == OS_PATH_SEPARATOR_ALT) {
				*str = OS_PATH_SEPARATOR;
			}
		}
	}
}
