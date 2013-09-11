/*****************************************************************************

Copyright (c) 1995, 2013, Oracle and/or its affiliates. All Rights Reserved.
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
@file log/log0log.cc
Database log

Created 12/9/1995 Heikki Tuuri
*******************************************************/

#include "log0log.h"

#ifndef UNIV_HOTBACKUP
#include "mem0mem.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "srv0srv.h"
#include "log0recv.h"
#include "fil0fil.h"
#include "dict0boot.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "srv0mon.h"
#include "sync0sync.h"

#include <queue>
#include <vector>

/*
General philosophy of InnoDB redo-logs:

1) Every change to a contents of a data page must be done
through mtr, which in mtr_commit() writes log records
to the InnoDB redo log.

2) Normally these changes are performed using a mlog_write_ulint()
or similar function.

3) In some page level operations only a code number of a
c-function and its parameters are written to the log to
reduce the size of the log.

  3a) You should not add parameters to these kind of functions
  (e.g. trx_undo_header_create(), trx_undo_insert_header_reuse())

  3b) You should not add such functionality which either change
  working when compared with the old or are dependent on data
  outside of the page. These kind of functions should implement
  self-contained page transformation and it should be unchanged
  if you don't have very essential reasons to change log
  semantics or format.

*/

#ifdef UNIV_PFS_THREAD
mysql_pfs_key_t        log_writer_thread_key;
#endif /* UNIV_PFS_THREAD */

/** Size of block reads when the log groups are scanned forward to do a
roll-forward */
#define	SCAN_SIZE		(4 * UNIV_PAGE_SIZE)

/* These control how often we print warnings if the last checkpoint is too
old */
bool	log_has_printed_chkp_warning = false;
time_t	log_last_warning_time;

/** FIXME: Testing only */
RedoLog*		redo_log;

/** Values used as flags */
static const ulint RECOVER = 98887331;
static const ulint CHECKPOINT = 78656949;

/* Margins for free space in the log buffer after a log entry is catenated */
static const ulint BUF_FLUSH_RATIO = 2;

#define BUF_FLUSH_MARGIN		(BUF_WRITE_MARGIN + 4 * UNIV_PAGE_SIZE)

/* Margin for the free space in the smallest log group, before a new query
step which modifies the database, is started */

#define CHECKPOINT_FREE_PER_THREAD	(4 * UNIV_PAGE_SIZE)

#define CHECKPOINT_EXTRA_FREE		(8 * UNIV_PAGE_SIZE)

/* This parameter controls asynchronous making of a new checkpoint; the value
should be bigger than POOL_PREFLUSH_RATIO_SYNC */

static const ulint POOL_CHECKPOINT_RATIO_ASYNC = 32;

/* This parameter controls synchronous preflushing of modified buffer pages */
static const ulint POOL_PREFLUSH_RATIO_SYNC = 16;

/* The same ratio for asynchronous preflushing; this value should be less than
the previous */
static const ulint POOL_PREFLUSH_RATIO_ASYNC = 8;

/** Codes used in unlocking flush latches */
static const ulint UNLOCK_FLUSH_LOCK = 2;

static const ulint UNLOCK_NONE_FLUSHED_LOCK = 1;

/** A 32-byte field which contains the string 'ibbackup' and the creation
time if the log file was created by ibbackup --restore; when mysqld is first
time started on the restored database, it can print helpful info for the user */
static const ulint FILE_WAS_CREATED_BY_HOT_BACKUP = 16;

/** Offsets of a log file header */
/** log group number */
static const ulint LOG_GROUP_ID = 0;

/** lsn of the start of data in this log file */
static const ulint LOG_FILE_START_LSN = 4;

/** Total size of the log buffer */
#define LOG_BUFFER_SIZE		(srv_log_buffer_size * UNIV_PAGE_SIZE)

/** Block used for all IO */
template <int Size = OS_FILE_LOG_BLOCK_SIZE>
struct Block {

	enum { SIZE = Size};

	/**
	Do a shallow copy.

	@aram ptr	Start of the block. */
	Block(byte* ptr) : m_ptr(ptr) { }

	static ulint capacity()
	{
		return(SIZE - RedoLog::TRAILER_SIZE);
	}

	/**
	Initializes a log block in the log buffer in the old, < 3.23.52
	format, where there was no checksum yet.
	@param ptr		pointer to the log buffer
	@param block_no		block number */
	static void init_v1(byte* ptr, ulint block_no)
	{
		ulint	header_size = RedoLog::BLOCK_HDR_SIZE;

		ut_ad(block_no > 0);
		ut_ad(block_no < FLUSH_BIT_MASK);

		mach_write_to_4(ptr + HDR_NO, block_no);

		mach_write_to_4(ptr + SIZE - CHECKSUM, block_no);

		mach_write_to_2(ptr + FIRST_REC_GROUP, 0);

		mach_write_to_2(ptr + HDR_DATA_LEN, header_size);
	}

	/**
	Gets a log block data length.
	@param block		log block
	@return	log block data length measured as a byte offset
	from the block start */
	static ulint get_data_len(const byte* ptr)
	{
		return(mach_read_from_2(ptr + HDR_DATA_LEN));
	}

	/**
	Sets the log block data length.
	@param len		data length */
	static void set_data_len(byte* ptr, ulint len)
	{
		mach_write_to_2(ptr + HDR_DATA_LEN, len);
	}

	/**
	Gets a log block number stored in the header.
	@param block		log block
	@return	log block number stored in the block header */
	static ulint get_hdr_no(const byte* ptr)
	{
		return(~FLUSH_BIT_MASK & mach_read_from_4(ptr + HDR_NO));
	}

	/**
	Sets the log block number stored in the header; NOTE that this must
	be set before the flush bit!
	@param block_number	log block number: must be > 0 and
				< FLUSH_BIT_MASK */
	static void set_hdr_no(byte* ptr, ulint block_number)
	{
		ut_ad(block_number > 0);
		//ut_ad(block_number < FLUSH_BIT_MASK);

		mach_write_to_4(ptr + HDR_NO, block_number);
	}

	/**
	Sets the log block flush bit. */
	static void set_flush_bit(byte* ptr)
	{
		ulint	hdr_no = get_hdr_no(ptr);

		ut_ad(hdr_no > 0);
		hdr_no |= FLUSH_BIT_MASK;

		set_hdr_no(ptr, hdr_no);
	}

	/**
	Gets a log block flush bit.
	@param block	log block
	@return	true if this block was the first to be written in a log flush */
	static bool is_flush_bit_set(const byte* ptr)
	{
		return(FLUSH_BIT_MASK & mach_read_from_4(ptr + HDR_NO));
	}

	/**
	Calculates the checksum for a log block.
	@param block		log block
	@return	checksum */
	static ulint calc_checksum(const byte* ptr)
	{
		ulint	sh = 0;
		ulint	sum = 1;

		for (ulint i = 0; i < capacity(); ++i, ++ptr) {

			ulint	b = (ulint) *ptr;

			sum &= 0x7FFFFFFFUL;
			sum += b;
			sum += b << sh;
			++sh;

			if (sh > 24) {
				sh = 0;
			}
		}

		return(sum);
	}

	/**
	Gets a log block first mtr log record group offset.
	@param block	log block
	@return first mtr log record group byte offset from the block
		start, 0 if none */
	static ulint get_first_rec_group(const byte* ptr)
	{
		return(mach_read_from_2(ptr + FIRST_REC_GROUP));
	}

	/**
	Sets the log block first mtr log record group offset.
	@param offset		offset, 0 if none */
	static void set_first_rec_group(byte* ptr, ulint offset)
	{
		mach_write_to_2(ptr + FIRST_REC_GROUP, offset);
	}

	/**
	Gets a log block checkpoint number field (4 lowest bytes).
	@param log_block	log block
	@return	checkpoint no (4 lowest bytes) */
	static ulint get_checkpoint_no(const byte* ptr)
	{
		return(mach_read_from_4(ptr + CHECKPOINT_NO));
	}

	/**
	Sets a log block checkpoint number field (4 lowest bytes).
	@param no		checkpoint no */
	static void set_checkpoint_no(byte* ptr, ib_uint64_t no)
	{
		mach_write_to_4(ptr + CHECKPOINT_NO, ulint(no));
	}

	/**
	Gets a log block checksum field value.
	@param block		log block
	@return	checksum */
	static ulint get_checksum(const byte* ptr)
	{
		return(mach_read_from_4(ptr + SIZE - CHECKSUM));
	}

	/**
	Stores a 4-byte checksum to the trailer checksum field of a log block
	before writing it to a log file. This checksum is used in recovery to
	check the consistency of a log block.
	@param block		pointer to log block */
	static void store_checksum(byte* ptr)
	{
		ulint	checksum = calc_checksum(ptr);
		mach_write_to_4(ptr + SIZE - CHECKSUM, checksum);
	}

	/**
	Checks the 4-byte checksum to the trailer checksum field of a log
	block.  We also accept a log block in the old format before
	InnoDB-3.23.52 where the checksum field contains the log block number.
	@return true if ok, or if the log block may be in the format of InnoDB
	version predating 3.23.52 */
	static bool checksum_is_ok_or_old_format(const byte* ptr)
	{
		if (calc_checksum(ptr) == get_checksum(ptr)) {

			return(true);
		} else if (get_hdr_no(ptr) == get_checksum(ptr)) {

			/* We assume the log block is in the format of
			InnoDB version < 3.23.52 and the block is ok */
			return(true);
		}

		return(false);
	}

public:
	/** Mask used to get the highest bit in the header number. */
	static const ulint FLUSH_BIT_MASK =  0x80000000UL;

	/** Number of bytes written to this block */
	static const ulint HDR_DATA_LEN = 4;

	/** Offset of the first start of an mtr log record group in
	this log block, 0 if none; if the value is the same as
	HDR_DATA_LEN, it means that the first rec group has
	not yet been catenated to this log block, but if it will, it
	will start at this offset; an archive recovery can start parsing the
	log records starting from this offset in this log block, if value
	not 0 */
	static const ulint FIRST_REC_GROUP = 6;

	/** 4 lower bytes of the value of m_next_checkpoint_no when the log
	block was last written to: if the block has not yet been written full,
	this value is only updated before a log buffer flush */
	static const ulint CHECKPOINT_NO = 8;

	/** Block number which must be > 0 and is allowed to wrap around
	at 2G; the highest bit is set to 1 if this is the first log block
	in a log flush write segment */
	static const ulint HDR_NO = 0;

	/* Offsets of a log block trailer from the end of the block */

	/** 4 byte checksum of the log block contents; in InnoDB
	versions < 3.23.52 this did not contain the checksum but
	the same value as .._HDR_NO */
	static const ulint CHECKSUM = 4;

	/** Pointer to a block start in the log buffer */
	byte*		m_ptr;
};

typedef Block<OS_FILE_LOG_BLOCK_SIZE> IOBlock;

/* A margin for free space in the log buffer before a log entry is catenated */
static const ulint BUF_WRITE_MARGIN  = 4 * IOBlock::SIZE;

/* The counting of lsn's starts from this value: this must be non-zero */
static const lsn_t START_LSN = 16 * IOBlock::SIZE;

static const ulint LOG_FILE_HDR_SIZE = 4 * IOBlock::SIZE;

/** The buffer is made up of IOBlocks, each block is a fixed size. The memory
is allocated in one big chunk. */
class IOBuffer {
public:

	/**
	Allocate memory for a buffer and align the start of the
	buffer on IOBlock::Size.

	@param size	of the buffer in bytes */
	IOBuffer(size_t size)
		:
		m_size(size)
	{
		size += IOBlock::SIZE;

		m_unaligned_ptr = new(std::nothrow) byte[size];

		::memset(m_unaligned_ptr, 0x0, size);

		void*	ptr = ut_align(m_unaligned_ptr, IOBlock::SIZE);
		m_ptr = reinterpret_cast<byte*>(ptr);

		m_iobuffers.reserve(size / IOBlock::SIZE);

		byte*	p = m_ptr;

		for (ulint i = 0; i < m_iobuffers.capacity(); ++i) {
			m_iobuffers.push_back(new(std::nothrow) IOBlock(p));
			p += IOBlock::SIZE;
		}
	}

	/**
	Destructor */
	~IOBuffer()
	{
		iobuffers_t::iterator	end = m_iobuffers.end();

		for (iobuffers_t::iterator it = m_iobuffers.begin();
		     it != end;
		     ++it) {

			delete *it;
		}

		delete[] m_unaligned_ptr;
	}

	/*
	@return the aligned pointer */
	byte* ptr()
	{
		return(m_ptr);
	}

