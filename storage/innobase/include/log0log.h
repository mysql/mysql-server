/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All rights reserved.
Copyright (c) 2009, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
@file include/log0log.h
Database log

Created 12/9/1995 Heikki Tuuri
*******************************************************/

#ifndef log0log_h
#define log0log_h

#include "univ.i"

#ifndef UNIV_HOTBACKUP
# include "sync0sync.h"
# include "sync0rw.h"
#include "sync0mutex.h"
#include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */

#include "log0types.h"
#include "os0file.h"

/** Redo log implementation */
struct RedoLog {

	struct Command {
		Command();
		virtual ~Command();
		virtual void execute(RedoLog* redo_log) = 0;

	private:
		// Disable copying
		Command(const Command&);
		Command& operator=(const Command&);
	};

	// Forward declaration
	struct CommandQueue;

	/** Wait modes for write_up_to */
	enum wait_mode_t {
		WAIT_MODE_NO_WAIT = 91,
		WAIT_MODE_ONE_GROUP = 92
	};

	/** Redo states */
	enum state_t {
		/** Active and doing IO */
		STATE_RUNNING,

		/** IO done, needs to do the last checkpoint */
		STATE_CHECKPOINT,

		/** Last checkpoint done, needs to update the system LSN */
		STATE_FINISHED,

		/** Shutdown successful */
		STATE_SHUTDOWN
	};

	/** Constants */
	enum {
		/** Trailer size in bytes */
		TRAILER_SIZE = 4,

		/** Size of the log block header in bytes */
		BLOCK_HDR_SIZE = 12,

		/** Log 'spaces' have id's >= this */
		SPACE_FIRST_ID = 0xFFFFFFF0UL
	};

	// Forward declaration
	struct Scan;
	struct Group;
	struct LogBuffer;

	/** Constructor
	@param n_files		number of log files
	@param size		log file size in bytes
	@param mem_avail	Memory available in the buffer pool, in bytes */
	RedoLog(ulint n_files, os_offset_t size, ulint mem_avail);

	~RedoLog();

#ifndef UNIV_HOTBACKUP
	/**
	Acquires the log mutex. */
	void mutex_acquire() const
	{
		mutex_enter(&m_mutex);
	}

	/**
	Releases the log mutex. */
	void mutex_release() const
	{
		mutex_exit(&m_mutex);
	}

#ifdef UNIV_DEBUG
	/**
	@return true if the mutex is owned */
	bool is_mutex_owned() const
	{
		return(mutex_own(&m_mutex));
	}

#endif /* UNIV_DEBUG */
	/**
	Acquires the log mutex. */
	void flush_order_mutex_enter() const
	{
		mutex_enter(&m_flush_order_mutex);
	}

	/**
	Releases the log mutex. */
	void flush_order_mutex_exit() const
	{
		mutex_exit(&m_flush_order_mutex);
	}

#ifdef UNIV_DEBUG
	/**
	@return true if the flush order mutex is owned. */
	bool is_flush_order_mutex_owned() const
	{
		return(mutex_own(&m_flush_order_mutex));
	}
#endif /* UNIV_DEBUG */

	/**
	Checks if there is need for a log buffer flush or a new checkpoint,
	and does this if yes. Any database operation should call this when
	it has modified more than about 4 pages. NOTE that this function
	may only be called when the OS thread owns no synchronization objects
	except the dictionary mutex. */
	void free_check()
	{
#ifdef UNIV_SYNC_DEBUG
		{
			dict_sync_check	check(true);

			ut_ad(!sync_check_iterate(check));
		}
#endif /* UNIV_SYNC_DEBUG */

		if (m_check_flush_or_checkpoint) {

			check_margins();
		}
	}

	/**
	Submit a request to the command queue. */
	void submit(Command* command);

	/**
	Opens the log for log_write_low. The log must be closed with log_close
	and released with release().
	@param len		length of data to be catenated
	@param own_mutex	true if caller owns the mutex
	@return	start lsn of the log record */
	lsn_t open(ulint len, bool own_mutex);

	/**
	Writes to the log the string given. The log must be released with
	redo_log->release.
	@param str,		string
	@param len,		string length
	@param start_lsn	start lsn of the log recor
	@return	end lsn of the log record, zero if did not succeed */
	lsn_t open(const void* ptr, ulint len, lsn_t* start_lsn);

	/**
	Resize the log.
	@param n_files		The number of physical files
	@param size		Size in bytes
	@return true on success */
	bool resize(ulint n_files, os_offset_t size);

