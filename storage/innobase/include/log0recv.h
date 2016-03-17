/*****************************************************************************

Copyright (c) 1997, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
#include "mtr0types.h"
#include "ut0new.h"

#include <list>
#include <vector>

#ifdef UNIV_HOTBACKUP
extern bool	recv_replay_file_ops;

/*******************************************************************//**
Reads the checkpoint info needed in hot backup.
@return TRUE if success */
ibool
recv_read_checkpoint_info_for_backup(
/*=================================*/
	const byte*	hdr,	/*!< in: buffer containing the log group
				header */
	lsn_t*		lsn,	/*!< out: checkpoint lsn */
	lsn_t*		offset,	/*!< out: checkpoint offset in the log group */
	lsn_t*		cp_no,	/*!< out: checkpoint number */
	lsn_t*		first_header_lsn)
				/*!< out: lsn of of the start of the
				first log file */
	MY_ATTRIBUTE((nonnull));
/*******************************************************************//**
Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned. */
void
recv_scan_log_seg_for_backup(
/*=========================*/
	byte*		buf,		/*!< in: buffer containing log data */
	ulint		buf_len,	/*!< in: data length in that buffer */
	lsn_t*		scanned_lsn,	/*!< in/out: lsn of buffer start,
					we return scanned lsn */
	ulint*		scanned_checkpoint_no,
					/*!< in/out: 4 lowest bytes of the
					highest scanned checkpoint number so
					far */
	ulint*		n_bytes_scanned);/*!< out: how much we were able to
					scan, smaller than buf_len if log
					data ended here */
#endif /* UNIV_HOTBACKUP */
/*******************************************************************//**
Returns TRUE if recovery is currently running.
@return recv_recovery_on */
UNIV_INLINE
bool
recv_recovery_is_on(void);
/*=====================*/
/************************************************************************//**
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool. */
void
recv_recover_page_func(
/*===================*/
#ifndef UNIV_HOTBACKUP
	ibool		just_read_in,
				/*!< in: TRUE if the i/o handler calls
				this for a freshly read page */
#endif /* !UNIV_HOTBACKUP */
	buf_block_t*	block);	/*!< in/out: buffer block */
#ifndef UNIV_HOTBACKUP
/** Wrapper for recv_recover_page_func().
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.
@param jri in: TRUE if just read in (the i/o handler calls this for
a freshly read page)
@param block in/out: the buffer block
*/
# define recv_recover_page(jri, block)	recv_recover_page_func(jri, block)
#else /* !UNIV_HOTBACKUP */
/** Wrapper for recv_recover_page_func().
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool.
@param jri in: TRUE if just read in (the i/o handler calls this for
a freshly read page)
@param block in/out: the buffer block
*/
# define recv_recover_page(jri, block)	recv_recover_page_func(block)
#endif /* !UNIV_HOTBACKUP */
/** Start recovering from a redo log checkpoint.
@see recv_recovery_from_checkpoint_finish
@param[in]	flush_lsn	FIL_PAGE_FILE_FLUSH_LSN
of first system tablespace page
@return error code or DB_SUCCESS */
dberr_t
recv_recovery_from_checkpoint_start(
	lsn_t	flush_lsn);
/** Complete recovery from a checkpoint. */
void
recv_recovery_from_checkpoint_finish(void);
/********************************************************//**
Initiates the rollback of active transactions. */
void
recv_recovery_rollback_active(void);
/*===============================*/
/******************************************************//**
Resets the logs. The contents of log files will be lost! */
void
recv_reset_logs(
/*============*/
	lsn_t		lsn);		/*!< in: reset to this lsn
					rounded up to be divisible by
					OS_FILE_LOG_BLOCK_SIZE, after
					which we add
					LOG_BLOCK_HDR_SIZE */
#ifdef UNIV_HOTBACKUP
/******************************************************//**
Creates new log files after a backup has been restored. */
void
recv_reset_log_files_for_backup(
/*============================*/
	const char*	log_dir,	/*!< in: log file directory path */
	ulint		n_log_files,	/*!< in: number of log files */
	lsn_t		log_file_size,	/*!< in: log file size */
	lsn_t		lsn);		/*!< in: new start lsn, must be
					divisible by OS_FILE_LOG_BLOCK_SIZE */
