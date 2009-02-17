/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
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

#ifdef UNIV_HOTBACKUP
extern ibool	recv_replay_file_ops;

/***********************************************************************
Reads the checkpoint info needed in hot backup. */
UNIV_INTERN
ibool
recv_read_cp_info_for_backup(
/*=========================*/
				/* out: TRUE if success */
	byte*		hdr,	/* in: buffer containing the log group
				header */
	ib_uint64_t*	lsn,	/* out: checkpoint lsn */
	ulint*		offset,	/* out: checkpoint offset in the log group */
	ulint*		fsp_limit,/* out: fsp limit of space 0,
				1000000000 if the database is running
				with < version 3.23.50 of InnoDB */
	ib_uint64_t*	cp_no,	/* out: checkpoint number */
	ib_uint64_t*	first_header_lsn);
				/* out: lsn of of the start of the
				first log file */
/***********************************************************************
Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned. */
UNIV_INTERN
void
recv_scan_log_seg_for_backup(
/*=========================*/
	byte*		buf,		/* in: buffer containing log data */
	ulint		buf_len,	/* in: data length in that buffer */
	ib_uint64_t*	scanned_lsn,	/* in/out: lsn of buffer start,
					we return scanned lsn */
	ulint*		scanned_checkpoint_no,
					/* in/out: 4 lowest bytes of the
					highest scanned checkpoint number so
					far */
	ulint*		n_bytes_scanned);/* out: how much we were able to
					scan, smaller than buf_len if log
					data ended here */
#endif /* UNIV_HOTBACKUP */
/***********************************************************************
Returns TRUE if recovery is currently running. */
UNIV_INLINE
ibool
recv_recovery_is_on(void);
/*=====================*/
/***********************************************************************
Returns TRUE if recovery from backup is currently running. */
UNIV_INLINE
ibool
recv_recovery_from_backup_is_on(void);
/*=================================*/
/****************************************************************************
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool. */
UNIV_INTERN
void
recv_recover_page(
/*==============*/
	ibool		recover_backup,
				/* in: TRUE if we are recovering a backup
				page: then we do not acquire any latches
				since the page was read in outside the
				buffer pool */
	ibool		just_read_in,
				/* in: TRUE if the i/o-handler calls this for
				a freshly read page */
	buf_block_t*	block);	/* in: buffer block */
/************************************************************
Recovers from a checkpoint. When this function returns, the database is able
to start processing of new user transactions, but the function
recv_recovery_from_checkpoint_finish should be called later to complete
the recovery and free the resources used in it. */
UNIV_INTERN
ulint
recv_recovery_from_checkpoint_start_func(
/*=====================================*/
					/* out: error code or DB_SUCCESS */
#ifdef UNIV_LOG_ARCHIVE
	ulint		type,		/* in: LOG_CHECKPOINT or LOG_ARCHIVE */
	ib_uint64_t	limit_lsn,	/* in: recover up to this lsn
					if possible */
#endif /* UNIV_LOG_ARCHIVE */
	ib_uint64_t	min_flushed_lsn,/* in: min flushed lsn from
					data files */
	ib_uint64_t	max_flushed_lsn);/* in: max flushed lsn from
					 data files */
#ifdef UNIV_LOG_ARCHIVE
# define recv_recovery_from_checkpoint_start(type,lim,min,max)		\
	recv_recovery_from_checkpoint_start_func(type,lim,min,max)
#else /* UNIV_LOG_ARCHIVE */
# define recv_recovery_from_checkpoint_start(type,lim,min,max)		\
	recv_recovery_from_checkpoint_start_func(min,max)
#endif /* UNIV_LOG_ARCHIVE */
/************************************************************
Completes recovery from a checkpoint. */
UNIV_INTERN
void
recv_recovery_from_checkpoint_finish(void);
/*======================================*/
/***********************************************************
Scans log from a buffer and stores new log data to the parsing buffer. Parses
and hashes the log records if new data found. */
UNIV_INTERN
ibool
recv_scan_log_recs(
/*===============*/
					/* out: TRUE if limit_lsn has been
					reached, or not able to scan any more
					in this log group */
	ibool		apply_automatically,/* in: TRUE if we want this
					function to apply log records
					automatically when the hash table
					becomes full; in the hot backup tool
					the tool does the applying, not this
					function */
	ulint		available_memory,/* in: we let the hash table of recs
					to grow to this size, at the maximum */
	ibool		store_to_hash,	/* in: TRUE if the records should be
					stored to the hash table; this is set
					to FALSE if just debug checking is
					needed */
	byte*		buf,		/* in: buffer containing a log segment
					or garbage */
	ulint		len,		/* in: buffer length */
	ib_uint64_t	start_lsn,	/* in: buffer start lsn */
	ib_uint64_t*	contiguous_lsn,	/* in/out: it is known that all log
					groups contain contiguous log data up
					to this lsn */
	ib_uint64_t*	group_scanned_lsn);/* out: scanning succeeded up to
					this lsn */