	/**
	Closes the log.
	@return	lsn */
	lsn_t close();

	/**
	Gets the current lsn.
	@return	current lsn */
	lsn_t get_lsn()
	{
		mutex_acquire();

		lsn_t	lsn = m_lsn;

		mutex_release();

		return(lsn);
	}

	/**
	Gets the log group capacity. It is OK to read the value without
	holding mutex because it is constant.
	@return	log group capacity */
	lsn_t get_capacity() const
	{
		return(m_log_group_capacity);
	}

	/**
	Get m_max_modified_age_async. It is OK to read the value without
	holding m_mutex because it is constant.
	@return	max_modified_age_async */
	lsn_t get_max_modified_age_async() const
	{
		return(m_max_modified_age_async);
	}

#ifdef UNIV_DEBUG
	bool is_write_allowed() const
	{
		return(m_write_allowed);
	}

	void enable_log_write()
	{
		m_write_allowed = true;
	}

	void disable_log_write()
	{
		m_write_allowed = false;
	}

	lsn_t checkpoint_lsn() const;
#endif /* UNIV_DEBUG */

	/**
	@return true if writes to the change buffer are allowed */
	bool is_ibuf_allowed() const
	{
		return(m_ibuf_allowed);
	}

	/**
	@return the free frames in the buffer pool */
	ulint get_free_frames() const
	{
		return(m_n_free_frames);
	}

	/**
	Returns true if recovery is currently running.
	@return	recv_recovery_on */
	bool is_recovery_on() const
	{
		return(m_recover != NULL);
	}

	/**
	@return true if there are pending reads or writes. */
	bool is_busy() const;

	/**
	Print busy status. */
	void print_busy_status();

	/**
	Initializes the log.
	@return true if success, false if not */
	bool init();

	/**
	This function is called, e.g., when a transaction wants to commit. It
	checks that the log has been written to the log file up to the last
	log entry written by the transaction. If there is a flush running,
	it waits and checks if the flush flushed enough. If not, starts a
	new flush.
	@param lsn		log sequence number up to which the log should
				be written, LSN_MAX if not specified
	@param mode		wait mode
	@param flush_to_disk	true if we want the written log also to be
				flushed to disk */
	void write_up_to(lsn_t lsn, wait_mode_t wait, bool flush_to_disk);

	/**
	Makes a checkpoint. Note that this function does not flush dirty
	blocks from the buffer pool: it only checks what is lsn of the oldest
	modification in the pool, and writes information about the lsn in
	log files. Use log_checkpoint_at to flush also the pool.
	@param sync		true if synchronous operation is desired
	@param write_always	the function normally checks if the
				the new checkpoint would have a greater
				lsn than the previous one: if not, then no
				physical write is done; by setting this
				parameter TRUE, a physical write will always be
				made to log files
	@return	true if success, false if a checkpoint write was already
		running */
	bool checkpoint(bool sync, bool write_always);

	/***
	Writes to the log the string given. It is assumed that the caller
	holds the log mutex.
	@param ptr		buffer to write
	@param len 		length of buffer to write */
	void write(const byte* ptr, ulint ptr_len);

	/**
	This functions writes the log buffer to the log file and if 'flush'
	is set it forces a flush of the log file as well. This is meant to be
	called from background master thread only as it does not wait for
	the write (+ possible flush) to finish.
	@param flush		flush the logs to disk */
	void async_flush(bool flush);

	/**
	Does a syncronous flush of the log buffer to disk. */
	void sync_flush();

	/**
	Makes a checkpoint at a given lsn or later.
	@param lsn,		make a checkpoint at this or a
				later lsn, if LSN_MAX, makes
				a checkpoint at the latest lsn
	@param write_always	the function normally checks if
				the new checkpoint would have a
				greater lsn than the previous one: if
				not, then no physical write is done;
				by setting this parameter TRUE, a
				physical write will always be made to
				log files */
	void checkpoint_at(lsn_t lsn, bool write_always);

	/**
	Makes a checkpoint at the latest lsn and writes it to first page of each
	data file in the database, so that we know that the file spaces contain
	all modifications up to that lsn.
	@param notify		if true then write to the error log
	@return current state */
	state_t start_shutdown(bool notify);

#else /* !UNIV_HOTBACKUP */
	/**
	Writes info to a buffer of a log group when log files are created in
	backup restoration.
	@param ptr		buffer which will be written to the
				start of the first log file
	@param start		lsn of the start of the first log file;
				we pretend that there is a checkpoint at
				start + LOG_BLOCK_HDR_SIZE */
	void reset_first_header_and_checkpoint(byte* ptr, ib_uint64_t start);
#endif /* !UNIV_HOTBACKUP */