#endif /* UNIV_HOTBACKUP */
/********************************************************//**
Creates the recovery system. */
void
recv_sys_create(void);
/*=================*/
/**********************************************************//**
Release recovery system mutexes. */
void
recv_sys_close(void);
/*================*/
/********************************************************//**
Frees the recovery system memory. */
void
recv_sys_mem_free(void);
/*===================*/
/********************************************************//**
Inits the recovery system for a recovery operation. */
void
recv_sys_init(
/*==========*/
	ulint	available_memory);	/*!< in: available memory in bytes */
#ifndef UNIV_HOTBACKUP
/********************************************************//**
Frees the recovery system. */
void
recv_sys_debug_free(void);
/*=====================*/
/********************************************************//**
Reset the state of the recovery system variables. */
void
recv_sys_var_init(void);
/*===================*/
#endif /* !UNIV_HOTBACKUP */
/*******************************************************************//**
Empties the hash table of stored log records, applying them to appropriate
pages. */
void
recv_apply_hashed_log_recs(
/*=======================*/
	ibool	allow_ibuf);	/*!< in: if TRUE, also ibuf operations are
				allowed during the application; if FALSE,
				no ibuf operations are allowed, and after
				the application all file pages are flushed to
				disk and invalidated in buffer pool: this
				alternative means that no new log records
				can be generated during the application */
#ifdef UNIV_HOTBACKUP
/*******************************************************************//**
Applies log records in the hash table to a backup. */
void
recv_apply_log_recs_for_backup(void);
/*================================*/
#endif /* UNIV_HOTBACKUP */

/** Block of log record data */
struct recv_data_t{
	recv_data_t*	next;	/*!< pointer to the next block or NULL */
				/*!< the log record data is stored physically
				immediately after this struct, max amount
				RECV_DATA_BLOCK_SIZE bytes of it */
};

/** Stored log record struct */
struct recv_t{
	mlog_id_t	type;	/*!< log record type */
	ulint		len;	/*!< log record body length in bytes */
	recv_data_t*	data;	/*!< chain of blocks containing the log record
				body */
	lsn_t		start_lsn;/*!< start lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the start lsn of
				this log record */
	lsn_t		end_lsn;/*!< end lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the end lsn of
				this log record */
	UT_LIST_NODE_T(recv_t)
			rec_list;/*!< list of log records for this page */
};

/** States of recv_addr_t */
enum recv_addr_state {
	/** not yet processed */
	RECV_NOT_PROCESSED,
	/** page is being read */
	RECV_BEING_READ,
	/** log records are being applied on the page */
	RECV_BEING_PROCESSED,
	/** log records have been applied on the page */
	RECV_PROCESSED,
	/** log records have been discarded because the tablespace
	does not exist */
	RECV_DISCARDED
};

/** Hashed page file address struct */
struct recv_addr_t{
	enum recv_addr_state state;
				/*!< recovery state of the page */
	unsigned	space:32;/*!< space id */
	unsigned	page_no:32;/*!< page number */
	UT_LIST_BASE_NODE_T(recv_t)
			rec_list;/*!< list of log records for this page */
	hash_node_t	addr_hash;/*!< hash node in the hash bucket chain */
};

struct recv_dblwr_t {
	/** Add a page frame to the doublewrite recovery buffer. */
	void add(const byte* page) {
		pages.push_back(page);
	}

	/** Find a doublewrite copy of a page.
	@param[in]	space_id	tablespace identifier
	@param[in]	page_no		page number
	@return	page frame
	@retval NULL if no page was found */
	const byte* find_page(ulint space_id, ulint page_no);

	typedef std::list<const byte*, ut_allocator<const byte*> >	list;

	/** Recovered doublewrite buffer page frames */
	list	pages;
};

/* Recovery encryption information */
typedef	struct recv_encryption {
	ulint		space_id;	/*!< the page number */
	byte*		key;		/*!< encryption key */
	byte*		iv;		/*!< encryption iv */
} recv_encryption_t;

typedef std::vector<recv_encryption_t, ut_allocator<recv_encryption_t> >
		encryption_list_t;