	const byte* ptr() const
	{
		return(m_ptr);
	}

	/**
	@return the capacity of the buffer */
	size_t capacity() const
	{
		return(m_size);
	}
private:
	// Disable copying
	IOBuffer(const IOBuffer&);
	IOBuffer& operator=(const IOBuffer&);

private:
	typedef std::vector<IOBlock*> iobuffers_t;

	/** Aligned version of m_unaligned_ptr */
	byte*		m_ptr;

	/** Requested size in bytes */
	size_t		m_size;

	/** Unaligned pointer */
	byte*		m_unaligned_ptr;

	/** Pointer to individual buffers */
	iobuffers_t	m_iobuffers;
};

/** The logical buffer that is used to control the physical buffer. */
struct RedoLog::LogBuffer {
	/**
	@param size		Size of the buffer */
	LogBuffer(ulint size)
		:
		m_next_to_write(),
		m_write_end_offset(),
		m_buffer(size)
	{
		ulint	ratio = LOG_BUFFER_SIZE / BUF_FLUSH_RATIO;

		m_max_free = ratio - BUF_FLUSH_MARGIN;

		reset();
	}

	/**
	Initiliase the free space, used during recovery.
	@param start		The start of the free space */
	void init(ulint start)
	{
		m_free = m_next_to_write = start;
	}

	/**
	@return true if will fit in one block */
	bool will_fit_in_one_block(ulint len) const
	{
		return(start_offset() + len < block_capacity());
	}

	/**
	@return true if will fit in the log (across multiple blocks) */
	bool will_fit_in_log(ulint len) const
	{
		return(m_free + len < capacity());
	}

	/**
	Copy the string to the log buffer
	@param ptr		string to copy
	@param len		Length of string to copy
	@return number of bytes copied */
	ulint write(const void* ptr, ulint len)
	{
		ulint	n_copied = (len <= remaining()) ? len : remaining();

		ut_ad(m_free >= BLOCK_HDR_SIZE);
		ut_ad(m_free + n_copied < capacity());

		::memcpy(m_buffer.ptr() + m_free, ptr, n_copied);

		set_data_len(start_offset() + n_copied);

		m_free += n_copied;

		ut_ad(m_free <= capacity());

		return(n_copied);
	}

	/**
	Rewind the current pointer */
	void reset()
	{
		m_next_to_write = 0;
		m_free = BLOCK_HDR_SIZE;
	}

	/**
	Advance to the next block in the buffer.
	@param block_no		next log block number */
	void advance(ulint block_no)
	{
		ut_ad(m_free + meta_data_size() < m_buffer.capacity());

		set_data_len(IOBlock::SIZE);

		m_free += meta_data_size();

		/* Initialize the next block header */
		init_v2(block_no);
	}

	/** Copy the last, incompletely written, log block a log block length
	up, so that when the flush operation writes from the log buffer, the
	segment to write will not be changed by writers to the log */
	void pack()
	{
		ulint	end = ut_calc_align(m_free, IOBlock::SIZE);

		::memcpy(ptr() + end,
			 ptr() + end - IOBlock::SIZE, IOBlock::SIZE);

		m_free += IOBlock::SIZE;
	}

	/** Write the buffer to the log
	@param redo_log		the redo log to write to */
	void group_write(RedoLog* redo_log)
	{
		ulint	end;
		ulint	start;

		end = ut_calc_align(m_free, IOBlock::SIZE);
		start = ut_calc_align_down(m_next_to_write, IOBlock::SIZE);

		ut_ad(end > start);

		/* Do the write to the log files */
		redo_log->group_write(
			ptr() + start,
			end - start,
			ut_uint64_align_down(
				redo_log->m_written_to_all_lsn,
				IOBlock::SIZE),
			m_next_to_write - start);

		m_write_end_offset = m_free;
	}

	/**
	@return true if free space is running low */
	bool is_full() const
	{
		return(m_free > m_max_free);
	}

	/**
	@return true if there are pending changes that need to be flushed */
	bool pending() const
	{
		return(m_free > m_next_to_write);
	}

	/**
	Call this when the log buffer has been flushed to disk */
	void flushed()
	{
		m_next_to_write = m_write_end_offset;

		if (m_write_end_offset > m_max_free / 2) {
			/* Move the log buffer content to the start of the
			buffer */

			ulint	start = ut_calc_align_down(
				m_write_end_offset, IOBlock::SIZE);

			ulint	end = ut_calc_align(m_free, IOBlock::SIZE);

			memmove(ptr(), ptr() + start, end - start);

			m_free -= start;

			m_next_to_write -= start;
		}
	}

	/**
	Initializes a log block in the log buffer
	@param block_no		log block number */
	void init_v2(ulint block_no)
	{
		set_hdr_no(block_no);
		set_first_rec_group(0);
		set_data_len(BLOCK_HDR_SIZE);
	}

	/**
	Sets the log block first mtr log record group offset.
	@param offset		offset, 0 if none */
	void set_first_rec_group(ulint offset)
	{
		IOBlock::set_first_rec_group(block_header(), offset);
	}

	bool is_block_full() const
	{
		return(!is_block_empty() && remaining() == 0);
	}

	/**
	Gets the log block data length.
	@return the data len from the header */
	ulint get_data_len() const
	{
		return(IOBlock::get_data_len(block_header()));
	}

	/**
	Sets the log block data length.
	@param len		data length */
	void set_data_len(ulint len)
	{
		IOBlock::set_data_len(block_header(), len);
	}

	/**
	Sets a log block checkpoint number field (4 lowest bytes).
	@param no		checkpoint no */
	void set_checkpoint_no(ib_uint64_t no)
	{
		IOBlock::set_checkpoint_no(block_header(), no);
	}

	/**
	Gets the log block number stored in the header
	@return the block number */
	ulint get_hdr_no() const
	{
		return(IOBlock::get_hdr_no(block_header()));
	}

	/**
	Sets the log block number stored in the header; NOTE that this must
	be set before the flush bit!
	@param block_number	log block number: must be > 0 and
				< FLUSH_BIT_MASK */
	void set_hdr_no(ulint block_number)
	{
		ut_ad(block_number > 0);
		ut_ad(block_number < IOBlock::FLUSH_BIT_MASK);
		IOBlock::set_hdr_no(block_header(), block_number);
	}

	/**
	@return the size of the log record meta-data in bytes */
	static ulint meta_data_size()
	{
		return(BLOCK_HDR_SIZE + TRAILER_SIZE);
	}

	/**
	@return true if the buffer is empty */
	bool is_block_empty() const
	{
		return(m_free == BLOCK_HDR_SIZE);
	}

	/**
	Sets the log block flush bit. */
	void set_flush_bit()
	{
		ulint	start;

		start = ut_calc_align_down(m_next_to_write, IOBlock::SIZE);

		IOBlock::set_flush_bit(ptr() + start);
	}

	/**
	Gets a log block first mtr log record group offset.
	@return first mtr log record group byte offset from the block
		start, 0 if none */
	ulint get_first_rec_group() const
	{
		return(IOBlock::get_first_rec_group(block_header()));
	}

	ulint capacity() const
	{
		return(m_buffer.capacity());
	}

	byte* ptr()
	{
		return(m_buffer.ptr());
	}

	const byte* ptr() const
	{
		return(m_buffer.ptr());
	}

private:
	ulint start_offset() const
	{
		return(m_free % IOBlock::SIZE);
	}

	byte* block_start() const
	{
		void*	ptr;

		ptr = ut_align_down(m_buffer.ptr() + m_free, IOBlock::SIZE);

		return(reinterpret_cast<byte*>(ptr));
	}

	byte* block_header()
	{
		return(block_start());
	}

	const byte* block_header() const
	{
		return(block_start());
	}

	static ulint block_capacity()
	{
		return(IOBlock::capacity());
	}

	ulint remaining() const
	{
		ut_ad(block_capacity() >= start_offset());
		return(block_capacity() - start_offset());
	}

private:
	/** First offset in the log buffer where the byte content may not
	exist written to file, e.g., the start offset of a log record
	catenated later; this is advanced when a flush operation is completed
	to all the log groups */
	ulint		m_next_to_write;

	/** The data in buffer has been written up to this offset when the
	current write ends: this field will then be copied to m_next_to_write */
	ulint		m_write_end_offset;

	/** Start of the free space */
	ulint		m_free;

	/** Recommended maximum value of m_free, after which the buffer
	is flushed */
	ulint		m_max_free;

	/** The buffer to use for storing the log entries */
	IOBuffer	m_buffer;

private:
	// Disable copying
	LogBuffer(const LogBuffer&);
	LogBuffer& operator=(const LogBuffer&);
};

/** Buffers for IO */

/** Log group consists of a number of log files, each of
the same size; a log group is implemented as a space in
the sense of the module fil0fil. */
struct RedoLog::Group {
	/**
	Constructor
	@param n_files		number of log files
	@param size		log file size in bytes */
	Group(ulint n_files, os_offset_t size);

	/** Destructor */
	~Group();

	/**
	Calculates the data capacity of a log group, when the log file headers
	are not included.
	@return	capacity in bytes */
	lsn_t capacity() const;

	/**
	Calculates the offset within a log group, when the log file
	headers are not included.
	@return	size offset (<= offset) */
	lsn_t size_offset();

	/**
	Calculates the offset within a log group, when the log file headers are
	included.
	@param offset		size offset within the log group
	@return	real offset (>= offset) */
	lsn_t real_offset(lsn_t offset);

	/**
	Calculates the offset of an lsn within a log group.
	@param lsn		lsn
	@return	offset within the log group */
	lsn_t lsn_offset(lsn_t lsn);

	/**
	Sets the field values in group to correspond to a given lsn. For
	this function to work, the values must already be correctly
	initialized to correspond to some lsn, for instance, a checkpoint lsn.

	@param lsn 		lsn for which the values should be set */
	void set_fields(lsn_t lsn);

	/**
	@return true if the buffers were allocated */
	bool is_valid() const
	{
		return(m_file_headers.size() == m_n_files);
	}

	/**
	Reads a specified log segment to a buffer.
	@param log_buffer	buffer to use for reading data
	@param start_lsn	read area start
	@param end_lsn		read area end */
	void read(LogBuffer* log_buffer, lsn_t start_lsn, lsn_t end_lsn);

private:
	// Disable copying
	Group(const Group&);
	Group& operator=(const Group&);

public:
	/** Group file states */
	enum state_t {
		STATE_OK = 301,
		STATE_CORRUPTED = 302
	};

	typedef std::vector<LogBuffer*> buffers_t;

	/* The following fields are protected by log_t::mutex */

	/** Log group id */
	ulint		m_id;

	/** Number of files in the group */
	ulint		m_n_files;

	/** Individual log file size in bytes, including the
	log file header */
	os_offset_t	m_file_size;

	/** file space which implements the log group */
	ulint		m_space_id;

	/** GROUP_OK or GROUP_CORRUPTED */
	state_t		m_state;

	/** LSN used to fix coordinates within the log group */
	lsn_t		m_lsn;

	/** The offset of the above lsn */
	lsn_t		m_lsn_offset;

	/** Used only in recovery: recovery scan succeeded up
	to this LSN in this log group */
	lsn_t		m_scanned_lsn;

	/** Number of currently pending flush writes for
	this log group */
	ulint		m_n_pending_writes;

	/** buffers for each file header in the group */
	buffers_t	m_file_headers;
};

struct RedoLog::Checkpoint {
public:
	Checkpoint(lsn_t last_lsn)
		:
		m_max_age_async(),
		m_max_age(),
		m_next_no(),
		m_last_lsn(last_lsn),
		m_next_lsn(),
		m_n_pending_writes()
	{
		ulint	size = 2 * IOBlock::SIZE;

		m_buf = new(std::nothrow) IOBuffer(size);

		rw_lock_create(
			checkpoint_lock_key, &m_lock, SYNC_NO_ORDER_CHECK);
	}

	~Checkpoint()
	{
		delete m_buf;

		rw_lock_free(&m_lock);
	}

	/**
	@return the checkpoint LSN */
	lsn_t get_lsn() const
	{
		return(mach_read_from_8(m_buf->ptr() + LSN));
	}

	/**
	@return the checkpoint number */
	lsn_t get_no() const
	{
		return(mach_read_from_8(m_buf->ptr() + NO));
	}

	/**
	@return the LSN offset */
	ib_uint64_t get_lsn_offset() const
	{
		ib_uint64_t	lsn_offset;
		const byte*	ptr = m_buf->ptr();

		lsn_offset = mach_read_from_4(ptr + OFFSET_HIGH32);
		lsn_offset <<= 32;
		lsn_offset |= mach_read_from_4(ptr + OFFSET_LOW32);

		return(lsn_offset);
	}