/**********************************************************
Resets the logs. The contents of log files will be lost! */
UNIV_INTERN
void
recv_reset_logs(
/*============*/
	ib_uint64_t	lsn,		/* in: reset to this lsn
					rounded up to be divisible by
					OS_FILE_LOG_BLOCK_SIZE, after
					which we add
					LOG_BLOCK_HDR_SIZE */
#ifdef UNIV_LOG_ARCHIVE
	ulint		arch_log_no,	/* in: next archived log file number */
#endif /* UNIV_LOG_ARCHIVE */
	ibool		new_logs_created);/* in: TRUE if resetting logs
					is done at the log creation;
					FALSE if it is done after
					archive recovery */
#ifdef UNIV_HOTBACKUP
/**********************************************************
Creates new log files after a backup has been restored. */
UNIV_INTERN
void
recv_reset_log_files_for_backup(
/*============================*/
	const char*	log_dir,	/* in: log file directory path */
	ulint		n_log_files,	/* in: number of log files */
	ulint		log_file_size,	/* in: log file size */
	ib_uint64_t	lsn);		/* in: new start lsn, must be
					divisible by OS_FILE_LOG_BLOCK_SIZE */
#endif /* UNIV_HOTBACKUP */
/************************************************************
Creates the recovery system. */
UNIV_INTERN
void
recv_sys_create(void);
/*=================*/
/************************************************************
Inits the recovery system for a recovery operation. */
UNIV_INTERN
void
recv_sys_init(
/*==========*/
	ibool	recover_from_backup,	/* in: TRUE if this is called
					to recover from a hot backup */
	ulint	available_memory);	/* in: available memory in bytes */
/***********************************************************************
Empties the hash table of stored log records, applying them to appropriate
pages. */
UNIV_INTERN
void
recv_apply_hashed_log_recs(
/*=======================*/
	ibool	allow_ibuf);	/* in: if TRUE, also ibuf operations are
				allowed during the application; if FALSE,
				no ibuf operations are allowed, and after
				the application all file pages are flushed to
				disk and invalidated in buffer pool: this
				alternative means that no new log records
				can be generated during the application */
#ifdef UNIV_HOTBACKUP
/***********************************************************************
Applies log records in the hash table to a backup. */
UNIV_INTERN
void
recv_apply_log_recs_for_backup(void);
/*================================*/
#endif
#ifdef UNIV_LOG_ARCHIVE
/************************************************************
Recovers from archived log files, and also from log files, if they exist. */
UNIV_INTERN
ulint
recv_recovery_from_archive_start(
/*=============================*/
					/* out: error code or DB_SUCCESS */
	ib_uint64_t	min_flushed_lsn,/* in: min flushed lsn field from the
					data files */
	ib_uint64_t	limit_lsn,	/* in: recover up to this lsn if
					possible */
	ulint		first_log_no);	/* in: number of the first archived
					log file to use in the recovery; the
					file will be searched from
					INNOBASE_LOG_ARCH_DIR specified in
					server config file */
/************************************************************
Completes recovery from archive. */
UNIV_INTERN
void
recv_recovery_from_archive_finish(void);
/*===================================*/
#endif /* UNIV_LOG_ARCHIVE */

/* Block of log record data */
typedef struct recv_data_struct	recv_data_t;
struct recv_data_struct{
	recv_data_t*	next;	/* pointer to the next block or NULL */
				/* the log record data is stored physically
				immediately after this struct, max amount
				RECV_DATA_BLOCK_SIZE bytes of it */
};

