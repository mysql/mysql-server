/*****************************************************************************

Copyright (c) 1997, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/log0recv.h
Recovery

Created 9/20/1997 Heikki Tuuri
*******************************************************/

#ifndef log0recv_h
#define log0recv_h

#include "univ.i"
#include "ut0byte.h"
#include "buf0types.h"
#include "hash0hash.h"
#include "log0log.h"
#include "mtr0log.h"

/** Block of log record data */
struct recv_data_t {
	/** pointer to the next block or NULL the log record data is
	stored physically immediately after this struct, max amount
	RECV_DATA_BLOCK_SIZE bytes of it */

	recv_data_t*	next;
};

/** Stored log record struct */
struct recv_t {
	/** log record type */
	mlog_id_t	type;

	/** log record body length in bytes */
	ulint		len;

	/** chain of blocks containing the log record body */
	recv_data_t*	data;

	/** start lsn of the log segment written by the mtr which generated
	this log record: NOTE that this is not necessarily the start lsn of
	this log record */
	lsn_t		start_lsn;

	/** End lsn of the log segment written by the mtr which generated
	this log record: NOTE that this is not necessarily the end lsn of
	this log record */
	lsn_t		end_lsn;

	/** List of log records for this page */
	UT_LIST_NODE_T(recv_t) rec_list;
};

/** States of recv_addr_t */
enum recv_addr_state {
	/** not yet processed */
	RECV_NOT_PROCESSED,

	/** page is being read */
	RECV_BEING_READ,

	/** log records are being applied on the page */
	RECV_BEING_PROCESSED,

	/** log records have been applied on the page, or they have
	been discarded because the tablespace does not exist */
	RECV_PROCESSED
};

/** Hashed page file address struct */
struct recv_addr_t {
 	/** recovery state of the page */
	recv_addr_state state;

	/** space id */
	ib_uint32_t	space;
	/** page number */
	ib_uint32_t	page_no;

	/** list of log records for this page */
	UT_LIST_BASE_NODE_T(recv_t) rec_list;

	/** hash node in the hash bucket chain */
	hash_node_t	addr_hash;
};

struct redo_recover_t {
	// FIXME
	redo_recover_t() : m_inited() { }
	~redo_recover_t() {}

#ifdef UNIV_HOTBACKUP
	/**
	Reads the checkpoint info needed in hot backup.
	@param hdr		buffer containing the log group header
	@param lsn		checkpoint lsn
	@param offse		checkpoint offset in the log group
	@param cp_no,		checkpoint number
	@param first_header_lsn lsn of of the start of the first log file
	@return	true if success */
	bool read_checkpoint_info_for_backup(
		const byte*	hdr,
		lsn_t*		lsn,
		lsn_t*		offset,
		lsn_t*		cp_no,
		lsn_t*		first_header_lsn);

#endif /* UNIV_HOTBACKUP */

	/**
	Applies the hashed log records to the page, if the page lsn is
	less than the lsn of a log record. This can be called when a
	buffer page has just been read in, or also for a page already
	in the buffer pool.

	@param just_read_in	true if the i/o handler calls this for
				a freshly read page
	@param block		buffer block */
	void recover_page(
#ifndef UNIV_HOTBACKUP
		bool		just_read_in,
#endif /* !UNIV_HOTBACKUP */
		buf_block_t*	block);

	/**
	Completes recovery from a checkpoint. */
	void complete();

	/**
	Creates the recovery system.
	@param mem_available	buffer pool memory available in bytes
	@return DB_FAIL if recovery should be skipped */
	dberr_t create(ulint mem_available);

	/**
	Release recovery system mutexes. */
	void destroy();

	/**
	Frees the recovery system memory. */
	void release_resources();

	/**
	Reset the state of the recovery system variables. */
	void var_init();

	/**
	Empties the hash table of stored log records, applying them
	to appropriate pages

	@param allow_ibuf	if true, also ibuf operations are
				allowed during the application; if FALSE,
				no ibuf operations are allowed, and after
				the application all file pages are flushed to
				disk and invalidated in buffer pool: this
				alternative means that no new log records
				can be generated during the application */
	void apply_hashed_log_recs(bool allow_ibuf);

	/**
	Parses log records from a buffer and stores them to a hash table to wait
	merging to file pages.

	@param store_to_hash	true if the records should be stored to the
				hash table; this is set to false if just debug
				checking is needed

	@return	currently always returns false */
	bool parse_log_recs(bool store_to_hash);

	/**
	Adds data from a new log block to the parsing buffer if
	redo_recover_t::sys->parse_start_lsn is non-zero.

	@param log_block	log block
	@param scanned_lsn	lsn of how far we were able to find data in
				this log block

	@return	true if more data added */
	bool add_to_parsing_buf(const byte* log_block, lsn_t scanned_lsn);

	/**
	Moves the parsing buffer data left to the buffer start. */
	void justify_left_parsing_buf();