	/**
	Reset the state */
	void reset()
	{
		m_next_no = 0;
		m_last_lsn = 0;
	}

	void set_age(ulint margin)
	{
		m_max_age_async = m_max_age = margin;

		m_max_age_async -= (margin / POOL_CHECKPOINT_RATIO_ASYNC);
	}

	/** Wait for the checkpoint write to complete */
	void wait_for_write()
	{
		rw_lock_s_lock(&m_lock);
		rw_lock_s_unlock(&m_lock);
	}

	/**
	Checks the consistency of the checkpoint info
	@return	true if ok */
	bool is_consistent() const;

	/**
	Completes an asynchronous checkpoint info write i/o to a log file. */
	void complete(RedoLog* redo_log);

	/**
	Writes the checkpoint info to a log group header. */
	void flush(RedoLog* redo_log);

	/**
	Check and set the latest checkpoint
	@param max_no		the number of the latest checkpoint
	@return true if value was updated */
	bool check_latest(ib_uint64_t& max_no) const
 		__attribute__((warn_unused_result));

	/**
	Reads a checkpoint info from a log group header to m_checkpoint.m_buf.
	@param read_offset	FIRST or SECOND 
	@param space_id		Space id to read from */
	void read(ulint read_offset, ulint space_id);

	/**
	Read the checkpoint and validate it.
	@param offset 		the checkpoint offset to read
	@param space_id		space id to read from 
	@return true if a valid checkpoint found */
	bool find(os_offset_t offset, ulint space_id)
		__attribute__((warn_unused_result));

	/**
	Looks for the maximum consistent checkpoint from the log groups.
	@param max_field	FIRST or SECOND 
	@param space_id		space id to read from 
	@return	true if valid checkpoint found */
	bool find_latest(os_offset_t& max_offset, ulint space_id)
 		__attribute__((warn_unused_result));

	/**
	@return lsn since last checkpoint */
	lsn_t age(lsn_t lsn) const
	{
		ut_ad(lsn >= m_last_lsn);
		return(lsn - m_last_lsn);
	}

private:
	/** First checkpoint field in the log header; we write
	alternately to the checkpoint fields when we make new checkpoints;
	this field is only defined in the first log file of a log group */
	static const ulint FIRST;

	/** Second checkpoint field in the log header */
	static const ulint SECOND;

	/* Offsets for a checkpoint field */
	static const ulint NO = 0;
	static const ulint LSN = 8;
	static const ulint OFFSET_LOW32 = 16;
	static const ulint LOG_BUF_SIZE = 20;
	static const ulint ARCHIVED_LSN = 24;
	
	static const ulint UNUSED_BEGIN = 32;
	static const ulint UNUSED = 256;
	static const ulint UNUSED_END = UNUSED_BEGIN + UNUSED;
	
	static const ulint CHECKSUM_1 = UNUSED_END;
	static const ulint CHECKSUM_2 = UNUSED_END + 4;
	static const ulint OFFSET_HIGH32 = UNUSED_END + 8;
	static const ulint SIZE = UNUSED_END + 12;
public:
	/** When this checkpoint age is exceeded we start
	an asynchronous writing of a new checkpoint */
	lsn_t		m_max_age_async;

	/** This is the maximum allowed value for lsn -
	last_checkpoint_lsn when a new query step is started */
	lsn_t		m_max_age;

	/** next checkpoint number */
	ib_uint64_t	m_next_no;

	/** Latest checkpoint lsn */
	lsn_t		m_last_lsn;

	/** Next checkpoint lsn */
	lsn_t		m_next_lsn;

	/** Number of currently pending checkpoint writes */
	ulint		m_n_pending_writes;

	/** Buffer for IO */
	IOBuffer*	m_buf;

	/** This latch is x-locked when a checkpoint write is
	running; a thread should wait for this without owning
	the log mutex */
	mutable rw_lock_t	m_lock;

};

/**
Redo command queue implementation, Start with an unbounded queue to keep
things simple. */
struct RedoLog::CommandQueue {
	CommandQueue()
	{
		mutex_create("log_cmdq_mutex", &m_mutex);
	}

	~CommandQueue()
	{
		ut_ad(m_queue.empty());
		mutex_free(&m_mutex);
	}

	void push(Command* cmd)
	{
		mutex_enter(&m_mutex);
		m_queue.push(cmd);
		mutex_exit(&m_mutex);
	}

	Command* pop()
	{
		mutex_enter(&m_mutex);
		Command*	cmd;

		cmd = m_queue.empty() ? NULL : m_queue.front();

		if (cmd != NULL) {
			m_queue.pop();
		}

		mutex_exit(&m_mutex);

		return(cmd);
	}

	typedef std::queue<Command*> CommandQ;

	CommandQ		m_queue;
	ib_mutex_t		m_mutex;
};

const ulint RedoLog::Checkpoint::FIRST = IOBlock::SIZE;
const ulint RedoLog::Checkpoint::SECOND = 3 * IOBlock::SIZE;

#if 0
// TODO: Make it proper pretty print
static
void
checkpoint_print(const char* msg, byte* ptr)
{
	fprintf(stderr,
		"*** %s ****  no=%llu, lsn=%llu, off1=%lu,off2=%lu "
		"size=%lu, alsn=%llu, c1=%lu, c2=%lu\n",
		msg,
		mach_read_from_8(ptr + NO),
		mach_read_from_8(ptr + LSN),
		mach_read_from_4(ptr + OFFSET_LOW32),
		mach_read_from_4(ptr + OFFSET_HIGH32),
		mach_read_from_4(ptr + LOG_BUF_SIZE),
		mach_read_from_8(ptr + ARCHIVED_LSN),
		mach_read_from_4(ptr + CHECKSUM_1) & 0xFFFFFFFFUL,
		mach_read_from_4(ptr + CHECKSUM_2) & 0xFFFFFFFFUL);

	fflush(stderr);
}
#endif

/**
Constructor
@param n_files		number of log files
@param size		log file size in bytes */
RedoLog::Group::Group(ulint n_files, os_offset_t size)
	:
	m_id(),
	m_n_files(n_files),
	m_file_size(size),
	m_space_id(SPACE_FIRST_ID),
	m_state(STATE_OK),
	m_lsn(START_LSN),
	m_lsn_offset(LOG_FILE_HDR_SIZE),
	m_scanned_lsn(),
	m_n_pending_writes()
{
	ulint	buffer_size = LOG_FILE_HDR_SIZE + IOBlock::SIZE;

	for (ulint i = 0; i < m_n_files; i++) {
		LogBuffer*	ptr;
		
		ptr = new(std::nothrow) LogBuffer(buffer_size);

		if (ptr == NULL) {
			ib_logf(IB_LOG_LEVEL_ERROR, "Out of memory");
		} else {
			m_file_headers.push_back(ptr);
		}
	}
}

/** Destructor */
RedoLog::Group::~Group()
{
	buffers_t::iterator end = m_file_headers.end();

	for (buffers_t::iterator i = m_file_headers.begin(); i != end; ++i) {
		delete *i;
	}
}

/**
Calculates the data capacity of a log group, when the log file headers
are not included.
@return	capacity in bytes */
lsn_t
RedoLog::Group::capacity() const
{
	return((m_file_size - LOG_FILE_HDR_SIZE) * m_n_files);
}

/**
Calculates the offset within a log group, when the log file
headers are not included.
@return	size offset (<= offset) */
lsn_t
RedoLog::Group::size_offset()
{
	return(m_lsn_offset - LOG_FILE_HDR_SIZE
		* (1 + m_lsn_offset / m_file_size));
}

/**
Calculates the offset within a log group, when the log file headers are
included.
@param offset		size offset within the log group
@return	real offset (>= offset) */
lsn_t
RedoLog::Group::real_offset(lsn_t offset)
{
	return(offset + LOG_FILE_HDR_SIZE
		* (1 + offset / (m_file_size - LOG_FILE_HDR_SIZE)));
}

/**
Calculates the offset of an lsn within a log group.
@param lsn		lsn
@return	offset within the log group */
lsn_t
RedoLog::Group::lsn_offset(lsn_t lsn)
{
	lsn_t	difference;
	lsn_t	group_size = capacity();
	lsn_t	offset = size_offset();

	if (lsn >= m_lsn) {
		difference = lsn - m_lsn;
	} else {
		difference = m_lsn - lsn;

		difference %= group_size;

		difference = group_size - difference;
	}

	offset = (offset + difference) % group_size;

	return(real_offset(offset));
}

/**
Sets the field values in group to correspond to a given lsn. For
this function to work, the values must already be correctly
initialized to correspond to some lsn, for instance, a checkpoint lsn.

@param lsn 		lsn for which the values should be set */
void
RedoLog::Group::set_fields(lsn_t lsn)
{
	m_lsn_offset = lsn_offset(lsn);
	m_lsn = lsn;
}

/** Constructor
@param n_files		number of log files
@param size		log file size in bytes
@param mem_avail	Memory available in the buffer pool, in bytes */
RedoLog::RedoLog(ulint n_files, os_offset_t size, ulint mem_avail)
	:
	m_group(),
	m_recover(),
	m_write_lsn(),
	m_n_log_ios(),
	m_n_log_ios_old(),
	m_file_size(size),
	m_print_counter(),
	m_n_files(n_files),
	m_n_pending_writes(),
	m_current_flush_lsn(),
	m_state(STATE_RUNNING),
	m_flushed_to_disk_lsn()
{
	mutex_create("log_sys", &m_mutex);

	mutex_create("log_flush_order", &m_flush_order_mutex);

	mutex_acquire();

#ifdef UNIV_DEBUG
	/** false if writing to the redo log (mtr_commit) is forbidden.
	Protected by log_sys->mutex. */
	enable_log_write();
#endif /* UNIV_DEBUG */

	/* Start the lsn from one log block from zero: this way every
	log record has a start lsn != zero, a fact which we will use */

	m_lsn = START_LSN;

	m_written_to_all_lsn = m_lsn;
	m_written_to_some_lsn = m_lsn;

	ut_ad(LOG_BUFFER_SIZE >= 16 * IOBlock::SIZE);
	ut_ad(LOG_BUFFER_SIZE >= 4 * UNIV_PAGE_SIZE);

	m_buf = new(std::nothrow) LogBuffer(LOG_BUFFER_SIZE);
	ut_ad(m_buf != NULL);

	m_checkpoint = new(std::nothrow) Checkpoint(m_lsn);

	m_last_printout_time = ut_time();

	m_no_flush_event = os_event_create("no_flush_event");

	m_check_flush_or_checkpoint = true;

	os_event_set(m_no_flush_event);

	m_one_flushed_event = os_event_create("one_flushed_event");

	os_event_set(m_one_flushed_event);

	m_buf->init_v2(convert_lsn_to_no(m_lsn));

	m_buf->set_first_rec_group(BLOCK_HDR_SIZE);

	m_lsn = START_LSN + BLOCK_HDR_SIZE;

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE, m_checkpoint->age(m_lsn));

	m_ibuf_allowed = true;

	mutex_release();

	m_n_free_frames = mem_avail >= (10 * 1024 * 1024) ? 512 : 256;

	m_cmdq = new CommandQueue();
}

RedoLog::~RedoLog()
{
	ut_a(m_cmdq == NULL);
}

/**
Converts a lsn to a log block number.
@param lsn		lsn of a byte within the block
@return	log block number, it is > 0 and <= 1G */
ulint
RedoLog::convert_lsn_to_no(lsn_t lsn)
{
	return(((ulint) (lsn / IOBlock::SIZE) & 0x3FFFFFFFUL) + 1);
}

/**
Returns the oldest modified block lsn in the pool, or m_lsn if none
exists.
@return	LSN of oldest modification */
lsn_t
RedoLog::buf_pool_get_oldest_modification()
{

	ut_ad(is_mutex_owned());

	lsn_t	lsn = ::buf_pool_get_oldest_modification();

	if (!lsn) {

		lsn = m_lsn;
	}

	return(lsn);
}

/**
Writes to the log the string given. The log must be released with close().
@param ptr		string
@param len		string length
@param start_lsn	start lsn of the log record
@return	end lsn of the log record, zero if did not succeed */
lsn_t
RedoLog::open(const void* ptr, ulint	 len, lsn_t* start_lsn)
{
	mutex_acquire();

	if (!m_buf->will_fit_in_one_block(len)) {

		return(0);
	}

	*start_lsn = m_lsn;

	ulint	n_copied = m_buf->write(ptr, len);

	ut_a(len == n_copied);

	m_lsn += len;

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE, m_checkpoint->age(m_lsn));

	return(m_lsn);
}