	/**
	Checks that there is enough free space in the log to start a
	new query step. Flushes the log buffer or makes a new checkpoint
	if necessary. NOTE: this function may only be called if the calling
	thread owns no synchronization objects! */
	void check_margins();

	/**
	Tries to establish a big enough margin of free space in
	the log buffer, such that a new log entry can be catenated
	without an immediate need for a flush. */
	void flush_margin();

	/**
	Prints info of the log.
	@param file	stream where to print */
	void print(FILE* file);

	/**
	Peeks the current lsn.
	@param lsn	if function returns true the the returned value is valid
	@return	true if success, false if could not get the log system mutex */
	bool peek_lsn(lsn_t* lsn);

	/**
	Refreshes the statistics used to print per-second averages. */
	void refresh_stats();

	/**
	Shutdown the log system but do not release all the memory. */
	void shutdown();

	/**
	Completes an i/o to a log file.
	@param group		log group */
	void io_complete(Group* group);

	/**
	Returns the oldest modified block lsn in the pool, or m_lsn if
	none exists.
	@return	LSN of oldest modification */
	lsn_t buf_pool_get_oldest_modification();

	/**
	Free the log system data structures. */
	void release_resources();

	/**
	Recovers from a checkpoint. When this function returns, the database
	is able to start processing of new user transactions, but the
	function recovery_from_checkpoint_finish should be called later to
	complete the recovery and free the resources used in it.
	@return	error code or DB_SUCCESS */
	dberr_t recovery_start(
		lsn_t		min_flushed_lsn,
		lsn_t		max_flushed_lsn,
		redo_recover_t* recover);

	/**
	Resets the logs. The contents of log files will be lost!
	@param size		Size of the log file, 0 if it shouldn't be
				changed
	@param lsn 		reset to this lsn rounded up to be divisible by
				OS_FILE_LOG_BLOCK_SIZE, after which we add
				LOG_BLOCK_HDR_SIZE */
	void reset_logs(os_offset_t size, lsn_t lsn);

#ifndef UNIV_HOTBACK
	/**
	Completes recovery from a checkpoint. */
	void recovery_finish();
#else
	/**
	Scans the log segment and n_bytes_scanned is set to the length of valid
	log scanned.
	@param buf,		buffer containing log data
	@param buf_len		data length in that buffer
	@param scanned_lsn	lsn of buffer start, we return scanned lsn
	@param scanned_checkpoint_no 4 lowest bytes of the highest scanned
				checkpoint number so far
	@param n_bytes_scanned	how much we were able to scan, smaller than
				buf_len if log data ended here */
	void scan_log_seg_for_backup(
		byte*		buf,
		ulint		buf_len,
		lsn_t*		scanned_lsn,
		ulint*		scanned_checkpoint_no,
		ulint*		n_bytes_scanned);
#endif /* UNIV_HOTBACKUP */

	/**
	Gets a log block data length.
	@param block		log block
	@return	log block data length measured as a byte offset from the
		block start */
	static ulint block_get_data_len(const byte* block);

	void handle_truncate();

private:
	/**
	Wait for IO to complete.
	@param mode		Wait mode */
	void wait_for_io(wait_mode_t mode);

	/**
	Inits a log group to the log system.
	@return true if success, false if not */
	bool group_init()
		__attribute__((warn_unused_result));

	/**
	Tries to establish a big enough margin of free space in the log
	groups, such that a new log entry can be catenated without an
	immediate need for a checkpoint. NOTE: this function may only
	be called if the calling thread owns no synchronization objects! */
	void checkpoint_margin();

	/**
	Advances the smallest lsn for which there are unflushed dirty blocks
	in the buffer pool. NOTE: this function may only be called if the
	calling thread owns no synchronization objects!

	@param new_oldest	try to advance oldest_modified_lsn at least
				to this lsn

	@return false if there was a flush batch of the same type running,
		which means that we could not start this flush batch */
	bool preflush_pool_modified_pages(lsn_t new_oldest);