/* Stored log record struct */
typedef struct recv_struct	recv_t;
struct recv_struct{
	byte		type;	/* log record type */
	ulint		len;	/* log record body length in bytes */
	recv_data_t*	data;	/* chain of blocks containing the log record
				body */
	ib_uint64_t	start_lsn;/* start lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the start lsn of
				this log record */
	ib_uint64_t	end_lsn;/* end lsn of the log segment written by
				the mtr which generated this log record: NOTE
				that this is not necessarily the end lsn of
				this log record */
	UT_LIST_NODE_T(recv_t)
			rec_list;/* list of log records for this page */
};

/* Hashed page file address struct */
typedef struct recv_addr_struct	recv_addr_t;
struct recv_addr_struct{
	ulint		state;	/* RECV_NOT_PROCESSED, RECV_BEING_PROCESSED,
				or RECV_PROCESSED */
	ulint		space;	/* space id */
	ulint		page_no;/* page number */
	UT_LIST_BASE_NODE_T(recv_t)
			rec_list;/* list of log records for this page */
	hash_node_t	addr_hash;
};

/* Recovery system data structure */
typedef struct recv_sys_struct	recv_sys_t;
struct recv_sys_struct{
	mutex_t		mutex;	/* mutex protecting the fields apply_log_recs,
				n_addrs, and the state field in each recv_addr
				struct */
	ibool		apply_log_recs;
				/* this is TRUE when log rec application to
				pages is allowed; this flag tells the
				i/o-handler if it should do log record
				application */
	ibool		apply_batch_on;
				/* this is TRUE when a log rec application
				batch is running */
	ib_uint64_t	lsn;	/* log sequence number */
	ulint		last_log_buf_size;
				/* size of the log buffer when the database
				last time wrote to the log */
	byte*		last_block;
				/* possible incomplete last recovered log
				block */
	byte*		last_block_buf_start;
				/* the nonaligned start address of the
				preceding buffer */
	byte*		buf;	/* buffer for parsing log records */
	ulint		len;	/* amount of data in buf */
	ib_uint64_t	parse_start_lsn;
				/* this is the lsn from which we were able to
				start parsing log records and adding them to
				the hash table; zero if a suitable
				start point not found yet */
	ib_uint64_t	scanned_lsn;
				/* the log data has been scanned up to this
				lsn */
	ulint		scanned_checkpoint_no;
				/* the log data has been scanned up to this
				checkpoint number (lowest 4 bytes) */
	ulint		recovered_offset;
				/* start offset of non-parsed log records in
				buf */
	ib_uint64_t	recovered_lsn;
				/* the log records have been parsed up to
				this lsn */
	ib_uint64_t	limit_lsn;/* recovery should be made at most up to this
				lsn */
	ibool		found_corrupt_log;
				/* this is set to TRUE if we during log
				scan find a corrupt log block, or a corrupt
				log record, or there is a log parsing
				buffer overflow */
#ifdef UNIV_LOG_ARCHIVE
	log_group_t*	archive_group;
				/* in archive recovery: the log group whose
				archive is read */
#endif /* !UNIV_LOG_ARCHIVE */
	mem_heap_t*	heap;	/* memory heap of log records and file
				addresses*/
	hash_table_t*	addr_hash;/* hash table of file addresses of pages */
	ulint		n_addrs;/* number of not processed hashed file
				addresses in the hash table */
};

extern recv_sys_t*	recv_sys;
extern ibool		recv_recovery_on;
extern ibool		recv_no_ibuf_operations;
extern ibool		recv_needed_recovery;

extern ibool		recv_lsn_checks_on;
#ifdef UNIV_HOTBACKUP
extern ibool		recv_is_making_a_backup;
#endif /* UNIV_HOTBACKUP */
extern ulint		recv_max_parsed_page_no;

/* Size of the parsing buffer; it must accommodate RECV_SCAN_SIZE many
times! */
#define RECV_PARSING_BUF_SIZE	(2 * 1024 * 1024)

/* Size of block reads when the log groups are scanned forward to do a
roll-forward */
#define RECV_SCAN_SIZE		(4 * UNIV_PAGE_SIZE)

/* States of recv_addr_struct */
#define RECV_NOT_PROCESSED	71
#define RECV_BEING_READ		72
#define RECV_BEING_PROCESSED	73
#define RECV_PROCESSED		74

extern ulint	recv_n_pool_free_frames;

#ifndef UNIV_NONINL
#include "log0recv.ic"
#endif

#endif