/***
Write the buffer to the log.
@param ptr		buffer to write
@param len 		length of buffer to write */
void
RedoLog::write(const byte* ptr, ulint len)
{
	ut_ad(is_mutex_owned());

	while (len > 0) {
		ut_ad(is_write_allowed());

		ulint	n_copied = m_buf->write(ptr, len);

		m_lsn += n_copied;

		/* Check if the write resulted in a full block. */
		if (n_copied < len) {

			ut_ad(m_buf->is_block_full());

			m_buf->set_checkpoint_no(m_checkpoint->m_next_no);

			m_lsn += m_buf->meta_data_size();

			m_buf->advance(convert_lsn_to_no(m_lsn));
		}

		ptr += n_copied;

		ut_ad(len >= n_copied);
		len -= n_copied;
	}

	srv_stats.log_write_requests.inc();
}

/**
Opens the log for log_write_low. The log must be closed with log_close and
released with close().
@param len 		length of data to be catenated
@param own_mutex	true if caller owns the mutex
@return	start lsn of the log record */

lsn_t
RedoLog::open(ulint len, bool own_mutex)
{
#ifdef UNIV_DEBUG
	ulint	count	= 0;
#endif /* UNIV_DEBUG */

	if (!own_mutex) {
		mutex_acquire();
	} else {
		ut_ad(is_mutex_owned());
	}

	ut_ad(len < m_buf->capacity() / 2);

	for (;;) {
		ut_ad(is_write_allowed());

		/* Calculate an upper limit for the space the
		string may take in the log buffer */

		if (!m_buf->will_fit_in_log(BUF_WRITE_MARGIN + (5 * len) / 4)) {

			mutex_release();

			/* Not enough free space, do a syncronous flush
			of the log buffer */
		
			sync_flush();

			srv_stats.log_waits.inc();

			ut_ad(++count < 50);

			mutex_acquire();
		} else {
			break;
		}
	}

	return(m_lsn);
}

/**
Closes the log.
@return	lsn */

lsn_t
RedoLog::close()
{
	ut_ad(is_mutex_owned());
	ut_ad(is_write_allowed());

	if (m_buf->get_first_rec_group() == 0) {
		/* We initialized a new log block which was not written
		full by the current mtr: the next mtr log record group
		will start within this block at the offset data_len */

		m_buf->set_first_rec_group(m_buf->get_data_len());
	}

	if (m_buf->is_full()) {

		m_check_flush_or_checkpoint = true;
	}

	lsn_t	checkpoint_age = m_checkpoint->age(m_lsn);

	if (checkpoint_age >= m_log_group_capacity) {
		/* TODO: split btr_store_big_rec_extern_fields() into small
		steps so that we can release all latches in the middle, and
		call free_check() to ensure we never write over log written
		after the latest checkpoint. In principle, we should split all
		big_rec operations, but other operations are smaller. */

		if (!log_has_printed_chkp_warning
		    || difftime(ut_time(), log_last_warning_time) > 15) {

			log_has_printed_chkp_warning = true;
			log_last_warning_time = time(NULL);

			ib_logf(IB_LOG_LEVEL_ERROR,
				"The age of the last checkpoint is "
				LSN_PF ", which exceeds the log group"
				" capacity " LSN_PF ".  If you are using"
				" big BLOB or TEXT rows, you must set the"
				" combined size of log files at least 10"
				" times bigger than the largest such row.",
				checkpoint_age,
				m_log_group_capacity);
		}
	}

	if (checkpoint_age <= m_max_modified_age_sync) {

		return(m_lsn);
	}

	lsn_t	oldest_lsn = buf_pool_get_oldest_modification();

	if (!oldest_lsn
	    || m_lsn - oldest_lsn > m_max_modified_age_sync
	    || checkpoint_age > m_checkpoint->m_max_age_async) {

		m_check_flush_or_checkpoint = true;
	}

	return(m_lsn);
}

/**
Calculates the recommended highest values for lsn - last_checkpoint_lsn,
lsn - buf_get_oldest_modification(), and lsn - max_archive_lsn_age.

@retval true on success
@retval false if the smallest log group is too small to
accommodate the number of OS threads in the database server */
bool
RedoLog::calc_max_ages()
{
	mutex_acquire();

	lsn_t	smallest_capacity;

	if (m_group->capacity() < LSN_MAX) {

		smallest_capacity = m_group->capacity();
	} else {
		smallest_capacity = LSN_MAX;
	}

	/* Add extra safety */
	smallest_capacity -= (smallest_capacity / 10);

	/* For each OS thread we must reserve so much free space in the
	smallest log group that it can accommodate the log entries produced
	by single query steps: running out of free log space is a serious
	system error which requires rebooting the database. */

	ulint	free;

	free = CHECKPOINT_FREE_PER_THREAD * (10 + srv_thread_concurrency)
		+ CHECKPOINT_EXTRA_FREE;

	bool	success	= true;

	if (free >= smallest_capacity / 2) {

		success = false;

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Cannot continue operation.  ib_logfiles are too "
			"small for innodb_thread_concurrency %lu. The "
			"combined size of ib_logfiles should be bigger than "
			"200 kB * innodb_thread_concurrency. To get mysqld "
			"to start up, set innodb_thread_concurrency in "
			"my.cnf to a lower value, for example, to 8. After "
			"an ERROR-FREE shutdown of mysqld you can adjust "
			"the size of ib_logfiles, as explained in " REFMAN " "
			"adding-and-removing.html.",
			(ulong) srv_thread_concurrency);

	} else {
		lsn_t	margin = smallest_capacity - free;

		/* Add still some extra safety */
		margin = margin - margin / 10;

		m_log_group_capacity = smallest_capacity;

		m_max_modified_age_async = margin
			- margin / POOL_PREFLUSH_RATIO_ASYNC;

		m_max_modified_age_sync = margin
			- margin / POOL_PREFLUSH_RATIO_SYNC;

		m_checkpoint->set_age(margin);
	}

	mutex_release();

	return(success);
}

/**
@return true if success, false if not */
bool
RedoLog::init()
{
	ut_ad(m_group == NULL);

	m_group = new(std::nothrow) Group(m_n_files, m_file_size);

	if (m_group == NULL) {

		ib_logf(IB_LOG_LEVEL_ERROR, "Out of memory!");

		return(false);

	} else if (!m_group->is_valid()) {

		delete m_group;
		m_group = NULL;

		return(false);
	}

	return(calc_max_ages());
}

/**
Resize the log.
@param n_files		The number of physical files
@param size		Size in bytes
@return true on success */
bool
RedoLog::resize(ulint n_files, os_offset_t size)
{
	if (m_group != NULL) {
		delete m_group;
		m_group = NULL;
	}

	m_file_size = size;
	m_n_files = n_files;

	return(init());
}

/**
Does the unlockings needed in flush i/o completion.
@param code		any ORed combination of UNLOCK_FLUSH_LOCK
			and UNLOCK_NONE_FLUSHED_LOCK */
void
RedoLog::flush_do_unlocks(ulint code)
{
	ut_ad(is_mutex_owned());

	/* NOTE that we must own the log mutex when doing the setting of the
	events: this is because transactions will wait for these events to
	be set, and at that moment the log flush they were waiting for must
	have ended. If the log mutex were not reserved here, the i/o-thread
	calling this function might be preempted for a while, and when it
	resumed execution, it might be that a new flush had been started, and
	this function would erroneously signal the NEW flush as completed.
	Thus, the changes in the state of these events are performed
	atomically in conjunction with the changes in the state of
	m_n_pending_writes etc. */

	if (code & UNLOCK_NONE_FLUSHED_LOCK) {
		os_event_set(m_one_flushed_event);
	}

	if (code & UNLOCK_FLUSH_LOCK) {
		os_event_set(m_no_flush_event);
	}
}

/**
Checks if a flush is completed for a log group and does the completion
routine if yes.
@return	UNLOCK_NONE_FLUSHED_LOCK or 0 */
ulint
RedoLog::group_check_flush_completion()
{
	ut_ad(is_mutex_owned());

	if (!m_one_flushed && m_group->m_n_pending_writes == 0) {

		m_written_to_some_lsn = m_write_lsn;
		m_one_flushed = true;

		return(UNLOCK_NONE_FLUSHED_LOCK);
	}

	return(0);
}

/**
Checks if a flush is completed and does the completion routine if yes.
@return	UNLOCK_FLUSH_LOCK or 0 */
ulint
RedoLog::check_flush_completion()
{
	ut_ad(is_mutex_owned());

	if (m_n_pending_writes == 0) {

		m_written_to_all_lsn = m_write_lsn;

		m_buf->flushed();

		return(UNLOCK_FLUSH_LOCK);
	}

	return(0);
}