	/**
	Calculates where in log files we find a specified lsn.

	@param log_file_offset	offset in that file (including the header)
	@param first_header_lsn	first log file start lsn
	@param lsn		lsn whose position to determine
	@param n_log_files	total number of log files
	@param log_file_size	log file size (including the header)

	@return	log file number */
	static ulint seek_to_lsn(
		os_offset_t*	log_file_offset,
		lsn_t		first_header_lsn,
		lsn_t		lsn,
		ulint		n_log_files,
		os_offset_t	log_file_size);

	/**
	Completes an asynchronous checkpoint info write i/o to a log file. */
	void checkpoint_complete();

	/**
	Calculates the recommended highest values for lsn - last_checkpoint_lsn,
	lsn - buf_get_oldest_modification(), and lsn - max_archive_lsn_age.

	@retval true on success
	@retval false if the smallest log group is too small to accommodate
			the number of OS threads in the database server */
	bool calc_max_ages();

	/**
	Checks if a flush is completed and does the completion routine if yes.
	@return	LOG_UNLOCK_FLUSH_LOCK or 0 */
	ulint check_flush_completion();

	/**
	Does the unlockings needed in flush i/o completion.
	@param code		any ORed combination of LOG_UNLOCK_FLUSH_LOCK
				and LOG_UNLOCK_NONE_FLUSHED_LOCK */
	void flush_do_unlocks(ulint code);

	/**
	Calculates the data capacity of a log group, when the log file
	headers are not included.
	@return	capacity in bytes */
	lsn_t group_get_capacity() const;

	/**
	Reads a checkpoint info from a log group header to m_checkpoint_buf.
	@param field		LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2
	@param field		LOG_CHECKPOINT_1 or LOG_CHECKPOINT_2
	@param space_id		Space id to read from */
	void checkpoint_read(ulint field, ulint space_id);

#ifndef UNIV_HOTBACKUP

	/**
	Writes a buffer to a log file group.
	@param ptr		buffer
	@param len		buffer len; must be divisible
				by OS_FILE_LOG_BLOCK_SIZE
	@param start_lsn	start lsn of the buffer; must be divisible by
				OS_FILE_LOG_BLOCK_SIZE
	@param offset		start offset of new data in buf: this
				parameter is used to decide if we have to
				write a new log file header */
	void group_write(
		byte*		ptr,
		ulint		len,	
		lsn_t		start_lsn,
		ulint		new_data_offset);

	/**
	Sets the field values in group to correspond to a given lsn. For
	this function to work, the values must already be correctly
	initialized to correspond to some lsn, for instance, a checkpoint
	lsn.
	@param lsn		lsn for which the values should be set */
	void group_set_fields(lsn_t lsn);

	/**
	Writes a log file header to a log file space.
	@param nth_file		header to the nth file in the log file space
	@param start_lsn	log file data starts at this lsn */
	void group_file_header_flush(
		ulint		nth_file,
		lsn_t		start_lsn);

	/**
	Initializes a log block in the log buffer in the old, < 3.23.52
	format, where there was no checksum yet.
	@param log_block	pointer to the log buffer
	@param lsn		lsn within the log block */
	static void block_init_v1(byte* block, lsn_t lsn);

	/**
	Converts a lsn to a log block number.
	@param lsn		lsn of a byte within the block
	@return	log block number, it is > 0 and <= 1G */
	static ulint convert_lsn_to_no(lsn_t lsn);

	/**
	Checks the 4-byte checksum to the trailer checksum field of a log
	block.  We also accept a log block in the old format before
	InnoDB-3.23.52 where the checksum field contains the log block number.
	@param block		pointer to a log block
	@return true if ok, or if the log block may be in the format of InnoDB
	version predating 3.23.52 */
	static bool log_block_checksum_is_ok_or_old_format(const byte* block);

	/**
	Calculates the offset of an lsn within a log group.
	@param lsn		lsn
	@return	offset within the log group */
	lsn_t group_calc_lsn_offset(lsn_t lsn);

	/**
	Calculates the offset within a log group, when the log file headers
	are not included.
	@param offset		real offset within the log group
	@return	size offset (<= offset) */
	lsn_t group_calc_size_offset(lsn_t offset);

	/**
	Calculates the offset within a log group, when the log file headers are
	included.
	@param offset           size offset within the log group
	@return real offset (>= offset) */
	lsn_t group_calc_real_offset(lsn_t offse);

	/**
	Writes the checkpoint info to a log group header. */
	void group_checkpoint();

	/**
	Closes a log group.
	@param group	log group to close */
	void group_close();