	/**
	Initialize crash recovery environment. Can be called iff
	redo_recover_t::needed_recovery == false. */
	void init_crash_recovery();

	/**
	Initiates the rollback of active transactions. */
	void recovery_rollback_active();

	/** Start the recovery process */
	void start(lsn_t checkpoint_lsn)
	{
		m_scanned_checkpoint_no = 0;
		m_scanned_lsn = checkpoint_lsn;
		m_recovered_lsn = checkpoint_lsn;
		m_parse_start_lsn = checkpoint_lsn;
	}

	/** Set the recovery state */
	void set_state_apply()
	{
		mutex_enter(&m_mutex);

		m_lsn_checks_on = true;
		m_apply_log_recs = true;

		mutex_exit(&m_mutex);
	}

	void handle_truncate()
	{
		mutex_enter(&m_mutex);
		ut_ad(m_n_addrs > 0);
		--m_n_addrs;
		mutex_exit(&m_mutex);
	}

	/** @return if lsn checks are on */
	bool is_lsn_check_on() const
	{
		return(m_lsn_checks_on);
	}

	bool requires_recovery() const
	{
		return(m_needed_recovery);
	}

	void set_log_corrupt()
	{
		m_found_corrupt_log = true;
	}

	bool is_log_corrupt() const
	{
		return(m_found_corrupt_log);
	}

	void writer_thread_started()
	{
		m_writer_thread_active = true;
	}

	void writer_thread_exit()
	{
		m_writer_thread_active = false;
	}

	/**
	Called at end of recovery for the recovery manager to wrap up */
	void finish();

private:
	/**
	Inits the recovery system for a recovery operation.
	@param n_bytes		memory available in bytes */
	void init(ulint n_bytes);

	/**
	Empties the hash table when it has been fully processed. */
	void empty_hash();

#ifndef UNIV_HOTBACKUP
	/**
	Frees the recovery system. */
	void debug_free();

	/**
	Checks the consistency of the checkpoint info
	@param buf	buffer containing checkpoint info
	@return	true if ok */
	bool check_cp_is_consistent(const byte* buf);

	/**
	Gets the hashed file address struct for a page.
	@param space		space id
	@param page_no		page number
	@return	file address struct, NULL if not found from the hash table */
	recv_addr_t* get_fil_addr_struct(ulint space, ulint page_no);

	/**
	Tries to parse a single log record body and also applies it to a page if
	specified. File ops are parsed, but not applied in this function.

	@param type		type
	@param ptr		pointer to a buffer
	@param end_ptr		pointer to the buffer end
	@param block		buffer block or NULL; if not NULL, then the
				log record is applied to the page, and the log
				record should be complete then
	@param 	mtr		mtr or NULL; should be non-NULL if and only
				if block is non-NULL
	@param space_id		tablespace id obtained by parsing initial log
				record

	@return	log record end, NULL if not a complete record */
	byte* parse_or_apply_log_rec_body(
		mlog_id_t	type,
		byte*		ptr,
		byte*		end_ptr,
		buf_block_t*	block,
		mtr_t*		mtr,
		ulint		space_id,
		ulint		page_no);

	/**
	Calculates the fold value of a page file address: used in inserting or
	searching for a log record in the hash table.

	@param space		space id
	@param page_no		page number

	@return	folded value */
	static ulint fold(ulint space, ulint page_no)
	{
		return(ut_fold_ulint_pair(space, page_no));
	}

	/**
	Calculates the hash value of a page file address: used in inserting or
	searching for a log record in the hash table.

	@param space		space id
	@param page_no		page number

	@return	folded value */
	ulint hash(ulint space, ulint page_no)
	{
		return(hash_calc_hash(fold(space, page_no), m_addr_hash));
	}

	/**
	Tries to parse a single log record and returns its length.

	@param ptr		pointer to a buffer
	@param end_ptr		pointer to the buffer end
	@param type		type
	@param space		space id
	@param page_no		page number
	@param body		log record body start

	@return	length of the record, or 0 if the record was not complete */
	ulint parse_log_rec(
		byte*		ptr,
		byte*		end_ptr,
		mlog_id_t*	type,
		ulint*		space,
		ulint*		page_no,
		byte**		body);

#endif /* !UNIV_HOTBACKUP */

	/**
	Copies the log record body from recv to buf.
	@param buf	buffer of length at least recv->len
	@param recv	log record */
	static void data_copy_to_buf(byte* buf, recv_t* recv);

	/**
	Adds a new log record to the hash table of log records.
	@param type		log record type
	@param space		space id
	@param page_no		page number
	@param body		log record body
	@param rec_end		log record end
	@param start_lsn	start lsn of the mtr
	@param end_lsn		end lsn of the mtr */
	void add_to_hash_table(
		mlog_id_t	type,
		ulint		space,
		ulint		page_no,
		byte*		body,
		byte*		rec_end,
		lsn_t		start_lsn,
		lsn_t		end_lsn);