/**
Completes an i/o to a log file.
@param group	log group or NULL */
void
RedoLog::io_complete(Group* ptr)
{
	if ((ulint) ptr & 0x1UL) {
		/* It was a checkpoint write */
		if (srv_unix_file_flush_method != SRV_UNIX_O_DSYNC
		    && srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {

			fil_flush(m_group->m_space_id);
		}

		m_checkpoint->complete(this);
	}
}

/**
Writes a log file header to a log file space.
@param nth_file		header to the nth file in the log file space
@param start_lsn	log file data starts at this lsn */
void
RedoLog::group_file_header_flush(
	ulint		nth_file,
	lsn_t		start_lsn)
{
	byte*		buf;

	ut_ad(is_mutex_owned());
	ut_ad(is_write_allowed());
	ut_ad(nth_file < m_group->m_n_files);

	buf = m_group->m_file_headers[nth_file]->ptr();

	mach_write_to_4(buf + LOG_GROUP_ID, m_group->m_id);
	mach_write_to_8(buf + LOG_FILE_START_LSN, start_lsn);

	/* Wipe over possible label of ibbackup --restore */
	memcpy(buf + FILE_WAS_CREATED_BY_HOT_BACKUP, "    ", 4);

	os_offset_t	dest_offset = nth_file * m_group->m_file_size;

	m_n_log_ios++;

	MONITOR_INC(MONITOR_LOG_IO);

	srv_stats.os_log_pending_writes.inc();

	fil_io(OS_FILE_WRITE | OS_FILE_LOG,
		true,
		m_group->m_space_id,
		0,
		(ulint) (dest_offset / UNIV_PAGE_SIZE),
		(ulint) (dest_offset % UNIV_PAGE_SIZE),
		IOBlock::SIZE,
		buf,
		m_group);

	srv_stats.os_log_pending_writes.dec();
}

/**
Writes a buffer to a log file group
@param buf		buffer
@param len		buffer len; must be divisible by IOBlock::SIZE
@param start_lsn	start lsn of the buffer; must be divisible by
			IOBlock::SIZE
@param new_data_offset	start offset of new data in buf: this parameter is
			used to decide if we have to write a new log file
			header */
void
RedoLog::group_write(
	byte*		buf,
	ulint		len,
	lsn_t		start_lsn,
	ulint		new_data_offset)
{
	ut_ad(mutex_own(&m_mutex));
	ut_ad(is_write_allowed());
	ut_ad(len % IOBlock::SIZE == 0);
	ut_ad(start_lsn % IOBlock::SIZE == 0);

	bool	write_header = (new_data_offset == 0);

	while (len > 0) {

		os_offset_t	next_offset = m_group->lsn_offset(start_lsn);

		if ((next_offset % m_group->m_file_size == LOG_FILE_HDR_SIZE)
		    && write_header) {

			/* We start to write a new log file instance in
			the group */

			ut_ad(next_offset / m_group->m_file_size <= ULINT_MAX);

			group_file_header_flush(
				(ulint) (next_offset / m_group->m_file_size),
				start_lsn);

			srv_stats.os_log_written.add(IOBlock::SIZE);

			srv_stats.log_writes.inc();
		}

		ulint	write_len;

		if ((next_offset % m_file_size) + len > m_file_size) {

			/* if the above condition holds, then the below
			expression is < len which is ulint, so the typecast
			is ok */

			write_len = (ulint)
				(m_group->m_file_size
				 - (next_offset % m_group->m_file_size));
		} else {
			write_len = len;
		}

		/* Calculate the checksums for each log block and write them to
		the trailer fields of the log blocks */

		for (ulint i = 0; i < write_len / IOBlock::SIZE; ++i) {
			IOBlock::store_checksum(buf + i * IOBlock::SIZE);
		}

		++m_n_log_ios;

		MONITOR_INC(MONITOR_LOG_IO);

		srv_stats.os_log_pending_writes.inc();

		ut_ad(next_offset / UNIV_PAGE_SIZE <= ULINT_MAX);

		fil_io(OS_FILE_WRITE | OS_FILE_LOG,
		       true,
		       m_group->m_space_id,
		       0,
		       (ulint) (next_offset / UNIV_PAGE_SIZE),
		       (ulint) (next_offset % UNIV_PAGE_SIZE),
		       write_len,
		       buf,
		       m_group);

		srv_stats.os_log_pending_writes.dec();

		srv_stats.os_log_written.add(write_len);
		srv_stats.log_writes.inc();

		if (write_len >= len) {
			break;
		}

		start_lsn += write_len;
		len -= write_len;
		buf += write_len;

		write_header = true;
	}
}

/**
Wait for IO to complete.
@param mode		Wait mode */
void
RedoLog::wait_for_io(wait_mode_t mode)
{
	switch (mode) {
	case WAIT_MODE_ONE_GROUP:
		os_event_wait(m_one_flushed_event);
		break;

	case WAIT_MODE_NO_WAIT:
		break;
	}
}

/**
This function is called, e.g., when a transaction wants to commit. It checks
that the log has been written to the log file up to the last log entry written
by the transaction. If there is a flush running, it waits and checks if the
flush flushed enough. If not, starts a new flush.
@param lsn		log sequence number up to which
			the log should be written, LSN_MAX if not specified
@param mode		wait mode
@param flush_to_disk	true if we want the written log also to be flushed
			to disk */
void
RedoLog::write_up_to(lsn_t lsn, wait_mode_t mode, bool flush_to_disk)
{
	ut_ad(!srv_read_only_mode);

	if (!is_ibuf_allowed()) {
		/* Recovery is running and no operations on the log files are
		allowed yet. */

		return;
	}

	for (;;) {
		mutex_acquire();

		ut_ad(is_write_allowed());

		if (flush_to_disk && m_flushed_to_disk_lsn >= lsn) {

			mutex_release();

			return;
		}

		if (!flush_to_disk
		    && (m_written_to_all_lsn >= lsn
			|| m_written_to_some_lsn >= lsn)) {

			mutex_release();

			return;
		}

		if (m_n_pending_writes == 0) {
			break;
		}

		/* A write (+ possibly flush to disk) is running */

		if ((flush_to_disk && m_current_flush_lsn >= lsn)
		    || (!flush_to_disk && m_write_lsn >= lsn)) {

			/* Wait for IO to complete */

			mutex_release();

			wait_for_io(mode);

			return;
		}

		mutex_release();

		/* Wait for the write to complete and try to start
		a new write */

		os_event_wait(m_no_flush_event);
	}

	if (!flush_to_disk && m_buf->pending()) {
		/* Nothing to write and no flush to disk requested */

		mutex_release();

		return;
	}

	++m_n_pending_writes;

	MONITOR_INC(MONITOR_PENDING_LOG_WRITE);

	/* We assume here that we have only one log group! */
	m_group->m_n_pending_writes++;

	os_event_reset(m_no_flush_event);
	os_event_reset(m_one_flushed_event);

	m_write_lsn = m_lsn;

	if (flush_to_disk) {
		m_current_flush_lsn = m_lsn;
	}

	m_one_flushed = false;

	m_buf->set_flush_bit();

	m_buf->set_checkpoint_no(m_checkpoint->m_next_no);

	m_buf->pack();

	m_buf->group_write(this);

	m_group->set_fields(m_write_lsn);

	mutex_release();

	if (srv_unix_file_flush_method == SRV_UNIX_O_DSYNC) {
		/* O_DSYNC means the OS did not buffer the log file at all:
		so we have also flushed to disk what we have written */

		m_flushed_to_disk_lsn = m_write_lsn;

	} else if (flush_to_disk) {

		fil_flush(m_group->m_space_id);
		m_flushed_to_disk_lsn = m_write_lsn;
	}

	mutex_acquire();

	ut_ad(m_n_pending_writes == 1);
	ut_ad(m_group->m_n_pending_writes == 1);

	--m_group->m_n_pending_writes;

	--m_n_pending_writes;

	MONITOR_DEC(MONITOR_PENDING_LOG_WRITE);

	{
		ulint unlock = group_check_flush_completion();
		unlock |= check_flush_completion();

		flush_do_unlocks(unlock);
	}

	mutex_release();
}

/**
Does a syncronous flush of the log buffer to disk. */
void
RedoLog::sync_flush()
{
	ut_ad(!srv_read_only_mode);
	mutex_acquire();

	lsn_t	lsn = m_lsn;

	mutex_release();

	write_up_to(lsn, WAIT_MODE_ONE_GROUP, true);
}

/**
This functions writes the log buffer to the log file and if 'flush'
is set it forces a flush of the log file as well. This is meant to be
called from background master thread only as it does not wait for
the write (+ possible flush) to finish.
@param flush		flush the logs to disk */
void
RedoLog::async_flush(bool flush)
{
	mutex_acquire();

	lsn_t	lsn = m_lsn;

	mutex_release();

	write_up_to(lsn, WAIT_MODE_NO_WAIT, flush);
}

/**
Tries to establish a big enough margin of free space in the log buffer, such
that a new log entry can be catenated without an immediate need for a flush. */
void
RedoLog::flush_margin()
{
	lsn_t	lsn	= 0;

	mutex_acquire();

	if (m_buf->is_full()) {

		if (m_n_pending_writes > 0) {
			/* A flush is running: hope that it will provide enough
			free space */
		} else {
			lsn = m_lsn;
		}
	}

	mutex_release();

	if (lsn > 0) {
		write_up_to(lsn, WAIT_MODE_NO_WAIT, false);
	}
}

/**
Advances the smallest lsn for which there are unflushed dirty blocks in the
buffer pool. NOTE: this function may only be called if the calling thread owns
no synchronization objects!

@param recover		Recovery manager
@param new_oldest	try to advance oldest_modified_lsn at least to this lsn

@return false if there was a flush batch of the same type running,
which means that we could not start this flush batch */
bool
RedoLog::preflush_pool_modified_pages(
	lsn_t		new_oldest)
{
	bool		success;
	ulint		n_pages;

	if (is_recovery_on()) {
		/* If the recovery is running, we must first apply all
		log records to their respective file pages to get the
		right modify lsn values to these pages: otherwise, there
		might be pages on disk which are not yet recovered to the
		current lsn, and even after calling this function, we could
		not know how up-to-date the disk version of the database is,
		and we could not make a new checkpoint on the basis of the
		info on the buffer pool only. */

		m_recover->apply_hashed_log_recs(true);
	}

	success = buf_flush_list(ULINT_MAX, new_oldest, &n_pages);

	buf_flush_wait_batch_end(NULL, BUF_FLUSH_LIST);

	if (!success) {
		MONITOR_INC(MONITOR_FLUSH_SYNC_WAITS);
	}

	MONITOR_INC_VALUE_CUMULATIVE(
		MONITOR_FLUSH_SYNC_TOTAL_PAGE,
		MONITOR_FLUSH_SYNC_COUNT,
		MONITOR_FLUSH_SYNC_PAGES,
		n_pages);

	return(success);
}

/**
Completes an asynchronous checkpoint info write i/o to a log file. */
void
RedoLog::Checkpoint::complete(RedoLog* redo_log)
{
	redo_log->mutex_acquire();

	ut_ad(m_n_pending_writes > 0);

	--m_n_pending_writes;

	MONITOR_DEC(MONITOR_PENDING_CHECKPOINT_WRITE);

	if (m_n_pending_writes == 0) {

		++m_next_no;

		m_last_lsn = m_next_lsn;

		MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE, age(redo_log->m_lsn));

		rw_lock_x_unlock_gen(&m_lock, CHECKPOINT);
	}

	redo_log->mutex_release();
}

/**
Writes the checkpoint info to a log group header. */
void
RedoLog::Checkpoint::flush(RedoLog* redo_log)
{
	ut_ad(!srv_read_only_mode);
	ut_ad(redo_log->is_mutex_owned());

	byte*	ptr = m_buf->ptr();
	RedoLog::LogBuffer*	log_buf = redo_log->m_buf;

	ut_ad(SIZE <= IOBlock::SIZE);

	mach_write_to_8(ptr + NO, m_next_no);

	mach_write_to_8(ptr + LSN, m_next_lsn);

	lsn_t	lsn_offset = redo_log->m_group->lsn_offset(m_next_lsn);
	ulint	low32 = lsn_offset & 0xFFFFFFFFUL;

	mach_write_to_4(ptr + OFFSET_LOW32, low32);

	mach_write_to_4(ptr + OFFSET_HIGH32, lsn_offset >> 32);

	mach_write_to_4(ptr + LOG_BUF_SIZE, log_buf->capacity());

	mach_write_to_8(ptr + ARCHIVED_LSN, LSN_MAX);

	/* For backward compatibility */
	memset(ptr + UNUSED_BEGIN, 0x0, UNUSED);

	ulint	fold1 = ut_fold_binary(ptr, CHECKSUM_1);

	mach_write_to_4(ptr + CHECKSUM_1, fold1);

	ulint	fold2 = ut_fold_binary(ptr + LSN, CHECKSUM_2 - LSN);

	mach_write_to_4(ptr + CHECKSUM_2, fold2);

	/* We alternate the physical place of the checkpoint
	info in the first log file */

	ulint	write_offset;

	write_offset = !(m_next_no & 1) ? FIRST : SECOND;

	if (m_n_pending_writes == 0) {

		rw_lock_x_lock_gen(&m_lock, CHECKPOINT);
	}

	++m_n_pending_writes;

	MONITOR_INC(MONITOR_PENDING_CHECKPOINT_WRITE);

	++redo_log->m_n_log_ios;

	MONITOR_INC(MONITOR_LOG_IO);

	/* We send as the last parameter the group machine address
	added with 1, as we want to distinguish between a normal log
	file write and a checkpoint field write */

	fil_io(OS_FILE_WRITE | OS_FILE_LOG,
		false,		/* Async IO */
		redo_log->m_group->m_space_id,
		0,
		write_offset / UNIV_PAGE_SIZE,
		write_offset % UNIV_PAGE_SIZE,
		IOBlock::SIZE,
		ptr,
		((byte*) redo_log->m_group + 1));

	ut_ad(((ulint) redo_log->m_group & 0x1UL) == 0);
}

/**
Reads a checkpoint info from a log group header to m_checkpoint.m_buf.
@param read_offset	FIRST or SECOND 
@param space_id		Space id to read from */
void
RedoLog::Checkpoint::read(ulint read_offset, ulint space_id)
{
	ut_ad(read_offset == FIRST || read_offset == SECOND);

	fil_io(OS_FILE_READ | OS_FILE_LOG,
	       true,
	       space_id,
	       0,
	       read_offset / UNIV_PAGE_SIZE,
	       read_offset % UNIV_PAGE_SIZE,
	       IOBlock::SIZE,
	       m_buf->ptr(),
	       NULL);
}

/**
Makes a checkpoint. Note that this function does not flush dirty
blocks from the buffer pool: it only checks what is lsn of the oldest
modification in the pool, and writes information about the lsn in
log files. Use log_checkpoint_at to flush also the pool.

@param sync		true if synchronous operation is desired
@param write_always	the function normally checks if the the new checkpoint
			would have a greater lsn than the previous one: if not,
			then no physical write is done; by setting this
			parameter true, a physical write will always be made
			to log files

@return	true if success, false if a checkpoint write was already running */
bool
RedoLog::checkpoint(bool sync, bool write_always)
{
	lsn_t	oldest_lsn;

	ut_ad(!srv_read_only_mode);

	if (m_recover != NULL) {
		m_recover->apply_hashed_log_recs(true);
	}

	if (srv_unix_file_flush_method != SRV_UNIX_NOSYNC) {
		fil_flush_file_spaces(FIL_TABLESPACE);
	}

	mutex_acquire();

	ut_ad(is_write_allowed());
	oldest_lsn = buf_pool_get_oldest_modification();

	mutex_release();

	/* Because log also contains headers and dummy log records,
	if the buffer pool contains no dirty buffers, oldest_lsn
	gets the value m_lsn from the previous function,
	and we must make sure that the log is flushed up to that
	lsn. If there are dirty buffers in the buffer pool, then our
	write-ahead-logging algorithm ensures that the log has been flushed
	up to oldest_lsn. */

	write_up_to(oldest_lsn, WAIT_MODE_ONE_GROUP, true);

	mutex_acquire();

	if (!write_always && m_checkpoint->m_last_lsn >= oldest_lsn) {

		mutex_release();

		return(true);
	}

	ut_ad(m_flushed_to_disk_lsn >= oldest_lsn);

	if (m_checkpoint->m_n_pending_writes > 0) {
		/* A checkpoint write is running */

		mutex_release();

		if (sync) {
			m_checkpoint->wait_for_write();
		}

		return(false);
	}

	m_checkpoint->m_next_lsn = oldest_lsn;

	if (!srv_read_only_mode) {
		m_checkpoint->flush(this);
	}

	mutex_release();

	DBUG_PRINT("ib_log", ("checkpoint " LSN_PF " at " LSN_PF,
			      m_checkpoint->m_next_no,
			      oldest_lsn));

	MONITOR_INC(MONITOR_NUM_CHECKPOINT);

	if (sync) {
		m_checkpoint->wait_for_write();
	}

	return(true);
}