	/**
	Completes a checkpoint. */
	void complete_checkpoint();

	/**
	Copies a log segment from the most up-to-date log group to the
	other log groups, so that they all contain the latest log data.
	Also writes the info about the latest checkpoint to the groups,
	and inits the fields in the group memory structs to up-to-date
	values. */
	void synchronize_groups(lsn_t recovered_lsn);

	/**
	Checks if a flush is completed for a log group and does the completion
	routine if yes.
	@return	LOG_UNLOCK_NONE_FLUSHED_LOCK or 0 */
	ulint group_check_flush_completion();

	/**
	Scans log from a buffer and stores new log data to the parsing
	buffer. Parses and hashes the log records if new data found.
	@param contiguous_lsn	it is known that all log groups contain
				contiguous log data up to this lsn
	@param recover		The recovery manager */
	void group_scan_log_recs(
		lsn_t*		contiguous_lsn,
		redo_recover_t*	recover);	
	/**
	Scans log from a buffer and stores new log data to the parsing buffer.
	Parses and hashes the log records if new data found.  Unless
	UNIV_HOTBACKUP is defined, this function will apply log records
	automatically when the hash table becomes full.

	@param available_memory	we let the hash table of recs to grow to
				this size, at the maximum
	@param store_to_hash	true if the records should be stored to the
				hash table; this is set to false if just
				debug checking is needed
	@param lsn		buffer start lsn
	@param contiguous_lsn	it is known that all log groups contain
				contiguous log data up to this lsn
	@param	recover		recovery manager to use

	@return true if limit_lsn has been reached, or not able to scan any
	more in this log group */
	bool scan_log_recs(
		ulint		available_memory,
		bool		store_to_hash,
		lsn_t		start_lsn,
		lsn_t*		contiguous_lsn,
		redo_recover_t*	recover);

	/** Read the first log file header to print a note if this is
	a recovery from a restored InnoDB Hot Backup. Rest if it is.
	@return DB_SUCCESS or error code. */
	dberr_t check_ibbackup();

	/**
	Prepare for recovery, check the the flushed LSN.
	@return DB_SUCCESS or error code */
	dberr_t prepare_for_recovery(
		lsn_t		min_flushed_lsn,
		lsn_t		max_flushed_lsn);

	/**
	Check a block and verify that it is a valid block. Update scan state.
	@return DB_SUCCESS or error code */
	dberr_t check_block(Scan& scan, const byte* block);

	/** Extends the log buffer.
	@param[in] len	requested minimum size in bytes */
	void extend(ulint len);
#endif /* !UNIV_HOTBACKUP */

private:
	/** Log group, currently only one group is used */
	Group*			m_group;

	/** true when applying redo log records during crash recovery; FALSE
	otherwise.  Note that this is FALSE while a background thread is
	rolling back incomplete transactions. */
	redo_recover_t*		m_recover;

	/** End lsn for the current running write */
	lsn_t			m_write_lsn;

	/** Number of log i/os initiated thus far */
	ulint			m_n_log_ios;

	/** Number of log i/o's at the previous printout */
	ulint			m_n_log_ios_old;

	/** Size of a file in bytes, in the group */
	os_offset_t		m_file_size;

	/** For printing percentages */
	ulint			m_print_counter;

	/** Number of files in a group */
	ulint			m_n_files;

	/** number of currently pending flushes or writes */
	ulint			m_n_pending_writes;

	/** End lsn for the current running write + flush operation */
	lsn_t			m_current_flush_lsn;

	/** System states */
	state_t			m_state;

 	/** How far we have written the log AND flushed to disk */
	lsn_t			m_flushed_to_disk_lsn;

#ifndef UNIV_HOTBACKUP

	struct Checkpoint;

	/** Mutex protecting the log */
	mutable	ib_mutex_t	m_mutex;

	/** Mutex to serialize access to the flush list when we are putting
	dirty blocks in the list. The idea behind this mutex is to be able to
	release log_t::mutex during mtr_commit and still ensure that insertions
	in the flush_list happen in the LSN order. */
	mutable	ib_mutex_t	m_flush_order_mutex;

#endif /* !UNIV_HOTBACKUP */

	/** Log sequence number */
	lsn_t			m_lsn;

	/** log buffer */
	LogBuffer*		m_buf;