	/**
	Reads in pages which have hashed log records, from an area around
	a given page number.

	@param space		space
	@param zip_size		compressed page size in bytes, or 0
	@param page_no		page number

	@return	number of pages found */
	ulint read_in_area(ulint space, ulint zip_size, ulint page_no);

	/**
	Calculates the new value for lsn when more data is added to the log.
	@param lsn		old lsn
	@param len		this many bytes of data is added, log block
				headers not included */
	lsn_t calc_lsn_on_data_add(lsn_t lsn, ib_uint64_t len);

	/**
	Prints diagnostic info of corrupt log.
	@param ptr		pointer to corrupt log record
	@param type,		type of the record
	@param space		space id, this may also be garbage
	@param page_no		page number, this may also be garbage */
	void report_corrupt_log(
		byte*		ptr,
		mlog_id_t	type,
		ulint		space,
		ulint		page_no);
public:
	// FIXME: Used by the recovery flush thread
	/** mutex coordinating flushing between recv_writer_thread and the
	recovery thread. */
	ib_mutex_t	m_writer_mutex;

private:
	// FIXME: They shouldn't be public
	/** true if buf_page_is_corrupted() should check if the log sequence
	number (FIL_PAGE_LSN) is in the future.  Initially false, and set by
	redo_recover_t::recovery_from_checkpoint_start_func(). */
	bool		m_lsn_checks_on;

	/** The maximum lsn we see for a page during the recovery process. If
	this is bigger than the lsn we are able to scan up to, that is an
	indication that the recovery failed and the database may be corrupt. */
	lsn_t		m_max_page_lsn;

	/** true when recv_init_crash_recovery() has been called. */
	bool		m_needed_recovery;

	/** There are two conditions under which we scan the logs, the first
	is normal startup and the second is when we do a recovery from an
	archive.

	This flag is set if we are doing a scan from the last checkpoint during
	startup. If we find log entries that were written after the last
	checkpoint we know that the server was not cleanly shutdown. We must
	then initialize the crash recovery environment before attempting to
	store these entries in the log hash table. */
	bool		m_log_scan_is_startup_type;

	/** true when the redo log is being backed up */
	bool		m_is_making_a_backup;

	bool		m_is_from_backup;

 	/** the log records have been parsed up to this lsn */
	lsn_t		m_recovered_lsn;

	// FIXME: Used by mtr0log.cc
	/** This is set to true if we during log scan find a corrupt
	log block, or a corrupt log record, or there is a log parsing
	buffer overflow */
	bool		m_found_corrupt_log;

private:
	/** mutex protecting the fields apply_log_recs, n_addrs, and the
	state field in each recv_addr struct */
	ib_mutex_t	m_mutex;

 	/** this is true when log rec application to pages is allowed;
	this flag tells the i/o-handler if it should do log record
	application */
	bool		m_apply_log_recs;

 	/** this is TRUE when a log rec application batch is running */
	bool		m_apply_batch_on;

	/** log sequence number */
	lsn_t		m_lsn;

 	/** Size of the log buffer when the database last time wrote
	to the log */
	ulint		m_last_log_buf_size;

	/** buffer for parsing log records */
	byte*		m_buf;

	/** amount of data in buf */
	ulint		m_len;

	/** this is the lsn from which we were able to start parsing
	log records and adding them to the hash table; zero if a suitable
	start point not found yet */
	lsn_t		m_parse_start_lsn;

 	/** the log data has been scanned up to this lsn */
	lsn_t		m_scanned_lsn;

 	/** the log data has been scanned up to this checkpoint number
	(lowest 4 bytes) */
	ulint		m_scanned_checkpoint_no;

 	/** start offset of non-parsed log records in buf */
	ulint		m_recovered_offset;

	/** recovery should be made at most up to this lsn */
	lsn_t		m_limit_lsn;

	/** Memory heap of log records and file addresses*/
	mem_heap_t*	m_heap;

	/** hash table of file addresses of pages */
	hash_table_t*	m_addr_hash;

	/** number of not processed hashed file addresses in the hash table */
	ulint		m_n_addrs;

	bool		m_inited;

	/** The redo log manager */
	redo_log_t*	m_redo;

	/** The type of the previous parsed redo log record */
	ulint		m_previous_parsed_rec_type;

	/** The offset of the previous parsed redo log record */
	ulint		m_previous_parsed_rec_offset;

	/** The 'multi' flag of the previous parsed redo log record */
	ulint		m_previous_parsed_rec_is_multi;

	/** Maximum page number encountered in the redo log */
	ulint		m_max_parsed_page_no;

	/** Flag indicating if recv_writer thread is active. */
	bool		m_writer_thread_active;

	static const ulint	s_parsing_buf_size;

	// FIXME: Temporary hack
	friend class redo_log_t;
};

extern redo_recover_t*	recover_ptr;

#endif /* log0recv_h */