/**
Makes a checkpoint at a given lsn or later.
@param lsn,		make a checkpoint at this or a later lsn, if LSN_MAX,
			makes a checkpoint at the latest lsn
@param write_always	the function normally checks if the new checkpoint
			would have a greater lsn than the previous one: if not,
			then no physical write is done; by setting this
			parameter TRUE, a physical write will always be made
			to log files */
void
RedoLog::checkpoint_at(lsn_t lsn, bool write_always)
{
	/* Preflush pages synchronously */

	while (!preflush_pool_modified_pages(lsn)) {
		/* Flush as much as we can */
	}

	while (!checkpoint(true, write_always)) {
		/* Force a checkpoint */
	}
}

/**
Tries to establish a big enough margin of free space in the log groups, such
that a new log entry can be catenated without an immediate need for a
checkpoint. NOTE: this function may only be called if the calling thread
owns no synchronization objects!
@param recover		The recovery manager */
void
RedoLog::checkpoint_margin()
{
	for (;;) {
		lsn_t	advance = 0;
		bool	do_checkpoint = false;
		bool	checkpoint_sync = false;

		mutex_acquire();
		ut_ad(is_write_allowed());

		if (m_check_flush_or_checkpoint == false) {
			mutex_release();
			break;
		}

		lsn_t	oldest_lsn = buf_pool_get_oldest_modification();
		lsn_t	age = m_lsn - oldest_lsn;

		if (age > m_max_modified_age_sync) {

			/* A flush is urgent: we have to do a synchronous
			preflush */
			advance = 2 * (age - m_max_modified_age_sync);
		}

		lsn_t	checkpoint_age = m_checkpoint->age(m_lsn);

		if (checkpoint_age > m_checkpoint->m_max_age) {
			/* A checkpoint is urgent: we do it synchronously */

			checkpoint_sync = true;

			do_checkpoint = true;

		} else if (checkpoint_age > m_checkpoint->m_max_age_async) {
			/* A checkpoint is not urgent: do it asynchronously */

			do_checkpoint = true;

			m_check_flush_or_checkpoint = false;
		} else {
			m_check_flush_or_checkpoint = false;
		}

		mutex_release();

		if (advance > 0) {
			lsn_t	new_oldest = oldest_lsn + advance;

			bool	success = preflush_pool_modified_pages(
				new_oldest);

			/* If the flush succeeded, this thread has done
			its part and can proceed. If it did not succeed,
			there was another thread doing a flush at the
			same time. */

			if (!success) {
				mutex_acquire();

				m_check_flush_or_checkpoint = true;

				mutex_release();

				continue;
			}
		}

		if (do_checkpoint) {
			checkpoint(checkpoint_sync, false);

			if (!checkpoint_sync) {
				break;
			}

			continue;
		}

		break;
	}
}

/**
Reads a specified log segment to a buffer.
@param log_buffer	buffer to use for reading data
@param start_lsn	read area start
@param end_lsn		read area end */
void
RedoLog::Group::read(LogBuffer* log_buffer, lsn_t start_lsn, lsn_t end_lsn)
{
	byte*	buf = log_buffer->ptr();

	do {
		lsn_t	source_offset = lsn_offset(start_lsn);

		ut_ad(end_lsn - start_lsn <= ULINT_MAX);

		ulint	len = (ulint) (end_lsn - start_lsn);

		ut_ad(len != 0);

		if ((source_offset % m_file_size) + len > m_file_size) {

			/* If the above condition is true then len (which
			is ulint) is > the expression below, so the typecast
			is ok */

			ulint	offset = source_offset % m_file_size;

			len = m_file_size - offset;
		}

		MONITOR_INC(MONITOR_LOG_IO);

		ut_ad(source_offset / UNIV_PAGE_SIZE <= ULINT_MAX);

		fil_io(OS_FILE_READ | OS_FILE_LOG,
		       true,
		       m_space_id,
		       0,
		       (ulint) (source_offset / UNIV_PAGE_SIZE),
		       (ulint) (source_offset % UNIV_PAGE_SIZE),
		       len,
		       buf,
		       NULL);

		buf += len;
		start_lsn += len;

		ut_ad(start_lsn <= end_lsn);
		ut_ad(buf < log_buffer->ptr() + log_buffer->capacity());

	} while (start_lsn != end_lsn);
}

/**
Checks that there is enough free space in the log to start a new query step.
Flushes the log buffer or makes a new checkpoint if necessary. NOTE: this
function may only be called if the calling thread owns no synchronization
objects! */

void
RedoLog::check_margins()
{
	for (;;) {
		flush_margin();

		checkpoint_margin();

		mutex_acquire();

		ut_ad(is_write_allowed());

		if (m_check_flush_or_checkpoint) {

			mutex_release();
		} else {
			break;
		}
	}

	mutex_release();
}

/**
Peeks the current lsn.
@param lsn	if returns true, current lsn returned valid
@return	true if success, false if could not get the log system mutex */
bool
RedoLog::peek_lsn(lsn_t* lsn)
{
	if (mutex_enter_nowait(&m_mutex) == 0) {

		*lsn = m_lsn;

		mutex_release();

		return(true);
	}

	return(false);
}

/**
Prints info of the log.
@param file	Stream for output */
void
RedoLog::print(FILE* file)
{
	double	time_elapsed;
	time_t	current_time;

	mutex_enter(&m_mutex);

	fprintf(file,
		"Log sequence number " LSN_PF "\n"
		"Log flushed up to   " LSN_PF "\n"
		"Pages flushed up to " LSN_PF "\n"
		"Last checkpoint at  " LSN_PF "\n",
		m_lsn,
		m_flushed_to_disk_lsn,
		buf_pool_get_oldest_modification(),
		m_checkpoint->m_last_lsn);

	current_time = time(NULL);

	time_elapsed = difftime(current_time, m_last_printout_time);

	if (time_elapsed <= 0) {
		time_elapsed = 1;
	}

	fprintf(file,
		"%lu pending log writes, %lu pending chkp writes\n"
		"%lu log i/o's done, %.2f log i/o's/second\n",
		(ulong) m_n_pending_writes,
		(ulong) m_checkpoint->m_n_pending_writes,
		(ulong) m_n_log_ios,
		((double)(m_n_log_ios - m_n_log_ios_old)
		 / time_elapsed));

	m_n_log_ios_old = m_n_log_ios;
	m_last_printout_time = current_time;

	mutex_exit(&m_mutex);
}

/**
Refreshes the statistics used to print per-second averages. */
void
RedoLog::refresh_stats()
{
	m_last_printout_time = ut_time();
	m_n_log_ios_old = m_n_log_ios;
}

/**
Shutdown the sub-system */
void
RedoLog::shutdown()
{
	delete m_group;
	m_group = NULL;

	delete m_buf;
	m_buf = NULL;

	delete m_checkpoint;
	m_checkpoint = NULL;

	os_event_destroy(m_no_flush_event);
	os_event_destroy(m_one_flushed_event);

	mutex_free(&m_mutex);

	delete m_cmdq;
	m_cmdq = NULL;
}

#else /* !UNIV_HOTBACKUP */

/**
Writes info to a buffer of a log group when log files are created in
backup restoration.
@param hdr_buf		buffer which will be written to the start of the first
			log file
@param start		lsn of the start of the first log file; we pretend
			that there is a checkpoint at
			start + BLOCK_HDR_SIZE */
void
RedoLog::reset_first_header_and_checkpoint(byte* hdr_buf, ib_uint64_t start)
{
	byte*		buf;
	ib_uint64_t	lsn;
	ulint		fold;

	mach_write_to_4(hdr_buf + LOG_GROUP_ID, 0);
	mach_write_to_8(hdr_buf + LOG_FILE_START_LSN, start);

	lsn = start + BLOCK_HDR_SIZE;

	/* Write the label of ibbackup --restore */
	strcpy((char*) hdr_buf + FILE_WAS_CREATED_BY_HOT_BACKUP,
	       "ibbackup ");

	ut_sprintf_timestamp(
		(char*) hdr_buf
		+ (FILE_WAS_CREATED_BY_HOT_BACKUP
		   + (sizeof "ibbackup ") - 1));

	buf = hdr_buf + FIRST;

	mach_write_to_8(buf + CHECKPOINT_NO, 0);
	mach_write_to_8(buf + CHECKPOINT_LSN, lsn);

	mach_write_to_4(buf + CHECKPOINT_OFFSET_LOW32,
			LOG_FILE_HDR_SIZE + BLOCK_HDR_SIZE);

	mach_write_to_4(buf + CHECKPOINT_OFFSET_HIGH32, 0);

	mach_write_to_4(buf + CHECKPOINT_LOG_BUF_SIZE, 2 * 1024 * 1024);

	mach_write_to_8(buf + CHECKPOINT_ARCHIVED_LSN, LSN_MAX);

	fold = ut_fold_binary(buf, CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(buf + CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(buf + CHECKPOINT_LSN,
			      CHECKPOINT_CHECKSUM_2 - CHECKPOINT_LSN);

	mach_write_to_4(buf + CHECKPOINT_CHECKSUM_2, fold);

	/* Starting from InnoDB-3.23.50, we should also write info on
	allocated size in the tablespace, but unfortunately we do not
	know it here */
}
#endif /* !UNIV_HOTBACKUP */

/** Redo log recovery scan state */
struct RedoLog::Scan {

	Scan(bool store_to_hash, lsn_t start_lsn, lsn_t contiguous_lsn)
		:
		m_lsn(start_lsn),
	       	m_more_data(),
		m_store_to_hash(store_to_hash),
       		m_contiguous_lsn(contiguous_lsn) { }

	lsn_t		m_lsn;
	bool		m_more_data;
	bool		m_store_to_hash;
	lsn_t		m_contiguous_lsn;
};

/**
Check a block and verify that it is a valid block. Update scan state.
@return DB_SUCCESS or error code */
dberr_t
RedoLog::check_block(Scan& scan, const byte* block)
{
	ulint	no = IOBlock::get_hdr_no(block);

	if (!IOBlock::checksum_is_ok_or_old_format(block)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Log block no %lu at lsn " LSN_PF " "
			"checksum failed contains %lu, should be %lu",
			no,
			scan.m_lsn,
			IOBlock::get_checksum(block),
			IOBlock::calc_checksum(block));

		return(DB_END_OF_INDEX);

	} else if (no != convert_lsn_to_no(scan.m_lsn)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Log block number %lu in the header at "
			"lsn " LSN_PF "does not match, expected number "
			"to be %lu",
			no,
			scan.m_lsn,
			convert_lsn_to_no(scan.m_lsn));

		return(DB_END_OF_INDEX);

	} else if (IOBlock::is_flush_bit_set(block)) {
		/* This block was a start of a log flush operation:
		we know that the previous flush operation must have
		been completed for all log groups before this block
		can have been flushed to any of the groups. Therefore,
		we know that log data is contiguous up to scanned_lsn
		in all non-corrupt log groups. */

		if (scan.m_lsn > scan.m_contiguous_lsn) {
			scan.m_contiguous_lsn = scan.m_lsn;
		}
	}

	ulint	data_len = IOBlock::get_data_len(block);

	if ((scan.m_store_to_hash || data_len == IOBlock::SIZE)
	    && scan.m_lsn + data_len > m_recover->m_scanned_lsn
	    && m_recover->m_scanned_checkpoint_no > 0
	    && (IOBlock::get_checkpoint_no(block)
		< m_recover->m_scanned_checkpoint_no)
	    && (m_recover->m_scanned_checkpoint_no
		- IOBlock::get_checkpoint_no(block) > 0x80000000UL)) {

		/* Garbage from a log buffer flush which was made
		before the most recent database recovery */

		return(DB_END_OF_INDEX);
	}

	if (m_recover->m_parse_start_lsn == 0
	    && IOBlock::get_first_rec_group(block) > 0) {

		/* We found a point from which to start the parsing
		of log records */

		m_recover->m_parse_start_lsn = scan.m_lsn
			+ IOBlock::get_first_rec_group(block);

		m_recover->m_scanned_lsn = m_recover->m_parse_start_lsn;

		m_recover->m_recovered_lsn = m_recover->m_parse_start_lsn;
	}

	scan.m_lsn += data_len;

	if (scan.m_lsn > m_recover->m_scanned_lsn) {

		/* We have found more entries. If this scan is
		of startup type, we must initiate crash recovery
		environment before parsing these log records. */

#ifndef UNIV_HOTBACKUP
		if (m_recover->m_log_scan_is_startup_type
		    && !m_recover->m_needed_recovery) {

			if (!srv_read_only_mode) {
				ib_logf(IB_LOG_LEVEL_INFO,
					"Log scan progressed past the "
					"checkpoint lsn " LSN_PF "",
					m_recover->m_scanned_lsn);

				m_recover->init_crash_recovery();
			} else {

				ib_logf(IB_LOG_LEVEL_WARN,
					"Recovery skipped, "
					"--innodb-read-only set!");

				return(DB_NOT_FOUND);
			}
		}
#endif /* !UNIV_HOTBACKUP */

		/* We were able to find more log data: add it to the
		parsing buffer if parse_start_lsn is already
		non-zero */

		if (m_recover->m_len + 4 * IOBlock::SIZE 
		    == m_recover->s_parsing_buf_size) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Log parsing buffer overflow."
				" Recovery may have failed!");

			m_recover->m_found_corrupt_log = true;

#ifndef UNIV_HOTBACKUP
			if (!srv_force_recovery) {
				ib_logf(IB_LOG_LEVEL_FATAL,
					"Set innodb_force_recovery"
					" to ignore this error.");
			}
#endif /* !UNIV_HOTBACKUP */

		} else if (!m_recover->m_found_corrupt_log) {

			scan.m_more_data = m_recover->add_to_parsing_buf(
				block, scan.m_lsn);
		}

		m_recover->m_scanned_lsn = scan.m_lsn;

		m_recover->m_scanned_checkpoint_no
			= IOBlock::get_checkpoint_no(block);
	}

	return((data_len < IOBlock::SIZE) ? DB_END_OF_INDEX : DB_SUCCESS);
}