/** Recovery system data structure */
struct recv_sys_t{
#ifndef UNIV_HOTBACKUP
	ib_mutex_t		mutex;	/*!< mutex protecting the fields apply_log_recs,
				n_addrs, and the state field in each recv_addr
				struct */
	ib_mutex_t		writer_mutex;/*!< mutex coordinating
				flushing between recv_writer_thread and
				the recovery thread. */
	os_event_t		flush_start;/*!< event to acticate
				page cleaner threads */
	os_event_t		flush_end;/*!< event to signal that the page
				cleaner has finished the request */
	buf_flush_t		flush_type;/*!< type of the flush request.
				BUF_FLUSH_LRU: flush end of LRU, keeping free blocks.
				BUF_FLUSH_LIST: flush all of blocks. */
#endif /* !UNIV_HOTBACKUP */
	ibool		apply_log_recs;
				/*!< this is TRUE when log rec application to
				pages is allowed; this flag tells the
				i/o-handler if it should do log record
				application */
	ibool		apply_batch_on;
				/*!< this is TRUE when a log rec application
				batch is running */
	byte*		last_block;
				/*!< possible incomplete last recovered log
				block */
	byte*		last_block_buf_start;
				/*!< the nonaligned start address of the
				preceding buffer */
	byte*		buf;	/*!< buffer for parsing log records */
	ulint		len;	/*!< amount of data in buf */
	lsn_t		parse_start_lsn;
				/*!< this is the lsn from which we were able to
				start parsing log records and adding them to
				the hash table; zero if a suitable
				start point not found yet */
	lsn_t		scanned_lsn;
				/*!< the log data has been scanned up to this
				lsn */
	ulint		scanned_checkpoint_no;
				/*!< the log data has been scanned up to this
				checkpoint number (lowest 4 bytes) */
	ulint		recovered_offset;
				/*!< start offset of non-parsed log records in
				buf */
	lsn_t		recovered_lsn;
				/*!< the log records have been parsed up to
				this lsn */
	bool		found_corrupt_log;
				/*!< set when finding a corrupt log
				block or record, or there is a log
				parsing buffer overflow */
	bool		found_corrupt_fs;
				/*!< set when an inconsistency with
				the file system contents is detected
				during log scan or apply */
	lsn_t		mlog_checkpoint_lsn;
				/*!< the LSN of a MLOG_CHECKPOINT
				record, or 0 if none was parsed */
	mem_heap_t*	heap;	/*!< memory heap of log records and file
				addresses*/
	hash_table_t*	addr_hash;/*!< hash table of file addresses of pages */
	ulint		n_addrs;/*!< number of not processed hashed file
				addresses in the hash table */

	recv_dblwr_t	dblwr;

	encryption_list_t*	/*!< Encryption information list */
			encryption_list;
};

/** The recovery system */
extern recv_sys_t*	recv_sys;

/** TRUE when applying redo log records during crash recovery; FALSE
otherwise.  Note that this is FALSE while a background thread is
rolling back incomplete transactions. */
extern volatile bool	recv_recovery_on;
/** If the following is TRUE, the buffer pool file pages must be invalidated
after recovery and no ibuf operations are allowed; this becomes TRUE if
the log record hash table becomes too full, and log records must be merged
to file pages already before the recovery is finished: in this case no
ibuf operations are allowed, as they could modify the pages read in the
buffer pool before the pages have been recovered to the up-to-date state.

TRUE means that recovery is running and no operations on the log files
are allowed yet: the variable name is misleading. */
extern bool		recv_no_ibuf_operations;
/** TRUE when recv_init_crash_recovery() has been called. */
extern bool		recv_needed_recovery;
#ifdef UNIV_DEBUG
/** TRUE if writing to the redo log (mtr_commit) is forbidden.
Protected by log_sys->mutex. */
extern bool		recv_no_log_write;
#endif /* UNIV_DEBUG */

/** TRUE if buf_page_is_corrupted() should check if the log sequence
number (FIL_PAGE_LSN) is in the future.  Initially FALSE, and set by
recv_recovery_from_checkpoint_start(). */
extern bool		recv_lsn_checks_on;
#ifdef UNIV_HOTBACKUP
/** TRUE when the redo log is being backed up */
extern bool		recv_is_making_a_backup;
#endif /* UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/** Flag indicating if recv_writer thread is active. */
extern volatile bool	recv_writer_thread_active;
#endif /* !UNIV_HOTBACKUP */

/** Size of the parsing buffer; it must accommodate RECV_SCAN_SIZE many
times! */
#define RECV_PARSING_BUF_SIZE	(2 * 1024 * 1024)

/** Size of block reads when the log groups are scanned forward to do a
roll-forward */
#define RECV_SCAN_SIZE		(4 * UNIV_PAGE_SIZE)

/** This many frames must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free frames to read in pages when we start applying the
log records to the database. */
extern ulint	recv_n_pool_free_frames;

#ifndef UNIV_NONINL
#include "log0recv.ic"
#endif

#endif