	/** This is set to true when there may be need to flush the log
	buffer, or preflush buffer pool pages, or make a checkpoint; this
	MUST be true when lsn - last_checkpoint_lsn > max_checkpoint_age;
	this flag is peeked at by redo_log->free_check(), which does
	not reserve the log mutex */
	bool			m_check_flush_or_checkpoint;

#ifndef UNIV_HOTBACKUP

	/** The fields involved in the log buffer flush @{ */

	/** First log sequence number not yet written to any log group; for
	this to be advanced, it is enough that the write i/o has been completed
	for any one log group */
	lsn_t			m_written_to_some_lsn;

	/** First log sequence number not yet written to some log group; for
	this to be advanced, it is enough that the write i/o has been completed
	for all log groups.  Note that since InnoDB currently has only one log
	group therefore this value is redundant. Also it is possible that this
	value falls behind the flushed_to_disk_lsn transiently.  It is
	appropriate to use either flushed_to_disk_lsn or write_lsn which are
	always up-to-date and accurate. */
	lsn_t			m_written_to_all_lsn;

	/* NOTE on the 'flush' in names of the fields below: starting from
	4.0.14, we separate the write of the log file and the actual fsync()
	or other method to flush it to disk. The names below shhould really
	be 'flush_or_write'! */

	/** this event is in the reset state when a flush or a write is
	running; a thread should wait for this without owning the log mutex,
	but NOTE that to set or reset this event, the thread MUST own the log
	mutex! */
	os_event_t		m_no_flush_event;

	/** During a flush, this is first false and becomes true when one log
	group has been written or flushed */
	bool			m_one_flushed;

	/** This event is reset when the flush or write has not yet completed
	for any log group; e.g., this means that a transaction has been
	committed when this is set; a thread should wait for this without
	owning the log mutex, but NOTE that to set or reset this event, the
	thread MUST own the log mutex! */
	os_event_t		m_one_flushed_event;

	/** When log_print was last time called */
	time_t			m_last_printout_time;

	/** Capacity of the log group; if the checkpoint age exceeds this,
	it is a serious error because it is possible we will then overwrite
	log and spoil crash recovery */
	lsn_t			m_log_group_capacity;

 	/** When this recommended value for lsn -
	buf_pool_get_oldest_modification() is exceeded, we start an
	asynchronous preflush of pool pages */
	lsn_t			m_max_modified_age_async;

	/** When this recommended value for lsn -
	buf_pool_get_oldest_modification() is exceeded, we start a
	synchronous preflush of pool pages */
	lsn_t			m_max_modified_age_sync;

	/** Set to true when we extend the log buffer size */
	volatile bool		m_is_extending;

	Checkpoint*		m_checkpoint;
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
	/** false if writing to the redo log (mtr_commit) is forbidden.
	Protected by log_sys->mutex. */
	bool			m_write_allowed;
#endif /* UNIV_DEBUG */

	/** If the following is true, the buffer pool file pages must
	be invalidated after recovery and no ibuf operations are allowed;
	this becomes true if the log record hash table becomes too full,
	and log records must be merged to file pages already before the
	recovery is finished: in this case no ibuf operations are allowed,
	as they could modify the pages read in the buffer pool before the
	pages have been recovered to the up-to-date state.

	true means that recovery is running and no operations on the log files
	are allowed yet: the variable name is misleading. */
	bool			m_ibuf_allowed;

	/** This many frames must be left free in the buffer pool when we scan
	the log and store the scanned log records in the buffer pool: we will
	use these free frames to read in pages when we start applying the
	log records to the database.
	This is the default value. If the actual size of the buffer pool is
	larger than 10 MB we'll set this value to 512. */
	ulint			m_n_free_frames;

	/**
	Command queue for redo requests. */
	CommandQueue*		m_cmdq;
};

extern RedoLog*		redo_log;

/** Test if flush order mutex is owned. */
#define log_flush_order_mutex_own()					\
	redo_log->m_flush_order_mutex.is_owned()

/** Acquire the flush order mutex. */
#define log_flush_order_mutex_enter()					\
	do {								\
		redo_log->m_flush_order_mutex.enter(			\
			1, srv_spin_wait_delay / 2, __FILE__, __LINE__);\
	} while (0)

/** Release the flush order mutex. */
#define log_flush_order_mutex_exit()	redo_log->m_flush_order_mutex.exit();

/**
Redo log writer thread.
@param	arg		a dummy parameter required by os_thread_create
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(log_writer_thread)(void* arg __attribute__((unused)));

#endif /* log0log_h */