/**
Scans log from a buffer and stores new log data to the parsing buffer.
Parses and hashes the log records if new data found.  Unless
UNIV_HOTBACKUP is defined, this function will apply log records
automatically when the hash table becomes full.

@param available_memory	we let the hash table of recs to grow to this size, at
			the maximum
@param store_to_hash	true if the records should be stored to the hash
			table; this is set to false if just debug checking
			is needed
@param start_lsn	buffer start lsn
@param contiguous_lsn	it is known that all log groups contain contiguous
			log data up to this lsn

@return true if limit_lsn has been reached, or not able to scan any
more in this log group */
bool
RedoLog::scan_log_recs(
	ulint		available_memory,
	bool		store_to_hash,
	lsn_t		start_lsn,
	lsn_t*		contiguous_lsn,
	redo_recover_t*	recover)
{
	ut_ad(start_lsn % IOBlock::SIZE == 0);
	ut_ad(SCAN_SIZE % IOBlock::SIZE == 0);
	ut_ad(SCAN_SIZE >= IOBlock::SIZE);

	bool		finished = false;
	const byte*	block = m_buf->ptr();
	Scan		scan(store_to_hash, start_lsn, *contiguous_lsn);

	do {
		dberr_t	err = check_block(scan, block);

		if (err == DB_NOT_FOUND) {

			return(true);

		} else if (err == DB_END_OF_INDEX) {

			finished = true;
			break;
		}

		block += IOBlock::SIZE;

	} while (block < m_buf->ptr() + SCAN_SIZE && !finished);

	m_group->m_scanned_lsn = scan.m_lsn;
	*contiguous_lsn = scan.m_contiguous_lsn;

	if (m_recover->m_needed_recovery
	    || (m_recover->m_is_from_backup
		&& !m_recover->m_is_making_a_backup)) {

		if (finished || !(++m_print_counter % 80)) {

			ib_logf(IB_LOG_LEVEL_INFO,
				"Doing recovery: scanned up to"
				" log sequence number " LSN_PF "",
				m_group->m_scanned_lsn);
		}
	}

	if (scan.m_more_data && !m_recover->m_found_corrupt_log) {
		/* Try to parse more log records */

		m_recover->parse_log_recs(store_to_hash);

#ifndef UNIV_HOTBACKUP
		if (store_to_hash
		    && mem_heap_get_size(m_recover->m_heap)
		    > available_memory) {

			/* Hash table of log records has grown too big:
			empty it; no ibuf operations allowed, as we cannot
			add new records to the log yet: they would be
			produced by ibuf operations */

			m_ibuf_allowed = false;

			m_recover->apply_hashed_log_recs(m_ibuf_allowed);

			m_ibuf_allowed = true;
		}
#endif /* !UNIV_HOTBACKUP */

		if (m_recover->m_recovered_offset
		    > (m_recover->s_parsing_buf_size / 4)) {

			/* Move parsing buffer data to the buffer start */

			m_recover->justify_left_parsing_buf();
		}
	}

	return(finished);
}

#ifdef UNIV_HOTBACKUP
/**
Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned.
@param buf,		buffer containing log data
@param buf_len		data length in that buffer
@param scanned_lsn	lsn of buffer start, we return scanned lsn
@param scanned_checkpoint_no 4 lowest bytes of the highest scanned checkpoint
			number so fa
@param n_bytes_scanned	how much we were able to scan, smaller than buf_len if
			log data ended here */
void
RedoLog::scan_log_seg_for_backup(
	byte*		buf,
	ulint		buf_len,
	lsn_t*		scanned_lsn,
	ulint*		scanned_checkpoint_no,
	ulint*		n_bytes_scanned)
{
	*n_bytes_scanned = 0;

	for (const byte* log_block = buf;
	     log_block < buf + buf_len;
	     log_block += IOBlock::SIZE) {

		ulint	no = block_get_hdr_no(log_block);

		if (no != convert_lsn_to_no(*scanned_lsn)
		    || !IOBlock::checksum_is_ok_or_old_format(log_block)) {

			/* Garbage or an incompletely written log block */

			log_block += IOBlock::SIZE;
			break;
		}

		if (*scanned_checkpoint_no > 0
		    && m_buf->get_checkpoint_no(log_block)
		    < *scanned_checkpoint_no
		    && *scanned_checkpoint_no
		    - m_buf->get_checkpoint_no(log_block) > 0x80000000UL) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */
			break;
		}

		ulint	data_len;

		data_len = IOBlock::get_data_len(log_block);

		*scanned_checkpoint_no
			= m_buf->get_checkpoint_no(log_block);

		*scanned_lsn += data_len;

		*n_bytes_scanned += data_len;

		if (data_len < IOBlock::SIZE) {
			/* Log data ends here */

			break;
		}
	}
}
#else

/**
Completes recovery from a checkpoint. */
void
RedoLog::recovery_finish()
{
	/* Apply the hashed log records to the respective file pages */

	if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {

		m_recover->apply_hashed_log_recs(true);
	}

	m_recover->complete();

	redo_recover_t*	recover = m_recover;

	m_recover->m_redo = NULL;

	/* The redo writer threads checks this for this variable to be NULL. */
	m_recover = NULL;

	recover->finish();
}

/**
Free the log system data structures. */
void
RedoLog::release_resources()
{

}

/**
Copies a log segment from the most up-to-date log group to the
other log groups, so that they all contain the latest log data.
Also writes the info about the latest checkpoint to the groups,
and inits the fields in the group memory structs to up-to-date
values. */
void
RedoLog::synchronize_groups(lsn_t recovered_lsn)
{
	/* Read the last recovered log block to the recovery system buffer:
	the block is always incomplete */

	ulint	start_lsn = ut_uint64_align_down(recovered_lsn, IOBlock::SIZE);

	ulint	end_lsn = ut_uint64_align_up(recovered_lsn, IOBlock::SIZE);

	ut_ad(start_lsn != end_lsn);

	m_group->read(m_buf, start_lsn, end_lsn);

	/* Update the fields in the group struct to correspond to
	recovered_lsn */

	m_group->set_fields(recovered_lsn);

	/* Copy the checkpoint info to the groups; remember that we have
	incremented checkpoint_no by one, and the info will not be written
	over the max checkpoint info, thus making the preservation of max
	checkpoint info on disk certain */

	if (!srv_read_only_mode) {
		m_checkpoint->flush(this);
	}

	mutex_release();

	m_checkpoint->wait_for_write();

	mutex_acquire();
}

/**
Checks the consistency of the checkpoint info
@param buf	buffer containing checkpoint info
@return	true if ok */
bool
RedoLog::Checkpoint::is_consistent() const
{
	const byte*	ptr =  m_buf->ptr();

	ulint	fold1 = ut_fold_binary(ptr, CHECKSUM_1);
	ulint	checksum1 = mach_read_from_4(ptr + CHECKSUM_1);
	ulint	checksum2 = mach_read_from_4(ptr + CHECKSUM_2);

	ulint	fold2 = ut_fold_binary(ptr + LSN, CHECKSUM_2 - LSN);

	return((fold1 & 0xFFFFFFFFUL) == checksum1
	       && (fold2 & 0xFFFFFFFFUL) == checksum2);
}

/**
Read the checkpoint and validate it.
@param offset 		the checkpoint offset to read
@param space_id		space id to read from
@return true if a valid checkpoint found */
bool
RedoLog::Checkpoint::find(os_offset_t offset, ulint space_id)
{
	read(offset, space_id);

	return(is_consistent());
}

/**
Check and set the latest checkpoint
@param max_no		the number of the latest checkpoint
@return true if value was updated */
bool
RedoLog::Checkpoint::check_latest(ib_uint64_t& max_no) const
{
	ib_uint64_t	checkpoint_no = get_no();

	if (checkpoint_no >= max_no) {
		max_no = checkpoint_no;
		return(true);
	}

	return(false);
}

/**
Looks for the maximum consistent checkpoint from the log groups.
@param max_field		FIRST or SECOND 
@return	true if valid checkpoint found */
bool
RedoLog::Checkpoint::find_latest(os_offset_t& max_offset, ulint space_id)
{
	ib_uint64_t	max_no = 0;
	bool		found = false;

	max_offset = 0;

	if (find(FIRST, space_id)) {

		if (check_latest(max_no)) {
			max_offset = FIRST;
			found = true;
		}
	}

	if (find(SECOND, space_id)) {

		if (check_latest(max_no)) {
			max_offset = SECOND;
			found = true;
		}
	}

	return(found);
}

/**
Scans log from a buffer and stores new log data to the parsing buffer. Parses
and hashes the log records if new data found.
@param contiguous_lsn	it is known that all log groups contain contiguous log
			data up to this lsn
@param recover		The recovery manager */
void
RedoLog::group_scan_log_recs(lsn_t* contiguous_lsn, redo_recover_t*	recover)
{
	bool	finished = false;

	for (lsn_t lsn = *contiguous_lsn; !finished; lsn += SCAN_SIZE) {

		m_group->read(m_buf, lsn, lsn + SCAN_SIZE);

		finished = scan_log_recs(
			(buf_pool_get_n_pages()
			- (m_n_free_frames * srv_buf_pool_instances))
			* UNIV_PAGE_SIZE,
			true,
			lsn,
			contiguous_lsn,
			recover);
	}
}

/** Read the first log file header to print a note if this is
a recovery from a restored InnoDB Hot Backup. Rest if it is.
@return DB_SUCCESS or error code. */
dberr_t
RedoLog::check_ibbackup()
{
	byte	log_hdr_buf[LOG_FILE_HDR_SIZE];

	fil_io(OS_FILE_READ | OS_FILE_LOG,
	       true,
	       m_group->m_space_id,
	       0,
	       0,
	       0,
	       LOG_FILE_HDR_SIZE,
	       log_hdr_buf,
	       m_group);

	if (!memcmp(log_hdr_buf + FILE_WAS_CREATED_BY_HOT_BACKUP,
		    "ibbackup", (sizeof "ibbackup") - 1)) {

		if (srv_read_only_mode) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Cannot restore from ibbackup, InnoDB running "
				"in read-only mode!");

			return(DB_ERROR);
		}

		/* This log file was created by ibbackup --restore: print
		a note to the user about it */

		ib_logf(IB_LOG_LEVEL_INFO,
			"The log file was created by ibbackup --apply-log "
			"at %s. The following crash recovery is part of a "
			"normal restore.",
			log_hdr_buf + FILE_WAS_CREATED_BY_HOT_BACKUP);

		/* Wipe over the label now */

		memset(log_hdr_buf + FILE_WAS_CREATED_BY_HOT_BACKUP, ' ', 4);

		/* Write to the log file to wipe over the label */
		fil_io(OS_FILE_WRITE | OS_FILE_LOG,
		       true,
		       m_group->m_space_id,
		       0,
		       0,
		       0,
		       IOBlock::SIZE,
		       log_hdr_buf,
		       m_group);
	}

	return(DB_SUCCESS);
}

/**
Prepare for recovery, check the the flushed LSN.
@return DB_SUCCESS or error code */
dberr_t
RedoLog::prepare_for_recovery(lsn_t min_flushed_lsn, lsn_t max_flushed_lsn)
{
	if (m_checkpoint->get_lsn() != max_flushed_lsn
	    || m_checkpoint->get_lsn() != min_flushed_lsn) {


		if (m_checkpoint->get_lsn() < max_flushed_lsn) {

			ib_logf(IB_LOG_LEVEL_WARN,
				"The log sequence number "
				"in the ibdata files is higher "
				"than the log sequence number "
				"in the ib_logfiles! Are you sure "
				"you are using the right "
				"ib_logfiles to start up the database. "
				"Log sequence number in the "
				"ib_logfiles is " LSN_PF ", log"
				"sequence numbers stamped "
				"to ibdata file headers are between "
				"" LSN_PF " and " LSN_PF ".",
				m_checkpoint->get_lsn(),
				min_flushed_lsn,
				max_flushed_lsn);
		}

		if (!m_recover->m_needed_recovery) {
			ib_logf(IB_LOG_LEVEL_INFO,
				"The log sequence numbers "
				LSN_PF " and " LSN_PF
				" in ibdata files do not match"
				" the log sequence number "
				LSN_PF
				" in the ib_logfiles!",
				min_flushed_lsn,
				max_flushed_lsn,
				m_checkpoint->get_lsn());

			if (!srv_read_only_mode) {
				m_recover->init_crash_recovery();
			} else {

				ib_logf(IB_LOG_LEVEL_ERROR,
					"Can't initiate database "
					"recovery, running "
					"in read-only-mode.");

				return(DB_READ_ONLY);
			}
		}

		if (!m_recover->m_needed_recovery && !srv_read_only_mode) {
			/* Init the doublewrite buffer memory structure */
			buf_dblwr_init_or_restore_pages(FALSE);
		}
	}

	return(DB_SUCCESS);
}

/**
Recovers from a checkpoint. When this function returns, the database
is able to start processing of new user transactions, but the
function recovery_from_checkpoint_finish should be called later to
complete the recovery and free the resources used in it.
@param min_flushed_lsn	
@param max_flushed_lsn
@param recover		The recovery manager
@return	error code or DB_SUCCESS */
dberr_t
RedoLog::recovery_start(
	lsn_t		min_flushed_lsn,
	lsn_t		max_flushed_lsn,
	redo_recover_t* recover)
{
	mutex_acquire();

	os_offset_t	offset;

	m_group->m_state = Group::STATE_CORRUPTED;

	/* Look for the latest checkpoint from any of the log groups */
	if (!m_checkpoint->find_latest(offset, m_group->m_space_id)) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"No valid checkpoint found. If this error appears "
			"when you are creating an InnoDB database, the "
			"problem may be that during an earlier attempt you "
			"managed to create the InnoDB data files, but log "
			"file creation failed. If that is the case, please "
			"refer to " REFMAN "error-creating-innodb.html");

		mutex_release();

		return(DB_ERROR);
	}

	m_checkpoint->read(offset, m_group->m_space_id);

	m_group->m_state = Group::STATE_OK;

	m_group->m_lsn = m_checkpoint->get_lsn();

	m_group->m_lsn_offset = m_checkpoint->get_lsn_offset();

	++m_n_log_ios;

	MONITOR_INC(MONITOR_LOG_IO);

	dberr_t	err = check_ibbackup();

	if (err != DB_SUCCESS) {
		return(err);
	}

	m_recover = recover;
	m_recover->m_redo = this;

	/* Start reading the log groups from the checkpoint lsn up. The
	variable contiguous_lsn contains an lsn up to which the log is
	known to be contiguously written to all log groups. */

	m_recover->start(m_checkpoint->get_lsn());

	srv_start_lsn = m_checkpoint->get_lsn();

	lsn_t	contiguous_lsn = ut_uint64_align_down(
		m_checkpoint->get_lsn(), IOBlock::SIZE);

	ut_ad(SCAN_SIZE <= m_buf->capacity());

	// FIXME
	/* Set the flag to publish that we are doing startup scan. */
	m_recover->m_log_scan_is_startup_type = true;

	group_scan_log_recs(&contiguous_lsn, recover);

	/* Done with startup scan. Clear the flag. */
	m_recover->m_log_scan_is_startup_type = false;

	/* NOTE: we always do a 'recovery' at startup, but only if
	there is something wrong we will print a message to the
	user about recovery: */

	err = prepare_for_recovery(min_flushed_lsn, max_flushed_lsn);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* We currently have only one log group */
	if (m_group->m_scanned_lsn < m_checkpoint->get_lsn()
	    || m_group->m_scanned_lsn < m_recover->m_max_page_lsn) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			"We scanned the log up to " LSN_PF ". A checkpoint "
			"was at " LSN_PF " and the maximum LSN on a database "
			"page was " LSN_PF ". It is possible that the database "
			"is now corrupt!",
			m_group->m_scanned_lsn,
			m_checkpoint->get_lsn(),
			m_recover->m_max_page_lsn);
	}

	if (m_recover->m_recovered_lsn < m_checkpoint->get_lsn()) {

		mutex_release();

		if (m_recover->m_recovered_lsn >= LSN_MAX) {

			return(DB_SUCCESS);
		}

		/* No harm in trying to do RO access. */
		if (!srv_read_only_mode) {
			ut_error;
		}

		return(DB_ERROR);
	}

	/* Synchronize the uncorrupted log groups to the most up-to-date log
	group; we also copy checkpoint info to groups */

	m_checkpoint->m_next_lsn = m_checkpoint->get_lsn();
	m_checkpoint->m_next_no = m_checkpoint->get_no() + 1;

	synchronize_groups(m_recover->m_recovered_lsn);

	if (!m_recover->m_needed_recovery) {
		ut_ad(m_checkpoint->get_lsn() == m_recover->m_recovered_lsn);
	} else {
		srv_start_lsn = m_recover->m_recovered_lsn;
	}

	m_lsn = m_recover->m_recovered_lsn;

	m_buf->init(ulint(m_lsn) % IOBlock::SIZE);

	m_written_to_all_lsn = m_lsn;

	m_written_to_some_lsn = m_lsn;

	m_checkpoint->m_last_lsn = m_checkpoint->get_lsn();

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE, m_checkpoint->age(m_lsn));

	m_recover->set_state_apply();

	mutex_release();

	/* The database is now ready to start almost normal processing of user
	transactions: transaction rollbacks and the application of the log
	records in the hash table can be run in background. */

	return(DB_SUCCESS);
}

/**
Resets the logs. The contents of log files will be lost!
@param size		Size of the log file, 0 if it shouldn't be
			changed
@param lsn 		reset to this lsn rounded up to be divisible by
			IOBlock::SIZE, after which we add
			BLOCK_HDR_SIZE */
void
RedoLog::reset_logs(os_offset_t size, lsn_t lsn)
{
	if (size > 0) {
		m_file_size = size;
	}

	if (m_group != NULL) {
		delete m_group;
		m_group = NULL;
	}

	init();

	mutex_acquire();

	ut_d(enable_log_write());

	m_lsn = ut_uint64_align_up(lsn, IOBlock::SIZE);

	m_group->m_lsn = m_lsn;
	m_group->m_lsn_offset = LOG_FILE_HDR_SIZE;

	m_written_to_all_lsn = m_lsn;
	m_written_to_some_lsn = m_lsn;

	m_checkpoint->reset();

	m_buf->reset();

	m_buf->init_v2(convert_lsn_to_no(m_lsn));

	m_buf->set_first_rec_group(BLOCK_HDR_SIZE);

	m_lsn += BLOCK_HDR_SIZE;

	MONITOR_SET(MONITOR_LSN_CHECKPOINT_AGE, m_checkpoint->age(m_lsn));

	mutex_release();

	/* Reset the checkpoint fields in logs */

	checkpoint_at(LSN_MAX, true);
}
#endif /* UNIV_HOTBACKUP */

/**
@return true if there are pending reads or writes. */
bool
RedoLog::is_busy() const
{
	mutex_acquire();

	bool	busy = m_checkpoint->m_n_pending_writes || m_n_pending_writes;

	mutex_release();

	return(busy);
}

/**
Print busy status. */
void
RedoLog::print_busy_status()
{
	ib_logf(IB_LOG_LEVEL_INFO,
		"Pending checkpoint_writes: %lu. "
		"Pending log flush writes: %lu",
		m_checkpoint->m_n_pending_writes, m_n_pending_writes);
}

/**
Makes a checkpoint at the latest lsn and writes it to first page of each
data file in the database, so that we know that the file spaces contain
all modifications up to that lsn.
@param notify		if true then write notifications to the error log
@return current state */
RedoLog::state_t
RedoLog::start_shutdown(bool notify)
{
	switch (is_busy() ? STATE_RUNNING : m_state) {
	case STATE_RUNNING:

		if (is_busy()) {

			if (notify) {
				ib_logf(IB_LOG_LEVEL_INFO,
					"Pending checkpoint_writes: %lu. "
					"Pending log flush writes: %lu",
					m_checkpoint->m_n_pending_writes,
					m_n_pending_writes);
			}

			m_state = STATE_RUNNING;

		} else {
			m_state = STATE_CHECKPOINT;
		}
	
	case STATE_CHECKPOINT:

		ut_ad(!is_busy());

		if (!srv_read_only_mode) {
			checkpoint_at(LSN_MAX, true);
		}

		mutex_acquire();

		if (m_checkpoint->age(m_lsn) == 0) {
			m_state = STATE_FINISHED;
		}

		mutex_release();
		break;

	case STATE_FINISHED:

		ut_ad(m_checkpoint->age(m_lsn) == 0);

		if (m_lsn < srv_start_lsn) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Log sequence number at shutdown " LSN_PF " "
				"is lower than at startup " LSN_PF "!",
				m_lsn, srv_start_lsn);
		}

		srv_shutdown_lsn = m_lsn;

		m_state = STATE_SHUTDOWN;
		break;	

	case STATE_SHUTDOWN:
		ut_ad(!is_busy());
		ut_ad(m_lsn == srv_shutdown_lsn);
		ut_ad(m_checkpoint->age(m_lsn) == 0);
		break;
	}

	return(m_state);
}

void
RedoLog::handle_truncate()
{
	if (is_recovery_on()) {
		m_recover->handle_truncate();
	}
}

#ifdef UNIV_DEBUG
/**
@return the last checkpoint LSN */
lsn_t
RedoLog::checkpoint_lsn() const
{
	return(m_checkpoint->m_last_lsn);
}
#endif /* UNIV_DEBUG */

/**
Gets a log block data length.
@param block		log block
@return	log block data length measured as a byte offset from the
	block start */
ulint
RedoLog::block_get_data_len(const byte* block)
{
	return(IOBlock::get_data_len(block));
}

/**
Initializes a log block in the log buffer in the old, < 3.23.52
format, where there was no checksum yet.
@param block		pointer to the log buffer
@param lsn		lsn within the log block */
void
RedoLog::block_init_v1(byte* block, lsn_t lsn)
{
	IOBlock::init_v1(block, convert_lsn_to_no(lsn));
}

/**
Default constructor */
RedoLog::Command::Command()
{
	/* Do nothing */
}

/**
Destructor */
RedoLog::Command::~Command()
{
	/* Do nothing */
}

/**
The redo log command queue */
void
RedoLog::submit(Command* cmd)
{
	cmd->execute(this);
}


/**
Redo log writer thread.
@param	arg		a dummy parameter required by os_thread_create
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(log_writer_thread)(
	void*	arg __attribute__((unused)))
{
	ut_ad(!srv_read_only_mode);

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(log_writer_thread_key);
#endif /* UNIV_PFS_THREAD */

	ib_logf(IB_LOG_LEVEL_INFO, "Log writer thread started, id %lu",
		os_thread_pf(os_thread_get_curr_id()));

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {

		sleep(1);

	}

	ut_ad(srv_shutdown_state > 0);

	ib_logf(IB_LOG_LEVEL_INFO, "Log writer thread exit, id %lu",
		os_thread_pf(os_thread_get_curr_id()));

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */
	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}
